#pragma once

#include <string>
#include <vector>

// The resource panel (core/resource_panel.cpp; owner, 2026-10-29: "We might
// need some sort of custom hud element to display focus points", then "go
// with the recommendation"): above the quickbar's left end, one block per
// class pool - its name, a pip per point (filled while left, hollow once
// spent; rows of 10) and "left/total" - with the class's once-per-rest
// features as dots after the count (lit while ready), and a hover tooltip.
// What the blocks hold comes from the class rules (rules_monk.cpp's Focus
// first); this file lays them out.

namespace rules {

struct HudFlag {
  std::string name;
  bool ready = false;
};

struct HudPool {
  std::string name;
  int left = 0;
  int total = 0;
  std::vector<HudFlag> flags;
  std::string tooltip;
};

constexpr int kHudPipsPerRow = 10;
constexpr int kHudMaxPips = 40;           // more points than that: the count alone
constexpr float kHudPip = 10.0f;          // a pip's diameter
constexpr float kHudPipStep = 13.0f;      // pip to pip, across and down
constexpr float kHudCharWidth = 7.5f;     // the NUI font's average character (as the queue strip's)
constexpr float kHudPad = 6.0f;           // around the blocks
constexpr float kHudLine = 18.0f;         // a text line's height (a block's first row)
constexpr float kHudGap = 8.0f;           // name -> pips -> count
constexpr float kHudFlagGap = 12.0f;      // before each flag
constexpr float kHudPoolGap = 4.0f;       // between blocks

struct HudBox {
  float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
  float Right() const { return x + w; }
  float Bottom() const { return y + h; }
};

struct HudPoolLayout {
  HudBox name;
  std::vector<HudBox> pips;  // the circles' boxes, min(total, kHudMaxPips) of them (none past it)
  HudBox count;
  std::vector<HudBox> flag_names;
  std::vector<HudBox> flag_dots;
  HudBox block;  // all of it
};

struct HudLayout {
  std::vector<HudPoolLayout> pools;
  float width = 0.0f;   // the content, kHudPad included
  float height = 0.0f;
};

HudLayout LayoutHud(const std::vector<HudPool>& pools);

// "14/18".
std::string HudCount(const HudPool& pool);

// What gives the panel its shape (names, totals, flags) - a change rebuilds it;
// points spent or a flag lit only recolour it.
std::string HudShape(const std::vector<HudPool>& pools);

// A pip's fade: from `alpha` toward `target` (0 or 1) over `seconds`, `dt` later.
float HudFade(float alpha, float target, float dt, float seconds);

// Where the panel's card goes (0.57.2). NUI places a window by window pixels and
// sizes it in GUI units times the GUI scale. The quickbar is 800 x 36 GUI units
// centred on the bottom edge (CPanelQuickBar::Reposition); the chat's Talk line
// (CGuiInGameChatPrompt, PNL_CHAT_PROMPT, always shown in game) is a band the
// window's full width from 33 to 56 GUI units over the bottom edge; the chat log
// (PNL_CHAT) is 720 GUI units wide from the left edge, its bottom on the Talk
// line - only its height can be dragged. The card's right edge on the bar's
// right edge, its bottom
// clear of the Talk line (owner: "move the box slightly up as it's just a bit
// over the Talk input box"); where that would put it over the chat window (a
// narrow game window), centred under the action queue strip instead, below
// where its leaving tiles slide (owner: "it should be repositioned and placed
// underneath the action queue panel").
struct HudScreen {
  float width = 0.0f;       // the game window, pixels (gui_width)
  float height = 0.0f;
  float scale = 1.0f;       // the GUI scale (gui_scale / 100)
  float chat_right = 0.0f;  // the chat window's right edge, window pixels
};
struct HudSpot {
  float x = 0.0f, y = 0.0f;  // the card's top-left, window pixels
  bool under_strip = false;
};
constexpr float kQuickbarWidth = 800.0f;
constexpr float kQuickbarHeight = 36.0f;
constexpr float kTalkLineHeight = 20.0f;  // the Talk line's top at 36 + 20 = 56 over the bottom edge
constexpr float kHudAbove = 4.0f;         // the card's gap over the Talk line, under the strip
constexpr float kHudClearOfChat = 8.0f;   // at least this far right of the chat window
HudSpot PlaceHud(const HudScreen& screen, float card_w, float card_h);

}  // namespace rules
