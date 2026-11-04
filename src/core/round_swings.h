#pragma once

#include <stdint.h>

// One swing per attack (#53, 0.66.2): round_swings.cpp.
void InstallRoundSwings();

// Action Surge in a running round: `count` attacks at `target` added as swings of their own, spaced with the
// round's pending swings (the round's extra attacks counted); false when the round can't take them (not running, a
// cast round, nothing to make them with) - nothing changed then.
bool AddRoundSwings(void* round, uint32_t target, int count);

// #124 1A: a player's grenade thrown mid-round takes one of the round's remaining swings (the last), or - none left -
// one of the next round's. False when the creature has no attack round running.
bool TakeRoundSwing(void* creature);
