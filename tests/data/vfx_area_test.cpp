#include "check.h"
#include "data/vfx_area.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

using namespace data;

namespace {

// A fixture's text, its line ends as LF (a Windows checkout may have given it CRLF).
std::string Fixture(const char* name) {
  std::ifstream file(std::string("tests/data/fixtures/") + name, std::ios::binary);
  std::ostringstream text;
  text << file.rdbuf();
  std::string out;
  for (char c : text.str())
    if (c != '\r') out += c;
  return out;
}

std::string Col(const std::vector<std::pair<std::string, std::string>>& columns, const std::string& name) {
  for (const auto& c : columns)
    if (c.first == name) return c.second;
  return "(missing)";
}

}  // namespace

// A column's value, named in the failure with what it was.
#define CHECK_COL(columns, name, want)                                                                \
  do {                                                                                                \
    const std::string check_got_ = Col((columns), (name));                                            \
    if (check_got_ != std::string(want)) {                                                            \
      const std::string check_msg_ = std::string(name) + " is " + check_got_ + ", want " + (want);    \
      check::FailText(__FILE__, __LINE__, check_msg_.c_str());                                        \
    }                                                                                                 \
  } while (0)

// #161: an area's vfx_persistent.2da columns (the dev layer's !vfxarea preview row; the VFX Bench's saved areas).
TEST(VfxAreaRowCircle) {
  VfxArea a;
  a.shape = 'C';
  a.radius = 3.0f;
  a.slots[0].model = "n5va0001";
  a.slots[0].count = 5;
  a.slots[0].lifetime = 2.0f;
  a.slots[0].edge = 0.5f;
  const auto c = VfxAreaRowColumns(a);
  CHECK_COL(c, "SHAPE", "C");
  CHECK_COL(c, "RADIUS", "3");
  CHECK_COL(c, "WIDTH", "****");
  CHECK_COL(c, "LENGTH", "****");
  CHECK_COL(c, "MODEL01", "n5va0001");
  CHECK_COL(c, "NUMACT01", "5");
  CHECK_COL(c, "DURATION01", "2000");  // ms in the row
  CHECK_COL(c, "EDGEWGHT01", "0.5");
  CHECK_COL(c, "MODELMIN01", "****");
  for (const char* blank : {"MODEL02", "NUMACT02", "DURATION02", "EDGEWGHT02", "MODEL03", "NUMACT03", "DURATION03", "EDGEWGHT03",
                            "MODELMIN02", "MODELMIN03", "DurationVFX", "SoundImpact", "SoundDuration", "SoundCessation",
                            "SoundOneShot", "SoundOneShotPercentage"}) {
    CHECK_COL(c, blank, "****");
  }
  CHECK_COL(c, "OrientWithGround", "0");
}

TEST(VfxAreaRowRectangle) {
  VfxArea a;
  a.shape = 'R';
  a.width = 10.0f;
  a.length = 2.0f;
  a.slots[1].model = "vps_wallfire";
  a.slots[1].count = 6;
  a.slots[1].low_model = "vps_wallfire_L";
  const auto c = VfxAreaRowColumns(a);
  CHECK_COL(c, "SHAPE", "R");
  CHECK_COL(c, "RADIUS", "****");
  CHECK_COL(c, "WIDTH", "10");
  CHECK_COL(c, "LENGTH", "2");
  CHECK_COL(c, "MODEL01", "****");
  CHECK_COL(c, "MODEL02", "vps_wallfire");
  CHECK_COL(c, "NUMACT02", "6");
  CHECK_COL(c, "DURATION02", "0");
  CHECK_COL(c, "MODELMIN02", "vps_wallfire_L");
}

// The game divides by (int)(size x 100) when it places a copy: a tiny area would crash it. Out-of-range values are
// clamped, not refused (as #159's twist).
TEST(VfxAreaRowClamps) {
  VfxArea a;
  a.radius = 0.2f;
  a.slots[0] = {"m1", 0, -1.0f, -1.0f, ""};
  a.slots[2] = {"m3", 99, 99.0f, 2.0f, ""};
  auto c = VfxAreaRowColumns(a);
  CHECK_COL(c, "RADIUS", "0.6");
  CHECK_COL(c, "NUMACT01", "1");
  CHECK_COL(c, "DURATION01", "0");
  CHECK_COL(c, "EDGEWGHT01", "0");
  CHECK_COL(c, "NUMACT03", "30");
  CHECK_COL(c, "DURATION03", "60000");
  CHECK_COL(c, "EDGEWGHT03", "1");
  a.radius = 99.0f;
  CHECK_COL(VfxAreaRowColumns(a), "RADIUS", "30");
  a.radius = std::nanf("");
  CHECK_COL(VfxAreaRowColumns(a), "RADIUS", "0.6");
  a.shape = 'R';
  a.width = 0.01f;
  a.length = 50.0f;
  c = VfxAreaRowColumns(a);
  CHECK_COL(c, "WIDTH", "0.05");
  CHECK_COL(c, "LENGTH", "30");
  a.shape = 'x';  // anything but R is a circle
  CHECK_COL(VfxAreaRowColumns(a), "SHAPE", "C");
}

// Web's and Darkness's kind: no models, an effect for the whole duration; sounds and the low-detail flag as given.
TEST(VfxAreaRowNoModels) {
  VfxArea a;
  a.radius = 6.7f;
  a.duration_effect = "230";
  a.sound_duration = "sps_darkness";
  a.sound_oneshot = "sps_fog";
  a.oneshot_chance = 0.3f;
  a.orient_ground = true;
  const auto c = VfxAreaRowColumns(a);
  CHECK_COL(c, "RADIUS", "6.7");
  CHECK_COL(c, "MODEL01", "****");
  CHECK_COL(c, "NUMACT01", "****");
  CHECK_COL(c, "DurationVFX", "230");
  CHECK_COL(c, "SoundDuration", "sps_darkness");
  CHECK_COL(c, "SoundOneShot", "sps_fog");
  CHECK_COL(c, "SoundOneShotPercentage", "0.3");
  CHECK_COL(c, "OrientWithGround", "1");
}

// --- The area draft (.area; Task 2): the VFX Bench's area-mode twin (vfx_bench/public/area.js) reads and writes the same
// text byte for byte (its check_area.js reads this fixture too).

#define CHECK_TEXT(got, want)                                                                         \
  do {                                                                                                \
    const std::string check_got_ = (got), check_want_ = (want);                                       \
    if (check_got_ != check_want_) {                                                                  \
      const std::string check_msg_ = "got [" + check_got_ + "] want [" + check_want_ + "]";          \
      check::FailText(__FILE__, __LINE__, check_msg_.c_str());                                        \
    }                                                                                                 \
  } while (0)

TEST(VfxAreaDraftRoundTrip) {
  const std::string text = Fixture("fog_area.area");
  VfxArea a;
  std::string err;
  CHECK_EQ(ReadVfxAreaDraft(text, &a, &err), true);
  CHECK_TEXT(err, "");
  CHECK_TEXT(a.name, "n5_fogcloud");
  CHECK_EQ(a.shape == 'C', true);
  CHECK_EQ(a.radius == 6.0f, true);
  CHECK_TEXT(a.slots[0].model, "n5_fog");
  CHECK_EQ(a.slots[0].count, 12);
  CHECK_EQ(std::fabs(a.slots[0].lifetime - 3.4f) < 1e-6f, true);
  CHECK_EQ(std::fabs(a.slots[0].edge - 0.25f) < 1e-6f, true);
  CHECK_TEXT(a.slots[0].low_model, "n5_fog_l");
  CHECK_TEXT(a.slots[1].model, "n5_fogwisp");
  CHECK_TEXT(a.slots[1].low_model, "");
  CHECK_TEXT(a.slots[2].model, "");
  CHECK_TEXT(a.duration_effect, "230");
  CHECK_TEXT(a.sound_duration, "sps_fog_loop");
  CHECK_TEXT(a.sound_oneshot, "sps_fog");
  CHECK_EQ(std::fabs(a.oneshot_chance - 0.3f) < 1e-6f, true);
  CHECK_EQ(a.orient_ground, true);
  CHECK_TEXT(WriteVfxAreaDraft(a), text);
  const auto c = VfxAreaRowColumns(a);
  CHECK_COL(c, "RADIUS", "6");
  CHECK_COL(c, "NUMACT01", "12");
  CHECK_COL(c, "DURATION01", "3400");
  CHECK_COL(c, "EDGEWGHT01", "0.25");
  CHECK_COL(c, "MODELMIN01", "n5_fog_l");
  CHECK_COL(c, "MODEL02", "n5_fogwisp");
  CHECK_COL(c, "DURATION02", "1100");
  CHECK_COL(c, "MODELMIN02", "****");
  CHECK_COL(c, "SoundOneShotPercentage", "0.3");
}

// A rectangle writes its width and length, not a radius; the shape line only for R. A minimal draft is three lines.
TEST(VfxAreaDraftShapes) {
  const std::string header = Fixture("fog_area.area").substr(0, Fixture("fog_area.area").find('\n') + 1);
  const std::string rect = header + "area = n5_wall\nshape = R\nwidth = 10\nlength = 2\nslot 3 = vps_wallfire 6 0 0.5\n";
  VfxArea a;
  std::string err;
  CHECK_EQ(ReadVfxAreaDraft(rect, &a, &err), true);
  CHECK_EQ(a.shape == 'R', true);
  CHECK_EQ(a.width == 10.0f && a.length == 2.0f, true);
  CHECK_TEXT(a.slots[2].model, "vps_wallfire");
  CHECK_TEXT(WriteVfxAreaDraft(a), rect);
  const std::string minimal = header + "area = n5_a\nradius = 3\nslot 1 = n5_m 5 0 0\n";
  VfxArea m;
  CHECK_EQ(ReadVfxAreaDraft(minimal, &m, &err), true);
  CHECK_TEXT(WriteVfxAreaDraft(m), minimal);
}

// Out of range in a draft: clamped as the row is (not refused), and written back clamped.
TEST(VfxAreaDraftClamps) {
  const std::string header = Fixture("fog_area.area").substr(0, Fixture("fog_area.area").find('\n') + 1);
  VfxArea a;
  std::string err;
  CHECK_EQ(ReadVfxAreaDraft(header + "area = n5_a\nradius = 0.1\nslot 1 = n5_m 99 99 2\nsound_oneshot = sps_x 5\n", &a, &err), true);
  CHECK_TEXT(WriteVfxAreaDraft(a), header + "area = n5_a\nradius = 0.6\nslot 1 = n5_m 30 60 1\nsound_oneshot = sps_x 1\n");
}

// A malformed line is refused, named by its number: a bad number, an unknown key, a slot past 3, no '=', a bad name,
// no area name at all.
TEST(VfxAreaDraftBadLineRefused) {
  struct Case {
    const char* text;
    const char* says;
  };
  const Case cases[] = {
      {"area = n5_a\nradius = x\n", "line 2"},
      {"area = n5_a\ncolour = 1\n", "line 2"},
      {"area = n5_a\nslot 4 = n5_m 1 0 0\n", "line 2"},
      {"area = n5_a\nradius 3\n", "line 2"},
      {"area = n5_a\nslot 1 = bad-name! 1 0 0\n", "line 2"},
      {"area = n5_a\nslot 1 = n5_m 1\n", "line 2"},
      {"area = n5_a\nshape = Q\n", "line 2"},
      {"radius = 3\n", "no area name"},
  };
  for (const Case& c : cases) {
    VfxArea a;
    std::string err;
    const bool ok = ReadVfxAreaDraft(c.text, &a, &err);
    if (ok || err.find(c.says) == std::string::npos) {
      const std::string msg = std::string("[") + c.text + "] read " + (ok ? "ok" : "refused: " + err) + ", want it refused naming " + c.says;
      check::FailText(__FILE__, __LINE__, msg.c_str());
    }
  }
}

// Play in game for an area (Task 5): the bench's request is the area's draft, then each slot's effect draft after a
// "### slot <k>" line (its low-detail effect after "### low <k>"); the game splits it. A request line before it is skipped.
TEST(VfxAreaRequestSplit) {
  const std::string area = Fixture("fog_area.area");
  const std::string text = "request = area 12 permanent\n" + area + "### slot 1\n# nwn5e VFX draft\nname = a\n\n[emitter e]\nbirthrate = 5\n" +
                           "### low 1\nname = b\n### slot 2\nname = c\n";
  std::string area_text, err;
  std::vector<std::pair<std::string, std::string>> parts;
  CHECK_EQ(SplitVfxAreaRequest(text, &area_text, &parts, &err), true);
  CHECK_TEXT(area_text, area);
  CHECK_EQ(parts.size(), 3u);
  if (parts.size() != 3) return;
  CHECK_TEXT(parts[0].first, "slot 1");
  CHECK_TEXT(parts[0].second, "# nwn5e VFX draft\nname = a\n\n[emitter e]\nbirthrate = 5\n");
  CHECK_TEXT(parts[1].first, "low 1");
  CHECK_TEXT(parts[2].first, "slot 2");
  CHECK_TEXT(parts[2].second, "name = c\n");
  VfxArea a;
  CHECK_EQ(ReadVfxAreaDraft(area_text, &a, &err), true);
  // A part named past the slots, or none of the area, is refused.
  CHECK_EQ(SplitVfxAreaRequest(area + "### slot 4\nname = x\n", &area_text, &parts, &err), false);
  CHECK_EQ(SplitVfxAreaRequest("request = area 3\n### slot 1\nname = x\n", &area_text, &parts, &err), false);
}
