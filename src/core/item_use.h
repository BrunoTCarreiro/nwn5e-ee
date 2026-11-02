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

// Phase 6 part 2 (#120): a use's whole order - kept by a guard that refuses it
// for now (a potion waiting for the next round's Bonus Action) and given again
// later through the hook (its guards and listeners run again).
struct ItemUseOrder {
  uint32_t item = 0x7F000000;
  uint8_t active_property = 0;
  uint8_t sub_property = 0;
  uint32_t target = 0x7F000000;
  float x = 0.0f, y = 0.0f, z = 0.0f;
  uint32_t area = 0x7F000000;
  int flag = 0;
};
// Inside a guard or listener: the use's order (null outside one).
const ItemUseOrder* CurrentItemUse();
int UseItemAgain(void* creature, const ItemUseOrder& order);
