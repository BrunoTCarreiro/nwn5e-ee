// The client's own smoothing of a creature nwn5e moves (client_smoothing.h;
// #11, 0.58.7). Read 89.8193.37 (RVAs):
// - CNWCCreature's vtable slot 37 (0x80AD90) sets the client creature's
//   position: it copies the vector its second argument points at to +0x38,
//   moves its light, checks its area, and through the animation object puts
//   the model there (Gob::SetPosition). Every client move goes through it:
//   HandleServerToPlayerCreatureUpdate_Update's snap (a jump of 4 m or more,
//   or the first placement), the per-frame chase toward the last network
//   position (+0x2C0, over 512 ms, sub_804840 - it calls the setter every
//   frame, also once arrived) and the path walker.
// - So one hook: for a creature nwn5e is moving, the vector is copied and its
//   x / y replaced by nwn5e's point (the height stays the engine's), then the
//   setter runs with the copy - the caller's own vector (maybe the network
//   target itself) untouched.
// - The client creature's id is the server's with bit 31 set (+0x8).
// - Timed by the performance counter, not GetTickCount (its 15.6 ms steps
//   would stutter at a high frame rate).

#include "client_smoothing.h"

#include <windows.h>

#include <math.h>
#include <stdint.h>

#include <atomic>
#include <mutex>
#include <unordered_map>

#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "rules/smoothing.h"

namespace {

constexpr const char* kRttiClientCreature = ".?AVCNWCCreature@@";
constexpr int kSetPositionSlot = 37;
constexpr uint8_t kSetPositionEntry[] = {0x48, 0x89, 0x74, 0x24, 0x18, 0x57, 0x48,
                                         0x83, 0xec, 0x30, 0x48, 0x8b, 0xf2};
constexpr size_t kClientNetworkTarget = 0x2c0;  // CNWCCreature: the last network position (x, y, z)
constexpr uint32_t kClientIdBit = 0x80000000u;
constexpr int kLogLines = 40;

using SetPositionFn = void (*)(void* creature, const float* position);
SetPositionFn g_set_position = nullptr;
bool g_on = false;

enum class Kind { kSlide, kFollow };
struct Entry {
  Kind kind = Kind::kSlide;
  rules::SmoothPoint from, to;
  double start_ms = 0.0;  // ClockMs at its start
  double ms = 0.0;        // the slide's length (0 for a hold to hand back)
  int frames = 0;
};

std::mutex g_lock;
std::unordered_map<uint32_t, Entry> g_entries;  // by server id
std::atomic<int> g_count{0};
int g_log_lines = 0;

double ClockMs() {
  static const double frequency = [] {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return static_cast<double>(f.QuadPart);
  }();
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return static_cast<double>(now.QuadPart) * 1000.0 / frequency;
}

bool LogSlot() { return g_log_lines < kLogLines && ++g_log_lines; }

void Put(uint32_t id, const Entry& entry) {
  std::lock_guard<std::mutex> hold(g_lock);
  g_entries[id] = entry;
  g_count.store(static_cast<int>(g_entries.size()));
}

void SetPosition(void* creature, const float* position) {
  if (g_count.load(std::memory_order_relaxed) == 0 || !creature || !position) {
    g_set_position(creature, position);
    return;
  }
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId) & ~kClientIdBit;
  float moved[3] = {position[0], position[1], position[2]};
  bool substitute = false;
  {
    std::lock_guard<std::mutex> hold(g_lock);
    const auto it = g_entries.find(id);
    if (it != g_entries.end()) {
      Entry& e = it->second;
      const double elapsed = ClockMs() - e.start_ms;
      if (e.kind == Kind::kFollow) {
        if (void* server = CreatureById(id)) {
          moved[0] = Field<float>(server, ee::kObjectPositionX);
          moved[1] = Field<float>(server, ee::kObjectPositionY);
          substitute = true;
        } else {
          g_entries.erase(it);
        }
      } else {
        const rules::SmoothPoint target{Field<float>(creature, kClientNetworkTarget),
                                        Field<float>(creature, kClientNetworkTarget + 4)};
        if (rules::SmoothHandBack(e.to, target, elapsed, e.ms)) {
          if (LogSlot()) {
            Log("smoothing: 0x%08X handed back to the client after %.0f ms (%d frames; its target %.2f m from the "
                "end)",
                id, elapsed, e.frames,
                sqrtf((target.x - e.to.x) * (target.x - e.to.x) + (target.y - e.to.y) * (target.y - e.to.y)));
          }
          g_entries.erase(it);
        } else {
          const rules::SmoothPoint p = rules::SlidePoint(e.from, e.to, elapsed, e.ms);
          moved[0] = p.x;
          moved[1] = p.y;
          substitute = true;
          ++e.frames;
        }
      }
      g_count.store(static_cast<int>(g_entries.size()));
    }
  }
  g_set_position(creature, substitute ? moved : position);
}

}  // namespace

void SmoothSlide(uint32_t id, float from_x, float from_y, float to_x, float to_y, uint32_t ms) {
  if (!g_on || !id) return;
  Entry e;
  e.kind = Kind::kSlide;
  e.from = rules::SmoothPoint{from_x, from_y};
  e.to = rules::SmoothPoint{to_x, to_y};
  e.start_ms = ClockMs();
  e.ms = static_cast<double>(ms);
  Put(id, e);
  if (LogSlot()) {
    Log("smoothing: 0x%08X slides on the client too - %.1f m over %u ms", id,
        sqrtf((to_x - from_x) * (to_x - from_x) + (to_y - from_y) * (to_y - from_y)), ms);
  }
}

void SmoothFollow(uint32_t id) {
  if (!g_on || !id) return;
  Entry e;
  e.kind = Kind::kFollow;
  e.start_ms = ClockMs();
  Put(id, e);
  if (LogSlot()) Log("smoothing: 0x%08X held - its model follows the server's position each frame", id);
}

void SmoothStop(uint32_t id) {
  if (!g_on || !id) return;
  void* server = CreatureById(id);
  std::lock_guard<std::mutex> hold(g_lock);
  const auto it = g_entries.find(id);
  if (it == g_entries.end()) return;
  if (!server) {
    g_entries.erase(it);
  } else {
    Entry& e = it->second;  // held where it is now, handed back once the client catches up
    e.kind = Kind::kSlide;
    e.to = rules::SmoothPoint{Field<float>(server, ee::kObjectPositionX), Field<float>(server, ee::kObjectPositionY)};
    e.from = e.to;
    e.start_ms = ClockMs();
    e.ms = 0.0;
  }
  g_count.store(static_cast<int>(g_entries.size()));
}

void InstallClientSmoothing() {
  const uintptr_t set_position =
      VtableSlot(FindVtable("client creature: its vtable (smoothing)", kRttiClientCreature), kSetPositionSlot);
  if (!set_position || FunctionEntry(set_position) != set_position ||
      !ContainsBytes(set_position, set_position + sizeof(kSetPositionEntry), kSetPositionEntry,
                     sizeof(kSetPositionEntry))) {
    Log("SKIP smoothing: the client creature's position setter wasn't found - the client's own smoothing (a long "
        "slide snaps)");
    return;
  }
  g_on = HookAddress("client creature position setter (nwn5e's slides and drags smoothed)", set_position,
                     reinterpret_cast<void*>(&SetPosition), reinterpret_cast<void**>(&g_set_position));
  if (g_on) Log("smoothing: the client draws nwn5e's slides and drags itself, every frame (single player)");
}
