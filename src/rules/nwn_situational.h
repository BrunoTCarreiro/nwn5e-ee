#pragma once

// Phase 1b (PLAN.md, decided 2026-10-18): NWN's situational attack modifiers
// become 2024 advantage/disadvantage. The engine
// adds them as flat 3e bonuses inside GetAttackModifierVersus (RE-NOTES); the
// hook works out which ones it applied, takes their total back out and rolls
// the d20 with the matching roll mode instead.

#include "rules/d20.h"

namespace rules {

// The situational terms NWN applied to one attack roll.
struct NwnSituationalTerms {
  bool ranged = false;
  bool target_blind = false;        // +2 (not against Blind-Fight in melee)
  bool attacker_invisible = false;  // +2: the target can't see the attacker (same exception)
  bool cant_see_target = false;     // -4
  bool target_moving = false;       // -2, ranged attacks only
  bool target_prone = false;        // melee +4, ranged -4
  bool target_stunned = false;      // +2
  bool target_flanked = false;      // +2 (ranged: within 10 m)
  bool ranged_in_melee = false;     // -4: target within its melee reach, no Point Blank Shot
};

// The flat total those terms added to the engine's attack modifier.
int NwnSituationalTotal(const NwnSituationalTerms& terms);

// B5/B3: a ranged attack is in close combat when at least one enemy within
// its own melee reach of the attacker can see the attacker and is not
// incapacitated — unless the attacker has Point Blank Shot.
bool RangedCloseCombat(bool ranged, bool point_blank_shot, int threatening_enemies);

// The 2024 roll mode. Advantage: target blinded or stunned, attacker unseen,
// prone target in melee. Disadvantage: target unseen, prone target at range,
// a ranged attack in close combat (`ranged_close_combat`, the 2024 check —
// NWN's target-only term is not used). Nothing for a moving target (no 2024
// rule) or flanking (B2: off). Sources cancel as in ResolveRollMode.
// `extra_advantage` / `extra_disadvantage`: other sources (weapon mastery:
// Vex, Sap) that count like the situational ones.
// R11 Elusive (Rogue 18): `no_advantage` - no attack roll against the target
// has Advantage (its Disadvantage sources still count).
RollMode SituationalRollMode(const NwnSituationalTerms& terms, bool ranged_close_combat, int extra_advantage = 0,
                             int extra_disadvantage = 0, bool no_advantage = false);

}  // namespace rules
