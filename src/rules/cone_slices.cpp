#include "rules/cone_slices.h"

#include <math.h>
#include <stdint.h>

namespace rules {

const float kConeSliceLengths[kConeSliceLengthCount] = {1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 30, 35, 40, 50};

namespace {
constexpr int kDrawnCones[] = {60, 90, 120, 150};  // core/stealth_watch.cpp's kConeAngles
constexpr float kPi = 3.14159265f;
}  // namespace

int DrawnConeDegrees(int degrees) {
  int best = kDrawnCones[0];
  for (int drawn : kDrawnCones) {
    const int d = drawn > degrees ? drawn - degrees : degrees - drawn;
    const int b = best > degrees ? best - degrees : degrees - best;
    if (d < b) best = drawn;
  }
  return best;
}

SliceSpan ConeSliceSpan(int drawn_degrees) {
  SliceSpan span;
  span.count = static_cast<int>(drawn_degrees / kConeSliceDegrees + 0.5f);
  if (span.count < 0) span.count = 0;
  if (span.count > kConeSlicePositions) span.count = kConeSlicePositions;
  span.first = (kConeSlicePositions - span.count) / 2;
  return span;
}

float ConeSliceMiddle(int position) {
  return -kConeSliceDegrees * kConeSlicePositions / 2.0f + kConeSliceDegrees * (static_cast<float>(position) + 0.5f);
}

int ConeSliceAt(float degrees_off_facing) {
  const float from_edge = degrees_off_facing + kConeSliceDegrees * kConeSlicePositions / 2.0f;
  if (!(from_edge >= 0.0f)) return -1;
  const int k = static_cast<int>(from_edge / kConeSliceDegrees);
  return k < kConeSlicePositions ? k : -1;
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
  int clear_to = -1;          // known clear up to this index
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
  return kConeSliceRow + colour * kConeSlicePositions * kConeSliceLengthCount + position * kConeSliceLengthCount +
         length_index;
}

bool ConeSliceRowParts(int row, int* colour, int* position, int* length_index) {
  if (row < kConeSliceRow || row > kConeSliceRowLast) return false;
  const int at = row - kConeSliceRow;
  *colour = at / (kConeSlicePositions * kConeSliceLengthCount);
  *position = at / kConeSliceLengthCount % kConeSlicePositions;
  *length_index = at % kConeSliceLengthCount;
  return true;
}

bool SeenInsideSlices(const int rows[kConeSlicePositions], float degrees_off_facing, float metres) {
  const int k = ConeSliceAt(degrees_off_facing);
  if (k < 0) return false;
  int colour = 0, position = 0, length = 0;
  if (!ConeSliceRowParts(rows[k], &colour, &position, &length) || position != k) return false;
  return metres <= kConeSliceLengths[length] + 1e-4f;
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
