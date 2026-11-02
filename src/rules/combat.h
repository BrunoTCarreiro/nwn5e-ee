#pragma once

// Attack rolls, damage and attacks per Attack action (SRD 5.2).

#include <vector>

#include "rules/dice.h"

namespace rules {

// Which ability a weapon attack uses: Strength (melee), Dexterity (ranged),
// or the better of the two (Finesse).
enum class AttackAbility { kStrength, kDexterity, kFinesse };
int AttackAbilityModifier(AttackAbility ability, int strength_modifier, int dexterity_modifier);

// Ability modifier + proficiency bonus (if proficient) + other bonuses (magic weapon, etc.).
int AttackBonus(int ability_modifier, int proficiency_bonus, bool proficient, int other_bonus);

enum class AttackOutcome { kMiss, kHit, kCritical };

// Resolves an attack roll. A natural 1 misses and a natural 20 is a Critical
// Hit regardless of AC. `critical_floor` is the lowest natural roll that can
// be critical (20 normally, 19 with Improved Critical); a value above 20
// (the engine's "cannot crit") still lets a natural 20 be critical.
// Ruling (docs/PLAN.md, decision 8): an expanded range only upgrades a
// hit — a natural 19 that does not reach the AC still misses.
AttackOutcome ResolveAttackRoll(int natural_roll, int attack_bonus, int armor_class, int critical_floor);

struct DamageDice {
  int count;
  int sides;
};

// Rolls every damage die group (weapon dice, Sneak Attack, smite...) and adds
// `modifier` once. On a Critical Hit each group's dice are rolled twice;
// the modifier is not doubled. The total never drops below 0.
int RollDamage(const std::vector<DamageDice>& dice, int modifier, bool critical, Dice& roller);

enum class CharacterClass {
  kBarbarian, kBard, kCleric, kDruid, kFighter, kMonk,
  kPaladin, kRanger, kRogue, kSorcerer, kWarlock, kWizard,
};

struct ClassLevels {
  CharacterClass character_class;
  int levels;
};

// Attacks made with one Attack action. Extra Attack (Barbarian, Fighter, Monk,
// Paladin, Ranger at 5) gives 2; a Fighter gets 3 at 11 and 4 at 20. Extra
// Attack from several classes does not add up.
int AttacksPerAttackAction(const std::vector<ClassLevels>& classes);

// D2's one reaction a round, spent (0.60.7): one a rule took comes back once a
// round's time (6 s) went by with none of the creature's own combat rounds
// ended since - it isn't fighting, so the engine's round start never refilled
// it. #54 (0.63.1; the owner's RE-7: Retaliation "didn't proc" on a Barbarian
// standing still): one spent otherwise (an opportunity attack, or none of its
// rounds ran yet) comes back while no round of its own runs - the engine only
// refills it as one starts; while its rounds run, the engine's refill.
constexpr long long kReactionRoundMs = 6000;
bool ReactionBackWithoutRounds(bool taken_by_rule, bool round_ended_since, long long ms_since, bool round_running);

}  // namespace rules
