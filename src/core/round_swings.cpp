// One swing per attack (#53; docs/PLAN.md, #53's section; owner, 2026-11-03: "AS-1 and 2 look funky, especially 2.
// if AS gets activated before combat the PC looks like it half swings the sword before each attack but only attacks
// once", then "go with A"). Both modes: how a round shows its attacks, not how many it has.
//
// CNWSCombatRound::InitializeAttackActions (0x376EE0; StartCombatRound's, after InitializeNumberOfAttacks) lays a
// round's n attacks out as at most three swings - 0x28-byte round actions, type 1 (+0x10), due (+0x00) at the start
// delay, +2 s, +4 s, the attacks they carry (+0x0C) n / 3 (the first ones one more), each one's animation (+0x08)
// rules::EngineSwingMs(n) - then takes their total (rules::EngineRoundSwingTotalMs) off the round's timer
// (DecrementRoundLength: the dues scaled down with it). AIActionAttackObject runs a due swing: the round paused (its
// timer standing still) for its animation, or (k + 1) / 2 of it for k attacks - the next attack's animation starts
// halfway through the first, so past three attacks the first of each pair is cut off (the owner's "half swings") -
// then ResolveAttack(target, k, animation), and the action freed.
//
// After it, a round with a swing carrying more than one attack gets one swing per attack: the swing split (copies
// by the engine's operator new - the call AddCleaveAttack makes - added with CNWSCombatRound::AddAction, which keeps
// the list in due order), the dues and the round's timer from rules::OneSwingPerAttack (the round's 6 s kept).
// Action Surge in a running round (extra_actions.cpp) adds its attacks the same way, as swings of their own spaced
// with the pending ones (rules::AddedSwings), in place of AddCleaveAttack (all of them at once, at the list's head,
// a Cleave tag on each).

#include "round_swings.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <vector>

#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "rules/swing_spacing.h"

namespace {

using InitializeAttackActionsFn = void (*)(void* round, uint32_t target);
using AddActionFn = void (*)(void* round, void* action);
using NewFn = void* (*)(size_t size);

constexpr const char* kInitializeAttackActions = "?InitializeAttackActions@CNWSCombatRound@@QEAAXI@Z";
constexpr const char* kAddAction = "?AddAction@CNWSCombatRound@@QEAAXPEAVCNWSCombatRoundAction@@@Z";
constexpr const char* kNewPattern = "B9 28 00 00 00 E8 ?? ?? ?? ??";  // AddCleaveAttack: mov ecx, 28h; call new
constexpr size_t kNewCall = 5;

constexpr size_t kRoundStarted = 0x2f04;
constexpr size_t kRoundCastRound = 0x2f08;
constexpr size_t kRoundTimer = 0x2f0c;
constexpr size_t kRoundLength = 0x2f10;
constexpr size_t kRoundEffectAttacks = 0x2f64;  // InitializeAttackActions adds +0x2F50..+0x2F64
constexpr size_t kRoundActionList = 0x2f70;     // -> list wrapper -> CExoLinkedListInternal* (head +0)
constexpr size_t kNodeNext = 0x8;
constexpr size_t kNodeData = 0x10;
// A round action (0x28 bytes; its constructor: +0x14 and +0x20 0x7F000000, +0x18 int64 1, +0x24 uint16 0xFFFF).
constexpr size_t kActionSize = 0x28;
constexpr size_t kActionDue = 0x00;
constexpr size_t kActionAnimation = 0x04;  // uint16, 9 for an attack
constexpr size_t kActionLength = 0x08;     // the swing's animation, ms
constexpr size_t kActionAttacks = 0x0c;
constexpr size_t kActionType = 0x10;
constexpr size_t kActionTarget = 0x14;
constexpr size_t kActionSet18 = 0x18;
constexpr size_t kActionSet20 = 0x20;
constexpr size_t kActionSet24 = 0x24;
constexpr uint8_t kActionAttack = 1;
constexpr uint16_t kAnimationAttack = 9;
constexpr int kMaxNodes = 128;
constexpr int kMaxAttacks = 40;  // the round's attack data holds 50 (GetAttack)

InitializeAttackActionsFn g_initialize = nullptr;
AddActionFn g_add_action = nullptr;
NewFn g_new = nullptr;
int g_lines = 0;

bool LogSlot() { return g_lines < 300 && ++g_lines; }

uint32_t OwnerId(void* round) {
  void* owner = Field<void*>(round, ee::kRoundOwner);
  return owner ? Field<uint32_t>(owner, ee::kObjectId) : ee::kObjectInvalid;
}

std::vector<void*> Nodes(void* round) {
  std::vector<void*> nodes;
  void* wrapper = Field<void*>(round, kRoundActionList);
  void* list = wrapper ? *static_cast<void**>(wrapper) : nullptr;
  void* node = list ? *static_cast<void**>(list) : nullptr;
  for (int guard = 0; node && guard < kMaxNodes; ++guard, node = Field<void*>(node, kNodeNext)) nodes.push_back(node);
  return nodes;
}

std::vector<void*> AttackActions(void* round) {
  std::vector<void*> attacks;
  for (void* node : Nodes(round)) {
    void* action = Field<void*>(node, kNodeData);
    if (action && Field<uint8_t>(action, kActionType) == kActionAttack) attacks.push_back(action);
  }
  return attacks;
}

// The list back in due order (stable): its actions moved between its nodes.
void SortByDue(void* round) {
  const std::vector<void*> nodes = Nodes(round);
  for (size_t i = 1; i < nodes.size(); ++i) {
    for (size_t j = i; j > 0; --j) {
      void* before = Field<void*>(nodes[j - 1], kNodeData);
      void* after = Field<void*>(nodes[j], kNodeData);
      if (!before || !after || Field<int32_t>(before, kActionDue) <= Field<int32_t>(after, kActionDue)) break;
      SetField<void*>(nodes[j - 1], kNodeData, after);
      SetField<void*>(nodes[j], kNodeData, before);
    }
  }
}

// A one-attack swing: a copy of `model` (a swing of the round's), or made as InitializeAttackActions makes one.
void* NewSwing(const void* model, int due, uint32_t target, int length) {
  void* action = g_new(kActionSize);
  if (!action) return nullptr;
  if (model) {
    memcpy(action, model, kActionSize);
  } else {
    memset(action, 0, kActionSize);
    SetField<uint16_t>(action, kActionAnimation, kAnimationAttack);
    SetField<uint8_t>(action, kActionType, kActionAttack);
    SetField<int64_t>(action, kActionSet18, 1);
    SetField<uint32_t>(action, kActionSet20, ee::kObjectInvalid);
    SetField<uint16_t>(action, kActionSet24, 0xffff);
  }
  SetField<int32_t>(action, kActionDue, due);
  SetField<int32_t>(action, kActionAttacks, 1);
  SetField<uint32_t>(action, kActionTarget, target);
  SetField<int32_t>(action, kActionLength, length);
  return action;
}

// After InitializeAttackActions: a swing carrying k > 1 attacks becomes k swings.
void SplitSwings(void* round) {
  const std::vector<void*> attacks = AttackActions(round);
  int total = 0;
  bool shared = false;
  for (void* action : attacks) {
    const int carried = Field<int32_t>(action, kActionAttacks);
    if (carried < 1) return;
    total += carried;
    shared = shared || carried > 1;
  }
  if (!shared || total > kMaxAttacks) return;
  const int swing = Field<int32_t>(attacks.front(), kActionLength);
  const int length_was = Field<int32_t>(round, kRoundLength);
  const rules::SwingLayout plan =
      rules::OneSwingPerAttack(total, Field<int32_t>(attacks.front(), kActionDue), length_was, swing);
  if (static_cast<int>(plan.dues.size()) != total) return;
  std::vector<void*> copies;  // made first: all of them or no change
  for (int i = static_cast<int>(attacks.size()); i < total; ++i) {
    void* copy = g_new(kActionSize);
    if (!copy) {
      Log("round swings: 0x%08X the engine's operator new answered nothing - the round left as it was",
          OwnerId(round));
      return;
    }
    copies.push_back(copy);
  }
  size_t made = 0;
  size_t index = 0;
  for (void* action : attacks) {
    const int carried = Field<int32_t>(action, kActionAttacks);
    SetField<int32_t>(action, kActionDue, plan.dues[index++]);
    SetField<int32_t>(action, kActionAttacks, 1);
    for (int j = 1; j < carried; ++j) {
      void* copy = copies[made++];
      memcpy(copy, action, kActionSize);
      SetField<int32_t>(copy, kActionDue, plan.dues[index++]);
      g_add_action(round, copy);
    }
  }
  SetField<int32_t>(round, kRoundLength, plan.round_length);
  SortByDue(round);
  if (LogSlot()) {
    Log("round swings: 0x%08X %d attacks in %d swings -> one swing each (%d ms, every %d ms of round time; the "
        "round's timer %d -> %d ms)",
        OwnerId(round), total, static_cast<int>(attacks.size()), swing,
        total > 1 ? plan.dues[1] - plan.dues[0] : 0, length_was, plan.round_length);
  }
}

void InitializeAttackActions(void* round, uint32_t target) {
  g_initialize(round, target);
  if (round) SplitSwings(round);
}

}  // namespace

bool AddRoundSwings(void* round, uint32_t target, int count) {
  if (!round || count <= 0 || !g_new || !g_add_action) return false;
  if (Field<int32_t>(round, kRoundStarted) == 0 || Field<int32_t>(round, kRoundCastRound) != 0) return false;
  const std::vector<void*> pending = AttackActions(round);
  const int extra = Field<int32_t>(round, ee::kRoundExtraAttacks);
  const int total = Field<int32_t>(round, ee::kRoundMainHandAttacks) + Field<int32_t>(round, ee::kRoundOffHandAttacks) +
                    extra + Field<int32_t>(round, kRoundEffectAttacks) + count;
  if (total > kMaxAttacks) return false;
  const int swing = rules::EngineSwingMs(total);
  const int now = Field<int32_t>(round, kRoundTimer);
  const int length_was = Field<int32_t>(round, kRoundLength);
  const rules::SwingLayout plan = rules::AddedSwings(static_cast<int>(pending.size()), count, now, length_was);
  if (plan.dues.size() != pending.size() + static_cast<size_t>(count)) return false;
  std::vector<void*> swings;  // made first: all of them or no change
  for (int i = 0; i < count; ++i) {
    void* action = NewSwing(pending.empty() ? nullptr : pending.front(), 0, target, swing);
    if (!action) return false;
    swings.push_back(action);
  }
  size_t index = 0;
  for (void* action : pending) SetField<int32_t>(action, kActionDue, plan.dues[index++]);
  for (void* action : swings) {
    SetField<int32_t>(action, kActionDue, plan.dues[index++]);
    g_add_action(round, action);
  }
  SetField<int32_t>(round, ee::kRoundExtraAttacks, extra + count);  // counted, as AddCleaveAttack counts its own
  SetField<int32_t>(round, kRoundLength, plan.round_length);
  SortByDue(round);
  if (LogSlot()) {
    Log("round swings: 0x%08X %d swing(s) added at %d ms (%d to come; %d ms each, every %d ms of round time; the "
        "round's timer %d -> %d ms)",
        OwnerId(round), count, now, static_cast<int>(pending.size()), swing,
        plan.dues.size() > 1 ? plan.dues[1] - plan.dues[0] : 0, length_was, plan.round_length);
  }
  return true;
}

void InstallRoundSwings() {
  g_add_action = reinterpret_cast<AddActionFn>(EngineExport(kAddAction));
  const uintptr_t cleave = EngineExport(ee::kAddCleaveAttack);
  const uintptr_t site = cleave ? FindPatternIn("round swings: operator new", FunctionAt(cleave), kNewPattern) : 0;
  g_new = reinterpret_cast<NewFn>(site ? CallTarget(site + kNewCall) : 0);
  if (!g_add_action || !g_new) {
    Log("round swings: not installed (AddAction %s, operator new %s) - NWN's swings kept",
        g_add_action ? "found" : "missing", g_new ? "found" : "missing");
    return;
  }
  if (!HookExport("CNWSCombatRound::InitializeAttackActions (one swing per attack)", kInitializeAttackActions,
                  reinterpret_cast<void*>(&InitializeAttackActions), reinterpret_cast<void**>(&g_initialize))) {
    return;
  }
  Log("round swings: one swing per attack (#53)");
}
