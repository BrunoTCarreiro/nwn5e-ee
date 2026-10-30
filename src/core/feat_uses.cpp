#include "feat_uses.h"

#include <intrin.h>

#include <stdint.h>

#include <algorithm>
#include <vector>

#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "records.h"
#include "rest.h"

namespace {

using FeatUsesFn = uint8_t (*)(void* stats, uint16_t feat);
using FeatUseChangeFn = void (*)(void* stats, uint16_t feat);

struct Provider {
  const char* record;
  FeatUsesTotal total;
  FeatUsesCost cost;
  FeatUsesSpend spend;
};

int CostOf(const Provider& provider, void* stats, int feat) {
  const int cost = provider.cost ? provider.cost(stats, feat) : 1;
  return cost > 1 ? cost : 1;
}

FeatUsesFn g_remaining = nullptr;
FeatUsesFn g_total = nullptr;
FeatUseChangeFn g_decrement = nullptr;
FeatUseChangeFn g_increment = nullptr;
std::vector<Provider> g_providers;  // filled at install, then only read
bool g_hooked = false;
bool g_hook_failed = false;

// The provider claiming `feat` for `stats`, with its total; nullptr: the engine's.
const Provider* Claim(void* stats, int feat, int* total) {
  if (!stats) return nullptr;
  for (const Provider& provider : g_providers) {
    if (provider.total(stats, feat, total)) return &provider;
  }
  return nullptr;
}

int Spent(void* stats, const Provider& provider) {
  void* creature = CreatureOf(stats);
  return creature ? SpentFeatUses(creature, provider.record) : 0;
}

uint8_t GetFeatRemainingUses(void* stats, uint16_t feat) {
  int total = 0;
  const Provider* provider = Claim(stats, feat, &total);
  if (!provider) return g_remaining(stats, feat);
  return static_cast<uint8_t>(std::clamp((total - Spent(stats, *provider)) / CostOf(*provider, stats, feat), 0, 100));
}

uint8_t GetFeatTotalUses(void* stats, uint16_t feat) {
  int total = 0;
  const Provider* provider = Claim(stats, feat, &total);
  if (!provider) return g_total(stats, feat);
  return static_cast<uint8_t>(std::clamp(total / CostOf(*provider, stats, feat), 0, 100));
}

void DecrementFeatRemainingUses(void* stats, uint16_t feat) {
  const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
  int total = 0;
  const Provider* provider = Claim(stats, feat, &total);
  void* creature = provider ? CreatureOf(stats) : nullptr;
  if (!creature) {
    g_decrement(stats, feat);
    return;
  }
  if (provider->spend && !provider->spend(stats, feat, caller)) return;
  SetSpentFeatUses(creature, provider->record,
                   std::min(total, Spent(stats, *provider) + CostOf(*provider, stats, feat)));
}

void IncrementFeatRemainingUses(void* stats, uint16_t feat) {
  int total = 0;
  const Provider* provider = Claim(stats, feat, &total);
  void* creature = provider ? CreatureOf(stats) : nullptr;
  if (!creature) {
    g_increment(stats, feat);
    return;
  }
  SetSpentFeatUses(creature, provider->record, Spent(stats, *provider) - CostOf(*provider, stats, feat));
}

// The quickbar's and the radial's counts (0.57.8; owner, 2026-10-30: Stunning Strike "greyed out with a 0.
// my focus is full"): the engine sends a player's client the feat uses only when the feat list or a feat
// entry's own byte changes (ee_layout.h kStatsFeatUses), then GetFeatRemainingUses for each. A pool lives
// in a record, so nothing the engine compares changed when it did - the client kept the count it was last
// sent. For a feat a provider claims, the byte now holds its remaining uses, set here at a player
// character's AIUpdate when it differs: a spend, a rest, a !focus or a level reaches the client at the next
// update. Every engine accessor of a claimed feat is hooked above, and a save writes GetFeatRemainingUses,
// so the byte is free for this.
int g_sync_lines = 0;

void SyncClientUses(void* creature) {
  void* stats = StatsOf(creature);
  if (!stats || Field<int32_t>(stats, ee::kStatsIsPlayer) == 0) return;
  uint8_t* const* entries = Field<uint8_t* const*>(stats, ee::kStatsFeatUses);
  const int count = Field<int32_t>(stats, ee::kStatsFeatUsesCount);
  if (!entries || count <= 0 || count > 1000) return;
  for (int i = 0; i < count; ++i) {
    uint8_t* entry = entries[i];
    if (!entry) continue;
    const uint16_t feat = *reinterpret_cast<const uint16_t*>(entry);
    int total = 0;
    if (!Claim(stats, feat, &total)) continue;
    const uint8_t remaining = GetFeatRemainingUses(stats, feat);
    if (entry[ee::kFeatUsesEntryByte] == remaining) continue;
    entry[ee::kFeatUsesEntryByte] = remaining;
    if (g_sync_lines < 60) {
      ++g_sync_lines;
      Log("feat uses: 0x%08X feat %u - %u left, for the client's counts", ObjectIdOf(stats), feat, remaining);
    }
  }
}

// The end of a Long Rest: every pool full.
void AfterLongRest(void* stats) {
  void* creature = CreatureOf(stats);
  if (!creature) return;
  for (const Provider& provider : g_providers) {
    if (SpentFeatUses(creature, provider.record) != 0) SetSpentFeatUses(creature, provider.record, 0);
  }
}

bool Hook() {
  if (g_hooked || g_hook_failed) return g_hooked;
  g_hooked = HookExport("CNWSCreatureStats::GetFeatRemainingUses (X6: nwn5e's feature uses)",
                        ee::kGetFeatRemainingUses, reinterpret_cast<void*>(&GetFeatRemainingUses),
                        reinterpret_cast<void**>(&g_remaining)) &&
             HookExport("CNWSCreatureStats::GetFeatTotalUses (X6)", ee::kGetFeatTotalUses,
                        reinterpret_cast<void*>(&GetFeatTotalUses), reinterpret_cast<void**>(&g_total)) &&
             HookExport("CNWSCreatureStats::DecrementFeatRemainingUses (X6)", ee::kDecrementFeatRemainingUses,
                        reinterpret_cast<void*>(&DecrementFeatRemainingUses), reinterpret_cast<void**>(&g_decrement)) &&
             HookExport("CNWSCreatureStats::IncrementFeatRemainingUses (X6)", ee::kIncrementFeatRemainingUses,
                        reinterpret_cast<void*>(&IncrementFeatRemainingUses), reinterpret_cast<void**>(&g_increment));
  if (g_hooked && !AddRestListener(&AfterLongRest)) {
    Log("feat uses: the end of a rest is not hooked - nwn5e's feature uses never come back");
  }
  if (g_hooked && !AddCreatureUpdateListener(&SyncClientUses)) {
    Log("feat uses: AIUpdate is not hooked - the quickbar's counts of nwn5e's uses change only with the feat list");
  }
  g_hook_failed = !g_hooked;
  if (!g_hooked) Log("SKIP feat uses: a feat-use export is missing - the engine counts every feat's uses");
  return g_hooked;
}

}  // namespace

bool AddFeatUsesProvider(const char* record, FeatUsesTotal total, FeatUsesCost cost, FeatUsesSpend spend) {
  if (!Hook()) return false;
  RegisterRecord(record);
  g_providers.push_back(Provider{record, total, cost, spend});
  return true;
}

int SpentFeatUses(void* creature, const char* record) {
  if (!creature) return 0;
  EnsureRecords(creature);
  return std::max(0, engine::GetLocalInt(creature, record));
}

void SetSpentFeatUses(void* creature, const char* record, int spent) {
  if (creature) SetRecord(creature, record, std::max(0, spent));
}
