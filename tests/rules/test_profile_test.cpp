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
}
