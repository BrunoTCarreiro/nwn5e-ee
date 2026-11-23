#include "check.h"
#include "rules/class_features.h"
#include "rules/hide_end.h"
#include "rules/spells2024.h"

using namespace rules;

namespace {
HideActFacts Facts(HideAct act, bool verbal, bool without_verbal, bool stealth_attack) {
  HideActFacts f;
  f.on = true;
  f.hiding = true;
  f.act = act;
  f.verbal = verbal;
  f.without_verbal = without_verbal;
  f.stealth_attack = stealth_attack;
  return f;
}
}  // namespace

// #9 / #33 T3 (owner, 2026-11-23: "yes to both"): what ends the 2024 Hide. Breaks caught: an attack roll keeping the
// hide, a Verbal cast keeping it, a cast without Verbal ending it, Subtle Spell's hook ignored, Stealth Attack ending
// it at once, the key off still deciding, a creature not hiding decided for, Supreme Sneak keeping at Half, its
// level off by one, a spell's later use (Heat Metal's Again) taken for a cast or a cast for a later use.
TEST(HideEndsAfterAttackOrVerbalCast) {
  CHECK_EQ(HideAfterAct(Facts(HideAct::kAttackRoll, false, false, false)) == HideAfter::kEndsAfter, true);
  CHECK_EQ(HideAfterAct(Facts(HideAct::kCast, true, false, false)) == HideAfter::kEndsAfter, true);
  CHECK_EQ(HideAfterAct(Facts(HideAct::kCast, false, false, false)) == HideAfter::kStays, true);
  CHECK_EQ(HideAfterAct(Facts(HideAct::kCast, true, true, false)) == HideAfter::kStays, true);  // Subtle Spell
  CHECK_EQ(HideAfterAct(Facts(HideAct::kAttackRoll, false, false, true)) == HideAfter::kAtTurnEnd, true);
  CHECK_EQ(HideAfterAct(Facts(HideAct::kCast, true, false, true)) == HideAfter::kEndsAfter, true);  // not an attack
  HideActFacts off = Facts(HideAct::kAttackRoll, false, false, false);
  off.on = false;
  CHECK_EQ(HideAfterAct(off) == HideAfter::kEngine, true);
  HideActFacts seen = Facts(HideAct::kAttackRoll, false, false, false);
  seen.hiding = false;
  CHECK_EQ(HideAfterAct(seen) == HideAfter::kNotHiding, true);
  CHECK_EQ(SupremeSneakKeeps(Cover::kTotal) && SupremeSneakKeeps(Cover::kThreeQuarters), true);
  CHECK_EQ(SupremeSneakKeeps(Cover::kHalf) || SupremeSneakKeeps(Cover::kNone), false);
  CHECK_EQ(LessCover(Cover::kTotal, Cover::kHalf) == Cover::kHalf, true);
  CHECK_EQ(LessCover(Cover::kNone, Cover::kThreeQuarters) == Cover::kNone, true);
  CHECK_EQ(HasSupremeSneak(9) && !HasSupremeSneak(8), true);
  CHECK_EQ(SpellRowIsALaterUse(kHeatMetalAgain) && SpellRowIsALaterUse(kSpiritualWeaponStrike), true);
  CHECK_EQ(SpellRowIsALaterUse(kFlameBladeEvoke) && SpellRowIsALaterUse(kDragonsBreathExhaleLast), true);
  CHECK_EQ(SpellRowIsALaterUse(kHeatMetal) || SpellRowIsALaterUse(kSpiritualWeapon), false);
  CHECK_EQ(SpellRowIsALaterUse(kDragonsBreath) || SpellRowIsALaterUse(kDragonsBreathExhaleLast + 1), false);
  CHECK_EQ(CunningEffectAvailable(CunningEffect::kStealthAttack, 9), true);
  CHECK_EQ(CunningEffectAvailable(CunningEffect::kStealthAttack, 8), false);
  CHECK_EQ(CunningEffectCost(CunningEffect::kStealthAttack), 1);
  CunningChoice choice;
  choice.first = CunningEffect::kStealthAttack;
  CHECK_EQ(DecodeCunningChoice(EncodeCunningChoice(choice)).first == CunningEffect::kStealthAttack, true);
}
