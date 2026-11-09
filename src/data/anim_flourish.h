#pragma once

// A flourish inside the combat round (#121): Rage's n5_rage, Intimidating Presence's victory - played without cutting a
// swing or dropping the attack order. Each tick the DLL (core/anim_flourishes.cpp) reads the creature and its round into a
// FlourishTick and acts on the step: wait for its swing (a swing is the creature's own round pause, an enemy's swing at
// it pauses it too), pause the round for the clip and play it - however little of the round is left, the round
// stretches by the clip (#128) - play at once out of combat, or drop the request. Pure, so test.bat proves the order.

#include <cstdint>

namespace data {

struct FlourishTick {
  bool alive = false;          // not dead or dying
  bool wanted = false;         // still asked for (Rage still on)
  bool has_round = false;      // the creature has a combat round
  bool round_started = false;  // and it's running
  bool paused = false;         // the round is paused (a swing: its own, or an enemy's at it - running round or not)
  bool moving = false;         // walking or running (a chase between swings)
  bool attacking = false;      // its current action is an attack (a round not started yet starts next update)
  int64_t now_ms = 0;          // game time
  int64_t not_before_ms = 0;   // the animation swap reaches the client first
  int64_t give_up_ms = 0;      // past this the request drops
};

enum class FlourishStep { Wait, PlayNow, PauseAndPlay, Drop };

// In order: drop (dead, not wanted, past give-up); wait before the swap's gap; wait while moving (the walk would
// overwrite it) and while its round is paused (an enemy's swing pauses it before its own round runs too); play at
// once with no round, or one not running while it isn't attacking; wait for an attacker's next round; else pause
// and play (#128: however little of the round is left - the round stretches by the clip).
FlourishStep DecideFlourish(const FlourishTick& t);

// The round's pause for a clip of `clip_seconds`: the clip and a 75 ms pad, so the engine's "ready" at the pause's end
// comes after the clip.
int FlourishPauseMs(float clip_seconds);

// #150: how long a cast clip's swap stays (core/anim_cast_clip): the caller's `seconds` in ms, held to 0.1 - 10 s;
// 0 - no swap - for a time that isn't positive and finite.
int CastClipMs(float seconds);

}  // namespace data
