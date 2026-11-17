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
//   items     = a comma list of "<blueprint> [xN] [equip] [offhand]" (equip:
//               in the item's own slot - #109)
//   levels    = a comma list of "<class> <level>": more classes, each up to
//               its level (after class / level; another class multiclasses)
//   switches  = a comma list of "<switch>=<value>": test switches for this
//               session only (TestSwitchRange); every !test <name> first puts
//               back the ini's values, then sets these
//   note      = a line shown after the profile is applied (one per line)
//   spells    = a comma list of spell names, as the Prepare spells window
//               shows them (any case; punctuation and spaces ignored): a
//               built 2024 caster's cantrips and prepared spells - a
//               Wizard's in its Spellbook too - set instead of the window
//               opening (#169); more lines add to it
//   spellbook = a comma list of names: a Wizard's Spellbook only (#169)
// A profile that sets swap_class, class or levels builds a character: it
// starts from level 1 (TestProfileBuildsCharacter; 0.57.6, owner: "go with
// a"), so it gives the same character whatever the character was before.
// Spells need one (a profile that doesn't build a character keeps its lists).

#include <string>
#include <vector>

#include "rules/skills.h"

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
  std::vector<std::string> spells;     // #169: names, as written
  std::vector<std::string> spellbook;  // #169: a Wizard's Spellbook only
  std::vector<std::string> errors;  // lines that didn't parse
};

// #169 (owner, 2026-11-17: "is it possible for the !commands to give em a character that already has
// spellbok/cantrip/prepared spells ready?", then "make the changes, it'll make testing so MUCH easier"): a profile's
// spells sorted into a built caster's lists, as the Prepare spells window's Accept would hold them.
struct ProfileListSpell {  // an entry of a class's list (spells.2da row, its level for the class, its name)
  int id = 0;
  int level = 0;
  std::string name;
};
struct ProfileCaster {  // one of the character's 2024 casting classes
  int top_level = 0;    // the highest spell level it has slots for (0: cantrips only)
  int cantrips = 0;     // its counts: cantrips, prepared spells
  int prepared = 0;
  int book = 0;         // a Wizard's Spellbook (0: the class has none)
  std::vector<ProfileListSpell> list;
};
struct ProfileCasterLists {  // spells.2da rows, in the profile's order
  std::vector<int> cantrips;
  std::vector<int> prepared;
  std::vector<int> book;  // a Wizard's: its prepared spells and its `spellbook` names
};
enum class ProfileSpellSkip {
  kNotOnList,      // on none of the classes' lists
  kTooHigh,        // a level the class has no slots for yet
  kCantripsFull,
  kPreparedFull,
  kBookFull,
  kNoSpellbook,    // a `spellbook` name, and no class with a Spellbook
  kCantripInBook,  // a `spellbook` name that is a cantrip
};
struct ProfileSpellSkipped {
  std::string name;  // as the profile wrote it
  ProfileSpellSkip why = ProfileSpellSkip::kNotOnList;
  int level = 0;     // kTooHigh: the spell's level
};

// A name as the lists are searched: lower case, letters and digits only ("Hunter's Mark" = "hunters mark").
std::string SpellNameKey(const std::string& name);

// Each `spells` name, in order, goes to the first class whose list has it and whose list has room - a cantrip to its
// cantrips, another to its prepared spells (and a Spellbook's, if the class has one and it isn't there). Then each
// `spellbook` name to the first class with a Spellbook whose list has it, below its count. A name placed already is
// passed over; a name that can't go anywhere is skipped, with the first class's reason (`skipped`, may be null).
// One result per caster, in order.
std::vector<ProfileCasterLists> PickProfileSpells(const std::vector<ProfileCaster>& casters,
                                                  const std::vector<std::string>& spells,
                                                  const std::vector<std::string>& spellbook,
                                                  std::vector<ProfileSpellSkipped>* skipped);

TestProfile ParseTestProfile(const std::string& text);

// A profile's name as `!test <name>` takes it - a file name, no path: 1..32
// letters, digits, '-' and '_'.
bool IsTestProfileName(const std::string& name);

// Does the profile build a character (swap_class, class or levels), and so
// start from level 1? A profile of switches, notes, items or xp only doesn't.
bool TestProfileBuildsCharacter(const TestProfile& profile);

// #103 (owner, 2026-10-31: "it should be emptied and only filled up with
// what's necessary, otherwise the characters will end up with multiple
// duplicates of the same items and very encumbered"): does the profile empty
// the inventory before its items? One that builds a character or gives items
// does; switches, notes, xp or scores only don't.
bool TestProfileEmptiesInventory(const TestProfile& profile);

// The switches a profile may set: the ones read each time they are used (one
// that installs a hook at start-up can't be set later). attack_trace 0..1,
// attacks_per_round 0..6, creature_hp_multiplier 0..100, creature_min_hp
// 0..30000, flask_patches 0..1 (#130's house rule), rest_clock 0..1; the push slides' times (#11, 0.58.8;
// rules/push_slide.h) push_slide_delay_ms 0..500, push_slide_ms 100..1500,
// shove_slide_ms 100..1500; the HUD clock's spot (#105; rules/hud_clock.h)
// clock_x 0..2000, clock_y 0..2000; false for any other name.
bool TestSwitchRange(const std::string& name, int* low, int* high);

// #96 (owner, 2026-10-31: "add !skill"): "!skill <skill> <dc>"'s arguments - a
// 2024 skill (any case; more words allowed: "sleight of hand 12") and a DC
// 1..60 as the last word. False (nothing set) for a tool, an unknown skill, no
// DC or one out of range.
bool ParseSkillCheckArgs(const std::string& text, Skill2024* skill, int* dc);

// #104 (owner, 2026-10-31: "add !fear"): "!fear [charm] [seconds]"'s
// arguments, in any order - "charm" (or "fear", the default) and seconds
// 1..kTestFearMaxSeconds (else kTestFearSeconds). False (nothing set) for any
// other word, a second number or a time out of range.
constexpr int kTestFearSeconds = 30;
constexpr int kTestFearMaxSeconds = 120;
bool ParseFearArgs(const std::string& text, bool* charm, int* seconds);
// #135: "!time [hours]" - nothing (0: say the time) or a number of hours to move the clock, 1 to
// kTestClockMaxHours, "+16" as "16". False for anything else.
constexpr int kTestClockMaxHours = 48;
bool ParseClockHours(const std::string& text, int* hours);
// #38: "!gold [amount]" - nothing (-1: say the gold) or the gold to have, 0 to kTestGoldMax. False for anything
// else.
constexpr int kTestGoldMax = 1000000;
bool ParseGoldAmount(const std::string& text, int* gold);
// #156 (owner, 2026-11-10: "yes, open the issue and pass it to animation"): the Test Grounds (n5test.mod) mark
// each area's spot with a waypoint tagged kCastMarkPrefix + a name (field, hall, dark). "!go <name>"'s argument:
// one word of letters, digits and '_' (any case, read lower), at most kCastMarkNameMax, so the tag fits NWN's 32.
// The tag, or "" for anything else (nothing else reaches the script).
constexpr const char* kCastMarkPrefix = "n5test_cast_";
constexpr size_t kCastMarkNameMax = 20;
std::string CastMarkTag(const std::string& text);

}  // namespace rules
