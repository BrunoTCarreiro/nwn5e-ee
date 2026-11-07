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

}  // namespace rules
