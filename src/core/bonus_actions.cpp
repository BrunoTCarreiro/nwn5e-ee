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
// 0.61.1 (owner: "This option should allow the player to select what bonus action they'd want to be triggered
// automatically"; "yes, build both as described"):
// - The default may also be a Bonus Action ability (record: its family + 2): at the start of each of the creature's
//   rounds in combat, while the round's Bonus Action is free, it fires (as a waiting one does) - unless it isn't
//   ready (not held, no use left, Rage while raging) or is pointless (Second Wind, Wholeness of Body at full hit
//   points); then the bonus attack. Never out of combat (the round's start is in combat only).
// - The action queue strip shows a waiting Bonus Action ("next round"; a click cancels it) and one just used (about
//   1.5 s): they never enter NWN's queue.
// Phase 6 part 2 (#120; #51's decision 4 (i): potions a Bonus Action; SRD 5.2: drinking a potion takes a Bonus
// Action) - everyone: a potion's use (CNWSCreature::UseItem; baseitems.2da 49, 104) spends the round's Bonus Action
// once the engine queued it; one more is refused and waits, as a feat does (the latest one), and the next round's
// start drinks it (UseItem again, its order kept) - placed before a running attack (action_order.cpp), as a
// player's potion is.

#include "bonus_actions.h"

#include <windows.h>

#include <stdint.h>

#include <string>
#include <unordered_map>

#include "chat.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "engine.h"
#include "action_order.h"
#include "feat_use.h"
#include "game_clock.h"
#include "hooks.h"
#include "item_use.h"
#include "log.h"
#include "mastery.h"
#include "records.h"
#include "rules_loaded.h"
#include "rules/action_economy.h"
#include "rules/weapons.h"

int MartialArtsLevel(void* creature);        // rules_monk.cpp (M1)
bool BarbarianRaging(void* creature);        // rules_barbarian.cpp (B1)
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
  bool potion = false;      // #120: or a potion waiting (its use's order)
  ItemUseOrder potion_order;
  bool run_pending = false;  // the round's start spent the Bonus Action on it: run it now
  bool decided = false;
  uint32_t decided_round = 0;
  bool keep_off_hand = true;
  bool martial_arts = true;
};
std::unordered_map<uint32_t, BonusState> g_state;  // by creature id; the engine's thread
struct RecentUse {
  int feat = -1;
  ULONGLONG until = 0;
};
std::unordered_map<uint32_t, RecentUse> g_recent;  // the strip's "used" tile, by creature id
constexpr ULONGLONG kRecentMs = 1500;
thread_local bool t_running_pending = false;
int g_lines = 0;

bool Faithful() { return g_config.actions == rules::RuleVariant::kFaithful; }
uint32_t IdOf(const void* object) { return object ? Field<uint32_t>(object, ee::kObjectId) : 0; }
bool Waiting(const BonusState& s) { return s.pending >= 0 || s.potion; }
const char* WaitingName(const BonusState& s) { return s.potion ? "a potion" : rules::BonusActionName(s.pending); }
bool IsPotionItem(const void* item) { return item && rules::IsPotionBaseItem(engine::BaseItemRow(item)); }
bool LogSlot() { return g_lines < 600 && ++g_lines; }

bool IsPlayer(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && ReadCreatureFacts(stats).is_player;
}

void Tell(void* creature, const std::string& line) {
  if (IsPlayer(creature)) TellPlayer(IdOf(creature), "nwn5e: " + line);
}

rules::DefaultBonus DefaultOf(void* creature) {
  return rules::DecodeDefaultBonus(creature ? engine::GetLocalInt(creature, kVarDefault) : 0);
}

// The feat of a family the creature holds (Rage's and Second Wind's highest first), or -1.
int HeldFeatOf(void* creature, int family) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (!stats) return -1;
  static const int kRage[] = {331, 330, 329, 328, 327, 326, 293};
  static const int kSecondWind[] = {1222, 1221, 1220};
  if (family == 293) {
    for (int feat : kRage) {
      if (engine::HasFeat(stats, feat)) return feat;
    }
    return -1;
  }
  if (family == 1220) {
    for (int feat : kSecondWind) {
      if (engine::HasFeat(stats, feat)) return feat;
    }
    return -1;
  }
  return engine::HasFeat(stats, family) ? family : -1;
}

// Ready to fire: a use left (nwn5e's pools answer the engine's count), not already raging, not pointless.
bool FeatReady(void* creature, int feat, int family) {
  using RemainingUsesFn = uint8_t (*)(void* stats, uint16_t feat);
  using MaxHitPointsFn = int16_t (*)(void* creature, int);
  static const auto remaining = reinterpret_cast<RemainingUsesFn>(EngineExport(ee::kGetFeatRemainingUses));
  static const auto max_hp = reinterpret_cast<MaxHitPointsFn>(EngineExport(ee::kGetMaxHitPoints));
  void* stats = StatsOf(creature);
  if (feat < 0 || !stats) return false;
  if (remaining && remaining(stats, static_cast<uint16_t>(feat)) == 0) return false;
  if (family == 293 && BarbarianRaging(creature)) return false;
  const int hp = Field<int16_t>(creature, ee::kObjectCurrentHitPoints);
  return !(max_hp && rules::DefaultBonusPointless(family, hp, max_hp(creature, 1)));
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
  s.potion = false;
  if (LogSlot()) Log("actions: 0x%08X %s waits - the round's Bonus Action is spent", id, rules::BonusActionName(feat));
  Tell(creature, std::string(rules::BonusActionName(feat)) +
                     " - your Bonus Action is spent this round; it goes at the start of your next round.");
  return false;
}

void BonusActionUsed(void* creature, int feat) {
  if (!Faithful() || !creature || !rules::TakesBonusAction(feat)) return;
  const uint32_t id = IdOf(creature);
  Spend(id, &g_state[id]);
  g_recent[id] = RecentUse{feat, GetTickCount64() + kRecentMs};
  if (LogSlot()) Log("actions: 0x%08X %s - the round's Bonus Action", id, rules::BonusActionName(feat));
}

// #120: UseItem - drinking a potion takes the Bonus Action, one a round; one more waits for the next round.
bool PotionGuard(void* creature, uint32_t, void* item) {
  if (!Faithful() || !creature || !IsPotionItem(item) || t_running_pending) return true;
  const uint32_t id = IdOf(creature);
  BonusState& s = g_state[id];
  const ItemUseOrder* order = CurrentItemUse();
  if (Free(id, s) || !order) return true;  // spent once the engine queued the drink (PotionUsed)
  s.potion = true;
  s.potion_order = *order;
  s.pending = -1;
  if (LogSlot()) Log("actions: 0x%08X a potion waits - the round's Bonus Action is spent", id);
  Tell(creature, "Drinking a potion takes your Bonus Action, already spent this round; you drink it at the start of "
                 "your next round.");
  return false;
}

void PotionUsed(void* creature, uint32_t, void* item) {
  if (!Faithful() || !creature || !IsPotionItem(item)) return;
  const uint32_t id = IdOf(creature);
  Spend(id, &g_state[id]);
  if (LogSlot()) Log("actions: 0x%08X drinks a potion - the round's Bonus Action", id);
}

// A waiting Bonus Action: run once the round's start spent the Bonus Action on it - or, out of combat, once it's free.
void BonusActionUpdate(void* creature) {
  if (g_state.empty() || !creature) return;
  const auto it = g_state.find(IdOf(creature));
  if (it == g_state.end() || !Waiting(it->second)) return;
  BonusState& s = it->second;
  const uint32_t id = it->first;
  if (!s.run_pending) {
    if (RoundRunning(creature) || !Free(id, s)) return;  // in a fight the next round's start decides
    Spend(id, &s);
  }
  s.run_pending = false;
  if (s.potion) {  // #120: the waiting potion - given again, before a running attack
    s.potion = false;
    const std::vector<void*> before = QueueNodes(creature);
    t_running_pending = true;
    const int drunk = UseItemAgain(creature, s.potion_order);
    t_running_pending = false;
    if (drunk) PlaceNewOrder(creature, before, "the waiting potion");
    if (LogSlot()) Log("actions: 0x%08X a potion, waited for this round's Bonus Action: %s", id,
                       drunk ? "drunk" : "refused");
    return;
  }
  const int feat = s.pending;
  s.pending = -1;
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
    if (Waiting(s) && !s.used) {
      Spend(id, &s);
      s.run_pending = true;  // the creature update runs it
    }
    // 0.61.1: the default - a Bonus Action ability fires while the round's is free and it's ready; else the bonus
    // attack (or none).
    const rules::DefaultBonus choice = DefaultOf(creature);
    const int held = choice.kind == rules::DefaultBonusAction::kFeat ? HeldFeatOf(creature, choice.feat) : -1;
    const rules::DefaultPlan plan =
        rules::PlanDefaultBonus(choice, s.used, held >= 0 && FeatReady(creature, held, choice.feat));
    if (plan.fire_feat) {
      s.pending = held;
      Spend(id, &s);
      s.run_pending = true;
    }
    const bool nick = off_hand > 0 && NickOffHand(creature);
    const bool martial_arts = off_hand <= 0 && MartialArtsLevel(creature) > 0;
    const rules::RoundBonus r = rules::DecideRoundBonus(s.used, plan.attack, off_hand > 0, martial_arts, nick);
    s.keep_off_hand = r.keep_off_hand;
    s.martial_arts = r.keep_martial_arts;
    if (r.spends_bonus) Spend(id, &s);
    if ((off_hand > 0 || martial_arts || s.run_pending) && LogSlot()) {
      Log("actions: 0x%08X round %u - Bonus Action: %s", id, now,
          s.run_pending ? WaitingName(s)
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

int DefaultBonusKey(void* creature) {
  if (creature) EnsureRecords(creature);
  const rules::DefaultBonus choice = DefaultOf(creature);
  return choice.kind == rules::DefaultBonusAction::kNone   ? -1
         : choice.kind == rules::DefaultBonusAction::kFeat ? choice.feat
                                                           : 0;
}

void SetDefaultBonusKey(void* creature, int key) {
  if (!creature || !Faithful()) return;
  EnsureRecords(creature);
  rules::DefaultBonus choice;
  if (key < 0) {
    choice.kind = rules::DefaultBonusAction::kNone;
  } else if (key > 0) {
    choice.kind = rules::DefaultBonusAction::kFeat;
    choice.feat = rules::BonusFeatFamily(key);
  }
  SetRecord(creature, kVarDefault, rules::EncodeDefaultBonus(choice));
  const uint32_t id = IdOf(creature);
  const std::string name = choice.kind == rules::DefaultBonusAction::kFeat ? rules::BonusActionName(choice.feat) : "";
  Log("actions: 0x%08X default Bonus Action: %s", id,
      choice.kind == rules::DefaultBonusAction::kNone ? "none" : name.empty() ? "the bonus attack" : name.c_str());
  if (choice.kind == rules::DefaultBonusAction::kNone) {
    Tell(creature, "Default Bonus Action: none - your bonus attack is left out, the Bonus Action kept for one you use.");
  } else if (choice.kind == rules::DefaultBonusAction::kFeat) {
    Tell(creature, "Default Bonus Action: " + name + " - at the start of each of your rounds in a fight while your "
                   "Bonus Action is free (skipped when it can't be used or would do nothing: then your bonus attack).");
  } else {
    Tell(creature, "Default Bonus Action: your bonus attack each round (the off-hand attack, Martial Arts' strike or "
                   "Flurry of Blows).");
  }
  RefreshCombatOptions(id);
}

bool HasBonusFamily(void* creature, int family) { return HeldFeatOf(creature, family) >= 0; }

std::vector<BonusStripItem> BonusActionStripItems(void* creature) {
  std::vector<BonusStripItem> items;
  if (!Faithful() || !creature) return items;
  const uint32_t id = IdOf(creature);
  const auto state = g_state.find(id);
  if (state != g_state.end() && Waiting(state->second) && !state->second.run_pending) {
    BonusStripItem waiting;
    waiting.feat = state->second.pending;
    waiting.waiting = true;
    if (state->second.potion) {
      waiting.item = state->second.potion_order.item;
      waiting.active_property = state->second.potion_order.active_property;
    }
    items.push_back(waiting);
  }
  const auto recent = g_recent.find(id);
  if (recent != g_recent.end()) {
    if (GetTickCount64() < recent->second.until) {
      BonusStripItem used;
      used.feat = recent->second.feat;
      items.push_back(used);
    } else {
      g_recent.erase(recent);
    }
  }
  return items;
}

bool CancelWaitingBonusAction(void* creature) {
  const auto state = creature ? g_state.find(IdOf(creature)) : g_state.end();
  if (state == g_state.end() || !Waiting(state->second) || state->second.run_pending) return false;
  Log("actions: 0x%08X the waiting %s cancelled", state->first, WaitingName(state->second));
  state->second.pending = -1;
  state->second.potion = false;
  return true;
}

void InstallBonusActions() {
  if (!Faithful()) return;
  RegisterRecords({kVarDefault});
  CountCombatRounds();
  const bool instant = OnRulesLoaded(&MakeFeatsInstant);
  const bool guard = AddFeatUseGuard(&BonusActionGuard) && AddFeatUseListener(&BonusActionUsed);
  const bool updates = AddCreatureUpdateListener(&BonusActionUpdate);
  const bool potions = AddItemUseGuard(&PotionGuard) && AddItemUseListener(&PotionUsed);
  Log("actions: Bonus Actions - %s; %s%s; %s",
      instant ? "the class abilities instant" : "NOT instant (no rules-load hook)",
      guard ? "one a round, one more waits for the next round" : "NOT one a round (no UseFeat hook)",
      updates ? "" : " - a waiting one never runs (no creature updates)",
      potions ? "potions too (#120)" : "potions NOT (no UseItem hook)");
}
