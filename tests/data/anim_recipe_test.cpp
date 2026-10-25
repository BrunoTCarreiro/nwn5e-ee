#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "check.h"
#include "data/anim_recipe.h"

// Animation recipes (data/anim_recipe.h). Breaks caught: a recipe that loses
// a field on the way back; a mix that isn't halfway at 0.5 (or slerps the long
// way round); trim and speed that move keys to the wrong time; a pack whose
// root isn't the pack or that doesn't point at its library; the compiled
// library's layout (bone tree, animation list, a clip's keys) read wrong -
// tested on a library built here, nothing of the game's. The crouch step: hips
// that don't drop by the crouch, an ankle that leaves the walk's footfall
// (sinks or floats), a knee that doesn't come forward, a lean that tips the
// wrong way or drags the head with it, a pelvis that doesn't hinge, feet moved
// by the hinge, arms that trail back with the torso.

using namespace data;

namespace {

int Milli(float v) { return static_cast<int>(v * 1000.0f + (v < 0 ? -0.5f : 0.5f)); }
bool Has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

AnimClip Turning(const char* name, float length, float end_z_degrees) {
  AnimClip clip;
  clip.name = name;
  clip.length = length;
  clip.animroot = "rootdummy";
  AnimNode root;
  root.name = "a_ba";
  clip.nodes.push_back(root);
  AnimNode bone;
  bone.name = "torso_g";
  bone.parent = 0;
  bone.has_orientation = true;
  const float half = end_z_degrees * 3.14159265f / 360.0f;
  bone.orientation.times = {0.0f, length};
  bone.orientation.values = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, sinf(half), cosf(half)}};
  clip.nodes.push_back(bone);
  return clip;
}

AnimNode Bone(const char* name, int parent, float x, float y, float z) {
  AnimNode b;
  b.name = name;
  b.parent = parent;
  b.has_position = true;
  b.position.times = {0.0f};
  b.position.values = {{x, y, z, 1.0f}};
  b.has_orientation = true;
  b.orientation.times = {0.0f};
  b.orientation.values = {{0.0f, 0.0f, 0.0f, 1.0f}};
  return b;
}

// A body: the hips 1 m up, one leg (45 cm thigh, 42 cm shin), the torso to the
// neck, one arm (30 cm upper arm) from the shoulder.
std::vector<AnimNode> Body() {
  return {Bone("a_test", -1, 0.0f, 0.0f, 0.0f),   Bone("rootdummy", 0, 0.0f, 0.0f, 1.0f),
          Bone("pelvis_g", 1, 0.0f, 0.0f, 0.0f),  Bone("lthigh_g", 2, -0.1f, 0.0f, -0.05f),
          Bone("lshin_g", 3, 0.0f, 0.0f, -0.45f), Bone("lfoot_g", 4, 0.0f, 0.0f, -0.42f),
          Bone("torso_g", 1, 0.0f, 0.0f, 0.0f),   Bone("neck_g", 6, 0.0f, 0.0f, 0.45f),
          Bone("head_g", 7, 0.0f, 0.0f, 0.08f),   Bone("lbicep_g", 6, -0.2f, 0.0f, 0.42f),
          Bone("lforearm_g", 9, 0.0f, 0.0f, -0.3f)};
}

std::array<float, 4> TurnX(float degrees) {
  const float half = degrees * 3.14159265f / 360.0f;
  return {sinf(half), 0.0f, 0.0f, cosf(half)};
}

// A stride: the thigh swings 25 degrees forward, back, forward; the knee nearly
// straight on the front leg (where a careless solve flips it backward), bent behind.
AnimClip Stride(const std::vector<AnimNode>& body) {
  AnimClip clip;
  clip.name = "walk";
  clip.length = 1.0f;
  clip.animroot = "rootdummy";
  for (const AnimNode& b : body) {
    AnimNode n;
    n.name = b.name;
    n.parent = b.parent;
    clip.nodes.push_back(n);
  }
  AnimNode& root = clip.nodes[1];
  root.has_position = true;
  root.position.times = {0.0f, 1.0f};
  root.position.values = {{0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}};
  AnimNode& thigh = clip.nodes[3];
  thigh.has_orientation = true;
  thigh.orientation.times = {0.0f, 0.5f, 1.0f};
  thigh.orientation.values = {TurnX(25.0f), TurnX(-25.0f), TurnX(25.0f)};
  AnimNode& shin = clip.nodes[4];
  shin.has_orientation = true;
  shin.orientation.times = {0.0f, 0.5f, 1.0f};
  shin.orientation.values = {TurnX(-1.0f), TurnX(-12.0f), TurnX(-1.0f)};
  return clip;
}

}  // namespace

TEST(AnimRecipeRoundTrip) {
  AnimRecipe r;
  r.name = "n5_psnmild";
  r.base = "pausepsn";
  r.mix = "pause1";
  r.amount = 0.4f;
  r.speed = 0.8f;
  r.start = 0.5f;
  r.end = 3.0f;
  r.crouch = 0.25f;
  r.lean = 15.0f;
  AnimRecipe back;
  std::string error;
  CHECK_EQ(ReadAnimRecipe(WriteAnimRecipe(r), &back, &error), true);
  CHECK_EQ(back.name == "n5_psnmild", true);
  CHECK_EQ(back.base == "pausepsn", true);
  CHECK_EQ(back.mix == "pause1", true);
  CHECK_EQ(Milli(back.amount), 400);
  CHECK_EQ(Milli(back.speed), 800);
  CHECK_EQ(Milli(back.start), 500);
  CHECK_EQ(Milli(back.end), 3000);
  CHECK_EQ(Milli(back.crouch), 250);
  CHECK_EQ(Milli(back.lean), 15000);
  CHECK_EQ(ReadAnimRecipe("base = pause1\n", &back, &error), false);  // no name
  CHECK_EQ(ReadAnimRecipe("name = x\nbase = walk\ncrouch = 2\nlean = 120\n", &back, &error), true);
  CHECK_EQ(Milli(back.crouch), 600);  // kept to 0 - 0.6 m
  CHECK_EQ(Milli(back.lean), 90000);  // and -45 - 90 degrees (owner: "increase the lean range")
}

TEST(AnimRecipeCrouchKeepsTheFootfalls) {
  const std::vector<AnimNode> body = Body();
  const AnimClip walk = Stride(body);
  AnimRecipe r;
  r.name = "n5_crwalk";
  r.base = "walk";
  r.crouch = 0.2f;
  const AnimClip low = DeriveAnimClip(r, walk, nullptr, &body);
  CHECK_EQ(low.nodes[1].position.times.size(), 31u);  // keys every 1/30 s over 1 s
  int worst = 0, forward = 0, ahead = 0, samples = 0;
  for (int k = 0; k <= 60; ++k) {  // between the keys too: a flipped knee shows there
    const float t = k / 60.0f;
    const auto a = AnimWorldPositions(walk, body, t), b = AnimWorldPositions(low, body, t);
    if (k % 12 == 0) CHECK_EQ(Milli(a[1][2] - b[1][2]), 200);  // the hips 20 cm lower
    for (int c = 0; c < 3; ++c) worst = std::max(worst, abs(Milli(a[5][c] - b[5][c])));
    if (b[4][1] > a[4][1] + 0.01f) ++forward;  // the knee further forward than it was
    // and ahead of the hip-ankle line (bent the right way)
    const float line_y = b[5][1] - b[3][1], line_z = b[5][2] - b[3][2];
    const float knee_y = b[4][1] - b[3][1], knee_z = b[4][2] - b[3][2];
    if (knee_y * line_z - knee_z * line_y < 0.0f) ++ahead;
    ++samples;
  }
  CHECK_EQ(worst <= 10, true);  // the ankle within 1 cm of the walk's
  CHECK_EQ(forward, samples);
  CHECK_EQ(ahead, samples);
  // Without the bones it's skipped; without a crouch or lean there's nothing to do.
  CHECK_EQ(DeriveAnimClip(r, walk, nullptr).nodes[1].position.times.size(), 2u);
  AnimClip copy = walk;
  CHECK_EQ(ApplyCrouch(&copy, body, 0.0f, 0.0f), false);
}

// A weapon's walk layer (animroot torso_g): the lean reaches the torso, head and
// arms; nothing of the hips or legs is keyed, crouch or not.
TEST(AnimRecipeCrouchKeepsToTheAnimroot) {
  const std::vector<AnimNode> body = Body();
  AnimClip layer = Stride(body);
  layer.animroot = "torso_g";
  layer.nodes[6].has_orientation = true;  // the layer's torso key
  layer.nodes[6].orientation.times = {0.0f, 1.0f};
  layer.nodes[6].orientation.values = {TurnX(0.0f), TurnX(0.0f)};
  AnimRecipe r;
  r.name = "n5_crwalk_swordr";
  r.base = "walk_swordr";
  r.crouch = 0.2f;
  r.lean = 20.0f;
  const AnimClip low = DeriveAnimClip(r, layer, nullptr, &body);
  CHECK_EQ(low.nodes[1].position.times.size(), 2u);     // rootdummy: the layer's own keys, not 31
  CHECK_EQ(low.nodes[2].has_orientation, false);         // the pelvis untouched
  CHECK_EQ(low.nodes[3].orientation.times.size(), 3u);  // the thigh: its own keys
  CHECK_EQ(low.nodes[6].orientation.times.size(), 31u); // the torso leans
  CHECK_EQ(low.nodes[9].orientation.times.size(), 31u); // and the arm is turned back
  const auto b = AnimWorldPositions(low, body, 0.0f);
  CHECK_EQ(Milli(b[7][1]), 154);  // the neck 20 degrees forward
}

TEST(AnimRecipeLeanTiltsTheUpperBody) {
  const std::vector<AnimNode> body = Body();
  const AnimClip walk = Stride(body);
  AnimRecipe r;
  r.name = "n5_lean";
  r.base = "walk";
  r.lean = 20.0f;
  const AnimClip leaning = DeriveAnimClip(r, walk, nullptr, &body);
  const auto a = AnimWorldPositions(walk, body, 0.3f), b = AnimWorldPositions(leaning, body, 0.3f);
  CHECK_EQ(Milli(b[7][1]), 154);            // the neck, 45 cm up, 20 degrees forward: 0.45 sin 20
  CHECK_EQ(Milli(b[7][2] - a[7][2]), -27);  // and 0.45 (cos 20 - 1) lower
  CHECK_EQ(Milli(leaning.nodes[8].orientation.values[0][0]), 122);  // the head back 14 degrees: sin 7
  const AnimKeys& hips = leaning.nodes[2].orientation;  // the pelvis tipped 35%, 7 degrees: sin -3.5
  CHECK_EQ(hips.values.empty() ? 0 : Milli(hips.values[0][0]), -61);
  CHECK_EQ(Milli(b[1][2] - a[1][2]), 0);    // the hips at their height
  CHECK_EQ(b[9][1] > a[9][1] + 0.1f, true);  // the shoulder forward with the torso
  for (int c = 0; c < 3; ++c) {                // but the arm hanging as it did, not trailing back
    CHECK_EQ(Milli((b[10][c] - b[9][c]) - (a[10][c] - a[9][c])), 0);
  }
  // The feet stay put though the pelvis they hang from tipped, between keys too.
  int worst = 0;
  for (int k = 0; k <= 60; ++k) {
    const auto w0 = AnimWorldPositions(walk, body, k / 60.0f), w1 = AnimWorldPositions(leaning, body, k / 60.0f);
    for (int c = 0; c < 3; ++c) worst = std::max(worst, abs(Milli(w0[5][c] - w1[5][c])));
  }
  CHECK_EQ(worst <= 10, true);
}

TEST(AnimRecipeMixesTrimsAndSpeeds) {
  const AnimClip base = Turning("pausepsn", 1.0f, 90.0f);  // ends turned 90 degrees
  const AnimClip still = Turning("pause1", 2.0f, 0.0f);    // stays straight
  AnimRecipe r;
  r.name = "n5_half";
  r.base = "pausepsn";
  r.mix = "pause1";
  r.amount = 0.5f;
  AnimClip half = DeriveAnimClip(r, base, &still);
  CHECK_EQ(half.nodes.size(), 2u);
  const AnimKeys& keys = half.nodes[1].orientation;
  CHECK_EQ(keys.times.size(), 2u);
  CHECK_EQ(Milli(keys.values[1][2]), 383);  // 45 degrees: sin(22.5)
  CHECK_EQ(Milli(keys.values[1][3]), 924);
  // Twice as fast, the first half only: keys at 0 and 0.25 s, length 0.25.
  r.amount = 0.0f;
  r.speed = 2.0f;
  r.end = 0.5f;
  const AnimClip quick = DeriveAnimClip(r, base, &still);
  CHECK_EQ(Milli(quick.length), 250);
  CHECK_EQ(Milli(quick.nodes[1].orientation.times.back()), 250);
  CHECK_EQ(Milli(quick.nodes[1].orientation.values.back()[2]), 383);  // halfway through the turn
  // The pack.
  const std::string pack = WriteAnimPack("n5_ba", "a_ba", base.nodes, {half});
  CHECK_EQ(Has(pack, "setsupermodel n5_ba a_ba"), true);
  CHECK_EQ(Has(pack, "node dummy n5_ba\n  parent NULL"), true);
  CHECK_EQ(Has(pack, "newanim n5_half n5_ba"), true);
  CHECK_EQ(Has(pack, "  orientationkey 2"), true);
  CHECK_EQ(Has(pack, "  parent n5_ba"), true);
}

TEST(AnimRecipeReadsACompiledLibrary) {
  std::string b(0x800, '\0');
  const size_t base = 12;
  const auto put32 = [&](size_t at, uint32_t v) { memcpy(&b[at], &v, 4); };
  const auto put16 = [&](size_t at, uint16_t v) { memcpy(&b[at], &v, 2); };
  const auto putf = [&](size_t at, float v) { memcpy(&b[at], &v, 4); };
  const auto puts = [&](size_t at, const char* text) { memcpy(&b[at], text, strlen(text)); };
  const auto rel = [&](size_t file_offset) { return static_cast<uint32_t>(file_offset - base); };
  puts(base + 8, "a_test");
  puts(base + 0xA8, "NULL");
  // Geometry: a_test -> rootdummy (a position).
  put32(base + 0x48, rel(0x200));
  puts(0x200 + 0x20, "a_test");
  put32(0x200 + 0x48, rel(0x278));
  put32(0x200 + 0x4C, 1);
  put32(0x278, rel(0x280));
  puts(0x280 + 0x20, "rootdummy");
  put32(0x280 + 0x54, rel(0x600));
  put32(0x280 + 0x58, 1);
  put32(0x280 + 0x60, rel(0x640));
  put32(0x280 + 0x64, 4);
  put32(0x600, 8);
  put16(0x604, 1);
  put16(0x606, 0);
  put16(0x608, 1);
  b[0x60A] = 3;
  putf(0x640, 0.0f);
  putf(0x644, 0.1f);
  putf(0x648, 0.2f);
  putf(0x64C, 1.2f);
  // One animation, "idle": a_test -> rootdummy turning 90 degrees over 2 s.
  put32(base + 0x78, rel(0x300));
  put32(base + 0x7C, 1);
  put32(0x300, rel(0x400));
  puts(0x400 + 8, "idle");
  put32(0x400 + 0x48, rel(0x500));
  putf(0x400 + 0x70, 2.0f);
  putf(0x400 + 0x74, 0.25f);
  puts(0x400 + 0x78, "rootdummy");
  puts(0x500 + 0x20, "a_test");
  put32(0x500 + 0x48, rel(0x578));
  put32(0x500 + 0x4C, 1);
  put32(0x578, rel(0x580));
  puts(0x580 + 0x20, "rootdummy");
  put32(0x580 + 0x54, rel(0x700));
  put32(0x580 + 0x58, 1);
  put32(0x580 + 0x60, rel(0x740));
  put32(0x580 + 0x64, 10);
  put32(0x700, 20);
  put16(0x704, 2);
  put16(0x706, 0);
  put16(0x708, 2);
  b[0x70A] = 4;
  putf(0x740, 0.0f);
  putf(0x744, 2.0f);
  const float q[8] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.7071f, 0.7071f};
  for (int i = 0; i < 8; ++i) putf(0x748 + 4 * i, q[i]);

  AnimLibrary library;
  std::string error;
  CHECK_EQ(ReadAnimLibrary(b, &library, &error), true);
  CHECK_EQ(library.name == "a_test", true);
  CHECK_EQ(library.supermodel.empty(), true);  // "NULL"
  CHECK_EQ(library.bones.size(), 2u);
  CHECK_EQ(library.bones[1].name == "rootdummy", true);
  CHECK_EQ(library.bones[1].parent, 0);
  CHECK_EQ(Milli(library.bones[1].position.values[0][2]), 1200);
  CHECK_EQ(library.clip_names.size(), 1u);
  CHECK_EQ(library.clip_names[0] == "idle", true);
  AnimClip clip;
  CHECK_EQ(ReadAnimClip(b, "IDLE", &clip), true);
  CHECK_EQ(Milli(clip.length), 2000);
  CHECK_EQ(clip.animroot == "rootdummy", true);
  CHECK_EQ(clip.nodes.size(), 2u);
  CHECK_EQ(clip.nodes[1].orientation.times.size(), 2u);
  CHECK_EQ(Milli(clip.nodes[1].orientation.values[1][3]), 707);
  CHECK_EQ(ReadAnimClip(b, "walk", &clip), false);
}
