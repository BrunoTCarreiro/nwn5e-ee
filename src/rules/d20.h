#pragma once

// D20 Tests (SRD 5.2): advantage/disadvantage and the Exhaustion penalty.

#include "rules/dice.h"

namespace rules {

enum class RollMode { kNormal, kAdvantage, kDisadvantage };

// Any number of advantage sources gives advantage, any number of disadvantage
// sources gives disadvantage, and having both — in any amounts — gives neither.
RollMode ResolveRollMode(int advantage_sources, int disadvantage_sources);

struct D20Roll {
  int kept;    // the die that counts (the "natural" roll)
  int first;   // first die rolled
  int second;  // second die (advantage/disadvantage), 0 for a normal roll
};

// Rolls one d20, or two keeping the higher (advantage) / lower (disadvantage).
D20Roll RollD20(RollMode mode, Dice& dice);

// Every D20 Test is reduced by 2 x Exhaustion level (levels below 0 count as 0).
int ExhaustionD20Penalty(int exhaustion_level);

// A creature dies at Exhaustion level 6.
bool IsDeadFromExhaustion(int exhaustion_level);

}  // namespace rules
