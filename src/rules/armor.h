#pragma once

// 2024 armour (SRD 5.2.1 p. 92, "Armor"; owner 2026-10-27, option A: "make sure
// the NWN armor AC matches 2024"). NWN's armour comes in tiers - the chest
// item's base AC 0-8 (parts_chest ACBONUS, armor.2da) - and the game's own
// description of each names its armour (dialog.tlk 1727-1736): 1 padded, 2
// leather, 3 studded leather or hide, 4 scale mail, 5 chainmail, 6 banded or
// splint, 7 half-plate, 8 full plate. Each tier is read as that 2024 armour
// (3: Studded Leather, the light one; 6: Splint).

namespace rules {

enum class ArmorCategory { kNone, kLight, kMedium, kHeavy };

struct Armor2024 {
  const char* name;
  ArmorCategory category;
  int base_ac;        // AC before Dexterity
  int dex_cap;        // the most Dexterity adds; -1: no cap; heavy armour adds none (0)
  int strength;       // the Strength it asks for (0: none)
  bool stealth_disadvantage;
};

// The 2024 armour a NWN tier is read as (0 or out of range: no armour).
const Armor2024& ArmorForNwnTier(int nwn_base_ac);
bool IsHeavyArmor2024(int nwn_base_ac);
// The Dexterity modifier the armour lets into AC: light / none all of it,
// medium at most +2, heavy none (neither a bonus nor a penalty).
int ArmorDexterity2024(int nwn_base_ac, int dexterity_modifier);
// 2024 base AC - NWN's (10 + tier): what the armour's AC changes by (0 without armour).
int ArmorBaseAdjustment2024(int nwn_base_ac);

}  // namespace rules
