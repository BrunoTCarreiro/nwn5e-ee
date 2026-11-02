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

// The left hand's place and turn in the right hand's (rhand_g's) frame.
struct HandInHand {
  Vec3 p;
  Quat q;
};
HandInHand LeftInRight(const Pose& p, const std::vector<AnimNode>& bones) {
  const int r = IndexOf(bones, "rhand_g"), l = IndexOf(bones, "lhand_g");
  return {Rotate(QConj(p.wq[r]), Sub(p.wp[l], p.wp[r])), QMul(QConj(p.wq[r]), p.wq[l])};
}
float TurnDegrees(const Quat& a, const Quat& b) {
  const float d = fabsf(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]);
  return 2.0f * acosf(d > 1.0f ? 1.0f : d) * 180.0f / kPi;
}

// The two-handed grip (#112): the left hand held where the stance holds it against the right hand, place and turn, all
// through - while the torso turns 30 deg and the right forearm 40 deg; without the grip it drifts off. Breaks caught:
// the grip skipped, the place without the turn, the early path taken.
TEST(AnimIkGripHoldsTheOtherHand) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 30.0f);
  for (AnimKeyTrack& t : r.keys["a_ik"]) {
    if (t.bone != "rforearm_g") continue;
    const float half = 40.0f * kPi / 360.0f;
    t.keys.values = {{0.0f, 0.0f, 0.0f, 1.0f}, {sinf(half), 0.0f, 0.0f, cosf(half)}};
  }
  const HandInHand stance = LeftInRight(PoseOf(IkStance(bones), bones, 0.0f), bones);
  AnimRecipe held = r;
  held.grip = true;
  const AnimClip out = Run(held, bones), loose = Run(r, bones);
  float worst_place = 0.0f, worst_turn = 0.0f;
  for (const float t : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
    const HandInHand g = LeftInRight(PoseOf(out, bones, t), bones);
    worst_place = std::max(worst_place, Dist(g.p, stance.p));
    worst_turn = std::max(worst_turn, TurnDegrees(g.q, stance.q));
  }
  CHECK_EQ(worst_place < 1e-3f, true);
  CHECK_EQ(worst_turn < 0.5f, true);
  CHECK_EQ(Dist(LeftInRight(PoseOf(loose, bones, 1.0f), bones).p, stance.p) > 0.05f, true);
}

// The grip's give-way (#112 addendum): the off arm's limit - the straighter of 95% of its length and its span in the
// start pose; the goal's distance from the left shoulder in a pose; the right arm swung `degrees` about X over the second.
float GripLimit(const std::vector<AnimNode>& bones) {
  const Pose st = PoseOf(IkStance(bones), bones, 0.0f);
  return std::max(0.95f * ArmLength(bones), Dist(st.wp[IndexOf(bones, "lhand_g")], st.wp[IndexOf(bones, "lbicep_g")]));
}
float GoalReach(const Pose& p, const std::vector<AnimNode>& bones, const HandInHand& grip) {
  const int r = IndexOf(bones, "rhand_g");
  return Dist(Add(p.wp[r], Rotate(p.wq[r], grip.p)), p.wp[IndexOf(bones, "lbicep_g")]);
}
Quat AboutY(float degrees) { return AxisAngleQ({0.0f, 1.0f, 0.0f}, degrees * kPi / 180.0f); }
// The right arm swung sideways: in across the body to `in_deg` at `at` (the left arm reaches), then out to `out_deg` at
// the end (out of its reach). Sideways, since this body's grip mirrors the hands: a swing about X keeps the reach.
AnimRecipe SwingRecipe(const std::vector<AnimNode>& bones, float in_deg, float at, float out_deg) {
  AnimRecipe r = IkRecipe(bones, 0.0f);
  for (AnimKeyTrack& t : r.keys["a_ik"]) {
    if (t.bone != "rbicep_g") continue;
    t.keys.times = {0.0f, at, 1.0f};
    t.keys.values = {{0.0f, 0.0f, 0.0f, 1.0f}, AboutY(in_deg), AboutY(out_deg)};
  }
  return r;
}

// A turn split about an axis (degrees): its twist about the axis and its swing (the rest).
void SplitDegrees(const Quat& q, const Vec3& axis, float* twist, float* swing) {
  const float p = q[0] * axis[0] + q[1] * axis[1] + q[2] * axis[2];
  const Quat tw = QNormalized({axis[0] * p, axis[1] * p, axis[2] * p, q[3]});
  const Quat sw = QMul(q, QConj(tw));
  *twist = 2.0f * acosf(std::min(1.0f, fabsf(tw[3]))) * 180.0f / kPi;
  *swing = 2.0f * acosf(std::min(1.0f, fabsf(sw[3]))) * 180.0f / kPi;
}

// Where the left arm can't reach the grip, the weapon gives way (#112 addenda 1-2): on every frame the left hand is on
// the grip (place and turn) with the left arm within its limit; the weapon's turn against the take's stays within the
// forearm's roll (80 deg) and the wrist's (60 deg); the right hand only ever comes nearer the left shoulder; the report
// names the hand's move as "grip" and leaves no left-hand shortfall. Breaks caught: the give-way off, the turn uncapped.
TEST(AnimGripWeaponHandGivesWay) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  const AnimRecipe r = SwingRecipe(bones, 40.0f, 0.4f, -90.0f);
  const HandInHand stance = LeftInRight(PoseOf(IkStance(bones), bones, 0.0f), bones);
  const AnimClip loose = Run(r, bones);
  CHECK_EQ(GoalReach(PoseOf(loose, bones, 1.0f), bones, stance) > GripLimit(bones) + 0.05f, true);  // out of reach alone
  AnimRecipe held = r;
  held.grip = true;
  std::vector<AnimIkShort> report;
  const AnimClip out = Run(held, bones, &report);
  const int rh = IndexOf(bones, "rhand_g"), lu = IndexOf(bones, "lbicep_g"), lh = IndexOf(bones, "lhand_g");
  const int rf = IndexOf(bones, "rforearm_g");
  float worst_place = 0.0f, worst_turn = 0.0f, worst_span = 0.0f, worst_twist = 0.0f, worst_swing = 0.0f, worst_away = 0.0f;
  for (int f = 0; f <= 30; ++f) {
    const float t = static_cast<float>(f) / 30.0f;
    const Pose g = PoseOf(out, bones, t), l = PoseOf(loose, bones, t);
    const HandInHand h = LeftInRight(g, bones);
    worst_place = std::max(worst_place, Dist(h.p, stance.p));
    worst_turn = std::max(worst_turn, TurnDegrees(h.q, stance.q));
    worst_span = std::max(worst_span, Dist(g.wp[lh], g.wp[lu]) - GripLimit(bones));
    float twist = 0.0f, swing = 0.0f;
    SplitDegrees(QMul(g.wq[rh], QConj(l.wq[rh])), Normalized(Sub(l.wp[rh], l.wp[rf])), &twist, &swing);
    worst_twist = std::max(worst_twist, twist);
    worst_swing = std::max(worst_swing, swing);
    worst_away = std::max(worst_away, Dist(g.wp[rh], g.wp[lu]) - Dist(l.wp[rh], l.wp[lu]));
  }
  CHECK_EQ(worst_place < 1e-3f, true);
  CHECK_EQ(worst_turn < 0.5f, true);
  CHECK_EQ(worst_span < 5e-3f, true);  // the hand's rounds (kGripRounds) leave the arm at most a few mm past its limit
  CHECK_EQ(worst_twist < 81.0f, true);
  CHECK_EQ(worst_swing < 61.0f, true);
  CHECK_EQ(worst_away < 1e-3f, true);
  bool named = false, left_short = false;
  for (const AnimIkShort& s : report) {
    named = named || (s.limb == "grip" && s.metres > 0.05f);
    left_short = left_short || s.limb == "left_hand";
  }
  CHECK_EQ(named, true);
  CHECK_EQ(left_short, false);
}

// Where the left arm reaches the grip within its limit, the right arm is the clip's without the grip - the rule
// changes nothing it doesn't need to. Break caught: a give-way on every frame.
TEST(AnimGripUntouchedWhereItReaches) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  const AnimRecipe r = SwingRecipe(bones, 40.0f, 0.4f, -90.0f);
  const HandInHand stance = LeftInRight(PoseOf(IkStance(bones), bones, 0.0f), bones);
  AnimRecipe held = r;
  held.grip = true;
  const AnimClip out = Run(held, bones), loose = Run(r, bones);
  int within = 0;
  float worst = 0.0f;
  for (int f = 0; f <= 30; ++f) {
    const float t = static_cast<float>(f) / 30.0f;
    const Pose g = PoseOf(out, bones, t), l = PoseOf(loose, bones, t);
    if (GoalReach(l, bones, stance) > GripLimit(bones) - 1e-3f) continue;
    ++within;
    for (const char* n : {"rbicep_g", "rforearm_g", "rhand_g"}) worst = std::max(worst, TurnDegrees(g.lq[IndexOf(bones, n)], l.lq[IndexOf(bones, n)]));
  }
  CHECK_EQ(within >= 5, true);
  CHECK_EQ(worst < 0.01f, true);
}

// The grip holds through the clip's blends (#112 review I1): the blends turn each bone on its own, so a grip solved
// before them drifts inside their windows (a sideways swing: mirrored arms would blend in step). Break caught: the grip
// solved before Step 9.
TEST(AnimGripHoldsThroughTheBlends) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = SwingRecipe(bones, 20.0f, 0.5f, 40.0f);  // in across the body, within reach all through
  r.grip = true;
  r.blend_in = 0.3f;
  r.blend_out = 0.3f;
  const HandInHand stance = LeftInRight(PoseOf(IkStance(bones), bones, 0.0f), bones);
  const AnimClip out = Run(r, bones);
  float worst_place = 0.0f, worst_turn = 0.0f;
  for (int f = 0; f <= 30; ++f) {
    const HandInHand h = LeftInRight(PoseOf(out, bones, static_cast<float>(f) / 30.0f), bones);
    worst_place = std::max(worst_place, Dist(h.p, stance.p));
    worst_turn = std::max(worst_turn, TurnDegrees(h.q, stance.q));
  }
  CHECK_EQ(worst_place < 1e-3f, true);
  CHECK_EQ(worst_turn < 0.5f, true);
}

// A start pose whose off arm is straighter than 95% of its length (this one's: ~96.6%) keeps its own: the clip's ends -
// the start pose - don't move. Break caught: the limit without the start pose's span.
TEST(AnimGripEndsOnTheStance) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  const Pose st = PoseOf(IkStance(bones), bones, 0.0f);
  const int rh = IndexOf(bones, "rhand_g"), lh = IndexOf(bones, "lhand_g"), lu = IndexOf(bones, "lbicep_g");
  CHECK_EQ(Dist(st.wp[lh], st.wp[lu]) > 0.95f * ArmLength(bones), true);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  r.grip = true;
  const AnimClip out = Run(r, bones);
  for (const float t : {0.0f, 1.0f}) {
    const Pose p = PoseOf(out, bones, t);
    CHECK_EQ(Dist(p.wp[rh], st.wp[rh]) < 1e-4f, true);
    CHECK_EQ(Dist(p.wp[lh], st.wp[lh]) < 1e-4f, true);
  }
}

// Under the grip a left-hand target does nothing (the grip wins): the clip is the grip's alone, and the target's
// shortfall isn't reported. Break caught: the target solved first (its bend survives into the grip).
TEST(AnimGripSkipsALeftHandTarget) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe held = IkRecipe(bones, 30.0f);
  held.grip = true;
  AnimRecipe aimed = held;
  aimed.targets["left_hand"] = {Key(2.5f, {0.8f, 0.9f, -0.2f})};
  std::vector<AnimIkShort> report;
  const AnimClip a = Run(aimed, bones, &report), b = Run(held, bones);
  CHECK_EQ(SameKeys(a, b), true);
  bool left_short = false;
  for (const AnimIkShort& s : report) left_short = left_short || s.limb == "left_hand";
  CHECK_EQ(left_short, false);
}

// The weapon turns before the hand moves (#112 addendum 2, the owner's p2 / t3): where turning within its caps brings
// the grip to the left arm, the right hand keeps its place, and over the clip the hand moves under half of what moving
// alone needs (here ~10%). Break caught: no turn (the hand moves the whole way on every frame past reach).
TEST(AnimGripTurnsBeforeItMoves) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  const AnimRecipe r = SwingRecipe(bones, 40.0f, 0.4f, -90.0f);
  const HandInHand stance = LeftInRight(PoseOf(IkStance(bones), bones, 0.0f), bones);
  AnimRecipe held = r;
  held.grip = true;
  const AnimClip out = Run(held, bones), loose = Run(r, bones);
  const int rh = IndexOf(bones, "rhand_g"), rf = IndexOf(bones, "rforearm_g");
  int turned_only = 0;
  float moved = 0.0f, alone = 0.0f;
  for (int f = 0; f <= 30; ++f) {
    const float t = static_cast<float>(f) / 30.0f;
    const Pose g = PoseOf(out, bones, t), l = PoseOf(loose, bones, t);
    const float excess = GoalReach(l, bones, stance) - GripLimit(bones);
    if (excess <= 1e-3f) continue;
    alone += excess;
    moved += Dist(g.wp[rh], l.wp[rh]);
    float twist = 0.0f, swing = 0.0f;
    SplitDegrees(QMul(g.wq[rh], QConj(l.wq[rh])), Normalized(Sub(l.wp[rh], l.wp[rf])), &twist, &swing);
    if (Dist(g.wp[rh], l.wp[rh]) <= 1e-3f && twist + swing > 1.0f) ++turned_only;
  }
  CHECK_EQ(turned_only >= 3, true);
  CHECK_EQ(alone > 0.5f, true);
  CHECK_EQ(moved < 0.5f * alone, true);
}

// The weapon's twist goes to the forearm's roll (#112 addendum 2): where the turn alone brings the grip (the hand keeps
// its place), the wrist - the hand against the forearm - turns from the take's by no more than its cap (60 deg), and the
// forearm rolling moves neither the elbow nor the hand. (Where the hand moves, the wrist keeps its caps only while
// they converge - see hold_grip.) Break caught: the whole turn on the wrist.
TEST(AnimGripForearmTakesTheRoll) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  const AnimRecipe r = SwingRecipe(bones, 40.0f, 0.4f, -90.0f);
  AnimRecipe held = r;
  held.grip = true;
  const AnimClip out = Run(held, bones), loose = Run(r, bones);
  const int rh = IndexOf(bones, "rhand_g"), rf = IndexOf(bones, "rforearm_g");
  float worst_wrist = 0.0f, worst_elbow = 0.0f, most_roll = 0.0f;
  for (int f = 0; f <= 30; ++f) {
    const float t = static_cast<float>(f) / 30.0f;
    const Pose g = PoseOf(out, bones, t), l = PoseOf(loose, bones, t);
    most_roll = std::max(most_roll, TurnDegrees(g.lq[rf], l.lq[rf]));
    if (Dist(g.wp[rh], l.wp[rh]) > 1e-3f) continue;
    worst_wrist = std::max(worst_wrist, TurnDegrees(g.lq[rh], l.lq[rh]));
    worst_elbow = std::max(worst_elbow, Dist(g.wp[rf], l.wp[rf]));
  }
  CHECK_EQ(worst_wrist < 60.5f, true);
  CHECK_EQ(most_roll > 5.0f, true);
  CHECK_EQ(worst_elbow < 1e-3f, true);
}

// The off arm doesn't depend on the take's free arm (#112 second review C1): under the grip the left arm is solved from
// the start pose's arm, so two takes that differ only in their left arm give the same left arm, key for key - the old
// solve started from the take's arm and flipped with it (n5_rage's polearm version: 92-179 deg a frame at the strike).
// Break caught: the left arm solved from the take's.
TEST(AnimGripOffArmIgnoresTheTake) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe calm = SwingRecipe(bones, 40.0f, 0.4f, -90.0f), wild = calm;
  for (AnimKeyTrack& t : wild.keys["a_ik"]) {
    if (t.bone == "lbicep_g") t.keys.values = {{0.0f, 0.0f, 0.0f, 1.0f}, QMul(TurnX(170.0f), AboutY(25.0f))};
    if (t.bone == "lforearm_g") t.keys.values = {{0.0f, 0.0f, 0.0f, 1.0f}, TurnX(-60.0f)};
  }
  calm.grip = wild.grip = true;
  const AnimClip a = Run(calm, bones), b = Run(wild, bones);
  float worst = 0.0f;
  for (int f = 0; f <= 30; ++f) {
    const Pose pa = PoseOf(a, bones, static_cast<float>(f) / 30.0f), pb = PoseOf(b, bones, static_cast<float>(f) / 30.0f);
    for (const char* n : {"lbicep_g", "lforearm_g", "lhand_g"}) worst = std::max(worst, TurnDegrees(pa.lq[IndexOf(bones, n)], pb.lq[IndexOf(bones, n)]));
  }
  CHECK_EQ(worst < 0.01f, true);
}

// The arms turn smoothly under the grip: with the take's left arm swung up and back and the weapon arm swung out past
// reach, no arm bone jumps (over 45 deg in a frame: the flips were 92-179), and between frames the grip holds within
// 3 cm.
TEST(AnimGripArmsTurnSmoothly) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = SwingRecipe(bones, 40.0f, 0.4f, -90.0f);
  for (AnimKeyTrack& t : r.keys["a_ik"]) {
    if (t.bone == "lbicep_g") t.keys.values = {{0.0f, 0.0f, 0.0f, 1.0f}, QMul(TurnX(170.0f), AboutY(25.0f))};
  }
  r.grip = true;
  const HandInHand stance = LeftInRight(PoseOf(IkStance(bones), bones, 0.0f), bones);
  const AnimClip out = Run(r, bones);
  float worst_step = 0.0f, worst_mid = 0.0f;
  for (int f = 0; f < 30; ++f) {
    const float a = static_cast<float>(f) / 30.0f, b = static_cast<float>(f + 1) / 30.0f;
    const Pose ga = PoseOf(out, bones, a), gb = PoseOf(out, bones, b);
    for (const char* n : {"lbicep_g", "lforearm_g", "rbicep_g", "rforearm_g"}) worst_step = std::max(worst_step, TurnDegrees(ga.lq[IndexOf(bones, n)], gb.lq[IndexOf(bones, n)]));
    worst_mid = std::max(worst_mid, Dist(LeftInRight(PoseOf(out, bones, (a + b) / 2.0f), bones).p, stance.p));
  }
  CHECK_EQ(worst_step < 45.0f, true);
  CHECK_EQ(worst_mid < 0.03f, true);
}

// A start pose whose elbows bend sideways (about the bone's Y, not toward the back): at the clip's ends - the start pose
// - both elbows stay on it (#112 second review I1: the solve's torso hint swivelled the left one 2-4 cm). Break caught:
// the left arm solved from the take's arm with the torso hint.
TEST(AnimGripElbowsOnTheStanceAtTheEnds) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimClip side = IkStance(bones);
  for (const char* n : {"lforearm_g", "rforearm_g"}) {
    const int i = IndexOf(bones, n);
    side.nodes[i].orientation.values = {AboutY(n[0] == 'l' ? -30.0f : 30.0f)};
  }
  const Pose st = PoseOf(side, bones, 0.0f);
  for (const float follow : {1.0f, 0.25f}) {  // the take's hand, and the weapon held by the body (addendum 3)
    AnimRecipe r = IkRecipe(bones, 0.0f);
    r.grip = true;
    r.grip_follow = follow;
    AnimClip out;
    std::string err;
    CHECK_EQ(AssembleAnimClip(r, "a_ik", bones, side, &out, &err, nullptr), true);
    float worst = 0.0f;
    for (const float t : {0.0f, 1.0f}) {
      const Pose p = PoseOf(out, bones, t);
      for (const char* n : {"lforearm_g", "rforearm_g", "lhand_g", "rhand_g"}) worst = std::max(worst, Dist(p.wp[IndexOf(bones, n)], st.wp[IndexOf(bones, n)]));
    }
    CHECK_EQ(worst < 1e-3f, true);
  }
}

// The grip on the IK path (#112 second review I3): a hips target sends the clip through the IK pass, and the grip still
// holds on every frame. Break caught: the pass left off the IK path.
TEST(AnimGripOnTheIkPath) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = SwingRecipe(bones, 20.0f, 0.5f, 40.0f);
  r.targets["hips"] = {Key(2.5f, {0.0f, 0.0f, -0.05f})};
  r.grip = true;
  const HandInHand stance = LeftInRight(PoseOf(IkStance(bones), bones, 0.0f), bones);
  const AnimClip out = Run(r, bones);
  float worst = 0.0f;
  for (int f = 0; f <= 30; ++f) worst = std::max(worst, Dist(LeftInRight(PoseOf(out, bones, static_cast<float>(f) / 30.0f), bones).p, stance.p));
  CHECK_EQ(worst < 1e-3f, true);
}

// The grip through a loop's crossfade (#112 first review I1): an in-place loop crossfades its end into its start after
// the IK; the grip runs after it. Break caught: the grip solved before Step 9.
TEST(AnimGripThroughALoop) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = SwingRecipe(bones, 20.0f, 0.5f, 40.0f);
  r.grip = true;
  r.loop = true;
  r.loop_blend = 0.3f;
  const HandInHand stance = LeftInRight(PoseOf(IkStance(bones), bones, 0.0f), bones);
  const AnimClip out = Run(r, bones);
  float worst = 0.0f;
  for (int f = 0; f <= 30; ++f) worst = std::max(worst, Dist(LeftInRight(PoseOf(out, bones, static_cast<float>(f) / 30.0f), bones).p, stance.p));
  CHECK_EQ(worst < 1e-3f, true);
}

// The weapon held by the body (#112 addendum 3, the owner's f1): the weapon hand's place and turn against the torso are
// the start pose's mixed toward the take's by the follow share. Its place and turn against the torso at t.
struct HandOnBody {
  Vec3 p;
  Quat q;
};
HandOnBody RightOnTorso(const Pose& p, const std::vector<AnimNode>& bones) {
  const int r = IndexOf(bones, "rhand_g"), t = IndexOf(bones, "torso_g");
  return {Rotate(QConj(p.wq[t]), Sub(p.wp[r], p.wp[t])), QMul(QConj(p.wq[t]), p.wq[r])};
}

// Share 0: the weapon hand stays on the start pose's place and turn against the torso all through, however the take's
// right arm swings (in across the body, then out past reach), and the left hand holds the grip. Break caught: the share
// ignored.
TEST(AnimGripFollowNothingHoldsTheWeapon) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = SwingRecipe(bones, 40.0f, 0.4f, -90.0f);
  r.grip = true;
  r.grip_follow = 0.0f;
  const HandOnBody st = RightOnTorso(PoseOf(IkStance(bones), bones, 0.0f), bones);
  const HandInHand grip = LeftInRight(PoseOf(IkStance(bones), bones, 0.0f), bones);
  const AnimClip out = Run(r, bones);
  float worst_place = 0.0f, worst_turn = 0.0f, worst_grip = 0.0f;
  for (int f = 0; f <= 30; ++f) {
    const Pose p = PoseOf(out, bones, static_cast<float>(f) / 30.0f);
    const HandOnBody h = RightOnTorso(p, bones);
    worst_place = std::max(worst_place, Dist(h.p, st.p));
    worst_turn = std::max(worst_turn, TurnDegrees(h.q, st.q));
    worst_grip = std::max(worst_grip, Dist(LeftInRight(p, bones).p, grip.p));
  }
  CHECK_EQ(worst_place < 1e-3f, true);
  CHECK_EQ(worst_turn < 0.5f, true);
  CHECK_EQ(worst_grip < 1e-3f, true);
}

// Share 0.5: where nothing else gives way (the swing stays within reach), the weapon hand is halfway between the start
// pose's and the take's, in place and in turn, against the torso. Break caught: the place without the turn.
TEST(AnimGripFollowHalf) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = SwingRecipe(bones, 30.0f, 0.5f, 30.0f);
  const AnimClip loose = Run(r, bones);
  r.grip = true;
  r.grip_follow = 0.5f;
  const HandOnBody st = RightOnTorso(PoseOf(IkStance(bones), bones, 0.0f), bones);
  const AnimClip out = Run(r, bones);
  float worst_place = 0.0f, worst_turn = 0.0f, most_apart = 0.0f;
  for (int f = 0; f <= 30; ++f) {
    const float t = static_cast<float>(f) / 30.0f;
    const HandOnBody h = RightOnTorso(PoseOf(out, bones, t), bones), take = RightOnTorso(PoseOf(loose, bones, t), bones);
    most_apart = std::max(most_apart, Dist(take.p, st.p));
    worst_place = std::max(worst_place, Dist(h.p, Scale(Add(st.p, take.p), 0.5f)));
    float a = 0.0f, b = 0.0f;
    a = TurnDegrees(h.q, st.q);
    b = TurnDegrees(h.q, take.q);
    worst_turn = std::max(worst_turn, fabsf(a - b));
  }
  CHECK_EQ(most_apart > 0.1f, true);
  CHECK_EQ(worst_place < 0.02f, true);
  CHECK_EQ(worst_turn < 1.0f, true);
}

// A body without the right hand: no grip, the clip as without it.
TEST(AnimIkGripMissingHandSkipped) {
  std::vector<AnimNode> bones = WholeBody(1.0f);
  bones[IndexOf(bones, "rhand_g")].name = "rhand_x";
  const AnimRecipe r = IkRecipe(bones, 30.0f);
  AnimRecipe held = r;
  held.grip = true;
  CHECK_EQ(SameKeys(Run(r, bones), Run(held, bones)), true);
}

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

// Pose keys' turns (#90): degrees about the stance axes, X first, then Y, then Z (R = Rz * Ry * Rx), and back.
// Breaks caught: the order reversed, an axis swapped, the gimbal case read as NaN.
namespace {
bool SameTurn(const Quat& a, const Quat& b, float tolerance) {  // up to the quaternion's sign
  const float s = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3] < 0.0f ? -1.0f : 1.0f;
  float worst = 0.0f;
  for (int i = 0; i < 4; ++i) worst = fmaxf(worst, fabsf(a[i] - s * b[i]));
  return worst < tolerance;
}
bool Near(const Vec3& a, const Vec3& b) { return Dist(a, b) < 1e-5f; }
}  // namespace

TEST(TurnFromDegreesAxes) {
  CHECK_EQ(SameTurn(TurnFromDegrees({90.0f, 0.0f, 0.0f}), AxisAngleQ({1.0f, 0.0f, 0.0f}, kPi / 2.0f), 1e-5f), true);
  CHECK_EQ(SameTurn(TurnFromDegrees({0.0f, 90.0f, 0.0f}), AxisAngleQ({0.0f, 1.0f, 0.0f}, kPi / 2.0f), 1e-5f), true);
  CHECK_EQ(SameTurn(TurnFromDegrees({0.0f, 0.0f, 90.0f}), AxisAngleQ({0.0f, 0.0f, 1.0f}, kPi / 2.0f), 1e-5f), true);
  CHECK_EQ(Near(Rotate(TurnFromDegrees({90.0f, 0.0f, 0.0f}), {0.0f, 1.0f, 0.0f}), {0.0f, 0.0f, 1.0f}), true);
  CHECK_EQ(Near(Rotate(TurnFromDegrees({0.0f, 0.0f, 0.0f}), {0.3f, -0.2f, 0.9f}), {0.3f, -0.2f, 0.9f}), true);
}

TEST(TurnFromDegreesOrder) {
  // X first: +Y goes to +Z, then Y's quarter turn takes +Z to +X. (Y first would leave +Y at +Z.)
  CHECK_EQ(Near(Rotate(TurnFromDegrees({90.0f, 90.0f, 0.0f}), {0.0f, 1.0f, 0.0f}), {1.0f, 0.0f, 0.0f}), true);
  // Y, then Z: +X goes to -Z by Y's quarter, and Z leaves it there; +Z goes to +X, then to +Y.
  CHECK_EQ(Near(Rotate(TurnFromDegrees({0.0f, 90.0f, 90.0f}), {0.0f, 0.0f, 1.0f}), {0.0f, 1.0f, 0.0f}), true);
}

TEST(eulerRoundTripAtGimbal) {
  const Vec3 cases[] = {{10.0f, 20.0f, 30.0f}, {-170.0f, 45.0f, 120.0f}, {25.0f, 90.0f, -40.0f}, {0.0f, -90.0f, 10.0f}, {0.0f, 0.0f, 0.0f}};
  for (const Vec3& c : cases) {
    const Quat q = TurnFromDegrees(c);
    const Vec3 back = DegreesFromTurn(q);
    CHECK_EQ(isfinite(back[0]) && isfinite(back[1]) && isfinite(back[2]), true);
    CHECK_EQ(back[1] >= -90.0f && back[1] <= 90.0f, true);
    CHECK_EQ(SameTurn(TurnFromDegrees(back), q, 1e-4f), true);  // ~0.01 degrees
  }
  // Away from the gimbal the numbers themselves come back.
  const Vec3 plain = DegreesFromTurn(TurnFromDegrees({10.0f, 20.0f, 30.0f}));
  CHECK_EQ(fabsf(plain[0] - 10.0f) < 1e-3f && fabsf(plain[1] - 20.0f) < 1e-3f && fabsf(plain[2] - 30.0f) < 1e-3f, true);
}

// Step 4, pose keys (#90): push turns the capture further, replace sets the turn, keys blend by smoothstep and
// ease in and out of the capture; turns are about the part's stance axes. Breaks caught: push applied after
// the capture (the bone's own axes), replace ignored, no ease, the stance axes ignored, the clock off by the
// take's start.
namespace {
AnimPoseKey PoseKey(float t, const std::array<float, 3>& turn, bool replace = false, float ease = 0.2f) {
  AnimPoseKey k;
  k.t = t;
  k.turn = turn;
  k.replace = replace;
  k.ease = ease;
  return k;
}
Quat LocalTurn(const AnimClip& clip, const std::vector<AnimNode>& bones, const char* name, float t) {
  return PoseOf(clip, bones, t).lq[IndexOf(bones, name)];
}
}  // namespace

TEST(AnimPosePushAddsToCapture) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 30.0f);
  const AnimClip base = Run(r, bones);
  // About X, while the capture turns the torso about Z: the key turns about the stance's X, before the
  // capture's turn (R * delta), not about the torso's own turned X.
  r.poses["torso_g"] = {PoseKey(2.5f, {15.0f, 0.0f, 0.0f})};
  const AnimClip out = Run(r, bones);
  const Quat want = QMul(TurnFromDegrees({15.0f, 0.0f, 0.0f}), LocalTurn(base, bones, "torso_g", 0.5f));
  CHECK_EQ(SameTurn(LocalTurn(out, bones, "torso_g", 0.5f), want, 4e-4f), true);
  CHECK_EQ(SameTurn(LocalTurn(out, bones, "torso_g", 0.25f), LocalTurn(base, bones, "torso_g", 0.25f), 1e-5f), true);
  CHECK_EQ(SameTurn(LocalTurn(out, bones, "torso_g", 0.75f), LocalTurn(base, bones, "torso_g", 0.75f), 1e-5f), true);
  const float partly = DegreesBetween(LocalTurn(out, bones, "torso_g", 0.4f), LocalTurn(base, bones, "torso_g", 0.4f));
  CHECK_EQ(partly > 1.0f && partly < 14.0f, true);  // easing in
}

TEST(AnimPoseReplaceIgnoresCapture) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 30.0f);
  r.poses["torso_g"] = {PoseKey(2.5f, {10.0f, 0.0f, 0.0f}, true)};
  // The torso's stance turn is its rest (identity): replace sets it to the key's turn, the capture's 15 degrees
  // about Z there gone.
  CHECK_EQ(SameTurn(LocalTurn(Run(r, bones), bones, "torso_g", 0.5f), TurnFromDegrees({10.0f, 0.0f, 0.0f}), 4e-4f), true);
}

TEST(AnimPoseEaseAndBetweenKeys) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe still = IkRecipe(bones, 0.0f);
  const AnimClip base = Run(still, bones);
  // Ease 0: the key's own frame only.
  AnimRecipe step = still;
  step.poses["torso_g"] = {PoseKey(2.5f, {0.0f, 0.0f, 15.0f}, false, 0.0f)};
  const AnimClip stepped = Run(step, bones);
  CHECK_EQ(SameTurn(LocalTurn(stepped, bones, "torso_g", 0.5f), TurnFromDegrees({0.0f, 0.0f, 15.0f}), 4e-4f), true);
  CHECK_EQ(SameTurn(LocalTurn(stepped, bones, "torso_g", 14.0f / 30.0f), LocalTurn(base, bones, "torso_g", 14.0f / 30.0f), 1e-5f), true);
  CHECK_EQ(SameTurn(LocalTurn(stepped, bones, "torso_g", 16.0f / 30.0f), LocalTurn(base, bones, "torso_g", 16.0f / 30.0f), 1e-5f), true);
  // Two keys 0.4 s apart: halfway between, the halfway turn.
  AnimRecipe two = still;
  two.poses["torso_g"] = {PoseKey(2.3f, {0.0f, 0.0f, 10.0f}), PoseKey(2.7f, {0.0f, 0.0f, 30.0f})};
  CHECK_EQ(SameTurn(LocalTurn(Run(two, bones), bones, "torso_g", 0.5f), TurnFromDegrees({0.0f, 0.0f, 20.0f}), 4e-4f), true);
  // Push then replace, the torso turning 30 about Z over the second (15 at 0.5): halfway between their results,
  // push (10 + 15 = 25) and replace (10) - 17.5.
  AnimRecipe mixed = IkRecipe(bones, 30.0f);
  mixed.poses["torso_g"] = {PoseKey(2.3f, {0.0f, 0.0f, 10.0f}), PoseKey(2.7f, {0.0f, 0.0f, 10.0f}, true)};
  CHECK_EQ(SameTurn(LocalTurn(Run(mixed, bones), bones, "torso_g", 0.5f), TurnFromDegrees({0.0f, 0.0f, 17.5f}), 4e-4f), true);
}

TEST(AnimPoseNoKeysIdentical) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  const AnimRecipe plain = IkRecipe(bones, 30.0f);
  AnimRecipe empty = plain;
  empty.poses["torso_g"] = {};  // a part with no keys
  CHECK_EQ(SameKeys(Run(plain, bones), Run(empty, bones)), true);
  std::vector<AnimNode> headless = bones;
  headless[IndexOf(bones, "lforearm_g")].name = "lforearm_x";  // a part this body hasn't
  AnimRecipe absent = IkRecipe(headless, 30.0f);
  const AnimRecipe absent_plain = absent;
  absent.poses["lforearm_g"] = {PoseKey(2.5f, {20.0f, 0.0f, 0.0f})};
  CHECK_EQ(SameKeys(Run(absent_plain, headless), Run(absent, headless)), true);
}

TEST(AnimPoseCarriesInStanceAxes) {
  // A pelvis resting turned 22 degrees about Z (as a_dfa's is): a key's X is the pelvis's own stance X.
  std::vector<AnimNode> bones = WholeBody(1.0f);
  const float half = 22.0f * kPi / 360.0f;
  bones[IndexOf(bones, "pelvis_g")].orientation.values = {{0.0f, 0.0f, sinf(half), cosf(half)}};
  AnimRecipe r = IkRecipe(bones, 0.0f);
  r.poses["pelvis_g"] = {PoseKey(2.5f, {15.0f, 0.0f, 0.0f})};
  const Quat want = QMul(TurnFromDegrees({0.0f, 0.0f, 22.0f}), TurnFromDegrees({15.0f, 0.0f, 0.0f}));
  CHECK_EQ(SameTurn(LocalTurn(Run(r, bones), bones, "pelvis_g", 0.5f), want, 4e-4f), true);
}

TEST(poseUnderIkTargetDeterministic) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  const AnimClip base = Run(r, bones);
  const Vec3 place = Add(At(base, bones, "lhand_g", 0.5f), {0.1f, 0.15f, 0.1f});
  r.targets["left_hand"] = {Key(2.5f, OffsetFor(base, bones, "lbicep_g", "torso_g", ArmLength(bones), place, 0.5f))};
  r.poses["lforearm_g"] = {PoseKey(2.5f, {30.0f, 0.0f, 0.0f})};
  const AnimClip once = Run(r, bones);
  CHECK_EQ(Dist(At(once, bones, "lhand_g", 0.5f), place) < 1e-3f, true);  // IK wins on its chain
  CHECK_EQ(SameKeys(once, Run(r, bones)), true);
}

TEST(loopStillMeetsWithPoses) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 30.0f);
  r.loop = true;
  r.loop_blend = 0.2f;
  r.poses["torso_g"] = {PoseKey(2.9f, {0.0f, 0.0f, 20.0f})};
  const AnimClip out = Run(r, bones);
  bool meets = true;
  for (const AnimNode& n : out.nodes) {
    if (!n.has_orientation || n.orientation.values.size() < 2) continue;
    const auto& a = n.orientation.values.front();
    const auto& b = n.orientation.values.back();
    float same = 0.0f, flipped = 0.0f;
    for (int i = 0; i < 4; ++i) {
      same = fmaxf(same, fabsf(a[i] - b[i]));
      flipped = fmaxf(flipped, fabsf(a[i] + b[i]));
    }
    meets = meets && fminf(same, flipped) < 1e-5f;
  }
  CHECK_EQ(meets, true);
}

// The pelvis keeps the feet (#90: "pose keys turn parts; they don't move the body"): a turned pelvis leaves
// the feet where the capture put them, the legs re-solved; a turned leg moves its own foot on purpose.
// Breaks caught: the legs left unsolved on pelvis frames, the planted place taken from the posed pose.
TEST(AnimPosePelvisKeepsFeet) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  const AnimClip base = Run(r, bones);
  r.poses["pelvis_g"] = {PoseKey(2.5f, {0.0f, 0.0f, 25.0f})};
  const AnimClip out = Run(r, bones);
  for (const char* foot : {"lfoot_g", "rfoot_g"}) {
    for (float t : {0.4f, 0.5f, 0.6f}) {
      CHECK_EQ(Dist(At(out, bones, foot, t), At(base, bones, foot, t)) < 1e-3f, true);
      const int i = IndexOf(bones, foot);  // the foot's turn in the world stays too: it doesn't swivel with the pelvis
      CHECK_EQ(SameTurn(PoseOf(out, bones, t).wq[i], PoseOf(base, bones, t).wq[i], 4e-4f), true);
    }
  }
  const Quat want = QMul(TurnFromDegrees({0.0f, 0.0f, 25.0f}), LocalTurn(base, bones, "pelvis_g", 0.5f));
  CHECK_EQ(SameTurn(LocalTurn(out, bones, "pelvis_g", 0.5f), want, 4e-4f), true);
}

TEST(AnimPoseLegPoseMovesItsFoot) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  const AnimClip base = Run(r, bones);
  r.poses["lthigh_g"] = {PoseKey(2.5f, {40.0f, 0.0f, 0.0f})};
  const AnimClip out = Run(r, bones);
  CHECK_EQ(Dist(At(out, bones, "lfoot_g", 0.5f), At(base, bones, "lfoot_g", 0.5f)) > 0.1f, true);
  CHECK_EQ(Dist(At(out, bones, "rfoot_g", 0.5f), At(base, bones, "rfoot_g", 0.5f)) < 1e-3f, true);
  // With the pelvis turned too: the posed leg still takes its own foot along; the other keeps the capture's.
  r.poses["pelvis_g"] = {PoseKey(2.5f, {0.0f, 0.0f, 25.0f})};
  const AnimClip both = Run(r, bones);
  CHECK_EQ(Dist(At(both, bones, "lfoot_g", 0.5f), At(base, bones, "lfoot_g", 0.5f)) > 0.1f, true);
  CHECK_EQ(Dist(At(both, bones, "rfoot_g", 0.5f), At(base, bones, "rfoot_g", 0.5f)) < 1e-3f, true);
}

TEST(pelvisPoseWithFootPinAndHips) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  const AnimClip base = Run(r, bones);
  const Vec3 planted = At(base, bones, "lfoot_g", 0.3f);
  const std::array<float, 3> off = OffsetFor(base, bones, "lthigh_g", "pelvis_g", LegLength(bones), planted, 0.3f);
  r.poses["pelvis_g"] = {PoseKey(2.5f, {0.0f, 0.0f, 25.0f})};
  r.targets["left_foot"] = {Key(2.3f, off, true), Key(2.8f, off)};
  r.targets["hips"] = {Key(2.5f, {0.0f, 0.0f, -0.05f})};
  const AnimClip out = Run(r, bones);
  bool finite = true;
  for (const AnimNode& n : out.nodes) {
    for (const auto& v : n.orientation.values) for (float x : v) finite = finite && isfinite(x);
    for (const auto& v : n.position.values) for (float x : v) finite = finite && isfinite(x);
  }
  CHECK_EQ(finite, true);
  bool held = true;
  for (int f = 9; f <= 18; ++f) held = held && Dist(At(out, bones, "lfoot_g", f / 30.0f), planted) < 1e-3f;
  CHECK_EQ(held, true);
  CHECK_EQ(Dist(At(out, bones, "rfoot_g", 0.5f), At(base, bones, "rfoot_g", 0.5f)) < 1e-3f, true);
  const float drop = At(base, bones, "rootdummy", 0.5f)[2] - At(out, bones, "rootdummy", 0.5f)[2];
  CHECK_EQ(drop <= (0.05f + 0.25f) * RestHipHeight(bones) + 1e-4f, true);
}

// The final review's findings (#90). C1: a posed pelvis with a foot key, or a leg key inside the pelvis's
// window, popped the planted foot ~15 cm (a binary switch; the foot's own key counted). I1: ease 0 keys
// vanished at the bench's 4-decimal times. I3: a chain key under an active target still turned the end.
TEST(AnimPosePelvisFootKeyKeepsAnkle) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  const AnimClip base = Run(r, bones);
  r.poses["pelvis_g"] = {PoseKey(2.5f, {0.0f, 0.0f, 25.0f})};
  r.poses["lfoot_g"] = {PoseKey(2.5f, {20.0f, 0.0f, 0.0f})};
  const AnimClip out = Run(r, bones);
  CHECK_EQ(Dist(At(out, bones, "lfoot_g", 0.5f), At(base, bones, "lfoot_g", 0.5f)) < 1e-3f, true);  // a foot's key turns it in place
  const int foot = IndexOf(bones, "lfoot_g");
  CHECK_EQ(DegreesBetween(PoseOf(out, bones, 0.5f).wq[foot], PoseOf(base, bones, 0.5f).wq[foot]) > 10.0f, true);  // and it shows
}

TEST(AnimPoseLegKeyInsidePelvisContinuous) {
  // A thigh key inside a posed pelvis's window: the ankle follows exactly the path the thigh key alone gives it
  // (the pelvis never moves a foot), frame by frame - no jump where the thigh's influence starts or ends.
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  r.poses["lthigh_g"] = {PoseKey(2.5f, {5.0f, 0.0f, 0.0f}, false, 0.1f)};
  const AnimClip thigh = Run(r, bones);
  r.poses["pelvis_g"] = {PoseKey(2.2f, {0.0f, 0.0f, 25.0f}), PoseKey(2.8f, {0.0f, 0.0f, 25.0f})};
  const AnimClip both = Run(r, bones);
  float off = 0.0f;
  for (int f = 0; f <= 30; ++f) off = fmaxf(off, Dist(At(both, bones, "lfoot_g", f / 30.0f), At(thigh, bones, "lfoot_g", f / 30.0f)));
  CHECK_EQ(off < 1e-3f, true);
  CHECK_EQ(Dist(At(thigh, bones, "lfoot_g", 0.5f), At(thigh, bones, "lfoot_g", 0.3f)) > 0.02f, true);  // the key does move the foot
}

TEST(AnimPoseEaseZeroAtBenchTimes) {
  // The bench writes take_start and key times to 4 decimals: frame 16's take time is 1.5333 + 16/30 = 2.06663,
  // the key's 2.0667 - it still shows on its frame, and only there.
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  r.take_start = 1.5333f;
  const AnimClip base = Run(r, bones);
  r.poses["torso_g"] = {PoseKey(2.0667f, {0.0f, 0.0f, 15.0f}, false, 0.0f)};
  const AnimClip out = Run(r, bones);
  CHECK_EQ(SameTurn(LocalTurn(out, bones, "torso_g", 16.0f / 30.0f), TurnFromDegrees({0.0f, 0.0f, 15.0f}), 4e-4f), true);
  CHECK_EQ(SameTurn(LocalTurn(out, bones, "torso_g", 15.0f / 30.0f), LocalTurn(base, bones, "torso_g", 15.0f / 30.0f), 1e-5f), true);
  CHECK_EQ(SameTurn(LocalTurn(out, bones, "torso_g", 17.0f / 30.0f), LocalTurn(base, bones, "torso_g", 17.0f / 30.0f), 1e-5f), true);
}

TEST(AnimPoseChainKeyInertUnderTarget) {
  const std::vector<AnimNode> bones = WholeBody(1.0f);
  AnimRecipe r = IkRecipe(bones, 0.0f);
  const AnimClip base = Run(r, bones);
  const Vec3 hand = Add(At(base, bones, "lhand_g", 0.5f), {0.05f, 0.1f, 0.05f});
  const Vec3 foot = Add(At(base, bones, "lfoot_g", 0.5f), {0.0f, 0.12f, 0.08f});
  r.targets["left_hand"] = {Key(2.5f, OffsetFor(base, bones, "lbicep_g", "torso_g", ArmLength(bones), hand, 0.5f))};
  r.targets["left_foot"] = {Key(2.5f, OffsetFor(base, bones, "lthigh_g", "pelvis_g", LegLength(bones), foot, 0.5f))};
  const AnimClip targeted = Run(r, bones);
  r.poses["lforearm_g"] = {PoseKey(2.5f, {30.0f, 0.0f, 0.0f})};
  r.poses["lshin_g"] = {PoseKey(2.5f, {-25.0f, 0.0f, 0.0f})};
  const AnimClip both = Run(r, bones);
  for (const char* end : {"lhand_g", "lfoot_g"}) {
    const int i = IndexOf(bones, end);
    CHECK_EQ(Dist(At(both, bones, end, 0.5f), At(targeted, bones, end, 0.5f)) < 1e-3f, true);
    CHECK_EQ(SameTurn(PoseOf(both, bones, 0.5f).wq[i], PoseOf(targeted, bones, 0.5f).wq[i], 4e-4f), true);  // the chain's key: no effect
  }
}
