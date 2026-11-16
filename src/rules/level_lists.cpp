#include "rules/level_lists.h"

namespace rules {

const std::vector<ClassListRow>& Level1ListSwitch() {
  // Retired: no SRD 5.2.1 version (Endure Elements, Resist Elements and Lesser Dispel fold into Protection from Energy
  // and Dispel Magic's upcasting; Camouflage and Shelgarn's Persistent Blade are only like Pass without Trace and
  // Spiritual Weapon) - off every list. Moved: SRD 5.2.1's level 2 - Ray of Enfeeblement the Wizard's (and the
  // Warlock's), Darkvision (NWN's Ultravision) the Druid's, Ranger's, Sorcerer's and Wizard's, Magic Weapon the
  // Paladin's, Ranger's, Sorcerer's and Wizard's; their scripts NWN's until level 2's pass.
  static const std::vector<ClassListRow> kRows{
      {46, "Doom", ListSwitch::kRetired, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel},
      {50, "Endure Elements", ListSwitch::kRetired, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel},
      {94, "Lesser Dispel", ListSwitch::kRetired, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel},
      {143, "Ray of Enfeeblement", ListSwitch::kMoved, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, 2},
      {148, "Remove Fear", ListSwitch::kRetired, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel},
      {150, "Resist Elements", ListSwitch::kRetired, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel},
      {155, "Scare", ListSwitch::kRetired, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel},
      {365, "Darkvision (NWN's Ultravision)", ListSwitch::kMoved, kNoLevel, kNoLevel, 2, kNoLevel, 2, 2},
      {371, "Negative Energy Ray", ListSwitch::kRetired, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel},
      {418, "Entropic Shield", ListSwitch::kRetired, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel},
      {421, "Camouflage", ListSwitch::kRetired, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel},
      {442, "Amplify", ListSwitch::kRetired, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel},
      {452, "Magic Fang", ListSwitch::kRetired, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel},
      {521, "Horizikaul's Boom", ListSwitch::kRetired, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel},
      {522, "Ironguts", ListSwitch::kRetired, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel},
      {534, "Shelgarn's Persistent Blade", ListSwitch::kRetired, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel},
      {536, "Deafening Clang", ListSwitch::kRetired, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel},
      {537, "Bless Weapon", ListSwitch::kRetired, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel},
      {544, "Magic Weapon", ListSwitch::kMoved, kNoLevel, kNoLevel, kNoLevel, 2, 2, 2},
  };
  return kRows;
}

const std::vector<ClassListRow>& RetiredSummons() {
  static const std::vector<ClassListRow> kRows{
      {174, "Summon Creature I", ListSwitch::kRetired, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel, kNoLevel},
  };
  return kRows;
}

const ClassListRow* ListSwitchRowOf(int row) {
  for (const std::vector<ClassListRow>* table : {&Level1ListSwitch(), &RetiredSummons()}) {
    for (const ClassListRow& entry : *table) {
      if (entry.row == row) return &entry;
    }
  }
  return nullptr;
}

int ListLevel(const ClassListRow& row, int column) {
  switch (column) {
    case 0: return row.bard;
    case 1: return row.cleric;
    case 2: return row.druid;
    case 3: return row.paladin;
    case 4: return row.ranger;
    case 5: return row.wiz_sorc;
    default: return kNoLevel;
  }
}

}  // namespace rules
