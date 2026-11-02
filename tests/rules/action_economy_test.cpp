#include "check.h"
#include "rules/action_economy.h"
#include "rules/class_features.h"

using namespace rules;

// Phase 6 part 1 (#52; owner, 2026-11-02): one Bonus Action a round. Breaks caught: a used Bonus Action free in
// the same round, never free again, free out of combat before a round's time, an unused one refused.
TEST(BonusActionOnceARound) {
  CHECK_EQ(BonusActionFree(false, false, 0), true);
  CHECK_EQ(BonusActionFree(true, false, 1000), false);
  CHECK_EQ(BonusActionFree(true, true, 1000), true);    // its next round
  CHECK_EQ(BonusActionFree(true, false, 5999), false);
  CHECK_EQ(BonusActionFree(true, false, 6000), true);   // out of combat: a round's time
}

// Breaks caught: a 2024 Bonus Action feat left an action (Second Wind, Rage and a successor, Intimidating
// Presence, Patient Defense, Step of the Wind, Wholeness of Body), a free one costing the Bonus Action (Reckless
// Attack, Superior Defense, the pickers), an action ability (Quivering Palm, Stunning Fist) taken as either.
TEST(BonusActionFeats) {
  for (int feat : {kFeatSecondWind2, kFeatSecondWind3, kFeatSecondWind4, kFeatRage, 326, 331,
                   kFeatIntimidatingPresence, kFeatPatientDefense, kFeatStepOfTheWind, kFeatWholenessOfBody}) {
    CHECK_EQ(TakesBonusAction(feat), true);
    CHECK_EQ(TakesNoAction(feat), false);
    CHECK_EQ(BonusActionName(feat)[0] != 0, true);
  }
  for (int feat : {kFeatRecklessAttack, kFeatSuperiorDefense, kFeatBrutalStrike, kFeatCunningStrike,
                   kFeatDeviousStrikes, kFeatOpenHandTechnique}) {
    CHECK_EQ(TakesNoAction(feat), true);
    CHECK_EQ(TakesBonusAction(feat), false);
  }
  for (int feat : {kFeatQuiveringPalm, kFeatStunningFist, kFeatFlurryOfBlows}) {
    CHECK_EQ(TakesBonusAction(feat), false);
    CHECK_EQ(TakesNoAction(feat), false);
  }
}

// Breaks caught: "never chosen" not the bonus attack, a choice not coming back; a round keeping its bonus attack
// after the Bonus Action went (or with "none"), the Monk's strike kept then, a Nick off-hand attack dropped or
// spending the Bonus Action, a plain off-hand attack or the Monk's strike not spending it, a round with no bonus
// attack spending it.
TEST(BonusActionRoundStart) {
  CHECK_EQ(DecodeDefaultBonusAction(0) == DefaultBonusAction::kBonusAttack, true);
  CHECK_EQ(DecodeDefaultBonusAction(EncodeDefaultBonusAction(DefaultBonusAction::kNone)) == DefaultBonusAction::kNone,
           true);
  const DefaultBonusAction attack = DefaultBonusAction::kBonusAttack, none = DefaultBonusAction::kNone;
  RoundBonus r = DecideRoundBonus(false, attack, true, false, false);  // two weapons
  CHECK_EQ(r.keep_off_hand, true);
  CHECK_EQ(r.keep_martial_arts, false);
  CHECK_EQ(r.spends_bonus, true);
  r = DecideRoundBonus(false, attack, true, false, true);  // Nick: part of the Attack action
  CHECK_EQ(r.keep_off_hand, true);
  CHECK_EQ(r.spends_bonus, false);
  r = DecideRoundBonus(false, attack, false, true, false);  // a Monk's strike or Flurry
  CHECK_EQ(r.keep_martial_arts, true);
  CHECK_EQ(r.spends_bonus, true);
  r = DecideRoundBonus(true, attack, true, false, false);  // the Bonus Action already went
  CHECK_EQ(r.keep_off_hand, false);
  CHECK_EQ(r.spends_bonus, false);
  r = DecideRoundBonus(true, attack, false, true, false);
  CHECK_EQ(r.keep_martial_arts, false);
  r = DecideRoundBonus(true, attack, true, false, true);  // Nick's still part of the Attack action
  CHECK_EQ(r.keep_off_hand, true);
  r = DecideRoundBonus(false, none, true, true, false);  // kept free
  CHECK_EQ(r.keep_off_hand, false);
  CHECK_EQ(r.keep_martial_arts, false);
  CHECK_EQ(r.spends_bonus, false);
  r = DecideRoundBonus(false, attack, false, false, false);  // nothing to make
  CHECK_EQ(r.spends_bonus, false);
}
