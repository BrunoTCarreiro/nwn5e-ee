// The look of a wounded player character (owner, 2026-10-24: "I want to make
// some additions to the look of the PC when they are exhausted and low on
// health" ... then "just do 1 bleeding effect, mild, with a small puddle when
// the PC is under 10%"): under bleed_percent of its hit points the player
// character bleeds - a small puddle (0.22.8: no dripping; visualeffects 1570,
// nwn5e's own model: tools/data/bleed_vfx.py), a duration effect tagged
// nwn5e_bleed, put back if something strips it; none at 0 HP (the down visuals
// show then). Checked once a second from the creature update; after a load the
// whole state is sent. Ini: low_hp_bleed, bleed_percent. The exhausted look is
// rules_dying.cpp's.

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <unordered_map>

#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"

namespace {

constexpr int kBleedRow = 1570;  // NWN5E_BLEED
constexpr const char* kTag = "nwn5e_bleed";
constexpr uint64_t kCheckMs = 1000;
constexpr int kChecksPerRepair = 6;  // put a stripped effect back every 6 s

using MaxHitPointsFn = int16_t (*)(void* creature, int flag);

struct Bleed {
  int tier = -1;  // 1 bleeding, 0 not; -1: nothing sent yet (a new session or a load)
  uint64_t next = 0;
  int checks = 0;
};
MaxHitPointsFn g_max_hit_points = nullptr;
std::unordered_map<uint32_t, Bleed> g_bleeds;  // by player character, the engine's thread only

// 1 under bleed_percent of the hit points (above 0), else 0.
int TierFor(int hit_points, int max_hit_points) {
  if (hit_points < 1 || max_hit_points <= 0 || g_config.bleed_percent <= 0) return 0;
  return hit_points * 100 < max_hit_points * g_config.bleed_percent ? 1 : 0;
}

// The effect on (one, applied if missing) or off. Any older nwn5e_bleed_*
// tag (0.22.3's three tiers) goes too.
void Sync(uint32_t id, int tier) {
  char code[700];
  sprintf_s(code,
            "int on = %d; int have = FALSE; effect e = GetFirstEffect(OBJECT_SELF);"
            "while (GetIsEffectValid(e)) { string t = GetEffectTag(e);"
            " if (t == \"%s\" && on) have = TRUE;"
            " else if (GetStringLeft(t, %d) == \"%s\") RemoveEffect(OBJECT_SELF, e);"
            " e = GetNextEffect(OBJECT_SELF); }"
            "if (on && !have) ApplyEffectToObject(DURATION_TYPE_PERMANENT,"
            " TagEffect(SupernaturalEffect(EffectVisualEffect(%d)), \"%s\"), OBJECT_SELF);",
            tier, kTag, static_cast<int>(strlen(kTag)), kTag, kBleedRow, kTag);
  RunScriptChunk(code, id);
}

void Update(void* creature) {
  if (!creature || Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) == 0) return;
  const uint32_t id = Field<uint32_t>(creature, ee::kObjectId);
  Bleed& bleed = g_bleeds[id];
  const uint64_t now = GetTickCount64();
  if (now < bleed.next) return;
  bleed.next = now + kCheckMs;
  const int hit_points = Field<int16_t>(creature, ee::kObjectCurrentHitPoints);
  const int max_hit_points = g_max_hit_points ? g_max_hit_points(creature, 1) : 0;
  const int tier = TierFor(hit_points, max_hit_points);
  if (tier != bleed.tier) {
    if (bleed.tier >= 0 || tier > 0) {
      Log("look: 0x%08X %d / %d HP - bleeding %s", id, hit_points, max_hit_points, tier ? "starts" : "stops");
    }
    bleed.tier = tier;
    bleed.checks = 0;
    Sync(id, tier);
  } else if (tier > 0 && ++bleed.checks % kChecksPerRepair == 0) {
    Sync(id, tier);  // a script or a rest may have stripped it
  }
}

}  // namespace

void InstallLowHpLook() {
  if (!g_config.low_hp_bleed) return;
  g_max_hit_points = reinterpret_cast<MaxHitPointsFn>(EngineExport(ee::kGetMaxHitPoints));
  if (!g_max_hit_points) {
    Log("SKIP look: no %s - no bleeding at low hit points", ee::kGetMaxHitPoints);
    return;
  }
  if (!AddCreatureUpdateListener(&Update)) return;
  Log("look: a player character bleeds under %d%% of its hit points (low_hp_bleed)", g_config.bleed_percent);
}
