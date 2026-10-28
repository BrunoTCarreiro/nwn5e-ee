// The action queue strip (owner, 2026-10-28: "a sort of action queue at the
// middle top of the screen going left to right from the next action to the
// most recently added to the queue. Each item would consist of an action and
// target"; the design he took: docs/PLAN.md "Action queue strip", 0.54.0).
//
// The engine already keeps a player's queue for the portrait's small action
// panel (CNWSCreature::UpdateActionQueue 0x3a6270 -> CNWSMessage::
// ComputeLastUpdate_ActionQueue): one entry per action group, in order
// (CNWSObject::GetPositionByGroupIndex, then GetActionByGroupId - the group's
// defining action), the combat round's queued special attacks just before the
// attack they ride on (CNWSCombatRound::GetSpecialAttack / GetSpecialAttackID;
// feat 0, 6 and the internal 0xfdea / 0xfdeb left out, as the engine does),
// each with its display id - NWScript's ACTION_* value
// (CNWSObject::GetAQActionIDByID; with flag 1 a special attack's feat). nwn5e
// reads the same, with each action's parameters where UpdateActionQueue reads
// them (8 bytes each from +0x38): move - a point (0-2) or an object (3);
// attack - the target (0); cast spell - the spell (0), the target (5) or a
// point (6-8); an item's spell - the item (0), its active property (1: its
// subtype +0x2 the iprp_spells row), the target (3) or a point (4-6); set trap
// - the object (1); the others the object they act on (0).
//
// The strip is a NUI window ("n5queue"): no title bar or border, centred at
// the top of the player's screen (its GUI width), up to 8 items left to right
// from the action in progress (its labels gold) to the newest, "+N" for more;
// each an icon (the spell's, the feat's or the radial menu's), a name and the
// target ("a spot" for a point, "yourself" for oneself). It opens when
// something is queued and closes when nothing is; one script per change sets
// its names (spells.2da, feat.2da, the target's name) and binds. A click on an
// item cancels it: CNWSCreature::CancelAction(display id, group or
// special-attack id) - what the portrait panel asks for - and for nwn5e's own
// special attacks (Shove: push, Grapple - no display id) the combat round's
// RemoveSpecialAttack. Player characters only; `!queue` turns it off and on
// (the record nwn5e_queue_off). Both modes - it shows, it doesn't rule.
//
// 0.54.2 (owner: "clicking an item in the queue doesn't cancel it"): nui.cpp's
// event hook knows a window by the token kept on the character
// (nwn5e_nui_<id>), which NuiOpen sets - the strip's own script now sets it
// too. And (owner: "I can queue the same item (a potion for example) multiple
// times while not having enough"): a player's use of an item's power is refused
// when its queued uses (ItemCastSpell actions of that item and power) already
// take every use it has left (rules::ItemPowerUsesLeft: the stack, the charges,
// the uses left today - read by script from its Cast Spell property).

#include <windows.h>

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "chat.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "item_use.h"
#include "log.h"
#include "nui.h"
#include "records.h"
#include "rules/queue_strip.h"

namespace {

constexpr const char* kWindow = "n5queue";
constexpr const char* kVarOff = "nwn5e_queue_off";
constexpr ULONGLONG kCheckEveryMs = 150;
constexpr int kMaxGroups = 32;
constexpr int kMaxSpecials = 9;  // the engine's own cap


using PositionByGroupIndexFn = void* (*)(void* object, int index);
using ActionByGroupIdFn = uint16_t (*)(void* object, uint16_t group, void** node);
using AqIdFn = uint16_t (*)(void* object, uint16_t id, int feat);
using CancelActionFn = int (*)(void* creature, uint16_t action, uint16_t id);
using RoundCountFn = int (*)(void* round);
using RoundSpecialFn = uint16_t (*)(void* round, int index);
using RemoveSpecialFn = void (*)(void* round, int index);
using ActivePropertyFn = void* (*)(void* item, int index);

PositionByGroupIndexFn g_position_by_group_index = nullptr;
ActionByGroupIdFn g_action_by_group_id = nullptr;
AqIdFn g_aq_id = nullptr;
CancelActionFn g_cancel_action = nullptr;
RoundCountFn g_special_count = nullptr;
RoundSpecialFn g_special_feat = nullptr;
RoundSpecialFn g_special_id = nullptr;
RemoveSpecialFn g_remove_special = nullptr;
ActivePropertyFn g_active_property = nullptr;

enum class Kind { kPlain, kSpell, kItemSpell, kSpecial };

struct Item {
  Kind kind = Kind::kPlain;
  int action = rules::kQueueNone;  // ACTION_* (the plain name and icon)
  int spell = -1;                  // kSpell
  int feat = -1;                   // kSpecial
  uint32_t item = ee::kObjectInvalid;  // kItemSpell
  int iprp_row = -1;                   // kItemSpell: the power's iprp_spells row
  uint32_t target = ee::kObjectInvalid;
  bool spot = false;  // a point, no object
  // How to cancel it.
  uint16_t cancel_action = 0xFFFF;
  uint16_t cancel_id = 0xFFFF;
  bool own_special = false;  // nwn5e's special attack: RemoveSpecialAttack by its id
};

struct Player {
  ULONGLONG next = 0;
  std::string signature;
  std::vector<Item> items;  // what the strip shows, for its clicks
};
std::unordered_map<uint32_t, ULONGLONG> g_next_check;  // by creature id
std::unordered_map<uint32_t, Player> g_players;      // by player character id

uint32_t IdOf(const void* creature) { return creature ? Field<uint32_t>(creature, ee::kObjectId) : ee::kObjectInvalid; }

uint32_t Param(const void* node, int index) { return Field<uint32_t>(node, ee::kActionFirstParam + 8 * index); }

bool TargetInFirstParam(int type) {
  switch (type) {
    case 7: case 20: case 21: case 24: case 25: case 26: case 27: case 28: case 38: case 39: case 40: case 41:
    case 43: case 50: case 54: case 55: case 56: case 57: case 59:
      return true;
    default:
      return false;
  }
}

bool InternalSpecial(uint16_t feat) { return feat == 0 || feat == 6 || static_cast<uint16_t>(feat + 0x216) <= 1; }

void AddSpecials(void* creature, uint32_t target, std::vector<Item>* items) {
  void* round = Field<void*>(creature, ee::kCreatureCombatRound);
  if (!round) return;
  const int count = g_special_count(round);
  for (int i = 0; i < count && i < kMaxSpecials; ++i) {
    const uint16_t feat = g_special_feat(round, i);
    if (InternalSpecial(feat)) continue;
    Item it;
    it.kind = Kind::kSpecial;
    it.feat = feat;
    it.target = target;
    it.cancel_id = g_special_id(round, i);
    it.cancel_action = g_aq_id(creature, feat, 1);
    it.own_special = it.cancel_action == 0xFFFF;
    items->push_back(it);
  }
}

std::vector<Item> ReadQueue(void* creature) {
  std::vector<Item> items;
  for (int index = 0; index < kMaxGroups; ++index) {
    void* position = g_position_by_group_index(creature, index);
    if (!position) break;
    const void* data = Field<void*>(position, ee::kListNodeData);
    if (!data) continue;
    const uint16_t group = Field<uint16_t>(data, ee::kActionGroupId);
    void* node = nullptr;
    const uint16_t type = g_action_by_group_id(creature, group, &node);
    if (type == 0xFFFF || !node) continue;
    Item it;
    it.action = g_aq_id(creature, type, 0);
    it.cancel_action = static_cast<uint16_t>(it.action);
    it.cancel_id = group;
    switch (type) {
      case ee::kActionMoveToPoint:
        it.target = Param(node, 3);
        it.spot = true;
        break;
      case ee::kActionAttackObject:
        it.target = Param(node, 0);
        AddSpecials(creature, it.target, &items);
        break;
      case ee::kActionCastSpell:
        it.kind = Kind::kSpell;
        it.spell = static_cast<int>(Param(node, 0));
        it.target = Param(node, 5);
        it.spot = true;
        break;
      case ee::kActionItemCastSpell: {
        it.kind = Kind::kItemSpell;
        it.item = Param(node, 0);
        it.target = Param(node, 3);
        it.spot = true;
        void* item = ItemById(it.item);
        const void* property = item ? g_active_property(item, static_cast<int>(Param(node, 1))) : nullptr;
        if (property) it.iprp_row = Field<uint16_t>(property, ee::kItemPropertySubtype);
        break;
      }
      case ee::kActionDropItem:
        it.target = Param(node, 0);  // the item dropped
        break;
      case ee::kActionSetTrap:
        it.target = Param(node, 1);
        it.spot = true;
        break;
      default:
        // The engine's own list of actions aimed at an object (UpdateActionQueue +0x3a68b6).
        if (TargetInFirstParam(type)) it.target = Param(node, 0);
        break;
    }
    // Bookkeeping actions with no ACTION_* value (no name) are left off, as the portrait panel leaves them blank.
    if (it.kind == Kind::kPlain && it.action == rules::kQueueNone) continue;
    items.push_back(it);
  }
  return items;
}

std::string Signature(const std::vector<Item>& items) {
  std::string s;
  char part[96];
  for (const Item& it : items) {
    sprintf_s(part, "%d,%d,%d,%d,%x,%d,%x,%d,%d,%d;", static_cast<int>(it.kind), it.action, it.spell, it.feat,
              it.item, it.iprp_row, it.target, it.spot ? 1 : 0, it.cancel_action, it.cancel_id);
    s += part;
  }
  return s;
}

// A script string literal.
std::string Quoted(const std::string& text) {
  std::string out = "\"";
  for (char c : text) {
    if (c == '"' || c == '\\') out += '\\';
    if (c == '\n') {
      out += "\\n";
      continue;
    }
    out += c;
  }
  return out + "\"";
}

// One slot's JSON, "@" where its number goes (the script builds the eight).
std::string SlotTemplate() {
  const std::string icon =
      "{\"type\":\"button_image\",\"label\":{\"bind\":\"aq_i@\"},\"value\":null,\"id\":\"aq_@\",\"width\":40.0,"
      "\"height\":40.0,\"tooltip\":{\"bind\":\"aq_h@\"}}";
  auto label = [](const char* text, const char* colour) {
    return std::string("{\"type\":\"label\",\"label\":null,\"value\":{\"bind\":\"") + text +
           "\"},\"text_halign\":0,\"text_valign\":0,\"height\":18.0,\"foreground_color\":{\"bind\":\"" + colour +
           "\"}}";
  };
  const std::string icon_row = nui::Row(nui::List({nui::Spacer(), icon, nui::Spacer()}));
  const std::string column = nui::Col(nui::List({icon_row, label("aq_n@", "aq_c@"), label("aq_t@", "aq_d@")}));
  char size[96];
  sprintf_s(size, ",\"width\":%.1f,\"visible\":{\"bind\":\"aq_v@\"}}", rules::kQueueSlotWidth);
  return column.substr(0, column.size() - 1) + size;
}

// The script expression for the template with k's number (the string sk).
std::string SlotExpression() {
  const std::string text = SlotTemplate();
  std::string expression;
  size_t from = 0;
  for (size_t at = text.find('@'); at != std::string::npos; at = text.find('@', from)) {
    expression += Quoted(text.substr(from, at - from)) + " + sk + ";
    from = at + 1;
  }
  return expression + Quoted(text.substr(from));
}

// Creates the window when it isn't open (one script: the slots in a loop).
std::string CreateScript() {
  char more[256];
  sprintf_s(more,
            "{\"type\":\"label\",\"label\":null,\"value\":{\"bind\":\"aq_more\"},\"text_halign\":0,\"text_valign\":0,"
            "\"width\":%.1f,\"visible\":{\"bind\":\"aq_vm\"}}",
            rules::kQueueMoreWidth);
  const std::string window =
      "{\"version\":1,\"title\":false,\"root\":null,\"geometry\":{\"bind\":\"geometry\"},\"resizable\":false,"
      "\"collapsed\":false,\"closable\":false,\"transparent\":false,\"border\":false,\"accepts_input\":true,"
      "\"size_constraint\":null,\"edge_constraint\":null,\"font\":\"\"}";
  char loop[96];
  sprintf_s(loop, "for (k = 0; k < %d; k++) { sk = IntToString(k); ", rules::kQueueStripSlots);
  return std::string("if (!t) { json kids = JsonArray(); int k; string sk; ") + loop +
         "kids = JsonArrayInsert(kids, JsonParse(" + SlotExpression() + ")); } kids = JsonArrayInsert(kids, JsonParse(" +
         Quoted(more) + ")); json row = JsonParse(\"{\\\"type\\\":\\\"row\\\",\\\"label\\\":null,\\\"value\\\":null}\");"
         " row = JsonObjectSet(row, \"children\", kids);"
         // 0.54.1 (owner: two items queued, "the scrollbars ... rapidly appear and disappear alternatingly"):
         // the hidden slots still take room, so the row overflowed a window sized to the shown ones and each
         // scrollbar took room for the other. The row sits in a group without scrollbars - clipped, never scrolled.
         " json root = JsonParse(\"{\\\"type\\\":\\\"group\\\",\\\"label\\\":null,\\\"value\\\":null,"
         "\\\"border\\\":false,\\\"scrollbars\\\":0}\"); root = JsonObjectSet(root, \"children\", "
         "JsonArrayInsert(JsonArray(), row)); json w = JsonParse(" + Quoted(window) +
         "); w = JsonObjectSet(w, \"root\", root); t = NuiCreate(p, w, \"n5queue\"); if (!t) return; }"
         // nui.cpp's event hook finds the window by this token (as NuiOpen keeps it).
         " SetLocalInt(p, \"nwn5e_nui_n5queue\", t);";
}

// The script that sets slot k's binds.
std::string SlotScript(int k, const Item& it, uint32_t player) {
  const std::string number = std::to_string(k);
  auto set = [&](const char* which, const std::string& value) {
    return "NuiSetBind(p, t, \"aq_" + std::string(which) + number + "\", " + value + ");";
  };
  std::string code;
  char line[256];
  switch (it.kind) {
    case Kind::kSpell:
      sprintf_s(line, "s = %d;", it.spell);
      code += line;
      code += set("n", "JsonString(GetStringByStrRef(StringToInt(Get2DAString(\"spells\", \"Name\", s))))");
      code += set("i", "JsonString(Get2DAString(\"spells\", \"IconResRef\", s))");
      break;
    case Kind::kItemSpell:
      sprintf_s(line, "s = StringToInt(Get2DAString(\"iprp_spells\", \"SpellIndex\", %d)); o = StringToObject(\"%x\");",
                it.iprp_row, it.item);
      code += line;
      code += set("n", "JsonString(GetIsObjectValid(o) ? GetName(o) : \"Use item\")");
      code += set("i", it.iprp_row >= 0 ? "JsonString(Get2DAString(\"spells\", \"IconResRef\", s))"
                                        : "JsonString(\"ir_use\")");
      break;
    case Kind::kSpecial:
      sprintf_s(line, "s = %d;", it.feat);
      code += line;
      code += set("n", "JsonString(GetStringByStrRef(StringToInt(Get2DAString(\"feat\", \"FEAT\", s))))");
      code += set("i", "JsonString(Get2DAString(\"feat\", \"ICON\", s))");
      break;
    case Kind::kPlain:
      code += set("n", "JsonString(" + Quoted(rules::QueueActionName(it.action)) + ")");
      code += set("i", "JsonString(" + Quoted(rules::QueueActionIcon(it.action)) + ")");
      break;
  }
  if (it.target == player) {
    code += set("t", "JsonString(\"yourself\")");
  } else {
    sprintf_s(line, "o = StringToObject(\"%x\");", it.target);
    code += line;
    code += set("t", std::string("JsonString(GetIsObjectValid(o) ? GetName(o) : \"") + (it.spot ? "a spot" : "") +
                         "\")");
  }
  // The action in progress in gold, its target pale gold; the others white and grey.
  code += set("c", k == 0 ? "gold" : "white");
  code += set("d", k == 0 ? "pale" : "grey");
  code += set("h", "JsonString(\"Click to cancel\")");
  return code;
}

void Show(uint32_t player, const std::vector<Item>& items) {
  if (items.empty()) {
    RunScriptChunk("int t = NuiFindWindow(OBJECT_SELF, \"n5queue\"); if (t) NuiDestroy(OBJECT_SELF, t);", player);
    return;
  }
  const rules::QueueStripCount count = rules::QueueStripSlots(static_cast<int>(items.size()));
  std::string code = "object p = OBJECT_SELF; int t = NuiFindWindow(p, \"n5queue\"); " + CreateScript() +
                     " object o; int s;"
                     " json gold = JsonParse(\"{\\\"r\\\":255,\\\"g\\\":215,\\\"b\\\":0,\\\"a\\\":255}\");"
                     " json pale = JsonParse(\"{\\\"r\\\":230,\\\"g\\\":200,\\\"b\\\":120,\\\"a\\\":255}\");"
                     " json white = JsonParse(\"{\\\"r\\\":240,\\\"g\\\":240,\\\"b\\\":240,\\\"a\\\":255}\");"
                     " json grey = JsonParse(\"{\\\"r\\\":185,\\\"g\\\":185,\\\"b\\\":185,\\\"a\\\":255}\");";
  for (int k = 0; k < rules::kQueueStripSlots; ++k) {
    char visible[96];
    sprintf_s(visible, "NuiSetBind(p, t, \"aq_v%d\", JsonBool(%s));", k, k < count.shown ? "TRUE" : "FALSE");
    code += visible;
    if (k < count.shown) code += SlotScript(k, items[k], player);
  }
  char tail[640];
  const float width = rules::QueueStripWidth(count.shown, count.more);
  sprintf_s(tail,
            "NuiSetBind(p, t, \"aq_vm\", JsonBool(%s)); NuiSetBind(p, t, \"aq_more\", JsonString(\"+%d\"));"
            "float x = (IntToFloat(GetPlayerDeviceProperty(p, \"gui_width\")) - %.1f) / 2.0; if (x < 0.0) x = 0.0;"
            "json g = JsonObject(); g = JsonObjectSet(g, \"x\", JsonFloat(x)); g = JsonObjectSet(g, \"y\", "
            "JsonFloat(4.0)); g = JsonObjectSet(g, \"w\", JsonFloat(%.1f)); g = JsonObjectSet(g, \"h\", "
            "JsonFloat(%.1f)); NuiSetBind(p, t, \"geometry\", g);",
            count.more > 0 ? "TRUE" : "FALSE", count.more, width, width, rules::kQueueStripHeight);
  code += tail;
  const int result = RunScriptChunk(code.c_str(), player);
  static int logged = 0;
  if ((result != 0 || logged < 3) && logged < 12) {
    ++logged;
    Log("queue: 0x%08X the strip %s: %d item(s) (script %zu chars, result %d)", player,
        result == 0 ? "shown" : "script FAILED", static_cast<int>(items.size()), code.size(), result);
  }
}

bool Off(void* creature) { return engine::GetLocalInt(creature, kVarOff) != 0; }

void QueueCreatureUpdate(void* creature) {
  const uint32_t id = IdOf(creature);
  if (id == ee::kObjectInvalid) return;
  const ULONGLONG now = GetTickCount64();
  ULONGLONG& next = g_next_check[id];
  if (now < next) return;
  next = now + kCheckEveryMs;
  void* stats = StatsOf(creature);
  if (!stats || !engine::AreaOf(creature) || !ReadCreatureFacts(stats).is_player) return;
  Player& state = g_players[id];
  std::vector<Item> items = Off(creature) ? std::vector<Item>() : ReadQueue(creature);
  const std::string signature = Signature(items);
  if (signature == state.signature) return;
  state.signature = signature;
  state.items = items;
  Show(id, items);
}

bool Cancel(void* creature, const Item& it) {
  if (it.own_special) {
    void* round = Field<void*>(creature, ee::kCreatureCombatRound);
    if (!round) return false;
    const int count = g_special_count(round);
    for (int i = 0; i < count; ++i) {
      if (g_special_id(round, i) == it.cancel_id) {
        g_remove_special(round, i);
        return true;
      }
    }
    return false;
  }
  return g_cancel_action(creature, it.cancel_action, it.cancel_id) != 0;
}

void OnQueueEvent(const NuiEvent& event) {
  if (event.type != "click" || event.element.rfind("aq_", 0) != 0 || event.element.size() != 4) return;
  const int k = event.element[3] - '0';
  auto state = g_players.find(event.player);
  void* creature = CreatureById(event.player);
  if (!creature || state == g_players.end() || k < 0 || k >= static_cast<int>(state->second.items.size())) return;
  const Item it = state->second.items[k];
  const bool done = Cancel(creature, it);
  Log("queue: 0x%08X cancel item %d (action %d, id %d%s): %s", event.player, k, it.cancel_action, it.cancel_id,
      it.own_special ? ", nwn5e's special attack" : "", done ? "cancelled" : "not found");
  state->second.signature.clear();  // shown again at the next check
  g_next_check[event.player] = 0;
}

// 0.54.2: a power's uses already queued (its ItemCastSpell actions), every action in the list.
int QueuedUses(void* creature, uint32_t item_id, int active_property) {
  const void* list = Field<void*>(creature, ee::kObjectActionQueue);
  const void* node = list ? Field<void*>(list, ee::kListHead) : nullptr;
  int queued = 0;
  for (int i = 0; node && i < 256; ++i, node = Field<void*>(node, ee::kListNodeNext)) {
    const void* action = Field<void*>(node, ee::kListNodeData);
    if (action && Field<int32_t>(action, ee::kActionId) == ee::kActionItemCastSpell && Param(action, 0) == item_id &&
        static_cast<int>(Param(action, 1)) == active_property) {
      ++queued;
    }
  }
  return queued;
}

bool NoOverQueuedUses(void* creature, uint32_t item_id, void* item) {
  const int active_property = ItemUseActiveProperty();
  if (!item || active_property < 0 || !g_active_property) return true;
  void* stats = StatsOf(creature);
  if (!stats || !ReadCreatureFacts(stats).is_player) return true;
  const int queued = QueuedUses(creature, item_id, active_property);
  if (queued <= 0) return true;
  const void* property = g_active_property(item, active_property);
  if (!property) return true;
  const int row = Field<uint16_t>(property, ee::kItemPropertySubtype);
  char script[700];
  sprintf_s(script,
            "object o = StringToObject(\"%x\"); itemproperty ip = GetFirstItemProperty(o); int n = -1; int d = 0;"
            " while (GetIsItemPropertyValid(ip)) { if (GetItemPropertyType(ip) == ITEM_PROPERTY_CAST_SPELL &&"
            " GetItemPropertySubType(ip) == %d) { n = GetItemPropertyCostTableValue(ip);"
            " d = GetItemPropertyUsesPerDayRemaining(o, ip); break; } ip = GetNextItemProperty(o); }"
            " SetLocalInt(OBJECT_SELF, \"nwn5e_aq_uses\", n); SetLocalInt(OBJECT_SELF, \"nwn5e_aq_stack\","
            " GetItemStackSize(o)); SetLocalInt(OBJECT_SELF, \"nwn5e_aq_charges\", GetItemCharges(o));"
            " SetLocalInt(OBJECT_SELF, \"nwn5e_aq_day\", d); SetLocalString(OBJECT_SELF, \"nwn5e_aq_name\","
            " GetName(o));",
            item_id, row);
  const uint32_t id = IdOf(creature);
  RunScriptChunk(script, id);
  const int left = rules::ItemPowerUsesLeft(engine::GetLocalInt(creature, "nwn5e_aq_uses"),
                                            engine::GetLocalInt(creature, "nwn5e_aq_stack"),
                                            engine::GetLocalInt(creature, "nwn5e_aq_charges"),
                                            engine::GetLocalInt(creature, "nwn5e_aq_day"));
  if (left < 0 || queued < left) return true;
  const std::string name = engine::GetLocalString(creature, "nwn5e_aq_name");
  TellPlayer(id, "nwn5e: " + (name.empty() ? std::string("that item") : name) + " - " +
                     (left == 1 ? std::string("its one use is") : "all " + std::to_string(left) + " uses are") +
                     " already queued.");
  Log("queue: 0x%08X refused another use of item 0x%08X (power %d): %d queued, %d left", id, item_id,
      active_property, queued, left);
  return false;
}

std::vector<std::string> QueueCommand(void* creature, const std::string& /*args*/) {
  const uint32_t id = IdOf(creature);
  EnsureRecords(creature);
  const bool off = !Off(creature);
  SetRecord(creature, kVarOff, off ? 1 : 0);
  g_players[id].signature = "-";  // redraw (or close) at the next check
  g_next_check[id] = 0;
  Log("queue: 0x%08X the strip turned %s", id, off ? "off" : "on");
  return {off ? "nwn5e: the action queue strip is off (!queue turns it back on)."
              : "nwn5e: the action queue strip is on - your queued actions at the top of the screen; click one to "
                "cancel it (!queue turns it off)."};
}

}  // namespace

void InstallActionQueue() {
  g_position_by_group_index =
      reinterpret_cast<PositionByGroupIndexFn>(EngineExport(ee::kGetPositionByGroupIndex));
  g_action_by_group_id = reinterpret_cast<ActionByGroupIdFn>(EngineExport(ee::kGetActionByGroupId));
  g_aq_id = reinterpret_cast<AqIdFn>(EngineExport(ee::kGetAQActionIDByID));
  g_cancel_action = reinterpret_cast<CancelActionFn>(EngineExport(ee::kCreatureCancelAction));
  g_special_count = reinterpret_cast<RoundCountFn>(EngineExport(ee::kGetNumSpecialAttacks));
  g_special_feat = reinterpret_cast<RoundSpecialFn>(EngineExport(ee::kGetSpecialAttack));
  g_special_id = reinterpret_cast<RoundSpecialFn>(EngineExport(ee::kGetSpecialAttackID));
  g_remove_special = reinterpret_cast<RemoveSpecialFn>(EngineExport(ee::kRemoveSpecialAttack));
  g_active_property = reinterpret_cast<ActivePropertyFn>(EngineExport(ee::kItemGetActiveProperty));
  if (!g_position_by_group_index || !g_action_by_group_id || !g_aq_id || !g_cancel_action || !g_special_count ||
      !g_special_feat || !g_special_id || !g_remove_special || !g_active_property) {
    Log("SKIP queue: the action queue strip - an engine export is missing (a different game build)");
    return;
  }
  RegisterRecords({kVarOff});
  AddChatCommand("!queue", &QueueCommand);
  if (!AddItemUseGuard(&NoOverQueuedUses)) Log("queue: an item's uses can be queued past what it has (no UseItem hook)");
  const bool events = AddNuiWindow(kWindow, &OnQueueEvent);
  if (!AddCreatureUpdateListener(&QueueCreatureUpdate)) {
    Log("SKIP queue: the action queue strip - no creature updates");
    return;
  }
  Log("queue: the action queue strip at the top of the screen (!queue on / off)%s; its window script %zu bytes",
      events ? "; a click cancels an item" : "; clicks NOT heard (no NUI events)", CreateScript().size());
}
