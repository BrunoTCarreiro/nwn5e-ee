// 2024 critical hits (SRD 5.2) on EE: a critical hit happens on a natural 20
// only, with no confirmation roll, and the attack's damage dice are rolled
// twice while flat bonuses count once (port of 1.69's rules_crit.cpp and
// rules_crit_confirm.cpp; docs/EE-PORT.md, E0).
//
// - crit_nat20_only: GetCriticalHitRoll (the lowest d20 that threatens)
//   answers 20. Its callers: ResolveAttackRoll and, twice (both hands), the
//   character sheet's combat information.
// - crit_no_confirmation: EE has no lever for it. ResolveAttackRoll asks
//   GetCriticalHitRoll after a hit and rolls one more d20 — the confirmation —
//   only on a threat, before anything else that rolls. So GetCriticalHitRoll
//   asked inside an attack roll arms the next d20 that ResolveAttackRoll
//   rolls, and that roll is answered with a value no AC beats. The engine's own checks
//   that follow still apply (critical immunity; crits_against_players). After
//   the attack roll the threat flag and roll are cleared from the attack's
//   data, so the combat log shows no "Threat Roll".
// - crit_double_damage / crit_dice_only: GetDamageRoll and GetTotalEffectBonus
//   repeat each damage part `multiplier` times — dice re-rolled, flat bonuses
//   added again. The multiplier is 2 except where a flat bonus asks: the two
//   calls of GetDamageRoll's loop over GetDamageBonus (Strength etc.) and, in
//   GetTotalEffectBonus, entries whose dice count is 0. Both sets of call
//   sites are found in the engine's code at install (checked shapes; the rule
//   part is skipped and logged if the code differs).
// - crits_against_players (decision B7): EE's diffsettings.2da row 0
//   NoCriticalOnPC (1 at DM-Easy, Easy and Normal) turns a creature's
//   critical hit on a player-controlled character into a hit; the difficulty
//   option answers 0 for that row.
//
// The engine answers threat 21 / multiplier x0 for a hand with nothing to
// attack with (the empty off-hand, for the sheet); those stay as they are.

#include <intrin.h>

#include <algorithm>
#include <vector>

#include "attack_roll.h"
#include "config.h"
#include "dice.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "rules/nwn_conversion.h"

namespace {

using CritFn = int (*)(void* stats, int offhand);
using DifficultyOptionFn = int (*)(void* app, int option);

CritFn g_get_critical_hit_roll = nullptr;
CritFn g_get_critical_hit_multiplier = nullptr;
DifficultyOptionFn g_get_difficulty_option = nullptr;
int* g_combat_debugging = nullptr;

// Crit values depend on the weapon, not the creature: each distinct one once.
LogOnce g_threat_logged(64);
LogOnce g_multiplier_logged(64);
LogOnce g_difficulty_logged(4);

// Return addresses of the multiplier calls that multiply flat bonuses.
std::vector<uintptr_t> g_flat_bonus_returns;    // GetDamageRoll's GetDamageBonus loop
std::vector<uintptr_t> g_effect_bonus_returns;  // GetTotalEffectBonus (dice count decides)

CodeRange g_attack_roll_code;
constexpr int kConfirmingRoll = 1000;  // d20 + any modifier beats any AC
thread_local bool t_confirmation_armed = false;
thread_local bool t_confirmation_forced = false;
int g_confirmations_logged = 0;

bool RemovesConfirmation() { return g_config.crit_no_confirmation || g_config.debug_every_hit_crits; }

bool Contains(const std::vector<uintptr_t>& list, uintptr_t value) {
  return std::find(list.begin(), list.end(), value) != list.end();
}

int GetCriticalHitRoll(void* stats, int offhand) {
  const int vanilla = g_get_critical_hit_roll(stats, offhand);
  int floor = vanilla;
  if (g_config.debug_every_hit_crits) {
    floor = vanilla > 20 ? vanilla : 1;  // every d20 threatens; 21 = nothing to attack with
    if (g_config.log_attacks && g_threat_logged.First(LogOnce::Key(0, vanilla, 1))) {
      Log("debug every_hit_crits: threat %d-20 -> 1-20", vanilla);
    }
  } else if (g_config.crit_nat20_only) {
    floor = rules::ConvertCriticalThreatFloor(vanilla);
    if (g_config.log_attacks && floor != vanilla && g_threat_logged.First(LogOnce::Key(0, vanilla, floor))) {
      Log("crit threat: 3e %d-20 -> 5e %d-20 (logged once per value)", vanilla, floor);
    }
  }
  // Inside an attack roll the next d20 it rolls is the confirmation (it rolls
  // it only when the hit threatens).
  if (RemovesConfirmation() && CurrentAttackRoll() &&
      g_attack_roll_code.Contains(reinterpret_cast<uintptr_t>(_ReturnAddress()))) {
    t_confirmation_armed = true;
  }
  return floor;
}

bool ConfirmThreat(uintptr_t caller, int count, int sides, int* result) {
  if (!t_confirmation_armed || count != 1 || sides != 20 || !g_attack_roll_code.Contains(caller)) return false;
  t_confirmation_armed = false;
  t_confirmation_forced = true;
  *result = kConfirmingRoll;
  return true;
}

void BeforeAttackRoll(AttackRoll&) {
  t_confirmation_armed = false;
  t_confirmation_forced = false;
  if (g_combat_debugging) *g_combat_debugging = 1;
}

void AfterAttackRoll(AttackRoll& roll) {
  t_confirmation_armed = false;
  if (!t_confirmation_forced) return;
  t_confirmation_forced = false;
  int result = -1;
  if (roll.data) {
    *reinterpret_cast<int32_t*>(roll.data + ee::kAttackThreatFlag) = 0;
    roll.data[ee::kAttackThreatRoll] = 0;
    result = roll.data[ee::kAttackResult];
  }
  if (g_config.log_attacks && g_confirmations_logged < 50) {
    ++g_confirmations_logged;
    Log("crit: threat -> %s, no confirmation roll (logged for the first 50)",
        result == ee::kAttackResultCritical ? "critical hit"
                                            : "hit (critical immunity or a difficulty rule)");
  }
}

int GetCriticalHitMultiplier(void* stats, int offhand) {
  const int vanilla = g_get_critical_hit_multiplier(stats, offhand);
  if (!g_config.crit_double_damage) return vanilla;
  const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
  rules::CritDamagePart part = rules::CritDamagePart::kDice;
  int log_kind = 0;
  if (g_config.crit_dice_only && Contains(g_flat_bonus_returns, caller)) {
    part = rules::CritDamagePart::kFlatBonus;
    log_kind = 1;
  } else if (g_config.crit_dice_only && Contains(g_effect_bonus_returns, caller)) {
    const auto* return_slot = static_cast<const uint8_t*>(_AddressOfReturnAddress());
    const int dice_count =
        *reinterpret_cast<const int32_t*>(return_slot + ee::kEffectBonusDiceCountFromReturnSlot);
    part = rules::EffectDamagePart(dice_count);
    log_kind = part == rules::CritDamagePart::kFlatBonus ? 2 : 0;
  }
  const int converted = rules::CriticalMultiplierFor(part, vanilla);
  if (g_config.log_attacks && converted != vanilla &&
      g_multiplier_logged.First(LogOnce::Key(log_kind, vanilla, converted))) {
    static const char* const kWhat[] = {"crit multiplier (dice)", "crit damage: flat bonuses (Strength etc.)",
                                        "crit damage: flat effect damage bonuses"};
    Log("%s: 3e x%d -> 5e x%d (logged once per value)", kWhat[log_kind], vanilla, converted);
  }
  return converted;
}

int GetDifficultyOption(void* app, int option) {
  const int vanilla = g_get_difficulty_option(app, option);
  if (g_config.crits_against_players && option == ee::kDifficultyNoCriticalOnPc && vanilla != 0) {
    if (g_difficulty_logged.First(1)) {
      Log("crits_against_players: difficulty rule NoCriticalOnPC %d -> 0 (creatures' crits on your party stay "
          "crits; logged once)", vanilla);
    }
    return 0;
  }
  return vanilla;
}

// GetDamageRoll's flat-bonus loop: the two multiplier calls right after its
// only GetDamageBonus call (EE +0x16dd, then +0x16f9 / +0x170e).
void FindFlatBonusCalls(uintptr_t multiplier) {
  const CodeRange damage_roll = FunctionAt(EngineExport(ee::kGetDamageRoll));
  const std::vector<uintptr_t> bonus_calls = FindCallsTo(damage_roll, EngineExport(ee::kGetDamageBonus));
  const std::vector<uintptr_t> multiplier_calls = FindCallsTo(damage_roll, multiplier);
  std::vector<uintptr_t> loop;
  if (bonus_calls.size() == 1) {
    for (uintptr_t site : multiplier_calls) {
      if (site > bonus_calls[0] && site <= bonus_calls[0] + 0x40) loop.push_back(site);
    }
  }
  if (loop.size() != 2) {
    Log("SKIP crit_dice_only for Strength etc.: GetDamageRoll's flat-bonus loop not found (%zu GetDamageBonus "
        "calls, %zu multiplier calls, %zu in the loop)", bonus_calls.size(), multiplier_calls.size(), loop.size());
    return;
  }
  for (uintptr_t site : loop) g_flat_bonus_returns.push_back(site + kCallLength);
  Log("crit_dice_only: GetDamageRoll flat-bonus loop at +0x%llx / +0x%llx (of %zu multiplier calls)",
      static_cast<unsigned long long>(loop[0] - damage_roll.begin),
      static_cast<unsigned long long>(loop[1] - damage_roll.begin), multiplier_calls.size());
}

// GetTotalEffectBonus: each multiplier call sits in a block that reads the
// entry's dice count (mov eax, [rsp+44h]) within 0x60 bytes.
void FindEffectBonusCalls(uintptr_t multiplier) {
  const CodeRange effect_bonus = FunctionAt(EngineExport(ee::kGetTotalEffectBonus));
  const std::vector<uintptr_t> sites = FindCallsTo(effect_bonus, multiplier);
  size_t shaped = 0;
  for (uintptr_t site : sites) {
    if (ContainsBytes(site - 0x60, site + 0x60, ee::kEffectBonusDiceCountRead, sizeof(ee::kEffectBonusDiceCountRead))) {
      ++shaped;
    }
  }
  if (sites.empty() || shaped != sites.size()) {
    Log("SKIP crit_dice_only for effect damage bonuses: GetTotalEffectBonus has %zu multiplier calls, %zu with the "
        "dice-count read", sites.size(), shaped);
    return;
  }
  for (uintptr_t site : sites) g_effect_bonus_returns.push_back(site + kCallLength);
  Log("crit_dice_only: GetTotalEffectBonus multiplier calls found (%zu)", sites.size());
}

}  // namespace

void InstallCritRules() {
  g_attack_roll_code = ResolveAttackRollCode();
  if (g_config.crit_double_damage && g_config.crit_dice_only) {
    const uintptr_t multiplier = EngineExport(ee::kGetCriticalHitMultiplier);
    FindFlatBonusCalls(multiplier);
    FindEffectBonusCalls(multiplier);
  }
  if (g_config.crit_nat20_only || RemovesConfirmation()) {
    HookExport("CNWSCreatureStats::GetCriticalHitRoll", ee::kGetCriticalHitRoll,
               reinterpret_cast<void*>(&GetCriticalHitRoll), reinterpret_cast<void**>(&g_get_critical_hit_roll));
  }
  if (g_config.crit_double_damage) {
    HookExport("CNWSCreatureStats::GetCriticalHitMultiplier", ee::kGetCriticalHitMultiplier,
               reinterpret_cast<void*>(&GetCriticalHitMultiplier),
               reinterpret_cast<void**>(&g_get_critical_hit_multiplier));
  }
  if (g_config.debug_combat_breakdown) {
    g_combat_debugging = reinterpret_cast<int*>(EngineExport(ee::kCombatDebugging));
    if (!g_combat_debugging) Log("SKIP [debug] combat_breakdown: no export %s", ee::kCombatDebugging);
  }
  if (RemovesConfirmation()) {
    if (!g_attack_roll_code.Valid()) {
      Log("SKIP crit_no_confirmation: ResolveAttackRoll's code range is unknown");
    } else if (AddDiceFilter(&ConfirmThreat) && AddAttackRollListeners(&BeforeAttackRoll, &AfterAttackRoll)) {
      Log("crit_no_confirmation: a threat in ResolveAttackRoll (+0x%llx bytes of code) confirms without a roll",
          static_cast<unsigned long long>(g_attack_roll_code.end - g_attack_roll_code.begin));
    }
  } else if (g_combat_debugging) {
    AddAttackRollListeners(&BeforeAttackRoll, nullptr);
  }
  if (g_config.crits_against_players) {
    HookExport("CServerExoApp::GetDifficultyOption", ee::kGetDifficultyOption,
               reinterpret_cast<void*>(&GetDifficultyOption), reinterpret_cast<void**>(&g_get_difficulty_option));
  }
}
