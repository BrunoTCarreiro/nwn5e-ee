#include <math.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "data/anim_look.h"

// Spell looks (data/anim_look.h): what the Spell & Attack Bench saves and the game plays. Breaks caught: a look that
// loses a key or a variant on the way back, or comes back in another form than the bench's JS writes (the fixture is
// the shared canonical text); a section that doesn't start from the defaults or keeps their sounds; a value that would
// break the script text it becomes (a quote, a long resref) or a table column that isn't presentation; a release before
// the rule runs.

namespace {

std::string Fixture(const char* name) {
  std::ifstream file(std::string("tests/data/fixtures/") + name, std::ios::binary);
  std::stringstream read;
  read << file.rdbuf();
  std::string text = read.str();
  text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());  // a checkout may give it CRLFs
  return text;
}

}  // namespace

TEST(AnimLookReadsTheFixture) {
  data::SpellLook look;
  std::string error;
  CHECK_EQ(data::ReadSpellLook(Fixture("chromatic_orb.look"), &look, &error), true);
  CHECK_EQ(look.name == "chromatic_orb", true);
  CHECK_EQ(look.rows.size(), 7);
  CHECK_EQ(data::LookPartsFor(look, 1154).missile, 2100);  // no section: the defaults
  CHECK_EQ(data::LookPartsFor(look, 1155).missile, 2101);
  CHECK_EQ(data::LookPartsFor(look, 1155).hit, 63);
  CHECK_EQ(data::LookPartsFor(look, 1155).sounds.size(), 1);  // a section's sounds replace the defaults'
  CHECK_EQ(data::LookPartsFor(look, 1155).table.at("ConjAnim") == "hand", true);  // copied from the defaults
}

TEST(AnimLookWritesTheFixtureBack) {  // the canonical form; the JS twin must match it too
  data::SpellLook look;
  std::string error;
  const std::string text = Fixture("chromatic_orb.look");
  CHECK_EQ(text.empty(), false);
  CHECK_EQ(data::ReadSpellLook(text, &look, &error), true);
  CHECK_EQ(data::WriteSpellLook(look) == text, true);
}

TEST(AnimLookRefuses) {
  const std::string head = "name = x\nrows = 1153\nrule_at = 1.5\nrelease = 1.6\n";
  data::SpellLook look;
  std::string error;
  CHECK_EQ(data::ReadSpellLook(head + "sound = hit 0 sim\"x\n", &look, &error), false);             // a quote
  CHECK_EQ(data::ReadSpellLook(head + "sound = hit 0 abcdefghijklmnopq\n", &look, &error), false);  // 17 characters
  CHECK_EQ(data::ReadSpellLook(head + "table.CastTime = 500\n", &look, &error), false);
  CHECK_EQ(error.find("CastTime") != std::string::npos, true);
  CHECK_EQ(data::ReadSpellLook("name = x\nrows = 1153\nrule_at = 1.5\nrelease = 1.2\n", &look, &error), false);  // before the rule
  CHECK_EQ(data::ReadSpellLook("name = x\nrule_at = 1\nrelease = 1\n", &look, &error), false);  // no rows
  CHECK_EQ(data::ReadSpellLook(head + "[1155]\nrelease = 2\n", &look, &error), false);  // can't vary
  CHECK_EQ(error.find("line 6") != std::string::npos, true);
}
