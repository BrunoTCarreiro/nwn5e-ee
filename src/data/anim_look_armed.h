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

// #162 1A (owner, 2026-11-13: "1. A"): what an arming plays. With a look for the smite's row (made in the Spell & Attack
// Bench): the look on the caster - its cast (the clip swap, the caster's parts and sounds, the gesture) and its hit on
// the caster - instead of the fixed flash; only its hit when that look's cast already began for this caster (a cast that
// ran - an item's, a script's - was broadcast, and the cast listener started the look then: never twice). Without a
// look: the fixed flash. The glow is the same either way.
struct ArmedPlay {
  bool flash = false;    // the fixed flash (ArmedLook.flash)
  bool cast = false;     // the look's cast: its effects and sounds
  bool gesture = false;  // the gesture (ArmedGestureScript) - none for a still look
  bool hit = false;      // the look's hit on the caster
};
// `still`: the look has no animation (data::LookStill - the Animation card's None; owner, AS-11: "there should be an
// option for no animation"): its cast's effects and sounds and its hit, no gesture.
ArmedPlay ArmedPlayOf(bool has_look, bool cast_begun, bool still);
// The arming's script, run on the creature: the flash (play.flash) or none, the glow, and the gesture when play.gesture.
std::string ArmedArmScript(const ArmedLook& look, const ArmedPlay& play, int spell_row);
// The gesture: spells.2da `spell_row`'s own conjure and cast, faked at the caster (ActionCastFakeSpellAtObject: the
// look's clips swapped in by its cast, nothing cast) - only when the caster isn't doing anything else, so it's never
// queued behind an attack and never clears one. A player's arming cast is refused before it runs: this is its gesture.
std::string ArmedGestureScript(int spell_row);

}  // namespace data
