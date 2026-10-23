#include "nui.h"

#include <stdio.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "creature.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"

namespace {

using SetCurrentNuiEventFn = void (*)(void* module, const void* event);

SetCurrentNuiEventFn g_set_current_nui_event = nullptr;
std::unordered_map<std::string, NuiEventHandler> g_windows;  // by window id
int g_log_lines = 0;

bool LogSlot() { return g_log_lines < 300 && ++g_log_lines; }


std::string TokenVar(const char* window_id) { return std::string("nwn5e_nui_") + window_id; }

// An NWScript string literal.
std::string ScriptString(const std::string& text) {
  std::string out = "\"";
  for (char c : text) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out + "\"";
}

void SetCurrentNuiEvent(void* module, const void* event) {
  g_set_current_nui_event(module, event);
  if (!event || g_windows.empty()) return;
  const auto* e = static_cast<const uint8_t*>(event);
  NuiEvent ev;
  ev.player = Field<uint32_t>(e, ee::kNuiEventPlayer);
  ev.token = Field<int32_t>(e, ee::kNuiEventToken);
  void* creature = ev.token > 0 ? CreatureById(ev.player) : nullptr;
  if (!creature) return;
  for (const auto& window : g_windows) {
    const std::string var = TokenVar(window.first.c_str());
    if (engine::GetLocalInt(creature, var.c_str()) != ev.token) continue;
    ev.type = engine::StdString(e + ee::kNuiEventType);
    ev.element = engine::StdString(e + ee::kNuiEventElement);
    ev.array_index = Field<int32_t>(e, ee::kNuiEventArrayIndex);
    if (LogSlot()) {
      Log("nui: 0x%08X window %s (token %d): %s %s", ev.player, window.first.c_str(), ev.token, ev.type.c_str(),
          ev.element.c_str());
    }
    if (ev.type == "close") engine::SetLocalInt(creature, var.c_str(), 0);  // the player closed it
    window.second(ev);
    return;
  }
}

}  // namespace

bool AddNuiWindow(const char* window_id, NuiEventHandler handler) {
  static bool tried = false, hooked = false;
  if (!tried) {
    tried = true;
    hooked = HookExport("CNWSModule::SetCurrentNuiEvent (nwn5e's windows)", ee::kSetCurrentNuiEvent,
                        reinterpret_cast<void*>(&SetCurrentNuiEvent),
                        reinterpret_cast<void**>(&g_set_current_nui_event));
  }
  if (hooked) g_windows[window_id] = handler;
  return hooked;
}

int NuiOpen(uint32_t player, const char* window_id, const std::string& layout,
            const std::vector<std::pair<std::string, std::string>>& binds) {
  void* creature = CreatureById(player);
  if (!creature) return 0;
  const std::string var = TokenVar(window_id);
  std::string code = "object p = OBJECT_SELF; json j = JsonParse(" + ScriptString(layout) +
                     "); int t = NuiCreate(p, j, " + ScriptString(window_id) + "); SetLocalInt(p, " +
                     ScriptString(var) + ", t); if (!t) { SendMessageToPC(p, \"nwn5e: a window failed to open (\" + " +
                     "JsonGetError(j) + \").\"); return; }";
  for (const auto& bind : binds) {
    code += " NuiSetBind(p, t, " + ScriptString(bind.first) + ", JsonString(" + ScriptString(bind.second) + "));";
  }
  RunScriptChunk(code.c_str(), player);
  const int token = engine::GetLocalInt(creature, var.c_str());
  Log("nui: 0x%08X opened %s: token %d%s", player, window_id, token, token ? "" : " (NuiCreate refused the layout)");
  return token;
}

int NuiToken(uint32_t player, const char* window_id) {
  void* creature = CreatureById(player);
  return creature ? engine::GetLocalInt(creature, TokenVar(window_id).c_str()) : 0;
}

void NuiSetText(uint32_t player, const char* window_id, const char* bind, const std::string& text) {
  const int token = NuiToken(player, window_id);
  if (!token) return;
  const std::string code = "NuiSetBind(OBJECT_SELF, " + std::to_string(token) + ", " + ScriptString(bind) +
                           ", JsonString(" + ScriptString(text) + "));";
  RunScriptChunk(code.c_str(), player);
}

void NuiSetJson(uint32_t player, const char* window_id, const char* bind, const std::string& json) {
  const int token = NuiToken(player, window_id);
  if (!token) return;
  const std::string code = "NuiSetBind(OBJECT_SELF, " + std::to_string(token) + ", " + ScriptString(bind) +
                           ", JsonParse(" + ScriptString(json) + "));";
  RunScriptChunk(code.c_str(), player);
}

void NuiClose(uint32_t player, const char* window_id) {
  void* creature = CreatureById(player);
  const int token = NuiToken(player, window_id);
  if (!creature || !token) return;
  engine::SetLocalInt(creature, TokenVar(window_id).c_str(), 0);
  const std::string code = "DelayCommand(0.0, NuiDestroy(OBJECT_SELF, " + std::to_string(token) + "));";
  RunScriptChunk(code.c_str(), player);
}

namespace nui {

std::string Str(const std::string& text) {
  std::string out = "\"";
  for (char c : text) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) >= 0x20) out += c;
    }
  }
  return out + "\"";
}

std::string Bind(const char* id) {
  return "{\"bind\":" + Str(id) + ",\"number_flags\":0,\"number_precision\":0,\"text_flags\":0}";
}

std::string List(std::initializer_list<std::string> items) {
  std::string out;
  for (const std::string& item : items) {
    if (!out.empty()) out += ",";
    out += item;
  }
  return out;
}

namespace {
std::string Element(const char* type, const std::string& label, const std::string& value, const std::string& more) {
  return std::string("{\"type\":") + Str(type) + ",\"label\":" + label + ",\"value\":" + value + more + "}";
}
std::string Size(const char* key, float value) {
  if (value <= 0.0f) return "";
  char b[48];
  sprintf_s(b, ",\"%s\":%.1f", key, value);
  return b;
}
}  // namespace

std::string Col(const std::string& children) { return Element("col", "null", "null", ",\"children\":[" + children + "]"); }

std::string Row(const std::string& children) { return Element("row", "null", "null", ",\"children\":[" + children + "]"); }

std::string Label(const std::string& value, float height) {
  return Element("label", "null", value, ",\"text_halign\":1,\"text_valign\":0" + Size("height", height));
}

std::string Text(const std::string& value, float height) {
  return Element("text", "null", value, ",\"border\":true,\"scrollbars\":4" + Size("height", height));
}

std::string Button(const char* id, const std::string& label, float width) {
  return Element("button", label, "null", ",\"id\":" + Str(id) + Size("width", width));
}

std::string Spacer() { return Element("spacer", "null", "null", ""); }

std::string TextEdit(const char* id, const std::string& placeholder, const char* value_bind, int max_length,
                     float height) {
  return Element("textedit", Str(placeholder), Bind(value_bind),
                 ",\"id\":" + Str(id) + ",\"max\":" + std::to_string(max_length) +
                     ",\"multiline\":false,\"wordwrap\":false" + Size("height", height));
}

std::string ListView(const std::string& cells, const std::string& row_count, float row_height) {
  char height[48];
  sprintf_s(height, ",\"row_height\":%.1f", row_height);
  return Element("list", "null", "null",
                 ",\"row_template\":[" + cells + "],\"row_count\":" + row_count + height +
                     ",\"border\":true,\"scrollbars\":2");
}

std::string Cell(const std::string& element, float width, bool variable) {
  char rest[48];
  sprintf_s(rest, ",%.1f,%s]", width, variable ? "true" : "false");
  return "[" + element + rest;
}

std::string Window(const std::string& root, const std::string& title, float x, float y, float w, float h,
                   bool closable) {
  char geometry[128];
  sprintf_s(geometry, "{\"x\":%.1f,\"y\":%.1f,\"w\":%.1f,\"h\":%.1f}", x, y, w, h);
  return "{\"version\":1,\"title\":" + title + ",\"root\":" + root + ",\"geometry\":" + geometry +
         ",\"resizable\":true,\"collapsed\":null,\"closable\":" + (closable ? "true" : "false") +
         ",\"transparent\":false,\"border\":true,\"accepts_input\":true,\"size_constraint\":null,"
         "\"edge_constraint\":null,\"font\":\"\"}";
}

}  // namespace nui
