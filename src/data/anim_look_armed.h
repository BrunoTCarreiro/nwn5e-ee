#pragma once

// #162 (owner, 2026-11-13: "go with G, B and D for #162"): the look of an armed smite - the game session's G is the icon
// and the panel line; this is the animation session's part:
// - B: as it arms, a flash and its sound - the game's own impact effect in the smite's colour, with the sound its row
//   plays (on the creature: the effect's own node; a hand node is a dev-only seam);
// - D: while it's armed, the game's own weapon glow - an item-property visual on the weapon in the right hand, tagged
//   (only ours ever comes off), temporary and refreshed while armed, so a save never keeps it for long; the weapon kept on
//   the creature (a local object), since the game may unequip it before the smite ends.
// NWScript run on the creature (OBJECT_SELF) by core/anim_look_player's PlaySpellLookArmed. No engine code; unit-tested.

#include <string>

namespace data {

struct ArmedLook {
  int flash = -1;  // visualeffects.2da row, -1: none
  int glow = -1;   // ITEM_VISUAL_* (iprp_visualfx.2da row), -1: none
};

// Divine Smite (1183): VFX_IMP_HOLY_AID (71), Holy (5); Searing Smite (1184): VFX_IMP_FLAME_S (61), Fire (3); Ensnaring
// Strike (1185): VFX_IMP_ACID_S (44), Acid (0). Any other row: none.
ArmedLook ArmedLookOf(int spell_row);

constexpr const char* kArmedGlowTag = "nwn5e_armed_glow";
constexpr float kArmedGlowSeconds = 12.0f;  // each glow lasts this long ...
constexpr int kArmedRefreshMs = 6000;       // ... and is put on again this often while armed

// The flash, then the glow (ArmedRefreshScript's).
std::string ArmedOnScript(const ArmedLook& look);
// The glow off the weapon kept, then on the weapon in the right hand now (kept in its place); nothing without a glow.
std::string ArmedRefreshScript(const ArmedLook& look);
// The glow off the weapon kept - its tagged properties only - and the record gone.
std::string ArmedOffScript();

}  // namespace data
