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
                   kFeatIntimidatingPresence, kFeatPatientDefense, kFeatStepOfTheWind, kFeatWholenessOfBody,
                   1548}) {  // #39 build 3a: Produce Flame's Bonus Action feat
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

// Part 2 (#120; owner, 2026-11-02): the default attack. Breaks caught: one out of combat, one never ordered (or
// after a move), the nearest enemy preferred to the last target standing, no fallback when it fell, an attack on
// nobody.
TEST(DefaultAttackTarget) {
  CHECK_EQ(PickDefaultAttack(true, true, true, true) == DefaultAttackPick::kLastTarget, true);
  CHECK_EQ(PickDefaultAttack(true, true, true, false) == DefaultAttackPick::kLastTarget, true);
  CHECK_EQ(PickDefaultAttack(true, true, false, true) == DefaultAttackPick::kNearest, true);  // it fell
  CHECK_EQ(PickDefaultAttack(true, true, false, false) == DefaultAttackPick::kNone, true);
  CHECK_EQ(PickDefaultAttack(true, false, true, true) == DefaultAttackPick::kNone, true);  // the fight is over
  CHECK_EQ(PickDefaultAttack(false, true, true, true) == DefaultAttackPick::kNone, true);  // never ordered / moved
}

// Part 2 (#120): the Action queue. Breaks caught: a move or an attack order keeping the attack (they replace the
// queue, as in NWN), a spell / item / feat / talent order dropping it, a spell placed before the round's attacks, a
// potion after them, an order placed while not attacking, the attack yielding mid-round or with nothing waiting.
TEST(ActionQueueOrders) {
  CHECK_EQ(OrderKeepsAttack(0x12), true);   // a spell
  CHECK_EQ(OrderKeepsAttack(0x09), true);   // an item
  CHECK_EQ(OrderKeepsAttack(0x06), true);   // a feat
  CHECK_EQ(OrderKeepsAttack(0x23), true);   // a talent
  CHECK_EQ(OrderKeepsAttack(0x01), false);  // a move
  CHECK_EQ(OrderKeepsAttack(0x02), false);  // an attack
  CHECK_EQ(PlaceOrder(true, true) == OrderPlace::kAfterAttacks, true);
  CHECK_EQ(PlaceOrder(true, false) == OrderPlace::kBeforeAttacks, true);
  CHECK_EQ(PlaceOrder(false, true) == OrderPlace::kAsNwn, true);
  CHECK_EQ(PlaceOrder(false, false) == OrderPlace::kAsNwn, true);
  CHECK_EQ(AttackYields(false, true), true);
  CHECK_EQ(AttackYields(true, true), false);
  CHECK_EQ(AttackYields(false, false), false);
}

// Part 3 (#54; owner, 2026-11-02): the reaction switches. Breaks caught: a reaction off by default (or the redirect
// on), a switch that doesn't stick, one switch turning another, a switch back that leaves a bit behind.
TEST(ReactionSwitches) {
  CHECK_EQ(ReactionOn(0, Reaction::kOpportunityAttack), true);
  CHECK_EQ(ReactionOn(0, Reaction::kUncannyDodge), true);
  CHECK_EQ(ReactionOn(0, Reaction::kDeflectAttacks), true);
  CHECK_EQ(ReactionOn(0, Reaction::kRetaliation), true);
  CHECK_EQ(ReactionOn(0, Reaction::kDeflectRedirect), false);  // it spends a Focus Point
  const int no_aoo = SetReactionOn(0, Reaction::kOpportunityAttack, false);
  CHECK_EQ(ReactionOn(no_aoo, Reaction::kOpportunityAttack), false);
  CHECK_EQ(ReactionOn(no_aoo, Reaction::kUncannyDodge), true);
  CHECK_EQ(ReactionOn(no_aoo, Reaction::kDeflectRedirect), false);
  const int redirect = SetReactionOn(no_aoo, Reaction::kDeflectRedirect, true);
  CHECK_EQ(ReactionOn(redirect, Reaction::kDeflectRedirect), true);
  CHECK_EQ(ReactionOn(redirect, Reaction::kOpportunityAttack), false);
  CHECK_EQ(ReactionOn(SetReactionOn(redirect, Reaction::kRetaliation, false), Reaction::kRetaliation), false);
  CHECK_EQ(SetReactionOn(SetReactionOn(redirect, Reaction::kOpportunityAttack, true), Reaction::kDeflectRedirect, false),
           0);
  CHECK_EQ(SetReactionOn(0, Reaction::kUncannyDodge, true), 0);  // already its default
}

// #56 (owner, 2026-11-02): Dash doubles the speed for its round; Help's house rule is a DC 10 Medicine check.
// Breaks caught: Dash not doubling, doubling when not dashing; Help one short, Help needing more than 10.
// #53: Cunning Action and Steady Aim take the Bonus Action, Action Surge none; their names. Breaks caught: one left
// an action, Action Surge costing the Bonus Action, a name missing.
TEST(ExtraActionsEconomy) {
  CHECK_EQ(TakesBonusAction(kFeatCunningDash), true);
  CHECK_EQ(TakesBonusAction(kFeatCunningDisengage), true);
  CHECK_EQ(TakesBonusAction(kFeatCunningHide), true);
  CHECK_EQ(TakesBonusAction(kFeatSteadyAim), true);
  CHECK_EQ(TakesBonusAction(kFeatActionSurge), false);
  CHECK_EQ(TakesNoAction(kFeatActionSurge), true);
  CHECK_EQ(TakesNoAction(kFeatSteadyAim), false);
  CHECK_EQ(std::string(BonusActionName(kFeatCunningDash)) == "Cunning Action: Dash", true);
  CHECK_EQ(std::string(BonusActionName(kFeatCunningHide)) == "Cunning Action: Hide", true);
  CHECK_EQ(std::string(BonusActionName(kFeatSteadyAim)) == "Steady Aim", true);
  // #55: the Monk's free Patient Defense / Step of the Wind take the Bonus Action.
  CHECK_EQ(TakesBonusAction(kFeatPatientDefenseFree), true);
  CHECK_EQ(TakesBonusAction(kFeatStepOfTheWindFree), true);
  CHECK_EQ(std::string(BonusActionName(kFeatPatientDefenseFree)) == "Patient Defense: Disengage", true);
  CHECK_EQ(std::string(BonusActionName(kFeatStepOfTheWindFree)) == "Step of the Wind: Dash", true);
}

TEST(StapleActions) {
  CHECK_EQ(DashMovementFactor(1.0f, true) == 2.0f, true);
  CHECK_EQ(DashMovementFactor(1.33f, true) == 2.66f, true);
  CHECK_EQ(DashMovementFactor(1.0f, false) == 1.0f, true);
  CHECK_EQ(HelpMedicineSucceeds(7, 3), true);   // 10
  CHECK_EQ(HelpMedicineSucceeds(6, 3), false);  // 9
  CHECK_EQ(HelpMedicineSucceeds(10, 0), true);
  CHECK_EQ(HelpMedicineSucceeds(12, -3), false);
}

// Breaks caught: a melee weapon looking as far as a bow, a bow looking 10 m, a bad setting giving no range;
// potions not known, another item taken for one.
TEST(DefaultAttackRangeAndPotions) {
  CHECK_EQ(DefaultAttackRange(false, 40) == kDefaultAttackMeleeRange, true);
  CHECK_EQ(DefaultAttackRange(true, 40) == 40.0f, true);
  CHECK_EQ(DefaultAttackRange(true, 25) == 25.0f, true);
  CHECK_EQ(DefaultAttackRange(true, 0) == kDefaultAttackMeleeRange, true);
  CHECK_EQ(IsPotionBaseItem(49), true);
  CHECK_EQ(IsPotionBaseItem(54), false);  // a deleted row
  CHECK_EQ(IsPotionBaseItem(104), true);
  CHECK_EQ(IsPotionBaseItem(46), false);   // a wand
  CHECK_EQ(IsPotionBaseItem(75), false);   // a scroll
  CHECK_EQ(IsPotionBaseItem(-1), false);
}
