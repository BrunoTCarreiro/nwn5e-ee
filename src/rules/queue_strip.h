#pragma once

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
// The strip's width for `shown` items (and the "+N" when `more` > 0).
float QueueStripWidth(int shown, int more);

}  // namespace rules
