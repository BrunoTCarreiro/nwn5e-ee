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
  // 0.58.8: the push slides' times.
  CHECK_EQ(ParseTestProfile("switches = push_slide_ms=1500, push_slide_delay_ms=0, shove_slide_ms=100\n")
               .errors.empty(),
           true);
  CHECK_EQ(ParseTestProfile("switches = push_slide_ms=99\n").errors.size(), 1u);
  CHECK_EQ(ParseTestProfile("switches = push_slide_delay_ms=501\n").errors.size(), 1u);
  CHECK_EQ(ParseTestProfile("switches = shove_slide_ms=1501\n").errors.size(), 1u);
  // #105: the clock's spot.
  CHECK_EQ(ParseTestProfile("switches = clock_x=0, clock_y=2000\n").errors.empty(), true);
  CHECK_EQ(ParseTestProfile("switches = clock_x=2001\n").errors.size(), 1u);
  CHECK_EQ(ParseTestProfile("switches = clock_y=-1\n").errors.size(), 1u);
}

// Breaks caught (#104): charm not read, the default time not 30 s, a time
// out of 1..120 taken, an unknown word or a second number taken, a value set
// on a refusal.
TEST(FearArguments) {
  bool charm = true;
  int seconds = 0;
  int hours = -1;  // #135: !time [hours]
  CHECK_EQ(ParseClockHours("", &hours), true);
  CHECK_EQ(hours, 0);
  CHECK_EQ(ParseClockHours("16", &hours), true);
  CHECK_EQ(hours, 16);
  CHECK_EQ(ParseClockHours(" +8 ", &hours), true);
  CHECK_EQ(hours, 8);
  CHECK_EQ(ParseClockHours("48", &hours), true);
  CHECK_EQ(ParseClockHours("49", &hours), false);
  CHECK_EQ(ParseClockHours("0", &hours), false);
  CHECK_EQ(ParseClockHours("-3", &hours), false);
  CHECK_EQ(ParseClockHours("8 9", &hours), false);
  CHECK_EQ(ParseClockHours("soon", &hours), false);
  CHECK_EQ(ParseClockHours("8x", &hours), false);
  int gold = 5;  // #38: !gold [amount]
  CHECK_EQ(ParseGoldAmount("", &gold), true);
  CHECK_EQ(gold, -1);
  CHECK_EQ(ParseGoldAmount(" 500 ", &gold), true);
  CHECK_EQ(gold, 500);
  CHECK_EQ(ParseGoldAmount("0", &gold), true);
  CHECK_EQ(gold, 0);
  CHECK_EQ(ParseGoldAmount("1000000", &gold), true);
  CHECK_EQ(gold, 1000000);
  CHECK_EQ(ParseGoldAmount("1000001", &gold), false);
  CHECK_EQ(ParseGoldAmount("-5", &gold), false);
  CHECK_EQ(ParseGoldAmount("5 6", &gold), false);
  CHECK_EQ(ParseGoldAmount("lots", &gold), false);
  CHECK_EQ(ParseGoldAmount("12345678", &gold), false);
  CHECK_EQ(ParseFearArgs("", &charm, &seconds), true);
  CHECK_EQ(charm, false);
  CHECK_EQ(seconds, 30);
  CHECK_EQ(ParseFearArgs("charm", &charm, &seconds), true);
  CHECK_EQ(charm, true);
  CHECK_EQ(seconds, 30);
  CHECK_EQ(ParseFearArgs("60 Charm", &charm, &seconds), true);
  CHECK_EQ(charm, true);
  CHECK_EQ(seconds, 60);
  CHECK_EQ(ParseFearArgs(" 120 ", &charm, &seconds), true);
  CHECK_EQ(charm, false);
  CHECK_EQ(seconds, 120);
  CHECK_EQ(ParseFearArgs("fear 1", &charm, &seconds), true);
  CHECK_EQ(seconds, 1);
  charm = true;
  seconds = 7;
  CHECK_EQ(ParseFearArgs("121", &charm, &seconds), false);
  CHECK_EQ(ParseFearArgs("0", &charm, &seconds), false);
  CHECK_EQ(ParseFearArgs("sleep", &charm, &seconds), false);
  CHECK_EQ(ParseFearArgs("10 20", &charm, &seconds), false);
  CHECK_EQ(ParseFearArgs("5s", &charm, &seconds), false);
  CHECK_EQ(charm, true);  // nothing set on a refusal
  CHECK_EQ(seconds, 7);
}

// Breaks caught (#103): a rebuilt character keeping its old items, a profile
// giving items not emptying first, a switches-only profile (rest, slides)
// emptying the inventory.
TEST(TestProfilesEmptyTheInventory) {
  CHECK_EQ(TestProfileEmptiesInventory(ParseTestProfile("swap_class = monk\n")), true);
  CHECK_EQ(TestProfileEmptiesInventory(ParseTestProfile("class = fighter\nlevel = 13\n")), true);
  CHECK_EQ(TestProfileEmptiesInventory(ParseTestProfile("levels = rogue 1\n")), true);
  CHECK_EQ(TestProfileEmptiesInventory(ParseTestProfile("items = nw_wswls001\n")), true);
  CHECK_EQ(TestProfileEmptiesInventory(ParseTestProfile("switches = rest_clock=1\nnote = the hour\n")), false);
  CHECK_EQ(TestProfileEmptiesInventory(ParseTestProfile("xp = 2700\n")), false);
  CHECK_EQ(TestProfileEmptiesInventory(ParseTestProfile("species = halfling\nabilities = 10 16 14 10 16 8\n")),
           false);
}

// Breaks caught (#96): a DC that isn't the last word, out of 1..60, not a
// number, a tool taken for a skill, a skill of several words refused, the
// skill or DC not set.
TEST(SkillCheckArguments) {
  Skill2024 skill = Skill2024::kArcana;
  int dc = 0;
  CHECK_EQ(ParseSkillCheckArgs("intimidation 15", &skill, &dc), true);
  CHECK_EQ(skill == Skill2024::kIntimidation, true);
  CHECK_EQ(dc, 15);
  CHECK_EQ(ParseSkillCheckArgs("  Sleight of Hand 12 ", &skill, &dc), true);
  CHECK_EQ(skill == Skill2024::kSleightOfHand, true);
  CHECK_EQ(dc, 12);
  CHECK_EQ(ParseSkillCheckArgs("perception 1", &skill, &dc), true);
  CHECK_EQ(dc, 1);
  CHECK_EQ(ParseSkillCheckArgs("stealth 60", &skill, &dc), true);
  CHECK_EQ(skill == Skill2024::kStealth, true);
  CHECK_EQ(dc, 60);
  skill = Skill2024::kArcana;
  dc = 7;
  CHECK_EQ(ParseSkillCheckArgs("stealth 61", &skill, &dc), false);
  CHECK_EQ(ParseSkillCheckArgs("stealth 0", &skill, &dc), false);
  CHECK_EQ(ParseSkillCheckArgs("stealth 1x", &skill, &dc), false);
  CHECK_EQ(ParseSkillCheckArgs("stealth -5", &skill, &dc), false);
  CHECK_EQ(ParseSkillCheckArgs("stealth", &skill, &dc), false);
  CHECK_EQ(ParseSkillCheckArgs("15 stealth", &skill, &dc), false);
  CHECK_EQ(ParseSkillCheckArgs("thieves' tools 10", &skill, &dc), false);  // a tool
  CHECK_EQ(ParseSkillCheckArgs("swimming 10", &skill, &dc), false);
  CHECK_EQ(ParseSkillCheckArgs("", &skill, &dc), false);
  CHECK_EQ(skill == Skill2024::kArcana, true);  // nothing set on a refusal
  CHECK_EQ(dc, 7);
}

// Breaks caught (0.57.6): a profile that builds a character not starting
// from level 1 (it would keep the levels it had), or one that only sets
// switches, notes, items or XP taking the character's levels away.
TEST(TestProfilesThatBuildStartFromLevelOne) {
  CHECK_EQ(TestProfileBuildsCharacter(ParseTestProfile("swap_class = monk\n")), true);
  CHECK_EQ(TestProfileBuildsCharacter(ParseTestProfile("class = fighter\nlevel = 13\n")), true);
  CHECK_EQ(TestProfileBuildsCharacter(ParseTestProfile("levels = rogue 1\n")), true);
  CHECK_EQ(TestProfileBuildsCharacter(ParseTestProfile("switches = rest_clock=1\nnote = the hour\n")), false);
  CHECK_EQ(TestProfileBuildsCharacter(ParseTestProfile("items = nw_wswls001\nxp = 2700\n")), false);
  CHECK_EQ(TestProfileBuildsCharacter(ParseTestProfile("species = halfling\nabilities = 10 16 14 10 16 8\n")), false);
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

// Breaks caught (#156): a name not lowered, a second word, a script character or a name past 20 let through,
// the prefix wrong.
TEST(CastMarks) {
  CHECK_EQ(CastMarkTag("field") == "n5test_cast_field", true);
  CHECK_EQ(CastMarkTag(" Hall ") == "n5test_cast_hall", true);
  CHECK_EQ(CastMarkTag("dark_2") == "n5test_cast_dark_2", true);
  CHECK_EQ(CastMarkTag(std::string(20, 'a')) == "n5test_cast_" + std::string(20, 'a'), true);
  CHECK_EQ(CastMarkTag(std::string(21, 'a')).empty(), true);
  CHECK_EQ(CastMarkTag("").empty(), true);
  CHECK_EQ(CastMarkTag("field hall").empty(), true);
  CHECK_EQ(CastMarkTag("dark\");").empty(), true);
  CHECK_EQ(CastMarkTag("ha-ll").empty(), true);
}

// Breaks caught (#169): a second spells line replacing the first, spells and spellbook mixed up, a name with no
// letters taken, spells taken on a profile that doesn't build a character (its lists aren't emptied or set), a name
// matched with its case, spaces or apostrophe.
TEST(TestProfileSpellsParse) {
  const TestProfile profile = ParseTestProfile(
      "swap_class = wizard\n"
      "spells = Minor Illusion, Hunter's Mark\n"
      "spells = Light\n"
      "spellbook = Alarm\n");
  CHECK_EQ(profile.errors.empty(), true);
  CHECK_EQ(profile.spells.size(), 3u);
  CHECK_EQ(profile.spellbook.size(), 1u);
  if (profile.spells.size() == 3u) {
    CHECK_EQ(profile.spells[1] == "Hunter's Mark", true);
    CHECK_EQ(profile.spells[2] == "Light", true);
  }
  CHECK_EQ(ParseTestProfile("swap_class = wizard\nspells = Light, !!!\n").errors.size(), 1u);
  CHECK_EQ(ParseTestProfile("spells = Light\n").errors.size(), 1u);  // no character built
  CHECK_EQ(ParseTestProfile("spellbook = Alarm\n").errors.size(), 1u);
  CHECK_EQ(ParseTestProfile("levels = cleric 2\nspells = Bless\n").errors.empty(), true);
  CHECK_EQ(SpellNameKey("Hunter's Mark") == "huntersmark", true);
  CHECK_EQ(SpellNameKey(" MINOR illusion ") == "minorillusion", true);
  CHECK_EQ(SpellNameKey("Blindness/Deafness") == SpellNameKey("blindness deafness"), true);
  CHECK_EQ(SpellNameKey("!!!").empty(), true);
}

namespace {

ProfileCaster TestWizard() {  // Wizard-like: 2 cantrips, 2 prepared, a Spellbook of 3, slots to level 1
  ProfileCaster c;
  c.top_level = 1;
  c.cantrips = 2;
  c.prepared = 2;
  c.book = 3;
  c.list = {{10, 0, "Light"},         {11, 0, "Minor Illusion"}, {12, 0, "Fire Bolt"}, {20, 1, "Magic Missile"},
            {21, 1, "Shield"},        {22, 1, "Alarm"},          {23, 1, "Sleep"},     {30, 3, "Fireball"}};
  return c;
}

}  // namespace

// Breaks caught (#169): a list filled past its count (cantrips, prepared, Spellbook), a spell of a level with no
// slots prepared, a Wizard's prepared spell left out of its Spellbook, a repeated name taking two places, a skip
// without its reason or the level, the spellbook names placed before the spells.
TEST(ProfileSpellsPicked) {
  std::vector<ProfileSpellSkipped> skipped;
  const std::vector<ProfileCasterLists> lists = PickProfileSpells(
      {TestWizard()},
      {"light", "Minor Illusion", "Fire Bolt", "Magic Missile", "magic missile", "Fireball", "Shield", "Sleep", "Wish"},
      {"Alarm", "Sleep", "Light"}, &skipped);
  CHECK_EQ(lists.size(), 1u);
  if (lists.size() != 1u) return;
  CHECK_EQ(lists[0].cantrips == std::vector<int>({10, 11}), true);
  CHECK_EQ(lists[0].prepared == std::vector<int>({20, 21}), true);
  CHECK_EQ(lists[0].book == std::vector<int>({20, 21, 22}), true);
  CHECK_EQ(skipped.size(), 6u);
  if (skipped.size() != 6u) return;
  CHECK_EQ(skipped[0].name == "Fire Bolt" && skipped[0].why == ProfileSpellSkip::kCantripsFull, true);
  CHECK_EQ(skipped[1].name == "Fireball" && skipped[1].why == ProfileSpellSkip::kTooHigh, true);
  CHECK_EQ(skipped[1].level, 3);
  CHECK_EQ(skipped[2].name == "Sleep" && skipped[2].why == ProfileSpellSkip::kPreparedFull, true);
  CHECK_EQ(skipped[3].name == "Wish" && skipped[3].why == ProfileSpellSkip::kNotOnList, true);
  CHECK_EQ(skipped[4].name == "Sleep" && skipped[4].why == ProfileSpellSkip::kBookFull, true);
  CHECK_EQ(skipped[5].name == "Light" && skipped[5].why == ProfileSpellSkip::kCantripInBook, true);
  // No skipped list: the same lists.
  CHECK_EQ(PickProfileSpells({TestWizard()}, {"Light"}, {}, nullptr)[0].cantrips == std::vector<int>({10}), true);
  // A Spellbook full before the prepared spells are: the spell isn't prepared either.
  ProfileCaster small_book = TestWizard();
  small_book.book = 1;
  std::vector<ProfileSpellSkipped> book_full;
  const std::vector<ProfileCasterLists> small =
      PickProfileSpells({small_book}, {"Magic Missile", "Shield"}, {}, &book_full);
  CHECK_EQ(small[0].prepared == std::vector<int>({20}), true);
  CHECK_EQ(book_full.size(), 1u);
  if (book_full.size() == 1u) CHECK_EQ(book_full[0].why == ProfileSpellSkip::kBookFull, true);
}

// Breaks caught (#169): a multiclass name not moving on to the next class with room, a name both lists have going to
// both, a skip given the last class's reason instead of the first's, a spellbook name given to a class with no
// Spellbook, the reasons for a book name (no Spellbook at all, not on the Wizard's list) mixed up.
TEST(ProfileSpellsMulticlass) {
  ProfileCaster wizard;
  wizard.top_level = 1;
  wizard.cantrips = 1;
  wizard.prepared = 1;
  wizard.book = 2;
  wizard.list = {{10, 0, "Light"}, {20, 1, "Magic Missile"}, {40, 1, "Detect Magic"}, {80, 1, "Silence"}};
  ProfileCaster cleric;
  cleric.top_level = 1;
  cleric.cantrips = 2;
  cleric.prepared = 2;
  cleric.list = {{10, 0, "Light"}, {50, 0, "Guidance"}, {60, 1, "Bless"}, {40, 1, "Detect Magic"}, {80, 2, "Silence"}};
  std::vector<ProfileSpellSkipped> skipped;
  const std::vector<ProfileCasterLists> lists = PickProfileSpells(
      {wizard, cleric}, {"Light", "Guidance", "Light", "Magic Missile", "Detect Magic", "Bless", "Silence"}, {"Bless"},
      &skipped);
  CHECK_EQ(lists.size(), 2u);
  if (lists.size() != 2u) return;
  CHECK_EQ(lists[0].cantrips == std::vector<int>({10}), true);
  CHECK_EQ(lists[0].prepared == std::vector<int>({20}), true);
  CHECK_EQ(lists[0].book == std::vector<int>({20}), true);
  CHECK_EQ(lists[1].cantrips == std::vector<int>({50}), true);
  CHECK_EQ(lists[1].prepared == std::vector<int>({40, 60}), true);  // Detect Magic: the Wizard's list was full
  CHECK_EQ(lists[1].book.empty(), true);
  CHECK_EQ(skipped.size(), 2u);
  if (skipped.size() == 2u) {
    // Silence: the first class's reason (the Wizard's list full), not the Cleric's (level 2).
    CHECK_EQ(skipped[0].name == "Silence" && skipped[0].why == ProfileSpellSkip::kPreparedFull, true);
    CHECK_EQ(skipped[1].why == ProfileSpellSkip::kNotOnList, true);  // the book's Bless: not a Wizard's
  }
  std::vector<ProfileSpellSkipped> no_book;
  PickProfileSpells({cleric}, {}, {"Bless"}, &no_book);
  CHECK_EQ(no_book.size(), 1u);
  if (no_book.size() == 1u) CHECK_EQ(no_book[0].why == ProfileSpellSkip::kNoSpellbook, true);
}

// Breaks caught (#170): away not read, or read inside another word; a word's case kept; ally not winning over
// neutral; an unknown word ignored instead of refused; a refused line setting anything; the creature turned the
// wrong way (facing you while away, or its back to you without it).
TEST(SpawnOptionsParse) {
  SpawnOptions o;
  CHECK_EQ(ParseSpawnOptions("", &o), true);
  CHECK_EQ(o.ally || o.neutral || o.reduction || o.away, false);
  CHECK_EQ(SpawnTurnDegrees(o), 180);  // facing you, as before
  CHECK_EQ(ParseSpawnOptions("AWAY", &o), true);
  CHECK_EQ(o.away, true);
  CHECK_EQ(SpawnTurnDegrees(o), 0);  // its back to you
  CHECK_EQ(ParseSpawnOptions("neutral away", &o), true);
  CHECK_EQ(o.neutral && o.away && !o.ally, true);
  CHECK_EQ(ParseSpawnOptions("ally neutral dr", &o), true);
  CHECK_EQ(o.ally && !o.neutral && o.reduction && !o.away, true);
  SpawnOptions kept;
  kept.away = true;
  CHECK_EQ(ParseSpawnOptions("away behind", &kept), false);  // an unknown word: refused
  CHECK_EQ(kept.away && !kept.ally, true);                     // nothing changed
  CHECK_EQ(ParseSpawnOptions("faraway", &o), false);           // a whole word only
  CHECK_EQ(ParseSpawnOptions("drake", &o), false);
}
