#include "rules/cover_objects.h"

#include <ctype.h>
#include <math.h>

#include <sstream>

namespace rules {

bool ParseBoundsLine(const std::string& line, int* row, ModelBounds* bounds) {
  size_t first = line.find_first_not_of(" \t");
  if (first == std::string::npos || line[first] == '#') return false;
  std::istringstream in(line);
  int r = -1;
  std::string model;
  ModelBounds b;
  if (!(in >> r >> model >> b.lo[0] >> b.hi[0] >> b.lo[1] >> b.hi[1] >> b.lo[2] >> b.hi[2])) return false;
  if (r < 0) return false;
  for (int k = 0; k < 3; ++k) {
    if (!(b.hi[k] >= b.lo[k])) return false;
  }
  *row = r;
  *bounds = b;
  return true;
}

bool BoundsCount(const ModelBounds& bounds) { return bounds.hi[2] - bounds.lo[2] >= kCoverMinHeight; }

ObjectBox BoxOf(const ModelBounds& bounds, float x, float y, float z, float fx, float fy, float scale) {
  ObjectBox box;
  float f = sqrtf(fx * fx + fy * fy);
  if (f < 1e-6f) {
    fx = 0.0f;
    fy = 1.0f;
    f = 1.0f;
  }
  box.bx = fx / f;
  box.by = fy / f;
  box.ax = box.by;   // the model's +X: its +Y turned a quarter clockwise
  box.ay = -box.bx;
  if (!(scale > 0.0f)) scale = 1.0f;
  float c[3], h[3];
  for (int k = 0; k < 3; ++k) {
    c[k] = (bounds.lo[k] + bounds.hi[k]) * 0.5f * scale;
    h[k] = (bounds.hi[k] - bounds.lo[k]) * 0.5f * scale;
    box.half[k] = h[k];
  }
  box.centre[0] = x + c[0] * box.ax + c[1] * box.bx;
  box.centre[1] = y + c[0] * box.ay + c[1] * box.by;
  box.centre[2] = z + c[2];
  const float ex = fabsf(box.ax) * h[0] + fabsf(box.bx) * h[1];
  const float ey = fabsf(box.ay) * h[0] + fabsf(box.by) * h[1];
  box.lo[0] = box.centre[0] - ex;
  box.hi[0] = box.centre[0] + ex;
  box.lo[1] = box.centre[1] - ey;
  box.hi[1] = box.centre[1] + ey;
  box.lo[2] = box.centre[2] - h[2];
  box.hi[2] = box.centre[2] + h[2];
  return box;
}

bool SegmentNearBox(const float a[3], const float b[3], const ObjectBox& box) {
  for (int k = 0; k < 3; ++k) {
    const float lo = a[k] < b[k] ? a[k] : b[k];
    const float hi = a[k] < b[k] ? b[k] : a[k];
    if (hi < box.lo[k] || lo > box.hi[k]) return false;
  }
  return true;
}

bool PointInBox(const float p[3], const ObjectBox& box) {
  const float dx = p[0] - box.centre[0], dy = p[1] - box.centre[1];
  return fabsf(dx * box.ax + dy * box.ay) <= box.half[0] && fabsf(dx * box.bx + dy * box.by) <= box.half[1] &&
         fabsf(p[2] - box.centre[2]) <= box.half[2];
}

bool SegmentHitsBox(const float a[3], const float b[3], const ObjectBox& box, float* t) {
  // Into the box's own frame: its +X, +Y and up.
  const float da[3] = {a[0] - box.centre[0], a[1] - box.centre[1], a[2] - box.centre[2]};
  const float db[3] = {b[0] - box.centre[0], b[1] - box.centre[1], b[2] - box.centre[2]};
  const float p[3] = {da[0] * box.ax + da[1] * box.ay, da[0] * box.bx + da[1] * box.by, da[2]};
  const float q[3] = {db[0] * box.ax + db[1] * box.ay, db[0] * box.bx + db[1] * box.by, db[2]};
  float enter = 0.0f, leave = 1.0f;
  for (int k = 0; k < 3; ++k) {
    const float d = q[k] - p[k];
    if (fabsf(d) < 1e-9f) {
      if (p[k] < -box.half[k] || p[k] > box.half[k]) return false;  // parallel, outside that slab
      continue;
    }
    float t0 = (-box.half[k] - p[k]) / d, t1 = (box.half[k] - p[k]) / d;
    if (t0 > t1) {
      const float s = t0;
      t0 = t1;
      t1 = s;
    }
    if (t0 > enter) enter = t0;
    if (t1 < leave) leave = t1;
    if (enter > leave) return false;
  }
  if (t) *t = enter;
  return true;
}

Cover CoverFromBlocked(int blocked, int points) {
  if (points <= 0) return Cover::kNone;
  if (blocked >= points) return Cover::kTotal;
  if (blocked >= (3 * points + 2) / 4) return Cover::kThreeQuarters;
  if (blocked >= (points + 1) / 2) return Cover::kHalf;
  return Cover::kNone;
}

float EyeHeight(int nwn_size) {
  switch (nwn_size) {
    case 1: return 0.3f;
    case 2: return 0.9f;
    case 4: return 2.5f;
    case 5: return 4.0f;
    default: return nwn_size > 5 ? 4.0f : 1.6f;
  }
}

float SliceTargetHeight(int hider_size) { return ScaledBodyPoint(5, hider_size, true).height; }

bool CoverHides(Cover cover) { return cover == Cover::kThreeQuarters || cover == Cover::kTotal; }

int CoverSaveBonus(Cover cover) {
  switch (cover) {
    case Cover::kHalf: return 2;
    case Cover::kThreeQuarters:
    case Cover::kTotal: return 5;
    case Cover::kNone: break;
  }
  return 0;
}

bool SpellIgnoresCover(int spell) { return spell == kSpellSacredFlame; }

bool CoverRefusesAttack(Cover cover, bool ranged, float metres) {
  return cover == Cover::kTotal && (ranged || metres <= kMeleeCoverReach);
}

}  // namespace rules
