#include "check.h"
#include "rules/level_lists.h"
#include "rules/spells2024.h"

#include <vector>

using namespace rules;

// #39 Part 2a's end, the level 1 list switch (owner, 2026-11-16: "yes, build the list switch"; the rules decided
// 2026-11-09: Q3 A, Q4 A, Q5 A).
TEST(LevelOneListSwitch) {
  const std::vector<ClassListRow>& rows = Level1ListSwitch();
  CHECK_EQ(rows.size(), 19u);
  int retired = 0;
  int moved = 0;
  int previous = -1;
  bool ascending = true;
  bool none_at_level_1 = true;
  bool retired_on_no_list = true;
  for (const ClassListRow& row : rows) {
    ascending = ascending && row.row > previous;
    previous = row.row;
    for (int column = 0; column < 6; ++column) none_at_level_1 = none_at_level_1 && ListLevel(row, column) != 1;
    if (row.change == ListSwitch::kRetired) {
      ++retired;
      for (int column = 0; column < 6; ++column) retired_on_no_list = retired_on_no_list && ListLevel(row, column) == kNoLevel;
    } else {
      ++moved;
    }
  }
  CHECK_EQ(ascending, true);
  CHECK_EQ(none_at_level_1, true);
  CHECK_EQ(retired_on_no_list, true);
  CHECK_EQ(retired, 16);
  CHECK_EQ(moved, 3);
  // Moved to SRD 5.2.1's level 2 and classes.
  const ClassListRow* weapon = ListSwitchRowOf(544);  // Magic Weapon: Paladin, Ranger, Sorcerer, Wizard
  CHECK_EQ(weapon != nullptr && weapon->paladin == 2 && weapon->ranger == 2 && weapon->wiz_sorc == 2 &&
               weapon->bard == kNoLevel && weapon->cleric == kNoLevel && weapon->druid == kNoLevel,
           true);
  const ClassListRow* darkvision = ListSwitchRowOf(365);  // NWN's Ultravision: Druid, Ranger, Sorcerer, Wizard
  CHECK_EQ(darkvision != nullptr && darkvision->druid == 2 && darkvision->ranger == 2 && darkvision->wiz_sorc == 2 &&
               darkvision->bard == kNoLevel && darkvision->cleric == kNoLevel && darkvision->paladin == kNoLevel,
           true);
  const ClassListRow* ray = ListSwitchRowOf(kRayOfEnfeeblement);  // the Wizard's (and the Warlock's)
  CHECK_EQ(ray != nullptr && ray->wiz_sorc == 2 && ray->change == ListSwitch::kMoved, true);
  CHECK_EQ(WizardOnlySpell(kRayOfEnfeeblement), true);
  // Retired: Doom, Bless Weapon ...
  CHECK_EQ(ListSwitchRowOf(46) != nullptr && ListSwitchRowOf(46)->change == ListSwitch::kRetired, true);
  CHECK_EQ(ListSwitchRowOf(537) != nullptr && ListSwitchRowOf(537)->change == ListSwitch::kRetired, true);
  // SRD 5.2.1's level 1 spells keep their builds' rows.
  for (int built : {10, kHideousLaughter, kProtectionFromEvilAndGood, kIdentify, kLongstrider, kFindFamiliar}) {
    CHECK_EQ(ListSwitchRowOf(built) == nullptr, true);
  }
  CHECK_EQ(ListLevel(*weapon, 6), kNoLevel);
  // Summon Creature I: off unless the house rule keeps it.
  CHECK_EQ(RetiredSummons().size(), 1u);
  CHECK_EQ(RetiredSummons()[0].row, 174);
  CHECK_EQ(ListSwitchRowOf(174) == &RetiredSummons()[0], true);
  for (int column = 0; column < 6; ++column) CHECK_EQ(ListLevel(RetiredSummons()[0], column), kNoLevel);
}

// NWN's one Wiz_Sorc column: the Wizard's alone of its spells (SRD 5.2.1) - the Sorcerer is answered no level for them.
TEST(WizardOnlyRowsSplit) {
  CHECK_EQ(WizardOnlyRows().size(), 10u);
  for (int row : {kIdentify, kRayOfEnfeeblement, kProtectionFromEvilAndGood, kHideousLaughter, kLongstrider, kAlarm,
                  kIllusoryScript, kUnseenServant, kFindFamiliar, kFloatingDisk}) {
    CHECK_EQ(WizardOnlySpell(row), true);
  }
  CHECK_EQ(WizardOnlySpell(544), false);  // Magic Weapon: the Sorcerer's too
  CHECK_EQ(WizardOnlySpell(kChromaticOrb), false);
}
