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

// 0.54.2: how many queued uses an item's power allows. Breaks caught: a stack
// of potions counted as one, charges per use read the wrong way round (a 5-per-
// use wand with 10 charges allowing 10), per-day uses ignored, unlimited or
// unknown powers limited.
TEST(ItemPowerUsesLeft) {
  CHECK_EQ(ItemPowerUsesLeft(1, 3, 0, 0), 3);    // three potions
  CHECK_EQ(ItemPowerUsesLeft(1, 0, 0, 0), 0);
  CHECK_EQ(ItemPowerUsesLeft(6, 1, 25, 0), 25);  // a wand, 1 charge a use
  CHECK_EQ(ItemPowerUsesLeft(2, 1, 10, 0), 2);   // 5 charges a use
  CHECK_EQ(ItemPowerUsesLeft(5, 1, 7, 0), 3);    // 2 charges a use
  CHECK_EQ(ItemPowerUsesLeft(10, 1, 0, 2), 2);   // 3 uses a day, 2 left
  CHECK_EQ(ItemPowerUsesLeft(8, 1, 0, 0), 0);
  CHECK_EQ(ItemPowerUsesLeft(7, 1, 0, 0), -1);   // 0 charges a use
  CHECK_EQ(ItemPowerUsesLeft(13, 1, 0, 0), -1);  // unlimited
  CHECK_EQ(ItemPowerUsesLeft(-1, 1, 0, 0), -1);  // not a Cast Spell power
}
