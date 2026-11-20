#pragma once

// DH (#172; agreed with the game session and the owner, 2026-11-20): does a placeable's model already throw debris of its
// own when it breaks? The game session's destruction trigger plays DE's material debris (anim_debris.h) only for one
// that doesn't. Its game call: core/anim_destruction.h's ModelHasDeathDebris(model_resref), which reads the model the
// game serves and caches the answer.
//
// How the game breaks a placeable (the destruction recon, read from nwmain): the server sets its animation to "dead";
// the client plays "die" once, then loops "dead" until the object is deleted 2 s later. A "detonate" event in either sets
// off every Explosion emitter of the model's instance - each ExplosionEmitter registers its "Detonate" handler on the
// instance as it is created (MdlNodeEmitter::InternalCreateInstance), and an Explosion emitter with a chunk model throws
// those chunks.
//
// The rule: the model has its own debris when
// - its "die" or its "dead" (each the first of that name - any case - the model lists) has a "detonate" event (any case),
// - and its own geometry has an emitter whose update is "Explosion" (any case) and whose chunk name is set (an ASCII
//   model's "chunkName NULL" or an empty one is none) - the recon's "die with chunk debris" (525 of the game's placeable
//   models). Its render isn't looked at.
// An animation the model hasn't is its supermodel chain's, as the engine plays it (an animation's own, else its
// supermodel's - FindAnimClip's rule, up to eight models deep); the emitters are always the model's own (the supermodel
// lends animations, not nodes). So a "die" whose detonate only sets off a smoke puff (Explosion emitters with no
// chunk), a "die" with no detonate, or no "die" and no "dead" -> no debris of its own.
//
// No engine code; unit-tested (tests/data/anim_death_debris_test.cpp). The animations are vfx_model's ReadModelAnimations
// (LD), the emitters its ReadVfxBinaryModel / ReadVfxAsciiModel.

#include <functional>
#include <string>

namespace data {

// What one model's own bytes say (no supermodel followed).
struct ModelDeathParts {
  std::string supermodel;  // lower case; "" for none (NULL)
  bool die = false;        // it has its own "die"
  bool dead = false;       // ... its own "dead"
  int die_detonates = 0;   // "detonate" events in its own "die"
  int dead_detonates = 0;  // ... in its own "dead"
  int explosions = 0;        // its Explosion emitters
  int chunk_explosions = 0;  // of them, those with a chunk model
};

// A model's parts, compiled or ASCII. False and why when its animations can't be read (not a model, a list or an
// array past the file): nothing is known of it.
bool ReadModelDeathParts(const std::string& bytes, ModelDeathParts* parts, std::string* error);

// A model's bytes by its resref (lower case); false when there's no such model. The game reads them through its
// resource manager (served first), the tests from the install.
using ModelBytesLookup = std::function<bool(const std::string& lower_resref, std::string* bytes)>;

// The rule (above) for `model` as the game plays it: its own parts, its "die" / "dead" from its supermodel chain when
// it hasn't one. False for a model that isn't there or can't be read. `why` (may be null): one line of how it was
// decided, for the log.
bool HasOwnDeathDebris(const ModelBytesLookup& lookup, const std::string& model, std::string* why);

}  // namespace data
