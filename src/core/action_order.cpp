// Phase 6 part 2 (#120; docs/PLAN.md "Phase 6 — Action economy"; the owner, 2026-11-02, on #51: "allow multiple
// actions to be queued up, one being executed each passing turn"; the design's point 1: "The queue holds Actions
// (spells, items, abilities that take the Action), one a round in order"), `actions` faithful.
//
// #120's recon (89.8193.37): a player's order to cast a spell, use an item or a feat or a talent reaches
// CNWSCreature::ActionManager(1) first, which - while the creature is idle, moving or attacking (its AI action
// state +0x8D6 below 10) - clears its queue: ClearAllActions asks ClearAction of each action, and ClearAction of
// the AttackObject deletes the combat round's schedule (RemoveAllActions, ClearAllAttacks) - the rest of the
// round's attacks are lost, the creature stands until the round ends, then casts. An attack never yields to an
// action queued behind it (AIActionAttackObject doesn't read the queue; it only ends when its target falls), and a
// round's end only starts the next round. NWN already keeps one spell a round (WaitForEndOfRound, the cast round)
// and queues several (once one is queued the state is 0x0E and a new order appends).
//
// So, for a player character attacking mid-round (the queue's head an AttackObject, the round running) - its
// Action this round is the Attack:
// - Its spell, item, feat or talent order (HandlePlayerToServerInputMessage, minors 0x12, 0x09, 0x06, 0x23) keeps
//   the attack: the order's ClearAllActions runs with ClearAction refusing the AttackObject (ClearAllActions keeps
//   an action whose ClearAction says no); the queue's other actions go, as in NWN.
// - The order's new actions (rules::PlaceOrder): those NWN makes wait for the round's end (WaitForEndOfRound 0x10 -
//   a spell, a scroll, a wand) stay after the attack; the others (a potion's one-second wait - a Bonus Action) go
//   before it, at the queue's head: the drink happens now and the attack carries on after it.
// - At the round's end (AIActionAttackObject with no round running) the attack yields to a spell or an item's
//   spell waiting behind it (rules::AttackYields): its own clean-up (ClearAction), done - the wait for the round's
//   end is over at once and the cast starts its cast round; after it, the default attack (default_attack.cpp).
// - An attack order on the target it's attacking (AddAttackActions, clearing the queue) clears what waits behind
//   the attack and keeps the attack - NWN's ClearAllActions would delete the round's attacks with it.
// Monsters and henchmen: their AI gives them one action a round itself (ClearAllActions, then the round's choice).

#include "action_order.h"

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "grenades.h"
#include "hooks.h"
#include "log.h"
#include "round_swings.h"
#include "rules/action_economy.h"
#include "rules/grenades.h"

bool UnseenHiderSearch(void* creature, void* node);  // rules_perception.cpp (#133)

namespace {

constexpr const char* kInputMessage = "?HandlePlayerToServerInputMessage@CNWSMessage@@QEAAHPEAVCNWSPlayer@@E@Z";
constexpr const char* kActionManager = "?ActionManager@CNWSCreature@@QEAAXI@Z";
constexpr const char* kClearAction = "?ClearAction@CNWSCreature@@MEAAHPEAVCNWSObjectActionNode@@H@Z";
constexpr const char* kClearAllActions = "?ClearAllActions@CNWSObject@@QEAAXXZ";
constexpr const char* kAIActionAttackObject = "?AIActionAttackObject@CNWSCreature@@QEAAIPEAVCNWSObjectActionNode@@@Z";
constexpr const char* kPlayerGameObject = "?GetGameObject@CNWSPlayer@@QEAAPEAVCNWSObject@@XZ";
constexpr const char* kListRemove = "?Remove@CExoLinkedListInternal@@QEAAPEAXPEAVCExoLinkedListNode@@@Z";
constexpr const char* kListAddHead = "?AddHead@CExoLinkedListInternal@@QEAAPEAVCExoLinkedListNode@@PEAX@Z";
constexpr size_t kRoundStarted = 0x2f04;           // CNWSCombatRound: a round running (#51's recon)
constexpr int kActionWaitForEndOfRound = 0x10;     // the engine's action ids (#120's recon)
constexpr uint32_t kOrderKept = 1;                 // ActionManager's type for a spell / item / feat / talent order

using InputFn = int (*)(void* message, void* player, uint8_t minor);
using ActionManagerFn = void (*)(void* creature, uint32_t type);
using ClearActionFn = int (*)(void* creature, void* node, int is_head);
using ClearAllActionsFn = void (*)(void* object);
using AttackObjectFn = uint32_t (*)(void* creature, void* node);
using AddAttackActionsFn = int (*)(void* creature, uint32_t target, int switched, int clear_first, int front);
using PlayerGameObjectFn = void* (*)(void* player);
using ListRemoveFn = void* (*)(void* list, void* position);
using ListAddHeadFn = void* (*)(void* list, void* data);

InputFn g_input = nullptr;
ActionManagerFn g_action_manager = nullptr;
ClearActionFn g_clear_action = nullptr;
AttackObjectFn g_attack_object = nullptr;
AddAttackActionsFn g_add_attack_actions = nullptr;

struct Api {
  ClearAllActionsFn clear_all = reinterpret_cast<ClearAllActionsFn>(EngineExport(kClearAllActions));
  PlayerGameObjectFn player_object = reinterpret_cast<PlayerGameObjectFn>(EngineExport(kPlayerGameObject));
  ListRemoveFn remove = reinterpret_cast<ListRemoveFn>(EngineExport(kListRemove));
  ListAddHeadFn add_head = reinterpret_cast<ListAddHeadFn>(EngineExport(kListAddHead));
};

const Api& E() {
  static const Api api;
  return api;
}

// The engine's server thread only.
thread_local int t_minor = -1;                       // the player's order under way (an input minor)
thread_local std::vector<void*>* t_before = nullptr;  // its queue once the order's clearing is done
thread_local bool t_keep_attack = false;             // ClearAllActions keeps the running attack
thread_local bool t_kept = false;
int g_lines = 0;

bool Faithful() { return g_config.actions == rules::RuleVariant::kFaithful; }
bool LogSlot() { return g_lines < 400 && ++g_lines; }
uint32_t IdOf(const void* object) { return object ? Field<uint32_t>(object, ee::kObjectId) : ee::kObjectInvalid; }
int ActionOf(const void* node) { return node ? Field<int32_t>(node, ee::kActionId) : -1; }

bool IsPlayer(void* creature) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  return stats && ReadCreatureFacts(stats).is_player;
}

void* ListOf(void* creature) { return creature ? Field<void*>(creature, ee::kObjectActionQueue) : nullptr; }

std::vector<void*> Positions(void* creature) {
  std::vector<void*> positions;
  void* list = ListOf(creature);
  for (void* at = list ? Field<void*>(list, ee::kListHead) : nullptr; at && positions.size() < 256;
       at = Field<void*>(at, ee::kListNodeNext)) {
    positions.push_back(at);
  }
  return positions;
}

bool RoundRunning(void* creature) {
  void* round = creature ? Field<void*>(creature, ee::kCreatureCombatRound) : nullptr;
  return round && Field<int32_t>(round, kRoundStarted) != 0;
}

// The running attack: an AttackObject at the queue's head (RunActions puts it back there between its turns).
void* HeadAttack(void* creature) {
  void* list = ListOf(creature);
  void* head = list ? Field<void*>(list, ee::kListHead) : nullptr;
  void* node = head ? Field<void*>(head, ee::kListNodeData) : nullptr;
  return ActionOf(node) == ee::kActionAttackObject ? node : nullptr;
}

bool AttackingMidRound(void* creature) { return HeadAttack(creature) && RoundRunning(creature); }

// #124 1A: a thrown grenade's wait for the round's end, left behind the attack (it runs, harmlessly, once the fight's
// attack is over) - not an Action waiting. By creature id; pruned as the queue loses them.
std::unordered_map<uint32_t, std::vector<void*>> g_left_waits;

bool LeftWait(void* creature, void* node) {
  if (g_left_waits.empty()) return false;
  const auto it = g_left_waits.find(IdOf(creature));
  if (it == g_left_waits.end()) return false;
  const std::vector<void*> queue = QueueNodes(creature);
  std::vector<void*>& left = it->second;
  left.erase(std::remove_if(left.begin(), left.end(),
                            [&](void* wait) { return std::find(queue.begin(), queue.end(), wait) == queue.end(); }),
             left.end());
  const bool found = std::find(left.begin(), left.end(), node) != left.end();
  if (left.empty()) g_left_waits.erase(it);
  return found;
}

// A spell or an item's spell (or its wait for the round's end) waiting in the queue.
bool ActionWaiting(void* creature) {
  for (void* node : QueueNodes(creature)) {
    const int action = ActionOf(node);
    if (action == kActionWaitForEndOfRound && LeftWait(creature, node)) continue;  // #124
    if (action == kActionWaitForEndOfRound || action == ee::kActionCastSpell || action == ee::kActionItemCastSpell) {
      return true;
    }
  }
  return false;
}

const char* OrderName(int minor) {
  switch (minor) {
    case 0x12: return "a spell";
    case 0x09: return "an item";
    case 0x06: return "a feat";
    case 0x23: return "a talent";
    default: return "an order";
  }
}

// A player's spell, item, feat or talent order while it attacks mid-round.
int OnInput(void* message, void* player, uint8_t minor) {
  if (!Faithful() || !rules::OrderKeepsAttack(minor) || !player || !E().player_object) {
    return g_input(message, player, minor);
  }
  void* object = E().player_object(player);  // a creature only (a DM's avatar, say, may not be)
  void* creature = object ? CreatureById(IdOf(object)) : nullptr;
  if (!creature || !IsPlayer(creature) || !AttackingMidRound(creature)) return g_input(message, player, minor);
  std::vector<void*> before = QueueNodes(creature);
  const int outer_minor = t_minor;
  std::vector<void*>* outer_before = t_before;
  t_minor = minor;
  t_before = &before;
  const int result = g_input(message, player, minor);
  t_minor = outer_minor;
  t_before = outer_before;
  PlaceNewOrder(creature, before, OrderName(minor));
  return result;
}

// The order's clearing (ActionManager(1)) keeps the running attack.
void OnActionManager(void* creature, uint32_t type) {
  if (type != kOrderKept || t_minor < 0 || t_keep_attack || !IsPlayer(creature) || !AttackingMidRound(creature)) {
    g_action_manager(creature, type);
    return;
  }
  t_keep_attack = true;
  t_kept = false;
  g_action_manager(creature, type);
  t_keep_attack = false;
  if (t_before) *t_before = QueueNodes(creature);  // what the clearing left (freed actions' memory may be reused)
  if (t_kept && LogSlot()) {
    Log("actions: 0x%08X %s mid-round - its attack kept (the round's attacks go on)", IdOf(creature),
        OrderName(t_minor));
  }
}

int OnClearAction(void* creature, void* node, int is_head) {
  if (t_keep_attack && ActionOf(node) == ee::kActionAttackObject) {
    t_kept = true;
    return 0;  // ClearAllActions keeps it
  }
  return g_clear_action(creature, node, is_head);
}

// At the round's end the attack yields to an Action waiting behind it.
uint32_t OnAttackObject(void* creature, void* node) {
  // #133 (owner, 2026-11-04: "go with B"): an AI creature's attack on a hider it doesn't see becomes a search.
  if (creature && node && UnseenHiderSearch(creature, node)) {
    static const auto clear_action = reinterpret_cast<int (*)(void*, void*, int)>(EngineExport(kClearAction));
    const auto clear = g_clear_action ? g_clear_action : clear_action;
    if (clear) clear(creature, node, 1);  // the attack's own clean-up: its target, the round's schedule
    return ee::kActionDone;
  }
  if (Faithful() && creature && !RoundRunning(creature) && IsPlayer(creature) &&
      rules::AttackYields(false, ActionWaiting(creature))) {
    g_clear_action(creature, node, 1);  // the attack's own clean-up: its target, the round's schedule
    if (LogSlot()) {
      Log("actions: 0x%08X the round is over - its attack yields to the Action waiting behind it", IdOf(creature));
    }
    return ee::kActionDone;
  }
  return g_attack_object(creature, node);
}

// An attack order on the target it's attacking mid-round: what waits behind goes, the attack (its round) stays.
int OnAddAttackActions(void* creature, uint32_t target, int switched, int clear_first, int front) {
  if (Faithful() && clear_first && E().clear_all && IsPlayer(creature)) {
    void* attack = HeadAttack(creature);
    if (attack && RoundRunning(creature) && Field<uint32_t>(attack, ee::kActionFirstParam) == target &&
        QueueNodes(creature).size() > 1) {
      t_keep_attack = true;
      E().clear_all(creature);
      t_keep_attack = false;
      if (LogSlot()) {
        Log("actions: 0x%08X ordered to attack 0x%08X, its own target - the queue cleared, the round's attacks kept",
            IdOf(creature), target);
      }
      return 1;
    }
  }
  return g_add_attack_actions(creature, target, switched, clear_first, front);
}

}  // namespace

std::vector<void*> QueueNodes(void* creature) {
  std::vector<void*> nodes;
  for (void* at : Positions(creature)) nodes.push_back(Field<void*>(at, ee::kListNodeData));
  return nodes;
}

void PlaceNewOrder(void* creature, const std::vector<void*>& before, const char* what) {
  if (!Faithful() || !creature || !AttackingMidRound(creature) || !E().remove || !E().add_head) return;
  std::vector<void*> fresh;  // the order's new actions (their list positions), in order
  bool waits = false;
  for (void* at : Positions(creature)) {
    void* node = Field<void*>(at, ee::kListNodeData);
    if (std::find(before.begin(), before.end(), node) != before.end()) continue;
    fresh.push_back(at);
    if (ActionOf(node) == kActionWaitForEndOfRound) waits = true;
  }
  if (fresh.empty()) return;
  const uint32_t id = IdOf(creature);
  // #124 1A: a grenade thrown mid-round goes at once - its cast before the attack, its wait for the round's end left
  // behind it - and takes one of the round's remaining swings (round_swings.cpp).
  bool grenade = false;
  for (void* at : fresh) grenade = grenade || IsGrenadeCast(Field<void*>(at, ee::kListNodeData));
  if (rules::ThrowGoesNow(true, grenade, GrenadesFaithful())) {
    void* list = ListOf(creature);
    for (auto at = fresh.rbegin(); at != fresh.rend(); ++at) {
      void* node = Field<void*>(*at, ee::kListNodeData);
      if (ActionOf(node) == kActionWaitForEndOfRound) {
        g_left_waits[id].push_back(node);
        continue;
      }
      void* moved = E().remove(list, *at);
      if (moved) E().add_head(list, moved);
    }
    TakeRoundSwing(creature);
    if (LogSlot()) Log("actions: 0x%08X %s - a grenade thrown at once, one of its swings for it (#124)", id, what);
    return;
  }
  if (rules::PlaceOrder(true, waits) == rules::OrderPlace::kAfterAttacks) {
    if (LogSlot()) Log("actions: 0x%08X %s waits behind the round's attacks (the next round's Action)", id, what);
    return;
  }
  void* list = ListOf(creature);
  for (auto at = fresh.rbegin(); at != fresh.rend(); ++at) {
    void* node = E().remove(list, *at);
    if (node) E().add_head(list, node);
  }
  if (LogSlot()) Log("actions: 0x%08X %s goes before its attack - the attack carries on after it", id, what);
}

void InstallActionOrders() {
  if (!Faithful()) {
    // #133: the attack hook for the search of an unseen hider (perception faithful), without the Action queue.
    if (g_config.perception == rules::RuleVariant::kFaithful) {
      HookExport("CNWSCreature::AIActionAttackObject (#133: an unseen hider searched for)", kAIActionAttackObject,
                 reinterpret_cast<void*>(&OnAttackObject), reinterpret_cast<void**>(&g_attack_object));
    }
    return;
  }
  if (!E().clear_all || !E().player_object || !E().remove || !E().add_head) {
    Log("SKIP the Action queue: engine functions missing");
    return;
  }
  const bool clear = HookExport("CNWSCreature::ClearAction (the Action queue)", kClearAction,
                                reinterpret_cast<void*>(&OnClearAction), reinterpret_cast<void**>(&g_clear_action));
  const bool manager = clear && HookExport("CNWSCreature::ActionManager (the Action queue)", kActionManager,
                                           reinterpret_cast<void*>(&OnActionManager),
                                           reinterpret_cast<void**>(&g_action_manager));
  const bool input = manager && HookExport("CNWSMessage::HandlePlayerToServerInputMessage (the Action queue)",
                                           kInputMessage, reinterpret_cast<void*>(&OnInput),
                                           reinterpret_cast<void**>(&g_input));
  const bool yields = HookExport("CNWSCreature::AIActionAttackObject (the Action queue)", kAIActionAttackObject,
                                 reinterpret_cast<void*>(&OnAttackObject), reinterpret_cast<void**>(&g_attack_object));
  const bool again = clear && HookExport("CNWSCreature::AddAttackActions (the Action queue)", ee::kAddAttackActions,
                                         reinterpret_cast<void*>(&OnAddAttackActions),
                                         reinterpret_cast<void**>(&g_add_attack_actions));
  Log("actions: the Action queue - %s; %s; %s", input ? "an order mid-round keeps the attack" : "NOT kept (no hook)",
      yields ? "the attack yields at the round's end" : "NOT yielding (no hook)",
      again ? "an attack order on its own target keeps the round" : "a re-click drops the round's attacks");
}
