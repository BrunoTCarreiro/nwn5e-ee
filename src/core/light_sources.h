#pragma once

// light_sources.cpp's, for other rules.

// 0.70.0 (#124, #130; owner, 2026-11-04: "is it possible to create a temporary light source while it's active?"):
// while on, an invisible object a script creates (CreateObject "plc_invisobj": a grenade's fire light,
// grenade_gear.cpp) loads as a normal placeable - not static, so the client gets it and its light effect - not
// clickable, plot. Its LoadPlaceable hook is installed with the grenades faithful too.
void CreatingFireLight(bool on);
