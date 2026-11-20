#pragma once

// #171's target window: the target-count marker over each picked enemy - how many darts or rays are assigned to it
// (the owner, 2026-11-19: "a permanently visible indicator on top of each enemy saying how many darts/bolts are
// assigned to them, not how many are left"). Its look since TF (the owner, 2026-11-20, after the probe): the count as
// tally marks facing the camera from every angle - "black on blue/orange/violet glow for magic missile, scorching ray
// and eldritch blast respectively".
//
// The game session's one line: apply the row as a temporary duration effect tagged n5tw_count on the target, swap it as
// the count changes (remove the old effect, apply the new row), remove it when the window closes (OK / Cancel). The red
// glow (VFX_DUR_GLOW_LIGHT_RED, n5tw_mark) stays the "picked" mark; the tally floats over the head.
//
// Paused is fine: the marker is geometry (data/anim_target_count.h) whose vertex shader turns it to the camera on every
// draw with no clock - drawn the moment it's applied, turning while paused - and removing it takes it off the screen at
// once (the client's StopVisualEffect detaches it; no cessation) - paused or not.
//
// What it shows (the animation session's): black tally bars (a five four bars struck through) on the spell's glow, 0.5 m
// wide, its centre 0.55 m over the head node, face-on to the camera.

#include "data/anim_target_count.h"

// The spell whose look the marker takes (data::TargetTallyStyleCode's order): Magic Missile's blue glow, Scorching Ray's
// orange, Eldritch Blast's violet.
enum class TargetCountStyle { MagicMissile = 0, ScorchingRay = 1, EldritchBlast = 2 };

// The visualeffects.2da row showing `count` (1..11) in `style`'s look; -1 for anything else (0, negatives, 12 or more,
// a style the enum doesn't name).
inline int TargetCountMarkerVfx(TargetCountStyle style, int count) {
  return data::TargetTallyRow(static_cast<int>(style), count);
}
