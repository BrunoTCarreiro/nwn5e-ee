#include "rules/action_economy.h"

#include "rules/class_features.h"

namespace rules {

bool BonusActionFree(bool used, bool round_ended_since, long long ms_since_use) {
  return !used || round_ended_since || ms_since_use >= kActionRoundMs;
}

bool TakesBonusAction(int feat) {
  return feat == kFeatSecondWind2 || feat == kFeatSecondWind3 || feat == kFeatSecondWind4 || IsRageFeat(feat) ||
         feat == kFeatIntimidatingPresence || feat == kFeatPatientDefense || feat == kFeatStepOfTheWind ||
         feat == kFeatWholenessOfBody;
}

bool TakesNoAction(int feat) {
  return feat == kFeatRecklessAttack || feat == kFeatSuperiorDefense || feat == kFeatBrutalStrike ||
         feat == kFeatCunningStrike || feat == kFeatDeviousStrikes || feat == kFeatOpenHandTechnique;
}

const char* BonusActionName(int feat) {
  if (feat == kFeatSecondWind2 || feat == kFeatSecondWind3 || feat == kFeatSecondWind4) return "Second Wind";
  if (IsRageFeat(feat)) return "Rage";
  if (feat == kFeatIntimidatingPresence) return "Intimidating Presence";
  if (feat == kFeatPatientDefense) return "Patient Defense";
  if (feat == kFeatStepOfTheWind) return "Step of the Wind";
  if (feat == kFeatWholenessOfBody) return "Wholeness of Body";
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

}  // namespace rules
