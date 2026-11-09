#pragma once

// #39 Part 1, the cantrips (docs/PLAN.md "Phase 4"; owner, 2026-11-06: "go for 39 focusing on the cantrips", then
// "1. A, but do not reuse icons yet ... 2. A, but poison and acid are different things and force and psychic aren't
// magical ... 4. A ... 6. ok"). SRD 5.2's damage cantrips: a die of damage that grows with the caster - "The damage
// increases by 1d10 when you reach levels 5 (2d10), 11 (3d10), and 17 (4d10)" (Cantrip Upgrade, by character
// level) - and each its rider. Build 1: Acid Splash, Ray of Frost and Light rewritten; Fire Bolt, Chill Touch,
// Shocking Grasp, Poison Spray, Sacred Flame, Starry Wisp and Vicious Mockery new (served spells.2da rows); NWN's
// own cantrips (Daze, Flare, Electric Jolt, Virtue, Cure / Inflict Minor Wounds) off the class lists.

#include <stdint.h>

#include <string>

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
// 0.94.1 (owner, 2026-11-08: "there should be 5 new magic mark symbols ... can you wire them up for a test"): Magic
// Mark's look - one of the owner's symbols. 0.96.0 (owner, 2026-11-08: "the new marks are in the x1 folder, you can
// delete the previous marks and add the new ones only"): 12 of them, visualeffects.2da rows 1860-1871 (the first five
// were 1840-1844).
constexpr int kMagicMarkFirstVisual = 1860;
constexpr int kMagicMarkVisuals = 12;
// The symbol for a roll of 1..kMagicMarkVisuals (out of range: the first).
int MagicMarkVisual(int roll);

// 0.96.0 (owner, 2026-11-08: "Go with A" - Magic Mark on a door or a placeable; SRD 5.2: "on an object or a
// surface"): the same symbols upright (rows from kMagicMarkUprightFirstVisual), standing on the object.
constexpr int kMagicMarkUprightFirstVisual = 1880;
int MagicMarkUprightVisual(int roll);

// 0.96.1 (owner, 2026-11-09: "Option b + random" - asked "would it also be possible to add a tint to the marks so
// that they're not always white?"; SRD 5.2: "a color, a small mark, or a symbol"): the mark's colour, picked from
// Prestidigitation's menu - Magic Mark: White (row 1116), Gold, Red, Green, Blue, Violet (1135-1139) or Random
// (1140, one of the six). White's rows are the ones above; each other colour's flat ones from
// kMagicMarkTintFirstVisual + (tint - 1) * kMagicMarkTintStride, its upright ones kMagicMarkUprightOffset on.
enum class MarkTint { kWhite, kGold, kRed, kGreen, kBlue, kViolet };
constexpr int kMagicMarkTints = 6;
constexpr int kMagicMarkWhite = 1116;
constexpr int kMagicMarkGold = 1135;
constexpr int kMagicMarkRandom = 1140;
constexpr int kMagicMarkTintFirstVisual = 1900;
constexpr int kMagicMarkTintStride = 40;
constexpr int kMagicMarkUprightOffset = 20;
// A Magic Mark row's colour (0 White .. 5 Violet); -1 for Random, or a row that isn't one.
int MagicMarkTintOf(int row);
// The visualeffects row of `symbol` (1..kMagicMarkVisuals; out of range: the first) in colour `tint` (0..5; out of
// range: White), flat on the ground or upright.
int MagicMarkRow(int symbol, int tint, bool upright);
// 0.96.2 (owner, 2026-11-09: "go with 1"): that row's model - n5v_markNN, the colour's letter (g r n b v; White
// none), u upright. A mark on an object is named by its model (its effect's tag, its record).
std::string MagicMarkModel(int symbol, int tint, bool upright);
// A new mark's symbol on an object: the `roll`-th (1..FreeMarkSymbols; out of range: the nearest) of those not on
// it already in that colour (taken[symbol - 1]); all of them taken, of all 12.
int FreeMarkSymbols(const std::vector<bool>& taken);
int FreeMarkSymbol(int roll, const std::vector<bool>& taken);

// Where an upright mark stands: a point (the mark's centre) and the way it faces (NWN degrees: 0 east, 90 north).
struct MarkSpot {
  float x = 0, y = 0, z = 0;
  float facing = 0;
};
constexpr float kMarkGap = 0.03f;          // metres off the surface
constexpr float kMarkDoorHeight = 1.4f;    // a door's mark: its centre above the door's base
constexpr float kMarkObjectHeight = 0.7f;  // a placeable's: above the ground
constexpr float kMarkNoMeshReach = 0.35f;  // a placeable without a walkmesh: this far from its centre
// A closed door's slab in its own frame (x across it, y through it, as NWN's door walkmeshes: T_Door01's is
// 2.1 x 0.3 x 2.97 m).
constexpr float kDoorHalfWidth = 1.05f;
constexpr float kDoorHalfDepth = 0.15f;

// A door at (dx, dy, dz) facing `facing`, the caster at (cx, cy): the mark on the slab's face toward the caster -
// where the line from the door's centre to the caster leaves the slab - flat on that face (facing out of it),
// kMarkGap off it, kMarkDoorHeight up. NWN models face +y: an object's +y points along its facing.
MarkSpot MarkOnDoor(float dx, float dy, float dz, float facing, float cx, float cy);

// A placeable at (px, py) and the caster at (cx, cy); `surface` the share of the way from the caster to it where its
// walkmesh starts (0..1; below 0: none found). The mark kMarkGap before that point (or kMarkNoMeshReach from the
// placeable's centre), facing the caster; z `ground` + kMarkObjectHeight.
MarkSpot MarkOnPlaceable(float px, float py, float cx, float cy, float surface, float ground);

// The largest share t in [0, 1] of a line that is still clear, by halving `steps` times: `blocked(t)` is whether the
// line's first t is blocked (false at 0, true at 1). -1 when even the whole line is clear.
template <typename Blocked>
float ClearShare(Blocked blocked, int steps) {
  if (!blocked(1.0f)) return -1.0f;
  float clear = 0.0f, hit = 1.0f;
  for (int i = 0; i < steps; ++i) {
    const float mid = (clear + hit) * 0.5f;
    if (blocked(mid)) {
      hit = mid;
    } else {
      clear = mid;
    }
  }
  return clear;
}

// #150 (owner, 2026-11-09: "go with your recommendation for #150"; relayed from #149: "most casting animations are
// very elaborate, especially for cantrips"): every cantrip casts with no conjure, straight into castout - played as
// n5_cantrip, castout's arms on the combat-ready legs, then its held loop the same way (the animation session's
// RequestCastClip) - for kCantripCastMs (spells.2da's CastTime; ConjTime 0); the swap stands kCantripClipSeconds.
constexpr int kCantripCastMs = 1500;
constexpr float kCantripClipSeconds = 1.6f;

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
// #152 (owner, 2026-11-09: "by alignment I meant disposition towards the player, eg enemies. go for A on all"):
// how a creature that sees the image reacts, by what it is.
// Q1 A, sentient: not an animal (NWN's racial type 8), beast (9), construct (10), vermin (25) or ooze (29), and
// Intelligence 4 or more.
bool IsSentient(int nwn_racial_type, int intelligence_score);
// Q2 A, aggressive: hostile to the caster. Q3 A: a hostile creature the leopard's size or larger attacks it.
enum class ImageReaction {
  kIgnore,            // pays it no mind
  kStudy,             // comes to look and studies it
  kFlee,              // flees for 2 rounds
  kFleeThenStudy,     // flees for 2 rounds, then comes back and studies it
  kAttack,            // attacks it, then goes back
  kAttackThenSearch,  // attacks it, then searches round it for the caster
};
// `size` and `image_size`: NWN's creature sizes (1 tiny .. 5 huge).
ImageReaction ReactionToImage(ImageKind kind, bool sentient, bool hostile, int size, int image_size);
// After a study - Q4 A: an image seen through vanishes for everyone; a hostile creature then searches for the
// caster, any other goes back. A fooled one flees a creature image again (and stays away), else goes back.
enum class AfterStudy { kVanishThenSearch, kVanishThenGoBack, kFleeAgain, kGoBack };
AfterStudy AfterStudyOf(ImageKind kind, bool sees_through, bool hostile);
// Once the image is gone (seen through or hit), a sentient hostile creature searches for the caster.
bool SearchesAfterVanish(bool sentient, bool hostile);
constexpr int kIllusionSearchSeconds = 60;  // a minute
constexpr int kIllusionSearchFeet = 30;     // round the image: the spell's range, where the caster stood
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
