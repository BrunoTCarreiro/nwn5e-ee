#include "rules/area_origin.h"

#include <ctype.h>

namespace rules {

namespace {
std::string Lower(const std::string& text) {
  std::string out;
  for (char c : text) out += static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return out;
}
}  // namespace

AreaOrigin OriginOf2024(SpellShape2024 shape) {
  switch (shape) {
    case SpellShape2024::kCubeFromCaster: return AreaOrigin::kCasterFace;
    case SpellShape2024::kSphere: return AreaOrigin::kCentre;
    case SpellShape2024::kSquare: return AreaOrigin::kTop;
    case SpellShape2024::kCone:
    case SpellShape2024::kOne: break;
  }
  return AreaOrigin::kCaster;
}

AreaOrigin OriginOfNwn(const std::string& target_shape, bool from_above) {
  if (from_above) return AreaOrigin::kTop;
  const std::string s = Lower(target_shape);
  if (s == "sphere" || s == "hsphere" || s == "cube") return AreaOrigin::kCentre;
  return AreaOrigin::kCaster;  // cone, rectangle (a line), none
}

bool ComesFromAbove(const std::string& label) {
  const std::string s = Lower(label);
  return s == "flame_strike" || s == "ice_storm" || s == "call_lightning" || s == "storm_of_vengeance" ||
         s == "moonbeam";
}

float OriginHeight(AreaOrigin origin, float eye_height, int area_feet) {
  switch (origin) {
    case AreaOrigin::kCasterFace: return FeetToMetres(area_feet) * 0.5f;
    case AreaOrigin::kCentre: return kSphereCentreHeight;
    case AreaOrigin::kTop: return FeetToMetres(area_feet > 0 ? area_feet : kCylinderHeightFeet);
    case AreaOrigin::kCaster:
    case AreaOrigin::kCreature: break;
  }
  return eye_height;
}

bool OriginAtCaster(AreaOrigin origin) {
  return origin == AreaOrigin::kCaster || origin == AreaOrigin::kCasterFace || origin == AreaOrigin::kCreature;
}

AreaCoverEffect AreaCoverEffectFor(Cover cover, bool own_spell, bool dexterity_save) {
  if (cover == Cover::kTotal && own_spell) return AreaCoverEffect::kLeftOut;
  if (cover != Cover::kNone && dexterity_save) return AreaCoverEffect::kSaveBonus;
  return AreaCoverEffect::kNone;
}

bool LastingAreaSpell(int spell) {
  switch (spell) {
    case kEntangle:
    case kGrease:
    case kFogCloud:
    case kNwnWeb:
    case kNwnCloudkill:
    case kNwnStinkingCloud:
    case kNwnIncendiaryCloud: return true;
    default: break;
  }
  return false;
}

bool AreaIsLasting(bool from_area_object, int spell) { return from_area_object || LastingAreaSpell(spell); }

bool AreaCountsPlaceables(bool lasting, bool fill_around) { return !(lasting && fill_around); }

}  // namespace rules
