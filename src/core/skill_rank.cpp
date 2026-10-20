#include "skill_rank.h"

#include <intrin.h>

#include <algorithm>
#include <vector>

#include "ee_layout.h"
#include "hooks.h"
#include "log.h"

#pragma intrinsic(_ReturnAddress)

namespace {

using GetSkillRankFn = char (*)(void* stats, uint8_t skill, void* vs, int base_only);
GetSkillRankFn g_get_skill_rank = nullptr;
std::vector<SkillAdjuster> g_adjusters;  // filled at install, then only read
bool g_hooked = false;
bool g_hook_failed = false;

char GetSkillRank(void* stats, uint8_t skill, void* vs, int base_only) {
  const char engine = g_get_skill_rank(stats, skill, vs, base_only);
  if (!stats) return engine;
  const SkillQuery query{stats, skill, vs, base_only != 0, reinterpret_cast<uintptr_t>(_ReturnAddress())};
  int value = engine;
  for (SkillAdjuster adjuster : g_adjusters) value = adjuster(query, value);
  return static_cast<char>(std::clamp(value, -127, 127));
}

}  // namespace

bool AddSkillAdjuster(SkillAdjuster adjuster) {
  if (!g_hooked && !g_hook_failed) {
    g_hooked = HookExport("CNWSCreatureStats::GetSkillRank (skill totals)", ee::kGetSkillRank,
                          reinterpret_cast<void*>(&GetSkillRank), reinterpret_cast<void**>(&g_get_skill_rank));
    g_hook_failed = !g_hooked;
  }
  if (!g_hooked) return false;
  g_adjusters.push_back(adjuster);
  return true;
}

uintptr_t SkillRankCallerIn(const char* export_name) {
  const uintptr_t function = EngineExport(export_name);
  const uintptr_t target = EngineExport(ee::kGetSkillRank);
  if (!function || !target) {
    Log("SKIP skills: %s or GetSkillRank is not exported", export_name);
    return 0;
  }
  const std::vector<uintptr_t> calls = FindCallsTo(FunctionAt(function), target);
  if (calls.size() != 1) {
    Log("SKIP skills: %zu GetSkillRank calls in %s (expected 1)", calls.size(), export_name);
    return 0;
  }
  return calls[0] + kCallLength;
}
