#pragma once

#include <stdint.h>

// #124: NWN's grenades as 2024 gear (grenade_gear.cpp - its own name: build.bat's one obj folder), key `grenades`.
void InstallGrenades();
bool GrenadesFaithful();
// The thrower's save DC (rules::GrenadeSaveDc); false when `thrower` isn't a creature (a trap, an area of effect).
bool GrenadeDcOf(uint32_t thrower, int* dc);
// An order's ItemCastSpell action of a grenade (action_order.cpp: 1A).
bool IsGrenadeCast(const void* action_node);
