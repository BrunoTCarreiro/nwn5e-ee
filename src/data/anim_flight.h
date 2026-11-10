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

}  // namespace data
