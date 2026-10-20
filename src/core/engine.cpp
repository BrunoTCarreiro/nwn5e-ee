#include "engine.h"

#include <math.h>

#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"

namespace engine {

namespace {

using HasFeatFn = int (*)(void* stats, uint16_t feat);
using GetItemInSlotFn = void* (*)(void* inventory, uint32_t slot);
using GetBaseItemFn = const void* (*)(void* base_items, int row);
using GetAreaFn = void* (*)(void* object);
using ReactionFn = uint8_t (*)(void* object, uint32_t other_id);
using ObjectCheckFn = int (*)(void* object);
using VisibleElementFn = const void* (*)(void* creature, uint32_t other_id);

struct Api {
  HasFeatFn has_feat = reinterpret_cast<HasFeatFn>(EngineExport(ee::kHasFeat));
  GetItemInSlotFn item_in_slot = reinterpret_cast<GetItemInSlotFn>(EngineExport(ee::kGetItemInSlot));
  GetBaseItemFn base_item = reinterpret_cast<GetBaseItemFn>(EngineExport(ee::kGetBaseItem));
  void** rules = reinterpret_cast<void**>(EngineExport(ee::kRules));
  GetAreaFn area = reinterpret_cast<GetAreaFn>(EngineExport(ee::kGetArea));
  ReactionFn reaction = reinterpret_cast<ReactionFn>(EngineExport(ee::kGetAIStateReaction));
  ObjectCheckFn dead = reinterpret_cast<ObjectCheckFn>(EngineExport(ee::kGetDead));
  ObjectCheckFn pc_dying = reinterpret_cast<ObjectCheckFn>(EngineExport(ee::kGetIsPCDying));
  VisibleElementFn visible = reinterpret_cast<VisibleElementFn>(EngineExport(ee::kGetVisibleListElement));
};

const Api& E() {
  static const Api api;
  return api;
}

float PersonalSpace(const void* creature) {
  const void* path_info = Field<const void*>(creature, ee::kCreaturePathInfo);
  return path_info ? Field<float>(path_info, ee::kPathInfoPersonalSpace) : 0.0f;
}

}  // namespace

bool HasFeat(void* stats, int feat) { return stats && E().has_feat && E().has_feat(stats, static_cast<uint16_t>(feat)); }

void* ItemInSlot(void* creature, int slot) {
  void* inventory = creature ? Field<void*>(creature, ee::kCreatureInventory) : nullptr;
  return inventory && E().item_in_slot ? E().item_in_slot(inventory, static_cast<uint32_t>(slot)) : nullptr;
}

int BaseItemRow(const void* item) { return item ? Field<int32_t>(item, ee::kItemBaseItem) : -1; }

int BaseItemRowInSlot(void* creature, int slot) { return BaseItemRow(ItemInSlot(creature, slot)); }

bool IsDoubleWeapon(int base_item_row) {
  if (base_item_row < 0 || !E().rules || !*E().rules || !E().base_item) return false;
  void* base_items = Field<void*>(*E().rules, ee::kRulesBaseItems);
  const void* base_item = base_items ? E().base_item(base_items, base_item_row) : nullptr;
  return base_item && Field<uint8_t>(base_item, ee::kBaseItemWield) == ee::kWieldDouble;
}

int RaceOf(void* stats) { return stats ? Field<uint16_t>(stats, ee::kStatsRace) : -1; }

bool IsEnemy(void* creature, uint32_t other_id) {
  return creature && E().reaction && E().reaction(creature, other_id) == ee::kReactionEnemy;
}

bool IsDeadOrDying(void* creature) {
  return creature && ((E().dead && E().dead(creature)) || (E().pc_dying && E().pc_dying(creature)));
}

bool Sees(void* creature, uint32_t other_id) {
  if (!creature || !E().visible) return false;
  const void* node = E().visible(creature, other_id);
  return node && (Field<uint8_t>(node, ee::kVisibilityFlags) & 1) != 0;
}

float CentreDistanceSquared(const void* a, const void* b) {
  const float dx = Field<float>(a, ee::kObjectPositionX) - Field<float>(b, ee::kObjectPositionX);
  const float dy = Field<float>(a, ee::kObjectPositionY) - Field<float>(b, ee::kObjectPositionY);
  return dx * dx + dy * dy;
}

float EdgeDistance(const void* a, const void* b) {
  return sqrtf(CentreDistanceSquared(a, b)) - PersonalSpace(a) - PersonalSpace(b);
}

void ForEachOtherCreatureInArea(void* creature, CreatureVisitor visit, void* context) {
  if (!creature || !E().area) return;
  void* area = E().area(creature);
  if (!area) return;
  const uint32_t* ids = Field<const uint32_t*>(area, ee::kAreaObjectIds);
  const int count = Field<int32_t>(area, ee::kAreaObjectCount);
  if (!ids || count <= 0 || count > 100000) return;
  const uint32_t self = Field<uint32_t>(creature, ee::kObjectId);
  for (int i = 0; i < count; ++i) {
    if (ids[i] == self) continue;
    if (void* other = CreatureById(ids[i])) visit(context, ids[i], other);
  }
}

}  // namespace engine
