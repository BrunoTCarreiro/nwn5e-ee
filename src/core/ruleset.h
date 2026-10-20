#pragma once

#include <stdint.h>

// ruleset.2da values answered at runtime (docs/EE-PORT.md, "Key enabler").
// EE asks CNWRules::GetRulesetIntEntry / GetRulesetFloatEntry at the moment a
// value is used (710 + 70 call sites), with a key that is the standard
// FNV-1a-64 hash of the label. A rule registers the labels it changes; the
// engine's value (the table's, or the call site's default) is passed in, so
// a rule can keep it, change it, or answer per mode. Data files stay vanilla.

using RulesetIntAnswer = int (*)(int engine_value);
using RulesetFloatAnswer = float (*)(float engine_value);

// The key EE uses for a label.
uint64_t RulesetKey(const char* label);

// Register before HooksCommit; the hook is installed on first use. `why`
// names the rule for the log (each label's first changed answer is logged).
bool AnswerRulesetInt(const char* label, const char* why, RulesetIntAnswer answer);
bool AnswerRulesetFloat(const char* label, const char* why, RulesetFloatAnswer answer);
