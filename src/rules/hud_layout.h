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
// narrow game window), centred under the action queue strip instead (owner:
// "it should be repositioned and placed underneath the action queue panel").
// 0.57.3 (owner: "the resource panel should be right up against the talk
// input"; "if the action queue is hidden, the resources should be up against
// the top of the window, if it's showing ... right up against it, no space
// between them"): no gaps - the card's bottom on the Talk line's top, its top
// on the strip's tiles' bottom (4 + 122 x s), or on the window's top edge
// with the strip turned off (!queue).
struct HudScreen {
  float width = 0.0f;       // the game window, pixels (gui_width)
  float height = 0.0f;
  float scale = 1.0f;       // the GUI scale (gui_scale / 100)
  float chat_right = 0.0f;  // the chat window's right edge, window pixels
  bool strip_on = true;     // the action queue strip shown (not turned off with !queue)
};
struct HudSpot {
  float x = 0.0f, y = 0.0f;  // the card's top-left, window pixels
  bool under_strip = false;
};
constexpr float kQuickbarWidth = 800.0f;
constexpr float kQuickbarHeight = 36.0f;
constexpr float kTalkLineHeight = 20.0f;  // the Talk line's top at 36 + 20 = 56 over the bottom edge
constexpr float kHudClearOfChat = 8.0f;   // at least this far right of the chat window
HudSpot PlaceHud(const HudScreen& screen, float card_w, float card_h);

// 0.57.11 (owner, 2026-10-30: the hover tooltip "doesn't show paragraphs" -
// NUI's tooltip draws its text as one line): nwn5e's own hover box, shown while
// the mouse is over the panel (core/resource_panel.cpp). Its text as lines:
// each of the tooltip's lines wrapped at word breaks to `width` (kHudCharWidth
// a character; a word longer than a line is cut), an empty line kept as the
// gap between paragraphs, none at the end.
constexpr float kHudTipWidth = 330.0f;  // the text's width, GUI units
constexpr float kHudTipPad = 8.0f;      // the box around the text
constexpr float kHudTipGap = 4.0f;      // the box to the panel
std::vector<std::string> HudTipLines(const std::string& text, float width);
// Where the box goes (window pixels, the sizes in pixels too): its right edge
// on the panel's, above it `gap` away - below it when there is no room above;
// never off the left edge.
HudBox PlaceHudTip(const HudBox& panel, float tip_w, float tip_h, float gap);

}  // namespace rules
