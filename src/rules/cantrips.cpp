#include "rules/cantrips.h"

#include <algorithm>

namespace rules {

namespace {

// SRD 5.2.1's cantrips (CC-BY-4.0): the die, the damage type and how it reaches its target.
const std::vector<Cantrip> kCantrips = {
    {100, "n5_c_light", 0, 0, CantripRoll::kNone},                                  // Light (NWN's row)
    {144, "n5_c_rayfrost", 8, kDamageTypeCold, CantripRoll::kRangedSpellAttack},   // Ray of Frost (NWN's row)
    {424, "n5_c_acidsplash", 6, kDamageTypeAcid, CantripRoll::kDexteritySave},     // Acid Splash (NWN's row)
    {1050, "n5_c_firebolt", 10, kDamageTypeFire, CantripRoll::kRangedSpellAttack},
    {1051, "n5_c_chilltouch", 10, kDamageTypeNecrotic, CantripRoll::kMeleeSpellAttack},
    {1052, "n5_c_shockgrasp", 8, kDamageTypeLightning, CantripRoll::kMeleeSpellAttack},
    {1053, "n5_c_poisonspray", 12, kDamageTypePoison, CantripRoll::kRangedSpellAttack},
    {1054, "n5_c_sacredflame", 8, kDamageTypeRadiant, CantripRoll::kDexteritySave},
    {1055, "n5_c_starrywisp", 8, kDamageTypeRadiant, CantripRoll::kRangedSpellAttack},
    {1056, "n5_c_mockery", 6, kDamageTypePsychic, CantripRoll::kWisdomSave},
};

// Daze 37, Virtue 189, Cure Minor Wounds 33, Flare 416, Inflict Minor Wounds 431, Electric Jolt 439.
const std::vector<int> kRetired = {33, 37, 189, 416, 431, 439};

}  // namespace

const std::vector<Cantrip>& Cantrips() { return kCantrips; }

const Cantrip* CantripOf(int spell) {
  const auto at = std::lower_bound(kCantrips.begin(), kCantrips.end(), spell,
                                   [](const Cantrip& cantrip, int id) { return cantrip.spell < id; });
  return at != kCantrips.end() && at->spell == spell ? &*at : nullptr;
}

int CantripDice(int character_level) {
  return 1 + (character_level >= 5 ? 1 : 0) + (character_level >= 11 ? 1 : 0) + (character_level >= 17 ? 1 : 0);
}

int CantripDiceRolled(int character_level, bool critical) {
  return CantripDice(character_level) * (critical ? 2 : 1);
}

const std::vector<int>& RetiredCantrips() { return kRetired; }

bool IsRetiredCantrip(int spell) { return std::binary_search(kRetired.begin(), kRetired.end(), spell); }

}  // namespace rules
