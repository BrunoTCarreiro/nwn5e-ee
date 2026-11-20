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

// #171's target-count marker (data/anim_target_count.h; the game's call, core/anim_target_counts.h): the agreed call's
// mapping and bounds, the rows file's rows naming the models (one source of truth with kTargetCountFirstRow), and the
// models as the game and the VFX Bench read them - four one-sided discs facing out round the head, each reading its
// number upright and never mirrored from the side it is seen from, nothing keyed (a key waits for the clock, and the
// clock stops while paused). The textures themselves: tools/data/tcount/check.js. Since TF the call shows the final
// look - black tallies on each spell's glow (below; the discs' rows stay, data::TargetCountRow).

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

// The four-disc marker's own rows (TC) stay where they were; the game's one-argument call has shown the final marker
// (TF) since TF: Magic Missile's look, until the game session moves to the style call.
TEST(TargetCountMarkerVfxMapsEachCountToItsRow) {
  for (int count = 1; count <= 11; ++count) {
    CHECK_EQ(data::TargetCountRow(count), data::kTargetCountFirstRow + count - 1);
    CHECK_EQ(TargetCountMarkerVfx(count), TargetCountMarkerVfx(TargetCountStyle::MagicMissile, count));
  }
  CHECK_EQ(data::TargetCountRow(1), 2200);
  CHECK_EQ(data::TargetCountRow(11), 2210);
  CHECK_EQ(TargetCountMarkerVfx(1), 2215);
  CHECK_EQ(TargetCountMarkerVfx(11), 2225);
  CHECK_EQ(data::kTargetCountMax, 11);
}

TEST(TargetCountMarkerVfxRefusesOutOfRange) {
  for (int count : {0, -1, -11, 12, 13, 100, INT_MIN, INT_MAX}) {
    CHECK_EQ(TargetCountMarkerVfx(count), -1);
    CHECK_EQ(data::TargetCountRow(count), -1);
    for (TargetCountStyle style : {TargetCountStyle::MagicMissile, TargetCountStyle::ScorchingRay, TargetCountStyle::EldritchBlast}) {
      CHECK_EQ(TargetCountMarkerVfx(style, count), -1);
    }
  }
}

TEST(TargetCountModelNames) {
  CHECK_EQ(data::TargetCountModel(1) == "n5_tcnt_01", true);
  CHECK_EQ(data::TargetCountModel(9) == "n5_tcnt_09", true);
  CHECK_EQ(data::TargetCountModel(10) == "n5_tcnt_10", true);
  CHECK_EQ(data::TargetCountModel(11) == "n5_tcnt_11", true);
  for (int count : {0, -1, 12, INT_MIN, INT_MAX}) CHECK_EQ(data::TargetCountModel(count).empty(), true);
  for (int count = 1; count <= 11; ++count) CHECK_EQ(data::TargetCountModel(count).size() <= 16, true);
}

// data/rows/visualeffects.tcount.txt: exactly rows kTargetCountFirstRow .. + 10, each a lasting effect on the head node
// naming its count's model, upright (OrientWithObject 0: the client turns a position-only effect to the world's axes);
// each model, texture and TXI committed in data/files.
TEST(TargetCountRowsFileNamesTheModels) {
  std::istringstream lines(ReadText("data/rows/visualeffects.tcount.txt"));
  std::vector<std::string> header;
  int rows = 0;
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
    const int count = row - data::kTargetCountFirstRow + 1;
    CHECK_EQ(data::TargetCountRow(count), row);
    for (size_t c = 0; c < header.size(); ++c) {
      const std::string& column = header[c];
      const std::string& value = w[c + 1];
      if (column == "Label") CHECK_EQ(value == "NWN5E_TARGET_COUNT_" + std::to_string(count), true);
      else if (column == "Type_FD") CHECK_EQ(value == "D", true);
      else if (column == "Imp_HeadCon_Node") CHECK_EQ(value == data::TargetCountModel(count), true);
      else if (column == "OrientWithGround" || column == "OrientWithObject") CHECK_EQ(value == "0", true);
      else CHECK_EQ(column == "an unexpected column", true);
    }
    for (const char* ext : {".mdl", ".tga", ".txi"}) {
      CHECK_EQ(std::filesystem::exists("data/files/" + data::TargetCountModel(count) + ext), true);
    }
  }
  CHECK_EQ(rows, 11);
  CHECK_EQ(header.size(), 5u);
}

// Each model as the VFX Bench's nwn5e tab reads it (ReadVfxAsciiModel, the dev layer's export): four planes with its
// texture, each a 0.4 m square standing upright, its centre 0.2 m out from the head's axis and 0.55 m over the head
// node, facing out (counter-clockwise seen from outside: the game culls the back); seen from the side it faces, its u
// runs to the viewer's right and its v up - the number reads, never mirrored; drawn as an opaque cut-out (hint 0) lit by
// itself alone; nothing keyed; one emitter, inert (it only lists the model in the bench: no particle, never drawn).
TEST(TargetCountModelsAreFourOutwardDiscs) {
  for (int count = 1; count <= 11; ++count) {
    const std::string name = data::TargetCountModel(count);
    data::VfxEffect fx;
    std::string error;
    const bool read = data::ReadVfxAsciiModel(ReadText("data/files/" + name + ".mdl"), &fx, &error);
    if (!read) printf("    %s: %s\n", name.c_str(), error.c_str());
    CHECK_EQ(read, true);
    if (!read) continue;
    CHECK_EQ(fx.name == name, true);
    CHECK_EQ(fx.planes.size(), 4u);
    CHECK_EQ(fx.emitters.size(), 1u);
    if (fx.emitters.size() == 1) {
      CHECK_EQ(fx.emitters[0].birthrate == 0.0f, true);
      CHECK_EQ(fx.emitters[0].life == 0.0f, true);
      CHECK_EQ(fx.emitters[0].keys.empty(), true);
    }
    int facing[4] = {0, 0, 0, 0};  // +x, +y, -x, -y
    for (const data::VfxPlane& p : fx.planes) {
      CHECK_EQ(p.bitmap == name, true);
      CHECK_EQ(p.transparency_hint, 0);
      CHECK_EQ(p.render, true);
      CHECK_EQ(Near(p.alpha, 1.0f), true);
      CHECK_EQ(Near(p.selfillum[0], 1.0f) && Near(p.selfillum[1], 1.0f) && Near(p.selfillum[2], 1.0f), true);
      CHECK_EQ(Near(p.ambient[0] + p.ambient[1] + p.ambient[2], 0.0f), true);
      CHECK_EQ(Near(p.diffuse[0] + p.diffuse[1] + p.diffuse[2], 0.0f), true);
      CHECK_EQ(p.keys.empty(), true);
      CHECK_EQ(p.parent.empty(), true);
      CHECK_EQ(Near(p.position[0], 0.0f) && Near(p.position[1], 0.0f) && Near(p.position[2], 0.55f), true);
      CHECK_EQ(Near(p.orientation[3], 0.0f), true);
      CHECK_EQ(p.verts.size(), 12u);
      CHECK_EQ(p.tverts.size(), 8u);
      CHECK_EQ(p.faces.size(), 6u);
      if (p.verts.size() != 12 || p.tverts.size() != 8 || p.faces.size() != 6) continue;
      for (size_t t = 0; t < 2; ++t) {
        const int* f = &p.faces[t * 3];
        const float* a = &p.verts[f[0] * 3];
        const float* b = &p.verts[f[1] * 3];
        const float* c = &p.verts[f[2] * 3];
        const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const float e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const float n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
        const float len = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        CHECK_EQ(len > 0.0f, true);
        if (len <= 0.0f) continue;
        const float out[3] = {n[0] / len, n[1] / len, n[2] / len};
        CHECK_EQ(Near(out[2], 0.0f), true);  // upright
        const float centre[3] = {(a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3, (a[2] + b[2] + c[2]) / 3};
        CHECK_EQ(out[0] * centre[0] + out[1] * centre[1] > 0.0f, true);  // facing out, away from the head's axis
        // Seen from the front (looking along -out, z up), the viewer's right is z x out.
        const float right[3] = {-out[1], out[0], 0.0f};
        // u and v across the triangle: solve the 2 x 2 for the gradient of u and v along (right, up).
        const float du1 = p.tverts[f[1] * 2] - p.tverts[f[0] * 2], dv1 = p.tverts[f[1] * 2 + 1] - p.tverts[f[0] * 2 + 1];
        const float du2 = p.tverts[f[2] * 2] - p.tverts[f[0] * 2], dv2 = p.tverts[f[2] * 2 + 1] - p.tverts[f[0] * 2 + 1];
        const float r1 = e1[0] * right[0] + e1[1] * right[1], z1 = e1[2];
        const float r2 = e2[0] * right[0] + e2[1] * right[1], z2 = e2[2];
        const float det = r1 * z2 - r2 * z1;
        CHECK_EQ(fabsf(det) > 1e-6f, true);
        if (fabsf(det) <= 1e-6f) continue;
        const float du_dright = (du1 * z2 - du2 * z1) / det, du_dup = (r1 * du2 - r2 * du1) / det;
        const float dv_dright = (dv1 * z2 - dv2 * z1) / det, dv_dup = (r1 * dv2 - r2 * dv1) / det;
        CHECK_EQ(Near(du_dright, 1.0f / 0.4f), true);  // u left to right across 0.4 m: not mirrored
        CHECK_EQ(Near(dv_dup, 1.0f / 0.4f), true);     // v bottom to top: not upside down
        CHECK_EQ(Near(du_dup, 0.0f) && Near(dv_dright, 0.0f), true);
      }
      // The square: 0.4 m wide and tall, 0.2 m out (its plane), centred on the axis's side.
      float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
      for (size_t v = 0; v < 4; ++v) {
        for (int k = 0; k < 3; ++k) {
          lo[k] = fminf(lo[k], p.verts[v * 3 + k]);
          hi[k] = fmaxf(hi[k], p.verts[v * 3 + k]);
        }
        CHECK_EQ(p.tverts[v * 2] == 0.0f || p.tverts[v * 2] == 1.0f, true);
        CHECK_EQ(p.tverts[v * 2 + 1] == 0.0f || p.tverts[v * 2 + 1] == 1.0f, true);
      }
      CHECK_EQ(Near(lo[2], -0.2f) && Near(hi[2], 0.2f), true);
      const bool alongX = Near(hi[0] - lo[0], 0.4f) && Near(hi[1], lo[1]);
      const bool alongY = Near(hi[1] - lo[1], 0.4f) && Near(hi[0], lo[0]);
      CHECK_EQ(alongX || alongY, true);
      if (alongY && Near(lo[0], 0.2f)) ++facing[0];
      if (alongX && Near(lo[1], 0.2f)) ++facing[1];
      if (alongY && Near(lo[0], -0.2f)) ++facing[2];
      if (alongX && Near(lo[1], -0.2f)) ++facing[3];
    }
    for (int side = 0; side < 4; ++side) CHECK_EQ(facing[side], 1);
    // The dev layer's export for the bench's nwn5e tab: the model as a draft (WriteVfxDraft), read back as the bench's
    // Play in game sends it - the four discs whole.
    data::VfxEffect draft;
    CHECK_EQ(data::ReadVfxDraft(data::WriteVfxDraft(fx), &draft, &error), true);
    CHECK_EQ(draft.planes.size(), fx.planes.size());
    for (size_t i = 0; i < draft.planes.size() && i < fx.planes.size(); ++i) {
      CHECK_EQ(draft.planes[i].bitmap == name, true);
      CHECK_EQ(draft.planes[i].verts == fx.planes[i].verts && draft.planes[i].tverts == fx.planes[i].tverts, true);
      CHECK_EQ(draft.planes[i].faces == fx.planes[i].faces, true);
      CHECK_EQ(Near(draft.planes[i].position[2], 0.55f), true);
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
