#pragma once

// Phase 3 CM9 / CM10 (PLAN.md): the 2024 class features, level by level, and
// each class's SRD 5.2 subclass (granted at class level 3). The status of
// each feature in nwn5e is part of the table, so the game can show what
// works. Filled one class at a time, each with its own brief.

#include <vector>

#include "rules/combat.h"

namespace rules {

constexpr int kSubclassLevel = 3;

// The SRD 5.2 subclass of each class (one per class).
const char* Subclass2024(CharacterClass character_class);

struct ClassFeature {
  int level;
  const char* name;
  bool subclass;       // a feature of the SRD subclass
  const char* status;  // "built", "built: <how>", or the decision / phase it waits for
};

// The class's features up to `level` (all of them for level 20); empty for a
// class whose brief hasn't been written yet.
std::vector<ClassFeature> ClassFeatures2024(CharacterClass character_class, int level);

// Has the class's brief been written (its features listed)?
bool HasFeatureList(CharacterClass character_class);

// Fighter (PLAN.md "Phase 3 — Fighter"), by Fighter level:
// C1 Champion's Improved / Superior Critical: the lowest d20 that crits with
// weapons and unarmed strikes (20; 19 from 3; 18 from 15).
int ChampionCriticalFloor(int fighter_level);
// C2 Remarkable Athlete (Champion 3): advantage on initiative.
bool HasRemarkableAthlete(int fighter_level);
// F6 Studied Attacks (13): a miss gives advantage on the next attack at that
// creature before the end of your next turn.
bool HasStudiedAttacks(int fighter_level);

// F5 Indomitable: rerolls of a failed save per long rest (1 at 9, 2 at 13, 3 at 17).
int IndomitableUses(int fighter_level);
// C5 Survivor's Heroic Rally (Champion 18): Bloodied = at most half the
// maximum; at the start of each turn, Bloodied and above 0, regain 5 + Con.
bool IsBloodied(int hit_points, int max_hit_points);
int HeroicRallyHitPoints(int con_modifier);
bool HasHeroicRally(int fighter_level);

// F1: the Fighting Style feats (data/rows/feat.styles.txt), feat.2da rows.
constexpr int kFeatFightingStyleArchery = 1210;
constexpr int kFeatFightingStyleDefense = 1211;
constexpr int kFeatFightingStyleGreatWeapon = 1212;
constexpr int kFeatFightingStyleTwoWeapon = 1213;
// The class levels with a Fighting Style feat slot: Fighter 1, and 7 (the
// Champion's Additional Fighting Style). Paladin / Ranger 2 with their briefs.
bool IsFightingStyleLevel2024(CharacterClass character_class, int class_level);
constexpr int kArcheryAttackBonus = 2;
constexpr int kDefenseArmorClassBonus = 1;
// Great Weapon Fighting: a damage die of 1 or 2 counts as 3.
int GreatWeaponFightingDie(int roll);
// The NWN base items with 2024's Two-Handed or Versatile property (the
// double weapons are two weapons in NWN, so not here); NWN-only weapons as
// their nearest 2024 weapon (bastard sword, katana, dwarven waraxe, magic
// staff: versatile; heavy flail, scythe: two-handed).
bool IsTwoHandedWeapon2024(int base_item);
bool IsVersatileWeapon2024(int base_item);

// F2: Second Wind, one feat row per number of uses (data/rows/feat.fighter.txt).
constexpr int kFeatSecondWind2 = 1220;  // Fighter 1
constexpr int kFeatSecondWind3 = 1221;  // Fighter 4
constexpr int kFeatSecondWind4 = 1222;  // Fighter 10

// Barbarian (PLAN.md "Phase 3 — Barbarian", decided 2026-10-24), by
// Barbarian level: B5 Danger Sense (2: advantage on Dexterity saves unless
// Incapacitated), B8 Fast Movement (5: +10 ft out of Heavy armour), B9 Feral
// Instinct (7: advantage on initiative), B13 Primal Champion (20: Strength
// and Constitution +4, at most 25).
bool HasDangerSense(int barbarian_level);
bool HasBarbarianFastMovement(int barbarian_level);
bool HasFeralInstinct(int barbarian_level);
bool HasPrimalChampion(int barbarian_level);
// B8: +10 ft of a 30 ft speed (NWN's movement-rate factor).
constexpr float kBarbarianFastMovementFactor = 1.0f / 3.0f;
// B4 Unarmored Defense: the AC an unarmoured Barbarian adds - its Constitution
// modifier (nothing when negative) - less a Monk's Wisdom bonus the engine
// already gives it (X5: the higher of the two Unarmored Defenses, not both).
int UnarmoredDefenseConBonus(int con_modifier, int monk_wis_bonus);
// NWN heavy armour: a chest item of base AC 6 or more (NWN's own heavy range).
constexpr int kHeavyArmorBaseAc = 6;

// X1 (owner, 2026-10-24: "remove them completely"): NWN class feats with no
// 2024 counterpart, taken off characters in faithful and no longer granted
// (the classes' feat tables, data/rows/cls_feat_*.if_classfeat2024.txt).
// So far the Barbarian's: Uncanny Dodge 1-6, Damage Reduction 1-4 and epic.
const std::vector<int>& NwnOnlyClassFeats();

// The feats nwn5e grants a character for its class levels (the DLL adds them —
// so characters made earlier get them too — and removes them in NWN mode).
std::vector<int> GrantedFeats2024(const std::vector<ClassLevels>& classes);
// Every feat nwn5e may grant, in a fixed order (the record kept on a character).
const std::vector<int>& GrantableFeats2024();

}  // namespace rules
