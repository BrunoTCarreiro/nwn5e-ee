#pragma once

#include <stdint.h>

#include <string>

// Floating text over any object, shown to one player: the engine's own floaty message (what
// FloatingTextStringOnCreature builds - a CNWCCMessageData, type 0x5e, the object and the text), sent to the player
// of `player_creature` whatever the object is. The script command sends it only to the object's own player (or, by
// faction, to its party), so over an NPC no player saw it (0.99.88, #171's TW-9: "no text above the orcs").
// False when the player has no client or a piece of the engine is missing (logged once).
bool FloatTextFor(uint32_t player_creature, uint32_t over, const std::string& text);
