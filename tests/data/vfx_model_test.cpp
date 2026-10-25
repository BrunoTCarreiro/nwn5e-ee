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
