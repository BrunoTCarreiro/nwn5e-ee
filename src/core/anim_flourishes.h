#pragma once

// A flourish inside the combat round (#121; owner: "I've had the rage animation cancel an attack animation"): Rage's
// n5_rage and Intimidating Presence's victory, played without cutting a swing or dropping the attack order.
//
// A script PlayAnimation runs ClearAllActions - the swing in progress is cut, the target dropped, the round emptied
// (the game session's BA-12). So a flourish never goes through one: the animation is set on the creature directly
// (CNWSCreature::SetAnimation) from a creature-update listener, at the moment data::DecideFlourish picks:
// - in a fight, after the creature's swing, with its round paused for the clip as the engine pauses it for an equip
//   or a flinch (CheckActionLength first; no attack is lost - the round's length shrinks by the pause);
// - out of combat, at once (the animation set back to the idle after the clip, so the next one is sent again).
// It waits while the creature moves, and drops if it dies, stops wanting it (Rage over) or waits 12 s.
//
// The clip: when `clip`'s recipe was built (Mocap mode) and the body is part-based (appearance.2da MODELTYPE P: the
// skeletons the animation packs serve), `base` is swapped for `clip` (EE's ReplaceObjectAnimation, as stance_look's
// crouch) 100 ms before it plays and mapped back to itself 0.6 s after; else `base` plays, `base_seconds` long.

#include <string>

constexpr int kFlourishVictory2 = 45;  // ANIMATION_FIREFORGET_VICTORY2: victorymg
constexpr int kFlourishVictory3 = 46;  // ANIMATION_FIREFORGET_VICTORY3: victoryth

using FlourishWanted = bool (*)(void* creature);

// Asks for `engine_animation` on `creature` (see the top). `wanted`: null for always. A new request for the same
// creature replaces the old one (its swap restored first).
void RequestFlourishClip(void* creature, int engine_animation, const char* base, float base_seconds,
                         const std::string& clip, FlourishWanted wanted);

// Registers the creature-update listener (InstallAnimLayers calls it); logged if it can't.
void InstallFlourish();
