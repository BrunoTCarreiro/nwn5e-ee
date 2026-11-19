#pragma once

// #171's target window (the owner, 2026-11-19: "a permanently visible indicator on top of each enemy saying how many
// darts/bolts are assigned to them"; "A, A, as you said"): the target-count marker - the number on a dark-red disc over
// each picked enemy, 1 to 11 (Magic Missile from a 9th-level slot: 11 darts; Scorching Ray: 10 rays). Its rows and
// models; the game's call is core/anim_target_counts.h. No engine code; unit-tested.
//
// The window runs paused, and the engine animates no particle while paused (the TC diagnosis, docs/TOOLS-PLAN.md "The
// target-count marker": Scene::DisableAnimations on pause; Scene::DoEmitterBucket ticks an emitter only while the scene
// animates): a camera-facing sprite would appear only on unpause. So the marker is geometry, drawn at once: four discs facing out round the head (north, east, south,
// west), one-sided - a disc is never seen from behind, so the number is never mirrored - and upright on the world's
// axes (a position-only head attach), so never upside down. From any side one disc faces the camera within 45 degrees.

#include <string>

namespace data {

// The visualeffects.2da rows (data/rows/visualeffects.tcount.txt): count n is row kTargetCountFirstRow + n - 1, a
// lasting effect on the head (Imp_HeadCon_Node), its model TargetCountModel(n). 2200-2210: past the looks' 2100-2199
// (data/look_rows.h) and every other rows file's (tests/data/anim_vfx_rows_test.cpp).
constexpr int kTargetCountFirstRow = 2200;
constexpr int kTargetCountMax = 11;

// The row for `count` darts or rays: 1..kTargetCountMax; -1 otherwise (0, negatives, more).
int TargetCountRow(int count);

// The model (and texture) showing `count`: "n5_tcnt_01" .. "n5_tcnt_11" (data/files, tools/data/tcount/build.js); ""
// outside 1..kTargetCountMax.
std::string TargetCountModel(int count);

}  // namespace data
