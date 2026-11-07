#include "check.h"
#include "rules/cantrips.h"
#include "rules/skills.h"

#include <string.h>

#include <algorithm>
#include <vector>

using namespace rules;

// #39 Part 1: Cantrip Upgrade - a die more at character levels 5, 11 and 17; doubled on a Critical Hit.
TEST(CantripUpgrade) {
  CHECK_EQ(CantripDice(1), 1);
  CHECK_EQ(CantripDice(4), 1);
  CHECK_EQ(CantripDice(5), 2);
  CHECK_EQ(CantripDice(10), 2);
  CHECK_EQ(CantripDice(11), 3);
  CHECK_EQ(CantripDice(16), 3);
  CHECK_EQ(CantripDice(17), 4);
  CHECK_EQ(CantripDice(20), 4);
  CHECK_EQ(CantripDice(30), 4);  // an epic level: still four
  CHECK_EQ(CantripDiceRolled(5, false), 2);
  CHECK_EQ(CantripDiceRolled(5, true), 4);
  CHECK_EQ(CantripDiceRolled(1, true), 2);
}

// SRD 5.2.1's dice, damage types and rolls.
TEST(CantripsAreSrd52) {
  const Cantrip* fire_bolt = CantripOf(1050);
  CHECK_EQ(fire_bolt != nullptr, true);
  CHECK_EQ(fire_bolt->die, 10);
  CHECK_EQ(fire_bolt->damage_type, kDamageTypeFire);
  CHECK_EQ(fire_bolt->roll == CantripRoll::kRangedSpellAttack, true);
  CHECK_EQ(CantripOf(144)->die, 8);                                       // Ray of Frost
  CHECK_EQ(CantripOf(144)->damage_type, kDamageTypeCold);
  CHECK_EQ(CantripOf(424)->roll == CantripRoll::kDexteritySave, true);    // Acid Splash: a Dexterity save
  CHECK_EQ(CantripOf(424)->die, 6);
  CHECK_EQ(CantripOf(1051)->roll == CantripRoll::kMeleeSpellAttack, true);  // Chill Touch
  CHECK_EQ(CantripOf(1051)->damage_type, kDamageTypeNecrotic);
  CHECK_EQ(CantripOf(1052)->damage_type, kDamageTypeLightning);           // Shocking Grasp
  CHECK_EQ(CantripOf(1053)->die, 12);                                     // Poison Spray
  CHECK_EQ(CantripOf(1053)->damage_type, kDamageTypePoison);
  CHECK_EQ(CantripOf(1054)->damage_type, kDamageTypeRadiant);             // Sacred Flame
  CHECK_EQ(CantripOf(1055)->die, 8);                                      // Starry Wisp
  CHECK_EQ(CantripOf(1056)->roll == CantripRoll::kWisdomSave, true);      // Vicious Mockery
  CHECK_EQ(CantripOf(1056)->damage_type, kDamageTypePsychic);
  CHECK_EQ(CantripOf(100)->die, 0);                                       // Light: no damage
  CHECK_EQ(CantripOf(107) == nullptr, true);                              // Magic Missile
  CHECK_EQ(CantripOf(37) == nullptr, true);                               // Daze: retired
  // 2024's types NWN lacks are custom types of their own (owner: "poison and acid are different things and force
  // and psychic aren't magical"): none is NWN's Acid or Magical.
  CHECK_EQ(kDamageTypePoison != kDamageTypeAcid && kDamageTypePoison == 8192u, true);
  CHECK_EQ(kDamageTypeForce == 16384u && kDamageTypePsychic == 32768u, true);
  const std::vector<Cantrip>& all = Cantrips();
  CHECK_EQ(all.size(), 10u);
  CHECK_EQ(std::is_sorted(all.begin(), all.end(), [](const Cantrip& a, const Cantrip& b) { return a.spell < b.spell; }),
           true);
}

// 4A: NWN's own cantrips retired in faithful.
TEST(CantripsRetired) {
  CHECK_EQ(IsRetiredCantrip(37), true);    // Daze
  CHECK_EQ(IsRetiredCantrip(416), true);   // Flare
  CHECK_EQ(IsRetiredCantrip(439), true);   // Electric Jolt
  CHECK_EQ(IsRetiredCantrip(189), true);   // Virtue
  CHECK_EQ(IsRetiredCantrip(33), true);    // Cure Minor Wounds
  CHECK_EQ(IsRetiredCantrip(431), true);   // Inflict Minor Wounds
  CHECK_EQ(IsRetiredCantrip(144), false);  // Ray of Frost stays
  CHECK_EQ(IsRetiredCantrip(100), false);  // Light stays
  CHECK_EQ(RetiredCantrips().size(), 6u);
}

// #39 build 2: the choice cantrips' options are SRD 5.2.1's, each a variant row of its own.
TEST(ChoiceCantripsAreSrd52) {
  const ChoiceCantrip* resistance = ChoiceCantripOf(kResistance);
  CHECK_EQ(resistance != nullptr, true);
  CHECK_EQ(resistance->master, 151);
  CHECK_EQ(resistance->kind == ChoiceKind::kDamageType, true);
  CHECK_EQ(resistance->options.size(), 11u);
  CHECK_EQ(resistance->options.front().spell, 1060);
  CHECK_EQ(resistance->options.front().value, kDamageTypeAcid);
  CHECK_EQ(resistance->options[1].value, 1u);    // Bludgeoning
  CHECK_EQ(resistance->options[6].value, 2u);    // Piercing
  CHECK_EQ(resistance->options[7].value, kDamageTypePoison);
  CHECK_EQ(resistance->options[9].value, 4u);    // Slashing
  CHECK_EQ(resistance->options.back().spell, 1070);
  CHECK_EQ(resistance->options.back().value, kDamageTypeThunder);
  const ChoiceCantrip* guidance = ChoiceCantripOf(kGuidance);
  CHECK_EQ(guidance->kind == ChoiceKind::kSkill, true);
  CHECK_EQ(guidance->options.size(), 18u);
  CHECK_EQ(guidance->options.front().spell, 1071);
  CHECK_EQ(guidance->options.front().value, static_cast<uint32_t>(Skill2024::kAcrobatics));
  CHECK_EQ(guidance->options[11].value, static_cast<uint32_t>(Skill2024::kPerception));
  CHECK_EQ(guidance->options.back().spell, 1088);
  CHECK_EQ(guidance->options.back().value, static_cast<uint32_t>(Skill2024::kSurvival));
  CHECK_EQ(strcmp(guidance->options[15].label, "Sleight of Hand"), 0);
  const ChoiceCantrip* burst = ChoiceCantripOf(kSorcerousBurst);
  CHECK_EQ(burst->options.size(), 7u);
  CHECK_EQ(burst->options.front().spell, 1089);
  CHECK_EQ(burst->options[5].value, kDamageTypePsychic);
  CHECK_EQ(burst->options.back().spell, 1095);
  CHECK_EQ(burst->options.back().value, kDamageTypeThunder);
  for (const CantripOption& option : burst->options) {
    CHECK_EQ(option.value != 1u && option.value != 2u && option.value != 4u, true);  // no weapon damage
  }
  // Every option's row is its master's +1, +2 ... and no row is two things.
  std::vector<int> rows;
  for (const ChoiceCantrip& cantrip : ChoiceCantrips()) {
    rows.push_back(cantrip.master);
    for (size_t i = 0; i < cantrip.options.size(); ++i) {
      rows.push_back(cantrip.options[i].spell);
      if (i > 0) CHECK_EQ(cantrip.options[i].spell, cantrip.options[i - 1].spell + 1);
    }
  }
  std::sort(rows.begin(), rows.end());
  CHECK_EQ(std::adjacent_find(rows.begin(), rows.end()) == rows.end(), true);
  for (int row : rows) CHECK_EQ(CantripOf(row) == nullptr || row == 151, true);  // not a build 1 cantrip
  CHECK_EQ(ChoiceCantripOf(1065)->master, 151);   // Resistance: Necrotic
  CHECK_EQ(ChoiceCantripOf(1080)->master, 1057);  // Guidance: Medicine
  CHECK_EQ(ChoiceCantripOf(1050) == nullptr, true);
  CHECK_EQ(ChoiceCantripOf(kSpareTheDying) == nullptr, true);
  CHECK_EQ(ChoiceOptionOf(151) == nullptr, true);  // the master: no choice
  CHECK_EQ(ChoiceOptionOf(1094)->value, kDamageTypePsychic);
  CHECK_EQ(ChoiceOptionOf(1080)->value, static_cast<uint32_t>(Skill2024::kMedicine));
}

// Decision 5: eight options to a page, or seven and "More".
TEST(RadialChoicePage) {
  CHECK_EQ(RadialChoicePage(7, 0).shown, 7);   // Sorcerous Burst: one page
  CHECK_EQ(RadialChoicePage(7, 0).more, false);
  CHECK_EQ(RadialChoicePage(8, 0).shown, 8);
  CHECK_EQ(RadialChoicePage(8, 0).more, false);
  CHECK_EQ(RadialChoicePage(9, 0).shown, 7);
  CHECK_EQ(RadialChoicePage(9, 0).more, true);
  CHECK_EQ(RadialChoicePage(9, 7).shown, 2);
  CHECK_EQ(RadialChoicePage(11, 0).shown, 7);  // Resistance: seven and More, then four
  CHECK_EQ(RadialChoicePage(11, 0).more, true);
  CHECK_EQ(RadialChoicePage(11, 7).shown, 4);
  CHECK_EQ(RadialChoicePage(11, 7).more, false);
  CHECK_EQ(RadialChoicePage(18, 7).shown, 7);  // Guidance: 7 + More, 7 + More, 4
  CHECK_EQ(RadialChoicePage(18, 7).more, true);
  CHECK_EQ(RadialChoicePage(18, 14).shown, 4);
  CHECK_EQ(RadialChoicePage(18, 14).more, false);
  CHECK_EQ(RadialChoicePage(0, 0).shown, 0);
  CHECK_EQ(RadialChoicePage(5, 5).shown, 0);
  CHECK_EQ(RadialChoicePage(5, -1).shown, 0);
}

// The page rides in the radial param; an option's cast keeps the class bits and drops the page.
TEST(RadialChoiceParams) {
  const long long menu = (2LL << 28) | 151LL | (3LL << 40);  // class slot 2, Resistance, #36's from-level 3
  CHECK_EQ(RadialChoiceFirst(menu), 0);
  const long long page = RadialChoiceWithFirst(menu, 14);
  CHECK_EQ(RadialChoiceFirst(page), 14);
  CHECK_EQ(page & 0xFFFF, 151LL);
  CHECK_EQ((page >> 28) & 3, 2LL);
  const long long cast = RadialChoiceCast(page, 1063);  // Resistance: Fire
  CHECK_EQ(cast & 0xFFFF, 1063LL);
  CHECK_EQ((cast >> 28) & 3, 2LL);
  CHECK_EQ(RadialChoiceFirst(cast), 0);
  CHECK_EQ((cast >> 40) & 0xF, 0LL);
  CHECK_EQ(RadialChoiceFirst(RadialChoiceWithFirst(page, 7)), 7);
}

namespace {
struct Rolls {
  std::vector<int> values;
  size_t next = 0;
};
int NextRoll(void* context) {
  Rolls* rolls = static_cast<Rolls*>(context);
  return rolls->next < rolls->values.size() ? rolls->values[rolls->next++] : 1;
}
}  // namespace

// Sorcerous Burst: each 8 adds a d8, at most the spellcasting modifier more.
TEST(SorcerousBurstDice) {
  Rolls a{{8, 3, 8, 8, 2}};
  int extra = -1;
  CHECK_EQ(SorcerousBurstDamage(2, 3, &NextRoll, &a, &extra), 29);  // 8 3, then 8 8 2
  CHECK_EQ(extra, 3);
  Rolls b{{8, 8, 5}};
  CHECK_EQ(SorcerousBurstDamage(1, 1, &NextRoll, &b, &extra), 16);  // one more only
  CHECK_EQ(extra, 1);
  CHECK_EQ(b.next, 2u);
  Rolls c{{8, 8}};
  CHECK_EQ(SorcerousBurstDamage(2, 0, &NextRoll, &c, &extra), 16);  // modifier +0: none
  CHECK_EQ(extra, 0);
  Rolls d{{8, 4}};
  CHECK_EQ(SorcerousBurstDamage(1, -1, &NextRoll, &d, &extra), 8);  // a negative modifier: none
  Rolls e{{5, 6, 7, 2}};
  CHECK_EQ(SorcerousBurstDamage(4, 5, &NextRoll, &e, &extra), 20);  // no 8: the dice alone
  CHECK_EQ(extra, 0);
  Rolls f{{}};
  CHECK_EQ(SorcerousBurstDamage(0, 5, &NextRoll, &f, nullptr), 0);
}

// Resistance's 1d4 off (at most the damage); Spare the Dying's range doubles at 5, 11, 17.
TEST(ResistanceAndSpareTheDying) {
  CHECK_EQ(ResistanceReduction(10, 3), 3);
  CHECK_EQ(ResistanceReduction(2, 4), 2);
  CHECK_EQ(ResistanceReduction(0, 3), 0);
  CHECK_EQ(ResistanceReduction(5, 0), 0);
  CHECK_EQ(SpareTheDyingRangeFeet(1), 15);
  CHECK_EQ(SpareTheDyingRangeFeet(4), 15);
  CHECK_EQ(SpareTheDyingRangeFeet(5), 30);
  CHECK_EQ(SpareTheDyingRangeFeet(10), 30);
  CHECK_EQ(SpareTheDyingRangeFeet(11), 60);
  CHECK_EQ(SpareTheDyingRangeFeet(16), 60);
  CHECK_EQ(SpareTheDyingRangeFeet(17), 120);
  CHECK_EQ(SpareTheDyingRangeFeet(20), 120);
}
