#pragma once

#include <stdint.h>

// Phase 6 (#56): the staple actions (staple_actions.cpp), `actions` faithful.

// Dodge taken (rules_monk.cpp's Dodge - the Disadvantage on attacks, the Dexterity saves' Advantage - asks it).
bool Dodging(uint32_t id);
// #53 (Cunning Action, extra_actions.cpp): a Dash or a Disengage started for `id` (as the actions do).
void StartDash(uint32_t id);
void StartDisengage(uint32_t id);  // disengage.cpp
