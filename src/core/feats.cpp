#include "feats.h"

#include <intrin.h>

#include <vector>

#include "ee_layout.h"
#include "hooks.h"

namespace {

using HasFeatFn = int (*)(void* stats, uint16_t feat);
HasFeatFn g_has_feat = nullptr;
std::vector<FeatFilter> g_filters;  // filled at install, then only read
bool g_hooked = false;
bool g_hook_failed = false;

int HasFeat(void* stats, uint16_t feat) {
  const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
  for (FeatFilter filter : g_filters) {
    int answer = 0;
    if (filter(stats, feat, caller, &answer)) return answer;
  }
  return g_has_feat(stats, feat);
}

}  // namespace

bool AddFeatFilter(FeatFilter filter) {
  if (!g_hooked && !g_hook_failed) {
    g_hooked = HookExport("CNWSCreatureStats::HasFeat", ee::kHasFeat, reinterpret_cast<void*>(&HasFeat),
                          reinterpret_cast<void**>(&g_has_feat));
    g_hook_failed = !g_hooked;
  }
  if (!g_hooked) return false;
  g_filters.push_back(filter);
  return true;
}
