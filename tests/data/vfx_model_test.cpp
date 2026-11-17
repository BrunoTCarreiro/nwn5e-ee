#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <set>
#include <sstream>
#include <string>

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
