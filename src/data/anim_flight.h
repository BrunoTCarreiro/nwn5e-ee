#pragma once

// A look's flight (owner, 2026-11-10: "yes, start piece 2 with flight paths";
// docs/superpowers/specs/2026-11-10-spell-bench-flight-paths-design.md). A flight is a short list of segments, each a
// duration, the offset from the launch point it ends at, and one of the game's visual-transform easings for the ground
// plane (x and y share it, so each segment is straight seen from above) and one for height. The game plays them with
// SetObjectVisualTransform on an invisible carrier; the bench's spell_bench/public/flight.js builds and evaluates the
// same segments, so the preview matches the game.

#include <string>
#include <vector>

namespace data {

struct FlightPoint {
  float x = 0.0f, y = 0.0f, z = 0.0f;
};

enum class FlightKind { kHoming, kArc, kStraight, kRiseDash };
bool FlightKindFromName(const std::string& name, FlightKind* kind);  // homing, arc, straight, rise_dash
const char* FlightKindName(FlightKind kind);

struct FlightSettings {
  FlightKind kind = FlightKind::kHoming;
  float speed = 12.0f;    // m/s (1-60): arc and straight - along the ground; rise-and-dash - the dash
  float height = 1.5f;    // m (0-10): arc - the apex over the line; rise-and-dash - the rise
  float rise = 0.4f;      // s (0.05-3): rise-and-dash - the slow part
  float fall = 0.4f;      // 0.1-0.9: arc - the share of its time spent falling (lower falls faster)
  bool speedup = false;   // straight - speeds up toward the end
  float fan = 20.0f;      // degrees (0-90) between neighbouring darts
  float stagger = 0.04f;  // s (0-0.5) between neighbouring darts' starts
};

// `to`: the offset from the launch point at the segment's end (metres); the lerps: OBJECT_VISUAL_TRANSFORM_LERP_*.
struct FlightSegment {
  float seconds = 0.0f;
  FlightPoint to;
  int lerp_xy = 1, lerp_z = 1;
};

// nwscript.nss's easings for x in 0..1: 0 none (1 at once), 1 linear, 2 smoothstep, 3 inverse smoothstep, 4 ease in,
// 5 ease out, 6 quadratic, 7 smootherstep.
float FlightLerp(int type, float x);

// One dart's flight from `from` to `to` (world metres): dart `dart` of `darts` (0-based; a lone missile is 0 of 1).
std::vector<FlightSegment> FlightSegments(const FlightSettings& settings, FlightPoint from, FlightPoint to, int dart,
                                          int darts);
float FlightSeconds(const std::vector<FlightSegment>& segments);
// The offset at `t` seconds into the flight (clamped), as the game's lerps draw it.
FlightPoint FlightOffset(const std::vector<FlightSegment>& segments, float t);
// When dart `dart` leaves, in seconds after the first.
float FlightStart(const FlightSettings& settings, int dart);

// The lasting-effect twin (data/looks/visualeffects.flight.look.txt, rows 2190-2199) a missile row flies as on its
// carrier: the orbs 2100-2105 -> 2190-2195, VFX_IMP_MIRV 181 -> 2196, VFX_IMP_MIRV_ELECTRIC 503 -> 2199; 0 for any
// other (it flies the game's way). The arrows (233 fire, 245 acid; twins 2197, 2198) aren't mapped until the carrier
// turns the model along its path - on it an arrow would point north whatever way it flew (the final review, 2026-11-11).
int FlightTwinRow(int missile_row);
// The same by the missile's projectile row (visualeffects ProgFX_Impact), for a row FlightTwinRow doesn't know - a bench
// copy of a missile (its sound changed) has the same ProgFX_Impact: 910-915 -> 2190-2195, 900 -> 2196, 905 -> 2199.
int FlightTwinForProgfx(int progfx_row);

// The whole NWScript (its own main(): RunWholeScript, run as the caster) that flies one dart: an invisible carrier
// (plc_invisobj, tag "n5_flight") at `from`, `twin_row` shown on it from `start` for the flight, each segment played by
// SetObjectVisualTransform through a void helper DelayCommand times, the carrier gone after. Times and metres %.3f.
// `sound_row` (the look's missile row; 0 none): its own flight sound (visualeffects SoundDuration) plays from the carrier
// as it leaves - the twin has none.
std::string FlightScript(const std::vector<FlightSegment>& segments, FlightPoint from, int twin_row, float start,
                         int sound_row = 0);

// A flight's landing at a point (a spell aimed at the ground): a whole script (RunWholeScript, run as the caster) whose
// helper Land(location, hit, sound) applies `hit_row` at the point (ApplyEffectAtLocation) and plays a plain sound
// there from a short-lived invisible placeable (tag "n5_land"). The hit lands at `at_time` with the sounds `after` 0;
// each other sound at its own time after it.
struct FlightLandingSound {
  float after = 0.0f;  // s after the landing
  std::string resref;
};
std::string FlightLandingScript(FlightPoint at, int hit_row, const std::vector<FlightLandingSound>& sounds, float at_time);

}  // namespace data
