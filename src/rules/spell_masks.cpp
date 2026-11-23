#include "rules/spell_masks.h"

#include <stdlib.h>
#include <string.h>

namespace rules {

namespace {

// The rows of the masked copies: tools/data/spell_mask.js's EFFECT_ROWS and AREA_ROWS, written into data\rows'
// visualeffects.mask.if_spellmask.txt ("# 4200 from 22") and vfx_persistent.maskcopies.if_spellmask.txt (the tests
// check both agree with this).
constexpr int kFireball = 4200, kSoundBurst = 4201, kFrostL = 4202, kIceStorm = 4203, kFireStorm = 4204,
              kMeteorSwarm = 4205, kHolyBurst = 4206, kSunbeam = 4207, kFlameStrike = 4208;
constexpr int kFogFire = 120, kFogStink = 121, kFogKill = 122, kWeb = 123;

// A cone's look plays from the cast's start (conjuring ~1 s, then its 2 s cast animation's particles): 5 s.
constexpr int kConeMs = 5000;

std::vector<SpellMaskEntry> Build() {
  using W = MaskWhen;
  std::vector<SpellMaskEntry> m;
  // nwn5e's own area spells.
  m.push_back({10, W::kCast, kConeMs, {}, {}});                      // Burning Hands: spells.2da's cone
  m.push_back({24, W::kCast, kConeMs, {}, {}});                      // Color Spray
  for (int exhale : {1269, 1270, 1271, 1273}) m.push_back({exhale, W::kCast, kConeMs, {}, {}});  // Dragon's Breath's
  m.push_back({436, W::kCast, 6000, {{183, kSoundBurst}}, {}});      // Thunderwave: the sound burst at its cube
  m.push_back({167, W::kImpact, 6000, {{183, kSoundBurst}}, {}});    // Shatter: the sound burst at its point
  m.push_back({543, W::kByCode, 4000, {{62, kFrostL}}, {}});         // Ice Knife: the burst round the creature struck
  m.push_back({53, W::kLasting, 0, {}, {}});                         // Entangle: vfx_persistent 105's own models
  m.push_back({66, W::kLasting, 0, {}, {}});                         // Grease: 106's
  m.push_back({1180, W::kLasting, 0, {}, {}});                       // Fog Cloud: 107-115's
  // NWN's own area spells (their scripts NWN's; nwn5e's area code gives their saves cover from the origin).
  m.push_back({25, W::kCast, kConeMs, {}, {}});                      // Cone of Cold
  m.push_back({135, W::kCast, kConeMs, {}, {}});                     // Prismatic Spray
  m.push_back({58, W::kImpact, 7000, {{22, kFireball}}, {}});        // Fireball (its burst plays 5 s)
  m.push_back({368, W::kImpact, 6500, {{231, kIceStorm}}, {}});      // Ice Storm (its ice chunks aren't masked)
  m.push_back({57, W::kImpact, 6500, {{23, kFireStorm}}, {}});       // Fire Storm
  m.push_back({116, W::kImpact, 5500, {{28, kMeteorSwarm}}, {}});    // Meteor Swarm (its meteors aren't masked)
  m.push_back({427, W::kImpact, 4000, {{190, kHolyBurst}}, {}});     // Sunburst
  m.push_back({183, W::kImpact, 5000, {{37, kSunbeam}}, {}});        // Sunbeam
  m.push_back({61, W::kImpact, 4000, {{54, kFlameStrike}}, {}});     // Flame Strike: the column from above
  m.push_back({23, W::kLasting, 0, {}, {{3, kFogKill}}});            // Cloudkill
  m.push_back({171, W::kLasting, 0, {}, {{2, kFogStink}}});          // Stinking Cloud
  m.push_back({89, W::kLasting, 0, {}, {{1, kFogFire}}});            // Incendiary Cloud
  m.push_back({192, W::kLasting, 0, {}, {{8, kWeb}}});               // Web
  return m;
}

int Swap(const std::vector<std::pair<int, int>>& pairs, int row) {
  for (const auto& p : pairs) {
    if (p.first == row) return p.second;
  }
  return row;
}

}  // namespace

const std::vector<SpellMaskEntry>& SpellMasks() {
  static const std::vector<SpellMaskEntry> masks = Build();
  return masks;
}

const SpellMaskEntry* SpellMaskOf(int spell) {
  for (const SpellMaskEntry& e : SpellMasks()) {
    if (e.spell == spell) return &e;
  }
  return nullptr;
}

int MaskedEffectRow(int spell, int row) {
  const SpellMaskEntry* e = SpellMaskOf(spell);
  return e ? Swap(e->effects, row) : row;
}

int MaskedAreaRow(int spell, int row) {
  const SpellMaskEntry* e = SpellMaskOf(spell);
  return e ? Swap(e->areas, row) : row;
}

float MaskReach(float area_metres) { return (area_metres > 0.0f ? area_metres : 0.0f) + kMaskReachPast; }

float FogCloudRadius(const char* tag) {
  static const char kPrefix[] = "NWN5E_FOG_CLOUD_";
  if (!tag || _strnicmp(tag, kPrefix, sizeof(kPrefix) - 1) != 0) return 0.0f;
  const int level = atoi(tag + sizeof(kPrefix) - 1);
  return level >= 1 && level <= 9 ? 6.096f * static_cast<float>(level) : 0.0f;
}

bool MaskEnded(const HeldMask& mask, uint64_t now) { return mask.end_ms != 0 && now >= mask.end_ms; }

uint64_t MaskToShow(const std::vector<HeldMask>& masks, uint64_t area, uint64_t now) {
  const HeldMask* best = nullptr;
  for (const HeldMask& m : masks) {
    if (m.area != area || area == 0 || MaskEnded(m, now)) continue;
    if (!best || m.set_ms > best->set_ms || (m.set_ms == best->set_ms && m.id > best->id)) best = &m;
  }
  return best ? best->id : 0;
}

}  // namespace rules
