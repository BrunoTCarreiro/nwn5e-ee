#include <math.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "data/anim_assemble.h"
#include "data/anim_recipe.h"

// Assembling an assembled recipe (data/anim_assemble.h, #87): the capture composed onto the
// stance's first frame, the root offset from the stance's, the blends from and to the stance and
// the loop - on a synthetic body (rootdummy and one bone, b1) and a stance clip whose frame 0 turns
// b1 30 degrees about Z with the root at (0, 0, 1). Breaks caught: delta * S instead of S * delta;
// a linear blend instead of smoothstep; the loop applied after the blends; no hold past the last key.

using namespace data;

namespace {

using Q = std::array<float, 4>;
constexpr float kDeg = 3.14159265f / 180.0f;

Q Rx(float degrees) { return {sinf(degrees * kDeg / 2.0f), 0.0f, 0.0f, cosf(degrees * kDeg / 2.0f)}; }
Q Rz(float degrees) { return {0.0f, 0.0f, sinf(degrees * kDeg / 2.0f), cosf(degrees * kDeg / 2.0f)}; }

// The same turn (q and -q are one), component by component.
bool Close(const Q& a, const Q& b, float eps) {
  float same = 0.0f, flipped = 0.0f;
  for (int i = 0; i < 4; ++i) {
    same = std::max(same, fabsf(a[i] - b[i]));
    flipped = std::max(flipped, fabsf(a[i] + b[i]));
  }
  return std::min(same, flipped) <= eps;
}

bool At(const Q& p, float x, float y, float z, float eps) {
  return fabsf(p[0] - x) <= eps && fabsf(p[1] - y) <= eps && fabsf(p[2] - z) <= eps;
}

float AngleDegrees(const Q& a, const Q& b) {
  float dot = 0.0f;
  for (int i = 0; i < 4; ++i) dot += a[i] * b[i];
  return 2.0f * acosf(std::min(1.0f, fabsf(dot))) / kDeg;
}

bool Has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

std::vector<AnimNode> Body() {
  std::vector<AnimNode> bones(3);
  bones[0].name = "a_ba";
  bones[1].name = "rootdummy";
  bones[1].parent = 0;
  bones[1].has_position = bones[1].has_orientation = true;
  bones[1].position.times = {0.0f};
  bones[1].position.values = {{0.0f, 0.0f, 0.95f, 0.0f}};  // rest: not the stance's root
  bones[1].orientation.times = {0.0f};
  bones[1].orientation.values = {{0.0f, 0.0f, 0.0f, 1.0f}};
  bones[2].name = "b1";
  bones[2].parent = 1;
  bones[2].has_orientation = true;
  bones[2].orientation.times = {0.0f};
  bones[2].orientation.values = {{0.0f, 0.0f, 0.0f, 1.0f}};
  return bones;
}

// The stance moves on after frame 0, so only its first frame may count.
AnimClip Stance() {
  AnimClip clip;
  clip.name = "1hreadyr";
  clip.length = 1.0f;
  clip.animroot = "rootdummy";
  clip.nodes = Body();
  clip.nodes[1].position.times = {0.0f, 1.0f};
  clip.nodes[1].position.values = {{0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.05f, 0.0f}};
  clip.nodes[2].orientation.times = {0.0f, 1.0f};
  clip.nodes[2].orientation.values = {Rz(30.0f), Rz(45.0f)};
  return clip;
}

AnimRecipe Recipe() {
  AnimRecipe r;
  r.name = "n5_asm";
  r.kind = AnimRecipeKind::kAssembled;
  r.stance = "1hreadyr";
  r.length = 1.0f;
  r.loop_blend = 0.25f;
  return r;
}

AnimKeyTrack Turn(std::vector<float> times, std::vector<Q> values) {
  AnimKeyTrack track;
  track.bone = "b1";
  track.keys.times = times;
  track.keys.values = values;
  return track;
}

AnimKeyTrack Offset(float x, float y, float z) {
  AnimKeyTrack track;
  track.bone = "rootdummy";
  track.position = true;
  track.keys.times = {0.0f};
  track.keys.values = {{x, y, z, 0.0f}};
  return track;
}

const AnimNode* Node(const AnimClip& clip, const char* name) {
  for (const AnimNode& node : clip.nodes) {
    if (node.name == name) return &node;
  }
  return nullptr;
}

}  // namespace

TEST(AssembleIdentityDeltaIsTheStance) {
  AnimRecipe r = Recipe();
  r.keys["a_ba"] = {Turn({0.0f}, {Q{0.0f, 0.0f, 0.0f, 1.0f}}), Offset(0.0f, 0.0f, 0.0f)};
  AnimClip out;
  std::string err;
  CHECK_EQ(AssembleAnimClip(r, "a_ba", Body(), Stance(), &out, &err), true);
  const AnimNode* b1 = Node(out, "b1");
  const AnimNode* root = Node(out, "rootdummy");
  CHECK_EQ(b1 != nullptr && root != nullptr, true);
  if (!b1 || !root) return;
  CHECK_EQ(b1->has_orientation && root->has_position, true);
  CHECK_EQ(static_cast<int>(b1->orientation.times.size()), 31);  // 0 .. 1 s every 1/30 s
  CHECK_EQ(static_cast<int>(root->position.times.size()), 31);
  bool turns = true, roots = true;
  for (const Q& q : b1->orientation.values) turns = turns && Close(q, Rz(30.0f), 1e-5f);
  for (const Q& p : root->position.values) roots = roots && At(p, 0.0f, 0.0f, 1.0f, 1e-5f);
  CHECK_EQ(turns, true);
  CHECK_EQ(roots, true);
  CHECK_EQ(out.name == "n5_asm" && fabsf(out.length - 1.0f) < 1e-6f, true);
}

TEST(AssembleComposesOnTheStance) {
  AnimRecipe r = Recipe();
  r.keys["a_ba"] = {Turn({0.0f}, {Rx(20.0f)})};
  AnimClip out;
  std::string err;
  CHECK_EQ(AssembleAnimClip(r, "a_ba", Body(), Stance(), &out, &err), true);
  const AnimNode* b1 = Node(out, "b1");
  CHECK_EQ(b1 != nullptr && b1->orientation.values.size() == 31, true);
  if (!b1 || b1->orientation.values.size() != 31) return;
  CHECK_EQ(Close(b1->orientation.values[15], QMul(Rz(30.0f), Rx(20.0f)), 1e-5f), true);  // t = 0.5: S * delta
}

TEST(AssembleBlendsFromAndToTheStance) {
  AnimRecipe r = Recipe();
  r.blend_in = 0.2f;
  r.blend_out = 0.2f;
  r.keys["a_ba"] = {Turn({0.0f}, {Rx(20.0f)})};
  AnimClip out;
  std::string err;
  CHECK_EQ(AssembleAnimClip(r, "a_ba", Body(), Stance(), &out, &err), true);
  const AnimNode* b1 = Node(out, "b1");
  CHECK_EQ(b1 != nullptr && b1->orientation.values.size() == 31, true);
  if (!b1 || b1->orientation.values.size() != 31) return;
  const std::vector<Q>& q = b1->orientation.values;
  CHECK_EQ(Close(q[0], Rz(30.0f), 1e-5f), true);                           // starts on the stance
  CHECK_EQ(Close(q[15], QMul(Rz(30.0f), Rx(20.0f)), 1e-5f), true);         // the capture between
  CHECK_EQ(Close(q[30], Rz(30.0f), 1e-5f), true);                          // ends on it
  CHECK_EQ(fabsf(AngleDegrees(q[3], Rz(30.0f)) - 10.0f) <= 0.5f, true);    // t = 0.1: smoothstep(0.5) = 0.5
  // t = 1/30: smoothstep(1/6) = 0.074 of the way (1.48 degrees), not a linear 1/6 (3.33).
  CHECK_EQ(fabsf(AngleDegrees(q[1], Rz(30.0f)) - 1.48f) <= 0.3f, true);
}

TEST(AssembleRootOffset) {
  AnimRecipe r = Recipe();
  r.blend_in = 0.2f;
  r.blend_out = 0.2f;
  r.keys["a_ba"] = {Offset(0.0f, 0.3f, 0.0f)};
  AnimClip out;
  std::string err;
  CHECK_EQ(AssembleAnimClip(r, "a_ba", Body(), Stance(), &out, &err), true);
  const AnimNode* root = Node(out, "rootdummy");
  CHECK_EQ(root != nullptr && root->position.values.size() == 31, true);
  if (!root || root->position.values.size() != 31) return;
  CHECK_EQ(At(root->position.values[15], 0.0f, 0.3f, 1.0f, 1e-5f), true);  // the stance's root + the offset
  CHECK_EQ(At(root->position.values[0], 0.0f, 0.0f, 1.0f, 1e-5f), true);   // home at the blended ends
  CHECK_EQ(At(root->position.values[30], 0.0f, 0.0f, 1.0f, 1e-5f), true);
}

TEST(AssembleLoopsBackToTheStart) {
  AnimRecipe r = Recipe();
  r.loop = true;
  r.loop_blend = 0.25f;
  r.keys["a_ba"] = {Turn({0.0f, 1.0f}, {Rx(10.0f), Rx(60.0f)})};
  AnimClip out;
  std::string err;
  CHECK_EQ(AssembleAnimClip(r, "a_ba", Body(), Stance(), &out, &err), true);
  const AnimNode* b1 = Node(out, "b1");
  CHECK_EQ(b1 != nullptr && b1->orientation.values.size() == 31, true);
  if (!b1 || b1->orientation.values.size() != 31) return;
  CHECK_EQ(Close(b1->orientation.values[30], b1->orientation.values[0], 1e-4f), true);
  CHECK_EQ(Close(b1->orientation.values[0], QMul(Rz(30.0f), Rx(10.0f)), 1e-5f), true);  // untouched before the crossfade
  // The loop is applied under the blends: with a blend out, the last frame is the stance, not frame 0.
  r.blend_out = 0.2f;
  CHECK_EQ(AssembleAnimClip(r, "a_ba", Body(), Stance(), &out, &err), true);
  b1 = Node(out, "b1");
  CHECK_EQ(b1 != nullptr && b1->orientation.values.size() == 31, true);
  if (!b1 || b1->orientation.values.size() != 31) return;
  CHECK_EQ(Close(b1->orientation.values[30], Rz(30.0f), 1e-5f), true);
}

// Review Focus 4: a length past the last key holds that key.
TEST(AssembleHoldsThePastLastKey) {
  AnimRecipe r = Recipe();
  r.keys["a_ba"] = {Turn({0.0f, 0.5f}, {Q{0.0f, 0.0f, 0.0f, 1.0f}, Rx(40.0f)})};
  AnimClip out;
  std::string err;
  CHECK_EQ(AssembleAnimClip(r, "a_ba", Body(), Stance(), &out, &err), true);
  const AnimNode* b1 = Node(out, "b1");
  CHECK_EQ(b1 != nullptr && b1->orientation.values.size() == 31, true);
  if (!b1 || b1->orientation.values.size() != 31) return;
  bool held = true;
  for (int f = 16; f <= 30; ++f) held = held && Close(b1->orientation.values[f], QMul(Rz(30.0f), Rx(40.0f)), 1e-5f);
  CHECK_EQ(held, true);
}

TEST(AssembleNoKeysForLibrary) {
  AnimRecipe r = Recipe();
  r.keys["a_ba"] = {Turn({0.0f}, {Rx(20.0f)})};
  AnimClip out;
  std::string err;
  CHECK_EQ(AssembleAnimClip(r, "a_fa", Body(), Stance(), &out, &err), false);
  CHECK_EQ(Has(err, "a_fa"), true);
}

// Review Focus 3: a stance clip with nothing in it (a library without the stance gives none) is
// refused by name, not composed onto rest.
TEST(AssembleEmptyStanceRefused) {
  AnimRecipe r = Recipe();
  r.keys["a_ba"] = {Turn({0.0f}, {Rx(20.0f)})};
  AnimClip out;
  std::string err;
  CHECK_EQ(AssembleAnimClip(r, "a_ba", Body(), AnimClip(), &out, &err), false);
  CHECK_EQ(Has(err, "1hreadyr"), true);
}

// The phase 1 launch check's recipe (tests/data/fixtures/n5_asmtiny.recipe) is exactly what
// WriteAnimRecipe writes for it - stance pause1, a_ba only, lthigh_g turned 20 degrees about X for
// 1 s, blends 0.1 / 0.1 - and reads back as one; the other bodies get "no keys".
TEST(AssembleTinyFixtureIsTheWritersOwn) {
  AnimRecipe r;
  r.name = "n5_asmtiny";
  r.kind = AnimRecipeKind::kAssembled;
  r.stance = "pause1";
  r.length = 1.0f;
  r.blend_in = 0.1f;
  r.blend_out = 0.1f;
  r.source = "tests/data/fixtures - the phase 1 launch check (#87)";
  AnimKeyTrack thigh;
  thigh.bone = "lthigh_g";
  thigh.keys.times = {0.0f, 1.0f};
  thigh.keys.values = {Rx(20.0f), Rx(20.0f)};
  r.keys["a_ba"] = {thigh};
  std::ifstream file("tests/data/fixtures/n5_asmtiny.recipe", std::ios::binary);
  std::stringstream read;
  read << file.rdbuf();
  std::string text = read.str();
  text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());  // a checkout may give it CRLFs
  CHECK_EQ(text.empty(), false);
  CHECK_EQ(text == WriteAnimRecipe(r), true);
  AnimRecipe back;
  std::string error;
  CHECK_EQ(ReadAnimRecipe(text, &back, &error), true);
  CHECK_EQ(back.kind == AnimRecipeKind::kAssembled && back.stance == "pause1" && back.keys.size() == 1, true);
  AnimClip out;
  CHECK_EQ(AssembleAnimClip(back, "a_fa", Body(), Stance(), &out, &error), false);
  CHECK_EQ(Has(error, "no keys"), true);
}
