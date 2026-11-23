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

// #184's house rule, areas_fill_around (owner, 2026-11-23, on Fog Cloud by a bookcase: "inside the fog there should not
// be "shaded" areas as these are an area affect and not projected from a point. There doesn't seem to be an area
// without fog", then "come on, I think it makes sense it would go around corners. I know 5e explicitly said it and
// 2024 doesn't, but come on", and "bursts stay as 2024"). A lasting area - a cloud or a zone that stays where it's made
// - fills in round the placeables in it: from its origin only the tileset's walls and shut doors block its lines (it
// doesn't pass a solid wall into the next room), no placeable's box does. Instant cones, lines and bursts keep the
// 2024 rule (SRD 5.2.1, Areas of Effect: a location every line to which is blocked isn't in the area).
//
// What is lasting: an area-of-effect object's own area - any of them (its scripts run as it: a cloud's or a zone's
// every save) - and, as they're made (the save as an area appears, run by its caster's spell), the spells that make
// one: nwn5e's Entangle, Grease and Fog Cloud, NWN's Web, Cloudkill, Stinking Cloud and Incendiary Cloud. Not a
// spell's shape: Fog Cloud and Shatter are both spheres, Entangle and Faerie Fire both squares.
constexpr int kNwnWeb = 192, kNwnCloudkill = 23, kNwnStinkingCloud = 171, kNwnIncendiaryCloud = 89;
bool LastingAreaSpell(int spell);
// The area's lines start at an area-of-effect object (`from_area_object`), or `spell` makes one.
bool AreaIsLasting(bool from_area_object, int spell);
// Whether placeables' boxes block the area's lines from its origin: always, but for a lasting area under the house
// rule (`fill_around`).
bool AreaCountsPlaceables(bool lasting, bool fill_around);

// #166 / #184, decision A (owner, 2026-11-23: "A"): NWN's own unconverted area spells follow the 2024 rule now - a
// creature behind Total Cover from the spell's point of origin isn't in its area (no save, no effect), as in nwn5e's
// own. core/spell_gather.cpp leaves it out of the spell's own gather: GetFirstObjectInShape / GetNextObjectInShape
// while that spell's impact script runs on its caster (not a script it runs in turn, nor any other).
// - Which spells: an area - spells.2da TargetShape cone, sphere, hsphere, cube or rectangle (a line), any case - and
//   not one of nwn5e's own (`converted`: they leave a creature out in their own code, LeftOutOfArea).
bool NwnAreaSpell(const std::string& target_shape, bool converted);
// - Which objects the gather skips: a creature, not the caster, with Total Cover from the origin (Half and
//   Three-Quarters stay in: +2 / +5 on a Dexterity save).
bool GatherSkips(bool creature, bool caster, Cover cover);
// - The Dexterity save's cover bonus: Half +2, Three-Quarters +5; Total Cover +5 only where no gather left the creature
//   out (`gathered`: the save made in such a spell's script - its +5 for Total Cover gone, the creature isn't in it).
int AreaSaveCoverBonus(Cover cover, bool gathered);

}  // namespace rules
