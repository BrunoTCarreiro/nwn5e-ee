// The hiding character's see-through look by the light (owner, 2026-10-25:
// "change the stealth vfx, make it dependent on the light level, 90% visible on
// light, 70% in dim and 50% in dark" - "do not make the ghostly effect pulse,
// have it maintain the transparency level"). While a character hides,
// stealth_watch.cpp puts EE's steady ghost effect on it (VFX_DUR_GHOST_TRANSPARENT,
// 424: programmatic effect 420 - not the pulsing 401 / 404) and tells this file the
// opacity its light calls for, by object id.
//
// How the client draws an object see-through (read from nwmain, 89.8193.37): a
// client object (CNWCObject: id +0x8, its animation object +0x78, its visual
// effects +0x80, its alpha +0xb0) fades towards a target alpha; the target comes
// from a routine that walks its visual effects and returns the see-through one's
// alpha (the effect's +0x100), or -1 when none is on it. That routine is hooked
// here: for an object registered as hiding that has a see-through effect on, its
// light's opacity is returned instead - nothing else changes (0.33.0 overrode
// every see-through request at the model level, so fading-in creatures and other
// models got the hider's value too). The routine is not exported: it is found by
// its entry bytes, checked to be one function's entry that reads an effect's
// alpha, and skipped (logged) otherwise. `stealth_ghost` (2024 perception,
// stealth_light).
//
// The outline (0.34.0; owner, 2026-10-25: "there might be an issue with the
// transparency levels where the player might not see their own character in
// high levels of stealth. Is it possible to add any kind of outline to
// characters?" - "go with A"): a hider gets EE's own highlight glow, the one
// the mouse-over and Tab draw round a silhouette, in a pale white-cyan. The
// scene keeps a list of highlighted models and colours (Scene::AddHighlightGob /
// RemoveHighlightGob, vtable slots 35 / 36, checked against their exports); the
// engine reaches the scene as client app (*g_pAppManager) +0x8 -> +0x30 -> its
// area +0xf8 -> +0x248, and a creature's model as its animation object (+0x78)
// +0xc8, checked with the model's own vtable slot 67 first - all as its
// highlight code (0x966170) does. Each frame, after a client creature's update
// (CNWCCreature vtable slot 29, entry bytes checked), a hiding creature's model
// is put on the list when it isn't on it (the mouse-over takes it off when the
// pointer leaves; a hover's colour shows while hovering), and taken off when it
// stops hiding. A model or scene seen before is only compared, never followed:
// after a new area the old ones are just forgotten. `stealth_outline`.

#include <stdint.h>

#include <atomic>

#include "config.h"
#include "creature.h"
#include "engine.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"

namespace {

using EffectAlphaFn = float (*)(void* object);
EffectAlphaFn g_effect_alpha = nullptr;

// The routine's entry: mov [rsp+10h],rbx; mov [rsp+18h],rsi; push rdi; sub rsp,30h;
// mov rax,[rcx+80h] (the visual effects); mov rsi,rcx; movaps [rsp+20h],xmm6; mov dil,0FFh.
constexpr const char* kEffectAlphaEntry =
    "48 89 5c 24 10 48 89 74 24 18 57 48 83 ec 30 48 8b 81 80 00 00 00 48 8b f1 0f 29 74 24 20 40 b7 ff";
// Inside it: movss xmm6, [rbx+100h] (an effect's alpha).
constexpr uint8_t kReadsEffectAlpha[] = {0xf3, 0x0f, 0x10, 0xb3, 0x00, 0x01, 0x00, 0x00};

// The outline (see above).
constexpr const char* kRttiClientCreature = ".?AVCNWCCreature@@";
constexpr int kCreatureUpdateSlot = 29;
// Its entry: mov rax,rsp; push rbp; push r12; push r13; push r14; push r15; lea rbp,[rax-5Fh].
constexpr uint8_t kCreatureUpdateEntry[] = {0x48, 0x8b, 0xc4, 0x55, 0x41, 0x54, 0x41, 0x55,
                                            0x41, 0x56, 0x41, 0x57, 0x48, 0x8d, 0x68, 0xa1};
constexpr const char* kSceneAddHighlight = "?AddHighlightGob@Scene@@UEAAXPEAVCAurObject@@AEAVVector@@@Z";
constexpr const char* kSceneRemoveHighlight = "?RemoveHighlightGob@Scene@@UEAAXPEAVCAurObject@@@Z";
constexpr int kSceneAddSlot = 35, kSceneRemoveSlot = 36;
constexpr size_t kClientAppInternal = 0x8;      // CClientExoApp -> its internal object
constexpr size_t kInternalGame = 0x30;          // -> what holds the area shown
constexpr size_t kGameArea = 0xf8;              // -> the client area
constexpr size_t kAreaScene = 0x248;            // -> its Scene
constexpr size_t kSceneHighlights = 0x80;       // CAurObject** (count int32 +0x88)
constexpr size_t kSceneHighlightCount = 0x88;
constexpr size_t kClientObjectAnim = 0x78;      // CNWCObject -> its animation object
constexpr size_t kAnimModel = 0xc8;             // -> its model (Gob)
constexpr int kModelReadySlot = 0x218 / 8;      // the model's check before a highlight (0x96628c)
constexpr float kOutlineColour[3] = {0.80f, 0.95f, 1.0f};  // pale white-cyan

using CreatureUpdateFn = int64_t (*)(void* creature);
using AddHighlightFn = void (*)(void* scene, void* model, const float* colour);
using RemoveHighlightFn = void (*)(void* scene, void* model);
using ModelReadyFn = void* (*)(void* model);
CreatureUpdateFn g_creature_update = nullptr;
uintptr_t g_add_export = 0, g_remove_export = 0;

// Who hides now and how see-through each is (0: the slot is free).
struct Hider {
  std::atomic<uint32_t> id{0};
  std::atomic<float> opacity{0.0f};
  std::atomic<float> logged{-1.0f};  // the last opacity logged for it
};
constexpr int kHiders = 8;
Hider g_hiders[kHiders];
std::atomic<int> g_others_logged{0};  // see-through objects left alone, logged while someone hides

float OpacityOf(uint32_t id, Hider** slot) {
  for (Hider& h : g_hiders) {
    if (h.id.load() == id) {
      *slot = &h;
      return h.opacity.load();
    }
  }
  return 0.0f;
}

bool AnyoneHides() {
  for (const Hider& h : g_hiders) {
    if (h.id.load() != 0) return true;
  }
  return false;
}

float EffectAlpha(void* object) {
  const float alpha = g_effect_alpha(object);
  if (alpha < 0.0f || !object) return alpha;  // no see-through effect on it
  const uint32_t id = Field<uint32_t>(object, ee::kObjectId);
  Hider* slot = nullptr;
  const float want = id ? OpacityOf(id, &slot) : 0.0f;
  if (want <= 0.0f) {
    if (AnyoneHides() && g_others_logged.load() < 12) {
      ++g_others_logged;
      Log("stealth ghost: 0x%08X is see-through (%.2f) - not a hider, left alone", id, alpha);
    }
    return alpha;
  }
  if (slot && slot->logged.load() != want) {
    slot->logged.store(want);
    Log("stealth ghost: 0x%08X drawn %.0f%% visible (its ghost effect asked %.2f)", id, want * 100.0f, alpha);
  }
  return want;
}

// The outline's bookkeeping, the client's thread only.
struct Outline {
  uint32_t id = 0;
  void* model = nullptr;  // compared only
  void* scene = nullptr;  // compared only
};
Outline g_outlines[kHiders];
int g_outlined = 0;
int g_scene_slots_ok = -1;  // the scene's vtable slots are the exported highlight functions: -1 not checked yet

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

void* VirtualAt(void* object, int slot) { return reinterpret_cast<void* const*>(Field<void*>(object, 0))[slot]; }

bool SceneSlotsOk(void* scene) {
  if (g_scene_slots_ok < 0) {
    const uintptr_t add = reinterpret_cast<uintptr_t>(VirtualAt(scene, kSceneAddSlot));
    const uintptr_t remove = reinterpret_cast<uintptr_t>(VirtualAt(scene, kSceneRemoveSlot));
    g_scene_slots_ok = add == g_add_export && remove == g_remove_export ? 1 : 0;
    if (!g_scene_slots_ok) Log("SKIP stealth outline: the scene's highlight slots aren't the exported functions");
  }
  return g_scene_slots_ok == 1;
}

void Forget(Outline* o) {
  *o = Outline{};
  --g_outlined;
}

// One client creature, after its update: its outline on while it hides, off after.
void OutlineStep(void* creature) {
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  if (!id) return;
  Hider* slot = nullptr;
  const bool hiding = OpacityOf(id, &slot) > 0.0f;
  Outline* o = nullptr;
  Outline* free_slot = nullptr;
  for (Outline& x : g_outlines) {
    if (x.id == id) o = &x;
    if (!x.id && !free_slot) free_slot = &x;
  }
  if (!hiding && !o) return;
  void* anim = Field<void*>(creature, kClientObjectAnim);
  void* model = anim ? Field<void*>(anim, kAnimModel) : nullptr;
  void* scene = SceneNow();
  if (!hiding) {
    // Off - only from the model and scene it was put on (a new area took them away).
    if (model && scene && o->model == model && o->scene == scene && SceneSlotsOk(scene) && Listed(scene, model)) {
      reinterpret_cast<RemoveHighlightFn>(VirtualAt(scene, kSceneRemoveSlot))(scene, model);
    }
    Log("stealth outline: 0x%08X outline off", id);
    Forget(o);
    return;
  }
  if (!model || !scene || !SceneSlotsOk(scene)) return;
  if (o && (o->model != model || o->scene != scene)) {
    Forget(o);  // a new area or body: the old ones went with it
    o = nullptr;
    for (Outline& x : g_outlines) {
      if (!x.id) {
        free_slot = &x;
        break;
      }
    }
  }
  if (!o) {
    if (!free_slot) return;
    o = free_slot;
    o->id = id;
    o->model = model;
    o->scene = scene;
    ++g_outlined;
    Log("stealth outline: 0x%08X outlined while it hides", id);
  }
  if (Listed(scene, model)) return;  // on (ours, or a hover's while the pointer is on it)
  if (!reinterpret_cast<ModelReadyFn>(VirtualAt(model, kModelReadySlot))(model)) return;
  reinterpret_cast<AddHighlightFn>(VirtualAt(scene, kSceneAddSlot))(scene, model, kOutlineColour);
}

int64_t CreatureUpdate(void* creature) {
  const int64_t result = g_creature_update(creature);
  if (creature && (g_outlined > 0 || AnyoneHides())) OutlineStep(creature);
  return result;
}

}  // namespace

// stealth_watch.cpp: the opacity a hider's light calls for (0.9 light, 0.7 dim, 0.5
// dark); 0 when it stops hiding.
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
        h.logged.store(-1.0f);
        h.id.store(0);
      }
      return;
    }
    if (!held && !free_slot) free_slot = &h;
  }
  if (opacity <= 0.0f || !free_slot) return;
  if (!AnyoneHides()) g_others_logged.store(0);
  free_slot->opacity.store(opacity);
  free_slot->logged.store(-1.0f);
  free_slot->id.store(id);
}

bool StealthGhostOn() {
  return g_config.stealth_ghost && g_config.stealth_light && g_config.perception == rules::RuleVariant::kFaithful;
}

void InstallStealthGhost() {
  if (!StealthGhostOn()) return;
  const uintptr_t entry = FindPattern("the client's see-through alpha of an object (stealth_ghost)", kEffectAlphaEntry);
  if (!entry) return;
  const CodeRange code = FunctionAt(entry);
  if (FunctionEntry(entry) != entry || !code.Valid() ||
      !ContainsBytes(code.begin, code.end, kReadsEffectAlpha, sizeof(kReadsEffectAlpha))) {
    Log("SKIP stealth ghost: the see-through alpha code at %p is not the expected function", reinterpret_cast<void*>(entry));
    return;
  }
  if (HookAddress("client see-through alpha of an object (a hiding character's look by its light)", entry,
                  reinterpret_cast<void*>(&EffectAlpha), reinterpret_cast<void**>(&g_effect_alpha))) {
    Log("stealth ghost: a hiding character is 90%% visible in light, 70%% in dim, 50%% in dark (stealth_ghost)");
  }
  if (!g_config.stealth_outline) return;
  g_add_export = EngineExport(kSceneAddHighlight);
  g_remove_export = EngineExport(kSceneRemoveHighlight);
  const uintptr_t vtable = FindVtable("client creature: its vtable (stealth outline)", kRttiClientCreature);
  const uintptr_t update = VtableSlot(vtable, kCreatureUpdateSlot);
  if (!g_add_export || !g_remove_export || !update || FunctionEntry(update) != update ||
      !ContainsBytes(update, update + sizeof(kCreatureUpdateEntry), kCreatureUpdateEntry, sizeof(kCreatureUpdateEntry))) {
    Log("SKIP stealth outline: the client creature update or the scene's highlight functions weren't found");
    return;
  }
  if (HookAddress("client creature update (a hiding character's outline)", update,
                  reinterpret_cast<void*>(&CreatureUpdate), reinterpret_cast<void**>(&g_creature_update))) {
    Log("stealth outline: a hiding character is outlined in EE's highlight glow (stealth_outline)");
  }
}
