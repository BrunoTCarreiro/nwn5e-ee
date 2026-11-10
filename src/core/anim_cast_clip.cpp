// A cast's animation swapped for a clip of nwn5e's on one caster (#150): see anim_cast_clip.h. data::CastClipMs
// (src/data/anim_flourish) holds the swap's time; a creature-update listener maps it back.

#include "anim_cast_clip.h"

#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "creature.h"
#include "creature_update.h"
#include "data/anim_flourish.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "game_clock.h"
#include "hooks.h"
#include "log.h"

bool AnimationRecipeBuilt(const std::string& name, std::string* text);  // anim_packs.cpp

namespace {

constexpr const char* kSwapVar = "n5_castclip_swap";  // the swap script's answer: 1 = part-based, swapped

struct Swap {
  std::vector<std::string> bases;  // the animations swapped: `base`, and `base`lp when its clip was built
  std::string clip;
  int64_t until = 0;  // game ms
};
std::unordered_map<uint32_t, Swap> g_swaps;
bool g_installed = false;

uint32_t IdOf(void* creature) { return Field<uint32_t>(creature, ee::kObjectId); }

int64_t Now() { return static_cast<int64_t>(GameMs()); }

void MapBack(uint32_t id, const Swap& swap) {
  std::string code;
  for (const std::string& base : swap.bases) code += "ReplaceObjectAnimation(OBJECT_SELF, \"" + base + "\", \"" + base + "\");";
  if (!code.empty()) RunScriptChunk(code.c_str(), id);
}

void OnCreatureUpdate(void* creature) {
  if (g_swaps.empty() || !creature) return;
  const uint32_t id = IdOf(creature);
  const auto found = g_swaps.find(id);
  if (found == g_swaps.end() || Now() < found->second.until) return;
  MapBack(id, found->second);
  g_swaps.erase(found);
}

}  // namespace

void RequestCastClip(void* creature, const char* base, const std::string& clip, float seconds) {
  if (!base || !*base || clip.empty()) return;
  RequestCastClips(creature, {{base, clip}}, seconds);
}

void RequestCastClips(void* creature, const std::vector<CastClipSwap>& swaps, float seconds) {
  if (!creature || swaps.empty()) return;
  const uint32_t id = IdOf(creature);
  if (!g_installed) {
    Log("cast clip: 0x%08X %s -> %s skipped - not installed", id, swaps[0].base.c_str(), swaps[0].clip.c_str());
    return;
  }
  EndCastClip(creature);  // a new request replaces the last
  const int ms = data::CastClipMs(seconds);
  if (ms <= 0) {
    Log("cast clip: 0x%08X plays its own animations - no time given", id);
    return;
  }
  // Each pair, and its held loop too (castout, then castoutlp while the cast's time runs) when that clip was built.
  Swap swap;
  std::string replace, said;
  for (const CastClipSwap& s : swaps) {
    if (s.base.empty() || s.clip.empty()) continue;
    if (!AnimationRecipeBuilt(s.clip, nullptr)) {
      Log("cast clip: 0x%08X plays %s - %s isn't built", id, s.base.c_str(), s.clip.c_str());
      continue;
    }
    const std::string loop_base = s.base + "lp", loop_clip = s.clip + "lp";
    const bool loop = AnimationRecipeBuilt(loop_clip, nullptr);
    replace += " ReplaceObjectAnimation(OBJECT_SELF, \"" + s.base + "\", \"" + s.clip + "\");";
    if (loop) replace += " ReplaceObjectAnimation(OBJECT_SELF, \"" + loop_base + "\", \"" + loop_clip + "\");";
    swap.bases.push_back(s.base);
    if (loop) swap.bases.push_back(loop_base);
    said += (said.empty() ? "" : ", ") + s.base + " as " + s.clip + (loop ? " (and " + loop_base + " as " + loop_clip + ")" : "");
    if (swap.clip.empty()) swap.clip = s.clip;
  }
  if (replace.empty()) return;
  const std::string var = std::string("\"") + kSwapVar + "\"";
  RunScriptChunk(("SetLocalInt(OBJECT_SELF, " + var + ", 0);"
                  " if (Get2DAString(\"appearance\", \"MODELTYPE\", GetAppearanceType(OBJECT_SELF)) == \"P\") {" +
                  replace + " SetLocalInt(OBJECT_SELF, " + var + ", 1); }")
                     .c_str(),
                 id);
  const bool swapped = engine::GetLocalInt(creature, kSwapVar) == 1;
  RunScriptChunk(("DeleteLocalInt(OBJECT_SELF, " + var + ");").c_str(), id);
  if (!swapped) {
    Log("cast clip: 0x%08X plays %s - not a part-based body", id, swaps[0].base.c_str());
    return;
  }
  swap.until = Now() + ms;
  g_swaps[id] = swap;
  Log("cast clip: 0x%08X plays %s for %.2f s", id, said.c_str(), ms / 1000.0f);
}

void EndCastClip(void* creature) {
  if (!creature || g_swaps.empty()) return;
  const uint32_t id = IdOf(creature);
  const auto found = g_swaps.find(id);
  if (found == g_swaps.end()) return;
  MapBack(id, found->second);
  g_swaps.erase(found);
}

void InstallCastClip() {
  if (g_installed) return;
  if (!AddCreatureUpdateListener(&OnCreatureUpdate)) {
    Log("cast clip: no creature update - casts play their own animations");
    return;
  }
  g_installed = true;
  Log("cast clip: a cast's animation can be swapped for a clip of nwn5e's while it plays (#150)");
}
