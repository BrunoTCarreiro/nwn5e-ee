#include "data/vfx_area.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace data {

namespace {

constexpr const char* kBlank = "****";

float Clamp(float v, float lo, float hi) { return std::isfinite(v) ? std::max(lo, std::min(hi, v)) : lo; }

std::string Num(float v) {
  char text[32];
  std::snprintf(text, sizeof(text), "%g", std::round(v * 10000.0f) / 10000.0f);
  return text;
}

std::string OrBlank(const std::string& s) { return s.empty() ? kBlank : s; }

}  // namespace

std::vector<std::pair<std::string, std::string>> VfxAreaRowColumns(const VfxArea& area) {
  std::vector<std::pair<std::string, std::string>> out;
  const bool rect = area.shape == 'R';
  out.emplace_back("SHAPE", rect ? "R" : "C");
  out.emplace_back("RADIUS", rect ? kBlank : Num(Clamp(area.radius, kVfxAreaMinRadius, kVfxAreaMaxSize)));
  out.emplace_back("WIDTH", rect ? Num(Clamp(area.width, kVfxAreaMinSide, kVfxAreaMaxSize)) : kBlank);
  out.emplace_back("LENGTH", rect ? Num(Clamp(area.length, kVfxAreaMinSide, kVfxAreaMaxSize)) : kBlank);
  out.emplace_back("OrientWithGround", area.orient_ground ? "1" : "0");
  out.emplace_back("DurationVFX", OrBlank(area.duration_effect));
  for (int i = 0; i < 3; ++i) {
    const VfxAreaSlot& s = area.slots[i];
    const std::string k = "0" + std::to_string(i + 1);
    const bool on = !s.model.empty();
    out.emplace_back("MODEL" + k, on ? s.model : kBlank);
    out.emplace_back("NUMACT" + k, on ? std::to_string(std::max(1, std::min(kVfxAreaMaxCount, s.count))) : kBlank);
    out.emplace_back("DURATION" + k, on ? std::to_string(static_cast<int>(std::lround(Clamp(s.lifetime, 0.0f, kVfxAreaMaxLifetime) * 1000.0f))) : kBlank);
    out.emplace_back("EDGEWGHT" + k, on ? Num(Clamp(s.edge, 0.0f, 1.0f)) : kBlank);
    out.emplace_back("MODELMIN" + k, on ? OrBlank(s.low_model) : kBlank);
  }
  out.emplace_back("SoundImpact", OrBlank(area.sound_impact));
  out.emplace_back("SoundDuration", OrBlank(area.sound_duration));
  out.emplace_back("SoundCessation", OrBlank(area.sound_cessation));
  out.emplace_back("SoundOneShot", OrBlank(area.sound_oneshot));
  out.emplace_back("SoundOneShotPercentage", area.sound_oneshot.empty() ? kBlank : Num(Clamp(area.oneshot_chance, 0.0f, 1.0f)));
  return out;
}

}  // namespace data
