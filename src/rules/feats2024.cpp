#include "rules/feats2024.h"

#include "rules/class_features.h"
#include "rules/progression.h"
#include "rules/weapons.h"

namespace rules {

bool IsChoosableFeat2024(int feat, bool masteries) {
  if (feat == kFeatAlert || feat == kFeatSavageAttacker || feat == kFeatSkilled) return true;  // Origin
  if (masteries && WeaponKindOfMasteryFeat(feat) >= 0) return true;
  const BonusFeatGroup group = BonusFeatGroupOf(feat);
  return group == BonusFeatGroup::kGeneral || group == BonusFeatGroup::kFightingStyle;
}

BonusFeatGroup BonusFeatGroupOf(int feat) {
  if (feat >= kAsiFeatFirst && feat < kAsiFeatFirst + kAsiFeatCount) return BonusFeatGroup::kGeneral;
  if (feat >= kFeatFightingStyleArchery && feat <= kFeatFightingStyleTwoWeapon) return BonusFeatGroup::kFightingStyle;
  if (WeaponKindOfMasteryFeat(feat) >= 0) return BonusFeatGroup::kWeaponMastery;
  return BonusFeatGroup::kOther;
}

BonusPicks BonusPicks2024(CharacterClass character_class, int class_level, bool styles, bool masteries) {
  BonusPicks picks;
  picks.general = IsFeatLevel2024(character_class, class_level) ? 1 : 0;
  picks.style = styles && IsFightingStyleLevel2024(character_class, class_level) ? 1 : 0;
  if (masteries && class_level >= 1) {
    picks.mastery = WeaponMasterySlots(character_class, class_level) -
                    WeaponMasterySlots(character_class, class_level - 1);
    if (picks.mastery < 0) picks.mastery = 0;
  }
  return picks;
}

bool FitsBonusPicks(const BonusPicks& taken, const BonusPicks& allowed) {
  if (taken.mastery > allowed.mastery || taken.general > allowed.general) return false;
  return taken.style <= allowed.style + (allowed.general - taken.general);
}

}  // namespace rules
