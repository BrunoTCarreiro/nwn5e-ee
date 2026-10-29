#include "check.h"
#include "rules/test_profile.h"

using namespace rules;

// Breaks caught: a key or value misread (a halfling Fighter 5 becoming
// something else), an item's stack or equip flag lost, a bad line applied
// silently instead of reported, comments not ignored.
TEST(TestProfilesParse) {
  const TestProfile profile = ParseTestProfile(
      "# a comment\n"
      "name = halfling Fighter 5\n"
      "species = Halfling\n"
      "class = fighter   # trailing comment\n"
      "level = 5\n"
      "abilities = 16 14 14 8 12 10\n"
      "feats = savage attacker, mastery longsword\n"
      "items = nw_wswls001 equip, nw_ashto001 offhand, nw_wamar001 x20\n");
  CHECK_EQ(profile.errors.empty(), true);
  CHECK_EQ(profile.race, 3);
  CHECK_EQ(profile.nwn_class, 4);
  CHECK_EQ(profile.level, 5);
  CHECK_EQ(profile.has_abilities, true);
  CHECK_EQ(profile.abilities[0], 16);
  CHECK_EQ(profile.abilities[5], 10);
  CHECK_EQ(profile.feats.size(), 2u);
  CHECK_EQ(profile.items.size(), 3u);
  CHECK_EQ(profile.items[0].equip, true);
  CHECK_EQ(profile.items[1].offhand, true);
  CHECK_EQ(profile.items[2].stack, 20);
  CHECK_EQ(profile.items[2].equip, false);

  const TestProfile bad = ParseTestProfile("species = goblin\nlevel = 3\nabilities = 10 10\nitems = bad item!\n");
  CHECK_EQ(bad.errors.size(), 4u);  // the species, the abilities, the item, a level without a class
  CHECK_EQ(ParseTestProfile("species = orc\n").race, 5);
  CHECK_EQ(ParseTestProfile("swap_class = barbarian\n").swap_class, 0);  // the first class swapped
  CHECK_EQ(ParseTestProfile("swap_class = barbarian\n").errors.empty(), true);
  CHECK_EQ(ParseTestProfile("swap_class = goblin\n").errors.size(), 1u);
  CHECK_EQ(ParseTestProfile("xp = 2700\n").xp, 2700);  // experience only: no class needed
  CHECK_EQ(ParseTestProfile("xp = 2700\n").errors.empty(), true);
  CHECK_EQ(ParseTestProfile("xp = -5\n").errors.size(), 1u);
}

// Breaks caught (#82): a second class or its level misread, a switch out of
// its range or one a profile can't set taken, a note lost, a bad part applied
// silently instead of reported.
TEST(TestProfilesLevelsSwitchesNotes) {
  const TestProfile profile = ParseTestProfile(
      "swap_class = fighter\n"
      "levels = Rogue 1, wizard 2\n"
      "switches = attack_trace=1, attacks_per_round = 3, creature_min_hp=50\n"
      "note = start from a new level 1 character\n"
      "note = then level up\n");
  CHECK_EQ(profile.errors.empty(), true);
  CHECK_EQ(profile.levels.size(), 2u);
  if (profile.levels.size() == 2u) {
    CHECK_EQ(profile.levels[0].nwn_class, 8);  // rogue
    CHECK_EQ(profile.levels[0].level, 1);
    CHECK_EQ(profile.levels[1].nwn_class, 10);  // wizard
    CHECK_EQ(profile.levels[1].level, 2);
  }
  CHECK_EQ(profile.switches.size(), 3u);
  if (profile.switches.size() == 3u) {
    CHECK_EQ(profile.switches[0].name == "attack_trace", true);
    CHECK_EQ(profile.switches[0].value, 1);
    CHECK_EQ(profile.switches[1].name == "attacks_per_round", true);
    CHECK_EQ(profile.switches[1].value, 3);
    CHECK_EQ(profile.switches[2].name == "creature_min_hp", true);
    CHECK_EQ(profile.switches[2].value, 50);
  }
  CHECK_EQ(profile.notes.size(), 2u);
  if (profile.notes.size() == 2u) CHECK_EQ(profile.notes[1] == "then level up", true);

  CHECK_EQ(ParseTestProfile("levels = rogue 20\n").errors.empty(), true);
  CHECK_EQ(ParseTestProfile("levels = rogue 0\n").errors.size(), 1u);  // a level is 1..20
  CHECK_EQ(ParseTestProfile("levels = rogue 21\n").errors.size(), 1u);
  CHECK_EQ(ParseTestProfile("levels = goblin 2\n").errors.size(), 1u);
  CHECK_EQ(ParseTestProfile("levels = rogue 2 3\n").errors.size(), 1u);
  CHECK_EQ(ParseTestProfile("levels = rogue\n").errors.size(), 1u);
  CHECK_EQ(ParseTestProfile("switches = attacks_per_round=6\n").errors.empty(), true);
  CHECK_EQ(ParseTestProfile("switches = attacks_per_round=7\n").errors.size(), 1u);  // out of its range
  CHECK_EQ(ParseTestProfile("switches = rest_clock=2\n").errors.size(), 1u);
  CHECK_EQ(ParseTestProfile("switches = every_hit_crits=1\n").errors.size(), 1u);  // start-up only: the ini's
  CHECK_EQ(ParseTestProfile("switches = attack_trace\n").errors.size(), 1u);       // no value
  CHECK_EQ(ParseTestProfile("switches = creature_min_hp=-5\n").errors.size(), 1u);
  CHECK_EQ(ParseTestProfile("switches = creature_min_hp=30000\n").errors.empty(), true);
  CHECK_EQ(ParseTestProfile("switches = creature_min_hp=30001\n").errors.size(), 1u);
  CHECK_EQ(ParseTestProfile("switches = creature_hp_multiplier=100, rest_clock=1\n").switches.size(), 2u);
}

// Breaks caught (#82): a path, a dot or a long name let through into
// data\tests\<name>.txt.
TEST(TestProfileNames) {
  CHECK_EQ(IsTestProfileName("monk"), true);
  CHECK_EQ(IsTestProfileName("small-monk_2"), true);
  CHECK_EQ(IsTestProfileName(std::string(32, 'a')), true);
  CHECK_EQ(IsTestProfileName(""), false);
  CHECK_EQ(IsTestProfileName("..\\nwn5e"), false);
  CHECK_EQ(IsTestProfileName("tests/monk"), false);
  CHECK_EQ(IsTestProfileName("monk.txt"), false);
  CHECK_EQ(IsTestProfileName("monk part"), false);
  CHECK_EQ(IsTestProfileName(std::string(33, 'a')), false);
}
