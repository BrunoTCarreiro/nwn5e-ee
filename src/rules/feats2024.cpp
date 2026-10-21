#include "rules/feats2024.h"

#include "rules/class_features.h"
#include "rules/progression.h"

namespace rules {

bool IsChoosableFeat2024(int feat) {
  if (feat == kFeatAlert) return true;
  if (feat >= kAsiFeatFirst && feat < kAsiFeatFirst + kAsiFeatCount) return true;
  return feat >= kFeatFightingStyleArchery && feat <= kFeatFightingStyleTwoWeapon;
}

}  // namespace rules
