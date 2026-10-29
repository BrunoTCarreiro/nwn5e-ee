// 2024 Extra Attack in place of NWN's iterative attacks (port of 1.69's
// rules_extra_attack.cpp; docs/EE-PORT.md E1):
//
// - Player characters and henchmen (decision 11) make the attacks of the 2024
//   Attack action, from their class levels (rules::MainHandAttacksPerRound);
//   prestige levels count as levels of their parent class (decision 10),
//   racial levels do not count. GetAttacksPerRound is asked by
//   InitializeNumberOfAttacks and by NWScript's GetAttacksPerRound.
// - Other creatures (monsters, summons, familiars, animal companions) keep
//   the engine's count, standing in for a Multiattack.
// - No attack takes the 3e iterative penalty: in EE it is one ruleset value
//   (MULTIPLE_ATTACKS_BAB_PENALTY_MULTIPLIER, -5 per attack; _MONK, -3 with
//   monk tables) for main-hand, off-hand and additional attacks (attacks of
//   opportunity, Haste, Flurry, Rapid Shot) alike — answered 0. (1.69 patched
//   three branches.)
//
// The engine's own exceptions still apply around the hooked count: script
// overrides of the number of attacks, crossbows without Rapid Reload, Dirty
// Fighting, and the extra attack of Flurry / Rapid Shot / Haste.
// [debug] attacks_per_round gives player characters a fixed count instead.

#include <stdint.h>

#include <algorithm>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "rules/nwn_conversion.h"
#include "ruleset.h"
#include "sheet.h"

namespace {

using AttacksPerRoundFn = uint8_t (*)(void* stats);
AttacksPerRoundFn g_get_attacks_per_round = nullptr;

// The engine never computes more than 6 main-hand attacks.
constexpr int kEngineMaxAttacks = 6;

LogOnce g_attacks_logged(2000);
LogOnce g_sheet_logged(500);

uint8_t GetAttacksPerRound(void* stats) {
  const int engine_attacks = g_get_attacks_per_round(stats);
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character) return static_cast<uint8_t>(engine_attacks);
  int attacks = engine_attacks;
  const char* reason = nullptr;
  if (facts.is_player && g_config.debug_attacks_per_round > 0) {
    attacks = std::min(g_config.debug_attacks_per_round, kEngineMaxAttacks);
    reason = "debug attacks_per_round";
  } else if (g_config.extra_attack) {
    attacks = std::min(rules::MainHandAttacksPerRound(true, ReadClassSlots(stats), engine_attacks), kEngineMaxAttacks);
    reason = "Extra Attack";
  }
  if (reason && g_config.log_attacks &&
      g_attacks_logged.First(LogOnce::Key(ObjectIdOf(stats), engine_attacks, attacks))) {
    Log("attacks per round: object 0x%08X %s: 3e %d -> 5e %d (%s)", ObjectIdOf(stats), KindOf(facts), engine_attacks,
        attacks, reason);
  }
  return static_cast<uint8_t>(attacks);
}

// The sheet's attacks per round: UpdateCombatInformation computes its own
// (BAB + 4) / 5; characters get the count they actually make.
void FixSheetAttacks(void* stats, uint8_t* info) {
  if (!g_config.extra_attack && g_config.debug_attacks_per_round <= 0) return;  // in for a !test profile only (#82)
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.character) return;
  const int engine_attacks = info[ee::kCombatInfoAttacks];
  const int attacks = std::clamp<int>(GetAttacksPerRound(stats), 1, kEngineMaxAttacks);
  info[ee::kCombatInfoAttacks] = static_cast<uint8_t>(attacks);
  if (g_config.log_attacks && engine_attacks != attacks &&
      g_sheet_logged.First(LogOnce::Key(ObjectIdOf(stats), engine_attacks, attacks))) {
    Log("sheet: object 0x%08X %s attacks per round 3e %d -> 5e %d", ObjectIdOf(stats), KindOf(facts), engine_attacks,
        attacks);
  }
}

}  // namespace

void InstallExtraAttackRules() {
  // With test_commands, in from the start: a !test profile may set attacks_per_round (#82).
  if (!g_config.extra_attack && g_config.debug_attacks_per_round <= 0 && !g_config.debug_test_commands) return;
  HookExport("CNWSCreatureStats::GetAttacksPerRound", ee::kGetAttacksPerRound,
             reinterpret_cast<void*>(&GetAttacksPerRound), reinterpret_cast<void**>(&g_get_attacks_per_round));
  AddCombatInformationListener(&FixSheetAttacks);
  if (g_config.extra_attack) {
    AnswerRulesetInt("MULTIPLE_ATTACKS_BAB_PENALTY_MULTIPLIER", "extra_attack: no 3e -5 per attack",
                     [](int) { return 0; });
    AnswerRulesetInt("MULTIPLE_ATTACKS_BAB_PENALTY_MULTIPLIER_MONK", "extra_attack: no 3e -3 per monk attack",
                     [](int) { return 0; });
  }
}
