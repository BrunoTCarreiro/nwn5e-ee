#include "check.h"
#include "rules/cantrips.h"
#include "rules/concentration.h"
#include "rules/spells2024.h"

#include <vector>

using namespace rules;

// #39 Part 2, level 2, build 1a (owner, 2026-11-18: "A for all, except Q3"): SRD 5.2.1's Acid Arrow, Shatter, Mind
// Spike, Shining Smite and Prayer of Healing - their rows, numbers and upcasting.
TEST(Level2Build1aTable) {
  const Spell2024* acid = Spell2024Of(kAcidArrow);
  CHECK_EQ(acid != nullptr, true);
  CHECK_EQ(acid->level, 2);
  CHECK_EQ(acid->effect == SpellEffect2024::kSpellAttack, true);
  CHECK_EQ(acid->dice, 4);
  CHECK_EQ(acid->die, 4);
  CHECK_EQ(acid->damage_type, kDamageTypeAcid);
  CHECK_EQ(SpellDiceAt(*acid, 2), 4);
  CHECK_EQ(SpellDiceAt(*acid, 3), 5);  // 1d4 more a slot level above 2
  const Spell2024* shatter = Spell2024Of(kShatter);
  CHECK_EQ(shatter != nullptr, true);
  CHECK_EQ(shatter->level, 2);
  CHECK_EQ(shatter->effect == SpellEffect2024::kSave, true);
  CHECK_EQ(shatter->save == Ability::kConstitution, true);
  CHECK_EQ(shatter->half_on_save, true);
  CHECK_EQ(shatter->shape == SpellShape2024::kSphere, true);
  CHECK_EQ(shatter->feet, 10);
  CHECK_EQ(shatter->dice, 3);
  CHECK_EQ(shatter->die, 8);
  CHECK_EQ(shatter->damage_type, kDamageTypeThunder);
  CHECK_EQ(SpellDiceAt(*shatter, 4), 5);
  const Spell2024* spike = Spell2024Of(kMindSpike);
  CHECK_EQ(spike != nullptr, true);
  CHECK_EQ(spike->level, 2);
  CHECK_EQ(spike->save == Ability::kWisdom, true);
  CHECK_EQ(spike->half_on_save, true);
  CHECK_EQ(spike->shape == SpellShape2024::kOne, true);
  CHECK_EQ(spike->damage_type, kDamageTypePsychic);
  CHECK_EQ(SpellDiceAt(*spike, 2), 3);
  CHECK_EQ(SpellDiceAt(*spike, 3), 4);
  const Spell2024* shining = Spell2024Of(kShiningSmite);
  CHECK_EQ(shining != nullptr, true);
  CHECK_EQ(shining->level, 2);
  CHECK_EQ(shining->effect == SpellEffect2024::kSmite, true);
  CHECK_EQ(shining->damage_type, kDamageTypeRadiant);
  const Spell2024* prayer = Spell2024Of(kPrayerOfHealing);
  CHECK_EQ(prayer != nullptr, true);
  CHECK_EQ(prayer->level, 2);
  CHECK_EQ(prayer->effect == SpellEffect2024::kHealing, true);
  CHECK_EQ(prayer->dice, 2);
  CHECK_EQ(prayer->die, 8);
  CHECK_EQ(SpellDiceAt(*prayer, 3), 3);
  CHECK_EQ(prayer->ritual, false);
}

TEST(Level2Build1aRules) {
  // Acid Arrow: a miss splashes for half the initial roll; the acid later 2d4, 1d4 more a level above 2.
  CHECK_EQ(AcidArrowMissDamage(9), 4);
  CHECK_EQ(AcidArrowMissDamage(8), 4);
  CHECK_EQ(AcidArrowMissDamage(1), 0);
  CHECK_EQ(AcidArrowMissDamage(0), 0);
  CHECK_EQ(AcidArrowLaterDice(2), 2);
  CHECK_EQ(AcidArrowLaterDice(3), 3);
  CHECK_EQ(AcidArrowLaterDice(5), 5);
  CHECK_EQ(WizardOnlySpell(kAcidArrow), true);  // the Wizard's alone (2024)
  CHECK_EQ(WizardOnlySpell(kShatter), false);    // the Sorcerer's too
  CHECK_EQ(WizardOnlySpell(kMindSpike), false);
  // Shatter: a Construct saves with Disadvantage; no one else does.
  CHECK_EQ(ShatterSaveDisadvantage(10), true);
  CHECK_EQ(ShatterSaveDisadvantage(24), false);  // an Undead
  CHECK_EQ(ShatterSaveDisadvantage(6), false);   // a Human
  // Shining Smite: build 3's smite - a melee weapon or an Unarmed Strike; 2d6, 1d6 more a level; its armed icon.
  CHECK_EQ(IsSmiteSpell(kShiningSmite), true);
  CHECK_EQ(SmiteRidesOn(kShiningSmite, false, false), true);
  CHECK_EQ(SmiteRidesOn(kShiningSmite, false, true), true);
  CHECK_EQ(SmiteRidesOn(kShiningSmite, true, false), false);
  CHECK_EQ(ShiningSmiteDice(2), 2);
  CHECK_EQ(ShiningSmiteDice(3), 3);
  CHECK_EQ(ShiningSmiteDice(5), 5);
  CHECK_EQ(SmiteArmedIconRow(kShiningSmite), 236);
  CHECK_EQ(IsSmiteArmedIcon(225), true);
  CHECK_EQ(IsSmiteArmedIcon(227), true);
  CHECK_EQ(IsSmiteArmedIcon(236), true);
  CHECK_EQ(IsSmiteArmedIcon(228), false);  // Detect Evil and Good's mark
  CHECK_EQ(IsSmiteArmedIcon(224), false);
  CHECK_EQ(IsSmiteArmedIcon(235), false);  // Silent Image's
  // Prayer of Healing: 10 minutes to cast (no Ritual).
  CHECK_EQ(CastingMinutes(kPrayerOfHealing), 10);
  CHECK_EQ(IsRitualSpell(kPrayerOfHealing), false);
  CHECK_EQ(CastingMinutes(kShatter), 0);
  // Concentration: Mind Spike (from the cast) and Shining Smite (from its hit); not the other three.
  CHECK_EQ(IsConcentrationSpell(kMindSpike), true);
  CHECK_EQ(IsConcentrationSpell(kShiningSmite), true);
  CHECK_EQ(IsConcentrationSpell(kAcidArrow) || IsConcentrationSpell(kShatter) ||
               IsConcentrationSpell(kPrayerOfHealing),
           false);
}

TEST(Level2PrayerOfHealingTargets) {
  // The caster first, then the nearest within 30 feet (9.14 m); five at most; none prayed for since its Long Rest.
  std::vector<PrayerCandidate> near = {{7, 8.0f, false}, {1, 0.0f, false}, {2, 3.0f, false}, {3, 1.0f, false},
                                       {4, 9.5f, false}, {5, 2.0f, true},  {6, 5.0f, false}, {8, 6.0f, false}};
  std::vector<uint32_t> picked = PrayerOfHealingTargetsOf(1, near);
  CHECK_EQ(picked.size(), 5u);
  CHECK_EQ(picked[0], 1u);  // the caster
  CHECK_EQ(picked[1], 3u);  // 1 m
  CHECK_EQ(picked[2], 2u);  // 3 m (5 at 2 m prayed already)
  CHECK_EQ(picked[3], 6u);  // 5 m
  CHECK_EQ(picked[4], 8u);  // 6 m; 7 at 8 m is the sixth - left out
  // Past 30 feet: out; the caster prayed for: not itself.
  std::vector<PrayerCandidate> far = {{1, 0.0f, true}, {2, 9.0f, false}, {3, 9.2f, false}};
  picked = PrayerOfHealingTargetsOf(1, far);
  CHECK_EQ(picked.size(), 1u);
  CHECK_EQ(picked[0], 2u);
  CHECK_EQ(PrayerOfHealingTargetsOf(1, {}).empty(), true);
  // The caster first even when another stands as close (a familiar on its shoulder) and is listed before it.
  std::vector<PrayerCandidate> tie = {{2, 0.0f, false}, {1, 0.0f, false}, {3, 9.0f, false}};
  picked = PrayerOfHealingTargetsOf(1, tie);
  CHECK_EQ(picked.size(), 3u);
  CHECK_EQ(picked[0], 1u);
  CHECK_EQ(picked[1], 2u);
  CHECK_EQ(picked[2], 3u);  // 9 m: inside the 30 feet (9.14 m)
  CHECK_EQ(kPrayerOfHealingTargets, 5);
  CHECK_EQ(kPrayerOfHealingFeet, 30);
}
