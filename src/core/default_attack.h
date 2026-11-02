#pragma once

#include <stdint.h>

// Phase 6 part 2 (#120): the default attack (default_attack.cpp), `actions` faithful.

// The action queue strip (action_queue.cpp): the enemy the default attack goes for once the queue runs dry - armed
// and in a fight - or invalid; a click on its tile disarms it until the next attack order.
uint32_t DefaultAttackTarget(void* creature);
bool CancelDefaultAttack(void* creature);
// #54: another rule turns it off (Disengage: leaving is the point), until the next attack order.
bool StopDefaultAttack(void* creature, const char* why);
