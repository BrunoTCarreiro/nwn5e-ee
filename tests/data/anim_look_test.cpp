#include <math.h>

#include <algorithm>
#include <cmath>
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

// The timing and the script text (Task 2). Breaks caught: a flight time off the game's Magic Missile curve, or one that
// goes negative or jumps near the curve's knee (under a metre); a release that goes negative when the rule runs late;
// a hit that doesn't wait one flight after the release; script text that applies the wrong row, at the wrong time, or
// sounds on the wrong creature.

TEST(AnimLookFlightCurve) {
  CHECK_EQ(std::fabs(data::MirvFlightSeconds(10) - 1.1226f) < 0.001f, true);
  CHECK_EQ(std::fabs(data::MirvFlightSeconds(5) - 0.7323f) < 0.001f, true);
  CHECK_EQ(std::fabs(data::MirvFlightSeconds(0.5f) - 0.5f) < 0.001f, true);  // under the curve's knee: d / 1
  CHECK_EQ(data::MirvFlightSeconds(0) == 0.0f, true);
  CHECK_EQ(std::fabs(data::MirvFlightSeconds(0.7165f) - 0.7165f) < 0.002f, true);  // continuous at the knee
}

TEST(AnimLookTiming) {
  data::SpellLook look;
  std::string error;
  data::ReadSpellLook(Fixture("chromatic_orb.look"), &look, &error);
  const data::LookTiming t = data::LookHitTiming(look, look.parts, 1.5f, 10);
  CHECK_EQ(std::fabs(t.release_in - 0.1f) < 0.001f, true);
  CHECK_EQ(std::fabs(t.hit_in - 1.2226f) < 0.001f, true);
  CHECK_EQ(t.late, false);
}

TEST(AnimLookLate) {
  data::SpellLook look;
  std::string error;
  data::ReadSpellLook(Fixture("chromatic_orb.look"), &look, &error);
  const data::LookTiming t = data::LookHitTiming(look, look.parts, 2.0f, 10);
  CHECK_EQ(t.release_in == 0.0f, true);
  CHECK_EQ(t.late, true);
  CHECK_EQ(std::fabs(t.hit_in - 1.1226f) < 0.001f, true);
}

TEST(AnimLookHitScript) {
  data::LookParts p;
  p.missile = 2100;
  p.hit = 61;
  p.sounds = {{data::LookAnchor::kRelease, 0, "sco_x"}, {data::LookAnchor::kHit, 0.05f, "sim_explflame"}};
  const std::string s = data::LookHitScript(p, 0x1a2b, {0.10f, 1.22f, false});
  CHECK_EQ(s.find("object t = StringToObject(\"1a2b\");") != std::string::npos, true);
  CHECK_EQ(s.find("DelayCommand(0.10, ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(2100), t));") !=
               std::string::npos,
           true);
  CHECK_EQ(s.find("DelayCommand(1.22, ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(61), t));") !=
               std::string::npos,
           true);
  CHECK_EQ(s.find("DelayCommand(0.10, PlaySound(\"sco_x\"));") != std::string::npos, true);
  CHECK_EQ(s.find("AssignCommand(t, DelayCommand(1.27, PlaySound(\"sim_explflame\")));") != std::string::npos, true);
  p.missile = 0;
  CHECK_EQ(data::LookHitScript(p, 0x1a2b, {0.10f, 0.10f, false}).find("EffectVisualEffect(2100)") == std::string::npos,
           true);
}

TEST(AnimLookLeapScript) {
  data::LookParts p;
  p.hit = 61;
  p.leap_beam = 210;
  const std::string s = data::LookLeapScript(p, 1.0f, 0x10, 0x11, 1.22f, 1.52f);
  CHECK_EQ(s.find("EffectBeam(210, StringToObject(\"10\"), BODY_NODE_CHEST)") != std::string::npos, true);
  CHECK_EQ(s.find("DelayCommand(1.22, ApplyEffectToObject(DURATION_TYPE_TEMPORARY,") != std::string::npos, true);
  CHECK_EQ(s.find("DelayCommand(1.52, ApplyEffectToObject(DURATION_TYPE_INSTANT, EffectVisualEffect(61)") !=
               std::string::npos,
           true);
}
