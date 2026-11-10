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

// The final review's finds (2026-11-10). A variant's own cast-effect timing must survive Save when the defaults have no
// cast effect (their timing isn't written, so the reader sees 0 there); the check for a row a look names must read the
// row's Type_FD, so a row the game doesn't serve shows in the log.

TEST(AnimLookVariantCastVfxSurvives) {
  data::SpellLook look;
  look.name = "x";
  look.rows = {1153, 1155};
  look.rule_at = 1.5f;
  look.release = 1.6f;
  look.parts.cast_vfx = 0;  // no shared cast effect, but timing left in memory from an edit
  look.parts.cast_vfx_at = 0.5f;
  look.parts.cast_vfx_for = 1.0f;
  data::LookParts own = look.parts;
  own.cast_vfx = 9;
  look.variants[1155] = own;
  data::SpellLook back;
  std::string error;
  CHECK_EQ(data::ReadSpellLook(data::WriteSpellLook(look), &back, &error), true);
  CHECK_EQ(data::LookPartsFor(back, 1155).cast_vfx, 9);
  CHECK_EQ(std::fabs(data::LookPartsFor(back, 1155).cast_vfx_at - 0.5f) < 0.001f, true);
  CHECK_EQ(std::fabs(data::LookPartsFor(back, 1155).cast_vfx_for - 1.0f) < 0.001f, true);
}

TEST(AnimLookRowCheckScript) {
  const std::string s = data::LookRowCheckScript(2100);
  CHECK_EQ(s.find("SetLocalString(OBJECT_SELF, \"n5look_row\", Get2DAString(\"visualeffects\", \"Type_FD\", 2100));") !=
               std::string::npos,
           true);
}

// Sound effects on a look's sounds (owner, 2026-11-10: "a way to add the effects we looked at earlier to each of the
// effects" - pitch, pitch shift, muffle, echo, reverb, volume; #154's proof) and a sound's voice (a chant moved from the
// spell row plays for the caster's own). Breaks caught: an effect lost or reordered on the way back (the JS twin writes
// the same), a value out of range or an unknown one passed to the player, a voice line played for both.

TEST(AnimLookSoundEffects) {
  const std::string head = "name = x\nrows = 1153\nrule_at = 1.5\nrelease = 1.6\n";
  data::SpellLook look;
  std::string error;
  CHECK_EQ(data::ReadSpellLook(head + "sound = hit 0 sim_x pitch=0.8 muffle=0.5 echo=0.3 reverb=0.4 shift=-3 volume=1.5\n"
                                      "sound = cast 0 vs_chant_evoc_lf voice=female\n",
                               &look, &error),
           true);
  const data::LookSound& s = look.parts.sounds[0];
  CHECK_EQ(std::fabs(s.pitch - 0.8f) < 1e-4f && std::fabs(s.muffle - 0.5f) < 1e-4f && std::fabs(s.echo - 0.3f) < 1e-4f, true);
  CHECK_EQ(std::fabs(s.reverb - 0.4f) < 1e-4f && std::fabs(s.shift + 3.0f) < 1e-4f && std::fabs(s.volume - 1.5f) < 1e-4f, true);
  CHECK_EQ(look.parts.sounds[1].voice == "female", true);
  const std::string text = data::WriteSpellLook(look);  // the canonical order: pitch shift muffle echo reverb volume voice
  CHECK_EQ(text.find("sound = hit 0 sim_x pitch=0.8 shift=-3 muffle=0.5 echo=0.3 reverb=0.4 volume=1.5\n") != std::string::npos, true);
  CHECK_EQ(text.find("sound = cast 0 vs_chant_evoc_lf voice=female\n") != std::string::npos, true);
  data::SpellLook back;
  CHECK_EQ(data::ReadSpellLook(text, &back, &error), true);
  CHECK_EQ(data::WriteSpellLook(back) == text, true);
  CHECK_EQ(data::ReadSpellLook(head + "sound = hit 0 sim_x pitch=9\n", &look, &error), false);  // out of range
  CHECK_EQ(data::ReadSpellLook(head + "sound = hit 0 sim_x wobble=1\n", &look, &error), false);  // unknown
  CHECK_EQ(data::ReadSpellLook(head + "sound = hit 0 sim_x voice=robot\n", &look, &error), false);
}

TEST(AnimLookVoiceInTheScript) {  // a voice line plays only for that voice (GetGender)
  data::LookParts p;
  p.sounds = {{data::LookAnchor::kCast, 0, "vs_chant_evoc_lf"}};
  p.sounds[0].voice = "female";
  const std::string s = data::LookCastScript(p);
  CHECK_EQ(s.find("if (GetGender(OBJECT_SELF) == GENDER_FEMALE) DelayCommand(0.00, PlaySound(\"vs_chant_evoc_lf\"));") !=
               std::string::npos,
           true);
}

// A conjure of one's own (owner, 2026-11-10: "can I create other conjure setups?"): a recipe swapped for the conjure
// (conjure1 for hand, conjure2 for head), as the clip is for the cast. Breaks caught: the conjure clip lost on the way
// back or written elsewhere than the JS twin writes it, a base that isn't a conjure.

TEST(AnimLookConjureClip) {
  const std::string head = "name = x\nrows = 1153\nrule_at = 1.5\nrelease = 1.6\n";
  data::SpellLook look;
  std::string error;
  CHECK_EQ(data::ReadSpellLook(head + "conj_clip = n5_cridle\nconj_clip_base = conjure2\n", &look, &error), true);
  CHECK_EQ(look.conj_clip == "n5_cridle" && look.conj_clip_base == "conjure2", true);
  const std::string text = data::WriteSpellLook(look);
  CHECK_EQ(text.find("rule_at = 1.5\nconj_clip = n5_cridle\nconj_clip_base = conjure2\nrelease = 1.6\n") != std::string::npos, true);
  data::SpellLook plain;
  CHECK_EQ(data::ReadSpellLook(head, &plain, &error), true);
  CHECK_EQ(plain.conj_clip.empty() && plain.conj_clip_base == "conjure1", true);
  CHECK_EQ(data::WriteSpellLook(plain).find("conj_clip") == std::string::npos, true);
  CHECK_EQ(data::ReadSpellLook(head + "conj_clip = n5_cridle\nconj_clip_base = jump\n", &look, &error), false);
}
