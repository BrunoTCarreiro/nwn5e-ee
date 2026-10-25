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
