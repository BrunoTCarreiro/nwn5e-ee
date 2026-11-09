#include "check.h"
#include "rules/class_features.h"
#include "rules/cleric.h"

#include <string>
#include <vector>

using namespace rules;

// #42 (owner, 2026-11-09: "A on all"): the 2024 Cleric's numbers, SRD 5.2.1 pp. 36-40.
TEST(ClericChannelDivinity) {
  CHECK_EQ(ChannelDivinityUses(1), 0);
  CHECK_EQ(ChannelDivinityUses(2), 2);
  CHECK_EQ(ChannelDivinityUses(5), 2);
  CHECK_EQ(ChannelDivinityUses(6), 3);
  CHECK_EQ(ChannelDivinityUses(17), 3);
  CHECK_EQ(ChannelDivinityUses(18), 4);
  CHECK_EQ(ChannelDivinityUses(20), 4);
  // One back on a Short Rest, never past the uses.
  CHECK_EQ(ChannelDivinityAfterShortRest(0, 6), 1);
  CHECK_EQ(ChannelDivinityAfterShortRest(2, 6), 3);
  CHECK_EQ(ChannelDivinityAfterShortRest(3, 6), 3);
  CHECK_EQ(ChannelDivinityAfterShortRest(0, 1), 0);
  // Divine Spark: 1d8, 2d8 at 7, 3d8 at 13, 4d8 at 18.
  CHECK_EQ(DivineSparkDice(2), 1);
  CHECK_EQ(DivineSparkDice(6), 1);
  CHECK_EQ(DivineSparkDice(7), 2);
  CHECK_EQ(DivineSparkDice(12), 2);
  CHECK_EQ(DivineSparkDice(13), 3);
  CHECK_EQ(DivineSparkDice(17), 3);
  CHECK_EQ(DivineSparkDice(18), 4);
  // Sear Undead: a d8 per point of Wisdom modifier, at least one.
  CHECK_EQ(SearUndeadDice(4), 4);
  CHECK_EQ(SearUndeadDice(0), 1);
  CHECK_EQ(SearUndeadDice(-1), 1);
  CHECK_EQ(kTurnUndeadFeet, 30);
  CHECK_EQ(kTurnUndeadSeconds, 60);
}

TEST(ClericLifeDomain) {
  // Disciple of Life and Blessed Healer: 2 + the slot's level.
  CHECK_EQ(LifeHealingBonus(1), 3);
  CHECK_EQ(LifeHealingBonus(3), 5);
  CHECK_EQ(LifeHealingBonus(0), 0);  // a cantrip uses no slot
  // Preserve Life: 5 x Cleric level, a creature up to half its maximum.
  CHECK_EQ(PreserveLifePool(3), 15);
  CHECK_EQ(PreserveLifePool(10), 50);
  CHECK_EQ(PreserveLifeRoom(4, 30), 11);
  CHECK_EQ(PreserveLifeRoom(15, 30), 0);
  CHECK_EQ(PreserveLifeRoom(20, 30), 0);
  CHECK_EQ(PreserveLifeRoom(3, 15), 4);  // half of 15 is 7
  // Shared most room first, each up to its room.
  const std::vector<int> shares = PreserveLifeShares({5, 12, 3}, 15);
  CHECK_EQ(shares.size(), 3u);
  CHECK_EQ(shares[1], 12);
  CHECK_EQ(shares[0], 3);
  CHECK_EQ(shares[2], 0);
  const std::vector<int> all = PreserveLifeShares({5, 2}, 50);
  CHECK_EQ(all[0] + all[1], 7);
  // The domain spells, always prepared: 4 at 3, 6 at 5, 8 at 7, 10 at 9.
  CHECK_EQ(LifeDomainSpells(2).size(), 0u);
  CHECK_EQ(LifeDomainSpells(3).size(), 4u);
  CHECK_EQ(LifeDomainSpells(5).size(), 6u);
  CHECK_EQ(LifeDomainSpells(7).size(), 8u);
  CHECK_EQ(LifeDomainSpells(9).size(), 10u);
  CHECK_EQ(std::string(LifeDomainSpells(3)[2]) == "Cure Wounds", true);
}

// The Cleric's features as !features lists them: Channel Divinity from 2, the Life Domain at 3.
TEST(ClericFeatureList) {
  CHECK_EQ(HasFeatureList(CharacterClass::kCleric), true);
  CHECK_EQ(ClassFeatures2024(CharacterClass::kCleric, 1).size(), 2u);
  CHECK_EQ(ClassFeatures2024(CharacterClass::kCleric, 2).size(), 3u);  // Channel Divinity at 2
  bool channel = false, domain = false;
  for (const ClassFeature& f : ClassFeatures2024(CharacterClass::kCleric, 3)) {
    channel = channel || std::string(f.name) == "Channel Divinity";
    domain = domain || std::string(f.name) == "Life Domain";
  }
  CHECK_EQ(channel, true);
  CHECK_EQ(domain, true);
  CHECK_EQ(std::string(Subclass2024(CharacterClass::kCleric)) == "Life Domain", true);
  CHECK_EQ(ClassFeatures2024(CharacterClass::kCleric, 20).size(), 19u);
}

TEST(ClericOrderAndStrikes) {
  // Thaumaturge: the Wisdom modifier on Arcana and Religion checks, at least +1.
  CHECK_EQ(ThaumaturgeCheckBonus(3), 3);
  CHECK_EQ(ThaumaturgeCheckBonus(0), 1);
  CHECK_EQ(ThaumaturgeCheckBonus(-2), 1);
  // Divine Strike: 1d8 from 7, 2d8 from 14.
  CHECK_EQ(DivineStrikeDice(6), 0);
  CHECK_EQ(DivineStrikeDice(7), 1);
  CHECK_EQ(DivineStrikeDice(13), 1);
  CHECK_EQ(DivineStrikeDice(14), 2);
  // Potent Spellcasting at 14: Temporary Hit Points twice the Wisdom modifier.
  CHECK_EQ(PotentSpellcastingTempHp(4), 8);
  CHECK_EQ(PotentSpellcastingTempHp(-1), 0);
}
