// CM0 (data_serving.h), EE: at the rules object's constructor entry (the
// resource manager has its key tables; the rules tables load right after),
// the original tables are read through the engine's own resource manager
// (CExoResMan::Get: the override folder, patches and the base keys in the
// engine's order), merged with nwn5e's rows and written to <dll>\served; then
// an alias NWN5E -> that folder and AddResourceDirectory("NWN5E:",
// 15,000,000) make the engine read them. (At SetupDefaultSearchPath's end, an
// earlier point, Get found none of the base tables: the base keys come later.)

#include "data_serving.h"

#include <windows.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "config.h"
#include "creature.h"
#include "data/twoda.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "rules_loaded.h"

void GenerateAnimationPacks(const std::wstring& served);  // anim_packs.cpp
void GenerateTorchFlames(const std::wstring& served);     // light_sources.cpp

namespace {

using ExoStringCtorFn = void* (*)(void* string, const char* text);
using AliasAddFn = void (*)(void* list, void* alias, void* path);
using AddResourceDirectoryFn = int (*)(void* resman, const void* name, uint32_t priority, int detect_changes,
                                       void* filter);
using ResManGetFn = void* (*)(void* resman, void* result, const void* resref, uint16_t type);
using ResRefAssignFn = void* (*)(void* resref, const char* text);
using DataBlockReleaseFn = void (*)(void* shared_ptr);
using GetSimpleStringFn = void* (*)(void* tlk, void* result, uint32_t strref);
using ExoStringDtorFn = void (*)(void* string);

struct Api {
  ExoStringCtorFn string_ctor = reinterpret_cast<ExoStringCtorFn>(EngineExport(ee::kExoStringFromChars));
  ExoStringDtorFn string_dtor = reinterpret_cast<ExoStringDtorFn>(EngineExport(ee::kExoStringDestructor));
  AliasAddFn alias_add = reinterpret_cast<AliasAddFn>(EngineExport(ee::kAliasListAdd));
  AddResourceDirectoryFn add_directory = reinterpret_cast<AddResourceDirectoryFn>(EngineExport(ee::kAddResourceDirectory));
  ResManGetFn get = reinterpret_cast<ResManGetFn>(EngineExport(ee::kResManGet));
  ResRefAssignFn resref_assign = reinterpret_cast<ResRefAssignFn>(EngineExport(ee::kResRefAssignChars));
  DataBlockReleaseFn release = reinterpret_cast<DataBlockReleaseFn>(EngineExport(ee::kDataBlockPtrDestructor));
  void** exo_base = reinterpret_cast<void**>(EngineExport(ee::kExoBase));
  void** resman = reinterpret_cast<void**>(EngineExport(ee::kExoResMan));

  bool Complete() const {
    return string_ctor && alias_add && add_directory && get && resref_assign && release && exo_base && resman;
  }
};

const Api& E() {
  static const Api api;
  return api;
}

std::map<uint32_t, std::string> g_strings;
std::wstring g_dll;
std::string g_served;  // ANSI, for the engine
bool g_registered = false;
int g_probe_row = -1;  // feat.2da row of the probe (served_data_probe)
bool g_serve_creation_layouts = false;  // ServeCreationLayouts()
bool g_serve_skills_layout = false;     // ServeSkillsPageLayout()
std::set<std::string> g_row_flags;       // ServeConditionalRows()

// "packages.if_kits.hide" -> "kits"; "" when no part is conditional.
std::string ConditionalFlag(const std::string& name) {
  size_t at = name.find('.');
  while (at != std::string::npos) {
    const size_t next = name.find('.', at + 1);
    const std::string part = name.substr(at + 1, next == std::string::npos ? std::string::npos : next - at - 1);
    if (part.rfind("if_", 0) == 0) return part.substr(3);
    at = next;
  }
  return "";
}

constexpr const char* kAlias = "NWN5E";
constexpr uint32_t kProbeName = kDataStringFirst;
constexpr uint32_t kProbeDescription = kDataStringFirst + 1;

std::string Narrow(const std::wstring& wide) {
  char buffer[MAX_PATH * 2] = {};
  WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, buffer, sizeof(buffer) - 1, nullptr, nullptr);
  return buffer;
}

bool ReadText(const std::wstring& path, std::string* text) {
  FILE* file = nullptr;
  if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file) return false;
  char buffer[4096];
  size_t read = 0;
  text->clear();
  while ((read = fread(buffer, 1, sizeof(buffer), file)) > 0) text->append(buffer, read);
  fclose(file);
  return true;
}

bool WriteText(const std::wstring& path, const std::string& text) {
  FILE* file = nullptr;
  if (_wfopen_s(&file, path.c_str(), L"wb") != 0 || !file) return false;
  const size_t written = fwrite(text.data(), 1, text.size(), file);
  fclose(file);
  return written == text.size();
}

std::vector<std::wstring> FilesIn(const std::wstring& directory, const wchar_t* pattern) {
  std::vector<std::wstring> names;
  WIN32_FIND_DATAW found;
  HANDLE handle = FindFirstFileW((directory + L"\\" + pattern).c_str(), &found);
  if (handle == INVALID_HANDLE_VALUE) return names;
  do {
    if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) names.push_back(found.cFileName);
  } while (FindNextFileW(handle, &found));
  FindClose(handle);
  return names;
}

// "<id> <text>" per line ('#' comments); "\n" in the text is a line break.
void LoadStrings(const std::wstring& path, const char* label) {
  std::string text;
  if (!ReadText(path, &text)) return;
  int count = 0;
  size_t start = 0;
  while (start < text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(start, end - start);
    start = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const size_t first = line.find_first_not_of(" \t");
    if (first == std::string::npos || line[first] == '#') continue;
    char* rest = nullptr;
    const unsigned long id = strtoul(line.c_str() + first, &rest, 10);
    if (id < kDataStringFirst || id >= kDataStringEnd) {
      Log("data: %s: string id %lu outside %u..%u, skipped", label, id, kDataStringFirst, kDataStringEnd - 1);
      continue;
    }
    std::string value = rest;
    value.erase(0, value.find_first_not_of(" \t"));
    for (size_t at = value.find("\\n"); at != std::string::npos; at = value.find("\\n", at + 1)) value.replace(at, 2, "\n");
    g_strings[static_cast<uint32_t>(id)] = value;
    ++count;
  }
  Log("data: %d strings from %s", count, label);
}

// The probe (served_data_probe): a copy of feat 0 named by our strings that no
// class can take, so the log can show the served table and strings arrived.
void AddProbe(data::TwoDA* feat) {
  if (feat->rows.empty()) return;
  std::vector<std::string> row = feat->rows[0];
  for (size_t c = 0; c < feat->columns.size(); ++c) {
    const std::string& column = feat->columns[c];
    if (_stricmp(column.c_str(), "LABEL") == 0) row[c] = "NWN5E_PROBE";
    if (_stricmp(column.c_str(), "FEAT") == 0) row[c] = std::to_string(kProbeName);
    if (_stricmp(column.c_str(), "DESCRIPTION") == 0) row[c] = std::to_string(kProbeDescription);
    if (_stricmp(column.c_str(), "ALLCLASSESCANUSE") == 0) row[c] = "0";
  }
  g_probe_row = static_cast<int>(feat->rows.size());
  feat->rows.push_back(row);
  g_strings[kProbeName] = "nwn5e probe feat";
  g_strings[kProbeDescription] = "Served by nwn5e (served_data_probe).";
}

// The engine's current copy of `table`.2da (override, patches, base keys).
bool ReadResource(const std::string& resref_text, uint16_t type, std::string* bytes, std::string* why) {
  void* resman = *E().resman;
  if (!resman) {
    *why = "no resource manager yet";
    return false;
  }
  alignas(16) uint8_t resref[32] = {};
  E().resref_assign(resref, resref_text.c_str());
  alignas(16) uint8_t block[16] = {};  // shared_ptr<DataBlock>
  E().get(resman, block, resref, type);
  // As ReadScriptFile reads it (+0xdd): the DataBlock's first field points at
  // {char* data, size_t size}.
  const auto* data_block = *reinterpret_cast<const uint8_t* const*>(block);
  const auto* contents = data_block ? *reinterpret_cast<const uint8_t* const*>(data_block) : nullptr;
  bool found = false;
  if (contents) {
    const char* data = *reinterpret_cast<const char* const*>(contents);
    const size_t size = *reinterpret_cast<const size_t*>(contents + 8);
    if (data && size > 0 && size < (64u << 20)) {
      bytes->assign(data, size);
      found = true;
    }
  }
  if (!found) {
    char text[64];
    sprintf_s(text, "Get gave block %p", static_cast<const void*>(data_block));
    *why = text;
  }
  E().release(block);
  return found;
}

bool ReadGameTable(const std::string& table, std::string* bytes, std::string* why) {
  return ReadResource(table, ee::kResourceType2da, bytes, why);
}

// Builds <dll>\served\<table>.2da for every table with rows. Returns the count.
int BuildServedTables() {
  const std::wstring served = g_dll + L"\\served";
  std::map<std::string, std::vector<std::pair<std::wstring, std::string>>> rows;  // table -> (file, label)
  const std::pair<std::wstring, const char*> sources[] = {{g_dll + L"\\data\\rows", "data"},
                                                           {g_dll + L"\\data\\private\\rows", "private"}};
  for (const auto& source : sources) {
    for (const std::wstring& name : FilesIn(source.first, L"*.txt")) {
      std::string table = Narrow(name.substr(0, name.size() - 4));
      std::string flag = ConditionalFlag(table);
      for (char& c : flag) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
      if (!flag.empty() && !g_row_flags.count(flag)) continue;  // the other mode's rows
      table = table.substr(0, table.find('.'));  // "feat.styles.txt" -> feat.2da
      for (char& c : table) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
      if (table == "readme") continue;
      rows[table].push_back({source.first + L"\\" + name, std::string(source.second) + "\\rows\\" + Narrow(name)});
    }
  }
  if (g_config.debug_served_data_probe) rows["feat"];  // the probe needs feat.2da even without rows
  CreateDirectoryW(served.c_str(), nullptr);
  for (const std::wstring& old : FilesIn(served, L"*")) DeleteFileW((served + L"\\" + old).c_str());
  // Files served as they are (compiled scripts: data\files).
  int files = 0;
  for (const std::wstring& folder : {g_dll + L"\\data\\files", g_dll + L"\\data\\private\\files"}) {
    for (const std::wstring& name : FilesIn(folder, L"*")) {
      if (CopyFileW((folder + L"\\" + name).c_str(), (served + L"\\" + name).c_str(), FALSE)) ++files;
    }
  }
  if (files) Log("data: served %d files as they are (data\\files: compiled scripts ...)", files);
  GenerateAnimationPacks(served);  // animation recipes (anim_packs.cpp), before the folder is registered
  GenerateTorchFlames(served);     // the held torch's flame rising (light_sources.cpp), likewise
  // The creation pages' layouts (data\gui), only when their code is in place
  // (chargen_ui.cpp): a layout without it would show nwn5e's captions on the
  // game's own pages.
  if (g_serve_creation_layouts) {
    int layouts = 0;
    const std::wstring folder = g_dll + L"\\data\\gui";
    for (const std::wstring& name : FilesIn(folder, L"*.gui")) {
      if (CopyFileW((folder + L"\\" + name).c_str(), (served + L"\\" + name).c_str(), FALSE)) ++layouts;
    }
    Log("data: served %d creation page layouts (data\\gui)", layouts);
  }
  // The Skills page's layout (its Reset button), only with its code in place
  // (chargen_skills.cpp): otherwise a button with nothing behind it.
  if (g_serve_skills_layout) {
    const std::wstring folder = g_dll + L"\\data\\gui\\skills";
    int layouts = 0;
    for (const std::wstring& name : FilesIn(folder, L"*.gui")) {
      if (CopyFileW((folder + L"\\" + name).c_str(), (served + L"\\" + name).c_str(), FALSE)) ++layouts;
    }
    Log("data: served the Skills page's layout (data\\gui\\skills: %d)", layouts);
  }
  int built = 0;
  for (const auto& entry : rows) {
    std::string bytes, error, why;
    if (!ReadGameTable(entry.first, &bytes, &why)) {
      Log("data: SKIP %s.2da: not found in the game's resources (%s)", entry.first.c_str(), why.c_str());
      continue;
    }
    data::TwoDA table;
    if (!data::ParseTwoDA(bytes, &table, &error)) {
      Log("data: SKIP %s.2da: %s", entry.first.c_str(), error.c_str());
      continue;
    }
    const size_t original = table.rows.size();
    std::string applied;
    bool ok = true;
    for (const auto& file : entry.second) {
      std::string text;
      int changed = 0;
      if (!ReadText(file.first, &text) || !data::ApplyRows(&table, text, &error, &changed)) {
        Log("data: SKIP %s.2da: %s: %s", entry.first.c_str(), file.second.c_str(), error.c_str());
        ok = false;
        break;
      }
      applied += " + " + std::to_string(changed) + " rows (" + file.second + ")";
    }
    if (!ok) continue;
    if (entry.first == "feat" && g_config.debug_served_data_probe) {
      AddProbe(&table);
      applied += " + the probe (row " + std::to_string(g_probe_row) + ")";
    }
    if (!WriteText(served + L"\\" + std::wstring(entry.first.begin(), entry.first.end()) + L".2da",
                   data::WriteTwoDA(table))) {
      Log("data: SKIP %s.2da: can't write to served", entry.first.c_str());
      continue;
    }
    ++built;
    Log("data: served %s.2da: %u rows%s -> %u rows", entry.first.c_str(), static_cast<unsigned>(original),
        applied.c_str(), static_cast<unsigned>(table.rows.size()));
  }
  return built + files;
}

void ServeBeforeRulesLoad() {
  if (g_registered || !E().Complete()) return;
  g_registered = true;
  // The originals first: once our folder is registered, Get finds our copies.
  if (BuildServedTables() == 0) {
    Log("data: no tables to serve");
    return;
  }
  void* base = *E().exo_base;
  void* aliases = base ? Field<void*>(base, ee::kExoBaseAliasList) : nullptr;
  if (!aliases || !*E().resman) {
    Log("data: SKIP registering the served folder: aliases %p, resource manager %p", aliases, *E().resman);
    return;
  }
  alignas(16) uint8_t alias[16] = {};
  alignas(16) uint8_t path[16] = {};
  E().string_ctor(alias, kAlias);
  E().string_ctor(path, g_served.c_str());
  E().alias_add(aliases, alias, path);  // frees both
  alignas(16) uint8_t name[16] = {};
  E().string_ctor(name, (std::string(kAlias) + ":").c_str());
  const int added = E().add_directory(*E().resman, name, ee::kServedPriority, 0, nullptr);
  if (E().string_dtor) E().string_dtor(name);
  Log("data: %s: = %s registered with the resource manager at priority %u (%d)", kAlias, g_served.c_str(),
      ee::kServedPriority, added);
}

// served_data_probe: after loading, the probe row in the engine's feat table
// and its name through the string table (our hook).
void CheckProbe(void* rules, bool reload) {
  if (reload || g_probe_row < 0) return;
  const int count = Field<uint16_t>(rules, ee::kRulesFeatCount);
  auto* feats = Field<uint8_t*>(rules, ee::kRulesFeats);
  if (!feats || g_probe_row >= count) {
    Log("data: probe FAILED - the engine loaded %d feats, the probe is row %d", count, g_probe_row);
    return;
  }
  const uint32_t name = Field<uint32_t>(feats + g_probe_row * ee::kFeatStride, ee::kFeatName);
  Log("data: probe %s - the engine loaded %d feats; row %d's name is string %u (%s)",
      name == kProbeName ? "OK" : "FAILED", count, g_probe_row, name, DataString(name) ? DataString(name) : "none");
}

}  // namespace

void InstallDataServing(const wchar_t* dll_directory) {
  g_dll = dll_directory;
  LoadStrings(g_dll + L"\\data\\strings.txt", "data\\strings.txt");
  LoadStrings(g_dll + L"\\data\\private\\strings.txt", "private\\strings.txt");
  if (!g_config.served_data) {
    Log("data: served_data=0 - nwn5e's rows are not served");
    return;
  }
  if (!E().Complete()) {
    Log("SKIP data: a resource-manager export is missing in this game build");
    return;
  }
  g_served = Narrow(g_dll + L"\\served");
  BeforeRulesLoad(&ServeBeforeRulesLoad);
  if (g_config.debug_served_data_probe) OnRulesLoaded(&CheckProbe);
}

bool ReadGameResource(const std::string& resref, uint16_t type, std::string* bytes, std::string* why) {
  return ReadResource(resref, type, bytes, why);
}

std::vector<std::string> GameTableColumn(const char* table, const char* column) {
  std::vector<std::string> values;
  std::string bytes;
  std::string why;
  std::string error;
  data::TwoDA parsed;
  if (!ReadGameTable(table, &bytes, &why) || !data::ParseTwoDA(bytes, &parsed, &error)) return values;
  size_t index = parsed.columns.size();
  for (size_t i = 0; i < parsed.columns.size(); ++i) {
    if (_stricmp(parsed.columns[i].c_str(), column) == 0) index = i;
  }
  if (index == parsed.columns.size()) return values;
  for (const std::vector<std::string>& row : parsed.rows) {
    values.push_back(index < row.size() && row[index] != data::kEmpty ? row[index] : std::string());
  }
  return values;
}

const char* DataString(uint32_t strref) {
  if (strref < kDataStringFirst || strref >= kDataStringEnd) return nullptr;
  const auto found = g_strings.find(strref);
  return found == g_strings.end() ? nullptr : found->second.c_str();
}

void ServeCreationLayouts() { g_serve_creation_layouts = true; }
void ServeSkillsPageLayout() { g_serve_skills_layout = true; }

void ServeConditionalRows(const char* flag) { g_row_flags.insert(flag); }

bool HaveDataStrings() { return !g_strings.empty(); }

bool ServedFileExists(const wchar_t* name) {
  if (g_dll.empty() || !name) return false;
  const DWORD attributes = GetFileAttributesW((g_dll + L"\\served\\" + name).c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}
