// The look player: see anim_look_player.h. The timing and the script text are data::'s (src/data/anim_look, tested);
// this file loads the looks, remembers when each caster's cast began, and runs the chunks.

#include "anim_look_player.h"

#include <windows.h>

#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "anim_cast_clip.h"
#include "creature.h"
#include "data/anim_look.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "game_clock.h"
#include "log.h"
#include "spell_cast_listeners.h"

namespace {

constexpr int64_t kRepeatMs = 500;        // a caster that started a look this recently isn't started again (a replay's fake cast)
constexpr int64_t kCastMemoryMs = 15000;  // a cast start older than this isn't this impact's
constexpr float kAtLeast = 0.01f;         // a look that played answers more than 0 (0 = no look)

std::vector<data::SpellLook> g_looks;
std::unordered_map<int, size_t> g_by_row;              // spells.2da row -> g_looks
std::unordered_map<uint32_t, int64_t> g_cast_started;  // caster id -> game ms its look's cast began
bool g_installed = false;

int64_t Now() { return static_cast<int64_t>(GameMs()); }

// File helpers as anim_packs.cpp's (file-local there).
std::wstring DllDirectory() {
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&DllDirectory), &module);
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(module, path, MAX_PATH);
  std::wstring directory = path;
  return directory.substr(0, directory.find_last_of(L"\\/"));
}

std::string Narrow(const std::wstring& text) {
  std::string out;
  for (wchar_t c : text) out += c < 128 ? static_cast<char>(c) : '?';
  return out;
}

bool ReadFile(const std::wstring& path, std::string* text) {
  std::ifstream file(path.c_str(), std::ios::binary);
  if (!file) return false;
  std::stringstream buffer;
  buffer << file.rdbuf();
  *text = buffer.str();
  return true;
}

std::vector<std::wstring> LookFiles(const std::wstring& directory) {  // *.look, by the exact suffix (not spells.look.txt)
  std::vector<std::wstring> names;
  WIN32_FIND_DATAW found;
  HANDLE handle = FindFirstFileW((directory + L"\\*.look").c_str(), &found);
  if (handle == INVALID_HANDLE_VALUE) return names;
  do {
    const std::wstring name = found.cFileName;
    if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && name.size() > 5 &&
        _wcsicmp(name.c_str() + name.size() - 5, L".look") == 0) {
      names.push_back(name);
    }
  } while (FindNextFileW(handle, &found));
  FindClose(handle);
  std::sort(names.begin(), names.end());
  return names;
}

uint64_t WriteTime(const std::wstring& path) {
  WIN32_FILE_ATTRIBUTE_DATA data;
  if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return 0;
  return (static_cast<uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
}

// The looks, by name: the shipped ones, then the bench's over them; within a folder the newest file with a name wins.
void ReadLooks() {
  std::map<std::string, data::SpellLook> by_name;
  for (const std::wstring& directory : {DllDirectory() + L"\\data\\looks", DllDirectory() + L"\\spellbench\\looks"}) {
    std::map<std::string, uint64_t> newest;
    for (const std::wstring& file : LookFiles(directory)) {
      std::string text, error;
      data::SpellLook look;
      if (!ReadFile(directory + L"\\" + file, &text) || !data::ReadSpellLook(text, &look, &error)) {
        Log("look: SKIP %s: %s", Narrow(file).c_str(), error.c_str());
        continue;
      }
      const uint64_t written = WriteTime(directory + L"\\" + file);
      const auto seen = newest.find(look.name);
      if (seen != newest.end() && seen->second > written) continue;
      newest[look.name] = written;
      by_name[look.name] = look;
    }
  }
  g_looks.clear();
  g_by_row.clear();
  std::string names;
  for (const auto& named : by_name) {
    g_looks.push_back(named.second);
    names += (names.empty() ? "" : ", ") + named.first;
    for (int row : named.second.rows) {
      const auto taken = g_by_row.find(row);
      if (taken != g_by_row.end()) {
        Log("look: row %d is in both %s and %s - %s plays", row, g_looks[taken->second].name.c_str(),
            named.first.c_str(), named.first.c_str());
      }
      g_by_row[row] = g_looks.size() - 1;
    }
  }
  Log("look: %d spell looks loaded (%s)", static_cast<int>(g_looks.size()), names.empty() ? "none" : names.c_str());
}

const data::SpellLook* LookFor(int row) {
  const auto found = g_by_row.find(row);
  return found == g_by_row.end() ? nullptr : &g_looks[found->second];
}

void LookCastBroadcast(void* creature, uint32_t spell) {
  const data::SpellLook* look = LookFor(static_cast<int>(spell));
  if (!look || !creature) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  const auto started = g_cast_started.find(id);
  if (started != g_cast_started.end() && Now() - started->second < kRepeatMs) return;
  StartLookCast(id, *look, static_cast<int>(spell));
}

// The metres from caster to target, through the script (GetDistanceBetween); -1 when the chunk can't run.
float Metres(uint32_t caster, uint32_t target) {
  if (RunScriptChunk(data::LookDistanceScript(target).c_str(), caster) == -1) return -1.0f;
  void* object = CreatureById(caster);
  return object ? static_cast<float>(atof(engine::GetLocalString(object, "n5look_d").c_str())) : 0.0f;
}

}  // namespace

void StartLookCast(uint32_t caster, const data::SpellLook& look, int row) {
  const int64_t now = Now();
  if (g_cast_started.size() > 64) {  // forget casts long past
    for (auto it = g_cast_started.begin(); it != g_cast_started.end();) {
      it = now - it->second > kCastMemoryMs ? g_cast_started.erase(it) : std::next(it);
    }
  }
  g_cast_started[caster] = now;
  void* creature = CreatureById(caster);
  if (creature && !look.clip.empty()) RequestCastClip(creature, look.clip_base.c_str(), look.clip, look.clip_seconds);
  const std::string code = data::LookCastScript(data::LookPartsFor(look, row));
  if (!code.empty()) RunScriptChunk(code.c_str(), caster);
  Log("look: 0x%08X %s row %d: the cast begins (clip %s)", caster, look.name.c_str(), row,
      look.clip.empty() ? "none" : look.clip.c_str());
}

float PlayLookHit(uint32_t caster, uint32_t target, const data::SpellLook& look, int row) {
  float since = look.rule_at;
  bool seen = false;
  const auto started = g_cast_started.find(caster);
  if (started != g_cast_started.end() && Now() - started->second < kCastMemoryMs) {
    since = static_cast<float>(Now() - started->second) / 1000.0f;
    seen = true;
  }
  const data::LookParts& parts = data::LookPartsFor(look, row);
  const float metres = Metres(caster, target);
  if (metres < 0.0f) {
    Log("look: 0x%08X %s row %d: the script bridge isn't there - nothing played", caster, look.name.c_str(), row);
    return 0.0f;
  }
  const data::LookTiming timing = data::LookHitTiming(look, parts, since, metres);
  RunScriptChunk(data::LookHitScript(parts, target, timing).c_str(), caster);
  Log("look: 0x%08X %s row %d: the rule ran %.2f s after the cast began%s (the look expects %.2f); the missile leaves "
      "in %.2f s and lands in %.2f s (%.1f m)%s",
      caster, look.name.c_str(), row, since, seen ? "" : " (no cast seen: assumed)", look.rule_at, timing.release_in,
      timing.hit_in, metres, timing.late ? " - late" : "");
  return std::max(kAtLeast, timing.hit_in);
}

float PlayLookLeap(uint32_t from, uint32_t to, const data::SpellLook& look, int row, float after) {
  const data::LookParts& parts = data::LookPartsFor(look, row);
  const float lands = std::max(0.0f, after) + look.leap_after;
  if (RunScriptChunk(data::LookLeapScript(parts, look.leap_for, from, to, after, lands).c_str(), from) == -1) {
    Log("look: 0x%08X %s row %d: the script bridge isn't there - no leap played", from, look.name.c_str(), row);
    return 0.0f;
  }
  Log("look: 0x%08X %s row %d leaps to 0x%08X, landing in %.2f s", from, look.name.c_str(), row, to, lands);
  return std::max(kAtLeast, lands);
}

float PlaySpellLook(uint32_t caster, uint32_t target, int row) {
  const data::SpellLook* look = LookFor(row);
  return look ? PlayLookHit(caster, target, *look, row) : 0.0f;
}

float PlaySpellLookLeap(uint32_t from, uint32_t to, int row, float after) {
  const data::SpellLook* look = LookFor(row);
  return look ? PlayLookLeap(from, to, *look, row, after) : 0.0f;
}

void InstallLookPlayer() {
  if (g_installed) return;
  g_installed = true;
  ReadLooks();
  if (!g_looks.empty()) AddSpellCastListener(&LookCastBroadcast);  // logs itself when it can't
}
