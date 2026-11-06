#include "rules/radial_targeting.h"

#include <stdlib.h>

namespace rules {

namespace {

constexpr long kTargetSelf = 0x01;  // spells.2da TargetType: the caster
constexpr int64_t kFeatMask = 0xffff;
constexpr int64_t kFeatCursorExtraFeat = 5;
constexpr int64_t kFeatCursorExtraFrom = 0x430;  // past it: the extra goes to the cursor too

}  // namespace

RadialSpell RadialSpellFromRow(const std::string& range, const std::string& target_type,
                               const std::string& hostile_setting, const std::string& target_shape) {
  RadialSpell spell;
  spell.area = !target_shape.empty();
  spell.harmful = hostile_setting == "1";
  const bool personal = range == "P" || range == "p";
  const long types = target_type.empty() ? 0 : strtol(target_type.c_str(), nullptr, 0);
  spell.at_once = personal || types == kTargetSelf;
  spell.self_target = types == 0 || (types & kTargetSelf) != 0;
  return spell;
}

bool RadialRetargets(RadialOn on, const RadialSpell& spell) {
  if (spell.at_once) return false;
  switch (on) {
    case RadialOn::kGround:
      return true;
    case RadialOn::kOther:
      return spell.area;
    case RadialOn::kSelf:
      return spell.area || spell.harmful || !spell.self_target;
    case RadialOn::kAlly:
      return spell.area || spell.harmful;
  }
  return false;
}

int64_t CreatureMenuFeatCursorParam(int64_t param) {
  const int64_t feat = param & kFeatMask;
  if (feat != kFeatCursorExtraFeat && feat <= kFeatCursorExtraFrom) return feat;
  const int32_t extra = static_cast<int32_t>(static_cast<uint32_t>(param >> 16) << 16);
  return feat | static_cast<int64_t>(extra);
}

uint32_t CreatureMenuFeatUseParam(int64_t param) {
  return (static_cast<uint32_t>(param >> 16) << 16) | static_cast<uint32_t>(param & kFeatMask);
}

const char* RadialOnName(RadialOn on) {
  switch (on) {
    case RadialOn::kSelf:
      return "yourself";
    case RadialOn::kAlly:
      return "an ally";
    case RadialOn::kOther:
      return "a creature or object";
    case RadialOn::kGround:
      return "the ground";
  }
  return "?";
}

}  // namespace rules
