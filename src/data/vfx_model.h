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

// A name usable as a resref and file name: letters, digits, '_', lower case,
// at most `max` characters ("" when nothing is left).
std::string VfxResRef(const std::string& text, size_t max = 16);

}  // namespace data
