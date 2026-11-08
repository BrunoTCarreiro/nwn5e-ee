#pragma once

#include <stdint.h>

#include <vector>

// Phase 6 part 1 (#52): the 2024 Bonus Action (bonus_actions.cpp), `actions` faithful.

// rules_two_weapon.cpp, after InitializeNumberOfAttacks set the round's off-hand attacks and before the Monk's
// strike: the round's automatic bonus attacks - the off-hand attack dropped when the Bonus Action is taken or kept
// free (unless Nick makes it part of the Attack action).
void BonusActionRoundStart(void* round);
// rules_monk.cpp: whether the Martial Arts strike / Flurry of Blows is this round's Bonus Action.
bool MartialArtsBonusAllowed(void* creature);
// The Combat configuration window (combat_options.cpp): the default Bonus Action - its key: 0 the automatic bonus
// attack, -1 none, else a Bonus Action ability's family (rules::BonusFeatFamily); whether the creature has one of a
// family's feats.
bool ActionEconomyOn();
int DefaultBonusKey(void* creature);
void SetDefaultBonusKey(void* creature, int key);
bool HasBonusFamily(void* creature, int family);
// The action queue strip (action_queue.cpp): a waiting Bonus Action ("next round") and one just used (a moment).
struct BonusStripItem {
  int feat = -1;
  bool waiting = false;
  uint32_t item = 0x7F000000;  // #120: a potion waiting - the item and its power (active property)
  int active_property = -1;
};
std::vector<BonusStripItem> BonusActionStripItems(void* creature);
bool CancelWaitingBonusAction(void* creature);
// #39 build 4b: a rule's own Bonus Action (Dancing Lights' Move): spends the round's if it's free (true), else tells
// the player it's spent (false). True with `actions` in NWN mode (no Bonus Action).
bool SpendBonusAction(void* creature, const char* what);
// #127 (the round bar): 0 the round's Bonus Action free, 1 spent, 2 spent with one waiting for the next round.
int BonusActionStatus(void* creature);
// #127: the round bar (round_bar_window.cpp) - on unless turned off (!roundbar, Combat configuration).
bool RoundBarEnabled(void* creature);
void SetRoundBar(void* creature, bool on);
