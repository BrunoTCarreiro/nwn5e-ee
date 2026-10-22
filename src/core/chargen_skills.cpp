// C5 (docs/CHARGEN.md): the creation screen's Skills page as the 2024 pick,
// with `skills` faithful. The page shows one row per 2024 skill (served
// skills.2da rows: rules::SkillsPageRow; nwn5e's rows 28-34 for the skills NWN
// has none for) and the class tables mark the 2024 class lists
// (tools/data/class_skills.py); four client functions, found by anchors,
// play the 2024 rules:
// - the level's skill points (client 0x7ec0c0, the page's constructor and
//   Recommended, the level-up's start): at level 1 the page's picks — the
//   class's count, +1 for a Human (Skillful) or an Elf (Keen Senses) — and the
//   background's skills set to rank 1 before the page copies its starting
//   ranks, so "-" can't take them back; at later levels none (2024 gives no
//   skill points; later choices stay with !skills until C7);
// - "+" (0x7f7a00): 1 point and rank 1 for any skill, taken only if
//   rules::CanPickOnSkillsPage says the picks still fit;
// - "-" (0x7ee3e0): 1 point back, never below the background's rank;
// - the package's picks (0x7eb300: Recommended, and OK without Customize):
//   rules::RecommendedSkillsPagePicks.
// A rank on a page row is the proficiency: rules_skills.cpp reads a new
// character's 2024 skills from its ranks. Skilled (chosen on the Feats page,
// after this one), Expertise and multiclass picks stay with !skills.
// Any anchor missing: nothing is hooked, the rows stay NWN's, and the page is
// the game's own.

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <string>

#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "rules/backgrounds.h"
#include "rules/nwn_conversion.h"
#include "rules/skills.h"

int CreationBackgroundFeat();  // rules_abilities.cpp

namespace {

using PointsFn = void (*)(void* stats);
using AddRankFn = int (*)(void* stats, uint16_t skill);
using RemoveRankFn = void (*)(void* stats, uint16_t skill, uint8_t min_rank);
using PackageSkillsFn = void (*)(void* stats, const void* table_name);
using SetRankFn = void (*)(void* stats, uint16_t skill, int8_t rank);
using ClientByteFn = uint8_t (*)(void* stats);

PointsFn g_points = nullptr;
AddRankFn g_add_rank = nullptr;
RemoveRankFn g_remove_rank = nullptr;
PackageSkillsFn g_package_skills = nullptr;
SetRankFn g_set_rank = nullptr;
ClientByteFn g_get_level = nullptr;  // the level after the level-up being made
ClientByteFn g_get_class = nullptr;  // the class being levelled
bool g_installed = false;
LogOnce g_logged(64);

bool Active() { return g_installed && g_config.skills == rules::RuleVariant::kFaithful; }

rules::Species2024 SpeciesOfRace(int race) {
  if (race == ee::kRaceHuman) return rules::Species2024::kHuman;
  if (race == ee::kRaceElf) return rules::Species2024::kElf;
  return rules::Species2024::kOther;
}

// The page's choices for this character at level 1.
struct Page {
  rules::CharacterClass character_class = rules::CharacterClass::kFighter;
  rules::Species2024 species = rules::Species2024::kOther;
  rules::SkillAllowance allowance;
  rules::SkillSet background = 0;  // the background's skills (tools left out)
  bool valid = false;
};

Page PageFor(void* stats) {
  Page page;
  const auto character_class = rules::ClassFromNwnId(g_get_class(stats));
  if (!character_class) return page;
  page.character_class = *character_class;
  page.species = SpeciesOfRace(Field<uint16_t>(stats, ee::kClientStatsRace));
  page.allowance = rules::SkillsPageAllowance(page.character_class, page.species);
  if (const auto choice = rules::BackgroundFromFeat(CreationBackgroundFeat())) {
    if (const rules::BackgroundInfo* info = rules::BackgroundData(choice->background)) {
      for (int i = 0; i < rules::kSkill2024Count; ++i) {
        const auto skill = static_cast<rules::Skill2024>(i);
        if (rules::Has(info->skills, skill) && !rules::IsTool(skill)) page.background |= rules::SkillBit(skill);
      }
    }
  }
  page.valid = true;
  return page;
}

// The creation screen at level 1 (the level-up screen at 1 is the same).
bool AtLevelOne(void* stats) { return Active() && stats && g_get_level(stats) == 1; }

int8_t* Ranks(void* stats) { return Field<int8_t*>(stats, ee::kClientStatsSkillRanks); }
int Rows() { return std::min<int>(rules::kNwnSkillCount, engine::SkillRowCount()); }

// The page's picks so far: its rows with a rank, the background's left out.
rules::SkillSet Picked(void* stats, rules::SkillSet background) {
  const int8_t* ranks = Ranks(stats);
  rules::SkillSet picked = 0;
  if (!ranks) return picked;
  for (int row = 0; row < Rows(); ++row) {
    if (ranks[row] <= 0 || !rules::IsSkillsPageRow(row)) continue;
    picked |= rules::SkillBit(*rules::Skill2024ForNwnSkill(row));
  }
  return picked & ~background;
}

void SetBackgroundRanks(void* stats, rules::SkillSet background) {
  for (int i = 0; i < rules::kSkill2024Count; ++i) {
    const auto skill = static_cast<rules::Skill2024>(i);
    const int row = rules::SkillsPageRow(skill);
    if (rules::Has(background, skill) && row >= 0 && row < Rows()) g_set_rank(stats, static_cast<uint16_t>(row), 1);
  }
}

uint16_t PointsLeft(void* stats) { return Field<uint16_t>(stats, ee::kClientStatsSkillPoints); }
void SetPointsLeft(void* stats, int points) {
  SetField<uint16_t>(stats, ee::kClientStatsSkillPoints, static_cast<uint16_t>(points < 0 ? 0 : points));
}

std::string Names(rules::SkillSet set) {
  std::string names;
  for (int i = 0; i < rules::kSkill2024Count; ++i) {
    if (!rules::Has(set, static_cast<rules::Skill2024>(i))) continue;
    names += (names.empty() ? "" : ", ") + std::string(rules::Skill2024Name(static_cast<rules::Skill2024>(i)));
  }
  return names.empty() ? "none" : names;
}

void Points(void* stats) {
  if (!Active() || !stats) {
    g_points(stats);
    return;
  }
  const int level = g_get_level(stats);
  if (level > 1) {
    if (g_logged.First(LogOnce::Key(1, 0, 0))) Log("skills page: no skill points after level 1 (2024; !skills for later picks)");
    return;
  }
  const Page page = PageFor(stats);
  if (level != 1 || !page.valid) {
    g_points(stats);
    return;
  }
  const int points = rules::SkillsPagePoints(page.allowance);
  SetPointsLeft(stats, PointsLeft(stats) + points);
  SetBackgroundRanks(stats, page.background);
  if (g_logged.First(LogOnce::Key(2, static_cast<uint32_t>(page.background), points))) {
    Log("skills page: %d pick(s) (class %d, species %d); the background's %s taken", points,
        static_cast<int>(page.character_class), static_cast<int>(page.species), Names(page.background).c_str());
  }
}

int AddRank(void* stats, uint16_t skill) {
  if (!AtLevelOne(stats)) return g_add_rank(stats, skill);
  const Page page = PageFor(stats);
  int8_t* ranks = Ranks(stats);
  const auto skill2024 = rules::Skill2024ForNwnSkill(skill);
  if (!page.valid || !ranks || skill >= Rows() || !skill2024 || ranks[skill] >= 1 || PointsLeft(stats) < 1) return 0;
  const rules::SkillSet picked = Picked(stats, page.background);
  if (!rules::CanPickOnSkillsPage(page.allowance, page.background, picked, *skill2024)) {
    Log("skills page: %s not taken - picks so far: %s (the class's list, then a species pick)",
        rules::Skill2024Name(*skill2024), Names(picked).c_str());
    return 0;
  }
  g_set_rank(stats, skill, 1);
  SetPointsLeft(stats, PointsLeft(stats) - 1);
  return 1;
}

void RemoveRank(void* stats, uint16_t skill, uint8_t min_rank) {
  if (!AtLevelOne(stats)) {
    g_remove_rank(stats, skill, min_rank);
    return;
  }
  const Page page = PageFor(stats);
  int8_t* ranks = Ranks(stats);
  const auto skill2024 = rules::Skill2024ForNwnSkill(skill);
  if (!ranks || skill >= Rows() || ranks[skill] <= static_cast<int8_t>(min_rank) || ranks[skill] <= 0) return;
  if (skill2024 && rules::Has(page.background, *skill2024)) return;  // the background's: locked
  g_set_rank(stats, skill, static_cast<int8_t>(ranks[skill] - 1));
  SetPointsLeft(stats, PointsLeft(stats) + 1);
}

void PackageSkills(void* stats, const void* table_name) {
  if (!AtLevelOne(stats)) {
    g_package_skills(stats, table_name);
    return;
  }
  const Page page = PageFor(stats);
  if (!page.valid) return;
  SetBackgroundRanks(stats, page.background);
  const rules::SkillSet recommended =
      rules::RecommendedSkillsPagePicks(page.character_class, page.species, page.background);
  int8_t* ranks = Ranks(stats);
  for (int i = 0; i < rules::kSkill2024Count && ranks; ++i) {
    const auto skill = static_cast<rules::Skill2024>(i);
    const int row = rules::SkillsPageRow(skill);
    if (!rules::Has(recommended, skill) || row < 0 || row >= Rows() || ranks[row] >= 1 || PointsLeft(stats) < 1) {
      continue;
    }
    if (!rules::CanPickOnSkillsPage(page.allowance, page.background, Picked(stats, page.background), skill)) continue;
    g_set_rank(stats, static_cast<uint16_t>(row), 1);
    SetPointsLeft(stats, PointsLeft(stats) - 1);
  }
  Log("skills page: Recommended - %s (with the background's %s)", Names(Picked(stats, page.background)).c_str(),
      Names(page.background).c_str());
}

// The function containing the one match of `pattern` in .text, 0 (logged) if none.
uintptr_t FunctionOf(const char* what, const char* pattern) {
  const uintptr_t match = FindPattern(what, pattern);
  return match ? FunctionEntry(match) : 0;
}

}  // namespace

void InstallSkillsPage() {
  if (g_config.skills != rules::RuleVariant::kFaithful) return;
  // The four functions, each by an instruction found once in the client's code.
  const uintptr_t points = FunctionOf("skills page: the level's skill points",
                                      "0F B7 4E 50 0F AF C8 66 01 8B 0E 01 00 00");
  const uintptr_t add = FunctionOf("skills page: +", "66 83 C1 02 66 29 8F 0E 01 00 00");
  const uintptr_t remove = FunctionOf("skills page: -", "66 83 C0 02 66 89 83 0E 01 00 00");
  const uintptr_t package = FunctionOf("skills page: the package's picks", "66 44 01 AB 0E 01 00 00");
  // Inside "+": the class being levelled, the level, the rank setter.
  const CodeRange add_code = add ? FunctionAt(add) : CodeRange();
  const uintptr_t get_class_call =
      add ? FindPatternIn("skills page: GetClass", add_code, "48 8B CF E8 ?? ?? ?? ?? 0F B6 D0 48 8B 05") : 0;
  const uintptr_t get_level_call =
      add ? FindPatternIn("skills page: GetLevel", add_code, "48 8B CF 8B D8 E8 ?? ?? ?? ?? 02 D8") : 0;
  const uintptr_t set_rank_call =
      add ? FindPatternIn("skills page: the rank setter", add_code, "41 FE C0 0F B7 D6 48 8B CF E8") : 0;
  g_get_class = reinterpret_cast<ClientByteFn>(get_class_call ? CallTarget(get_class_call + 3) : 0);
  g_get_level = reinterpret_cast<ClientByteFn>(get_level_call ? CallTarget(get_level_call + 5) : 0);
  g_set_rank = reinterpret_cast<SetRankFn>(set_rank_call ? CallTarget(set_rank_call + 9) : 0);
  if (!points || !add || !remove || !package || !g_get_class || !g_get_level || !g_set_rank) {
    Log("SKIP skills page: a client function not found - the Skills page stays NWN's");
    return;
  }
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  Log("skills page: points rva 0x%zx, + 0x%zx, - 0x%zx, package 0x%zx, rank setter 0x%zx", points - base, add - base,
      remove - base, package - base, reinterpret_cast<uintptr_t>(g_set_rank) - base);
  const bool hooked =
      HookAddress("client skill points (C5: the 2024 picks at level 1, none later)", points,
                  reinterpret_cast<void*>(&Points), reinterpret_cast<void**>(&g_points)) &&
      HookAddress("client Skills page + (C5: a 2024 pick)", add, reinterpret_cast<void*>(&AddRank),
                  reinterpret_cast<void**>(&g_add_rank)) &&
      HookAddress("client Skills page - (C5)", remove, reinterpret_cast<void*>(&RemoveRank),
                  reinterpret_cast<void**>(&g_remove_rank)) &&
      HookAddress("client package skills (C5: Recommended)", package, reinterpret_cast<void*>(&PackageSkills),
                  reinterpret_cast<void**>(&g_package_skills));
  if (!hooked) return;
  g_installed = true;
  ServeConditionalRows("skills2024");
  Log("skills page: the 2024 pick - the class's skills, a species pick, the background's taken (skills faithful)");
}
