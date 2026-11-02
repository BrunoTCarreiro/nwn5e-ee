#include "item_use.h"

#include <stdint.h>

#include <vector>

#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"

namespace {

struct Vec3 {
  float x, y, z;
};
using UseItemFn = int (*)(void* creature, uint32_t item, uint8_t active_property, uint8_t sub_property,
                          uint32_t target, Vec3 location, uint32_t area, int flag);
using ItemByIdFn = void* (*)(void* app, uint32_t id);

UseItemFn g_use_item = nullptr;
std::vector<ItemUseGuard> g_guards;        // filled at install, then only read
std::vector<ItemUseListener> g_listeners;  // the same
bool g_hooked = false;
bool g_hook_failed = false;
thread_local int t_active_property = -1;
thread_local const ItemUseOrder* t_order = nullptr;  // #120: the use under way

int UseItem(void* creature, uint32_t item, uint8_t active_property, uint8_t sub_property, uint32_t target,
            Vec3 location, uint32_t area, int flag) {
  void* object = creature ? ItemById(item) : nullptr;
  const int outer = t_active_property;
  t_active_property = active_property;
  ItemUseOrder order;
  order.item = item;
  order.active_property = active_property;
  order.sub_property = sub_property;
  order.target = target;
  order.x = location.x;
  order.y = location.y;
  order.z = location.z;
  order.area = area;
  order.flag = flag;
  const ItemUseOrder* outer_order = t_order;
  t_order = &order;
  struct Restore {
    int value;
    const ItemUseOrder* order;
    ~Restore() {
      t_active_property = value;
      t_order = order;
    }
  } restore{outer, outer_order};
  if (creature) {
    for (ItemUseGuard guard : g_guards) {
      if (!guard(creature, item, object)) return 0;
    }
  }
  const int result = g_use_item(creature, item, active_property, sub_property, target, location, area, flag);
  if (result && creature) {
    for (ItemUseListener listener : g_listeners) listener(creature, item, object);
  }
  return result;
}

bool Hook() {
  if (!g_hooked && !g_hook_failed) {
    g_hooked = HookExport("CNWSCreature::UseItem (item-use guards and listeners)", ee::kUseItem,
                          reinterpret_cast<void*>(&UseItem), reinterpret_cast<void**>(&g_use_item));
    g_hook_failed = !g_hooked;
  }
  return g_hooked;
}

}  // namespace

void* ItemById(uint32_t item_id) {
  static const auto item_by_id = reinterpret_cast<ItemByIdFn>(EngineExport(ee::kGetItemByGameObjectId));
  static void** const app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  if (!item_by_id || !app_manager || !*app_manager) return nullptr;
  void* app = Field<void*>(*app_manager, ee::kAppManagerServerApp);
  return app ? item_by_id(app, item_id) : nullptr;
}

int ItemUseActiveProperty() { return t_active_property; }

const ItemUseOrder* CurrentItemUse() { return t_order; }

int UseItemAgain(void* creature, const ItemUseOrder& order) {
  if (!creature || !g_hooked) return 0;
  return UseItem(creature, order.item, order.active_property, order.sub_property, order.target,
                 Vec3{order.x, order.y, order.z}, order.area, order.flag);
}

bool AddItemUseGuard(ItemUseGuard guard) {
  if (!Hook()) return false;
  g_guards.push_back(guard);
  return true;
}

bool AddItemUseListener(ItemUseListener listener) {
  if (!Hook()) return false;
  g_listeners.push_back(listener);
  return true;
}
