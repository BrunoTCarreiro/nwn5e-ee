#pragma once

#include <stdint.h>

// light_sources.cpp's, for other rules.

// 0.70.0 (#124, #130; owner, 2026-11-04: "is it possible to create a temporary light source while it's active?"):
// while on, an invisible object a script creates (CreateObject "plc_invisobj": a grenade's fire light,
// grenade_gear.cpp) loads as a normal placeable - not static, so the client gets it and its light effect - not
// clickable, plot. Its LoadPlaceable hook is installed with the grenades faithful too.
void CreatingFireLight(bool on);

// 0.71.0 (#124; owner, 2026-11-04: "bombs (poison and fire) along with thunderstone should tunr off torches and other
// such light sources in range of the explosion as they blow up"): a blast puts out the lit lights within `radius` m of
// the spell target location of `self` (the thrower, its grenade's script just run) - lamps, braziers, torch brackets
// and the tiles' torches, as a hit puts one out; relit as ever (Use, the Light spell). With light_sources on only.
// Returns how many went out.
int BlastPutsOutLights(uint32_t self, float radius, const char* what);
