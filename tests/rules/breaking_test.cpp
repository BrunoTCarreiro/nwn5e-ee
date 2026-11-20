#include "check.h"
#include "rules/breaking.h"

using namespace rules;

// #172 DE (0.99.106): a breakable's debris material from placeableobjsnds.2da's ArmorType - wood 0, stone 1, plate
// (metal) 2, leather (cloth) 3 - glass (4) from the hand list of labels; Ethereal or none: no debris (-1).
TEST(BreakingDebrisMaterial) {
  CHECK_EQ(DebrisMaterialFor("wood", false), 0);
  CHECK_EQ(DebrisMaterialFor("stone", false), 1);
  CHECK_EQ(DebrisMaterialFor("plate", false), 2);
  CHECK_EQ(DebrisMaterialFor("leather", false), 3);
  CHECK_EQ(DebrisMaterialFor("WOOD", false), 0);
  CHECK_EQ(DebrisMaterialFor("wood", true), 4);       // a window's frame: glass
  CHECK_EQ(DebrisMaterialFor("Ethereal", false), -1);  // a force field
  CHECK_EQ(DebrisMaterialFor("Ethereal", true), -1);
  CHECK_EQ(DebrisMaterialFor("", false), -1);
  CHECK_EQ(DebrisMaterialFor(nullptr, false), -1);
  CHECK_EQ(DebrisMaterialFor("", true), 4);           // a glass label with no sound row: still glass
  CHECK_EQ(DebrisMaterialFor("woodwork", false), -1);  // not an ArmorType
}

TEST(BreakingGlassLabels) {
  CHECK_EQ(IsGlassLabel("WindowExterior"), true);
  CHECK_EQ(IsGlassLabel("\"new:Small Arch Window 2\""), true);
  CHECK_EQ(IsGlassLabel("BigMirror"), true);
  CHECK_EQ(IsGlassLabel("CrystalBall"), true);
  CHECK_EQ(IsGlassLabel("\"Bottle, Green\""), true);
  CHECK_EQ(IsGlassLabel("SoulJar"), true);
  CHECK_EQ(IsGlassLabel("\"Lantern - Elven (Tinted Glass, Dynamic)\""), true);
  CHECK_EQ(IsGlassLabel("\"Floorblood 1\""), false);
  CHECK_EQ(IsGlassLabel("Bench"), false);
  CHECK_EQ(IsGlassLabel(""), false);
  CHECK_EQ(IsGlassLabel(nullptr), false);
}

// The size from the Destroyed sound (cb_bu_*sml Small 0, cb_bu_*lrg Large 1); none: Small.
TEST(BreakingDebrisSize) {
  CHECK_EQ(DebrisSizeFor("cb_bu_woodsml"), 0);
  CHECK_EQ(DebrisSizeFor("cb_bu_woodlrg"), 1);
  CHECK_EQ(DebrisSizeFor("cb_bu_metallrg"), 1);
  CHECK_EQ(DebrisSizeFor("CB_BU_STONELRG"), 1);
  CHECK_EQ(DebrisSizeFor("scm_hitelec"), 0);
  CHECK_EQ(DebrisSizeFor(""), 0);
  CHECK_EQ(DebrisSizeFor(nullptr), 0);
  CHECK_EQ(DebrisSizeFor("lrg_cb_bu_wood"), 0);  // "lrg" at its end only
}
