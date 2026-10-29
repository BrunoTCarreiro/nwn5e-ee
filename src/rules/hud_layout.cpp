#include "rules/hud_layout.h"

namespace rules {

namespace {

float TextWidth(const std::string& text) { return static_cast<float>(text.size()) * kHudCharWidth + 4.0f; }

int ShownPips(int total) { return total > 0 && total <= kHudMaxPips ? total : 0; }

}  // namespace

std::string HudCount(const HudPool& pool) { return std::to_string(pool.left) + "/" + std::to_string(pool.total); }

HudLayout LayoutHud(const std::vector<HudPool>& pools) {
  HudLayout layout;
  float top = kHudPad;
  float right = 0.0f;
  for (const HudPool& pool : pools) {
    HudPoolLayout p;
    p.name = {kHudPad, top, TextWidth(pool.name), kHudLine};
    const int pips = ShownPips(pool.total);
    const int rows = pips > 0 ? (pips + kHudPipsPerRow - 1) / kHudPipsPerRow : 1;
    const int across = pips < kHudPipsPerRow ? pips : kHudPipsPerRow;
    const float pips_x = p.name.Right() + kHudGap;
    const float first_row = top + (kHudLine - kHudPip) / 2.0f;  // the first row level with the text
    for (int i = 0; i < pips; ++i) {
      p.pips.push_back({pips_x + (i % kHudPipsPerRow) * kHudPipStep, first_row + (i / kHudPipsPerRow) * kHudPipStep,
                        kHudPip, kHudPip});
    }
    const float pips_right = pips > 0 ? pips_x + (across - 1) * kHudPipStep + kHudPip : p.name.Right();
    p.count = {pips_right + kHudGap, top, TextWidth(HudCount(pool)), kHudLine};
    float x = p.count.Right();
    for (const HudFlag& flag : pool.flags) {
      x += kHudFlagGap;
      p.flag_names.push_back({x, top, TextWidth(flag.name), kHudLine});
      x += TextWidth(flag.name) + 2.0f;
      p.flag_dots.push_back({x, first_row, kHudPip, kHudPip});
      x += kHudPip;
    }
    const float height = kHudLine + (rows - 1) * kHudPipStep;
    p.block = {kHudPad, top, x - kHudPad, height};
    if (x > right) right = x;
    top += height + kHudPoolGap;
    layout.pools.push_back(p);
  }
  if (!pools.empty()) {
    layout.width = right + kHudPad;
    layout.height = top - kHudPoolGap + kHudPad;
  }
  return layout;
}

std::string HudShape(const std::vector<HudPool>& pools) {
  std::string shape;
  for (const HudPool& pool : pools) {
    shape += pool.name + "#" + std::to_string(pool.total);
    for (const HudFlag& flag : pool.flags) shape += "+" + flag.name;
    shape += ";";
  }
  return shape;
}

float HudFade(float alpha, float target, float dt, float seconds) {
  if (seconds <= 0.0f) return target;
  const float step = dt / seconds;
  if (alpha < target) return alpha + step >= target ? target : alpha + step;
  if (alpha > target) return alpha - step <= target ? target : alpha - step;
  return alpha;
}

}  // namespace rules
