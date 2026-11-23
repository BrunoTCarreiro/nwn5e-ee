#include "rules/los_probe.h"

#include <math.h>

namespace rules {

const LosBodyPoint kLosBody[kLosBodyPoints] = {
    {"feet", 0.0f, 0.2f},          {"waist", 0.0f, 1.0f},         {"chest", 0.0f, 1.3f},
    {"head", 0.0f, 1.7f},          {"left_shoulder", 0.3f, 1.4f}, {"right_shoulder", -0.3f, 1.4f},
};

void LosPointAt(const LosBodyPoint& point, float ox, float oy, float tx, float ty, float tz, float* x, float* y,
                float* z) {
  float dx = tx - ox, dy = ty - oy;
  const float d = sqrtf(dx * dx + dy * dy);
  if (d > 1e-4f) {
    dx /= d;
    dy /= d;
  } else {
    dx = 1.0f;
    dy = 0.0f;
  }
  // The observer's left of its line to the target: the direction turned a quarter anticlockwise.
  *x = tx - dy * point.side;
  *y = ty + dx * point.side;
  *z = tz + point.height;
}

namespace {
std::string Fill(std::string text, const std::string& name, const std::string& value) {
  const std::string mark = "{" + name + "}";
  for (size_t at = text.find(mark); at != std::string::npos; at = text.find(mark, at + value.size())) {
    text.replace(at, mark.size(), value);
  }
  return text;
}
}  // namespace

std::string LosSummary(const std::vector<LosPointResult>& points, const std::string& clear_words,
                       const std::string& blocked_words) {
  std::vector<bool> done(points.size(), false);
  std::string out;
  for (size_t i = 0; i < points.size(); ++i) {
    if (done[i]) continue;
    std::string names;
    for (size_t j = i; j < points.size(); ++j) {
      if (done[j] || points[j].clear != points[i].clear || points[j].blocker != points[i].blocker) continue;
      done[j] = true;
      names += (names.empty() ? "" : ", ") + points[j].name;
    }
    const std::string part = points[i].clear ? Fill(clear_words, "points", names)
                                             : Fill(Fill(blocked_words, "points", names), "blocker", points[i].blocker);
    out += (out.empty() ? "" : "; ") + part;
  }
  return out;
}

}  // namespace rules
