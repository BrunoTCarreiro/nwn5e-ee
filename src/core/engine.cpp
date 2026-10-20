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
using VarGetIntFn = int (*)(void* table, void* name);
using VarSetIntFn = void (*)(void* table, void* name, int value, int flag);
using ExoStringFn = void* (*)(void* string, const char* text);
using ExoStringDestroyFn = void (*)(void* string);
using StatFn = uint8_t (*)(void* stats);
using CalcModifierFn = int8_t (*)(void* stats, uint8_t score);
using DexModFn = int8_t (*)(void* stats, int armor_dex_cap);
using AbilityModFn = int8_t (*)(void* stats, uint8_t ability);
using IsDmFn = int (*)(void* stats);

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
  VarGetIntFn var_get_int = reinterpret_cast<VarGetIntFn>(EngineExport(ee::kVarTableGetInt));
  VarSetIntFn var_set_int = reinterpret_cast<VarSetIntFn>(EngineExport(ee::kVarTableSetInt));
  ExoStringFn make_string = reinterpret_cast<ExoStringFn>(EngineExport(ee::kExoStringFromChars));
  ExoStringDestroyFn destroy_string = reinterpret_cast<ExoStringDestroyFn>(EngineExport(ee::kExoStringDestructor));
  StatFn strength = reinterpret_cast<StatFn>(EngineExport(ee::kGetSTRStat));
  CalcModifierFn calc_modifier = reinterpret_cast<CalcModifierFn>(EngineExport(ee::kCalcStatModifier));
  DexModFn dex_mod = reinterpret_cast<DexModFn>(EngineExport(ee::kGetDEXMod));
  AbilityModFn ability_mod = reinterpret_cast<AbilityModFn>(EngineExport(ee::kGetAbilityMod));
  IsDmFn is_dm = reinterpret_cast<IsDmFn>(EngineExport(ee::kGetIsDM));
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

// A CExoString the engine builds and frees (its buffer is the engine's).
class EngineString {
 public:
  explicit EngineString(const char* text) { E().make_string(bytes_, text); }
  ~EngineString() { E().destroy_string(bytes_); }
  void* get() { return bytes_; }

 private:
  alignas(16) uint8_t bytes_[32] = {};
};

bool VarsAvailable() { return E().var_get_int && E().var_set_int && E().make_string && E().destroy_string; }

int GetLocalInt(void* object, const char* name) {
  if (!object || !VarsAvailable()) return 0;
  EngineString text(name);
  return E().var_get_int(static_cast<uint8_t*>(object) + ee::kObjectVarTable, text.get());
}

void SetLocalInt(void* object, const char* name, int value) {
  if (!object || !VarsAvailable()) return;
  EngineString text(name);
  E().var_set_int(static_cast<uint8_t*>(object) + ee::kObjectVarTable, text.get(), value, 0);
}

int StrengthModifier(void* stats) {
  return stats && E().strength && E().calc_modifier ? E().calc_modifier(stats, E().strength(stats)) : 0;
}

int DexterityModifier(void* stats) { return stats && E().dex_mod ? E().dex_mod(stats, 0) : 0; }

int AbilityModifier(void* stats, int ability) {
  if (!stats || ability < 0 || ability > 5 || !E().ability_mod) return 0;
  return E().ability_mod(stats, static_cast<uint8_t>(ability));
}

void* AreaOf(void* object) { return object && E().area ? E().area(object) : nullptr; }

bool IsDungeonMaster(void* stats) { return stats && E().is_dm && E().is_dm(stats) != 0; }

bool HasNoOrders(void* creature) {
  if (!creature) return false;
  const void* list = Field<void*>(creature, ee::kObjectActionQueue);
  const void* head = list ? Field<void*>(list, ee::kListHead) : nullptr;
  return !head && Field<uint16_t>(creature, ee::kObjectCurrentAction) == ee::kActionNone;
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
