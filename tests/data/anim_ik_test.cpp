#include <math.h>

#include <string>
#include <vector>

#include "check.h"
#include "data/anim_assemble.h"
#include "data/anim_pose.h"
#include "data/anim_recipe.h"

// The recipe core's IK (#89): the two-bone solver arms and legs share (data/anim_pose.h). Breaks
// caught: the damping ignored (a limb snaps straight), the end's world turn not kept, the bend flipped
// (a knee or elbow folding the wrong way), the shortfall misreported, a broken chain accepted.

using namespace data;
using namespace data::pose;

namespace {

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

// A body: the hips 1 m up, a leg (45 cm thigh, 42 cm shin), the torso, an arm (30 cm upper arm, 28 cm
// forearm) hanging from the shoulder.
std::vector<AnimNode> Body() {
  return {Bone("a_ik", -1, 0.0f, 0.0f, 0.0f),     Bone("rootdummy", 0, 0.0f, 0.0f, 1.0f),
          Bone("pelvis_g", 1, 0.0f, 0.0f, 0.0f),  Bone("lthigh_g", 2, -0.1f, 0.0f, -0.05f),
          Bone("lshin_g", 3, 0.0f, 0.0f, -0.45f), Bone("lfoot_g", 4, 0.0f, 0.0f, -0.42f),
          Bone("torso_g", 1, 0.0f, 0.0f, 0.0f),   Bone("lbicep_g", 6, -0.2f, 0.0f, 0.42f),
          Bone("lforearm_g", 7, 0.0f, 0.0f, -0.3f), Bone("lhand_g", 8, 0.0f, 0.0f, -0.28f)};
}

Quat TurnX(float degrees) { return AxisAngleQ({1.0f, 0.0f, 0.0f}, degrees * kPi / 180.0f); }
float Dist(const Vec3& a, const Vec3& b) { return Length(Sub(a, b)); }

// The arm's chain, bent at the elbow (the forearm turned forward), and the pose.
struct Arm {
  std::vector<AnimNode> bones = Body();
  Chain c = FindChain(bones, "lbicep_g", "lforearm_g", "lhand_g");
  Pose before = RestPose(bones);
  Arm(float elbow_degrees) {
    before.lq[c.mid] = TurnX(elbow_degrees);
    Forward(bones, &before);
  }
  // Solves toward `goal` from the bent pose and returns the hand's place.
  Vec3 Solve(const Vec3& goal, float damping, float* shortfall = nullptr, Pose* out = nullptr) {
    Pose now = before;
    const float s = SolveTwoBone(bones, before, &now, c, goal, {0.0f, -1.0f, 0.0f}, damping);
    Forward(bones, &now);
    if (shortfall) *shortfall = s;
    if (out) *out = now;
    return now.wp[c.end];
  }
};

}  // namespace

TEST(AnimIkChainAndLengths) {
  const std::vector<AnimNode> bones = Body();
  const Chain arm = FindChain(bones, "LBICEP_G", "lforearm_g", "lhand_g");
  CHECK_EQ(arm.root >= 0, true);
  CHECK_EQ(static_cast<int>(lroundf(ChainLength(bones, arm) * 1000.0f)), 580);
  CHECK_EQ(FindChain(bones, "lbicep_g", "lhand_g", "lforearm_g").root, -1);  // not a chain
  CHECK_EQ(FindChain(bones, "rbicep_g", "rforearm_g", "rhand_g").root, -1);  // not on this body
  CHECK_EQ(static_cast<int>(lroundf(RestHipHeight(bones) * 1000.0f)), 950);
}

TEST(AnimIkTwoBoneReaches) {
  Arm arm(40.0f);
  const Vec3 shoulder = arm.before.wp[arm.c.root];
  const Vec3 goal = Add(shoulder, {0.05f, 0.25f, -0.3f});  // 0.39 m away: inside reach
  const Vec3 hand = arm.Solve(goal, kLimbDamping);
  CHECK_EQ(Dist(hand, goal) < 1e-4f, true);
}

TEST(AnimIkTwoBoneShortfall) {
  Arm arm(40.0f);
  const Vec3 shoulder = arm.before.wp[arm.c.root];
  const Vec3 dir = Normalized({0.3f, 0.4f, -0.5f});
  const Vec3 goal = Add(shoulder, Scale(dir, 0.58f + 0.3f));
  float shortfall = 0.0f;
  const Vec3 hand = arm.Solve(goal, kLimbDamping, &shortfall);
  CHECK_EQ(fabsf(shortfall - 0.3f) < 1e-4f, true);
  // The arm points at the goal, nearly straight.
  const float off = Length(Cross(Normalized(Sub(hand, shoulder)), dir));
  CHECK_EQ(off < 0.01f, true);
  CHECK_EQ(Dist(hand, shoulder) > 0.55f, true);
}

TEST(AnimIkKeepsTheBendSideAndTheHandsTurn) {
  Arm arm(40.0f);
  const Vec3 shoulder = arm.before.wp[arm.c.root];
  const Vec3 old_line = Normalized(Sub(arm.before.wp[arm.c.end], shoulder));
  Vec3 bend = Sub(arm.before.wp[arm.c.mid], shoulder);
  bend = Normalized(Sub(bend, Scale(old_line, Dot(bend, old_line))));
  Pose now;
  const Vec3 goal = Add(shoulder, {-0.1f, 0.2f, -0.35f});
  arm.Solve(goal, 0.0f, nullptr, &now);
  const Vec3 line = Normalized(Sub(goal, shoulder));
  Vec3 elbow = Sub(now.wp[arm.c.mid], shoulder);
  elbow = Sub(elbow, Scale(line, Dot(elbow, line)));
  CHECK_EQ(Dot(elbow, bend) > 0.0f, true);  // the elbow stays on the side it bent to
  const Quat a = arm.before.wq[arm.c.end], b = now.wq[arm.c.end];
  CHECK_EQ(fabsf(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]) > 0.99999f, true);  // the hand's turn kept
}

TEST(AnimIkDampingSoftensNearStraight) {
  Arm arm(40.0f);
  const Vec3 shoulder = arm.before.wp[arm.c.root];
  const Vec3 dir = Normalized({0.2f, 0.3f, -1.0f});
  const float full = 0.58f;
  // At 99% of the length the damped hand stops a little short; undamped it reaches.
  const Vec3 goal = Add(shoulder, Scale(dir, full * 0.99f));
  const float gap = Dist(arm.Solve(goal, kLimbDamping), goal);
  CHECK_EQ(gap > 0.0001f && gap < 0.005f, true);
  CHECK_EQ(Dist(arm.Solve(goal, 0.0f), goal) < 1e-4f, true);
  // Swept from 90% to 105% of the length the damped reach grows smoothly: no step over 2 mm per mm.
  float last = -1.0f;
  bool smooth = true;
  for (int mm = 0; mm <= 87; ++mm) {
    const float r = full * 0.9f + mm * 0.001f;
    const float got = Dist(arm.Solve(Add(shoulder, Scale(dir, r)), kLimbDamping), shoulder);
    if (last >= 0.0f && (got < last - 1e-5f || got - last > 0.002f)) smooth = false;
    last = got;
  }
  CHECK_EQ(smooth, true);
}

TEST(AnimIkKneesNeverBackward) {
  const std::vector<AnimNode> bones = Body();
  const Chain leg = FindChain(bones, "lthigh_g", "lshin_g", "lfoot_g");
  Pose before = RestPose(bones);
  before.lq[leg.root] = TurnX(20.0f);   // the thigh forward
  before.lq[leg.mid] = TurnX(-35.0f);   // the knee bent: the shin back
  Forward(bones, &before);
  const Vec3 hip = before.wp[leg.root];
  const Vec3 old_line = Normalized(Sub(before.wp[leg.end], hip));
  Vec3 bend = Sub(before.wp[leg.mid], hip);
  bend = Normalized(Sub(bend, Scale(old_line, Dot(bend, old_line))));
  bool forward = true;
  for (int k = 0; k <= 20; ++k) {  // the ankle swept from in front to behind, near and far
    const Vec3 goal = Add(hip, {0.0f, 0.3f - 0.03f * k, -0.6f - 0.01f * k});
    Pose now = before;
    SolveTwoBone(bones, before, &now, leg, goal, Rotate(before.wq[leg.root], {0.0f, 1.0f, 0.0f}), kLimbDamping);
    Forward(bones, &now);
    const Vec3 line = Normalized(Sub(now.wp[leg.end], hip));
    Vec3 knee = Sub(now.wp[leg.mid], hip);
    knee = Sub(knee, Scale(line, Dot(knee, line)));
    if (Dot(knee, bend) < -1e-5f) forward = false;
  }
  CHECK_EQ(forward, true);
}

// --- The IK pass (data/anim_assemble.h, step 8) ------------------------------------------------------
// On a whole body (both legs and arms) whose stance bends the knees and elbows a little, from a recipe
// of identity deltas (the capture = the stance) whose torso may turn. Breaks caught: no reach ease,
// pins re-placed every frame, the hips not scaled by the hip height, no sinking, the report dropped.
namespace {

std::vector<AnimNode> WholeBody(float s) {
  return {Bone("a_ik", -1, 0.0f, 0.0f, 0.0f),
          Bone("rootdummy", 0, 0.0f, 0.0f, 1.0f * s),
          Bone("pelvis_g", 1, 0.0f, 0.0f, 0.0f),
          Bone("lthigh_g", 2, -0.1f * s, 0.0f, -0.05f * s),
          Bone("lshin_g", 3, 0.0f, 0.0f, -0.45f * s),
          Bone("lfoot_g", 4, 0.0f, 0.0f, -0.42f * s),
          Bone("rthigh_g", 2, 0.1f * s, 0.0f, -0.05f * s),
          Bone("rshin_g", 6, 0.0f, 0.0f, -0.45f * s),
          Bone("rfoot_g", 7, 0.0f, 0.0f, -0.42f * s),
          Bone("torso_g", 1, 0.0f, 0.0f, 0.0f),
          Bone("lbicep_g", 9, -0.2f * s, 0.0f, 0.42f * s),
          Bone("lforearm_g", 10, 0.0f, 0.0f, -0.3f * s),
          Bone("lhand_g", 11, 0.0f, 0.0f, -0.28f * s),
          Bone("rbicep_g", 9, 0.2f * s, 0.0f, 0.42f * s),
          Bone("rforearm_g", 13, 0.0f, 0.0f, -0.3f * s),
          Bone("rhand_g", 14, 0.0f, 0.0f, -0.28f * s)};
}

int IndexOf(const std::vector<AnimNode>& bones, const char* name) {
  for (size_t i = 0; i < bones.size(); ++i) {
    if (bones[i].name == name) return static_cast<int>(i);
  }
  return -1;
}

// The stance's frame 0: knees and elbows a little bent, the root where the rest has it.
AnimClip IkStance(const std::vector<AnimNode>& bones) {
  AnimClip clip;
  clip.name = "pause1";
  clip.length = 1.0f;
  clip.animroot = "rootdummy";
  clip.nodes = bones;
  const auto turn = [&](const char* name, float degrees) {
    const int i = IndexOf(bones, name);
    if (i < 0) return;
    clip.nodes[i].orientation.times = {0.0f};
    clip.nodes[i].orientation.values = {TurnX(degrees)};
  };
  turn("lthigh_g", 10.0f);
  turn("rthigh_g", 10.0f);
  turn("lshin_g", -20.0f);
  turn("rshin_g", -20.0f);
  turn("lforearm_g", 30.0f);
  turn("rforearm_g", 30.0f);
  return clip;
}

// Identity deltas for every bone below the model root (the capture is the stance), the root offset 0,
// the torso turning `torso_degrees` about Z over the second; take_start 2 s.
AnimRecipe IkRecipe(const std::vector<AnimNode>& bones, float torso_degrees) {
  AnimRecipe r;
  r.name = "n5_ik";
  r.kind = AnimRecipeKind::kAssembled;
  r.stance = "pause1";
  r.length = 1.0f;
  r.take_start = 2.0f;
  for (size_t i = 1; i < bones.size(); ++i) {
    AnimKeyTrack track;
    track.bone = bones[i].name;
    track.keys.times = {0.0f, 1.0f};
    const float half = (bones[i].name == "torso_g" ? torso_degrees : 0.0f) * kPi / 360.0f;
    track.keys.values = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, sinf(half), cosf(half)}};
    r.keys["a_ik"].push_back(track);
  }
  AnimKeyTrack root;
  root.bone = "rootdummy";
  root.position = true;
  root.keys.times = {0.0f};
  root.keys.values = {{0.0f, 0.0f, 0.0f, 0.0f}};
  r.keys["a_ik"].push_back(root);
  return r;
}

AnimClip Run(const AnimRecipe& r, const std::vector<AnimNode>& bones, std::vector<AnimIkShort>* report = nullptr) {
  AnimClip out;
  std::string err;
  const bool ok = AssembleAnimClip(r, "a_ik", bones, IkStance(bones), &out, &err, report);
  CHECK_EQ(ok, true);
  return out;
}

Pose PoseOf(const AnimClip& clip, const std::vector<AnimNode>& bones, float t) {
  return PoseAt(clip, ClipIndex(clip, bones), bones, t);
}
Vec3 At(const AnimClip& clip, const std::vector<AnimNode>& bones, const char* name, float t) {
  return PoseOf(clip, bones, t).wp[IndexOf(bones, name)];
}

// The offset putting the end of the chain from `from` at `place`, in `frame`'s frame, in lengths.
std::array<float, 3> OffsetFor(const AnimClip& base, const std::vector<AnimNode>& bones, const char* from,
                               const char* frame, float length, const Vec3& place, float t) {
  const Pose p = PoseOf(base, bones, t);
  const Vec3 local = Rotate(QConj(p.wq[IndexOf(bones, frame)]), Sub(place, p.wp[IndexOf(bones, from)]));
  return {local[0] / length, local[1] / length, local[2] / length};
}

AnimTargetKey Key(float t, const std::array<float, 3>& offset, bool pinned = false, float reach = 1.0f) {
  AnimTargetKey k;
  k.t = t;
  k.offset = offset;
  k.pinned = pinned;
  k.reach = reach;
  return k;
}

float ArmLength(const std::vector<AnimNode>& bones) {
  return ChainLength(bones, FindChain(bones, "lbicep_g", "lforearm_g", "lhand_g"));
}

bool SameKeys(const AnimClip& a, const AnimClip& b) {
  if (a.nodes.size() != b.nodes.size()) return false;
  for (size_t i = 0; i < a.nodes.size(); ++i) {
    if (a.nodes[i].orientation.values != b.nodes[i].orientation.values) return false;
    if (a.nodes[i].position.values != b.nodes[i].position.values) return false;
  }
  return true;
}

}  // namespace

TEST(AnimIkNoTargetsIdentical) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  const AnimRecipe plain = IkRecipe(bones, 30.0f);
  AnimRecipe empty = plain;
  empty.targets["left_hand"] = {};  // a limb with no keys
  std::vector<AnimNode> armless = bones;
  armless[IndexOf(bones, "lbicep_g")].name = "lbicep_x";
  AnimRecipe absent = IkRecipe(armless, 30.0f);  // a limb this body hasn't
  const AnimRecipe absent_plain = absent;
  absent.targets["left_hand"] = {Key(2.5f, {0.0f, 0.5f, -0.5f})};
  CHECK_EQ(SameKeys(Run(plain, bones), Run(empty, bones)), true);
  CHECK_EQ(SameKeys(Run(absent_plain, armless), Run(absent, armless)), true);
}

TEST(AnimIkHandTargetReached) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  const AnimClip base = Run(r, bones);
  const Vec3 place = Add(At(base, bones, "lhand_g", 0.5f), {0.1f, 0.15f, 0.1f});
  r.targets["left_hand"] = {Key(2.5f, OffsetFor(base, bones, "lbicep_g", "torso_g", ArmLength(bones), place, 0.5f))};
  const AnimClip out = Run(r, bones);
  CHECK_EQ(Dist(At(out, bones, "lhand_g", 0.5f), place) < 1e-3f, true);
}

TEST(AnimIkReachEases) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  const AnimClip base = Run(r, bones);
  const Vec3 place = Add(At(base, bones, "lhand_g", 0.5f), {0.1f, 0.15f, 0.1f});
  r.targets["left_hand"] = {Key(2.5f, OffsetFor(base, bones, "lbicep_g", "torso_g", ArmLength(bones), place, 0.5f))};
  const AnimClip out = Run(r, bones);
  const auto moved = [&](float t) { return Dist(At(out, bones, "lhand_g", t), At(base, bones, "lhand_g", t)); };
  const float full = moved(0.5f);
  CHECK_EQ(full > 0.15f, true);
  CHECK_EQ(moved(0.3f) < 1e-4f && moved(0.7f) < 1e-4f, true);  // eased out 0.2 s either side
  CHECK_EQ(fabsf(moved(0.4f) - full * 0.5f) < 0.003f, true);    // halfway in (smoothstep 0.5)
}

TEST(AnimIkPinHoldsWorld) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 40.0f);  // the torso turns: the shoulder swings
  const AnimClip base = Run(r, bones);
  const Vec3 place = Add(At(base, bones, "lhand_g", 0.3f), {0.05f, 0.1f, 0.05f});
  const std::array<float, 3> offset = OffsetFor(base, bones, "lbicep_g", "torso_g", ArmLength(bones), place, 0.3f);
  r.targets["left_hand"] = {Key(2.3f, offset, true), Key(2.8f, offset)};
  const AnimClip pinned = Run(r, bones);
  bool held = true;
  for (int f = 9; f <= 16; ++f) held = held && Dist(At(pinned, bones, "lhand_g", f / 30.0f), place) < 1e-3f;
  CHECK_EQ(held, true);
  r.targets["left_hand"] = {Key(2.3f, offset), Key(2.8f, offset)};  // not pinned: it follows the torso
  CHECK_EQ(Dist(At(Run(r, bones), bones, "lhand_g", 0.5f), place) > 0.01f, true);
}

TEST(AnimIkHipsTargetByProportion) {
  for (float s : {1.0f, 1.25f}) {
    const std::vector<AnimNode> bones = WholeBody(s);
    AnimRecipe r = IkRecipe(bones, 0.0f);
    const AnimClip base = Run(r, bones);
    r.targets["hips"] = {Key(2.5f, {0.0f, 0.0f, -0.1f})};
    const AnimClip out = Run(r, bones);
    const float drop = At(base, bones, "rootdummy", 0.5f)[2] - At(out, bones, "rootdummy", 0.5f)[2];
    CHECK_EQ(fabsf(drop - 0.1f * RestHipHeight(bones)) < 1e-4f, true);
    CHECK_EQ(Dist(At(out, bones, "lfoot_g", 0.5f), At(base, bones, "lfoot_g", 0.5f)) < 1e-3f, true);  // the feet stay
    CHECK_EQ(Dist(At(out, bones, "rfoot_g", 0.5f), At(base, bones, "rfoot_g", 0.5f)) < 1e-3f, true);
  }
}

// The hips raised 15 cm with the feet planted: the legs can't reach, so the root sinks back (as far as
// the legs need) and the feet stay put.
TEST(AnimIkSinkingKeepsFeetPlanted) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  const AnimClip base = Run(r, bones);
  r.targets["hips"] = {Key(2.5f, {0.0f, 0.0f, 0.15f / RestHipHeight(bones)})};
  const AnimClip out = Run(r, bones);
  const float rise = At(out, bones, "rootdummy", 0.5f)[2] - At(base, bones, "rootdummy", 0.5f)[2];
  CHECK_EQ(rise > -0.001f && rise < 0.03f, true);
  CHECK_EQ(Dist(At(out, bones, "lfoot_g", 0.5f), At(base, bones, "lfoot_g", 0.5f)) < 1e-3f, true);  // exact: no damping
}

TEST(AnimIkProportionAcrossBodies) {
  Vec3 reach[2];
  int n = 0;
  for (float s : {1.0f, 1.25f}) {
    const std::vector<AnimNode> bones = WholeBody(s);
    AnimRecipe r = IkRecipe(bones, 0.0f);
    r.targets["left_hand"] = {Key(2.5f, {0.1f, 0.6f, -0.4f})};
    const AnimClip out = Run(r, bones);
    reach[n++] = Sub(At(out, bones, "lhand_g", 0.5f), At(out, bones, "lbicep_g", 0.5f));
  }
  CHECK_EQ(Dist(Scale(reach[0], 1.25f), reach[1]) < 1e-3f, true);
}

TEST(AnimIkFarTargetSafe) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  const AnimClip base = Run(r, bones);
  const float hip = RestHipHeight(bones);
  r.targets["hips"] = {Key(2.5f, {0.0f, 0.0f, 2.0f / hip})};  // 2 m up: the planted legs can't follow
  r.targets["left_hand"] = {Key(2.5f, {0.0f, 4.0f, 0.0f})};   // ~2.3 m in front of the shoulder
  std::vector<AnimIkShort> report;
  const AnimClip out = Run(r, bones, &report);
  bool finite = true;
  for (const AnimNode& n : out.nodes) {
    for (const auto& v : n.orientation.values) finite = finite && isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]) && isfinite(v[3]);
    for (const auto& v : n.position.values) finite = finite && isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]);
  }
  CHECK_EQ(finite, true);
  const float rise = At(out, bones, "rootdummy", 0.5f)[2] - At(base, bones, "rootdummy", 0.5f)[2];
  CHECK_EQ(rise > 2.0f - 0.25f * hip - 0.01f, true);  // the sinking stops at its cap
  bool hand = false, foot = false;
  for (const AnimIkShort& s : report) {
    if (s.limb == "left_hand" && s.metres > 1.0f && fabsf(s.t - 2.5f) < 0.02f) hand = true;
    if (s.limb == "left_foot" && s.metres > 1.0f) foot = true;
  }
  CHECK_EQ(hand && foot, true);
}

TEST(AnimIkBlendPullsTargetBack) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  const AnimClip base = Run(r, bones);
  const Vec3 place = Add(At(base, bones, "lhand_g", 0.2f), {0.1f, 0.15f, 0.1f});
  r.targets["left_hand"] = {Key(2.2f, OffsetFor(base, bones, "lbicep_g", "torso_g", ArmLength(bones), place, 0.2f))};
  CHECK_EQ(Dist(At(Run(r, bones), bones, "lhand_g", 0.2f), place) < 1e-3f, true);
  r.blend_in = 0.4f;  // halfway into the blend at 0.2 s: the stance pulls the hand back
  CHECK_EQ(Dist(At(Run(r, bones), bones, "lhand_g", 0.2f), place) > 0.005f, true);
}

TEST(AnimIkMissingLimbSkipped) {
  std::vector<AnimNode> bones = WholeBody(1.0f);
  bones[IndexOf(bones, "lforearm_g")].name = "lforearm_x";  // the left arm isn't a chain
  AnimRecipe r = IkRecipe(bones, 0.0f);
  const AnimClip base = Run(r, bones);
  const Vec3 place = Add(At(base, bones, "rhand_g", 0.5f), {-0.1f, 0.15f, 0.1f});
  const float arm = ChainLength(bones, FindChain(bones, "rbicep_g", "rforearm_g", "rhand_g"));
  r.targets["left_hand"] = {Key(2.5f, {0.0f, 0.5f, 0.0f})};
  r.targets["right_hand"] = {Key(2.5f, OffsetFor(base, bones, "rbicep_g", "torso_g", arm, place, 0.5f))};
  const AnimClip out = Run(r, bones);
  CHECK_EQ(Dist(At(out, bones, "rhand_g", 0.5f), place) < 1e-3f, true);
}

TEST(AnimIkLoopStillMeets) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 30.0f);
  r.loop = true;
  r.loop_blend = 0.2f;
  r.targets["left_hand"] = {Key(2.9f, {0.1f, 0.6f, -0.4f})};
  const AnimClip out = Run(r, bones);
  bool meets = true;
  for (const AnimNode& n : out.nodes) {
    if (!n.has_orientation || n.orientation.values.size() < 2) continue;
    const auto& a = n.orientation.values.front();
    const auto& b = n.orientation.values.back();
    float same = 0.0f, flipped = 0.0f;
    for (int i = 0; i < 4; ++i) {
      same = std::max(same, fabsf(a[i] - b[i]));
      flipped = std::max(flipped, fabsf(a[i] + b[i]));
    }
    meets = meets && std::min(same, flipped) < 1e-5f;
  }
  CHECK_EQ(meets, true);
}

// The final review's findings (#89). A foot target is body-relative (from the hip joint): sinking can't
// help it reach, so it never sinks the hips - it stays short, reported, and the other foot stays planted.
TEST(AnimIkFootPastReachDoesntCrouch) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  const AnimClip base = Run(r, bones);
  const float leg = ChainLength(bones, FindChain(bones, "lthigh_g", "lshin_g", "lfoot_g"));
  r.targets["left_foot"] = {Key(2.5f, {0.0f, 0.0f, -(leg + 0.04f) / leg})};  // 4 cm past reach, straight down
  std::vector<AnimIkShort> report;
  const AnimClip out = Run(r, bones, &report);
  const float drop = At(base, bones, "rootdummy", 0.5f)[2] - At(out, bones, "rootdummy", 0.5f)[2];
  CHECK_EQ(fabsf(drop) < 1e-4f, true);  // no crouch
  CHECK_EQ(Dist(At(out, bones, "rfoot_g", 0.5f), At(base, bones, "rfoot_g", 0.5f)) < 1e-3f, true);  // the planted foot stays
  bool left = false, right = false;
  for (const AnimIkShort& s : report) {
    if (s.limb == "left_foot" && fabsf(s.metres - 0.04f) < 0.005f) left = true;
    if (s.limb == "right_foot") right = true;
  }
  CHECK_EQ(left && !right, true);
}

// The hips raised past the legs' reach: the root sinks back until the planted feet reach (they do), so the
// report names the hips' own gap, not the feet.
TEST(AnimIkReportAfterSinking) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  r.targets["hips"] = {Key(2.5f, {0.0f, 0.0f, 0.10f / RestHipHeight(bones)})};
  std::vector<AnimIkShort> report;
  const AnimClip out = Run(r, bones, &report);
  bool feet = false, hips = false;
  for (const AnimIkShort& s : report) {
    if (s.limb == "left_foot" || s.limb == "right_foot") feet = true;
    if (s.limb == "hips" && s.metres > 0.05f && s.metres < 0.11f && fabsf(s.t - 2.5f) < 0.02f) hips = true;
  }
  CHECK_EQ(!feet && hips, true);
}

// A held key made while the hips sink lands where it was made: its place is taken on the body after the
// hips and the legs, as the frame itself stands.
TEST(AnimIkPinWhileSinking) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  r.targets["hips"] = {Key(2.5f, {0.0f, 0.0f, 0.10f / RestHipHeight(bones)})};
  const AnimClip sunk = Run(r, bones);  // the body as the bench shows it, the hips sunk back
  const Vec3 place = Add(At(sunk, bones, "lhand_g", 0.5f), {0.05f, 0.1f, 0.05f});
  r.targets["left_hand"] = {Key(2.5f, OffsetFor(sunk, bones, "lbicep_g", "torso_g", ArmLength(bones), place, 0.5f), true)};
  CHECK_EQ(Dist(At(Run(r, bones), bones, "lhand_g", 0.5f), place) < 1e-3f, true);
}

// A take time that isn't finite (only a hand-edited file) assembles safely: no key read past the end.
TEST(AnimIkNonFiniteTimeSafe) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  r.take_start = NAN;
  // Two keys a limb: with one, the unguarded code reads past the keys (undefined); with two, a nan time
  // reads as full reach - deterministically wrong without the guard.
  r.targets["left_hand"] = {Key(2.5f, {0.1f, 0.6f, -0.4f}), Key(2.6f, {0.1f, 0.6f, -0.4f})};
  r.targets["hips"] = {Key(2.5f, {0.0f, 0.0f, -0.1f}), Key(2.6f, {0.0f, 0.0f, -0.1f})};
  const AnimClip out = Run(r, bones);
  AnimRecipe zero = r;
  zero.take_start = 0.0f;  // read as 0: the clip a take_start of 0 gives
  CHECK_EQ(SameKeys(out, Run(zero, bones)), true);
}

// The owner's test (#88): a held foot key held the ankle while the foot went on turning with the animation
// (the swivel the capture's toe point gives a planted foot). A held foot key holds the foot's world turn
// too, as it was at the key, until the next key; a free key leaves the turn to the animation.
namespace {

// The recipe with the left foot turning `degrees` about up over the second: a swivel on the ankle, whose
// place doesn't move.
AnimRecipe SwivelRecipe(const std::vector<AnimNode>& bones, float degrees) {
  AnimRecipe r = IkRecipe(bones, 0.0f);
  const float half = degrees * kPi / 360.0f;
  for (AnimKeyTrack& t : r.keys["a_ik"]) {
    if (t.bone == "lfoot_g" && !t.position) t.keys.values = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, sinf(half), cosf(half)}};
  }
  return r;
}
float LegLength(const std::vector<AnimNode>& bones) { return ChainLength(bones, FindChain(bones, "lthigh_g", "lshin_g", "lfoot_g")); }
float DegreesBetween(const Quat& a, const Quat& b) {
  const float d = fabsf(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]);
  return 2.0f * acosf(fminf(1.0f, d)) * 180.0f / kPi;
}
Quat FootTurn(const AnimClip& clip, const std::vector<AnimNode>& bones, float t) { return PoseOf(clip, bones, t).wq[IndexOf(bones, "lfoot_g")]; }

}  // namespace

TEST(AnimIkHeldFootKeepsItsTurn) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = SwivelRecipe(bones, 30.0f);
  const AnimClip base = Run(r, bones);
  const Vec3 place = At(base, bones, "lfoot_g", 0.3f);
  const std::array<float, 3> offset = OffsetFor(base, bones, "lthigh_g", "pelvis_g", LegLength(bones), place, 0.3f);
  const Quat landed = FootTurn(base, bones, 0.3f);
  CHECK_EQ(DegreesBetween(landed, FootTurn(base, bones, 0.6f)) > 8.0f, true);  // the animation swivels it
  r.targets["left_foot"] = {Key(2.3f, offset, true), Key(2.8f, offset)};
  const AnimClip held = Run(r, bones);
  float most = 0.0f, off = 0.0f;
  for (int f = 9; f <= 18; ++f) {  // from the held key (2.3 s) to the next one's window (2.6 s)
    most = fmaxf(most, DegreesBetween(landed, FootTurn(held, bones, f / 30.0f)));
    off = fmaxf(off, Dist(At(held, bones, "lfoot_g", f / 30.0f), place));
  }
  CHECK_EQ(most < 0.5f, true);
  CHECK_EQ(off < 1e-3f, true);
  r.targets["left_foot"] = {Key(2.3f, offset), Key(2.8f, offset)};  // not held: the animation's turn
  CHECK_EQ(DegreesBetween(landed, FootTurn(Run(r, bones), bones, 0.6f)) > 8.0f, true);
}

// The hold eases in with the key's reach and out over the window before the next key: no frame turns the
// foot more than 5 degrees (the animation turns it 1 a frame; letting go catches up 15 over 0.2 s) - no snap.
TEST(AnimIkHeldFootTurnEases) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = SwivelRecipe(bones, 30.0f);
  const AnimClip base = Run(r, bones);
  const std::array<float, 3> offset = OffsetFor(base, bones, "lthigh_g", "pelvis_g", LegLength(bones), At(base, bones, "lfoot_g", 0.3f), 0.3f);
  r.targets["left_foot"] = {Key(2.3f, offset, true), Key(2.8f, offset)};
  const AnimClip held = Run(r, bones);
  float fastest = 0.0f;
  for (int f = 1; f <= 30; ++f) fastest = fmaxf(fastest, DegreesBetween(FootTurn(held, bones, (f - 1) / 30.0f), FootTurn(held, bones, f / 30.0f)));
  CHECK_EQ(fastest < 5.0f, true);  // measured 3.65 at the release (15 degrees caught up over 0.2 s); a snap is ~15
}
