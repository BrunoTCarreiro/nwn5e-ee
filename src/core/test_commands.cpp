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
// - "!test <profile>" applies data\tests\<profile>.txt (next to nwn5e.dll;
//   "!test" alone lists them; the format is in rules/test_profile.h) to the
//   speaker: species (race, model,
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
//   characters: an old caster's spell lists aren't rewritten. A Fighting
//   Style feat needs a class with the Fighting Style feature (Fighter 1,
//   Paladin / Ranger 2); with none, !test takes it away (0.41.3; owner: "make
//   it clear the fighting style") - one left from a swapped Fighter. One named
//   under `feats` is still given.
//   0.57.5 (#82; owner: "set different "!test" commands for each of the
//   different requirements"): a profile per test setup in data\tests; more
//   classes (`levels`); test switches for the session (`switches` - the ones
//   read each time they are used; every !test <profile> puts the ini's values
//   back first, so one test's switches don't stay on for the next); `note`
//   lines for what a profile can't do itself.
//   0.57.6 (owner, offered a reset or "levels are only added": "go with a"): a
//   profile that builds a character (swap_class, class or levels) first takes
//   it down to level 1 - a script SetXP 0, whose level loss (the engine's, as
//   the DM level-down script's; CM1 reads 0 as "level 1") takes the levels
//   away, the last taken first - so `!test fighter` is Fighter 13 whatever the
//   character was. If no level is lost, its XP goes back and the profile only
//   adds, as before.
//   0.57.7 (the owner's log, 2026-10-30: fourclass's Wizard refused - "Wizard
//   needs Intelligence 13" on the previous profile's Int 10): the profile's
//   scores are set before the levels too, so a new class's 13 test (CM2) sees
//   them; and again after, so the levels' own increases don't change them.
//   0.57.9 (owner: after "!test monk" "the quickbar's slots all became
//   empty"): a profile that builds a character forgets the quickbar's
//   placement record, so nwn5e's actions come back on empty slots (the reset
//   took the old buttons with their feats; the record kept them off).
// - "!export" saves the character to the vault as a campaign's chapter change
//   does (C6's check: its records come back from its own database).
// - "!spawn <blueprint> [ally]" (0.49.1, for the Grapple's and the Shove's size
//   rows - "a Huge creature wasn't available"): a creature from its blueprint 5 m
//   in front of the speaker, facing it, hostile - or with "ally" its henchman
//   (one that fights the others). E.g. nw_gntfrost (Frost Giant, Huge),
//   nw_gnthill (Hill Giant, Large), nw_orcb (Orc, Medium).
// - 0.50.3 (owner: "can't create a situation where EL1 and SL1 are testable"):
//   "!prone [seconds]" knocks the speaker down (6 s; at most 60) - attacks
//   against a prone creature (Elusive); "!save <ability> <dc>" rolls that save
//   for the speaker against a 2024 DC (the save wrapper: Stroke of Luck, Indomitable
//   Might ...); "!reflex <dc> <damage>" a Reflex-halved hit through the engine's
//   GetReflexAdjustedDamage, as an area spell's (Evasion), as fire damage.
// - 0.50.5 (owner: "add a !level command"): "!level <n> [class]" - the
//   engine's own level-ups up to level n in the class (the first class when
//   none is named; another one multiclasses), as !test's `level` makes them:
//   only added, never taken away; abilities and items untouched.

#include <windows.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <string>
#include <utility>
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
#include "saves.h"
#include "rules/abilities.h"
#include "rules/class_features.h"
#include "rules/feats2024.h"
#include "rules/nwn_conversion.h"
#include "rules/progression.h"
#include "rules/test_profile.h"
#include "rules/weapons.h"

void SetTestAbilityScores(void* creature, const int scores[6]);  // rules_abilities.cpp
void ForgetQuickbarPlacements(void* creature);                    // rules_quickbar.cpp

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

// !spawn <blueprint> [ally] [dr] - dr: damage reduction 10/+5 (Monk part 1: Empowered
// Strikes gets through it, a Monk 5's fists don't).
std::vector<std::string> SpawnCommand(void* creature, const std::string& args) {
  std::string blueprint = args, rest;
  const size_t space = args.find(' ');
  if (space != std::string::npos) {
    blueprint = args.substr(0, space);
    rest = args.substr(space + 1);
  }
  blueprint = Lower(blueprint);
  if (!IsResRef(blueprint)) {
    return {"nwn5e: !spawn <creature blueprint> [ally] [dr] - e.g. !spawn nw_gntfrost (Frost Giant, Huge), "
            "nw_gnthill (Hill Giant, Large), nw_orcb (Orc); ally: as your henchman; dr: damage reduction 10/+5."};
  }
  const bool ally = Lower(rest).find("ally") != std::string::npos;
  const bool reduction = Lower(rest).find("dr") != std::string::npos;
  const uint32_t id = ObjectIdOf(StatsOf(creature));
  std::string join = ally ? "ChangeFaction(o, OBJECT_SELF); AddHenchman(OBJECT_SELF, o); "
                          : "ChangeToStandardFaction(o, STANDARD_FACTION_HOSTILE); ";
  if (reduction) {
    join += "ApplyEffectToObject(DURATION_TYPE_PERMANENT, SupernaturalEffect(EffectDamageReduction(10, "
            "DAMAGE_POWER_PLUS_FIVE)), o); ";
  }
  const std::string code =
      "float f = GetFacing(OBJECT_SELF); vector p = GetPosition(OBJECT_SELF) + AngleToVector(f) * 5.0; "
      "object o = CreateObject(OBJECT_TYPE_CREATURE, \"" + blueprint + "\", Location(GetArea(OBJECT_SELF), p, f + 180.0)); "
      "if (GetIsObjectValid(o)) { " + join + "SendMessageToPC(OBJECT_SELF, \"nwn5e: spawned \" + GetName(o) + \"" +
      (ally ? " (your henchman)" : " (hostile)") + (reduction ? ", damage reduction 10/+5" : "") +
      ".\"); } else SendMessageToPC(OBJECT_SELF, \"nwn5e: no creature blueprint \\\"" + blueprint + "\\\".\");";
  RunScriptChunk(code.c_str(), id);
  Log("spawn: 0x%08X asked for %s%s%s", id, blueprint.c_str(), ally ? " (ally)" : "", reduction ? " (DR 10/+5)" : "");
  return {};
}

std::vector<std::string> ProneCommand(void* creature, const std::string& args) {
  int seconds = atoi(Trim(args).c_str());
  if (seconds <= 0) seconds = 6;
  if (seconds > 60) seconds = 60;
  const uint32_t id = ObjectIdOf(StatsOf(creature));
  char code[200];
  sprintf_s(code, "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectKnockdown(), OBJECT_SELF, %d.0);", seconds);
  RunScriptChunk(code, id);
  Log("test: 0x%08X knocked down for %d s (!prone)", id, seconds);
  return {"nwn5e: you are knocked down for " + std::to_string(seconds) + " s (!prone) - attack rolls against you "
          "from within 5 ft have Advantage, unless something says otherwise (Elusive)."};
}

std::vector<std::string> SaveCommand(void* creature, const std::string& args) {
  const std::string text = Lower(Trim(args));
  const size_t space = text.find(' ');
  const std::optional<rules::Ability> ability =
      space == std::string::npos ? std::nullopt : rules::ParseAbility(text.substr(0, space).c_str());
  const int dc = space == std::string::npos ? 0 : atoi(text.substr(space + 1).c_str());
  if (!ability || dc <= 0 || dc > 60) return {"nwn5e: !save <str | dex | con | int | wis | cha> <dc> - e.g. !save dex 25"};
  using SaveRollFn = uint8_t (*)(void* creature, uint8_t save, uint16_t dc, uint8_t type, uint32_t versus,
                                 int feedback, uint16_t feat, int from_combat);
  static const auto save_roll = reinterpret_cast<SaveRollFn>(EngineExport(ee::kSavingThrowRoll));
  if (!save_roll) return {"nwn5e: no save roll export."};
  const uint32_t id = ObjectIdOf(StatsOf(creature));
  const uint8_t slot = *ability == rules::Ability::kDexterity ? ee::kSaveReflex
                       : *ability == rules::Ability::kStrength || *ability == rules::Ability::kConstitution
                           ? ee::kSaveFortitude
                           : ee::kSaveWill;
  RememberSaveDc2024(id, dc);  // a 2024 DC: not converted
  SetForcedSaveAbility(*ability);
  const uint8_t result = save_roll(creature, slot, static_cast<uint16_t>(dc), 0, id, 1, 0, 0);
  SetForcedSaveAbility(std::nullopt);
  const std::string name = rules::AbilityName(*ability);
  Log("test: 0x%08X %s save DC %d (!save): %s", id, name.c_str(), dc,
      result == 0 ? "failed" : result == 2 ? "immune" : "made");
  return {"nwn5e: test " + name + " save, DC " + std::to_string(dc) + ": " +
          (result == 0 ? "failed" : result == 2 ? "immune" : "made") + "."};
}

std::vector<std::string> ReflexCommand(void* creature, const std::string& args) {
  const std::string text = Trim(args);
  const size_t space = text.find(' ');
  const int dc = atoi(text.c_str());
  const int damage = space == std::string::npos ? 0 : atoi(text.substr(space + 1).c_str());
  if (dc <= 0 || dc > 60 || damage <= 0 || damage > 500) {
    return {"nwn5e: !reflex <dc> <damage> - a Reflex-halved fire hit on you, as an area spell's (e.g. !reflex 15 20)"};
  }
  const uint32_t id = ObjectIdOf(StatsOf(creature));
  RememberSaveDc2024(id, dc);  // a 2024 DC: not converted
  char code[600];
  sprintf_s(code,
            "int d = GetReflexAdjustedDamage(%d, OBJECT_SELF, %d, SAVING_THROW_TYPE_FIRE, OBJECT_SELF);"
            " SetLocalInt(OBJECT_SELF, \"nwn5e_test_reflex\", d);"
            " SendMessageToPC(OBJECT_SELF, \"nwn5e: test Reflex hit - %d fire damage, DC %d: you take \" +"
            " IntToString(d) + \".\");"
            " if (d > 0) ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectDamage(d, DAMAGE_TYPE_FIRE), OBJECT_SELF);",
            damage, dc, damage, dc);
  engine::SetLocalInt(creature, "nwn5e_test_reflex", -1);
  RunScriptChunk(code, id);
  Log("test: 0x%08X a Reflex-halved hit, %d damage DC %d (!reflex): %d taken", id, damage, dc,
      engine::GetLocalInt(creature, "nwn5e_test_reflex"));
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

// The engine's own level-ups in `nwn_class` up to `level` in it (the XP for the
// new total first); the chat line saying what it reached.
std::string AddLevels(void* stats, int nwn_class, int level) {
  const uint32_t id = ObjectIdOf(stats);
  const int had = ClassLevel(stats, nwn_class);
  const int add = level - had;
  if (add > 0) {
    const int total = TotalLevel(stats) + add;
    const uint32_t xp = g_config.progression == rules::RuleVariant::kFaithful ? rules::FaithfulXpForLevel(total)
                                                                              : rules::NwnXpForLevel(total);
    char code[300];
    sprintf_s(code,
              "if (GetXP(OBJECT_SELF) < %u) SetXP(OBJECT_SELF, %u); int i;"
              " for (i = 0; i < %d; i++) if (!LevelUpHenchman(OBJECT_SELF, %d, TRUE)) break;",
              xp, xp, add, nwn_class);
    RunScriptChunk(code, id);  // the automatic level-ups take the 2024 feats (rules_feats.cpp)
  }
  return std::string("  class: ") + ClassName(nwn_class) + " " + std::to_string(ClassLevel(stats, nwn_class)) +
         (add <= 0 ? " (already there)"
          : had == 0 && TotalLevel(stats) > level ? " (a new class: multiclassed)"
                                                  : " (" + std::to_string(add) + " levels added)");
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

// The Fighting Style feats taken away when no class has the Fighting Style
// feature; "" when none was there or one is allowed.
std::string DropFightingStyles(void* stats) {
  for (const rules::NwnClassSlot& slot : ReadClassSlots(stats)) {
    const auto character_class = rules::ClassFromNwnId(slot.nwn_class_id);
    if (character_class && rules::HasFightingStyleFeature2024(*character_class, slot.levels)) return {};
  }
  static const char* const kStyleNames[] = {"Archery", "Defense", "Great Weapon Fighting", "Two-Weapon Fighting"};
  std::string removed;
  for (int feat = rules::kFeatFightingStyleArchery; feat <= rules::kFeatFightingStyleTwoWeapon; ++feat) {
    if (!engine::HasFeat(stats, feat)) continue;
    engine::RemoveFeat(stats, feat);
    removed += (removed.empty() ? "" : ", ") + std::string(kStyleNames[feat - rules::kFeatFightingStyleArchery]);
  }
  if (removed.empty()) return {};
  Log("test: 0x%08X Fighting Style removed (no class with the Fighting Style feature): %s", ObjectIdOf(stats),
      removed.c_str());
  return "  fighting style removed (no Fighter, Paladin 2 or Ranger 2): " + removed;
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

std::vector<std::string> LevelCommand(void* creature, const std::string& args) {
  void* stats = StatsOf(creature);
  if (!stats) return {};
  const std::string text = Lower(Trim(args));
  const size_t space = text.find(' ');
  const int level = atoi(text.c_str());
  int nwn_class = -1;
  if (space != std::string::npos) {
    const std::string name = Trim(text.substr(space + 1));
    for (int i = 0; i < static_cast<int>(sizeof(kClassNames) / sizeof(kClassNames[0])); ++i) {
      if (Lower(kClassNames[i]) == name) nwn_class = i;
    }
    if (nwn_class < 0) {
      return {"nwn5e: no class \"" + name + "\" - barbarian, bard, cleric, druid, fighter, monk, paladin, ranger, "
              "rogue, sorcerer or wizard."};
    }
  } else {
    const std::vector<rules::NwnClassSlot> slots = ReadClassSlots(stats);
    if (!slots.empty()) nwn_class = slots[0].nwn_class_id;
  }
  if (level < 1 || level > 40 || nwn_class < 0) {
    return {"nwn5e: !level <n> [class] - levels up to n in the class (your first class when none is named), e.g. "
            "!level 20 or !level 3 fighter. Levels are only added."};
  }
  const int had = ClassLevel(stats, nwn_class);
  if (had >= level) {
    return {"nwn5e: you are already " + std::string(ClassName(nwn_class)) + " " + std::to_string(had) +
            " - levels are only added, never taken away."};
  }
  const std::string line = AddLevels(stats, nwn_class, level);
  Log("test: 0x%08X !level %d %s:%s", ObjectIdOf(stats), level, ClassName(nwn_class), line.c_str());
  return {"nwn5e: !level -" + line.substr(1)};
}

// --- !test profiles (#82) -------------------------------------------------

// A profile that builds a character starts from level 1: the line to show.
std::string ResetToLevelOne(void* stats, uint32_t id) {
  const int before = TotalLevel(stats);
  if (before <= 1) return "  level: 1 (a fresh start)";
  const uint32_t xp = Field<uint32_t>(stats, ee::kStatsExperience);
  RunScriptChunk("SetXP(OBJECT_SELF, 0);", id);
  const int after = TotalLevel(stats);
  if (after > 1) {
    SetField<uint32_t>(stats, ee::kStatsExperience, xp);  // no level lost: its XP back
    Log("test: 0x%08X reset to level 1 failed - still level %d after SetXP 0 (XP %u put back)", id, after, xp);
    return "  level: couldn't go down from " + std::to_string(before) + " (still " + std::to_string(after) +
           ") - the profile only adds levels";
  }
  Log("test: 0x%08X reset from level %d to 1 (SetXP 0)", id, before);
  return "  level: " + std::to_string(before) + " -> 1 (a fresh start), then the profile's";
}

// The switches a profile may set (rules::TestSwitchRange: their ranges), as
// g_config holds them. Their ini values are kept at start-up and put back by
// every !test <profile> before its own.
struct SwitchField {
  const char* name;
  int (*get)();
  void (*set)(int);
};

const SwitchField kSwitchFields[] = {
    {"attack_trace", [] { return g_config.debug_attack_trace ? 1 : 0; },
     [](int value) { g_config.debug_attack_trace = value != 0; }},
    {"attacks_per_round", [] { return g_config.debug_attacks_per_round; },
     [](int value) { g_config.debug_attacks_per_round = value; }},
    {"creature_hp_multiplier", [] { return g_config.debug_creature_hp_multiplier; },
     [](int value) { g_config.debug_creature_hp_multiplier = value; }},
    {"creature_min_hp", [] { return g_config.debug_creature_min_hp; },
     [](int value) { g_config.debug_creature_min_hp = value; }},
    {"rest_clock", [] { return g_config.rest_clock ? 1 : 0; }, [](int value) { g_config.rest_clock = value != 0; }},
};
constexpr size_t kSwitchCount = sizeof(kSwitchFields) / sizeof(kSwitchFields[0]);
int g_ini_switches[kSwitchCount] = {};

// Every switch back to its ini value, then the profile's: the lines to show.
std::vector<std::string> ApplySwitches(uint32_t id, const std::vector<rules::TestSwitch>& wanted) {
  std::string set, back;
  for (size_t i = 0; i < kSwitchCount; ++i) {
    int value = g_ini_switches[i];
    bool from_profile = false;
    for (const rules::TestSwitch& test_switch : wanted) {
      if (test_switch.name == kSwitchFields[i].name) {
        value = test_switch.value;
        from_profile = true;
      }
    }
    const std::string text = std::string(kSwitchFields[i].name) + "=" + std::to_string(value);
    if (from_profile) {
      set += (set.empty() ? "" : ", ") + text;
    } else if (kSwitchFields[i].get() != value) {
      back += (back.empty() ? "" : ", ") + text;
    }
    kSwitchFields[i].set(value);
  }
  std::vector<std::string> lines;
  if (!set.empty()) {
    lines.push_back("  switches for this session: " + set + " (the ini's come back with the next !test <profile>)");
  }
  if (!back.empty()) lines.push_back("  switches back to the ini's: " + back);
  if (!set.empty() || !back.empty()) {
    Log("test: 0x%08X switches set %s; back to the ini's %s", id, set.empty() ? "none" : set.c_str(),
        back.empty() ? "none" : back.c_str());
  }
  return lines;
}

// "!test" alone: the profiles in data\tests, each with its `name` line.
std::vector<std::string> ListProfiles() {
  const std::wstring folder = DllDirectory() + L"\\data\\tests";
  std::vector<std::pair<std::string, std::string>> profiles;  // (profile, what it is for)
  WIN32_FIND_DATAW found;
  HANDLE handle = FindFirstFileW((folder + L"\\*.txt").c_str(), &found);
  if (handle != INVALID_HANDLE_VALUE) {
    do {
      const std::wstring file = found.cFileName;
      if (file.size() <= 4 || _wcsicmp(file.c_str() + file.size() - 4, L".txt") != 0) continue;
      std::string profile;
      for (wchar_t c : file.substr(0, file.size() - 4)) profile += c < 128 ? static_cast<char>(c) : '?';
      profile = Lower(profile);
      if (!rules::IsTestProfileName(profile)) continue;
      std::string text;
      ReadTextFile(folder + L"\\" + file, &text);
      profiles.push_back({profile, rules::ParseTestProfile(text).name});
    } while (FindNextFileW(handle, &found));
    FindClose(handle);
  }
  if (profiles.empty()) return {"nwn5e: !test - no profiles in data\\tests next to nwn5e.dll."};
  std::sort(profiles.begin(), profiles.end());
  std::vector<std::string> lines = {"nwn5e: !test <profile> - sets you up for a test (data\\tests\\<profile>.txt):"};
  for (const auto& profile : profiles) {
    lines.push_back("  " + profile.first + (profile.second.empty() ? "" : " - " + profile.second));
  }
  return lines;
}

std::vector<std::string> TestCommand(void* creature, const std::string& args) {
  void* stats = StatsOf(creature);
  if (!stats) return {};
  const std::string name = Lower(Trim(args));
  if (name.empty()) return ListProfiles();
  if (!rules::IsTestProfileName(name)) {
    return {"nwn5e: !test <profile> - letters, digits, '-' and '_' (!test alone lists the profiles)."};
  }
  const std::wstring path = DllDirectory() + L"\\data\\tests\\" + std::wstring(name.begin(), name.end()) + L".txt";
  std::string text;
  if (!ReadTextFile(path, &text)) {
    return {"nwn5e: !test - no profile \"" + name + "\" (data\\tests\\" + name + ".txt); !test alone lists them."};
  }
  const rules::TestProfile profile = rules::ParseTestProfile(text);
  if (!profile.errors.empty()) {
    std::vector<std::string> lines = {"nwn5e: !test " + name + " - data\\tests\\" + name +
                                      ".txt has errors, nothing applied:"};
    for (const std::string& error : profile.errors) lines.push_back("  " + error);
    return lines;
  }
  const uint32_t id = ObjectIdOf(stats);
  std::vector<std::string> lines = {"nwn5e: !test " + name +
                                    (profile.name.empty() ? std::string() : " - " + profile.name)};
  // 1. Species: the race and its model.
  if (profile.race >= 0) {
    SetField<uint16_t>(stats, ee::kStatsRace, static_cast<uint16_t>(profile.race));
    RunScriptChunk(("SetCreatureAppearanceType(OBJECT_SELF, " + std::to_string(profile.race) + ");").c_str(), id);
    // The size the weapon, perception and shove rules read (a gnome's or a halfling's is Small).
    SetField<int32_t>(creature, ee::kCreatureSize,
                      profile.race == 2 || profile.race == 3 ? ee::kCreatureSizeSmall : ee::kCreatureSizeMedium);
    lines.push_back(std::string("  species: ") + kRaceNames[profile.race]);
  }
  // 1a. A profile that builds a character starts from level 1.
  if (rules::TestProfileBuildsCharacter(profile)) lines.push_back(ResetToLevelOne(stats, id));
  // 1a'. The profile's scores before the levels: a new class needs 13 in its main ability (CM2).
  if (profile.has_abilities) SetTestAbilityScores(creature, profile.abilities);
  // 1b. The first class swapped (before the levels, which may then add some in it).
  if (profile.swap_class >= 0) lines.push_back(SwapFirstClass(stats, profile.swap_class));
  // 2. Level: the XP for it, then the engine's own level-ups in the class.
  if (profile.nwn_class >= 0 && profile.level > 0) lines.push_back(AddLevels(stats, profile.nwn_class, profile.level));
  // 2a. More classes (`levels`), each up to its level.
  for (const rules::TestLevels& more : profile.levels) lines.push_back(AddLevels(stats, more.nwn_class, more.level));
  // 2b. Experience only: the level-up screen makes the levels.
  if (profile.xp > 0) {
    char code[200];
    sprintf_s(code, "if (GetXP(OBJECT_SELF) < %d) SetXP(OBJECT_SELF, %d);", profile.xp, profile.xp);
    RunScriptChunk(code, id);
    lines.push_back("  xp: " + std::to_string(profile.xp) + " (level up on the level-up screen)");
  }
  // 2c. A Fighting Style without a class that has the feature (one left from a swapped Fighter) goes.
  const std::string styles = DropFightingStyles(stats);
  if (!styles.empty()) lines.push_back(styles);
  // 3. Ability scores (through the ability sync) - again, after the levels' own increases.
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
  // 5b. A built character's quickbar: nwn5e's actions placed afresh, on empty slots.
  if (rules::TestProfileBuildsCharacter(profile)) {
    ForgetQuickbarPlacements(creature);
    lines.push_back("  quickbar: nwn5e's class actions go back on empty slots");
  }
  // 6. Test switches for this session: the ini's values back, then the profile's.
  for (const std::string& line : ApplySwitches(id, profile.switches)) lines.push_back(line);
  // 7. What the profile can't do itself.
  for (const std::string& note : profile.notes) lines.push_back("  note: " + note);
  Log("test: 0x%08X applied data\\tests\\%s.txt (%s)", id, name.c_str(), profile.name.c_str());
  return lines;
}

}  // namespace

void InstallTestCommands() {
  if (!g_config.debug_test_commands) return;
  for (size_t i = 0; i < kSwitchCount; ++i) {
    g_ini_switches[i] = kSwitchFields[i].get();
    int low = 0, high = 0;
    if (!rules::TestSwitchRange(kSwitchFields[i].name, &low, &high)) {
      Log("note: !test switch %s has no range in rules::TestSwitchRange (a profile can't set it)",
          kSwitchFields[i].name);
    }
  }
  AddChatCommand("!give", &GiveCommand);
  AddChatCommand("!feat", &FeatCommand);
  AddChatCommand("!test", &TestCommand);
  AddChatCommand("!export", &ExportCommand);
  AddChatCommand("!spawn", &SpawnCommand);
  AddChatCommand("!prone", &ProneCommand);
  AddChatCommand("!save", &SaveCommand);
  AddChatCommand("!reflex", &ReflexCommand);
  AddChatCommand("!level", &LevelCommand);
  Log("config: [debug] test_commands=1 - !test [profile] (data\\tests), !give <blueprint>, !feat add / remove <feat>, "
      "!export, !spawn <blueprint> [ally], !prone [s], !save <ability> <dc>, !reflex <dc> <damage>, !level <n> [class]");
}
