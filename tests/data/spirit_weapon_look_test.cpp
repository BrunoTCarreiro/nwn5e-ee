// #39's Spiritual Weapon, option C's look (the owner, 2026-11-22: "weapon appears, but doesn't look very "spiritual", is it
// possible to customize the look and idle animation?", then "A, make the draft look"): the force an effect of nwn5e's own on
// an invisible marker - visualeffects 4180 (n5_swp_idle, as it waits) and 4182 (n5_swp_hit, its strike), and n5_swp_fly
// (in flight: spells.2da 1279's ProjModel, the game session's row), each a VFX Bench draft (tools/data/vfx) and the model the
// game plays (data/files), written by tools/data/spiritweapon/build.js with its two textures (n5_swp_blade, n5_swp_glow).
#include <math.h>
#include <stdlib.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "data/vfx_model.h"

namespace {

const char* const kModels[] = {"n5_swp_idle", "n5_swp_fly", "n5_swp_hit"};

std::string SwpText(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream text;
  text << file.rdbuf();
  std::string out;
  for (char c : text.str()) {
    if (c != '\r') out += c;
  }
  return out;
}

std::vector<std::string> SwpWords(const std::string& line) {
  std::istringstream in(line);
  std::vector<std::string> words;
  for (std::string word; in >> word;) words.push_back(word);
  return words;
}

bool SwpDraft(const std::string& name, data::VfxEffect* effect) {
  std::string error;
  return data::ReadVfxDraft(SwpText("tools/data/vfx/" + name + ".vfx"), effect, &error);
}

const data::VfxKeys* SwpKeys(const std::vector<data::VfxKeys>& keys, const std::string& anim) {
  for (const data::VfxKeys& k : keys) {
    if (k.anim == anim) return &k;
  }
  return nullptr;
}

// A model's geometry: each emitter node's "  birthrate" line, by its node name.
std::map<std::string, float> SwpGeometryRates(const std::string& model) {
  std::map<std::string, float> rates;
  const std::string geometry = model.substr(0, model.find("endmodelgeom"));
  std::istringstream lines(geometry);
  std::string node;
  for (std::string line; std::getline(lines, line);) {
    if (line.rfind("node emitter ", 0) == 0) node = line.substr(13);
    if (line.rfind("  birthrate ", 0) == 0 && !node.empty()) rates[node] = static_cast<float>(atof(line.c_str() + 12));
  }
  return rates;
}

}  // namespace

// data/rows/visualeffects.spiritweapon.txt: 4180 and 4182, each naming its model in the four root columns, flat; the idle a
// lasting effect (D), the strike a one-shot (F); no 4181 (the flight is spells.2da's projectile); each model in
// data/files, its draft in tools/data/vfx.
TEST(SpiritWeaponRowsNameTheModels) {
  std::istringstream lines(SwpText("data/rows/visualeffects.spiritweapon.txt"));
  std::vector<std::string> header;
  std::map<int, std::vector<std::string>> rows;
  for (std::string line; std::getline(lines, line);) {
    const std::vector<std::string> w = SwpWords(line);
    if (w.empty() || w[0][0] == '#') continue;
    if (header.empty()) {
      header = w;
      continue;
    }
    rows[atoi(w[0].c_str())] = w;
  }
  CHECK_EQ(header.size() == 7 && header[0] == "Label" && header[1] == "Type_FD" && header[2] == "OrientWithGround", true);
  CHECK_EQ(rows.size() == 2 && rows.count(4181) == 0, true);
  const int numbers[] = {4180, 4182};
  const char* const labels[] = {"NWN5E_SPIRIT_WEAPON", "NWN5E_SPIRIT_WEAPON_STRIKE"};
  const char* const types[] = {"D", "F"};
  const char* const shown[] = {"n5_swp_idle", "n5_swp_hit"};
  for (int i = 0; i < 2; ++i) {
    const auto it = rows.find(numbers[i]);
    CHECK_EQ(it != rows.end(), true);
    if (it == rows.end()) continue;
    const std::vector<std::string>& w = it->second;
    CHECK_EQ(w.size(), 8u);
    if (w.size() != 8) continue;
    CHECK_EQ(w[1] == labels[i] && w[2] == types[i] && w[3] == "0", true);
    for (int c = 4; c < 8; ++c) CHECK_EQ(w[c] == shown[i], true);
  }
  for (const char* name : kModels) {
    CHECK_EQ(std::filesystem::exists(std::string("data/files/") + name + ".mdl"), true);
    CHECK_EQ(std::filesystem::exists(std::string("tools/data/vfx/") + name + ".vfx"), true);
  }
}

// Each draft reads and writes back its own bytes and writes the committed model through the game's model writer - the
// draft the bench opens, the model the game plays. The model as the bench's nwn5e tab reads it (the dev layer's
// ReadVfxAsciiModel; it lists a model with emitters): its emitters, nodes and meshes whole - under the writer's names
// (n5_swp_idle_0_glow...) and without its planes' fades (nwn5e's own models' rule), so the owner refines the draft itself
// (the bench's Draft text box, then Yours).
TEST(SpiritWeaponDraftsWriteTheModels) {
  for (const char* name : kModels) {
    const std::string text = SwpText(std::string("tools/data/vfx/") + name + ".vfx");
    data::VfxEffect effect;
    std::string error;
    CHECK_EQ(!text.empty() && data::ReadVfxDraft(text, &effect, &error), true);
    CHECK_EQ(data::WriteVfxDraft(effect) == text, true);
    CHECK_EQ(effect.name == name && effect.attach == "root" && !effect.emitters.empty(), true);
    const std::string model = SwpText(std::string("data/files/") + name + ".mdl");
    CHECK_EQ(!model.empty() && data::WriteVfxModel(effect, name) == model, true);
    data::VfxEffect back;
    CHECK_EQ(data::ReadVfxAsciiModel(model, &back, &error), true);
    CHECK_EQ(back.emitters.size() == effect.emitters.size() && back.nodes.size() == effect.nodes.size() &&
                 back.planes.size() == effect.planes.size() && !back.emitters.empty(),
             true);
    for (size_t i = 0; i < back.planes.size() && i < effect.planes.size(); ++i) {
      CHECK_EQ(back.planes[i].verts == effect.planes[i].verts && back.planes[i].faces == effect.planes[i].faces &&
                   back.planes[i].bitmap == effect.planes[i].bitmap,
               true);
    }
  }
}

// The textures beside them: the blade's with its TXI saying added light (the game draws a mesh adding its light only by
// its texture's TXI), the glow's with none (a particle's blend is its emitter's) - 32-bit TGAs with alpha.
TEST(SpiritWeaponTexturesShipBesideTheModels) {
  for (const char* name : {"n5_swp_blade", "n5_swp_glow"}) {
    const std::string tga = SwpText(std::string("data/files/") + name + ".tga");
    CHECK_EQ(tga.size() > 18 && tga[2] == 2 && static_cast<unsigned char>(tga[16]) == 32, true);
  }
  CHECK_EQ(SwpText("data/files/n5_swp_blade.txi").rfind("blending additive\n", 0) == 0, true);
  CHECK_EQ(std::filesystem::exists("data/files/n5_swp_glow.txi"), false);
}

// The idle: a lasting effect whose loop starts and ends on the same pose (its hover node's first and last keys), its blade
// fading in over its start and out over its end; every particle added light (Lighten).
TEST(SpiritWeaponIdleLoopsSeamlessly) {
  data::VfxEffect idle;
  CHECK_EQ(SwpDraft("n5_swp_idle", &idle), true);
  CHECK_EQ(idle.duration && idle.nodes.size() == 1 && idle.planes.size() == 1, true);
  if (idle.nodes.size() != 1 || idle.planes.size() != 1) return;
  const data::VfxKeys* loop = SwpKeys(idle.nodes[0].keys, "duration");
  CHECK_EQ(loop != nullptr, true);
  if (!loop) return;
  const std::vector<float>& p = loop->position;
  const std::vector<float>& o = loop->orientation;
  CHECK_EQ(p.size() >= 8 && p.size() % 4 == 0 && o.size() >= 10 && o.size() % 5 == 0, true);
  if (p.size() < 8 || o.size() < 10) return;
  const size_t lp = p.size() - 4, lo = o.size() - 5;
  CHECK_EQ(p[0] == 0.0f && fabsf(p[lp] - idle.lengths[1]) < 1e-4f, true);  // the loop's whole length
  for (int k = 1; k < 4; ++k) CHECK_EQ(p[k] == p[lp + k], true);
  for (int k = 1; k < 5; ++k) CHECK_EQ(o[k] == o[lo + k], true);
  // Its bob: about 5 cm either way of where it hangs.
  float lowest = 1e9f, highest = -1e9f;
  for (size_t i = 0; i + 3 < p.size(); i += 4) {
    lowest = fminf(lowest, p[i + 3]);
    highest = fmaxf(highest, p[i + 3]);
  }
  CHECK_EQ(fabsf((highest - lowest) - 0.1f) < 0.005f && fabsf(idle.nodes[0].position[2] - p[3]) < 1e-4f, true);
  const data::VfxKeys* in = SwpKeys(idle.planes[0].keys, "impact");
  const data::VfxKeys* out = SwpKeys(idle.planes[0].keys, "cessation");
  CHECK_EQ(in && out && in->alpha.size() >= 4 && out->alpha.size() >= 4, true);
  if (in && out && in->alpha.size() >= 4 && out->alpha.size() >= 4) {
    CHECK_EQ(in->alpha[1] == 0.0f && in->alpha[in->alpha.size() - 1] == 1.0f, true);
    CHECK_EQ(out->alpha[1] == 1.0f && out->alpha[out->alpha.size() - 1] == 0.0f, true);
  }
  for (const data::VfxEmitter& e : idle.emitters) CHECK_EQ(e.blend == "Lighten" && e.parent == "hover", true);
}

// The flight: a projectile model plays no keyed animation (the game's own vpr_* have none), so every emitter carries its
// rate in the model's geometry; the blade's point along +y (the game's own ammunition flies point first along +y), 1.2 m
// over its root (cast from a marker at ground level).
TEST(SpiritWeaponFlightNeedsNoAnimation) {
  data::VfxEffect fly;
  CHECK_EQ(SwpDraft("n5_swp_fly", &fly), true);
  const std::map<std::string, float> rates = SwpGeometryRates(SwpText("data/files/n5_swp_fly.mdl"));
  CHECK_EQ(rates.size() == fly.emitters.size() && !rates.empty(), true);
  for (const auto& r : rates) CHECK_EQ(r.second > 0.0f, true);
  CHECK_EQ(fly.planes.size(), 1u);
  if (fly.planes.size() != 1) return;
  float most = -1e9f;
  size_t tip = 0;
  const std::vector<float>& v = fly.planes[0].verts;
  for (size_t i = 0; i + 2 < v.size(); i += 3) {
    if (v[i + 1] > most) {
      most = v[i + 1];
      tip = i;
    }
  }
  CHECK_EQ(most > 0.4f && fabsf(v[tip]) < 1e-4f && fabsf(v[tip + 2]) < 1e-4f, true);  // the point: on the +y axis
  CHECK_EQ(fly.planes[0].orientation[3] == 0.0f && fly.planes[0].position[2] == 1.2f, true);
}

// The strike: a one-shot whose impact starts on its keys (transtime 0 - its blade's fade starts at 0, off its rest), so
// the game doesn't blend the swing in from the rest pose; its swing node turns through about 72 degrees in 0.45 s.
TEST(SpiritWeaponStrikeStartsOnItsKeys) {
  data::VfxEffect hit;
  CHECK_EQ(SwpDraft("n5_swp_hit", &hit), true);
  CHECK_EQ(!hit.duration && hit.nodes.size() == 1, true);
  const std::string model = SwpText("data/files/n5_swp_hit.mdl");
  CHECK_EQ(model.find("newanim impact n5_swp_hit\n  length 0.9\n  transtime 0\n") != std::string::npos, true);
  if (hit.nodes.size() != 1) return;
  const data::VfxKeys* swing = SwpKeys(hit.nodes[0].keys, "impact");
  CHECK_EQ(swing && swing->orientation.size() == 55u, true);
  if (!swing || swing->orientation.size() != 55u) return;
  CHECK_EQ(swing->orientation[0] == 0.0f && fabsf(swing->orientation[50] - 0.45f) < 1e-4f, true);
  for (int k = 1; k < 5; ++k) CHECK_EQ(swing->orientation[k] == hit.nodes[0].orientation[k - 1], true);  // from its rest
}
