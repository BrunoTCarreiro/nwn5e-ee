#pragma once

// DE (#172; the owner, 2026-11-20: "yes on all", after the destruction recon): consistent destruction effects. A
// placeable's debris is its own model's "die" animation (Explosion emitters throwing the game's chunk models); 71 of the
// game's breakable models have a "die" with no debris, 181 a smoke puff only and 491 no "die" at all, and a lit light's
// light is cut at once (the client's UnloadLight on dead / die). So nwn5e plays, from the game session's death hook:
// - material-matched debris for a breakable whose model has none of its own - wood, stone, metal (with sparks), cloth
//   (dust and scraps) and glass, each small and large - the game's own chunk models thrown by name (nothing of the
//   game's kept), on the game's own "die" recipe;
// - for any lit light's death, on top of its own debris: ember sparks, a short rising smoke column and a warm light
//   fading to nothing over about 1.5 s.
// Its rows and models; the game's call is core/anim_destruction.h. No engine code; unit-tested
// (tests/data/anim_debris_test.cpp). The effects: tools/data/debris/build.js (README.md there).

#include <string>

namespace data {

// The visualeffects.2da rows (data/rows/visualeffects.debris.txt; a test reads the file): material m (core's
// DebrisMaterial order: wood, stone, metal, cloth, glass), size s (small, large) at kDebrisFirstRow + 2 m + s -
// 2248 .. 2257 - and the light's death after them, 2258. One-shot effects (F) in the four root columns, after the
// tallies' 2215-2247 (the clash test, tests/data/anim_vfx_rows_test.cpp).
constexpr int kDebrisFirstRow = 2248;
constexpr int kDebrisMaterials = 5;
constexpr int kDebrisSizes = 2;
constexpr int kLightDeathRow = kDebrisFirstRow + kDebrisMaterials * kDebrisSizes;

// The row for `material` (0..4) and `size` (0 small, 1 large); -1 for anything else.
int DebrisRow(int material, int size);

// The material's word in its models' names and labels: "wood", "stone", "metal", "cloth", "glass"; "" for any other.
std::string DebrisMaterialCode(int material);

// The model: "n5_db_wood_s", "n5_db_wood_l" .. "n5_db_glass_l" (data/files; its draft tools/data/vfx/<model>.vfx); ""
// outside.
std::string DebrisModel(int material, int size);

// The light's death's model: "n5_db_light".
std::string LightDeathModel();

}  // namespace data
