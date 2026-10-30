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
