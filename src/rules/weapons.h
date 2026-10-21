#pragma once

// Phase 1d (proposed — PLAN.md "Phase 1d proposal"): 2024 weapon mastery
// properties on NWN weapons, the classes that use them, and two-weapon
// fighting damage.

#include <stdint.h>

#include <string>
#include <vector>

#include "rules/combat.h"

namespace rules {

enum class Mastery { kNone, kCleave, kGraze, kNick, kPush, kSap, kSlow, kTopple, kVex };

// The mastery property of an NWN base item (baseitems.2da row); NWN-only
// weapons take the closest 2024 weapon's. kNone for anything that is not a
// manufactured weapon.
Mastery MasteryForNwnBaseItem(int base_item);

// D3: Barbarian, Fighter, Paladin, Ranger and Rogue have Weapon Mastery.
bool HasWeaponMastery(CharacterClass character_class);

// D1: the extra (off-hand) attack adds the ability modifier to damage only
// when it is negative or with the Two-Weapon Fighting style.
int OffHandDamageAbilityModifier(int ability_modifier, bool two_weapon_fighting_style);

// D1: the 2024 Light weapons among NWN base items (club, dagger, handaxe,
// light hammer, sickle, scimitar, shortsword, plus kukri and kama).
bool IsLightNwnBaseItem(int base_item);

// D1: off-hand attacks per round. `nwn_attacks` is the engine's count (0 when
// it sees no second weapon; 2 with Improved Two-Weapon Fighting). 2024: one
// extra attack at most — with two Light weapons, a double weapon, or Dual
// Wielder (Improved Two-Weapon Fighting) with any weapons NWN allows.
int OffHandAttacks(int nwn_attacks, bool double_weapon, bool dual_wielder, bool main_hand_light,
                   bool off_hand_light);

// NWN's off-hand Strength damage: half the modifier (truncated) when it is
// positive, all of it when not (0x697D10).
int NwnOffHandStrengthDamage(int strength_modifier);

// NWN's main-hand Strength damage (0x697D10): Str x 1.5 (truncated) with a
// two-handed weapon when positive, the modifier itself otherwise.
int NwnMainHandStrengthDamage(int strength_modifier, bool two_handed);

// D3: weapon mastery slots (2024): Fighter 3/4/5/6 at levels 1/4/10/16,
// Barbarian 2/3/4 at 1/4/10, Paladin, Ranger and Rogue 2; 0 for the rest.
int WeaponMasterySlots(CharacterClass character_class, int levels);
// Several classes: the most slots any one of them gives (they do not add up,
// like Extra Attack).
int WeaponMasterySlots(const std::vector<ClassLevels>& classes);

// D3: the weapon kinds a mastery can be chosen for, as NWN base items, in
// display order. A magic staff (45) is a quarterstaff (50).
const std::vector<int>& NwnMasteryWeapons();
int MasteryKindOfNwnBaseItem(int base_item);

// C4 (docs/CHARGEN.md): the Weapon Mastery feats, one per weapon kind:
// feat.2da row kWeaponMasteryFeatFirst + the kind's place in
// NwnMasteryWeapons() (data/rows/feat.mastery.txt, tools/data/mastery_feats.py;
// append only — the ids are saved in characters).
constexpr int kWeaponMasteryFeatFirst = 1260;
int WeaponMasteryFeat(int base_item);        // -1 when not a mastery weapon kind
int WeaponKindOfMasteryFeat(int feat);       // the base item, or -1
// "Longsword"; nullptr when the base item is not a mastery weapon kind.
const char* NwnWeaponName(int base_item);
// Base item named by `text` (case, spaces and hyphens ignored; a few aliases
// such as "spear", "staff", "waraxe"); -1 when unknown.
int NwnWeaponFromName(const std::string& text);
const char* MasteryName(Mastery mastery);

// The Weapon Focus feat for a base item (GetWeaponFocus 0x6A5C20); -1 none.
int WeaponFocusFeatForNwnBaseItem(int base_item);
// Proficiency feats (baseitems.2da ReqFeat0-4): any one makes the wielder
// proficient; none listed means everyone is.
std::vector<int> ProficiencyFeatsForNwnBaseItem(int base_item);

// D3 slots: `chosen` holds one base item per slot (-1 = empty); only the
// first `slots` count. The first empty slot, or -1.
int FreeMasterySlot(const std::vector<int>& chosen, int slots);
// The slot holding `base_item` among the first `slots`, or -1.
int MasterySlotOf(const std::vector<int>& chosen, int slots, int base_item);

// D3 swap, "!mastery A for B" in either word order: the one of A and B that is
// in a slot is given up, the other taken. Both or neither in a slot: none.
struct MasterySwap {
  int give_up;
  int take;
};
bool ResolveMasterySwap(const std::vector<int>& chosen, int slots, int a, int b, MasterySwap* swap);

// D4: NWN attack results that hit (1 hit, 3 critical, 7 automatic hit, 10
// devastating critical); 2 parried, 4 miss, 8/9 concealment are misses.
bool MasteryAttackHit(int nwn_attack_result);
// D4 timing, counted in the creature's ended combat rounds (an NWN round is a
// 2024 turn). Vex: advantage until the end of the attacker's next round.
bool VexActive(uint32_t attacker_rounds_now, uint32_t attacker_rounds_at_hit);
// Sap: disadvantage until the start of the sapping creature's next round.
bool SapActive(uint32_t source_rounds_now, uint32_t source_rounds_at_hit);

// D4 Graze: results that are a miss of the attack roll (2 parried, 4 miss,
// 8/9 concealment).
bool MasteryAttackMiss(int nwn_attack_result);
// 2024 Finesse weapons among NWN base items (dagger, dart, rapier, scimitar,
// shortsword, whip; kukri and kama like the dagger and sickle's cousins).
bool IsFinesseNwnBaseItem(int base_item);
// The ability modifier an attack roll uses: Dex for ranged, Str for melee —
// or Dex with Weapon Finesse and a finesse weapon when it is higher.
int AttackAbilityModifier(bool ranged, bool weapon_finesse, bool finesse_weapon, int strength, int dexterity);
// D4 Topple: DC 8 + the attack's ability modifier + proficiency bonus.
int ToppleDc(int ability_modifier, int proficiency_bonus);
// D4 Graze: the attack's ability modifier as damage, nothing when not positive.
int GrazeDamage(int ability_modifier);

}  // namespace rules
