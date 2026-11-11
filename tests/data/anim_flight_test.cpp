// A look's flight (owner, 2026-11-10: "yes, start piece 2 with flight paths"; the design:
// docs/superpowers/specs/2026-11-10-spell-bench-flight-paths-design.md): the segments each kind is made of, as the game's
// visual transforms play them (nwscript.nss's easings), and the darts' fan and stagger. The bench's flight.js is held to
// the same numbers (spell_bench/checks/flight_check.js).

#include <math.h>

#include <string>
#include <vector>

#include "check.h"
#include "data/anim_flight.h"
#include "data/anim_look.h"

namespace {

bool Near(float a, float b) { return fabsf(a - b) < 0.001f; }

}  // namespace

TEST(AnimFlightLerps) {  // nwscript.nss's formulas
  CHECK_EQ(Near(data::FlightLerp(1, 0.5f), 0.5f) && Near(data::FlightLerp(2, 0.5f), 0.5f) && Near(data::FlightLerp(6, 0.5f), 0.25f), true);
  CHECK_EQ(Near(data::FlightLerp(4, 0.5f), 0.29289f) && Near(data::FlightLerp(5, 0.5f), 0.70711f) && Near(data::FlightLerp(7, 0.5f), 0.5f), true);
  CHECK_EQ(Near(data::FlightLerp(3, 0.25f), 0.32635f) && Near(data::FlightLerp(0, 0.01f), 1.0f) && Near(data::FlightLerp(0, 0.0f), 0.0f), true);
}

TEST(AnimFlightStraight) {
  data::FlightSettings s;
  s.kind = data::FlightKind::kStraight;
  s.speed = 12;
  auto seg = data::FlightSegments(s, {0, 0, 1}, {12, 0, 1}, 0, 1);
  CHECK_EQ(seg.size() == 1 && Near(seg[0].seconds, 1.0f) && Near(seg[0].to.x, 12) && seg[0].lerp_xy == 1, true);
  CHECK_EQ(Near(data::FlightOffset(seg, 0.5f).x, 6), true);
  s.speedup = true;
  CHECK_EQ(Near(data::FlightOffset(data::FlightSegments(s, {0, 0, 1}, {12, 0, 1}, 0, 1), 0.5f).x, 3), true);  // quadratic
}

TEST(AnimFlightArc) {
  data::FlightSettings s;
  s.kind = data::FlightKind::kArc;
  s.speed = 10;
  s.height = 2;
  s.fall = 0.4f;
  auto seg = data::FlightSegments(s, {0, 0, 0}, {10, 0, 0}, 0, 1);
  CHECK_EQ(seg.size() == 2 && Near(seg[0].seconds, 0.6f) && Near(seg[0].to.x, 6) && Near(seg[0].to.z, 2) && seg[0].lerp_z == 5, true);
  CHECK_EQ(Near(seg[1].seconds, 0.4f) && Near(seg[1].to.x, 10) && Near(seg[1].to.z, 0) && seg[1].lerp_z == 4, true);
  const data::FlightPoint p = data::FlightOffset(seg, 0.3f);
  CHECK_EQ(Near(p.x, 3) && Near(p.z, 1.41421f), true);
}

TEST(AnimFlightRiseDash) {
  data::FlightSettings s;
  s.kind = data::FlightKind::kRiseDash;
  s.height = 1.5f;
  s.rise = 0.4f;
  s.speed = 20;
  auto seg = data::FlightSegments(s, {0, 0, 0}, {10, 0, 0}, 0, 1);
  CHECK_EQ(Near(seg[0].seconds, 0.4f) && Near(seg[0].to.x, 1.0f) && Near(seg[0].to.z, 1.5f) && seg[0].lerp_xy == 5, true);
  CHECK_EQ(Near(seg[1].seconds, 0.45620f) && Near(seg[1].to.x, 10) && seg[1].lerp_xy == 6 && seg[1].lerp_z == 6, true);
  CHECK_EQ(Near(data::FlightSeconds(seg), 0.85620f), true);
}

TEST(AnimFlightHoming) {  // the rebuilt homing: today's MIRV time, a lift of 5% of the way
  data::FlightSettings s;
  auto seg = data::FlightSegments(s, {0, 0, 0}, {10, 0, 0}, 0, 1);
  CHECK_EQ(Near(seg[0].seconds, 0.5613f) && Near(seg[0].to.x, 5) && Near(seg[0].to.z, 0.5f) && seg[1].lerp_z == 4, true);
  CHECK_EQ(Near(data::FlightSeconds(seg), data::MirvFlightSeconds(10)), true);
}

TEST(AnimFlightDartsFan) {
  data::FlightSettings s;
  s.kind = data::FlightKind::kRiseDash;
  s.fan = 20;
  auto a = data::FlightSegments(s, {0, 0, 0}, {10, 0, 0}, 0, 3), b = data::FlightSegments(s, {0, 0, 0}, {10, 0, 0}, 1, 3),
       c = data::FlightSegments(s, {0, 0, 0}, {10, 0, 0}, 2, 3);
  CHECK_EQ(Near(a[0].to.x, 0.93969f) && Near(a[0].to.y, -0.34202f) && Near(b[0].to.y, 0) && Near(c[0].to.y, 0.34202f), true);
  CHECK_EQ(Near(a[1].to.x, 10) && Near(c[1].to.y, 0), true);  // each still lands on its target
  CHECK_EQ(Near(data::FlightStart(s, 2), 0.08f), true);
}

TEST(AnimFlightZeroDistance) {  // Review Focus 1: a target straight above - no NaN, a valid flight
  for (auto k : {data::FlightKind::kHoming, data::FlightKind::kArc, data::FlightKind::kStraight, data::FlightKind::kRiseDash}) {
    data::FlightSettings s;
    s.kind = k;
    auto seg = data::FlightSegments(s, {1, 1, 0}, {1, 1, 3}, 1, 3);
    const data::FlightPoint p = data::FlightOffset(seg, data::FlightSeconds(seg));
    CHECK_EQ(!seg.empty() && data::FlightSeconds(seg) >= 0.05f && Near(p.z, 3) && p.x == p.x, true);
  }
}

TEST(AnimFlightTwinRows) {  // the lasting-effect twin a missile flies on its carrier (data/looks/visualeffects.flight.look.txt)
  CHECK_EQ(data::FlightTwinRow(2100) == 2190 && data::FlightTwinRow(2105) == 2195 && data::FlightTwinRow(181) == 2196, true);
  CHECK_EQ(data::FlightTwinRow(503) == 2199 && data::FlightTwinRow(61) == 0, true);
  // The final review (2026-11-11): the arrows (233 fire, 245 acid) don't fly a twin until the carrier turns the model
  // along its path - on it an arrow would point north whatever way it flew; they keep the game's own MIRV, which does.
  CHECK_EQ(data::FlightTwinRow(233) == 0 && data::FlightTwinRow(245) == 0, true);
}

// The twin by the missile's projectile row (visualeffects ProgFX_Impact), so a bench copy of a missile (its sound
// changed: a new row, 2106 on, with the same ProgFX_Impact) still flies (the final review, 2026-11-11).
TEST(AnimFlightTwinForProgfx) {
  CHECK_EQ(data::FlightTwinForProgfx(910) == 2190 && data::FlightTwinForProgfx(915) == 2195, true);  // the orbs
  CHECK_EQ(data::FlightTwinForProgfx(900) == 2196 && data::FlightTwinForProgfx(905) == 2199, true);  // MIRV, electric
  CHECK_EQ(data::FlightTwinForProgfx(901) == 0 && data::FlightTwinForProgfx(902) == 0 && data::FlightTwinForProgfx(0) == 0,
           true);  // the arrows, none
}

// A flight to a point (a spell aimed at the ground, Task 6): its landing - the hit effect at the location, the plain
// hit sounds from a short-lived invisible placeable there, each at its own time. Breaks caught: the hit on the caster
// instead of the point, a sound with no object to play from, every sound bunched at the landing.
TEST(AnimFlightLanding) {
  const std::string code = data::FlightLandingScript({5, 0, 0}, 61, {{0.0f, "sim_explflame"}, {0.3f, "sim_x"}}, 1.25f);
  for (const char* part : {
           "void Land(location l, int hit, string s)",
           "ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(hit), l);",
           "CreateObject(OBJECT_TYPE_PLACEABLE, \"plc_invisobj\", l, FALSE, \"n5_land\")",
           "AssignCommand(o, PlaySound(s));",
           "void main()",
           "DelayCommand(1.250, Land(Location(GetArea(OBJECT_SELF), Vector(5.000, 0.000, 0.000), 0.0), 61, \"sim_explflame\"));",
           "DelayCommand(1.550, Land(Location(GetArea(OBJECT_SELF), Vector(5.000, 0.000, 0.000), 0.0), 0, \"sim_x\"));"}) {
    CHECK_EQ(code.find(part) != std::string::npos, true);
  }
  // No sound at the landing itself: the hit still lands then, alone.
  CHECK_EQ(data::FlightLandingScript({5, 0, 0}, 61, {}, 1.25f)
                   .find("DelayCommand(1.250, Land(Location(GetArea(OBJECT_SELF), Vector(5.000, 0.000, 0.000), 0.0), 61, \"\"));") !=
               std::string::npos,
           true);
}

TEST(AnimFlightScript) {  // the whole script that flies a dart: the carrier, the missile on it, the segments, the end
  data::FlightSettings s;
  s.kind = data::FlightKind::kRiseDash;
  s.speed = 20;
  const auto segs = data::FlightSegments(s, {0, 0, 0}, {10, 0, 0}, 0, 1);
  const std::string code = data::FlightScript(segs, {1, 2, 3}, 2190, 0.5f);
  for (const char* part : {
           "void Seg(object o, float x, float y, float z, int a, int b, float s)",
           "SetObjectVisualTransform(o, OBJECT_VISUAL_TRANSFORM_TRANSLATE_Z, z, b, s);",
           "void main()",
           // Facing north (90): a visual transform's translate runs along the carrier's own axes - forward is its +Y -
           // so facing 90 lines them up with the world's (SB-21, owner 2026-11-10: at facing 0 "the projectile fly 90º
           // the right of the character's facing direction").
           "CreateObject(OBJECT_TYPE_PLACEABLE, \"plc_invisobj\", Location(GetArea(OBJECT_SELF), Vector(1.000, 2.000, "
           "3.000), 90.0), FALSE, \"n5_flight\")",
           "DelayCommand(0.500, ApplyEffectToObject(DURATION_TYPE_TEMPORARY, EffectVisualEffect(2190), o, 0.956));",
           "DelayCommand(0.500, Seg(o, 1.000, 0.000, 1.500, 5, 5, 0.400));",
           "DelayCommand(0.900, Seg(o, 10.000, 0.000, 0.000, 6, 6, 0.456));",
           "DestroyObject(o, 1.556);"}) {
    CHECK_EQ(code.find(part) != std::string::npos, true);
  }
  CHECK_EQ(code.find("Get2DAString") == std::string::npos, true);  // no sound row: no flight sound
}

// The missile's own flight sound (visualeffects SoundDuration of the look's missile row - a copy's own too): the
// twin has none, so the flight plays it from the carrier as it leaves (the final review, 2026-11-11: Magic Missile's
// 181 flies with spr_magicmissile; a twin flew silent).
TEST(AnimFlightScriptPlaysTheMissileSound) {
  const auto segs = data::FlightSegments(data::FlightSettings(), {0, 0, 0}, {10, 0, 0}, 0, 1);
  const std::string code = data::FlightScript(segs, {1, 2, 3}, 2196, 0.5f, 181);
  CHECK_EQ(code.find("string s = Get2DAString(\"visualeffects\", \"SoundDuration\", 181);") != std::string::npos, true);
  CHECK_EQ(code.find("if (s != \"\") DelayCommand(0.500, AssignCommand(o, PlaySound(s)));") != std::string::npos, true);
}
