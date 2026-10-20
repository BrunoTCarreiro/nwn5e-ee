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

#include <stdint.h>

#include <intrin.h>

#include <vector>

#include "config.h"
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

int GetIsClassAvailable(void* stats, uint8_t nwn_class) {
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
  return 0;
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
  }
}
