#pragma once

// #124 (owner, 2026-11-04: "good recs, go with them" - 1A 2A 3A 4A 5A; docs/PLAN.md, Phase 6): NWN's grenades (base
// item 81; spells.2da 464-471, 744-745) as 2024 gear - core/grenade_gear.cpp, core/action_order.cpp,
// core/round_swings.cpp, core/rules_save_roll.cpp.

namespace rules {

enum class Grenade {
  kNone,
  kAlchemistsFire,
  kTanglefoot,
  kHolyWater,
  kChokingPowder,
  kThunderstone,
  kAcid,
  kCaltrops,
  kFireBomb,
  kAcidBomb,
};
constexpr int kBaseItemGrenade = 81;

Grenade GrenadeForSpell(int spell_id);  // 464-469, 471, 744, 745 (470, the joke Chickenegg: none)
bool IsGrenadeSpell(int spell_id);

// The thrower's save DC (2024: 8 + its Dexterity modifier + its Proficiency Bonus); a trap's or a placeable's: 15.
int GrenadeSaveDc(int dexterity_modifier, int proficiency_bonus);
constexpr int kGrenadeObjectDc = 15;
// A grenade's save on the thrower's DC: all but Caltrops (2024's fixed DC 15).
bool GrenadeSaveOnThrowerDc(Grenade grenade);

// 2A / 5A: the damage on a failed save - Acid 2d6, Alchemist's Fire 1d4, Holy Water 2d8, the bombs 10d6 (half on a
// success); none for the others.
struct GrenadeDice {
  int count = 0;
  int sides = 0;
};
GrenadeDice GrenadeHitDice(Grenade grenade);
bool GrenadeHalfOnSave(Grenade grenade);  // the bombs

// 3A, burning (Alchemist's Fire): 1d4 Fire at the start of each of its next 3 rounds.
constexpr int kBurningRounds = 3;
constexpr int kBurningDieSides = 4;

// 4A, Caltrops: NWN's 5 m field, 2024's effect - entering, a DC 15 Dexterity save or 1 Piercing and stopped until its
// next round; the field gone after 25 points (NWN's).
constexpr int kCaltropsSaveDc = 15;
constexpr int kCaltropsDamage = 1;
constexpr int kCaltropsFieldDamage = 25;

// 1A: mid-round a player's throw goes at once and takes one of the round's remaining swings - the attack of the
// Attack action it replaces (none left: one of the next round's).
bool ThrowGoesNow(bool attacking_mid_round, bool grenade, bool faithful);

// The item's description in 2024 terms (nwn5e's string, 16000555-16000563) for a grenade's in dialog.tlk (the
// blueprints' 2989-2997, 84353-84354); 0 for any other.
unsigned GrenadeDescription2024(unsigned game_strref);

}  // namespace rules
