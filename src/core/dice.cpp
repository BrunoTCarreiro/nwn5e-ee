#include "dice.h"

#include <intrin.h>

#include <vector>

#include "ee_layout.h"
#include "hooks.h"

namespace {

using RollDiceFn = uint16_t (*)(void* rules, uint8_t count, uint8_t sides);
RollDiceFn g_roll_dice = nullptr;
std::vector<DiceFilter> g_filters;  // filled at install, then only read
bool g_hooked = false;
bool g_hook_failed = false;

uint16_t RollDice(void* rules, uint8_t count, uint8_t sides) {
  const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
  for (DiceFilter filter : g_filters) {
    int result = 0;
    if (filter(caller, count, sides, &result)) return static_cast<uint16_t>(result < 0 ? 0 : result);
  }
  return g_roll_dice(rules, count, sides);
}

}  // namespace

bool AddDiceFilter(DiceFilter filter) {
  if (!g_hooked && !g_hook_failed) {
    g_hooked = HookExport("CNWRules::RollDice", ee::kRollDice, reinterpret_cast<void*>(&RollDice),
                          reinterpret_cast<void**>(&g_roll_dice));
    g_hook_failed = !g_hooked;
  }
  if (!g_hooked) return false;
  g_filters.push_back(filter);
  return true;
}
