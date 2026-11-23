#include "rules/particle_mask.h"

#include <math.h>

#include <algorithm>

namespace rules {

namespace {

constexpr float kTau = 6.28318531f;
constexpr float kStep = kTau / kMaskRingDirections;  // the shader's 0.19634954

// One slab of the box's frame (the shader's N5Slab): the line's part inside it narrows [t0, t1]; parallel, it is in
// the slab or never.
bool Slab(float o, float d, float h, float* t0, float* t1) {
  if (fabsf(d) < 1e-6f) return fabsf(o) <= h;
  const float u = (-h - o) / d, v = (h - o) / d;
  *t0 = fmaxf(*t0, fminf(u, v));
  *t1 = fminf(*t1, fmaxf(u, v));
  return *t0 <= *t1;
}

// The shader's N5Box: `c` the box's centre, `h` its half sizes, `a` the angle of its +X.
bool Shadowed(const float o[3], const float p[3], const float c[3], const float h[3], float a) {
  if (h[0] <= 0.0f) return false;
  const float xa[2] = {cosf(a), sinf(a)};
  const float ya[2] = {-xa[1], xa[0]};
  const float ro[2] = {o[0] - c[0], o[1] - c[1]};
  const float rp[2] = {p[0] - c[0], p[1] - c[1]};
  const float lo[3] = {ro[0] * xa[0] + ro[1] * xa[1], ro[0] * ya[0] + ro[1] * ya[1], o[2] - c[2]};
  const float lp[3] = {rp[0] * xa[0] + rp[1] * xa[1], rp[0] * ya[0] + rp[1] * ya[1], p[2] - c[2]};
  float t0 = 0.0f, t1 = 1.0f;
  return Slab(lo[0], lp[0] - lo[0], h[0], &t0, &t1) && Slab(lo[1], lp[1] - lo[1], h[1], &t0, &t1) &&
         Slab(lo[2], lp[2] - lo[2], h[2], &t0, &t1);
}

float* Component(MaskVec* v, int c) { return c == 0 ? &v->x : c == 1 ? &v->y : c == 2 ? &v->z : &v->w; }
float Component(const MaskVec& v, int c) { return c == 0 ? v.x : c == 1 ? v.y : c == 2 ? v.z : v.w; }

// The shader's N5Near: direction k's nearness.
float Nearness(const std::vector<MaskVec>& u, int k) {
  return Component(u[kMaskRingFirstSlot + k / 4], k % 4);
}

// Box number n of the 28 (the shader reads each at its fixed place).
float BoxNumber(const std::vector<MaskVec>& u, int n) { return Component(u[kMaskBoxFirstSlot + n / 4], n % 4); }

// The box's footprint corners in the area.
void Corners(const ObjectBox& box, float out[4][2]) {
  const float sx[4] = {1, 1, -1, -1}, sy[4] = {1, -1, -1, 1};
  for (int i = 0; i < 4; ++i) {
    out[i][0] = box.centre[0] + sx[i] * box.half[0] * box.ax + sy[i] * box.half[1] * box.bx;
    out[i][1] = box.centre[1] + sx[i] * box.half[0] * box.ay + sy[i] * box.half[1] * box.by;
  }
}

}  // namespace

float MaskRingAngle(int k) { return kStep * static_cast<float>(k); }

std::vector<MaskVec> PackParticleMask(const ParticleMask& mask) {
  std::vector<MaskVec> out(kParticleMaskSlots);
  if (!(mask.reach > 0.0f)) return out;
  out[0].x = mask.origin[0];
  out[0].y = mask.origin[1];
  out[0].z = mask.origin[2];
  out[0].w = mask.reach;
  for (int k = 0; k < kMaskRingDirections; ++k) {
    const float wall = mask.walls[k];
    if (wall > 0.0f && wall < mask.reach) {
      *Component(&out[kMaskRingFirstSlot + k / 4], k % 4) = 1.0f / (wall + kMaskWallMargin);
    }
  }
  const int boxes = std::min(static_cast<int>(mask.boxes.size()), kParticleMaskMaxBoxes);
  for (int j = 0; j < boxes; ++j) {
    const ObjectBox& box = mask.boxes[static_cast<size_t>(j)];
    const float numbers[kMaskBoxNumbers] = {box.centre[0], box.centre[1], box.centre[2], box.half[0],
                                            box.half[1],   box.half[2],   atan2f(box.ay, box.ax)};
    for (int i = 0; i < kMaskBoxNumbers; ++i) {
      const int n = kMaskBoxNumbers * j + i;
      *Component(&out[kMaskBoxFirstSlot + n / 4], n % 4) = numbers[i];
    }
  }
  return out;
}

bool ParticleMasked(const std::vector<MaskVec>& u, const float p[3]) {
  if (u.size() != static_cast<size_t>(kParticleMaskSlots) || !(u[0].w > 0.0f)) return false;
  const float o[3] = {u[0].x, u[0].y, u[0].z};
  const float dx = p[0] - o[0], dy = p[1] - o[1];
  const float r = sqrtf(dx * dx + dy * dy);
  if (r > u[0].w) return false;
  if (r > 0.01f) {
    float f = atan2f(dy, dx) / kStep;
    if (f < 0.0f) f += static_cast<float>(kMaskRingDirections);
    float k0 = floorf(f);
    const float t = f - k0;
    if (k0 >= static_cast<float>(kMaskRingDirections)) k0 -= static_cast<float>(kMaskRingDirections);
    const int i0 = static_cast<int>(k0);
    const int i1 = i0 + 1 >= kMaskRingDirections ? 0 : i0 + 1;
    const float n = Nearness(u, i0) + (Nearness(u, i1) - Nearness(u, i0)) * t;
    if (r * n > 1.0f) return true;
  }
  for (int j = 0; j < kParticleMaskMaxBoxes; ++j) {
    const int n = kMaskBoxNumbers * j;
    const float c[3] = {BoxNumber(u, n), BoxNumber(u, n + 1), BoxNumber(u, n + 2)};
    const float h[3] = {BoxNumber(u, n + 3), BoxNumber(u, n + 4), BoxNumber(u, n + 5)};
    if (Shadowed(o, p, c, h, BoxNumber(u, n + 6))) return true;
  }
  return false;
}

float BoxGroundDistance(const float origin[3], const ObjectBox& box) {
  const float dx = origin[0] - box.centre[0], dy = origin[1] - box.centre[1];
  const float lx = fabsf(dx * box.ax + dy * box.ay) - box.half[0];
  const float ly = fabsf(dx * box.bx + dy * box.by) - box.half[1];
  const float ex = lx > 0.0f ? lx : 0.0f, ey = ly > 0.0f ? ly : 0.0f;
  return sqrtf(ex * ex + ey * ey);
}

float BoxAngleFrom(const float origin[3], const ObjectBox& box) {
  if (BoxGroundDistance(origin, box) <= 0.0f) return 3.14159265f;
  const float middle = atan2f(box.centre[1] - origin[1], box.centre[0] - origin[0]);
  float corners[4][2];
  Corners(box, corners);
  float lo = 0.0f, hi = 0.0f;
  for (int i = 0; i < 4; ++i) {
    float a = atan2f(corners[i][1] - origin[1], corners[i][0] - origin[0]) - middle;
    while (a > 3.14159265f) a -= kTau;
    while (a < -3.14159265f) a += kTau;
    lo = fminf(lo, a);
    hi = fmaxf(hi, a);
  }
  return hi - lo;
}

std::vector<ObjectBox> PickMaskBoxes(const float origin[3], float reach, const std::vector<ObjectBox>& boxes) {
  struct Scored {
    const ObjectBox* box;
    float angle;
    float distance;
  };
  std::vector<Scored> scored;
  for (const ObjectBox& box : boxes) {
    if (PointInBox(origin, box)) continue;
    const float distance = BoxGroundDistance(origin, box);
    if (distance > reach) continue;
    scored.push_back({&box, BoxAngleFrom(origin, box), distance});
  }
  std::stable_sort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) {
    if (a.angle != b.angle) return a.angle > b.angle;
    return a.distance < b.distance;
  });
  std::vector<ObjectBox> out;
  for (const Scored& s : scored) {
    if (static_cast<int>(out.size()) == kParticleMaskMaxBoxes) break;
    out.push_back(*s.box);
  }
  return out;
}

}  // namespace rules
