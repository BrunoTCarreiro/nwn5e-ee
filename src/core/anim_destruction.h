#pragma once

// #172's destruction effects (DE; the owner, 2026-11-20: "yes on all", after the destruction recon) - the game session's
// call from its destruction trigger (its CNWSPlaceable::DoDamage hook: a placeable reaching 0 HP, once per object):
// - DestructionDebrisVfx(material, size): material-matched debris, for a breakable whose model has no debris of its own
//   (a "die" with no detonate and chunk emitter, or no "die" - ModelHasDeathDebris, below, answers it: DH). The
//   material from placeables.2da SoundAppType ->
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

#include <string>

#include "data/anim_debris.h"

// The breakable's material, in data::DebrisMaterialCode's order.
enum class DebrisMaterial { Wood = 0, Stone = 1, Metal = 2, Cloth = 3, Glass = 4 };
static_assert(static_cast<int>(DebrisMaterial::Glass) + 1 == data::kDebrisMaterials,
              "every DebrisMaterial has its rows (data::kDebrisMaterials)");

// Its size: Small (cb_bu_*sml), Large (cb_bu_*lrg).
enum class DebrisSize { Small = 0, Large = 1 };
static_assert(static_cast<int>(DebrisSize::Large) + 1 == data::kDebrisSizes, "every DebrisSize has its rows (data::kDebrisSizes)");

// The visualeffects.2da row of the debris for `material` and `size`; -1 for anything the enums don't name.
inline int DestructionDebrisVfx(DebrisMaterial material, DebrisSize size) {
  return data::DebrisRow(static_cast<int>(material), static_cast<int>(size));
}

// The visualeffects.2da row of a lit light's death.
inline int LightDeathVfx() { return data::kLightDeathRow; }

// DH (#172; agreed with the game session and the owner, 2026-11-20): does the placeable's model already throw debris of
// its own when it breaks? The game session's destruction trigger (its CNWSPlaceable::DoDamage hook: a placeable reaching
// 0 HP, once per object) plays ours only when it doesn't:
//   if (!ModelHasDeathDebris(model)) <apply DestructionDebrisVfx(material, size)>;  LightDeathVfx() for a lit light.
// `model_resref`: the placeable's model (placeables.2da ModelName), any case.
// - True: the model throws chunks of its own when its "die" plays (or its "dead"). That is what the model does; whether
//   the death plays it is the game's - a lamp script that turns the death into "off" plays nothing (the game session's
//   kDouse guard handles that).
// - The rule (data/anim_death_debris.h): its "die" or "dead" animation - its own, else its supermodel chain's, as the
//   engine plays it - has a "detonate" event, and the model has an emitter it sets off that throws something: update
//   exactly "Explosion" (as the engine compares it), a chunk model named and there, and a birthrate above 0 (or that
//   animation's key of it). A "die" that only puffs smoke, chunks that throw nothing or aren't there, no detonate, no
//   "die" or "dead": false.
// - Served first: the model (its supermodels and chunk models too) is read through the engine's resource manager
//   (CExoResMan::Get, by data_serving's ReadGameResource), so the copy the game plays decides - LD's one-burst copies in
//   nwn5e's served folder win over the game's (the Cot, the Birdbath and Flag3 then have none: they get ours), a
//   module's haks over all.
// - Cached: one answer a model for the session (a model doesn't change while it runs), logged once ("destruction: ...").
// - Never throws or crashes: a missing, unreadable or odd model, or a name that is no resref -> false. Server side, on
//   the game's thread (thread-safe all the same).
bool ModelHasDeathDebris(const std::string& model_resref);
