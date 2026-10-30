#pragma once

// The recipe core's pose math, shared by the crouch step (anim_recipe.cpp) and the assembled recipe's IK
// pass (anim_assemble.cpp, #89): vectors and quaternions (x y z w), a pose's local and world transforms,
// and one two-bone solver for arms and legs. Internal to the core; the DLL and the tools' helper build it.

#include <array>
#include <vector>

#include "data/anim_recipe.h"

namespace data {
namespace pose {

using Vec3 = std::array<float, 3>;
using Quat = std::array<float, 4>;
constexpr Quat kIdentity = {0.0f, 0.0f, 0.0f, 1.0f};
constexpr float kPi = 3.14159265f;

Vec3 Add(const Vec3& a, const Vec3& b);
Vec3 Sub(const Vec3& a, const Vec3& b);
Vec3 Scale(const Vec3& a, float s);
float Dot(const Vec3& a, const Vec3& b);
Vec3 Cross(const Vec3& a, const Vec3& b);
float Length(const Vec3& a);
Vec3 Normalized(const Vec3& a);
Quat QConj(const Quat& q);
Quat QNormalized(const Quat& q);
Vec3 Rotate(const Quat& q, const Vec3& v);
Quat AxisAngleQ(const Vec3& axis, float angle);
// The shortest turn taking direction `from` to direction `to`.
Quat FromTo(const Vec3& from, const Vec3& to);

// Local transforms (a clip's keys at t, else the bone's rest) and world ones, per bone.
struct Pose {
  std::vector<Vec3> lp, wp;
  std::vector<Quat> lq, wq;
};

// Each bone's node in the clip (-1: none), by name, any case.
std::vector<int> ClipIndex(const AnimClip& clip, const std::vector<AnimNode>& bones);
// The world transforms from the local ones, parents first.
void Forward(const std::vector<AnimNode>& bones, Pose* pose);
Pose PoseAt(const AnimClip& clip, const std::vector<int>& in_clip, const std::vector<AnimNode>& bones, float t);
// The bones at rest (their first keys), world transforms included.
Pose RestPose(const std::vector<AnimNode>& bones);

// A bone chain: root -> mid -> end (thigh, shin, foot; bicep, forearm, hand).
struct Chain {
  int root = -1, mid = -1, end = -1;
};
// The chain by its bones' names (any case), or root = -1 when the body lacks one or they aren't a chain.
Chain FindChain(const std::vector<AnimNode>& bones, const char* root, const char* mid, const char* end);
// Its length at rest: the mid's offset from the root plus the end's from the mid.
float ChainLength(const std::vector<AnimNode>& bones, const Chain& c);
// The hip joints' mean height at rest (the thighs' tops), 1 without them: the bench solve's hipHeight.
float RestHipHeight(const std::vector<AnimNode>& bones);

// The IK pass's damping: a limb softens over the last 2% of its length (measured: 5% left a reachable
// hand 7 mm short at 99% extension and 11 mm at full; 2% leaves 1.2 mm and 4 mm, and still eases).
constexpr float kLimbDamping = 0.02f;

// The chain's root and mid turned so its end reaches `goal`:
// - softened within the last `damping` (a fraction) of its length, so a limb doesn't snap straight;
// - the mid bending in the plane it bent in `before`, else toward `pole_hint`;
// - the end keeping its turn in the world from `before`.
// `now` must have its world transforms current; they're left stale (Forward again to use them).
// Returns how far `goal` lies beyond the chain's length (0 when it reaches).
float SolveTwoBone(const std::vector<AnimNode>& bones, const Pose& before, Pose* now, const Chain& c, const Vec3& goal,
                   const Vec3& pole_hint, float damping);

// The crouch step's leg solve: the ankle back where it was in `before`, the knee in the plane it bent in
// (the thigh's facing when straight), the foot keeping its turn.
struct Leg {
  int thigh, shin, foot;
};
void SolveLeg(const std::vector<AnimNode>& bones, const Pose& before, Pose* now, const Leg& leg);

}  // namespace pose
}  // namespace data
