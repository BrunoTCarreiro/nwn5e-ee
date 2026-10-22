#pragma once

#include <stdint.h>

#include <string>
#include <vector>

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

// One column of the engine's copy of `table`.2da (served by nwn5e or the
// game's own), by row ("" for "****"); empty when it can't be read (no
// resource manager yet, no such table or column).
std::vector<std::string> GameTableColumn(const char* table, const char* column);

// The text for a private string id, or nullptr.
const char* DataString(uint32_t strref);
bool HaveDataStrings();

// Serve the creation pages' layouts (data\gui\*.gui) too — called by
// chargen_ui.cpp at install, once every hook those layouts need resolved.
void ServeCreationLayouts();
// C5: the Skills page's layout (data\gui\skills: its Reset button).
void ServeSkillsPageLayout();

// Rows for one mode: a rows file with a part "if_<flag>"
// (packages.if_kits.txt) is served only once a rule has called this with
// <flag> at install — data that would change the other mode's tables.
void ServeConditionalRows(const char* flag);
