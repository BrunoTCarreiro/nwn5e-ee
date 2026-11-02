// Phase 6 part 1 (#52; docs/PLAN.md "Phase 6 — Action economy", decided by the owner 2026-11-02 on #51),
// `actions` faithful - everyone on the same rules, monsters too:
// - The feats that take the 2024 Bonus Action (rules::TakesBonusAction: Second Wind, Rage, Intimidating Presence,
//   Patient Defense, Step of the Wind, Wholeness of Body) and those that take no action (rules::TakesNoAction:
//   Reckless Attack, Superior Defense, the standing-choice pickers) get feat.2da ReqAction 0 in memory after every
//   rules load (CNWFeat +0x70): UseFeat then runs the feat's script at once (AI event 8) - no queue cleared, no cast
//   round, the round's attacks go on. Before, each cost the rest of the round and the next one (#51's recon).
// - One Bonus Action a round - the creature's own round (CombatRoundsEnded), out of combat every 6 s: a UseFeat
//   guard lets one through while it's free (a listener spends it once the engine took the use); one more waits -
//   the guard refuses and remembers it, and the next round spends its Bonus Action on it (a creature update runs
//   it), in place of the default.
// - The default Bonus Action (Combat configuration; record nwn5e_bonus_default): the automatic bonus attack - the
//   off-hand attack (rules_two_weapon.cpp), the Monk's Martial Arts strike or Flurry of Blows (rules_monk.cpp) - or
//   none, kept free for one clicked. An off-hand attack through Nick (weapon mastery) is part of the Attack action.

#include "bonus_actions.h"

#include <stdint.h>

#include <string>
#include <unordered_map>

#include "chat.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "engine.h"
#include "feat_use.h"
#include "game_clock.h"
#include "hooks.h"
#include "log.h"
#include "mastery.h"
#include "records.h"
#include "rules_loaded.h"
#include "rules/action_economy.h"
#include "rules/weapons.h"

int MartialArtsLevel(void* creature);        // rules_monk.cpp (M1)
void RefreshCombatOptions(uint32_t player);  // combat_options.cpp

namespace {

constexpr const char* kVarDefault = "nwn5e_bonus_default";  // rules::EncodeDefaultBonusAction (0: the bonus attack)
constexpr size_t kFeatReqAction = 0x70;  // CNWFeat: feat.2da ReqAction (int32; UseFeat tests it - #51's recon)
constexpr size_t kRoundStarted = 0x2f04;  // CNWSCombatRound: a round running (#51's recon)

struct Vector3 {  // UseFeat's location (unused: the feats target their user)
  float x, y, z;
};
using UseFeatFn = int (*)(void* creature, uint16_t feat, uint16_t subfeat, uint32_t target, uint32_t area,
                          Vector3* location);

struct BonusState {
  bool used = false;
  uint32_t used_round = 0;  // CombatRoundsEnded when it was spent
  uint64_t used_at = 0;     // game ms
  int pending = -1;         // a Bonus Action feat waiting for the next round
  bool run_pending = false;  // the round's start spent the Bonus Action on it: run it now
  bool decided = false;
  uint32_t decided_round = 0;
  bool keep_off_hand = true;
  bool martial_arts = true;
};
std::unordered_map<uint32_t, BonusState> g_state;  // by creature id; the engine's thread
thread_local bool t_running_pending = false;
int g_lines = 0;

bool Faithful() { return g_config.actions == rules::RuleVariant::kFaithful; }
uint32_t IdOf(const void* object) { return object ? Field<uint32_t>(object, ee::kObjectId) : 0; }
bool LogSlot() { return g_lines < 600 && ++g_lines; }

bool IsPlayer(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && ReadCreatureFacts(stats).is_player;
}

void Tell(void* creature, const std::string& line) {
  if (IsPlayer(creature)) TellPlayer(IdOf(creature), "nwn5e: " + line);
}

rules::DefaultBonusAction DefaultOf(void* creature) {
  return rules::DecodeDefaultBonusAction(creature ? engine::GetLocalInt(creature, kVarDefault) : 0);
}

bool Free(uint32_t id, const BonusState& s) {
  return rules::BonusActionFree(s.used, CombatRoundsEnded(id) != s.used_round,
                                static_cast<long long>(GameMs() - s.used_at));
}

void Spend(uint32_t id, BonusState* s) {
  s->used = true;
  s->used_round = CombatRoundsEnded(id);
  s->used_at = GameMs();
}

bool RoundRunning(void* creature) {
  void* round = Field<void*>(creature, ee::kCreatureCombatRound);
  return round && Field<int32_t>(round, kRoundStarted) != 0;
}

bool NickWith(void* creature, int base_item) {
  return base_item >= 0 && rules::MasteryForNwnBaseItem(base_item) == rules::Mastery::kNick &&
         HasWeaponMasteryWith(creature, base_item);
}

bool NickOffHand(void* creature) {
  return NickWith(creature, engine::BaseItemRowInSlot(creature, ee::kSlotLeftHand)) ||
         NickWith(creature, engine::BaseItemRowInSlot(creature, ee::kSlotRightHand));
}

// After every rules load: the Bonus Action and no-action feats instant (ReqAction 0).
void MakeFeatsInstant(void* rules_object, bool /*reload*/) {
  auto* feats = rules_object ? Field<uint8_t*>(rules_object, ee::kRulesFeats) : nullptr;
  if (!feats) return;
  const int count = Field<uint16_t>(rules_object, ee::kRulesFeatCount);
  int made = 0;
  for (int feat = 0; feat < count; ++feat) {
    if (!rules::TakesBonusAction(feat) && !rules::TakesNoAction(feat)) continue;
    auto* req_action = reinterpret_cast<int32_t*>(feats + static_cast<size_t>(feat) * ee::kFeatStride + kFeatReqAction);
    if (*req_action == 0) continue;
    *req_action = 0;
    ++made;
  }
  Log("actions: %d feats made instant (feat.2da ReqAction 0) - the Bonus Action ones (Second Wind, Rage, Patient "
      "Defense ...) and the free ones (Reckless Attack, Superior Defense, the pickers)", made);
}

// UseFeat: one Bonus Action a round; one more waits for the next round.
bool BonusActionGuard(void* creature, int feat) {
  if (!Faithful() || !creature || !rules::TakesBonusAction(feat) || t_running_pending) return true;
  const uint32_t id = IdOf(creature);
  BonusState& s = g_state[id];
  if (Free(id, s)) return true;  // spent once the engine takes the use (BonusActionUsed)
  s.pending = feat;
  if (LogSlot()) Log("actions: 0x%08X %s waits - the round's Bonus Action is spent", id, rules::BonusActionName(feat));
  Tell(creature, std::string(rules::BonusActionName(feat)) +
                     " - your Bonus Action is spent this round; it goes at the start of your next round.");
  return false;
}

void BonusActionUsed(void* creature, int feat) {
  if (!Faithful() || !creature || !rules::TakesBonusAction(feat)) return;
  const uint32_t id = IdOf(creature);
  Spend(id, &g_state[id]);
  if (LogSlot()) Log("actions: 0x%08X %s - the round's Bonus Action", id, rules::BonusActionName(feat));
}

// A waiting Bonus Action: run once the round's start spent the Bonus Action on it - or, out of combat, once it's free.
void BonusActionUpdate(void* creature) {
  if (g_state.empty() || !creature) return;
  const auto it = g_state.find(IdOf(creature));
  if (it == g_state.end() || it->second.pending < 0) return;
  BonusState& s = it->second;
  const uint32_t id = it->first;
  if (!s.run_pending) {
    if (RoundRunning(creature) || !Free(id, s)) return;  // in a fight the next round's start decides
    Spend(id, &s);
  }
  const int feat = s.pending;
  s.pending = -1;
  s.run_pending = false;
  static const auto use_feat = reinterpret_cast<UseFeatFn>(EngineExport(ee::kUseFeat));  // through the hook
  if (!use_feat) return;
  Vector3 nowhere{0.0f, 0.0f, 0.0f};
  t_running_pending = true;
  const int done = use_feat(creature, static_cast<uint16_t>(feat), 0, id, ee::kObjectInvalid, &nowhere);
  t_running_pending = false;
  if (LogSlot()) Log("actions: 0x%08X %s, waited for this round's Bonus Action: %s", id, rules::BonusActionName(feat),
                     done ? "used" : "refused");
}

}  // namespace

void BonusActionRoundStart(void* round) {
  if (!Faithful() || !round) return;
  void* creature = Field<void*>(round, ee::kRoundOwner);
  if (!creature) return;
  const uint32_t id = IdOf(creature);
  const int off_hand = Field<int32_t>(round, ee::kRoundOffHandAttacks);
  BonusState& s = g_state[id];
  const uint32_t now = CombatRoundsEnded(id);
  if (!s.decided || s.decided_round != now) {  // a new round (a recount mid-round keeps the decision)
    s.decided = true;
    s.decided_round = now;
    if (s.used && Free(id, s)) s.used = false;
    if (s.pending >= 0 && !s.used) {
      Spend(id, &s);
      s.run_pending = true;  // the creature update runs it
    }
    const bool nick = off_hand > 0 && NickOffHand(creature);
    const bool martial_arts = off_hand <= 0 && MartialArtsLevel(creature) > 0;
    const rules::RoundBonus r = rules::DecideRoundBonus(s.used, DefaultOf(creature), off_hand > 0, martial_arts, nick);
    s.keep_off_hand = r.keep_off_hand;
    s.martial_arts = r.keep_martial_arts;
    if (r.spends_bonus) Spend(id, &s);
    if ((off_hand > 0 || martial_arts || s.run_pending) && LogSlot()) {
      Log("actions: 0x%08X round %u - Bonus Action: %s", id, now,
          s.run_pending ? rules::BonusActionName(s.pending)
          : r.spends_bonus ? (off_hand > 0 ? "the off-hand attack" : "Martial Arts' strike / Flurry")
          : nick ? "free (Nick: the off-hand attack is part of the Attack action)"
                 : "free (the bonus attack left out)");
    }
  }
  if (off_hand > 0 && !s.keep_off_hand) SetField<int32_t>(round, ee::kRoundOffHandAttacks, 0);
}

bool MartialArtsBonusAllowed(void* creature) {
  if (!Faithful() || g_state.empty() || !creature) return true;
  const auto it = g_state.find(IdOf(creature));
  return it == g_state.end() || !it->second.decided || it->second.martial_arts;
}

bool ActionEconomyOn() { return Faithful(); }

bool DefaultBonusAttackOn(void* creature) {
  if (creature) EnsureRecords(creature);
  return DefaultOf(creature) == rules::DefaultBonusAction::kBonusAttack;
}

void SetDefaultBonusAttack(void* creature, bool on) {
  if (!creature || !Faithful()) return;
  EnsureRecords(creature);
  SetRecord(creature, kVarDefault,
            rules::EncodeDefaultBonusAction(on ? rules::DefaultBonusAction::kBonusAttack : rules::DefaultBonusAction::kNone));
  const uint32_t id = IdOf(creature);
  Log("actions: 0x%08X default Bonus Action: %s", id, on ? "the bonus attack" : "none");
  Tell(creature, on ? "Default Bonus Action: your bonus attack each round (the off-hand attack, Martial Arts' strike "
                      "or Flurry of Blows)."
                    : "Default Bonus Action: none - your bonus attack is left out, the Bonus Action kept for one you "
                      "use.");
  RefreshCombatOptions(id);
}

void InstallBonusActions() {
  if (!Faithful()) return;
  RegisterRecords({kVarDefault});
  CountCombatRounds();
  const bool instant = OnRulesLoaded(&MakeFeatsInstant);
  const bool guard = AddFeatUseGuard(&BonusActionGuard) && AddFeatUseListener(&BonusActionUsed);
  const bool updates = AddCreatureUpdateListener(&BonusActionUpdate);
  Log("actions: Bonus Actions - %s; %s%s", instant ? "the class abilities instant" : "NOT instant (no rules-load hook)",
      guard ? "one a round, one more waits for the next round" : "NOT one a round (no UseFeat hook)",
      updates ? "" : " - a waiting one never runs (no creature updates)");
}
