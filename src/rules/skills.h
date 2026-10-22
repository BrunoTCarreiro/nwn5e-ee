#pragma once

// Phase 2 skills (PLAN.md "Phase 2 — skills and checks", decided 2026-10-19):
// NWN's skill rows as the 2024 (SRD 5.2) skills and tools, proficiency instead
// of ranks, and what a character can choose. The engine side reads the
// creature; these functions only decide.

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "rules/combat.h"
#include "rules/saves.h"

namespace rules {

// The 2024 skills, then the tools nwn5e maps NWN skills to.
enum class Skill2024 {
  kAcrobatics, kAnimalHandling, kArcana, kAthletics, kDeception, kHistory, kInsight, kIntimidation,
  kInvestigation, kMedicine, kNature, kPerception, kPerformance, kPersuasion, kReligion, kSleightOfHand,
  kStealth, kSurvival, kThievesTools, kSmithsTools,
  kCount
};
constexpr int kSkill2024Count = static_cast<int>(Skill2024::kCount);
bool IsTool(Skill2024 skill);
const char* Skill2024Name(Skill2024 skill);
Ability AbilityFor(Skill2024 skill);  // 2024 key ability (Smith's Tools: Strength)

// skills.2da rows: NWN's 28 (0-27), then the seven nwn5e adds for the 2024
// skills NWN has no row for (C5, data/rows/skills.added.txt; no use in NWN, the
// Skills page and the sheet show them).
enum NwnSkill {
  kNwnAnimalEmpathy = 0, kNwnConcentration, kNwnDisableTrap, kNwnDiscipline, kNwnHeal, kNwnHide, kNwnListen,
  kNwnLore, kNwnMoveSilently, kNwnOpenLock, kNwnParry, kNwnPerform, kNwnPersuade, kNwnPickPocket, kNwnSearch,
  kNwnSetTrap, kNwnSpellcraft, kNwnSpot, kNwnTaunt, kNwnUseMagicDevice, kNwnAppraise, kNwnTumble, kNwnCraftTrap,
  kNwnBluff, kNwnIntimidate, kNwnCraftArmor, kNwnCraftWeapon, kNwnRide,
  kNwnAthletics, kNwnHistory, kNwnInsight, kNwnInvestigation, kNwnNature, kNwnReligion, kNwnSurvival,
  kNwnSkillCount
};
constexpr int kNwnOwnSkillCount = kNwnAthletics;  // NWN's own rows (0-27)

// SK1: the 2024 skill or tool an NWN skill row stands for in faithful mode;
// none for Concentration, Discipline, Parry, Taunt, Appraise, Use Magic Device
// and Craft Trap (SK6–SK8, Phase 3), which keep NWN's value until then.
std::optional<Skill2024> Skill2024ForNwnSkill(int nwn_skill);

// SK1: the faithful name of an NWN skill row as the game shows it, e.g. "Stealth
// (Hide)"; nullptr for rows without a 2024 skill.
const char* Faithful2024SkillLabel(int nwn_skill);
// The same, by the dialog.tlk string the game asks for (skills.2da Name).
const char* Faithful2024SkillLabelForStrref(uint32_t strref);

// "stealth", "sleight of hand", "thieves' tools", ... (any case; spaces,
// apostrophes and hyphens ignored); nullopt if unknown.
std::optional<Skill2024> ParseSkill2024(const std::string& text);
// Splits "stealth sleight of hand perception" into skills; on an unknown word
// returns false and puts it in `unknown`.
bool ParseSkill2024List(const std::string& text, std::vector<Skill2024>* skills, std::string* unknown);

// SK2: ability modifier + proficiency bonus if proficient + again with
// Expertise + item and spell effects.
int SkillBonus2024(int ability_modifier, int proficiency_bonus, bool proficient, bool expertise, int effect_bonus);

// SK3 (a): a skill value a *script* reads (the official campaigns' AutoDC
// checks, DCs set for 3e totals), scaled to the 3e range — about twice the 2024
// bonus — so conversation checks stay as passable as in NWN.
int ScriptSkillValue2024(int skill_bonus);

// SK3 (faithful): an NWN skill DC on the 2024 scale — 10 + (DC - 10) / 2,
// rounded as saves' C6 (3e 15 -> 13, 20 -> 15, 30 -> 20, 40 -> 25) — but a
// DC of 1 or less stays (NWN's "always works" floor) and nothing is clamped
// (a DC 100 lock stays out of reach).
int ConvertNwnSkillDc(int dc);

// A set of skills (bit k = Skill2024 k).
using SkillSet = uint32_t;
inline SkillSet SkillBit(Skill2024 skill) { return 1u << static_cast<int>(skill); }
inline bool Has(SkillSet set, Skill2024 skill) { return (set & SkillBit(skill)) != 0; }
int CountSkills(SkillSet set);

// What a character can choose (2024): class skills (first class: its count from
// its list; multiclassing into Bard, Ranger or Rogue: one more from that list),
// background (2 skills, 1 tool), species (Human: 1 skill; Elf: Insight,
// Perception or Survival), the Rogue's Thieves' Tools, and Expertise (Rogue 1 /
// 6, Bard 2 / 9, Ranger 2 / 9, Wizard 2).
enum class Species2024 { kOther, kHuman, kElf };
struct SkillAllowance {
  int class_skills = 0;
  SkillSet class_list = 0;      // the skills the class picks come from
  int multiclass_skills = 0;    // picks from multiclass_list
  SkillSet multiclass_list = 0;
  int background_skills = 2;
  int background_tools = 1;
  bool species_any_skill = false;   // Human: Skillful
  SkillSet species_list = 0;        // Elf: Keen Senses
  int feat_skills = 0;              // the Skilled feat: 3 skills of any kind each
  int lore_skills = 0;              // C7b: Bard 3, College of Lore's Bonus Proficiencies (any)
  int primal_skills = 0;            // C7b: Barbarian 3, Primal Knowledge (the Barbarian's list)
  SkillSet primal_list = 0;
  SkillSet granted = 0;             // always proficient (Rogue: Thieves' Tools)
  int expertise = 0;
  SkillSet expertise_list = 0;      // 0 = any proficient skill
  int TotalSkills() const;          // skill choices, tools excluded
};
SkillAllowance SkillAllowanceFor(const std::vector<ClassLevels>& classes, Species2024 species,
                                 int skilled_feats = 0);

// Can `chosen` (skills and tools, `granted` excluded or included) be split
// into those choices? Returns an empty string when it can, else the reason.
std::string CheckSkillChoice(const SkillAllowance& allowance, SkillSet chosen);

// C8: skills `proficient` still has room for in `allowance`, taken in
// `preference` order (Recommended's) and then any other (tools excluded), at
// most `most` of them - the Skilled feat's picks for a character that took it
// without the Skills page (a build's Recommended).
std::vector<Skill2024> FillSkillChoices(const SkillAllowance& allowance, SkillSet proficient,
                                        const std::vector<Skill2024>& preference, int most);
std::string CheckExpertiseChoice(const SkillAllowance& allowance, SkillSet proficient, SkillSet expertise);

// SK5 (faithful): an observer's Perception die — the d20 it rolled when it
// searches (detect mode: the Search action), else 10 (passive Perception).
int PerceptionDie(bool searching, int d20);

// SK11 (faithful): a passive check's base — 10, or 5 with disadvantage (2024:
// disadvantage subtracts 5 from a passive score).
int PassiveCheckBase(bool disadvantage);
// SK11: the d20 kept from a check's dice — `second` is 0 when only one was
// rolled; advantage and disadvantage cancel.
int KeptCheckD20(int first, int second, int advantage, int disadvantage);

// C5 (docs/CHARGEN.md, faithful): the creation screen's Skills page as the
// 2024 pick. The page shows one skills.2da row per 2024 skill (below) and
// hides the others; a rank there is the proficiency. -1 for the tools (a
// class or background gives them, not the page).
int SkillsPageRow(Skill2024 skill);
bool IsSkillsPageRow(int nwn_skill);
// The level-1 picks the page takes: the class's count from its list, a
// Human's Skillful (any skill) or an Elf's Keen Senses (Insight, Perception
// or Survival). The background's skills are given already; Skilled, Expertise
// and later levels' picks stay with !skills.
SkillAllowance SkillsPageAllowance(CharacterClass first_class, Species2024 species);
int SkillsPagePoints(const SkillAllowance& page);
// Whether the page may add `skill` to its picks so far (`picked`), the
// background's skills (`background`) being taken already.
bool CanPickOnSkillsPage(const SkillAllowance& page, SkillSet background, SkillSet picked, Skill2024 skill);
// The page's Recommended (and the Background page's OK): the class's usual
// skills in order, then common ones for the species pick, each taken while it
// still fits.
SkillSet RecommendedSkillsPagePicks(CharacterClass first_class, Species2024 species, SkillSet background);
// A class's usual skills in order, then common ones (Recommended's order).
std::vector<Skill2024> SkillPreference2024(CharacterClass character_class);

// C7b: what a level adds on the Skills page — new skills (from `skill_list`)
// and Expertise (in a skill the character is proficient in, from
// `expertise_list`, 0 = any). A class's level 1 as the first class: its
// skills (the page adds the species pick); multiclassing into a Bard (any),
// Ranger or Rogue: one of its list; Barbarian 3 (Primal Knowledge): one of
// its list; Bard 3 (College of Lore): three of any. Expertise: Rogue 1 and 6
// (2 each), Bard 2 and 9 (2), Ranger 2 (1, Deft Explorer) and 9 (2), Wizard 2
// (1 of Arcana, History, Investigation, Medicine, Nature, Religion: Scholar).
struct LevelSkillPicks {
  int skills = 0;
  SkillSet skill_list = 0;
  int expertise = 0;
  SkillSet expertise_list = 0;
};
LevelSkillPicks LevelSkillPicks2024(CharacterClass character_class, int class_level, bool first_class);
// Whether one more Expertise may go to `skill` (the caller checks it is
// proficient), `taken` already chosen at this level.
bool CanPickExpertise(const LevelSkillPicks& picks, int taken, Skill2024 skill);
// Whether a character's ranks are the 2024 record (C5 / C7b): ranks only on
// the Skills page's rows, 1 (proficient) or 2 (Expertise). An older
// character's NWN ranks aren't.
bool RanksAre2024(const int8_t* ranks, int rows);

// SK9 (faithful): the 2024 initiative modifier — Dexterity, + the proficiency
// bonus with Alert (NWN's Improved Initiative).
int InitiativeModifier2024(int dexterity_modifier, int proficiency_bonus, bool alert);

// SK5 B (faithful): is a hider at (dx, dy) from an observer facing (fx, fy)
// in front of it (ahead of its shoulders) and within `metres`? Then, with a
// clear line between them, the observer finds it (2024 hides you only out of
// sight).
bool InFrontWithin(float dx, float dy, float fx, float fy, float metres);

}  // namespace rules
