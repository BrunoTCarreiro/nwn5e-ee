#pragma once

#include <vector>

// One swing per attack (#53; owner, 2026-11-03, on Action Surge used before a fight: "the PC looks like it half
// swings the sword before each attack but only attacks once", then "go with A"; core/round_swings.cpp).
// CNWSCombatRound::InitializeAttackActions gives a round at most three swings; past three attacks a swing carries
// n / 3 of them in one animation, the first cut off. Here each attack gets a whole swing of its own, and the round
// keeps its 6 s: the engine takes its swings' total off the round's timer (their pauses give it back), so the timer
// gets the engine's total back and gives up each swing's instead.

namespace rules {

constexpr int kMinSwingGapMs = 300;  // a running round's timer between added swings, at least

// The engine's swing for a round of `attacks` attacks (InitializeAttackActions, AddCleaveAttack):
// min(750 n + 500, 3000) / (3 + (n - 3) / 2) ms - one attack 1250, two 1000.
int EngineSwingMs(int attacks);
// The engine's swings' total for a round, taken off its timer: min(750 n + 500, 3000).
int EngineRoundSwingTotalMs(int attacks);

struct SwingLayout {
  int round_length = 0;   // the round's timer length, ms
  std::vector<int> dues;  // each swing's due time on the timer, in order
};

// A round the engine laid out for `attacks` attacks - its timer `length_now` long, its first swing due at
// `first_due` - as one `swing_ms` swing per attack, due evenly from the first with the last gap at the round's end
// (as the engine spaces its three). No dues when it can't be done (fewer than two attacks, no room).
SwingLayout OneSwingPerAttack(int attacks, int first_due, int length_now, int swing_ms);

// `added` swings joining a running round (Action Surge) whose `pending` swings are still to come, its timer at
// `now` of `length`: all of them due evenly from now, at least kMinSwingGapMs apart - the round's end moved out
// when they don't fit, never in. The pending swings' dues first, then the added ones'.
SwingLayout AddedSwings(int pending, int added, int now, int length);

}  // namespace rules
