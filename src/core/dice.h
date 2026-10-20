#pragma once

#include <stdint.h>

// CNWRules::RollDice(count, sides) — the engine's dice (76 call sites in EE:
// attack and confirmation d20s, saves, initiative, damage dice, ...). A rule
// that must decide a roll adds a filter: it returns true and sets *result to
// take the roll over (`caller` = the engine call site's return address, to
// tell the rolls apart), or false to leave it to the next filter and finally
// to the engine. Filters are added at install and run on the engine's thread.
using DiceFilter = bool (*)(uintptr_t caller, int count, int sides, int* result);

// Hooks RollDice on first use; false (and logged) if it can't.
bool AddDiceFilter(DiceFilter filter);
