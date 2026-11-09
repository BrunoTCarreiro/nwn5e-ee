#pragma once

// The 2024 Cleric (#42; owner, 2026-11-09: "A on all"; docs/PLAN.md "Phase 4"): SRD 5.2.1's numbers for its
// features (pp. 36-40). The engine side: core/rules_cleric.cpp.

#include <vector>

namespace rules {

// Channel Divinity (2): its uses - 2 from Cleric level 2, 3 from 6, 4 from 18; none before 2. "You regain one of its
// expended uses when you finish a Short Rest, and you regain all expended uses when you finish a Long Rest."
int ChannelDivinityUses(int cleric_level);
int ChannelDivinityAfterShortRest(int left, int cleric_level);

// Divine Spark: 1d8 + Wisdom modifier, a d8 more at Cleric levels 7, 13 and 18.
int DivineSparkDice(int cleric_level);

// Turn Undead: the Undead within 30 feet; Frightened and Incapacitated for 1 minute on a failed Wisdom save. Sear
// Undead (5): as many d8s of Radiant damage as the Wisdom modifier, at least one.
constexpr int kTurnUndeadFeet = 30;
constexpr int kTurnUndeadSeconds = 60;
int SearUndeadDice(int wisdom_modifier);

// Life Domain (3). Disciple of Life: a spell cast with a slot that restores Hit Points restores 2 + the slot's level
// more; Blessed Healer (6): healing others with a slot heals the caster that much. 0 without a slot.
int LifeHealingBonus(int slot_level);
// Preserve Life: 5 x Cleric level shared among Bloodied creatures within 30 feet, none past half its Hit Point
// maximum. A creature's room; the shares (the most room first, each up to its room), in the rooms' order.
int PreserveLifePool(int cleric_level);
int PreserveLifeRoom(int hit_points, int max_hit_points);
std::vector<int> PreserveLifeShares(const std::vector<int>& rooms, int pool);
// The Life Domain Spells, always prepared (not counted against the prepared spells): Aid, Bless, Cure Wounds,
// Lesser Restoration (3); Mass Healing Word, Revivify (5); Aura of Life, Death Ward (7); Greater Restoration, Mass
// Cure Wounds (9) - SRD names.
std::vector<const char*> LifeDomainSpells(int cleric_level);

// Divine Order (Cleric 1; Q2 A: feats in the Cleric's bonus list, a bonus pick at Cleric 1 - data/rows/feat.cleric.txt):
// Protector - Martial weapons and Heavy armour; Thaumaturge - one more Cleric cantrip, and the Wisdom modifier (at
// least +1) on Arcana and Religion checks.
constexpr int kFeatDivineOrderProtector = 1551;
constexpr int kFeatDivineOrderThaumaturge = 1552;
bool IsDivineOrderFeat(int feat);
int ThaumaturgeCheckBonus(int wisdom_modifier);
int ThaumaturgeExtraCantrips(bool thaumaturge);

// Blessed Strikes (7): Divine Strike - once on each of your turns, a weapon hit deals 1d8 Necrotic or Radiant (2d8
// from 14, Improved Blessed Strikes); 0 before 7. Potent Spellcasting at 14: Temporary Hit Points twice the Wisdom
// modifier.
int DivineStrikeDice(int cleric_level);
int PotentSpellcastingTempHp(int wisdom_modifier);

}  // namespace rules
