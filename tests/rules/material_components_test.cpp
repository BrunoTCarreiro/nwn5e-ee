#include "check.h"
#include "rules/material_components.h"

#include <algorithm>

using namespace rules;

// #38: SRD 5.2's costly material components in gold - the cost on hand, the consumed part taken.
TEST(MaterialComponentCosts) {
  CHECK_EQ(CostlyComponent(142).needed, 500);   // Raise Dead: a diamond, consumed
  CHECK_EQ(CostlyComponent(142).consumed, 500);
  CHECK_EQ(CostlyComponent(86).needed, 100);    // Identify: a pearl, kept
  CHECK_EQ(CostlyComponent(86).consumed, 0);
  CHECK_EQ(CostlyComponent(6).needed, 5);       // Bless: a holy symbol, kept (the table's first)
  CHECK_EQ(CostlyComponent(6).consumed, 0);
  CHECK_EQ(CostlyComponent(376).needed, 450);   // Legend Lore: 250 consumed of 450
  CHECK_EQ(CostlyComponent(376).consumed, 250);
  CHECK_EQ(CostlyComponent(138).consumed, 25);  // Protection from Evil: a variant of row 321
  CHECK_EQ(CostlyComponent(172).consumed, 100); // Stoneskin
  CHECK_EQ(CostlyComponent(74).consumed, 100);  // Greater Stoneskin as Stoneskin (owner: "keep the gp cost")
  CHECK_EQ(CostlyComponent(63).needed, 5000);   // Gate, kept
  CHECK_EQ(CostlyComponent(549).consumed, 200); // Glyph of Warding (the table's last)
  CHECK_EQ(CostlyComponent(152).needed, 0);     // Restoration: free (owner, 2026-11-06)
  CHECK_EQ(CostlyComponent(97).needed, 0);      // Lesser Restoration: no cost
  CHECK_EQ(CostlyComponent(107).needed, 0);     // Magic Missile
  CHECK_EQ(CostlyComponent(-1).needed, 0);
  CHECK_EQ(ComponentAffordable(500, CostlyComponent(142)), true);
  CHECK_EQ(ComponentAffordable(499, CostlyComponent(142)), false);
  CHECK_EQ(ComponentAffordable(449, CostlyComponent(376)), false);  // the whole 450 on hand, not the 250
  CHECK_EQ(ComponentAffordable(0, CostlyComponent(107)), true);     // nothing to pay
  const std::vector<int>& all = CostlySpells();
  CHECK_EQ(all.size(), 49u);
  CHECK_EQ(std::is_sorted(all.begin(), all.end()), true);
  CHECK_EQ(std::adjacent_find(all.begin(), all.end()) == all.end(), true);
  for (int spell : all) CHECK_EQ(CostlyComponent(spell).consumed <= CostlyComponent(spell).needed, true);
}
