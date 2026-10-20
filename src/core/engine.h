#pragma once

// Engine reads shared by the rules: feats, items, races, the area's
// creatures, reactions, sight and distances (EE port of 1.69's
// engine_queries; exported functions where EE has them, offsets in
// ee_layout.h).

#include <stdint.h>

namespace engine {

bool HasFeat(void* stats, int feat);
void* ItemInSlot(void* creature, int slot);  // CNWSItem*, or null
int BaseItemRow(const void* item);           // baseitems.2da row, -1 for no item
int BaseItemRowInSlot(void* creature, int slot);
bool IsDoubleWeapon(int base_item_row);      // baseitems.2da WeaponWield 8
int RaceOf(void* stats);                     // racialtypes.2da row, -1 if unknown

bool IsEnemy(void* creature, uint32_t other_id);  // GetAIStateReaction == enemy
bool IsDeadOrDying(void* creature);
bool Sees(void* creature, uint32_t other_id);     // its visibility list, bit 0

// Centre-to-centre distance squared, and edge to edge (minus both personal spaces).
float CentreDistanceSquared(const void* a, const void* b);
float EdgeDistance(const void* a, const void* b);

// Script local variables on an object (saved with the game): its
// CNWSScriptVarTable GetInt / SetInt. 0 when unset.
int GetLocalInt(void* object, const char* name);
void SetLocalInt(void* object, const char* name, int value);

// Ability modifiers as the engine computes them.
int StrengthModifier(void* stats);
int DexterityModifier(void* stats);

// Calls fn(id, creature) for each other creature in `creature`'s area.
using CreatureVisitor = void (*)(void* context, uint32_t id, void* creature);
void ForEachOtherCreatureInArea(void* creature, CreatureVisitor visit, void* context);

}  // namespace engine
