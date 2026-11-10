// A look's flight: see anim_flight.h.

#include "data/anim_flight.h"

#include <math.h>

#include <algorithm>
#include <string>
#include <vector>

#include "data/anim_look.h"  // MirvFlightSeconds: the rebuilt homing keeps today's time

namespace data {

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kShortest = 0.05f;  // s: no segment, and no flight, shorter

FlightPoint Add(FlightPoint a, FlightPoint b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
FlightPoint Sub(FlightPoint a, FlightPoint b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
FlightPoint Scale(FlightPoint a, float k) { return {a.x * k, a.y * k, a.z * k}; }
float Length(FlightPoint a) { return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z); }
FlightPoint Up(float metres) { return {0.0f, 0.0f, metres}; }

}  // namespace

bool FlightKindFromName(const std::string& name, FlightKind* kind) {
  if (name == "homing") *kind = FlightKind::kHoming;
  else if (name == "arc") *kind = FlightKind::kArc;
  else if (name == "straight") *kind = FlightKind::kStraight;
  else if (name == "rise_dash") *kind = FlightKind::kRiseDash;
  else return false;
  return true;
}

const char* FlightKindName(FlightKind kind) {
  switch (kind) {
    case FlightKind::kArc: return "arc";
    case FlightKind::kStraight: return "straight";
    case FlightKind::kRiseDash: return "rise_dash";
    default: return "homing";
  }
}

float FlightLerp(int type, float x) {
  x = std::max(0.0f, std::min(1.0f, x));
  switch (type) {
    case 0: return x > 0.0f ? 1.0f : 0.0f;
    case 2: return x * x * (3.0f - 2.0f * x);
    case 3: return 0.5f - sinf(asinf(1.0f - 2.0f * x) / 3.0f);
    case 4: return 1.0f - cosf(x * kPi * 0.5f);
    case 5: return sinf(x * kPi * 0.5f);
    case 6: return x * x;
    case 7: return x * x * x * (x * (6.0f * x - 15.0f) + 10.0f);
    default: return x;  // 1 linear
  }
}

std::vector<FlightSegment> FlightSegments(const FlightSettings& s, FlightPoint from, FlightPoint to, int dart, int darts) {
  const FlightPoint d = Sub(to, from);
  const float len = Length(d), ground = sqrtf(d.x * d.x + d.y * d.y);
  const FlightPoint dir = ground < 0.001f ? FlightPoint{1.0f, 0.0f, 0.0f} : FlightPoint{d.x / ground, d.y / ground, 0.0f};
  // A dart's first part turned about the vertical through the launch point: neighbours `fan` degrees apart.
  const float turn = darts > 1 ? (static_cast<float>(dart) - (darts - 1) * 0.5f) * s.fan * kPi / 180.0f : 0.0f;
  const auto fanned = [&](FlightPoint p) {
    const float c = cosf(turn), n = sinf(turn);
    return FlightPoint{p.x * c - p.y * n, p.x * n + p.y * c, p.z};
  };
  const auto segment = [](float seconds, FlightPoint at, int xy, int z) {
    FlightSegment out;
    out.seconds = seconds;
    out.to = at;
    out.lerp_xy = xy;
    out.lerp_z = z;
    return out;
  };
  std::vector<FlightSegment> out;
  switch (s.kind) {
    case FlightKind::kArc: {
      const float total = std::max(kShortest, ground / std::max(1.0f, s.speed));
      const float down = total * s.fall, up = total - down;
      out.push_back(segment(up, Add(fanned(Scale(d, up / total)), Up(s.height)), 1, 5));
      out.push_back(segment(down, d, 1, 4));
      break;
    }
    case FlightKind::kStraight: {
      FlightPoint start;
      if (darts > 1) {
        start = Add(fanned(Scale(dir, 0.8f)), Up(0.3f));
        out.push_back(segment(0.15f, start, 5, 5));
      }
      const int lerp = s.speedup ? 6 : 1;
      out.push_back(segment(std::max(kShortest, Length(Sub(d, start)) / std::max(1.0f, s.speed)), d, lerp, lerp));
      break;
    }
    case FlightKind::kRiseDash: {
      const FlightPoint top = Add(fanned(Scale(dir, std::min(0.15f * ground, 1.0f))), Up(s.height));
      out.push_back(segment(std::max(kShortest, s.rise), top, 5, 5));
      out.push_back(segment(std::max(kShortest, Length(Sub(d, top)) / std::max(1.0f, s.speed)), d, 6, 6));
      break;
    }
    default: {  // homing: today's MIRV time, a lift of 5% of the way at the middle
      const float total = std::max(kShortest, MirvFlightSeconds(len));
      out.push_back(segment(total * 0.5f, Add(fanned(Scale(d, 0.5f)), Up(0.05f * len)), 1, 5));
      out.push_back(segment(total * 0.5f, d, 1, 4));
      break;
    }
  }
  return out;
}

float FlightSeconds(const std::vector<FlightSegment>& segments) {
  float total = 0.0f;
  for (const FlightSegment& s : segments) total += s.seconds;
  return total;
}

FlightPoint FlightOffset(const std::vector<FlightSegment>& segments, float t) {
  FlightPoint before;
  float start = 0.0f;
  for (size_t k = 0; k < segments.size(); ++k) {
    const FlightSegment& s = segments[k];
    if (t <= start + s.seconds || k + 1 == segments.size()) {
      const float u = s.seconds > 0.0f ? (t - start) / s.seconds : 1.0f;
      const float a = FlightLerp(s.lerp_xy, u), b = FlightLerp(s.lerp_z, u);
      return {before.x + (s.to.x - before.x) * a, before.y + (s.to.y - before.y) * a, before.z + (s.to.z - before.z) * b};
    }
    before = s.to;
    start += s.seconds;
  }
  return before;
}

float FlightStart(const FlightSettings& settings, int dart) { return static_cast<float>(std::max(0, dart)) * settings.stagger; }

}  // namespace data
