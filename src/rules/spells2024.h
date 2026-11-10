#pragma once

#include <stdint.h>

#include <vector>

#include "rules/grapple.h"  // FeetToMetres
#include "rules/saves.h"

// #39 Part 2a (owner, 2026-11-09: "go A, level by level", then "A on all, but mark featherfall for later removal"),
// `spellcasting` faithful: SRD 5.2.1's levelled spells, cast by core/rules_spells2024.cpp. Build 1a: the level 1
// damage and healing spells - Burning Hands, Magic Missile, Guiding Bolt, Inflict Wounds, Ray of Sickness, Cure
// Wounds, False Life, Dissonant Whispers, Thunderwave, Ice Knife. Their numbers, their areas and their upcasting.
// Build 1b (owner, 2026-11-09: "go for 1b"): Healing Word (a Bonus Action spell that spends a slot), Chromatic Orb
// (a damage-type menu, its orb leaping on matching dice), Heroism (Concentration: Temporary Hit Points each turn).

namespace rules {

enum class SpellEffect2024 {
  kSpellAttack,         // a spell attack: its dice on a hit, twice the dice on a Critical Hit
  kSave,                // a saving throw: its dice on a failure; half (rounded down) on a success if `half_on_save`
  kDarts,               // Magic Missile: darts that always hit, each its own damage
  kHealing,             // Hit Points regained: its dice + the spellcasting ability modifier
  kTemporaryHitPoints,  // False Life: its dice + `flat`
  kBuff,                // Heroism: no roll
};

enum class SpellShape2024 {
  kOne,             // one creature
  kCone,            // a Cone from the caster toward the point aimed at, `feet` long
  kCubeFromCaster,  // a Cube with one face on the caster, toward the point aimed at, `feet` on a side
};

struct Spell2024 {
  int row = -1;              // spells.2da
  const char* name = "";
  const char* script = "";   // its ImpactScript (core/rules_spells2024.cpp runs it)
  int level = 1;
  SpellEffect2024 effect = SpellEffect2024::kSave;
  bool melee = false;        // a Touch spell
  Ability save = Ability::kDexterity;
  bool half_on_save = false;
  int dice = 0;              // per target (Magic Missile: per dart)
  int die = 0;
  int flat = 0;              // + flat (Magic Missile's dart, False Life)
  int upcast_dice = 0;       // dice more for each slot level above `level`
  int upcast_flat = 0;       // False Life: 5 more for each slot level above 1
  uint32_t damage_type = 0;  // the engine's damage type (cantrips.h's constants; 0: none)
  SpellShape2024 shape = SpellShape2024::kOne;
  int feet = 0;              // the area's size
  bool bonus_action = false;  // cast with a Bonus Action (Healing Word)
};

// The spells built (ascending by row) and one by its row (null: not one of them).
const std::vector<Spell2024>& Spells2024();
const Spell2024* Spell2024Of(int row);

// The new rows (spells.if_spells2024.level1.txt).
constexpr int kGuidingBolt = 1147;
constexpr int kRayOfSickness = 1148;
constexpr int kFalseLife = 1149;
constexpr int kDissonantWhispers = 1150;
// NWN's rows rewritten.
constexpr int kBurningHands = 10;
constexpr int kCureWounds = 32;     // NWN's Cure Light Wounds
constexpr int kMagicMissile = 107;
constexpr int kInflictWounds = 432;  // NWN's Inflict Light Wounds
constexpr int kThunderwave = 436;    // NWN's Balagarn's Iron Horn
constexpr int kIceKnife = 543;       // NWN's Ice Dagger
constexpr int kNwnShield = 417;      // NWN's Shield (until build 3): no Magic Missile damage while it lasts
// Build 1b.
constexpr int kHealingWord = 1151;
constexpr int kHealingWordBonus = 1152;  // its Bonus Action: the row its feat casts (no class lists)
constexpr int kFeatHealingWord = 1555;   // the feat carrying it (feat.2da SPELLID 1152)
constexpr int kChromaticOrb = 1153;      // its six types, 1154-1159: cantrips.h's choice menus
constexpr int kChromaticOrbAcid = 1154;
constexpr int kChromaticOrbCold = 1155;
constexpr int kChromaticOrbFire = 1156;
constexpr int kChromaticOrbLightning = 1157;
constexpr int kChromaticOrbPoison = 1158;
constexpr int kChromaticOrbThunder = 1159;
constexpr int kHeroism = 1160;
constexpr int kHealingWordFeet = 60;
constexpr int kChromaticOrbLeapFeet = 30;
// nwn5e: an upcast Heroism's extra creatures - the friends this close to you ("a willing creature you touch").
constexpr int kHeroismReachFeet = 10;

// The slot level a cast spends: the one chosen (0: the spell's own), never below the spell's own, at most 9.
int CastSlotLevel(int spell_level, int chosen);
// The dice at a slot level: `dice`, and `per_level` more for each slot level above the spell's.
int DiceAtSlot(int dice, int per_level, int spell_level, int slot_level);
int SpellDiceAt(const Spell2024& spell, int slot_level);
// A Critical Hit rolls the attack's damage dice twice.
int CriticalDice(int dice, bool critical);
// The damage after a save: all of it on a failure; on a success half (rounded down) or none.
int DamageAfterSave(int damage, bool saved, bool half_on_save);
// Magic Missile: three darts, one more for each slot level above 1.
int MagicMissileDarts(int slot_level);
// False Life: the 2d4 roll + 4, and 5 more for each slot level above 1.
int FalseLifeTemporaryHitPoints(int roll, int slot_level);
// Temporary Hit Points don't stack: new ones replace those you have only when there are more of them.
bool TemporaryHitPointsReplace(int current, int offered);
// Ice Knife: the shard's 1d10 Piercing on a hit, then its explosion - the target and each creature within 5 feet,
// a Dexterity save or 2d6 Cold (nothing on a success), 1d6 more for each slot level above 1.
constexpr int kIceKnifeExplosionDie = 6;
constexpr int kIceKnifeExplosionFeet = 5;
int IceKnifeExplosionDice(int slot_level);
// Chromatic Orb: two or more of its d8s alike - the orb leaps to another creature within 30 feet of the target (a
// new attack roll, a new damage roll), at most as many times as the slot's level, each creature once a casting.
bool HasMatchingDice(const std::vector<int>& rolls);
int ChromaticOrbLeaps(int slot_level);
// Heroism: one creature, one more for each slot level above 1; Temporary Hit Points equal to the spellcasting
// ability modifier (none below 1) at the start of each of its turns.
int HeroismTargets(int slot_level);
int HeroismTemporaryHitPoints(int modifier);
// SRD 5.2.1, One Spell with a Spell Slot per Turn: a turn is the creature's round - the same one when none of its
// rounds ended since and less than a round's time (6 s, out of combat) went by.
bool SameTurn(uint32_t rounds_then, uint32_t rounds_now, long long ms_since);
// Dissonant Whispers: a failed save spends the target's Reaction moving away - as far as its Speed (nwn5e: 30
// feet, everyone's) takes it.
constexpr int kDissonantWhispersFleeFeet = 30;

// The areas, from the caster toward the point it aims at: a creature `along` metres ahead and `side` metres to the
// side is in it when part of it is (`slack`: its own half-width).
bool InCone(float along, float side, float length, float slack);
bool InCubeFromCaster(float along, float side, float edge, float slack);

}  // namespace rules
