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
  CHECK_EQ(Milli(e.emit_seconds), 500);
  CHECK_EQ(Milli(e.size_end), 800);
  CHECK_EQ(Milli(e.color_start[0]), 330);
  CHECK_EQ(Milli(e.xsize), 12000);
  CHECK_EQ(e.grid, 2);
  CHECK_EQ(e.frame_end, 3);
  CHECK_EQ(e.splat, true);
  CHECK_EQ(e.random, true);
  CHECK_EQ(e.inherit, false);
  CHECK_EQ(ReadVfxDraft("name = x\n[weird]\n", &back, &error), false);
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
  CHECK_EQ(Milli(e.birthrate), 150000);
  CHECK_EQ(Milli(e.color_start[0]), 300);
  CHECK_EQ(Milli(e.color_start[1]), 100);
  CHECK_EQ(ReadVfxBinaryModel("newmodel x", &effect, &error), false);
}

// A lasting effect read from a compiled model laid out as the game's (0.42.0;
// vdr_eleshield): its animations' headers, then their copies of the emitter
// nodes. Breaks caught: the cessation's switch-off (every lasting game effect
// has one) read as "on for 0.03 s" - the VFX Bench showed the elemental shield
// dying after 2.6 s; a lasting effect's impact burst (an emitter otherwise off)
// losing its on-time; a copy given to the wrong animation.
TEST(VfxLastingEffectKeepsEmitting) {
  std::string b(0x1000, '\0');
  const auto put32 = [&](size_t at, uint32_t v) { memcpy(&b[at], &v, 4); };
  const auto put16 = [&](size_t at, uint16_t v) { memcpy(&b[at], &v, 2); };
  const auto putf = [&](size_t at, float v) { memcpy(&b[at], &v, 4); };
  const auto puts = [&](size_t at, const char* text) { memcpy(&b[at], text, strlen(text)); };
  const size_t base = 12;
  // A node named `name` with one birthrate controller (one column).
  const auto node = [&](size_t at, const char* name, size_t keys, size_t data, const std::vector<float>& times,
                        const std::vector<float>& values) {
    puts(at + 0x10, name);
    put32(at + 0x44, static_cast<uint32_t>(keys - base));
    put32(at + 0x48, 1);
    put32(at + 0x50, static_cast<uint32_t>(data - base));
    put32(at + 0x54, static_cast<uint32_t>(times.size() * 2));
    put32(keys, 88);
    put16(keys + 4, static_cast<uint16_t>(times.size()));
    put16(keys + 6, 0);
    put16(keys + 8, static_cast<uint16_t>(times.size()));
    b[keys + 10] = 1;
    for (size_t i = 0; i < times.size(); ++i) {
      putf(data + 4 * i, times[i]);
      putf(data + 4 * (times.size() + i), values[i]);
    }
  };
  const auto anim = [&](size_t header, const char* name) {
    puts(header + 8, name);
    putf(header + 0x70, 1.0f);
  };
  const auto emitter = [&](size_t at) {
    puts(at + 0x60 + 0x18, "Fountain");
    puts(at + 0x60 + 0x38, "Normal");
    puts(at + 0x60 + 0x78, "fxpa_sphere");
  };
  puts(base + 8, "test_shield");
  anim(0x100, "impact");
  node(0x200, "flash", 0xC00, 0xC10, {0.0f, 0.2f, 0.21f}, {30.0f, 30.0f, 0.0f});  // a burst at the start
  node(0x300, "ring", 0xC40, 0xC50, {0.0f, 0.5f, 0.51f}, {50.0f, 50.0f, 0.0f});  // on again in the duration
  anim(0x400, "duration");
  node(0x480, "ring", 0xC80, 0xC90, {0.0f, 1.0f}, {50.0f, 50.0f});
  anim(0x500, "cessation");
  node(0x600, "ring", 0xCC0, 0xCD0, {0.0f, 0.033f}, {50.0f, 0.0f});  // how it ends
  node(0x700, "flash", 0xD00, 0xD10, {0.0f, 0.033f}, {30.0f, 0.0f});
  node(0x800, "ring", 0xD40, 0xD50, {0.0f}, {50.0f});  // the geometry: on
  emitter(0x800);
  node(0xA00, "flash", 0xD80, 0xD90, {0.0f}, {0.0f});  // the geometry: off
  emitter(0xA00);
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
  CHECK_EQ(Milli(ring->birthrate), 50000);
  CHECK_EQ(Milli(ring->emit_seconds), 0);
  CHECK_EQ(Milli(flash->birthrate), 30000);
  CHECK_EQ(Milli(flash->emit_seconds), 200);
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
