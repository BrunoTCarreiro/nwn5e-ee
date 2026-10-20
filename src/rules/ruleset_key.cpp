#include "rules/ruleset_key.h"

namespace rules {

uint64_t RulesetLabelKey(const char* label) {
  uint64_t hash = 0xcbf29ce484222325ull;
  for (const char* c = label; *c; ++c) {
    hash ^= static_cast<uint64_t>(static_cast<int64_t>(static_cast<signed char>(*c)));
    hash *= 0x100000001b3ull;
  }
  return hash;
}

}  // namespace rules
