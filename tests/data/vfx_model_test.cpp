#include <string.h>

#include <fstream>
#include <sstream>
#include <string>

#include "check.h"
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
  CHECK_EQ(ReadVfxDraft("name = x\n[weird]\n", &back, &error), false);
  // Deadspace past the most the game's loop survives is held there (0.42.4).
  CHECK_EQ(ReadVfxDraft("name = x\n[emitter a]\ndeadspace = 0.6\n", &back, &error), true);
  if (!back.emitters.empty()) CHECK_EQ(Milli(back.emitters[0].deadspace), 390);
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
