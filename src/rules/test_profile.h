#pragma once

// The `!test` profiles (test_commands.cpp, [debug] test_commands): a test
// setup per file, data\tests\<name>.txt, applied by `!test <name>` to the
// character who types it; `!test` alone lists them (#82). Lines
// "key = value" (# starts a comment, so no # in a name or a note):
//   name      = what the test is for (shown back)
//   species   = dwarf | elf | gnome | halfling | half-elf | half-orc (orc) | human
//   class     = barbarian | bard | cleric | druid | fighter | monk | paladin |
//               ranger | rogue | sorcerer | wizard
//   swap_class = one of those: the character's first class becomes it, its
//               level kept, the old class's automatic feats swapped for the
//               new one's (applied before class / level, which can then add
//               levels in it)
//   level     = the level to reach in that class (levels are only added)
//   xp        = experience points to have (only raised; no level-ups made —
//               the level-up screen makes them, for testing it)
//   abilities = six scores, Str Dex Con Int Wis Cha
//   feats     = a comma list of names !feat knows (alert, savage attacker,
//               mastery longsword, ...) or feat numbers
//   items     = a comma list of "<blueprint> [xN] [equip] [offhand]"
//   levels    = a comma list of "<class> <level>": more classes, each up to
//               its level (after class / level; another class multiclasses)
//   switches  = a comma list of "<switch>=<value>": test switches for this
//               session only (TestSwitchRange); every !test <name> first puts
//               back the ini's values, then sets these
//   note      = a line shown after the profile is applied (one per line)
// A profile that sets swap_class, class or levels builds a character: it
// starts from level 1 (TestProfileBuildsCharacter; 0.57.6, owner: "go with
// a"), so it gives the same character whatever the character was before.

#include <string>
#include <vector>

namespace rules {

struct TestItem {
  std::string blueprint;
  int stack = 1;
  bool equip = false;
  bool offhand = false;
};

struct TestLevels {
  int nwn_class = -1;  // classes.2da row
  int level = 0;
};

struct TestSwitch {
  std::string name;
  int value = 0;
};

struct TestProfile {
  std::string name;
  int race = -1;       // racialtypes.2da row (the base races 0..6)
  int nwn_class = -1;  // classes.2da row (the base classes 0..10)
  int swap_class = -1;  // classes.2da row: the first class becomes it
  int level = 0;
  int xp = 0;
  bool has_abilities = false;
  int abilities[6] = {0, 0, 0, 0, 0, 0};
  std::vector<std::string> feats;
  std::vector<TestItem> items;
  std::vector<TestLevels> levels;
  std::vector<TestSwitch> switches;
  std::vector<std::string> notes;
  std::vector<std::string> errors;  // lines that didn't parse
};

TestProfile ParseTestProfile(const std::string& text);

// A profile's name as `!test <name>` takes it - a file name, no path: 1..32
// letters, digits, '-' and '_'.
bool IsTestProfileName(const std::string& name);

// Does the profile build a character (swap_class, class or levels), and so
// start from level 1? A profile of switches, notes, items or xp only doesn't.
bool TestProfileBuildsCharacter(const TestProfile& profile);

// The switches a profile may set: the ones read each time they are used (one
// that installs a hook at start-up can't be set later). attack_trace 0..1,
// attacks_per_round 0..6, creature_hp_multiplier 0..100, creature_min_hp
// 0..30000, rest_clock 0..1; false for any other name.
bool TestSwitchRange(const std::string& name, int* low, int* high);

}  // namespace rules
