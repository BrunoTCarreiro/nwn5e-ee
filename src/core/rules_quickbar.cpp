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
//   (see InstallQuickbarKeys below); the slots' key labels stay shown and read
//   1-9 (InstallQuickbarLabels).

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

// !test (#82, 0.57.9; owner, 2026-10-30: after "!test monk" "the quickbar's slots all became empty"): a
// profile that builds a character starts its placements afresh. Its reset to level 1 (and a swapped first
// class) took the old actions' feats away, the engine their buttons with them, and the record kept them
// "placed" - so off. Cleared, every action the new character has comes back on an empty slot at its next
// update, as with !quickbar reset.
void ForgetQuickbarPlacements(void* creature) {
  if (g_config.quickbar != rules::RuleVariant::kFaithful || !creature) return;
  EnsureRecords(creature);
  SetRecord(creature, kVarPlaced, 0);
  g_last_check.erase(IdOf(creature));  // looked at in the next update, not in up to 2 s
}

void InstallQuickbarKeys();    // below
void InstallQuickbarLabels();  // below

void InstallQuickbar() {
  if (g_config.quickbar != rules::RuleVariant::kFaithful) return;
  AddChatCommand("!quickbar", &QuickbarCommand);
  InstallQuickbarKeys();
  InstallQuickbarLabels();
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

// --- The key labels (the client) ----------------------------------------------
// Owner, 2026-10-27: "the quickbar needs to have the numbers always visible
// otherwise it's not that useful". Static analysis of 89.8193.37:
// - The labels: the quickbar panel's constructor (CPanelQuickBar, 0x8584a0 -
//   the only code using "F%d") gives each of its 36 buttons (CGuiQuickButton,
//   0x2c8 bytes from panel +0xf0; a CGuiTextButton) its key in the button's
//   text (+0xe0, a CAurString from NewCAurString): "F1".., "Shift-F1"..,
//   "Ctrl-F1".. After the constructor ran, nwn5e renames them (SetText,
//   CAurString vtable slot 26) to rules::QuickbarSlotLabel - no row prefixes
//   (0.48.2: a row shows only while its key is held).
// - Shown on filled slots (0.48.1): every setter that fills a button
//   (SetButtonFeat 0x867070 and some 40 more) sets the label's colour to
//   0,0,0,0; only clearing (0x85c240) gives back white at 60%. The label's
//   SetColor (slot 15, 0x4a3a0) is hooked: a label of the current panel (the
//   in-game GUI +0x60) stays white and opaque.
// - Over the icon (0.48.4; owner, twice: "the label is being hidden by the
//   icon"): the GUI draws by depth, and the button model (qb_but67) has its
//   text hookpoint at z 0.40, the icon's at 0.55, the counts' at 0.75.
//   CGuiTextButton::InitializeModel (0x1ea890, called by CGuiObject::SetModel)
//   attaches the label to the "text" hookpoint with Gob::AttachToObject mode 0
//   - a behaviour that only follows the hookpoint; SetPosition (slot 4) moves
//   only an offset behaviour (type 0x3ea), so 0.48.3's move found nothing
//   ("0 moved" in the owner's log). The counts on icons
//   (CGuiIcon::AttachCountLabel 0x1e3310) attach with mode 1 - that offset
//   behaviour (0x53d70, its offset at +0x70) - then SetPosition(x, y, 0.1).
//   So once InitializeModel ran on a quickbar button, its label is attached
//   again with mode 1 (Gob::SetBehavior frees the old behaviour; SetScene is a
//   no-op for the same scene) and SetPosition(0, 0, 0.25): z 0.65, over the
//   icon, under the counts.
// - 0.48.2's 1.6x width did nothing (a label on one hookpoint has width 0 -
//   "no limit" in CAurFont::CalculateVisibleStringLengthAndWidth) and is gone.
//   Slot 10's text is logged when a panel is made, should "F10" still read
//   "F1".
namespace {

using PanelCtorFn = void* (*)(void* self, void* a2, void* a3, void* a4);
using InitializeModelFn = void (*)(void* button);
using SetColorFn = void (*)(void* label, float r, float g, float b, float a);
using SetTextFn = void (*)(void* label, const char* text);
using AttachFn = void* (*)(void* label, void* object, const char* hookpoint, int mode);
struct LabelOffset {
  float x, y, z;
};
using SetPositionFn = int (*)(void* label, const LabelOffset* offset);

constexpr const char* kNewCAurString = "?NewCAurString@@YAPEAVCAurString@@H@Z";
constexpr const char* kTextButtonInitializeModel = "?InitializeModel@CGuiTextButton@@UEAAXXZ";
constexpr const char* kGobAttachToObject = "?AttachToObject@Gob@@UEAAPEAXPEAVCAurObject@@PEBDH@Z";
constexpr const char* kRttiQuickButton = ".?AVCGuiQuickButton@@";
// In NewCAurString: the CAurString part of the new StringGob gets its vtable
// (lea rax, [rip + vtable]; mov [rbx + 0x4c0], rax).
constexpr const char* kStringVtableStore = "48 8d 05 ?? ?? ?? ?? 48 89 83 c0 04 00 00";
// SetColor's own code: mov rax, [rcx + 0x58]; movss xmm0, [rsp + 0x28].
constexpr uint8_t kSetColorBody[] = {0x48, 0x8b, 0x41, 0x58, 0xf3, 0x0f, 0x10, 0x44, 0x24, 0x28};
// SetPosition's code (0x4a900): the Gob (label - 0x4c0), its behaviour
// (GetBehavior(-1), Gob vtable +0x228), an offset one (type 0x3ea), the offset
// at +0x70 - checked so slot 4 is what it was read as.
constexpr uint8_t kSetPositionBody[] = {
    0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83, 0xec, 0x20, 0x48, 0x81, 0xc1, 0x40, 0xfb, 0xff, 0xff,
    0x48, 0x8b, 0xfa, 0xba, 0xff, 0xff, 0xff, 0xff, 0x48, 0x8b, 0x01, 0xff, 0x90, 0x28, 0x02, 0x00, 0x00,
    0x48, 0x8b, 0xd8, 0x48, 0x85, 0xc0, 0x74, 0x30, 0x4c, 0x8b, 0x00, 0x48, 0x8b, 0xc8, 0x41, 0xff, 0x50,
    0x28, 0x3d, 0xea, 0x03, 0x00, 0x00, 0x75, 0x1f, 0x8b, 0x47, 0x08, 0xf2, 0x0f, 0x10, 0x07, 0xf2, 0x0f,
    0x11, 0x43, 0x70, 0x89, 0x43, 0x78};
// Slot 1's thunk: sub rcx, 0x4c0; jmp -> jmp Gob::AttachToObject.
constexpr uint8_t kAttachThunk[] = {0x48, 0x81, 0xe9, 0xc0, 0x04, 0x00, 0x00, 0xe9};
constexpr int kAttachSlot = 1;
constexpr int kSetPositionSlot = 4;
constexpr int kSetColorSlot = 15;
constexpr int kSetTextSlot = 26;
constexpr size_t kClientAppInternal = 0x8;  // CClientExoApp -> CClientExoAppInternal
constexpr size_t kGuiQuickbar = 0x60;       // the in-game GUI: CPanelQuickBar
constexpr size_t kPanelButtons = 0xf0;      // CPanelQuickBar: CGuiQuickButton[36]
constexpr size_t kQuickButtonSize = 0x2c8;
constexpr size_t kQuickButtonLabel = 0xe0;  // CGuiTextButton: its text, the key label
constexpr size_t kGuiObjectModel = 0x10;    // CGuiObject: its model (SetModel)
constexpr size_t kLabelLines = 0x40;        // CAurString: its lines (pointers), +0x48 the count
constexpr int kAttachWithOffset = 1;        // AttachToObject mode 1: the offset behaviour
constexpr float kLabelForward = 0.25f;      // text 0.40 -> 0.65: over the icon (0.55), under the counts (0.75)
constexpr float kLabelAlpha = 1.0f;

PanelCtorFn g_panel_ctor = nullptr;
InitializeModelFn g_text_button_init = nullptr;
SetColorFn g_set_color = nullptr;
SetTextFn g_set_text = nullptr;
AttachFn g_attach = nullptr;
SetPositionFn g_set_position = nullptr;
uintptr_t g_quick_button_vtable = 0;
void** g_client_app_manager = nullptr;
void* g_panel = nullptr;  // the last quickbar panel made
void* g_labels[rules::kQuickbarSlots] = {};
int g_over_icon = 0;         // labels attached over the icon
int g_over_icon_failed = 0;  // ... and not
int g_label_logs = 0;

void* CurrentQuickbarPanel() {
  if (!g_client_app_manager || !*g_client_app_manager) return nullptr;
  void* client = *static_cast<void**>(*g_client_app_manager);  // CAppManager +0: CClientExoApp
  void* app = client ? Field<void*>(client, kClientAppInternal) : nullptr;
  void* gui = app ? Field<void*>(app, kAppGui) : nullptr;
  return gui ? Field<void*>(gui, kGuiQuickbar) : nullptr;
}

void SetColor(void* label, float r, float g, float b, float a) {
  if (a < 0.01f && label && g_panel) {
    for (void* known : g_labels) {
      if (known != label) continue;
      if (CurrentQuickbarPanel() == g_panel) {  // a filled slot: its key stays shown
        r = g = b = 1.0f;
        a = kLabelAlpha;
      }
      break;
    }
  }
  g_set_color(label, r, g, b, a);
}

// A quickbar button's label attached again to its "text" hookpoint, with an
// offset toward the viewer (over the icon).
void LabelOverIcon(void* button) {
  void* label = Field<void*>(button, kQuickButtonLabel);
  void* model = Field<void*>(button, kGuiObjectModel);
  void* behaviour = label && model ? g_attach(label, model, "text", kAttachWithOffset) : nullptr;
  const LabelOffset forward{0.0f, 0.0f, kLabelForward};
  if (behaviour && g_set_position(label, &forward)) {
    ++g_over_icon;
  } else {
    ++g_over_icon_failed;
  }
}

void TextButtonInitializeModel(void* button) {
  g_text_button_init(button);
  if (button && *static_cast<uintptr_t*>(button) == g_quick_button_vtable) LabelOverIcon(button);
}

// Slot 10's label as laid out (should "F10" read "F1"): its lines, the first
// line's text and the two numbers CalculateVisibleStringLengthAndWidth sets.
std::string DescribeLabel(void* label) {
  if (!label) return "no label";
  const int lines = Field<int32_t>(label, kLabelLines + 8);
  void** items = Field<void**>(label, kLabelLines);
  if (lines <= 0 || lines > 64 || !items || !items[0]) return "lines " + std::to_string(lines);
  const char* text = Field<const char*>(items[0], 8);
  char buffer[160];
  snprintf(buffer, sizeof(buffer), "lines %d, \"%.12s\", +0x1c %d, +0x20 %.3f, +0x24 %.3f", lines,
           text ? text : "", Field<int32_t>(items[0], 0x1c), Field<float>(items[0], 0x20),
           Field<float>(items[0], 0x24));
  return buffer;
}

void* QuickbarPanelCtor(void* self, void* a2, void* a3, void* a4) {
  const int over_before = g_over_icon, failed_before = g_over_icon_failed;
  void* result = g_panel_ctor(self, a2, a3, a4);  // its SetModel calls run InitializeModel on each button
  if (!self) return result;
  g_panel = self;
  uint8_t* buttons = static_cast<uint8_t*>(self) + kPanelButtons;
  for (int i = 0; i < rules::kQuickbarSlots; ++i) {
    g_labels[i] = Field<void*>(buttons + i * kQuickButtonSize, kQuickButtonLabel);
    if (g_labels[i]) g_set_text(g_labels[i], rules::QuickbarSlotLabel(i, QuickbarNumberKeysOn()).c_str());
  }
  if (g_label_logs < 5) {
    ++g_label_logs;
    Log("quickbar: a quickbar panel made - key labels renamed (1-9 / F10-F12; F1-F12 on the Ctrl row), %d attached "
        "over the icon (%d not), shown on filled slots; slot 10: %s",
        g_over_icon - over_before, g_over_icon_failed - failed_before, DescribeLabel(g_labels[9]).c_str());
  }
  return result;
}

// The target of a jmp rel32 at `at` (0 if it isn't one).
uintptr_t JumpTarget(uintptr_t at) {
  if (!at || *reinterpret_cast<const uint8_t*>(at) != 0xe9) return 0;
  return at + 5 + *reinterpret_cast<const int32_t*>(at + 1);
}

}  // namespace

void InstallQuickbarLabels() {
  const uintptr_t ctor = FunctionByString("the quickbar panel's constructor (its \"F%d\" key labels)", "F%d");
  const uintptr_t new_string = EngineExport(kNewCAurString);
  const uintptr_t store =
      new_string ? FindPatternIn("NewCAurString's vtable store", FunctionAt(new_string), kStringVtableStore) : 0;
  const uintptr_t* vtable = store ? reinterpret_cast<const uintptr_t*>(store + 7 + *reinterpret_cast<const int32_t*>(store + 3))
                                  : nullptr;
  const uintptr_t set_color = vtable ? vtable[kSetColorSlot] : 0;
  const uintptr_t attach = vtable ? vtable[kAttachSlot] : 0;
  g_set_text = reinterpret_cast<SetTextFn>(vtable ? vtable[kSetTextSlot] : 0);
  g_set_position = reinterpret_cast<SetPositionFn>(vtable ? vtable[kSetPositionSlot] : 0);
  g_attach = reinterpret_cast<AttachFn>(attach);
  g_quick_button_vtable = FindVtable("the quickbar button's vtable", kRttiQuickButton);
  g_client_app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  const uintptr_t init_model = EngineExport(kTextButtonInitializeModel);
  const bool color_ok =
      set_color && memcmp(reinterpret_cast<const void*>(set_color), kSetColorBody, sizeof(kSetColorBody)) == 0;
  const bool position_ok =
      g_set_position &&
      memcmp(reinterpret_cast<const void*>(g_set_position), kSetPositionBody, sizeof(kSetPositionBody)) == 0;
  const bool attach_ok = attach && memcmp(reinterpret_cast<const void*>(attach), kAttachThunk, sizeof(kAttachThunk)) == 0 &&
                         JumpTarget(JumpTarget(attach + sizeof(kAttachThunk) - 1)) == EngineExport(kGobAttachToObject);
  if (!ctor || !color_ok || !position_ok || !attach_ok || !g_set_text || !g_quick_button_vtable ||
      !g_client_app_manager || !init_model) {
    Log("SKIP quickbar: key labels - a piece wasn't found where expected (labels show on empty slots only)");
    return;
  }
  const bool hooked =
      HookAddress("CAurString::SetColor (a quickbar key label stays shown)", set_color,
                  reinterpret_cast<void*>(&SetColor), reinterpret_cast<void**>(&g_set_color)) &&
      HookAddress("CGuiTextButton::InitializeModel (a quickbar key label over the icon)", init_model,
                  reinterpret_cast<void*>(&TextButtonInitializeModel), reinterpret_cast<void**>(&g_text_button_init)) &&
      HookAddress("CPanelQuickBar::CPanelQuickBar (key labels named after the number keys)", ctor,
                  reinterpret_cast<void*>(&QuickbarPanelCtor), reinterpret_cast<void**>(&g_panel_ctor));
  if (hooked) Log("quickbar: key labels always shown, over the icon (1-9 with the number keys)");
}
