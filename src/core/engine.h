#pragma once

// Engine reads shared by the rules: feats, items, races, the area's
// creatures, reactions, sight and distances (EE port of 1.69's
// engine_queries; exported functions where EE has them, offsets in
// ee_layout.h).

#include <stdint.h>

#include <string>

namespace engine {

// A copy of an engine std::string (MSVC layout: the characters inline below
// capacity 16, else behind a pointer at +0; size +0x10, capacity +0x18) — read
// only, never freed by nwn5e; "" when it doesn't look like one.
std::string StdString(const void* engine_string);

bool HasFeat(void* stats, int feat);
// The feat added to / removed from the character's own list (the client
// follows on the next player update); AddFeat is false when it didn't take.
bool AddFeat(void* stats, int feat);
void RemoveFeat(void* stats, int feat);
void* ItemInSlot(void* creature, int slot);  // CNWSItem*, or null
int BaseItemRow(const void* item);           // baseitems.2da row, -1 for no item
int BaseItemRowInSlot(void* creature, int slot);
bool IsDoubleWeapon(int base_item_row);      // baseitems.2da WeaponWield 8
const void* BaseItem(int base_item_row);     // CNWBaseItem*, or null
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

// Ability modifier 0 Str .. 5 Cha, as the engine caches it (GetAbilityMod).
int AbilityModifier(void* stats, int ability);

// The object's area (CNWSArea*), or null.
void* AreaOf(void* object);

// A Dungeon Master's creature (GetIsDM): its skills are always 127.
bool IsDungeonMaster(void* stats);

// No orders: its action queue is empty and it carries out none (outside the
// engine's RunActions, an action in progress sits at the queue's head).
bool HasNoOrders(void* creature);

// Calls fn(id, creature) for each other creature in `creature`'s area.
using CreatureVisitor = void (*)(void* context, uint32_t id, void* creature);
void ForEachOtherCreatureInArea(void* creature, CreatureVisitor visit, void* context);

}  // namespace engine
