#pragma once

#include <stdint.h>

// CNWSCreatureStats::UpdateCombatInformation — builds the character sheet's
// combat block (CCombatInformation at stats + 0xd70: attacks per round,
// attack bonuses, crit ranges and multipliers ...), which the server sends to
// the client. Several rules correct numbers EE computes inline there, so the
// function is hooked once and each rule adds a listener that runs after it.
using CombatInformationListener = void (*)(void* stats, uint8_t* info);

// Hooks UpdateCombatInformation on first use; false (and logged) if it can't.
bool AddCombatInformationListener(CombatInformationListener listener);
