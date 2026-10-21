// nwn5e's client windows (client_ui.h; docs/CHARGEN.md §10, spike B1).
//
// How (ee_layout.h "Client NUI"): the client's NUI message handler builds a
// window from a message; nwn5e writes that message with the engine's own
// CNWMessage writers into its own CNWMessage (constructed by the engine, so
// the engine heap owns its buffers), points the read side at it and calls the
// handler — the engine parses the JSON (UBJSON) and creates, updates or
// destroys the window exactly as for a script window. No engine object is
// made or freed by nwn5e. Events of nwn5e's windows are taken at
// JsonWindow::QueueEvent (they would otherwise go to a server) and handed to
// their handler at the next frame, from the CGuiMan::UpdateAndRender pre-hook
// that also runs the queued requests.
//
// [debug] client_nui_test=1: a test window opens on the main menu (B1).

#include "client_ui.h"

#include <windows.h>

#include <stdint.h>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "config.h"
#include "creature.h"
#include "data/ubjson.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "nui.h"

namespace {

using MessageFn = void (*)(void* message);
using CreateWriteFn = void (*)(void* message, uint32_t extra, uint32_t player, int flag);
using WriteIntFn = void (*)(void* message, int value, int bits);
using WriteByteFn = void (*)(void* message, uint8_t value, int bits, int tagged);
using WriteTypeFn = void (*)(void* message, int type);
using WriteDwordFn = void (*)(void* message, uint32_t value, int bits);
using WriteVoidPtrFn = void (*)(void* message, const void* data, int size);
using GetWriteFn = int (*)(void* message, uint8_t** data, uint32_t* size);
using SetReadFn = int (*)(void* message, uint8_t* data, uint32_t size, uint32_t player, int flag);
using HandlerFn = int (*)(void* message, uint8_t minor);
using UpdateAndRenderFn = void (*)(void* gui, float seconds);
using QueueEventFn = void (*)(void* window, const void* type, const void* element, int index, const void* payload);

struct Api {
  MessageFn construct = nullptr;
  CreateWriteFn create_write = nullptr;
  WriteIntFn write_int = nullptr;
  WriteByteFn write_byte = nullptr;
  WriteTypeFn write_type = nullptr;
  WriteDwordFn write_dword = nullptr;
  WriteVoidPtrFn write_void_ptr = nullptr;
  GetWriteFn get_write = nullptr;
  SetReadFn set_read = nullptr;
  HandlerFn handler = nullptr;
};
Api g_api;
UpdateAndRenderFn g_update_and_render = nullptr;
QueueEventFn g_queue_event = nullptr;
bool g_ready = false;

alignas(16) uint8_t g_message[0x100];  // nwn5e's CNWMessage (about 0x68 bytes)
constexpr uint32_t kNoPlayer = 0xFFFFFFFF;
constexpr int kFirstToken = 0x7E5E0000;  // far above the server's tokens (1, 2, ...)
int g_next_token = kFirstToken;

struct OpenWindow {
  std::string id;
  ClientNuiHandler handler;
};
std::unordered_map<int, OpenWindow> g_windows;  // by token

struct Request {
  enum Kind { kOpen, kBind, kClose } kind;
  int token;
  std::string name;    // the window id, or the bind name
  std::string ubjson;  // the layout, or the bind value
};
std::vector<Request> g_requests;
std::vector<ClientNuiEvent> g_events;
std::vector<ClientTickListener> g_tick_listeners;
int g_log_lines = 0;

bool LogSlot() { return g_log_lines < 300 && ++g_log_lines; }

void WriteString(const std::string& text) {
  g_api.write_type(g_message, ee::kMessageTypeExoString);
  g_api.write_int(g_message, static_cast<int>(text.size()), 32);
  for (char c : text) g_api.write_byte(g_message, static_cast<uint8_t>(c), 8, 0);
}

void WriteJson(const std::string& ubjson) {
  g_api.write_type(g_message, ee::kMessageTypeJson);
  g_api.write_dword(g_message, static_cast<uint32_t>(ubjson.size()), 32);
  g_api.write_void_ptr(g_message, ubjson.data(), static_cast<int>(ubjson.size()));
}

// Hands the written message to the client's NUI handler.
bool Deliver(uint8_t minor) {
  uint8_t* data = nullptr;
  uint32_t size = 0;
  if (!g_api.get_write(g_message, &data, &size) || !data || size <= 3) return false;
  g_api.set_read(g_message, data + 3, size - 3, kNoPlayer, 1);  // past the 3 header bytes, as the engine does
  try {
    return g_api.handler(g_message, minor) != 0;
  } catch (...) {
    Log("client nui: the engine refused a message (minor %d)", minor);
    return false;
  }
}

void Run(const Request& request) {
  static bool constructed = false;  // on the first frame: the engine's heap is ready by then
  if (!constructed) {
    g_api.construct(g_message);
    constructed = true;
  }
  g_api.create_write(g_message, 0, kNoPlayer, 1);
  bool delivered = false;
  switch (request.kind) {
    case Request::kOpen:
      g_api.write_int(g_message, request.token, 32);
      WriteString(request.name);
      g_api.write_int(g_message, 1, 32);  // the layout inline, as JSON
      WriteJson(request.ubjson);
      delivered = Deliver(ee::kNuiMinorCreate);
      Log("client nui: window %s opened (token 0x%08X)%s", request.name.c_str(), request.token,
          delivered ? "" : " - REFUSED by the engine");
      break;
    case Request::kBind:
      g_api.write_int(g_message, 1, 32);  // one bind
      g_api.write_int(g_message, request.token, 32);
      WriteString(request.name);
      WriteJson(request.ubjson);
      delivered = Deliver(ee::kNuiMinorBinds);
      if (!delivered && LogSlot()) Log("client nui: bind %s refused (token 0x%08X)", request.name.c_str(), request.token);
      break;
    case Request::kClose:
      g_api.write_int(g_message, request.token, 32);
      delivered = Deliver(ee::kNuiMinorDestroy);
      g_windows.erase(request.token);
      Log("client nui: window 0x%08X closed", request.token);
      break;
  }
}

void UpdateAndRender(void* gui, float seconds) {
  if (!g_events.empty()) {
    std::vector<ClientNuiEvent> events;
    events.swap(g_events);
    for (const ClientNuiEvent& event : events) {
      const auto window = g_windows.find(event.token);
      if (window == g_windows.end()) continue;
      const ClientNuiHandler handler = window->second.handler;
      if (event.type == "close") g_windows.erase(window);  // the player closed it; the engine already let go
      if (handler) handler(event);
    }
  }
  for (ClientTickListener listener : g_tick_listeners) listener();
  if (!g_requests.empty()) {
    std::vector<Request> requests;
    requests.swap(g_requests);
    for (const Request& request : requests) Run(request);
  }
  g_update_and_render(gui, seconds);
}

void QueueEvent(void* window, const void* type, const void* element, int index, const void* payload) {
  const int token = window ? Field<int32_t>(window, ee::kJsonWindowToken) : 0;
  if (token < kFirstToken || !g_windows.count(token)) {
    g_queue_event(window, type, element, index, payload);
    return;
  }
  ClientNuiEvent event;
  event.token = token;
  event.type = engine::StdString(type);
  event.element = engine::StdString(element);
  event.array_index = index;
  if (LogSlot() && event.type != "mousescroll") {
    Log("client nui: window 0x%08X: %s %s", token, event.type.c_str(), event.element.c_str());
  }
  g_events.push_back(std::move(event));  // handled next frame, outside the window's own update
}

template <typename Fn>
bool Export(Fn* out, const char* name) {
  *out = reinterpret_cast<Fn>(EngineExport(name));
  if (!*out) Log("SKIP client nui: no export %s", name);
  return *out != nullptr;
}

// The client's NUI message handler: the one function with a lea of the
// enable_make<JsonWindow> vtable, checked by its three ReadJSON calls.
uintptr_t FindHandler() {
  const uintptr_t vtable = FindVtable("client nui: enable_make<JsonWindow>", ee::kRttiEnableMakeJsonWindow);
  const std::vector<uintptr_t> references = FindRipReferences(vtable);
  if (references.size() != 1) {
    Log("SKIP client nui: the message handler - %zu references to its window type (expected 1)", references.size());
    return 0;
  }
  const uintptr_t handler = FunctionEntry(references[0]);
  const size_t reads = FindCallsTo(FunctionAt(handler), EngineExport(ee::kReadJSON)).size();
  if (reads != ee::kClientNuiHandlerReadJsonCalls) {
    Log("SKIP client nui: the message handler reads JSON %zu times (expected %d)", reads,
        ee::kClientNuiHandlerReadJsonCalls);
    return 0;
  }
  return handler;
}

// JsonWindow::QueueEvent, by its pattern; checked: JsonWindow::OnOpened (vtable slot 1) calls it.
uintptr_t FindQueueEvent() {
  const uintptr_t at = FindPattern("client nui: JsonWindow::QueueEvent", ee::kQueueEventPattern);
  const uintptr_t queue_event = at ? FunctionEntry(at) : 0;
  const uintptr_t on_opened = VtableSlot(FindVtable("client nui: JsonWindow", ee::kRttiJsonWindow), 1);
  if (!queue_event || !on_opened || FindCallsTo(FunctionAt(on_opened), queue_event).empty()) {
    Log("SKIP client nui: JsonWindow::QueueEvent not confirmed");
    return 0;
  }
  return queue_event;
}

// --- B1: the test window ([debug] client_nui_test) --------------------------

int g_test_token = 0;
DWORD g_test_started = 0;
int g_test_stage = 0;

void TestEvent(const ClientNuiEvent& event) {
  if (event.type == "close") {
    Log("client nui test: closed with the window's X");
    return;
  }
  if (event.type != "click") return;
  if (event.element == "close") {
    ClientNuiClose(event.token);
    return;
  }
  ClientNuiSetText(event.token, "status", "You clicked \"" + event.element + "\" - nwn5e got the click.");
}

void TestTick() {
  const DWORD now = GetTickCount();
  if (!g_test_started) g_test_started = now;
  if (g_test_stage == 0 && now - g_test_started > 3000) {
    g_test_stage = 1;
    using namespace nui;
    const std::string layout = Window(
        Col(List({
            Text(Str("This window was opened by nwn5e on the main menu - EE's own NUI, no game loaded."), 60.0f),
            Label(Bind("status"), 30.0f),
            Row(List({Button("background", Str("Background")), Button("species", Str("Species")),
                      Button("class", Str("Class"))})),
            Row(List({Spacer(), Button("close", Str("Close"), 120.0f), Spacer()})),
        })),
        Str("nwn5e - client window test"), 60.0f, 60.0f, 560.0f, 300.0f);
    g_test_token = ClientNuiOpen("nwn5eclienttest", layout, &TestEvent);
    ClientNuiSetText(g_test_token, "status", "Opened - waiting for a bind update...");
  }
  if (g_test_stage == 1 && now - g_test_started > 6000) {
    g_test_stage = 2;
    ClientNuiSetText(g_test_token, "status", "Bind updated by nwn5e. Click a button.");
  }
}

}  // namespace

bool ClientNuiReady() { return g_ready; }

int ClientNuiOpen(const char* id, const std::string& layout, ClientNuiHandler handler) {
  if (!g_ready) return 0;
  std::string ubjson, error;
  if (!data::JsonToUbjson(layout, &ubjson, &error)) {
    Log("client nui: window %s not opened - its layout isn't JSON (%s)", id, error.c_str());
    return 0;
  }
  for (auto it = g_windows.begin(); it != g_windows.end();) {  // the same id replaces a window
    it = it->second.id == id ? g_windows.erase(it) : std::next(it);
  }
  const int token = g_next_token++;
  g_windows[token] = OpenWindow{std::string(id), handler};
  g_requests.push_back({Request::kOpen, token, id, std::move(ubjson)});
  return token;
}

void ClientNuiSetText(int token, const char* bind, const std::string& text) {
  if (!g_ready || !token) return;
  std::string ubjson, error;
  if (!data::JsonToUbjson(nui::Str(text), &ubjson, &error)) return;
  g_requests.push_back({Request::kBind, token, bind, std::move(ubjson)});
}

void ClientNuiClose(int token) {
  if (!g_ready || !token) return;
  g_requests.push_back({Request::kClose, token, "", ""});
}

void AddClientTickListener(ClientTickListener listener) { g_tick_listeners.push_back(listener); }

void InstallClientUi() {
  bool exports = Export(&g_api.construct, ee::kCNWMessageConstructor);
  exports &= Export(&g_api.create_write, ee::kCreateWriteMessage);
  exports &= Export(&g_api.write_int, ee::kWriteINT);
  exports &= Export(&g_api.write_byte, ee::kWriteBYTE);
  exports &= Export(&g_api.write_type, ee::kWriteType);
  exports &= Export(&g_api.write_dword, ee::kWriteDWORD);
  exports &= Export(&g_api.write_void_ptr, ee::kWriteVOIDPtr);
  exports &= Export(&g_api.get_write, ee::kGetWriteMessage);
  exports &= Export(&g_api.set_read, ee::kSetReadMessage);
  if (!exports) return;
  g_api.handler = reinterpret_cast<HandlerFn>(FindHandler());
  const uintptr_t queue_event = FindQueueEvent();
  if (!g_api.handler || !queue_event) return;
  if (!HookExport("CGuiMan::UpdateAndRender (nwn5e's client windows: requests and events)", ee::kGuiUpdateAndRender,
                  reinterpret_cast<void*>(&UpdateAndRender), reinterpret_cast<void**>(&g_update_and_render)) ||
      !HookAddress("JsonWindow::QueueEvent (nwn5e's client windows: their events)", queue_event,
                   reinterpret_cast<void*>(&QueueEvent), reinterpret_cast<void**>(&g_queue_event))) {
    return;
  }
  g_ready = true;
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  Log("client nui: ready (handler rva 0x%zx, QueueEvent rva 0x%zx)", reinterpret_cast<uintptr_t>(g_api.handler) - base,
      queue_event - base);
  if (g_config.debug_client_nui_test) AddClientTickListener(&TestTick);
}
