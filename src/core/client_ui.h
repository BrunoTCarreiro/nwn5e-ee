#pragma once

// nwn5e's windows on the client (docs/CHARGEN.md §10): EE's own client NUI
// windows (Nui::JSON::JsonWindow), opened from nwn5e's layouts (the JSON the
// nui:: builders write) anywhere — the main menu, character creation,
// level-up — with no server or module. The engine builds and owns every
// window; nwn5e only hands it messages written with the engine's writers and
// reads the events of its own windows. Everything runs on the game's main
// thread, a frame later (requests made inside an event are safe).

#include <stdint.h>

#include <string>

struct ClientNuiEvent {
  int token = 0;
  std::string type;     // "click", "close", "open", "mousedown", ...
  std::string element;  // the id of the element ("_window_" for the window)
  int array_index = -1;
};

using ClientNuiHandler = void (*)(const ClientNuiEvent& event);

// Every anchor found and checked (false: client windows are unavailable, logged).
bool ClientNuiReady();

// Opens `layout` as window `id` (alphanumeric: a window with the same id is
// replaced) on the next frame; returns its token, 0 when not ready or the
// layout isn't JSON (logged).
int ClientNuiOpen(const char* id, const std::string& layout, ClientNuiHandler handler);

// Sets a string bind of an open window (next frame).
void ClientNuiSetText(int token, const char* bind, const std::string& text);
// Sets a bind to a JSON value (a geometry rect, a colour, a bool; next frame).
void ClientNuiSetJson(int token, const char* bind, const std::string& json);

// Closes a window (next frame).
void ClientNuiClose(int token);

// Called every frame on the main thread, before the windows render.
using ClientTickListener = void (*)();
void AddClientTickListener(ClientTickListener listener);
