// CM8 (which feats exist) and C3 (docs/CHARGEN.md), `feats` faithful: the
// creation and level-up screens offer only the SRD 5.2 feats nwn5e has built
// (rules::IsChoosableFeat2024) — the client's CanChooseFeat, which the Feats
// page asks for every feat it lists and the packages' default picks ask for
// every feat they try, says no to the others. With feat_cadence's slots that
// makes the level-1 Feats page 2024's: a Fighter's bonus pick is its Fighting
// Style (ASI waits for level 4), a human's extra pick an Origin feat (Alert so
// far). The packages' feat tables get the 2024 feats appended
// (data/rows/packft*.txt), so Recommended picks them too; NWN mode reaches
// them only after its own.
//
// Characters keep the NWN feats they have (the feature mask, CM9, decides
// what those do). Henchmen's automatic level-ups (the server's own
// CanChooseFeat) are not changed here.

#include <windows.h>

#include <stdint.h>

#include "config.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "rules/feats2024.h"

namespace {

using CanChooseFeatFn = int (*)(void* level_up_stats, uint16_t feat, uint8_t general_left, uint8_t bonus_left,
                                void* chosen);
CanChooseFeatFn g_can_choose_feat = nullptr;
bool g_told = false;

int CanChooseFeat(void* level_up_stats, uint16_t feat, uint8_t general_left, uint8_t bonus_left, void* chosen) {
  if (!g_told) {
    g_told = true;
    Log("feats: the screens offer only the 2024 feats nwn5e has built (feats faithful)");
  }
  if (!rules::IsChoosableFeat2024(feat)) return 0;
  return g_can_choose_feat(level_up_stats, feat, general_left, bonus_left, chosen);
}

}  // namespace

void InstallFeatRules() {
  if (g_config.feats != rules::RuleVariant::kFaithful) return;
  const uintptr_t can_choose = FunctionByString("feats: the client's CanChooseFeat", ee::kCanChooseFeatMessage);
  if (!can_choose || FunctionEntry(can_choose) != can_choose) {
    Log("SKIP feats: the client's CanChooseFeat not found - NWN's feats stay on the screens");
    return;
  }
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  Log("feats: the client's CanChooseFeat at rva 0x%zx", can_choose - base);
  HookAddress("CNWCLevelUpStats::CanChooseFeat (feats: only the 2024 feats offered)", can_choose,
              reinterpret_cast<void*>(&CanChooseFeat), reinterpret_cast<void**>(&g_can_choose_feat));
}
