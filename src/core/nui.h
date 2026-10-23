#pragma once

// NUI, EE's scriptable windows: nwn5e builds a window's layout (the JSON of
// EE's nw_inc_nui.nss), opens it for a player character through the script
// bridge (JsonParse + NuiCreate) and receives its events from the engine
// (CNWSModule::SetCurrentNuiEvent, ee_layout.h) — no compiled script needed.
// A window belongs to a character in the game: the creation screens are the
// game's own.

#include <stdint.h>

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

struct NuiEvent {
  uint32_t player = 0;  // the player character's object id
  int token = 0;        // the window
  std::string type;     // "click", "close", "open", "watch", "mousedown", ...
  std::string element;  // the id of the element that sent it ("" for the window)
  int array_index = -1;
};

// A window's events, by its id. Run on the engine's thread, inside the
// engine's event handling (it still uses the window afterwards): change binds
// freely, close with NuiClose (it waits a tick), and don't open a window from
// here.
using NuiEventHandler = void (*)(const NuiEvent& event);

// Hooks the event store on first use; false (and logged) if it can't.
bool AddNuiWindow(const char* window_id, NuiEventHandler handler);

// Opens `layout` (a window's JSON) for `player` as `window_id` (alphanumeric,
// short; an open window with that id is replaced) and sets the given string
// binds; the token is kept on the character (nwn5e_nui_<id>). Returns the
// token, 0 when NuiCreate refused the layout (the player is told why).
int NuiOpen(uint32_t player, const char* window_id, const std::string& layout,
            const std::vector<std::pair<std::string, std::string>>& binds = {});

// The open window's token for `player`, or 0.
int NuiToken(uint32_t player, const char* window_id);

// Sets a string bind of an open window.
void NuiSetText(uint32_t player, const char* window_id, const char* bind, const std::string& text);

// Sets a bind of an open window to a JSON value (an array, a number...).
void NuiSetJson(uint32_t player, const char* window_id, const char* bind, const std::string& json);

// Closes the window a tick later (safe from inside its own events).
void NuiClose(uint32_t player, const char* window_id);

// Layout builders (nw_inc_nui.nss's JSON). Children lists are JSON arrays'
// contents: elements joined with commas (nui::List).
namespace nui {
std::string Str(const std::string& text);  // a JSON string value
std::string Bind(const char* id);           // a bind: its value is set with NuiSetText
std::string List(std::initializer_list<std::string> items);
std::string Col(const std::string& children);
std::string Row(const std::string& children);
std::string Label(const std::string& value, float height = 0.0f);
std::string Text(const std::string& value, float height = 0.0f);
std::string Button(const char* id, const std::string& label, float width = 0.0f);
std::string Spacer();
// A list view (nw_inc_nui's NuiList): `cells` the row template's cells (Cell,
// joined with commas), each element's binds arrays of `row_count` entries; a
// click on a row's element reports its array_index.
std::string ListView(const std::string& cells, const std::string& row_count, float row_height);
std::string Cell(const std::string& element, float width, bool variable);
std::string Window(const std::string& root, const std::string& title, float x, float y, float w, float h,
                   bool closable = true);
}  // namespace nui
