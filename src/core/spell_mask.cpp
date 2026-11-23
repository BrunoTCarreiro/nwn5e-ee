// The spells' particle masks (#184): spell_mask.h. The mask's shape and its uniforms' layout are rules/particle_mask.h's;
// this file measures it in the area and sends it.

#include "spell_mask.h"

#include <math.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "ee_layout.h"
#include "hooks.h"

namespace {

struct Vec3 {
  float x, y, z;
};
using ClearLineOfSightFn = int (*)(void* area, Vec3 from, Vec3 to, Vec3* hit, uint32_t* hit_object, uint32_t from_id,
                                   uint32_t to_id, int flag);
using ComputeHeightFn = float (*)(void* area, Vec3 at);

constexpr float kRingLowest = 0.5f;    // the ring's height over the ground under the origin, held between these
constexpr float kRingHighest = 2.0f;
constexpr float kWallPrecision = 0.1f;  // metres: a wall's distance found to this
constexpr float kWallNearest = 0.05f;   // a wall right at the origin (0 would read as none)

ClearLineOfSightFn ClearLine() {
  static const auto fn = reinterpret_cast<ClearLineOfSightFn>(EngineExport(ee::kClearLineOfSight));
  return fn;
}

ComputeHeightFn GroundHeight() {
  static const auto fn = reinterpret_cast<ComputeHeightFn>(EngineExport(ee::kComputeHeight));
  return fn;
}

}  // namespace

bool MeasureMaskWalls(void* area, uint32_t self, const float origin[3], float reach,
                      float walls[rules::kMaskRingDirections], int* lines) {
  for (int k = 0; k < rules::kMaskRingDirections; ++k) walls[k] = 0.0f;
  int counted = 0;
  if (!lines) lines = &counted;
  const ClearLineOfSightFn clear_line = ClearLine();
  const ComputeHeightFn ground_at = GroundHeight();
  if (!area || !clear_line || !ground_at || !(reach > 0.0f)) return false;
  float ground = ground_at(area, Vec3{origin[0], origin[1], origin[2]});
  if (ground <= ee::kAreaOffGridHeight + 0.01f) ground = origin[2] - 1.0f;
  float h = origin[2] - ground;
  h = h < kRingLowest ? kRingLowest : h > kRingHighest ? kRingHighest : h;
  const Vec3 from{origin[0], origin[1], ground + h};
  for (int k = 0; k < rules::kMaskRingDirections; ++k) {
    const float a = rules::MaskRingAngle(k);
    const float cx = cosf(a), cy = sinf(a);
    // Clear to `d` metres out (its end over the ground there, as high as the ring; off the area's tiles: blocked).
    const auto clear = [&](float d, Vec3* hit) {
      ++*lines;
      Vec3 to{origin[0] + cx * d, origin[1] + cy * d, from.z};
      const float g = ground_at(area, to);
      if (g <= ee::kAreaOffGridHeight + 0.01f) {
        if (hit) *hit = to;
        return false;
      }
      to.z = g + h;
      Vec3 at = to;
      uint32_t object = ee::kObjectInvalid;
      const bool open = clear_line(area, from, to, &at, &object, self, ee::kObjectInvalid, 1) == 1;
      if (hit) *hit = at;
      return open;
    };
    Vec3 hit{0.0f, 0.0f, 0.0f};
    if (clear(reach, &hit)) continue;
    float lo = 0.0f, hi = reach;
    // Where the engine found it blocked, when that is on the line short of its end: tried either side first.
    const float along = (hit.x - origin[0]) * cx + (hit.y - origin[1]) * cy;
    const float across = fabsf((hit.y - origin[1]) * cx - (hit.x - origin[0]) * cy);
    if (along > kWallPrecision && along < reach - kWallPrecision && across < 1.0f) {
      if (clear(along - kWallPrecision, nullptr)) {
        lo = along - kWallPrecision;
        if (!clear(along + kWallPrecision, nullptr)) {
          hi = along + kWallPrecision;
        } else {
          lo = along + kWallPrecision;
        }
      } else {
        hi = along - kWallPrecision;
      }
    }
    while (hi - lo > kWallPrecision) {
      const float mid = (lo + hi) / 2.0f;
      if (clear(mid, nullptr)) {
        lo = mid;
      } else {
        hi = mid;
      }
    }
    walls[k] = lo > kWallNearest ? lo : kWallNearest;
  }
  return true;
}

std::string MaskUniformScript(const std::string& player, const std::vector<rules::MaskVec>& uniforms) {
  std::string code;
  for (size_t i = 0; i < uniforms.size(); ++i) {
    char line[240];
    sprintf_s(line, "SetShaderUniformVec(%s, SHADER_UNIFORM_%d, %.4f, %.4f, %.4f, %.4f);", player.c_str(),
              static_cast<int>(i) + 1, uniforms[i].x, uniforms[i].y, uniforms[i].z, uniforms[i].w);
    code += line;
  }
  return code;
}

std::string MaskWords(const rules::ParticleMask& mask) {
  int walled = 0;
  float nearest = 0.0f;
  for (int k = 0; k < rules::kMaskRingDirections; ++k) {
    const float w = mask.walls[k];
    if (!(w > 0.0f) || w >= mask.reach) continue;
    ++walled;
    if (nearest == 0.0f || w < nearest) nearest = w;
  }
  char nearest_text[48] = "";
  if (walled) sprintf_s(nearest_text, " (the nearest %.1f m)", nearest);
  const size_t boxes = mask.boxes.size() > static_cast<size_t>(rules::kParticleMaskMaxBoxes)
                           ? static_cast<size_t>(rules::kParticleMaskMaxBoxes)
                           : mask.boxes.size();
  char text[320];
  sprintf_s(text, "origin (%.1f, %.1f, %.1f), reach %.1f m; walls in %d of %d directions%s; %zu box(es)", mask.origin[0],
            mask.origin[1], mask.origin[2], mask.reach, walled, rules::kMaskRingDirections, nearest_text, boxes);
  std::string out = text;
  for (size_t i = 0; i < mask.boxes.size() && i < static_cast<size_t>(rules::kParticleMaskMaxBoxes); ++i) {
    const rules::ObjectBox& b = mask.boxes[i];
    char box[120];
    sprintf_s(box, "%s(%.1f, %.1f) %.1f x %.1f x %.1f m", i ? ", " : " - ", b.centre[0], b.centre[1], 2 * b.half[0],
              2 * b.half[1], 2 * b.half[2]);
    out += box;
  }
  return out;
}
