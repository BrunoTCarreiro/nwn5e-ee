#pragma once

// The vision cones cut at walls (#9 c; owner, 2026-11-15: "yes" - the rule already needs a clear line of sight, only
// the drawing ignored walls). A hostile's vision cone, drawn while a player character hides (core/stealth_watch.cpp),
// is a fan of slices kConeSliceDegrees wide, each as long as the creature's line of sight along its middle - the
// engine's own CNWSArea::ClearLineOfSight, from its eyes as its perception calls it - rounded down to a drawn length
// (kConeSliceLengths), never past its sight range. The rule agrees with the drawing (core/rules_perception.cpp): a
// hostile whose cone is drawn in slices sees a hider only inside a drawn slice, no farther than that slice's length.
// The sweeping cone (#9; owner, 2026-11-22: "A, go with the sweeping cone"): a searching hostile keeps a cone too,
// wider (search_cone), sweeping side to side round its facing as it scans (SweepAngle), stepped a slice at a time; it
// sees a hider only inside its cone as it points now. The slice positions go all round the creature for it. This
// file only decides; the engine side measures and draws.

#include <stdint.h>

#include <functional>

namespace rules {

constexpr float kConeSliceDegrees = 15.0f;
// Round the creature: position p covers -180 + 15 p .. -165 + 15 p degrees off its facing (anticlockwise +); the
// facing is the line between 11 and 12.
constexpr int kConeSlicePositions = 24;
constexpr int kConeSliceLengthCount = 16;
// Metres, shortest first (tools/data/stealth_rings.py has the same list): fine steps near the creature, where a wall
// is most often, coarser far out.
extern const float kConeSliceLengths[kConeSliceLengthCount];
constexpr int kConeSliceColours = 5;  // 0 yellow (very unlikely to see the hider) .. 4 red (very likely), as the cones
// visualeffects.2da: the front ten positions (7-16: -75 .. +75 degrees, #9 part 1; visualeffects.slices.txt) at
// 2260 + 160 x colour + 16 x (p - 7) + the length's index; the other fourteen (0-6 and 17-23; the sweep's,
// visualeffects.sweep.txt) at 3060 + 224 x colour + 16 x j + the length's index, j = p for 0-6, p - 10 for 17-23.
constexpr int kConeSliceRow = 2260;
constexpr int kConeSliceRowLast = 3059;
constexpr int kConeSliceRearRow = 3060;
constexpr int kConeSliceRearRowLast = 4179;

// The drawn cone for `degrees`: the nearest multiple of 30 (an even count of slices, centred on its line), 30-330.
int DrawnConeDegrees(int degrees);

// The positions a cone `drawn_degrees` wide takes, centred `step` x 15 degrees off the facing (anticlockwise +):
// [first, first + count) round the circle (positions past 23 wrap to 0).
struct SliceSpan {
  int first = 0;
  int count = 0;
};
SliceSpan ConeSliceSpan(int drawn_degrees, int step = 0);
bool InSliceSpan(const SliceSpan& span, int position);

// Position `p`'s middle, degrees off the facing (anticlockwise +): -172.5 + 15 p.
float ConeSliceMiddle(int position);

// The position a bearing `degrees_off_facing` falls in (any angle; wrapped round).
int ConeSliceAt(float degrees_off_facing);

// Degrees from a facing (fx, fy) to a direction (dx, dy), anticlockwise +, -180 .. 180; 0 for a zero vector.
float DegreesOffFacing(float dx, float dy, float fx, float fy);

// The longest drawn length of at most `metres` (an index into kConeSliceLengths), -1 below the shortest.
int ConeSliceLengthAtMost(float metres);

// How far a slice's line of sight reaches: the farthest of the lengths 0 .. `top` whose line is clear (`clear(i)`:
// the line out to kConeSliceLengths[i] is clear), -1 when not even the shortest one is. A line is taken to stay clear
// up to where it is first blocked. `hint` (an index; -1 none): the length just short of where the engine said the
// line was blocked - tried first, and taken when clear (the next one up is past the block).
int ConeSliceReach(int top, int hint, const std::function<bool(int)>& clear);

// A slice's visualeffects.2da row (0 for a part out of range), and back.
int ConeSliceRow(int colour, int position, int length_index);
bool ConeSliceRowParts(int row, int* colour, int* position, int* length_index);

// The rule's side: with the rows drawn at each position (0 = none there), may the creature see a hider `metres`
// away, `degrees_off_facing` off its facing? Only inside a drawn slice, no farther than its length.
bool SeenInsideSlices(const int rows[kConeSlicePositions], float degrees_off_facing, float metres);

// A drawn set's signature: 0 when nothing is drawn, never 0 otherwise.
int ConeSliceSignature(const int rows[kConeSlicePositions]);

// The sweep (owner, 2026-11-22: "A, go with the sweeping cone"): a searching creature's cone, cone_degrees wide,
// swings sweep_degrees to either side of its facing and back every period_ms, holding pause_ms at each end - linear in
// between. nwn5e.ini: search_cone, search_sweep_degrees, search_sweep_ms, search_sweep_pause_ms.
struct SearchSweep {
  int cone_degrees = 120;
  int sweep_degrees = 60;
  int period_ms = 4000;
  int pause_ms = 500;
};
// The keys as read, made sane: the cone drawn as DrawnConeDegrees does, the sweep 0-180 degrees, the period
// 1000-60000 ms, the pause 0 up to half the period less 200 ms (it always moves).
SearchSweep SaneSearchSweep(SearchSweep sweep);

// Where the cone points `ms` after the search began, degrees off the facing (anticlockwise +): it starts on the
// facing, swinging left (anticlockwise).
float SweepAngle(const SearchSweep& sweep, int64_t ms);

// The cone's step as drawn (15-degree steps): the angle rounded to the nearest step.
int SweepStep(float degrees);

// Every position the sweeping cone covers over its swing (what is measured for it), capped at the whole circle.
SliceSpan SweepRange(const SearchSweep& sweep);

// The rule for a searching creature whose cone isn't drawn: is a hider `degrees_off_facing` off its facing inside
// its cone as it points `ms` after the search began (the drawn step's positions)?
bool InSweepCone(const SearchSweep& sweep, int64_t ms, float degrees_off_facing);

}  // namespace rules
