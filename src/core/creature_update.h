#pragma once

// CNWSCreature::AIUpdate — once per creature per pass of the server's AI
// loop. Rules that act on a creature over time (the Push slide and fall)
// listen here; listeners run after the engine's own update, so the engine's
// walking can't undo what they set.
using CreatureUpdateListener = void (*)(void* creature);

// Hooks it on first use; false (and logged) if it can't.
bool AddCreatureUpdateListener(CreatureUpdateListener listener);
