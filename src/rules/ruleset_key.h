#pragma once

#include <stdint.h>

namespace rules {

// NWN: EE looks ruleset.2da values up by the standard FNV-1a-64 hash of the
// label (basis 0xcbf29ce484222325, prime 0x100000001b3, characters
// sign-extended — CNWRules::fnv1a; docs/EE-PORT.md).
uint64_t RulesetLabelKey(const char* label);

}  // namespace rules
