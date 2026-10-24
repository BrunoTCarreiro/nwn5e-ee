#pragma once

#include <stdint.h>

// Game time: milliseconds that pass while the game runs - not while it is
// paused (the server's pause state; a single-player pause). Ticked from the
// creature updates, which run during a pause too; a step longer than a second
// (a load, a stall) doesn't count. For rules timed in rounds (Rage lapsing
// after one). rules_dying keeps its own clock (Phase 5b).
uint64_t GameMs();

// Starts the clock (hooks the creature update); false (and logged) if it can't.
bool StartGameClock();
