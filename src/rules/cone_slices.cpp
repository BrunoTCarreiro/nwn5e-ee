#include "rules/cone_slices.h"

#include <math.h>
#include <stdint.h>

namespace rules {

const float kConeSliceLengths[kConeSliceLengthCount] = {1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 30, 35, 40, 50};

namespace {
constexpr float kPi = 3.14159265f;
constexpr int kFrontFirst = 7;   // the front ten positions: #9 part 1's rows
constexpr int kFrontLast = 16;
constexpr int kRearPerColour = (kConeSlicePositions - (kFrontLast - kFrontFirst + 1)) * kConeSliceLengthCount;
int Wrap(int position) { return ((position % kConeSlicePositions) + kConeSlicePositions) % kConeSlicePositions; }
int Clamp(int value, int low, int high) { return value < low ? low : (value > high ? high : value); }
}  // namespace

int DrawnConeDegrees(int degrees) {
  const int drawn = (degrees + 15) / 30 * 30;
  return Clamp(drawn, 30, 330);
}

SliceSpan ConeSliceSpan(int drawn_degrees, int step) {
  SliceSpan span;
  span.count = Clamp(static_cast<int>(drawn_degrees / kConeSliceDegrees + 0.5f), 0, kConeSlicePositions);
  span.first = Wrap(kConeSlicePositions / 2 - span.count / 2 + step);
  return span;
}

bool InSliceSpan(const SliceSpan& span, int position) {
  return Wrap(position - span.first) < span.count;
}

float ConeSliceMiddle(int position) {
  return -180.0f + kConeSliceDegrees * (static_cast<float>(position) + 0.5f);
}

int ConeSliceAt(float degrees_off_facing) {
  float d = fmodf(degrees_off_facing + 180.0f, 360.0f);
  if (d < 0.0f) d += 360.0f;
  return Wrap(static_cast<int>(d / kConeSliceDegrees));
}

float DegreesOffFacing(float dx, float dy, float fx, float fy) {
  if ((dx == 0.0f && dy == 0.0f) || (fx == 0.0f && fy == 0.0f)) return 0.0f;
  float d = (atan2f(dy, dx) - atan2f(fy, fx)) * 180.0f / kPi;
  while (d > 180.0f) d -= 360.0f;
  while (d < -180.0f) d += 360.0f;
  return d;
}

int ConeSliceLengthAtMost(float metres) {
  int best = -1;
  for (int i = 0; i < kConeSliceLengthCount; ++i) {
    if (kConeSliceLengths[i] <= metres + 1e-4f) best = i;
  }
  return best;
}

int ConeSliceReach(int top, int hint, const std::function<bool(int)>& clear) {
  if (top < 0) return -1;
  if (top >= kConeSliceLengthCount) top = kConeSliceLengthCount - 1;
  int clear_to = -1;           // known clear up to this index
  int blocked_from = top + 1;  // known blocked from this index (past `top`: not looked at)
  if (hint >= 0 && hint <= top) {
    if (clear(hint)) return hint;  // the next length up is past where the engine found the block
    blocked_from = hint;
  }
  while (blocked_from - clear_to > 1) {
    const int mid = (clear_to + blocked_from) / 2;
    if (clear(mid)) {
      clear_to = mid;
    } else {
      blocked_from = mid;
    }
  }
  return clear_to;
}

int ConeSliceRow(int colour, int position, int length_index) {
  if (colour < 0 || colour >= kConeSliceColours || position < 0 || position >= kConeSlicePositions ||
      length_index < 0 || length_index >= kConeSliceLengthCount) {
    return 0;
  }
  if (position >= kFrontFirst && position <= kFrontLast) {
    return kConeSliceRow + colour * (kFrontLast - kFrontFirst + 1) * kConeSliceLengthCount +
           (position - kFrontFirst) * kConeSliceLengthCount + length_index;
  }
  const int j = position < kFrontFirst ? position : position - (kFrontLast - kFrontFirst + 1);
  return kConeSliceRearRow + colour * kRearPerColour + j * kConeSliceLengthCount + length_index;
}

bool ConeSliceRowParts(int row, int* colour, int* position, int* length_index) {
  if (row >= kConeSliceRow && row <= kConeSliceRowLast) {
    const int at = row - kConeSliceRow;
    const int per_colour = (kFrontLast - kFrontFirst + 1) * kConeSliceLengthCount;
    *colour = at / per_colour;
    *position = kFrontFirst + at % per_colour / kConeSliceLengthCount;
    *length_index = at % kConeSliceLengthCount;
    return true;
  }
  if (row >= kConeSliceRearRow && row <= kConeSliceRearRowLast) {
    const int at = row - kConeSliceRearRow;
    *colour = at / kRearPerColour;
    const int j = at % kRearPerColour / kConeSliceLengthCount;
    *position = j < kFrontFirst ? j : j + (kFrontLast - kFrontFirst + 1);
    *length_index = at % kConeSliceLengthCount;
    return true;
  }
  return false;
}

bool SeenInsideSlices(const int rows[kConeSlicePositions], float degrees_off_facing, float metres) {
  const int k = ConeSliceAt(degrees_off_facing);
  int colour = 0, position = 0, length = 0;
  if (!ConeSliceRowParts(rows[k], &colour, &position, &length) || position != k) return false;
  return metres <= kConeSliceLengths[length] + 1e-4f;
}

float SeeableReach(float cut, float step, float dark_reach, const std::function<bool(float)>& lit) {
  if (!(cut > 0.0f)) return 0.0f;
  if (dark_reach >= cut) return cut;
  if (!(step > 0.0f)) step = 1.0f;
  for (int i = 0;; ++i) {
    const float d = cut - step * static_cast<float>(i);
    if (d <= 0.0f || d <= dark_reach + 1e-4f) break;
    if (lit(d)) return d;  // the farthest lit point: all of the slice up to it
  }
  return dark_reach > 0.0f ? dark_reach : 0.0f;
}

int ConeSliceSignature(const int rows[kConeSlicePositions]) {
  uint32_t hash = 2166136261u;
  bool any = false;
  for (int k = 0; k < kConeSlicePositions; ++k) {
    any = any || rows[k] != 0;
    hash = (hash ^ static_cast<uint32_t>(rows[k])) * 16777619u;
  }
  if (!any) return 0;
  return static_cast<int>(hash % 0x3ffffffeu) + 1;
}

}  // namespace rules
