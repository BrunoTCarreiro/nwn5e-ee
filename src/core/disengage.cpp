// Phase 6 part 3 (#54; docs/PLAN.md "Phase 6 — Action economy"; the owner, 2026-11-02: "go with your
// recommendations" - 3 B), `actions` faithful: the Disengage action, every player character's (feat 1321 -
// rules::FeatsEveryoneHas2024, granted by rules_features.cpp; spell 1038 -> n5_disengage, data/rows/feat.actions.txt
// and spells.actions.txt; the radial's Special Attacks page, slot 1 over Called Shot: Arm - radial_special_attacks.cpp;
// the quickbar - rules/quickbar.cpp).
// - It takes the round's Action: a feat with a spell and feat.2da ReqAction 1 - NWN's cast path. Given while the
//   character attacks mid-round it waits behind the round's attacks (action_order.cpp); then a cast round, no
//   attacks. Given otherwise it goes at once.
// - Its script: for a round's time (6 s of game time) moving provokes no opportunity attacks (rules_opportunity.cpp
//   asks Disengaging), and the default attack goes off (default_attack.cpp) - leaving is the point; the next attack
//   order turns it on again.
// - Tumble no longer avoids opportunity attacks in faithful (rules_skill_dcs.cpp); NWN mode keeps NWN's Tumble.

#include <stdint.h>

#include <unordered_map>

#include "chat.h"
#include "config.h"
#include "creature.h"
#include "default_attack.h"
#include "ee_layout.h"
#include "game_clock.h"
#include "log.h"
#include "reactions.h"
#include "script_hooks.h"
#include "staple_actions.h"

namespace {

constexpr uint64_t kDisengageMs = 6000;          // its round
std::unordered_map<uint32_t, uint64_t> g_until;  // by creature id: game ms; the engine's thread

bool IsPlayer(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && ReadCreatureFacts(stats).is_player;
}

void DisengageScript(uint32_t self) { StartDisengage(self); }

}  // namespace

void StartDisengage(uint32_t self) {
  void* creature = CreatureById(self);
  if (!creature) return;
  g_until[self] = GameMs() + kDisengageMs;
  StopDefaultAttack(creature, "it Disengaged");
  Log("actions: 0x%08X Disengage - moving provokes no opportunity attacks until its next round", self);
  if (IsPlayer(creature)) {
    TellPlayer(self, "nwn5e: Disengage - until your next round, moving away provokes no opportunity attacks.");
  }
}

bool Disengaging(uint32_t id) {
  if (g_until.empty()) return false;
  const auto it = g_until.find(id);
  if (it == g_until.end()) return false;
  if (GameMs() < it->second) return true;
  g_until.erase(it);
  return false;
}

void InstallDisengage() {
  if (g_config.actions != rules::RuleVariant::kFaithful) return;
  const bool script = AddScriptHandler("n5_disengage", &DisengageScript);
  Log("actions: Disengage - %s", script ? "every player character's action (feat 1321): no opportunity attacks from "
                                          "moving for its round"
                                        : "NOT available (no script hook)");
}
