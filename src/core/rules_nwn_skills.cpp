// SK8 (port of 1.69's rules_nwn_skills.cpp; docs/EE-PORT.md E3b, EP14),
// `nwn_only_skills` faithful: the 3e-only skills do nothing — 2024 has none
// of them.
// - Parry: the mode is refused (EP14) — CNWSCreature::SetCombatMode is the
//   only writer of the mode, so player toggles, scripts and the AI all meet
//   it; the player is told once.
// - Taunt: the animation plays; no roll, effect or hostility (the second pass
//   of AIActionTaunt is ended the way the engine ends it).
// - Appraise: stores keep their own prices — the markup / markdown a script
//   passes to OpenStore (the campaigns' Appraise check, or any other script
//   discount) is recorded as 0.
// - Use Magic Device never lifts an item's class, race or alignment limits,
//   and Holy Avenger gives a non-paladin its plain +2: GetSkillRank(19) is
//   -127 inside CheckUseMagicDeviceSkill, CheckItemAlignmentRestrictions and
//   ApplyHolyAvenger. Scripts (x2_pc_umdcheck) keep NWN's value.
// SK7 (`concentration` faithful) shares the mode hook: the defensive-casting
// mode is refused too (2024 has none; casting draws no opportunity attacks).
// NWN mode: NWN's.

#include <stdint.h>

#include <string>
#include <unordered_set>

#include "chat.h"
#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "skill_rank.h"

namespace {

using SetCombatModeFn = void (*)(void* creature, uint8_t mode, int force);
using TauntFn = uint32_t (*)(void* creature, void* action);
using AddCustomerFn = void (*)(void* store, void* player, char markup, char markdown);

SetCombatModeFn g_set_combat_mode = nullptr;
TauntFn g_taunt = nullptr;
AddCustomerFn g_add_customer = nullptr;
uintptr_t g_umd_callers[3] = {};
std::unordered_set<uint64_t> g_told;  // (creature id, mode) pairs already told
int g_log_lines = 0;

bool NwnOnlyFaithful() { return g_config.nwn_only_skills == rules::RuleVariant::kFaithful; }
bool ConcentrationFaithful() { return g_config.concentration == rules::RuleVariant::kFaithful; }
bool LogSlot() { return g_config.log_attacks && g_log_lines < 200 && ++g_log_lines; }
uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }

void SetCombatMode(void* creature, uint8_t mode, int force) {
  const bool parry = mode == ee::kCombatModeParry && NwnOnlyFaithful();
  const bool defensive = mode == ee::kCombatModeDefensiveCasting && ConcentrationFaithful();
  if (!creature || (!parry && !defensive)) {
    g_set_combat_mode(creature, mode, force);
    return;
  }
  const uint32_t id = IdOf(creature);
  if (g_told.insert((static_cast<uint64_t>(id) << 8) | mode).second) {
    void* stats = StatsOf(creature);
    if (stats && ReadCreatureFacts(stats).is_player) {
      TellPlayer(id, parry ? "nwn5e: there is no parry mode in the 2024 rules; you keep fighting normally."
                           : "nwn5e: there is no defensive casting in the 2024 rules - casting draws no "
                             "opportunity attacks, so you need none.");
    }
    if (LogSlot()) Log("3e-only skills: 0x%08X %s mode refused (faithful)", id, parry ? "parry" : "defensive casting");
  }
}

uint32_t AIActionTaunt(void* creature, void* action) {
  if (!creature || Field<int32_t>(creature, ee::kCreatureTauntPass) == 0) return g_taunt(creature, action);
  // The second pass rolls and applies the taunt; ended here as the engine
  // ends it, without either.
  g_set_combat_mode(creature, 0, 1);
  if (void* cooldowns = Field<void*>(creature, ee::kCreatureSkillCooldowns)) {
    SetField<int32_t>(cooldowns, ee::kSkillCooldownTaunt, ee::kTauntCooldownMs);
  }
  SetField<int32_t>(creature, ee::kCreatureTauntPass, 0);
  if (LogSlot()) Log("3e-only skills: 0x%08X Taunt does nothing (faithful)", IdOf(creature));
  return ee::kActionDone;
}

void AddCustomer(void* store, void* player, char markup, char markdown) {
  if ((markup || markdown) && LogSlot()) {
    Log("3e-only skills: store price change %+d / %+d ignored (no Appraise, faithful)", markup, markdown);
  }
  g_add_customer(store, player, 0, 0);
}

int NoUseMagicDevice(const SkillQuery& query, int value) {
  if (query.skill != ee::kSkillUseMagicDevice || query.base_only) return value;
  for (uintptr_t caller : g_umd_callers) {
    if (caller && query.caller == caller) return -127;
  }
  return value;
}

}  // namespace

void InstallNwnOnlySkillRules() {
  if (NwnOnlyFaithful() || ConcentrationFaithful()) {
    HookExport(NwnOnlyFaithful() ? "CNWSCreature::SetCombatMode (no parry, no defensive casting)"
                                 : "CNWSCreature::SetCombatMode (no defensive casting)",
               ee::kSetCombatMode, reinterpret_cast<void*>(&SetCombatMode),
               reinterpret_cast<void**>(&g_set_combat_mode));
  }
  if (!NwnOnlyFaithful()) return;
  HookExport("CNWSCreature::AIActionTaunt (Taunt does nothing)", ee::kAIActionTaunt,
             reinterpret_cast<void*>(&AIActionTaunt), reinterpret_cast<void**>(&g_taunt));
  HookExport("CNWSStore::AddCustomer (stores keep their prices)", ee::kAddCustomer,
             reinterpret_cast<void*>(&AddCustomer), reinterpret_cast<void**>(&g_add_customer));
  g_umd_callers[0] = SkillRankCallerIn(ee::kCheckUseMagicDeviceSkill);
  g_umd_callers[1] = SkillRankCallerIn(ee::kCheckItemAlignmentRestrictions);
  g_umd_callers[2] = SkillRankCallerIn(ee::kApplyHolyAvenger);
  const int found = (g_umd_callers[0] != 0) + (g_umd_callers[1] != 0) + (g_umd_callers[2] != 0);
  if (found && AddSkillAdjuster(&NoUseMagicDevice)) {
    Log("3e-only skills: Use Magic Device lifts no item limits (%d of 3 checks found)", found);
  }
}
