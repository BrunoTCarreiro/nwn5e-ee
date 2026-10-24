// Test helpers (`[debug] test_commands=1`, or the older give_command=1; off by
// default), so combat rules can be tried on an existing character:
// - "!give <blueprint> [stack]" creates an item from its blueprint (resref)
//   in the speaker's inventory — e.g. a Tower Shield (nw_ashto001). The
//   blueprint is checked to be a resref (letters, digits, '_', at most 16)
//   before it goes into the script.
// - "!feat add <feat>" / "!feat remove <feat>": a feat by its number or by
//   one of nwn5e's names (alert, savage attacker, skilled, archery, defense,
//   great weapon fighting, two-weapon fighting, asi 1..7, mastery <weapon>).
//   No prerequisites or slots are checked: it is for testing.
// - "!test" applies the profile in data	est.txt (next to nwn5e.dll; the
//   format is in rules/test_profile.h) to the speaker: species (race, model,
//   size), levels in a class (the engine's own level-ups; with feats faithful
//   they take the 2024 feats, rules_feats.cpp), ability scores, feats and items. The file is read each time,
//   so a new test needs only a new file, not a rebuild. Species features that
//   come as feats at creation stay those of the character's first species.
//   swap_class (owner, 2026-10-24: "change my PC's class to barbarian so I can
//   test this"): the first class becomes another, its level kept, through the
//   engine's SetClass; the old class's automatic feats (its feat table's List 3
//   up to that level) give way to the new one's - a Barbarian's Rage chain.
//   The level history follows (0.22.12: each level's class; the level-up
//   refused "invalid number of multiclasses" without it). For test
//   characters: an old caster's spell lists aren't rewritten.
// - "!export" saves the character to the vault as a campaign's chapter change
//   does (C6's check: its records come back from its own database).

#include <windows.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include "chat.h"
#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "rules/class_features.h"
#include "rules/feats2024.h"
#include "rules/nwn_conversion.h"
#include "rules/progression.h"
#include "rules/test_profile.h"
#include "rules/weapons.h"

void SetTestAbilityScores(void* creature, const int scores[6]);  // rules_abilities.cpp

namespace {

bool IsResRef(const std::string& text) {
  if (text.empty() || text.size() > 16) return false;
  for (char c : text) {
    if (!isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
  }
  return true;
}

std::string Lower(std::string text) {
  for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return text;
}

std::string Trim(const std::string& text) {
  const size_t first = text.find_first_not_of(' ');
  if (first == std::string::npos) return "";
  return text.substr(first, text.find_last_not_of(' ') - first + 1);
}

std::vector<std::string> GiveCommand(void* creature, const std::string& args) {
  std::string blueprint = args, count_text;
  const size_t space = args.find(' ');
  if (space != std::string::npos) {
    blueprint = args.substr(0, space);
    count_text = args.substr(space + 1);
  }
  blueprint = Lower(blueprint);
  if (!IsResRef(blueprint)) {
    return {"nwn5e: !give <item blueprint> [stack] - e.g. !give nw_ashto001 (Tower Shield), nw_wswgs001 "
            "(Greatsword), nw_wplss001 (Spear), nw_wamar001 20 (20 arrows)."};
  }
  int count = count_text.empty() ? 1 : atoi(count_text.c_str());
  if (count < 1) count = 1;
  if (count > 99) count = 99;
  const uint32_t id = ObjectIdOf(StatsOf(creature));
  const std::string code = "object o = CreateItemOnObject(\"" + blueprint + "\", OBJECT_SELF, " +
                           std::to_string(count) +
                           "); if (GetIsObjectValid(o)) SendMessageToPC(OBJECT_SELF, \"nwn5e: given \" + GetName(o) + "
                           "\".\"); else SendMessageToPC(OBJECT_SELF, \"nwn5e: no item blueprint \\\"" +
                           blueprint + "\\\".\");";
  RunScriptChunk(code.c_str(), id);
  Log("give: 0x%08X asked for %s x%d", id, blueprint.c_str(), count);
  return {};
}

// A feat named by its number or by one of nwn5e's names; -1 when unknown.
int FeatByName(const std::string& text) {
  const std::string name = Lower(Trim(text));
  if (name.empty()) return -1;
  if (isdigit(static_cast<unsigned char>(name[0]))) return atoi(name.c_str());
  static const struct {
    const char* name;
    int feat;
  } kNames[] = {
      {"alert", rules::kFeatAlert},
      {"savage attacker", rules::kFeatSavageAttacker},
      {"skilled", rules::kFeatSkilled},
      {"archery", rules::kFeatFightingStyleArchery},
      {"defense", rules::kFeatFightingStyleDefense},
      {"great weapon fighting", rules::kFeatFightingStyleGreatWeapon},
      {"two-weapon fighting", rules::kFeatFightingStyleTwoWeapon},
      {"two weapon fighting", rules::kFeatFightingStyleTwoWeapon},
  };
  for (const auto& entry : kNames) {
    if (name == entry.name) return entry.feat;
  }
  if (name.rfind("asi ", 0) == 0) {
    const int n = atoi(name.c_str() + 4);
    return n >= 1 && n <= rules::kAsiFeatCount ? rules::kAsiFeatFirst + n - 1 : -1;
  }
  if (name.rfind("mastery ", 0) == 0) {
    const int weapon = rules::NwnWeaponFromName(name.substr(8));
    return weapon >= 0 ? rules::WeaponMasteryFeat(weapon) : -1;
  }
  return -1;
}

std::vector<std::string> FeatCommand(void* creature, const std::string& args) {
  const std::string usage =
      "nwn5e: !feat add <feat> / !feat remove <feat> - a feat number or: alert, savage attacker, skilled, archery, "
      "defense, great weapon fighting, two-weapon fighting, asi 1..7, mastery <weapon> (testing: no checks).";
  void* stats = StatsOf(creature);
  const std::string lower = Lower(Trim(args));
  const bool add = lower.rfind("add ", 0) == 0, remove = lower.rfind("remove ", 0) == 0;
  if (!stats || (!add && !remove)) return {usage};
  const std::string what = Trim(Trim(args).substr(add ? 4 : 7));
  const int feat = FeatByName(what);
  if (feat < 0) return {"nwn5e: unknown feat \"" + what + "\". " + usage};
  const bool had = engine::HasFeat(stats, feat);
  const char* result = nullptr;
  if (add) {
    result = had ? "already known" : engine::AddFeat(stats, feat) ? "added" : "could not be added";
  } else {
    if (had) engine::RemoveFeat(stats, feat);
    result = had ? "removed" : "not known";
  }
  char code[400];
  sprintf_s(code,
            "SendMessageToPC(OBJECT_SELF, \"nwn5e: feat \" + GetStringByStrRef(StringToInt(Get2DAString(\"feat\", "
            "\"FEAT\", %d))) + \" (%d) %s.\");",
            feat, feat, result);
  const uint32_t id = ObjectIdOf(stats);
  RunScriptChunk(code, id);
  Log("feat command: 0x%08X %s feat %d - %s", id, add ? "add" : "remove", feat, result);
  return {};
}

// --- !export --------------------------------------------------------------------
// C6's chapter-export check: the character saved to the vault the way a
// campaign's chapter change does (ExportSingleCharacter); start a new game
// with it and its records come back from its own database.
std::vector<std::string> ExportCommand(void* creature, const std::string&) {
  const uint32_t id = ObjectIdOf(StatsOf(creature));
  RunScriptChunk("ExportSingleCharacter(OBJECT_SELF); SendMessageToPC(OBJECT_SELF, \"nwn5e: exported to the vault as "
                 "a chapter change does - start a new game with this character to check what it keeps.\");",
                 id);
  Log("export: 0x%08X exported (ExportSingleCharacter)", id);
  return {};
}

// --- !test --------------------------------------------------------------------

std::wstring DllDirectory() {
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&DllDirectory), &module);
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(module, path, MAX_PATH);
  std::wstring directory = path;
  const size_t slash = directory.find_last_of(L"\\/");
  return slash == std::wstring::npos ? L"." : directory.substr(0, slash);
}

bool ReadTextFile(const std::wstring& path, std::string* text) {
  FILE* file = nullptr;
  if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file) return false;
  char buffer[4096];
  size_t read = 0;
  while ((read = fread(buffer, 1, sizeof(buffer), file)) > 0) text->append(buffer, read);
  fclose(file);
  return true;
}

int ClassLevel(void* stats, int nwn_class) {
  int levels = 0;
  for (const rules::NwnClassSlot& slot : ReadClassSlots(stats)) {
    if (slot.nwn_class_id == nwn_class) levels += slot.levels;
  }
  return levels;
}

int TotalLevel(void* stats) {
  int levels = 0;
  for (const rules::NwnClassSlot& slot : ReadClassSlots(stats)) levels += slot.levels;
  return levels;
}

const char* const kClassNames[] = {"Barbarian", "Bard",    "Cleric", "Druid",    "Fighter", "Monk",
                                   "Paladin",   "Ranger",  "Rogue",  "Sorcerer", "Wizard"};
const char* const kRaceNames[] = {"Dwarf", "Elf", "Gnome", "Halfling", "Half-Elf", "Half-Orc", "Human"};

// The automatic feats (List 3) of a class's feat table (classes.2da
// FeatsTable; the served table, so faithful's changes count) up to `level`.
std::vector<int> AutomaticClassFeats(int nwn_class, int level) {
  const std::vector<std::string> tables = GameTableColumn("classes", "FeatsTable");
  if (nwn_class < 0 || nwn_class >= static_cast<int>(tables.size()) || tables[nwn_class].empty()) return {};
  const std::string table = Lower(tables[nwn_class]);
  const std::vector<std::string> feats = GameTableColumn(table.c_str(), "FeatIndex");
  const std::vector<std::string> lists = GameTableColumn(table.c_str(), "List");
  const std::vector<std::string> levels = GameTableColumn(table.c_str(), "GrantedOnLevel");
  std::vector<int> out;
  for (size_t i = 0; i < feats.size() && i < lists.size() && i < levels.size(); ++i) {
    if (feats[i].empty() || lists[i] != "3") continue;
    const int granted = atoi(levels[i].c_str());
    if (granted >= 1 && granted <= level) out.push_back(atoi(feats[i].c_str()));
  }
  return out;
}

const char* ClassName(int nwn_class) {
  return nwn_class >= 0 && nwn_class < static_cast<int>(sizeof(kClassNames) / sizeof(kClassNames[0]))
             ? kClassNames[nwn_class]
             : "another class";
}

// The level history: a level taken in a class the character no longer has
// becomes `to`'s. Returns how many levels changed.
int RewriteLevelHistory(void* stats, int to) {
  using GetLevelStatsFn = void* (*)(void* stats, uint8_t index);
  static const auto get_level_stats = reinterpret_cast<GetLevelStatsFn>(EngineExport(ee::kGetLevelStats));
  if (!get_level_stats) return 0;
  const std::vector<rules::NwnClassSlot> slots = ReadClassSlots(stats);
  int changed = 0;
  for (int i = 0; i < 60; ++i) {
    void* level = get_level_stats(stats, static_cast<uint8_t>(i));
    if (!level) break;
    const int nwn_class = Field<uint8_t>(level, ee::kLevelStatsClass);
    bool held = false;
    for (const rules::NwnClassSlot& slot : slots) held = held || slot.nwn_class_id == nwn_class;
    if (held) continue;
    SetField<uint8_t>(level, ee::kLevelStatsClass, static_cast<uint8_t>(to));
    ++changed;
  }
  return changed;
}

// swap_class: the first class becomes `to`, its level kept; the automatic feats follow.
std::string SwapFirstClass(void* stats, int to) {
  using SetClassFn = void (*)(void* stats, uint8_t slot, uint8_t nwn_class);
  static const auto set_class = reinterpret_cast<SetClassFn>(EngineExport(ee::kSetClass));
  const std::vector<rules::NwnClassSlot> slots = ReadClassSlots(stats);
  if (slots.empty() || !set_class) return "  swap_class: no class to swap (or no SetClass in this build)";
  const int from = slots[0].nwn_class_id, level = slots[0].levels;
  if (from == to) {  // swapped before: the level history may still be the old class's (0.22.11)
    const int repaired = RewriteLevelHistory(stats, to);
    if (repaired) Log("test: 0x%08X level history: %d levels now %d", ObjectIdOf(stats), repaired, to);
    return std::string("  swap_class: already a ") + ClassName(to) +
           (repaired ? " - its level history repaired (" + std::to_string(repaired) + " levels)" : "");
  }
  const std::vector<int> old_feats = AutomaticClassFeats(from, level);
  const std::vector<int> new_feats = AutomaticClassFeats(to, level);
  set_class(stats, 0, static_cast<uint8_t>(to));
  int removed = 0, added = 0;
  for (int feat : old_feats) {
    bool kept = false;
    for (int f : new_feats) kept = kept || f == feat;
    if (!kept && engine::HasFeat(stats, feat)) {
      engine::RemoveFeat(stats, feat);
      ++removed;
    }
  }
  for (int feat : new_feats) {
    if (!engine::HasFeat(stats, feat) && engine::AddFeat(stats, feat)) ++added;
  }
  const int history = RewriteLevelHistory(stats, to);
  Log("test: 0x%08X first class %d -> %d at level %d (automatic feats -%d +%d, %d levels of history)",
      ObjectIdOf(stats), from, to, level, removed, added, history);
  return std::string("  class swapped: ") + ClassName(from) + " " + std::to_string(level) + " -> " + ClassName(to) +
         " " + std::to_string(level) + " (automatic feats: " + std::to_string(removed) + " removed, " +
         std::to_string(added) + " added" + (slots.size() > 1 ? "; only the first class" : "") + ")";
}

std::vector<std::string> TestCommand(void* creature, const std::string&) {
  void* stats = StatsOf(creature);
  if (!stats) return {};
  const std::wstring path = DllDirectory() + L"\\data\\test.txt";
  std::string text;
  if (!ReadTextFile(path, &text)) return {"nwn5e: !test - no data\\test.txt next to nwn5e.dll."};
  const rules::TestProfile profile = rules::ParseTestProfile(text);
  if (!profile.errors.empty()) {
    std::vector<std::string> lines = {"nwn5e: !test - data\\test.txt has errors, nothing applied:"};
    for (const std::string& error : profile.errors) lines.push_back("  " + error);
    return lines;
  }
  const uint32_t id = ObjectIdOf(stats);
  std::vector<std::string> lines = {"nwn5e: !test" + (profile.name.empty() ? std::string() : " - " + profile.name)};
  // 1. Species: the race and its model.
  if (profile.race >= 0) {
    SetField<uint16_t>(stats, ee::kStatsRace, static_cast<uint16_t>(profile.race));
    RunScriptChunk(("SetCreatureAppearanceType(OBJECT_SELF, " + std::to_string(profile.race) + ");").c_str(), id);
    // The size the weapon, perception and shove rules read (a gnome's or a halfling's is Small).
    SetField<int32_t>(creature, ee::kCreatureSize,
                      profile.race == 2 || profile.race == 3 ? ee::kCreatureSizeSmall : ee::kCreatureSizeMedium);
    lines.push_back(std::string("  species: ") + kRaceNames[profile.race]);
  }
  // 1b. The first class swapped (before the levels, which may then add some in it).
  if (profile.swap_class >= 0) lines.push_back(SwapFirstClass(stats, profile.swap_class));
  // 2. Level: the XP for it, then the engine's own level-ups in the class.
  if (profile.nwn_class >= 0 && profile.level > 0) {
    const int had = ClassLevel(stats, profile.nwn_class);
    const int add = profile.level - had;
    if (add > 0) {
      const int total = TotalLevel(stats) + add;
      const uint32_t xp = g_config.progression == rules::RuleVariant::kFaithful ? rules::FaithfulXpForLevel(total)
                                                                                : rules::NwnXpForLevel(total);
      char code[300];
      sprintf_s(code,
                "if (GetXP(OBJECT_SELF) < %u) SetXP(OBJECT_SELF, %u); int i;"
                " for (i = 0; i < %d; i++) if (!LevelUpHenchman(OBJECT_SELF, %d, TRUE)) break;",
                xp, xp, add, profile.nwn_class);
      RunScriptChunk(code, id);  // the automatic level-ups take the 2024 feats (rules_feats.cpp)
    }
    lines.push_back(std::string("  class: ") + kClassNames[profile.nwn_class] + " " +
                    std::to_string(ClassLevel(stats, profile.nwn_class)) +
                    (add <= 0 ? " (already there)"
                     : had == 0 && TotalLevel(stats) > profile.level ? " (a new class: multiclassed)"
                                                                     : " (" + std::to_string(add) + " levels added)"));
  }
  // 2b. Experience only: the level-up screen makes the levels.
  if (profile.xp > 0) {
    char code[200];
    sprintf_s(code, "if (GetXP(OBJECT_SELF) < %d) SetXP(OBJECT_SELF, %d);", profile.xp, profile.xp);
    RunScriptChunk(code, id);
    lines.push_back("  xp: " + std::to_string(profile.xp) + " (level up on the level-up screen)");
  }
  // 3. Ability scores (through the ability sync).
  if (profile.has_abilities) {
    SetTestAbilityScores(creature, profile.abilities);
    char scores[80];
    sprintf_s(scores, "  abilities: Str %d Dex %d Con %d Int %d Wis %d Cha %d", profile.abilities[0],
              profile.abilities[1], profile.abilities[2], profile.abilities[3], profile.abilities[4],
              profile.abilities[5]);
    lines.push_back(scores);
  }
  // 4. Feats.
  std::string added, unknown;
  for (const std::string& name : profile.feats) {
    const int feat = FeatByName(name);
    if (feat < 0) {
      unknown += (unknown.empty() ? "" : ", ") + name;
      continue;
    }
    if (!engine::HasFeat(stats, feat)) engine::AddFeat(stats, feat);
    added += (added.empty() ? "" : ", ") + name;
  }
  if (!added.empty()) lines.push_back("  feats: " + added);
  if (!unknown.empty()) lines.push_back("  unknown feats (skipped): " + unknown);
  // 5. Items, equipped where asked (armour to the chest, shields and "offhand" to the left hand).
  for (const rules::TestItem& item : profile.items) {
    char code[700];
    sprintf_s(code,
              "object o = CreateItemOnObject(\"%s\", OBJECT_SELF, %d);"
              " if (!GetIsObjectValid(o)) { SendMessageToPC(OBJECT_SELF, \"nwn5e: !test - no item blueprint %s\"); return; }"
              " if (%d) { int b = GetBaseItemType(o); int slot = INVENTORY_SLOT_RIGHTHAND;"
              " if (b == BASE_ITEM_ARMOR) slot = INVENTORY_SLOT_CHEST;"
              " else if (%d || b == BASE_ITEM_SMALLSHIELD || b == BASE_ITEM_LARGESHIELD || b == BASE_ITEM_TOWERSHIELD)"
              " slot = INVENTORY_SLOT_LEFTHAND;"
              " AssignCommand(OBJECT_SELF, ActionEquipItem(o, slot)); }",
              item.blueprint.c_str(), item.stack, item.blueprint.c_str(), item.equip ? 1 : 0, item.offhand ? 1 : 0);
    RunScriptChunk(code, id);
  }
  if (!profile.items.empty()) {
    std::string names;
    for (const rules::TestItem& item : profile.items) {
      names += (names.empty() ? "" : ", ") + item.blueprint + (item.stack > 1 ? " x" + std::to_string(item.stack) : "") +
               (item.equip ? " (equipped)" : "");
    }
    lines.push_back("  items: " + names);
  }
  Log("test: 0x%08X applied data\\test.txt (%s)", id, profile.name.c_str());
  return lines;
}

}  // namespace

void InstallTestCommands() {
  if (!g_config.debug_test_commands) return;
  AddChatCommand("!give", &GiveCommand);
  AddChatCommand("!feat", &FeatCommand);
  AddChatCommand("!test", &TestCommand);
  AddChatCommand("!export", &ExportCommand);
  Log("config: [debug] test_commands=1 - !test (data\\test.txt), !give <blueprint>, !feat add / remove <feat>, "
      "!export");
}
