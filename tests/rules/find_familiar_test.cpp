#include "check.h"
#include "rules/find_familiar.h"
#include "rules/spells2024.h"

#include <string>

using namespace rules;

// #39 Part 2a build 4c (owner, 2026-11-16: "go with your recommendations for find familiar, excep allow the player to
// pick alignment too").
TEST(FindFamiliarForms) {
  CHECK_EQ(FamiliarFormOf(kFamiliarBat) == FamiliarForm::kBat, true);
  CHECK_EQ(FamiliarFormOf(kFamiliarCat) == FamiliarForm::kCat, true);
  CHECK_EQ(FamiliarFormOf(kFamiliarHawk) == FamiliarForm::kHawk, true);
  CHECK_EQ(FamiliarFormOf(kFamiliarLizard) == FamiliarForm::kLizard, true);
  CHECK_EQ(FamiliarFormOf(kFamiliarRat) == FamiliarForm::kRat, true);
  CHECK_EQ(FamiliarFormOf(kFamiliarRaven) == FamiliarForm::kRaven, true);
  CHECK_EQ(FamiliarFormOf(kFamiliarSpider) == FamiliarForm::kSpider, true);
  CHECK_EQ(FamiliarFormOf(kFamiliarWeasel) == FamiliarForm::kWeasel, true);
  CHECK_EQ(FamiliarFormOf(kFindFamiliar) == FamiliarForm::kNone, true);
  CHECK_EQ(FamiliarFormOf(kFamiliarDismiss) == FamiliarForm::kNone, true);
  // SRD 5.2.1's numbers
  const FamiliarStats& bat = FamiliarStatsOf(FamiliarForm::kBat);
  CHECK_EQ(bat.armor_class, 12);
  CHECK_EQ(bat.hit_points, 1);
  CHECK_EQ(bat.abilities[1], 15);  // Dexterity
  CHECK_EQ(bat.appearance, 10);
  const FamiliarStats& hawk = FamiliarStatsOf(FamiliarForm::kHawk);
  CHECK_EQ(hawk.armor_class, 13);
  CHECK_EQ(hawk.abilities[4], 14);  // Wisdom
  CHECK_EQ(hawk.appearance, 144);   // the falcon
  CHECK_EQ(FamiliarStatsOf(FamiliarForm::kCat).hit_points, 2);
  CHECK_EQ(FamiliarStatsOf(FamiliarForm::kCat).appearance, 203);  // the cougar, shrunk
  CHECK_EQ(FamiliarStatsOf(FamiliarForm::kCat).scale < 1.0f, true);
  CHECK_EQ(FamiliarStatsOf(FamiliarForm::kLizard).armor_class, 10);
  CHECK_EQ(FamiliarStatsOf(FamiliarForm::kLizard).appearance, 369);  // the basilisk, shrunk
  CHECK_EQ(FamiliarStatsOf(FamiliarForm::kRat).abilities[2], 9);     // Constitution
  CHECK_EQ(FamiliarStatsOf(FamiliarForm::kRaven).hit_points, 2);
  CHECK_EQ(FamiliarStatsOf(FamiliarForm::kRaven).abilities[3], 5);  // Intelligence
  CHECK_EQ(FamiliarStatsOf(FamiliarForm::kSpider).appearance, 159);
  CHECK_EQ(FamiliarStatsOf(FamiliarForm::kWeasel).armor_class, 13);
  CHECK_EQ(FamiliarStatsOf(FamiliarForm::kWeasel).appearance, 8);  // the badger, shrunk
  CHECK_EQ(std::string(FamiliarStatsOf(FamiliarForm::kWeasel).name) == "Weasel", true);
  CHECK_EQ(std::string(FamiliarStatsOf(FamiliarForm::kNone).name).empty(), true);
}

TEST(FindFamiliarKind) {
  CHECK_EQ(FamiliarKindFor(kFamiliarByAlignment, 4) == FamiliarKind::kCelestial, true);  // good
  CHECK_EQ(FamiliarKindFor(kFamiliarByAlignment, 5) == FamiliarKind::kFiend, true);      // evil
  CHECK_EQ(FamiliarKindFor(kFamiliarByAlignment, 1) == FamiliarKind::kFey, true);        // neutral
  CHECK_EQ(FamiliarKindFor(2, 4) == FamiliarKind::kFey, true);                            // picked over alignment
  CHECK_EQ(FamiliarKindFor(3, 4) == FamiliarKind::kFiend, true);
  CHECK_EQ(FamiliarKindFor(1, 5) == FamiliarKind::kCelestial, true);
  CHECK_EQ(std::string(FamiliarKindName(FamiliarKind::kFiend)) == "Fiend", true);
  CHECK_EQ(std::string(FamiliarKindName(FamiliarKind::kCelestial)) == "Celestial", true);
  CHECK_EQ(FamiliarRacialType(FamiliarKind::kFey), 17);
  CHECK_EQ(FamiliarRacialType(FamiliarKind::kCelestial), 20);
  CHECK_EQ(FamiliarRacialType(FamiliarKind::kFiend), 20);
  CHECK_EQ(FamiliarGoodEvil(FamiliarKind::kCelestial), 4);
  CHECK_EQ(FamiliarGoodEvil(FamiliarKind::kFiend), 5);
  CHECK_EQ(FamiliarGoodEvil(FamiliarKind::kFey), 1);
  // Detect Evil and Good senses each (Protection from Evil and Good's types)
  CHECK_EQ(std::string(DetectEvilAndGoodKind(FamiliarRacialType(FamiliarKind::kFiend),
                                             FamiliarGoodEvil(FamiliarKind::kFiend))) == "a Fiend",
           true);
  CHECK_EQ(std::string(DetectEvilAndGoodKind(FamiliarRacialType(FamiliarKind::kCelestial),
                                             FamiliarGoodEvil(FamiliarKind::kCelestial))) == "a Celestial",
           true);
  CHECK_EQ(std::string(DetectEvilAndGoodKind(FamiliarRacialType(FamiliarKind::kFey), 1)) == "a Fey", true);
}

TEST(FindFamiliarTouchAndRows) {
  CHECK_EQ(FamiliarDeliversTouch(30.0f, 5.0f, 40.0f, true), true);
  CHECK_EQ(FamiliarDeliversTouch(30.0f, 5.0f, 40.0f, false), false);   // its Reaction spent
  CHECK_EQ(FamiliarDeliversTouch(30.0f, 6.0f, 40.0f, true), false);    // out of its reach
  CHECK_EQ(FamiliarDeliversTouch(30.0f, 5.0f, 101.0f, true), false);   // more than 100 feet off
  CHECK_EQ(FamiliarDeliversTouch(5.0f, 3.0f, 10.0f, true), false);     // the caster touches it itself
  CHECK_EQ(kFamiliarRecallFeet, 30);
  // the spell: a Ritual, an hour's casting (70 minutes as a Ritual), the Wizard's alone
  CHECK_EQ(IsRitualSpell(kFindFamiliar) && IsRitualSpell(kFamiliarRaven), true);
  CHECK_EQ(IsRitualSpell(kFamiliarDismiss), false);
  CHECK_EQ(CastingMinutes(kFindFamiliar), 60);
  CHECK_EQ(CastingMinutes(kFamiliarCat), 60);
  CHECK_EQ(RitualCastingMinutes(CastingMinutes(kFamiliarCat)), 70);
  CHECK_EQ(WizardOnlySpell(kFindFamiliar) && WizardOnlySpell(kFamiliarSpider), true);
  CHECK_EQ(IsFamiliarFormRow(kFamiliarBat) && IsFamiliarFormRow(kFamiliarWeasel), true);
  CHECK_EQ(IsFamiliarFormRow(kFindFamiliar) || IsFamiliarFormRow(kFamiliarDismiss), false);
  CHECK_EQ(ChoiceOptionFeat(kFamiliarDismiss), kFeatFamiliarDismiss);
  CHECK_EQ(ChoiceOptionFeat(kFamiliarRecall), kFeatFamiliarRecall);
  CHECK_EQ(IsNoSlotOption(kFamiliarDismiss) && IsNoSlotOption(kFamiliarRecall), true);
  CHECK_EQ(Spell2024Of(kFindFamiliar) != nullptr && Spell2024Of(kFindFamiliar)->ritual, true);
}
