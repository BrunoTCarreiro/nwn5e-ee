// The quickbar (rules/quickbar.h; docs/PLAN.md "Quickbar"; owner, 2026-10-27:
// "do the 1-9 hook plus auto-placing new actions"), `quickbar` faithful:
// - Auto-placing (the server): nwn5e's class actions go on a player's
//   quickbar the first time the character has them - the first empty slot,
//   keys 1-9 / F1-F9 first - and a chat line says where. The engine keeps a
//   creature's 36 buttons at CNWSCreature +0x4e0 (CNWSQuickbarButton, 0x60
//   bytes: +0x08 the type, 4 a feat; +0x50 the number, for a feat the feat or a
//   sub-radial option's FeatID - CNWSCreature::GetQuickbarButton 0x392f60,
//   SetQuickbarButton_GeneralINTParam 0x3a3f80); a button set there goes to the
//   player with CNWSMessage::SendServerToPlayerGuiQuickbar_SetButton (0x4d9da0:
//   one slot, or all 36) and is saved with the character. What was placed is
//   recorded (nwn5e_quickbar_placed, a bit per action), so a button the
//   player takes off stays off. `!quickbar` lists them, `!quickbar reset`
//   forgets the record (the missing ones come back on empty slots).
// - The number keys (the client): keys 1-9 press quickbar slots 1-9, as F1-F9
//   (see InstallQuickbarKeys below).

#include <windows.h>

#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "chat.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "records.h"
#include "rules/nwn_conversion.h"
#include "rules/quickbar.h"

bool QuickbarNumberKeysOn();  // below: the client hook installed

namespace {

using SetIntParamFn = void (*)(void* creature, uint8_t slot, uint8_t type, int value);
using SendButtonFn = int (*)(void* message, void* player, uint8_t slot, int all);
using GetMessageFn = void* (*)(void* server_app);
using PlayerByObjectFn = void* (*)(void* server_app, uint32_t object_id);

constexpr const char* kSetIntParam = "?SetQuickbarButton_GeneralINTParam@CNWSCreature@@QEAAXEEH@Z";
constexpr const char* kSendButton = "?SendServerToPlayerGuiQuickbar_SetButton@CNWSMessage@@QEAAHPEAVCNWSPlayer@@EH@Z";
constexpr const char* kGetNWSMessage = "?GetNWSMessage@CServerExoApp@@QEAAPEAVCNWSMessage@@XZ";
constexpr const char* kPlayerByObject = "?GetClientObjectByObjectId@CServerExoApp@@QEAAPEAVCNWSPlayer@@I@Z";
constexpr size_t kCreatureQuickbar = 0x4e0;  // CNWSQuickbarButton[36] (null before InitializeQuickbar)
constexpr size_t kButtonSize = 0x60;
constexpr size_t kButtonType = 0x08;      // byte
constexpr size_t kButtonIntParam = 0x50;  // int32
constexpr const char* kVarPlaced = "nwn5e_quickbar_placed";
constexpr DWORD kCheckEveryMs = 2000;

SetIntParamFn g_set_int_param = nullptr;
SendButtonFn g_send_button = nullptr;
GetMessageFn g_get_message = nullptr;
PlayerByObjectFn g_player_by_object = nullptr;
void** g_app_manager = nullptr;
std::unordered_map<uint32_t, DWORD> g_last_check;  // by creature id
bool g_full_logged = false;

uint32_t IdOf(const void* creature) { return Field<uint32_t>(creature, ee::kObjectId); }

void* ServerApp() {
  return g_app_manager && *g_app_manager ? Field<void*>(*g_app_manager, ee::kAppManagerServerApp) : nullptr;
}

std::vector<rules::QuickbarAction> ActionsOf(void* stats) {
  rules::QuickbarFacts facts;
  for (int feat : rules::QuickbarActionFeats2024()) {
    if (engine::HasFeat(stats, feat)) facts.feats.push_back(feat);
  }
  for (const rules::ClassLevels& c : rules::GroupClassLevels(ReadClassSlots(stats))) {
    if (c.character_class == rules::CharacterClass::kBarbarian) facts.barbarian_level = c.levels;
  }
  return rules::QuickbarActions2024(facts);
}

bool ReadBar(void* creature, std::vector<rules::QuickbarSlot>* bar) {
  const uint8_t* buttons = Field<const uint8_t*>(creature, kCreatureQuickbar);
  if (!buttons) return false;
  bar->assign(rules::kQuickbarSlots, rules::QuickbarSlot{});
  for (int i = 0; i < rules::kQuickbarSlots; ++i) {
    (*bar)[i].type = buttons[i * kButtonSize + kButtonType];
    (*bar)[i].value = *reinterpret_cast<const int32_t*>(buttons + i * kButtonSize + kButtonIntParam);
  }
  return true;
}

// Plans and writes; the chat line names the new buttons. Returns how many were written.
int PlaceActions(void* creature, int placed, bool tell) {
  if (!g_set_int_param || !g_send_button || !g_get_message || !g_player_by_object) return 0;
  void* stats = StatsOf(creature);
  void* app = ServerApp();
  if (!stats || !app) return 0;
  void* player = g_player_by_object(app, IdOf(creature));
  void* message = g_get_message(app);
  std::vector<rules::QuickbarSlot> bar;
  if (!player || !message || !ReadBar(creature, &bar)) return 0;
  const std::vector<rules::QuickbarAction> actions = ActionsOf(stats);
  if (actions.empty()) return 0;
  int now_placed = placed;
  const std::vector<rules::QuickbarChange> changes = rules::PlanQuickbar(bar, placed, actions, &now_placed);
  const uint32_t id = IdOf(creature);
  std::string line;
  for (const rules::QuickbarChange& c : changes) {
    g_set_int_param(creature, static_cast<uint8_t>(c.slot), static_cast<uint8_t>(rules::kQuickbarTypeFeat), c.value);
    g_send_button(message, player, static_cast<uint8_t>(c.slot), 0);
    const char* name = "";
    for (const rules::QuickbarAction& a : actions) {
      if (a.record_bit == c.record_bit) name = a.name;
    }
    Log("quickbar: 0x%08X slot %d <- %s (%d)%s", id, c.slot, name, c.value, c.refresh ? " - the button follows the feat" : "");
    if (c.refresh) continue;
    line += (line.empty() ? "" : ", ") + std::string(name) + " (" + rules::QuickbarSlotKeys(c.slot, QuickbarNumberKeysOn()) + ")";
  }
  if (now_placed != placed) SetRecord(creature, kVarPlaced, now_placed);
  if (tell && !line.empty()) TellPlayer(id, "nwn5e: on your quickbar - " + line + ".");
  for (const rules::QuickbarAction& a : actions) {  // a full bar: said once a session in the log
    if (!(now_placed & (1 << a.record_bit)) && !g_full_logged) {
      g_full_logged = true;
      Log("quickbar: 0x%08X no empty slot for %s - it waits for one", id, a.name);
    }
  }
  return static_cast<int>(changes.size());
}

void QuickbarCreatureUpdate(void* creature) {
  const uint32_t id = IdOf(creature);
  if (id == ee::kObjectInvalid) return;
  const DWORD now = GetTickCount();
  auto last = g_last_check.find(id);
  if (last != g_last_check.end() && now - last->second < kCheckEveryMs) return;
  g_last_check[id] = now;
  void* stats = StatsOf(creature);
  if (!stats || !engine::AreaOf(creature)) return;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.is_player || !facts.character || !facts.plausible) return;
  EnsureRecords(creature);
  PlaceActions(creature, engine::GetLocalInt(creature, kVarPlaced), true);
}

// !quickbar [reset]
std::vector<std::string> QuickbarCommand(void* creature, const std::string& args) {
  if (g_config.quickbar != rules::RuleVariant::kFaithful) {
    return {"nwn5e: !quickbar - the quickbar's number keys and placed actions are faithful-only (quickbar=faithful)."};
  }
  void* stats = StatsOf(creature);
  if (!stats) return {"nwn5e: no character."};
  EnsureRecords(creature);
  std::vector<std::string> lines;
  if (args == "reset") {
    SetRecord(creature, kVarPlaced, 0);
    const int written = PlaceActions(creature, 0, true);
    lines.push_back("nwn5e: quickbar record cleared - " + std::to_string(written) +
                    " action(s) put back on empty slots.");
  }
  lines.push_back(QuickbarNumberKeysOn() ? "nwn5e: keys 1-9 press quickbar slots 1-9 (as F1-F9) when no text box has the keyboard."
                                         : "nwn5e: the number keys aren't hooked (see the log) - F1-F12 as usual.");
  std::vector<rules::QuickbarSlot> bar;
  const bool have_bar = ReadBar(creature, &bar);
  const int placed = engine::GetLocalInt(creature, kVarPlaced);
  for (const rules::QuickbarAction& a : ActionsOf(stats)) {
    std::string where;
    for (int i = 0; have_bar && i < rules::kQuickbarSlots; ++i) {
      if (bar[i].type == rules::kQuickbarTypeFeat && rules::SameQuickbarAction(bar[i].value, a.value)) {
        where += (where.empty() ? "" : ", ") + rules::QuickbarSlotKeys(i, QuickbarNumberKeysOn());
      }
    }
    if (where.empty()) {
      where = (placed & (1 << a.record_bit)) ? "not on the quickbar (taken off - !quickbar reset puts it back)"
                                             : "not on the quickbar yet (no empty slot)";
    }
    lines.push_back("  " + std::string(a.name) + " - " + where);
  }
  if (lines.size() == 1 + (args == "reset" ? 1u : 0u)) lines.push_back("  no nwn5e actions yet.");
  return lines;
}

}  // namespace

void InstallQuickbarKeys();  // below

void InstallQuickbar() {
  if (g_config.quickbar != rules::RuleVariant::kFaithful) return;
  AddChatCommand("!quickbar", &QuickbarCommand);
  InstallQuickbarKeys();
  g_set_int_param = reinterpret_cast<SetIntParamFn>(EngineExport(kSetIntParam));
  g_send_button = reinterpret_cast<SendButtonFn>(EngineExport(kSendButton));
  g_get_message = reinterpret_cast<GetMessageFn>(EngineExport(kGetNWSMessage));
  g_player_by_object = reinterpret_cast<PlayerByObjectFn>(EngineExport(kPlayerByObject));
  g_app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  if (!g_set_int_param || !g_send_button || !g_get_message || !g_player_by_object || !g_app_manager) {
    Log("SKIP quickbar: placing actions - a quickbar export is missing");
    return;
  }
  RegisterRecords({kVarPlaced});
  AddCreatureUpdateListener(&QuickbarCreatureUpdate);
  Log("quickbar: nwn5e's class actions go on empty quickbar slots when gained");
}

// --- The number keys (the client) ---------------------------------------------
// Static analysis of 89.8193.37 (0.48.0): the client turns each key into an
// input event (the input setup 0x6df630, CExoInput::AddInputEventDesc - fixed
// ids, not keymap.2da's) and CClientExoAppInternal::HandleInputEvent (0x6ed720,
// not exported: app, event, value - 1 down / 0 up -, the previous result, a
// flag; called once per event by the input loop 0x6f14e0) acts on it. F1-F12
// are events 5-16: on a key down, with a player creature and the in-game GUI
// (app +0x90) up, CPanelQuickBar::ClickButton(gui +0x60, event - 5) (0x85c3b0)
// clicks that slot of the row on screen - Shift / Ctrl held show the second /
// third row (CPanelQuickBar::SetBank 0x859a60), so Shift+F1 is slot 13. Keys
// 1-9 are events 0x19-0x21: in a conversation (gui +0x70) they pick its
// replies, otherwise 1-4 select EE's Quick Groups and 5-9 do nothing; Ctrl+1-4
// (events 0x4b-0x4e) set the groups. While a text box has the keyboard
// (chat, a NUI field) CExoInput's AddEvent drops every key (keyboard mode), so
// none of this sees typing.
// The hook hands keys 1-9 on as F1-F9 - the engine's own checks and rows then
// apply (Shift+1 = Shift+F1) - except in a conversation, with Ctrl held (the
// Quick Groups' setting keys) or while app +0x630 is set (the handler then
// passes only the mouse, Esc, the number keys and a few more; its meaning
// isn't known, so the keys keep their old use there). So plain 1-4 no longer
// select one of EE's Quick Groups - they're the quickbar's now.
namespace {

using HandleInputEventFn = int (*)(void* app, int event_id, int value, int previous, int flag);
using ModalKeysFn = int* (*)(void* exo_input);

// The handler's own code (0x6ed753, +0x33 from its entry): the arguments kept,
// then `cmp ebx, 0x13` (Shift, the first modifier event).
constexpr const char* kHandleInputEventBody = "45 8b f9 41 8b f8 48 63 da 48 8b f1 45 33 f6 45 8b e6 83 fb 13";
constexpr uintptr_t kHandleInputEventBodyOffset = 0x33;
constexpr const char* kGetModalKeys = "?GetModalKeys@CExoInput@@QEAAPEAHXZ";
constexpr const char* kExoInput = "?g_pExoInput@@3PEAVCExoInput@@EA";
constexpr int kEventF1 = 5;
constexpr int kEventKey1 = 0x19;
constexpr int kEventKey9 = 0x21;
constexpr size_t kAppGui = 0x90;              // CClientExoAppInternal: the in-game GUI
constexpr size_t kGuiConversation = 0x70;     // the conversation panel when one is open
constexpr size_t kAppRestrictedInput = 0x630;  // int32: see above
constexpr int kModalCtrl = 3;                 // GetModalKeys()[3]: Ctrl held ([2] Shift)
constexpr int kKeyLogMax = 30;

HandleInputEventFn g_handle_input = nullptr;
ModalKeysFn g_modal_keys = nullptr;
void** g_exo_input = nullptr;
bool g_keys_on = false;
int g_keys_logged = 0;

int HandleInputEvent(void* app, int event_id, int value, int previous, int flag) {
  if (event_id >= kEventKey1 && event_id <= kEventKey9 && app) {
    void* gui = Field<void*>(app, kAppGui);
    const bool conversation = gui && Field<void*>(gui, kGuiConversation);
    const bool restricted = Field<int32_t>(app, kAppRestrictedInput) != 0;
    const int* modal = g_exo_input && *g_exo_input ? g_modal_keys(*g_exo_input) : nullptr;
    const bool ctrl = modal && modal[kModalCtrl] != 0;
    const int key = event_id - kEventKey1 + 1;
    if (value && g_keys_logged < kKeyLogMax) {
      ++g_keys_logged;
      Log("quickbar: key %d%s", key,
          conversation ? " - a conversation's reply (left alone)"
          : restricted ? " - the input is restricted (left alone)"
          : ctrl       ? " - Ctrl held (Quick Groups, left alone)"
                       : " -> F-key (quickbar slot of the row on screen)");
    }
    if (!conversation && !restricted && !ctrl) {
      return g_handle_input(app, kEventF1 + key - 1, value, previous, flag);
    }
  }
  return g_handle_input(app, event_id, value, previous, flag);
}

}  // namespace

bool QuickbarNumberKeysOn() { return g_keys_on; }

void InstallQuickbarKeys() {
  const uintptr_t body = FindPattern("the client's input event handler (HandleInputEvent)", kHandleInputEventBody);
  const uintptr_t entry = body ? FunctionEntry(body) : 0;
  g_modal_keys = reinterpret_cast<ModalKeysFn>(EngineExport(kGetModalKeys));
  g_exo_input = reinterpret_cast<void**>(EngineExport(kExoInput));
  if (!entry || body - entry != kHandleInputEventBodyOffset || !g_modal_keys || !g_exo_input) {
    Log("SKIP quickbar: number keys - the input handler wasn't found where expected (F1-F12 as usual)");
    return;
  }
  g_keys_on = HookAddress("CClientExoAppInternal::HandleInputEvent (keys 1-9 press quickbar slots 1-9)", entry,
                          reinterpret_cast<void*>(&HandleInputEvent), reinterpret_cast<void**>(&g_handle_input));
  if (g_keys_on) Log("quickbar: keys 1-9 press quickbar slots 1-9 (as F1-F9; Shift+1-9 the Shift row)");
}
