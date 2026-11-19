#pragma once

// #171's target window: the target-count marker over each picked enemy - how many darts or rays are assigned to it
// (the owner, 2026-11-19: "a permanently visible indicator on top of each enemy saying how many darts/bolts are
// assigned to them, not how many are left"; look, glow and size: "A, A, as you said").
//
// The game session's one line: apply the row as a temporary duration effect tagged n5tw_count on the target, swap it as
// the count changes (remove the old effect, apply the new row), remove it when the window closes (OK / Cancel). The red
// glow (VFX_DUR_GLOW_LIGHT_RED, n5tw_mark) stays the "picked" mark; the number floats over the head.
//
// Paused is fine: the marker is geometry (data/anim_target_count.h), drawn the moment it's applied, and removing it
// takes it off the screen at once (the client's StopVisualEffect detaches it) - paused or not.
//
// What it shows (the animation session's): a bold white number with a dark outline on a 0.4 m dark-red disc, its centre
// 0.55 m over the head node, four discs facing out (one always faces the camera within 45 degrees), drawn unlit.

#include "data/anim_target_count.h"

// The visualeffects.2da row showing `count` (1..11); -1 for anything else (0, negatives, 12 or more).
inline int TargetCountMarkerVfx(int count) { return data::TargetCountRow(count); }
