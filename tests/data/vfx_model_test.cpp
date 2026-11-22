#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "data/game_resources.h"
#include "data/vfx_model.h"

// The VFX editor's effect (data/vfx_model.h). Breaks caught: a draft that
// loses a field on the way back (the editor would reset it on load), a model
// that switches an emitter off at the wrong time or never, an F effect with
// a duration animation, our own bleed model (tools/data/bleed_vfx.py) read
// back with its splat flag or texture lost, a resref with a character the
// engine refuses.

using namespace data;

namespace {

VfxEffect Sample() {
  VfxEffect effect;
  effect.name = "test puddle";
  effect.duration = true;
  effect.seconds = 6.5f;
  effect.attach = "head";
  effect.shake_type = 2;
  effect.shake_delay = 0.5f;
  effect.shake_duration = 1.25f;
  VfxEmitter e;
  e.name = "drops";
  e.update = "Fountain";
  e.render = "Motion_Blur";
  e.texture = "blood";
  e.position[2] = 0.95f;
  e.orientation[0] = 1.0f;
  e.orientation[3] = 2.5f;
  e.fps = 16.0f;
  e.particle_rot = 0.5f;
  e.birthrate = 2.0f;
  e.emit_seconds = 0.5f;
  e.life = 6.0f;
  e.mass = 2.4f;
  e.size_start = 0.02f;
  e.size_end = 0.8f;
  e.color_start[0] = 0.33f;
  e.alpha_end = 0.0f;
  e.xsize = 12.0f;
  e.grid = 2;
  e.frame_end = 3;
  e.splat = true;
  e.random = true;
  e.twosidedtex = true;
  e.deadspace = 0.2f;
  e.chunk = "fx_meteor";
  effect.emitters.push_back(e);
  return effect;
}

bool Contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

int Milli(float value) { return static_cast<int>(value * 1000.0f + (value < 0 ? -0.5f : 0.5f)); }

}  // namespace

TEST(VfxDraftRoundTrip) {
  const VfxEffect effect = Sample();
  VfxEffect back;
  std::string error;
  CHECK_EQ(ReadVfxDraft(WriteVfxDraft(effect), &back, &error), true);
  CHECK_EQ(back.name == "test puddle", true);
  CHECK_EQ(back.duration, true);
  CHECK_EQ(Milli(back.seconds), 6500);
  CHECK_EQ(back.attach == "head", true);
  CHECK_EQ(back.shake_type, 2);
  CHECK_EQ(Milli(back.shake_delay), 500);
  CHECK_EQ(Milli(back.shake_duration), 1250);
  VfxEffect calm;  // no shake: no line (and an odd type reads as none)
  CHECK_EQ(WriteVfxDraft(calm).find("shake") == std::string::npos, true);
  CHECK_EQ(ReadVfxDraft("name = x\nshake = 7 1 1\n", &calm, &error) && calm.shake_type == 0, true);
  CHECK_EQ(back.emitters.size(), 1u);
  const VfxEmitter& e = back.emitters[0];
  CHECK_EQ(e.name == "drops", true);
  CHECK_EQ(e.render == "Motion_Blur", true);
  CHECK_EQ(e.texture == "blood", true);
  CHECK_EQ(Milli(e.position[2]), 950);
  CHECK_EQ(Milli(e.orientation[0]), 1000);
  CHECK_EQ(Milli(e.orientation[3]), 2500);
  CHECK_EQ(Milli(e.fps), 16000);
  CHECK_EQ(Milli(e.particle_rot), 500);
  CHECK_EQ(Milli(e.emit_seconds), 500);
  CHECK_EQ(Milli(e.size_end), 800);
  CHECK_EQ(Milli(e.color_start[0]), 330);
  CHECK_EQ(Milli(e.xsize), 12000);
  CHECK_EQ(e.grid, 2);
  CHECK_EQ(e.frame_end, 3);
  CHECK_EQ(e.splat, true);
  CHECK_EQ(e.random, true);
  CHECK_EQ(e.inherit, false);
  CHECK_EQ(e.twosidedtex, true);
  CHECK_EQ(Milli(e.deadspace), 200);
  CHECK_EQ(e.chunk == "fx_meteor", true);
  CHECK_EQ(ReadVfxDraft("name = x\n[weird]\n", &back, &error), false);
  // Deadspace past the most the game's loop survives is held there (0.42.4).
  CHECK_EQ(ReadVfxDraft("name = x\n[emitter a]\ndeadspace = 0.6\n", &back, &error), true);
  if (!back.emitters.empty()) CHECK_EQ(Milli(back.emitters[0].deadspace), 390);
}

// #145: an effect's light through the draft - its section after the emitters, the same bytes as the bench's
// writer (vfx_bench/check_effect.js holds the same text), and back.
const char* const kLightSection =
    "\n[light glow]\nparent = orbit\nposition = 0 0 1.25\ncolor = 1 0.5 0.25\nradius = 6\nbrightness = 1.5\nshadow = 1\n"
    "duration_color = 0 1 0.5 0.25 1 0.5 0.25 0.125 2 1 0.5 0.25\nduration_radius = 0 6 2 6\n";

TEST(VfxLightThroughTheDraft) {
  VfxEffect effect;
  VfxLight light;
  light.name = "glow";
  light.parent = "orbit";
  light.position[2] = 1.25f;
  light.color[1] = 0.5f;
  light.color[2] = 0.25f;
  light.radius = 6.0f;
  light.multiplier = 1.5f;
  light.shadow = true;
  VfxKeys k;
  k.anim = "duration";
  k.color = {0, 1, 0.5f, 0.25f, 1, 0.5f, 0.25f, 0.125f, 2, 1, 0.5f, 0.25f};
  k.radius = {0, 6, 2, 6};
  light.keys.push_back(k);
  effect.lights.push_back(light);
  const std::string text = WriteVfxDraft(effect);
  const size_t n = strlen(kLightSection);
  CHECK_EQ(text.size() >= n && text.compare(text.size() - n, n, kLightSection) == 0, true);
  VfxEffect back;
  std::string error;
  CHECK_EQ(ReadVfxDraft(text, &back, &error), true);
  CHECK_EQ(back.lights.size(), 1u);
  if (!back.lights.empty()) {
    const VfxLight& l = back.lights[0];
    CHECK_EQ(l.name == "glow" && l.parent == "orbit", true);
    CHECK_EQ(Milli(l.position[2]), 1250);
    CHECK_EQ(Milli(l.color[1]), 500);
    CHECK_EQ(Milli(l.radius), 6000);
    CHECK_EQ(Milli(l.multiplier), 1500);
    CHECK_EQ(l.shadow, true);
    CHECK_EQ(l.keys.size() == 1 && l.keys[0].anim == "duration" && l.keys[0].color.size() == 12u && l.keys[0].radius.size() == 4u, true);
  }
  CHECK_EQ(WriteVfxDraft(back) == text, true);
  // A light with the defaults: no parent, shadow or key lines.
  VfxEffect plain;
  plain.lights.emplace_back();
  const std::string p = WriteVfxDraft(plain);
  CHECK_EQ(p.find("\n[light light]\nposition = 0 0 0\ncolor = 1 1 1\nradius = 5\nbrightness = 1\n") != std::string::npos, true);
  CHECK_EQ(p.find("shadow") == std::string::npos && p.find("parent") == std::string::npos, true);
  // A light before an emitter (a draft written by hand): each keeps its own lines.
  VfxEffect mixed;
  CHECK_EQ(ReadVfxDraft("name = x\n[light a]\nradius = 3\n[emitter e]\nbirthrate = 7\n", &mixed, &error), true);
  CHECK_EQ(mixed.lights.size() == 1 && mixed.emitters.size() == 1, true);
  if (mixed.lights.size() == 1 && mixed.emitters.size() == 1) {
    CHECK_EQ(Milli(mixed.lights[0].radius), 3000);
    CHECK_EQ(Milli(mixed.emitters[0].birthrate), 7000);
  }
}

TEST(VfxDraftWithoutALightUnchanged) {
  const std::string text = WriteVfxDraft(Sample());
  CHECK_EQ(text.find("[light") == std::string::npos && text.find("_color") == std::string::npos &&
               text.find("_radius") == std::string::npos, true);
  VfxEffect back;
  std::string error;
  CHECK_EQ(ReadVfxDraft(text, &back, &error) && back.lights.empty(), true);
}

// #145: the model writes each light as a light node of its geometry - under its parent (a node of the effect), where it
// sits, the flags the game's own effect lights carry (Task 0: dynamic, affecting creatures, no flare, priority 3, fading),
// its colour, radius and brightness - and, in each animation, its colour and radius keys; the loop lasts its keys.
std::string Section(const std::string& text, const std::string& from, const std::string& to) {
  const size_t a = text.find(from), b = a == std::string::npos ? a : text.find(to, a);
  return a == std::string::npos || b == std::string::npos ? std::string() : text.substr(a, b - a);
}

TEST(VfxModelWritesLight) {
  VfxEffect effect;
  VfxNode orbit;
  orbit.name = "orbit";
  effect.nodes.push_back(orbit);
  VfxLight light;
  light.name = "glow";
  light.parent = "orbit";
  light.position[2] = 1.25f;
  light.color[1] = 0.5f;
  light.color[2] = 0.25f;
  light.radius = 6.0f;
  light.multiplier = 1.5f;
  light.shadow = true;
  VfxKeys loop;
  loop.anim = "duration";
  loop.color = {0, 1, 0.5f, 0.25f, 1, 0.5f, 0.25f, 0.125f, 2.5f, 1, 0.5f, 0.25f};
  loop.radius = {0, 6, 2.5f, 6};
  VfxKeys start;
  start.anim = "impact";
  start.radius = {0, 0, 0.5f, 6};
  light.keys = {start, loop};
  effect.lights.push_back(light);
  const std::string model = WriteVfxModel(effect, "n5t");
  const std::string geometry = Section(model, "beginmodelgeom n5t", "endmodelgeom n5t");
  const std::string node = Section(geometry, "node light n5t_l0_glow", "endnode");
  CHECK_EQ(node.empty(), false);
  for (const char* line : {"  parent n5t_n0_orbit\n", "  position 0 0 1.25\n", "  color 1 0.5 0.25\n", "  radius 6\n",
                           "  multiplier 1.5\n", "  shadow 1\n", "  isdynamic 1\n", "  affectdynamic 1\n", "  ambientonly 0\n",
                           "  lightpriority 3\n", "  fadingLight 1\n", "  flareradius 0\n"}) {
    CHECK_EQ(node.find(line) != std::string::npos, true);
  }
  const std::string duration = Section(model, "newanim duration n5t", "doneanim duration n5t");
  const std::string in_loop = Section(duration, "node light n5t_l0_glow", "endnode");
  CHECK_EQ(in_loop.find("  parent n5t_n0_orbit\n") != std::string::npos, true);
  CHECK_EQ(in_loop.find("  colorkey 3\n    0 1 0.5 0.25\n    1 0.5 0.25 0.125\n    2.5 1 0.5 0.25\n") != std::string::npos, true);
  CHECK_EQ(in_loop.find("  radiuskey 2\n    0 6\n    2.5 6\n") != std::string::npos, true);
  CHECK_EQ(duration.find("  length 2.5\n") != std::string::npos, true);  // the loop lasts its keys (2.5 s, over the 2 s default)
  const std::string impact = Section(model, "newanim impact n5t", "doneanim impact n5t");
  CHECK_EQ(Section(impact, "node light n5t_l0_glow", "endnode").find("  radiuskey 2\n    0 0\n    0.5 6\n") != std::string::npos, true);
  const std::string cessation = Section(model, "newanim cessation n5t", "doneanim cessation n5t");
  // Listed, with no keys of its own: it goes out with the effect (a 0.4 s fade from its radius).
  CHECK_EQ(Section(cessation, "node light n5t_l0_glow", "endnode").find("  radiuskey 2\n    0 6\n    0.4 0\n") != std::string::npos, true);
  CHECK_EQ(cessation.find("node light n5t_l0_glow") != std::string::npos, true);
  // Without a parent: under the model's root; without shadow: shadow 0.
  VfxEffect plain;
  plain.lights.emplace_back();
  const std::string p = Section(WriteVfxModel(plain, "n5p"), "node light n5p_l0_light", "endnode");
  CHECK_EQ(p.find("  parent n5p\n") != std::string::npos && p.find("  shadow 0\n") != std::string::npos, true);
}

// #145, the review: a game light's own flags - ambient only, dynamic, priority, fading - kept through the draft (a line
// each, only off the default, so a new light's draft is unchanged; vfx_bench/check_effect.js holds the same lines), the
// model and the ASCII reader (either spelling of the dynamic flag), so a game effect saved again lights as it did.
TEST(VfxLightKeepsItsFlags) {
  VfxEffect effect;
  VfxEmitter flame;
  flame.name = "flame";
  effect.emitters.push_back(flame);
  VfxLight light;
  light.name = "glow";
  light.shadow = true;
  light.ambient_only = true;
  light.dynamic = false;
  light.priority = 5;
  light.fading = false;
  effect.lights.push_back(light);
  const std::string text = WriteVfxDraft(effect);
  CHECK_EQ(text.find("\nshadow = 1\nambientonly = 1\ndynamic = 0\npriority = 5\nfading = 0\n") != std::string::npos, true);
  VfxEffect back;
  std::string error;
  CHECK_EQ(ReadVfxDraft(text, &back, &error), true);
  CHECK_EQ(back.lights.size(), 1u);
  if (back.lights.size() == 1) {
    const VfxLight& l = back.lights[0];
    CHECK_EQ(l.ambient_only && !l.dynamic && l.priority == 5 && !l.fading, true);
  }
  CHECK_EQ(WriteVfxDraft(back) == text, true);
  const std::string model = WriteVfxModel(effect, "n5f");
  const std::string node = Section(Section(model, "beginmodelgeom n5f", "endmodelgeom n5f"), "node light n5f_l0_glow", "endnode");
  for (const char* line : {"  ambientonly 1\n", "  isdynamic 0\n", "  lightpriority 5\n", "  fadingLight 0\n"}) {
    CHECK_EQ(node.find(line) != std::string::npos, true);
  }
  VfxEffect read;
  CHECK_EQ(ReadVfxAsciiModel(model, &read, &error), true);
  CHECK_EQ(read.lights.size(), 1u);
  if (read.lights.size() == 1) {
    const VfxLight& l = read.lights[0];
    CHECK_EQ(l.ambient_only && !l.dynamic && l.priority == 5 && !l.fading, true);
  }
  // The other spelling of the dynamic flag (nDynamicType, as some ASCII models write it).
  std::string other = model;
  const size_t at = other.find("  isdynamic 0");
  CHECK_EQ(at != std::string::npos, true);
  if (at != std::string::npos) other.replace(at, 13, "  nDynamicType 0");
  VfxEffect spelled;
  CHECK_EQ(ReadVfxAsciiModel(other, &spelled, &error), true);
  CHECK_EQ(spelled.lights.size() == 1 && !spelled.lights[0].dynamic, true);
}

// #145, the owner's test ("Stop in game: the light starts again, gone about 3 s later"; a fade out of 0.1 s cures it): a
// light without radius keys in its effect's last animation goes out with it - its radius from itself to 0 over 0.4 s,
// at the cessation's start (a lasting effect) or the impact's end (a one-shot; all of it if shorter) - as the game's own
// effects end theirs dark; a light with its own radius keys there keeps just those.
TEST(VfxLightGoesOutWithItsEffect) {
  const auto light_in = [](const std::string& model, const char* anim, const char* name) {
    return Section(Section(model, std::string("newanim ") + anim + " n5o", std::string("doneanim ") + anim + " n5o"),
                   std::string("node light n5o_l0_") + name, "endnode");
  };
  VfxEffect lasting;
  lasting.duration = true;
  VfxLight glow;
  glow.name = "glow";
  glow.radius = 6.0f;
  lasting.lights.push_back(glow);
  std::string model = WriteVfxModel(lasting, "n5o");
  CHECK_EQ(light_in(model, "cessation", "glow").find("  radiuskey 2\n    0 6\n    0.4 0\n") != std::string::npos, true);
  // The fade only at the end: the duration just lights it at its radius (#161 VA-2: lit again after a cessation).
  CHECK_EQ(light_in(model, "duration", "glow").find("  radiuskey 1\n    0 6\n") != std::string::npos &&
               light_in(model, "duration", "glow").find(" 0\n", light_in(model, "duration", "glow").find("    0 6\n") + 8) == std::string::npos,
           true);
  lasting.lengths[2] = 0.25f;  // a short end: the whole of it
  model = WriteVfxModel(lasting, "n5o");
  CHECK_EQ(light_in(model, "cessation", "glow").find("  radiuskey 2\n    0 6\n    0.25 0\n") != std::string::npos, true);
  VfxKeys own;
  own.anim = "cessation";
  own.radius = {0, 6, 0.75f, 0};
  lasting.lights[0].keys.push_back(own);
  lasting.lengths[2] = 1.0f;
  model = WriteVfxModel(lasting, "n5o");
  const std::string kept = light_in(model, "cessation", "glow");
  CHECK_EQ(kept.find("  radiuskey 2\n    0 6\n    0.75 0\n") != std::string::npos && kept.find("0.4 0") == std::string::npos, true);
  VfxEffect once;
  once.duration = false;
  once.lengths[0] = 2.0f;
  once.lights.push_back(glow);
  model = WriteVfxModel(once, "n5o");
  CHECK_EQ(light_in(model, "impact", "glow").find("  radiuskey 2\n    1.6 6\n    2 0\n") != std::string::npos, true);
}

// #145 Task 3: a light read back from an ASCII model (the one the writer makes: draft -> model -> draft keeps it, its
// parent node kept, its keys in each animation).
TEST(VfxLightThroughTheAsciiModel) {
  VfxEffect effect;
  VfxNode orbit;
  orbit.name = "orbit";
  effect.nodes.push_back(orbit);
  VfxNode lamp;  // only the light hangs under it: kept for the light's sake
  lamp.name = "lamp";
  effect.nodes.push_back(lamp);
  VfxEmitter flame;
  flame.name = "flame";
  flame.parent = "orbit";
  effect.emitters.push_back(flame);
  VfxLight light;
  light.name = "glow";
  light.parent = "lamp";
  light.position[2] = 1.25f;
  light.color[1] = 0.5f;
  light.color[2] = 0.25f;
  light.radius = 6.0f;
  light.multiplier = 1.5f;
  light.shadow = true;
  VfxKeys loop;
  loop.anim = "duration";
  loop.color = {0, 1, 0.5f, 0.25f, 1, 0.5f, 0.25f, 0.125f, 2, 1, 0.5f, 0.25f};
  loop.radius = {0, 6, 2, 6};
  VfxKeys start;
  start.anim = "impact";
  start.radius = {0, 0, 0.5f, 6};
  light.keys = {start, loop};
  effect.lights.push_back(light);
  VfxEffect back;
  std::string error;
  CHECK_EQ(ReadVfxAsciiModel(WriteVfxModel(effect, "n5r"), &back, &error), true);
  CHECK_EQ(back.lights.size(), 1u);
  if (back.lights.size() != 1) return;
  const VfxLight& l = back.lights[0];
  CHECK_EQ(l.name == "n5r_l0_glow", true);
  CHECK_EQ(l.parent == "n5r_n1_lamp", true);
  CHECK_EQ(Milli(l.position[2]), 1250);
  CHECK_EQ(Milli(l.color[1]), 500);
  CHECK_EQ(Milli(l.radius), 6000);
  CHECK_EQ(Milli(l.multiplier), 1500);
  CHECK_EQ(l.shadow, true);
  bool parent_kept = false;
  for (const VfxNode& n : back.nodes) parent_kept = parent_kept || n.name == "n5r_n1_lamp";
  CHECK_EQ(parent_kept, true);
  const VfxKeys* d = nullptr;
  const VfxKeys* i = nullptr;
  for (const VfxKeys& k : l.keys) {
    if (k.anim == "duration") d = &k;
    if (k.anim == "impact") i = &k;
  }
  CHECK_EQ(d && d->color.size() == 12u && d->radius.size() == 4u && Milli(d->color[7]) == 125, true);
  CHECK_EQ(i && i->radius.size() == 4u && Milli(i->radius[3]) == 6000, true);
}

// #145 Task 3: a light read from a compiled model, as the game's own effects keep theirs (Task 0): node flags 0x002 at
// +0x5C, shadow at +0xB0, colour 76, radius 88, multiplier 140 - and its loop keys of colour and radius.
TEST(VfxReadsALightFromACompiledModel) {
  std::string b(0x1800, '\0');
  const auto put32 = [&](size_t at, uint32_t v) { memcpy(&b[at], &v, 4); };
  const auto put16 = [&](size_t at, uint16_t v) { memcpy(&b[at], &v, 2); };
  const auto putf = [&](size_t at, float v) { memcpy(&b[at], &v, 4); };
  const auto puts = [&](size_t at, const char* text) { memcpy(&b[at], text, strlen(text)); };
  const size_t base = 12;
  struct Ctrl {
    uint32_t type;
    int columns;
    std::vector<float> times, values;
  };
  const auto node = [&](size_t at, const char* name, size_t keys, const std::vector<Ctrl>& ctrls) {
    puts(at + 0x10, name);
    const size_t data = keys + 0x40;
    uint16_t used = 0;
    put32(at + 0x44, static_cast<uint32_t>(keys - base));
    put32(at + 0x48, static_cast<uint32_t>(ctrls.size()));
    put32(at + 0x50, static_cast<uint32_t>(data - base));
    for (size_t i = 0; i < ctrls.size(); ++i) {
      const Ctrl& c = ctrls[i];
      const size_t k = keys + 12 * i;
      put32(k, c.type);
      put16(k + 4, static_cast<uint16_t>(c.times.size()));
      put16(k + 6, used);
      for (float t : c.times) putf(data + 4 * used++, t);
      put16(k + 8, used);
      for (float v : c.values) putf(data + 4 * used++, v);
      b[k + 10] = static_cast<char>(c.columns);
    }
    put32(at + 0x54, used);
  };
  const auto children = [&](size_t at, size_t array, const std::vector<size_t>& kids) {
    put32(at + 0x38, static_cast<uint32_t>(array - base));
    put32(at + 0x3C, static_cast<uint32_t>(kids.size()));
    for (size_t i = 0; i < kids.size(); ++i) put32(array + 4 * i, static_cast<uint32_t>(kids[i] - 0x10 - base));
  };
  puts(base + 8, "test_lit");
  // The geometry: root -> [Group01 (1 m up) -> eyes (an emitter), Lamp (0.5 m along x) -> glow (a light)].
  put32(base + 0x48, static_cast<uint32_t>(0x100 - 0x10 - base));
  node(0x100, "test_lit", 0x900, {{8, 3, {0}, {0, 0, 0}}});
  children(0x100, 0x1600, {0x200, 0x500});
  node(0x200, "Group01", 0x980, {{8, 3, {0}, {0, 0, 1}}});
  children(0x200, 0x1610, {0x300});
  node(0x500, "Lamp", 0xD80, {{8, 3, {0}, {0.5f, 0, 0}}});
  children(0x500, 0x1640, {0x400});
  node(0x300, "eyes", 0xA00, {{8, 3, {0}, {0, 0, 0}}});
  puts(0x300 + 0x60 + 0x18, "Fountain");
  puts(0x300 + 0x60 + 0x38, "Normal");
  puts(0x300 + 0x60 + 0x78, "fxpa_eye1");
  node(0x400, "glow", 0xA80, {{8, 3, {0}, {0, 0, 0.7f}}, {88, 1, {0}, {10}}, {140, 1, {0}, {1}}, {76, 3, {0}, {1, 0.75f, 0}}});
  put32(0x400 + 0x5C, 0x003);  // header | light
  put32(0x400 + 0xB0, 1);      // shadow
  put32(0x400 + 0xA0, 5);      // priority (the review: its flags kept)
  put32(0x400 + 0xA4, 1);      // ambient only
  put32(0x400 + 0xA8, 0);      // dynamic type: not dynamic
  put32(0x400 + 0xAC, 1);      // affect dynamic
  put32(0x400 + 0xB8, 0);      // fading light: no
  // The duration (2 s): the light's colour flickers, its radius holds.
  puts(0x1000 + 8, "duration");
  put32(0x1000 + 0x48, static_cast<uint32_t>(0x1100 - 0x10 - base));
  putf(0x1000 + 0x70, 2.0f);
  node(0x1100, "test_lit", 0xB00, {{8, 3, {0}, {0, 0, 0}}});
  children(0x1100, 0x1620, {0x1200});
  node(0x1200, "Group01", 0xB80, {});
  children(0x1200, 0x1630, {0x1300});
  node(0x1300, "glow", 0xC00, {{76, 3, {0, 1, 2}, {1, 0.75f, 0, 1, 1, 1, 1, 0.75f, 0}}, {88, 1, {0, 2}, {10, 8}}});
  // The model header's list of its animations (+0x78 its headers' offsets, +0x7C their count), as the game's models have it.
  put32(base + 0x78, static_cast<uint32_t>(0x1700 - base));
  put32(base + 0x7C, 1);
  put32(0x1700, static_cast<uint32_t>(0x1000 - base));
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(b, &effect, &error), true);
  CHECK_EQ(effect.lights.size(), 1u);
  if (effect.lights.size() != 1) return;
  const VfxLight& l = effect.lights[0];
  CHECK_EQ(l.name == "glow" && l.parent == "Lamp", true);
  CHECK_EQ(Milli(l.position[2]), 700);
  CHECK_EQ(Milli(l.color[1]), 750);
  CHECK_EQ(Milli(l.radius), 10000);
  CHECK_EQ(Milli(l.multiplier), 1000);
  CHECK_EQ(l.shadow, true);
  CHECK_EQ(l.priority, 5);
  CHECK_EQ(l.ambient_only && !l.dynamic && !l.fading, true);
  bool parent_kept = false;
  for (const VfxNode& n : effect.nodes) parent_kept = parent_kept || n.name == "Lamp";
  CHECK_EQ(parent_kept, true);
  const VfxKeys* d = nullptr;
  for (const VfxKeys& k : l.keys) {
    if (k.anim == "duration") d = &k;
  }
  CHECK_EQ(d && d->color.size() == 12u && Milli(d->color[5]) == 1000 && d->radius.size() == 4u && Milli(d->radius[3]) == 8000, true);
}

TEST(VfxModelWithoutALightUnchanged) {
  CHECK_EQ(WriteVfxModel(Sample(), "n5s").find("node light") == std::string::npos, true);
}

TEST(VfxModelKeysEmission) {
  const std::string model = WriteVfxModel(Sample(), "n5vx0001");
  CHECK_EQ(Contains(model, "newmodel n5vx0001"), true);
  CHECK_EQ(Contains(model, "  splat 1"), true);
  CHECK_EQ(Contains(model, "  texture blood"), true);
  CHECK_EQ(Contains(model, "  birthrate 0"), true);  // switched off after 0.5 s: keyed in the impact
  CHECK_EQ(Contains(model, "    0.5 2"), true);
  CHECK_EQ(Contains(model, "    0.51 0"), true);
  CHECK_EQ(Contains(model, "newanim duration n5vx0001"), true);
  CHECK_EQ(Contains(model, "  twosidedtex 1"), true);
  CHECK_EQ(Contains(model, "  chunkName fx_meteor\n"), true);
  VfxEffect sprites = Sample();
  sprites.emitters[0].chunk.clear();
  const std::string plain = WriteVfxModel(sprites, "n5vx0004");
  CHECK_EQ(Contains(plain, "chunkName"), false);  // none: no line (the game would read "NULL" as a model)
  CHECK_EQ(Contains(plain, "\n\nendnode"), false);  // and no blank line left in its place
  CHECK_EQ(Contains(model, "  deadspace 0.2\n"), true);
  VfxEffect wild = Sample();
  wild.emitters[0].deadspace = 0.8f;  // the game would hang on it: written as the most it takes
  CHECK_EQ(Contains(WriteVfxModel(wild, "n5vx0003"), "  deadspace 0.39\n"), true);
  VfxEffect once = Sample();
  once.duration = false;
  once.emitters[0].emit_seconds = 0.0f;
  const std::string burst = WriteVfxModel(once, "n5vx0002");
  CHECK_EQ(Contains(burst, "newanim duration"), false);  // F: the impact only
  CHECK_EQ(Contains(burst, "    6.5 2"), true);            // an always-on emitter runs for `seconds`
  // The writer's model reads back.
  VfxEffect back;
  std::string error;
  CHECK_EQ(ReadVfxAsciiModel(model, &back, &error), true);
  CHECK_EQ(back.emitters.size(), 1u);
  CHECK_EQ(Milli(back.emitters[0].birthrate), 2000);
  CHECK_EQ(Milli(back.emitters[0].emit_seconds), 500);
  CHECK_EQ(back.emitters[0].twosidedtex, true);
  CHECK_EQ(Milli(back.emitters[0].deadspace), 200);
  CHECK_EQ(back.emitters[0].chunk == "fx_meteor", true);
  CHECK_EQ(back.duration, true);
}

TEST(VfxReadsOurBleedModel) {
  std::ifstream file("data/files/n5_bleed.mdl", std::ios::binary);
  std::stringstream text;
  text << file.rdbuf();
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxAsciiModel(text.str(), &effect, &error), true);
  CHECK_EQ(effect.name == "n5_bleed", true);
  CHECK_EQ(effect.emitters.empty(), false);
  CHECK_EQ(effect.emitters[0].texture == "blood", true);
  CHECK_EQ(effect.emitters[0].splat, true);
  CHECK_EQ(Milli(effect.emitters[0].birthrate), 2000);
  CHECK_EQ(VfxResRef("My Effect-2!") == "myeffect2", true);
  CHECK_EQ(VfxResRef("abcdefghijklmnopqrstuvwxyz").size(), 16u);
}

// A compiled model laid out as the game's are (built here, nothing of the
// game's): one emitter node with its header, flags and two controllers.
// Breaks caught: the emitter header's offsets, the flag bits, the controller
// arrays' base (byte 12) or layout drifting from what the game's models use.
TEST(VfxReadsACompiledModel) {
  std::string b(0x400, '\0');
  const auto put32 = [&](size_t at, uint32_t v) { memcpy(&b[at], &v, 4); };
  const auto putf = [&](size_t at, float v) { memcpy(&b[at], &v, 4); };
  const auto puts = [&](size_t at, const char* text) { memcpy(&b[at], text, strlen(text)); };
  const size_t base = 12, node = 0x100, emitter = node + 0x60;
  puts(base + 8, "test_fx");
  puts(node + 0x10, "spray");
  put32(emitter + 0x0C, 2);  // x grid
  puts(emitter + 0x18, "Fountain");
  puts(emitter + 0x38, "Motion_Blur");
  puts(emitter + 0x58, "Lighten");
  puts(emitter + 0x78, "fxpa_sphere");
  put32(emitter + 0xD4, 0x200 | 0x040);  // splat, inherit
  putf(emitter, 0.2094f);                // deadspace
  puts(emitter + 0xB8, "Fx_RockChunkSml");  // chunkName (the game's own case)
  put32(emitter + 0xC8, 1);              // twosidedtex
  const size_t keys = 0x300, data = 0x340;
  put32(node + 0x44, static_cast<uint32_t>(keys - base));
  put32(node + 0x48, 2);
  put32(node + 0x50, static_cast<uint32_t>(data - base));
  put32(node + 0x54, 5);
  // birthrate: 1 row, time at 0, value at 1, 1 column; colorStart: 1 row, value at 2, 3 columns.
  put32(keys, 88);
  b[keys + 4] = 1; b[keys + 6] = 0; b[keys + 8] = 1; b[keys + 10] = 1;
  put32(keys + 12, 108);
  b[keys + 16] = 1; b[keys + 18] = 0; b[keys + 20] = 2; b[keys + 22] = 3;
  putf(data, 0.0f);
  putf(data + 4, 150.0f);
  putf(data + 8, 0.3f);
  putf(data + 12, 0.1f);
  putf(data + 16, 0.0f);
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(b, &effect, &error), true);
  CHECK_EQ(effect.name == "test_fx", true);
  CHECK_EQ(effect.emitters.size(), 1u);
  const VfxEmitter& e = effect.emitters[0];
  CHECK_EQ(e.name == "spray", true);
  CHECK_EQ(e.update == "Fountain", true);
  CHECK_EQ(e.render == "Motion_Blur", true);
  CHECK_EQ(e.blend == "Lighten", true);
  CHECK_EQ(e.texture == "fxpa_sphere", true);
  CHECK_EQ(e.grid, 2);
  CHECK_EQ(e.splat, true);
  CHECK_EQ(e.inherit, true);
  CHECK_EQ(e.bounce, false);
  CHECK_EQ(Milli(e.deadspace), 209);
  CHECK_EQ(e.twosidedtex, true);
  CHECK_EQ(e.chunk == "fx_rockchunksml", true);
  CHECK_EQ(Milli(e.birthrate), 150000);
  CHECK_EQ(Milli(e.color_start[0]), 300);
  CHECK_EQ(Milli(e.color_start[1]), 100);
  CHECK_EQ(ReadVfxBinaryModel("newmodel x", &effect, &error), false);
}

// A lasting effect read from a compiled model laid out as the game's (0.42.0 /
// 0.42.1; vdr_eleshield, vcm_hitsonc): the geometry's node tree and each
// animation's (impact, duration, cessation), whose copies of the emitter nodes
// carry the same emitter header but only what that animation keys - and come
// first in the file. Breaks caught: an animation's copy read as the emitter
// (vcm_hitsonc's cloud 1 m up with default values, the VFX Bench's preview
// too high); the cessation's switch-off (every lasting game effect has one)
// read as "on for 0.03 s" - the elemental shield died after 2.6 s; a lasting
// effect's impact switch-off cutting an emitter short that the duration keeps
// on; an impact burst (an emitter otherwise off) losing its on-time.
TEST(VfxLastingEffectKeepsEmitting) {
  std::string b(0x2000, '\0');
  const auto put32 = [&](size_t at, uint32_t v) { memcpy(&b[at], &v, 4); };
  const auto put16 = [&](size_t at, uint16_t v) { memcpy(&b[at], &v, 2); };
  const auto putf = [&](size_t at, float v) { memcpy(&b[at], &v, 4); };
  const auto puts = [&](size_t at, const char* text) { memcpy(&b[at], text, strlen(text)); };
  const size_t base = 12;
  // A node (the reader's terms: its header + 0x10) named `name`, with a birthrate
  // controller and, if `z` isn't negative, a position (0, 0, z).
  const auto node = [&](size_t at, const char* name, size_t keys, const std::vector<float>& times,
                        const std::vector<float>& values, float z) {
    puts(at + 0x10, name);
    const uint16_t rows = static_cast<uint16_t>(times.size());
    const size_t data = keys + 0x20;
    put32(at + 0x44, static_cast<uint32_t>(keys - base));
    put32(at + 0x48, z < 0.0f ? 1 : 2);
    put32(at + 0x50, static_cast<uint32_t>(data - base));
    put32(at + 0x54, rows * 2u + (z < 0.0f ? 0u : 4u));
    put32(keys, 88);
    put16(keys + 4, rows);
    put16(keys + 6, 0);
    put16(keys + 8, rows);
    b[keys + 10] = 1;
    for (size_t i = 0; i < rows; ++i) {
      putf(data + 4 * i, times[i]);
      putf(data + 4 * (rows + i), values[i]);
    }
    if (z >= 0.0f) {
      put32(keys + 12, 8);
      put16(keys + 16, 1);
      put16(keys + 18, static_cast<uint16_t>(2 * rows));
      put16(keys + 20, static_cast<uint16_t>(2 * rows + 1));
      b[keys + 22] = 3;
      putf(data + 4 * (2 * rows + 3), z);
    }
  };
  const auto emitter = [&](size_t at) {
    puts(at + 0x60 + 0x18, "Fountain");
    puts(at + 0x60 + 0x38, "Normal");
    puts(at + 0x60 + 0x78, "fxpa_sphere");
  };
  // A tree: `at`'s children (an array of header offsets at `array`).
  const auto children = [&](size_t at, size_t array, const std::vector<size_t>& kids) {
    put32(at + 0x38, static_cast<uint32_t>(array - base));
    put32(at + 0x3C, static_cast<uint32_t>(kids.size()));
    for (size_t i = 0; i < kids.size(); ++i) put32(array + 4 * i, static_cast<uint32_t>(kids[i] - 0x10 - base));
  };
  const auto anim = [&](size_t header, const char* name, size_t root) {
    puts(header + 8, name);
    put32(header + 0x48, static_cast<uint32_t>(root - 0x10 - base));
    putf(header + 0x70, 1.0f);
  };
  puts(base + 8, "test_shield");
  // The impact: a dummy with the two emitters' copies (before the geometry in the file).
  anim(0x100, "impact", 0x190);
  puts(0x190 + 0x10, "test_shield");
  children(0x190, 0x1F00, {0x200, 0x300});
  node(0x200, "flash", 0x1800, {0.0f, 0.2f, 0.21f}, {30.0f, 30.0f, 0.0f}, -1.0f);  // a burst at the start
  emitter(0x200);
  node(0x300, "ring", 0x1840, {0.0f, 0.5f, 0.51f}, {50.0f, 50.0f, 0.0f}, -1.0f);  // on again in the duration
  emitter(0x300);
  anim(0x400, "duration", 0x480);
  node(0x480, "ring", 0x1880, {0.0f, 1.0f}, {50.0f, 50.0f}, -1.0f);
  emitter(0x480);
  anim(0x600, "cessation", 0x690);
  puts(0x690 + 0x10, "test_shield");
  children(0x690, 0x1F10, {0x700, 0x800});
  node(0x700, "ring", 0x18C0, {0.0f, 0.033f}, {50.0f, 0.0f}, -1.0f);  // how it ends
  emitter(0x700);
  node(0x800, "flash", 0x1900, {0.0f, 0.033f}, {30.0f, 0.0f}, -1.0f);
  emitter(0x800);
  // The geometry: a dummy, the ring (on, half a metre up) and the flash (off).
  put32(base + 0x48, static_cast<uint32_t>(0x1000 - 0x10 - base));
  puts(0x1000 + 0x10, "test_shield");
  children(0x1000, 0x1F20, {0x1100, 0x1300});
  node(0x1100, "ring", 0x1940, {0.0f}, {50.0f}, 0.5f);
  emitter(0x1100);
  node(0x1300, "flash", 0x1980, {0.0f}, {0.0f}, 0.0f);
  emitter(0x1300);
  // The model header's list of its animations (+0x78 their headers' offsets, +0x7C their count), as the game's models have it.
  put32(base + 0x78, static_cast<uint32_t>(0x1F40 - base));
  put32(base + 0x7C, 3);
  for (int i = 0; i < 3; ++i) put32(0x1F40 + 4 * i, static_cast<uint32_t>(std::vector<size_t>{0x100, 0x400, 0x600}[i] - base));
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(b, &effect, &error), true);
  CHECK_EQ(effect.duration, true);
  CHECK_EQ(effect.emitters.size(), 2u);
  const VfxEmitter* ring = nullptr;
  const VfxEmitter* flash = nullptr;
  for (const VfxEmitter& e : effect.emitters) (e.name == "ring" ? ring : flash) = &e;
  CHECK_EQ(ring != nullptr && flash != nullptr, true);
  if (!ring || !flash) return;
  CHECK_EQ(Milli(ring->position[2]), 500);
  CHECK_EQ(Milli(ring->birthrate), 50000);
  CHECK_EQ(Milli(ring->emit_seconds), 0);
  CHECK_EQ(Milli(flash->position[2]), 0);
  CHECK_EQ(Milli(flash->birthrate), 30000);
  CHECK_EQ(Milli(flash->emit_seconds), 200);
}

// An emitter under a dummy that is moved, turned and keyed (0.42.2 / 0.42.3; the game's
// Premonition hangs its eye emitters under dummies 1.3 m up, its Dummy01 spins in the
// duration): the dummy is kept as a node with its keys, the emitter under it as the model
// has it. Breaks caught: the emitter's own position read alone with no parent (at the
// feet); the parent's place baked in and its motion lost; keys of a node or an emitter not
// read; a quaternion key turned into the wrong axis-angle; the animation's length lost.
TEST(VfxEmitterUnderAMovedDummy) {
  std::string b(0x1400, '\0');
  const auto put32 = [&](size_t at, uint32_t v) { memcpy(&b[at], &v, 4); };
  const auto put16 = [&](size_t at, uint16_t v) { memcpy(&b[at], &v, 2); };
  const auto putf = [&](size_t at, float v) { memcpy(&b[at], &v, 4); };
  const auto puts = [&](size_t at, const char* text) { memcpy(&b[at], text, strlen(text)); };
  const size_t base = 12;
  struct Ctrl {
    uint32_t type;
    int columns;
    std::vector<float> times, values;
  };
  // A node (the reader's terms: its header + 0x10) with controllers, keys at `keys`.
  const auto node = [&](size_t at, const char* name, size_t keys, const std::vector<Ctrl>& ctrls) {
    puts(at + 0x10, name);
    const size_t data = keys + 0x40;
    uint16_t used = 0;
    put32(at + 0x44, static_cast<uint32_t>(keys - base));
    put32(at + 0x48, static_cast<uint32_t>(ctrls.size()));
    put32(at + 0x50, static_cast<uint32_t>(data - base));
    for (size_t i = 0; i < ctrls.size(); ++i) {
      const Ctrl& c = ctrls[i];
      const size_t k = keys + 12 * i;
      put32(k, c.type);
      put16(k + 4, static_cast<uint16_t>(c.times.size()));
      put16(k + 6, used);
      for (float t : c.times) putf(data + 4 * used++, t);
      put16(k + 8, used);
      for (float v : c.values) putf(data + 4 * used++, v);
      b[k + 10] = static_cast<char>(c.columns);
    }
    put32(at + 0x54, used);
  };
  const auto children = [&](size_t at, size_t array, size_t kid) {
    put32(at + 0x38, static_cast<uint32_t>(array - base));
    put32(at + 0x3C, 1);
    put32(array, static_cast<uint32_t>(kid - 0x10 - base));
  };
  const float r = 0.70710678f;
  puts(base + 8, "test_omen");
  // The geometry: root -> Group01 (1.3 m up, a quarter turn round z) -> eyes (1 m along x).
  put32(base + 0x48, static_cast<uint32_t>(0x100 - 0x10 - base));
  node(0x100, "test_omen", 0x900, {{8, 3, {0}, {0, 0, 0}}});
  children(0x100, 0x1300, 0x200);
  node(0x200, "Group01", 0x980, {{8, 3, {0}, {0.5f, 0, 1.3f}}, {20, 4, {0}, {0, 0, r, r}}});
  children(0x200, 0x1310, 0x300);
  node(0x300, "eyes", 0xA00, {{8, 3, {0}, {1, 0, 0}}});
  puts(0x300 + 0x60 + 0x18, "Fountain");
  puts(0x300 + 0x60 + 0x38, "Normal");
  puts(0x300 + 0x60 + 0x78, "fxpa_eye1");
  // The duration (1.5 s): Group01 turns a half round z; the eyes move along x; the root's
  // single key is no motion.
  puts(0x500 + 8, "duration");
  put32(0x500 + 0x48, static_cast<uint32_t>(0x600 - 0x10 - base));
  putf(0x500 + 0x70, 1.5f);
  node(0x600, "test_omen", 0xA80, {{8, 3, {0}, {0, 0, 0}}});
  children(0x600, 0x1320, 0x700);
  node(0x700, "Group01", 0xB00, {{20, 4, {0, 1.5f}, {0, 0, 0, 1, 0, 0, 1, 0}}});
  children(0x700, 0x1330, 0x800);
  node(0x800, "eyes", 0xB80, {{8, 3, {0, 0.75f}, {1, 0, 0, 2, 0, 0}}});
  // The model header's list of its animations (+0x78 its headers' offsets, +0x7C their count), as the game's models have it.
  put32(base + 0x78, static_cast<uint32_t>(0x1380 - base));
  put32(base + 0x7C, 1);
  put32(0x1380, static_cast<uint32_t>(0x500 - base));
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(b, &effect, &error), true);
  CHECK_EQ(effect.emitters.size(), 1u);
  CHECK_EQ(effect.nodes.size(), 1u);
  if (effect.emitters.empty() || effect.nodes.empty()) return;
  const VfxEmitter& e = effect.emitters[0];
  const VfxNode& n = effect.nodes[0];
  CHECK_EQ(e.parent == "Group01", true);
  CHECK_EQ(Milli(e.position[0]), 1000);
  CHECK_EQ(Milli(e.position[2]), 0);
  CHECK_EQ(n.name == "Group01", true);
  CHECK_EQ(n.parent.empty(), true);
  CHECK_EQ(Milli(n.position[0]), 500);
  CHECK_EQ(Milli(n.position[2]), 1300);
  CHECK_EQ(Milli(n.orientation[2]), 1000);  // a quarter round z
  CHECK_EQ(Milli(n.orientation[3]), 1571);
  CHECK_EQ(Milli(effect.lengths[1]), 1500);
  const VfxKeys* turn = FindKeys(const_cast<std::vector<VfxKeys>*>(&n.keys), "duration", false);
  CHECK_EQ(turn != nullptr, true);
  if (turn) {
    CHECK_EQ(turn->orientation.size(), 10u);  // 2 keys: t + axis-angle
    CHECK_EQ(turn->position.empty(), true);
    if (turn->orientation.size() == 10) {
      CHECK_EQ(Milli(turn->orientation[5]), 1500);  // the 2nd key's time
      CHECK_EQ(Milli(turn->orientation[8]), 1000);  // round z
      CHECK_EQ(Milli(turn->orientation[9]), 3142);  // a half turn
    }
  }
  const VfxKeys* move = FindKeys(const_cast<std::vector<VfxKeys>*>(&e.keys), "duration", false);
  CHECK_EQ(move != nullptr, true);
  if (move) {
    CHECK_EQ(move->position.size(), 8u);
    if (move->position.size() == 8) CHECK_EQ(Milli(move->position[5]), 2000);
  }
}

// Point to point (0.42.5; the game's Bigby's hands fly to a reference node under each emitter,
// Greater Dispelling pulls its streaks to one): a compiled model's p2p emitter with its
// settings and its reference child kept as its target; the same through an ASCII model, the
// draft, and the model nwn5e writes. Breaks caught: the p2p flags or their kind (p2p_sel)
// lost; a setting's controller not read; the target not kept, or kept off its emitter, or its
// reference model lost; the written target a dummy (the game finds none - and crashes) or
// written before its emitter; a p2p emitter with no target written as p2p (a crash).
// Names the game can find (0.43.3): every node a written model has is named in at most 31
// characters (the game keeps 31 and finds a parent by the whole name), the names are unique,
// and every "parent" line - the geometry's and the animations' - names a node defined before
// it. A long effect name (16), long node and emitter names and many of them (two-digit tags)
// at once. Breaks caught: the 2026-10-26 crash - Build from parts' tilted rings wrote
// 32-33-character node names; the game cut them, didn't find the parent, and read through null.
TEST(VfxModelNodeNamesFit) {
  VfxEffect effect;
  effect.name = "n5_long_effect01";
  for (int i = 0; i < 24; ++i) {
    VfxNode n;
    n.name = "flare_gather_ringspin_" + std::to_string(i);
    if (i % 2) n.parent = "flare_gather_ringspin_" + std::to_string(i - 1);
    effect.nodes.push_back(n);
  }
  for (int i = 0; i < 24; ++i) {
    VfxEmitter e;
    e.name = "a_very_long_emitter_name_" + std::to_string(i);
    e.parent = "flare_gather_ringspin_" + std::to_string(i);
    if (i < 3) e.p2p = "gravity";
    VfxKeys move;
    move.anim = "duration";
    move.position = {0, 0, 0, 0, 1, 1, 0, 0};
    e.keys.push_back(move);
    effect.emitters.push_back(e);
  }
  for (int i = 0; i < 3; ++i) {
    VfxNode t;
    t.name = "a_very_long_emitter_name_" + std::to_string(i) + "_target";
    t.parent = "a_very_long_emitter_name_" + std::to_string(i);
    effect.nodes.push_back(t);
  }
  const std::string model = WriteVfxModel(effect, effect.name);
  std::istringstream in(model);
  std::string line;
  std::set<std::string> geometry;
  std::set<std::string> seen;  // the nodes defined so far in the section being read
  bool in_geometry = false, ok_lengths = true, ok_unique = true, ok_parents = true;
  std::string bad;
  while (std::getline(in, line)) {
    std::istringstream words(line);
    std::string first, second, third;
    words >> first >> second >> third;
    if (first == "beginmodelgeom") in_geometry = true;
    if (first == "endmodelgeom") in_geometry = false;
    if (first == "newanim") seen.clear();
    if (first == "node") {
      if (third.size() > 31) { ok_lengths = false; bad = third; }
      if (in_geometry && !geometry.insert(third).second) { ok_unique = false; bad = third; }
      seen.insert(third);
    }
    if (first == "parent" && second != "NULL" && !seen.count(second)) { ok_parents = false; bad = second; }
  }
  CHECK_EQ(ok_lengths, true);
  CHECK_EQ(ok_unique, true);
  CHECK_EQ(ok_parents, true);
  if (!ok_lengths || !ok_unique || !ok_parents) printf("    the name: %s\n", bad.c_str());
  CHECK_EQ(geometry.size(), 1u + 27u + 24u);  // the model's own node, the nodes, the emitters
  // The model still reads back with its tree.
  VfxEffect read;
  std::string error;
  CHECK_EQ(ReadVfxAsciiModel(model, &read, &error), true);
  CHECK_EQ(read.emitters.size(), 24u);
}

TEST(VfxPointToPoint) {
  std::string b(0x1800, '\0');
  const auto put32 = [&](size_t at, uint32_t v) { memcpy(&b[at], &v, 4); };
  const auto put16 = [&](size_t at, uint16_t v) { memcpy(&b[at], &v, 2); };
  const auto putf = [&](size_t at, float v) { memcpy(&b[at], &v, 4); };
  const auto puts = [&](size_t at, const char* text) { memcpy(&b[at], text, strlen(text)); };
  const size_t base = 12;
  struct Ctrl {
    uint32_t type;
    std::vector<float> values;
  };
  const auto node = [&](size_t at, const char* name, size_t keys, const std::vector<Ctrl>& ctrls) {
    puts(at + 0x10, name);
    const size_t data = keys + 0x80;
    uint16_t used = 0;
    put32(at + 0x44, static_cast<uint32_t>(keys - base));
    put32(at + 0x48, static_cast<uint32_t>(ctrls.size()));
    put32(at + 0x50, static_cast<uint32_t>(data - base));
    for (size_t i = 0; i < ctrls.size(); ++i) {
      const size_t k = keys + 12 * i;
      put32(k, ctrls[i].type);
      put16(k + 4, 1);
      put16(k + 6, used);
      putf(data + 4 * used++, 0.0f);
      put16(k + 8, used);
      for (float v : ctrls[i].values) putf(data + 4 * used++, v);
      b[k + 10] = static_cast<char>(ctrls[i].values.size());
    }
    put32(at + 0x54, used);
  };
  const auto children = [&](size_t at, size_t array, std::vector<size_t> kids) {
    put32(at + 0x38, static_cast<uint32_t>(array - base));
    put32(at + 0x3C, static_cast<uint32_t>(kids.size()));
    for (size_t i = 0; i < kids.size(); ++i) put32(array + 4 * i, static_cast<uint32_t>(kids[i] - 0x10 - base));
  };
  puts(base + 8, "test_beam");
  put32(base + 0x48, static_cast<uint32_t>(0x100 - 0x10 - base));
  node(0x100, "test_beam", 0x800, {{8, {0, 0, 0}}});
  children(0x100, 0x1700, {0x200});
  // The emitter (bezier) and, under it, a dummy first and then its reference node.
  node(0x200, "spray", 0x900, {{8, {0, 0, 1}}, {140, {2.5f}}, {188, {0.1f}}, {152, {1.5f}}, {156, {0.75f}},
                               {124, {0.5f}}, {120, {0.25f}}});
  puts(0x200 + 0x60 + 0x18, "Fountain");
  puts(0x200 + 0x60 + 0x38, "Normal");
  puts(0x200 + 0x60 + 0x78, "fxpa_spark");
  put32(0x200 + 0x60 + 0xD4, 0x1 | 0x2);
  children(0x200, 0x1710, {0x300, 0x400});
  node(0x300, "spray_dummy", 0xA00, {{8, {0, 0, 5}}});
  put32(0x300 + 0x5C, 0x1);
  node(0x400, "spray_ref", 0xB00, {{8, {0, 0.5f, 2}}});
  put32(0x400 + 0x5C, 0x11);
  puts(0x400 + 0x60, "fx_ref_cold");
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(b, &effect, &error), true);
  CHECK_EQ(effect.emitters.size(), 1u);
  if (effect.emitters.empty()) return;
  const VfxEmitter& e = effect.emitters[0];
  CHECK_EQ(e.p2p == "bezier", true);
  CHECK_EQ(Milli(e.grav), 2500);
  CHECK_EQ(Milli(e.threshold), 100);
  CHECK_EQ(Milli(e.p2p_bezier2), 1500);
  CHECK_EQ(Milli(e.p2p_bezier3), 750);
  CHECK_EQ(Milli(e.drag), 500);
  CHECK_EQ(Milli(e.combinetime), 250);
  CHECK_EQ(effect.nodes.size(), 1u);  // the target, not the dummy beside it
  if (effect.nodes.empty()) return;
  CHECK_EQ(effect.nodes[0].name == "spray_ref", true);
  CHECK_EQ(effect.nodes[0].parent == "spray", true);
  CHECK_EQ(effect.nodes[0].ref_model == "fx_ref_cold", true);
  CHECK_EQ(Milli(effect.nodes[0].position[1]), 500);
  CHECK_EQ(Milli(effect.nodes[0].position[2]), 2000);
  // Through the draft.
  VfxEffect back;
  CHECK_EQ(ReadVfxDraft(WriteVfxDraft(effect), &back, &error), true);
  CHECK_EQ(back.emitters.size() == 1 && back.nodes.size() == 1, true);
  if (back.emitters.empty() || back.nodes.empty()) return;
  CHECK_EQ(back.emitters[0].p2p == "bezier", true);
  CHECK_EQ(Milli(back.emitters[0].p2p_bezier3), 750);
  CHECK_EQ(Milli(back.emitters[0].combinetime), 250);
  CHECK_EQ(Milli(back.emitters[0].threshold), 100);
  CHECK_EQ(back.nodes[0].parent == "spray", true);
  CHECK_EQ(back.nodes[0].ref_model == "fx_ref_cold", true);
  // The model nwn5e writes: the target a reference node after its emitter; gravity's p2p_sel 0.
  back.emitters[0].p2p = "gravity";
  const std::string model = WriteVfxModel(back, "n5vx0009");
  const size_t emitter_at = model.find("node emitter n5vx0009_0_spray");
  const size_t target_at = model.find("node reference n5vx0009_n0_spray_ref");
  CHECK_EQ(emitter_at != std::string::npos && target_at != std::string::npos && emitter_at < target_at, true);
  CHECK_EQ(Contains(model, "  parent n5vx0009_0_spray\n  position 0 0.5 2"), true);
  CHECK_EQ(Contains(model, "  refModel fx_ref_cold\n  reattachable 0"), true);
  CHECK_EQ(Contains(model, "  p2p 1\n  p2p_sel 0"), true);
  CHECK_EQ(Contains(model, "  grav 2.5"), true);
  CHECK_EQ(Contains(model, "node dummy n5vx0009_n0_spray_ref"), true);  // in the animations, as the game's
  // ... and read back from it.
  VfxEffect read;
  CHECK_EQ(ReadVfxAsciiModel(model, &read, &error), true);
  CHECK_EQ(read.emitters.size() == 1 && read.nodes.size() == 1, true);
  if (read.emitters.empty() || read.nodes.empty()) return;
  CHECK_EQ(read.emitters[0].p2p == "gravity", true);
  CHECK_EQ(Milli(read.emitters[0].grav), 2500);
  CHECK_EQ(read.nodes[0].parent == read.emitters[0].name, true);
  CHECK_EQ(read.nodes[0].ref_model == "fx_ref_cold", true);
  // No target: written as off.
  VfxEffect lonely = back;
  lonely.nodes.clear();
  CHECK_EQ(Contains(WriteVfxModel(lonely, "n5vx0010"), "  p2p 0\n"), true);
}

// Lightning (0.42.7; the game's Doom, Death Armor and Power Word: Kill bolts): a compiled
// model's Lightning emitter with its bolt settings and its reference child kept as its
// target; the same through the draft and the model nwn5e writes (read back). Breaks caught:
// a bolt setting's controller not read, or lost in the draft or the model; the target of a
// Lightning emitter (not p2p) not kept - the bolt would have nowhere to go.
TEST(VfxLightning) {
  std::string b(0x1800, '\0');
  const auto put32 = [&](size_t at, uint32_t v) { memcpy(&b[at], &v, 4); };
  const auto put16 = [&](size_t at, uint16_t v) { memcpy(&b[at], &v, 2); };
  const auto putf = [&](size_t at, float v) { memcpy(&b[at], &v, 4); };
  const auto puts = [&](size_t at, const char* text) { memcpy(&b[at], text, strlen(text)); };
  const size_t base = 12;
  struct Ctrl {
    uint32_t type;
    std::vector<float> values;
  };
  const auto node = [&](size_t at, const char* name, size_t keys, const std::vector<Ctrl>& ctrls) {
    puts(at + 0x10, name);
    const size_t data = keys + 0x80;
    uint16_t used = 0;
    put32(at + 0x44, static_cast<uint32_t>(keys - base));
    put32(at + 0x48, static_cast<uint32_t>(ctrls.size()));
    put32(at + 0x50, static_cast<uint32_t>(data - base));
    for (size_t i = 0; i < ctrls.size(); ++i) {
      const size_t k = keys + 12 * i;
      put32(k, ctrls[i].type);
      put16(k + 4, 1);
      put16(k + 6, used);
      putf(data + 4 * used++, 0.0f);
      put16(k + 8, used);
      for (float v : ctrls[i].values) putf(data + 4 * used++, v);
      b[k + 10] = static_cast<char>(ctrls[i].values.size());
    }
    put32(at + 0x54, used);
  };
  const auto children = [&](size_t at, size_t array, size_t kid) {
    put32(at + 0x38, static_cast<uint32_t>(array - base));
    put32(at + 0x3C, 1);
    put32(array, static_cast<uint32_t>(kid - 0x10 - base));
  };
  puts(base + 8, "test_bolt");
  put32(base + 0x48, static_cast<uint32_t>(0x100 - 0x10 - base));
  node(0x100, "test_bolt", 0x800, {{8, {0, 0, 0}}});
  children(0x100, 0x1700, 0x200);
  node(0x200, "bolt", 0x900, {{8, {0, 0, 1}}, {88, {9}}, {208, {0.1f}}, {212, {0.15f}}, {216, {0.2f}}});
  puts(0x200 + 0x60 + 0x18, "Lightning");
  puts(0x200 + 0x60 + 0x38, "Linked");
  puts(0x200 + 0x60 + 0x78, "fxpa_lbolt2l");
  put32(0x200 + 0x60 + 0xD4, 0x102);
  children(0x200, 0x1710, 0x300);
  node(0x300, "bolt_ref", 0xA00, {{8, {0, 1, 1}}});
  put32(0x300 + 0x5C, 0x11);
  puts(0x300 + 0x60, "fx_ref");
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(b, &effect, &error), true);
  CHECK_EQ(effect.emitters.size(), 1u);
  if (effect.emitters.empty()) return;
  const VfxEmitter& e = effect.emitters[0];
  CHECK_EQ(e.update == "Lightning", true);
  CHECK_EQ(e.p2p.empty(), true);
  CHECK_EQ(Milli(e.birthrate), 9000);
  CHECK_EQ(Milli(e.lightning_delay), 100);
  CHECK_EQ(Milli(e.lightning_radius), 150);
  CHECK_EQ(Milli(e.lightning_scale), 200);
  CHECK_EQ(effect.nodes.size() == 1 && effect.nodes[0].parent == "bolt", true);
  VfxEffect back;
  CHECK_EQ(ReadVfxDraft(WriteVfxDraft(effect), &back, &error), true);
  CHECK_EQ(back.emitters.size() == 1 && back.nodes.size() == 1, true);
  if (back.emitters.empty()) return;
  CHECK_EQ(Milli(back.emitters[0].lightning_radius), 150);
  CHECK_EQ(Milli(back.emitters[0].lightning_scale), 200);
  const std::string model = WriteVfxModel(back, "n5vx0011");
  CHECK_EQ(Contains(model, "  lightningDelay 0.1\n  lightningRadius 0.15\n  lightningScale 0.2"), true);
  CHECK_EQ(Contains(model, "node reference n5vx0011_n0_bolt_ref"), true);
  VfxEffect read;
  CHECK_EQ(ReadVfxAsciiModel(model, &read, &error), true);
  CHECK_EQ(read.emitters.size() == 1 && read.nodes.size() == 1, true);
  if (read.emitters.empty() || read.nodes.empty()) return;
  CHECK_EQ(Milli(read.emitters[0].lightning_delay), 100);
  CHECK_EQ(read.nodes[0].parent == read.emitters[0].name, true);
}

// An effect with motion (0.42.3): a node that turns in the duration and an emitter under it
// that moves - through the draft, and through the model nwn5e writes and reads back.
// Breaks caught: a node, a parent, a key list, the lengths or the follow flags lost on the
// way (the bench and Play in game would lose the motion); the model's parent names not the
// renamed nodes' (the emitter falls off its parent); a key list written uncounted or half.
namespace {
VfxEffect Moving() {
  VfxEffect effect;
  effect.name = "test_orbit";
  effect.lengths[1] = 1.5f;
  VfxNode n;
  n.name = "spinner";
  n.position[2] = 1.2f;
  VfxKeys turn;
  turn.anim = "duration";
  turn.orientation = {0, 0, 0, 1, 0, 1.5f, 0, 0, 1, 6.2831853f};
  n.keys.push_back(turn);
  effect.nodes.push_back(n);
  VfxEmitter e;
  e.name = "spark";
  e.parent = "spinner";
  e.position[0] = 0.8f;
  e.inherit_local = true;
  e.inherit_part = true;
  VfxKeys move;
  move.anim = "impact";
  move.position = {0, 0.8f, 0, 0, 0.5f, 1.2f, 0, 0.3f};
  e.keys.push_back(move);
  effect.emitters.push_back(e);
  return effect;
}
}  // namespace

TEST(VfxMotionThroughDraftAndModel) {
  VfxEffect back;
  std::string error;
  CHECK_EQ(ReadVfxDraft(WriteVfxDraft(Moving()), &back, &error), true);
  CHECK_EQ(back.nodes.size(), 1u);
  CHECK_EQ(back.emitters.size(), 1u);
  if (back.nodes.empty() || back.emitters.empty()) return;
  CHECK_EQ(Milli(back.lengths[1]), 1500);
  CHECK_EQ(back.nodes[0].name == "spinner", true);
  CHECK_EQ(Milli(back.nodes[0].position[2]), 1200);
  CHECK_EQ(back.nodes[0].keys.size(), 1u);
  if (!back.nodes[0].keys.empty()) CHECK_EQ(back.nodes[0].keys[0].orientation.size(), 10u);
  CHECK_EQ(back.emitters[0].parent == "spinner", true);
  CHECK_EQ(back.emitters[0].inherit_local, true);
  CHECK_EQ(back.emitters[0].inherit_part, true);
  CHECK_EQ(back.emitters[0].keys.size(), 1u);
  if (!back.emitters[0].keys.empty()) {
    CHECK_EQ(back.emitters[0].keys[0].anim == "impact", true);
    CHECK_EQ(Milli(back.emitters[0].keys[0].position[5]), 1200);
  }
  const std::string model = WriteVfxModel(Moving(), "n5vx0007");
  CHECK_EQ(Contains(model, "node dummy n5vx0007_n0_spinner"), true);
  CHECK_EQ(Contains(model, "  parent n5vx0007_n0_spinner"), true);
  CHECK_EQ(Contains(model, "  orientationkey 2\n    0 0 0 1 0\n    1.5 0 0 1 6.28319"), true);
  CHECK_EQ(Contains(model, "  positionkey 2\n    0 0.8 0 0\n    0.5 1.2 0 0.3"), true);
  CHECK_EQ(Contains(model, "  inherit_local 1"), true);
  CHECK_EQ(Contains(model, "newanim duration n5vx0007\n  length 1.5"), true);
  VfxEffect read;
  CHECK_EQ(ReadVfxAsciiModel(model, &read, &error), true);
  CHECK_EQ(read.nodes.size(), 1u);
  CHECK_EQ(read.emitters.size(), 1u);
  if (read.nodes.empty() || read.emitters.empty()) return;
  CHECK_EQ(read.emitters[0].parent == read.nodes[0].name, true);
  CHECK_EQ(Milli(read.lengths[1]), 1500);
  const VfxKeys* turn = FindKeys(&read.nodes[0].keys, "duration", false);
  CHECK_EQ(turn != nullptr && turn->orientation.size() == 10, true);
  const VfxKeys* move = FindKeys(&read.emitters[0].keys, "impact", false);
  CHECK_EQ(move != nullptr && move->position.size() == 8, true);
}

// The game's own ASCII effects key without a count, to "endlist" (vdr_magearmor), and hang
// emitters under meshes. Breaks caught: an uncounted list read as nothing or swallowing the
// next lines; a mesh ancestor dropped (the emitter loses its place); a node that no emitter
// hangs under kept; the root taken for a node.
TEST(VfxReadsTheGamesAsciiKeys) {
  const std::string text =
      "newmodel vdr_test\nbeginmodelgeom vdr_test\n"
      "node dummy vdr_test\n  parent NULL\nendnode\n"
      "node dummy Dummy01\n  parent vdr_test\n  position 0.0 0.0 0.0\n  orientation  0.0 0.0 0.0  0.0\nendnode\n"
      "node trimesh shield\n  parent Dummy01\n  position 0 -1.4 1.4\nendnode\n"
      "node trimesh lonely\n  parent vdr_test\nendnode\n"
      "node emitter Omen\n  parent shield\n  position 0 0 0.2\n  update Fountain\n  birthrate 5\nendnode\n"
      "endmodelgeom vdr_test\n"
      "newanim duration vdr_test\n length 0.966667\n transtime 0.25\nanimroot vdr_test\n"
      "node dummy Dummy01\n  parent vdr_test\n    orientationkey \n"
      "      0.0   0.0 0.0 0.0  0.0\n      0.966667   0.0 0.0 1.0  1.5708\n    endlist \n"
      "    alphakey\n      0.0 0.0\n      0.5 1.0\n    endlist\nendnode\n"
      "node emitter Omen\n  parent shield\n    positionkey \n      0.0  0 0 0.2\n      0.5  0 0 0.4\n    endlist\nendnode\n"
      "doneanim duration vdr_test\ndonemodel vdr_test\n";
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxAsciiModel(text, &effect, &error), true);
  CHECK_EQ(effect.nodes.size(), 2u);  // Dummy01 and shield, not "lonely", not the root
  CHECK_EQ(effect.emitters.size(), 1u);
  if (effect.nodes.size() != 2 || effect.emitters.empty()) return;
  CHECK_EQ(effect.nodes[0].name == "Dummy01", true);
  CHECK_EQ(effect.nodes[0].parent.empty(), true);
  CHECK_EQ(effect.nodes[1].parent == "Dummy01", true);
  CHECK_EQ(effect.emitters[0].parent == "shield", true);
  CHECK_EQ(Milli(effect.lengths[1]), 967);
  const VfxKeys* turn = FindKeys(&effect.nodes[0].keys, "duration", false);
  CHECK_EQ(turn != nullptr, true);
  if (turn) {
    CHECK_EQ(turn->orientation.size(), 10u);
    if (turn->orientation.size() == 10) CHECK_EQ(Milli(turn->orientation[9]), 1571);
  }
  const VfxKeys* move = FindKeys(&effect.emitters[0].keys, "duration", false);
  CHECK_EQ(move != nullptr && move->position.size() == 8, true);
  if (move && move->position.size() == 8) CHECK_EQ(Milli(move->position[7]), 400);
}

// Tile torches: a dummy node's position by name in a compiled model laid out as
// the game's (built here). Breaks caught: the name's case mattering; a longer
// name containing it ("...sl10") matched; the node's base (name - 0x10) or the
// controllers' base drifting; a node without a position read as one; a name
// that only ends in the wanted one matched.
TEST(TileTorchNodePosition) {
  std::string b(0x400, '\0');
  const auto put32 = [&](size_t at, uint32_t v) { memcpy(&b[at], &v, 4); };
  const auto putf = [&](size_t at, float v) { memcpy(&b[at], &v, 4); };
  const auto puts = [&](size_t at, const char* text) { memcpy(&b[at], text, strlen(text)); };
  const size_t base = 12;
  puts(0x60, "tin01_a01_01sl10");  // a longer name, no controllers
  const size_t node = 0x100;
  puts(node + 0x10, "TIN01_A01_01sl1");
  const size_t keys = 0x300, data = 0x340;
  put32(node + 0x44, static_cast<uint32_t>(keys - base));
  put32(node + 0x48, 1);
  put32(node + 0x50, static_cast<uint32_t>(data - base));
  put32(node + 0x54, 4);
  put32(keys, 8);  // position: 1 row, time at 0, value at 1, 3 columns
  b[keys + 4] = 1; b[keys + 6] = 0; b[keys + 8] = 1; b[keys + 10] = 3;
  putf(data, 0.0f);
  putf(data + 4, 3.38f);
  putf(data + 8, -0.01f);
  putf(data + 12, 1.76f);
  float p[3] = {};
  CHECK_EQ(ReadModelNodePosition(b, "tin01_a01_01sl1", p), true);
  CHECK_EQ(Milli(p[0]), 3380);
  CHECK_EQ(Milli(p[1]), -10);
  CHECK_EQ(Milli(p[2]), 1760);
  CHECK_EQ(ReadModelNodePosition(b, "tin01_a01_01sl2", p), false);
  // A node whose name only ends in the wanted one, with a position: not it.
  const size_t other = 0x200;
  puts(other + 0x10 - 1, "xtin01_a01_01sl3");  // the wanted part where a node's name sits
  put32(other + 0x44, static_cast<uint32_t>(keys - base));
  put32(other + 0x48, 1);
  put32(other + 0x50, static_cast<uint32_t>(data - base));
  put32(other + 0x54, 4);
  CHECK_EQ(ReadModelNodePosition(b, "tin01_a01_01sl3", p), false);
  CHECK_EQ(ReadModelNodePosition(b, "tin01_a01_01sl10", p), false);  // no controllers there
}

// Breaks caught: the section or key matched by prefix ([TILE1] for [TILE10]),
// case mattering, spaces or a CR left in the value; Transition missing read as
// garbage; the turn going clockwise; the tile corner used for its centre.
TEST(TileTorchInTheArea) {
  const std::string set =
      "; tileset\r\n[GENERAL]\r\nName=TIN01\r\ntransition = 3\r\n\r\n[TILE10]\r\nModel=tin01_x10\r\n"
      "[TILE1]\r\nModel = tin01_a01_01 \r\nWalkMesh=msb01\r\n";
  CHECK_EQ(TilesetTileModel(set, 1) == "tin01_a01_01", true);
  CHECK_EQ(TilesetTileModel(set, 10) == "tin01_x10", true);
  CHECK_EQ(TilesetTileModel(set, 2).empty(), true);
  CHECK_EQ(Milli(TilesetTransition(set)), 3000);
  CHECK_EQ(Milli(TilesetTransition("[GENERAL]\nName=X\n")), 0);
  const float local[3] = {3.0f, 1.0f, 1.5f};
  float p[3] = {};
  TilePointInArea(local, 2, 4, 0, 0, 3.0f, p);
  CHECK_EQ(Milli(p[0]), 28000);  // 25 + 3
  CHECK_EQ(Milli(p[1]), 46000);  // 45 + 1
  CHECK_EQ(Milli(p[2]), 1500);
  TilePointInArea(local, 2, 4, 1, 2, 3.0f, p);  // 90 degrees counter-clockwise: (3, 1) -> (-1, 3); 2 levels up
  CHECK_EQ(Milli(p[0]), 24000);
  CHECK_EQ(Milli(p[1]), 48000);
  CHECK_EQ(Milli(p[2]), 7500);
  TilePointInArea(local, 0, 0, 3, 0, 0.0f, p);  // 270: (3, 1) -> (1, -3)
  CHECK_EQ(Milli(p[0]), 6000);
  CHECK_EQ(Milli(p[1]), 2000);
}

// The torch flame made to rise (built here, laid out as the game's). Breaks
// caught: a controller of another type changed; a 3-column one (a colour)
// written as one; the inherit flag left on, or other flags cleared; an
// animation's copy (no emitter header) skipped; a name that only ends in it hit.
TEST(TorchFlamePatchedToRise) {
  std::string b(0x600, '\0');
  const auto put32 = [&](size_t at, uint32_t v) { memcpy(&b[at], &v, 4); };
  const auto putf = [&](size_t at, float v) { memcpy(&b[at], &v, 4); };
  const auto getf = [&](size_t at) { float v; memcpy(&v, &b[at], 4); return v; };
  const auto get32 = [&](size_t at) { uint32_t v; memcpy(&v, &b[at], 4); return v; };
  const auto puts = [&](size_t at, const char* text) { memcpy(&b[at], text, strlen(text)); };
  const size_t base = 12, node = 0x100, emitter = node + 0x60;
  puts(node + 0x10, "Flame02");
  puts(emitter + 0x18, "Fountain");
  put32(emitter + 0xD4, 0x42);  // inherit + p2p_sel
  const size_t keys = 0x300, data = 0x340;
  put32(node + 0x44, static_cast<uint32_t>(keys - base));
  put32(node + 0x48, 3);
  put32(node + 0x50, static_cast<uint32_t>(data - base));
  put32(node + 0x54, 7);
  // velocity (1 col, value 1), mass (1 col, value 2), colorStart (3 cols, values 3-5)
  put32(keys, 192); b[keys + 4] = 1; b[keys + 6] = 0; b[keys + 8] = 1; b[keys + 10] = 1;
  put32(keys + 12, 148); b[keys + 16] = 1; b[keys + 18] = 0; b[keys + 20] = 2; b[keys + 22] = 1;
  put32(keys + 24, 192 + 1); b[keys + 28] = 1; b[keys + 30] = 0; b[keys + 32] = 3; b[keys + 34] = 3;
  putf(data + 4, 0.15f);
  putf(data + 8, 0.0f);
  putf(data + 12, 0.5f);
  // An animation's copy: the same name, its own controllers, no emitter header.
  const size_t copy = 0x400;
  puts(copy + 0x10, "Flame02");
  const size_t keys2 = 0x500, data2 = 0x540;
  put32(copy + 0x44, static_cast<uint32_t>(keys2 - base));
  put32(copy + 0x48, 1);
  put32(copy + 0x50, static_cast<uint32_t>(data2 - base));
  put32(copy + 0x54, 2);
  put32(keys2, 192); b[keys2 + 4] = 1; b[keys2 + 6] = 0; b[keys2 + 8] = 1; b[keys2 + 10] = 1;
  putf(data2 + 4, 0.3f);
  const int changed = PatchEmitter(&b, "Flame02", {{192, 0.0f}, {148, -0.1f}}, 0x40);
  CHECK_EQ(changed, 2);
  CHECK_EQ(Milli(getf(data + 4)), 0);        // velocity
  CHECK_EQ(Milli(getf(data + 8)), -100);     // mass
  CHECK_EQ(Milli(getf(data + 12)), 500);     // the 3-column one untouched
  CHECK_EQ(static_cast<int>(get32(emitter + 0xD4)), 0x02);  // inherit off, the rest kept
  CHECK_EQ(Milli(getf(data2 + 4)), 0);       // the animation's copy too
  CHECK_EQ(PatchEmitter(&b, "ame02", {{192, 1.0f}}, 0), 0);
}

// Fill lights follow the lights round them. Breaks caught: a tile with no
// light near turned off; one lit light 1 tile away not keeping it on; the
// neighbourhood wider or narrower than the 8 round it; x and y swapped in the
// row layout; a light off the area's edge counted.
TEST(TileFillLightsFollowTheLightsRoundThem) {
  std::vector<TileLight> lights;
  lights.push_back({1, 1, false});  // out
  lights.push_back({4, 3, true});   // lit
  const std::vector<uint8_t> off = TileFillLightsOff(6, 5, lights);
  const auto at = [&](int x, int y) { return static_cast<int>(off[static_cast<size_t>(y) * 6 + x]); };
  CHECK_EQ(off.size(), 30u);
  CHECK_EQ(at(0, 0), 1);  // only the doused light near
  CHECK_EQ(at(2, 2), 1);  // (1,1) out is near, (4,3) is 2 away
  CHECK_EQ(at(3, 2), 0);  // the lit one is near
  CHECK_EQ(at(5, 4), 0);
  CHECK_EQ(at(5, 0), 0);  // no light near: the module's
  CHECK_EQ(at(0, 4), 0);
  CHECK_EQ(at(2, 0), 1);
  CHECK_EQ(at(3, 0), 0);  // nothing within 1
  lights[1].lit = false;
  CHECK_EQ(static_cast<int>(TileFillLightsOff(6, 5, lights)[2 * 6 + 3]), 1);  // both out now
  CHECK_EQ(TileFillLightsOff(0, 5, lights).empty(), true);
  // Strict (indoors): no lit light near, no fill - near a lit one, kept.
  lights[1].lit = true;
  const std::vector<uint8_t> strict = TileFillLightsOff(6, 5, lights, true);
  CHECK_EQ(static_cast<int>(strict[0 * 6 + 5]), 1);  // no light near at all: off now
  CHECK_EQ(static_cast<int>(strict[4 * 6 + 0]), 1);
  CHECK_EQ(static_cast<int>(strict[0 * 6 + 0]), 1);  // only the doused one near
  CHECK_EQ(static_cast<int>(strict[2 * 6 + 3]), 0);  // the lit one near
  CHECK_EQ(static_cast<int>(strict[4 * 6 + 5]), 0);
}

// Breaks caught: an emitter keyed on at 0 and off at the next key (the game's
// short bursts) read as always on - a loaded copy would emit for the whole
// effect.
TEST(VfxBirthrateKeyedOffAtTheNextKey) {
  const std::string model =
      "newmodel t\nbeginmodelgeom t\nnode emitter e\n  update Fountain\n  birthrate 0\nendnode\nendmodelgeom t\n"
      "newanim impact t\n  length 1\nnode emitter e\n  birthratekey 2\n    0 600\n    0.1 0\nendnode\n"
      "doneanim impact t\n";
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxAsciiModel(model, &effect, &error), true);
  CHECK_EQ(Milli(effect.emitters[0].birthrate), 600000);
  CHECK_EQ(Milli(effect.emitters[0].emit_seconds), 100);
  // Keyed down to a negative value, as the game's models do: off where it crosses 0.
  const std::string negative =
      "newmodel t\nbeginmodelgeom t\nnode emitter e\n  update Fountain\n  birthrate 100\nendnode\nendmodelgeom t\n"
      "newanim impact t\n  length 1\nnode emitter e\n  birthratekey 3\n    0 100\n    0.1 100\n    0.3 -100\n"
      "endnode\ndoneanim impact t\n";
  CHECK_EQ(ReadVfxAsciiModel(negative, &effect, &error), true);
  CHECK_EQ(Milli(effect.emitters[0].emit_seconds), 200);
}

// --- Planes (#161 Task 3; the owner's Q1 A: Entangle's vines are flat textured planes, drawn and kept, not edited) ---

namespace {

// A compiled model with a plain mesh node "Plane01" (flags 0x21: header | mesh) half a metre along x, turned 90 degrees
// about z: a quad of 4 vertices (2 faces), its UVs, textured fxpa_fern - and, `with_emitter`, an emitter beside it.
// Its material the game's defaults (Torlack's mesh header: diffuse 0.8, ambient 0.2, drawn) - or, `material`, a
// material of its own: diffuse 0.5 0.25 1 (+0x3C), ambient 0.1 0.2 0.3 (+0x48) and its controllers selfillumcolor 0 1
// 0.5 (100) and alpha 0.6 (128). `flags`: its node's (0xA1 an animated mesh, 0x61 skin, 0x121 dangly, 0x221 walkmesh).
std::string PlaneModel(bool with_emitter, uint32_t flags = 0x21, bool material = false) {
  std::string b(0x2000, '\0');
  const auto put32 = [&](size_t at, uint32_t v) { memcpy(&b[at], &v, 4); };
  const auto put16 = [&](size_t at, uint16_t v) { memcpy(&b[at], &v, 2); };
  const auto putf = [&](size_t at, float v) { memcpy(&b[at], &v, 4); };
  const auto puts = [&](size_t at, const char* text) { memcpy(&b[at], text, strlen(text)); };
  const size_t base = 12, raw = 0x1800;
  put32(4, static_cast<uint32_t>(raw - base));  // the raw data's offset (vertices, UVs)
  struct Ctrl {
    uint32_t type;
    int columns;
    std::vector<float> values;
  };
  const auto node = [&](size_t at, const char* name, size_t keys, const std::vector<Ctrl>& ctrls) {
    puts(at + 0x10, name);
    const size_t data = keys + 0x40;
    uint16_t used = 0;
    put32(at + 0x44, static_cast<uint32_t>(keys - base));
    put32(at + 0x48, static_cast<uint32_t>(ctrls.size()));
    put32(at + 0x50, static_cast<uint32_t>(data - base));
    for (size_t i = 0; i < ctrls.size(); ++i) {
      const size_t k = keys + 12 * i;
      put32(k, ctrls[i].type);
      put16(k + 4, 1);
      put16(k + 6, used);
      putf(data + 4 * used++, 0.0f);
      put16(k + 8, used);
      for (float v : ctrls[i].values) putf(data + 4 * used++, v);
      b[k + 10] = static_cast<char>(ctrls[i].columns);
    }
    put32(at + 0x54, used);
  };
  const auto children = [&](size_t at, size_t array, const std::vector<size_t>& kids) {
    put32(at + 0x38, static_cast<uint32_t>(array - base));
    put32(at + 0x3C, static_cast<uint32_t>(kids.size()));
    for (size_t i = 0; i < kids.size(); ++i) put32(array + 4 * i, static_cast<uint32_t>(kids[i] - 0x10 - base));
  };
  puts(base + 8, "test_plane");
  put32(base + 0x48, static_cast<uint32_t>(0x100 - 0x10 - base));
  node(0x100, "test_plane", 0x900, {{8, 3, {0, 0, 0}}});
  std::vector<size_t> kids = {0x200};
  if (with_emitter) kids.push_back(0x600);
  children(0x100, 0x1600, kids);
  std::vector<Ctrl> plane_ctrls = {{8, 3, {0.5f, 0, 0}}, {20, 4, {0, 0, 0.70710678f, 0.70710678f}}};
  if (material) {
    plane_ctrls.push_back({100, 3, {0, 1, 0.5f}});
    plane_ctrls.push_back({128, 1, {0.6f}});
  }
  node(0x200, "Plane01", 0x980, plane_ctrls);
  put32(0x200 + 0x5C, flags);
  const size_t mh = 0x200 + 0x60;
  const float diffuse[3] = {material ? 0.5f : 0.8f, material ? 0.25f : 0.8f, material ? 1.0f : 0.8f};
  const float ambient[3] = {material ? 0.1f : 0.2f, 0.2f, material ? 0.3f : 0.2f};
  for (int k = 0; k < 3; ++k) {
    putf(mh + 0x3C + 4 * k, diffuse[k]);
    putf(mh + 0x48 + 4 * k, ambient[k]);
  }
  put32(mh + 0x6C, 1);  // render
  puts(mh + 0x78, "fxpa_fern");
  put32(mh + 0x08, static_cast<uint32_t>(0x1400 - base));  // faces
  put32(mh + 0x0C, 2);
  const uint16_t faces[2][3] = {{0, 1, 2}, {2, 1, 3}};
  for (int f = 0; f < 2; ++f)
    for (int k = 0; k < 3; ++k) put16(0x1400 + 32 * f + 26 + 2 * k, faces[f][k]);
  put32(mh + 0x1BC, 0);   // vertices at the raw data's start
  put16(mh + 0x1C0, 4);
  put32(mh + 0x1C4, 48);  // UVs after the 4 vertices
  const float verts[4][3] = {{-0.2f, 0, 0}, {0.2f, 0, 0}, {-0.2f, 0, 0.4f}, {0.2f, 0, 0.4f}};
  const float uvs[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
  for (int v = 0; v < 4; ++v) {
    for (int c = 0; c < 3; ++c) putf(raw + 12 * v + 4 * c, verts[v][c]);
    for (int c = 0; c < 2; ++c) putf(raw + 48 + 8 * v + 4 * c, uvs[v][c]);
  }
  if (with_emitter) {
    node(0x600, "spores", 0xA00, {{8, 3, {0, 0, 0.1f}}});
    puts(0x600 + 0x60 + 0x18, "Fountain");
    puts(0x600 + 0x60 + 0x38, "Normal");
    puts(0x600 + 0x60 + 0x78, "fxpa_mist");
  }
  return b;
}

bool SamePlane(const VfxPlane& a, const VfxPlane& b) {
  const auto near = [](const std::vector<float>& x, const std::vector<float>& y) {
    if (x.size() != y.size()) return false;
    for (size_t i = 0; i < x.size(); ++i)
      if (Milli(x[i]) != Milli(y[i])) return false;
    return true;
  };
  bool same = a.name == b.name && a.parent == b.parent && a.bitmap == b.bitmap && near(a.verts, b.verts) && near(a.tverts, b.tverts) && a.faces == b.faces;
  for (int k = 0; k < 3; ++k) same = same && Milli(a.position[k]) == Milli(b.position[k]);
  for (int k = 0; k < 4; ++k) same = same && Milli(a.orientation[k]) == Milli(b.orientation[k]);
  return same;
}

}  // namespace

TEST(VfxModelPlanesRead) {
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(PlaneModel(true), &effect, &error), true);
  CHECK_EQ(effect.emitters.size(), 1u);
  CHECK_EQ(effect.planes.size(), 1u);
  if (effect.planes.size() != 1) return;
  const VfxPlane& p = effect.planes[0];
  CHECK_EQ(p.name == "Plane01" && p.parent.empty() && p.bitmap == "fxpa_fern", true);
  CHECK_EQ(Milli(p.position[0]), 500);
  CHECK_EQ(Milli(p.orientation[2]), 1000);  // about z ...
  CHECK_EQ(Milli(p.orientation[3]), 1571);  // ... by 90 degrees
  CHECK_EQ(p.verts.size(), 12u);
  CHECK_EQ(p.tverts.size(), 8u);
  CHECK_EQ(p.faces.size(), 6u);
  CHECK_EQ(p.verts.size() == 12 && Milli(p.verts[11]) == 400 && Milli(p.verts[3]) == 200, true);
  CHECK_EQ(p.tverts.size() == 8 && Milli(p.tverts[7]) == 1000, true);
  CHECK_EQ(p.faces.size() == 6 && p.faces[3] == 2 && p.faces[5] == 3, true);
}

// A model of planes alone (no emitter) reads - its planes - where it used to fail "no emitters".
TEST(VfxModelPlanesOnly) {
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(PlaneModel(false), &effect, &error), true);
  CHECK_EQ(effect.emitters.size(), 0u);
  CHECK_EQ(effect.planes.size(), 1u);
}

// Through the draft (a [plane <name>] section) and the ASCII model (a trimesh node) the plane comes back the same: an
// edited Entangle keeps its vines.
TEST(VfxDraftPlanesRoundTrip) {
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(PlaneModel(true), &effect, &error), true);
  if (effect.planes.size() != 1) return;
  const std::string draft = WriteVfxDraft(effect);
  CHECK_EQ(draft.find("[plane Plane01]") != std::string::npos, true);
  VfxEffect back;
  CHECK_EQ(ReadVfxDraft(draft, &back, &error), true);
  CHECK_EQ(back.planes.size() == 1 && SamePlane(back.planes[0], effect.planes[0]), true);
  CHECK_EQ(WriteVfxDraft(back) == draft, true);
  const std::string model = WriteVfxModel(back, "n5t");
  CHECK_EQ(model.find("node trimesh n5t_p0_plane01") != std::string::npos && model.find("bitmap fxpa_fern") != std::string::npos, true);
  VfxEffect again;
  CHECK_EQ(ReadVfxAsciiModel(model, &again, &error), true);
  CHECK_EQ(again.planes.size(), 1u);
  if (again.planes.size() != 1) return;
  CHECK_EQ(again.planes[0].name == "n5t_p0_plane01", true);  // the model's own node name (ModelNodeName)
  again.planes[0].name = effect.planes[0].name;
  CHECK_EQ(SamePlane(again.planes[0], effect.planes[0]), true);
}

// The VFX Bench's core.js writes the same bytes (its check_area.js reads this fixture too): a plane's draft section.
TEST(VfxDraftPlaneFixture) {
  std::ifstream file("tests/data/fixtures/plane_effect.vfx", std::ios::binary);
  std::ostringstream read;
  read << file.rdbuf();
  std::string text;
  for (char c : read.str())
    if (c != '\r') text += c;
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxDraft(text, &effect, &error), true);
  CHECK_EQ(effect.planes.size(), 1u);
  CHECK_EQ(effect.emitters.size(), 0u);
  CHECK_EQ(WriteVfxDraft(effect) == text, true);
}

// --- A mesh's material, and the mesh kinds a chunk has (the owner, 2026-11-16: "fear skulls in
// VFX_DUR_MIND_AFFECTING_FEAR are untextured and in VFX_DUR_PDK_FEAR don't even show the mesh") ---

// An untextured mesh is drawn in its material: the compiled mesh header's diffuse (+0x3C) and ambient (+0x48), whether
// it's drawn (+0x6C), and the node's selfillumcolor (100) and alpha (128) controllers - read into the plane.
TEST(VfxModelPlaneMaterialRead) {
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(PlaneModel(true, 0x21, true), &effect, &error), true);
  CHECK_EQ(effect.planes.size(), 1u);
  if (effect.planes.size() != 1) return;
  const VfxPlane& p = effect.planes[0];
  CHECK_EQ(Milli(p.diffuse[0]) == 500 && Milli(p.diffuse[1]) == 250 && Milli(p.diffuse[2]) == 1000, true);
  CHECK_EQ(Milli(p.ambient[0]) == 100 && Milli(p.ambient[1]) == 200 && Milli(p.ambient[2]) == 300, true);
  CHECK_EQ(Milli(p.selfillum[0]) == 0 && Milli(p.selfillum[1]) == 1000 && Milli(p.selfillum[2]) == 500, true);
  CHECK_EQ(Milli(p.alpha), 600);
  CHECK_EQ(p.render, true);
  CHECK_EQ(p.flags, 0x21u);
  CHECK_EQ(Milli(p.position[0]), 500);  // the other controllers still read
  // The game's defaults read as they are: no selfillum or alpha controller - none, and fully there.
  VfxEffect plain;
  CHECK_EQ(ReadVfxBinaryModel(PlaneModel(true), &plain, &error), true);
  CHECK_EQ(plain.planes.size(), 1u);
  if (plain.planes.size() != 1) return;
  const VfxPlane& d = plain.planes[0];
  CHECK_EQ(Milli(d.diffuse[1]) == 800 && Milli(d.ambient[1]) == 200 && Milli(d.selfillum[1]) == 0 && Milli(d.alpha) == 1000, true);
  // A mesh with render 0 isn't drawn.
  std::string hidden = PlaneModel(true);
  const uint32_t off = 0;
  memcpy(&hidden[0x200 + 0x60 + 0x6C], &off, 4);
  VfxEffect h;
  CHECK_EQ(ReadVfxBinaryModel(hidden, &h, &error) && h.planes.size() == 1 && !h.planes[0].render, true);
}

// A chunk's meshes of any kind (an animated, skin or dangly mesh: its own vertices, the rest pose) read with the option;
// an effect read without it keeps the plain-mesh rule (re-saving one would turn such a mesh into a static one). A
// walkmesh never.
TEST(VfxModelAnyMeshKind) {
  for (const uint32_t flags : {0xA1u, 0x61u, 0x121u}) {
    VfxEffect effect;
    std::string error;
    CHECK_EQ(ReadVfxBinaryModel(PlaneModel(true, flags), &effect, &error), true);  // an effect: its emitter, no plane
    CHECK_EQ(effect.emitters.size() == 1 && effect.planes.empty(), true);
    VfxEffect alone;
    CHECK_EQ(ReadVfxBinaryModel(PlaneModel(false, flags), &alone, &error), false);  // nothing else in it: not read, as before
    VfxEffect chunk;
    CHECK_EQ(ReadVfxBinaryModel(PlaneModel(false, flags), &chunk, &error, true), true);
    CHECK_EQ(chunk.planes.size(), 1u);
    if (chunk.planes.size() != 1) continue;
    const VfxPlane& p = chunk.planes[0];
    CHECK_EQ(p.flags, flags);
    CHECK_EQ(p.name == "Plane01" && p.verts.size() == 12 && p.tverts.size() == 8 && p.faces.size() == 6, true);
    CHECK_EQ(Milli(p.verts[11]), 400);
  }
  VfxEffect walk;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(PlaneModel(false, 0x221), &walk, &error, true), false);
  CHECK_EQ(walk.planes.empty(), true);
  // A plain mesh reads the same with the option as without it.
  VfxEffect a, b;
  CHECK_EQ(ReadVfxBinaryModel(PlaneModel(true, 0x21, true), &a, &error) && ReadVfxBinaryModel(PlaneModel(true, 0x21, true), &b, &error, true), true);
  CHECK_EQ(WriteVfxDraft(a) == WriteVfxDraft(b), true);
}

// The material through the draft: only what isn't the default - "diffuse = ", "ambient = ", "selfillum = ", "alpha = ",
// "render = 0" after the bitmap (core.js writes the same bytes) - and back; a plane with the defaults writes none (every
// draft before them keeps its bytes). The model's trimesh carries them (the default's ambient / diffuse 1 1 1 as before),
// and the ASCII reader reads them back.
const char* const kPlaneMaterial = "bitmap = fxpa_fern\ndiffuse = 0.5 0.25 1\nambient = 0.1 0.2 0.3\nselfillum = 0 1 0.5\nalpha = 0.6\nverts =";

TEST(VfxDraftPlaneMaterial) {
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(PlaneModel(true, 0x21, true), &effect, &error), true);
  if (effect.planes.size() != 1) return;
  const std::string draft = WriteVfxDraft(effect);
  CHECK_EQ(Contains(draft, kPlaneMaterial), true);
  CHECK_EQ(Contains(draft.substr(draft.find("[plane")), "render"), false);  // drawn: the default
  VfxEffect back;
  CHECK_EQ(ReadVfxDraft(draft, &back, &error), true);
  CHECK_EQ(back.planes.size(), 1u);
  if (back.planes.size() != 1) return;
  const VfxPlane& p = back.planes[0];
  CHECK_EQ(Milli(p.diffuse[1]) == 250 && Milli(p.ambient[2]) == 300 && Milli(p.selfillum[2]) == 500 && Milli(p.alpha) == 600 && p.render, true);
  CHECK_EQ(WriteVfxDraft(back) == draft, true);
  // Not drawn: "render = 0", and back.
  back.planes[0].render = false;
  const std::string hidden = WriteVfxDraft(back);
  CHECK_EQ(Contains(hidden, "alpha = 0.6\nrender = 0\nverts ="), true);
  VfxEffect again;
  CHECK_EQ(ReadVfxDraft(hidden, &again, &error) && again.planes.size() == 1 && !again.planes[0].render && WriteVfxDraft(again) == hidden, true);
  // The game's defaults: no material lines (the fixture's plane, above, keeps its bytes).
  VfxEffect plain;
  CHECK_EQ(ReadVfxBinaryModel(PlaneModel(true), &plain, &error), true);
  const std::string text = WriteVfxDraft(plain), section = text.substr(text.find("[plane"));
  for (const char* key : {"diffuse", "ambient", "selfillum", "alpha", "render"}) CHECK_EQ(Contains(section, key), false);
  CHECK_EQ(Contains(text, "bitmap = fxpa_fern\nverts ="), true);
  // The model: a trimesh with the material (selfillumcolor, alpha after its shadow line), and the ASCII reader's way back.
  const std::string model = WriteVfxModel(effect, "n5t");
  CHECK_EQ(Contains(model, "  ambient 0.1 0.2 0.3\n  diffuse 0.5 0.25 1\n"), true);
  CHECK_EQ(Contains(model, "  render 1\n  shadow 0\n  selfillumcolor 0 1 0.5\n  alpha 0.6\n"), true);
  VfxEffect read;
  CHECK_EQ(ReadVfxAsciiModel(model, &read, &error) && read.planes.size() == 1, true);
  if (read.planes.size() == 1) {
    const VfxPlane& r = read.planes[0];
    CHECK_EQ(Milli(r.diffuse[0]) == 500 && Milli(r.ambient[0]) == 100 && Milli(r.selfillum[1]) == 1000 && Milli(r.alpha) == 600 && r.render, true);
  }
  const std::string hidden_model = WriteVfxModel(again, "n5t");
  VfxEffect hidden_read;
  CHECK_EQ(Contains(hidden_model, "  render 0\n") && ReadVfxAsciiModel(hidden_model, &hidden_read, &error) && hidden_read.planes.size() == 1 &&
               !hidden_read.planes[0].render, true);
  // A plane with the defaults: its trimesh as nwn5e has always written it (no selfillumcolor or alpha line).
  const std::string plain_model = WriteVfxModel(plain, "n5t");
  CHECK_EQ(Contains(plain_model, "  wirecolor 1 1 1\n  ambient 1 1 1\n  diffuse 1 1 1\n  specular 0 0 0\n  shininess 1\n  bitmap fxpa_fern\n  render 1\n  shadow 0\n  verts 4\n"), true);
}

// The VFX Bench's core.js reads and writes the same bytes (its check_chunks.js reads this fixture too): an untextured
// plane with a material of its own, and one that isn't drawn.
TEST(VfxDraftPlaneMaterialFixture) {
  std::ifstream file("tests/data/fixtures/plane_material.vfx", std::ios::binary);
  std::ostringstream read;
  read << file.rdbuf();
  std::string text;
  for (char c : read.str())
    if (c != '\r') text += c;
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxDraft(text, &effect, &error), true);
  CHECK_EQ(effect.planes.size(), 2u);
  if (effect.planes.size() != 2) return;
  const VfxPlane& skull = effect.planes[0];
  CHECK_EQ(skull.bitmap == "null" && Milli(skull.diffuse[2]) == 1000 && Milli(skull.ambient[0]) == 100 && Milli(skull.selfillum[1]) == 1000 &&
               Milli(skull.alpha) == 600 && skull.render, true);
  const VfxPlane& hidden = effect.planes[1];
  CHECK_EQ(!hidden.render && Milli(hidden.diffuse[0]) == 800 && Milli(hidden.ambient[0]) == 200 && Milli(hidden.alpha) == 1000, true);
  CHECK_EQ(WriteVfxDraft(effect) == text, true);
}

// --- A mesh beat (the Composer, #39): a spell mesh placed in an effect as planes, timed and faded as a light is ---

namespace {

// A fixture's text with its line ends as LF (as the bench reads it).
std::string FixtureText(const char* path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream read;
  read << file.rdbuf();
  std::string text;
  for (char c : read.str())
    if (c != '\r') text += c;
  return text;
}

// The same plane, and the same bench-only keys: the mesh it came from, its scale and timing, its alpha keys.
bool SameBeat(const VfxPlane& a, const VfxPlane& b) {
  bool same = SamePlane(a, b) && a.mesh == b.mesh && Milli(a.scale) == Milli(b.scale) &&
              Milli(a.start_seconds) == Milli(b.start_seconds) && Milli(a.emit_seconds) == Milli(b.emit_seconds) &&
              a.keys.size() == b.keys.size();
  for (size_t k = 0; same && k < a.keys.size(); ++k) {
    same = a.keys[k].anim == b.keys[k].anim && a.keys[k].alpha.size() == b.keys[k].alpha.size();
    for (size_t i = 0; same && i < a.keys[k].alpha.size(); ++i) same = Milli(a.keys[k].alpha[i]) == Milli(b.keys[k].alpha[i]);
  }
  return same;
}

// plane_beat.vfx's effect: a 0.2 m quad (a 0.4 m spell mesh at scale 0.5) under the node ring, on from 0.3 s for 1.2 s,
// fading in to 0.8 and out again over 0.2 s.
VfxEffect BeatEffect() {
  VfxEffect effect;
  effect.name = "n5_beat";
  VfxNode ring;
  ring.name = "ring";
  ring.position[2] = 1.0f;
  effect.nodes.push_back(ring);
  VfxPlane beat;
  beat.name = "beat";
  beat.parent = "ring";
  beat.orientation[2] = 1.0f;
  beat.orientation[3] = 1.5708f;
  beat.bitmap = "fxpa_fern";
  beat.verts = {-0.1f, 0, 0, 0.1f, 0, 0, -0.1f, 0, 0.2f, 0.1f, 0, 0.2f};
  beat.tverts = {0, 0, 1, 0, 0, 1, 1, 1};
  beat.faces = {0, 1, 2, 2, 1, 3};
  beat.mesh = "n5m_test";
  beat.scale = 0.5f;
  beat.start_seconds = 0.3f;
  beat.emit_seconds = 1.2f;
  VfxKeys fade;
  fade.anim = "impact";
  fade.alpha = {0, 0, 0.3f, 0, 0.5f, 0.8f, 1.3f, 0.8f, 1.5f, 0};
  beat.keys.push_back(fade);
  effect.planes.push_back(beat);
  return effect;
}

}  // namespace

// A mesh beat through the draft: after the plane's render line and before its verts, each only when it isn't the
// default - "mesh = ", "scale = ", "start_seconds = ", "emit_seconds = ", then "<anim>_alpha = t a ..." per animation, in
// the animations' order (core.js writes the same bytes) - and back the same.
TEST(VfxDraftPlaneBeatRoundTrip) {
  VfxEffect effect = BeatEffect();
  VfxKeys out;
  out.anim = "cessation";
  out.alpha = {0, 0.8f, 0.4f, 0};
  effect.planes[0].keys.push_back(out);
  effect.planes[0].render = false;
  const std::string draft = WriteVfxDraft(effect);
  CHECK_EQ(Contains(draft, "[plane beat]\nparent = ring\nposition = 0 0 0\norientation = 0 0 1 1.5708\nbitmap = fxpa_fern\nrender = 0\n"
                           "mesh = n5m_test\nscale = 0.5\nstart_seconds = 0.3\nemit_seconds = 1.2\n"
                           "impact_alpha = 0 0 0.3 0 0.5 0.8 1.3 0.8 1.5 0\ncessation_alpha = 0 0.8 0.4 0\nverts = "),
           true);
  VfxEffect back;
  std::string error;
  CHECK_EQ(ReadVfxDraft(draft, &back, &error), true);
  CHECK_EQ(back.planes.size(), 1u);
  if (back.planes.size() != 1) return;
  CHECK_EQ(SameBeat(back.planes[0], effect.planes[0]) && !back.planes[0].render, true);
  CHECK_EQ(WriteVfxDraft(back) == draft, true);
}

// The VFX Bench's core.js reads and writes the same bytes (check_chunks.js reads this fixture too): a mesh beat.
TEST(VfxDraftPlaneBeatFixture) {
  const std::string text = FixtureText("tests/data/fixtures/plane_beat.vfx");
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxDraft(text, &effect, &error), true);
  CHECK_EQ(effect.nodes.size() == 1 && effect.emitters.empty() && effect.planes.size() == 1, true);
  if (effect.planes.size() != 1) return;
  CHECK_EQ(SameBeat(effect.planes[0], BeatEffect().planes[0]), true);
  CHECK_EQ(WriteVfxDraft(effect) == text, true);
  CHECK_EQ(WriteVfxDraft(BeatEffect()) == text, true);  // the fixture is in WriteVfxDraft's order
}

// The model keys a beat's alpha (alphakey: controller 128, as 22 of the game's own effect models key a mesh's): in each
// animation with alpha keys for it, after the lights, a trimesh node of its geometry's name, under its geometry's
// parent, with its rows; none in the others, nor for a plane without keys. Its geometry as before; its mesh, scale and
// timing never in the model; the animation as long as its keys.
TEST(VfxModelPlaneAlphaKeys) {
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxDraft(FixtureText("tests/data/fixtures/plane_beat.vfx"), &effect, &error), true);
  CHECK_EQ(effect.planes.size(), 1u);
  if (effect.planes.size() != 1) return;
  const std::string model = WriteVfxModel(effect, "n5t");
  const std::string impact = Section(model, "newanim impact n5t", "doneanim impact n5t");
  CHECK_EQ(Section(impact, "node trimesh n5t_p0_beat", "endnode") ==
               "node trimesh n5t_p0_beat\n  parent n5t_n0_ring\n  alphakey 5\n    0 0\n    0.3 0\n    0.5 0.8\n    1.3 0.8\n    1.5 0\n",
           true);
  for (const char* other : {"duration", "cessation"}) {
    const std::string anim = Section(model, std::string("newanim ") + other + " n5t", std::string("doneanim ") + other + " n5t");
    CHECK_EQ(!anim.empty() && !Contains(anim, "trimesh") && !Contains(anim, "alphakey"), true);
  }
  const size_t length = impact.find("  length ");
  CHECK_EQ(length != std::string::npos && std::stof(impact.substr(length + 9)) >= 1.5f, true);
  // Its geometry: the trimesh the same plane without the new keys writes.
  VfxEffect plain = effect;
  VfxPlane& p = plain.planes[0];
  p.mesh.clear();
  p.scale = 1.0f;
  p.start_seconds = p.emit_seconds = 0.0f;
  p.keys.clear();
  const auto geometry = [](const std::string& m) { return Section(m, "beginmodelgeom n5t", "endmodelgeom n5t"); };
  CHECK_EQ(Contains(geometry(model), "node trimesh n5t_p0_beat\n  parent n5t_n0_ring\n"), true);
  CHECK_EQ(geometry(model) == geometry(WriteVfxModel(plain, "n5t")), true);
  CHECK_EQ(Contains(model, "n5m_test") || Contains(model, "  scale") || Contains(model, "_seconds"), false);
  // After the lights; a plane without keys in no animation.
  VfxEffect lit = effect;
  lit.lights.emplace_back();
  lit.planes.push_back(plain.planes[0]);
  lit.planes.back().name = "still";
  const std::string lit_model = WriteVfxModel(lit, "n5t");
  const std::string lit_impact = Section(lit_model, "newanim impact n5t", "doneanim impact n5t");
  const size_t light = lit_impact.find("node light n5t_l0_light"), mesh = lit_impact.find("node trimesh n5t_p0_beat");
  CHECK_EQ(light != std::string::npos && mesh != std::string::npos && light < mesh, true);
  CHECK_EQ(Contains(geometry(lit_model), "node trimesh n5t_p1_still"), true);
  CHECK_EQ(lit_model.find("n5t_p1_still", lit_model.find("newanim")) == std::string::npos, true);
}

// Review Focus 10: a plane without the new keys writes the draft and the model it wrote before - plane_material.vfx's
// draft is the file, and its model keys no mesh: no trimesh in any animation, as nwn5e has always written planes.
TEST(VfxModelPlaneWithoutKeysUnchanged) {
  const std::string text = FixtureText("tests/data/fixtures/plane_material.vfx");
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxDraft(text, &effect, &error) && effect.planes.size() == 2, true);
  for (const VfxPlane& p : effect.planes) {
    CHECK_EQ(p.mesh.empty() && p.scale == 1.0f && p.start_seconds == 0.0f && p.emit_seconds == 0.0f && p.keys.empty(), true);
  }
  CHECK_EQ(WriteVfxDraft(effect) == text, true);
  const std::string model = WriteVfxModel(effect, "n5t");
  const size_t first = model.find("newanim");
  CHECK_EQ(first != std::string::npos && model.find("node trimesh", first) == std::string::npos, true);
  CHECK_EQ(Contains(model, "alphakey"), false);
}

// #161 VA-2 (owner, 2026-11-13: "after a while the effects stop happening in the whole area and just focus in the
// middle"): an area's copies run their animations again and again (moved, the cessation, then the impact and the
// duration), so a lasting effect's emitter turned off in the cessation stays off unless the impact and the duration set
// its birthrate again - the game's own vps_entangle has no birthrate keys at all, and its vines stay. A light the
// cessation fades (#145) gets its radius back the same way. An emitter with its own emit time keeps its impact keys and
// stays off in the duration, as before.
TEST(VfxLastingEmittersRestartAfterCessation) {
  VfxEffect effect;
  effect.duration = true;
  VfxEmitter vines;
  vines.name = "vines";
  vines.birthrate = 8.0f;
  effect.emitters.push_back(vines);
  VfxEmitter burst;
  burst.name = "burst";
  burst.birthrate = 20.0f;
  burst.emit_seconds = 0.5f;
  effect.emitters.push_back(burst);
  VfxLight light;
  light.name = "glow";
  light.radius = 6.0f;
  effect.lights.push_back(light);
  const std::string model = WriteVfxModel(effect, "n5t");
  const std::string impact = Section(model, "newanim impact n5t", "doneanim impact n5t");
  const std::string duration = Section(model, "newanim duration n5t", "doneanim duration n5t");
  const std::string cessation = Section(model, "newanim cessation n5t", "doneanim cessation n5t");
  const auto in = [](const std::string& anim, const std::string& node) { return Section(anim, node, "endnode"); };
  CHECK_EQ(in(impact, "node emitter n5t_0_vines").find("  birthratekey 1\n    0 8\n") != std::string::npos, true);
  CHECK_EQ(in(duration, "node emitter n5t_0_vines").find("  birthratekey 1\n    0 8\n") != std::string::npos, true);
  CHECK_EQ(in(cessation, "node emitter n5t_0_vines").find("  birthratekey 1\n    0 0\n") != std::string::npos, true);
  CHECK_EQ(in(impact, "node emitter n5t_1_burst").find("  birthratekey 3\n    0 20\n    0.5 20\n    0.51 0\n") != std::string::npos, true);
  CHECK_EQ(in(duration, "node emitter n5t_1_burst").find("birthratekey") == std::string::npos, true);
  CHECK_EQ(in(impact, "node light n5t_l0_glow").find("  radiuskey 1\n    0 6\n") != std::string::npos, true);
  CHECK_EQ(in(duration, "node light n5t_l0_glow").find("  radiuskey 1\n    0 6\n") != std::string::npos, true);
  CHECK_EQ(in(cessation, "node light n5t_l0_glow").find("  radiuskey 2\n    0 6\n    0.4 0\n") != std::string::npos, true);
}

// The owner, 2026-11-14: "is it possible to have an effect that has multiple phases? ... there should be some sort of
// delay attached to emitters", then "go with all three": an emitter's start (start_seconds) - off until then, then on,
// its On for counted from there. In the draft only when set (older drafts unchanged); in the model, its impact keys.
TEST(VfxEmitterStartsAfter) {
  VfxEffect effect;
  effect.duration = false;  // a one-shot (F)
  effect.seconds = 2.0f;
  VfxEmitter late;
  late.name = "late";
  late.birthrate = 20.0f;
  late.start_seconds = 0.5f;
  late.emit_seconds = 1.0f;
  late.life = 1.0f;
  effect.emitters.push_back(late);
  VfxEmitter now;
  now.name = "now";
  now.life = 0.25f;
  effect.emitters.push_back(now);
  const std::string draft = WriteVfxDraft(effect);
  CHECK_EQ(draft.find("emit_seconds = 1\nstart_seconds = 0.5\nlife = 1\n") != std::string::npos, true);
  CHECK_EQ(draft.find("start_seconds", draft.find("[emitter now]")) == std::string::npos, true);  // unset: no line
  VfxEffect back;
  std::string error;
  CHECK_EQ(ReadVfxDraft(draft, &back, &error), true);
  CHECK_EQ(back.emitters.size(), 2u);
  CHECK_EQ(back.emitters[0].start_seconds, 0.5f);
  CHECK_EQ(back.emitters[1].start_seconds, 0.0f);
  // A one-shot: off, on at 0.5 s, off after its second on; the impact long enough for it and its particles' life.
  std::string model = WriteVfxModel(effect, "n5t");
  std::string impact = Section(model, "newanim impact n5t", "doneanim impact n5t");
  CHECK_EQ(Section(impact, "node emitter n5t_0_late", "endnode")
                   .find("  birthratekey 5\n    0 0\n    0.5 0\n    0.51 20\n    1.5 20\n    1.51 0\n") != std::string::npos,
           true);
  CHECK_EQ(impact.find("  length 2.51\n") != std::string::npos, true);  // its off at 1.5 s + its particles' 1 s
  // A lasting effect: an always-on emitter off until 0.3 s, then on through the impact and (its restore key) the duration.
  VfxEffect lasting;
  lasting.duration = true;
  VfxEmitter vines;
  vines.name = "vines";
  vines.birthrate = 8.0f;
  vines.start_seconds = 0.3f;
  lasting.emitters.push_back(vines);
  model = WriteVfxModel(lasting, "n5t");
  impact = Section(model, "newanim impact n5t", "doneanim impact n5t");
  const std::string duration = Section(model, "newanim duration n5t", "doneanim duration n5t");
  CHECK_EQ(Section(impact, "node emitter n5t_0_vines", "endnode").find("  birthratekey 3\n    0 0\n    0.3 0\n    0.31 8\n") !=
               std::string::npos,
           true);
  CHECK_EQ(Section(duration, "node emitter n5t_0_vines", "endnode").find("  birthratekey 1\n    0 8\n") != std::string::npos, true);
  CHECK_EQ(Section(Section(model, "beginmodelgeom n5t", "endmodelgeom n5t"), "node emitter n5t_0_vines", "endnode")
                   .find("  birthrate 0\n") != std::string::npos,
           true);  // keyed: off until the impact turns it on
}

// A burst (Explosion) that starts late: the game bursts an Explosion on the impact's detonate event with its birthrate
// then, so a late one gets its own event (0.01 s after its start) and each Explosion's rate is on only round its own
// event (kVfxBurstWindow), so no burst goes off twice. Without a late burst: written as ever.
TEST(VfxExplosionStartsAfter) {
  VfxEffect effect;
  effect.duration = false;
  effect.seconds = 2.0f;
  VfxEmitter bang;
  bang.name = "bang";
  bang.update = "Explosion";
  bang.birthrate = 30.0f;
  effect.emitters.push_back(bang);
  std::string impact = Section(WriteVfxModel(effect, "n5t"), "newanim impact n5t", "doneanim impact n5t");
  CHECK_EQ(Section(impact, "node emitter n5t_0_bang", "endnode").find("  birthratekey 3\n    0 30\n    2 30\n    2.01 0\n") !=
               std::string::npos,
           true);  // alone: unchanged
  CHECK_EQ(impact.find("  event 0 detonate\n") != std::string::npos && impact.find("detonate", impact.find("detonate") + 1) == std::string::npos,
           true);
  VfxEmitter echo;
  echo.name = "echo";
  echo.update = "Explosion";
  echo.birthrate = 12.0f;
  echo.start_seconds = 0.5f;
  effect.emitters.push_back(echo);
  impact = Section(WriteVfxModel(effect, "n5t"), "newanim impact n5t", "doneanim impact n5t");
  CHECK_EQ(impact.find("  event 0 detonate\n") != std::string::npos && impact.find("  event 0.51 detonate\n") != std::string::npos, true);
  CHECK_EQ(Section(impact, "node emitter n5t_0_bang", "endnode").find("  birthratekey 3\n    0 30\n    0.05 30\n    0.06 0\n") !=
               std::string::npos,
           true);
  CHECK_EQ(Section(impact, "node emitter n5t_1_echo", "endnode")
                   .find("  birthratekey 5\n    0 0\n    0.5 0\n    0.51 12\n    0.56 12\n    0.57 0\n") != std::string::npos,
           true);
  CHECK_EQ(kVfxBurstWindow, 0.05f);
}

// --- An emitter's start frame and its sheet's frames down (the owner, 2026-11-17: "I'm trying to understand how vfx 145
// works, it calls down a magic circle from somewhere, selecting one of the quadrants from the texture file, but I can't
// seem to alter these configurations" - vim_protevil's OmenEmitter01 shows one frame of a 2 x 2 sheet, its start frame,
// which nothing of nwn5e's read) ---

// The draft: "grid_down = N" right after "grid = " only when it isn't 0 or the grid (frames across), "frame_start = N"
// right after "frame_end = " only when it isn't 0 - every draft before them keeps its bytes - and back as written (a start
// past the end too: the draft keeps what it says; frames down and the start never below 0).
TEST(VfxFrameStartAndGridDownThroughTheDraft) {
  VfxEffect effect = Sample();  // fps 16
  effect.emitters[0].grid = 4;
  effect.emitters[0].grid_down = 2;
  effect.emitters[0].frame_end = 7;
  effect.emitters[0].frame_start = 3;
  const std::string draft = WriteVfxDraft(effect);
  CHECK_EQ(Contains(draft, "\ngrid = 4\ngrid_down = 2\nframe_end = 7\nframe_start = 3\nfps = 16\n"), true);
  VfxEffect back;
  std::string error;
  CHECK_EQ(ReadVfxDraft(draft, &back, &error) && back.emitters.size() == 1, true);
  if (back.emitters.size() != 1) return;
  CHECK_EQ(back.emitters[0].grid, 4);
  CHECK_EQ(back.emitters[0].grid_down, 2);
  CHECK_EQ(back.emitters[0].frame_end, 7);
  CHECK_EQ(back.emitters[0].frame_start, 3);
  CHECK_EQ(WriteVfxDraft(back) == draft, true);
  // The defaults - and frames down the same as across - write no line: the bytes nwn5e wrote before.
  const std::string plain = WriteVfxDraft(Sample());
  CHECK_EQ(Contains(plain, "\ngrid = 2\nframe_end = 3\nfps = 16\n"), true);
  CHECK_EQ(Contains(plain, "grid_down") || Contains(plain, "frame_start"), false);
  VfxEffect square = Sample();
  square.emitters[0].grid_down = 2;
  CHECK_EQ(WriteVfxDraft(square) == plain, true);
  CHECK_EQ(ReadVfxDraft(plain, &back, &error) && back.emitters.size() == 1 && back.emitters[0].grid_down == 0 &&
               back.emitters[0].frame_start == 0,
           true);
  // As written: a start past the end kept; below 0 read as 0.
  CHECK_EQ(ReadVfxDraft("name = x\n[emitter a]\nframe_end = 1\nframe_start = 5\ngrid_down = -2\n", &back, &error), true);
  if (!back.emitters.empty()) CHECK_EQ(back.emitters[0].frame_start == 5 && back.emitters[0].grid_down == 0, true);
  CHECK_EQ(ReadVfxDraft("name = x\n[emitter a]\nframe_start = -1\n", &back, &error), true);
  if (!back.emitters.empty()) CHECK_EQ(back.emitters[0].frame_start, 0);
}

// A random frame each (chkRandom) is frameStart + rand() % (frameEnd - frameStart + 1) in the game (nwn5e-tools
// PARTICLES.md, from the decompile): a start one past the end divides by 0. The model writes "random 0" then - the draft
// keeps the flag as written; a span of 1 or more frames keeps "random 1".
TEST(VfxModelRandomWithoutFramesOff) {
  VfxEffect effect = Sample();  // random, frames 0..3
  effect.emitters[0].frame_start = 4;
  const std::string draft = WriteVfxDraft(effect);
  CHECK_EQ(Contains(draft, "\nframe_end = 3\nframe_start = 4\n") && Contains(draft, "\nflags = splat random twosidedtex\n"), true);
  VfxEffect back;
  std::string error;
  CHECK_EQ(ReadVfxDraft(draft, &back, &error) && back.emitters.size() == 1 && back.emitters[0].random, true);
  CHECK_EQ(WriteVfxDraft(back) == draft, true);
  const std::string model = WriteVfxModel(back, "n5t");
  CHECK_EQ(Contains(model, "  random 0\n"), true);
  CHECK_EQ(Contains(model, "  random 1\n"), false);
  for (const int start : {0, 3}) {  // frames 0..3, and the one frame 3..3
    VfxEffect span = Sample();
    span.emitters[0].frame_start = start;
    CHECK_EQ(Contains(WriteVfxModel(span, "n5t"), "  random 1\n"), true);
  }
  VfxEffect plain = Sample();
  plain.emitters[0].random = false;
  plain.emitters[0].frame_start = 4;
  CHECK_EQ(Contains(WriteVfxModel(plain, "n5t"), "  random 0\n"), true);
}

// A draft's frames down the same as across reads as 0, after the whole section (either line first) - the rule of both
// model readers, so the three agree - and then follows the grid: a square sheet stays square when the grid changes.
TEST(VfxDraftGridDownSameAsGridReadsAsZero) {
  VfxEffect back;
  std::string error;
  for (const char* text : {"name = x\n[emitter a]\ngrid = 2\ngrid_down = 2\n", "name = x\n[emitter a]\ngrid_down = 2\ngrid = 2\n"}) {
    CHECK_EQ(ReadVfxDraft(text, &back, &error) && back.emitters.size() == 1, true);
    if (back.emitters.size() != 1) continue;
    CHECK_EQ(back.emitters[0].grid, 2);
    CHECK_EQ(back.emitters[0].grid_down, 0);
    VfxEffect bigger = back;
    bigger.emitters[0].grid = 4;  // 4 x 4 now: no frames-down line, ygrid 4
    CHECK_EQ(Contains(WriteVfxDraft(bigger), "grid_down"), false);
    CHECK_EQ(Contains(WriteVfxModel(bigger, "n5t"), "  xgrid 4\n  ygrid 4\n"), true);
  }
  CHECK_EQ(ReadVfxDraft("name = x\n[emitter a]\ngrid = 4\ngrid_down = 2\n", &back, &error) && back.emitters.size() == 1 &&
               back.emitters[0].grid_down == 2,
           true);  // not square: kept
}

// The model: "ygrid" is the frames down (the grid when 0) and "frameStart" the start frame (0 before: always). The ASCII
// reader takes them back - ygrid the same as xgrid, in either order, as 0 (the draft stays as it was).
TEST(VfxModelWritesFrameStartAndGridDown) {
  VfxEffect effect = Sample();
  effect.emitters[0].grid = 4;
  effect.emitters[0].grid_down = 2;
  effect.emitters[0].frame_end = 7;
  effect.emitters[0].frame_start = 3;
  const std::string model = WriteVfxModel(effect, "n5t");
  CHECK_EQ(Contains(model, "  xgrid 4\n  ygrid 2\n"), true);
  CHECK_EQ(Contains(model, "  frameStart 3\n  frameEnd 7\n"), true);
  VfxEffect back;
  std::string error;
  CHECK_EQ(ReadVfxAsciiModel(model, &back, &error) && back.emitters.size() == 1, true);
  if (back.emitters.size() == 1) {
    CHECK_EQ(back.emitters[0].grid, 4);
    CHECK_EQ(back.emitters[0].grid_down, 2);
    CHECK_EQ(back.emitters[0].frame_end, 7);
    CHECK_EQ(back.emitters[0].frame_start, 3);
  }
  const std::string plain = WriteVfxModel(Sample(), "n5t");
  CHECK_EQ(Contains(plain, "  xgrid 2\n  ygrid 2\n") && Contains(plain, "  frameStart 0\n  frameEnd 3\n"), true);
  CHECK_EQ(ReadVfxAsciiModel(plain, &back, &error) && back.emitters.size() == 1, true);
  if (back.emitters.size() == 1) CHECK_EQ(back.emitters[0].grid == 2 && back.emitters[0].grid_down == 0 && back.emitters[0].frame_start == 0, true);
  const auto one = [&](const std::string& lines) {
    VfxEffect e;
    std::string why;
    const bool ok = ReadVfxAsciiModel("newmodel t\nbeginmodelgeom t\nnode dummy t\n  parent NULL\nendnode\nnode emitter e\n"
                                      "  parent t\n" + lines + "endnode\nendmodelgeom t\ndonemodel t\n", &e, &why);
    return ok && e.emitters.size() == 1 ? e.emitters[0] : VfxEmitter();
  };
  VfxEmitter e = one("  ygrid 3\n  xgrid 3\n  framestart 2\n  frameend 2\n");  // another writer's order
  CHECK_EQ(e.grid == 3 && e.grid_down == 0 && e.frame_start == 2 && e.frame_end == 2, true);
  e = one("  ygrid 1\n  xgrid 2\n  frameEnd 1\n");
  CHECK_EQ(e.grid == 2 && e.grid_down == 1 && e.frame_start == 0, true);
}

// A compiled model's: the emitter header's y grid at +0x10, beside the x grid at +0x0C (the game's own sheets that aren't
// square: head_01's fxpa_flame03 8 x 4, frames 0..31; tin01_b02_03's fxpa_candle01 4 x 2, 0..7; vim_rayblack's
// fxpa_beam02 2 x 1, 0..1), and controller 136, frameStart (the field offset - 28, as 128 fps and 132 frameEnd). A y grid
// of 0 or past 64 (no game sheet has more than 8) is no sheet's: 0, the same as across. A frame below 0 is 0.
TEST(VfxReadsFrameStartAndGridDownFromACompiledModel) {
  const auto read = [](uint32_t xgrid, uint32_t ygrid, bool keyed, float start, float end = 3.0f) {
    std::string b(0x400, '\0');
    const auto put32 = [&](size_t at, uint32_t v) { memcpy(&b[at], &v, 4); };
    const auto putf = [&](size_t at, float v) { memcpy(&b[at], &v, 4); };
    const auto puts = [&](size_t at, const char* text) { memcpy(&b[at], text, strlen(text)); };
    const size_t base = 12, node = 0x100, emitter = node + 0x60;
    puts(base + 8, "test_fx");
    puts(node + 0x10, "omen");
    put32(emitter + 0x0C, xgrid);
    put32(emitter + 0x10, ygrid);
    puts(emitter + 0x18, "Fountain");
    puts(emitter + 0x38, "Normal");
    puts(emitter + 0x78, "fxpa_symbl_001");
    const size_t keys = 0x300, data = 0x340;
    const uint32_t count = keyed ? 3 : 2;
    put32(node + 0x44, static_cast<uint32_t>(keys - base));
    put32(node + 0x48, count);
    put32(node + 0x50, static_cast<uint32_t>(data - base));
    put32(node + 0x54, count + 1);
    // birthrate (88), frameEnd (132), frameStart (136): 1 row each, the time at 0, 1 column.
    const uint32_t types[3] = {88, 132, 136};
    for (uint32_t k = 0; k < count; ++k) {
      put32(keys + 12 * k, types[k]);
      b[keys + 12 * k + 4] = 1;
      b[keys + 12 * k + 8] = static_cast<char>(k + 1);
      b[keys + 12 * k + 10] = 1;
    }
    putf(data + 4, 10.0f);
    putf(data + 8, end);
    putf(data + 12, start);
    VfxEffect effect;
    std::string error;
    return ReadVfxBinaryModel(b, &effect, &error) && effect.emitters.size() == 1 ? effect.emitters[0] : VfxEmitter();
  };
  VfxEmitter e = read(4, 2, true, 3.0f);
  CHECK_EQ(e.name == "omen", true);
  CHECK_EQ(e.grid, 4);
  CHECK_EQ(e.grid_down, 2);
  CHECK_EQ(e.frame_end, 3);
  CHECK_EQ(e.frame_start, 3);
  e = read(2, 2, true, 1.0f);  // a square sheet: frames down 0, as a draft has it
  CHECK_EQ(e.grid == 2 && e.grid_down == 0 && e.frame_start == 1, true);
  e = read(2, 2, false, 0.0f);  // no controller 136: frame 0
  CHECK_EQ(e.name == "omen" && e.frame_start == 0 && e.grid_down == 0, true);
  for (const uint32_t junk : {0xFFFFFFFFu, 0u, 1000u, 65u}) {  // no sheet's: the same as across
    e = read(4, junk, true, 0.0f);
    CHECK_EQ(e.name == "omen" && e.grid == 4 && e.grid_down == 0, true);
  }
  e = read(4, 64, true, 0.0f);  // the most kept
  CHECK_EQ(e.grid_down, 64);
  e = read(2, 2, true, -2.0f, -1.0f);  // below 0: frame 0
  CHECK_EQ(e.name == "omen" && e.frame_start == 0 && e.frame_end == 0, true);
}

// The drafts nwn5e keeps - the tests' fixtures, and Detect Magic's four (tools/data/vfx; their models in data/files,
// written by WriteVfxModel): each draft writes back its own bytes, each model the committed one - with a start frame or
// frames down set in the bench or without (before them, none had either).
TEST(VfxKeptDraftsUnchanged) {
  for (const char* path : {"tests/data/fixtures/plane_beat.vfx", "tests/data/fixtures/plane_effect.vfx",
                           "tests/data/fixtures/plane_material.vfx"}) {
    const std::string text = FixtureText(path);
    VfxEffect effect;
    std::string error;
    CHECK_EQ(!text.empty() && ReadVfxDraft(text, &effect, &error) && WriteVfxDraft(effect) == text, true);
  }
  for (const char* size : {"h", "l", "m", "s"}) {
    const std::string name = std::string("n5_detmag_") + size;
    const std::string text = FixtureText(("tools/data/vfx/" + name + ".vfx").c_str());
    VfxEffect effect;
    std::string error;
    CHECK_EQ(ReadVfxDraft(text, &effect, &error) && !effect.emitters.empty(), true);
    CHECK_EQ(WriteVfxDraft(effect) == text, true);
    const std::string model = FixtureText(("data/files/" + name + ".mdl").c_str());
    CHECK_EQ(!model.empty() && WriteVfxModel(effect, name) == model, true);
  }
}

// The anchor (the owner's question - visual effect row 145, VFX_IMP_GOOD_HELP): the game's own vim_protevil as the VFX
// Bench reads it - OmenEmitter01 shows the last quadrant of fxpa_Symbl_001 (2 x 2, frames 3..3, still) - and vim_rayblack's
// beam sheet, 2 across and 1 down. Only where the game is (NWN_GAME_DIR, or Steam's folder); nothing of it is kept.
TEST(VfxGameEffectsStartFrame) {
  const char* given = getenv("NWN_GAME_DIR");
  const std::string game = given && *given ? given : "X:\\SteamLibrary\\steamapps\\common\\Neverwinter Nights";
  if (!std::ifstream(game + "\\data\\nwn_base.key")) {
    printf("    (no game at %s: skipped)\n", game.c_str());
    return;
  }
  const std::wstring dir(game.begin(), game.end());
  GameResources resources(dir, GameResources::EeKeyFiles(), {dir + L"\\ovr"});
  const auto emitter = [&](const char* model, const char* name, VfxEffect* effect) -> const VfxEmitter* {
    std::string bytes, source, error;
    if (!resources.Read(model, kResourceTypeMdl, &bytes, &source) || !ReadVfxBinaryModel(bytes, effect, &error)) return nullptr;
    for (const VfxEmitter& e : effect->emitters) {
      if (e.name == name) return &e;
    }
    return nullptr;
  };
  VfxEffect protevil;
  const VfxEmitter* omen = emitter("vim_protevil", "OmenEmitter01", &protevil);
  CHECK_EQ(omen != nullptr, true);
  if (omen) {
    printf("    vim_protevil OmenEmitter01: %s, grid %d, grid_down %d, frames %d..%d, fps %g\n", omen->texture.c_str(), omen->grid,
           omen->grid_down, omen->frame_start, omen->frame_end, omen->fps);
    CHECK_EQ(omen->texture == "fxpa_Symbl_001", true);
    CHECK_EQ(omen->grid, 2);
    CHECK_EQ(omen->grid_down, 0);
    CHECK_EQ(omen->frame_end, 3);
    CHECK_EQ(omen->frame_start, 3);
    CHECK_EQ(Milli(omen->fps), 0);
    CHECK_EQ(Contains(WriteVfxDraft(protevil), "\ngrid = 2\nframe_end = 3\nframe_start = 3\n"), true);
  }
  VfxEffect ray;
  const VfxEmitter* beam = emitter("vim_rayblack", "OmenEmitter03", &ray);
  CHECK_EQ(beam != nullptr, true);
  if (beam) {
    CHECK_EQ(beam->grid, 2);
    CHECK_EQ(beam->grid_down, 1);
    CHECK_EQ(beam->frame_end, 1);
    CHECK_EQ(Contains(WriteVfxModel(ray, "n5t"), "  xgrid 2\n  ygrid 1\n"), true);
  }
}

// --- An emitter's blast (the owner, 2026-11-17, on row 145's WIND_Dummy: "nor most of the properties of the "WIND_Dummy"
// emitter which I assume is something that generates a shockwave on elements in the environment?" - then "go with all
// three"): the emitter header's blast radius (metres) and blast length (seconds), which nothing of nwn5e's read. When an
// Explosion emitter detonates with a radius (ExplosionEmitter::Update), the game adds a wind there
// (WindManager::AddPointSourceWind) that pushes what is within the radius away from it - grass, trees, particles that take
// wind - for the length. ---

// The draft: "blast_radius = R" and "blast_length = L" right after "deadspace = ", each only when it isn't 0 - every draft
// before them keeps its bytes - and back as written, by the rule every reader keeps (below 0, not finite or past 1000: 0).
TEST(VfxBlastThroughTheDraft) {
  VfxEffect effect = Sample();  // deadspace 0.2, no point to point: the flags next
  effect.emitters[0].update = "Explosion";
  effect.emitters[0].blast_radius = 8.0f;
  effect.emitters[0].blast_length = 1.3f;
  const std::string draft = WriteVfxDraft(effect);
  CHECK_EQ(Contains(draft, "\ndeadspace = 0.2\nblast_radius = 8\nblast_length = 1.3\nflags = splat random twosidedtex\n"), true);
  VfxEffect back;
  std::string error;
  CHECK_EQ(ReadVfxDraft(draft, &back, &error) && back.emitters.size() == 1, true);
  if (back.emitters.size() != 1) return;
  CHECK_EQ(Milli(back.emitters[0].blast_radius), 8000);
  CHECK_EQ(Milli(back.emitters[0].blast_length), 1300);
  CHECK_EQ(WriteVfxDraft(back) == draft, true);
  // Each on its own: a length without a radius (some of the game's door explosions have one), a radius without a length.
  VfxEffect one = Sample();
  one.emitters[0].blast_length = 0.3f;
  CHECK_EQ(Contains(WriteVfxDraft(one), "\ndeadspace = 0.2\nblast_length = 0.3\nflags ="), true);
  CHECK_EQ(Contains(WriteVfxDraft(one), "blast_radius"), false);
  one = Sample();
  one.emitters[0].blast_radius = 0.2f;
  CHECK_EQ(Contains(WriteVfxDraft(one), "\ndeadspace = 0.2\nblast_radius = 0.2\nflags ="), true);
  CHECK_EQ(Contains(WriteVfxDraft(one), "blast_length"), false);
  // None: no line - the bytes nwn5e wrote before.
  const std::string plain = WriteVfxDraft(Sample());
  CHECK_EQ(Contains(plain, "\ndeadspace = 0.2\nflags = "), true);
  CHECK_EQ(Contains(plain, "blast"), false);
  CHECK_EQ(ReadVfxDraft(plain, &back, &error) && back.emitters.size() == 1 && back.emitters[0].blast_radius == 0.0f &&
               back.emitters[0].blast_length == 0.0f,
           true);
  // Below 0, not a number, not finite or past 1000: 0 - the binary reader's rule, the same in every reader (the bench's
  // core.js keeps it too). 1000 itself is kept.
  CHECK_EQ(ReadVfxDraft("name = x\n[emitter a]\nblast_radius = -3\nblast_length = nan\n", &back, &error) &&
               back.emitters.size() == 1 && back.emitters[0].blast_radius == 0.0f && back.emitters[0].blast_length == 0.0f,
           true);
  CHECK_EQ(ReadVfxDraft("name = x\n[emitter a]\nblast_radius = inf\nblast_length = 1e30\n", &back, &error) &&
               back.emitters.size() == 1 && back.emitters[0].blast_radius == 0.0f && back.emitters[0].blast_length == 0.0f,
           true);
  CHECK_EQ(ReadVfxDraft("name = x\n[emitter a]\nblast_radius = 1e30\nblast_length = inf\n", &back, &error) &&
               back.emitters.size() == 1 && back.emitters[0].blast_radius == 0.0f && back.emitters[0].blast_length == 0.0f,
           true);
  CHECK_EQ(ReadVfxDraft("name = x\n[emitter a]\nblast_radius = 1000\nblast_length = 1000\n", &back, &error) &&
               back.emitters.size() == 1 && Milli(back.emitters[0].blast_radius) == 1000000 &&
               Milli(back.emitters[0].blast_length) == 1000000,
           true);
}

// The model: "blastRadius" and "blastLength" (the game's spelling - 2,430 of each in its ASCII models; its parser takes
// any case) carry the blast. A model without one keeps its bytes: nwn5e always wrote both lines as 0. The ASCII reader
// takes them back, any case, by the rule every reader keeps (below 0, not finite or past 1000: 0).
TEST(VfxModelWritesBlast) {
  VfxEffect effect = Sample();
  effect.emitters[0].update = "Explosion";
  effect.emitters[0].blast_radius = 8.0f;
  effect.emitters[0].blast_length = 1.3f;
  const std::string model = WriteVfxModel(effect, "n5t");
  CHECK_EQ(Contains(model, "  twosidedtex 1\n  blastRadius 8\n  blastLength 1.3\n  position "), true);
  VfxEffect back;
  std::string error;
  CHECK_EQ(ReadVfxAsciiModel(model, &back, &error) && back.emitters.size() == 1, true);
  if (back.emitters.size() == 1) {
    CHECK_EQ(Milli(back.emitters[0].blast_radius), 8000);
    CHECK_EQ(Milli(back.emitters[0].blast_length), 1300);
    CHECK_EQ(Contains(WriteVfxDraft(back), "\nblast_radius = 8\nblast_length = 1.3\n"), true);
  }
  const std::string plain = WriteVfxModel(Sample(), "n5t");
  CHECK_EQ(Contains(plain, "  twosidedtex 1\n  blastRadius 0\n  blastLength 0\n  position "), true);
  CHECK_EQ(ReadVfxAsciiModel(plain, &back, &error) && back.emitters.size() == 1 && back.emitters[0].blast_radius == 0.0f &&
               back.emitters[0].blast_length == 0.0f,
           true);
  VfxEffect negative = Sample();  // never from a reader: written as 0, as a draft reads it
  negative.emitters[0].blast_radius = -1.0f;
  negative.emitters[0].blast_length = -2.0f;
  CHECK_EQ(Contains(WriteVfxModel(negative, "n5t"), "  blastRadius 0\n  blastLength 0\n"), true);
  const auto one = [&](const std::string& lines) {
    VfxEffect e;
    std::string why;
    const bool ok = ReadVfxAsciiModel("newmodel t\nbeginmodelgeom t\nnode dummy t\n  parent NULL\nendnode\nnode emitter e\n"
                                      "  parent t\n" + lines + "endnode\nendmodelgeom t\ndonemodel t\n", &e, &why);
    return ok && e.emitters.size() == 1 ? e.emitters[0] : VfxEmitter();
  };
  VfxEmitter e = one("  update Explosion\n  BLASTRADIUS 15\n  blastlength 1.2\n");
  CHECK_EQ(e.update == "Explosion" && Milli(e.blast_radius) == 15000 && Milli(e.blast_length) == 1200, true);
  e = one("  update Explosion\n  blastRadius -2\n  blastLength -1\n");
  CHECK_EQ(e.update == "Explosion" && e.blast_radius == 0.0f && e.blast_length == 0.0f, true);
  // Not finite or past 1000: 0, as every reader has it; 1000 kept.
  e = one("  update Explosion\n  blastRadius inf\n  blastLength 1e30\n");
  CHECK_EQ(e.update == "Explosion" && e.blast_radius == 0.0f && e.blast_length == 0.0f, true);
  e = one("  update Explosion\n  blastRadius 1000\n  blastLength 1000\n");
  CHECK_EQ(e.update == "Explosion" && Milli(e.blast_radius) == 1000000 && Milli(e.blast_length) == 1000000, true);
}

// A compiled model's: the emitter header's floats at +0x04 (blast radius) and +0x08 (blast length), between deadspace
// (+0x00) and the x grid (+0x0C) - the game's parser (MdlNodeEmitter::InternalParseField) puts blastRadius at node +0xa4
// and blastLength at +0xa8, beside deadspace's +0xa0. Below 0, not a number or past 1000 (no game emitter's goes past 20 m
// or 4 s; an animation's copy of a node can hold other bytes there): 0.
TEST(VfxReadsBlastFromACompiledModel) {
  const auto read = [](float deadspace, float radius, float length) {
    std::string b(0x400, '\0');
    const auto put32 = [&](size_t at, uint32_t v) { memcpy(&b[at], &v, 4); };
    const auto putf = [&](size_t at, float v) { memcpy(&b[at], &v, 4); };
    const auto puts = [&](size_t at, const char* text) { memcpy(&b[at], text, strlen(text)); };
    const size_t base = 12, node = 0x100, emitter = node + 0x60;
    puts(base + 8, "test_fx");
    puts(node + 0x10, "wind");
    putf(emitter, deadspace);
    putf(emitter + 0x04, radius);
    putf(emitter + 0x08, length);
    put32(emitter + 0x0C, 1);
    put32(emitter + 0x10, 1);
    puts(emitter + 0x18, "Explosion");
    puts(emitter + 0x38, "Normal");
    puts(emitter + 0x78, "NULL");
    const size_t keys = 0x300, data = 0x340;
    put32(node + 0x44, static_cast<uint32_t>(keys - base));
    put32(node + 0x48, 1);
    put32(node + 0x50, static_cast<uint32_t>(data - base));
    put32(node + 0x54, 2);
    put32(keys, 88);  // birthrate: 1 row, the time at 0, the value at 1, 1 column
    b[keys + 4] = 1;
    b[keys + 8] = 1;
    b[keys + 10] = 1;
    putf(data + 4, 1.0f);
    VfxEffect effect;
    std::string error;
    return ReadVfxBinaryModel(b, &effect, &error) && effect.emitters.size() == 1 ? effect.emitters[0] : VfxEmitter();
  };
  VfxEmitter e = read(0.1f, 8.0f, 1.3f);
  CHECK_EQ(e.name == "wind" && e.update == "Explosion", true);
  CHECK_EQ(Milli(e.deadspace), 100);
  CHECK_EQ(Milli(e.blast_radius), 8000);
  CHECK_EQ(Milli(e.blast_length), 1300);
  e = read(0.0f, 0.0f, 0.0f);  // none
  CHECK_EQ(e.name == "wind" && e.blast_radius == 0.0f && e.blast_length == 0.0f, true);
  e = read(0.0f, 0.0f, 0.3f);  // a length without a radius: kept, as the game's models have it
  CHECK_EQ(e.name == "wind" && e.blast_radius == 0.0f && Milli(e.blast_length) == 300, true);
  float vim;  // "vim_" read as a float: an animation copy's bytes
  memcpy(&vim, "vim_", 4);
  for (const float junk : {-1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), 1000.5f, vim}) {
    e = read(0.0f, junk, junk);
    CHECK_EQ(e.name == "wind" && e.blast_radius == 0.0f && e.blast_length == 0.0f, true);
  }
  e = read(0.0f, 1000.0f, 1000.0f);  // the most kept
  CHECK_EQ(Milli(e.blast_radius) == 1000000 && Milli(e.blast_length) == 1000000, true);
}

// The anchor (the owner's row 145, VFX_IMP_GOOD_HELP): vim_protevil's WIND_Dummy - an Explosion that draws nothing
// (texture NULL) - blasts 8 m round for 1.3 s; its OmenEmitter01 (the Single that draws the symbol) has none. Only where
// the game is (NWN_GAME_DIR, or Steam's folder); nothing of it is kept.
TEST(VfxGameEffectsBlast) {
  const char* given = getenv("NWN_GAME_DIR");
  const std::string game = given && *given ? given : "X:\\SteamLibrary\\steamapps\\common\\Neverwinter Nights";
  if (!std::ifstream(game + "\\data\\nwn_base.key")) {
    printf("    (no game at %s: skipped)\n", game.c_str());
    return;
  }
  const std::wstring dir(game.begin(), game.end());
  GameResources resources(dir, GameResources::EeKeyFiles(), {dir + L"\\ovr"});
  std::string bytes, source, error;
  VfxEffect protevil;
  CHECK_EQ(resources.Read("vim_protevil", kResourceTypeMdl, &bytes, &source) && ReadVfxBinaryModel(bytes, &protevil, &error),
           true);
  const VfxEmitter* wind = nullptr;
  const VfxEmitter* omen = nullptr;
  for (const VfxEmitter& e : protevil.emitters) {
    if (e.name == "WIND_Dummy") wind = &e;
    if (e.name == "OmenEmitter01") omen = &e;
  }
  CHECK_EQ(wind != nullptr && omen != nullptr, true);
  if (!wind || !omen) return;
  printf("    vim_protevil WIND_Dummy: %s, texture %s, blast radius %g, blast length %g\n", wind->update.c_str(),
         wind->texture.c_str(), wind->blast_radius, wind->blast_length);
  CHECK_EQ(wind->update == "Explosion" && wind->texture == "NULL", true);
  CHECK_EQ(Milli(wind->blast_radius), 8000);
  CHECK_EQ(Milli(wind->blast_length), 1300);
  CHECK_EQ(omen->blast_radius == 0.0f && omen->blast_length == 0.0f, true);
  CHECK_EQ(Contains(WriteVfxDraft(protevil), "\ndeadspace = 0\nblast_radius = 8\nblast_length = 1.3\n"), true);
  CHECK_EQ(Contains(WriteVfxModel(protevil, "n5t"), "  blastRadius 8\n  blastLength 1.3\n"), true);
}

// --- A plane's transparencyhint (the owner, 2026-11-18: n5_zot's see-through meshes cut away with jagged edges, a rune
// emitter hidden behind them - "go"): without one the game draws a mesh solid - its shader drops a pixel where texture x
// material alpha <= 0.2 and writes depth; with one it is see-through (nwn5e's placeables with 1 blend in game). The game's
// own: 0 to 5, an integer (vdr_globe01's globe 1, vdr_magearmor2's shields 2). Every reader and writer keeps 0 to 5:
// below 0 (or not a number) 0, above 5 5. ---

// The draft: "transparencyhint = N" with the plane's material lines - after "render = 0" (where it would be), before a
// mesh beat's "mesh = " - only when it isn't 0, so every draft before it keeps its bytes; read back as written. Read as the
// game reads it: an integer's leading digits ("1.7" is 1), not a number 0.
TEST(VfxPlaneTransparencyHintThroughTheDraft) {
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(PlaneModel(true), &effect, &error) && effect.planes.size() == 1, true);
  if (effect.planes.size() != 1) return;
  CHECK_EQ(effect.planes[0].transparency_hint, 0);
  effect.planes[0].transparency_hint = 1;
  const std::string draft = WriteVfxDraft(effect);
  CHECK_EQ(Contains(draft, "bitmap = fxpa_fern\ntransparencyhint = 1\nverts ="), true);
  VfxEffect back;
  CHECK_EQ(ReadVfxDraft(draft, &back, &error) && back.planes.size() == 1 && back.planes[0].transparency_hint == 1, true);
  CHECK_EQ(WriteVfxDraft(back) == draft, true);
  // With the material and render 0: after them.
  VfxEffect lit;
  CHECK_EQ(ReadVfxBinaryModel(PlaneModel(true, 0x21, true), &lit, &error) && lit.planes.size() == 1, true);
  if (lit.planes.size() != 1) return;
  lit.planes[0].render = false;
  lit.planes[0].transparency_hint = 2;
  const std::string hidden = WriteVfxDraft(lit);
  CHECK_EQ(Contains(hidden, "selfillum = 0 1 0.5\nalpha = 0.6\nrender = 0\ntransparencyhint = 2\nverts ="), true);
  CHECK_EQ(ReadVfxDraft(hidden, &back, &error) && back.planes.size() == 1 && back.planes[0].transparency_hint == 2 &&
               !back.planes[0].render && WriteVfxDraft(back) == hidden,
           true);
  // A mesh beat's: before its mesh line.
  VfxEffect beat = BeatEffect();
  beat.planes[0].transparency_hint = 1;
  const std::string beat_draft = WriteVfxDraft(beat);
  CHECK_EQ(Contains(beat_draft, "bitmap = fxpa_fern\ntransparencyhint = 1\nmesh = n5m_test\n"), true);
  CHECK_EQ(ReadVfxDraft(beat_draft, &back, &error) && back.planes.size() == 1 && back.planes[0].transparency_hint == 1 &&
               WriteVfxDraft(back) == beat_draft,
           true);
  // None: no line - the bytes before it (plane_beat.vfx is BeatEffect's draft).
  CHECK_EQ(Contains(WriteVfxDraft(BeatEffect()), "transparencyhint"), false);
  CHECK_EQ(WriteVfxDraft(BeatEffect()) == FixtureText("tests/data/fixtures/plane_beat.vfx"), true);
  // Every value as read, 0 to 5; below 0 or not a number 0, above 5 5, a fraction its integer.
  const auto read = [](const std::string& value) {
    VfxEffect e;
    std::string why;
    const bool ok = ReadVfxDraft("name = x\n[plane p]\ntransparencyhint = " + value + "\n", &e, &why) && e.planes.size() == 1;
    return ok ? e.planes[0].transparency_hint : -100;
  };
  for (int v = 0; v <= 5; ++v) CHECK_EQ(read(std::to_string(v)), v);
  CHECK_EQ(read("6"), 5);
  CHECK_EQ(read("99999999999"), 5);
  CHECK_EQ(read("-1"), 0);
  CHECK_EQ(read("-99999999999"), 0);
  CHECK_EQ(read("x"), 0);
  CHECK_EQ(read(""), 0);
  CHECK_EQ(read("1.7"), 1);
  CHECK_EQ(read("+2"), 2);
  CHECK_EQ(read("\v3"), 3);       // before the digits only what isspace skips (floatdisk.js and core.js read alike) ...
  CHECK_EQ(read("\xA0" "2"), 0);  // ... a no-break space (latin1 0xA0) stops it
  // In memory past the range (never from a reader): written as a reader would keep it; below 0, no line.
  VfxEffect wide = BeatEffect();
  wide.planes[0].transparency_hint = 9;
  CHECK_EQ(Contains(WriteVfxDraft(wide), "\ntransparencyhint = 5\n"), true);
  wide.planes[0].transparency_hint = -1;
  CHECK_EQ(WriteVfxDraft(wide) == WriteVfxDraft(BeatEffect()), true);
}

// The model: "  transparencyhint N" on the plane's trimesh after its material lines (render, shadow, selfillumcolor,
// alpha), only when it isn't 0 - every model before it keeps its bytes; the ASCII reader takes it back (any case, the
// same rule), and so through the model to the draft.
TEST(VfxModelWritesTransparencyHint) {
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(PlaneModel(true, 0x21, true), &effect, &error) && effect.planes.size() == 1, true);
  if (effect.planes.size() != 1) return;
  const std::string before = WriteVfxModel(effect, "n5t");
  effect.planes[0].transparency_hint = 1;
  const std::string model = WriteVfxModel(effect, "n5t");
  CHECK_EQ(Contains(model, "  render 1\n  shadow 0\n  selfillumcolor 0 1 0.5\n  alpha 0.6\n  transparencyhint 1\n  verts 4\n"), true);
  // Only that line more.
  const size_t at = model.find("  transparencyhint 1\n");
  CHECK_EQ(at != std::string::npos && model.substr(0, at) + model.substr(at + 21) == before, true);
  VfxEffect back;
  CHECK_EQ(ReadVfxAsciiModel(model, &back, &error) && back.planes.size() == 1 && back.planes[0].transparency_hint == 1, true);
  if (back.planes.size() == 1) CHECK_EQ(Contains(WriteVfxDraft(back), "alpha = 0.6\ntransparencyhint = 1\nverts ="), true);
  // A plane with the defaults: after its shadow line.
  VfxEffect plain;
  CHECK_EQ(ReadVfxBinaryModel(PlaneModel(true), &plain, &error) && plain.planes.size() == 1, true);
  if (plain.planes.size() != 1) return;
  CHECK_EQ(Contains(WriteVfxModel(plain, "n5t"), "transparencyhint"), false);
  plain.planes[0].transparency_hint = 2;
  CHECK_EQ(Contains(WriteVfxModel(plain, "n5t"), "  bitmap fxpa_fern\n  render 1\n  shadow 0\n  transparencyhint 2\n  verts 4\n"), true);
  plain.planes[0].transparency_hint = 9;  // in memory past the range: as a reader would keep it
  CHECK_EQ(Contains(WriteVfxModel(plain, "n5t"), "  shadow 0\n  transparencyhint 5\n  verts 4\n"), true);
  plain.planes[0].transparency_hint = -1;
  CHECK_EQ(Contains(WriteVfxModel(plain, "n5t"), "transparencyhint"), false);
  // The ASCII reader: a trimesh's, any case, the same rule; none is 0.
  const auto one = [&](const std::string& lines) {
    VfxEffect e;
    std::string why;
    const bool ok = ReadVfxAsciiModel("newmodel t\nbeginmodelgeom t\nnode dummy t\n  parent NULL\nendnode\nnode trimesh m\n"
                                      "  parent t\n  bitmap fxpa_globe\n" + lines +
                                      "  verts 3\n    0 0 0\n    1 0 0\n    0 1 0\n  faces 1\n    0 1 2 1 0 1 2 1\nendnode\n"
                                      "endmodelgeom t\ndonemodel t\n",
                                      &e, &why);
    return ok && e.planes.size() == 1 ? e.planes[0].transparency_hint : -100;
  };
  CHECK_EQ(one(""), 0);
  CHECK_EQ(one("  transparencyhint 1\n"), 1);
  CHECK_EQ(one("  TransparencyHint 2\n"), 2);
  CHECK_EQ(one("  transparencyhint 5\n"), 5);
  CHECK_EQ(one("  transparencyhint 7\n"), 5);
  CHECK_EQ(one("  transparencyhint -3\n"), 0);
  CHECK_EQ(one("  transparencyhint x\n"), 0);
  // A keyed plane (a mesh beat's fade: its node in the impact animation too): the hint on its geometry only, as the game's
  // own have it - none of its 55,223 animation mesh nodes carries one.
  VfxEffect beat = BeatEffect();
  beat.planes[0].transparency_hint = 1;
  const std::string keyed = WriteVfxModel(beat, "n5t");
  const size_t first = keyed.find("transparencyhint"), anim = keyed.find("\nnewanim ");
  CHECK_EQ(first != std::string::npos && anim != std::string::npos && first < anim, true);
  CHECK_EQ(keyed.find("transparencyhint", first + 1) == std::string::npos, true);
  CHECK_EQ(Contains(Section(keyed, "newanim impact n5t", "doneanim impact n5t"), "node trimesh n5t_p0_beat\n  parent n5t_n0_ring\n  alphakey"), true);
}

// A compiled model's: the mesh header's dword at +0x70 (Torlack's TransparencyHint, after render at +0x6C; the game's
// parser keeps transparencyhint beside shadow, beaming and render in the same order) - a plain mesh's, and with any_mesh
// an animated one's too. Not the render beside it, nor the dword after it. Below 0 (its bytes as a signed integer) 0;
// above 5 5.
TEST(VfxReadsTransparencyHintFromACompiledModel) {
  const size_t mh = 0x200 + 0x60;
  const auto read = [&](uint32_t hint, uint32_t flags = 0x21, uint32_t after = 0) {
    std::string b = PlaneModel(true, flags);
    memcpy(&b[mh + 0x70], &hint, 4);
    memcpy(&b[mh + 0x74], &after, 4);
    VfxEffect effect;
    std::string error;
    return ReadVfxBinaryModel(b, &effect, &error, flags != 0x21) && effect.planes.size() == 1 ? effect.planes[0].transparency_hint
                                                                                              : -100;
  };
  CHECK_EQ(read(0), 0);  // render (+0x6C) is 1
  CHECK_EQ(read(1), 1);
  CHECK_EQ(read(2), 2);
  CHECK_EQ(read(5), 5);
  CHECK_EQ(read(6), 5);
  CHECK_EQ(read(0xFFFFFFFFu), 0);  // -1
  CHECK_EQ(read(0x80000000u), 0);
  CHECK_EQ(read(0, 0x21, 3), 0);  // the dword after it isn't it
  CHECK_EQ(read(1, 0xA1), 1);     // an animated mesh (vff_explword's Plane04 has 1)
  // Through the draft and the model.
  std::string b = PlaneModel(true);
  const uint32_t one = 1;
  memcpy(&b[mh + 0x70], &one, 4);
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(b, &effect, &error), true);
  CHECK_EQ(Contains(WriteVfxDraft(effect), "bitmap = fxpa_fern\ntransparencyhint = 1\nverts ="), true);
  CHECK_EQ(Contains(WriteVfxModel(effect, "n5t"), "  shadow 0\n  transparencyhint 1\n  verts 4\n"), true);
}

// The anchor: the game's own see-through effect meshes - vdr_globe01's globe (Cylinder01, fxpa_Globe) has 1,
// vdr_magearmor2's eight shields 2; vps_entangle's vines (#161's planes) none. Only where the game is (NWN_GAME_DIR, or
// Steam's folder); nothing of it is kept.
TEST(VfxGameEffectsTransparencyHint) {
  const char* given = getenv("NWN_GAME_DIR");
  const std::string game = given && *given ? given : "X:\\SteamLibrary\\steamapps\\common\\Neverwinter Nights";
  if (!std::ifstream(game + "\\data\\nwn_base.key")) {
    printf("    (no game at %s: skipped)\n", game.c_str());
    return;
  }
  const std::wstring dir(game.begin(), game.end());
  GameResources resources(dir, GameResources::EeKeyFiles(), {dir + L"\\ovr"});
  const auto effect = [&](const char* model, VfxEffect* out) {
    std::string bytes, source, error;
    return resources.Read(model, kResourceTypeMdl, &bytes, &source) && ReadVfxBinaryModel(bytes, out, &error);
  };
  VfxEffect globe, armor, entangle;
  CHECK_EQ(effect("vdr_globe01", &globe) && effect("vdr_magearmor2", &armor) && effect("vps_entangle", &entangle), true);
  const VfxPlane* cylinder = nullptr;
  for (const VfxPlane& p : globe.planes) {
    if (p.name == "Cylinder01") cylinder = &p;
  }
  CHECK_EQ(cylinder != nullptr, true);
  if (cylinder) {
    printf("    vdr_globe01 Cylinder01: bitmap %s, transparencyhint %d\n", cylinder->bitmap.c_str(), cylinder->transparency_hint);
    CHECK_EQ(cylinder->transparency_hint, 1);
  }
  int shields = 0;
  for (const VfxPlane& p : armor.planes) {
    if (p.name.rfind("shield", 0) == 0 && p.transparency_hint == 2) ++shields;
  }
  CHECK_EQ(shields, 8);
  CHECK_EQ(entangle.planes.size(), 6u);
  for (const VfxPlane& p : entangle.planes) CHECK_EQ(p.transparency_hint, 0);
  CHECK_EQ(Contains(WriteVfxDraft(globe), "\ntransparencyhint = 1\n"), true);
  CHECK_EQ(Contains(WriteVfxModel(globe, "n5t"), "\n  transparencyhint 1\n  verts "), true);
  CHECK_EQ(Contains(WriteVfxDraft(entangle), "transparencyhint") || Contains(WriteVfxModel(entangle, "n5t"), "transparencyhint"),
           false);
}

// --- A mesh beat's keys (#39, M4; the owner, 2026-11-18: "is it possible to add keyframes to animate the shapes? Or make
// them follow emitters, just like other elements"): a plane's own move, turn and scale keys, written beside its alpha in
// each animation - positionkey and orientationkey as a node's, and scalekey (controller 36, one value a row, as the game's
// own vdr_bigby_int keys its hand) - and a mesh beat's tint, the bench's, in the draft only. ---

namespace {

// plane_moves.vfx's effect: plane_beat's beat (BeatEffect, its fade kept) tinted 1 0.5 0.5, dropping in from 3 m over
// 0.5 s while it turns half round (in 1 s) and grows from 0.2 to 1 (in 0.5 s); in the duration a whole turn, in thirds,
// over 3 s.
VfxEffect MovesEffect() {
  VfxEffect effect = BeatEffect();
  effect.name = "n5_moves";
  VfxPlane& beat = effect.planes[0];
  beat.tint[1] = beat.tint[2] = 0.5f;
  VfxKeys& impact = beat.keys[0];
  impact.position = {0, 0, 0, 3, 0.5f, 0, 0, 0};
  impact.orientation = {0, 0, 0, 1, 0, 1, 0, 0, 1, 3.14159f};
  impact.scale = {0, 0.2f, 0.5f, 1};
  VfxKeys turn;
  turn.anim = "duration";
  turn.orientation = {0, 0, 0, 1, 0, 1, 0, 0, 1, 2.0944f, 2, 0, 0, 1, 4.18879f, 3, 0, 0, 1, 6.28319f};
  beat.keys.push_back(turn);
  return effect;
}

// An animation of a model named n5t, from its newanim line to its doneanim ("" when it has none).
std::string Anim(const std::string& model, const char* anim) {
  return Section(model, std::string("newanim ") + anim + " n5t", std::string("doneanim ") + anim + " n5t");
}

// That animation's length (-1 when it has none).
float AnimLength(const std::string& model, const char* anim) {
  const std::string text = Anim(model, anim);
  const size_t at = text.find("  length ");
  return at == std::string::npos ? -1.0f : std::stof(text.substr(at + 9));
}

// The names of the trimesh nodes in a text, in order.
std::vector<std::string> Trimeshes(const std::string& text) {
  std::vector<std::string> names;
  for (size_t at = text.find("node trimesh "); at != std::string::npos; at = text.find("node trimesh ", at + 1)) {
    const size_t from = at + 13;
    names.push_back(text.substr(from, text.find('\n', from) - from));
  }
  return names;
}

}  // namespace

// The model: in each animation with keys of the plane (any of its position, orientation, scale or alpha), a trimesh node
// of its geometry's name under its geometry's parent - its positionkey and orientationkey as a node's, then its scalekey
// (t s a row), then its alphakey as before; none in an animation without them. A plane keyed by one kind alone gets its
// node (one with only position keys too); a list too short for a row is no keys. Its geometry as before; its tint never in
// the model.
TEST(VfxModelPlaneMoveKeys) {
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxDraft(FixtureText("tests/data/fixtures/plane_moves.vfx"), &effect, &error) && effect.planes.size() == 1, true);
  if (effect.planes.size() != 1) return;
  const std::string model = WriteVfxModel(effect, "n5t");
  CHECK_EQ(Section(Anim(model, "impact"), "node trimesh n5t_p0_beat", "endnode") ==
               "node trimesh n5t_p0_beat\n  parent n5t_n0_ring\n"
               "  positionkey 2\n    0 0 0 3\n    0.5 0 0 0\n"
               "  orientationkey 2\n    0 0 0 1 0\n    1 0 0 1 3.14159\n"
               "  scalekey 2\n    0 0.2\n    0.5 1\n"
               "  alphakey 5\n    0 0\n    0.3 0\n    0.5 0.8\n    1.3 0.8\n    1.5 0\n",
           true);
  CHECK_EQ(Section(Anim(model, "duration"), "node trimesh n5t_p0_beat", "endnode") ==
               "node trimesh n5t_p0_beat\n  parent n5t_n0_ring\n"
               "  orientationkey 4\n    0 0 0 1 0\n    1 0 0 1 2.0944\n    2 0 0 1 4.18879\n    3 0 0 1 6.28319\n",
           true);
  const std::string cessation = Anim(model, "cessation");
  CHECK_EQ(!cessation.empty() && Trimeshes(cessation).empty() && !Contains(cessation, "key"), true);
  // Its geometry: the trimesh the plane without keys writes. Its tint, as its mesh and timing, never in the model.
  VfxEffect still = effect;
  still.planes[0].keys.clear();
  const auto geometry = [](const std::string& m) { return Section(m, "beginmodelgeom n5t", "endmodelgeom n5t"); };
  CHECK_EQ(!geometry(model).empty() && geometry(model) == geometry(WriteVfxModel(still, "n5t")), true);
  CHECK_EQ(Contains(model, "tint") || Contains(model, "1 0.5 0.5"), false);
  // Each kind alone (in the cessation, the impact's fade kept): its node, its parent and that one list.
  const char* const alone[] = {"  positionkey 1\n    0.5 0 0 1\n", "  orientationkey 1\n    0.5 0 0 1 1\n",
                               "  scalekey 1\n    0.5 2\n", "  alphakey 1\n    0.5 0\n"};
  for (int kind = 0; kind < 4; ++kind) {
    VfxEffect one = BeatEffect();
    VfxKeys k;
    k.anim = "cessation";
    if (kind == 0) k.position = {0.5f, 0, 0, 1};
    if (kind == 1) k.orientation = {0.5f, 0, 0, 1, 1};
    if (kind == 2) k.scale = {0.5f, 2};
    if (kind == 3) k.alpha = {0.5f, 0};
    one.planes[0].keys.push_back(k);
    CHECK_EQ(Section(Anim(WriteVfxModel(one, "n5t"), "cessation"), "node trimesh n5t_p0_beat", "endnode") ==
                 std::string("node trimesh n5t_p0_beat\n  parent n5t_n0_ring\n") + alone[kind],
             true);
  }
  // Too few numbers for a row of any kind: no keys there, no node.
  VfxEffect few = BeatEffect();
  VfxKeys short_lists;
  short_lists.anim = "cessation";
  short_lists.position = {0.5f, 0, 0};
  short_lists.orientation = {0.5f, 0, 0, 1};
  short_lists.scale = {0.5f};
  short_lists.alpha = {0.5f};
  few.planes[0].keys.push_back(short_lists);
  CHECK_EQ(Trimeshes(Anim(WriteVfxModel(few, "n5t"), "cessation")).empty(), true);
}

// The animations last the plane's keys: the fixture's impact at least as long as its last key (the fade's 1.5 s), its
// duration as long as its turn (3 s; else 2). Each kind alone, its last key at 4 s, makes the impact, the duration and the
// cessation 4 s long.
TEST(VfxModelPlaneMoveKeysLength) {
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxDraft(FixtureText("tests/data/fixtures/plane_moves.vfx"), &effect, &error), true);
  const std::string model = WriteVfxModel(effect, "n5t");
  CHECK_EQ(AnimLength(model, "impact") >= 1.5f, true);
  CHECK_EQ(Milli(AnimLength(model, "duration")), 3000);
  for (int kind = 0; kind < 4; ++kind) {
    VfxEffect one = BeatEffect();
    one.planes[0].keys.clear();
    for (const char* anim : {"impact", "duration", "cessation"}) {
      VfxKeys k;
      k.anim = anim;
      if (kind == 0) k.position = {0, 0, 0, 0, 4, 0, 0, 1};
      if (kind == 1) k.orientation = {0, 0, 0, 1, 0, 4, 0, 0, 1, 1};
      if (kind == 2) k.scale = {0, 1, 4, 2};
      if (kind == 3) k.alpha = {0, 1, 4, 0};
      one.planes[0].keys.push_back(k);
    }
    const std::string m = WriteVfxModel(one, "n5t");
    CHECK_EQ(Milli(AnimLength(m, "impact")) == 4000 && Milli(AnimLength(m, "duration")) == 4000 &&
                 Milli(AnimLength(m, "cessation")) == 4000,
             true);
  }
}

// The draft: a plane's "tint = r g b" after its emit_seconds (before its keys), only when it isn't 1 1 1, and
// "<anim>_scale = t s ..." after its "<anim>_alpha" (in any section, as every key list) - and back as written;
// plane_moves.vfx is MovesEffect's draft. A draft without them writes the bytes it did before.
TEST(VfxDraftScaleAndTint) {
  const std::string text = FixtureText("tests/data/fixtures/plane_moves.vfx");
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxDraft(text, &effect, &error) && effect.planes.size() == 1, true);
  if (effect.planes.size() != 1) return;
  VfxPlane& p = effect.planes[0];
  CHECK_EQ(Milli(p.tint[0]) == 1000 && Milli(p.tint[1]) == 500 && Milli(p.tint[2]) == 500, true);
  const VfxKeys* impact = FindKeys(&p.keys, "impact", false);
  CHECK_EQ(impact != nullptr && impact->scale.size() == 4 && impact->position.size() == 8 &&
               impact->orientation.size() == 10 && impact->alpha.size() == 10,
           true);
  if (impact && impact->scale.size() == 4) {
    CHECK_EQ(Milli(impact->scale[0]) == 0 && Milli(impact->scale[1]) == 200 && Milli(impact->scale[2]) == 500 &&
                 Milli(impact->scale[3]) == 1000,
             true);
  }
  const VfxKeys* duration = FindKeys(&p.keys, "duration", false);
  CHECK_EQ(duration != nullptr && duration->orientation.size() == 20 && duration->scale.empty(), true);
  CHECK_EQ(WriteVfxDraft(effect) == text, true);
  CHECK_EQ(WriteVfxDraft(MovesEffect()) == text, true);  // the fixture is in WriteVfxDraft's order
  CHECK_EQ(Contains(text, "\nemit_seconds = 1.2\ntint = 1 0.5 0.5\nimpact_position = "), true);
  CHECK_EQ(Contains(text, "\nimpact_alpha = 0 0 0.3 0 0.5 0.8 1.3 0.8 1.5 0\nimpact_scale = 0 0.2 0.5 1\nduration_orientation = "),
           true);
  // No tint (1 1 1): no line - only that line less.
  VfxEffect plain = MovesEffect();
  plain.planes[0].tint[1] = plain.planes[0].tint[2] = 1.0f;
  const size_t at = text.find("tint = 1 0.5 0.5\n");
  CHECK_EQ(at != std::string::npos && WriteVfxDraft(plain) == text.substr(0, at) + text.substr(at + 17), true);
  // A plane without a beat's other fields: after its bitmap, before its verts; back the same.
  VfxEffect vines;
  CHECK_EQ(ReadVfxDraft(FixtureText("tests/data/fixtures/plane_effect.vfx"), &vines, &error) && vines.planes.size() == 1, true);
  if (vines.planes.size() == 1) {
    vines.planes[0].tint[0] = 0.2f;
    vines.planes[0].tint[1] = 0.4f;
    vines.planes[0].tint[2] = 0.6f;
    const std::string tinted = WriteVfxDraft(vines);
    CHECK_EQ(Contains(tinted, "\nbitmap = fxpa_fern\ntint = 0.2 0.4 0.6\nverts = "), true);
    VfxEffect back;
    CHECK_EQ(ReadVfxDraft(tinted, &back, &error) && back.planes.size() == 1 && Milli(back.planes[0].tint[0]) == 200 &&
                 Milli(back.planes[0].tint[1]) == 400 && Milli(back.planes[0].tint[2]) == 600 && WriteVfxDraft(back) == tinted,
             true);
  }
  // Scale keys in any section, as every key list: a node's read and written back; only a plane's reach the model.
  VfxEffect spun;
  CHECK_EQ(ReadVfxDraft("name = x\n[node spin]\nimpact_scale = 0 1 1 2\n", &spun, &error) && spun.nodes.size() == 1, true);
  if (spun.nodes.size() == 1) {
    const VfxKeys* k = FindKeys(&spun.nodes[0].keys, "impact", false);
    CHECK_EQ(k != nullptr && k->scale.size() == 4, true);
    CHECK_EQ(Contains(WriteVfxDraft(spun), "\n[node spin]\nposition = 0 0 0\norientation = 0 0 0 0\nimpact_scale = 0 1 1 2\n"), true);
    CHECK_EQ(Contains(WriteVfxModel(spun, "n5t"), "scalekey"), false);
  }
}

// Review focus 2 (M4): nothing written today changes - every fixture draft but plane_moves.vfx (none keys a plane's move,
// turn or scale) writes a model with no scalekey whose animations' trimesh nodes are exactly its planes with alpha keys
// there, each its parent and alphakey only, as before (plane_beat's golden model: VfxModelPlaneAlphaKeys).
TEST(VfxModelUnchangedWithoutPlaneKeys) {
  int read = 0;
  for (const auto& entry : std::filesystem::directory_iterator("tests/data/fixtures")) {
    if (entry.path().extension() != ".vfx" || entry.path().filename() == "plane_moves.vfx") continue;
    const std::string path = entry.path().generic_string();
    VfxEffect effect;
    std::string error;
    const bool ok = ReadVfxDraft(FixtureText(path.c_str()), &effect, &error);
    CHECK_EQ(ok, true);
    if (!ok) continue;
    ++read;
    bool moves = false;
    for (const VfxPlane& p : effect.planes) {
      for (const VfxKeys& k : p.keys) moves = moves || !k.position.empty() || !k.orientation.empty() || !k.scale.empty();
    }
    CHECK_EQ(moves, false);
    const std::string model = WriteVfxModel(effect, "n5t");
    CHECK_EQ(Contains(model, "scalekey"), false);
    const std::vector<std::string> planes = Trimeshes(Section(model, "beginmodelgeom n5t", "endmodelgeom n5t"));
    CHECK_EQ(planes.size(), effect.planes.size());
    if (planes.size() != effect.planes.size()) continue;
    for (const char* anim : {"impact", "duration", "cessation"}) {
      const std::string text = Anim(model, anim);
      std::vector<std::string> faded;
      for (size_t i = 0; i < effect.planes.size() && !text.empty(); ++i) {
        const VfxKeys* k = FindKeys(&effect.planes[i].keys, anim, false);
        if (k && k->alpha.size() >= 2) faded.push_back(planes[i]);
      }
      CHECK_EQ(Trimeshes(text) == faded, true);
      for (const std::string& name : faded) {
        const std::string node = Section(text, "node trimesh " + name + "\n", "endnode");
        CHECK_EQ(Contains(node, "\n  parent ") && Contains(node, "\n  alphakey ") && !Contains(node, "positionkey") &&
                     !Contains(node, "orientationkey"),
                 true);
      }
    }
  }
  CHECK_EQ(read >= 3, true);
}

namespace {

template <typename T>
T BytesAt(const std::string& b, size_t at) {
  T v{};
  if (at + sizeof(T) <= b.size()) memcpy(&v, b.data() + at, sizeof(T));
  return v;
}

// A compiled model's node name (lower case).
std::string NameAt(const std::string& b, size_t at) {
  std::string name;
  for (size_t i = 0; i < 32 && at + i < b.size() && b[at + i]; ++i) {
    name += static_cast<char>(tolower(static_cast<unsigned char>(b[at + i])));
  }
  return name;
}

// A compiled model's controller `type` on the node `node` (lower case) in each of its animations: by the animation's
// name, rows of t and its values (*columns of them: the controller's columns, 0 when nothing keys it). Laid out as
// ReadVfxBinaryModel reads it, offsets from byte 12: the model's animations at +0x78 (header offsets, count +0x7C); an
// animation's name at +8, its root node at +0x48; a node (its header + 0x10): name +0x10, children +0x38 (count +0x3C),
// controller keys +0x44 (count +0x48), values +0x50; a key 12 bytes - type, rows (16 bits, +4), its first time (+6) and
// value (+8) as indices into the values, columns (+10, the low 4 bits).
std::map<std::string, std::vector<float>> CompiledKeys(const std::string& b, const std::string& node, uint32_t type,
                                                       int* columns) {
  constexpr size_t kBase = 12;
  std::map<std::string, std::vector<float>> out;
  *columns = 0;
  const uint32_t anims = BytesAt<uint32_t>(b, kBase + 0x78), anim_count = BytesAt<uint32_t>(b, kBase + 0x7C);
  for (uint32_t a = 0; a < anim_count && a < 256; ++a) {
    const size_t header = kBase + BytesAt<uint32_t>(b, kBase + anims + 4ull * a);
    const std::string anim = NameAt(b, header + 8);
    std::vector<size_t> stack = {kBase + BytesAt<uint32_t>(b, header + 0x48) + 0x10};
    std::set<size_t> seen;
    while (!stack.empty() && seen.size() < 4096) {
      const size_t at = stack.back();
      stack.pop_back();
      if (at + 0x60 > b.size() || !seen.insert(at).second) continue;
      const uint32_t children = BytesAt<uint32_t>(b, at + 0x38), count = BytesAt<uint32_t>(b, at + 0x3C);
      for (uint32_t c = 0; c < count && c < 1024; ++c) {
        stack.push_back(kBase + BytesAt<uint32_t>(b, kBase + children + 4ull * c) + 0x10);
      }
      if (NameAt(b, at + 0x10) != node) continue;
      const uint32_t keys = BytesAt<uint32_t>(b, at + 0x44), key_count = BytesAt<uint32_t>(b, at + 0x48);
      const uint32_t values = BytesAt<uint32_t>(b, at + 0x50);
      for (uint32_t k = 0; k < key_count && k < 96; ++k) {
        const size_t key = kBase + keys + 12ull * k;
        if (BytesAt<uint32_t>(b, key) != type) continue;
        const int rows = BytesAt<uint16_t>(b, key + 4), time = BytesAt<uint16_t>(b, key + 6), value = BytesAt<uint16_t>(b, key + 8);
        const int cols = BytesAt<uint8_t>(b, key + 10) & 0x0F;
        *columns = cols;
        std::vector<float>& list = out[anim];
        for (int r = 0; r < rows; ++r) {
          list.push_back(BytesAt<float>(b, kBase + values + 4ull * (time + r)));
          for (int c = 0; c < cols; ++c) list.push_back(BytesAt<float>(b, kBase + values + 4ull * (value + r * cols + c)));
        }
      }
    }
  }
  return out;
}

}  // namespace

// The anchor (the design's "Keys on a mesh beat": a scale as the game's own effects key it): vdr_bigby_int's hand - a plain
// mesh, read as a plane - grows in its impact and shrinks back in its cessation by its scale, controller 36, one value a
// row (1 to 10 over 0.333 s; 10 to 1 over 0.3 s). Those rows as the plane's scale keys are the model's scalekey on its
// node in each animation, t s a row. KF: the plane as read keeps them itself (its own keys - before KF the readers kept
// none of a plane's and it wrote no node there), so the model as read writes the same scalekey lines. Only where the game
// is (NWN_GAME_DIR, or Steam's folder); nothing of it is kept.
TEST(VfxGameEffectsScaleKeys) {
  const char* given = getenv("NWN_GAME_DIR");
  const std::string game = given && *given ? given : "X:\\SteamLibrary\\steamapps\\common\\Neverwinter Nights";
  if (!std::ifstream(game + "\\data\\nwn_base.key")) {
    printf("    (no game at %s: skipped)\n", game.c_str());
    return;
  }
  const std::wstring dir(game.begin(), game.end());
  GameResources resources(dir, GameResources::EeKeyFiles(), {dir + L"\\ovr"});
  std::string bytes, source, error;
  CHECK_EQ(resources.Read("vdr_bigby_int", kResourceTypeMdl, &bytes, &source), true);
  int columns = 0;
  const std::map<std::string, std::vector<float>> scale = CompiledKeys(bytes, "hand", 36, &columns);
  const auto rows = [&](const char* anim) {
    const auto it = scale.find(anim);
    return it == scale.end() ? std::vector<float>() : it->second;
  };
  const std::vector<float> grow = rows("impact"), shrink = rows("cessation");
  printf("    vdr_bigby_int hand, controller 36 (%d column): impact", columns);
  for (float v : grow) printf(" %g", v);
  printf(", cessation");
  for (float v : shrink) printf(" %g", v);
  printf("\n");
  CHECK_EQ(columns, 1);
  CHECK_EQ(grow.size() == 4 && Milli(grow[0]) == 0 && Milli(grow[1]) == 1000 && Milli(grow[2]) == 333 && Milli(grow[3]) == 10000,
           true);
  CHECK_EQ(shrink.size() == 4 && Milli(shrink[0]) == 0 && Milli(shrink[1]) == 10000 && Milli(shrink[2]) == 300 &&
               Milli(shrink[3]) == 1000,
           true);
  VfxEffect bigby;
  CHECK_EQ(ReadVfxBinaryModel(bytes, &bigby, &error) && bigby.duration, true);
  int hand = -1;
  for (size_t i = 0; i < bigby.planes.size(); ++i) {
    if (NameAt(bigby.planes[i].name, 0) == "hand") hand = static_cast<int>(i);
  }
  CHECK_EQ(hand >= 0, true);
  if (hand < 0 || grow.size() != 4 || shrink.size() != 4) return;
  const std::string node = "node trimesh n5t_p" + std::to_string(hand) + "_hand\n";
  // KF: as read, its own scale keys are the compiled rows.
  const VfxKeys* grown = FindKeys(&bigby.planes[hand].keys, "impact", false);
  const VfxKeys* shrunk = FindKeys(&bigby.planes[hand].keys, "cessation", false);
  CHECK_EQ(grown && grown->scale == grow && shrunk && shrunk->scale == shrink, true);
  const std::string as_read = WriteVfxModel(bigby, "n5t");
  VfxKeys in, out;
  in.anim = "impact";
  in.scale = grow;
  out.anim = "cessation";
  out.scale = shrink;
  bigby.planes[hand].keys = {in, out};
  const std::string model = WriteVfxModel(bigby, "n5t");
  const auto row = [](float t, float s) {
    char text[64];
    snprintf(text, sizeof(text), "    %g %g\n", t, s);
    return std::string(text);
  };
  for (const std::string* m : {&model, &as_read}) {
    CHECK_EQ(Contains(Section(Anim(*m, "impact"), node, "endnode"), "\n  scalekey 2\n" + row(grow[0], grow[1]) + row(grow[2], grow[3])),
             true);
    CHECK_EQ(Contains(Section(Anim(*m, "cessation"), node, "endnode"),
                      "\n  scalekey 2\n" + row(shrink[0], shrink[1]) + row(shrink[2], shrink[3])),
             true);
  }
}

// --- TT (#39; the owner, 2026-11-18, MBK-1 on 0.99.79, after Drop in's first key moved to 0: "the disk in-game still
// starts at ground level, rises to 3m and then drops down again"): the game blends an animation in over its transtime
// from the pose before it - an effect's impact from the rest pose, its geometry's. The impact is written with transtime 0
// when an item's first key there is off its rest, so it starts on its keys (the game's own fx_meteor and fx_rainice:
// transtime 0); else 0.25, as always. The duration and the cessation keep 0.25. ---

namespace {

// An animation's transtime, as written, in a model named n5t ("" when it has none).
std::string Transtime(const std::string& model, const char* anim) {
  const std::string text = Anim(model, anim);
  const size_t at = text.find("\n  transtime ");
  if (at == std::string::npos) return "";
  const size_t from = at + 13;
  return text.substr(from, text.find('\n', from) - from);
}

// A lasting effect's disc: a plane under the root with its rest at height `rest`, dropping in its impact from `from` m to
// the ground over 0.5 s.
VfxEffect DiscEffect(float rest, float from) {
  VfxEffect effect;
  effect.name = "n5_disc";
  VfxPlane disc;
  disc.name = "disc";
  disc.position[2] = rest;
  disc.bitmap = "fxpa_fern";
  disc.verts = {-1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0};
  disc.tverts = {0, 0, 1, 0, 0, 1, 1, 1};
  disc.faces = {0, 1, 2, 2, 1, 3};
  VfxKeys drop;
  drop.anim = "impact";
  drop.position = {0, 0, 0, from, 0.5f, 0, 0, 0};
  disc.keys.push_back(drop);
  effect.planes.push_back(disc);
  return effect;
}

// The disc at rest on the ground with one list of keys in an animation instead of its drop.
VfxEffect DiscKeyed(const char* anim, const VfxKeys& keys) {
  VfxEffect effect = DiscEffect(0.0f, 0.0f);
  effect.planes[0].keys = {keys};
  effect.planes[0].keys[0].anim = anim;
  return effect;
}

}  // namespace

// The owner's Drop in: the disc at rest on the ground, its impact's first key 3 m up - the impact is transtime 0, so the
// disc starts at 3 m and falls; the duration and the cessation 0.25. Only that line is new: one transtime 0, two 0.25.
TEST(VfxImpactStartsOnAnOffRestFirstKey) {
  const std::string model = WriteVfxModel(DiscEffect(0.0f, 3.0f), "n5t");
  CHECK_EQ(Transtime(model, "impact") == "0", true);
  CHECK_EQ(Transtime(model, "duration") == "0.25" && Transtime(model, "cessation") == "0.25", true);
  size_t snaps = 0, blends = 0;
  for (size_t at = model.find("\n  transtime "); at != std::string::npos; at = model.find("\n  transtime ", at + 1)) {
    snaps += model.compare(at, 15, "\n  transtime 0\n") == 0;
    blends += model.compare(at, 18, "\n  transtime 0.25\n") == 0;
  }
  CHECK_EQ(snaps, 1u);
  CHECK_EQ(blends, 2u);
  // Its keys and its geometry as before.
  CHECK_EQ(Contains(Anim(model, "impact"), "node trimesh n5t_p0_disc\n  parent n5t\n  positionkey 2\n    0 0 0 3\n    0.5 0 0 0\nendnode"),
           true);
  CHECK_EQ(Contains(model, "node trimesh n5t_p0_disc\n  parent n5t\n  position 0 0 0\n"), true);
  // A one-shot effect (no duration, no cessation) the same.
  VfxEffect once = DiscEffect(0.0f, 3.0f);
  once.duration = false;
  const std::string one = WriteVfxModel(once, "n5t");
  CHECK_EQ(Transtime(one, "impact") == "0" && Transtime(one, "duration").empty() && Transtime(one, "cessation").empty(), true);
}

// The same drop with its rest at its first key (3 m) blends as before (0.25); within 1e-4 m of it too, past that not. A
// list too short for a row is no key (none is written). Without keys, 0.25.
TEST(VfxImpactBlendsFromAFirstKeyAtRest) {
  CHECK_EQ(Transtime(WriteVfxModel(DiscEffect(3.0f, 3.0f), "n5t"), "impact") == "0.25", true);
  CHECK_EQ(Transtime(WriteVfxModel(DiscEffect(3.00005f, 3.0f), "n5t"), "impact") == "0.25", true);
  CHECK_EQ(Transtime(WriteVfxModel(DiscEffect(3.0003f, 3.0f), "n5t"), "impact") == "0", true);
  CHECK_EQ(Transtime(WriteVfxModel(DiscEffect(0.0f, 3.0f), "n5t"), "impact") == "0", true);
  CHECK_EQ(Transtime(WriteVfxModel(DiscKeyed("impact", VfxKeys()), "n5t"), "impact") == "0.25", true);
  // Off in x or y as well as z.
  VfxEffect aside = DiscEffect(3.0f, 3.0f);
  aside.planes[0].keys[0].position[1] = 0.5f;
  CHECK_EQ(Transtime(WriteVfxModel(aside, "n5t"), "impact") == "0", true);
  VfxKeys partial;
  partial.position = {0, 0, 0};
  CHECK_EQ(Transtime(WriteVfxModel(DiscKeyed("impact", partial), "n5t"), "impact") == "0.25", true);
}

// A fade from nothing (alpha keyed from 0, its material's alpha 1) and a growth from 0.2 start on their keys (0); a fade
// out from its own alpha, or a scale from 1 (the mesh as placed), blend (0.25). plane_beat.vfx's beat fades in from 0.
TEST(VfxImpactStartsOnAFadeFromNothing) {
  VfxKeys in;
  in.alpha = {0, 0, 0.5f, 1};
  CHECK_EQ(Transtime(WriteVfxModel(DiscKeyed("impact", in), "n5t"), "impact") == "0", true);
  VfxKeys out;
  out.alpha = {0, 1, 0.5f, 0};
  CHECK_EQ(Transtime(WriteVfxModel(DiscKeyed("impact", out), "n5t"), "impact") == "0.25", true);
  // Against its own alpha: 0.5 from 0.5 blends, from 1 doesn't.
  VfxEffect half = DiscKeyed("impact", out);
  half.planes[0].alpha = 0.5f;
  CHECK_EQ(Transtime(WriteVfxModel(half, "n5t"), "impact") == "0", true);
  half.planes[0].keys[0].alpha = {0, 0.5f, 0.5f, 0};
  CHECK_EQ(Transtime(WriteVfxModel(half, "n5t"), "impact") == "0.25", true);
  VfxKeys grow;
  grow.scale = {0, 0.2f, 0.5f, 1};
  CHECK_EQ(Transtime(WriteVfxModel(DiscKeyed("impact", grow), "n5t"), "impact") == "0", true);
  grow.scale = {0, 1, 0.5f, 2};
  CHECK_EQ(Transtime(WriteVfxModel(DiscKeyed("impact", grow), "n5t"), "impact") == "0.25", true);
  VfxEffect beat;
  std::string error;
  CHECK_EQ(ReadVfxDraft(FixtureText("tests/data/fixtures/plane_beat.vfx"), &beat, &error), true);
  CHECK_EQ(Transtime(WriteVfxModel(beat, "n5t"), "impact") == "0", true);
}

// A turn keyed from its rest orientation blends (0.25) - compared as turns, not as the four numbers: the axis and the
// angle both negated, an axis not of length 1, a whole turn more, or no turn about another axis (or about no axis) are
// the same turn. Off its rest (plane_moves.vfx's beat: rest a quarter turn about z, first key none) it starts on its keys
// (0); past 1e-4 radians.
TEST(VfxImpactTurnComparedAsATurn) {
  const auto turned = [](float rest_angle, std::vector<float> first) {
    VfxKeys spin;
    spin.orientation = first;
    spin.orientation.insert(spin.orientation.begin(), 0.0f);
    spin.orientation.insert(spin.orientation.end(), {1, 0, 0, 1, 3.14159f});
    VfxEffect effect = DiscKeyed("impact", spin);
    effect.planes[0].orientation[2] = 1.0f;
    effect.planes[0].orientation[3] = rest_angle;
    return Transtime(WriteVfxModel(effect, "n5t"), "impact");
  };
  CHECK_EQ(turned(1.5708f, {0, 0, 1, 1.5708f}) == "0.25", true);
  CHECK_EQ(turned(1.5708f, {0, 0, -1, -1.5708f}) == "0.25", true);
  CHECK_EQ(turned(1.5708f, {0, 0, 2, 1.5708f}) == "0.25", true);
  CHECK_EQ(turned(1.5708f, {0, 0, 1, 1.5708f + 6.2831853f}) == "0.25", true);
  CHECK_EQ(turned(1.5708f, {0, 0, 1, 1.57085f}) == "0.25", true);
  CHECK_EQ(turned(1.5708f, {0, 0, 1, 1.5711f}) == "0", true);
  CHECK_EQ(turned(1.5708f, {0, 0, 1, 0}) == "0", true);
  CHECK_EQ(turned(1.5708f, {1, 0, 0, 1.5708f}) == "0", true);
  CHECK_EQ(turned(0.0f, {1, 0, 0, 0}) == "0.25", true);
  CHECK_EQ(turned(0.0f, {0, 0, 0, 0}) == "0.25", true);
  CHECK_EQ(turned(0.0f, {0, 0, 0, 1.5f}) == "0.25", true);  // no axis: no turn, whatever its angle
  CHECK_EQ(turned(0.0f, {0, 1, 0, 0.5f}) == "0", true);
  VfxEffect moves;
  std::string error;
  CHECK_EQ(ReadVfxDraft(FixtureText("tests/data/fixtures/plane_moves.vfx"), &moves, &error) && moves.planes.size() == 1, true);
  VfxKeys* impact = moves.planes.size() == 1 ? FindKeys(&moves.planes[0].keys, "impact", false) : nullptr;
  CHECK_EQ(impact != nullptr && impact->orientation.size() == 10, true);
  if (!impact) return;
  impact->position.clear();  // only its turn (its fade and growth, off their rest too, gone)
  impact->alpha.clear();
  impact->scale.clear();
  CHECK_EQ(Transtime(WriteVfxModel(moves, "n5t"), "impact") == "0", true);
}

// Every kind of item: a node's, an emitter's or a light's move or turn, a light's colour or radius - each off its rest
// starts the impact on its keys (0), each at its rest blends (0.25). A light without keys of its own (a one-shot's goes
// out from its radius) blends.
TEST(VfxImpactStartsOnAnyItemsFirstKey) {
  const auto impact = [](const VfxEffect& effect) { return Transtime(WriteVfxModel(effect, "n5t"), "impact"); };
  VfxEffect base;
  base.name = "n5_items";
  VfxNode node;
  node.name = "hub";
  node.position[2] = 1.0f;
  node.orientation[2] = 1.0f;
  node.orientation[3] = 0.5f;
  base.nodes.push_back(node);
  VfxEmitter emitter;
  emitter.name = "spark";
  emitter.parent = "hub";
  emitter.position[0] = 0.25f;
  emitter.orientation[0] = 1.0f;
  emitter.orientation[3] = 1.0f;
  base.emitters.push_back(emitter);
  VfxLight light;
  light.parent = "hub";
  light.position[1] = 0.5f;
  light.color[1] = 0.5f;
  light.radius = 4.0f;
  base.lights.push_back(light);
  CHECK_EQ(impact(base) == "0.25", true);
  VfxEffect once = base;
  once.duration = false;
  CHECK_EQ(Contains(Anim(WriteVfxModel(once, "n5t"), "impact"), "  radiuskey 2\n"), true);
  CHECK_EQ(impact(once) == "0.25", true);
  // Each kind at its rest, then off it.
  const auto keyed = [&](int item, const char* what, std::vector<float> at_rest, std::vector<float> off_rest) {
    for (int off = 0; off < 2; ++off) {
      VfxEffect e = base;
      std::vector<VfxKeys>* list[] = {&e.nodes[0].keys, &e.emitters[0].keys, &e.lights[0].keys};
      VfxKeys k;
      k.anim = "impact";
      const std::string kind(what);
      std::vector<float>& into = kind == "position" ? k.position : kind == "orientation" ? k.orientation : kind == "color" ? k.color : k.radius;
      into = off ? off_rest : at_rest;
      list[item]->push_back(k);
      const std::string got = impact(e);
      if (got != (off ? "0" : "0.25")) printf("    item %d %s %s: transtime %s\n", item, what, off ? "off rest" : "at rest", got.c_str());
      CHECK_EQ(got == (off ? "0" : "0.25"), true);
    }
  };
  keyed(0, "position", {0, 0, 0, 1, 1, 0, 0, 2}, {0, 0, 0, 2, 1, 0, 0, 1});
  keyed(0, "orientation", {0, 0, 0, 1, 0.5f, 1, 0, 0, 1, 1}, {0, 0, 0, 1, 0, 1, 0, 0, 1, 0.5f});
  keyed(1, "position", {0, 0.25f, 0, 0, 1, 0, 0, 0}, {0, 0, 0, 0, 1, 0.25f, 0, 0});
  keyed(1, "orientation", {0, 1, 0, 0, 1, 1, 0, 0, 1, 0}, {0, 0, 0, 1, 0, 1, 1, 0, 0, 1});
  keyed(2, "position", {0, 0, 0.5f, 0, 1, 0, 0, 0}, {0, 0, 0, 0, 1, 0, 0.5f, 0});
  keyed(2, "orientation", {0, 0, 0, 1, 0, 1, 0, 0, 1, 1}, {0, 0, 0, 1, 1, 1, 0, 0, 1, 0});
  keyed(2, "color", {0, 1, 0.5f, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 1, 1, 0.5f, 1});
  keyed(2, "radius", {0, 4, 1, 0}, {0, 0, 1, 4});
}

// The duration and the cessation keep 0.25 whatever their first keys - each blends from the animation before it, not the
// rest - and their keys off rest don't change the impact's: a disc dropping in all three, its impact alone 0; a node, an
// emitter and a light keyed off rest only there, the impact 0.25.
TEST(VfxDurationAndCessationAlwaysBlend) {
  VfxEffect effect = DiscEffect(0.0f, 3.0f);
  for (const char* anim : {"duration", "cessation"}) {
    VfxKeys drop;
    drop.anim = anim;
    drop.position = {0, 0, 0, 3, 0.5f, 0, 0, 0};
    drop.alpha = {0, 0, 0.5f, 1};
    effect.planes[0].keys.push_back(drop);
  }
  const std::string model = WriteVfxModel(effect, "n5t");
  CHECK_EQ(Transtime(model, "impact") == "0", true);
  CHECK_EQ(Transtime(model, "duration") == "0.25" && Transtime(model, "cessation") == "0.25", true);
  effect.planes[0].keys.erase(effect.planes[0].keys.begin());  // the impact's drop gone: all three 0.25
  const std::string later = WriteVfxModel(effect, "n5t");
  CHECK_EQ(Transtime(later, "impact") == "0.25" && Transtime(later, "duration") == "0.25" && Transtime(later, "cessation") == "0.25",
           true);
  // A node's, an emitter's and a light's keys off their rest in the duration and the cessation alone: all three 0.25.
  VfxEffect items;
  VfxNode hub;
  hub.name = "hub";
  items.nodes.push_back(hub);
  VfxEmitter spark;
  spark.name = "spark";
  items.emitters.push_back(spark);
  items.lights.emplace_back();
  for (const char* anim : {"duration", "cessation"}) {
    VfxKeys k;
    k.anim = anim;
    k.position = {0, 0, 0, 2, 1, 0, 0, 0};
    k.orientation = {0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
    items.nodes[0].keys.push_back(k);
    items.emitters[0].keys.push_back(k);
    k.color = {0, 0, 0, 0, 1, 1, 1, 1};
    k.radius = {0, 0, 1, 5};
    items.lights[0].keys.push_back(k);
  }
  const std::string keyed = WriteVfxModel(items, "n5t");
  CHECK_EQ(Transtime(keyed, "impact") == "0.25" && Transtime(keyed, "duration") == "0.25" && Transtime(keyed, "cessation") == "0.25",
           true);
}

// The kept drafts (the fixtures and Detect Magic's four): the impact is 0 only for the two whose plane starts off its rest
// - plane_beat's fade from 0 and plane_moves' (that fade, its drop from 3 m, its turn from none and its growth from 0.2) -
// the rest 0.25 as before; every duration and cessation 0.25.
TEST(VfxKeptDraftsBlendUnlessOffRest) {
  std::vector<std::string> paths;
  for (const auto& entry : std::filesystem::directory_iterator("tests/data/fixtures")) {
    if (entry.path().extension() == ".vfx") paths.push_back(entry.path().generic_string());
  }
  for (const char* size : {"h", "l", "m", "s"}) paths.push_back(std::string("tools/data/vfx/n5_detmag_") + size + ".vfx");
  CHECK_EQ(paths.size() >= 8, true);
  for (const std::string& path : paths) {
    VfxEffect effect;
    std::string error;
    CHECK_EQ(ReadVfxDraft(FixtureText(path.c_str()), &effect, &error), true);
    const std::string model = WriteVfxModel(effect, "n5t");
    const bool off = Contains(path, "/plane_beat.vfx") || Contains(path, "/plane_moves.vfx");
    if (Transtime(model, "impact") != (off ? "0" : "0.25")) printf("    %s: impact transtime %s\n", path.c_str(), Transtime(model, "impact").c_str());
    CHECK_EQ(Transtime(model, "impact") == (off ? "0" : "0.25"), true);
    if (effect.duration) CHECK_EQ(Transtime(model, "duration") == "0.25" && Transtime(model, "cessation") == "0.25", true);
  }
}

// --- KF (#39; the owner, 2026-11-18, TMO-4: "in the bench the blue flame in the middle doesn't stop and pressing Play in
// game doesn't show the angled flame ring at the bottom in-game" - offered "keep a game effect's own keys" - "yeah"; TT's
// review: a re-saved game effect wrote transtime 0 where the game's own is 0.25): the model readers keep a game effect's
// own keys - a mesh's move, turn, scale and alpha, and an emitter's birthrate, alphaStart and alphaEnd - and each
// animation's transtime; the draft and the writer carry them. ---

namespace {

// A controller of a compiled model built here: its type, columns, rows' times and values.
struct KfCtrl {
  uint32_t type;
  int columns;
  std::vector<float> times, values;
};

// A compiled model laid out as the game's (built here, nothing of the game's): nodes with controllers, children, an
// emitter's header, a plain mesh (a quad), animations with their length and transtime. Offsets from byte 12, as
// ReadVfxBinaryModel reads them; the space handed out as it's asked for.
struct KfModel {
  static constexpr size_t kBase = 12, kRaw = 0x7000;
  std::string b = std::string(0x8000, '\0');
  size_t free_at = 0x100;
  KfModel(const char* name) {
    Puts(kBase + 8, name);
    Put32(4, static_cast<uint32_t>(kRaw - kBase));
  }
  size_t Alloc(size_t n) {
    const size_t at = free_at;
    free_at += (n + 15) & ~static_cast<size_t>(15);
    return at;
  }
  void Put32(size_t at, uint32_t v) { memcpy(&b[at], &v, 4); }
  void Put16(size_t at, uint16_t v) { memcpy(&b[at], &v, 2); }
  void PutF(size_t at, float v) { memcpy(&b[at], &v, 4); }
  void Puts(size_t at, const char* text) { memcpy(&b[at], text, strlen(text)); }
  // A node (the reader's terms: its header + 0x10): its name, flags and controllers.
  size_t Node(const char* name, uint32_t flags, const std::vector<KfCtrl>& ctrls) {
    const size_t at = Alloc(0x300);
    Puts(at + 0x10, name);
    Put32(at + 0x5C, flags);
    size_t numbers = 0;
    for (const KfCtrl& c : ctrls) numbers += c.times.size() + c.values.size();
    const size_t keys = Alloc(12 * ctrls.size() + 4), data = Alloc(4 * numbers + 4);
    Put32(at + 0x44, static_cast<uint32_t>(keys - kBase));
    Put32(at + 0x48, static_cast<uint32_t>(ctrls.size()));
    Put32(at + 0x50, static_cast<uint32_t>(data - kBase));
    uint16_t used = 0;
    for (size_t i = 0; i < ctrls.size(); ++i) {
      const size_t k = keys + 12 * i;
      Put32(k, ctrls[i].type);
      Put16(k + 4, static_cast<uint16_t>(ctrls[i].times.size()));
      Put16(k + 6, used);
      for (float t : ctrls[i].times) PutF(data + 4 * used++, t);
      Put16(k + 8, used);
      for (float v : ctrls[i].values) PutF(data + 4 * used++, v);
      b[k + 10] = static_cast<char>(ctrls[i].columns);
    }
    Put32(at + 0x54, used);
    return at;
  }
  void Children(size_t at, const std::vector<size_t>& kids) {
    const size_t array = Alloc(4 * kids.size() + 4);
    Put32(at + 0x38, static_cast<uint32_t>(array - kBase));
    Put32(at + 0x3C, static_cast<uint32_t>(kids.size()));
    for (size_t i = 0; i < kids.size(); ++i) Put32(array + 4 * i, static_cast<uint32_t>(kids[i] - 0x10 - kBase));
  }
  void Emitter(size_t at) {
    Puts(at + 0x60 + 0x18, "Fountain");
    Puts(at + 0x60 + 0x38, "Normal");
    Puts(at + 0x60 + 0x78, "fxpa_fireb");
  }
  // A quad of 4 vertices (2 faces) with its UVs, textured fxpa_flame02 (the mesh header at the node + 0x60).
  void Mesh(size_t at) {
    const size_t mh = at + 0x60, faces = Alloc(64);
    for (int k = 0; k < 3; ++k) {
      PutF(mh + 0x3C + 4 * k, 1.0f);
      PutF(mh + 0x48 + 4 * k, 1.0f);
    }
    Put32(mh + 0x6C, 1);
    Puts(mh + 0x78, "fxpa_flame02");
    Put32(mh + 0x08, static_cast<uint32_t>(faces - kBase));
    Put32(mh + 0x0C, 2);
    const uint16_t corners[2][3] = {{0, 1, 2}, {2, 1, 3}};
    for (int f = 0; f < 2; ++f)
      for (int k = 0; k < 3; ++k) Put16(faces + 32 * f + 26 + 2 * k, corners[f][k]);
    Put32(mh + 0x1BC, 0);
    Put16(mh + 0x1C0, 4);
    Put32(mh + 0x1C4, 48);
    const float verts[4][3] = {{-1, 0, 0}, {1, 0, 0}, {-1, 0, 0.5f}, {1, 0, 0.5f}};
    const float uvs[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    for (int v = 0; v < 4; ++v) {
      for (int c = 0; c < 3; ++c) PutF(kRaw + 12 * v + 4 * c, verts[v][c]);
      for (int c = 0; c < 2; ++c) PutF(kRaw + 48 + 8 * v + 4 * c, uvs[v][c]);
    }
  }
  // An animation: its name, length and transtime (the header's +0x70 and +0x74), its node tree from `root`.
  // An animation: its name, length and transtime (the header's +0x70 and +0x74), its node tree from `root`, its type (5:
  // an animation, at +0x6C, as every one of the game's) - listed in the model header's animations (List) unless `listed` is
  // false.
  std::vector<size_t> anims;
  void Anim(const char* name, float length, float transtime, size_t root, bool listed = true) {
    const size_t header = Alloc(0x100);
    Puts(header + 8, name);
    Put32(header + 0x48, static_cast<uint32_t>(root - 0x10 - kBase));
    b[header + 0x6C] = 5;
    PutF(header + 0x70, length);
    PutF(header + 0x74, transtime);
    if (listed) anims.push_back(header);
  }
  // The model header's animations (+0x78: an array of header offsets, +0x7C its count), as the game's models have them.
  void List() {
    const size_t array = Alloc(4 * anims.size() + 4);
    Put32(kBase + 0x78, static_cast<uint32_t>(array - kBase));
    Put32(kBase + 0x7C, static_cast<uint32_t>(anims.size()));
    for (size_t i = 0; i < anims.size(); ++i) Put32(array + 4 * i, static_cast<uint32_t>(anims[i] - kBase));
  }
};

// vdr_eleshield's shape (the owner's TMO-4), built here: a lasting effect whose emitter "flame" (birthrate 100,
// alphaStart 0.6, alphaEnd 0 in the geometry) is on in the impact only until 0.6 s (birthrate keys 100, 100, 0) and fades
// (alphaStart 0.6 to 0), its alphaStart held at 0 in the duration and its alphaEnd keyed in the cessation; and a plain
// mesh "ring" (alpha 0 in the geometry, 1 cm along x) fading in over the impact's first 0.3 s, held in the duration (where
// it also breathes: scale 1, 1.5, 1) and fading out in the cessation as it rises a metre and turns. The animations'
// transtimes 0.25 (impact), 0 (duration) and 0.15 (cessation). `transtimes`: false - none of them past the game's
// (-1, and 99 for the cessation: not read).
std::string KeyedModel(bool transtimes = true) {
  KfModel m("test_kf");
  const size_t root = m.Node("test_kf", 0x1, {{8, 3, {0}, {0, 0, 0}}});
  const size_t flame = m.Node("flame", 0x5, {{8, 3, {0}, {0, 0, 0.43f}}, {88, 1, {0}, {100}}, {84, 1, {0}, {0.6f}}, {80, 1, {0}, {0}}});
  m.Emitter(flame);
  const size_t ring = m.Node("ring", 0x21, {{8, 3, {0}, {0.01f, 0, 0}}, {128, 1, {0}, {0}}});
  m.Mesh(ring);
  m.Children(root, {flame, ring});
  m.Put32(KfModel::kBase + 0x48, static_cast<uint32_t>(root - 0x10 - KfModel::kBase));
  const float t_impact = transtimes ? 0.25f : -1.0f, t_duration = transtimes ? 0.0f : -1.0f, t_cessation = transtimes ? 0.15f : 99.0f;
  {
    const size_t r = m.Node("test_kf", 0x1, {});
    const size_t f = m.Node("flame", 0x5, {{88, 1, {0, 0.5f, 0.6f}, {100, 100, 0}}, {84, 1, {0.5f, 0.95f}, {0.6f, 0}}});
    const size_t g = m.Node("ring", 0x21, {{128, 1, {0, 0.3f, 0.95f}, {0, 1, 1}}, {8, 3, {0}, {0.01f, 0, 0}}});
    m.Children(r, {f, g});
    m.Anim("impact", 0.95f, t_impact, r);
  }
  {
    const size_t r = m.Node("test_kf", 0x1, {});
    const size_t f = m.Node("flame", 0x5, {{84, 1, {0}, {0}}});
    const size_t g = m.Node("ring", 0x21, {{128, 1, {0, 1}, {1, 1}}, {36, 1, {0, 0.5f, 1}, {1, 1.5f, 1}}});
    m.Children(r, {f, g});
    m.Anim("duration", 1.0f, t_duration, r);
  }
  {
    const size_t r = m.Node("test_kf", 0x1, {});
    const size_t f = m.Node("flame", 0x5, {{80, 1, {0, 0.5f}, {0, 0.3f}}});
    const size_t g = m.Node("ring", 0x21, {{128, 1, {0, 0.25f}, {1, 0}}, {8, 3, {0, 1}, {0.01f, 0, 0, 0.01f, 0, 1}},
                                           {20, 4, {0, 1}, {0, 0, 0, 1, 0, 0, 0.70710678f, 0.70710678f}}});
    m.Children(r, {f, g});
    m.Anim("cessation", 1.0f, t_cessation, r);
  }
  m.List();
  return m.b;
}

// A list's numbers as text ("%g" each, space-separated): what the draft writes.
std::string Numbers(const std::vector<float>& v) {
  std::string out;
  for (size_t i = 0; i < v.size(); ++i) {
    char text[32];
    snprintf(text, sizeof(text), "%g", v[i]);
    out += (i ? " " : "") + std::string(text);
  }
  return out;
}

const VfxKeys* KeysOf(const std::vector<VfxKeys>& list, const char* anim) {
  for (const VfxKeys& k : list) {
    if (k.anim == anim) return &k;
  }
  return nullptr;
}

// A lasting effect's emitter "flame" with birthrate 100 and no keys of its own, as nwn5e writes one.
VfxEffect FlameEffect() {
  VfxEffect effect;
  effect.name = "n5_flame";
  VfxEmitter flame;
  flame.name = "flame";
  flame.birthrate = 100.0f;
  effect.emitters.push_back(flame);
  return effect;
}

// The emitter node of `emitter` (the model's name for it) in one animation of a model named n5t ("" when none).
std::string EmitterIn(const std::string& model, const char* anim, const std::string& emitter) {
  return Section(Anim(model, anim), "node emitter " + emitter + "\n", "endnode");
}

}  // namespace

// The compiled reader keeps the emitter's own birthrate, alphaStart and alphaEnd keys in each animation that has them -
// every list as read (the duration's single alphaStart row too: a value set), none where it has none - and the mesh's
// alpha in each, its scale in the duration and its move and turn in the cessation (its impact's single position row at
// its geometry's place is no motion, as a node's), and each animation's transtime. What the emitter was read as before
// stays: birthrate 100 always on (a lasting effect's geometry rate), alpha 0.6 to 0.
TEST(VfxReadsTheGamesOwnKeysFromACompiledModel) {
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(KeyedModel(), &effect, &error) && effect.duration, true);
  CHECK_EQ(effect.emitters.size() == 1 && effect.planes.size() == 1, true);
  if (effect.emitters.size() != 1 || effect.planes.size() != 1) return;
  CHECK_EQ(Milli(effect.transtime[0]) == 250 && Milli(effect.transtime[1]) == 0 && Milli(effect.transtime[2]) == 150, true);
  const VfxEmitter& flame = effect.emitters[0];
  CHECK_EQ(Milli(flame.birthrate) == 100000 && Milli(flame.emit_seconds) == 0 && Milli(flame.alpha_start) == 600 &&
               Milli(flame.alpha_end) == 0,
           true);
  const VfxKeys* impact = KeysOf(flame.keys, "impact");
  const VfxKeys* duration = KeysOf(flame.keys, "duration");
  const VfxKeys* cessation = KeysOf(flame.keys, "cessation");
  CHECK_EQ(impact && Numbers(impact->birthrate) == "0 100 0.5 100 0.6 0" && Numbers(impact->alpha_start) == "0.5 0.6 0.95 0" &&
               impact->alpha_end.empty(),
           true);
  CHECK_EQ(duration && duration->birthrate.empty() && Numbers(duration->alpha_start) == "0 0" && duration->alpha_end.empty(), true);
  CHECK_EQ(cessation && cessation->birthrate.empty() && cessation->alpha_start.empty() && Numbers(cessation->alpha_end) == "0 0 0.5 0.3",
           true);
  const VfxPlane& ring = effect.planes[0];
  CHECK_EQ(Milli(ring.alpha), 0);
  const VfxKeys* fade_in = KeysOf(ring.keys, "impact");
  const VfxKeys* held = KeysOf(ring.keys, "duration");
  const VfxKeys* fade_out = KeysOf(ring.keys, "cessation");
  CHECK_EQ(fade_in && Numbers(fade_in->alpha) == "0 0 0.3 1 0.95 1" && fade_in->position.empty() && fade_in->scale.empty(), true);
  CHECK_EQ(held && Numbers(held->alpha) == "0 1 1 1" && Numbers(held->scale) == "0 1 0.5 1.5 1 1", true);
  CHECK_EQ(fade_out && Numbers(fade_out->alpha) == "0 1 0.25 0" && Numbers(fade_out->position) == "0 0.01 0 0 1 0.01 0 1", true);
  CHECK_EQ(fade_out && fade_out->orientation.size() == 10 && Milli(fade_out->orientation[5]) == 1000 &&
               Milli(fade_out->orientation[8]) == 1000 && Milli(fade_out->orientation[9]) == 1571,
           true);
  // A transtime past the game's (99) or below 0 isn't known.
  VfxEffect unknown;
  CHECK_EQ(ReadVfxBinaryModel(KeyedModel(false), &unknown, &error), true);
  CHECK_EQ(unknown.transtime[0] == -1.0f && unknown.transtime[1] == -1.0f && unknown.transtime[2] == -1.0f, true);
}

// The draft: an emitter's "<anim>_birthrate", "<anim>_alpha_start" and "<anim>_alpha_end" after its other key lines (any
// section, as every key list), and the effect's "<anim>_transtime = T" after its lengths, each only when there - and back
// as written; the model read writes the same draft again. A draft without them keeps its bytes.
TEST(VfxDraftKeepsTheGamesOwnKeys) {
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(KeyedModel(), &effect, &error), true);
  const std::string draft = WriteVfxDraft(effect);
  CHECK_EQ(Contains(draft, "\nlengths = 0.95 1 1\nimpact_transtime = 0.25\nduration_transtime = 0\ncessation_transtime = 0.15\n\n"), true);
  CHECK_EQ(Contains(draft, "\nflags =\nimpact_birthrate = 0 100 0.5 100 0.6 0\nimpact_alpha_start = 0.5 0.6 0.95 0\n"
                           "duration_alpha_start = 0 0\ncessation_alpha_end = 0 0 0.5 0.3\n"),
           true);
  CHECK_EQ(Contains(draft, "\nimpact_alpha = 0 0 0.3 1 0.95 1\nduration_alpha = 0 1 1 1\nduration_scale = 0 1 0.5 1.5 1 1\n"
                           "cessation_position = 0 0.01 0 0 1 0.01 0 1\n"),
           true);
  VfxEffect back;
  CHECK_EQ(ReadVfxDraft(draft, &back, &error), true);
  CHECK_EQ(WriteVfxDraft(back) == draft, true);
  CHECK_EQ(Milli(back.transtime[0]) == 250 && Milli(back.transtime[1]) == 0 && Milli(back.transtime[2]) == 150, true);
  CHECK_EQ(back.emitters.size() == 1 && KeysOf(back.emitters[0].keys, "impact") &&
               Numbers(KeysOf(back.emitters[0].keys, "impact")->birthrate) == "0 100 0.5 100 0.6 0",
           true);
  // Each transtime on its own, only where known; one not known writes no line.
  VfxEffect one = Sample();
  CHECK_EQ(Contains(WriteVfxDraft(one), "transtime"), false);
  one.transtime[1] = 0.15f;
  CHECK_EQ(Contains(WriteVfxDraft(one), "\nlengths = 0 2 1\nduration_transtime = 0.15\nshake = 2 0.5 1.25\n"), true);
  CHECK_EQ(Contains(WriteVfxDraft(one), "impact_transtime") || Contains(WriteVfxDraft(one), "cessation_transtime"), false);
  // Read by the rule every reader keeps: not a number, below 0 or past kVfxTranstimeMax - not known; -0 is 0.
  const auto read = [](const std::string& value) {
    VfxEffect e;
    std::string why;
    return ReadVfxDraft("name = x\nimpact_transtime = " + value + "\n", &e, &why) ? e.transtime[0] : -2.0f;
  };
  CHECK_EQ(read("0.25") == 0.25f && read("0") == 0.0f && read("60") == 60.0f, true);
  CHECK_EQ(read("-1") == -1.0f && read("-0.5") == -1.0f && read("nan") == -1.0f && read("inf") == -1.0f && read("60.5") == -1.0f &&
               read("x") == 0.0f,
           true);
  VfxEffect zero;
  CHECK_EQ(ReadVfxDraft("name = x\nimpact_transtime = -0\n", &zero, &error) && !std::signbit(zero.transtime[0]) &&
               Contains(WriteVfxDraft(zero), "\nimpact_transtime = 0\n"),
           true);
  // An emitter's key lines in any section: a node's read and written back (only an emitter's reach the model).
  VfxEffect spun;
  CHECK_EQ(ReadVfxDraft("name = x\n[node hub]\nimpact_birthrate = 0 5\nimpact_alpha_start = 0 1\nimpact_alpha_end = 0 0\n", &spun, &error) &&
               spun.nodes.size() == 1,
           true);
  CHECK_EQ(Contains(WriteVfxDraft(spun), "\norientation = 0 0 0 0\nimpact_birthrate = 0 5\nimpact_alpha_start = 0 1\nimpact_alpha_end = 0 0\n"),
           true);
  const std::string spun_model = WriteVfxModel(spun, "n5t");
  CHECK_EQ(Contains(spun_model, "birthratekey") || Contains(spun_model, "alphaStartkey") || Contains(spun_model, "alphaEndkey"), false);
}

// The model: an emitter with its own birthrate keys has them, and only them, in each animation - the impact's three rows,
// no birthratekey in the duration and the cessation (the game's controller holds the impact's 0: the flame stays out) -
// where an emitter without them has nwn5e's own (on again in the impact and the duration, off in the cessation). Its
// alphaStartkey and alphaEndkey after its move and turn keys, in each animation that has them. The animations last its
// keys.
TEST(VfxModelWritesTheEmittersOwnKeys) {
  const VfxEffect plain = FlameEffect();
  const std::string before = WriteVfxModel(plain, "n5t");
  CHECK_EQ(EmitterIn(before, "impact", "n5t_0_flame") == "node emitter n5t_0_flame\n  parent n5t\n  birthratekey 1\n    0 100\n", true);
  CHECK_EQ(EmitterIn(before, "cessation", "n5t_0_flame") == "node emitter n5t_0_flame\n  parent n5t\n  birthratekey 1\n    0 0\n", true);
  VfxEffect keyed = plain;
  VfxKeys impact;
  impact.anim = "impact";
  impact.birthrate = {0, 100, 0.5f, 100, 0.6f, 0};
  impact.position = {0, 0, 0, 1, 0.5f, 0, 0, 0};
  impact.alpha_start = {0.5f, 0.6f, 0.95f, 0};
  VfxKeys cessation;
  cessation.anim = "cessation";
  cessation.alpha_end = {0, 0, 0.5f, 0.3f};
  keyed.emitters[0].keys = {impact, cessation};
  const std::string model = WriteVfxModel(keyed, "n5t");
  CHECK_EQ(EmitterIn(model, "impact", "n5t_0_flame") ==
               "node emitter n5t_0_flame\n  parent n5t\n  birthratekey 3\n    0 100\n    0.5 100\n    0.6 0\n"
               "  positionkey 2\n    0 0 0 1\n    0.5 0 0 0\n  alphaStartkey 2\n    0.5 0.6\n    0.95 0\n",
           true);
  CHECK_EQ(EmitterIn(model, "duration", "n5t_0_flame") == "node emitter n5t_0_flame\n  parent n5t\n", true);
  CHECK_EQ(EmitterIn(model, "cessation", "n5t_0_flame") == "node emitter n5t_0_flame\n  parent n5t\n  alphaEndkey 2\n    0 0\n    0.5 0.3\n",
           true);
  // The geometry as before: its birthrate line, the rest.
  const auto geometry = [](const std::string& m) { return Section(m, "beginmodelgeom n5t", "endmodelgeom n5t"); };
  CHECK_EQ(!geometry(model).empty() && geometry(model) == geometry(before), true);
  // Its own keys win over nwn5e's emission keys: a one-shot's on-time, a start, an emit time - none of theirs written.
  for (int kind = 0; kind < 3; ++kind) {
    VfxEffect e = keyed;
    if (kind == 0) e.duration = false;
    if (kind == 1) e.emitters[0].start_seconds = 0.3f;
    if (kind == 2) e.emitters[0].emit_seconds = 0.2f;
    const std::string m = WriteVfxModel(e, "n5t");
    CHECK_EQ(Contains(EmitterIn(m, "impact", "n5t_0_flame"), "  birthratekey 3\n    0 100\n    0.5 100\n    0.6 0\n  positionkey"), true);
    size_t lists = 0;
    for (size_t at = m.find("birthratekey"); at != std::string::npos; at = m.find("birthratekey", at + 1)) ++lists;
    CHECK_EQ(lists, 1u);
  }
  // Alpha keys alone keep nwn5e's emission keys: only the birthrate's own replace them.
  VfxEffect faded = plain;
  faded.emitters[0].keys = {cessation};
  const std::string faded_model = WriteVfxModel(faded, "n5t");
  CHECK_EQ(Contains(EmitterIn(faded_model, "impact", "n5t_0_flame"), "  birthratekey 1\n    0 100\n"), true);
  CHECK_EQ(EmitterIn(faded_model, "cessation", "n5t_0_flame") ==
               "node emitter n5t_0_flame\n  parent n5t\n  birthratekey 1\n    0 0\n  alphaEndkey 2\n    0 0\n    0.5 0.3\n",
           true);
  // A list without a whole row is no keys: nwn5e's own then.
  VfxEffect few = plain;
  VfxKeys short_list;
  short_list.anim = "impact";
  short_list.birthrate = {0.5f};
  short_list.alpha_start = {0.5f};
  few.emitters[0].keys = {short_list};
  CHECK_EQ(WriteVfxModel(few, "n5t") == before, true);
  // The animations last its keys: a birthrate key at 4 s in the impact, an alphaStart key at 3 s in the duration, an
  // alphaEnd key at 5 s in the cessation.
  VfxEffect late = plain;
  VfxKeys a, b, c;
  a.anim = "impact";
  a.birthrate = {0, 100, 4, 0};
  b.anim = "duration";
  b.alpha_start = {0, 1, 3, 0};
  c.anim = "cessation";
  c.alpha_end = {0, 1, 5, 0};
  late.emitters[0].keys = {a, b, c};
  const std::string long_model = WriteVfxModel(late, "n5t");
  CHECK_EQ(Milli(AnimLength(long_model, "impact")) == 4000 && Milli(AnimLength(long_model, "duration")) == 3000 &&
               Milli(AnimLength(long_model, "cessation")) == 5000,
           true);
}

// The transtime: one kept (read from a model) is written as it is, in each animation - whatever TT's rule would say -
// unless the impact has a mesh beat (a plane the bench placed, its mesh named) whose first key there is off its rest: then
// the impact starts on its keys (0), as TT's rule has it - a Drop in on a game effect falls from its height. A plane of
// the effect's own (no mesh) never overrides a kept one. Without a kept one, TT's rule (0 when an item starts off its
// rest, else 0.25; the duration and the cessation 0.25), as before.
TEST(VfxModelWritesTheKeptTranstime) {
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(KeyedModel(), &effect, &error), true);
  const std::string model = WriteVfxModel(effect, "n5t");
  CHECK_EQ(Transtime(model, "impact") == "0.25" && Transtime(model, "duration") == "0" && Transtime(model, "cessation") == "0.15", true);
  // Kept against TT's rule: an emitter dropping in from off its rest keeps a kept 0.25; a kept 0 stays 0 at rest.
  VfxEffect off = FlameEffect();
  VfxKeys drop;
  drop.anim = "impact";
  drop.position = {0, 0, 0, 3, 0.5f, 0, 0, 0};
  off.emitters[0].keys = {drop};
  CHECK_EQ(Transtime(WriteVfxModel(off, "n5t"), "impact") == "0", true);  // TT's rule, none kept
  off.transtime[0] = 0.25f;
  CHECK_EQ(Transtime(WriteVfxModel(off, "n5t"), "impact") == "0.25", true);
  VfxEffect rest = FlameEffect();
  rest.transtime[0] = 0.0f;
  rest.transtime[2] = 0.5f;
  const std::string rest_model = WriteVfxModel(rest, "n5t");
  CHECK_EQ(Transtime(rest_model, "impact") == "0" && Transtime(rest_model, "duration") == "0.25" && Transtime(rest_model, "cessation") == "0.5",
           true);
  // A mesh beat dropping in from 3 m on a game effect (its impact kept at 0.25): 0. The same plane as the effect's own (no
  // mesh): 0.25, as kept. A mesh beat at its rest: 0.25. The duration's and the cessation's kept ones stay.
  VfxEffect beat = DiscEffect(0.0f, 3.0f);
  beat.transtime[0] = 0.25f;
  beat.transtime[1] = 0.15f;
  beat.transtime[2] = 0.0f;
  CHECK_EQ(Transtime(WriteVfxModel(beat, "n5t"), "impact") == "0.25", true);
  beat.planes[0].mesh = "n5m_test";
  const std::string beat_model = WriteVfxModel(beat, "n5t");
  CHECK_EQ(Transtime(beat_model, "impact") == "0" && Transtime(beat_model, "duration") == "0.15" && Transtime(beat_model, "cessation") == "0",
           true);
  VfxEffect landed = DiscEffect(3.0f, 3.0f);
  landed.planes[0].mesh = "n5m_test";
  landed.transtime[0] = 0.25f;
  CHECK_EQ(Transtime(WriteVfxModel(landed, "n5t"), "impact") == "0.25", true);
  // A mesh beat off its rest only in the duration leaves the kept impact.
  VfxEffect later = DiscKeyed("duration", drop);
  later.planes[0].mesh = "n5m_test";
  later.transtime[0] = 0.25f;
  CHECK_EQ(Transtime(WriteVfxModel(later, "n5t"), "impact") == "0.25", true);
}

// The ASCII reader: each animation's "transtime" line, an emitter's birthratekey, alphaStartkey and alphaEndkey (a count
// or an endlist, any case) and a trimesh's alphakey, scalekey and move and turn keys - into the same keys. The model the
// writer makes of the compiled one - its nwn5e line taken out, as a game's ASCII model (fix round 1: one with it is nwn5e's
// own, VfxNwn5esOwnModelsKeepNoneOfTheGamesKeys) - reads back with them, and writes the same model again.
TEST(VfxReadsTheGamesOwnKeysFromAnAsciiModel) {
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(KeyedModel(), &effect, &error), true);
  const std::string model = WriteVfxModel(effect, "n5t");
  const size_t mark = model.find(std::string("\n") + kVfxModelMark);
  CHECK_EQ(mark != std::string::npos, true);
  const std::string as_game = mark == std::string::npos ? model : model.substr(0, mark) + model.substr(model.find('\n', mark + 1));
  VfxEffect back;
  CHECK_EQ(ReadVfxAsciiModel(as_game, &back, &error) && back.emitters.size() == 1 && back.planes.size() == 1, true);
  if (back.emitters.size() != 1 || back.planes.size() != 1) return;
  CHECK_EQ(Milli(back.transtime[0]) == 250 && Milli(back.transtime[1]) == 0 && Milli(back.transtime[2]) == 150, true);
  const VfxKeys* impact = KeysOf(back.emitters[0].keys, "impact");
  CHECK_EQ(impact && Numbers(impact->birthrate) == "0 100 0.5 100 0.6 0" && Numbers(impact->alpha_start) == "0.5 0.6 0.95 0", true);
  CHECK_EQ(KeysOf(back.emitters[0].keys, "duration") && Numbers(KeysOf(back.emitters[0].keys, "duration")->alpha_start) == "0 0", true);
  const VfxKeys* held = KeysOf(back.planes[0].keys, "duration");
  CHECK_EQ(held && Numbers(held->alpha) == "0 1 1 1" && Numbers(held->scale) == "0 1 0.5 1.5 1 1", true);
  const VfxKeys* fade_out = KeysOf(back.planes[0].keys, "cessation");
  CHECK_EQ(fade_out && Numbers(fade_out->alpha) == "0 1 0.25 0" && fade_out->position.size() == 8 && fade_out->orientation.size() == 10, true);
  // Its names are the model's own; with the effect's names back it writes the same model again.
  back.name = effect.name;
  back.emitters[0].name = effect.emitters[0].name;
  back.planes[0].name = effect.planes[0].name;
  CHECK_EQ(WriteVfxModel(back, "n5t") == model, true);
  // Written by hand: an endlist list, the game's spelling and another case.
  const std::string text =
      "newmodel hand\nbeginmodelgeom hand\nnode dummy hand\n  parent NULL\nendnode\nnode emitter spark\n  parent hand\n"
      "  update Fountain\n  birthrate 10\nendnode\nnode trimesh disc\n  parent hand\n  verts 3\n    0 0 0\n    1 0 0\n    0 1 0\n"
      "  faces 1\n    0 1 2 1 0 1 2 1\nendnode\nendmodelgeom hand\n"
      "newanim impact hand\n  length 1\n  transtime 0.15\n  animroot hand\nnode emitter spark\n  parent hand\n"
      "  birthratekey\n    0 10\n    0.5 0\n  endlist\n  ALPHASTARTKEY 1\n    0 0.5\n  alphaEndkey 2\n    0 0\n    1 1\nendnode\n"
      "node trimesh disc\n  parent hand\n  alphakey 2\n    0 0\n    1 1\n  scalekey 1\n    0 2\nendnode\ndoneanim impact hand\n"
      "newanim duration hand\n  length 1\n  animroot hand\ndoneanim duration hand\ndonemodel hand\n";
  VfxEffect hand;
  CHECK_EQ(ReadVfxAsciiModel(text, &hand, &error) && hand.emitters.size() == 1 && hand.planes.size() == 1, true);
  if (hand.emitters.size() != 1 || hand.planes.size() != 1) return;
  CHECK_EQ(Milli(hand.transtime[0]) == 150 && hand.transtime[1] == -1.0f && hand.transtime[2] == -1.0f, true);
  const VfxKeys* spark = KeysOf(hand.emitters[0].keys, "impact");
  CHECK_EQ(spark && Numbers(spark->birthrate) == "0 10 0.5 0" && Numbers(spark->alpha_start) == "0 0.5" && Numbers(spark->alpha_end) == "0 0 1 1",
           true);
  CHECK_EQ(Milli(hand.emitters[0].emit_seconds), 500);  // read as before too
  const VfxKeys* disc = KeysOf(hand.planes[0].keys, "impact");
  CHECK_EQ(disc && Numbers(disc->alpha) == "0 0 1 1" && Numbers(disc->scale) == "0 2", true);
  // A mesh something hangs under is a node too: its fade and scale are its plane's alone (the node's keys move it).
  std::string under = text;
  const std::string hand_parent = "node emitter spark\n  parent hand\n";
  under.replace(under.find(hand_parent), hand_parent.size(), "node emitter spark\n  parent disc\n");
  VfxEffect hung;
  CHECK_EQ(ReadVfxAsciiModel(under, &hung, &error) && hung.planes.size() == 1 && hung.nodes.size() == 1 && hung.nodes[0].name == "disc", true);
  if (hung.planes.size() == 1 && hung.nodes.size() == 1) {
    const VfxKeys* node_keys = KeysOf(hung.nodes[0].keys, "impact");
    CHECK_EQ(!node_keys || (node_keys->alpha.empty() && node_keys->scale.empty()), true);
    CHECK_EQ(KeysOf(hung.planes[0].keys, "impact") && Numbers(KeysOf(hung.planes[0].keys, "impact")->alpha) == "0 0 1 1", true);
  }
  // An emitter keyed only in another animation (a placeable's animloop01: no effect's): none of its own in the draft, so
  // its model is nwn5e's emission keys as before - the model read and the model of its draft the same.
  std::string loop = text;
  loop.replace(loop.find("newanim impact hand"), 19, "newanim animloop01 hand");
  loop.replace(loop.find("doneanim impact hand"), 20, "doneanim animloop01 hand");
  VfxEffect looped;
  CHECK_EQ(ReadVfxAsciiModel(loop, &looped, &error) && looped.emitters.size() == 1, true);
  VfxEffect drafted;
  CHECK_EQ(ReadVfxDraft(WriteVfxDraft(looped), &drafted, &error), true);
  CHECK_EQ(Contains(WriteVfxDraft(looped), "_birthrate"), false);
  CHECK_EQ(WriteVfxModel(looped, "n5t") == WriteVfxModel(drafted, "n5t"), true);
  CHECK_EQ(Contains(EmitterIn(WriteVfxModel(looped, "n5t"), "impact", "n5t_0_spark"), "  birthratekey 1\n    0 10\n"), true);
}

// Byte for byte (the brief's guarantee): every draft nwn5e keeps - the fixtures and Detect Magic's four - has no transtime
// and no emitter keys of the new kinds, and writes the draft and the model it did (VfxKeptDraftsUnchanged holds Detect
// Magic's committed models; TT's tests their transtimes).
TEST(VfxKeptDraftsHaveNoneOfTheGamesOwnKeys) {
  std::vector<std::string> paths;
  for (const auto& entry : std::filesystem::directory_iterator("tests/data/fixtures")) {
    if (entry.path().extension() == ".vfx") paths.push_back(entry.path().generic_string());
  }
  for (const char* size : {"h", "l", "m", "s"}) paths.push_back(std::string("tools/data/vfx/n5_detmag_") + size + ".vfx");
  CHECK_EQ(paths.size() >= 8, true);
  for (const std::string& path : paths) {
    const std::string text = FixtureText(path.c_str());
    VfxEffect effect;
    std::string error;
    CHECK_EQ(ReadVfxDraft(text, &effect, &error) && WriteVfxDraft(effect) == text, true);
    bool kept = effect.transtime[0] != -1.0f || effect.transtime[1] != -1.0f || effect.transtime[2] != -1.0f;
    for (const VfxEmitter& e : effect.emitters) {
      for (const VfxKeys& k : e.keys) {
        kept = kept || !k.birthrate.empty() || !k.alpha_start.empty() || !k.alpha_end.empty() || !k.color_start.empty() || !k.color_end.empty() ||
               !k.size_start.empty() || !k.size_end.empty();
      }
    }
    CHECK_EQ(kept, false);
    const std::string model = WriteVfxModel(effect, "n5t");
    CHECK_EQ(Contains(model, "alphaStartkey") || Contains(model, "alphaEndkey") || Contains(model, "colorStartkey") || Contains(model, "colorEndkey") ||
                 Contains(model, "sizeStartkey") || Contains(model, "sizeEndkey"),
             false);
  }
}

// The anchor (the owner's TMO-4): the game's own vdr_eleshield (VFX_DUR_ELEMENTAL_SHIELD) as the VFX Bench reads it. Its
// Flame01 keys its birthrate 100 -> 0 in the impact (off by 0.6 s, none after: the flame stops) and its alphaStart; its
// ring Cylinder02 (alpha 0 at rest) fades in over the impact's first 0.3 s and out in the cessation; every transtime
// 0.25. The draft and the model carry them: the impact's birthratekey 3 for Flame01, no birthratekey of its in the
// duration, Cylinder02's alphakey in the impact. Only where the game is (NWN_GAME_DIR, or Steam's folder); nothing of it
// is kept.
TEST(VfxGameEffectsElementalShieldKeys) {
  const char* given = getenv("NWN_GAME_DIR");
  const std::string game = given && *given ? given : "X:\\SteamLibrary\\steamapps\\common\\Neverwinter Nights";
  if (!std::ifstream(game + "\\data\\nwn_base.key")) {
    printf("    (no game at %s: skipped)\n", game.c_str());
    return;
  }
  const std::wstring dir(game.begin(), game.end());
  GameResources resources(dir, GameResources::EeKeyFiles(), {dir + L"\\ovr"});
  std::string bytes, source, error;
  CHECK_EQ(resources.Read("vdr_eleshield", kResourceTypeMdl, &bytes, &source), true);
  VfxEffect shield;
  CHECK_EQ(ReadVfxBinaryModel(bytes, &shield, &error) && shield.duration, true);
  int flame = -1, ring = -1;
  for (size_t i = 0; i < shield.emitters.size(); ++i) {
    if (shield.emitters[i].name == "Flame01") flame = static_cast<int>(i);
  }
  for (size_t i = 0; i < shield.planes.size(); ++i) {
    if (shield.planes[i].name == "Cylinder02") ring = static_cast<int>(i);
  }
  CHECK_EQ(flame >= 0 && ring >= 0, true);
  if (flame < 0 || ring < 0) return;
  const VfxKeys* on = KeysOf(shield.emitters[flame].keys, "impact");
  printf("    vdr_eleshield Flame01 impact birthrate %s, alphaStart %s; Cylinder02 impact alpha %s; transtimes %g %g %g\n",
         on ? Numbers(on->birthrate).c_str() : "-", on ? Numbers(on->alpha_start).c_str() : "-",
         KeysOf(shield.planes[ring].keys, "impact") ? Numbers(KeysOf(shield.planes[ring].keys, "impact")->alpha).c_str() : "-",
         shield.transtime[0], shield.transtime[1], shield.transtime[2]);
  CHECK_EQ(on && on->birthrate.size() == 6 && Milli(on->birthrate[0]) == 0 && Milli(on->birthrate[1]) == 100000 &&
               Milli(on->birthrate[3]) == 100000 && Milli(on->birthrate[4]) == 600 && Milli(on->birthrate[5]) == 0,
           true);
  CHECK_EQ(on && on->alpha_start.size() >= 4 && Milli(on->alpha_start[1]) == 600 && Milli(on->alpha_start.back()) == 0, true);
  const VfxKeys* later = KeysOf(shield.emitters[flame].keys, "duration");
  CHECK_EQ(!later || later->birthrate.empty(), true);
  CHECK_EQ(Milli(shield.planes[ring].alpha), 0);
  const VfxKeys* fade_in = KeysOf(shield.planes[ring].keys, "impact");
  CHECK_EQ(fade_in && fade_in->alpha.size() >= 4 && Milli(fade_in->alpha[0]) == 0 && Milli(fade_in->alpha[1]) == 0 &&
               Milli(fade_in->alpha[2]) == 300 && Milli(fade_in->alpha[3]) == 1000,
           true);
  const VfxKeys* fade_out = KeysOf(shield.planes[ring].keys, "cessation");
  CHECK_EQ(fade_out && fade_out->alpha.size() >= 4 && Milli(fade_out->alpha[1]) == 1000 && Milli(fade_out->alpha.back()) == 0, true);
  CHECK_EQ(Milli(shield.transtime[0]) == 250 && Milli(shield.transtime[1]) == 250 && Milli(shield.transtime[2]) == 250, true);
  const std::string draft = WriteVfxDraft(shield);
  CHECK_EQ(Contains(draft, "\nimpact_transtime = 0.25\nduration_transtime = 0.25\ncessation_transtime = 0.25\n"), true);
  CHECK_EQ(Contains(draft, "\nimpact_birthrate = 0 100 "), true);
  VfxEffect back;
  CHECK_EQ(ReadVfxDraft(draft, &back, &error) && WriteVfxDraft(back) == draft, true);
  const std::string model = WriteVfxModel(back, "n5t");
  const std::string flame_node = "n5t_" + std::to_string(flame) + "_flame01";
  CHECK_EQ(Contains(EmitterIn(model, "impact", flame_node), "\n  birthratekey 3\n"), true);
  CHECK_EQ(Contains(EmitterIn(model, "duration", flame_node), "birthratekey"), false);
  const std::string ring_node = "node trimesh n5t_p" + std::to_string(ring) + "_cylinder02\n";
  CHECK_EQ(Contains(Section(Anim(model, "impact"), ring_node, "endnode"), "\n  alphakey "), true);
  CHECK_EQ(Transtime(model, "impact") == "0.25" && Transtime(model, "duration") == "0.25" && Transtime(model, "cessation") == "0.25", true);
}

// --- KF fix round 1 (the review, 2026-11-19) ---

namespace {

// Whether an effect carries none of KF's additions: no transtime known, no emitter's own birthrate / alphaStart /
// alphaEnd list, no plane key at all - what the readers kept before KF.
bool NoneOfTheGamesKeys(const VfxEffect& effect) {
  bool none = effect.transtime[0] == -1.0f && effect.transtime[1] == -1.0f && effect.transtime[2] == -1.0f;
  for (const VfxEmitter& e : effect.emitters) {
    for (const VfxKeys& k : e.keys) {
      none = none && k.birthrate.empty() && k.alpha_start.empty() && k.alpha_end.empty() && k.color_start.empty() && k.color_end.empty() &&
             k.size_start.empty() && k.size_end.empty();  // (#179 Phase 3's too)
    }
  }
  for (const VfxPlane& p : effect.planes) none = none && p.keys.empty();
  return none;
}

}  // namespace

// A model nwn5e wrote itself is read as before KF (the review: at format 20 the library's nwn5e drafts would have gained
// their own generated emission keys and transtimes as "the game's" - an On for on Detect Magic dropped, a Drop in on
// n5_ds_00's emitter blending 0.25 s from its rest). nwn5e's mark: its first comment lines (before newmodel) carry one
// starting kVfxModelMark - the writer's "# nwn5e VFX editor (!vfxedit): <name>", the generators' "# nwn5e
// (tools/data/<script>.py): ...", the Short Rest's "# nwn5e: ...". Such a model keeps no transtime, no emitter's own
// list (its emission keys are read into its fields, as before) and no plane key. Every kept draft's model, and every one
// of nwn5e's committed models (data/files), reads so - its draft has none of their lines. The same text without the mark
// (as a game's model) keeps them.
TEST(VfxNwn5esOwnModelsKeepNoneOfTheGamesKeys) {
  std::vector<std::string> paths;
  for (const auto& entry : std::filesystem::directory_iterator("tests/data/fixtures")) {
    if (entry.path().extension() == ".vfx") paths.push_back(entry.path().generic_string());
  }
  for (const char* size : {"h", "l", "m", "s"}) paths.push_back(std::string("tools/data/vfx/n5_detmag_") + size + ".vfx");
  const auto lines = [](const std::string& draft) {
    for (const char* kind : {"_transtime = ", "_birthrate = ", "_alpha_start = ", "_alpha_end = ", "_color_start = ", "_color_end = ", "_size_start = ",
                             "_size_end = "}) {
      if (Contains(draft, kind)) return true;
    }
    for (size_t at = draft.find("\n[plane "); at != std::string::npos; at = draft.find("\n[plane ", at + 1)) {
      const std::string section = draft.substr(at, draft.find("\n[", at + 1) - at);
      for (const char* kind : {"_position = ", "_orientation = ", "_alpha = ", "_scale = "}) {
        if (Contains(section, kind)) return true;
      }
    }
    return false;
  };
  int models = 0;
  for (const std::string& path : paths) {
    VfxEffect effect;
    std::string error;
    CHECK_EQ(ReadVfxDraft(FixtureText(path.c_str()), &effect, &error), true);
    const std::string model = WriteVfxModel(effect, "n5t");
    CHECK_EQ(model.rfind(std::string("#MAXMODEL ASCII\n") + kVfxModelMark + " VFX editor (!vfxedit): ", 0) == 0, true);
    VfxEffect read;
    if (!ReadVfxAsciiModel(model, &read, &error)) continue;  // (a draft of planes alone with no faces)
    ++models;
    if (!NoneOfTheGamesKeys(read) || lines(WriteVfxDraft(read))) printf("    %s: its model read back keeps KF's keys\n", path.c_str());
    CHECK_EQ(NoneOfTheGamesKeys(read) && !lines(WriteVfxDraft(read)), true);
  }
  CHECK_EQ(models >= 8, true);
  int committed = 0;
  for (const auto& entry : std::filesystem::directory_iterator("data/files")) {
    const std::string name = entry.path().filename().string();
    if (entry.path().extension() != ".mdl" || name.rfind("n5", 0) != 0) continue;
    const std::string text = FixtureText(entry.path().generic_string().c_str());
    VfxEffect read;
    std::string error;
    if (text.rfind("#", 0) != 0 || !ReadVfxAsciiModel(text, &read, &error)) continue;
    ++committed;
    if (!NoneOfTheGamesKeys(read)) printf("    data/files/%s keeps KF's keys\n", name.c_str());
    CHECK_EQ(NoneOfTheGamesKeys(read) && !lines(WriteVfxDraft(read)), true);
  }
  printf("    %d kept drafts' models and %d of nwn5e's committed models read with none of KF's keys\n", models, committed);
  CHECK_EQ(committed >= 18, true);
  // The control: plane_beat's model (its plane faded in by keys, its impact transtime 0) without nwn5e's line - as a game's
  // model - keeps its plane's keys and its transtimes; with it, none.
  VfxEffect beat;
  std::string error;
  CHECK_EQ(ReadVfxDraft(FixtureText("tests/data/fixtures/plane_beat.vfx"), &beat, &error), true);
  const std::string own = WriteVfxModel(beat, "n5t");
  const size_t mark = own.find(std::string("\n") + kVfxModelMark);
  CHECK_EQ(mark != std::string::npos, true);
  if (mark == std::string::npos) return;
  const std::string game = own.substr(0, mark) + own.substr(own.find('\n', mark + 1));
  VfxEffect as_own, as_game;
  CHECK_EQ(ReadVfxAsciiModel(own, &as_own, &error) && ReadVfxAsciiModel(game, &as_game, &error), true);
  CHECK_EQ(NoneOfTheGamesKeys(as_own), true);
  CHECK_EQ(as_game.planes.size() == 1 && KeysOf(as_game.planes[0].keys, "impact") &&
               Numbers(KeysOf(as_game.planes[0].keys, "impact")->alpha) == "0 0 0.3 0 0.5 0.8 1.3 0.8 1.5 0" &&
               Milli(as_game.transtime[0]) == 0 && Milli(as_game.transtime[1]) == 250,
           true);
  // A mark further down (not in its first comment lines) is no mark.
  const size_t after_newmodel = game.find('\n', game.find("newmodel")) + 1;
  const std::string later = game.substr(0, after_newmodel) + kVfxModelMark + " VFX editor (!vfxedit): n5t\n" + game.substr(after_newmodel);
  VfxEffect late;
  CHECK_EQ(ReadVfxAsciiModel(later, &late, &error) && !NoneOfTheGamesKeys(late), true);
}

// The animations are the model header's own (the review: the first "\0impact\0" in the file with a length in (0, 60) took a
// node named impact for one - c_air's, a_gnome's garbage transtimes - and missed a real one 0 s long - vps_bugs' impact,
// the game's transtime 0): its list at +0x78 (the headers' offsets, as the game keeps them; their count at +0x7C), each
// header's name at +8. A real animation 0 s long counts - its transtime and its keys read, its length not (0: the writer's
// own, as before); a node named impact before it is no animation; a model with no list has none. A one-shot's seconds are
// its listed impact's length.
TEST(VfxReadsTheModelsOwnAnimations) {
  const auto build = [](bool lasting, bool listed) {
    KfModel m("test_anims");
    const size_t root = m.Node("test_anims", 0x1, {{8, 3, {0}, {0, 0, 0}}});
    const size_t spark = m.Node("spark", 0x5, {{8, 3, {0}, {0, 0, 0}}, {88, 1, {0}, {20}}});
    m.Emitter(spark);
    const size_t decoy = m.Node("impact", 0x1, {});  // a dummy named impact, before the animations in the file
    m.PutF(decoy + 0x10 - 8 + 0x70, 0.5f);         // what a search for the name would take for a length ...
    m.PutF(decoy + 0x10 - 8 + 0x74, 0.633f);       // ... and a transtime
    m.Children(root, {spark, decoy});
    m.Put32(KfModel::kBase + 0x48, static_cast<uint32_t>(root - 0x10 - KfModel::kBase));
    {
      const size_t r = m.Node("test_anims", 0x1, {});
      const size_t f = m.Node("spark", 0x5, {{88, 1, {0, 0.4f}, {20, 0}}});
      m.Children(r, {f});
      m.Anim("impact", 1.5f, 0.25f, r, listed);
    }
    if (lasting) {
      const size_t r = m.Node("test_anims", 0x1, {});
      const size_t f = m.Node("spark", 0x5, {{88, 1, {0}, {5}}});
      m.Children(r, {f});
      m.Anim("duration", 0.0f, 0.0f, r, listed);
      // A second impact listed after the first: the first of a name is the one (as the game's ReadAnimClip takes it).
      const size_t r2 = m.Node("test_anims", 0x1, {});
      const size_t f2 = m.Node("spark", 0x5, {{88, 1, {0}, {7}}});
      m.Children(r2, {f2});
      m.Anim("impact", 2.0f, 0.6f, r2, listed);
    }
    m.List();
    return m.b;
  };
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(build(true, true), &effect, &error) && effect.duration && effect.emitters.size() == 1, true);
  if (effect.emitters.size() != 1) return;
  printf("    transtimes %g %g %g, lengths %g %g %g\n", effect.transtime[0], effect.transtime[1], effect.transtime[2], effect.lengths[0],
         effect.lengths[1], effect.lengths[2]);
  CHECK_EQ(Milli(effect.transtime[0]) == 250 && Milli(effect.transtime[1]) == 0 && effect.transtime[2] == -1.0f, true);
  CHECK_EQ(Milli(effect.lengths[0]) == 1500 && Milli(effect.lengths[1]) == 2000 && Milli(effect.lengths[2]) == 1000, true);
  const VfxKeys* impact = KeysOf(effect.emitters[0].keys, "impact");
  const VfxKeys* duration = KeysOf(effect.emitters[0].keys, "duration");
  CHECK_EQ(impact && Numbers(impact->birthrate) == "0 20 0.4 0" && duration && Numbers(duration->birthrate) == "0 5", true);
  // A one-shot: its seconds the listed impact's length.
  VfxEffect once;
  CHECK_EQ(ReadVfxBinaryModel(build(false, true), &once, &error) && !once.duration && Milli(once.seconds) == 1500 && Milli(once.transtime[0]) == 250, true);
  // No list: no animations - nothing keyed, nothing known, the lengths and seconds as a draft's.
  VfxEffect unlisted;
  CHECK_EQ(ReadVfxBinaryModel(build(false, false), &unlisted, &error) && unlisted.emitters.size() == 1, true);
  CHECK_EQ(NoneOfTheGamesKeys(unlisted) && unlisted.emitters[0].keys.empty() && Milli(unlisted.seconds) == 8000 && Milli(unlisted.lengths[0]) == 0, true);
}

// --- One burst (LD; the owner, 2026-11-20: "For the lectern A"): a placeable model's looping "dead" loses its detonate
// events, so it breaks with its "die" burst once (tools/data/oneburst.js).
namespace {

// An animation's events in a model built here (KfModel): the array (36 bytes an event: its time, then a 32-byte name)
// and the header's offset, count and allocated count (+0xB8, +0xBC, +0xC0), as the game's models lay them out.
void PutEvents(KfModel* m, size_t header, const std::vector<std::pair<float, const char*>>& events) {
  const size_t array = m->Alloc(36 * events.size());
  for (size_t i = 0; i < events.size(); ++i) {
    m->PutF(array + 36 * i, events[i].first);
    m->Puts(array + 36 * i + 4, events[i].second);
  }
  m->Put32(header + 0xB8, static_cast<uint32_t>(array - KfModel::kBase));
  m->Put32(header + 0xBC, static_cast<uint32_t>(events.size()));
  m->Put32(header + 0xC0, static_cast<uint32_t>(events.size()));
}

uint32_t U32At(const std::string& b, size_t at) {
  uint32_t v = 0;
  memcpy(&v, &b[at], 4);
  return v;
}

// The lectern's shape (PLC_K02), built here: an Explosion emitter; "damage" (no events), "die" (two detonates and a
// sound), "dead" - `dead` its events - and "default" (no events). -> the model; `dead_header` its "dead" header's offset.
std::string FountainModel(const std::vector<std::pair<float, const char*>>& dead, size_t* dead_header) {
  KfModel m("plc_test");
  const size_t root = m.Node("plc_test", 0x1, {{8, 3, {0}, {0, 0, 0}}});
  const size_t chunks = m.Node("ChunkyWood1", 0x5, {{88, 1, {0}, {2}}});
  m.Emitter(chunks);
  m.Puts(chunks + 0x60 + 0x18, "Explosion");
  m.Children(root, {chunks});
  m.Put32(KfModel::kBase + 0x48, static_cast<uint32_t>(root - 0x10 - KfModel::kBase));
  m.Anim("damage", 0.133f, 0.0f, root);
  m.Anim("die", 0.167f, 0.0f, root);
  PutEvents(&m, m.anims.back(), {{0.033f, "detonate"}, {0.033f, "detonate"}, {0.1f, "snd_crash"}});
  m.Anim("dead", 0.033f, 0.0f, root);
  *dead_header = m.anims.back();
  if (!dead.empty()) PutEvents(&m, m.anims.back(), dead);
  m.Anim("default", 0.0f, 0.25f, root);
  m.List();
  return m.b;
}

// "name time name time ..." of an animation's events, to compare in one line.
std::string EventsText(const ModelAnimation& a) {
  std::string out;
  for (const ModelEvent& e : a.events) out += (out.empty() ? "" : " ") + e.name + " " + std::to_string(Milli(e.time));
  return out;
}

}  // namespace

// A compiled model's "dead" loses its detonates, nothing else. Breaks caught: the die's detonates taken too (the burst
// lost); another event of the dead's lost, or the kept one left in its slot after a hole (the engine reads the first
// `count`); the count or the allocated count left as they were; a freed entry left with "detonate" in it; a name taken
// case-blind on one side only ("DEAD", "Detonate"); any byte outside the dead's array and its three fields changed; the
// file's length changed; a second run changing anything.
TEST(OneBurstCompiledDeadLosesItsDetonates) {
  size_t dead_header = 0;
  const std::string game = FountainModel({{0.033f, "detonate"}, {0.0f, "snd_dead"}, {0.033f, "Detonate"}}, &dead_header);
  std::vector<ModelAnimation> anims;
  std::string error;
  CHECK_EQ(ReadModelAnimations(game, &anims, &error), true);
  CHECK_EQ(anims.size(), 4u);
  if (anims.size() != 4) return;
  CHECK_EQ(anims[0].name == "damage" && anims[1].name == "die" && anims[2].name == "dead" && anims[3].name == "default", true);
  CHECK_EQ(Milli(anims[1].length), 167);
  CHECK_EQ(EventsText(anims[1]) == "detonate 33 detonate 33 snd_crash 100", true);
  CHECK_EQ(EventsText(anims[2]) == "detonate 33 snd_dead 0 Detonate 33", true);
  CHECK_EQ(anims[0].events.empty() && anims[0].event_at == 0 && anims[3].events.empty(), true);
  CHECK_EQ(anims[2].header, dead_header);
  const size_t array = anims[2].event_at;
  CHECK_EQ(array, KfModel::kBase + U32At(game, dead_header + 0xB8));

  std::string copy = game;
  CHECK_EQ(DropAnimationEvents(&copy, "DEAD", "detonate"), 2);
  CHECK_EQ(copy.size(), game.size());
  std::vector<ModelAnimation> after;
  CHECK_EQ(ReadModelAnimations(copy, &after, &error), true);
  CHECK_EQ(after.size(), 4u);
  if (after.size() != 4) return;
  CHECK_EQ(EventsText(after[2]) == "snd_dead 0", true);
  CHECK_EQ(EventsText(after[1]) == "detonate 33 detonate 33 snd_crash 100", true);  // the die's burst kept
  CHECK_EQ(after[2].event_at, array);  // the array where it was, the kept event first
  CHECK_EQ(U32At(copy, dead_header + 0xBC), 1u);
  CHECK_EQ(U32At(copy, dead_header + 0xC0), 1u);
  CHECK_EQ(copy.compare(array, 36, game, array + 36, 36), 0);  // the kept entry's own 36 bytes, moved up
  CHECK_EQ(copy.substr(array + 36, 72) == std::string(72, '\0'), true);
  // Every other byte as it was.
  int outside = 0;
  for (size_t i = 0; i < game.size(); ++i) {
    const bool fields = i >= dead_header + 0xB8 && i < dead_header + 0xC4, entries = i >= array && i < array + 3 * 36;
    if (copy[i] != game[i] && !fields && !entries) ++outside;
  }
  CHECK_EQ(outside, 0);
  // Again: nothing more to take out, nothing changed.
  const std::string once = copy;
  CHECK_EQ(DropAnimationEvents(&copy, "dead", "detonate"), 0);
  CHECK_EQ(copy == once, true);
  // Not there: an animation the model hasn't, an event it hasn't - 0, nothing changed.
  CHECK_EQ(DropAnimationEvents(&copy, "off", "detonate"), 0);
  CHECK_EQ(DropAnimationEvents(&copy, "dead", "snd_crash"), 0);
  CHECK_EQ(copy == once, true);
}

// A "dead" with only detonates (the lectern's: one) ends with no events, as the game's own models keep an animation with
// none (the bench's "dead": offset 0, count 0, allocated 0). Breaks caught: the offset kept (an empty array that points
// somewhere); the count or the allocated count not 0; the freed entries left as they were.
TEST(OneBurstCompiledDeadOfOnlyDetonatesEndsEmpty) {
  size_t dead_header = 0;
  const std::string game = FountainModel({{0.033f, "detonate"}, {0.0f, "detonate"}}, &dead_header);
  std::vector<ModelAnimation> anims;
  std::string error;
  CHECK_EQ(ReadModelAnimations(game, &anims, &error) && anims.size() == 4, true);
  if (anims.size() != 4) return;
  const size_t array = anims[2].event_at;
  std::string copy = game;
  CHECK_EQ(DropAnimationEvents(&copy, "dead", "DETONATE"), 2);
  CHECK_EQ(U32At(copy, dead_header + 0xB8), 0u);
  CHECK_EQ(U32At(copy, dead_header + 0xBC), 0u);
  CHECK_EQ(U32At(copy, dead_header + 0xC0), 0u);
  CHECK_EQ(copy.substr(array, 72) == std::string(72, '\0'), true);
  std::vector<ModelAnimation> after;
  CHECK_EQ(ReadModelAnimations(copy, &after, &error) && after.size() == 4, true);
  if (after.size() != 4) return;
  CHECK_EQ(after[2].events.empty() && after[2].event_at == 0, true);
  CHECK_EQ(EventsText(after[1]) == "detonate 33 detonate 33 snd_crash 100", true);
  // A "dead" with no events at all (the bench's): nothing to take out.
  size_t bench_dead = 0;
  std::string bench = FountainModel({}, &bench_dead);
  const std::string kept = bench;
  CHECK_EQ(DropAnimationEvents(&bench, "dead", "detonate"), 0);
  CHECK_EQ(bench == kept, true);
}

// Not a model it reads: nothing changed, false and why. Breaks caught: a list or an array running past the file read
// anyway (a crash on a cut file); text with no newmodel taken for an ASCII model.
TEST(OneBurstNotAModel) {
  std::vector<ModelAnimation> anims;
  std::string error;
  CHECK_EQ(ReadModelAnimations("", &anims, &error), false);
  CHECK_EQ(ReadModelAnimations("hello\r\nworld\r\n", &anims, &error), false);
  size_t dead_header = 0;
  std::string cut = FountainModel({{0.033f, "detonate"}}, &dead_header);
  std::vector<ModelAnimation> whole;
  CHECK_EQ(ReadModelAnimations(cut, &whole, &error) && whole.size() == 4, true);
  if (whole.size() != 4) return;
  cut.resize(whole[2].event_at + 10);  // the dead's array cut short
  CHECK_EQ(ReadModelAnimations(cut, &anims, &error), false);
  CHECK_EQ(error.empty(), false);
  const std::string kept = cut;
  CHECK_EQ(DropAnimationEvents(&cut, "dead", "detonate"), 0);
  CHECK_EQ(cut == kept, true);
  // A "dead" whose events' count runs its array past the file's end: refused, nothing changed.
  std::string over = FountainModel({{0.033f, "detonate"}}, &dead_header);
  const uint32_t many = 1000;  // 36,000 bytes from its array: past the 32 KB file
  memcpy(&over[dead_header + 0xBC], &many, 4);
  CHECK_EQ(ReadModelAnimations(over, &anims, &error), false);
  if (!ReadModelAnimations(over, &anims, &error)) {
    const std::string kept_over = over;
    CHECK_EQ(DropAnimationEvents(&over, "dead", "detonate"), 0);
    CHECK_EQ(over == kept_over, true);
  }
  std::string text = "just text\r\n";
  CHECK_EQ(DropAnimationEvents(&text, "dead", "detonate"), 0);
  CHECK_EQ(DropAnimationEvents(nullptr, "dead", "detonate"), 0);
}

// An ASCII model (PLC_C10's, the game's, is one) loses the dead block's detonate lines, nothing else. Breaks caught: the
// die's or the damage's detonate lines taken (another block's); a node's line taken; the line ends changed (CRLF kept);
// "newanim DEAD" or "Detonate" missed; the lengths and events read wrong.
TEST(OneBurstAsciiDeadLosesItsDetonates) {
  const std::string head =
      "# Exported from somewhere\r\nnewmodel PLC_T10\r\nsetsupermodel PLC_T10 NULL\r\nclassification Character\r\n"
      "beginmodelgeom PLC_T10\r\nnode dummy PLC_T10\r\n  parent NULL\r\nendnode\r\nnode emitter ChunkyWood1\r\n  parent PLC_T10\r\n"
      "  update Explosion\r\n  chunkName PLC_Chunk_W01\r\nendnode\r\nendmodelgeom PLC_T10\r\n\r\n";
  const std::string die =
      "newanim die PLC_T10\r\n  length 0.0333333\r\n  transtime 0.25\r\n  animroot PLC_T10\r\n  event 0.0333333 detonate\r\n"
      "  event 0.0333333 detonate\r\nnode dummy PLC_T10\r\n  parent NULL\r\nendnode\r\ndoneanim die PLC_T10\r\n\r\n";
  const std::string damage =
      "newanim damage PLC_T10\r\n  length 0.0\r\n  transtime 0.0\r\n  animroot PLC_T10\r\n  event 0.0 detonate\r\n"
      "node dummy PLC_T10\r\n  parent NULL\r\nendnode\r\ndoneanim damage PLC_T10\r\n\r\n";
  const auto dead = [](bool detonates) {
    return std::string("newanim DEAD PLC_T10\r\n  length 0.0333333\r\n  transtime 0.25\r\n  animroot PLC_T10\r\n") +
           (detonates ? "  event 0.0 detonate\r\n" : "") + "  event 0.0 snd_dead\r\n" + (detonates ? "\tevent 0.0 Detonate\r\n" : "") +
           "node dummy PLC_T10\r\n  parent NULL\r\nendnode\r\ndoneanim DEAD PLC_T10\r\n\r\n";
  };
  const std::string game = head + die + damage + dead(true) + "donemodel PLC_T10\r\n";
  std::vector<ModelAnimation> anims;
  std::string error;
  CHECK_EQ(ReadModelAnimations(game, &anims, &error), true);
  CHECK_EQ(anims.size(), 3u);
  if (anims.size() != 3) return;
  CHECK_EQ(anims[0].name == "die" && anims[1].name == "damage" && anims[2].name == "DEAD", true);
  CHECK_EQ(Milli(anims[0].length) == 33 && Milli(anims[2].length) == 33 && anims[0].header == 0, true);
  CHECK_EQ(EventsText(anims[0]) == "detonate 33 detonate 33", true);
  CHECK_EQ(EventsText(anims[2]) == "detonate 0 snd_dead 0 Detonate 0", true);
  std::string copy = game;
  CHECK_EQ(DropAnimationEvents(&copy, "dead", "detonate"), 2);
  CHECK_EQ(copy == head + die + damage + dead(false) + "donemodel PLC_T10\r\n", true);
  std::vector<ModelAnimation> after;
  CHECK_EQ(ReadModelAnimations(copy, &after, &error) && after.size() == 3, true);
  if (after.size() != 3) return;
  CHECK_EQ(EventsText(after[2]) == "snd_dead 0" && EventsText(after[0]) == "detonate 33 detonate 33" &&
               EventsText(after[1]) == "detonate 0",
           true);
  const std::string once = copy;
  CHECK_EQ(DropAnimationEvents(&copy, "dead", "detonate"), 0);
  CHECK_EQ(copy == once, true);
}

// --- #175: an emitter's area past one emitter's reach, and a circle, written as pieces --------------------------------

namespace {

bool Overlap(const VfxAreaTile& a, const VfxAreaTile& b) {
  const float ox = std::min(a.x + a.width / 2, b.x + b.width / 2) - std::max(a.x - a.width / 2, b.x - b.width / 2);
  const float oy = std::min(a.y + a.depth / 2, b.y + b.depth / 2) - std::max(a.y - a.depth / 2, b.y - b.depth / 2);
  return ox > 0.01f && oy > 0.01f;
}

VfxEmitter AreaEmitter(const char* update, float across, float deep, int shape, int edge) {
  VfxEmitter e;
  e.name = "ring";
  e.update = update;
  e.xsize = across;
  e.ysize = deep;
  e.area_shape = shape;
  e.area_edge = edge;
  e.birthrate = 80.0f;
  return e;
}

// A model's emitter blocks in its geometry (before the first newanim): name -> its lines' first word -> the rest.
std::map<std::string, std::map<std::string, std::string>> EmitterBlocks(const std::string& model) {
  std::map<std::string, std::map<std::string, std::string>> out;
  std::istringstream in(model.substr(0, model.find("newanim")));
  std::string line, current;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.rfind("node emitter ", 0) == 0) {
      current = line.substr(13);
      out[current];
    } else if (line == "endnode") {
      current.clear();
    } else if (!current.empty()) {
      const size_t at = line.find_first_not_of(' ');
      if (at == std::string::npos) continue;
      const size_t space = line.find(' ', at);
      if (space != std::string::npos) out[current][line.substr(at, space - at)] = line.substr(space + 1);
    }
  }
  return out;
}

}  // namespace

TEST(VfxAreaTilesOneWhenOneEmitterTakesIt) {
  const std::vector<VfxAreaTile> fits = VfxAreaTiles(AreaEmitter("Fountain", 600, 300, 0, 0));
  CHECK_EQ(fits.size(), 1u);
  CHECK_EQ(Milli(fits[0].width), 600000);
  CHECK_EQ(Milli(fits[0].depth), 300000);
  CHECK_EQ(Milli(fits[0].x), 0);
  CHECK_EQ(VfxAreaTiles(AreaEmitter("Fountain", 650, 650, 0, 0)).size(), 1u);  // the limit itself fits
  // Not a Fountain or an Explosion: its one area, a circle or past the limit as it is.
  CHECK_EQ(VfxAreaTiles(AreaEmitter("Single", 1200, 1200, 1, 0)).size(), 1u);
  CHECK_EQ(VfxAreaTiles(AreaEmitter("Lightning", 1200, 1200, 0, 0)).size(), 1u);
  CHECK_EQ(VfxAreaTiles(AreaEmitter("Fountain", 0, 0, 1, 0)).size(), 1u);  // a circle of no size: a point
}

TEST(VfxAreaTilesGridPastTheLimit) {
  const std::vector<VfxAreaTile> wide = VfxAreaTiles(AreaEmitter("Fountain", 1200, 500, 0, 0));
  CHECK_EQ(wide.size(), 2u);
  CHECK_EQ(Milli(wide[0].x), -300000);
  CHECK_EQ(Milli(wide[1].x), 300000);
  CHECK_EQ(Milli(wide[0].width), 600000);
  CHECK_EQ(Milli(wide[0].depth), 500000);
  CHECK_EQ(Milli(wide[0].y), 0);
  CHECK_EQ(VfxAreaTiles(AreaEmitter("Explosion", 1200, 1200, 0, 0)).size(), 4u);
  CHECK_EQ(VfxAreaTiles(AreaEmitter("Fountain", 651, 100, 0, 0)).size(), 2u);
  CHECK_EQ(VfxAreaTiles(AreaEmitter("Fountain", 3000, 3000, 0, 0)).size(), 4u);  // held to 12 m
}

TEST(VfxAreaTilesCircleMixedSizes) {
  struct Case {
    float across;
    int edge;
    size_t count;
  };
  for (const Case c : {Case{1200, 0, 8}, Case{1200, 1, 16}, Case{600, 0, 5}, Case{600, 1, 9}}) {
    const std::vector<VfxAreaTile> tiles = VfxAreaTiles(AreaEmitter("Fountain", c.across, 0, 1, c.edge));
    CHECK_EQ(tiles.size(), c.count);
    const float r = c.across / 2;
    float area = 0.0f;
    for (size_t i = 0; i < tiles.size(); ++i) {
      const VfxAreaTile& t = tiles[i];
      CHECK_EQ(t.width <= kVfxAreaTileMax + 0.01f && t.depth <= kVfxAreaTileMax + 0.01f, true);
      CHECK_EQ(t.width > 0.0f && t.depth > 0.0f, true);
      // Its outer corner at most a step past the rim: a coarse edge's strip is as wide as the circle at its middle
      // (its corners about 13% out), a fine edge's two about 7%.
      const float fx = std::fabs(t.x) + t.width / 2, fy = std::fabs(t.y) + t.depth / 2;
      CHECK_EQ(std::sqrt(fx * fx + fy * fy) <= r * (c.edge == 1 ? 1.08f : 1.15f), true);
      for (size_t j = i + 1; j < tiles.size(); ++j) CHECK_EQ(Overlap(t, tiles[j]), false);
      area += t.width * t.depth;
    }
    const float disc = 3.14159265f * r * r;
    CHECK_EQ(area > disc * 0.9f && area < disc * 1.1f, true);  // the circle's area, near enough
    // Round: as wide as deep, the same each way out.
    float left = 0, right = 0, up = 0, down = 0;
    for (const VfxAreaTile& t : tiles) {
      left = std::min(left, t.x - t.width / 2);
      right = std::max(right, t.x + t.width / 2);
      down = std::min(down, t.y - t.depth / 2);
      up = std::max(up, t.y + t.depth / 2);
    }
    CHECK_EQ(Milli(right), Milli(-left));
    CHECK_EQ(Milli(up), Milli(-down));
    CHECK_EQ(Milli(right), Milli(up));
  }
  // The 12 m circle's middle: 2 x 2 squares of about 4.24 m; a strip from the square (424) to the rim (600).
  const std::vector<VfxAreaTile> big = VfxAreaTiles(AreaEmitter("Fountain", 1200, 0, 1, 0));
  CHECK_EQ(std::lround(big[0].width), 424);
  CHECK_EQ(std::lround(big[0].x), -212);
  CHECK_EQ(std::lround(big[4].depth), 176);
  CHECK_EQ(std::lround(big[4].y), 512);
  CHECK_EQ(std::lround(big[4].width), 625);  // the circle's width at the strip's middle
}

TEST(VfxModelWritesAreaTilesUnderTheirCarrier) {
  VfxEffect effect;
  VfxEmitter e = AreaEmitter("Fountain", 1200, 0, 1, 0);
  e.position[2] = 0.05f;
  VfxKeys own;  // its own birthrate keys: shared as its rate is
  own.anim = "impact";
  own.birthrate = {0, 80, 1, 40};
  e.keys = {own};
  effect.emitters.push_back(e);
  const std::string model = WriteVfxModel(effect, "n5t");
  const auto blocks = EmitterBlocks(model);
  CHECK_EQ(blocks.size(), 9u);  // the carrier and its 8 pieces
  std::string carrier;
  for (const auto& b : blocks) {
    if (b.second.at("parent") == "n5t") carrier = b.first;
  }
  CHECK_EQ(carrier.empty(), false);
  CHECK_EQ(blocks.at(carrier).at("birthrate") == "0", true);
  CHECK_EQ(blocks.at(carrier).at("position").find("0.05") != std::string::npos, true);  // its place kept
  float rate = 0.0f, area = 0.0f;
  for (const auto& b : blocks) {
    if (b.first == carrier) continue;
    CHECK_EQ(b.second.at("parent") == carrier, true);
    CHECK_EQ(static_cast<float>(atof(b.second.at("xsize").c_str())) <= kVfxAreaTileMax, true);
    rate += static_cast<float>(atof(b.second.at("birthrate").c_str()));
    area += static_cast<float>(atof(b.second.at("xsize").c_str()) * atof(b.second.at("ysize").c_str()));
    // Its middle in metres under the carrier: within the 6 m radius, the 12 m circle's pieces 2.12 m or more out.
    float px = 0, py = 0, pz = 1;
    CHECK_EQ(sscanf(b.second.at("position").c_str(), "%f %f %f", &px, &py, &pz), 3);
    CHECK_EQ(std::fabs(px) <= 6.0f && std::fabs(py) <= 6.0f && pz == 0.0f, true);
    CHECK_EQ(std::max(std::fabs(px), std::fabs(py)) > 2.0f, true);
  }
  CHECK_EQ(std::lround(rate), 80);  // the whole rate, shared
  CHECK_EQ(area > 1.0e6f, true);
  // Its own keys: the carrier's at 0 in the impact.
  const size_t carrier_keys = model.find("node emitter " + carrier, model.find("newanim impact"));
  CHECK_EQ(carrier_keys != std::string::npos, true);
  const std::string keys = model.substr(carrier_keys, model.find("endnode", carrier_keys) - carrier_keys);
  CHECK_EQ(keys.find("    1 0") != std::string::npos, true);
  CHECK_EQ(keys.find("    0 80") == std::string::npos, true);
  // A draft that fits one emitter writes one.
  VfxEffect small;
  small.emitters.push_back(AreaEmitter("Fountain", 300, 300, 0, 0));
  CHECK_EQ(EmitterBlocks(WriteVfxModel(small, "n5t")).size(), 1u);
}

TEST(VfxModelAreaTilesExplosionKeepsWholeParticles) {
  VfxEffect effect;
  VfxEmitter e = AreaEmitter("Explosion", 1200, 0, 1, 1);
  e.birthrate = 30.0f;
  effect.emitters.push_back(e);
  const auto blocks = EmitterBlocks(WriteVfxModel(effect, "n5t"));
  CHECK_EQ(blocks.size(), 17u);
  int total = 0;
  for (const auto& b : blocks) {
    const float rate = static_cast<float>(atof(b.second.at("birthrate").c_str()));
    CHECK_EQ(rate == std::floor(rate), true);  // whole particles
    total += static_cast<int>(rate);
  }
  CHECK_EQ(total, 30);
}

TEST(VfxDraftAreaShapeRoundTrip) {
  VfxEffect effect;
  effect.emitters.push_back(AreaEmitter("Fountain", 1200, 0, 1, 1));
  const std::string draft = WriteVfxDraft(effect);
  CHECK_EQ(draft.find("area_shape = circle\n") != std::string::npos, true);
  CHECK_EQ(draft.find("area_edge = fine\n") != std::string::npos, true);
  VfxEffect back;
  std::string error;
  CHECK_EQ(ReadVfxDraft(draft, &back, &error), true);
  CHECK_EQ(back.emitters.size(), 1u);
  CHECK_EQ(back.emitters[0].area_shape, 1);
  CHECK_EQ(back.emitters[0].area_edge, 1);
  CHECK_EQ(Milli(back.emitters[0].xsize), 1200000);
  // Any other word: a rectangle, a coarse edge.
  std::string square = draft;
  square.replace(square.find("area_shape = circle"), 19, "area_shape = square");
  square.replace(square.find("area_edge = fine"), 16, "area_edge = rough");
  CHECK_EQ(ReadVfxDraft(square, &back, &error) && back.emitters[0].area_shape == 0 && back.emitters[0].area_edge == 0,
           true);
  VfxEffect plain;  // a rectangle: no new lines - the bytes it had
  plain.emitters.push_back(AreaEmitter("Fountain", 300, 200, 0, 0));
  const std::string old = WriteVfxDraft(plain);
  CHECK_EQ(old.find("area_shape") == std::string::npos && old.find("area_edge") == std::string::npos, true);
  CHECK_EQ(ReadVfxDraft(old, &back, &error) && back.emitters[0].area_shape == 0 && back.emitters[0].area_edge == 0,
           true);
}

// --- #179 Phase 3 (the owner, 2026-11-22: "3 start it" - an emitter's colour and size keyable, as its place and turn): an
// emitter's colour at birth and at the end (t r g b) and size at birth and at the end (t s) - through the draft
// ("<anim>_color_start" / "_color_end" / "_size_start" / "_size_end", after KF's lists, only when there), into the model as
// the game's own controllers (colorStartkey, colorEndkey, sizeStartkey, sizeEndkey - nwmain.exe's names, beside
// alphaStartkey's), and kept from a game effect's model by both readers (controllers 108, 96, 168, 172). ---

namespace {

// FlameEffect keyed: in the impact its alphaEnd (KF's), its colour at birth red -> green and its size at the end 0.1 -> 0.4
// over 2 s; in the duration its colour at the end blue and its size at birth 0.2 from 0.25 s.
VfxEffect ColouredFlame() {
  VfxEffect effect = FlameEffect();
  VfxKeys impact;
  impact.anim = "impact";
  impact.alpha_end = {0, 1};
  impact.color_start = {0, 1, 0, 0, 0.5f, 0, 1, 0};
  impact.size_end = {0, 0.1f, 2, 0.4f};
  VfxKeys loop;
  loop.anim = "duration";
  loop.color_end = {0, 0, 0, 1};
  loop.size_start = {0.25f, 0.2f};
  effect.emitters[0].keys = {impact, loop};
  return effect;
}

// A compiled model (built here) whose emitter "flame" keys its colour at birth (108: red -> green over 0.5 s) and its size
// at the end (172: 0.1 -> 0.4 over 1 s) in the impact, and its colour at the end (96: blue) and size at birth (168: 0.3) in
// the duration.
std::string ColouredModel() {
  KfModel m("test_cs");
  const size_t root = m.Node("test_cs", 0x1, {{8, 3, {0}, {0, 0, 0}}});
  const size_t flame = m.Node("flame", 0x5, {{8, 3, {0}, {0, 0, 0.5f}}, {88, 1, {0}, {20}}});
  m.Emitter(flame);
  m.Children(root, {flame});
  m.Put32(KfModel::kBase + 0x48, static_cast<uint32_t>(root - 0x10 - KfModel::kBase));
  {
    const size_t r = m.Node("test_cs", 0x1, {});
    const size_t f = m.Node("flame", 0x5, {{108, 3, {0, 0.5f}, {1, 0, 0, 0, 1, 0}}, {172, 1, {0, 1}, {0.1f, 0.4f}}});
    m.Children(r, {f});
    m.Anim("impact", 1.0f, 0.25f, r);
  }
  {
    const size_t r = m.Node("test_cs", 0x1, {});
    const size_t f = m.Node("flame", 0x5, {{96, 3, {0}, {0, 0, 1}}, {168, 1, {0}, {0.3f}}});
    m.Children(r, {f});
    m.Anim("duration", 1.0f, 0.25f, r);
  }
  m.List();
  return m.b;
}

}  // namespace

// The draft: the four lists after KF's (its alphaEnd's), in each animation, only when there - read back as written. In any
// section, as every key list: a node's read and written back, never reaching the model.
TEST(VfxDraftKeepsAnEmittersColourAndSizeKeys) {
  const std::string draft = WriteVfxDraft(ColouredFlame());
  CHECK_EQ(Contains(draft, "\nflags =\nimpact_alpha_end = 0 1\nimpact_color_start = 0 1 0 0 0.5 0 1 0\nimpact_size_end = 0 0.1 2 0.4\n"
                           "duration_color_end = 0 0 0 1\nduration_size_start = 0.25 0.2\n"),
           true);
  VfxEffect back;
  std::string error;
  CHECK_EQ(ReadVfxDraft(draft, &back, &error) && WriteVfxDraft(back) == draft, true);
  const VfxKeys* impact = back.emitters.empty() ? nullptr : KeysOf(back.emitters[0].keys, "impact");
  const VfxKeys* loop = back.emitters.empty() ? nullptr : KeysOf(back.emitters[0].keys, "duration");
  CHECK_EQ(impact && Numbers(impact->color_start) == "0 1 0 0 0.5 0 1 0" && Numbers(impact->size_end) == "0 0.1 2 0.4" &&
               impact->color_end.empty() && impact->size_start.empty(),
           true);
  CHECK_EQ(loop && Numbers(loop->color_end) == "0 0 0 1" && Numbers(loop->size_start) == "0.25 0.2" && loop->color_start.empty(), true);
  VfxEffect node;
  CHECK_EQ(ReadVfxDraft("name = x\n[node hub]\nimpact_color_start = 0 1 1 1\nimpact_size_end = 0 2\n", &node, &error) && node.nodes.size() == 1,
           true);
  CHECK_EQ(Contains(WriteVfxDraft(node), "\norientation = 0 0 0 0\nimpact_color_start = 0 1 1 1\nimpact_size_end = 0 2\n"), true);
  const std::string node_model = WriteVfxModel(node, "n5t");
  CHECK_EQ(Contains(node_model, "colorStartkey") || Contains(node_model, "sizeEndkey"), false);
}

// The model: the emitter's colorStartkey, colorEndkey, sizeStartkey and sizeEndkey after its alphaStartkey / alphaEndkey,
// in each animation that keys them (t r g b, t s rows); nwn5e's own emission keys as before; the geometry as before; the
// animations last them; a list without a whole row writes nothing.
TEST(VfxModelWritesAnEmittersColourAndSizeKeys) {
  const std::string before = WriteVfxModel(FlameEffect(), "n5t");
  const std::string model = WriteVfxModel(ColouredFlame(), "n5t");
  CHECK_EQ(EmitterIn(model, "impact", "n5t_0_flame") ==
               "node emitter n5t_0_flame\n  parent n5t\n  birthratekey 1\n    0 100\n  alphaEndkey 1\n    0 1\n"
               "  colorStartkey 2\n    0 1 0 0\n    0.5 0 1 0\n  sizeEndkey 2\n    0 0.1\n    2 0.4\n",
           true);
  CHECK_EQ(EmitterIn(model, "duration", "n5t_0_flame") ==
               "node emitter n5t_0_flame\n  parent n5t\n  birthratekey 1\n    0 100\n  colorEndkey 1\n    0 0 0 1\n  sizeStartkey 1\n    0.25 0.2\n",
           true);
  CHECK_EQ(EmitterIn(model, "cessation", "n5t_0_flame") == EmitterIn(before, "cessation", "n5t_0_flame"), true);
  const auto geometry = [](const std::string& m) { return Section(m, "beginmodelgeom n5t", "endmodelgeom n5t"); };
  CHECK_EQ(!geometry(model).empty() && geometry(model) == geometry(before), true);
  CHECK_EQ(Milli(AnimLength(model, "impact")), 2000);  // its size key at 2 s
  // Each kind's own: colour at the end and size at birth in the impact, a colour key at 3 s in the cessation.
  VfxEffect other = FlameEffect();
  VfxKeys a, c;
  a.anim = "impact";
  a.color_end = {0, 0.5f, 0.5f, 0.5f};
  a.size_start = {0, 1};
  c.anim = "cessation";
  c.color_start = {0, 1, 1, 1, 3, 0, 0, 0};
  other.emitters[0].keys = {a, c};
  const std::string other_model = WriteVfxModel(other, "n5t");
  CHECK_EQ(Contains(EmitterIn(other_model, "impact", "n5t_0_flame"), "  colorEndkey 1\n    0 0.5 0.5 0.5\n  sizeStartkey 1\n    0 1\n"), true);
  CHECK_EQ(Contains(EmitterIn(other_model, "impact", "n5t_0_flame"), "colorStartkey") || Contains(EmitterIn(other_model, "impact", "n5t_0_flame"), "sizeEndkey"),
           false);
  CHECK_EQ(Contains(EmitterIn(other_model, "cessation", "n5t_0_flame"), "  colorStartkey 2\n    0 1 1 1\n    3 0 0 0\n"), true);
  CHECK_EQ(Milli(AnimLength(other_model, "cessation")), 3000);
  // Not a whole row (a colour of three numbers, a size of one): nothing - the model as without them.
  VfxEffect few = FlameEffect();
  VfxKeys f;
  f.anim = "impact";
  f.color_start = {0, 1, 0};
  f.size_end = {0.5f};
  few.emitters[0].keys = {f};
  CHECK_EQ(WriteVfxModel(few, "n5t") == before, true);
  // Of each kind the first list in the animation with a whole row, as KF's: a short one before a whole one - the whole one.
  VfxEffect two = FlameEffect();
  VfxKeys g;
  g.anim = "impact";
  g.color_start = {0, 0, 1, 0};
  two.emitters[0].keys = {f, g};
  CHECK_EQ(Contains(EmitterIn(WriteVfxModel(two, "n5t"), "impact", "n5t_0_flame"), "  colorStartkey 1\n    0 0 1 0\n"), true);
}

// The readers: a game effect's compiled model keeps its emitter's colour and size keys (108, 96, 168, 172) in each
// animation, as read - the draft carries them and the model written has them; that model read as a game's ASCII model (its
// nwn5e line out) keeps them too, and writes the same model again; written by hand (an endlist list, another case) as well.
// nwn5e's own model (its line in) keeps none: its draft is the source.
TEST(VfxReadsTheGamesColourAndSizeKeys) {
  VfxEffect effect;
  std::string error;
  CHECK_EQ(ReadVfxBinaryModel(ColouredModel(), &effect, &error) && effect.emitters.size() == 1, true);
  if (effect.emitters.size() != 1) return;
  const VfxKeys* impact = KeysOf(effect.emitters[0].keys, "impact");
  const VfxKeys* loop = KeysOf(effect.emitters[0].keys, "duration");
  CHECK_EQ(impact && Numbers(impact->color_start) == "0 1 0 0 0.5 0 1 0" && Numbers(impact->size_end) == "0 0.1 1 0.4" &&
               impact->color_end.empty() && impact->size_start.empty(),
           true);
  CHECK_EQ(loop && Numbers(loop->color_end) == "0 0 0 1" && Numbers(loop->size_start) == "0 0.3", true);
  CHECK_EQ(Contains(WriteVfxDraft(effect), "\nimpact_color_start = 0 1 0 0 0.5 0 1 0\nimpact_size_end = 0 0.1 1 0.4\n"), true);
  const std::string model = WriteVfxModel(effect, "n5t");
  CHECK_EQ(Contains(model, "  colorStartkey 2\n    0 1 0 0\n    0.5 0 1 0\n  sizeEndkey 2\n    0 0.1\n    1 0.4\n") &&
               Contains(model, "  colorEndkey 1\n    0 0 0 1\n  sizeStartkey 1\n    0 0.3\n"),
           true);
  const size_t mark = model.find(std::string("\n") + kVfxModelMark);
  CHECK_EQ(mark != std::string::npos, true);
  if (mark == std::string::npos) return;
  const std::string as_game = model.substr(0, mark) + model.substr(model.find('\n', mark + 1));
  VfxEffect back;
  CHECK_EQ(ReadVfxAsciiModel(as_game, &back, &error) && back.emitters.size() == 1, true);
  if (back.emitters.size() != 1) return;
  const VfxKeys* again = KeysOf(back.emitters[0].keys, "impact");
  CHECK_EQ(again && Numbers(again->color_start) == "0 1 0 0 0.5 0 1 0" && Numbers(again->size_end) == "0 0.1 1 0.4", true);
  CHECK_EQ(KeysOf(back.emitters[0].keys, "duration") && Numbers(KeysOf(back.emitters[0].keys, "duration")->size_start) == "0 0.3", true);
  back.name = effect.name;
  back.emitters[0].name = effect.emitters[0].name;
  CHECK_EQ(WriteVfxModel(back, "n5t") == model, true);
  // nwn5e's own (its line in): none kept.
  VfxEffect own;
  CHECK_EQ(ReadVfxAsciiModel(model, &own, &error) && NoneOfTheGamesKeys(own), true);
  // By hand: an endlist list, the game's spelling and another case.
  const std::string text =
      "newmodel hand\nbeginmodelgeom hand\nnode dummy hand\n  parent NULL\nendnode\nnode emitter spark\n  parent hand\n"
      "  update Fountain\n  birthrate 10\n  sizeStart 0.2\nendnode\nendmodelgeom hand\n"
      "newanim impact hand\n  length 1\n  transtime 0.25\n  animroot hand\nnode emitter spark\n  parent hand\n"
      "  colorStartkey\n    0 1 1 1\n    1 1 0 0\n  endlist\n  COLORENDKEY 1\n    0 0 0 0\n  sizeStartkey 2\n    0 0.2\n    1 0.6\n"
      "  sizeEndkey 1\n    0 0.05\nendnode\ndoneanim impact hand\ndonemodel hand\n";
  VfxEffect hand;
  CHECK_EQ(ReadVfxAsciiModel(text, &hand, &error) && hand.emitters.size() == 1, true);
  if (hand.emitters.size() != 1) return;
  const VfxKeys* spark = KeysOf(hand.emitters[0].keys, "impact");
  CHECK_EQ(spark && Numbers(spark->color_start) == "0 1 1 1 1 1 0 0" && Numbers(spark->color_end) == "0 0 0 0" &&
               Numbers(spark->size_start) == "0 0.2 1 0.6" && Numbers(spark->size_end) == "0 0.05",
           true);
  CHECK_EQ(Milli(hand.emitters[0].size_start), 200);  // its geometry's, as before
}

// --- DE-1 (#172; the owner, 2026-11-22: "DE-1 shows with fountain ... Sometiems if I press play in game many times it might
// go through"): the debris effects' bursts threw nothing in game. An Explosion bursts int(birthrate) particles when its
// detonate rises, and with int(birthrate) 0 the detonate is spent on none (ExplosionEmitter::Update 0x1400647c0: the count
// read first, 0 -> straight to the end, detonate cleared); every float controller is blended in from the field's value
// over the animation's transtime (MdlNode::InternalControl 0x140051c50: field = (1 - w) field + w key), an effect's impact
// from its geometry - so a burst with birthrate 0 there and its count keyed on in the impact had only w x its count when the
// impact's detonate at 0 came (at 60 fps 0.67 of 10: none; a slow first frame a few). The game's own 356 bursts all carry
// their count in the geometry. Now a burst the impact's detonate at 0 sets off carries its count there too; a late one keeps
// 0 (the detonate at 0 mustn't set it off) and so does every other keyed emitter (a Fountain loses nothing to the blend). ---

namespace {

std::string GeometryOf(const std::string& model, const std::string& emitter) {
  return Section(Section(model, "beginmodelgeom n5t", "endmodelgeom n5t"), "node emitter n5t_" + emitter, "endnode");
}

}  // namespace

TEST(VfxBurstAtTheStartCarriesItsCountInItsGeometry) {
  VfxEffect effect;
  effect.duration = false;
  effect.seconds = 2.0f;
  VfxEmitter bang;
  bang.name = "bang";
  bang.update = "Explosion";
  bang.birthrate = 30.0f;
  effect.emitters.push_back(bang);
  std::string model = WriteVfxModel(effect, "n5t");
  CHECK_EQ(Contains(GeometryOf(model, "0_bang"), "  birthrate 30\n"), true);
  CHECK_EQ(Contains(Section(model, "newanim impact n5t", "doneanim impact n5t"), "  birthratekey 3\n    0 30\n    2 30\n    2.01 0\n"), true);
  // With a late burst: the first still carries its count (its keys round the detonate at 0 start at it), the late one 0.
  VfxEmitter echo = bang;
  echo.name = "echo";
  echo.birthrate = 12.0f;
  echo.start_seconds = 0.5f;
  effect.emitters.push_back(echo);
  model = WriteVfxModel(effect, "n5t");
  CHECK_EQ(Contains(GeometryOf(model, "0_bang"), "  birthrate 30\n"), true);
  CHECK_EQ(Contains(GeometryOf(model, "1_echo"), "  birthrate 0\n"), true);
  // A keyed Fountain: 0, as before.
  VfxEmitter spray = bang;
  spray.name = "spray";
  spray.update = "Fountain";
  effect.emitters = {spray};
  CHECK_EQ(Contains(GeometryOf(WriteVfxModel(effect, "n5t"), "0_spray"), "  birthrate 0\n"), true);
  // A lasting effect: a burst on for a while (its On for) carries its count; one starting late 0; one always on as ever.
  VfxEffect lasting = effect;
  lasting.duration = true;
  VfxEmitter puff = bang;
  puff.name = "puff";
  puff.emit_seconds = 0.5f;
  VfxEmitter late = bang;
  late.name = "late";
  late.start_seconds = 0.3f;
  late.birthrate = 7.0f;
  VfxEmitter always = bang;
  always.name = "always";
  always.birthrate = 5.0f;
  lasting.emitters = {puff, late, always};
  model = WriteVfxModel(lasting, "n5t");
  CHECK_EQ(Contains(GeometryOf(model, "0_puff"), "  birthrate 30\n"), true);
  CHECK_EQ(Contains(GeometryOf(model, "1_late"), "  birthrate 0\n"), true);
  CHECK_EQ(Contains(GeometryOf(model, "2_always"), "  birthrate 5\n"), true);
  // A game effect's burst with its own birthrate keys: its own geometry's, as read.
  VfxEmitter own = bang;
  own.name = "own";
  own.birthrate = 4.0f;
  VfxKeys k;
  k.anim = "impact";
  k.birthrate = {0, 0, 0.4f, 9};
  own.keys = {k};
  effect.emitters = {own};
  model = WriteVfxModel(effect, "n5t");
  CHECK_EQ(Contains(GeometryOf(model, "0_own"), "  birthrate 4\n"), true);
}
