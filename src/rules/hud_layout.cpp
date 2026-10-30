#include "rules/hud_layout.h"

#include "rules/queue_strip.h"

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

std::vector<std::string> HudTipLines(const std::string& text, float width) {
  const float chars = width / kHudCharWidth;
  const size_t limit = chars >= 1.0f ? static_cast<size_t>(chars) : 1;
  std::vector<std::string> lines;
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string rest = text.substr(start, end - start);
    if (rest.empty()) lines.push_back("");
    while (!rest.empty()) {
      if (rest.size() <= limit) {
        lines.push_back(rest);
        break;
      }
      const size_t cut = rest.rfind(' ', limit);
      if (cut == std::string::npos || cut == 0) {  // one word longer than a line: cut it
        lines.push_back(rest.substr(0, limit));
        rest = rest.substr(limit);
      } else {
        lines.push_back(rest.substr(0, cut));
        rest = rest.substr(cut + 1);
      }
    }
    if (end == text.size()) break;
    start = end + 1;
  }
  while (!lines.empty() && lines.back().empty()) lines.pop_back();
  return lines;
}

HudBox PlaceHudTip(const HudBox& panel, float tip_w, float tip_h, float gap) {
  HudBox box;
  box.w = tip_w;
  box.h = tip_h;
  box.x = panel.Right() - tip_w;
  box.y = panel.y - gap - tip_h;
  if (box.y < 0.0f) box.y = panel.Bottom() + gap;
  if (box.x < 0.0f) box.x = 0.0f;
  return box;
}

HudSpot PlaceHud(const HudScreen& screen, float card_w, float card_h) {
  const float s = screen.scale > 0.0f ? screen.scale : 1.0f;
  HudSpot spot;
  const float bar_right = screen.width / 2.0f + kQuickbarWidth / 2.0f * s;
  spot.x = bar_right - card_w * s;
  spot.y = screen.height - (kQuickbarHeight + kTalkLineHeight + card_h) * s;
  if (spot.x < screen.chat_right + kHudClearOfChat * s) {
    spot.under_strip = true;
    spot.x = (screen.width - card_w * s) / 2.0f;
    spot.y = screen.strip_on ? kQueueTileTop + kQueueTileHeight * s : 0.0f;
  }
  if (spot.x < 0.0f) spot.x = 0.0f;
  return spot;
}

float HudFade(float alpha, float target, float dt, float seconds) {
  if (seconds <= 0.0f) return target;
  const float step = dt / seconds;
  if (alpha < target) return alpha + step >= target ? target : alpha + step;
  if (alpha > target) return alpha - step <= target ? target : alpha - step;
  return alpha;
}

}  // namespace rules
