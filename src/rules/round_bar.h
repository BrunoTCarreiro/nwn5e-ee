#pragma once

#include <vector>

// The round bar (core/round_bar_window.cpp; #127, owner, 2026-11-03: "sometimes it's
// hard to figure out when a round ends and the next one begins ... like a
// small loading bar that would show the state of the current round?", then
// "go with A"): under the action queue strip, in a fight - the player
// character's combat round filling (gold an attack round, blue a cast round,
// dimmed while paused), its swings as ticks (filled once made), and three pips
// for what the round has left: the Action, the Bonus Action, the Reaction.

namespace rules {

constexpr float kRoundBarWidth = 300.0f;   // the content, GUI units
constexpr float kRoundBarHeight = 22.0f;
constexpr float kRoundBarWindowExtra = 16.0f;  // the window's and the group's padding, around the content
constexpr int kRoundBarMaxSwings = 10;

// How far the round is: its timer over its length (0..1; nothing without a length).
float RoundFill(long long timer_ms, long long length_ms);
// Where the round's swings fall (0..1), in order, at most kRoundBarMaxSwings.
std::vector<float> RoundSwingMarks(const std::vector<long long>& due_ms, long long length_ms);

enum class RoundKind { kNone, kAttack, kCast };
RoundKind RoundKindOf(bool running, bool cast_round);

// A pip: what the round still has.
enum class RoundPip { kFree, kSpent, kWaiting };
RoundPip ActionPip(bool round_running);           // a round of its own running: its Action taken
RoundPip BonusPip(bool free, bool waiting);       // waiting: spent, one more for the next round
RoundPip ReactionPip(bool available);

// The bar's top (window pixels): under the strip when it's on, else the screen's top.
float RoundBarTop(bool strip_on, float scale);

}  // namespace rules
