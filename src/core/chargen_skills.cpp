// C5 / C7b (docs/CHARGEN.md): the Skills page as the 2024 pick, with `skills`
// faithful — at creation and at every level-up. The page shows one row per
// 2024 skill (served skills.2da rows: rules::SkillsPageRow; nwn5e's rows
// 28-34 for the skills NWN has none for) and the class tables mark the 2024
// class lists (tools/data/class_skills.py). A level's floor is the ranks it
// starts from; rank 0 -> 1 above it is a new skill, 1 -> 2 Expertise. Four
// client functions, found by anchors, play the 2024 rules:
// - the level's skill points (client 0x7ec0c0: the level-up's start, the
//   page's constructor, Recommended): the level's picks — at level 1 the
//   class's count +1 for a Human (Skillful) or an Elf (Keen Senses), and the
//   background's skills set to rank 1 before the page copies its starting
//   ranks (so "-" can't take them back); after it what the level adds
//   (rules::LevelSkillPicks2024: multiclassing into a Bard, Ranger or Rogue,
//   Barbarian 3, Bard 3) — plus its Expertise (Rogue 1 and 6, Bard 2 and 9,
//   Ranger 2 and 9, Wizard 2);
// - "+" (0x7f7a00): 1 point; a new skill only if rules::CanPickOnSkillsPage
//   says the picks still fit, Expertise only by rules::CanPickExpertise;
// - "-" (0x7ee3e0): 1 point back, never below the floor;
// - the package's picks (0x7eb300: Recommended, and OK without Customize):
//   the class's usual skills, then Expertise in them.
// A character whose ranks aren't the 2024 record (an older character's NWN
// ranks: rules::RanksAre2024) gets no points after level 1 — !skills.
// The ranks are the record: rules_skills.cpp reads proficiency (1) and
// Expertise (2) from them. Skilled (its Feats page comes after this one)
// stays with !skills until C8. Any anchor missing: nothing is hooked, the
// rows stay NWN's, and the page is the game's own.

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

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

int8_t* Ranks(void* stats) { return Field<int8_t*>(stats, ee::kClientStatsSkillRanks); }
int Rows() { return std::min<int>(rules::kNwnSkillCount, engine::SkillRowCount()); }
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

// The level being made on the page (creation or a level-up): its picks and
// the ranks it started from (its floor) — anything above the floor is this
// level's pick, rank 0 -> 1 a new skill, 1 -> 2 Expertise.
struct Level {
  void* stats = nullptr;
  uint64_t key = 0;  // character level, class, its level, race, background: a new key, a new floor
  bool active = false;
  int level = 0;
  rules::CharacterClass character_class = rules::CharacterClass::kFighter;
  rules::SkillAllowance skills;  // this level's new skills (level 1: the page's allowance)
  rules::LevelSkillPicks expertise;
  rules::SkillSet background = 0;  // level 1: the background's skills, taken and locked
  rules::SkillSet known = 0;       // proficient before this level
  int8_t floor[rules::kNwnSkillCount] = {};
  int points = 0;
};
Level g_level;

rules::SkillSet BackgroundSkillsAtCreation() {
  rules::SkillSet skills = 0;
  if (const auto choice = rules::BackgroundFromFeat(CreationBackgroundFeat())) {
    if (const rules::BackgroundInfo* info = rules::BackgroundData(choice->background)) {
      for (int i = 0; i < rules::kSkill2024Count; ++i) {
        const auto skill = static_cast<rules::Skill2024>(i);
        if (rules::Has(info->skills, skill) && !rules::IsTool(skill)) skills |= rules::SkillBit(skill);
      }
    }
  }
  return skills;
}

void SetBackgroundRanks(void* stats, rules::SkillSet background) {
  const int8_t* ranks = Ranks(stats);
  for (int i = 0; ranks && i < rules::kSkill2024Count; ++i) {
    const auto skill = static_cast<rules::Skill2024>(i);
    const int row = rules::SkillsPageRow(skill);
    if (rules::Has(background, skill) && row >= 0 && row < Rows() && ranks[row] < 1) {
      g_set_rank(stats, static_cast<uint16_t>(row), 1);
    }
  }
}

// The level the page makes, worked out again when anything defining it changes.
void Begin(void* stats) {
  const int level = g_get_level(stats);
  const uint8_t slot = Field<uint8_t>(stats, ee::kClientStatsClassBeingLevelled);
  const auto character_class = rules::ClassFromNwnId(g_get_class(stats));
  const int class_level =
      slot < ee::kClientStatsClassSlots
          ? Field<uint8_t>(stats, slot * ee::kClientStatsClassSlotSize + ee::kClientStatsClassSlotLevel)
          : 0;
  const int race = Field<uint16_t>(stats, ee::kClientStatsRace);
  const rules::SkillSet background = level == 1 ? BackgroundSkillsAtCreation() : 0;
  const uint64_t key = (static_cast<uint64_t>(level) << 48) | (static_cast<uint64_t>(g_get_class(stats)) << 40) |
                       (static_cast<uint64_t>(class_level) << 32) | (static_cast<uint64_t>(race) << 24) |
                       static_cast<uint64_t>(background);
  if (g_level.stats == stats && g_level.key == key) return;
  Level next;
  next.stats = stats;
  next.key = key;
  next.level = level;
  int8_t* ranks = Ranks(stats);
  if (!character_class || !ranks || level < 1) {
    g_level = next;
    return;
  }
  next.character_class = *character_class;
  const bool first_class = level == 1;
  next.expertise = rules::LevelSkillPicks2024(*character_class, class_level, first_class);
  if (level == 1) {
    next.skills = rules::SkillsPageAllowance(*character_class, SpeciesOfRace(race));
    next.background = background;
    SetBackgroundRanks(stats, background);
  } else {
    if (!rules::RanksAre2024(ranks, Rows())) {  // an older character's NWN ranks: !skills
      g_level = next;
      if (g_logged.First(LogOnce::Key(3, 0, 0))) {
        Log("skills page: a character with NWN ranks levels up - its 2024 picks stay with !skills");
      }
      return;
    }
    next.skills.class_skills = next.expertise.skills;
    next.skills.class_list = next.expertise.skill_list;
    next.skills.background_skills = 0;
    next.skills.background_tools = 0;
  }
  for (int row = 0; row < Rows(); ++row) {
    next.floor[row] = ranks[row];
    if (ranks[row] >= 1 && rules::IsSkillsPageRow(row)) next.known |= rules::SkillBit(*rules::Skill2024ForNwnSkill(row));
  }
  next.points = rules::SkillsPagePoints(next.skills) + next.expertise.expertise;
  next.active = true;
  g_level = next;
  Log("skills page: level %d (class %d at its level %d) - %d skill pick(s), %d Expertise; the background's %s "
      "taken", level, static_cast<int>(*character_class), class_level, rules::SkillsPagePoints(next.skills),
      next.expertise.expertise, Names(next.background).c_str());
}

bool ActiveFor(void* stats) { return Active() && stats && g_level.active && g_level.stats == stats; }

// This level's new skills (rank above a floor of 0) and its Expertise (rank 2 above a floor below 2).
rules::SkillSet NewSkills(void* stats) {
  const int8_t* ranks = Ranks(stats);
  rules::SkillSet picked = 0;
  for (int row = 0; ranks && row < Rows(); ++row) {
    if (ranks[row] >= 1 && g_level.floor[row] == 0 && rules::IsSkillsPageRow(row)) {
      picked |= rules::SkillBit(*rules::Skill2024ForNwnSkill(row));
    }
  }
  return picked;
}

int NewExpertise(void* stats) {
  const int8_t* ranks = Ranks(stats);
  int taken = 0;
  for (int row = 0; ranks && row < Rows(); ++row) taken += ranks[row] >= 2 && g_level.floor[row] < 2 ? 1 : 0;
  return taken;
}

void Points(void* stats) {
  if (!Active() || !stats) {
    g_points(stats);
    return;
  }
  Begin(stats);
  if (!g_level.active) {
    if (g_get_level(stats) == 1) g_points(stats);  // not a base class: NWN's
    return;                                        // later levels: none (2024), or !skills
  }
  if (g_level.level == 1) SetBackgroundRanks(stats, g_level.background);
  SetPointsLeft(stats, PointsLeft(stats) + g_level.points);
}

// The 2024 pick "+" makes: a new skill (rank 0 -> 1) or Expertise (1 -> 2).
int AddRank(void* stats, uint16_t skill) {
  if (!ActiveFor(stats)) return g_level.stats == stats && Active() ? 0 : g_add_rank(stats, skill);
  int8_t* ranks = Ranks(stats);
  const auto skill2024 = rules::Skill2024ForNwnSkill(skill);
  if (!ranks || skill >= Rows() || !skill2024 || !rules::IsSkillsPageRow(skill) || PointsLeft(stats) < 1) return 0;
  if (ranks[skill] == 0) {
    const rules::SkillSet picked = NewSkills(stats);
    if (!rules::CanPickOnSkillsPage(g_level.skills, g_level.known | g_level.background, picked, *skill2024)) {
      Log("skills page: %s not taken - this level's picks so far: %s", rules::Skill2024Name(*skill2024),
          Names(picked).c_str());
      return 0;
    }
  } else if (ranks[skill] == 1) {
    if (!rules::CanPickExpertise(g_level.expertise, NewExpertise(stats), *skill2024)) {
      if (g_level.expertise.expertise > 0) {
        Log("skills page: Expertise in %s not taken (%d of %d this level)", rules::Skill2024Name(*skill2024),
            NewExpertise(stats), g_level.expertise.expertise);
      }
      return 0;
    }
  } else {
    return 0;
  }
  g_set_rank(stats, skill, static_cast<int8_t>(ranks[skill] + 1));
  SetPointsLeft(stats, PointsLeft(stats) - 1);
  return 1;
}

// "-": this level's picks only (never below the floor: earlier levels, the background).
void RemoveRank(void* stats, uint16_t skill, uint8_t min_rank) {
  if (!ActiveFor(stats)) {
    if (!(g_level.stats == stats && Active())) g_remove_rank(stats, skill, min_rank);
    return;
  }
  int8_t* ranks = Ranks(stats);
  if (!ranks || skill >= Rows() || ranks[skill] <= g_level.floor[skill] || ranks[skill] <= static_cast<int8_t>(min_rank)) {
    return;
  }
  g_set_rank(stats, skill, static_cast<int8_t>(ranks[skill] - 1));
  SetPointsLeft(stats, PointsLeft(stats) + 1);
}

// Recommended (and the hub's OK without Customize): the class's usual skills,
// then Expertise in its usual skills the character is proficient in.
void PackageSkills(void* stats, const void* table_name) {
  if (!ActiveFor(stats)) {
    if (!(g_level.stats == stats && Active())) g_package_skills(stats, table_name);
    return;
  }
  if (g_level.level == 1) SetBackgroundRanks(stats, g_level.background);
  const std::vector<rules::Skill2024> preference = rules::SkillPreference2024(g_level.character_class);
  const int8_t* ranks = Ranks(stats);
  for (int pass = 0; pass < 2 && ranks; ++pass) {  // new skills, then Expertise
    for (rules::Skill2024 skill : preference) {
      const int row = rules::SkillsPageRow(skill);
      if (row < 0 || row >= Rows() || PointsLeft(stats) < 1) continue;
      if ((pass == 0 && ranks[row] == 0) || (pass == 1 && ranks[row] == 1)) AddRank(stats, static_cast<uint16_t>(row));
    }
  }
  Log("skills page: Recommended - new %s; Expertise %d (background %s)", Names(NewSkills(stats)).c_str(),
      NewExpertise(stats), Names(g_level.background).c_str());
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
