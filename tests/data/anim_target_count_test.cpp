#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "core/anim_target_counts.h"
#include "data/vfx_model.h"

// #171's target-count marker (data/anim_target_count.h; the game's call, core/anim_target_counts.h): the call's bounds,
// then the final marker (TF) - the tallies' rows, models and files, below. The textures: tools/data/tally/check.js.
// (The first four-disc marker, TC, and the TP probe were retired 2026-11-20.)

namespace {

std::string ReadText(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

std::vector<std::string> Words(const std::string& line) {
  std::istringstream in(line);
  std::vector<std::string> words;
  for (std::string word; in >> word;) words.push_back(word);
  return words;
}

bool Near(float a, float b) { return fabsf(a - b) < 1e-4f; }

}  // namespace

TEST(TargetCountMarkerVfxRefusesOutOfRange) {
  CHECK_EQ(data::kTargetCountMax, 11);
  for (int count : {0, -1, -11, 12, 13, 100, INT_MIN, INT_MAX}) {
    for (TargetCountStyle style : {TargetCountStyle::MagicMissile, TargetCountStyle::ScorchingRay, TargetCountStyle::EldritchBlast}) {
      CHECK_EQ(TargetCountMarkerVfx(style, count), -1);
    }
  }
}

// --- TF (#171): the final marker - camera-facing tallies, black bars on each spell's glow ------------------------------
// The owner, 2026-11-20, after the probe (TP): the face-on style, and "black on blue/orange/violet glow for magic
// missile, scorching ray and eldritch blast respectively". The game's call TargetCountMarkerVfx(style, count): rows
// data::kTargetTallyFirstRow + 11 x style + count - 1 (data/rows/visualeffects.tally.txt; a test reads the file). Each
// model two quads our vertex shader vsn5bill_sl turns to the camera: the bars (black, alpha-blended) and the glow
// (additive, black where the bars are) 1 cm nearer the camera. The art, the shader's lines and its maths:
// tools/data/tally/check.js.

namespace {

const TargetCountStyle kTallyStyles[] = {TargetCountStyle::MagicMissile, TargetCountStyle::ScorchingRay,
                                         TargetCountStyle::EldritchBlast};

std::string Upper(std::string text) {
  for (char& c : text) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
  return text;
}

// A small text file's lines, CRs dropped, blank lines skipped.
std::vector<std::string> TextLines(const std::string& text) {
  std::vector<std::string> lines;
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty()) lines.push_back(line);
  }
  return lines;
}

std::string Two(int n) { return std::string(1, static_cast<char>('0' + n / 10)) + static_cast<char>('0' + n % 10); }

}  // namespace

TEST(TargetCountStyleMapsEachCountToItsRow) {
  for (int s = 0; s < 3; ++s) {
    for (int count = 1; count <= 11; ++count) {
      CHECK_EQ(TargetCountMarkerVfx(kTallyStyles[s], count),
               data::kTargetTallyFirstRow + s * data::kTargetCountMax + count - 1);
      CHECK_EQ(data::TargetTallyRow(s, count), TargetCountMarkerVfx(kTallyStyles[s], count));
    }
  }
  CHECK_EQ(data::kTargetTallyFirstRow, 2215);
  CHECK_EQ(data::kTargetTallyStyles, 3);
  CHECK_EQ(TargetCountMarkerVfx(TargetCountStyle::MagicMissile, 1), 2215);
  CHECK_EQ(TargetCountMarkerVfx(TargetCountStyle::MagicMissile, 11), 2225);
  CHECK_EQ(TargetCountMarkerVfx(TargetCountStyle::ScorchingRay, 1), 2226);
  CHECK_EQ(TargetCountMarkerVfx(TargetCountStyle::ScorchingRay, 11), 2236);
  CHECK_EQ(TargetCountMarkerVfx(TargetCountStyle::EldritchBlast, 1), 2237);
  CHECK_EQ(TargetCountMarkerVfx(TargetCountStyle::EldritchBlast, 11), 2247);
  // A style the enum doesn't name (a cast) has no row, no model.
  for (int s : {-1, 3, 4, 100, INT_MIN, INT_MAX}) {
    CHECK_EQ(TargetCountMarkerVfx(static_cast<TargetCountStyle>(s), 1), -1);
    CHECK_EQ(data::TargetTallyRow(s, 5), -1);
    CHECK_EQ(data::TargetTallyModel(s, 5).empty(), true);
    CHECK_EQ(data::TargetTallyGlowBitmap(s, 5).empty(), true);
  }
}

TEST(TargetTallyNames) {
  CHECK_EQ(data::TargetTallyStyleCode(0) == "mm" && data::TargetTallyStyleCode(1) == "sr" &&
               data::TargetTallyStyleCode(2) == "eb" && data::TargetTallyStyleCode(3).empty(),
           true);
  CHECK_EQ(data::TargetTallyModel(0, 1) == "n5_tf_mm01", true);
  CHECK_EQ(data::TargetTallyModel(1, 10) == "n5_tf_sr10", true);
  CHECK_EQ(data::TargetTallyModel(2, 11) == "n5_tf_eb11", true);
  CHECK_EQ(data::TargetTallyGlowBitmap(0, 3) == "n5_tfg_mm03", true);
  CHECK_EQ(data::TargetTallyGlowBitmap(2, 7) == "n5_tfg_eb07", true);
  CHECK_EQ(data::TargetTallyBarsBitmap(7) == "n5_tfb_07", true);
  std::vector<std::string> names;
  for (int s = 0; s < 3; ++s) {
    for (int count = 1; count <= 11; ++count) {
      for (const std::string& name : {data::TargetTallyModel(s, count), data::TargetTallyGlowBitmap(s, count)}) {
        CHECK_EQ(!name.empty() && name.size() <= 16, true);
        names.push_back(name);
      }
    }
  }
  for (int count = 1; count <= 11; ++count) names.push_back(data::TargetTallyBarsBitmap(count));
  for (size_t i = 0; i < names.size(); ++i) {
    for (size_t j = i + 1; j < names.size(); ++j) CHECK_EQ(names[i] == names[j], false);
  }
  for (int count : {0, -1, 12, INT_MIN, INT_MAX}) {
    CHECK_EQ(data::TargetTallyModel(0, count).empty(), true);
    CHECK_EQ(data::TargetTallyGlowBitmap(1, count).empty(), true);
    CHECK_EQ(data::TargetTallyBarsBitmap(count).empty(), true);
  }
}

// data/rows/visualeffects.tally.txt: exactly the 33 rows the call gives, each a lasting effect on the head node naming
// its model, upright (OrientWithObject 0), its label the style and count; each model in data/files.
TEST(TargetTallyRowsFileNamesTheModels) {
  std::istringstream lines(ReadText("data/rows/visualeffects.tally.txt"));
  std::vector<std::string> header;
  int rows = 0;
  int seen[3][12] = {};
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
    const int index = row - data::kTargetTallyFirstRow;
    CHECK_EQ(index >= 0 && index < 3 * data::kTargetCountMax, true);
    if (index < 0 || index >= 3 * data::kTargetCountMax) continue;
    const int s = index / data::kTargetCountMax, count = index % data::kTargetCountMax + 1;
    ++seen[s][count];
    CHECK_EQ(TargetCountMarkerVfx(kTallyStyles[s], count), row);
    for (size_t c = 0; c < header.size(); ++c) {
      const std::string& column = header[c];
      const std::string& value = w[c + 1];
      if (column == "Label") {
        CHECK_EQ(value == "NWN5E_TALLY_" + Upper(data::TargetTallyStyleCode(s)) + "_" + std::to_string(count), true);
      } else if (column == "Type_FD") {
        CHECK_EQ(value == "D", true);
      } else if (column == "Imp_HeadCon_Node") {
        CHECK_EQ(value == data::TargetTallyModel(s, count), true);
      } else if (column == "OrientWithGround" || column == "OrientWithObject") {
        CHECK_EQ(value == "0", true);
      } else {
        CHECK_EQ(column == "an unexpected column", true);
      }
    }
    CHECK_EQ(std::filesystem::exists("data/files/" + data::TargetTallyModel(s, count) + ".mdl"), true);
  }
  CHECK_EQ(rows, 33);
  CHECK_EQ(header.size(), 5u);
  for (int s = 0; s < 3; ++s) {
    for (int count = 1; count <= 11; ++count) CHECK_EQ(seen[s][count], 1);
  }
}

// Each model as the game's loader and the dev layer read it: two quads, 0.5 x 0.25 m, made round their node's origin in
// its x-z plane (the shader lays x across and z up toward the camera, and -y toward it), the node 0.55 m over the head
// node, counter-clockwise seen from -y; the bars on the plane (y 0), the glow 1 cm in front (y -0.01: nearer the camera,
// so the bars' depth never cuts it - it is black where the bars are). Each lit by itself alone (selfillum 1, ambient and
// diffuse 0), alpha 1, nothing keyed, no emitter, no cessation (a removed marker would linger). Their materials: our
// vertex shader and our unlit fragment shader fsn5unlit (fix round 1: nothing tints it - the picked target's red glow
// reached fslit's emission), texture0 the layer's picture; the glow's TXI additive, the bars' not (alpha-blended: black
// can't be added) and their material see-through (`transparency 1`: drawn with the blended meshes, cut at alpha 0.2 -
// never as an opaque box). Each TXI the blend line alone; the pictures have none.
TEST(TargetTallyModelsAreTwoFaceOnLayers) {
  CHECK_EQ(std::filesystem::exists("data/files/vsn5bill_sl.shd"), true);
  CHECK_EQ(std::filesystem::exists("data/files/fsn5unlit.shd"), true);
  for (int s = 0; s < 3; ++s) {
    for (int count = 1; count <= 11; ++count) {
      const std::string name = data::TargetTallyModel(s, count);
      const std::string bars = data::TargetTallyBarsBitmap(count), glow = data::TargetTallyGlowBitmap(s, count);
      const std::string text = ReadText("data/files/" + name + ".mdl");
      data::VfxEffect fx;
      std::string error;
      const bool read = data::ReadVfxAsciiModel(text, &fx, &error);
      if (!read) printf("    %s: %s\n", name.c_str(), error.c_str());
      CHECK_EQ(read, true);
      if (!read) continue;
      CHECK_EQ(fx.name == name, true);
      CHECK_EQ(fx.emitters.empty(), true);
      CHECK_EQ(fx.planes.size(), 2u);
      CHECK_EQ(text.find("\nnewanim cessation") == std::string::npos, true);
      CHECK_EQ(text.find("\nnewanim impact " + name) != std::string::npos, true);
      CHECK_EQ(text.find("\nnewanim duration " + name) != std::string::npos, true);
      int found_bars = 0, found_glow = 0;
      for (const data::VfxPlane& p : fx.planes) {
        const bool is_glow = p.bitmap == glow;
        CHECK_EQ(p.bitmap == bars || is_glow, true);
        (is_glow ? found_glow : found_bars)++;
        const float layer_y = is_glow ? -0.01f : 0.0f;
        CHECK_EQ(p.parent.empty(), true);
        CHECK_EQ(Near(p.position[0], 0.0f) && Near(p.position[1], 0.0f) && Near(p.position[2], 0.55f), true);
        CHECK_EQ(Near(p.orientation[3], 0.0f), true);
        CHECK_EQ(p.render, true);
        CHECK_EQ(Near(p.alpha, 1.0f), true);
        CHECK_EQ(p.transparency_hint, 0);
        CHECK_EQ(Near(p.selfillum[0], 1.0f) && Near(p.selfillum[1], 1.0f) && Near(p.selfillum[2], 1.0f), true);
        CHECK_EQ(Near(p.ambient[0] + p.ambient[1] + p.ambient[2], 0.0f), true);
        CHECK_EQ(Near(p.diffuse[0] + p.diffuse[1] + p.diffuse[2], 0.0f), true);
        CHECK_EQ(p.keys.empty(), true);
        CHECK_EQ(p.verts.size(), 12u);
        CHECK_EQ(p.tverts.size(), 8u);
        CHECK_EQ(p.faces.size(), 6u);
        if (p.verts.size() != 12 || p.tverts.size() != 8 || p.faces.size() != 6) continue;
        for (size_t v = 0; v < 4; ++v) {
          const float x = p.verts[v * 3], y = p.verts[v * 3 + 1], z = p.verts[v * 3 + 2];
          CHECK_EQ(Near(fabsf(x), 0.25f) && Near(fabsf(z), 0.125f) && Near(y, layer_y), true);
          CHECK_EQ(p.tverts[v * 2] == (x > 0 ? 1.0f : 0.0f) && p.tverts[v * 2 + 1] == (z > 0 ? 1.0f : 0.0f), true);
        }
        for (size_t t = 0; t < 2; ++t) {
          const int* f = &p.faces[t * 3];
          const float* a = &p.verts[f[0] * 3];
          const float* b = &p.verts[f[1] * 3];
          const float* c = &p.verts[f[2] * 3];
          // counter-clockwise seen from -y: (b - a) x (c - a) points along -y
          const float ny = (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2]);
          CHECK_EQ(ny < 0.0f, true);
        }
        // Its material and TXI, found by the bitmap's name.
        const std::vector<std::string> mtr = TextLines(ReadText("data/files/" + p.bitmap + ".mtr"));
        const std::string picture = is_glow ? "n5_tfgp_" + data::TargetTallyStyleCode(s) + Two(count) : "n5_tfbp_" + Two(count);
        std::vector<std::string> want = {"customshadervs vsn5bill_sl", "customshaderfs fsn5unlit", "texture0 " + picture};
        if (!is_glow) want.push_back("transparency 1");
        CHECK_EQ(mtr == want, true);
        CHECK_EQ(std::filesystem::exists("data/files/" + picture + ".tga"), true);
        // Its TXI: the blend line alone (fix round 1: no downsample lines - the engine reads none); no TXI for the picture.
        const std::vector<std::string> want_txi = {is_glow ? "blending additive" : "blending default"};
        CHECK_EQ(TextLines(ReadText("data/files/" + p.bitmap + ".txi")) == want_txi, true);
        CHECK_EQ(std::filesystem::exists("data/files/" + picture + ".txi"), false);
      }
      CHECK_EQ(found_bars, 1);
      CHECK_EQ(found_glow, 1);
    }
  }
}
