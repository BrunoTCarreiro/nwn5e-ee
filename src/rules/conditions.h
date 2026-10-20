#pragma once

// Conditions (PLAN.md "Conditions — decided by the owner", 2026-10-19): what
// a creature's conditions do to saves and attack rolls, in the faithful
// (2024) and NWN-mode versions. The engine side reads the conditions from the
// creature's effects; these functions only decide.

#include "rules/modes.h"
#include "rules/saves.h"

namespace rules {

// The conditions one creature has that change its saves or attacks.
struct Conditions {
  bool stunned = false;     // NWN state 6
  bool paralyzed = false;   // NWN state 8 (Hold Person and the like)
  bool asleep = false;      // NWN state 9 (2024: Unconscious)
  bool restrained = false;  // NWN entangle (Entangle, Web, ...)
  bool poisoned = false;    // an NWN poison effect is on it
  bool frightened_source_in_sight = false;  // frightened, and it can see the source
  bool concealed = false;   // an NWN concealment effect (Blur, Displacement, ...)
};

enum class SaveOutcome { kRoll, kAutoFail };

// CN2/CN3: a stunned, paralyzed or unconscious creature automatically fails
// Strength and Dexterity saves. Stunned: both modes (in NWN mode the save is
// Reflex, i.e. `ability` Dexterity). Paralyzed and unconscious: faithful only.
// `ability` is the save's 2024 ability (NWN mode: the NWN save's mapping).
SaveOutcome ConditionSaveOutcome(const Conditions& creature, Ability ability, RuleVariant variant);

// CN1: a restrained creature has disadvantage on Dexterity saves (faithful).
int ConditionSaveDisadvantage(const Conditions& creature, Ability ability, RuleVariant variant);

// Advantage / disadvantage sources that conditions add to one attack roll,
// counted like the situational ones (rules::SituationalRollMode extras).
struct RollSources {
  int advantage = 0;
  int disadvantage = 0;
};

// Both modes: target stunned, paralyzed or unconscious (CN2/CN3): advantage
// (NWN gives paralyzed / sleeping targets no attack bonus, only flat-footed;
// stunned already has B1's term, this covers a stunned creature that also
// holds a higher state). Faithful only:
//   target restrained (CN1): advantage;
//   target concealed (CN6, NWN's miss chance removed): disadvantage;
//   attacker restrained (CN1), poisoned (CN4), frightened with the source in
//   sight (CN5): disadvantage.
RollSources ConditionAttackSources(const Conditions& attacker, const Conditions& target, RuleVariant variant);

// CN2 (faithful): a hit on a paralyzed or unconscious creature from an
// attacker within 5 ft is a critical hit (2024: melee or ranged).
bool ConditionForcesCritical(const Conditions& target, bool within_five_feet, RuleVariant variant);

}  // namespace rules
