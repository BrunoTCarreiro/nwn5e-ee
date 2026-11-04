#include "check.h"
#include "rules/script_names.h"

using namespace rules;

// 0.69.2 (#124; owner, 2026-11-04: "The acid flask doesn't show an area of effect preview but it clearly has a pretty
// big splashzone" - the log: none of nwn5e's grenade scripts ever ran): the hook's lookup took only names starting
// "n5". Breaks caught: the "n5" filter back, a case-sensitive match, a prefix or a longer name matching, the length
// ignored.
TEST(ScriptNames) {
  const std::vector<std::string> names = {"n5_lamp_use", "x0_s3_acid", "x2_s3_bomb", "n5_fpacid_in"};
  CHECK_EQ(FindScriptName(names, "x0_s3_acid", 10), 1);
  CHECK_EQ(FindScriptName(names, "X0_S3_ACID", 10), 1);  // resrefs are case-blind
  CHECK_EQ(FindScriptName(names, "x2_s3_bomb", 10), 2);
  CHECK_EQ(FindScriptName(names, "N5_LAMP_USE", 11), 0);
  CHECK_EQ(FindScriptName(names, "n5_fpacid_in", 12), 3);
  CHECK_EQ(FindScriptName(names, "x0_s3_acid2", 11), -1);
  CHECK_EQ(FindScriptName(names, "x0_s3_aci", 9), -1);
  CHECK_EQ(FindScriptName(names, "x0_s3_acid", 9), -1);  // the engine's length counts, not the text's
  CHECK_EQ(FindScriptName(names, "nw_s0_acidfog", 13), -1);
  CHECK_EQ(FindScriptName(names, nullptr, 0), -1);
}
