// The hiding character's see-through look by the light (owner, 2026-10-25:
// "change the stealth vfx, make it dependent on the light level, 90% visible on
// light, 70% in dim and 50% in dark" - "do not make the ghostly effect pulse,
// have it maintain the transparency level"; RB1: "rogue abilities that give
// extra stealth bonus should stack on top of the normal stealth and affect ...
// the character transparency"). While a character hides, stealth_watch.cpp puts
// EE's steady ghost effect on it (VFX_DUR_GHOST_TRANSPARENT, 424: programmatic
// effect 420 - not the pulsing 401 / 404) and tells this file, by object id, the
// opacity its light and Stealth bonus call for (rules::StealthOpacity).
//
// How the client draws a creature see-through (read from nwmain, 89.8193.37): its
// model (client object +0x78 -> animation object +0xc8) has an alpha (Gob
// +0x108), set by Gob::SetColorShifting(colour, alpha, propagate) - each part of
// the model copies its parent's (PropagateColorShifting) - and the animation
// object's vtable slot 29 (0x966a40: alpha, 0xff) sets it with the right colour.
// Two things hold a hider at its opacity:
// - a hook on Gob::SetColorShifting: a see-through request (alpha under 1) for a
//   hider's own model gets its opacity instead (0.33.0 did this for every model
//   while anyone hid - creatures fading in and other see-through models too);
// - each frame, after a client creature's update (CNWCCreature vtable slot 29,
//   entry bytes checked), a hider whose model isn't at its opacity is set to it
//   through slot 29 (the ghost's own request may come before the model is known,
//   and 0.33.1's routine - the effect alpha picker at 0x763420 - turned out not
//   to be on the ghost's path: in play nothing reached it). When it stops hiding
//   and nothing else has changed the alpha since, it is set back to solid.
// `stealth_ghost` (2024 perception, stealth_light).
//
// The outline (0.34.0; owner: "there might be an issue with the transparency
// levels where the player might not see their own character in high levels of
// stealth. Is it possible to add any kind of outline to characters?" - "go with
// A"): a hider gets EE's own highlight glow, the one the mouse-over and Tab draw
// round a silhouette, in a pale white-cyan. The scene keeps a list of
// highlighted models and colours (Scene::AddHighlightGob / RemoveHighlightGob,
// vtable slots 35 / 36, checked against their exports); the engine reaches the
// scene as client app (*g_pAppManager) +0x8 -> +0x30 -> its area +0xf8 ->
// +0x248, checking the model with its own vtable slot 67 first - all as its
// highlight code (0x966170) does. Each frame a hiding creature's model is put
// on the list when it isn't on it (the mouse-over takes it off when the pointer
// leaves; a hover's colour shows while hovering), and taken off when it stops
// hiding. A model or scene seen before is only compared, never followed: after
// a new area the old ones are just forgotten. `stealth_outline`.
//
// Ids (0.35.0): the client knows an object by the server's id with bit 31 set
// (CNWSMessage::WriteOBJECTIDServer ORs 0x80000000 into every id but
// OBJECT_INVALID; ReadOBJECTIDServer clears it) - the player character
// 0x7FFFFFFF is 0xFFFFFFFF there. 0.34.0 compared the client's id with the
// server's, so no hider was ever matched (its outline never showed).

#include <windows.h>

#include <intrin.h>
#include <math.h>
#include <stdint.h>

#include <atomic>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"

namespace {

// --- What the server side registers: who hides now, and how see-through.
struct Hider {
  std::atomic<uint32_t> id{0};
  std::atomic<float> opacity{0.0f};
};
constexpr int kHiders = 8;
Hider g_hiders[kHiders];

float OpacityOf(uint32_t id) {
  if (!id) return 0.0f;
  for (Hider& h : g_hiders) {
    if (h.id.load() == id) return h.opacity.load();
  }
  return 0.0f;
}

bool AnyoneHides() {
  for (const Hider& h : g_hiders) {
    if (h.id.load() != 0) return true;
  }
  return false;
}

// --- The client side.
using ColorShiftFn = void (*)(void* gob, const float* colour, float alpha, int propagate);
using SetAlphaFn = int (*)(void* anim, float alpha, uint8_t parts);
using CreatureUpdateFn = int64_t (*)(void* creature);
using AddHighlightFn = void (*)(void* scene, void* model, const float* colour);
using RemoveHighlightFn = void (*)(void* scene, void* model);
using ModelReadyFn = void* (*)(void* model);

ColorShiftFn g_color_shift = nullptr;
CreatureUpdateFn g_creature_update = nullptr;
uintptr_t g_set_alpha = 0;  // the animation object's slot 29
uintptr_t g_add_export = 0, g_remove_export = 0;

constexpr const char* kColorShifting = "?SetColorShifting@Gob@@UEAAXVVector@@MH@Z";
constexpr const char* kRttiClientCreature = ".?AVCNWCCreature@@";
constexpr const char* kRttiAnimBase = ".?AVCNWCAnimBase@@";
constexpr int kCreatureUpdateSlot = 29;
constexpr int kAnimSetAlphaSlot = 29;
// mov rax,rsp; push rbp; push r12; push r13; push r14; push r15; lea rbp,[rax-5Fh].
constexpr uint8_t kCreatureUpdateEntry[] = {0x48, 0x8b, 0xc4, 0x55, 0x41, 0x54, 0x41, 0x55,
                                            0x41, 0x56, 0x41, 0x57, 0x48, 0x8d, 0x68, 0xa1};
// push rdi; sub rsp,50h; mov rdi,rcx; movaps [rsp+40h],xmm6; mov rcx,[rcx+0C8h] (the model).
constexpr uint8_t kSetAlphaEntry[] = {0x40, 0x57, 0x48, 0x83, 0xec, 0x50, 0x48, 0x8b, 0xf9,
                                      0x0f, 0x29, 0x74, 0x24, 0x40, 0x48, 0x8b, 0x89, 0xc8};
constexpr const char* kSceneAddHighlight = "?AddHighlightGob@Scene@@UEAAXPEAVCAurObject@@AEAVVector@@@Z";
constexpr const char* kSceneRemoveHighlight = "?RemoveHighlightGob@Scene@@UEAAXPEAVCAurObject@@@Z";
constexpr int kSceneAddSlot = 35, kSceneRemoveSlot = 36;
constexpr size_t kClientAppInternal = 0x8;  // CClientExoApp -> its internal object
constexpr size_t kInternalGame = 0x30;      // -> what holds the area shown
constexpr size_t kGameArea = 0xf8;          // -> the client area
constexpr size_t kAreaScene = 0x248;        // -> its Scene
constexpr size_t kSceneHighlights = 0x80;   // CAurObject** (count int32 +0x88)
constexpr size_t kSceneHighlightCount = 0x88;
constexpr size_t kClientObjectAnim = 0x78;  // CNWCObject -> its animation object
constexpr size_t kAnimModel = 0xc8;         // -> its model (Gob)
constexpr size_t kGobAlpha = 0x108;         // float (Gob::SetColorShifting)
constexpr int kModelReadySlot = 0x218 / 8;  // the model's check before a highlight (0x96628c)
constexpr float kOutlineColour[3] = {0.80f, 0.95f, 1.0f};  // pale white-cyan

// One hider as the client sees it; the client's thread only.
struct ClientHider {
  uint32_t id = 0;
  void* model = nullptr;  // compared only once it may be gone
  void* scene = nullptr;  // compared only
  float set = 0.0f;       // the alpha last set on it
  int sets = 0;           // how often it had to be set
  bool caught = false;    // a see-through request for it was caught
  bool outlined = false;
};
ClientHider g_client[kHiders];
int g_client_count = 0;
int g_scene_slots_ok = -1;  // the scene's vtable slots are the exported highlight functions: -1 not checked yet

void* VirtualAt(void* object, int slot) { return reinterpret_cast<void* const*>(Field<void*>(object, 0))[slot]; }

void* SceneNow() {
  static void** app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  if (!app_manager || !*app_manager) return nullptr;
  void* client = Field<void*>(*app_manager, 0);
  void* internal = client ? Field<void*>(client, kClientAppInternal) : nullptr;
  void* game = internal ? Field<void*>(internal, kInternalGame) : nullptr;
  void* area = game ? Field<void*>(game, kGameArea) : nullptr;
  return area ? Field<void*>(area, kAreaScene) : nullptr;
}

bool Listed(void* scene, void* model) {
  void* const* models = Field<void* const*>(scene, kSceneHighlights);
  const int count = Field<int32_t>(scene, kSceneHighlightCount);
  if (!models || count <= 0 || count > 4096) return false;
  for (int i = 0; i < count; ++i) {
    if (models[i] == model) return true;
  }
  return false;
}

bool SceneSlotsOk(void* scene) {
  if (g_scene_slots_ok < 0) {
    const uintptr_t add = reinterpret_cast<uintptr_t>(VirtualAt(scene, kSceneAddSlot));
    const uintptr_t remove = reinterpret_cast<uintptr_t>(VirtualAt(scene, kSceneRemoveSlot));
    g_scene_slots_ok = g_add_export && add == g_add_export && remove == g_remove_export ? 1 : 0;
    if (!g_scene_slots_ok) Log("SKIP stealth outline: the scene's highlight slots aren't the exported functions");
  }
  return g_scene_slots_ok == 1;
}

// The model's alpha set through its animation object, as the engine does.
bool SetAlpha(void* anim, float alpha) {
  if (!g_set_alpha || reinterpret_cast<uintptr_t>(VirtualAt(anim, kAnimSetAlphaSlot)) != g_set_alpha) return false;
  reinterpret_cast<SetAlphaFn>(g_set_alpha)(anim, alpha, 0xff);
  return true;
}

void Forget(ClientHider* c) {
  *c = ClientHider{};
  --g_client_count;
}

void ColorShift(void* gob, const float* colour, float alpha, int propagate) {
  if (alpha < 0.999f && g_client_count > 0 && gob) {
    for (ClientHider& c : g_client) {
      if (!c.id || c.model != gob) continue;
      const float want = OpacityOf(c.id);
      if (want > 0.0f) {
        if (!c.caught) {
          c.caught = true;
          const uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
          Log("stealth ghost: 0x%08X see-through request %.2f -> %.2f (from nwmain+0x%llx)", c.id, alpha, want,
              static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(_ReturnAddress()) - base));
        }
        alpha = want;
      }
      break;
    }
  }
  g_color_shift(gob, colour, alpha, propagate);
}

// The server's id of a client object (see above).
constexpr uint32_t kClientIdBit = 0x80000000u;
uint32_t ServerIdOf(uint32_t client_id) {
  return client_id == ee::kObjectInvalid ? 0 : client_id & ~kClientIdBit;
}

// While someone hides and no client creature matched it yet: the ids seen, logged once.
uint32_t g_seen[6] = {};
int g_unmatched_frames = 0;
bool g_unmatched_logged = false;

void NoteUnmatched(uint32_t client_id) {
  if (g_unmatched_logged) return;
  for (uint32_t& seen : g_seen) {
    if (seen == client_id) break;
    if (!seen) {
      seen = client_id;
      break;
    }
  }
  if (++g_unmatched_frames < 2000) return;
  g_unmatched_logged = true;
  Log("stealth ghost: no client creature matched a hider - client ids seen: %08X %08X %08X %08X %08X %08X", g_seen[0],
      g_seen[1], g_seen[2], g_seen[3], g_seen[4], g_seen[5]);
}

// One client creature, after its update: a hider held at its opacity and
// outlined; one that stopped, solid again and its outline off.
void ClientStep(void* creature) {
  const uint32_t client_id = Field<uint32_t>(creature, ee::kObjectId);
  const uint32_t id = ServerIdOf(client_id);
  if (!id) return;
  const float want = OpacityOf(id);
  ClientHider* c = nullptr;
  ClientHider* free_slot = nullptr;
  for (ClientHider& x : g_client) {
    if (x.id == id) c = &x;
    if (!x.id && !free_slot) free_slot = &x;
  }
  if (want <= 0.0f && !c) {
    if (g_client_count == 0) NoteUnmatched(client_id);
    return;
  }
  void* anim = Field<void*>(creature, kClientObjectAnim);
  void* model = anim ? Field<void*>(anim, kAnimModel) : nullptr;
  void* scene = SceneNow();
  if (want <= 0.0f) {
    // It stopped hiding: back to solid unless something else changed the alpha since,
    // the outline off - both only on the model (and scene) they were put on.
    if (model && c->model == model) {
      const bool ours = c->set > 0.0f && fabsf(Field<float>(model, kGobAlpha) - c->set) < 0.005f;
      if (ours) SetAlpha(anim, 1.0f);
      if (c->outlined && scene && c->scene == scene && SceneSlotsOk(scene) && Listed(scene, model)) {
        reinterpret_cast<RemoveHighlightFn>(VirtualAt(scene, kSceneRemoveSlot))(scene, model);
      }
      Log("stealth ghost: 0x%08X stopped hiding - %s, outline %s (its alpha was set %d times)", id,
          ours ? "solid again" : "its alpha left to the game", c->outlined ? "off" : "never shown", c->sets);
    }
    Forget(c);
    return;
  }
  if (!model) return;
  if (c && c->model != model) {
    Forget(c);  // a new area or body: the old ones went with it
    c = nullptr;
    for (ClientHider& x : g_client) {
      if (!x.id) {
        free_slot = &x;
        break;
      }
    }
  }
  if (!c) {
    if (!free_slot) return;
    c = free_slot;
    c->id = id;
    c->model = model;
    ++g_client_count;
    Log("stealth ghost: 0x%08X hides - client object %08X, model %p (alpha %.2f)", id, client_id, model,
        Field<float>(model, kGobAlpha));
  }
  // Its opacity.
  if (fabsf(Field<float>(model, kGobAlpha) - want) > 0.005f && SetAlpha(anim, want)) {
    if (c->set != want) {
      Log("stealth ghost: 0x%08X drawn %.0f%% visible", id, want * 100.0f);
    }
    c->set = want;
    ++c->sets;
  }
  // Its outline.
  if (!g_config.stealth_outline || !g_add_export || !scene || !SceneSlotsOk(scene)) return;
  if (c->scene != scene) {
    c->scene = scene;
    c->outlined = false;
  }
  if (Listed(scene, model)) return;  // on (ours, or a hover's while the pointer is on it)
  if (!reinterpret_cast<ModelReadyFn>(VirtualAt(model, kModelReadySlot))(model)) return;
  reinterpret_cast<AddHighlightFn>(VirtualAt(scene, kSceneAddSlot))(scene, model, kOutlineColour);
  if (!c->outlined) Log("stealth outline: 0x%08X outlined while it hides", id);
  c->outlined = true;
}

int64_t CreatureUpdate(void* creature) {
  const int64_t result = g_creature_update(creature);
  if (creature && (g_client_count > 0 || AnyoneHides())) ClientStep(creature);
  return result;
}

}  // namespace

// stealth_watch.cpp: the opacity a hider's light and Stealth bonus call for
// (rules::StealthOpacity); 0 when it stops hiding.
void SetStealthOpacity(uint32_t id, float opacity) {
  if (!id) return;
  Hider* free_slot = nullptr;
  for (Hider& h : g_hiders) {
    const uint32_t held = h.id.load();
    if (held == id) {
      if (opacity > 0.0f) {
        h.opacity.store(opacity);
      } else {
        h.opacity.store(0.0f);
        h.id.store(0);
      }
      return;
    }
    if (!held && !free_slot) free_slot = &h;
  }
  if (opacity <= 0.0f || !free_slot) return;
  free_slot->opacity.store(opacity);
  free_slot->id.store(id);
}

bool StealthGhostOn() {
  return g_config.stealth_ghost && g_config.stealth_light && g_config.perception == rules::RuleVariant::kFaithful;
}

void InstallStealthGhost() {
  if (!StealthGhostOn()) return;
  // The animation object's alpha setter (slot 29), the client creature update (slot 29).
  const uintptr_t set_alpha = VtableSlot(FindVtable("client animation object: its vtable (stealth ghost)", kRttiAnimBase),
                                         kAnimSetAlphaSlot);
  if (set_alpha && FunctionEntry(set_alpha) == set_alpha &&
      ContainsBytes(set_alpha, set_alpha + sizeof(kSetAlphaEntry), kSetAlphaEntry, sizeof(kSetAlphaEntry))) {
    g_set_alpha = set_alpha;
  } else {
    Log("SKIP stealth ghost: the animation object's alpha setter wasn't found - the ghost effect's own look");
  }
  const uintptr_t update = VtableSlot(FindVtable("client creature: its vtable (stealth ghost)", kRttiClientCreature),
                                      kCreatureUpdateSlot);
  if (!update || FunctionEntry(update) != update ||
      !ContainsBytes(update, update + sizeof(kCreatureUpdateEntry), kCreatureUpdateEntry, sizeof(kCreatureUpdateEntry))) {
    Log("SKIP stealth ghost: the client creature update wasn't found - the ghost effect's own look, no outline");
    return;
  }
  HookExport("Gob::SetColorShifting (a hider's own model at its opacity)", kColorShifting,
             reinterpret_cast<void*>(&ColorShift), reinterpret_cast<void**>(&g_color_shift));
  if (g_config.stealth_outline) {
    g_add_export = EngineExport(kSceneAddHighlight);
    g_remove_export = EngineExport(kSceneRemoveHighlight);
    if (!g_add_export || !g_remove_export) {
      Log("SKIP stealth outline: no export of the scene's highlight functions");
      g_add_export = 0;
    }
  }
  if (HookAddress("client creature update (a hider's opacity and outline)", update,
                  reinterpret_cast<void*>(&CreatureUpdate), reinterpret_cast<void**>(&g_creature_update))) {
    Log("stealth ghost: a hiding character is 90%% visible in light, 70%% in dim, 50%% in dark, less 2%% per "
        "point of Stealth bonus (30%% at least)%s",
        g_config.stealth_outline && g_add_export ? "; outlined in EE's highlight glow (stealth_outline)" : "");
  }
}
