#pragma once

// The area spells whose looks are masked (#184; the owner, 2026-11-23: "maskproof seems good!", then "wire it, lets test
// it out"): which effects each one plays have masked copies (tools/data/spell_mask.js builds them; data\rows'
// *.if_spellmask.txt rows name them), when its mask is set and how long it holds, and which mask a player's client
// shows when several are up. This file only decides; core/spell_mask.cpp measures and sends.
//
// Overlapping masks: the uniforms are one set per client, so a client shows one mask at a time - the newest of those
// up in its area (the last set; a lasting area's mask re-sent after an obstacle broke keeps its place). An older one
// still up comes back when the newer ends. While a newer one shows, an older lasting area's particles (a Fog Cloud) within the newer one's
// reach are masked by the newer one's walls and boxes, and those past its reach draw whole - both for the few seconds a
// burst lasts.

#include <stdint.h>

#include <utility>
#include <vector>

namespace rules {

enum class MaskWhen {
  kCast,     // its look plays with the cast (a cone; Thunderwave's cube): set as the cast begins - its origin the caster's
  kImpact,   // its look plays as it takes effect (a burst at a point, a column from above): set as its script starts
  kLasting,  // a lasting area (Fog Cloud, Entangle, a cloud): set as it takes effect, held while its area lasts
  kByCode,   // set by the spell's own code where its look is (Ice Knife's burst round the creature it struck)
};

struct SpellMaskEntry {
  int spell = -1;                            // spells.2da row
  MaskWhen when = MaskWhen::kImpact;
  int hold_ms = 0;                           // how long its look plays once the mask is set (a lasting area: its area's)
  std::vector<std::pair<int, int>> effects;  // a visualeffects.2da row its script plays -> its masked copy's row
  std::vector<std::pair<int, int>> areas;    // a vfx_persistent.2da row its script makes -> its masked copy's row
};

const std::vector<SpellMaskEntry>& SpellMasks();
const SpellMaskEntry* SpellMaskOf(int spell);
// The visualeffects row `spell`'s script plays in place of `row`; `row` itself when it has no masked copy.
int MaskedEffectRow(int spell, int row);
// The vfx_persistent row `spell`'s script makes in place of `row`; `row` itself when none.
int MaskedAreaRow(int spell, int row);

// How far the mask reaches across the ground from the origin: the area's own size (its cone's length, its sphere's
// radius, its square's half diagonal - metres) and kMaskReachPast more, for the particles thrown past its edge.
constexpr float kMaskReachPast = 3.0f;
float MaskReach(float area_metres);
// A Fog Cloud's radius from its area's tag: NWN5E_FOG_CLOUD_<n> is n x 20 feet (vfx_persistent rows 107-115); 0 for
// any other tag.
float FogCloudRadius(const char* tag);

// A mask that is up: its id (a later set gets a higher id), its area, when it was set and when it ends (0: held until
// it's dropped - a lasting area's).
struct HeldMask {
  uint64_t id = 0;
  uint64_t area = 0;
  uint64_t set_ms = 0;
  uint64_t end_ms = 0;
};
bool MaskEnded(const HeldMask& mask, uint64_t now);
// The mask a client in `area` shows at `now`: the newest set (the higher id on a tie) of those in its area that haven't
// ended; 0 none (the mask off).
uint64_t MaskToShow(const std::vector<HeldMask>& masks, uint64_t area, uint64_t now);

}  // namespace rules
