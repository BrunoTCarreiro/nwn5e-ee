#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "core/anim_destruction.h"
#include "data/vfx_model.h"

// DE (#172; the owner, 2026-11-20: "yes on all", after the destruction recon): consistent destruction effects - debris
// matched to a breakable's material (wood, stone, metal, cloth, glass; small and large) for a model with no debris of its
// own, and a lit light's death (ember sparks, a short rising smoke column, a warm light fading out). The game session's
// call (core/anim_destruction.h): its mapping and bounds, the rows file naming the models (one source of truth with
// data::kDebrisFirstRow), the drafts writing the committed models, and the models as the game and the VFX Bench's nwn5e
// tab read them - the game's own chunk models thrown by name, one-shot effects with nothing left behind.

namespace {

std::string ReadText(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

// A text file without its CRs (git's autocrlf checks the committed LF files out with CRLF).
std::string ReadLf(const std::string& path) {
  std::string text;
  for (char c : ReadText(path)) {
    if (c != '\r') text += c;
  }
  return text;
}

std::vector<std::string> Words(const std::string& line) {
  std::istringstream in(line);
  std::vector<std::string> words;
  for (std::string word; in >> word;) words.push_back(word);
  return words;
}

bool Near(float a, float b, float tolerance = 1e-4f) { return fabsf(a - b) <= tolerance; }

// A model's events: its one "event 0 detonate" and nothing else - every burst at once (the model writer adds an event for
// each burst that starts late; ReadVfxAsciiModel doesn't give such a start back).
bool OneDetonateAtZero(const std::string& model) {
  size_t events = 0;
  for (size_t at = model.find("\n  event "); at != std::string::npos; at = model.find("\n  event ", at + 1)) ++events;
  return events == 1 && model.find("\n  event 0 detonate\n") != std::string::npos;
}

const DebrisMaterial kMaterials[] = {DebrisMaterial::Wood, DebrisMaterial::Stone, DebrisMaterial::Metal, DebrisMaterial::Cloth,
                                     DebrisMaterial::Glass};
const DebrisSize kSizes[] = {DebrisSize::Small, DebrisSize::Large};

// Every model the call names: the ten debris models, then the light's death.
std::vector<std::string> AllModels() {
  std::vector<std::string> models;
  for (int m = 0; m < 5; ++m) {
    for (int s = 0; s < 2; ++s) models.push_back(data::DebrisModel(m, s));
  }
  models.push_back(data::LightDeathModel());
  return models;
}

std::string Upper(std::string text) {
  for (char& c : text) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
  return text;
}

// The game's chunk models each material's debris throws (by name; nothing of the game's is kept in nwn5e), small and
// large - read from the game's own "die" debris: the bench PLC_X01 and the bookshelf (wood), the sarcophagus PLC_B10
// and VFX_COM_CHUNK_STONE's vwp_m_chunk_rock (stone), the brazier PLC_I05 (metal), the sacks PLC_L03 (cloth), the
// bottles dag_btl* and the crystal ball px2_d01 (glass).
std::set<std::string> ChunksOf(int material, int size) {
  switch (material) {
    case 0: return {"plc_chunk_w01", "plc_chunk_w02"};
    case 1: return size == 0 ? std::set<std::string>{"plc_chunk_s01", "fx_rockchunksml"}
                             : std::set<std::string>{"plc_chunk_s01", "fx_rockchunksml", "plc_chunk_s02", "fx_rockchunkmed"};
    case 2: return {"plc_chunk_m01"};
    case 3: return {"plc_chunk_m01"};
    case 4: return size == 0 ? std::set<std::string>{"dag_chunk_g01"} : std::set<std::string>{"dag_chunk_g01", "px2_gls"};
  }
  return {};
}

}  // namespace

// The agreed call: material m, size s at kDebrisFirstRow + 2 m + s (2248 wood small ... 2257 glass large), the light's
// death after them (2258).
TEST(DestructionDebrisVfxMapsEachMaterialAndSize) {
  CHECK_EQ(data::kDebrisFirstRow, 2248);
  CHECK_EQ(data::kLightDeathRow, 2258);
  int expected = 2248;
  for (DebrisMaterial material : kMaterials) {
    for (DebrisSize size : kSizes) {
      CHECK_EQ(DestructionDebrisVfx(material, size), expected);
      CHECK_EQ(data::DebrisRow(static_cast<int>(material), static_cast<int>(size)), expected);
      ++expected;
    }
  }
  CHECK_EQ(DestructionDebrisVfx(DebrisMaterial::Wood, DebrisSize::Small), 2248);
  CHECK_EQ(DestructionDebrisVfx(DebrisMaterial::Metal, DebrisSize::Large), 2253);
  CHECK_EQ(DestructionDebrisVfx(DebrisMaterial::Glass, DebrisSize::Large), 2257);
  CHECK_EQ(LightDeathVfx(), 2258);
}

// Anything the enums don't name: -1 (the game session then plays nothing).
TEST(DestructionDebrisVfxRefusesOthers) {
  for (int material : {-1, 5, 6, 100, INT_MIN, INT_MAX}) {
    for (DebrisSize size : kSizes) {
      CHECK_EQ(DestructionDebrisVfx(static_cast<DebrisMaterial>(material), size), -1);
      CHECK_EQ(data::DebrisRow(material, static_cast<int>(size)), -1);
    }
  }
  for (int size : {-1, 2, 3, INT_MIN, INT_MAX}) {
    for (DebrisMaterial material : kMaterials) {
      CHECK_EQ(DestructionDebrisVfx(material, static_cast<DebrisSize>(size)), -1);
      CHECK_EQ(data::DebrisRow(static_cast<int>(material), size), -1);
    }
  }
}

// The models: n5_db_<material>_<s|l> and n5_db_light - nwn5e's own (n5_: the bench's nwn5e tab lists served n5_*.mdl
// with emitters), a resref each (16 letters at most).
TEST(DestructionDebrisNames) {
  CHECK_EQ(data::DebrisModel(0, 0) == "n5_db_wood_s", true);
  CHECK_EQ(data::DebrisModel(0, 1) == "n5_db_wood_l", true);
  CHECK_EQ(data::DebrisModel(1, 0) == "n5_db_stone_s", true);
  CHECK_EQ(data::DebrisModel(2, 1) == "n5_db_metal_l", true);
  CHECK_EQ(data::DebrisModel(3, 0) == "n5_db_cloth_s", true);
  CHECK_EQ(data::DebrisModel(4, 1) == "n5_db_glass_l", true);
  CHECK_EQ(data::LightDeathModel() == "n5_db_light", true);
  CHECK_EQ(data::DebrisMaterialCode(0) == "wood" && data::DebrisMaterialCode(4) == "glass", true);
  for (int bad : {-1, 5, INT_MIN, INT_MAX}) {
    CHECK_EQ(data::DebrisMaterialCode(bad).empty(), true);
    CHECK_EQ(data::DebrisModel(bad, 0).empty(), true);
  }
  for (int bad : {-1, 2, INT_MIN}) CHECK_EQ(data::DebrisModel(0, bad).empty(), true);
  std::set<std::string> seen;
  for (const std::string& model : AllModels()) {
    CHECK_EQ(model.size() <= 16 && model.rfind("n5_", 0) == 0, true);
    CHECK_EQ(data::VfxResRef(model) == model, true);
    seen.insert(model);
  }
  CHECK_EQ(seen.size(), 11u);
}

// data/rows/visualeffects.debris.txt: exactly the 11 rows the call gives, each a one-shot effect (F) naming its model in
// all four root columns (whichever the client picks - on an object by its size - it is this one), flat
// (OrientWithGround 0: the client casts no ground ray, so a location's height holds), its label the material and size;
// each model in data/files, its draft in tools/data/vfx.
TEST(DestructionDebrisRowsFileNamesTheModels) {
  std::istringstream lines(ReadText("data/rows/visualeffects.debris.txt"));
  std::vector<std::string> header;
  int rows = 0;
  std::set<int> seen;
  for (std::string line; std::getline(lines, line);) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const std::vector<std::string> w = Words(line);
    if (w.empty() || w[0][0] == '#') continue;
    if (header.empty()) {
      header = w;
      continue;
    }
    ++rows;
    CHECK_EQ(w.size(), header.size() + 1);
    if (w.size() != header.size() + 1) continue;
    const int row = atoi(w[0].c_str());
    seen.insert(row);
    std::string model, label;
    if (row == data::kLightDeathRow) {
      model = data::LightDeathModel();
      label = "NWN5E_LIGHT_DEATH";
      CHECK_EQ(LightDeathVfx(), row);
    } else {
      const int index = row - data::kDebrisFirstRow;
      CHECK_EQ(index >= 0 && index < 10, true);
      if (index < 0 || index >= 10) continue;
      const int m = index / 2, s = index % 2;
      model = data::DebrisModel(m, s);
      label = "NWN5E_DEBRIS_" + Upper(data::DebrisMaterialCode(m)) + (s ? "_L" : "_S");
      CHECK_EQ(DestructionDebrisVfx(kMaterials[m], kSizes[s]), row);
    }
    for (size_t c = 0; c < header.size(); ++c) {
      const std::string& column = header[c];
      const std::string& value = w[c + 1];
      if (column == "Label") {
        CHECK_EQ(value == label, true);
      } else if (column == "Type_FD") {
        CHECK_EQ(value == "F", true);
      } else if (column == "OrientWithGround") {
        CHECK_EQ(value == "0", true);
      } else if (column == "Imp_Root_S_Node" || column == "Imp_Root_M_Node" || column == "Imp_Root_L_Node" ||
                 column == "Imp_Root_H_Node") {
        CHECK_EQ(value == model, true);
      } else {
        CHECK_EQ(column == "an unexpected column", true);
      }
    }
    CHECK_EQ(std::filesystem::exists("data/files/" + model + ".mdl"), true);
    CHECK_EQ(std::filesystem::exists("tools/data/vfx/" + model + ".vfx"), true);
  }
  CHECK_EQ(rows, 11);
  CHECK_EQ(header.size(), 7u);
  CHECK_EQ(seen.size() == 11 && *seen.begin() == 2248 && *seen.rbegin() == 2258, true);
}

// Each draft (tools/data/vfx, written by tools/data/debris/build.js through the VFX Bench's own draft writer) reads and
// writes back its own bytes, and writes the committed model (data/files) through the game's model writer - the
// detect-magic way: the draft the bench opens, the model the game plays.
TEST(DestructionDebrisDraftsWriteTheModels) {
  for (const std::string& name : AllModels()) {
    const std::string text = ReadLf("tools/data/vfx/" + name + ".vfx");
    data::VfxEffect effect;
    std::string error;
    CHECK_EQ(!text.empty() && data::ReadVfxDraft(text, &effect, &error), true);
    CHECK_EQ(data::WriteVfxDraft(effect) == text, true);
    CHECK_EQ(effect.name == name && !effect.duration && effect.attach == "root", true);
    const std::string model = ReadLf("data/files/" + name + ".mdl");
    CHECK_EQ(!model.empty() && data::WriteVfxModel(effect, name) == model, true);
  }
}

// The debris models as the dev layer reads them for the bench (ReadVfxAsciiModel) and the game plays them: a one-shot
// (an impact, no duration or cessation - nothing stays or lingers after the effect), each burst on the impact's detonate
// at 0; every chunk emitter an Explosion throwing one of its material's game chunk models (all of them thrown), at the
// game's own "die" recipe (life 0.75 s, velocity 8, mass 3, bouncing at 0.5 - fx_rockchunk*'s their effect's: life 4);
// a smoke puff (fxpa_smoke01) for every material; metal's own sparks (the game's "spark", added light); glass's glint;
// no light; the large one throwing more, from higher up than the small one's highest emitter.
TEST(DestructionDebrisModelsThrowTheGamesChunks) {
  for (int m = 0; m < 5; ++m) {
    int particles[2] = {0, 0};
    float top[2] = {0.0f, 0.0f};
    for (int s = 0; s < 2; ++s) {
      const std::string name = data::DebrisModel(m, s);
      const std::string text = ReadLf("data/files/" + name + ".mdl");
      data::VfxEffect effect;
      std::string error;
      CHECK_EQ(data::ReadVfxAsciiModel(text, &effect, &error) && !effect.emitters.empty(), true);
      CHECK_EQ(text.find("\nnewanim impact " + name + "\n") != std::string::npos, true);
      CHECK_EQ(OneDetonateAtZero(text), true);
      CHECK_EQ(text.find("newanim duration") == std::string::npos && text.find("newanim cessation") == std::string::npos, true);
      CHECK_EQ(effect.lights.empty() && effect.planes.empty(), true);
      std::set<std::string> thrown;
      bool puff = false, spark = false, glint = false;
      for (const data::VfxEmitter& e : effect.emitters) {
        top[s] = std::max(top[s], e.position[2]);
        CHECK_EQ(e.update == "Explosion" && e.start_seconds == 0.0f, true);
        if (!e.chunk.empty()) {
          thrown.insert(e.chunk);
          particles[s] += static_cast<int>(e.birthrate);
          CHECK_EQ(ChunksOf(m, s).count(e.chunk), 1u);
          CHECK_EQ(e.bounce && e.birthrate >= 1.0f, true);
          if (e.chunk.rfind("fx_rockchunk", 0) == 0) {
            CHECK_EQ(Near(e.life, 4.0f) && e.velocity >= 3.0f, true);
          } else {
            CHECK_EQ(Near(e.life, 0.75f) && Near(e.velocity, 8.0f) && Near(e.mass, 3.0f) && Near(e.bounce_co, 0.5f), true);
          }
        } else if (e.texture == "fxpa_smoke01") {
          puff = true;
          CHECK_EQ(e.blend == "Normal" && Near(e.alpha_end, 0.0f) && Near(e.life, 2.5f), true);
        } else if (e.texture == "spark") {
          spark = true;
          CHECK_EQ(e.blend == "Lighten" && e.render == "Motion_Blur" && e.bounce, true);
        } else if (e.texture == "fxpa_strk_b") {
          glint = true;
          CHECK_EQ(e.blend == "Lighten", true);
        } else {
          CHECK_EQ(e.texture == "an unexpected emitter", true);
        }
      }
      CHECK_EQ(thrown == ChunksOf(m, s), true);
      CHECK_EQ(puff, true);
      CHECK_EQ(spark, m == 2);
      CHECK_EQ(glint, m == 4);
    }
    CHECK_EQ(particles[1] > particles[0], true);
    CHECK_EQ(top[1] >= top[0] + 0.4f, true);
  }
}

// The light's death (n5_db_light), as the game plays it where the light was (its own height): ember sparks - a burst
// and rising embers - and a short smoke column (fxpa_smoke01 rising from it, on for at least 1.5 s), and a light that
// starts warm (red over green over blue, as bright as the game's flame light) and fades to black by about 1.5 s,
// never brightening on the way; it comes on at once (fadingLight 0: the engine's own fade-in would leave a gap after the
// placeable's light is cut) and shines on its surroundings (not ambient only). No chunk: the light's own model throws
// its debris; a one-shot, nothing left behind.
TEST(LightDeathEmbersSmokeAndAFadingGlow) {
  const std::string name = data::LightDeathModel();
  const std::string text = ReadLf("data/files/" + name + ".mdl");
  data::VfxEffect effect;
  std::string error;
  CHECK_EQ(data::ReadVfxAsciiModel(text, &effect, &error), true);
  CHECK_EQ(text.find("newanim duration") == std::string::npos && text.find("newanim cessation") == std::string::npos, true);
  CHECK_EQ(OneDetonateAtZero(text), true);
  CHECK_EQ(text.find("\n  fadingLight 0\n") != std::string::npos && text.find("\n  ambientonly 0\n") != std::string::npos, true);
  bool burst = false, embers = false, smoke = false;
  for (const data::VfxEmitter& e : effect.emitters) {
    CHECK_EQ(e.chunk.empty(), true);
    if (e.update == "Explosion" && e.blend == "Lighten") burst = true;
    if (e.update == "Fountain" && e.blend == "Lighten" && e.mass < 0.0f && e.color_start[0] >= e.color_start[2] &&
        e.color_end[0] > e.color_end[2] && e.emit_seconds >= 1.0f) {
      embers = true;
    }
    if (e.update == "Fountain" && e.texture == "fxpa_smoke01" && e.velocity > 0.0f && e.emit_seconds >= 1.5f && e.life >= 2.0f) {
      smoke = true;
    }
  }
  CHECK_EQ(burst && embers && smoke, true);
  CHECK_EQ(effect.lights.size(), 1u);
  if (effect.lights.size() != 1) return;
  const data::VfxLight& light = effect.lights[0];
  CHECK_EQ(light.color[0] >= light.color[1] && light.color[1] >= light.color[2] && light.color[0] > 0.9f, true);
  CHECK_EQ(!light.fading && !light.ambient_only && light.radius >= 4.0f, true);
  const data::VfxKeys* keys = nullptr;
  for (const data::VfxKeys& k : light.keys) {
    if (k.anim == "impact") keys = &k;
  }
  CHECK_EQ(keys != nullptr && keys->color.size() >= 8 && keys->color.size() % 4 == 0, true);
  if (!keys || keys->color.size() < 8) return;
  const std::vector<float>& c = keys->color;
  CHECK_EQ(Near(c[0], 0.0f) && Near(c[1], light.color[0]) && Near(c[2], light.color[1]) && Near(c[3], light.color[2]), true);
  const size_t last = c.size() - 4;
  CHECK_EQ(c[last] >= 1.3f && c[last] <= 1.7f && c[last + 1] == 0.0f && c[last + 2] == 0.0f && c[last + 3] == 0.0f, true);
  for (size_t i = 4; i < c.size(); i += 4) {
    CHECK_EQ(c[i] > c[i - 4], true);
    for (int k = 1; k <= 3; ++k) CHECK_EQ(c[i + k] <= c[i - 4 + k], true);
  }
}
