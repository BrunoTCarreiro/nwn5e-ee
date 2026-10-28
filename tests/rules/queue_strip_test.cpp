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

// 0.55.3: tiles sized to their text, centred, the rest to the "+N". Breaks
// caught: "Potion of Cure Light Wounds" (27) left at the narrow width, the
// target ignored when longer, no floor or ceiling, the strip not centred,
// tiles overlapping, a screen too narrow showing them all anyway, none shown.
TEST(QueueTileWidths) {
  CHECK_EQ(QueueTileWidth(6, 3) == kQueueTileMin, true);                         // "Attack" / "Orc"
  CHECK_EQ(QueueTileWidth(27, 8) == kQueueTilePad + 27 * kQueueCharWidth, true);  // the potion
  CHECK_EQ(QueueTileWidth(5, 20) == kQueueTilePad + 20 * kQueueCharWidth, true);  // a long target
  CHECK_EQ(QueueTileWidth(200, 0) == kQueueTileMax, true);
  int fit = 0;
  const std::vector<float> xs = QueueTileXs({100.0f, 200.0f, 100.0f}, 1000.0f, &fit);
  CHECK_EQ(fit, 3);
  CHECK_EQ(xs.size(), 3u);
  CHECK_EQ(xs[0] == (1000.0f - (400.0f + 2 * kQueueTileGap)) / 2.0f, true);  // centred
  CHECK_EQ(xs[1] == xs[0] + 100.0f + kQueueTileGap, true);                  // no overlap
  CHECK_EQ(xs[2] == xs[1] + 200.0f + kQueueTileGap, true);
  const std::vector<float> narrow = QueueTileXs({300.0f, 300.0f, 300.0f}, 700.0f, &fit);
  CHECK_EQ(fit, 2);  // two fit with the "+N"'s room; the third goes to it
  CHECK_EQ(narrow.size(), 2u);
  QueueTileXs({900.0f}, 500.0f, &fit);
  CHECK_EQ(fit, 1);  // one always shows
  CHECK_EQ(QueueTileXs({}, 800.0f, &fit).empty(), true);
  CHECK_EQ(fit, 0);
}

// The owner's "Potion of / Cure Wounds". Breaks caught: a name split mid-word,
// a short name split, more lines than allowed (the rest dropped instead of
// kept), a long word cut, spaces doubled or kept at the ends.
TEST(QueueTextWraps) {
  const std::vector<std::string> potion = WrapQueueText("Potion of Cure Wounds", kQueueLineChars, kQueueNameLines);
  CHECK_EQ(potion.size(), 2u);
  CHECK_EQ(potion[0] == "Potion of", true);  // the owner's split: balanced, not "Potion of Cure / Wounds"
  CHECK_EQ(potion[1] == "Cure Wounds", true);
  const std::vector<std::string> light = WrapQueueText("Potion of Cure Light Wounds", kQueueLineChars, 2);
  CHECK_EQ(light.size(), 2u);
  CHECK_EQ(light[0] == "Potion of Cure", true);
  CHECK_EQ(light[1] == "Light Wounds", true);
  const std::vector<std::string> three = WrapQueueText("Potion of Cure Light Wounds", 10, 3);
  CHECK_EQ(three.size(), 3u);
  CHECK_EQ(three[2] == "Wounds", true);
  const std::vector<std::string> rest = WrapQueueText("a b c d e f", 1, 2);
  CHECK_EQ(rest.size(), 2u);  // never more lines than allowed, nothing dropped
  CHECK_EQ(rest[0].size() + rest[1].size() + 1 == 11u, true);
  CHECK_EQ(WrapQueueText("Attack", kQueueLineChars, 2).size(), 1u);
  CHECK_EQ(WrapQueueText("Mordenkainen's Disjunction", 10, 2)[0] == "Mordenkainen's", true);  // a word stays whole
  const std::vector<std::string> spaced = WrapQueueText("  Magic   Missile ", 15, 2);
  CHECK_EQ(spaced.size(), 1u);
  CHECK_EQ(spaced[0] == "Magic Missile", true);
  CHECK_EQ(WrapQueueText("", 15, 2).empty(), true);
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
