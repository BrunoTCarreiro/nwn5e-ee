#include "rules/find_familiar.h"

#include "rules/spells2024.h"

namespace rules {

FamiliarForm FamiliarFormOf(int row) {
  if (row >= kFamiliarVariantFirst && row <= kFamiliarVariantLast) row = kFamiliarBat + (row - kFamiliarVariantFirst) / 3;
  switch (row) {
    case kFamiliarBat: return FamiliarForm::kBat;
    case kFamiliarCat: return FamiliarForm::kCat;
    case kFamiliarHawk: return FamiliarForm::kHawk;
    case kFamiliarLizard: return FamiliarForm::kLizard;
    case kFamiliarRat: return FamiliarForm::kRat;
    case kFamiliarRaven: return FamiliarForm::kRaven;
    case kFamiliarSpider: return FamiliarForm::kSpider;
    case kFamiliarWeasel: return FamiliarForm::kWeasel;
    default: return FamiliarForm::kNone;
  }
}

int FamiliarKindOfRow(int row) {
  if (row < kFamiliarVariantFirst || row > kFamiliarVariantLast) return 0;
  return (row - kFamiliarVariantFirst) % 3 + 1;
}

const FamiliarStats& FamiliarStatsOf(FamiliarForm form) {
  // SRD 5.2.1: AC, HP, Str Dex Con Int Wis Cha; appearance.2da: 10 Bat, 203 Cat_Cougar, 144 Falcon, 369 Basilisk,
  // 386 Rat, 145 Raven, 159 Spider_Giant, 8 Badger.
  static const FamiliarStats kBat{"Bat", 12, 1, {2, 15, 8, 2, 12, 4}, 10, 1.0f};
  static const FamiliarStats kCat{"Cat", 12, 2, {3, 15, 10, 3, 12, 7}, 203, 0.5f};
  static const FamiliarStats kHawk{"Hawk", 13, 1, {5, 16, 8, 2, 14, 6}, 144, 1.0f};
  static const FamiliarStats kLizard{"Lizard", 10, 2, {2, 11, 10, 1, 8, 3}, 369, 0.35f};
  static const FamiliarStats kRat{"Rat", 10, 1, {2, 11, 9, 2, 10, 4}, 386, 0.6f};
  static const FamiliarStats kRaven{"Raven", 12, 2, {2, 14, 10, 5, 13, 6}, 145, 1.0f};
  static const FamiliarStats kSpider{"Spider", 12, 1, {2, 14, 8, 1, 10, 2}, 159, 0.3f};
  static const FamiliarStats kWeasel{"Weasel", 13, 1, {3, 16, 8, 2, 12, 3}, 8, 0.6f};
  static const FamiliarStats kNoForm{};
  switch (form) {
    case FamiliarForm::kBat: return kBat;
    case FamiliarForm::kCat: return kCat;
    case FamiliarForm::kHawk: return kHawk;
    case FamiliarForm::kLizard: return kLizard;
    case FamiliarForm::kRat: return kRat;
    case FamiliarForm::kRaven: return kRaven;
    case FamiliarForm::kSpider: return kSpider;
    case FamiliarForm::kWeasel: return kWeasel;
    default: return kNoForm;
  }
}

FamiliarKind FamiliarKindFor(int pick, int nwn_good_evil) {
  if (pick == 1) return FamiliarKind::kCelestial;
  if (pick == 2) return FamiliarKind::kFey;
  if (pick == 3) return FamiliarKind::kFiend;
  if (nwn_good_evil == 4) return FamiliarKind::kCelestial;  // ALIGNMENT_GOOD
  if (nwn_good_evil == 5) return FamiliarKind::kFiend;      // ALIGNMENT_EVIL
  return FamiliarKind::kFey;
}

const char* FamiliarKindName(FamiliarKind kind) {
  switch (kind) {
    case FamiliarKind::kCelestial: return "Celestial";
    case FamiliarKind::kFiend: return "Fiend";
    default: return "Fey";
  }
}

int FamiliarRacialType(FamiliarKind kind) { return kind == FamiliarKind::kFey ? 17 : 20; }

int FamiliarGoodEvil(FamiliarKind kind) {
  switch (kind) {
    case FamiliarKind::kCelestial: return 4;
    case FamiliarKind::kFiend: return 5;
    default: return 1;
  }
}

bool FamiliarDeliversTouch(float target_from_caster_feet, float target_from_familiar_feet,
                           float familiar_from_caster_feet, bool reaction_ready) {
  return reaction_ready && target_from_caster_feet > static_cast<float>(kFamiliarReachFeet) &&
         target_from_familiar_feet <= static_cast<float>(kFamiliarReachFeet) &&
         familiar_from_caster_feet <= static_cast<float>(kFamiliarTelepathyFeet);
}

}  // namespace rules
