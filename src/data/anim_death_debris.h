#pragma once

// DH (#172; agreed with the game session and the owner, 2026-11-20): does a placeable's model already throw debris of its
// own when it breaks? The game session's destruction trigger plays DE's material debris (anim_debris.h) only for one
// that doesn't. Its game call: core/anim_destruction.h's ModelHasDeathDebris(model_resref), which reads the model the
// game serves and caches the answer.
//
// How the game breaks a placeable (the destruction recon, read from nwmain): the server sets its animation to "dead";
// the client plays "die" once, then loops "dead" until the object is deleted 2 s later. A "detonate" event in either sets
// off every Explosion emitter of the model's instance - MdlNodeEmitter::InternalCreateInstance makes an ExplosionEmitter
// for a node whose update is exactly "Explosion" (case-sensitive) and registers its "Detonate" handler on the instance -
// and an Explosion emitter throws its birthrate of its chunk model.
//
// The rule: the model has its own debris when it has an emitter that throws something as it breaks:
// - its "die" or its "dead" (each the first of that name - any case - the model lists) has a "detonate" event (any case);
// - and its own geometry has an emitter whose update is "Explosion" (exactly; an ASCII "update explosion" is none) and
//   whose chunk name is set (an ASCII model's "chunkName NULL" or an empty one is none) - the recon's "die with chunk
//   debris" (525 of the game's placeable models); its render isn't looked at;
// - that emitter throws: its birthrate is above 0, or the animation that sets it off (a "die" or "dead" with the
//   detonate) keys its birthrate above 0 (plc_x0_jar's "die" does; plc_x0_cha's and plc_x0_ice's chunk emitter has
//   birthrate 0 and isn't keyed);
// - and its chunk model is there (the same lookup, a model: plc_trgegg's PLC_Chunk_Egg and px2_e01's PX2_S01 aren't in
//   the game).
// An animation the model hasn't is its supermodel chain's, as the engine plays it (FindAnimation: its own, else its
// supermodel's); the emitters are always the model's own (the supermodel lends animations, not nodes). nwn5e follows
// eight models at most (its own FindAnimClip's rule); the engine's FindAnimation has no cap and at the chain's end falls
// back to the last model's "default" - no game chain is deeper than 2, and no "default" there detonates. So a "die"
// whose detonate only sets off a smoke puff (Explosion emitters with no chunk), chunks that throw nothing or aren't
// there, a "die" with no detonate, or no "die" and no "dead" -> no debris of its own.
//
// No engine code; unit-tested (tests/data/anim_death_debris_test.cpp). The animations are vfx_model's ReadModelAnimations
// (LD), the emitters its ReadVfxBinaryModel / ReadVfxAsciiModel; the birthrates and their keys read here.

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace data {

// An Explosion emitter of the model's with a chunk model.
struct ChunkEmitter {
  std::string name;        // lower case
  std::string chunk;       // its chunk model, lower case
  float birthrate = 0.0f;  // its geometry's (controller 88 / "birthrate"; 0 when it has none)
};

// What one model's own bytes say (no supermodel followed).
struct ModelDeathParts {
  std::string supermodel;  // lower case; "" for none (NULL)
  bool die = false;        // it has its own "die"
  bool dead = false;       // ... its own "dead"
  int die_detonates = 0;   // "detonate" events in its own "die"
  int dead_detonates = 0;  // ... in its own "dead"
  int explosions = 0;        // its Explosion emitters
  int chunk_explosions = 0;  // of them, those with a chunk model named (chunk_emitters)
  std::vector<ChunkEmitter> chunk_emitters;
  // The nodes (lower case) whose birthrate its own "die" / "dead" keys above 0, by name - an inherited animation keys the
  // child's emitters so.
  std::set<std::string> die_births, dead_births;
};

// A model's parts, compiled or ASCII. False and why when its animations can't be read (not a model, a list or an
// array past the file): nothing is known of it.
bool ReadModelDeathParts(const std::string& bytes, ModelDeathParts* parts, std::string* error);

// A model's bytes by its resref (lower case); false when there's no such model. The game reads them through its
// resource manager (served first), the tests from the install.
using ModelBytesLookup = std::function<bool(const std::string& lower_resref, std::string* bytes)>;

// The rule (above) for `model` as the game plays it: its own parts, its "die" / "dead" from its supermodel chain when
// it hasn't one, its chunk models through `lookup`. True: it throws chunks of its own as "die" (or "dead") plays. False
// for a model that isn't there or can't be read. `why` (may be null): one line of how it was decided, for the log.
bool HasOwnDeathDebris(const ModelBytesLookup& lookup, const std::string& model, std::string* why);

}  // namespace data
