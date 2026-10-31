#include "data/anim_pose.h"

#include <ctype.h>
#include <math.h>

#include <algorithm>
#include <string>

namespace data {
namespace pose {
namespace {

std::string LowerName(std::string text) {
  for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return text;
}

int FindBone(const std::vector<AnimNode>& bones, const char* name) {
  const std::string want = LowerName(name);
  for (size_t i = 0; i < bones.size(); ++i) {
    if (LowerName(bones[i].name) == want) return static_cast<int>(i);
  }
  return -1;
}

Vec3 RestOffset(const AnimNode& b) {
  if (b.position.values.empty()) return {0.0f, 0.0f, 0.0f};
  const auto& v = b.position.values.front();
  return {v[0], v[1], v[2]};
}

}  // namespace

Vec3 Add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 Sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 Scale(const Vec3& a, float s) { return {a[0] * s, a[1] * s, a[2] * s}; }
float Dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 Cross(const Vec3& a, const Vec3& b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
float Length(const Vec3& a) { return sqrtf(Dot(a, a)); }
Vec3 Normalized(const Vec3& a) {
  const float l = Length(a);
  return l > 1e-9f ? Scale(a, 1.0f / l) : Vec3{0.0f, 0.0f, 0.0f};
}

Quat QConj(const Quat& q) { return {-q[0], -q[1], -q[2], q[3]}; }
Quat QNormalized(const Quat& q) {
  const float n = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  return n > 1e-9f ? Quat{q[0] / n, q[1] / n, q[2] / n, q[3] / n} : kIdentity;
}
Vec3 Rotate(const Quat& q, const Vec3& v) {
  const Vec3 u = {q[0], q[1], q[2]};
  const Vec3 t = Scale(Cross(u, v), 2.0f);
  return Add(Add(v, Scale(t, q[3])), Cross(u, t));
}
Quat AxisAngleQ(const Vec3& axis, float angle) {
  const float s = sinf(angle / 2.0f);
  return {axis[0] * s, axis[1] * s, axis[2] * s, cosf(angle / 2.0f)};
}
Quat TurnFromDegrees(const Vec3& xyz) {
  const float r = kPi / 180.0f;
  const Quat x = AxisAngleQ({1.0f, 0.0f, 0.0f}, xyz[0] * r), y = AxisAngleQ({0.0f, 1.0f, 0.0f}, xyz[1] * r),
             z = AxisAngleQ({0.0f, 0.0f, 1.0f}, xyz[2] * r);
  return QNormalized(QMul(z, QMul(y, x)));
}

Vec3 DegreesFromTurn(const Quat& q0) {
  const Quat q = QNormalized(q0);
  const float x = q[0], y = q[1], z = q[2], w = q[3];
  // The turn's matrix entries the Z-Y-X angles read from (M = Rz * Ry * Rx).
  const float m00 = 1.0f - 2.0f * (y * y + z * z), m01 = 2.0f * (x * y - z * w), m10 = 2.0f * (x * y + z * w);
  const float m11 = 1.0f - 2.0f * (x * x + z * z), m20 = 2.0f * (x * z - y * w), m21 = 2.0f * (y * z + x * w);
  const float m22 = 1.0f - 2.0f * (x * x + y * y);
  const float d = 180.0f / kPi;
  const float sy = std::max(-1.0f, std::min(1.0f, -m20));
  const float ay = asinf(sy);
  if (fabsf(sy) > 0.99999f) return {0.0f, ay * d, atan2f(-m01, m11) * d};  // gimbal: X folds into Z
  return {atan2f(m21, m22) * d, ay * d, atan2f(m10, m00) * d};
}

Quat FromTo(const Vec3& from, const Vec3& to) {
  const Vec3 a = Normalized(from), b = Normalized(to);
  const float d = Dot(a, b);
  if (d < -0.999999f) {
    Vec3 axis = Cross({1.0f, 0.0f, 0.0f}, a);
    if (Length(axis) < 1e-6f) axis = Cross({0.0f, 1.0f, 0.0f}, a);
    return AxisAngleQ(Normalized(axis), kPi);
  }
  const Vec3 c = Cross(a, b);
  return QNormalized({c[0], c[1], c[2], 1.0f + d});
}

std::vector<int> ClipIndex(const AnimClip& clip, const std::vector<AnimNode>& bones) {
  std::vector<int> index(bones.size(), -1);
  for (size_t i = 0; i < bones.size(); ++i) {
    const std::string name = LowerName(bones[i].name);
    for (size_t k = 0; k < clip.nodes.size(); ++k) {
      if (LowerName(clip.nodes[k].name) == name) {
        index[i] = static_cast<int>(k);
        break;
      }
    }
  }
  return index;
}

void Forward(const std::vector<AnimNode>& bones, Pose* pose) {
  for (size_t i = 0; i < bones.size(); ++i) {
    const int up = bones[i].parent;
    if (up < 0) {
      pose->wp[i] = pose->lp[i];
      pose->wq[i] = pose->lq[i];
    } else {
      pose->wq[i] = QNormalized(QMul(pose->wq[up], pose->lq[i]));
      pose->wp[i] = Add(pose->wp[up], Rotate(pose->wq[up], pose->lp[i]));
    }
  }
}

Pose PoseAt(const AnimClip& clip, const std::vector<int>& in_clip, const std::vector<AnimNode>& bones, float t) {
  Pose pose;
  const size_t n = bones.size();
  pose.lp.resize(n);
  pose.wp.resize(n);
  pose.lq.resize(n);
  pose.wq.resize(n);
  for (size_t i = 0; i < n; ++i) {
    const AnimNode& b = bones[i];
    const AnimNode* node = in_clip[i] >= 0 ? &clip.nodes[in_clip[i]] : nullptr;
    std::array<float, 4> p = b.has_position && !b.position.values.empty() ? b.position.values.front() : kIdentity;  // else 0 0 0
    Quat q = b.has_orientation && !b.orientation.values.empty() ? b.orientation.values.front() : kIdentity;
    if (node && node->has_position && !node->position.times.empty()) p = Sample(node->position, t, false);
    if (node && node->has_orientation && !node->orientation.times.empty()) q = Sample(node->orientation, t, true);
    pose.lp[i] = {p[0], p[1], p[2]};
    pose.lq[i] = QNormalized(q);
  }
  Forward(bones, &pose);
  return pose;
}

Pose RestPose(const std::vector<AnimNode>& bones) {
  AnimClip none;
  return PoseAt(none, std::vector<int>(bones.size(), -1), bones, 0.0f);
}

Chain FindChain(const std::vector<AnimNode>& bones, const char* root, const char* mid, const char* end) {
  Chain c{FindBone(bones, root), FindBone(bones, mid), FindBone(bones, end)};
  if (c.root < 0 || c.mid < 0 || c.end < 0 || bones[c.mid].parent != c.root || bones[c.end].parent != c.mid ||
      bones[c.root].parent < 0) {
    return Chain();
  }
  return c;
}

float ChainLength(const std::vector<AnimNode>& bones, const Chain& c) {
  if (c.root < 0) return 0.0f;
  return Length(RestOffset(bones[c.mid])) + Length(RestOffset(bones[c.end]));
}

float RestHipHeight(const std::vector<AnimNode>& bones) {
  const Pose rest = RestPose(bones);
  float sum = 0.0f;
  int n = 0;
  for (const char* name : {"lthigh_g", "rthigh_g"}) {
    const int i = FindBone(bones, name);
    if (i < 0) continue;
    sum += rest.wp[i][2];
    ++n;
  }
  return n ? sum / static_cast<float>(n) : 1.0f;
}

float SolveTwoBone(const std::vector<AnimNode>& bones, const Pose& before, Pose* now, const Chain& c, const Vec3& goal,
                   const Vec3& pole_hint, float damping, const Quat* end_turn) {
  if (c.root < 0) return 0.0f;
  const Vec3 base = now->wp[c.root];
  const float l1 = Length(now->lp[c.mid]), l2 = Length(now->lp[c.end]);
  const Vec3 to = Sub(goal, base);
  const float want = Length(to);
  const float full = l1 + l2;
  const float shortfall = std::max(0.0f, want - full);
  if (want < 1e-5f || l1 < 1e-5f || l2 < 1e-5f) return shortfall;
  const Vec3 u = Scale(to, 1.0f / want);
  // Damping: past (1 - damping) of the length, the reach approaches full length exponentially - the
  // limb eases to straight instead of snapping. 0: the goal itself (the crouch step's behaviour).
  float d = want;
  if (damping > 0.0f) {
    const float soft = full * (1.0f - damping), span = full * damping;
    if (d > soft) d = soft + span * (1.0f - expf(-(d - soft) / span));
  }
  d = std::max(fabsf(l1 - l2) + 1e-4f, std::min(full - 1e-4f, d));
  const Vec3 aim = damping > 0.0f ? Add(base, Scale(u, d)) : goal;
  // The mid bends the way it did - off the old root-end line (against the new one a straight limb reads
  // as bent the wrong way: the joint flipped) - and a nearly straight limb toward the hint.
  const Vec3 old_line = Normalized(Sub(before.wp[c.end], before.wp[c.root]));
  Vec3 bend = Sub(before.wp[c.mid], before.wp[c.root]);
  bend = Sub(bend, Scale(old_line, Dot(bend, old_line)));
  Vec3 pole = Add(bend, Scale(Normalized(pole_hint), 0.05f));
  pole = Sub(pole, Scale(u, Dot(pole, u)));
  if (Length(pole) < 1e-6f) return shortfall;
  pole = Normalized(pole);
  const float a = (l1 * l1 - l2 * l2 + d * d) / (2.0f * d);
  const float h = sqrtf(std::max(0.0f, l1 * l1 - a * a));
  const Vec3 mid = Add(base, Add(Scale(u, a), Scale(pole, h)));
  const Quat up = now->wq[bones[c.root].parent];
  const Quat root = QNormalized(QMul(FromTo(Rotate(now->wq[c.root], now->lp[c.mid]), Sub(mid, base)), now->wq[c.root]));
  Quat midq = QNormalized(QMul(root, now->lq[c.mid]));
  midq = QNormalized(QMul(FromTo(Rotate(midq, now->lp[c.end]), Sub(aim, mid)), midq));
  now->lq[c.root] = QNormalized(QMul(QConj(up), root));
  now->lq[c.mid] = QNormalized(QMul(QConj(root), midq));
  now->lq[c.end] = QNormalized(QMul(QConj(midq), end_turn ? *end_turn : before.wq[c.end]));
  return shortfall;
}

void SolveLeg(const std::vector<AnimNode>& bones, const Pose& before, Pose* now, const Leg& leg) {
  const Vec3 facing = Normalized(Rotate(before.wq[leg.thigh], Vec3{0.0f, 1.0f, 0.0f}));
  SolveTwoBone(bones, before, now, Chain{leg.thigh, leg.shin, leg.foot}, before.wp[leg.foot], facing, 0.0f);
}

}  // namespace pose
}  // namespace data
