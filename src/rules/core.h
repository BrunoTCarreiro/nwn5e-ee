#pragma once

// Core numbers of the 2024 rules (SRD 5.2): ability modifiers, proficiency
// bonus, and the derived save DC / spell attack bonus.

namespace rules {

// floor((score - 10) / 2): 9 -> -1, 10/11 -> 0, 12 -> +1.
int AbilityModifier(int score);

// Character proficiency bonus by total level: +2 at 1-4 ... +6 at 17-20.
// Levels outside 1-20 are clamped (NWN allows up to 40).
int ProficiencyBonusForLevel(int level);

// Creature proficiency bonus by challenge rating: +2 at CR 0-4 ... +9 at 29-30.
// Fractional CRs below 1 count as 0; other CRs round down; CR above 30 is 30.
int ProficiencyBonusForChallengeRating(double challenge_rating);

// 8 + proficiency bonus + ability modifier.
int SaveDifficultyClass(int ability_modifier, int proficiency_bonus);

// Proficiency bonus + ability modifier.
int SpellAttackBonus(int ability_modifier, int proficiency_bonus);

}  // namespace rules
