#include "rules/queue_strip.h"

namespace rules {

namespace {

struct QueueAction {
  int action;
  const char* name;
  const char* icon;
};

// NWScript's ACTION_* values with a name; the engine's GetAQActionIDByID also
// gives 38, which NWScript doesn't name ("Action").
const QueueAction kActions[] = {
    {0, "Move", "ir_move"},
    {1, "Pick up", "ir_pickup"},
    {2, "Drop", "ir_drop"},
    {3, "Attack", "ir_attack"},
    {4, "Cast", "ir_cast"},
    {5, "Open", "ir_open"},
    {6, "Close", "ir_close"},
    {7, "Talk", "ir_dialog"},
    {8, "Disarm trap", "ir_disarmtrap"},
    {9, "Recover trap", "ir_rec_trap"},
    {10, "Flag trap", "ir_flag_trap"},
    {11, "Examine trap", "ir_examine"},
    {12, "Set trap", "ir_trap"},
    {13, "Pick lock", "ir_picklock"},
    {14, "Lock", "ir_lock"},
    {15, "Use", "ir_use"},
    {16, "Animal Empathy", "ir_animalemp"},
    {17, "Rest", "ir_rest"},
    {18, "Taunt", "ir_taunt"},
    {19, "Use item", "ir_use"},
    {31, "Counterspell", "ir_cntrspell"},
    {33, "Heal", "ir_heal"},
    {34, "Pick pocket", "ir_pickpocket"},
    {35, "Follow", "ir_follow"},
    {36, "Wait", "ir_action"},
    {37, "Sit", "ir_sit"},
    {40, "Smite", "ir_smiteevil"},
    {41, "Ki Damage", "ir_action"},
    {43, "Wander", "ir_move"},
};

const QueueAction* Find(int action) {
  for (const QueueAction& a : kActions) {
    if (a.action == action) return &a;
  }
  return nullptr;
}

}  // namespace

const char* QueueActionName(int action) {
  const QueueAction* a = Find(action);
  return a ? a->name : "Action";
}

const char* QueueActionIcon(int action) {
  const QueueAction* a = Find(action);
  return a ? a->icon : "ir_action";
}

QueueStripCount QueueStripSlots(int queued) {
  if (queued < 0) queued = 0;
  const int shown = queued < kQueueStripSlots ? queued : kQueueStripSlots;
  return {shown, queued - shown};
}

int ItemPowerUsesLeft(int number_of_uses, int stack_size, int charges, int uses_left_today) {
  if (number_of_uses == 1) return stack_size > 0 ? stack_size : 0;  // single use
  if (number_of_uses >= 2 && number_of_uses <= 6) {                  // 5 / 4 / 3 / 2 / 1 charges per use
    const int per_use = 7 - number_of_uses;
    return charges > 0 ? charges / per_use : 0;
  }
  if (number_of_uses >= 8 && number_of_uses <= 12) return uses_left_today > 0 ? uses_left_today : 0;  // per day
  return -1;  // 0 charges per use, unlimited, unknown
}

float QueueStripWidth(int shown, int more) {
  constexpr float kFrame = 28.0f;  // the window's own padding and the group's
  return kFrame + shown * kQueueSlotWidth + (more > 0 ? kQueueMoreWidth : 0.0f);
}

}  // namespace rules
