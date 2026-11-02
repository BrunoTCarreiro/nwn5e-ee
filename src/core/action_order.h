#pragma once

#include <vector>

// Phase 6 part 2 (#120): the Action queue (action_order.cpp), `actions` faithful.

// The actions (CNWSObjectActionNode*) in a creature's queue, in order.
std::vector<void*> QueueNodes(void* creature);
// After an order a creature took, `before` its queue: when it attacks mid-round, the order's new actions go after
// the round's attacks (they wait for the round's end - a spell) or before them (they don't - a potion).
void PlaceNewOrder(void* creature, const std::vector<void*>& before, const char* what);
