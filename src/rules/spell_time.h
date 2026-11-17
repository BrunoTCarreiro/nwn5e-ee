#pragma once

#include <string>

namespace rules {

// #39 (owner, 2026-11-17: "every minute in-game is 2 seconds, so that means that the disk lasts 2 minutes as it has a
// 1 hour duration. this seems way too short" -> "go with A. no real-time clock"): with spellcasting faithful a spell's
// duration counts real time, on the round's scale (a round 6 seconds, so a minute 60 seconds and an hour 60 minutes) -
// not the module's clock (NWN's hour: 2 real minutes here), which keeps its own pace for the day, the night and the
// rests' wait. A rest is the time it says: a Short Rest an hour, a Long Rest 8 hours - what it outlasts ends.
constexpr int kShortRestSeconds = 3600;
constexpr int kLongRestSeconds = 8 * 3600;

// A duration of `minutes` in real seconds.
int SpellSeconds(int minutes);

// A rest of `rest_seconds` ends a spell's effect with `remaining_seconds` left (0 or less: none left).
bool RestEnds(int rest_seconds, int remaining_seconds);

// A spell's area (its vfx_persistent row's label, the area object's tag) a rest ends: nwn5e's Entangle and Grease (a
// minute) and Fog Cloud (an hour) at any rest, Alarm (8 hours) at a Long Rest; anything else - a creature's aura, a
// flask's puddle - isn't a spell of ours, left alone.
bool RestEndsSpellArea(const std::string& tag, bool long_rest);

}  // namespace rules
