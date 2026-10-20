#pragma once

#include <stdint.h>

// CNWSCreatureStats::HasFeat(feat) — asked everywhere (feat checks,
// prerequisites, rule branches). A rule that must change one answer at one
// place adds a filter: it returns true and sets *answer to decide (`caller` =
// the engine call site's return address, to pick the place), or false to
// leave it. Added at install, run on the engine's thread.
using FeatFilter = bool (*)(void* stats, int feat, uintptr_t caller, int* answer);

// Hooks HasFeat on first use; false (and logged) if it can't.
bool AddFeatFilter(FeatFilter filter);
