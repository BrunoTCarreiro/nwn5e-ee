#include "rules/feats2024.h"

#include <algorithm>

#include "rules/class_features.h"
#include "rules/progression.h"
#include "rules/weapons.h"

namespace rules {

bool IsSkilledFeat(int feat) {
  return feat == kFeatSkilled || (feat >= kFeatSkilledAgainFirst && feat < kFeatSkilledAgainFirst + kSkilledRepeats);
}

bool IsOriginFeat2024(int feat) { return feat == kFeatAlert || feat == kFeatSavageAttacker || IsSkilledFeat(feat); }

bool IsChoosableFeat2024(int feat, bool masteries) {
  if (IsOriginFeat2024(feat)) return true;
  if (masteries && WeaponKindOfMasteryFeat(feat) >= 0) return true;
  if (AsiFeatFromId(feat)) return true;  // C7: the ASI's choices (the older ASI rows are no longer taken)
  return BonusFeatGroupOf(feat) == BonusFeatGroup::kFightingStyle;
}

AbilityIncrease AsiSplit(int split) {
  if (split < 6) return {static_cast<Ability>(split), std::nullopt};
  int k = split - 6;
  for (int a = 0; a < 6; ++a) {
    for (int b = a + 1; b < 6; ++b) {
      if (k-- == 0) return {static_cast<Ability>(a), static_cast<Ability>(b)};
    }
  }
  return {Ability::kStrength, std::nullopt};
}

int AsiSplitIndex(const AbilityIncrease& increase) {
  for (int split = 0; split < kAsiSplits; ++split) {
    const AbilityIncrease candidate = AsiSplit(split);
    const bool same = candidate.first == increase.first && candidate.second == increase.second;
    const bool swapped = increase.second && candidate.second && candidate.first == *increase.second &&
                         *candidate.second == increase.first;
    if (same || swapped) return split;
  }
  return -1;
}

int AsiSplitFeat(int slot, int split) {
  if (slot < 1 || slot > kAsiFeatCount || split < 0 || split >= kAsiSplits) return -1;
  return kAsiSplitFeatFirst + kAsiSplits * (slot - 1) + split;
}

std::optional<AsiFeatChoice> AsiFeatFromId(int feat) {
  const int offset = feat - kAsiSplitFeatFirst;
  if (offset < 0 || offset >= kAsiSplits * kAsiFeatCount) return std::nullopt;
  return AsiFeatChoice{offset / kAsiSplits + 1, AsiSplit(offset % kAsiSplits)};
}

bool IsNextAsiChoice(int feat, int owned_asis) {
  const auto choice = AsiFeatFromId(feat);
  return choice && choice->slot == owned_asis + 1;
}

bool AsiChangesSomething(const AbilityIncrease& increase, const int scores[6]) {
  if (scores[static_cast<int>(increase.first)] < kAbilityIncreaseCap) return true;
  return increase.second && scores[static_cast<int>(*increase.second)] < kAbilityIncreaseCap;
}

BonusFeatGroup BonusFeatGroupOf(int feat) {
  if (feat >= kAsiFeatFirst && feat < kAsiFeatFirst + kAsiFeatCount) return BonusFeatGroup::kGeneral;
  if (AsiFeatFromId(feat)) return BonusFeatGroup::kGeneral;
  if (IsOriginFeat2024(feat)) return BonusFeatGroup::kOrigin;
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
  if (taken.mastery > allowed.mastery) return false;
  // Styles and Origin feats past their own picks take the general one(s).
  const int overflow = std::max(0, taken.style - allowed.style) + std::max(0, taken.origin - allowed.origin);
  return taken.general + overflow <= allowed.general;
}

bool KeepSecondSavageRoll(const std::vector<int>& first, const std::vector<int>& second) {
  int first_total = 0, second_total = 0;
  for (int roll : first) first_total += roll;
  for (int roll : second) second_total += roll;
  return second_total > first_total;
}

}  // namespace rules
