#pragma once

// #38 (docs/PLAN.md "Phase 4"; owner, 2026-11-06: "38, rituals A, components B for now, we'll need to check the
// meterial costs", then "for now keep the gp cost. we'll see how it works on a playthrough"): SRD 5.2's costly
// material components, paid in gold. A focus or a component pouch stands in for a spell's ordinary material
// components, not for one with a cost: the caster must have that component, and one the spell consumes is used up
// by each cast. nwn5e counts the cost in gold - the whole cost on hand to cast, the consumed part taken. The costs
// are SRD 5.2.1's, for the NWN spells that map to an SRD spell (concentration.h's table's way).

#include <vector>

namespace rules {

struct ComponentCost {
  int needed = 0;    // GP the caster must have (0: no costly component)
  int consumed = 0;  // GP each cast takes
};

// spells.2da row `spell`'s costly component ({0, 0}: none).
ComponentCost CostlyComponent(int spell);
// Whether `gold` pays for it (always, when it has none).
bool ComponentAffordable(int gold, const ComponentCost& cost);
// Every row with one, ascending.
const std::vector<int>& CostlySpells();

}  // namespace rules
