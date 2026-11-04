#pragma once

#include <stddef.h>

#include <string>
#include <vector>

namespace rules {

// The script handlers' names (core/script_hooks.cpp): which registered name the script the engine runs is - resrefs
// are case-blind. nwn5e's own (n5_...) and the game's it replaces in C++ (#124: x0_s3_acid, x2_s3_bomb ...); -1 for
// none. 0.69.2 (owner, 2026-11-04: "The acid flask ... clearly has a pretty big splashzone"): the lookup took only
// names starting "n5", so the game's grenade scripts ran as NWN's.
int FindScriptName(const std::vector<std::string>& names, const char* chars, size_t length);

}  // namespace rules
