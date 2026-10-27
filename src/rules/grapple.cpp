#include "rules/grapple.h"

namespace rules {

namespace {
constexpr int kSizeTiny = 1;
}

int GrappleDc(int strength_modifier, int proficiency_bonus) { return 8 + strength_modifier + proficiency_bonus; }

bool GrappleSizeAllowed(int grappler_size, int target_size) { return target_size <= grappler_size + 1; }

bool DragCostsExtra(int grappler_size, int target_size, bool grappler_feat) {
  if (target_size <= kSizeTiny || target_size <= grappler_size - 2) return false;
  return !(grappler_feat && target_size <= grappler_size);
}

float DragSpeedFactor(int grappler_size, int target_size, bool grappler_feat) {
  return DragCostsExtra(grappler_size, target_size, grappler_feat) ? 0.5f : 1.0f;
}

bool GrappledAttackHasDisadvantage(bool target_is_grappler) { return !target_is_grappler; }

bool GrappledCreatureTriesToEscape(bool fighting_grappler) { return !fighting_grappler; }

int EscapeCheckBonus(int athletics_bonus, int acrobatics_bonus) {
  return athletics_bonus > acrobatics_bonus ? athletics_bonus : acrobatics_bonus;
}

bool ThrowSizeAllowed(int thrower_size, int target_size) { return target_size < thrower_size; }

int ThrowFeet(int strength_modifier) { return strength_modifier > 1 ? 5 * strength_modifier : 5; }

float FeetToMetres(int feet) { return static_cast<float>(feet) * 0.3048f; }

int ThrowDamageDice(int feet) { return feet >= 20 ? feet / 10 : 1; }

int ThrowDamageTaken(int rolled, bool saved) { return saved ? rolled / 2 : rolled; }

}  // namespace rules
