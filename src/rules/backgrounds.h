#pragma once

// Phase 3 CM5 (PLAN.md): the four SRD 5.2 backgrounds. Non-SRD backgrounds
// belong in the private layer.

#include <optional>
#include <string>
#include <vector>

#include "rules/abilities.h"
#include "rules/skills.h"

namespace rules {

enum class Background { kNone, kAcolyte, kCriminal, kSage, kSoldier };

struct BackgroundInfo {
  Background background;
  const char* name;
  Ability abilities[3];     // the three the increases come from
  const char* origin_feat;  // its Origin feat
  SkillSet skills;          // its two skills (and Thieves' Tools for the Criminal)
  const char* tool;
};

const std::vector<BackgroundInfo>& Backgrounds2024();
const BackgroundInfo* BackgroundData(Background background);
std::optional<Background> ParseBackground(const std::string& text);

// The increases from `chosen` (two abilities: +2 / +1; three: +1 each), all
// from the background's three. "" when valid (amounts filled), else why not.
std::string BackgroundIncreases(Background background, const std::vector<Ability>& chosen, int amounts[6]);

// Saved forms (local variables): amounts 0..3 per ability in 2 bits each; an
// ASI as (first + 1) + (second + 1) * 8, 0 = not chosen.
int EncodeAmounts(const int amounts[6]);
void DecodeAmounts(int encoded, int amounts[6]);
int EncodeAsi(const AbilityIncrease& asi);
std::optional<AbilityIncrease> DecodeAsi(int encoded);

}  // namespace rules
