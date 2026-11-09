#pragma once

// #39 (0.96.2; owner, 2026-11-09: "go with 1"): Magic Mark on the surface of a door's or a placeable's model where the
// caster's eyes met it, fixed to the part it hit (magic_mark_surface.cpp). 0.96.3 (owner, 2026-11-09: "A. but we
// should try to find a way to deal with walls that have floors behind them so that the wall is marked and not the
// floor"): where the cursor was at the click.

#include <stdint.h>

#include <optional>

#include "rules/mark_surface.h"

// The game's Gob::AttachToPart (vfx_nodes.cpp's trampoline).
using MarkAttachPartFn = void* (*)(void* self, void* target, void* part, int mode);

// vfx_nodes.cpp's attach hooks ask first: true when `self` is an upright Magic Mark put on a door or a placeable with
// a record (rules/mark_surface.h) and was put on its surface - attached to the part, turning with it (mode 0), with
// *result the game's attach's. False: the game's attach (or the effect nodes') goes on.
bool AttachMagicMark(void* self, void* target, int game_mode, MarkAttachPartFn attach_to_part, void** result);

// Read at InstallVfxNodes, after its attach hooks: false (logged) when the game's hit test or the models' links to
// their objects aren't as read. MagicMarkSurfaceReady: the cast puts a mark on the object itself only then (0.96.0's
// invisible object otherwise).
bool InstallMagicMarkSurface();
bool MagicMarkSurfaceReady();

// A player's own Magic Mark click (single player; the cast order's hook, a frame after it): the point under the
// cursor, its surface's normal and the camera it was seen from. `wall`: cast at a point, the cursor wasn't on the
// floor the game sent (a wall with floor behind it, a pillar, a ledge) - the order's point moved in front of it.
struct MarkClick {
  uint32_t spell = 0;
  uint32_t target = 0;  // the order's object; invalid for a point
  rules::Vec3f camera;
  rules::Vec3f point;
  rules::Vec3f normal;
  bool wall = false;
  int64_t when_ms = 0;
};
// The caster's click for this spell, taken once by the cast's impact; none when there's no fresh one.
std::optional<MarkClick> TakeMarkClick(uint32_t caster, uint32_t spell);
