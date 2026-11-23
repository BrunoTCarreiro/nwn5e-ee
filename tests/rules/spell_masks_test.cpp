#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "rules/spell_masks.h"

using namespace rules;

namespace {

std::string ReadText(const char* path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

// "# <row> from <row>" comment lines of a rows file.
std::map<int, int> CopiedFrom(const std::string& text) {
  std::map<int, int> out;
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    int row = 0, from = 0;
    if (sscanf(line.c_str(), "# %d from %d", &row, &from) == 2) out[row] = from;
  }
  return out;
}

// The row numbers a rows file gives (its lines after the header that start with a number).
std::set<int> RowsOf(const std::string& text) {
  std::set<int> out;
  std::istringstream lines(text);
  std::string line;
  bool header = false;
  while (std::getline(lines, line)) {
    if (line.empty() || line[0] == '#' || line[0] == '\r') continue;
    if (!header) {
      header = true;
      continue;
    }
    out.insert(atoi(line.c_str()));
  }
  return out;
}

}  // namespace

// The table and the committed rows agree: every effect a spell's script plays is swapped for a copy of that very row
// (visualeffects.mask.if_spellmask.txt's "# 4200 from 22"), every area likewise (vfx_persistent.maskcopies), and every
// cone whose spells.2da row names a masked copy is set at its cast. Breaks caught: a swap to a row that copies another
// effect (Fireball's burst played as Ice Storm's), a swap to a row the rows don't give, a cone left out of the table
// (its copy drawn with no mask set), a table row the rows file lacks.
TEST(SpellMasksAgreeWithTheirRows) {
  const std::string effects = ReadText("data/rows/visualeffects.mask.if_spellmask.txt");
  const std::string areas = ReadText("data/rows/vfx_persistent.maskcopies.if_spellmask.txt");
  const std::string cones = ReadText("data/rows/spells.mask.if_spellmask.txt");
  const std::map<int, int> effect_from = CopiedFrom(effects), area_from = CopiedFrom(areas);
  const std::set<int> effect_rows = RowsOf(effects), area_rows = RowsOf(areas), cone_rows = RowsOf(cones);
  CHECK_EQ(effect_from.size() >= 9 && area_from.size() >= 3 && cone_rows.size() >= 6, true);
  int swaps = 0;
  for (const SpellMaskEntry& e : SpellMasks()) {
    for (const auto& p : e.effects) {
      const auto found = effect_from.find(p.second);
      CHECK_EQ(found != effect_from.end() && found->second == p.first && effect_rows.count(p.second) == 1, true);
      ++swaps;
    }
    for (const auto& p : e.areas) {
      const auto found = area_from.find(p.second);
      CHECK_EQ(found != area_from.end() && found->second == p.first && area_rows.count(p.second) == 1, true);
      ++swaps;
    }
  }
  CHECK_EQ(swaps, 14);  // ten effects, four areas
  for (int row : cone_rows) {
    const SpellMaskEntry* e = SpellMaskOf(row);
    CHECK_EQ(e != nullptr && e->when == MaskWhen::kCast && e->hold_ms > 0, true);
  }
}

// The swaps. Breaks caught: another effect of the spell swapped (Fireball's flame on each target); a spell's swap used
// for another spell's script (Shatter's sound burst for a Bard's own); an unknown spell or row changed; the area swap
// read from the effects.
TEST(SpellMasksSwapOnlyTheirOwnLooks) {
  CHECK_EQ(MaskedEffectRow(58, 22), 4200);   // Fireball's burst
  CHECK_EQ(MaskedEffectRow(58, 18), 18);     // its flame on each target: not an area look
  CHECK_EQ(MaskedEffectRow(167, 183), 4201); // Shatter's sound burst
  CHECK_EQ(MaskedEffectRow(436, 183), 4201); // Thunderwave's
  CHECK_EQ(MaskedEffectRow(1, 183), 183);    // another spell's script: the game's own
  CHECK_EQ(MaskedEffectRow(-1, 22), 22);
  CHECK_EQ(MaskedEffectRow(543, 62), 4202);  // Ice Knife's burst
  CHECK_EQ(MaskedAreaRow(23, 3), 122);       // Cloudkill's cloud
  CHECK_EQ(MaskedAreaRow(23, 22), 22);
  CHECK_EQ(MaskedEffectRow(23, 3), 3);
  CHECK_EQ(MaskedAreaRow(58, 3), 3);
  const SpellMaskEntry* fog = SpellMaskOf(1180);
  CHECK_EQ(fog != nullptr && fog->when == MaskWhen::kLasting, true);
  const SpellMaskEntry* knife = SpellMaskOf(543);
  CHECK_EQ(knife != nullptr && knife->when == MaskWhen::kByCode, true);
  CHECK_EQ(SpellMaskOf(1272) == nullptr, true);  // Dragon's Breath's lightning exhale: no masked look
}

// Which mask a client shows. Breaks caught: the oldest shown, not the newest; one that ended still shown, or a lasting
// one (no end) taken as ended; another area's shown; the older one not back once the newer ends; an id tie-break the
// wrong way; no area (a client between areas) given one.
TEST(SpellMasksNewestShows) {
  const uint64_t hall = 0x1000, field = 0x2000;
  std::vector<HeldMask> masks = {{1, hall, 1000, 0}, {2, hall, 5000, 9000}, {3, field, 6000, 12000}};
  CHECK_EQ(MaskToShow({masks[0]}, hall, 2000), 1u);  // the fog alone, before the burst
  CHECK_EQ(MaskToShow(masks, hall, 6000), 2u);
  CHECK_EQ(MaskToShow(masks, hall, 9000), 1u);   // the burst over: the fog's back
  CHECK_EQ(MaskToShow(masks, field, 6500), 3u);
  CHECK_EQ(MaskToShow(masks, field, 12000), 0u);
  CHECK_EQ(MaskToShow(masks, 0, 6500), 0u);
  CHECK_EQ(MaskEnded(masks[0], 1000000), false);
  CHECK_EQ(MaskEnded(masks[1], 8999) || !MaskEnded(masks[1], 9000), false);
  masks.push_back({4, hall, 5000, 0});  // set the same moment: the later set (the higher id)
  CHECK_EQ(MaskToShow(masks, hall, 6000), 4u);
  masks.push_back({5, 0, 6000, 0});  // a mask with no area (its creature between areas) goes to no client
  CHECK_EQ(MaskToShow(masks, 0, 6500), 0u);
}

// The reach. Breaks caught: the margin lost or doubled; a negative size taken; a Fog Cloud's level misread, a tag of
// another area taken for one, or the case minded.
TEST(SpellMasksReach) {
  CHECK_EQ(fabsf(MaskReach(6.67f) - 9.67f) < 0.001f, true);
  CHECK_EQ(fabsf(MaskReach(-1.0f) - kMaskReachPast) < 0.001f, true);
  CHECK_EQ(fabsf(FogCloudRadius("NWN5E_FOG_CLOUD_1") - 6.096f) < 0.01f, true);
  CHECK_EQ(fabsf(FogCloudRadius("nwn5e_fog_cloud_9") - 54.86f) < 0.01f, true);
  CHECK_EQ(FogCloudRadius("NWN5E_ENTANGLE") == 0.0f && FogCloudRadius("VFX_PER_FOGKILL") == 0.0f, true);
  CHECK_EQ(FogCloudRadius("NWN5E_FOG_CLOUD_") == 0.0f && FogCloudRadius(nullptr) == 0.0f, true);
}
