#include "rest.h"

#include <vector>

#include "chat.h"
#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"

namespace {

using ResetFn = void (*)(void* stats);
ResetFn g_reset = nullptr;
std::vector<RestListener> g_listeners;  // filled at install, then only read
bool g_hooked = false;
bool g_hook_failed = false;

void ResetFeatRemainingUses(void* stats) {
  g_reset(stats);
  for (RestListener listener : g_listeners) listener(stats);
}

}  // namespace

namespace {
std::vector<ShortRestListener> g_short_rest_listeners;  // filled at install, then only read

using RestFn = void (*)(void* creature, int associate, int enemy_line_of_sight);
RestFn g_rest = nullptr;
std::vector<RestStartListener> g_start_listeners;  // filled at install, then only read
std::vector<RestGate> g_gates;                     // #135; filled at install, then only read
bool g_rest_hooked = false;
bool g_rest_hook_failed = false;

void Rest(void* creature, int associate, int enemy_line_of_sight) {
  for (RestGate gate : g_gates) {
    const std::string why = creature ? gate(creature) : std::string();
    if (why.empty()) continue;
    if (Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) != 0) {
      TellPlayer(Field<uint32_t>(creature, ee::kObjectId), "nwn5e: " + why);
    }
    return;  // the rest doesn't start
  }
  g_rest(creature, associate, enemy_line_of_sight);
  if (!creature || !(Field<uint32_t>(creature, ee::kCreatureActivities) & ee::kActivityResting)) return;
  for (RestStartListener listener : g_start_listeners) listener(creature);
}
}  // namespace

namespace {
bool HookRest() {
  if (!g_rest_hooked && !g_rest_hook_failed) {
    g_rest_hooked = HookExport("CNWSCreature::Rest (a rest begins)", ee::kCreatureRest, reinterpret_cast<void*>(&Rest),
                               reinterpret_cast<void**>(&g_rest));
    g_rest_hook_failed = !g_rest_hooked;
  }
  return g_rest_hooked;
}
}  // namespace

bool AddRestStartListener(RestStartListener listener) {
  if (!HookRest() || !listener) return false;
  g_start_listeners.push_back(listener);
  return true;
}

bool AddRestGate(RestGate gate) {
  if (!HookRest() || !gate) return false;
  g_gates.push_back(gate);
  return true;
}

void AddShortRestListener(ShortRestListener listener) { g_short_rest_listeners.push_back(listener); }

std::string AfterShortRest(void* creature) {
  std::string words;
  for (ShortRestListener listener : g_short_rest_listeners) words += listener(creature);
  return words;
}

bool AddRestListener(RestListener listener) {
  if (!g_hooked && !g_hook_failed) {
    g_hooked = HookExport("CNWSCreatureStats::ResetFeatRemainingUses (after a rest)", ee::kResetFeatRemainingUses,
                          reinterpret_cast<void*>(&ResetFeatRemainingUses), reinterpret_cast<void**>(&g_reset));
    g_hook_failed = !g_hooked;
  }
  if (!g_hooked) return false;
  g_listeners.push_back(listener);
  return true;
}
