#pragma once

// The VFX editor's effect (core/vfx_editor.cpp, `!vfxedit`; owner, 2026-10-25:
// "a small vfx editor where I can make new or edit existing VFX ... Something
// that can be used in game would be ideal"): emitters with the parameters of
// NWN's ASCII model emitters. Written as an ASCII effect model (the format of
// tools/data/death_save_marker.py and bleed_vfx.py), saved as a draft (key =
// value text), read back from a draft or from an ASCII model of ours.

#include <string>
#include <vector>

namespace data {

// One node's keys in one of the effect's animations (0.42.3; owner, 2026-10-26: "Go for A" -
// the game's effects move and turn the dummies their emitters hang under): rows of
// t x y z (position) and t ax ay az angle (orientation, axis-angle as an ASCII model
// writes it), linear between keys.
struct VfxKeys {
  std::string anim;  // impact | duration | cessation
  std::vector<float> position;
  std::vector<float> orientation;
};

// A node an emitter hangs under (a dummy in the model; in the game's models also a mesh
// or a light, kept as a dummy): where it sits under its parent, and its keys.
struct VfxNode {
  std::string name = "node";
  std::string parent;  // "" = the model's root
  float position[3] = {0.0f, 0.0f, 0.0f};
  float orientation[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  std::vector<VfxKeys> keys;
  // 0.42.5: a node under an emitter is where its point-to-point particles go - written as a
  // reference node (the game's p2p target: the emitter's first reference child) of this
  // model ("" = fx_ref, what all the game's compiled effects use).
  std::string ref_model;
};

struct VfxEmitter {
  std::string name = "emitter";
  std::string parent;  // the node it hangs under ("" = the model's root); position / orientation are under it
  std::string update = "Fountain";  // Fountain | Single | Explosion
  std::string render = "Normal";    // Normal | Motion_Blur | Aligned_to_World_z | Billboard_to_World_Z | ...
  std::string blend = "Normal";     // Normal | Lighten | Punch-Through
  std::string texture = "fxpa_sphere";
  // 0.42.6: a chunk model - each particle is that model (ChunkyParticle: the game makes one per
  // particle and plays its "impact"), and the emitter draws no sprites ("" = sprites).
  std::string chunk;
  float position[3] = {0.0f, 0.0f, 0.0f};  // metres from the node (a model's emitter without one: on it)
  // Its turn, axis x y z and angle (radians) as a model writes it: particles leave along the
  // emitter's z, its area lies in its x / y (0 0 0 0: not turned - straight up). 0.42.2.
  float orientation[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float birthrate = 10.0f;                 // particles a second
  float emit_seconds = 0.0f;               // on this long, then off (0: all the time)
  float life = 1.0f;                       // seconds
  float velocity = 1.0f;
  float randvel = 0.5f;
  float spread = 0.8f;  // radians
  float mass = 0.0f;    // gravity
  float size_start = 0.1f;
  float size_end = 0.1f;
  float color_start[3] = {1.0f, 1.0f, 1.0f};
  float color_end[3] = {1.0f, 1.0f, 1.0f};
  float alpha_start = 1.0f;
  float alpha_end = 0.0f;
  float xsize = 0.0f;  // spawn area, centimetres
  float ysize = 0.0f;
  int grid = 1;       // a texture sheet of grid x grid frames
  int frame_end = 0;  // frames 0..frame_end
  float blur_length = 0.0f;
  // 0.42.2, as the game draws them (PartEmitter::RenderEmitter): the sheet's frames a second
  // (0: the frame stays - what nwn5e wrote before) and the particles' spin, radians a second.
  float fps = 0.0f;
  float particle_rot = 0.0f;
  float bounce_co = 0.0f;
  bool inherit = false;  // particles move (and turn) with the emitter
  bool splat = false;    // flatten where they land
  bool bounce = false;
  bool random = false;  // a random frame each
  // 0.42.3: particles follow the creature's move (inherit_local) or the emitter's (inherit_part).
  bool inherit_local = false;
  bool inherit_part = false;
  // 0.42.4: drawn from both sides (twosidedtex - the game turns back-face culling off for it),
  // and deadspace (radians): a particle whose sheet would leave within this of edge-on to the
  // camera is turned toward it at birth (Particle::randomDirection; only with a spread).
  bool twosidedtex = false;
  float deadspace = 0.0f;
  // 0.42.5: point to point - its particles go to its target, the first node under it
  // ("" = off). "bezier": along a curve from the emitter to the target, p2p_bezier2 metres
  // out along the emitter's z and p2p_bezier3 back along the target's; how much of the way
  // the curve holds them is (age / life)^3 to the power combinetime (0: on it all the way).
  // "gravity": pulled toward it at grav m/s^2, drag of their speed kept a second, gone
  // within threshold metres. Without a target the game would crash: written as off.
  std::string p2p;
  float p2p_bezier2 = 0.0f;
  float p2p_bezier3 = 0.0f;
  float combinetime = 0.0f;
  float grav = 0.0f;
  float drag = 0.0f;
  float threshold = 0.0f;
  // 0.42.7: a Lightning emitter's bolt to its target (the first node under it, as for point to
  // point - without one it draws nothing): a new bolt every lightning_delay seconds, its points
  // (birthrate of them) jagged up to lightning_radius metres across and lightning_scale of a
  // step along (LightningEmitter::Update, PartEmitter::RecursiveFractal).
  float lightning_delay = 0.0f;
  float lightning_radius = 0.0f;
  float lightning_scale = 0.0f;
  std::vector<VfxKeys> keys;  // its own move / turn in the animations
};

// The most deadspace a draft or a written model gets: above about 0.39 the game's turning loop
// (randomDirection) never ends for some particles - the game would hang (simulated, 0.42.4; the
// game's own effects use 0.0037, 0.1745 and 0.2094).
constexpr float kVfxDeadspaceMax = 0.39f;

struct VfxEffect {
  std::string name = "n5_new";
  bool duration = true;         // D: stays while applied; F: plays once
  float seconds = 8.0f;         // the preview's length (D), the emission of an always-on emitter (F)
  std::string attach = "root";  // root | head | impact
  // The animations' lengths: impact (0: as long as its emitters need), duration (loops while
  // applied), cessation (as it's removed). 0.42.3.
  float lengths[3] = {0.0f, 2.0f, 1.0f};
  std::vector<VfxNode> nodes;  // parents before children
  std::vector<VfxEmitter> emitters;
};

// The keys of `anim` in a list (created when `create`; nullptr when absent).
VfxKeys* FindKeys(std::vector<VfxKeys>* list, const std::string& anim, bool create);

// The ASCII effect model, named `model` (its file's resref).
std::string WriteVfxModel(const VfxEffect& effect, const std::string& model);

// The draft text, and back (false and why when it doesn't parse).
std::string WriteVfxDraft(const VfxEffect& effect);
bool ReadVfxDraft(const std::string& text, VfxEffect* effect, std::string* error);

// An ASCII effect model's emitters: its geometry's emitter nodes, plus what
// its animations key (an impact birthrate that switches off gives
// emit_seconds; a size keyed from 0 gives the size it reaches).
bool ReadVfxAsciiModel(const std::string& text, VfxEffect* effect, std::string* error);

// A compiled (binary) model's emitters - the game's own effects, read where
// the player's game has them, nothing of them kept in nwn5e: each emitter node
// (node header 0x60 bytes, then the emitter header: update +0x18, render
// +0x38, blend +0x58, texture +0x78, x grid +0x0C, flags +0xD4) and its
// controllers (keys at node +0x44, values at +0x50, offsets from byte 12); an
// animation's copy of the node with a birthrate keyed to 0 gives emit_seconds.
bool ReadVfxBinaryModel(const std::string& bytes, VfxEffect* effect, std::string* error);

// A name usable as a resref and file name: letters, digits, '_', lower case,
// at most `max` characters ("" when nothing is left).
std::string VfxResRef(const std::string& text, size_t max = 16);

// --- Tile torches (light_sources.cpp; owner, 2026-10-25: "there are smaller
// torches that should also be interactive"). A tile's source lights - its wall
// torches - are dummy nodes <model>sl1 / <model>sl2 straight under the tile
// model's root, their flame spawned there by the client while the light is on.

// A node's position (its position controller's first key) in a compiled model,
// found by name (case-blind); false if there's no such node with one.
bool ReadModelNodePosition(const std::string& bytes, const std::string& node_name, float position[3]);

// From a tileset's .set text: the model of [TILE<id>] ("" if none), and the
// height of one level ([GENERAL] Transition, metres; 0 if none).
std::string TilesetTileModel(const std::string& set_text, int id);
float TilesetTransition(const std::string& set_text);

// A point of a tile's model in the area: turned by the tile's orientation (x
// 90 degrees counter-clockwise), moved to its tile's centre ((x + 0.5) and
// (y + 0.5) x 10 m) and up by its height (levels x transition).
void TilePointInArea(const float local[3], int tile_x, int tile_y, int orientation, int height, float transition,
                     float out[3]);

// A tile's fill lights (its two main lights: real lights in the tile model,
// often over a door or an arch) follow the lights round it (owner, 2026-10-25:
// "I've extinguished all the lights in this one corridor, but there's still a
// shadow being cast by my character" - "let's try B"): off when every light on
// the tile and the 8 round it is out and there is one at all - a tile with no
// light near keeps the module's. One byte per tile, row by row (y x width + x).
// With `strict` (indoors / underground; owner: "I see there are more of these
// invisible lights that cast shadows in the corridors" - "Let's try A"): off
// unless a lit light is on the tile or the 8 round it - no light near, no fill.
struct TileLight {
  int x = 0, y = 0;  // the tile it stands on
  bool lit = false;
};
std::vector<uint8_t> TileFillLightsOff(int width, int height, const std::vector<TileLight>& lights,
                                       bool strict = false);

// A compiled model's emitter changed in place (owner, 2026-10-25: "a torch held
// sideways will spew flames sideways as well, is that something that can be
// fixed?"): every node of that name (its animations' copies too) gets the
// controllers listed set to the value (each key; a controller it lacks is left
// out), and the emitter's flags lose `clear_flags`. The number of nodes changed.
// Controller types: 148 mass, 192 velocity; flag 0x40 = inherit (particles
// carried in the emitter's frame).
int PatchEmitter(std::string* bytes, const std::string& emitter_name, const std::vector<std::pair<uint32_t, float>>& values,
                 uint32_t clear_flags);

}  // namespace data
