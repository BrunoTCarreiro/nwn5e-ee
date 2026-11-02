#pragma once

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
