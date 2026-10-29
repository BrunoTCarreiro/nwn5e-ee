#pragma once

#include <string>
#include <vector>

// The action queue strip (core/action_queue.cpp, rules/queue_strip.cpp; owner, 2026-10-28: "a sort of
// action queue at the middle top of the screen going left to right from the
// next action to the most recently added to the queue. Each item would
// consist of an action and target"): what a queued action is called and shown
// with, and the strip's size.

namespace rules {

// The engine's display ids for queued actions are NWScript's ACTION_* values
// (CNWSObject::GetAQActionIDByID - what GetCurrentAction returns); 0xFFFF none.
constexpr int kQueueMove = 0;
constexpr int kQueueAttack = 3;
constexpr int kQueueCastSpell = 4;
constexpr int kQueueItemSpell = 19;
constexpr int kQueueNone = 0xFFFF;

// A queued action's name ("Move", "Open", ...) and icon (the radial menu's
// ir_* textures); "Action" / ir_action for one without a name of its own.
// Spells, item powers and special attacks are named by what they are instead.
const char* QueueActionName(int action);
const char* QueueActionIcon(int action);

// Up to 8 items; the rest as "+N".
constexpr int kQueueStripSlots = 8;
struct QueueStripCount {
  int shown;
  int more;
};
QueueStripCount QueueStripSlots(int queued);

constexpr float kQueueSlotWidth = 112.0f;
constexpr float kQueueMoreWidth = 48.0f;
constexpr float kQueueStripHeight = 124.0f;
// The client's animated tiles (core/action_queue.cpp): a tile's top (window
// pixels), its height (GUI units), how far a leaving one slides down (window
// pixels) - the resource panel keeps clear of them (rules/hud_layout.h).
constexpr float kQueueTileTop = 4.0f;
constexpr float kQueueTileHeight = 122.0f;  // the icon and three lines
constexpr float kQueueExitDrop = 36.0f;
// The strip's width for `shown` items (and the "+N" when `more` > 0).
float QueueStripWidth(int shown, int more);

// 0.55.3 (owner: "we need to make sure the whole text is readable. a Potion of
// cure wounds right now reads as \"Potion of Cur\""): a tile as wide as its
// longer line (name or target) - the NUI font's characters about 7.5 px on
// average (13 of them filled the 92 px the 108 px tile left its text), plus the
// window's and the column's padding; at least the icon's room, at most 300 px.
constexpr float kQueueCharWidth = 7.5f;
constexpr float kQueueTilePad = 26.0f;
constexpr float kQueueTileMin = 96.0f;
constexpr float kQueueTileMax = 300.0f;
constexpr float kQueueTileGap = 4.0f;
float QueueTileWidth(int name_chars, int target_chars);
// 0.57.4 (owner: "fade in place under the strip"): how far down a leaving tile
// is, `progress` (0..1) through its exit - easing out to kQueueExitDrop; none
// while the resource panel sits right under the strip.
float QueueExitDrop(float progress, bool panel_under_strip);
// The owner, the same day: "is it possible to split the text into new lines?
// Potion of / Cure Wounds?" - a name wrapped at its spaces into lines of about
// `line_chars` (a word longer stays whole: the tile widens), at most
// `max_lines` (the last takes the rest).
constexpr int kQueueLineChars = 15;
constexpr int kQueueNameLines = 2;
std::vector<std::string> WrapQueueText(const std::string& text, int line_chars, int max_lines);
// The tiles' left edges, the strip centred on a screen `gui_width` wide, the
// gap between tiles; *fit = how many fit on it from the left (at least 1 when
// there are any) - the rest go to the "+N" (room kept for it when any don't).
std::vector<float> QueueTileXs(const std::vector<float>& widths, float gui_width, int* fit);

// 0.54.2 (owner: "I can queue the same item (a potion for example) multiple
// times while not having enough, this will pollute the queue"): how many more
// times an item's Cast Spell power can be used, by its number of uses
// (IP_CONST_CASTSPELL_NUMUSES_*, the property's cost value): single use - the
// stack; N charges per use - the charges / N; N uses per day - the uses left
// today; 0 charges per use, unlimited or anything else - no limit (-1).
int ItemPowerUsesLeft(int number_of_uses, int stack_size, int charges, int uses_left_today);

}  // namespace rules
