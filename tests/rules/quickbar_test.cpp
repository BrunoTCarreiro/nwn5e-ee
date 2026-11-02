#include "check.h"
#include "rules/class_features.h"
#include "rules/quickbar.h"

using namespace rules;

// Breaks caught (0.57.13): a lost class's button left on the bar, the current
// class's (Second Wind under another of its three feats) or everyone's Shove
// taken off, an item or a feat nwn5e doesn't place touched.
TEST(LostQuickbarActionsAfterAClassChange) {
  QuickbarFacts fighter;
  fighter.feats = {kFeatSecondWind2, kFeatKnockdownShove};
  const std::vector<QuickbarAction> now = QuickbarActions2024(fighter);
  std::vector<QuickbarSlot> bar(kQuickbarSlots);
  bar[0] = {kQuickbarTypeFeat, kFeatPatientDefense};  // the Monk's: lost
  bar[1] = {kQuickbarTypeFeat, kFeatSecondWind4};     // the Fighter's, another of its feats: kept
  bar[2] = {1, kFeatRage};                            // not a feat button (an item's type): left
  bar[3] = {kQuickbarTypeFeat, 9999};                 // a feat nwn5e doesn't place: left
  bar[4] = {kQuickbarTypeFeat, kFeatRage};            // the Barbarian's: lost
  bar[5] = {kQuickbarTypeFeat, kFeatKnockdownShove};  // everyone's Shove, held: kept
  bar[6] = {kQuickbarTypeFeat, QuickbarFeatValue(kFeatBrutalStrike, 16000)};  // Forceful Blow's: lost
  const std::vector<int> lost = LostQuickbarActions(bar, now);
  CHECK_EQ(lost.size(), 3u);
  if (lost.size() == 3u) {
    CHECK_EQ(lost[0], 0);
    CHECK_EQ(lost[1], 4);
    CHECK_EQ(lost[2], 6);
  }
  CHECK_EQ(LostQuickbarActions(std::vector<QuickbarSlot>(kQuickbarSlots), now).empty(), true);
}

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

// Rogue R6 (0.50.0): Cunning Strike's options from its feat (Rogue 5), Devious
// Strikes' from its own (14) - the spells.rogue.txt FeatIDs (Poison's row says
// 1048904995, Obscure's 1049298212). Breaks caught: a Devious option without
// its feat, an option's number not the row's, a record bit reused.
TEST(QuickbarCunningStrikeOptions) {
  QuickbarFacts facts;
  facts.feats = {kFeatCunningStrike};
  std::vector<int> want = {1048904995, QuickbarFeatValue(1315, 16006), QuickbarFeatValue(1315, 16007),
                           QuickbarFeatValue(1315, 16008), QuickbarFeatValue(1315, 16012)};
  CHECK_EQ(Values(QuickbarActions2024(facts)) == want, true);
  facts.feats = {kFeatCunningStrike, kFeatDeviousStrikes, kFeatGrapple};
  want.push_back(QuickbarFeatValue(1316, 16009));
  want.push_back(QuickbarFeatValue(1316, 16010));
  want.push_back(1049298212);
  want.push_back(kFeatGrapple);
  const std::vector<QuickbarAction> actions = QuickbarActions2024(facts);
  CHECK_EQ(Values(actions) == want, true);
  int bits = 0;
  for (const QuickbarAction& a : actions) {
    CHECK_EQ((bits >> a.record_bit) & 1, 0);  // each action its own bit
    bits |= 1 << a.record_bit;
  }
  CHECK_EQ(actions.size() == 9 && actions[0].record_bit == 12 && actions[7].record_bit == 18, true);
}

// Monk part 3 (0.56.0): Flurry of Blows (2), Patient Defense, Step of the Wind
// (2), Stunning Strike (5), Superior Defense (18) when held - their own bits,
// clear of the Rogue's (12-19). Breaks caught: one missing from the feats the
// server asks about, a bit reused, a Monk action without its feat.
TEST(QuickbarMonkActions) {
  for (int feat : {kFeatFlurryOfBlows, kFeatPatientDefense, kFeatStepOfTheWind, kFeatStunningFist,
                   kFeatSuperiorDefense}) {
    bool asked = false;
    for (int f : QuickbarActionFeats2024()) asked = asked || f == feat;
    CHECK_EQ(asked, true);
  }
  QuickbarFacts facts;
  facts.feats = {kFeatPatientDefense, kFeatStepOfTheWind};  // Monk 2-4
  std::vector<int> want = {kFeatPatientDefense, kFeatStepOfTheWind};
  CHECK_EQ(Values(QuickbarActions2024(facts)) == want, true);
  facts.feats = {kFeatFlurryOfBlows, kFeatPatientDefense, kFeatStepOfTheWind, kFeatStunningFist,
                 kFeatSuperiorDefense, kFeatCunningStrike, kFeatDeviousStrikes, kFeatRage};
  int bits = 0;
  const std::vector<QuickbarAction> actions = QuickbarActions2024(facts);
  for (const QuickbarAction& a : actions) {
    CHECK_EQ((bits >> a.record_bit) & 1, 0);
    bits |= 1 << a.record_bit;
  }
  CHECK_EQ(actions.size(), 14u);  // Rage, the Rogue's 8, the Monk's 5
  int monk_bits = 0;
  for (const QuickbarAction& a : actions) {
    if (a.record_bit >= 20) monk_bits |= 1 << a.record_bit;
  }
  CHECK_EQ(monk_bits, 0x1F << 20);
  // Part 4 (0.60.0): Wholeness of Body and Quivering Palm, bits 25 and 26 (the record's int: below 31).
  for (int feat : {kFeatWholenessOfBody, kFeatQuiveringPalm}) {
    bool asked = false;
    for (int f : QuickbarActionFeats2024()) asked = asked || f == feat;
    CHECK_EQ(asked, true);
  }
  facts.feats = {kFeatWholenessOfBody, kFeatQuiveringPalm};
  const std::vector<QuickbarAction> open_hand = QuickbarActions2024(facts);
  CHECK_EQ(open_hand.size(), 2u);
  CHECK_EQ(open_hand[0].value, kFeatWholenessOfBody);
  CHECK_EQ(open_hand[0].record_bit, 25);
  CHECK_EQ(open_hand[1].value, kFeatQuiveringPalm);
  CHECK_EQ(open_hand[1].record_bit, 26);
}

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
  facts.feats = {kFeatGrapple, kFeatKnockdownShove, kFeatShovePush, kFeatRage, kFeatRecklessAttack,
                 kFeatBrutalStrike, kFeatIntimidatingPresence};
  facts.barbarian_level = 17;
  std::vector<int> want = {kFeatRage,  kFeatRecklessAttack, kForceful, kHamstring, kStaggering, kSundering,
                           kBrutalOff, kFeatIntimidatingPresence, kFeatKnockdownShove, kFeatShovePush, kFeatGrapple};
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

// #102: a !test rebuild's buttons back in their slots. Breaks caught: a slot
// filled since overwritten, a feat the character no longer holds put back, an
// action already on the bar elsewhere doubled, a non-feat button put back, a
// sub-radial option judged by its FeatID instead of its feat.
TEST(QuickbarRebuildPutsButtonsBack) {
  CHECK_EQ(QuickbarButtonFeat(kFeatRage), kFeatRage);
  CHECK_EQ(QuickbarButtonFeat(kForceful), kFeatBrutalStrike);
  std::vector<QuickbarSlot> before(kQuickbarSlots), now(kQuickbarSlots);
  before[2] = QuickbarSlot{kQuickbarTypeFeat, kFeatRecklessAttack};
  before[5] = QuickbarSlot{kQuickbarTypeFeat, kForceful};
  before[11] = QuickbarSlot{kQuickbarTypeFeat, kFeatRage};
  before[12] = QuickbarSlot{kQuickbarTypeFeat, kFeatSecondWind2};   // a class the rebuild took away
  before[13] = QuickbarSlot{1, kFeatBrutalStrike};                    // an item (its number a held feat's)
  before[14] = QuickbarSlot{kQuickbarTypeFeat, kFeatKnockdownShove};  // on the bar elsewhere now
  before[15] = QuickbarSlot{kQuickbarTypeFeat, kHamstring};           // its slot filled since
  now[2] = QuickbarSlot{kQuickbarTypeFeat, kFeatRecklessAttack};      // kept by the rebuild
  now[20] = QuickbarSlot{kQuickbarTypeFeat, kFeatKnockdownShove};
  now[15] = QuickbarSlot{kQuickbarTypeFeat, kFeatRecklessAttack};
  const std::vector<int> held = {kFeatRecklessAttack, kFeatBrutalStrike, kFeatRage, kFeatKnockdownShove};
  const std::vector<QuickbarChange> back = RebuildRestoredButtons(before, now, held);
  std::vector<int> slots;
  for (const QuickbarChange& c : back) slots.push_back(c.slot);
  CHECK_EQ(slots == std::vector<int>({5, 11}), true);
  CHECK_EQ(back.size() == 2u && back[0].value == kForceful && back[1].value == kFeatRage, true);
  CHECK_EQ(RebuildRestoredButtons(before, now, {}).empty(), true);
  CHECK_EQ(RebuildRestoredButtons(before, before, held).empty(), true);  // nothing emptied
}

// #102 (0.59.5; the owner's log: Rage placed again at slots 2, 12 and 13 over
// three Barbarian rebuilds): NWN's Rage is 293 and its successors 326-331, the
// engine moving a Rage button along the chain. Breaks caught: a successor's
// button not counted as Rage (a second Rage placed), the engine's button
// rewritten, a Rage button a rebuild took off not put back, or put back as a
// successor the character may not hold.
TEST(QuickbarRageChainIsOneAction) {
  CHECK_EQ(SameQuickbarAction(kFeatRage, 330), true);
  CHECK_EQ(SameQuickbarAction(326, 331), true);
  CHECK_EQ(SameQuickbarAction(kFeatRage, kFeatRecklessAttack), false);
  CHECK_EQ(QuickbarButtonFeat(328), kFeatRage);
  // The engine's Rage 5 (328) on the bar: Rage is there - nothing placed, nothing rewritten.
  QuickbarFacts facts;
  facts.feats = {kFeatRage};
  facts.barbarian_level = 14;
  std::vector<QuickbarSlot> bar(kQuickbarSlots);
  bar[2] = QuickbarSlot{kQuickbarTypeFeat, 328};
  int placed = 0;
  bool rage_written = false;
  for (const QuickbarChange& c : PlanQuickbar(bar, 0, QuickbarActions2024(facts), &placed)) {
    if (IsRageFeat(c.value)) rage_written = true;
  }
  CHECK_EQ(rage_written, false);
  // A rebuild took Rage 6 (330) off slot 4: it goes back as Rage (293).
  std::vector<QuickbarSlot> before(kQuickbarSlots), now(kQuickbarSlots);
  before[4] = QuickbarSlot{kQuickbarTypeFeat, 330};
  const std::vector<QuickbarChange> back = RebuildRestoredButtons(before, now, {kFeatRage});
  CHECK_EQ(back.size(), 1u);
  if (back.size() == 1u) {
    CHECK_EQ(back[0].slot, 4);
    CHECK_EQ(back[0].value, kFeatRage);
  }
  // Not when the engine kept a Rage button in another slot (327 in slot 9).
  now[9] = QuickbarSlot{kQuickbarTypeFeat, 327};
  CHECK_EQ(RebuildRestoredButtons(before, now, {kFeatRage}).empty(), true);
}
