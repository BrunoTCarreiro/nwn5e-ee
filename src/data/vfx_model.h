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

struct VfxEmitter {
  std::string name = "emitter";
  std::string update = "Fountain";  // Fountain | Single | Explosion
  std::string render = "Normal";    // Normal | Motion_Blur | Aligned_to_World_z | Billboard_to_World_Z | ...
  std::string blend = "Normal";     // Normal | Lighten | Punch-Through
  std::string texture = "fxpa_sphere";
  float position[3] = {0.0f, 0.0f, 1.0f};  // metres from the node
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
  float bounce_co = 0.0f;
  bool inherit = false;  // particles move with the creature
  bool splat = false;    // flatten where they land
  bool bounce = false;
  bool random = false;  // a random frame each
};

struct VfxEffect {
  std::string name = "n5_new";
  bool duration = true;         // D: stays while applied; F: plays once
  float seconds = 8.0f;         // the preview's length (D), the emission of an always-on emitter (F)
  std::string attach = "root";  // root | head | impact
  std::vector<VfxEmitter> emitters;
};

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
