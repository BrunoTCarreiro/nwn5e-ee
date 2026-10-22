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
// - the package's picks (0x7eb300: the hub's OK without Customize): the
//   class's usual skills, then Expertise in them.
// The page goes one step at a time (class skills, a Human's or Elf's pick,
// Expertise): OK moves it on, Recommended makes the step's choices, Reset
// starts over.
// A character whose ranks aren't the 2024 record (an older character's NWN
// ranks: rules::RanksAre2024) gets no points after level 1 — !skills.
// The ranks are the record: rules_skills.cpp reads proficiency (1) and
// Expertise (2) from them. Skilled (its Feats page comes after this one):
// C8, the Skills page again (SkillsAgainOpened). Any anchor missing: nothing is hooked, the
// rows stay NWN's, and the page is the game's own.

#include <windows.h>

#include <stdint.h>
#include <string.h>

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
#include "rules/feats2024.h"
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
using PageFn = void (*)(void* page, void* a2, void* a3, void* a4);
using GetCreatureFn = void* (*)(uint32_t id);
using SetEnabledFn = void (*)(void* checkbox, int enabled);
using SetColorFn = void (*)(void* wrapper, float r, float g, float b, float a);
using SkillUseableFn = int (*)(void* character_class, uint16_t skill);

PointsFn g_points = nullptr;
AddRankFn g_add_rank = nullptr;
RemoveRankFn g_remove_rank = nullptr;
PackageSkillsFn g_package_skills = nullptr;
SetRankFn g_set_rank = nullptr;
ClientByteFn g_get_level = nullptr;  // the level after the level-up being made
ClientByteFn g_get_class = nullptr;  // the class being levelled
PageFn g_build_list = nullptr;       // BuildSkillList (the page's rows, once)
PageFn g_plus = nullptr;             // the rows' "+" and "-", the Recommended button
PageFn g_minus = nullptr;
PageFn g_recommend = nullptr;
PageFn g_ok = nullptr;         // the OK button
PageFn g_row_click = nullptr;  // a row's click (its description)
GetCreatureFn g_get_creature = nullptr;
SetEnabledFn g_set_enabled = nullptr;
SetColorFn g_set_color = nullptr;
SkillUseableFn g_skill_useable = nullptr;
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
  int class_level = 0;  // the class's level after this level-up
  rules::CharacterClass character_class = rules::CharacterClass::kFighter;
  rules::SkillAllowance skills;  // this level's new skills (level 1: the page's allowance)
  rules::LevelSkillPicks expertise;
  rules::SkillSet background = 0;  // level 1: the background's skills, taken and locked
  rules::SkillSet known = 0;       // proficient before this level
  int8_t floor[rules::kNwnSkillCount] = {};
  int8_t pick_step[rules::kNwnSkillCount] = {};  // the step a new skill (rank 0 -> 1) was taken in, -1 none
  int points = 0;
  int skilled_picks = 0;  // C8: the again page's one step - 3 per Skilled feat taken this level
};
Level g_level;

// --- C8: "Skills again" (owner, 2026-10-23: "go with recommendations") -------
// Skilled taken on the Feats page (it follows the Skills page) opens the
// Skills page again for its picks: built and activated as the Skills page's
// OK builds a Feats page; its OK goes on as the Feats page's would (no Feats
// page again: CalcNumberFeats answers 0 for it). See SkillsAgainOpened.
using SkillsCtorFn = void (*)(void* page, uint32_t creature, int level_up);
using PanelFn = void (*)(void* panel);
using DeactivateFn = void (*)(void* panel, int exit_code);
using GetLevelUpFn = void* (*)(void* app);
using FeatCountFn = uintptr_t (*)(void* stats, uint8_t* normal, uint8_t* bonus);
SkillsCtorFn g_skills_ctor = nullptr;
PanelFn g_activate_modal = nullptr;
DeactivateFn g_deactivate_modal = nullptr;
GetLevelUpFn g_get_level_up = nullptr;
uintptr_t g_app_global = 0;  // the global GetLevelUp's argument comes through (**it)
FeatCountFn g_feat_count = nullptr;
uintptr_t g_feats_vtable = 0;
bool g_skills_again = false;     // all of the above found
bool g_in_again_ctor = false;    // the again page's constructor: no points from the level
bool g_no_feat_count = false;    // the again page's OK: no Feats page after it
void* g_again_page = nullptr;
Level g_level_before_again;      // restored when the again page closes
int g_skilled_before = 0;        // Skilled feats when this level's first Skills page opened
void* g_skilled_stats = nullptr;  // ... for these stats

int CountSkilled(void* stats) {
  const auto* feats = Field<const uint16_t*>(stats, ee::kClientStatsFeats);
  const int count = Field<int32_t>(stats, ee::kClientStatsFeatCount);
  int skilled = 0;
  for (int i = 0; feats && i < count && i < 4096; ++i) {
    const int feat = feats[i];
    if (feat == rules::kFeatSkilled ||
        (feat >= rules::kFeatSkilledAgainFirst && feat < rules::kFeatSkilledAgainFirst + rules::kSkilledRepeats)) {
      ++skilled;
    }
  }
  return skilled;
}

uintptr_t FeatCount(void* stats, uint8_t* normal, uint8_t* bonus) {
  const uintptr_t result = g_feat_count(stats, normal, bonus);
  if (g_no_feat_count) {
    if (normal) *normal = 0;
    if (bonus) *bonus = 0;
  }
  return result;
}

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
  next.class_level = class_level;
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
  for (int row = 0; row < rules::kNwnSkillCount; ++row) next.pick_step[row] = -1;
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
  if (g_in_again_ctor) return;  // C8: the again page's picks are Skilled's (SkillsAgainOpened sets them)
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

// The page's steps (owner, 2026-10-22: "each in their step"): the class's
// skills, then a Human's or Elf's pick, then Expertise — a pick is taken only
// in its step, and the page greys every row the step can't take.
enum class Step { kClass, kSpecies, kExpertise, kSkilled, kDone };

// A step's choices: how many it makes, how many are made (a new skill counts
// in the step it was taken in), how many are left.
int StepTotal(Step step) {
  switch (step) {
    case Step::kClass: return g_level.skills.class_skills;
    case Step::kSpecies: return g_level.skills.species_any_skill || g_level.skills.species_list ? 1 : 0;
    case Step::kExpertise: return g_level.expertise.expertise;
    case Step::kSkilled: return g_level.skilled_picks;
    default: return 0;
  }
}

int StepPicks(void* stats, Step step) {
  if (step == Step::kExpertise) return NewExpertise(stats);
  const int8_t* ranks = Ranks(stats);
  int picks = 0;
  for (int row = 0; ranks && row < Rows(); ++row) {
    picks += ranks[row] >= 1 && g_level.floor[row] == 0 && rules::IsSkillsPageRow(row) &&
                     g_level.pick_step[row] == static_cast<int8_t>(step)
                 ? 1
                 : 0;
  }
  return picks;
}

int StepLeft(void* stats, Step step) { return std::max(0, StepTotal(step) - StepPicks(stats, step)); }

// The steps in order, those with a choice to make.
Step NextStep(Step step) {
  for (int next = static_cast<int>(step) + 1; next < static_cast<int>(Step::kDone); ++next) {
    if (StepTotal(static_cast<Step>(next)) > 0) return static_cast<Step>(next);
  }
  return Step::kDone;
}

Step FirstStep() { return StepTotal(Step::kClass) > 0 ? Step::kClass : NextStep(Step::kClass); }

// Without the page (the hub's OK without Customize): the first step with a choice left.
Step CurrentStep(void* stats) {
  for (Step step = FirstStep(); step != Step::kDone; step = NextStep(step)) {
    if (StepLeft(stats, step) > 0) return step;
  }
  return Step::kDone;
}

// The page's step (owner, 2026-10-22: "the user needs to press "OK" to move
// to the next step"): the first when the page opens, the next only by OK, the
// first again by Reset. Without the page the steps follow the picks.
void* g_step_page = nullptr;
Step g_page_step = Step::kDone;

Step PickStep(void* stats) { return g_step_page ? g_page_step : CurrentStep(stats); }

// Whether the class can use a skill at all (the page greys the others too).
bool Useable(void* stats, int row) {
  const void* rules_object = nullptr;
  if (const auto rules_pointer = reinterpret_cast<void**>(EngineExport(ee::kRules))) rules_object = *rules_pointer;
  if (!rules_object || !g_skill_useable) return true;
  auto* classes = Field<uint8_t*>(rules_object, ee::kRulesClasses);
  if (!classes) return true;
  return g_skill_useable(classes + g_get_class(stats) * ee::kClassStride, static_cast<uint16_t>(row)) != 0;
}

// Whether "+" may take `row` now.
bool Pickable(void* stats, int row, Step step) {
  const int8_t* ranks = Ranks(stats);
  const auto skill = rules::Skill2024ForNwnSkill(row);
  if (!ranks || row >= Rows() || !skill || !rules::IsSkillsPageRow(row) || !Useable(stats, row)) return false;
  if (StepLeft(stats, step) <= 0) return false;
  const rules::SkillSet taken = g_level.known | g_level.background;
  switch (step) {
    case Step::kClass:
      if (ranks[row] != 0 || !rules::Has(g_level.skills.class_list, *skill)) return false;
      break;
    case Step::kSpecies:
      if (ranks[row] != 0) return false;
      if (!g_level.skills.species_any_skill && !rules::Has(g_level.skills.species_list, *skill)) return false;
      break;
    case Step::kExpertise:
      return ranks[row] == 1 && rules::CanPickExpertise(g_level.expertise, NewExpertise(stats), *skill);
    case Step::kSkilled:
      return ranks[row] == 0 && !rules::IsTool(*skill);
    case Step::kDone:
      return false;
  }
  return rules::CanPickOnSkillsPage(g_level.skills, taken, NewSkills(stats), *skill);
}

// The rows a step lists, fixed for the whole step (owner, 2026-10-22: "Skill
// should only be added or removed to the list between steps"): the class
// step — the class's skills the character didn't have; the species step —
// the skills that step can take, not the class step's picks; the Expertise
// step — every skill the character has that Expertise can go to (those it
// can't take more of greyed, not removed).
bool Candidate(void* stats, int row, Step step) {
  const int8_t* ranks = Ranks(stats);
  const auto skill = rules::Skill2024ForNwnSkill(row);
  if (!ranks || row >= Rows() || !skill || !rules::IsSkillsPageRow(row) || !Useable(stats, row)) return false;
  const bool before = g_level.floor[row] > 0 || rules::Has(g_level.background, *skill);
  switch (step) {
    case Step::kClass:
      return !before && rules::Has(g_level.skills.class_list, *skill);
    case Step::kSpecies:
      if (before || g_level.pick_step[row] == static_cast<int8_t>(Step::kClass)) return false;
      return g_level.skills.species_any_skill ? !rules::IsTool(*skill) : rules::Has(g_level.skills.species_list, *skill);
    case Step::kExpertise:
      return ranks[row] >= 1 && !rules::IsTool(*skill) &&
             (!g_level.expertise.expertise_list || rules::Has(g_level.expertise.expertise_list, *skill));
    case Step::kSkilled:  // any skill the character didn't have when the step began
      return g_level.floor[row] == 0 && !rules::IsTool(*skill);
    default:
      return false;
  }
}

const char* StepName(Step step) {
  switch (step) {
    case Step::kClass: return "the class's skills";
    case Step::kSpecies: return "the species pick";
    case Step::kExpertise: return "Expertise";
    case Step::kSkilled: return "Skilled";
    default: return "done";
  }
}

// The 2024 pick "+" makes: a new skill (rank 0 -> 1) or Expertise (1 -> 2).
int AddRankAs(void* stats, uint16_t skill, bool quiet) {
  int8_t* ranks = Ranks(stats);
  if (!ranks || skill >= Rows() || PointsLeft(stats) < 1) return 0;
  const Step step = PickStep(stats);
  if (!Pickable(stats, skill, step)) {
    if (!quiet) {
      const auto skill2024 = rules::Skill2024ForNwnSkill(skill);
      Log("skills page: %s not taken now (step: %s)", skill2024 ? rules::Skill2024Name(*skill2024) : "?",
          StepName(step));
    }
    return 0;
  }
  if (ranks[skill] == 0) g_level.pick_step[skill] = static_cast<int8_t>(step);
  g_set_rank(stats, skill, static_cast<int8_t>(ranks[skill] + 1));
  SetPointsLeft(stats, PointsLeft(stats) - 1);
  return 1;
}

int AddRank(void* stats, uint16_t skill) {
  if (!ActiveFor(stats)) return g_level.stats == stats && Active() ? 0 : g_add_rank(stats, skill);
  return AddRankAs(stats, skill, false);
}

// This level's choices taken back - all of them (Step::kDone) or one step's -
// their points returned. Returns how many.
int TakeBack(void* stats, Step which) {
  int8_t* ranks = Ranks(stats);
  int back = 0;
  for (int row = 0; ranks && row < Rows(); ++row) {
    const int floor = g_level.floor[row];
    int target = ranks[row];
    if (which == Step::kDone) {
      target = std::min<int>(ranks[row], floor);
    } else if (which == Step::kExpertise) {
      if (ranks[row] >= 2 && floor < 2) target = 1;
    } else if (ranks[row] == 1 && floor == 0 && g_level.pick_step[row] == static_cast<int8_t>(which)) {
      target = 0;
    }
    if (target < ranks[row]) {
      back += ranks[row] - target;
      g_set_rank(stats, static_cast<uint16_t>(row), static_cast<int8_t>(target));
    }
    if (ranks[row] <= floor) g_level.pick_step[row] = -1;
  }
  if (which == Step::kDone && g_level.level == 1) SetBackgroundRanks(stats, g_level.background);
  SetPointsLeft(stats, PointsLeft(stats) + back);
  return back;
}

// Recommended's choices for one step: the class's usual skills in order, then
// any other the step can take.
void RecommendStep(void* stats, Step step) {
  auto take = [&](int row) {
    if (row >= 0 && row < Rows() && StepLeft(stats, step) > 0) AddRankAs(stats, static_cast<uint16_t>(row), true);
  };
  for (rules::Skill2024 skill : rules::SkillPreference2024(g_level.character_class)) take(rules::SkillsPageRow(skill));
  for (int row = 0; row < Rows(); ++row) take(row);
  Log("skills page: Recommended for %s - new %s; Expertise %d", StepName(step), Names(NewSkills(stats)).c_str(),
      NewExpertise(stats));
}

// Whether a step's choices are made: none left, or none it can still take.
bool StepDone(void* stats, Step step) {
  if (StepLeft(stats, step) <= 0) return true;
  for (int row = 0; row < Rows(); ++row) {
    if (Pickable(stats, row, step)) return false;
  }
  return true;
}

// The page's creature's client stats.
void* PageStats(void* page) {
  void* creature = page && g_get_creature ? g_get_creature(Field<uint32_t>(page, ee::kSkillsPageCreature)) : nullptr;
  return creature ? Field<void*>(creature, ee::kClientCreatureStats) : nullptr;
}

// --- The page in steps (owner, 2026-10-22: "In each step only pickable feats
// are displayed and feats that reach their maximum level get greyed out. There
// should also be text in the window that explains what kind of points are
// being used") ----------------------------------------------------------------

using DeleteButtonsFn = void (*)(void* listbox);
using SetTextFn = void (*)(void* wrapper, const void* exo_string);
using ExoStringFn = void (*)(void* exo_string, const char* text);
using ExoStringDestroyFn = void (*)(void* exo_string);
DeleteButtonsFn g_delete_buttons = nullptr;
SetTextFn g_set_text = nullptr;
ExoStringFn g_make_string = nullptr;
ExoStringDestroyFn g_destroy_string = nullptr;
using TextButtonEnabledFn = void (*)(void* button, int enabled);
TextButtonEnabledFn g_text_button_enabled = nullptr;

// A row the page lists: the shown step's candidates (live when "+" can take
// them now, else greyed).
bool Listed(void* stats, int row, Step display) { return Candidate(stats, row, display); }

void* RulesObject() {
  const auto rules_pointer = reinterpret_cast<void**>(EngineExport(ee::kRules));
  return rules_pointer ? *rules_pointer : nullptr;
}

void SetWrapperText(void* wrapper, const std::string& text) {
  if (!g_set_text || !g_make_string || !g_destroy_string) return;
  alignas(16) uint8_t exo[32] = {};  // CExoString: the engine owns its buffer
  g_make_string(exo, text.c_str());
  g_set_text(wrapper, exo);
  g_destroy_string(exo);
}

const char* ClassName(rules::CharacterClass c) {
  static const char* const kNames[] = {"Barbarian", "Bard",    "Cleric", "Druid",    "Fighter", "Monk",
                                       "Paladin",   "Ranger",  "Rogue",  "Sorcerer", "Warlock", "Wizard"};
  return kNames[static_cast<int>(c)];
}

std::string SpeciesLine() {
  if (g_level.skills.species_any_skill) return "Human (Skillful): 1 skill of any kind";
  if (g_level.skills.species_list) return "Elf (Keen Senses): Insight, Perception or Survival";
  return "";
}

std::string Count(int n, const char* what) { return std::to_string(n) + " " + what + (n == 1 ? "" : "s"); }

std::string CreationBackgroundName() {
  if (const auto choice = rules::BackgroundFromFeat(CreationBackgroundFeat())) {
    if (const rules::BackgroundInfo* info = rules::BackgroundData(choice->background)) return info->name;
  }
  return "";
}

// Where a level-up's new skills come from (rules::LevelSkillPicks2024).
std::string LevelSkillsReason(int total) {
  const std::string cls = ClassName(g_level.character_class);
  const auto c = g_level.character_class;
  if (g_level.class_level == 1) {
    return "Multiclassing into " + cls + " gives you " + Count(total, "skill") +
           (c == rules::CharacterClass::kBard ? " of any kind." : " from its list.");
  }
  if (c == rules::CharacterClass::kBarbarian && g_level.class_level == 3) {
    return "Primal Knowledge: choose 1 more skill from the Barbarian's list.";
  }
  if (c == rules::CharacterClass::kBard && g_level.class_level == 3) {
    return "College of Lore, Bonus Proficiencies: choose 3 skills of any kind.";
  }
  return "Choose the " + Count(total, "skill") + " this level gives you.";
}

// The help box's text for a step - what is being chosen and why, until a
// skill is clicked (owner, 2026-10-22: "where the skill description is have
// the default text change with each step").
std::string StepHelp(Step step) {
  int number = 0;
  int steps = 0;
  for (Step at = FirstStep(); at != Step::kDone; at = NextStep(at)) {
    ++steps;
    if (at == step) number = steps;
  }
  const std::string cls = ClassName(g_level.character_class);
  const int total = StepTotal(step);
  std::string help = number ? "Step " + std::to_string(number) + " of " + std::to_string(steps) + ": " : "";
  switch (step) {
    case Step::kClass:
      help += cls + " skills\n\n";
      help += g_level.level == 1 ? "Choose " + Count(total, "skill") + " from the " + cls + "'s list."
                                 : LevelSkillsReason(total);
      help += total == 1 ? " You become proficient in it: your proficiency bonus is added to its checks."
                         : " You become proficient in each: your proficiency bonus is added to its checks.";
      if (g_level.background) {
        help += "\n\nYour " + CreationBackgroundName() + " background gives you " + Names(g_level.background) +
                " already, so they are not listed.";
      }
      break;
    case Step::kSpecies:
      help += g_level.skills.species_any_skill
                  ? "Human - Skillful\n\nChoose 1 more skill of any kind. You become proficient in it."
                  : "Elf - Keen Senses\n\nChoose Insight, Perception or Survival. You become proficient in it.";
      help += " The skills you have already are not listed.";
      break;
    case Step::kExpertise:
      help += "Expertise\n\n";
      help += g_level.expertise.expertise_list
                  ? "Scholar: choose 1 of Arcana, History, Investigation, Medicine, Nature or Religion that you are "
                    "proficient in."
                  : "Choose " + Count(total, "skill") + " you are proficient in.";
      help += total == 1 ? " Your proficiency bonus counts twice for its checks."
                         : " Your proficiency bonus counts twice for their checks.";
      help += " A greyed skill is chosen already, or can't take Expertise.";
      break;
    case Step::kSkilled:
      help += "Skilled\n\nChoose " + Count(total, "skill") + " of any kind (the Skilled feat" +
              (total > 3 ? ", taken more than once" : "") +
              "). You become proficient in each. The skills you have already are not listed.";
      break;
    case Step::kDone:
      help += "This level gives no skill choices.";
      break;
  }
  help += NextStep(step) == Step::kDone ? "\n\nOK finishes once this step's choices are made."
                                        : "\n\nOK goes on to the next step once this step's choices are made.";
  help += " Recommended chooses for this step, Reset goes back to the first step, and - takes back a choice of "
          "this step.";
  return help;
}

// The header (the step, what is left, OK once it's done), the help box (the
// step's explanation), the points box (the step's choices left).
void ShowTexts(void* page, void* stats, Step step, bool help_too) {
  const std::string cls = ClassName(g_level.character_class);
  const int left = StepLeft(stats, step);
  const std::string more = left ? "choose " + std::to_string(left) + " more of " + std::to_string(StepTotal(step))
                                : std::string("chosen");
  std::string header;
  switch (step) {
    case Step::kClass:
      header = cls + " skills: " + more;
      break;
    case Step::kSpecies:
      header = SpeciesLine() + (left ? ": choose it" : ": chosen");
      break;
    case Step::kExpertise:
      header = "Expertise: " + more;
      break;
    case Step::kSkilled:
      header = "Skilled: " + more;
      break;
    case Step::kDone:
      header = "No skill choices at this level";
      break;
  }
  if (StepDone(stats, step)) header += NextStep(step) == Step::kDone ? " - OK to finish" : " - OK for the next step";
  SetWrapperText(static_cast<uint8_t*>(page) + ee::kSkillsPageHeader, header);
  if (help_too) SetWrapperText(static_cast<uint8_t*>(page) + ee::kSkillsPageHelp, StepHelp(step));  // not over a skill's text
  SetWrapperText(static_cast<uint8_t*>(page) + ee::kSkillsPagePoints, std::to_string(left));
}

// Every listed row live or greyed for the step.
void Grey(void* page, void* stats, Step step) {
  if (!g_set_enabled || !g_set_color) return;
  auto* rows = Field<void**>(page, ee::kSkillsPageRows);
  const int count = Field<int32_t>(page, ee::kSkillsPageRowCount);
  const auto* skills = Field<const int32_t*>(page, ee::kSkillsPageRowSkills);
  if (!rows || !skills) return;
  for (int i = 0; i < count; ++i) {
    const int row = skills[i];
    if (!rows[i] || row < 0 || row >= Rows()) continue;
    const bool live = Pickable(stats, row, step);
    g_set_enabled(rows[i], live ? 1 : 0);
    const float shade = live ? 1.0f : 0.5f;
    g_set_color(static_cast<uint8_t*>(rows[i]) + ee::kSkillsRowName, shade, shade, shade, 1.0f);
  }
}

// Whether the list shows exactly the step's rows.
bool ListMatches(void* page, void* stats, Step step) {
  const int count = Field<int32_t>(page, ee::kSkillsPageRowCount);
  const auto* skills = Field<const int32_t*>(page, ee::kSkillsPageRowSkills);
  int listed = 0;
  for (int row = 0; row < Rows(); ++row) listed += Listed(stats, row, step) ? 1 : 0;
  if (count != listed) return false;
  for (int i = 0; skills && i < count; ++i) {
    if (skills[i] < 0 || skills[i] >= Rows() || !Listed(stats, skills[i], step)) return false;
  }
  return true;
}

// The game's builder, showing only the step's rows: the others hidden from
// it (HideFromLevelUp) for the length of the call.
void BuildStepList(void* page, void* stats, Step step, void* a2, void* a3, void* a4) {
  auto* skills = Field<uint8_t*>(RulesObject(), ee::kRulesSkills);
  int32_t hidden[rules::kNwnSkillCount] = {};
  const int rows = Rows();
  for (int row = 0; skills && row < rows; ++row) {
    int32_t& flag = *reinterpret_cast<int32_t*>(skills + row * ee::kSkillStride + ee::kSkillHideFromLevelUp);
    hidden[row] = flag;
    if (!Listed(stats, row, step)) flag = 1;
  }
  g_build_list(page, a2, a3, a4);
  for (int row = 0; skills && row < rows; ++row) {
    *reinterpret_cast<int32_t*>(skills + row * ee::kSkillStride + ee::kSkillHideFromLevelUp) = hidden[row];
  }
}

// OK on once the step's choices are made, greyed before (as the pages grey a text button).
void ShowOk(void* page, void* stats, Step step) {
  if (!g_text_button_enabled || !g_set_color) return;
  const bool on = StepDone(stats, step);
  auto* ok = static_cast<uint8_t*>(page) + ee::kSkillsPageOk;
  g_text_button_enabled(ok, on ? 1 : 0);
  const float shade = on ? 1.0f : ee::kDisabledShade;
  g_set_color(ok + ee::kTextButtonCaption, shade, shade, shade, 1.0f);
}

// After the page opens or a click: the step's rows (built again when they
// changed, or with `rebuild` when ranks changed outside the rows' own "+" and
// "-" - their numbers are the rows'), their greying, OK, the texts (the help
// box only with a new list: a click leaves the clicked skill's description).
void ShowStep(void* page, bool help_too, bool rebuild = false) {
  void* stats = PageStats(page);
  if (!ActiveFor(stats) || page != g_step_page) return;
  const Step step = g_page_step;
  if ((rebuild || !ListMatches(page, stats, step)) && g_delete_buttons) {
    help_too = true;
    g_delete_buttons(static_cast<uint8_t*>(page) + ee::kSkillsPageList);
    BuildStepList(page, stats, step, nullptr, nullptr, nullptr);
    SetField<int32_t>(page, ee::kSkillsPageSelected, Field<int32_t>(page, ee::kSkillsPageRowCount) > 0 ? 0 : -1);
  }
  Grey(page, stats, step);
  ShowOk(page, stats, step);
  ShowTexts(page, stats, step, help_too);
}

// --- The Reset button (owner, 2026-10-22: "a "reset" button between the
// recommended and OK buttons that resets the point allocation and returns to
// user to the first step") ---------------------------------------------------

using NewFn = void* (*)(size_t size);
using ObjectFn = void (*)(void* object);
using ImportFn = int (*)(void* control, void* panel, const char* tag);
using DeletingDtorFn = void* (*)(void* object, int free_it);
NewFn g_new = nullptr;
ObjectFn g_text_button_ctor = nullptr;
ImportFn g_import = nullptr;
DeletingDtorFn g_page_dtor = nullptr;
void* g_reset_page = nullptr;    // the page the button belongs to
void* g_reset_button = nullptr;
void* g_open_page = nullptr;     // the page up now (its init ran, its destructor not yet)

// A level-up with nothing to pick skips the page (owner, 2026-10-22: "if
// there are no skills to pick, you can skip that screen"): its OK, pressed
// once the page is up - by a thread timer, which the game's message loop runs
// between frames, not inside the page's own opening.
void* g_skip_page = nullptr;
const char* g_skip_why = "";

void CALLBACK SkipTimer(HWND, UINT, UINT_PTR id, DWORD) {
  KillTimer(nullptr, id);
  void* page = g_skip_page;
  g_skip_page = nullptr;
  if (!page || page != g_open_page || !g_ok) return;  // closed already
  Log("skills page: skipped - %s", g_skip_why);
  g_ok(page, nullptr, nullptr, nullptr);
}

void SkipPage(void* page, const char* why) {
  g_skip_page = page;
  g_skip_why = why;
  if (!SetTimer(nullptr, 0, USER_TIMER_MINIMUM, &SkipTimer)) {
    g_skip_page = nullptr;
    Log("skills page: not skipped (%s) - no timer", why);
  }
}

// This level's picks taken back (the background's and earlier levels' kept),
// their points returned, the first step shown.
void ResetClicked(void* page) {
  void* stats = PageStats(page);
  if (!ActiveFor(stats) || page != g_step_page) return;
  const int back = TakeBack(stats, Step::kDone);
  g_page_step = FirstStep();
  ShowStep(page, true, true);
  Log("skills page: Reset - %d choice(s) taken back, %s again", back, StepName(g_page_step));
}

// Made and bound inside the page's init (the layout file still open).
void MakeResetButton(void* page, bool active) {
  if (!g_new || !g_text_button_ctor || !g_import) return;
  if (g_reset_button) {  // one page at a time; another's means its destructor was missed
    if (g_reset_page != page) Log("skills page: SKIP the Reset button - the last page's is still there");
    return;
  }
  void* button = g_new(ee::kTextButtonSize);
  if (!button) return;
  memset(button, 0, ee::kTextButtonSize);
  g_text_button_ctor(button);
  if (!g_import(button, page, "ResetButton")) {
    reinterpret_cast<DeletingDtorFn>((*reinterpret_cast<void***>(button))[0])(button, 1);
    Log("skills page: SKIP the Reset button - no \"ResetButton\" in the layout");
    return;
  }
  SetField<void*>(button, ee::kButtonClickObject, page);
  SetField<void*>(button, ee::kButtonClickFunction, reinterpret_cast<void*>(&ResetClicked));
  if (!active && g_text_button_enabled) {  // an older character's level-up: the game's page, no steps
    g_text_button_enabled(button, 0);
    if (g_set_color) g_set_color(static_cast<uint8_t*>(button) + ee::kTextButtonCaption, 0.5f, 0.5f, 0.5f, 1.0f);
  }
  g_reset_page = page;
  g_reset_button = button;
}

// The page's deleting destructor: the Reset button first (its model hangs
// under the page's frame).
void* PageDeletingDtor(void* page, int free_it) {
  if (page == g_step_page) g_step_page = nullptr;
  if (page == g_open_page) g_open_page = nullptr;
  if (page == g_again_page) {  // C8: the level as before the Skilled page (its picks stay this level's)
    g_again_page = nullptr;
    g_level = g_level_before_again;
  }
  if (page == g_reset_page && g_reset_button) {
    void* button = g_reset_button;
    g_reset_button = nullptr;
    g_reset_page = nullptr;
    reinterpret_cast<DeletingDtorFn>((*reinterpret_cast<void***>(button))[0])(button, 1);
  }
  return g_page_dtor(page, free_it);
}

void BuildList(void* page, void* a2, void* a3, void* a4) {
  void* stats = PageStats(page);
  g_open_page = page;
  MakeResetButton(page, ActiveFor(stats));
  if (!ActiveFor(stats)) {
    g_step_page = nullptr;
    g_build_list(page, a2, a3, a4);
    if (Active() && stats && g_level.stats == stats && !g_level.active && g_level.level > 1 && PointsLeft(stats) == 0) {
      SkipPage(page, "an older character's level-up: no points (its skills stay with !skills)");
    }
    return;
  }
  if (page != g_again_page) {  // C8: the Skilled feats this level starts from
    g_skilled_before = CountSkilled(stats);
    g_skilled_stats = stats;
  }
  if (g_level.level == 1) SetBackgroundRanks(stats, g_level.background);  // after the hub's clear
  if (const int back = TakeBack(stats, Step::kDone)) {  // the page opens on its first step, nothing chosen
    Log("skills page: opened with %d choice(s) made - taken back", back);
  }
  g_step_page = page;
  g_page_step = FirstStep();
  BuildStepList(page, stats, g_page_step, a2, a3, a4);
  ShowStep(page, true);
  if (g_page_step == Step::kDone && g_level.level > 1) SkipPage(page, "no skill choices at this level");
}

// A row's click: a greyed row shows "Unavailable Skill / You may not learn
// this skill." (its disabled bit) - on the stepped page every listed skill is
// one the step can take (owner, 2026-10-22: a skill greyed at its most
// "should not say that they cannot learn that skill"), so it shows the
// skill's description as a live row does: the bit off for the call.
void RowClick(void* page, void* a2, void* a3, void* a4) {
  const int index = static_cast<int>(reinterpret_cast<intptr_t>(a2));
  auto* rows = Field<void**>(page, ee::kSkillsPageRows);
  const int count = Field<int32_t>(page, ee::kSkillsPageRowCount);
  uint32_t* flags = nullptr;
  if (page == g_step_page && ActiveFor(PageStats(page)) && rows && index >= 0 && index < count && rows[index]) {
    flags = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(rows[index]) + ee::kGuiControlFlags);
  }
  const uint32_t disabled = flags ? (*flags & ee::kGuiControlDisabled) : 0;
  if (disabled) *flags &= ~ee::kGuiControlDisabled;
  g_row_click(page, a2, a3, a4);
  if (disabled) *flags |= disabled;
}

void Plus(void* page, void* a2, void* a3, void* a4) {
  g_plus(page, a2, a3, a4);
  ShowStep(page, false);
}

void Minus(void* page, void* a2, void* a3, void* a4) {
  g_minus(page, a2, a3, a4);
  ShowStep(page, false);
}

// Recommended (owner, 2026-10-22: "the recommended button only applies to the
// current choice and doesn't pick all skills at once"): the step's choices
// taken back and made again in the class's usual order. The game's handler
// (every step at once, and each skill's row found through the list - a skill
// the step's list doesn't hold has none: the 0.16.4 crash) only runs on a page
// nwn5e leaves to the game.
void Recommend(void* page, void* a2, void* a3, void* a4) {
  void* stats = PageStats(page);
  if (!ActiveFor(stats) || page != g_step_page) {
    g_recommend(page, a2, a3, a4);
    return;
  }
  TakeBack(stats, g_page_step);
  RecommendStep(stats, g_page_step);
  ShowStep(page, true, true);
}

// OK (owner, 2026-10-22: "the user needs to press "OK" to move to the next
// step"): once the step's choices are made, the next step; on the last, the
// game's OK (the page closes).
void Ok(void* page, void* a2, void* a3, void* a4) {
  void* stats = PageStats(page);
  if (!ActiveFor(stats) || page != g_step_page) {
    g_ok(page, a2, a3, a4);
    return;
  }
  const Step step = g_page_step;
  if (!StepDone(stats, step)) {
    Log("skills page: OK kept - %d choice(s) left (%s)", StepLeft(stats, step), StepName(step));
    ShowStep(page, false);
    return;
  }
  const Step next = NextStep(step);
  if (next == Step::kDone) {
    g_no_feat_count = page == g_again_page;  // C8: on as the Feats page's OK, not to a Feats page again
    g_ok(page, a2, a3, a4);                  // last: the page may be gone after this
    g_no_feat_count = false;
    return;
  }
  g_page_step = next;
  Log("skills page: OK - on to %s", StepName(next));
  ShowStep(page, true, true);
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
  // Only a choice of the page's step (owner, 2026-10-22: "Skill should only be
  // added or removed to the list between steps"): Reset goes back.
  if (g_step_page) {
    const Step step = g_page_step;
    const bool of_step = step == Step::kExpertise
                             ? ranks[skill] >= 2 && g_level.floor[skill] < 2
                             : ranks[skill] == 1 && g_level.floor[skill] == 0 &&
                                   g_level.pick_step[skill] == static_cast<int8_t>(step);
    if (!of_step) {
      Log("skills page: - kept (not a choice of this step)");
      return;
    }
  }
  g_set_rank(stats, skill, static_cast<int8_t>(ranks[skill] - 1));
  if (ranks[skill] == 0) g_level.pick_step[skill] = -1;
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
  for (int row = 0; ranks && row < Rows(); ++row) {  // the button reset the picks: their steps too
    if (ranks[row] <= g_level.floor[row]) g_level.pick_step[row] = -1;
  }
  for (int pass = 0; pass < 2 && ranks; ++pass) {  // new skills, then Expertise
    for (rules::Skill2024 skill : preference) {
      const int row = rules::SkillsPageRow(skill);
      if (row < 0 || row >= Rows() || PointsLeft(stats) < 1) continue;
      if ((pass == 0 && ranks[row] == 0) || (pass == 1 && ranks[row] == 1)) {
        AddRankAs(stats, static_cast<uint16_t>(row), true);
      }
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

// C8: the Feats page's OK (NextPage with its exit code) when it added Skilled:
// the Skills page again for Skilled's picks, built as the Skills page's OK
// builds a Feats page - the Feats page deactivated with the Skills page's
// code, the exit delegate copied, the page activated, the level-up pointed at
// it - and true: NextPage stops there (the Feats page's OK deletes its page).
bool SkillsAgainOpened(uint32_t creature, int level_up, void* panel, int exit_code) {
  if (!g_skills_again || exit_code != ee::kExitFeatsPage || !panel || Field<uintptr_t>(panel, 0) != g_feats_vtable) {
    return false;
  }
  void* client_creature = g_get_creature ? g_get_creature(creature) : nullptr;
  void* stats = client_creature ? Field<void*>(client_creature, ee::kClientCreatureStats) : nullptr;
  if (!ActiveFor(stats) || stats != g_skilled_stats) return false;
  const int added = CountSkilled(stats) - g_skilled_before;
  if (added <= 0) return false;
  void* page = g_new(ee::kSkillsPageSize);
  if (!page) return false;
  g_level_before_again = g_level;
  g_in_again_ctor = true;
  g_skills_ctor(page, creature, level_up ? 1 : 0);
  g_in_again_ctor = false;
  // The page's one step: Skilled's picks, above the ranks as they are now.
  g_level.skills.class_skills = 0;
  g_level.skills.species_any_skill = false;
  g_level.skills.species_list = 0;
  g_level.expertise.expertise = 0;
  g_level.skilled_picks = 3 * added;
  const int8_t* ranks = Ranks(stats);
  for (int row = 0; row < rules::kNwnSkillCount; ++row) {
    g_level.floor[row] = ranks && row < Rows() ? ranks[row] : 0;
    g_level.pick_step[row] = -1;
  }
  g_level.points = g_level.skilled_picks;
  SetPointsLeft(stats, g_level.skilled_picks);
  if (level_up) SetField<uint16_t>(page, ee::kSkillsPagePointsAtOpen, static_cast<uint16_t>(g_level.skilled_picks));
  g_again_page = page;
  g_deactivate_modal(panel, ee::kExitSkillsPage);
  memcpy(static_cast<uint8_t*>(page) + ee::kPageExitObject, static_cast<uint8_t*>(panel) + ee::kPageExitObject, 16);
  g_activate_modal(page);
  if (level_up) {
    const auto* holder = *reinterpret_cast<void* const* const*>(g_app_global);
    void* app = holder ? *holder : nullptr;
    if (void* level_up_object = app ? g_get_level_up(app) : nullptr) SetField<void*>(level_up_object, ee::kLevelUpPage, page);
  }
  Log("skills again: %d Skilled feat(s) taken - the Skills page again for %d pick(s)", added, g_level.skilled_picks);
  return true;
}

// The again page's OK: NextPage goes on as after the Feats page.
int SkillsAgainExitCode(void* panel, int exit_code) {
  if (panel && panel == g_again_page && exit_code == ee::kExitSkillsPage) return ee::kExitFeatsPage;
  return exit_code;
}

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
  // The page: its list builder (the one function with the row model's name),
  // its rows' "+" / "-" (stored by the builder), the client's GetCreature (its
  // first call), the Recommended button (registered by the page's init).
  const uintptr_t build = FunctionByString("skills page: the list", "CTL_CG_BTN_SKILL");
  const CodeRange build_code = build ? FunctionAt(build) : CodeRange();
  auto stored_handler = [&](const char* what, const char* pattern) -> uintptr_t {
    const uintptr_t lea = build ? FindPatternIn(what, build_code, pattern) : 0;
    return lea ? lea + 7 + *reinterpret_cast<const int32_t*>(lea + 3) : 0;
  };
  const uintptr_t plus = stored_handler("skills page: the + handler", "48 8D 05 ?? ?? ?? ?? 48 89 86 A0 01 00 00");
  const uintptr_t minus = stored_handler("skills page: the - handler", "48 8D 05 ?? ?? ?? ?? 48 89 86 90 02 00 00");
  const uintptr_t creature_call = build ? FindPatternIn("skills page: GetCreature", build_code, "8B 89 80 0F 00 00 E8") : 0;
  g_get_creature = reinterpret_cast<GetCreatureFn>(creature_call ? CallTarget(creature_call + 6) : 0);
  const uintptr_t init = FunctionByString("skills page: its init", "SkillsButtonBox");
  const uintptr_t recommend = init ? HandlerByTag("skills page: Recommended", init, "RecommendButton") : 0;
  const uintptr_t ok = init ? HandlerByTag("skills page: OK", init, "OkButton") : 0;
  // A row's click (the list's select callback), checked by its disabled-row test.
  const uintptr_t row_click = init ? HandlerByTag("skills page: a row's click", init, "SkillsButtonBox") : 0;
  const bool row_click_found =
      row_click && FindPatternIn("skills page: a row's disabled test", FunctionAt(row_click), ee::kSkillsRowDisabledTest);
  // C8: the Skills page's constructor (the one function referencing its vtable),
  // the Feats page's vtable, the modal panel's Activate / Deactivate, the OK's
  // CalcNumberFeats and GetLevelUp calls.
  g_set_enabled = reinterpret_cast<SetEnabledFn>(EngineExport(ee::kCheckboxSetEnabled));
  g_set_color = reinterpret_cast<SetColorFn>(EngineExport(ee::kStringWrapperSetColor));
  g_skill_useable = reinterpret_cast<SkillUseableFn>(EngineExport(ee::kClassIsSkillUseable));
  g_delete_buttons = reinterpret_cast<DeleteButtonsFn>(EngineExport(ee::kListboxDeleteButtons));
  // The Reset button: the engine's allocator (an anchor inside CGuiPanel::Activate, as chargen_ui.cpp
  // finds it), the text button's constructor, the layout import, the page's deleting destructor.
  const uintptr_t new_site = FindPatternIn("skills page: operator new", FunctionAt(EngineExport(ee::kGuiPanelActivate)),
                                           ee::kOperatorNewPattern);
  g_new = reinterpret_cast<NewFn>(new_site ? CallTarget(new_site + ee::kOperatorNewCall) : 0);
  g_text_button_ctor = reinterpret_cast<ObjectFn>(EngineExport(ee::kTextButtonConstructor));
  g_import = reinterpret_cast<ImportFn>(EngineExport(ee::kImportLayoutData));
  g_text_button_enabled = reinterpret_cast<TextButtonEnabledFn>(EngineExport(ee::kTextButtonSetEnabled));
  const uintptr_t page_vtable = FindVtable("skills page: its vtable", ee::kRttiSkillsPage);
  const uintptr_t page_dtor = page_vtable ? VtableSlot(page_vtable, 0) : 0;
  g_set_text = reinterpret_cast<SetTextFn>(EngineExport(ee::kStringWrapperSetText));
  g_make_string = reinterpret_cast<ExoStringFn>(EngineExport(ee::kExoStringFromChars));
  g_destroy_string = reinterpret_cast<ExoStringDestroyFn>(EngineExport(ee::kExoStringDestructor));
  if (!points || !add || !remove || !package || !g_get_class || !g_get_level || !g_set_rank || !build || !plus ||
      !minus || !g_get_creature || !recommend || !ok || !g_set_enabled || !g_set_color || !g_skill_useable || !g_delete_buttons || !g_set_text || !g_make_string ||
      !g_destroy_string || !g_new || !g_text_button_ctor || !g_import || !g_text_button_enabled || !page_dtor) {
    Log("SKIP skills page: a client function not found - the Skills page stays NWN's");
    return;
  }
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  Log("skills page: points rva 0x%zx, + 0x%zx, - 0x%zx, package 0x%zx, rank setter 0x%zx, list 0x%zx, + button 0x%zx, "
      "- button 0x%zx, Recommended 0x%zx, OK 0x%zx", points - base, add - base, remove - base, package - base,
      reinterpret_cast<uintptr_t>(g_set_rank) - base, build - base, plus - base, minus - base, recommend - base,
      ok - base);
  const bool hooked =
      HookAddress("client skill points (C5: the 2024 picks at level 1, none later)", points,
                  reinterpret_cast<void*>(&Points), reinterpret_cast<void**>(&g_points)) &&
      HookAddress("client Skills page + (C5: a 2024 pick)", add, reinterpret_cast<void*>(&AddRank),
                  reinterpret_cast<void**>(&g_add_rank)) &&
      HookAddress("client Skills page - (C5)", remove, reinterpret_cast<void*>(&RemoveRank),
                  reinterpret_cast<void**>(&g_remove_rank)) &&
      HookAddress("client package skills (C5: Recommended)", package, reinterpret_cast<void*>(&PackageSkills),
                  reinterpret_cast<void**>(&g_package_skills)) &&
      HookAddress("client Skills page list (the background's ranks, the steps' greying)", build,
                  reinterpret_cast<void*>(&BuildList), reinterpret_cast<void**>(&g_build_list)) &&
      HookAddress("client Skills page + button (the steps' greying)", plus, reinterpret_cast<void*>(&Plus),
                  reinterpret_cast<void**>(&g_plus)) &&
      HookAddress("client Skills page - button (the steps' greying)", minus, reinterpret_cast<void*>(&Minus),
                  reinterpret_cast<void**>(&g_minus)) &&
      HookAddress("client Skills page Recommended (the step's choices)", recommend, reinterpret_cast<void*>(&Recommend),
                  reinterpret_cast<void**>(&g_recommend)) &&
      HookAddress("client Skills page OK (the next step)", ok, reinterpret_cast<void*>(&Ok),
                  reinterpret_cast<void**>(&g_ok)) &&
      HookAddress("client Skills page deleting destructor (its Reset button first)", page_dtor,
                  reinterpret_cast<void*>(&PageDeletingDtor), reinterpret_cast<void**>(&g_page_dtor));
  if (!hooked) return;
  {
    const std::vector<uintptr_t> vtable_refs = page_vtable ? FindRipReferences(page_vtable) : std::vector<uintptr_t>();
    g_skills_ctor = reinterpret_cast<SkillsCtorFn>(vtable_refs.size() == 1 ? FunctionEntry(vtable_refs[0]) : 0);
    g_feats_vtable = FindVtable("skills again: the Feats page's vtable", ee::kRttiFeatsPageC8);
    g_activate_modal = reinterpret_cast<PanelFn>(EngineExport(ee::kGuiModalPanelActivate));
    g_deactivate_modal = reinterpret_cast<DeactivateFn>(EngineExport(ee::kGuiModalPanelDeactivate));
    const CodeRange ok_code = FunctionAt(ok);
    const uintptr_t count_site =
        FindPatternIn("skills again: the OK's CalcNumberFeats", ok_code, ee::kSkillsOkFeatCountCall);
    const uintptr_t feat_count = count_site ? CallTarget(count_site + ee::kSkillsOkFeatCountCallAt) : 0;
    const uintptr_t level_up_site = FindPatternIn("skills again: the OK's GetLevelUp", ok_code, ee::kSkillsOkLevelUpPage);
    if (level_up_site) {
      g_app_global = level_up_site + 7 + *reinterpret_cast<const int32_t*>(level_up_site + 3);
      g_get_level_up = reinterpret_cast<GetLevelUpFn>(CallTarget(level_up_site + ee::kSkillsOkLevelUpCallAt));
    }
    g_skills_again = g_skills_ctor && g_feats_vtable && g_activate_modal && g_deactivate_modal && feat_count &&
                     g_app_global && g_get_level_up &&
                     HookAddress("client CalcNumberFeats (C8: no Feats page after the Skills page again)", feat_count,
                                 reinterpret_cast<void*>(&FeatCount), reinterpret_cast<void**>(&g_feat_count));
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (g_skills_again) {
      Log("skills again (C8): the Skills page's constructor rva 0x%zx, CalcNumberFeats 0x%zx, GetLevelUp 0x%zx - "
          "Skilled taken on the Feats page opens the Skills page again",
          reinterpret_cast<uintptr_t>(g_skills_ctor) - base, feat_count - base,
          reinterpret_cast<uintptr_t>(g_get_level_up) - base);
    } else {
      Log("SKIP skills again (C8): an anchor is missing (%zu constructor reference(s)) - Skilled's picks are made for "
          "the character when it enters (!skills changes them)",
          vtable_refs.size());
    }
  }
  if (!row_click_found ||
      !HookAddress("client Skills page row click (a greyed skill's own description)", row_click,
                   reinterpret_cast<void*>(&RowClick), reinterpret_cast<void**>(&g_row_click))) {
    Log("SKIP skills page: a greyed row still says \"You may not learn this skill\" (its click not found)");
  }
  g_installed = true;
  ServeConditionalRows("skills2024");
  ServeSkillsPageLayout();  // the Reset button's place (data/gui/skills/cg_skills.gui)
  Log("skills page: the 2024 pick - the class's skills, a species pick, the background's taken (skills faithful)");
}
