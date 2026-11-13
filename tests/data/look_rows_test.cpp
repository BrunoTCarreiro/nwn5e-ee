#include "check.h"
#include "data/look_rows.h"
#include "data/twoda.h"

#include <string>

using namespace data;

namespace {

TwoDA Table(const std::vector<std::string>& columns, size_t rows) {
  TwoDA table;
  table.columns = columns;
  table.rows.assign(rows, std::vector<std::string>(columns.size(), kEmpty));
  return table;
}

}  // namespace

// #155: what the Spell & Attack Bench's looks files may change (the row server refuses the rest).
TEST(LookRowsSpells) {
  const TwoDA spells = Table({"Label", "ImpactScript", "CastAnim", "ConjHandVisual", "Wiz_Sorc", "ProjModel"}, 1200);
  std::string why;
  CHECK_EQ(CheckLookRows("spells", spells, "# a look\nCastAnim ConjHandVisual ProjModel\n1153 out vco_x vpr_y\n", &why),
           true);
  CHECK_EQ(CheckLookRows("spells", spells, "CastAnim ImpactScript\n1153 out n5_x\n", &why), false);
  CHECK_EQ(why.find("ImpactScript") != std::string::npos, true);
  CHECK_EQ(CheckLookRows("spells", spells, "Wiz_Sorc\n1153 1\n", &why), false);
  CHECK_EQ(CheckLookRows("spells", spells, "CastAnim\n1200 out\n", &why), false);  // no such row
  CHECK_EQ(CheckLookRows("spells", spells, "CastAnim\n1199 out\n", &why), true);
  CHECK_EQ(CheckLookRows("spells", spells, "CastAnim\n+ out\n", &why), false);
  CHECK_EQ(why.find("adds no rows") != std::string::npos, true);
  CHECK_EQ(IsLookSpellColumn("castanim"), true);
  CHECK_EQ(IsLookSpellColumn("ConjTime"), false);  // the rule's timings stay the game session's
  CHECK_EQ(IsLookSpellColumn("CastTime"), false);
  CHECK_EQ(IsLookSpellColumn("TargetType"), false);
}

TEST(LookRowsEffects) {
  TwoDA effects = Table({"Label", "Type_FD", "ProgFX_Impact"}, 2092);
  std::string why;
  const std::string orb = "Label Type_FD ProgFX_Impact\n2100 NWN5E_ORB_FIRE P 910\n2105 NWN5E_ORB_THUNDER P 915\n";
  CHECK_EQ(CheckLookRows("visualeffects", effects, orb, &why), true);  // past the end
  CHECK_EQ(CheckLookRows("visualeffects", effects, "Label\n2099 X\n", &why), false);
  CHECK_EQ(CheckLookRows("visualeffects", effects, "Label\n2200 X\n", &why), false);
  CHECK_EQ(CheckLookRows("visualeffects", effects, "Label\n2199 X\n", &why), true);
  effects.rows.resize(2101, std::vector<std::string>(3, kEmpty));
  CHECK_EQ(CheckLookRows("visualeffects", effects, orb, &why), true);  // an empty row inside the table
  effects.rows[2100][1] = "P";
  CHECK_EQ(CheckLookRows("visualeffects", effects, orb, &why), false);  // in use
  CHECK_EQ(why.find("2100") != std::string::npos, true);
  TwoDA progfx = Table({"Label", "Type", "Param1"}, 1301);
  CHECK_EQ(CheckLookRows("progfx", progfx, "Label Type Param1\n910 N5OrbFire 10 vpr_ectofire01\n", &why), true);
  CHECK_EQ(CheckLookRows("progfx", progfx, "Label\n909 X\n", &why), false);
  CHECK_EQ(CheckLookRows("progfx", progfx, "Label\n1000 X\n", &why), false);
  progfx.rows[905][0] = "MirvElectro";
  CHECK_EQ(CheckLookRows("progfx", progfx, "Label\n999 X\n", &why), true);
  progfx.rows[999][2] = "vpr_x";
  CHECK_EQ(CheckLookRows("progfx", progfx, "Label\n999 X\n", &why), false);
  CHECK_EQ(CheckLookRows("progfx", progfx, "Label\n+ X\n", &why), false);
  CHECK_EQ(CheckLookRows("feat", progfx, "Label\n1 X\n", &why), false);  // no other table
  CHECK_EQ(why.find("feat.2da") != std::string::npos, true);
  CHECK_EQ(CheckLookRows("feat", progfx, "Label\n", &why), false);  // not even an empty file of it
  CHECK_EQ(CheckLookRows("visualeffects", effects, "# only a comment\n", &why), true);
}

// #161 Task 6 (the owner's Q2 A; the game session's OK, 2026-11-12: "vfx_persistent 200-299 is yours for the VFX Bench"):
// a saved area's row - any of its look's columns, on an empty row 200-299; never its scripts (the rules').
TEST(LookRowsAcceptAreaRange) {
  TwoDA base = Table({"LABEL", "SHAPE", "RADIUS", "ONENTER", "ONEXIT", "HEARTBEAT", "MODEL01", "NUMACT01"}, 260);
  base.rows[250][0] = "SOMEONES";  // a row in use
  std::string why;
  CHECK_EQ(CheckLookRows("vfx_persistent", base, "LABEL SHAPE RADIUS MODEL01 NUMACT01\n200 N5_FOG C 3 n5_fog_s1 5\n", &why), true);
  CHECK_EQ(CheckLookRows("vfx_persistent", base, "SHAPE\n299 R\n", &why), true);  // past the table's end
  CHECK_EQ(CheckLookRows("vfx_persistent", base, "SHAPE\n199 C\n", &why), false);  // the editor's preview row
  CHECK_EQ(CheckLookRows("vfx_persistent", base, "SHAPE\n300 C\n", &why), false);
  CHECK_EQ(CheckLookRows("vfx_persistent", base, "SHAPE\n100 C\n", &why), false);  // the game's own
  CHECK_EQ(CheckLookRows("vfx_persistent", base, "SHAPE\n250 C\n", &why), false);  // in use
  CHECK_EQ(CheckLookRows("vfx_persistent", base, "ONENTER\n200 n5_x\n", &why), false);
  CHECK_EQ(why.find("ONENTER") != std::string::npos, true);
  CHECK_EQ(CheckLookRows("vfx_persistent", base, "SHAPE HEARTBEAT\n200 C n5_x\n", &why), false);
}
