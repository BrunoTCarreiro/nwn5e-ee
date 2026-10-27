#include "rules/armor.h"

namespace rules {

namespace {
const Armor2024 kArmor[] = {
    {"no armour", ArmorCategory::kNone, 10, -1, 0, false},
    {"Padded Armor", ArmorCategory::kLight, 11, -1, 0, true},
    {"Leather Armor", ArmorCategory::kLight, 11, -1, 0, false},
    {"Studded Leather Armor", ArmorCategory::kLight, 12, -1, 0, false},
    {"Scale Mail", ArmorCategory::kMedium, 14, 2, 0, true},
    {"Chain Mail", ArmorCategory::kHeavy, 16, 0, 13, true},
    {"Splint Armor", ArmorCategory::kHeavy, 17, 0, 15, true},
    {"Half Plate Armor", ArmorCategory::kMedium, 15, 2, 0, true},
    {"Plate Armor", ArmorCategory::kHeavy, 18, 0, 15, true},
};
constexpr int kTiers = static_cast<int>(sizeof(kArmor) / sizeof(kArmor[0]));
}  // namespace

const Armor2024& ArmorForNwnTier(int nwn_base_ac) {
  return nwn_base_ac > 0 && nwn_base_ac < kTiers ? kArmor[nwn_base_ac] : kArmor[0];
}

bool IsHeavyArmor2024(int nwn_base_ac) { return ArmorForNwnTier(nwn_base_ac).category == ArmorCategory::kHeavy; }

int ArmorDexterity2024(int nwn_base_ac, int dexterity_modifier) {
  const Armor2024& armor = ArmorForNwnTier(nwn_base_ac);
  if (armor.category == ArmorCategory::kHeavy) return 0;
  if (armor.dex_cap >= 0 && dexterity_modifier > armor.dex_cap) return armor.dex_cap;
  return dexterity_modifier;
}

int ArmorBaseAdjustment2024(int nwn_base_ac) {
  if (nwn_base_ac <= 0 || nwn_base_ac >= kTiers) return 0;
  return kArmor[nwn_base_ac].base_ac - (10 + nwn_base_ac);
}

}  // namespace rules
