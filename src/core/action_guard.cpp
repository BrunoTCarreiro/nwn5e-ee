// The order guards (action_guard.h; #106). One hook each on the engine's
// action queue and its spell order, the rules' guards asked in turn.

#include "action_guard.h"

#include <vector>

#include "ee_layout.h"
#include "hooks.h"
#include "log.h"

namespace {

using AddActionFn = void (*)(void* object, uint32_t action, uint16_t group, uint32_t t1, void* p1, uint32_t t2, void* p2,
                             uint32_t t3, void* p3, uint32_t t4, void* p4, uint32_t t5, void* p5, uint32_t t6, void* p6,
                             uint32_t t7, void* p7, uint32_t t8, void* p8, uint32_t t9, void* p9, uint32_t t10,
                             void* p10, uint32_t t11, void* p11, uint32_t t12, void* p12);
AddActionFn g_add_action = nullptr;
AddActionFn g_add_action_to_front = nullptr;
std::vector<ActionGuard> g_action_guards;
bool g_actions_hooked = false;
bool g_actions_failed = false;

struct Vec3 {
  float x, y, z;
};
using AddCastSpellActionsFn = int (*)(void* creature, uint32_t spell, int class_slot, int domain_level, int metamagic,
                                      int spontaneous, Vec3 location, uint32_t target, int area_target,
                                      int add_to_front, int fake, uint8_t projectile_path, int instant, int unknown,
                                      int feat, uint8_t unknown2);
AddCastSpellActionsFn g_add_cast_spell_actions = nullptr;
std::vector<SpellCastGuard> g_spell_guards;
std::vector<SpellOrderGuard> g_spell_order_guards;
bool g_spells_hooked = false;
bool g_spells_failed = false;

bool Refused(void* object, uint32_t action, uint32_t t1, const void* p1) {
  for (ActionGuard guard : g_action_guards) {
    if (guard(object, action, t1, p1)) return true;
  }
  return false;
}

void AddAction(void* object, uint32_t action, uint16_t group, uint32_t t1, void* p1, uint32_t t2, void* p2,
               uint32_t t3, void* p3, uint32_t t4, void* p4, uint32_t t5, void* p5, uint32_t t6, void* p6, uint32_t t7,
               void* p7, uint32_t t8, void* p8, uint32_t t9, void* p9, uint32_t t10, void* p10, uint32_t t11,
               void* p11, uint32_t t12, void* p12) {
  if (Refused(object, action, t1, p1)) return;
  g_add_action(object, action, group, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10,
               t11, p11, t12, p12);
}

void AddActionToFront(void* object, uint32_t action, uint16_t group, uint32_t t1, void* p1, uint32_t t2, void* p2,
                      uint32_t t3, void* p3, uint32_t t4, void* p4, uint32_t t5, void* p5, uint32_t t6, void* p6,
                      uint32_t t7, void* p7, uint32_t t8, void* p8, uint32_t t9, void* p9, uint32_t t10, void* p10,
                      uint32_t t11, void* p11, uint32_t t12, void* p12) {
  if (Refused(object, action, t1, p1)) return;
  g_add_action_to_front(object, action, group, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9,
                        t10, p10, t11, p11, t12, p12);
}

int AddCastSpellActions(void* creature, uint32_t spell, int class_slot, int domain_level, int metamagic,
                        int spontaneous, Vec3 location, uint32_t target, int area_target, int add_to_front, int fake,
                        uint8_t projectile_path, int instant, int unknown, int feat, uint8_t unknown2) {
  for (SpellCastGuard guard : g_spell_guards) {
    if (guard(creature, spell, target)) return 0;
  }
  if (!g_spell_order_guards.empty()) {
    SpellOrder order;
    order.spell = spell;
    order.class_slot = class_slot;
    order.domain_level = domain_level;
    order.metamagic = metamagic;
    order.spontaneous = spontaneous != 0;
    order.target = target;
    order.fake = fake != 0;
    order.x = location.x;
    order.y = location.y;
    order.z = location.z;
    for (SpellOrderGuard guard : g_spell_order_guards) {
      if (guard(creature, order)) return 0;
    }
  }
  return g_add_cast_spell_actions(creature, spell, class_slot, domain_level, metamagic, spontaneous, location, target,
                                  area_target, add_to_front, fake, projectile_path, instant, unknown, feat, unknown2);
}

}  // namespace

bool AddActionGuard(ActionGuard guard) {
  if (!g_actions_hooked && !g_actions_failed) {
    g_actions_hooked =
        HookExport("CNWSObject::AddAction (order guards)", ee::kObjectAddAction, reinterpret_cast<void*>(&AddAction),
                   reinterpret_cast<void**>(&g_add_action)) &&
        HookExport("CNWSObject::AddActionToFront (order guards)", ee::kObjectAddActionToFront,
                   reinterpret_cast<void*>(&AddActionToFront), reinterpret_cast<void**>(&g_add_action_to_front));
    g_actions_failed = !g_actions_hooked;
    if (g_actions_failed) Log("SKIP order guards: the action queue is not hooked");
  }
  if (!g_actions_hooked || !guard) return false;
  g_action_guards.push_back(guard);
  return true;
}

namespace {

bool HookSpellOrders() {
  if (!g_spells_hooked && !g_spells_failed) {
    g_spells_hooked = HookExport("CNWSCreature::AddCastSpellActions (order guards)", ee::kAddCastSpellActions,
                                 reinterpret_cast<void*>(&AddCastSpellActions),
                                 reinterpret_cast<void**>(&g_add_cast_spell_actions));
    g_spells_failed = !g_spells_hooked;
    if (g_spells_failed) Log("SKIP order guards: the spell order is not hooked");
  }
  return g_spells_hooked;
}

}  // namespace

bool AddSpellCastGuard(SpellCastGuard guard) {
  if (!HookSpellOrders() || !guard) return false;
  g_spell_guards.push_back(guard);
  return true;
}

bool AddSpellOrderGuard(SpellOrderGuard guard) {
  if (!HookSpellOrders() || !guard) return false;
  g_spell_order_guards.push_back(guard);
  return true;
}
