#include "rules/action_economy.h"

#include "rules/class_features.h"

namespace rules {

bool BonusActionFree(bool used, bool round_ended_since, long long ms_since_use) {
  return !used || round_ended_since || ms_since_use >= kActionRoundMs;
}

bool TakesBonusAction(int feat) {
  return feat == kFeatSecondWind2 || feat == kFeatSecondWind3 || feat == kFeatSecondWind4 || IsRageFeat(feat) ||
         feat == kFeatIntimidatingPresence || feat == kFeatPatientDefense || feat == kFeatStepOfTheWind ||
         feat == kFeatWholenessOfBody || feat == kFeatCunningDash || feat == kFeatCunningDisengage ||
         feat == kFeatCunningHide || feat == kFeatSteadyAim;  // #53
}

bool TakesNoAction(int feat) {
  return feat == kFeatRecklessAttack || feat == kFeatSuperiorDefense || feat == kFeatBrutalStrike ||
         feat == kFeatCunningStrike || feat == kFeatDeviousStrikes || feat == kFeatOpenHandTechnique ||
         feat == kFeatActionSurge;  // #53: Action Surge itself takes no action
}

const char* BonusActionName(int feat) {
  if (feat == kFeatSecondWind2 || feat == kFeatSecondWind3 || feat == kFeatSecondWind4) return "Second Wind";
  if (IsRageFeat(feat)) return "Rage";
  if (feat == kFeatIntimidatingPresence) return "Intimidating Presence";
  if (feat == kFeatPatientDefense) return "Patient Defense";
  if (feat == kFeatStepOfTheWind) return "Step of the Wind";
  if (feat == kFeatWholenessOfBody) return "Wholeness of Body";
  if (feat == kFeatCunningDash) return "Cunning Action: Dash";  // #53
  if (feat == kFeatCunningDisengage) return "Cunning Action: Disengage";
  if (feat == kFeatCunningHide) return "Cunning Action: Hide";
  if (feat == kFeatSteadyAim) return "Steady Aim";
  return "";
}

DefaultBonus DecodeDefaultBonus(int record) {
  DefaultBonus choice;
  if (record == 1) {
    choice.kind = DefaultBonusAction::kNone;
  } else if (record >= 2) {
    choice.kind = DefaultBonusAction::kFeat;
    choice.feat = record - 2;
  }
  return choice;
}

int EncodeDefaultBonus(DefaultBonus choice) {
  switch (choice.kind) {
    case DefaultBonusAction::kNone: return 1;
    case DefaultBonusAction::kFeat: return choice.feat >= 0 ? choice.feat + 2 : 0;
    case DefaultBonusAction::kBonusAttack: return 0;
  }
  return 0;
}

int BonusFeatFamily(int feat) {
  if (IsRageFeat(feat)) return kFeatRage;
  if (feat == kFeatSecondWind3 || feat == kFeatSecondWind4) return kFeatSecondWind2;
  return feat;
}

bool DefaultBonusPointless(int family, int hit_points, int max_hit_points) {
  const bool healing = family == kFeatSecondWind2 || family == kFeatWholenessOfBody;
  return healing && hit_points >= max_hit_points;
}

DefaultPlan PlanDefaultBonus(DefaultBonus choice, bool bonus_used, bool feat_ready) {
  DefaultPlan plan;
  if (choice.kind == DefaultBonusAction::kNone) {
    plan.attack = DefaultBonusAction::kNone;
    return plan;
  }
  plan.fire_feat = choice.kind == DefaultBonusAction::kFeat && !bonus_used && feat_ready;
  return plan;  // the bonus attack: the choice, or the ability's fallback
}

RoundBonus DecideRoundBonus(bool used, DefaultBonusAction choice, bool off_hand, bool martial_arts, bool nick) {
  RoundBonus round;
  const bool nick_off_hand = off_hand && nick;
  if (used || choice == DefaultBonusAction::kNone) {
    round.keep_off_hand = nick_off_hand;
    return round;
  }
  round.keep_off_hand = off_hand;
  round.keep_martial_arts = martial_arts;
  round.spends_bonus = martial_arts || (off_hand && !nick);
  return round;
}

DefaultAttackPick PickDefaultAttack(bool armed, bool in_combat, bool last_target_stands, bool nearest_found) {
  if (!armed || !in_combat) return DefaultAttackPick::kNone;
  if (last_target_stands) return DefaultAttackPick::kLastTarget;
  return nearest_found ? DefaultAttackPick::kNearest : DefaultAttackPick::kNone;
}

float DefaultAttackRange(bool ranged_weapon, int ranged_retarget_range) {
  if (!ranged_weapon || ranged_retarget_range <= 0) return kDefaultAttackMeleeRange;
  return static_cast<float>(ranged_retarget_range);
}

bool OrderKeepsAttack(int input_minor) {
  return input_minor == 0x12 || input_minor == 0x09 || input_minor == 0x06 || input_minor == 0x23;
}

OrderPlace PlaceOrder(bool attacking_mid_round, bool waits_for_round_end) {
  if (!attacking_mid_round) return OrderPlace::kAsNwn;
  return waits_for_round_end ? OrderPlace::kAfterAttacks : OrderPlace::kBeforeAttacks;
}

bool AttackYields(bool round_running, bool action_waiting) { return !round_running && action_waiting; }

bool IsPotionBaseItem(int base_item) { return base_item == 49 || base_item == 104; }

bool ReactionDefaultOn(Reaction reaction) { return reaction != Reaction::kDeflectRedirect; }

bool ReactionOn(int record, Reaction reaction) {
  const bool flipped = ((record >> static_cast<int>(reaction)) & 1) != 0;
  return ReactionDefaultOn(reaction) != flipped;
}

float DashMovementFactor(float factor, bool dashing) { return dashing ? factor * 2.0f : factor; }

bool HelpMedicineSucceeds(int d20, int medicine_bonus) { return d20 + medicine_bonus >= kHelpMedicineDc; }

int SetReactionOn(int record, Reaction reaction, bool on) {
  const int bit = 1 << static_cast<int>(reaction);
  return on == ReactionDefaultOn(reaction) ? (record & ~bit) : (record | bit);
}

}  // namespace rules
