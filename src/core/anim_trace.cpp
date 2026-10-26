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
//   each AI update.
// Then it writes them to nwn5e.log in time order, repeats folded ("x12 over
// 190 ms"), other models as a summary. The hook passes straight through when
// no trace runs.

#include <windows.h>

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "chat.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"

namespace {

using PlayAnimationFn = int (*)(void* gob, const char* name, float speed, int flag, float value);
PlayAnimationFn g_play = nullptr;
constexpr const char* kGobPlayAnimation = "?PlayAnimation@Gob@@UEAAHPEBDMHM@Z";
constexpr size_t kClientObjectAnim = 0x78;  // CNWCObject -> its animation object (stealth_ghost.cpp)
constexpr size_t kAnimModel = 0xc8;         // -> its model (Gob)
constexpr size_t kObjectAnimation = 0xd8;   // uint16: the animation playing (CNWSObject, stance_look.cpp)
constexpr uint32_t kClientIdBit = 0x80000000u;
constexpr size_t kMaxEvents = 80000;
constexpr int kMaxLines = 4000;

struct Event {
  double ms = 0;
  void* gob = nullptr;  // client: the model; server: nullptr
  char name[32] = "";   // client: the animation
  float speed = 0, value = 0;
  int flag = 0;
  int server_anim = -1;  // server: the animation id; client: -1
  bool hiding = false;
};

std::atomic<bool> g_active{false};
std::mutex g_lock;
std::vector<Event> g_events;
std::vector<void*> g_models;  // the traced creature's models seen on the client
uint32_t g_id = 0;            // the traced creature (server id)
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

bool Same(const Event& a, const Event& b) {
  if (a.server_anim >= 0 || b.server_anim >= 0) {
    return a.server_anim == b.server_anim && a.hiding == b.hiding && a.gob == b.gob;
  }
  return a.gob == b.gob && strcmp(a.name, b.name) == 0 && a.speed == b.speed && a.flag == b.flag &&
         a.value == b.value;
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
    if (e.gob && strncmp(e.name, "n5_", 3) == 0 && std::find(models.begin(), models.end(), e.gob) == models.end()) {
      models.push_back(e.gob);
    }
  }
  const auto label = [&](void* gob) -> char {
    const auto it = std::find(models.begin(), models.end(), gob);
    return it == models.end() ? '?' : static_cast<char>('A' + std::min<ptrdiff_t>(it - models.begin(), 25));
  };
  size_t ours = 0;
  for (const Event& e : events) ours += (!e.gob || label(e.gob) != '?') ? 1 : 0;
  Log("animtrace: 0x%08X, %.1f s - %zu of its events (client plays, server updates), %zu other client plays, %llu "
      "dropped; repeats folded",
      g_id, g_length_ms / 1000.0, ours, events.size() - ours, static_cast<unsigned long long>(dropped));
  for (size_t i = 0; i < models.size(); ++i) Log("animtrace: model [%c] = %p", label(models[i]), models[i]);
  int lines = 0;
  const Event* run = nullptr;
  int count = 0;
  double first = 0;
  const auto flush = [&]() {
    if (!run) return;
    char repeat[48] = "";
    if (count > 1) sprintf_s(repeat, " (x%d over %.0f ms)", count, run->ms - first);
    if (run->server_anim >= 0) {
      Log("animtrace: %8.1f server animation %d%s%s", first, run->server_anim, run->hiding ? " hiding" : "", repeat);
    } else {
      Log("animtrace: %8.1f [%c] play \"%s\" speed %.2f, %d, %.2f%s", first, label(run->gob), run->name, run->speed,
          run->flag, run->value, repeat);
    }
    ++lines;
  };
  for (const Event& e : events) {
    if (e.gob && label(e.gob) == '?') continue;
    if (run && Same(*run, e)) {
      ++count;
      continue;
    }
    flush();
    if (lines >= kMaxLines) {
      run = nullptr;
      Log("animtrace: ... cut at %d lines", kMaxLines);
      break;
    }
    run = &e;
    count = 1;
    first = e.ms;
  }
  if (lines < kMaxLines) flush();
  // Other models: a summary - the busiest 8, with what they played.
  std::map<void*, std::map<std::string, int>> others;
  for (const Event& e : events) {
    if (e.gob && label(e.gob) == '?') ++others[e.gob][e.name];
  }
  std::vector<std::pair<int, void*>> busiest;
  for (const auto& o : others) {
    int calls = 0;
    for (const auto& n : o.second) calls += n.second;
    busiest.push_back({calls, o.first});
  }
  std::sort(busiest.rbegin(), busiest.rend());
  Log("animtrace: other models: %zu", others.size());
  for (size_t i = 0; i < busiest.size() && i < 8; ++i) {
    std::string names;
    for (const auto& n : others[busiest[i].second]) {
      if (names.size() > 160) break;
      names += " " + n.first + " x" + std::to_string(n.second);
    }
    Log("animtrace:   %p %d plays:%s", busiest[i].second, busiest[i].first, names.c_str());
  }
  Log("animtrace: end");
}

// Each creature, each AI update (server).
void ServerStep(void* creature) {
  if (!g_active.load(std::memory_order_relaxed) || Field<uint32_t>(creature, ee::kObjectId) != g_id) return;
  Event e;
  e.ms = Now();
  e.server_anim = Field<uint16_t>(creature, kObjectAnimation);
  e.hiding = Field<uint8_t>(creature, ee::kCreatureStealthMode) == 1;
  Add(e);
  if (e.ms < g_length_ms) return;
  g_active.store(false);
  Dump();
  TellPlayer(g_id, "nwn5e: !animtrace done - written to nwn5e.log (animtrace: lines)");
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
    g_events.reserve(20000);
    g_models.clear();
    g_dropped = 0;
    g_id = Field<uint32_t>(creature, ee::kObjectId);
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_start);
    g_length_ms = seconds * 1000.0;
  }
  g_active.store(true);
  Log("animtrace: started for 0x%08X, %d s", g_id, seconds);
  return {"nwn5e: tracing your animations for " + std::to_string(seconds) +
          " s - walk now (S for backwards); nwn5e.log gets them at the end"};
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
  AddCreatureUpdateListener(&ServerStep);
  AddChatCommand("!animtrace", &TraceCommand);
}
