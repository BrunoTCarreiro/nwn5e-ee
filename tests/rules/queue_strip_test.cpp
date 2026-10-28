#include "check.h"
#include "rules/queue_strip.h"

#include <string>

using namespace rules;

// The action queue strip. Breaks caught: an ACTION_* value named for another
// (Attack as Move, Open as Close), a radial icon swapped, an unknown value
// without the generic name, more than 8 slots or the "+N" off by one, the
// width not following the slots.
TEST(ActionQueueStrip) {
  CHECK_EQ(std::string(QueueActionName(kQueueMove)) == "Move", true);
  CHECK_EQ(std::string(QueueActionName(kQueueAttack)) == "Attack", true);
  CHECK_EQ(std::string(QueueActionName(5)) == "Open", true);
  CHECK_EQ(std::string(QueueActionName(6)) == "Close", true);
  CHECK_EQ(std::string(QueueActionName(13)) == "Pick lock", true);
  CHECK_EQ(std::string(QueueActionName(18)) == "Taunt", true);
  CHECK_EQ(std::string(QueueActionName(38)) == "Action", true);
  CHECK_EQ(std::string(QueueActionName(kQueueNone)) == "Action", true);
  CHECK_EQ(std::string(QueueActionIcon(kQueueAttack)) == "ir_attack", true);
  CHECK_EQ(std::string(QueueActionIcon(kQueueMove)) == "ir_move", true);
  CHECK_EQ(std::string(QueueActionIcon(7)) == "ir_dialog", true);
  CHECK_EQ(std::string(QueueActionIcon(999)) == "ir_action", true);
  CHECK_EQ(QueueStripSlots(0).shown, 0);
  CHECK_EQ(QueueStripSlots(3).shown, 3);
  CHECK_EQ(QueueStripSlots(3).more, 0);
  CHECK_EQ(QueueStripSlots(8).shown, 8);
  CHECK_EQ(QueueStripSlots(8).more, 0);
  CHECK_EQ(QueueStripSlots(11).shown, 8);
  CHECK_EQ(QueueStripSlots(11).more, 3);
  CHECK_EQ(QueueStripSlots(-2).shown, 0);
  CHECK_EQ(QueueStripWidth(2, 0) < QueueStripWidth(3, 0), true);
  CHECK_EQ(QueueStripWidth(8, 3) - QueueStripWidth(8, 0) == kQueueMoreWidth, true);
  CHECK_EQ(QueueStripWidth(1, 0) - QueueStripWidth(0, 0) == kQueueSlotWidth, true);
}
