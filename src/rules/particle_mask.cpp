#include "rules/particle_mask.h"

#include <math.h>

namespace rules {

namespace {

// One slab of the box's frame (the shader's N5Slab): the line's part inside it narrows [t0, t1]; parallel, it is in
// the slab or never.
bool Slab(float o, float d, float h, float* t0, float* t1) {
  if (fabsf(d) < 1e-6f) return fabsf(o) <= h;
  const float u = (-h - o) / d, v = (h - o) / d;
  *t0 = fmaxf(*t0, fminf(u, v));
  *t1 = fminf(*t1, fmaxf(u, v));
  return *t0 <= *t1;
}

// The shader's N5Shadowed: `a` the box's centre and its +X's x, `b` its half sizes and its +X's y.
bool Shadowed(const float o[3], const float p[3], const MaskVec& a, const MaskVec& b) {
  const float xa[2] = {a.w, b.w};
  const float ya[2] = {-xa[1], xa[0]};
  const float ro[2] = {o[0] - a.x, o[1] - a.y};
  const float rp[2] = {p[0] - a.x, p[1] - a.y};
  const float lo[3] = {ro[0] * xa[0] + ro[1] * xa[1], ro[0] * ya[0] + ro[1] * ya[1], o[2] - a.z};
  const float lp[3] = {rp[0] * xa[0] + rp[1] * xa[1], rp[0] * ya[0] + rp[1] * ya[1], p[2] - a.z};
  float t0 = 0.0f, t1 = 1.0f;
  return Slab(lo[0], lp[0] - lo[0], b.x, &t0, &t1) && Slab(lo[1], lp[1] - lo[1], b.y, &t0, &t1) &&
         Slab(lo[2], lp[2] - lo[2], b.z, &t0, &t1);
}

}  // namespace

std::vector<MaskVec> PackParticleMask(const float origin[3], const std::vector<ObjectBox>& boxes) {
  std::vector<MaskVec> out(1);
  out[0].x = origin[0];
  out[0].y = origin[1];
  out[0].z = origin[2];
  int count = 0;
  for (const ObjectBox& box : boxes) {
    if (count == kParticleMaskMaxBoxes) break;
    if (PointInBox(origin, box)) continue;
    MaskVec a, b;
    a.x = box.centre[0];
    a.y = box.centre[1];
    a.z = box.centre[2];
    a.w = box.ax;
    b.x = box.half[0];
    b.y = box.half[1];
    b.z = box.half[2];
    b.w = box.ay;
    out.push_back(a);
    out.push_back(b);
    ++count;
  }
  out[0].w = static_cast<float>(count);
  return out;
}

bool ParticleMasked(const std::vector<MaskVec>& uniforms, const float p[3]) {
  if (uniforms.empty() || uniforms[0].w < 0.5f) return false;
  const int count = static_cast<int>(uniforms[0].w + 0.5f);
  if (count > kParticleMaskMaxBoxes || uniforms.size() < static_cast<size_t>(1 + 2 * count)) return false;
  const float o[3] = {uniforms[0].x, uniforms[0].y, uniforms[0].z};
  for (int k = 0; k < count; ++k) {
    if (Shadowed(o, p, uniforms[1 + 2 * k], uniforms[2 + 2 * k])) return true;
  }
  return false;
}

}  // namespace rules
