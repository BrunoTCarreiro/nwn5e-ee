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

// 0.61.1 (owner: "select what bonus action they'd want to be triggered automatically"; "yes, build both as
// described"). Breaks caught: "never chosen" not the bonus attack, a choice not coming back (none, an ability), Rage's
// or Second Wind's successors not their family, a healing ability fired at full HP or skipped when hurt, another
// ability counted as healing, the ability fired with the Bonus Action spent or not ready, no fallback to the bonus
// attack, "none" making the bonus attack.
TEST(DefaultBonusChoice) {
  CHECK_EQ(DecodeDefaultBonus(0).kind == DefaultBonusAction::kBonusAttack, true);
  DefaultBonus none;
  none.kind = DefaultBonusAction::kNone;
  CHECK_EQ(DecodeDefaultBonus(EncodeDefaultBonus(none)).kind == DefaultBonusAction::kNone, true);
  DefaultBonus patient;
  patient.kind = DefaultBonusAction::kFeat;
  patient.feat = kFeatPatientDefense;
  const DefaultBonus back = DecodeDefaultBonus(EncodeDefaultBonus(patient));
  CHECK_EQ(back.kind == DefaultBonusAction::kFeat, true);
  CHECK_EQ(back.feat, kFeatPatientDefense);
  CHECK_EQ(DecodeDefaultBonus(EncodeDefaultBonus(DecodeDefaultBonus(kFeatWholenessOfBody + 2))).feat,
           kFeatWholenessOfBody);
  CHECK_EQ(BonusFeatFamily(326), kFeatRage);
  CHECK_EQ(BonusFeatFamily(331), kFeatRage);
  CHECK_EQ(BonusFeatFamily(kFeatSecondWind4), kFeatSecondWind2);
  CHECK_EQ(BonusFeatFamily(kFeatPatientDefense), kFeatPatientDefense);
  CHECK_EQ(DefaultBonusPointless(kFeatSecondWind2, 40, 40), true);
  CHECK_EQ(DefaultBonusPointless(kFeatSecondWind2, 39, 40), false);
  CHECK_EQ(DefaultBonusPointless(kFeatWholenessOfBody, 40, 40), true);
  CHECK_EQ(DefaultBonusPointless(kFeatPatientDefense, 40, 40), false);
  DefaultPlan plan = PlanDefaultBonus(patient, false, true);
  CHECK_EQ(plan.fire_feat, true);
  plan = PlanDefaultBonus(patient, true, true);  // one clicked or waiting went this round
  CHECK_EQ(plan.fire_feat, false);
  plan = PlanDefaultBonus(patient, false, false);  // out of Focus points: the bonus attack
  CHECK_EQ(plan.fire_feat, false);
  CHECK_EQ(plan.attack == DefaultBonusAction::kBonusAttack, true);
  plan = PlanDefaultBonus(none, false, true);
  CHECK_EQ(plan.fire_feat, false);
  CHECK_EQ(plan.attack == DefaultBonusAction::kNone, true);
  plan = PlanDefaultBonus(DecodeDefaultBonus(0), false, true);
  CHECK_EQ(plan.fire_feat, false);
  CHECK_EQ(plan.attack == DefaultBonusAction::kBonusAttack, true);
}
