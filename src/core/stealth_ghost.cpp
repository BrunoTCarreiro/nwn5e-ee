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

#include <stdint.h>

#include <atomic>

#include "config.h"
#include "creature.h"
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
}
