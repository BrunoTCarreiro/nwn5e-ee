#pragma once

// A flourish inside the combat round (#121): Rage's n5_rage, Intimidating Presence's victory - played without cutting a
// swing or dropping the attack order. Each tick the DLL (core/anim_flourishes.cpp) reads the creature and its round into a
// FlourishTick and acts on the step: wait for its swing (a swing is the creature's own round pause, an enemy's swing at
// it pauses it too), pause the round for the clip and play it (as the engine's equip and flinch do: no attack lost), play
// at once out of combat, or drop the request. Pure, so test.bat proves the order.

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
  bool fits = false;           // the engine's CheckActionLength accepts the pause now
  int64_t now_ms = 0;          // game time
  int64_t not_before_ms = 0;   // the animation swap reaches the client first
  int64_t give_up_ms = 0;      // past this the request drops
};

enum class FlourishStep { Wait, PlayNow, PauseAndPlay, Drop };

// In order: drop (dead, not wanted, past give-up); wait before the swap's gap; wait while moving (the walk would
// overwrite it) and while its round is paused (an enemy's swing pauses it before its own round runs too); play at
// once with no round, or one not running while it isn't attacking; wait for an attacker's next round; pause and
// play when the pause fits; else wait (the next round's start clears its timer and any pause, so it fits then).
FlourishStep DecideFlourish(const FlourishTick& t);

// The round's pause for a clip of `clip_seconds`: the clip and a 75 ms pad, so the engine's "ready" at the pause's end
// comes after the clip.
int FlourishPauseMs(float clip_seconds);

}  // namespace data
