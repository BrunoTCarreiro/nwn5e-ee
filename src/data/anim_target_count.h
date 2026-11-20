#pragma once

// #171's target window (the owner, 2026-11-19: "a permanently visible indicator on top of each enemy saying how many
// darts/bolts are assigned to them"): the target-count marker, 1 to 11 (Magic Missile from a 9th-level slot: 11 darts;
// Scorching Ray: 10 rays). Its rows and models; the game's call is core/anim_target_counts.h. No engine code;
// unit-tested. (The first marker, four discs round the head, TC, and the TP probe were retired 2026-11-20, the owner's
// call: replaced by TF below; docs/TOOLS-PLAN.md keeps their history.)
//
// The window runs paused, and the engine animates no particle while paused (the TC diagnosis, docs/TOOLS-PLAN.md "The
// target-count marker": Scene::DisableAnimations on pause; Scene::DoEmitterBucket ticks an emitter only while the scene
// animates): a camera-facing sprite would appear only on unpause. So the marker is geometry, drawn at once, whose
// vertex shader turns it to the camera every frame.

#include <string>

namespace data {

constexpr int kTargetCountMax = 11;

// --- TF: the final marker (the owner, 2026-11-20) -------------------------------------------------------------------
// After the probe (TP: rows 2211-2214, one quad our vertex shader turns to the camera every frame - drawn and turning
// while paused) the owner chose the face-on style, and "black on blue/orange/violet glow for magic missile, scorching ray
// and eldritch blast respectively": the count as tally marks (a five four bars struck through), black bars on the spell's
// glow, over each picked target, facing the camera from every angle (tools/data/tally/build.js; docs/TOOLS-PLAN.md "The
// final marker"). Two quads a model: the bars (black, alpha-blended) and the glow (additive, black where the bars are,
// 1 cm nearer the camera) - an additive glow can't draw black.
//
// Its styles, in core's TargetCountStyle's order: 0 Magic Missile (blue), 1 Scorching Ray (orange), 2 Eldritch Blast
// (violet). The visualeffects.2da rows (data/rows/visualeffects.tally.txt): style s, count n at
// kTargetTallyFirstRow + s x kTargetCountMax + n - 1 - 2215-2225, 2226-2236, 2237-2247, after the probe's (the clash
// test, tests/data/anim_vfx_rows_test.cpp); each a lasting effect on the head, its model TargetTallyModel(s, n).
constexpr int kTargetTallyFirstRow = 2215;
constexpr int kTargetTallyStyles = 3;

// The row for `style` and `count` (1..kTargetCountMax); -1 for any other style or count.
int TargetTallyRow(int style, int count);

// The style's two letters in its files' names: "mm", "sr", "eb"; "" for any other.
std::string TargetTallyStyleCode(int style);

// The model: "n5_tf_mm01" .. "n5_tf_eb11" (data/files); "" outside.
std::string TargetTallyModel(int style, int count);

// The glow quad's bitmap - the name its material (.mtr: our shader, its picture) and TXI (additive) go by: "n5_tfg_mm01"
// .. ; "" outside.
std::string TargetTallyGlowBitmap(int style, int count);

// The bars quad's bitmap, one for the three styles (the bars are black in all three): "n5_tfb_01" .. "n5_tfb_11"; ""
// outside 1..kTargetCountMax.
std::string TargetTallyBarsBitmap(int count);

}  // namespace data
