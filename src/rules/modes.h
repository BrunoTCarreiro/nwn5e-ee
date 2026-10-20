#pragma once

// Rule variants (PLAN.md "Modes"): every rule decided since 2026-10-18 has a
// faithful (D&D 2024) and an NWN-mode version; off is NWN's original 3e
// behaviour. nwn5e.ini's `mode` sets the default, each rule's key overrides.

namespace rules {

enum class RuleVariant { kOff, kFaithful, kNwn };

// "faithful" / "1" -> faithful, "nwn" -> NWN mode, "off" / "0" -> off; any
// case, surrounding spaces ignored. Empty, null or unknown -> `fallback`.
RuleVariant ParseRuleVariant(const char* text, RuleVariant fallback);

}  // namespace rules
