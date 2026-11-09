#include "rules/cleric.h"

#include <algorithm>
#include <numeric>

namespace rules {

int ChannelDivinityUses(int cleric_level) {
  if (cleric_level >= 18) return 4;
  if (cleric_level >= 6) return 3;
  if (cleric_level >= 2) return 2;
  return 0;
}

int ChannelDivinityAfterShortRest(int left, int cleric_level) {
  return std::min(left + 1, ChannelDivinityUses(cleric_level));
}

int DivineSparkDice(int cleric_level) {
  if (cleric_level >= 18) return 4;
  if (cleric_level >= 13) return 3;
  if (cleric_level >= 7) return 2;
  return 1;
}

int SearUndeadDice(int wisdom_modifier) { return std::max(1, wisdom_modifier); }

int LifeHealingBonus(int slot_level) { return slot_level > 0 ? 2 + slot_level : 0; }

int PreserveLifePool(int cleric_level) { return 5 * cleric_level; }

int PreserveLifeRoom(int hit_points, int max_hit_points) { return std::max(0, max_hit_points / 2 - hit_points); }

std::vector<int> PreserveLifeShares(const std::vector<int>& rooms, int pool) {
  std::vector<size_t> order(rooms.size());
  std::iota(order.begin(), order.end(), size_t{0});
  std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return rooms[a] > rooms[b]; });
  std::vector<int> shares(rooms.size(), 0);
  for (size_t i : order) {
    const int share = std::max(0, std::min(rooms[i], pool));
    shares[i] = share;
    pool -= share;
  }
  return shares;
}

std::vector<const char*> LifeDomainSpells(int cleric_level) {
  std::vector<const char*> spells;
  if (cleric_level >= 3) spells.insert(spells.end(), {"Aid", "Bless", "Cure Wounds", "Lesser Restoration"});
  if (cleric_level >= 5) spells.insert(spells.end(), {"Mass Healing Word", "Revivify"});
  if (cleric_level >= 7) spells.insert(spells.end(), {"Aura of Life", "Death Ward"});
  if (cleric_level >= 9) spells.insert(spells.end(), {"Greater Restoration", "Mass Cure Wounds"});
  return spells;
}

int ThaumaturgeCheckBonus(int wisdom_modifier) { return std::max(1, wisdom_modifier); }

int DivineStrikeDice(int cleric_level) {
  if (cleric_level >= 14) return 2;
  if (cleric_level >= 7) return 1;
  return 0;
}

int PotentSpellcastingTempHp(int wisdom_modifier) { return std::max(0, 2 * wisdom_modifier); }

}  // namespace rules
