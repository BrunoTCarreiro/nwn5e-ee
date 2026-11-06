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

}  // namespace rules
