// CM1 / CM2 (port of 1.69's rules_progression.cpp; docs/EE-PORT.md E4a, EP16),
// faithful:
// - CM1: the engine's XP table is 2024's (rules +0x28, written after every
//   load), the level cap is 20 (CanLevelUp: EE stops player characters at 40
//   — henchmen and companions are capped at 20 too now), and a script's SetXP
//   is read in NWN's table (rules::ScriptSetXp2024): HotU's 105,000 = level 15
//   becomes 165,000, the respawn scripts' "never lose a level" clamp neither
//   gives XP nor takes levels, and no script XP loss drops a level.
// - CM2: no multiclass XP penalty (each class's XPPenalty 0 after load); a new
//   class needs 13 in its primary ability and in every current class's
//   (GetIsClassAvailable, which feeds the level-up screen's class list, the
//   server's checks and henchmen's level-ups); up to 8 classes (EP16: EE
//   stores 8 and reads the limit only from MULTICLASS_LIMIT).
// - CM10 (class_features faithful): no prestige classes.
// NWN mode: NWN's.

#include <windows.h>

#include <stdint.h>
#include <stdlib.h>

#include <intrin.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "config.h"
#include "data_serving.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "rules/nwn_conversion.h"
#include "rules/progression.h"
#include "rules_loaded.h"
#include "ruleset.h"

namespace {

constexpr int kLastBaseClass = 10;  // classes.2da rows 0..10: Barbarian .. Wizard
constexpr int kLevelCap2024 = 20;
constexpr int kClassLimit2024 = 8;  // EE's class block (EP16)

using CanLevelUpFn = int (*)(void* stats);
using SetExperienceFn = void (*)(void* stats, uint32_t xp, int level_changes);
using ClassAvailableFn = int (*)(void* stats, uint8_t nwn_class);
using LevelFn = uint8_t (*)(void* stats, int subtract_negative);
using StatFn = uint8_t (*)(void* stats);

CanLevelUpFn g_can_level_up = nullptr;
SetExperienceFn g_set_experience = nullptr;
ClassAvailableFn g_class_available = nullptr;
uintptr_t g_script_set_xp_caller = 0;
LogOnce g_logged(200);
int g_set_xp_lines = 0;

uint8_t* Classes(void* rules) { return rules ? Field<uint8_t*>(rules, ee::kRulesClasses) : nullptr; }

int Level(void* stats) {
  static const auto level = reinterpret_cast<LevelFn>(EngineExport(ee::kGetLevel));
  return level ? level(stats, 0) : 0;
}

// After every rules load: the XP table becomes 2024's.
void SetFaithfulXpTable(void* rules, bool reload) {
  auto* table = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(rules) + ee::kRulesXpTable);
  const uint32_t nwn_level_2 = table[1];
  for (int i = 0; i < rules::kXpTableSize; ++i) table[i] = rules::FaithfulXpForLevel(i + 1);
  Log("progression: XP table set to 2024%s (level 2 at %u, 5 at %u, 20 at %u; NWN's level 2 was %u)",
      reload ? " again after a rules load" : "", table[1], table[4], table[19], nwn_level_2);
}

// CM2: no multiclass XP penalty.
void NoXpPenalty(void* rules, bool reload) {
  uint8_t* classes = Classes(rules);
  const int count = Field<uint8_t>(rules, ee::kRulesClassCount);
  if (!classes) return;
  int changed = 0;
  for (int i = 0; i < count; ++i) {
    uint8_t* c = classes + i * ee::kClassStride;
    if (Field<int32_t>(c, ee::kClassXpPenalty) == 0) continue;
    SetField<int32_t>(c, ee::kClassXpPenalty, 0);
    ++changed;
  }
  if (!reload) Log("multiclass: no XP penalty (%d of %d classes had one)", changed, count);
}

int CanLevelUp(void* stats) {
  const int engine = g_can_level_up(stats);
  if (!engine || !stats || Level(stats) < kLevelCap2024) return engine;
  if (g_logged.First(LogOnce::Key(ObjectIdOf(stats), kLevelCap2024, -2))) {
    Log("progression: 0x%08X is level %d - the 2024 cap is 20", ObjectIdOf(stats), Level(stats));
  }
  return 0;
}

// The SetXP script command's SetExperience call: the XP read in NWN's table.
void SetExperience(void* stats, uint32_t xp, int level_changes) {
  if (stats && reinterpret_cast<uintptr_t>(_ReturnAddress()) == g_script_set_xp_caller &&
      Field<int32_t>(stats, ee::kStatsIsPlayer) != 0) {
    const uint32_t current = Field<uint32_t>(stats, ee::kStatsExperience);
    const int level = Level(stats);
    const uint32_t converted = rules::ScriptSetXp2024(xp, current, level);
    if (converted != xp && g_set_xp_lines < 100) {
      ++g_set_xp_lines;
      Log("progression: script SetXP %u -> %u for 0x%08X (level %d, XP %u)", xp, converted, ObjectIdOf(stats), level,
          current);
    }
    xp = converted;
  }
  g_set_experience(stats, xp, level_changes);
}

const char* ClassName(rules::CharacterClass c) {
  static const char* const kNames[] = {"Barbarian", "Bard",   "Cleric", "Druid",    "Fighter", "Monk",
                                       "Paladin",   "Ranger", "Rogue",  "Sorcerer", "Warlock", "Wizard"};
  return kNames[static_cast<int>(c)];
}

void ReadScores(void* stats, int scores[6]) {
  for (int i = 0; i < 6; ++i) {
    const auto getter = reinterpret_cast<StatFn>(EngineExport(ee::kGetAbilityStat[i]));
    scores[i] = getter ? getter(stats) : 10;
  }
}

// Why the Class page refuses a class, for its description (owner,
// 2026-10-22: "add the reason to the class descriptions"): the player's
// characters' checks only (ids from 0x7FFFFFFF down), each round of them (the
// page's list, 2 s apart from the last) afresh, all gone when the Class page
// closes; rules_labels.cpp appends a class's note to its description
// (classes.2da Description) as the string is looked up.
std::mutex g_notes_lock;
std::atomic<bool> g_have_notes{false};
std::map<uint8_t, std::string> g_class_notes;
uint64_t g_notes_at = 0;
std::map<uint32_t, uint8_t> g_description_class;  // Description strref -> classes.2da row

void NoteClass(void* stats, uint8_t nwn_class, const std::string& note) {
  if (!stats || ObjectIdOf(stats) <= 0x7F000000u) return;
  std::lock_guard<std::mutex> lock(g_notes_lock);
  const uint64_t now = GetTickCount64();
  if (now - g_notes_at > 2000) g_class_notes.clear();
  g_notes_at = now;
  if (note.empty()) {
    g_class_notes.erase(nwn_class);
  } else {
    g_class_notes[nwn_class] = note;
  }
  g_have_notes = !g_class_notes.empty();
}

void ForgetClassNotes() {
  std::lock_guard<std::mutex> lock(g_notes_lock);
  g_class_notes.clear();
  g_have_notes = false;
}

void MapClassDescriptions(void*, bool) {
  const std::vector<std::string> descriptions = GameTableColumn("classes", "Description");
  std::lock_guard<std::mutex> lock(g_notes_lock);
  g_description_class.clear();
  for (size_t row = 0; row < descriptions.size() && row < 256; ++row) {
    if (descriptions[row].empty()) continue;
    const unsigned long strref = strtoul(descriptions[row].c_str(), nullptr, 10);
    if (strref && !g_description_class.count(strref)) g_description_class[strref] = static_cast<uint8_t>(row);
  }
  Log("classes: %zu class descriptions mapped (a refused class's says why)", g_description_class.size());
}

using DestroyFn = void* (*)(void* object, int free_it);
DestroyFn g_class_page_dtor = nullptr;
void* ClassPageDtor(void* page, int free_it) {
  ForgetClassNotes();
  return g_class_page_dtor(page, free_it);
}

int ClassAvailable(void* stats, uint8_t nwn_class, std::string* note) {
  const int allowed = g_class_available(stats, nwn_class);
  if (!allowed || !stats) return allowed;
  // CM10: no prestige classes in faithful (their 2024 counterparts are the SRD
  // subclasses, granted at class level 3); levels already taken count as the
  // parent class (decision 10).
  if (g_config.class_features == rules::RuleVariant::kFaithful && nwn_class > kLastBaseClass) {
    if (g_logged.First(LogOnce::Key(ObjectIdOf(stats), nwn_class, -1))) {
      Log("classes: 0x%08X can't take class %u - prestige classes are off (2024: subclasses)", ObjectIdOf(stats),
          nwn_class);
    }
    *note = "2024 rules: no prestige classes - your class's subclass, at class level 3, takes their place.";
    return 0;
  }
  if (g_config.multiclass != rules::RuleVariant::kFaithful) return allowed;
  const auto next = rules::ClassFromNwnId(nwn_class);
  if (!next) return allowed;
  std::vector<rules::CharacterClass> current;
  for (const rules::NwnClassSlot& slot : ReadClassSlots(stats)) {
    if (const auto c = rules::ClassFromNwnId(slot.nwn_class_id)) current.push_back(*c);
  }
  int scores[6];
  ReadScores(stats, scores);
  rules::CharacterClass failing = *next;
  if (rules::CanMulticlassInto(*next, current, scores, &failing)) return allowed;
  if (g_logged.First(LogOnce::Key(ObjectIdOf(stats), nwn_class, static_cast<int>(failing)))) {
    Log("multiclass: 0x%08X can't take %s - %s needs %s (Str %d Dex %d Con %d Int %d Wis %d Cha %d)", ObjectIdOf(stats),
        ClassName(*next), ClassName(failing), rules::PrimaryAbilityText(failing), scores[0], scores[1], scores[2],
        scores[3], scores[4], scores[5]);
  }
  *note = rules::MulticlassRefusalText(*next, failing, scores);
  return 0;
}

int GetIsClassAvailable(void* stats, uint8_t nwn_class) {
  std::string note;
  const int allowed = ClassAvailable(stats, nwn_class, &note);
  NoteClass(stats, nwn_class, note);
  return allowed;
}

int ClassLimit(int) { return kClassLimit2024; }

}  // namespace

void InstallProgressionRules() {
  if (g_config.progression == rules::RuleVariant::kFaithful) {
    OnRulesLoaded(&SetFaithfulXpTable);
    HookExport("CNWSCreatureStats::CanLevelUp (level cap 20)", ee::kCanLevelUp, reinterpret_cast<void*>(&CanLevelUp),
               reinterpret_cast<void**>(&g_can_level_up));
    const std::vector<uintptr_t> calls =
        FindCallsTo(FunctionAt(EngineExport(ee::kExecuteCommandSetXP)), EngineExport(ee::kSetExperience));
    if (calls.size() == 1) {
      g_script_set_xp_caller = calls[0] + kCallLength;
      HookExport("CNWSCreatureStats::SetExperience (script SetXP read in NWN's table)", ee::kSetExperience,
                 reinterpret_cast<void*>(&SetExperience), reinterpret_cast<void**>(&g_set_experience));
    } else {
      Log("SKIP progression: %zu SetExperience calls in the SetXP command (expected 1)", calls.size());
    }
  }
  if (g_config.multiclass == rules::RuleVariant::kFaithful) {
    OnRulesLoaded(&NoXpPenalty);
    AnswerRulesetInt("MULTICLASS_LIMIT", "multiclass: up to 8 classes (EP16)", &ClassLimit);
  }
  // One hook for CM2 (13s to multiclass) and CM10 (no prestige classes).
  if (g_config.multiclass == rules::RuleVariant::kFaithful ||
      g_config.class_features == rules::RuleVariant::kFaithful) {
    HookExport("CNWSCreatureStats::GetIsClassAvailable (2024 multiclassing, no prestige classes)",
               ee::kGetIsClassAvailable, reinterpret_cast<void*>(&GetIsClassAvailable),
               reinterpret_cast<void**>(&g_class_available));
    // The refused classes' descriptions say why: the descriptions' strings,
    // the notes cleared as the Class page closes.
    OnRulesLoaded(&MapClassDescriptions);
    const uintptr_t class_page = FindVtable("classes: the Class page's vtable", ee::kRttiClassPage);
    const uintptr_t class_page_dtor = class_page ? VtableSlot(class_page, 0) : 0;
    if (!class_page_dtor ||
        !HookAddress("client Class page deleting destructor (the refused classes' notes cleared)", class_page_dtor,
                     reinterpret_cast<void*>(&ClassPageDtor), reinterpret_cast<void**>(&g_class_page_dtor))) {
      Log("SKIP classes: the refused classes' notes - no Class page destructor");
      return;
    }
  }
}

// The note for a class description's string (rules_labels.cpp), if its class
// was refused on the open Class page.
bool ClassDescriptionNote(uint32_t strref, std::string* note) {
  if (!g_have_notes) return false;
  std::lock_guard<std::mutex> lock(g_notes_lock);
  if (!g_class_page_dtor) return false;  // nothing would clear them
  const auto row = g_description_class.find(strref);
  if (row == g_description_class.end()) return false;
  const auto found = g_class_notes.find(row->second);
  if (found == g_class_notes.end()) return false;
  *note = found->second;
  return true;
}
