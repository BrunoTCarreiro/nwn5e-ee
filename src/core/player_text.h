#pragma once

#include <initializer_list>
#include <string>
#include <utility>

// #168: a player text by its key in data\text\en.txt (next to nwn5e.dll; data/text_catalog.h), its {names} filled in.
// A key the catalogue hasn't: "[nwn5e text: <key>]" (logged once) - wording is never made up in the code.
std::string PlayerText(const char* key, std::initializer_list<std::pair<const char*, std::string>> values = {});

// The catalogue's entry count (0: not loaded yet / not found) - the install logs it.
int PlayerTextEntries();
