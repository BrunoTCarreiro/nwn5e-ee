#include "check.h"
#include "rules/class_features.h"
#include "rules/quickbar.h"

using namespace rules;

namespace {
std::vector<int> Values(const std::vector<QuickbarAction>& actions) {
  std::vector<int> values;
  for (const QuickbarAction& a : actions) values.push_back(a.value);
  return values;
}
std::vector<QuickbarSlot> EmptyBar() { return std::vector<QuickbarSlot>(kQuickbarSlots); }
QuickbarSlot Feat(int value) { return {kQuickbarTypeFeat, value}; }
constexpr int kForceful = 16000 * 65536 + 1311;
constexpr int kHamstring = 16001 * 65536 + 1311;
constexpr int kStaggering = 16002 * 65536 + 1311;
constexpr int kSundering = 16003 * 65536 + 1311;
constexpr int kBrutalOff = 16004 * 65536 + 1311;
}  // namespace

// A sub-radial option's button number is its spells.2da FeatID (option * 65536
// + feat): spells.barbarian.txt's Forceful Blow row says 1048577311. Breaks
// caught: the feat alone (every option the same button), option and feat swapped.
TEST(QuickbarOptionValueIsTheFeatId) {
  CHECK_EQ(QuickbarFeatValue(1311, 16000), 1048577311);
  CHECK_EQ(QuickbarFeatValue(1311, 16004), 1048839455);
  CHECK_EQ(kForceful, 1048577311);
}

// A Berserker 17 with everything: placement order and the options by level.
// Breaks caught: Staggering / Sundering before 13, Off missing, the Shoves
// before the class actions, an action the character doesn't have.
TEST(QuickbarActionsByLevel) {
  QuickbarFacts facts;
  facts.feats = {kFeatKnockdownShove, kFeatShovePush, kFeatRage, kFeatRecklessAttack, kFeatBrutalStrike,
                 kFeatIntimidatingPresence};
  facts.barbarian_level = 17;
  std::vector<int> want = {kFeatRage,  kFeatRecklessAttack, kForceful, kHamstring, kStaggering, kSundering,
                           kBrutalOff, kFeatIntimidatingPresence, kFeatKnockdownShove, kFeatShovePush};
  CHECK_EQ(Values(QuickbarActions2024(facts)) == want, true);

  facts.feats = {kFeatRage, kFeatRecklessAttack, kFeatBrutalStrike};
  facts.barbarian_level = 9;
  want = {kFeatRage, kFeatRecklessAttack, kForceful, kHamstring, kBrutalOff};
  CHECK_EQ(Values(QuickbarActions2024(facts)) == want, true);

  facts.feats = {kFeatRage};
  facts.barbarian_level = 1;
  want = {kFeatRage};
  CHECK_EQ(Values(QuickbarActions2024(facts)) == want, true);
}

// Second Wind is one action whatever its uses: the feat held, the highest.
// Breaks caught: two Second Wind buttons, the lower feat kept.
TEST(QuickbarSecondWindIsOneAction) {
  QuickbarFacts facts;
  facts.feats = {kFeatSecondWind3};
  CHECK_EQ(Values(QuickbarActions2024(facts)) == std::vector<int>{kFeatSecondWind3}, true);
  facts.feats = {kFeatSecondWind2, kFeatSecondWind4};
  CHECK_EQ(Values(QuickbarActions2024(facts)) == std::vector<int>{kFeatSecondWind4}, true);
  CHECK_EQ(SameQuickbarAction(kFeatSecondWind2, kFeatSecondWind4), true);
  CHECK_EQ(SameQuickbarAction(kFeatRage, kFeatRecklessAttack), false);
  CHECK_EQ(SameQuickbarAction(kForceful, kFeatBrutalStrike), false);
}

// New actions go in the first empty slots, keys 1-9 first, and are recorded;
// nothing is overwritten. Breaks caught: an occupied slot written, a non-feat
// button counted as empty, the record not kept.
TEST(QuickbarFillsFirstEmptySlots) {
  std::vector<QuickbarSlot> bar = EmptyBar();
  bar[0] = {1, 0};          // an item
  bar[1] = Feat(kFeatRage);  // already there
  QuickbarFacts facts;
  facts.feats = {kFeatRage, kFeatRecklessAttack, kFeatKnockdownShove};
  int placed = -1;
  std::vector<QuickbarChange> changes = PlanQuickbar(bar, 0, QuickbarActions2024(facts), &placed);
  CHECK_EQ(static_cast<int>(changes.size()), 2);
  if (changes.size() != 2) return;
  CHECK_EQ(changes[0].slot, 2);
  CHECK_EQ(changes[0].value, kFeatRecklessAttack);
  CHECK_EQ(changes[1].slot, 3);
  CHECK_EQ(changes[1].value, kFeatKnockdownShove);
  CHECK_EQ(changes[1].refresh, false);
  CHECK_EQ(placed, (1 << 1) | (1 << 2) | (1 << 9));  // Rage recorded (already on the bar), not re-placed
}

// An action placed before isn't placed again - the player took it off. Breaks
// caught: the record ignored (the button keeps coming back).
TEST(QuickbarRespectsRemovedActions) {
  QuickbarFacts facts;
  facts.feats = {kFeatRage};
  int placed = 0;
  CHECK_EQ(static_cast<int>(PlanQuickbar(EmptyBar(), 1 << 1, QuickbarActions2024(facts), &placed).size()), 0);
  CHECK_EQ(placed, 1 << 1);
}

// A full quickbar: the action waits, unrecorded, for a free slot. Breaks
// caught: recorded anyway (never placed), a slot overwritten.
TEST(QuickbarFullBarWaits) {
  std::vector<QuickbarSlot> bar(kQuickbarSlots, QuickbarSlot{1, 0});
  QuickbarFacts facts;
  facts.feats = {kFeatRage};
  int placed = 0;
  CHECK_EQ(static_cast<int>(PlanQuickbar(bar, 0, QuickbarActions2024(facts), &placed).size()), 0);
  CHECK_EQ(placed, 0);
  bar[30] = QuickbarSlot{};
  std::vector<QuickbarChange> changes = PlanQuickbar(bar, 0, QuickbarActions2024(facts), &placed);
  CHECK_EQ(static_cast<int>(changes.size()), 1);
  if (changes.size() != 1) return;
  CHECK_EQ(changes[0].slot, 30);
}

// Fighter 4: the Second Wind button made at 1 follows the feat, in place, even
// though it was placed (recorded) long ago. Breaks caught: a second button, a
// dead old one left.
TEST(QuickbarSecondWindButtonFollowsTheFeat) {
  std::vector<QuickbarSlot> bar = EmptyBar();
  bar[4] = Feat(kFeatSecondWind2);
  QuickbarFacts facts;
  facts.feats = {kFeatSecondWind3};
  int placed = 0;
  std::vector<QuickbarChange> changes = PlanQuickbar(bar, 1 << 0, QuickbarActions2024(facts), &placed);
  CHECK_EQ(static_cast<int>(changes.size()), 1);
  if (changes.size() != 1) return;
  CHECK_EQ(changes[0].slot, 4);
  CHECK_EQ(changes[0].value, kFeatSecondWind3);
  CHECK_EQ(changes[0].refresh, true);
}

// The labels on the slots (0.48.2, owner: "F10,F11 and F12's labels only show
// up as \"F1\"" - the box fits about three characters, so no row prefix).
// Breaks caught: a prefix back, the number past 9, the Ctrl row numbered.
TEST(QuickbarSlotLabelsFitTheBox) {
  CHECK_EQ(QuickbarSlotLabel(0, true) == "1", true);
  CHECK_EQ(QuickbarSlotLabel(8, true) == "9", true);
  CHECK_EQ(QuickbarSlotLabel(9, true) == "F10", true);
  CHECK_EQ(QuickbarSlotLabel(11, true) == "F12", true);
  CHECK_EQ(QuickbarSlotLabel(12, true) == "1", true);
  CHECK_EQ(QuickbarSlotLabel(21, true) == "F10", true);
  CHECK_EQ(QuickbarSlotLabel(24, true) == "F1", true);
  CHECK_EQ(QuickbarSlotLabel(35, true) == "F12", true);
  CHECK_EQ(QuickbarSlotLabel(0, false) == "F1", true);
  CHECK_EQ(QuickbarSlotLabel(12, false) == "F1", true);
  for (int slot = 0; slot < kQuickbarSlots; ++slot) CHECK_EQ(QuickbarSlotLabel(slot, true).size() <= 3, true);
}

// The keys named in the chat line. Breaks caught: the banks off by one, the
// number key named past 9.
TEST(QuickbarSlotKeysNamed) {
  CHECK_EQ(QuickbarSlotKeys(0, true) == "1 / F1", true);
  CHECK_EQ(QuickbarSlotKeys(8, true) == "9 / F9", true);
  CHECK_EQ(QuickbarSlotKeys(8, false) == "F9", true);
  CHECK_EQ(QuickbarSlotKeys(9, true) == "F10", true);
  CHECK_EQ(QuickbarSlotKeys(12, true) == "Shift+1 / Shift+F1", true);
  CHECK_EQ(QuickbarSlotKeys(12, false) == "Shift+F1", true);
  CHECK_EQ(QuickbarSlotKeys(21, true) == "Shift+F10", true);
  CHECK_EQ(QuickbarSlotKeys(24, true) == "Ctrl+F1", true);
  CHECK_EQ(QuickbarSlotKeys(35, true) == "Ctrl+F12", true);
}
