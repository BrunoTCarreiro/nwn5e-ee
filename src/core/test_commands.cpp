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
//   size), levels in a class (the engine's own level-ups, offered only the
//   2024 feats), ability scores, feats and items. The file is read each time,
//   so a new test needs only a new file, not a rebuild. Species features that
//   come as feats at creation stay those of the character's first species.
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

using CanChooseFeatFn = int (*)(void* stats, uint16_t feat, uint8_t general_left, uint8_t bonus_left, void* chosen);
CanChooseFeatFn g_server_can_choose_feat = nullptr;
bool g_test_levelling = false;  // !test is levelling a character: the server offers the 2024 feats only

int ServerCanChooseFeat(void* stats, uint16_t feat, uint8_t general_left, uint8_t bonus_left, void* chosen) {
  if (g_test_levelling && g_config.feats == rules::RuleVariant::kFaithful &&
      !rules::IsChoosableFeat2024(feat, g_config.weapon_mastery == rules::RuleVariant::kFaithful)) {
    return 0;
  }
  return g_server_can_choose_feat(stats, feat, general_left, bonus_left, chosen);
}

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
      g_test_levelling = true;
      RunScriptChunk(code, id);
      g_test_levelling = false;
    }
    lines.push_back(std::string("  class: ") + kClassNames[profile.nwn_class] + " " +
                    std::to_string(ClassLevel(stats, profile.nwn_class)) +
                    (add <= 0 ? " (already there)"
                     : had == 0 && TotalLevel(stats) > profile.level ? " (a new class: multiclassed)"
                                                                     : " (" + std::to_string(add) + " levels added)"));
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
  HookExport("CNWSCreatureStats::CanChooseFeat (!test's level-ups: the 2024 feats only)", ee::kServerCanChooseFeat,
             reinterpret_cast<void*>(&ServerCanChooseFeat), reinterpret_cast<void**>(&g_server_can_choose_feat));
  Log("config: [debug] test_commands=1 - !test (data\\test.txt), !give <blueprint>, !feat add / remove <feat>, "
      "!export");
}
