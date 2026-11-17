// #168 (owner, 2026-11-16: "any new text is either written by me, or copied verbatim from the SRD"): the player text
// catalogue at run time - data\text\en.txt read once, next to nwn5e.dll.

#include "player_text.h"

#include <windows.h>

#include <fstream>
#include <set>
#include <sstream>
#include <vector>

#include "log.h"
#include "data/text_catalog.h"

namespace {

std::wstring DllDirectory() {
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&DllDirectory), &module);
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(module, path, MAX_PATH);
  std::wstring directory = path;
  const size_t slash = directory.find_last_of(L"\\/");
  return slash == std::wstring::npos ? L"." : directory.substr(0, slash);
}

const data::TextCatalog& Catalog() {
  static data::TextCatalog catalog;
  static bool loaded = false;
  if (loaded) return catalog;
  loaded = true;
  std::ifstream file(DllDirectory() + L"\\data\\text\\en.txt", std::ios::binary);
  if (!file.good()) {
    Log("text: data\\text\\en.txt not found - every player text shows its key");
    return catalog;
  }
  std::stringstream bytes;
  bytes << file.rdbuf();
  std::string error;
  if (!data::ParseTextCatalog(bytes.str(), &catalog, &error)) {
    Log("text: data\\text\\en.txt - %s (the entries before it kept)", error.c_str());
  }
  Log("text: %zu player texts from data\\text\\en.txt (#168)", catalog.size());
  return catalog;
}

}  // namespace

std::string PlayerText(const char* key, std::initializer_list<std::pair<const char*, std::string>> values) {
  const data::TextCatalog& catalog = Catalog();
  const auto it = key ? catalog.find(key) : catalog.end();
  if (it == catalog.end()) {
    static std::set<std::string> told;
    if (key && told.insert(key).second) Log("text: no entry %s in data\\text\\en.txt", key);
    return std::string("[nwn5e text: ") + (key ? key : "?") + "]";
  }
  std::vector<std::pair<std::string, std::string>> list;
  for (const auto& value : values) list.emplace_back(value.first, value.second);
  return data::FormatText(it->second.text, list);
}

int PlayerTextEntries() { return static_cast<int>(Catalog().size()); }
