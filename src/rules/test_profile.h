#pragma once

// The `!test` profile (test_commands.cpp, [debug] test_commands): a test
// setup in data\test.txt, applied to the character who types !test. Lines
// "key = value" (# starts a comment):
//   name      = what the test is for (shown back)
//   species   = dwarf | elf | gnome | halfling | half-elf | half-orc (orc) | human
//   class     = barbarian | bard | cleric | druid | fighter | monk | paladin |
//               ranger | rogue | sorcerer | wizard
//   level     = the level to reach in that class (levels are only added)
//   abilities = six scores, Str Dex Con Int Wis Cha
//   feats     = a comma list of names !feat knows (alert, savage attacker,
//               mastery longsword, ...) or feat numbers
//   items     = a comma list of "<blueprint> [xN] [equip] [offhand]"

#include <string>
#include <vector>

namespace rules {

struct TestItem {
  std::string blueprint;
  int stack = 1;
  bool equip = false;
  bool offhand = false;
};

struct TestProfile {
  std::string name;
  int race = -1;       // racialtypes.2da row (the base races 0..6)
  int nwn_class = -1;  // classes.2da row (the base classes 0..10)
  int level = 0;
  bool has_abilities = false;
  int abilities[6] = {0, 0, 0, 0, 0, 0};
  std::vector<std::string> feats;
  std::vector<TestItem> items;
  std::vector<std::string> errors;  // lines that didn't parse
};

TestProfile ParseTestProfile(const std::string& text);

}  // namespace rules
