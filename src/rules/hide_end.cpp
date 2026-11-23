#include "rules/hide_end.h"

#include "rules/spells2024.h"

namespace rules {

HideAfter HideAfterAct(const HideActFacts& facts) {
  if (!facts.on) return HideAfter::kEngine;
  if (!facts.hiding) return HideAfter::kNotHiding;
  if (facts.act == HideAct::kCast) {
    return facts.verbal && !facts.without_verbal ? HideAfter::kEndsAfter : HideAfter::kStays;
  }
  return facts.stealth_attack ? HideAfter::kAtTurnEnd : HideAfter::kEndsAfter;
}

bool SupremeSneakKeeps(Cover least_cover) {
  return least_cover == Cover::kThreeQuarters || least_cover == Cover::kTotal;
}

Cover LessCover(Cover a, Cover b) { return static_cast<int>(a) <= static_cast<int>(b) ? a : b; }

bool SpellRowIsALaterUse(int row) {
  return row == kSpiritualWeaponStrike || row == kFlameBladeEvoke || row == kHeatMetalAgain ||
         (row >= kDragonsBreathExhaleFirst && row <= kDragonsBreathExhaleLast);
}

bool HasSupremeSneak(int rogue_level) { return rogue_level >= 9; }

}  // namespace rules
