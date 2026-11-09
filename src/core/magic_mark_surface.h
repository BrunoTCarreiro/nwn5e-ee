#pragma once

// #39 (0.96.2; owner, 2026-11-09: "go with 1"): Magic Mark on the surface of a door's or a placeable's model where the
// caster's eyes met it, fixed to the part it hit (magic_mark_surface.cpp).

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
