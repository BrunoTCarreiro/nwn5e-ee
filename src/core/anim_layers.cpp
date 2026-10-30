// Stuck weapon arm layers (owner, 2026-10-26: "walking backwards while turning
// still triggers the torso lean and walking motion when stopped issue
// sometimes").
//
// The client plays a held weapon's walk / run arm layer (walk_swordr,
// run_bowl ... - or what it's swapped to, n5_crwalk_swordr ...) on top of the
// walk: Gob::PlayAnimation(layer, 1, 8, 0) starts it, (layer, 1, 128, x) stops
// it, and the stop comes when the walk ends. Turning (pauseturn, cwalkf - the
// turn clips !animtrace showed while S and the mouse turned the character)
// sits between the walk and the idle, and a stop can go missing there: the
// layer goes on over the idle - the torso leaning and swinging as if walking.
// Here, on the client: a creature's model whose base animation has been an idle
// (pause*, nwn5e's n5_*idle) for 300 ms while one of those layers, started 300
// ms ago or more, was never stopped, gets the stop the game would have sent.
// Logged. `stuck_layer_fix`.
//
// Gob::PlayAnimation is hooked here (one hook per function): !animtrace
// records through it too (the dev layer's, through g_dev_seam).
//
// A player's Shove swings "nwstab" (0.45.5; owner: "for shove the PC should use
// the nwstab animation for now") - or the owner's captured n5_shove once built
// (Mocap mode, 0.51.0): the server notes a Shove as it resolves it
// (rules_shove.cpp: ShoveSwing), the client creature update ties that to the
// creature's model, and the model's next melee swing (<hold> slash / stab /
// close / reach / cleave - not a parry, a walk or a layer) within 1.5 s plays
// nwstab, the unarmed thrust, instead.

#include <windows.h>

#include <stdint.h>
#include <string.h>

#include <atomic>

#include <string>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "dev_seam.h"

bool AnimationRecipeBuilt(const std::string& name, std::string* text);              // anim_packs.cpp

namespace {

using PlayAnimationFn = int (*)(void* gob, const char* name, float speed, int flag, float value);
PlayAnimationFn g_play = nullptr;
constexpr const char* kGobPlayAnimation = "?PlayAnimation@Gob@@UEAAHPEBDMHM@Z";
constexpr size_t kClientObjectAnim = 0x78;  // CNWCObject -> its animation object (stealth_ghost.cpp)
constexpr size_t kAnimModel = 0xc8;         // -> its model (Gob)
constexpr uint32_t kClientIdBit = 0x80000000u;
constexpr int kLayerStart = 8, kLayerStop = 128;
constexpr uint64_t kStuckMs = 300;
constexpr size_t kMaxModels = 512;
constexpr int kMaxLogged = 40;

struct Layers {
  std::string base;         // its last base animation
  uint64_t base_since = 0;  // since when (ms)
  std::unordered_map<std::string, uint64_t> playing;  // arm layers started and not stopped, and when (ms)
};
std::unordered_map<void*, Layers> g_models;  // creatures' models seen by the client creature update
SRWLOCK g_lock = SRWLOCK_INIT;
int g_logged = 0;

constexpr uint64_t kShoveSwingMs = 1500;
constexpr const char* kShoveSwing = "nwstab";
constexpr const char* kCapturedShove = "n5_shove";
std::unordered_map<uint32_t, uint64_t> g_shove_ids;     // server creature id -> until (ms): a Shove just resolved
std::unordered_map<void*, uint64_t> g_shove_models;     // its model -> until: the next swing is the Shove's
int g_shove_logged = 0;
std::atomic<bool> g_shove_active{false};  // either map not empty (read without the lock)

// A melee swing: <hold><move> - 1hslashl, 2hstab, nwcloseh, dwreach ... (not a parry or a dodge).
bool IsMeleeSwing(const char* name) {
  const size_t n = strlen(name);
  if (n < 6 || n > 12 || strstr(name, "parry") || strstr(name, "dodge")) return false;
  for (const char* move : {"slash", "stab", "close", "reach", "cleave", "smash"}) {
    if (strstr(name + 2, move)) return true;
  }
  return false;
}

bool EndsWith(const char* s, const char* end) {
  const size_t n = strlen(s), m = strlen(end);
  return n >= m && strcmp(s + n - m, end) == 0;
}

// A walk / run weapon arm layer: walk_<hold>, run_<hold>, or nwn5e's swap for one (n5_crwalk_<hold> ...;
// a swap name is cut at 16 characters: n5_crwalk_shield).
bool IsArmLayer(const char* name) {
  const bool gait = strncmp(name, "walk_", 5) == 0 || strncmp(name, "run_", 4) == 0 ||
                    (strncmp(name, "n5_", 3) == 0 && strstr(name, "walk_") != nullptr);
  if (!gait) return false;
  for (const char* hold : {"_swordr", "_swordl", "_shieldl", "_shield", "_bowl"}) {
    if (EndsWith(name, hold)) return true;
  }
  return false;
}

// An idle: the game's pause animations (not the turn) or nwn5e's idles (n5_cridle ...).
bool IsIdle(const std::string& base) {
  if (base.compare(0, 5, "pause") == 0) return base.find("turn") == std::string::npos;
  return base.compare(0, 3, "n5_") == 0 && base.find("idle") != std::string::npos;
}

void Note(void* gob, const char* name, int flag) {
  AcquireSRWLockExclusive(&g_lock);
  const auto it = g_models.find(gob);
  if (it != g_models.end()) {
    Layers& layers = it->second;
    const uint64_t now = GetTickCount64();
    if (IsArmLayer(name)) {
      if (flag & kLayerStop) {
        layers.playing.erase(name);
      } else if (flag & kLayerStart) {
        layers.playing.emplace(name, now);
      }
    } else if (layers.base != name) {
      layers.base = name;
      layers.base_since = now;
    }
  }
  ReleaseSRWLockExclusive(&g_lock);
}

int PlayAnimation(void* gob, const char* name, float speed, int flag, float value) {
  if (name && g_shove_active.load(std::memory_order_relaxed) && (flag & (kLayerStart | kLayerStop)) == 0 &&
      IsMeleeSwing(name)) {
    bool shove = false;
    AcquireSRWLockExclusive(&g_lock);
    const auto it = g_shove_models.find(gob);
    if (it != g_shove_models.end()) {
      shove = GetTickCount64() <= it->second;
      g_shove_models.erase(it);
    }
    g_shove_active = !g_shove_ids.empty() || !g_shove_models.empty();
    ReleaseSRWLockExclusive(&g_lock);
    if (shove) {
      // The packs are built at the start, before any Shove: asked once.
      static const bool captured = AnimationRecipeBuilt(kCapturedShove, nullptr);
      const char* swing = captured ? kCapturedShove : kShoveSwing;
      if (g_shove_logged < kMaxLogged) {
        ++g_shove_logged;
        Log("look: a Shove's swing %s played as %s", name, swing);
      }
      name = swing;
    }
  }
  if (name) {
    if (g_dev_seam.anim_play) g_dev_seam.anim_play(gob, name, speed, flag, value);  // !animtrace (dev layer)
    if (g_config.stuck_layer_fix) Note(gob, name, flag);
  }
  return g_play(gob, name, speed, flag, value);
}

}  // namespace

bool AnimLayersHooked() { return g_play != nullptr; }

// rules_shove.cpp: a player's Shove resolved - its next swing is nwstab (see the top).
void ShoveSwing(uint32_t server_id) {
  if (!g_play) return;
  AcquireSRWLockExclusive(&g_lock);
  if (g_shove_ids.size() > 64) g_shove_ids.clear();
  if (g_shove_models.size() > 64) g_shove_models.clear();
  g_shove_ids[server_id] = GetTickCount64() + kShoveSwingMs;
  g_shove_active = true;
  ReleaseSRWLockExclusive(&g_lock);
}

// stealth_ghost.cpp's client creature update, each creature each frame: its model is followed, and a stuck
// arm layer over an idle stopped (see the top).
void AnimLayersClient(void* client_creature) {
  if (!g_play || !client_creature) return;
  void* anim = Field<void*>(client_creature, kClientObjectAnim);
  void* model = anim ? Field<void*>(anim, kAnimModel) : nullptr;
  if (!model) return;
  if (g_shove_active.load(std::memory_order_relaxed)) {  // a Shove resolved for it: its model's next swing is the Shove's
    const uint32_t id = Field<uint32_t>(client_creature, ee::kObjectId) & ~kClientIdBit;
    AcquireSRWLockExclusive(&g_lock);
    const auto shove = g_shove_ids.find(id);
    if (shove != g_shove_ids.end()) {
      if (GetTickCount64() <= shove->second) g_shove_models[model] = shove->second;
      g_shove_ids.erase(shove);
    }
    g_shove_active = !g_shove_ids.empty() || !g_shove_models.empty();
    ReleaseSRWLockExclusive(&g_lock);
  }
  if (!g_config.stuck_layer_fix) return;
  std::vector<std::string> stuck;
  std::string idle;
  uint64_t idle_ms = 0;
  AcquireSRWLockExclusive(&g_lock);
  auto it = g_models.find(model);
  if (it == g_models.end()) {
    if (g_models.size() >= kMaxModels) g_models.clear();  // models of creatures gone, now and then
    g_models.emplace(model, Layers{});
  } else if (!it->second.playing.empty() && IsIdle(it->second.base)) {
    const uint64_t now = GetTickCount64();
    Layers& layers = it->second;
    if (now - layers.base_since >= kStuckMs) {
      for (auto p = layers.playing.begin(); p != layers.playing.end();) {
        if (now - p->second >= kStuckMs) {
          stuck.push_back(p->first);
          p = layers.playing.erase(p);
        } else {
          ++p;
        }
      }
      idle = layers.base;
      idle_ms = now - layers.base_since;
    }
  }
  ReleaseSRWLockExclusive(&g_lock);
  for (const std::string& layer : stuck) {
    if (g_dev_seam.anim_play) g_dev_seam.anim_play(model, layer.c_str(), 1.0f, kLayerStop, 0.0f);
    g_play(model, layer.c_str(), 1.0f, kLayerStop, 0.0f);
    if (g_logged < kMaxLogged) {
      ++g_logged;
      Log("look: 0x%08X a stuck arm layer stopped - %s over the idle %s (%llu ms)",
          Field<uint32_t>(client_creature, ee::kObjectId) & ~kClientIdBit, layer.c_str(), idle.c_str(),
          static_cast<unsigned long long>(idle_ms));
    }
  }
}

void InstallAnimLayers() {
  if (!HookExport("Gob::PlayAnimation (stuck arm layers; the dev layer's trace)", kGobPlayAnimation,
                  reinterpret_cast<void*>(&PlayAnimation), reinterpret_cast<void**>(&g_play))) {
    g_play = nullptr;
    return;
  }
  if (g_config.stuck_layer_fix) {
    Log("look: a weapon's walk / run arm layer left over an idle for %llu ms is stopped (stuck_layer_fix)",
        static_cast<unsigned long long>(kStuckMs));
  }
}
