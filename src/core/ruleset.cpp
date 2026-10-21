#include "ruleset.h"

#include <unordered_map>

#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "rules/ruleset_key.h"

namespace {

using GetIntFn = int (*)(void* rules, uint64_t key, int fallback);
using GetFloatFn = float (*)(void* rules, uint64_t key, float fallback);

struct IntEntry {
  const char* label;
  const char* why;
  RulesetIntAnswer answer;
  bool logged;
};
struct FloatEntry {
  const char* label;
  const char* why;
  RulesetFloatAnswer answer;
  bool logged;
};

GetIntFn g_get_int = nullptr;
GetFloatFn g_get_float = nullptr;
// Filled before the hooks go live, then only read (the `logged` flags are a
// benign race: at worst a label is logged twice).
std::unordered_map<uint64_t, IntEntry> g_ints;
std::unordered_map<uint64_t, FloatEntry> g_floats;
bool g_int_hooked = false, g_int_failed = false;
bool g_float_hooked = false, g_float_failed = false;
void* g_rules_object = nullptr;  // the CNWRules the engine last asked

int GetRulesetIntEntry(void* rules, uint64_t key, int fallback) {
  g_rules_object = rules;
  const int engine_value = g_get_int(rules, key, fallback);
  const auto it = g_ints.find(key);
  if (it == g_ints.end()) return engine_value;
  IntEntry& entry = it->second;
  const int value = entry.answer(engine_value);
  if (value != engine_value && !entry.logged) {
    entry.logged = true;
    Log("ruleset %s: %d -> %d (%s; logged once)", entry.label, engine_value, value, entry.why);
  }
  return value;
}

float GetRulesetFloatEntry(void* rules, uint64_t key, float fallback) {
  const float engine_value = g_get_float(rules, key, fallback);
  const auto it = g_floats.find(key);
  if (it == g_floats.end()) return engine_value;
  FloatEntry& entry = it->second;
  const float value = entry.answer(engine_value);
  if (value != engine_value && !entry.logged) {
    entry.logged = true;
    Log("ruleset %s: %g -> %g (%s; logged once)", entry.label, engine_value, value, entry.why);
  }
  return value;
}

}  // namespace

uint64_t RulesetKey(const char* label) { return rules::RulesetLabelKey(label); }

int RulesetIntValue(const char* label, int fallback) {
  if (!g_get_int || !g_rules_object) return fallback;
  return g_get_int(g_rules_object, RulesetKey(label), fallback);
}

bool AnswerRulesetInt(const char* label, const char* why, RulesetIntAnswer answer) {
  if (!g_int_hooked && !g_int_failed) {
    g_int_hooked = HookExport("CNWRules::GetRulesetIntEntry", ee::kGetRulesetIntEntry,
                              reinterpret_cast<void*>(&GetRulesetIntEntry), reinterpret_cast<void**>(&g_get_int));
    g_int_failed = !g_int_hooked;
  }
  if (!g_int_hooked) return false;
  g_ints[RulesetKey(label)] = {label, why, answer, false};
  return true;
}

bool AnswerRulesetFloat(const char* label, const char* why, RulesetFloatAnswer answer) {
  if (!g_float_hooked && !g_float_failed) {
    g_float_hooked = HookExport("CNWRules::GetRulesetFloatEntry", ee::kGetRulesetFloatEntry,
                                reinterpret_cast<void*>(&GetRulesetFloatEntry), reinterpret_cast<void**>(&g_get_float));
    g_float_failed = !g_float_hooked;
  }
  if (!g_float_hooked) return false;
  g_floats[RulesetKey(label)] = {label, why, answer, false};
  return true;
}
