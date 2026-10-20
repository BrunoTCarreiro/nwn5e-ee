#include "check.h"
#include "rules/modes.h"

using namespace rules;

// PLAN.md "Modes". Breaks caught: a value read case-sensitively or with stray
// spaces from the ini; the old 0/1 style not understood; an empty or unknown
// value not falling back to the global mode.
TEST(RuleVariantsParseFromTheIni) {
  CHECK_EQ(ParseRuleVariant("faithful", RuleVariant::kNwn), RuleVariant::kFaithful);
  CHECK_EQ(ParseRuleVariant("Faithful", RuleVariant::kNwn), RuleVariant::kFaithful);
  CHECK_EQ(ParseRuleVariant("  nwn ", RuleVariant::kFaithful), RuleVariant::kNwn);
  CHECK_EQ(ParseRuleVariant("NWN", RuleVariant::kFaithful), RuleVariant::kNwn);
  CHECK_EQ(ParseRuleVariant("off", RuleVariant::kFaithful), RuleVariant::kOff);
  CHECK_EQ(ParseRuleVariant("0", RuleVariant::kFaithful), RuleVariant::kOff);
  CHECK_EQ(ParseRuleVariant("1", RuleVariant::kNwn), RuleVariant::kFaithful);
  CHECK_EQ(ParseRuleVariant("", RuleVariant::kNwn), RuleVariant::kNwn);
  CHECK_EQ(ParseRuleVariant(nullptr, RuleVariant::kFaithful), RuleVariant::kFaithful);
  CHECK_EQ(ParseRuleVariant("banana", RuleVariant::kNwn), RuleVariant::kNwn);
}
