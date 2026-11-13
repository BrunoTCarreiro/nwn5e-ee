#include "check.h"
#include "data/vfx_area.h"

#include <cmath>
#include <string>

using namespace data;

namespace {

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
