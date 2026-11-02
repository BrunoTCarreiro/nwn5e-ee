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

DefaultBonusAction DecodeDefaultBonusAction(int record) {
  return record == 1 ? DefaultBonusAction::kNone : DefaultBonusAction::kBonusAttack;
}

int EncodeDefaultBonusAction(DefaultBonusAction choice) { return static_cast<int>(choice); }

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
