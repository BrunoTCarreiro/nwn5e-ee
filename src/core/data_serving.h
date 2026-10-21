#pragma once

#include <stdint.h>

// CM0 (port of 1.69's data_serving; docs/EE-PORT.md E4a, EP4, EP5): nwn5e's
// own 2da rows and strings. Each table with rows in <dll>\data\rows (and the
// private layer's <dll>\data\private\rows) is rebuilt from the game's own copy
// + those rows into <dll>\served, and that folder joins the engine's resource
// manager at priority 15,000,000 — above the override folder and the patch
// keys, below a module and its haks (EP5) — before any table loads. The
// install is never touched.

// Strings for the private range: 2da cells name them (FEAT, DESCRIPTION...),
// the string-table hook (rules_labels.cpp) serves them.
constexpr uint32_t kDataStringFirst = 16000000;
constexpr uint32_t kDataStringEnd = 16500000;

void InstallDataServing(const wchar_t* dll_directory);

// The text for a private string id, or nullptr.
const char* DataString(uint32_t strref);
bool HaveDataStrings();

// Serve the creation pages' layouts (data\gui\*.gui) too — called by
// chargen_ui.cpp at install, once every hook those layouts need resolved.
void ServeCreationLayouts();
