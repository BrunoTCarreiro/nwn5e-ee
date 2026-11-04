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

// #130 (owner, 2026-11-04: "1A (later we'll check if it's too small), 2B, 3A, 4B"), a house rule - not SRD 5.2, off
// by default (nwn5e.ini [house_rules] flask_patches) - after the 2024 Oil flask's doused space: Acid, Alchemist's Fire
// and Holy Water thrown at the ground (no creature, door or placeable at the target) make a patch one creature wide
// (1A: a 5-ft square - its area of effect's 0.83 m radius) for 2 rounds. A creature that enters it or ends its round
// there takes the flask's dice (2B: GrenadeHitDice), no save (3A), at most once a round; whom it hurts is NWN's
// area-spell rule (4B: allies only above Normal difficulty); Holy Water's hurts only undead and evil outsiders. Only
// with the grenades faithful (core/grenade_gear.cpp).
bool FlaskMakesPatch(Grenade grenade, bool grenades_faithful, bool house_rule);
constexpr int kFlaskPatchRounds = 2;
// Once a round: a creature a patch hurt isn't hurt by it again for 5 s - under the 6-second round, so the patch's
// round tick (every 6 s) still finds it next round.
constexpr float kFlaskPatchLockSeconds = 5.0f;
// The patch's area of effect, a vfx_persistent.2da row (data/rows/vfx_persistent.flasks.txt): Acid 100, Alchemist's
// Fire 101, Holy Water 102; -1 for any other.
int FlaskPatchAreaEffect(Grenade grenade);

// 1A: mid-round a player's throw goes at once and takes one of the round's remaining swings - the attack of the
// Attack action it replaces (none left: one of the next round's).
bool ThrowGoesNow(bool attacking_mid_round, bool grenade, bool faithful);

// 0.71.0 (owner, 2026-11-04: "one more thing, bombs (poison and fire) along with
// thunderstone should tunr off torches and other such light sources in range of the explosion as they blow up"): the
// radius in which a grenade's blast puts out the lit lights (core/light_sources.cpp's BlastPutsOutLights) - the bombs'
// 5 m (their cloud, their aiming circle), the Thunderstone's 3.33 m (its bang, NWN's RADIUS_SIZE_MEDIUM); 0 for the
// rest (the flasks, Tanglefoot Bag, Choking Powder, Caltrops - no blast).
float GrenadeBlastRadius(Grenade grenade);

// #132 (owner, 2026-11-04: "go with your recommendations" - A): a grenade thrown from hiding keeps the thrower hidden -
// the 2024 Hide ends on an attack roll, and a grenade's target makes a save. NWN's engine takes the thrower out of
// stealth mode as the throw starts (the item cast's ActivityManager(1)). Faithful only; a spell or another item keeps
// NWN's (a spell with a Verbal component ends the 2024 Hide too).
bool ThrowKeepsHiding(bool hiding, bool grenade_throw, bool faithful);

// The item's description in 2024 terms (nwn5e's string, 16000555-16000563) for a grenade's in dialog.tlk (the
// blueprints' 2989-2997, 84353-84354); with the flask patches' house rule (#130) on, the three flasks' name it
// (16000564-16000566). 0 for any other.
unsigned GrenadeDescription2024(unsigned game_strref, bool flask_patches);

}  // namespace rules
