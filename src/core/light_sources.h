#pragma once

#include <stdint.h>

#include "rules/cantrips.h"

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

// #39 build 4a: a cantrip's Fire Play at `target` by `caster` - a light nwn5e handles (a lamp, brazier or torch
// bracket, or a tile flame's stand-in), as a click, a hit or the Light spell does it: rules::FirePlay kToggle lights
// an unlit one and snuffs a lit one, kLight lights only, kSnuff snuffs only, kDim snuffs one for `dim_seconds` of
// game time, then lights it again. A light put out draws hostiles as a hit does (heard_investigates). Its name is
// left in its local n5c_name. Returns 1 lit, 2 put out, 0 nothing to do (already so), -1 not a light (or
// light_sources off).
int FirePlayAt(uint32_t caster, uint32_t target, rules::FirePlay mode, int dim_seconds);

// Any object of the server by id (CNWSObject*: a placeable, an area of effect ...), null if none (0.73.0, #131).
void* GameObjectById(uint32_t id);
