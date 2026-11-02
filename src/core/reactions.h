#pragma once

#include <stdint.h>

#include "rules/action_economy.h"

// D2's one reaction a round (rules_opportunity.cpp): the round's opportunity
// attacks left (CNWSCombatRound +0x2f40), which the engine refills when the
// creature's own combat round starts.
//
// Takes a creature's reaction for a rule (the Rogue's Uncanny Dodge, the Monk's
// Deflect Attacks): false when it's spent this round. A creature whose round
// isn't running (not fighting - standing still while shot at) never gets the
// engine's refill, so a reaction a rule took comes back a round (6 s of game
// time) later when none of the creature's rounds ended since (0.60.7; the
// owner's M8-1: Deflect Attacks once, then "reaction spent" rounds later).
bool TakeReaction(void* creature);

// Phase 6 part 3 (#54; owner, 2026-11-02): the reaction switches (Combat
// configuration's "Reactions"; rules::ReactionOn) - a player character's own,
// the record nwn5e_reactions; every other creature takes all its reactions, and
// so does everyone while the actions aren't faithful.
bool ReactionAllowed(void* creature, rules::Reaction reaction);
bool ReactionSwitchOn(void* creature, rules::Reaction reaction);
void SetReactionSwitch(void* creature, rules::Reaction reaction, bool on);

// #54 (3 B): a creature that took the Disengage action (disengage.cpp) - its
// moving provokes no opportunity attacks until its next round.
bool Disengaging(uint32_t id);
