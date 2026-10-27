#include "damage_listeners.h"

#include <vector>

#include "creature.h"
#include "ee_layout.h"
#include "effects.h"
#include "hooks.h"

namespace {

using OnApplyDamageFn = int (*)(void* handler, void* object, void* effect, int loading);

OnApplyDamageFn g_on_apply_damage = nullptr;
std::vector<DamageListener> g_listeners;  // filled at install, then only read
bool g_hooked = false;
bool g_hook_failed = false;

int HitPointsOf(const void* object) {
  return Field<int16_t>(object, ee::kObjectCurrentHitPoints) + Field<int16_t>(object, ee::kObjectTemporaryHitPoints);
}

int OnApplyDamage(void* handler, void* object, void* effect, int loading) {
  void* target = object && !g_listeners.empty() ? CreatureById(Field<uint32_t>(object, ee::kObjectId)) : nullptr;
  const int before = target ? HitPointsOf(target) : 0;
  const int result = g_on_apply_damage(handler, object, effect, loading);
  if (target && effect) {
    const uint32_t creator = effects::Creator(effect);
    const int damage = before - HitPointsOf(target);
    for (DamageListener listener : g_listeners) listener(creator, target, damage);
  }
  return result;
}

}  // namespace

bool AddDamageListener(DamageListener listener) {
  if (!g_hooked && !g_hook_failed) {
    g_hooked = HookExport("CNWSEffectListHandler::OnApplyDamage (damage listeners: Vex / Slow / Push, Retaliation)",
                          ee::kOnApplyDamage, reinterpret_cast<void*>(&OnApplyDamage),
                          reinterpret_cast<void**>(&g_on_apply_damage));
    g_hook_failed = !g_hooked;
  }
  if (!g_hooked) return false;
  g_listeners.push_back(listener);
  return true;
}
