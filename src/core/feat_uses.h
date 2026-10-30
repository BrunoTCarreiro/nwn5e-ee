#pragma once

#include <stdint.h>

// X6 (docs/PLAN.md "Phase 3", shared by the non-caster classes): feature
// uses nwn5e counts itself. The engine keeps a feat's uses per feat id (its
// total from feat.2da and the class tables, a spent byte on the creature's
// feat list); a provider takes a set of feats over - their total by its own
// rule, and one pool of spent uses shared by every feat it claims (Rage's
// chain 293, 326-331 is one pool), kept in a record on the creature (records.h:
// a saved game and a character export keep it). The engine's
// GetFeatRemainingUses / GetFeatTotalUses / Decrement- /
// IncrementFeatRemainingUses answer from the pool - the radial, UseFeat, the
// AI's talents and the scripts alike - and the end of a Long Rest
// (ResetFeatRemainingUses) empties it.
// 0.57.8: the client is told a feat's uses only when the feat list or a feat
// entry's own uses byte changes (ee_layout.h kStatsFeatUses), and a pool lives
// in a record - so for a player character the byte of each claimed feat
// mirrors its remaining uses (set at its AIUpdate when it differs), and the
// quickbar and the radial follow every spend, rest and level.

// The total uses of `feat` for `stats` (true), or false: the feat is the
// engine's. Asked often (the radial's refresh), so it tests the feat first.
using FeatUsesTotal = bool (*)(void* stats, int feat, int* total);

// 0.56.0 (the Monk's Focus): optional - how many of the pool's uses a feat
// costs (Superior Defense 3: its uses shown are the pool / 3), and whether an
// engine decrement at `caller` spends at all (Stunning Strike: not on a miss).
using FeatUsesCost = int (*)(void* stats, int feat);
using FeatUsesSpend = bool (*)(void* stats, int feat, uintptr_t caller);
// `record` names the pool's record (registered here). Hooks on first use;
// false (and logged) if a hook can't be made.
bool AddFeatUsesProvider(const char* record, FeatUsesTotal total, FeatUsesCost cost = nullptr,
                         FeatUsesSpend spend = nullptr);

// The pool's spent uses on a creature, and setting them (never below 0) -
// for a rule that gives uses back (a Short Rest, Persistent Rage).
int SpentFeatUses(void* creature, const char* record);
void SetSpentFeatUses(void* creature, const char* record, int spent);
