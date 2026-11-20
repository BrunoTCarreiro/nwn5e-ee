#pragma once

namespace rules {

// #172 DE / DH (the owner, 2026-11-20: "yes on all", after the animation session's destruction recon): what a breaking
// placeable throws when its model has no debris of its own (core/anim_destruction.h). The codes follow its
// DebrisMaterial / DebrisSize.

// The material from placeableobjsnds.2da's ArmorType (placeables.2da SoundAppType points there): wood 0, stone 1,
// plate (the metal rows) 2, leather (the cloth rows) 3; a glass one (IsGlassLabel) 4 whatever its sound row says;
// Ethereal (a force field) or anything else or none: -1, no debris.
int DebrisMaterialFor(const char* armor_type, bool glass);

// The game session's hand list of glass by placeables.2da's Label: a window, a mirror, a crystal, a bottle, a jar, or
// anything named glass.
bool IsGlassLabel(const char* label);

// The size from the sound row's Destroyed sound: one ending "lrg" (cb_bu_woodlrg, cb_bu_metallrg ...) Large 1; else
// (cb_bu_*sml, none) Small 0.
int DebrisSizeFor(const char* destroyed_sound);

}  // namespace rules
