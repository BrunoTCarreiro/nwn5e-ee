// A player's equip / unequip dropped in combat gets its answer (0.45.2; owner's
// test of 0.45.1: a halfling's greataxe, swapped out for a bow, "wouldn't
// even let me grab the axe" - an engine bug, both modes).
//
// In combat a player's equip or unequip is not done at once: AddEquipItemActions
// / AddUnequipActions put it on the combat round's action list
// (CNWSCombatRound::AddEquipAction / AddUnequipAction: a 0x28-byte action,
// type 6 equip - item +0x14, slot +0x1c - or 7 unequip - item +0x14), which a
// round's end replays (EndCombatRound -> ProcessPendingCombatActions; ClearAction
// on an attack does the same first). But RemoveAllActions (0x378d60) - called
// when the combat state ends (SetCombatState 0, ClearAllActions), the combat
// timer runs out (AIUpdate), the attack target changes, PacifyCreature, a
// whirlwind - frees the list and answers nothing. The client marks an item it
// asked about as waiting (CNWCItem +0x240) and won't let it be picked up until
// an Inventory reply names it (HandleServerToPlayerInventory), so the item stays
// stuck for the session (RE, 89.8193.37). Here, before RemoveAllActions, every
// queued equip / unequip of a player's item is answered EquipCancel /
// UnequipCancel, as AddEquipAction itself answers one it replaces: the item is
// free again and the player drags it once more. What a normal round end replays
// never reaches this (it is gone from the list by then); an item that no longer
// exists is skipped.

#include <stdint.h>

#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"

namespace {

using RemoveAllActionsFn = void (*)(void* round);
using GetMessageFn = void* (*)(void* server_app);
using PlayerIdFn = uint32_t (*)(void* server_app, uint32_t object_id);
using ItemByIdFn = void* (*)(void* server_app, uint32_t object_id);
using EquipCancelFn = int (*)(void* message, uint32_t player, uint32_t item, uint32_t slot, int other);
using UnequipCancelFn = int (*)(void* message, uint32_t player, uint32_t item, int other);

constexpr const char* kRemoveAllActions = "?RemoveAllActions@CNWSCombatRound@@QEAAXXZ";
constexpr const char* kGetNWSMessage = "?GetNWSMessage@CServerExoApp@@QEAAPEAVCNWSMessage@@XZ";
constexpr const char* kGetPlayerId = "?GetPlayerIDByGameObjectID@CServerExoApp@@QEAAII@Z";
constexpr const char* kGetItemById = "?GetItemByGameObjectID@CServerExoApp@@QEAAPEAVCNWSItem@@I@Z";
constexpr const char* kEquipCancel = "?SendServerToPlayerInventory_EquipCancel@CNWSMessage@@QEAAHIIIH@Z";
constexpr const char* kUnequipCancel = "?SendServerToPlayerInventory_UnequipCancel@CNWSMessage@@QEAAHIIH@Z";

constexpr size_t kRoundActionList = 0x2f70;  // -> list wrapper -> CExoLinkedListInternal* (head +0)
constexpr size_t kNodeNext = 0x8;
constexpr size_t kNodeData = 0x10;
constexpr size_t kActionType = 0x10;  // byte
constexpr size_t kActionItem = 0x14;
constexpr size_t kActionSlot = 0x1c;
constexpr uint8_t kActionEquip = 6;
constexpr uint8_t kActionUnequip = 7;
constexpr uint32_t kNoPlayer = 0xfffffffe;

RemoveAllActionsFn g_remove_all = nullptr;
GetMessageFn g_get_message = nullptr;
PlayerIdFn g_player_id = nullptr;
ItemByIdFn g_item_by_id = nullptr;
EquipCancelFn g_equip_cancel = nullptr;
UnequipCancelFn g_unequip_cancel = nullptr;
void** g_app_manager = nullptr;
int g_logged = 0;

void* ServerApp() {
  return g_app_manager && *g_app_manager ? Field<void*>(*g_app_manager, ee::kAppManagerServerApp) : nullptr;
}

// The queued equips / unequips of a player's items, answered as cancelled.
void AnswerDropped(void* round) {
  void* wrapper = Field<void*>(round, kRoundActionList);
  void* list = wrapper ? *static_cast<void**>(wrapper) : nullptr;
  void* node = list ? *static_cast<void**>(list) : nullptr;
  if (!node) return;
  void* owner = Field<void*>(round, ee::kRoundOwner);
  void* app = ServerApp();
  if (!owner || !app) return;
  const uint32_t player = g_player_id(app, Field<uint32_t>(owner, ee::kObjectId));
  if (player == kNoPlayer || player == ee::kObjectInvalid) return;  // not a player's creature
  void* message = g_get_message(app);
  if (!message) return;
  for (int guard = 0; node && guard < 256; ++guard, node = Field<void*>(node, kNodeNext)) {
    const void* action = Field<void*>(node, kNodeData);
    if (!action) continue;
    const uint8_t type = Field<uint8_t>(action, kActionType);
    if (type != kActionEquip && type != kActionUnequip) continue;
    const uint32_t item = Field<uint32_t>(action, kActionItem);
    if (!g_item_by_id(app, item)) continue;  // gone: nothing for the client to free
    if (type == kActionEquip) {
      g_equip_cancel(message, player, item, Field<uint32_t>(action, kActionSlot), 0);
    } else {
      g_unequip_cancel(message, player, item, 0);
    }
    if (g_logged < 20) {
      ++g_logged;
      Log("equip: 0x%08X's queued %s of item 0x%08X dropped by the engine - answered as cancelled (the item is free "
          "to move again)",
          Field<uint32_t>(owner, ee::kObjectId), type == kActionEquip ? "equip" : "unequip", item);
    }
  }
}

void RemoveAllActions(void* round) {
  if (round) AnswerDropped(round);
  g_remove_all(round);
}

}  // namespace

void InstallEquipCancelFix() {
  g_get_message = reinterpret_cast<GetMessageFn>(EngineExport(kGetNWSMessage));
  g_player_id = reinterpret_cast<PlayerIdFn>(EngineExport(kGetPlayerId));
  g_item_by_id = reinterpret_cast<ItemByIdFn>(EngineExport(kGetItemById));
  g_equip_cancel = reinterpret_cast<EquipCancelFn>(EngineExport(kEquipCancel));
  g_unequip_cancel = reinterpret_cast<UnequipCancelFn>(EngineExport(kUnequipCancel));
  g_app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  if (!g_get_message || !g_player_id || !g_item_by_id || !g_equip_cancel || !g_unequip_cancel || !g_app_manager) {
    Log("SKIP equip: an item dropped from the combat round stays stuck in the inventory (a piece is missing)");
    return;
  }
  if (HookExport("CNWSCombatRound::RemoveAllActions (a dropped equip / unequip is answered)", kRemoveAllActions,
                 reinterpret_cast<void*>(&RemoveAllActions), reinterpret_cast<void**>(&g_remove_all))) {
    Log("equip: an equip or unequip the combat round drops is answered as cancelled (no stuck items)");
  }
}
