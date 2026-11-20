#pragma once

// #172's destruction effects (DE; the owner, 2026-11-20: "yes on all", after the destruction recon) - the game session's
// call from its death hook (rules_dying.cpp's OnApplyDeath, after the original, for a placeable now dead):
// - DestructionDebrisVfx(material, size): material-matched debris, for a breakable whose model has no debris of its own
//   (a "die" with no detonate and chunk emitter, or no "die"). The material from placeables.2da SoundAppType ->
//   placeableobjsnds.2da ArmorType (wood; stone; plate = Metal; leather = Cloth; Ethereal or none: nothing), Glass from
//   the game session's hand list; the size from the row's Label or Destroyed sound (cb_bu_*sml = Small, *lrg = Large).
// - LightDeathVfx(): a lit light's death - ember sparks, a short rising smoke column and a warm light fading to nothing
//   over about 1.5 s - for any light lit as it dies, on top of its own debris.
//
// How to play them: each is a one-shot (F, no cessation) played at a location, so it outlives the placeable (the engine
// removes it 2 s after its death; the effects run 3.5 s to 5 s):
//   ApplyEffectAtLocation(DURATION_TYPE_INSTANT, EffectVisualEffect(row), <location>)
// - debris: the placeable's own location (its position, on the floor; its facing). The models lift their emitters
//   themselves - a small one's at 0.25-0.7 m, a large one's up to 1.3 m.
// - the light's death: its light's point - the position plus placeables.2da's LightOffsetX / Y turned by its facing,
//   and LightOffsetZ up (brazier 0.875 m, candelabra 1.90 m, lamp post 3.87 m). The engine keeps a location effect's z:
//   CNWSArea::PlayVisualEffect sends the position as given and the client's CNWCVisualEffect::SetPosition puts the
//   model there (its ground ray only turns an OrientWithGround effect; these rows have 0).
// What they look like is the animation session's (data/anim_debris.h, tools/data/debris).

#include "data/anim_debris.h"

// The breakable's material, in data::DebrisMaterialCode's order.
enum class DebrisMaterial { Wood = 0, Stone = 1, Metal = 2, Cloth = 3, Glass = 4 };

// Its size: Small (cb_bu_*sml), Large (cb_bu_*lrg).
enum class DebrisSize { Small = 0, Large = 1 };

// The visualeffects.2da row of the debris for `material` and `size`; -1 for anything the enums don't name.
inline int DestructionDebrisVfx(DebrisMaterial material, DebrisSize size) {
  return data::DebrisRow(static_cast<int>(material), static_cast<int>(size));
}

// The visualeffects.2da row of a lit light's death.
inline int LightDeathVfx() { return data::kLightDeathRow; }
