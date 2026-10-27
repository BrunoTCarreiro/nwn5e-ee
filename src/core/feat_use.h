#pragma once

#include <stdint.h>

// CNWSCreature::UseFeat — a feat used from the radial or the quickbar, and the
// scripts' ActionUseFeat (the AI's). Guards run first: one returning false
// refuses the use — nothing spent, nothing queued (the guard tells the player
// why). Listeners run after a use the engine queued. One hook for every rule
// (the potion resume, Rage).
using FeatUseGuard = bool (*)(void* creature, int feat);
using FeatUseListener = void (*)(void* creature, int feat);
// A feat nwn5e carries out itself (G1: its Unarmed Strike options): after the
// guards, a handler returning 0 or 1 is the use's result and the engine's
// UseFeat is not called; -1 leaves the feat to the next handler / the engine.
using FeatUseHandler = int (*)(void* creature, int feat, int subfeat, uint32_t target);

// Hook UseFeat on first use; false (and logged) if it can't.
bool AddFeatUseGuard(FeatUseGuard guard);
bool AddFeatUseListener(FeatUseListener listener);
bool AddFeatUseHandler(FeatUseHandler handler);
