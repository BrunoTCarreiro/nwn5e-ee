#pragma once

// CNWSCreature::UseFeat — a feat used from the radial or the quickbar, and the
// scripts' ActionUseFeat (the AI's). Guards run first: one returning false
// refuses the use — nothing spent, nothing queued (the guard tells the player
// why). Listeners run after a use the engine queued. One hook for every rule
// (the potion resume, Rage).
using FeatUseGuard = bool (*)(void* creature, int feat);
using FeatUseListener = void (*)(void* creature, int feat);

// Hook UseFeat on first use; false (and logged) if it can't.
bool AddFeatUseGuard(FeatUseGuard guard);
bool AddFeatUseListener(FeatUseListener listener);
