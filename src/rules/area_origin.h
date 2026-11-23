#pragma once

// Areas from each shape's point of origin (#166; owner, 2026-11-23: "for the area spell shouldn't it matter the
// direction? An obstacle would block a cone of cold as it comes from the enemy's position, but not an area effect that
// comes from above, right?"). SRD 5.2.1, Areas of Effect: "An area of effect has a point of origin, a location from
// which the effect's energy erupts... If all straight lines extending from the point of origin to a location in the
// area of effect are blocked, that location isn't included in the area of effect. To block a line, an obstruction
// must provide Total Cover."; Cylinder: "a point of origin located at the center of the circular top or bottom of the
// Cylinder". This file only decides.

#include <string>

#include "rules/cover.h"
#include "rules/spells2024.h"

namespace rules {

// Where an area's lines start.
enum class AreaOrigin {
  kCaster,      // a Cone or a Line: the caster (or the breather), at its eyes
  kCasterFace,  // a Cube from the caster (Thunderwave): the centre of the face on it, half the side up
  kCentre,      // a Sphere: its centre - the point the spell targets, kSphereCentreHeight over the ground there
  kTop,         // a Cylinder that comes down (Flame Strike, Ice Storm, Call Lightning ...), or an area that covers the
                // ground (Entangle, Grease, Faerie Fire's floor): the centre of its top - only overhead cover counts
  kCreature,    // an Emanation: its creature, at its eyes
};
constexpr float kSphereCentreHeight = 1.0f;  // a burst at about a creature's middle, not in the floor
constexpr int kCylinderHeightFeet = 40;  // Flame Strike's, Ice Storm's, Moonbeam's (SRD 5.2.1)

// nwn5e's own shapes.
AreaOrigin OriginOf2024(SpellShape2024 shape);
// NWN's spells.2da TargetShape for an unconverted spell: its cone and its spell cylinder (rectangle: a line) from the
// caster, its sphere, hsphere and cube from the target location; one that comes down (`from_above`) from its top; none
// ("****") from the caster.
AreaOrigin OriginOfNwn(const std::string& target_shape, bool from_above);
// NWN's own spells that come down from above, by spells.2da Label: Flame_Strike, Ice_Storm, Call_Lightning,
// Storm_of_Vengeance (and a Moonbeam, should one come).
bool ComesFromAbove(const std::string& label);
// The origin's height over its anchor's ground: the eyes for kCaster and kCreature; half the cube's side for
// kCasterFace; kSphereCentreHeight for kCentre; the area's height for kTop (`area_feet`, else a 40-foot cylinder).
float OriginHeight(AreaOrigin origin, float eye_height, int area_feet);
// Whether the origin is the caster's place (else the point the spell targets, or the area object's).
bool OriginAtCaster(AreaOrigin origin);

// What cover from the origin does: a creature behind Total Cover is left out of nwn5e's own area spells (no save, no
// effect); NWN's own keep it in, at +5 on a Dexterity save; Half and Three-Quarters give +2 / +5 on a Dexterity save.
enum class AreaCoverEffect { kNone, kSaveBonus, kLeftOut };
AreaCoverEffect AreaCoverEffectFor(Cover cover, bool own_spell, bool dexterity_save);

}  // namespace rules
