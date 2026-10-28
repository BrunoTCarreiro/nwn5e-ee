#pragma once

#include <stdint.h>

// CNWSCreature::UseItem - an item's power used (the inventory, the radial, the
// quickbar, the scripts' ActionUseItem*): it queues the item's cast. One hook
// for every rule (0.50.0: the potion resume, rules_potion.cpp; the Thief's Use
// Magic Device, rules_thief.cpp). Guards run first: one returning false refuses
// the use - nothing queued (the guard tells the player why). Listeners hear a
// use the engine queued. `item` is the CNWSItem (null if the id is unknown).
using ItemUseGuard = bool (*)(void* creature, uint32_t item_id, void* item);
using ItemUseListener = void (*)(void* creature, uint32_t item_id, void* item);

// Hook UseItem on first use; false (and logged) if it can't.
bool AddItemUseGuard(ItemUseGuard guard);
bool AddItemUseListener(ItemUseListener listener);

// The item with this object id (null if none).
void* ItemById(uint32_t item_id);

// Inside a guard or listener: the index of the item's active property (its
// power) being used; -1 outside one.
int ItemUseActiveProperty();
