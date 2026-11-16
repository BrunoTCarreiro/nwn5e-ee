#pragma once

#include <vector>

namespace rules {

// #39 Part 2a's end, the level 1 list switch (owner, 2026-11-09, Part 2's brief: "1.B 2.B 3.A 4.A 5.A 6.B" - Q3 A
// 2024's class lists and levels, Q4 A NWN's spells with no 2024 version off the class lists in faithful (NWN mode
// keeps them), Q5 A Summon Creature I-IX off in faithful with a house rule keeping them; built 2026-11-16: "yes,
// build the list switch"). After it the faithful level 1 lists are SRD 5.2.1's: every NWN row a class had at level 1
// that SRD 5.2.1 hasn't there is retired (on no class's list) or moved to its 2024 level and classes. Each row's
// spells.2da class columns as served (spells.if_spells2024.lists1.txt, spells.if_retiresummons.txt): kNoLevel ****.
// Rows SRD 5.2.1 has at level 1 are the builds' (their own rows files); NWN's one Wiz_Sorc column is split by
// rules::WizardOnlySpell.
constexpr int kNoLevel = -1;

enum class ListSwitch { kRetired, kMoved };

struct ClassListRow {
  int row = 0;
  const char* name = "";
  ListSwitch change = ListSwitch::kRetired;
  int bard = kNoLevel;
  int cleric = kNoLevel;
  int druid = kNoLevel;
  int paladin = kNoLevel;
  int ranger = kNoLevel;
  int wiz_sorc = kNoLevel;
};

// spells.if_spells2024.lists1.txt's rows (served with spellcasting faithful), ascending.
const std::vector<ClassListRow>& Level1ListSwitch();
// spells.if_retiresummons.txt's (served with spellcasting faithful unless [house_rules] summon_creature): Summon
// Creature I - II to IX join with their levels' passes.
const std::vector<ClassListRow>& RetiredSummons();
// Either table's row, or null.
const ClassListRow* ListSwitchRowOf(int row);
// A class column's level on the row (0 Bard, 1 Cleric, 2 Druid, 3 Paladin, 4 Ranger, 5 Wiz_Sorc).
int ListLevel(const ClassListRow& row, int column);

}  // namespace rules
