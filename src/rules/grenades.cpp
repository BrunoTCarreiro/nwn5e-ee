#include "rules/grenades.h"

namespace rules {

Grenade GrenadeForSpell(int spell_id) {
  switch (spell_id) {
    case 464: return Grenade::kAlchemistsFire;
    case 465: return Grenade::kTanglefoot;
    case 466: return Grenade::kHolyWater;
    case 467: return Grenade::kChokingPowder;
    case 468: return Grenade::kThunderstone;
    case 469: return Grenade::kAcid;
    case 471: return Grenade::kCaltrops;
    case 744: return Grenade::kFireBomb;
    case 745: return Grenade::kAcidBomb;
    default: return Grenade::kNone;
  }
}

bool IsGrenadeSpell(int spell_id) { return GrenadeForSpell(spell_id) != Grenade::kNone; }

int GrenadeSaveDc(int dexterity_modifier, int proficiency_bonus) { return 8 + dexterity_modifier + proficiency_bonus; }

bool GrenadeSaveOnThrowerDc(Grenade grenade) { return grenade != Grenade::kNone && grenade != Grenade::kCaltrops; }

GrenadeDice GrenadeHitDice(Grenade grenade) {
  switch (grenade) {
    case Grenade::kAcid: return {2, 6};
    case Grenade::kAlchemistsFire: return {1, 4};
    case Grenade::kHolyWater: return {2, 8};
    case Grenade::kFireBomb:
    case Grenade::kAcidBomb: return {10, 6};
    default: return {};
  }
}

bool GrenadeHalfOnSave(Grenade grenade) { return grenade == Grenade::kFireBomb || grenade == Grenade::kAcidBomb; }

bool ThrowGoesNow(bool attacking_mid_round, bool grenade, bool faithful) {
  return faithful && grenade && attacking_mid_round;
}

unsigned GrenadeDescription2024(unsigned game_strref) {
  switch (game_strref) {
    case 2989: return 16000555;   // Acid Flask
    case 2990: return 16000556;   // Alchemist's Fire
    case 2991: return 16000557;   // Caltrops
    case 2992: return 16000558;   // Choking Powder
    case 2994: return 16000559;   // Holy Water
    case 2995: return 16000560;   // Tanglefoot Bag
    case 2997: return 16000561;   // Thunderstone
    case 84353: return 16000562;  // Fire Bomb
    case 84354: return 16000563;  // Acid Bomb
    default: return 0;
  }
}

}  // namespace rules
