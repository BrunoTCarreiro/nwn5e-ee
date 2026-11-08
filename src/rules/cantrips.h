#pragma once

// #39 Part 1, the cantrips (docs/PLAN.md "Phase 4"; owner, 2026-11-06: "go for 39 focusing on the cantrips", then
// "1. A, but do not reuse icons yet ... 2. A, but poison and acid are different things and force and psychic aren't
// magical ... 4. A ... 6. ok"). SRD 5.2's damage cantrips: a die of damage that grows with the caster - "The damage
// increases by 1d10 when you reach levels 5 (2d10), 11 (3d10), and 17 (4d10)" (Cantrip Upgrade, by character
// level) - and each its rider. Build 1: Acid Splash, Ray of Frost and Light rewritten; Fire Bolt, Chill Touch,
// Shocking Grasp, Poison Spray, Sacred Flame, Starry Wisp and Vicious Mockery new (served spells.2da rows); NWN's
// own cantrips (Daze, Flare, Electric Jolt, Virtue, Cure / Inflict Minor Wounds) off the class lists.

#include <stdint.h>

#include <vector>

namespace rules {

// Damage-type bits (NWN's DAMAGE_TYPE_*): 2024's Lightning, Thunder, Necrotic and Radiant are NWN's Electrical,
// Sonic, Negative and Divine; Poison, Force and Psychic are NWN:EE's custom types 1-3 (damagetypes.2da rows 13-15,
// served: their names, groups and colours).
constexpr uint32_t kDamageTypeAcid = 16;
constexpr uint32_t kDamageTypeCold = 32;
constexpr uint32_t kDamageTypeRadiant = 64;     // DAMAGE_TYPE_DIVINE
constexpr uint32_t kDamageTypeLightning = 128;  // DAMAGE_TYPE_ELECTRICAL
constexpr uint32_t kDamageTypeFire = 256;
constexpr uint32_t kDamageTypeNecrotic = 512;   // DAMAGE_TYPE_NEGATIVE
constexpr uint32_t kDamageTypeThunder = 2048;   // DAMAGE_TYPE_SONIC
constexpr uint32_t kDamageTypePoison = 8192;    // DAMAGE_TYPE_CUSTOM1
constexpr uint32_t kDamageTypeForce = 16384;    // DAMAGE_TYPE_CUSTOM2
constexpr uint32_t kDamageTypePsychic = 32768;  // DAMAGE_TYPE_CUSTOM3

// How a cantrip reaches its target.
enum class CantripRoll {
  kRangedSpellAttack,
  kMeleeSpellAttack,
  kDexteritySave,
  kWisdomSave,
  kNone,  // Light
};

struct Cantrip {
  int spell = -1;         // spells.2da row
  const char* script;     // its impact script (run in C++, rules_cantrips.cpp)
  int die = 0;            // 6, 8, 10, 12; 0: no damage
  uint32_t damage_type = 0;
  CantripRoll roll = CantripRoll::kNone;
};

// The cantrips build 1 makes (ascending by row) and one by its row (null: not one of them).
const std::vector<Cantrip>& Cantrips();
const Cantrip* CantripOf(int spell);

// Cantrip Upgrade: the dice at a character level - 1, then 2 at 5, 3 at 11, 4 at 17.
int CantripDice(int character_level);
// The dice an attack cantrip rolls: doubled on a Critical Hit.
int CantripDiceRolled(int character_level, bool critical);

// 4A: NWN's own cantrips, off the class lists in faithful (monsters, items and NWN mode keep them).
const std::vector<int>& RetiredCantrips();
bool IsRetiredCantrip(int spell);

// --- #39 Part 1, build 2: the cantrips cast through a choice ---------------------------------------------------
// Resistance (a damage type), Guidance (a skill), Sorcerous Burst (a damage type) - decision 5 ("A a menu (past 5 a
// "more" entry)"). Each option is a spells.2da row of its own, Master the cantrip (NWN's sub-radial variants: the
// engine spends and checks the master's use for it - GetSpellUsesLeft, UnReadySpell, GetHasSpell), so the cast tells
// the choice and the quickbar can hold one. The radial lists them (rules_spell_slots.cpp): eight to a page, or seven
// and "More". The master cast with no choice (an item's, a creature's, a spellbook button) takes the first type of
// damage the creature takes (Resistance), the first skill it checks (Guidance), or Fire (Sorcerous Burst).

enum class ChoiceKind { kDamageType, kSkill, kMode };  // kMode: a way to use it (Produce Flame's Flame, Hurl)

struct CantripOption {
  int spell = -1;          // the variant's spells.2da row
  uint32_t value = 0;      // a damage-type bit, or a Skill2024 (skills.h)
  const char* label = "";  // the radial's label
};

struct ChoiceCantrip {
  int master = -1;  // the cantrip's row (the one on the class lists)
  const char* script = "";
  ChoiceKind kind = ChoiceKind::kDamageType;
  std::vector<CantripOption> options;  // SRD 5.2.1's order; their rows consecutive
  int icon = -1;  // its effect icon (effecticons.2da row) while it lasts: the master's, then each option's in turn
};

constexpr int kResistance = 151;      // NWN's row, rewritten
constexpr int kGuidance = 1057;
constexpr int kSpareTheDying = 1058;
constexpr int kSorcerousBurst = 1059;  // Sorcerer only (not the Wizard's, though NWN's Wiz_Sorc column is shared)
// Build 3a (owner, 2026-11-07: "1 A, 2 A, 3 A, 4 A and while Produce Flame is active the character is also a light
// source, 5 A"): Produce Flame - Flame (a Bonus Action: the flame in your hand, a light, 10 minutes) and Hurl (an
// action while it lasts: a ranged spell attack, d8 Fire).
constexpr int kProduceFlame = 1096;
constexpr int kProduceFlameFlame = 1097;
constexpr int kProduceFlameHurl = 1098;
constexpr int kProduceFlameDie = 8;
constexpr int kFeatProduceFlame = 1548;  // its Bonus Action: a feat carrying the Flame (bonus_actions.cpp)

const std::vector<ChoiceCantrip>& ChoiceCantrips();
// The choice cantrip a row belongs to - its master or one of its variants (null: none).
const ChoiceCantrip* ChoiceCantripOf(int spell);
// A variant's option (null: the master itself, or not one).
const CantripOption* ChoiceOptionOf(int spell);
// 0.90.1 (owner, 2026-11-07: "While concentrating on spells that give bonuses (like resistance to piercing, for
// example) these bonuses should appear on the top right while the concentrating is happening"): the effect icon a
// row shows on the creature it's cast on - its effecticons.2da row, named for the option ("Resistance: Piercing");
// -1 for none (Sorcerous Burst). EffectIcon takes rows 1-255, the game's run to 129: these are 130-160.
int EffectIconOf(int spell);

// A radial page of `count` options from option `first`: all that are left when they fit its eight slots, else seven
// and a "More" entry (the next page from first + 7).
constexpr int kRadialSlots = 8;
struct RadialPage {
  int shown = 0;
  bool more = false;
};
RadialPage RadialChoicePage(int count, int first);

// The choice menu's page in a radial param (bits 44-48; #36's upcast menu uses 40-43), and an option's cast param:
// the menu's param with the variant's row and no page bits.
int RadialChoiceFirst(long long param);
long long RadialChoiceWithFirst(long long param, int first);
long long RadialChoiceCast(long long param, int variant);

// Q1 A: a spell cast as a Bonus Action - the feat that carries it (feat.2da SPELLID; instant and one a round, as
// the class abilities' Bonus Actions): its cast becomes that feat's use. -1: a spell cast as NWN casts it.
int BonusActionFeatOfSpell(int spell);
bool IsBonusActionSpellFeat(int feat);

// Sorcerous Burst's damage: `dice` d8s (Cantrip Upgrade, doubled on a Critical Hit); each 8 rolled lets another d8 be
// rolled and added, at most `extra_cap` more (the spellcasting ability modifier). `roll_d8` rolls one d8.
int SorcerousBurstDamage(int dice, int extra_cap, int (*roll_d8)(void* context), void* context, int* extra_out);

// Resistance: the damage of the chosen type taken off (1d4, at most the damage); once per turn - the caller keeps it.
int ResistanceReduction(int damage, int d4);

// Spare the Dying's range in feet: 15, doubling at character levels 5 (30), 11 (60) and 17 (120).
int SpareTheDyingRangeFeet(int character_level);

// --- #39 Part 1, build 3b: the weapon cantrips (owner, 2026-11-07: "1 A, 2 A, 3 A, 4 A ... 5 A"; "build 3b") -----
// Shillelagh 1099 (the Druid's; a Bonus Action): the Club or Quarterstaff in your hand, 1 minute - its melee attacks
// may use your spellcasting ability instead of Strength (the better is taken), its damage die a d8 (d10 at 5, d12 at
// 11, 2d6 at 17), Force or the weapon's type (Force 1100, Bludgeoning 1101); it ends if you let go of the weapon or
// cast it again. True Strike (NWN's 415 rewritten; Bard, Sorcerer, Wizard): one attack with the weapon in your hand -
// the game's own, the round's other attacks dropped - using your spellcasting ability instead of Strength or
// Dexterity, Radiant (1102) or the weapon's type (1103), and 1d6 Radiant more at 5 (2d6 at 11, 3d6 at 17).
constexpr int kShillelagh = 1099;
constexpr int kShillelaghForce = 1100;
constexpr int kShillelaghWeapon = 1101;
constexpr int kFeatShillelaghForce = 1549;   // its Bonus Action with Force (the cantrip cast with no choice)
constexpr int kFeatShillelaghWeapon = 1550;  // its Bonus Action with the weapon's type
constexpr int kTrueStrike = 415;             // NWN's row, rewritten
constexpr int kTrueStrikeRadiant = 1102;
constexpr int kTrueStrikeWeapon = 1103;

// The cantrip or one of its variants.
bool IsShillelagh(int spell);
bool IsTrueStrike(int spell);
// NWN base items Shillelagh takes: Club 28, Quarterstaff 50, and NWN's magic staff 45 (a quarterstaff to 2024).
bool IsShillelaghWeapon(int nwn_base_item);

struct WeaponDie {
  int count = 0;
  int sides = 0;
};
// Shillelagh's damage die at a character level: d8, d10 at 5, d12 at 11, 2d6 at 17.
WeaponDie ShillelaghDie(int character_level);
// The dice a hit rolls when two could (the weapon's or Martial Arts' and Shillelagh's): the higher average; a tie
// keeps `a`.
WeaponDie BetterDice(WeaponDie a, WeaponDie b);
// True Strike's extra Radiant d6s at a character level: 0, 1 at 5, 2 at 11, 3 at 17; doubled on a Critical Hit.
int TrueStrikeExtraDice(int character_level, bool critical);
// The ability modifier the attack and damage rolls use, `normal` the one they would: True Strike's is the
// spellcasting ability's ("instead of using Strength or Dexterity"), Shillelagh's the better of the two ("you can
// use your spellcasting ability instead of Strength").
int WeaponCantripAbility(bool true_strike, int normal, int casting);
// The damage-type bit a variant's hit deals (the master cast with no choice: its first option's); 0 the weapon's.
uint32_t WeaponCantripDamageType(int spell);

// A hit's damage (CNWSCombatAttackData +0x16: an int16 per damage-type bit, -1 unused; bit 12 the weapon's own):
// the bit's index (-1 not one bit); `amount` more of `type`; the weapon's own damage moved to `type` (returns how
// much moved; 0 for type 0, the weapon's own).
int DamageTypeIndex(uint32_t type);
void AddDamage(int16_t* entries, int count, uint32_t type, int amount);
int MoveWeaponDamage(int16_t* entries, int count, uint32_t type);

// --- #39 Part 1, build 4a: the utility cantrips - Fire Play and the doors -----------------------------------------
// Owner, 2026-11-07: "1A, 2A, 3A, 4 ... A, 5 let's look into these deeper, 6 A", then Q5's closer look ("Sculpt
// elements, yes, don't use it. Prestidigitation is mostly for light sources. and marks ... thaumaturgy don't do the
// eyes and tremors", Q8 A), then "start building the utility cantrips". Each cantrip's radial menu (ChoiceKind::kMode)
// lists its effects, the option's value a UtilityEffect:
// - Druidcraft 1104 (Druid; 30 ft): Fire Play (1107), Animal Sound (1108) - 0.93.1 (owner, 2026-11-08: "weather
//   sensor, bloom and sensory effect don't seem to do much, they can be removed") took its other effects out; build 4b
//   (owner, 2026-11-08: "Go with your recommendations", Q3 A) gave back the animal's sound. Rows 1105-1106 unused.
// - Elementalism 1109 (Druid, Sorcerer, Wizard; 30 ft; a 5-foot Cube): Beckon Air, Beckon Earth, Beckon Fire,
//   Beckon Water (no Sculpt Element).
// - Prestidigitation 1114 (Bard, Sorcerer, Wizard; 10 ft): Fire Play, Magic Mark (up to three at a time, 1 hour).
// - Thaumaturgy 1117 (Cleric; 30 ft): Booming Voice, Fire Play (Q8 A: dimmed out for 1 minute), Invisible Hand,
//   Phantom Sound - whispers (1121) or thunder (1134).
// - Build 4b: Minor Illusion 1123 (Bard, Sorcerer, Wizard; 30 ft; 1 minute) - a sound (voices, footsteps, a roar,
//   thunder) or an image (an object, a humanoid, a small creature); Dancing Lights 1131 (Bard, Sorcerer, Wizard;
//   120 ft; Concentration, 1 minute) - its lights (1132), moved with a Bonus Action (1133).
// - Mage Hand 1122 (Bard, Sorcerer, Wizard; 30 ft): no menu - at a door it opens or shuts it (unlocked), at a light
//   it lights or snuffs it.
enum class UtilityEffect : uint32_t {
  kNone,
  kFirePlay,
  kBeckonAir,
  kBeckonEarth,
  kBeckonFire,
  kBeckonWater,
  kMagicMark,
  kBoomingVoice,
  kThaumaturgyFirePlay,
  kInvisibleHand,
  kPhantomWhispers,
  kMageHand,
  // Build 4b:
  kAnimalSound,
  kPhantomThunder,
  kIllusionVoices,
  kIllusionFootsteps,
  kIllusionRoar,
  kIllusionThunder,
  kImageObject,
  kImageHumanoid,
  kImageCreature,
  kDancingLights,
  kDancingLightsMove,
};
constexpr int kDruidcraft = 1104;
constexpr int kElementalism = 1109;
constexpr int kPrestidigitation = 1114;
constexpr int kThaumaturgy = 1117;
constexpr int kPhantomWhispers = 1121;
constexpr int kMageHand = 1122;
constexpr int kMinorIllusion = 1123;
constexpr int kDancingLights = 1131;
constexpr int kDancingLightsLights = 1132;
constexpr int kDancingLightsMove = 1133;
constexpr int kPhantomThunder = 1134;
constexpr int kMagicMarksAtOnce = 3;      // "up to three of its non-instantaneous effects active at a time"
constexpr int kFirePlayDimSeconds = 60;   // Thaumaturgy's Fire Play: "for 1 minute"
constexpr int kBoomingVoiceSeconds = 60;  // "for 1 minute"
constexpr int kElementCubeFeet = 5;       // Elementalism's 5-foot Cube
constexpr int kBoomingVoiceIcon = 164;    // its effect icon (effecticons.2da)

// A utility cantrip row's effect: a variant's; the cantrip cast with no choice, its first option's; Druidcraft's
// (Fire Play) and Mage Hand's own; kNone for any other row.
UtilityEffect UtilityEffectOf(int spell);
// How an effect plays with a light (light_sources.cpp): kToggle lights an unlit one and snuffs a lit one (Fire Play,
// Mage Hand), kLight lights only (Beckon Fire's embers), kSnuff snuffs only (Beckon Water), kDim puts it out for a
// minute (Thaumaturgy's Fire Play); kNone not at all.
enum class FirePlay { kNone, kToggle, kLight, kSnuff, kDim };
FirePlay FirePlayOf(UtilityEffect effect);
// And with a door: kToggle opens a shut unlocked one and shuts an open one (Invisible Hand, Mage Hand), kClose shuts
// an open one only (Beckon Air); kNone not at all.
enum class DoorPlay { kNone, kToggle, kClose };
DoorPlay DoorPlayOf(UtilityEffect effect);
// True when the effect fills Elementalism's 5-foot Cube (every light or door in it), not one object.
bool FillsElementCube(UtilityEffect effect);
// The range a row's effect reaches in feet (30; Prestidigitation's 10; Dancing Lights' 120); 0: the row's own (on
// yourself, or not one).
int CantripRangeFeet(int spell);

// --- Build 4b (owner, 2026-11-08: "go for 4b"): the sounds, the illusions, Dancing Lights, the patches ------------
// Q7 A (owner, 2026-11-07: "A, but 2 turns"): voices, footsteps or whispers draw the creatures that hear them, which
// come to look; a beast's roar or thunder (Druidcraft's Animal Sound too) makes animals, beasts and vermin flee from
// it for 2 rounds, and the others come to look. A creature already fighting ignores either.
enum class SoundPlay { kNone, kDraws, kScares };
SoundPlay SoundPlayOf(UtilityEffect effect);
// NWN's racial types that flee a scaring sound: animal 8, beast 9, vermin 25.
bool FleesFromScaringSound(int nwn_racial_type);
// The game's own sound for an effect ("" none).
const char* SoundResrefOf(UtilityEffect effect);
constexpr int kSoundHearingFeet = 60;  // how far creatures hear it, for these rules
constexpr int kScareSeconds = 12;      // "flee ... for 2 rounds"
// Minor Illusion's image (SRD 5.2.1: an object no larger than a 5-foot Cube; the owner's humanoid and small
// creature fit it): the game's own blueprint ("" none), for 1 minute. Creatures that see it come to look; one that
// gets there studies it - an Intelligence (Investigation) check against the spell save DC sees through it.
enum class ImageKind { kNone, kObject, kHumanoid, kCreature };
ImageKind ImageKindOf(UtilityEffect effect);
const char* ImageBlueprintOf(ImageKind kind);
bool SeesThroughIllusion(int d20, int investigation_bonus, int dc);
constexpr int kMinorIllusionSeconds = 60;
constexpr int kImageSightFeet = 60;
// Dancing Lights: four lights at the point, Dim Light 10 ft each (NWN's 5 m light, the stealth rules' light), for
// 1 minute on Concentration; a Bonus Action moves them up to 60 ft, and they go out more than 120 ft from the caster.
constexpr int kDancingLightsCount = 4;
constexpr int kDancingLightsSeconds = 60;
constexpr int kDancingLightsMoveFeet = 60;
constexpr int kDancingLightsRangeFeet = 120;
bool DancingLightsMoveAllowed(float feet_from_lights, float feet_from_caster);
// Q1 A (owner, 2026-11-08): Beckon Water at the ground leaves a puddle (1 minute: SRD 5.2.1's water evaporates in 1
// minute), Beckon Earth a dust patch (10 minutes) - a look for now (#142 their rules), served vfx_persistent rows.
constexpr int kPuddleAreaEffect = 103;
constexpr int kDustAreaEffect = 104;
int PatchAreaEffectOf(UtilityEffect effect);  // -1: none
int PatchSecondsOf(UtilityEffect effect);

}  // namespace rules
