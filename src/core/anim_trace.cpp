// !animtrace [seconds] - a diagnostic (owner, 2026-10-26: walking backwards
// with S while hiding stutters, while the unmodded game's stealth walks back
// like its normal walk - "Check if our alterations really didn't touch the
// original code"). For the player character who types it, for 15 s (3-60), it
// records in memory - nothing is written while it runs:
// - on the client, every animation its models are asked to play
//   (Gob::PlayAnimation(name, speed, int, float), the call that starts a clip
//   on a model); its model is the one the client creature update shows
//   (stealth_ghost.cpp calls AnimTraceClient), and any model that plays an
//   n5_ animation counts as its too;
// - on the server, its animation (CNWSObject +0xd8) and whether it hides, at
//   each AI update; each CNWSCreature::SetAnimation on it and who called it
//   (0.39.8 - the first traces showed the server switching between run (4)
//   and stand (0) while S walked a hider backwards); each drive action the
//   keyboard movement adds for it (CNWSCreature::AddDriveAction: the keys'
//   bits, the two flags after, the direction, and +0x76c - the limited
//   movement speed flag, which makes AddDriveAction drop the bits' 2).
// Then it writes the timeline to animtrace.txt next to nwn5e.log in one go
// (repeats folded: "x12 over 190 ms") and a summary per hiding state to
// nwn5e.log. The hooks pass straight through when no trace runs.

#include <windows.h>

#include <intrin.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <atomic>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "chat.h"
#include "creature.h"
#include "creature_update.h"
#include "config.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"

namespace {

using PlayAnimationFn = int (*)(void* gob, const char* name, float speed, int flag, float value);
using SetAnimationFn = void (*)(void* creature, int animation);
using AddDriveActionFn = int (*)(void* creature, uint16_t group, const float* target, int bits, int a, int b, int c);
PlayAnimationFn g_play = nullptr;
SetAnimationFn g_set_animation = nullptr;
AddDriveActionFn g_add_drive = nullptr;
constexpr const char* kGobPlayAnimation = "?PlayAnimation@Gob@@UEAAHPEBDMHM@Z";
constexpr const char* kAddDriveAction = "?AddDriveAction@CNWSCreature@@QEAAHGAEBVVector@@HHHH@Z";
constexpr size_t kClientObjectAnim = 0x78;  // CNWCObject -> its animation object (stealth_ghost.cpp)
constexpr size_t kAnimModel = 0xc8;         // -> its model (Gob)
constexpr size_t kObjectAnimation = 0xd8;   // uint16: the animation playing (CNWSObject, stance_look.cpp)
constexpr size_t kCreatureLimitSpeed = 0x76c;  // int32: OnApply / OnRemoveLimitMovementSpeed; AddDriveAction reads it
constexpr uint32_t kClientIdBit = 0x80000000u;
constexpr size_t kMaxEvents = 200000;

enum class Kind : uint8_t { kPlay, kServer, kSetAnimation, kDrive };

struct Event {
  Kind kind = Kind::kPlay;
  double ms = 0;
  void* gob = nullptr;  // kPlay: the model
  char name[40] = "";   // kPlay: the animation; kSetAnimation: the caller
  float speed = 0, value = 0, x = 0, y = 0;
  int flag = 0;
  int anim = -1;  // kServer / kSetAnimation: the animation
  int bits = 0, a = 0, b = 0, c = 0, limit = 0;
  bool hiding = false;
};

std::atomic<bool> g_active{false};
std::mutex g_lock;
std::vector<Event> g_events;
std::vector<void*> g_models;  // the traced creature's models seen on the client
uint32_t g_id = 0;            // the traced creature (server id)
bool g_hiding = false;        // as of its last AI update
LARGE_INTEGER g_start{}, g_freq{};
double g_length_ms = 0;
uint64_t g_dropped = 0;

double Now() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return static_cast<double>(t.QuadPart - g_start.QuadPart) * 1000.0 / static_cast<double>(g_freq.QuadPart);
}

void Add(const Event& e) {
  std::lock_guard<std::mutex> hold(g_lock);
  if (g_events.size() < kMaxEvents) {
    g_events.push_back(e);
  } else {
    ++g_dropped;
  }
}

bool Traced(void* creature) {
  return g_active.load(std::memory_order_relaxed) && creature && Field<uint32_t>(creature, ee::kObjectId) == g_id;
}

int PlayAnimation(void* gob, const char* name, float speed, int flag, float value) {
  if (g_active.load(std::memory_order_relaxed) && name) {
    Event e;
    e.ms = Now();
    e.gob = gob;
    strncpy_s(e.name, name, _TRUNCATE);
    e.speed = speed;
    e.flag = flag;
    e.value = value;
    Add(e);
  }
  return g_play(gob, name, speed, flag, value);
}

void SetAnimation(void* creature, int animation) {
  if (Traced(creature)) {
    Event e;
    e.kind = Kind::kSetAnimation;
    e.ms = Now();
    e.anim = animation;
    e.hiding = g_hiding;
    const uintptr_t from = reinterpret_cast<uintptr_t>(_ReturnAddress());
    uintptr_t offset = 0;
    const char* where = ExportContaining(from, &offset);
    if (where) {
      std::string name = where;
      if (name[0] == '?') name = name.substr(1, name.find("@@") == std::string::npos ? 30 : name.find("@@") - 1);
      sprintf_s(e.name, "%s+0x%llx", name.c_str(), static_cast<unsigned long long>(offset));
    } else {
      sprintf_s(e.name, "nwn5e/other %p", reinterpret_cast<void*>(from));
    }
    Add(e);
  }
  g_set_animation(creature, animation);
}

int AddDriveAction(void* creature, uint16_t group, const float* target, int bits, int a, int b, int c) {
  if (Traced(creature)) {
    Event e;
    e.kind = Kind::kDrive;
    e.ms = Now();
    e.bits = bits;
    e.a = a;
    e.b = b;
    e.c = c;
    e.limit = Field<int32_t>(creature, kCreatureLimitSpeed);
    e.x = target ? target[0] : 0;
    e.y = target ? target[1] : 0;
    e.hiding = g_hiding;
    Add(e);
  }
  return g_add_drive(creature, group, target, bits, a, b, c);
}

bool Same(const Event& a, const Event& b) {
  if (a.kind != b.kind) return false;
  switch (a.kind) {
    case Kind::kPlay:
      return a.gob == b.gob && strcmp(a.name, b.name) == 0 && a.speed == b.speed && a.flag == b.flag &&
             a.value == b.value;
    case Kind::kServer:
      return a.anim == b.anim && a.hiding == b.hiding;
    case Kind::kSetAnimation:
      return a.anim == b.anim && strcmp(a.name, b.name) == 0;
    case Kind::kDrive:
      return a.bits == b.bits && a.a == b.a && a.b == b.b && a.c == b.c && a.limit == b.limit;
  }
  return false;
}

std::wstring TraceFilePath() {
  std::wstring path = ConfigPath();
  const size_t slash = path.find_last_of(L"\\/");
  return (slash == std::wstring::npos ? std::wstring() : path.substr(0, slash + 1)) + L"animtrace.txt";
}

void Dump() {
  std::vector<Event> events;
  std::vector<void*> models;
  uint64_t dropped = 0;
  {
    std::lock_guard<std::mutex> hold(g_lock);
    events.swap(g_events);
    models = g_models;
    dropped = g_dropped;
  }
  // Its models: the ones the client creature update showed, and any that played an n5_ animation.
  for (const Event& e : events) {
    if (e.kind == Kind::kPlay && strncmp(e.name, "n5_", 3) == 0 &&
        std::find(models.begin(), models.end(), e.gob) == models.end()) {
      models.push_back(e.gob);
    }
  }
  const auto label = [&](void* gob) -> char {
    const auto it = std::find(models.begin(), models.end(), gob);
    return it == models.end() ? '?' : static_cast<char>('A' + std::min<ptrdiff_t>(it - models.begin(), 25));
  };
  // Its plays carry the hiding state of the server update before them.
  bool hiding = false;
  for (Event& e : events) {
    if (e.kind == Kind::kServer || e.kind == Kind::kSetAnimation || e.kind == Kind::kDrive) {
      if (e.kind == Kind::kServer) hiding = e.hiding;
    } else {
      e.hiding = hiding;
    }
  }
  // The timeline, to animtrace.txt.
  std::string text;
  char line[256];
  sprintf_s(line, "animtrace 0x%08X, %.1f s, %zu events, %llu dropped; models:", g_id, g_length_ms / 1000.0,
            events.size(), static_cast<unsigned long long>(dropped));
  text += line;
  for (void* m : models) {
    sprintf_s(line, " [%c] %p", label(m), m);
    text += line;
  }
  text += "\n";
  const Event* run = nullptr;
  int count = 0;
  double first = 0;
  const auto flush = [&]() {
    if (!run) return;
    char repeat[48] = "";
    if (count > 1) sprintf_s(repeat, " (x%d over %.0f ms)", count, run->ms - first);
    switch (run->kind) {
      case Kind::kServer:
        sprintf_s(line, "%9.1f SERVER animation %d%s%s\n", first, run->anim, run->hiding ? " hiding" : "", repeat);
        break;
      case Kind::kSetAnimation:
        sprintf_s(line, "%9.1f SetAnimation %d from %s%s\n", first, run->anim, run->name, repeat);
        break;
      case Kind::kDrive:
        sprintf_s(line, "%9.1f DRIVE bits 0x%x, %d, %d, %d, limit %d, to (%.2f, %.2f)%s\n", first, run->bits, run->a,
                  run->b, run->c, run->limit, run->x, run->y, repeat);
        break;
      case Kind::kPlay:
        sprintf_s(line, "%9.1f   [%c] play \"%s\" speed %.2f, %d, %.2f%s\n", first, label(run->gob), run->name,
                  run->speed, run->flag, run->value, repeat);
        break;
    }
    text += line;
  };
  for (const Event& e : events) {
    if (e.kind == Kind::kPlay && label(e.gob) == '?') continue;
    if (run && Same(*run, e)) {
      ++count;
      continue;
    }
    flush();
    run = &e;
    count = 1;
    first = e.ms;
  }
  flush();
  // Other models: a summary - the busiest 8.
  std::map<void*, std::map<std::string, int>> others;
  for (const Event& e : events) {
    if (e.kind == Kind::kPlay && label(e.gob) == '?') ++others[e.gob][e.name];
  }
  std::vector<std::pair<int, void*>> busiest;
  for (const auto& o : others) {
    int calls = 0;
    for (const auto& n : o.second) calls += n.second;
    busiest.push_back({calls, o.first});
  }
  std::sort(busiest.rbegin(), busiest.rend());
  sprintf_s(line, "other models: %zu\n", others.size());
  text += line;
  for (size_t i = 0; i < busiest.size() && i < 8; ++i) {
    std::string names;
    for (const auto& n : others[busiest[i].second]) {
      if (names.size() > 160) break;
      names += " " + n.first + " x" + std::to_string(n.second);
    }
    sprintf_s(line, "  %p %d plays:", busiest[i].second, busiest[i].first);
    text += line + names + "\n";
  }
  const std::wstring path = TraceFilePath();
  std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
  file << text;
  const bool written = static_cast<bool>(file);
  file.close();
  // The summary, per hiding state, to nwn5e.log.
  for (int hid = 1; hid >= 0; --hid) {
    std::map<int, int> server;
    std::map<std::string, int> sets, drives, plays;
    for (const Event& e : events) {
      if (e.hiding != (hid == 1)) continue;
      switch (e.kind) {
        case Kind::kServer:
          ++server[e.anim];
          break;
        case Kind::kSetAnimation:
          ++sets[std::to_string(e.anim) + " from " + e.name];
          break;
        case Kind::kDrive:
          ++drives["bits 0x" + [&] { char h[16]; sprintf_s(h, "%x", e.bits); return std::string(h); }() + " " +
                   std::to_string(e.a) + "/" + std::to_string(e.b) + "/" + std::to_string(e.c) + " limit " +
                   std::to_string(e.limit)];
          break;
        case Kind::kPlay:
          if (label(e.gob) == 'A') ++plays[std::string(e.name) + (e.speed < 0 ? " backwards" : "")];
          break;
      }
    }
    std::string s;
    for (const auto& x : server) s += " " + std::to_string(x.first) + " x" + std::to_string(x.second);
    Log("animtrace: %s - server animation:%s", hid ? "hiding" : "not hiding", s.empty() ? " none" : s.c_str());
    for (const auto& x : sets) Log("animtrace:   SetAnimation %s x%d", x.first.c_str(), x.second);
    for (const auto& x : drives) Log("animtrace:   drive %s x%d", x.first.c_str(), x.second);
    std::vector<std::pair<int, std::string>> top;
    for (const auto& x : plays) top.push_back({x.second, x.first});
    std::sort(top.rbegin(), top.rend());
    for (size_t i = 0; i < top.size() && i < 10; ++i) {
      Log("animtrace:   [A] plays \"%s\" x%d", top[i].second.c_str(), top[i].first);
    }
  }
  Log("animtrace: end - the timeline %s animtrace.txt (%zu bytes)", written ? "is in" : "could NOT be written to",
      text.size());
}

// Each creature, each AI update (server).
void ServerStep(void* creature) {
  if (!Traced(creature)) return;
  Event e;
  e.kind = Kind::kServer;
  e.ms = Now();
  e.anim = Field<uint16_t>(creature, kObjectAnimation);
  e.hiding = Field<uint8_t>(creature, ee::kCreatureStealthMode) == 1;
  g_hiding = e.hiding;
  Add(e);
  if (e.ms < g_length_ms) return;
  g_active.store(false);
  Dump();
  TellPlayer(g_id, "nwn5e: !animtrace done - animtrace.txt and nwn5e.log (animtrace: lines)");
}

std::vector<std::string> TraceCommand(void* creature, const std::string& args) {
  if (!g_play) return {"nwn5e: !animtrace isn't available (Gob::PlayAnimation wasn't hooked - see nwn5e.log)"};
  if (g_active.load()) return {"nwn5e: !animtrace is already running"};
  int seconds = atoi(args.c_str());
  if (seconds <= 0) seconds = 15;
  seconds = std::max(3, std::min(60, seconds));
  {
    std::lock_guard<std::mutex> hold(g_lock);
    g_events.clear();
    g_events.reserve(40000);
    g_models.clear();
    g_dropped = 0;
    g_id = Field<uint32_t>(creature, ee::kObjectId);
    g_hiding = Field<uint8_t>(creature, ee::kCreatureStealthMode) == 1;
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_start);
    g_length_ms = seconds * 1000.0;
  }
  g_active.store(true);
  Log("animtrace: started for 0x%08X, %d s", g_id, seconds);
  return {"nwn5e: tracing your animations for " + std::to_string(seconds) +
          " s - walk now (S for backwards); animtrace.txt gets them at the end"};
}

}  // namespace

// stealth_ghost.cpp's client creature update: the traced creature's model.
void AnimTraceClient(void* client_creature) {
  if (!g_active.load(std::memory_order_relaxed) || !client_creature) return;
  if ((Field<uint32_t>(client_creature, ee::kObjectId) & ~kClientIdBit) != g_id) return;
  void* anim = Field<void*>(client_creature, kClientObjectAnim);
  void* model = anim ? Field<void*>(anim, kAnimModel) : nullptr;
  if (!model) return;
  std::lock_guard<std::mutex> hold(g_lock);
  if (std::find(g_models.begin(), g_models.end(), model) == g_models.end()) g_models.push_back(model);
}

void InstallAnimTrace() {
  if (!HookExport("Gob::PlayAnimation (!animtrace)", kGobPlayAnimation, reinterpret_cast<void*>(&PlayAnimation),
                  reinterpret_cast<void**>(&g_play))) {
    g_play = nullptr;
    return;
  }
  HookExport("CNWSCreature::SetAnimation (!animtrace: who sets the traced creature's)", ee::kCreatureSetAnimation,
             reinterpret_cast<void*>(&SetAnimation), reinterpret_cast<void**>(&g_set_animation));
  HookExport("CNWSCreature::AddDriveAction (!animtrace: the keyboard movement's keys)", kAddDriveAction,
             reinterpret_cast<void*>(&AddDriveAction), reinterpret_cast<void**>(&g_add_drive));
  AddCreatureUpdateListener(&ServerStep);
  AddChatCommand("!animtrace", &TraceCommand);
}
