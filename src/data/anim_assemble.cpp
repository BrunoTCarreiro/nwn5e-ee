#include "data/anim_assemble.h"

#include <ctype.h>
#include <math.h>

#include <algorithm>
#include <array>
#include <map>

#include "data/anim_pose.h"

namespace data {
namespace {

using Value = std::array<float, 4>;
using pose::Pose;
using pose::Quat;
using pose::Vec3;

std::string LowerName(std::string text) {
  for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return text;
}

float Smoothstep(float x) {
  x = std::max(0.0f, std::min(1.0f, x));
  return x * x * (3.0f - 2.0f * x);
}

// How much of the capture shows at t: 0 at the clip's ends, eased over blend_in / blend_out.
float BlendWeight(const AnimRecipe& r, float t) {
  const float in = r.blend_in > 0.0f ? Smoothstep(t / r.blend_in) : 1.0f;
  const float out = r.blend_out > 0.0f ? Smoothstep((r.length - t) / r.blend_out) : 1.0f;
  return std::min(in, out);
}

// How far a loop's end has crossfaded toward frame 0 at t (a zero loop_blend snaps the last frame).
float LoopWeight(const AnimRecipe& r, float t) {
  if (!r.loop) return 0.0f;
  const float span = std::min(r.loop_blend, r.length);
  if (span <= 0.0f) return t >= r.length ? 1.0f : 0.0f;
  return Smoothstep((t - (r.length - span)) / span);
}

// A bone's value in the stance's first frame: the stance's key at 0, else the library's rest.
Value StanceValue(const AnimClip& stance, const AnimNode& bone, bool position) {
  for (const AnimNode& node : stance.nodes) {
    if (LowerName(node.name) != LowerName(bone.name)) continue;
    const AnimKeys& keys = position ? node.position : node.orientation;
    if ((position ? node.has_position : node.has_orientation) && !keys.times.empty()) {
      return Sample(keys, 0.0f, !position);
    }
    break;
  }
  const AnimKeys& rest = position ? bone.position : bone.orientation;
  if (!rest.values.empty()) return rest.values.front();
  return position ? Value{0.0f, 0.0f, 0.0f, 0.0f} : Value{0.0f, 0.0f, 0.0f, 1.0f};
}

// A delta track at t (its first and last keys held outside them).
Value DeltaAt(const AnimKeyTrack& track, float t) { return Sample(track.keys, t, !track.position); }

// A pose key with no ease (under half a frame) is its own frame: within half a frame of it - the bench writes
// times to 4 decimals, so the frames' times miss the keys' by a little (the final review's I1).
constexpr float kHalfFrame = 0.5f / 30.0f;

// Step 4 (#90): a part's turn from the stance at take time T under its pose keys (by time). Each key's
// result is R * capture (push) or R (replace), R its turn about the stance axes; between two keys their
// results blend by smoothstep; before the first key and after the last the result eases from the capture
// over that key's ease (0: a step - the key's own frame).
Value PosedDelta(const std::vector<AnimPoseKey>& keys, float T, const Value& capture) {
  if (keys.empty() || !isfinite(T)) return capture;
  const auto result = [&](const AnimPoseKey& k) {
    const Value turn = pose::TurnFromDegrees({k.turn[0], k.turn[1], k.turn[2]});
    return k.replace ? turn : QMul(turn, capture);
  };
  const AnimPoseKey& first = keys.front();
  const AnimPoseKey& last = keys.back();
  if (T <= first.t) {
    const float w = first.ease > kHalfFrame ? Smoothstep(1.0f - (first.t - T) / first.ease) : (first.t - T <= kHalfFrame ? 1.0f : 0.0f);
    return Mix(capture, result(first), w, true);
  }
  if (T >= last.t || keys.size() < 2) {
    const float w = last.ease > kHalfFrame ? Smoothstep(1.0f - (T - last.t) / last.ease) : (T - last.t <= kHalfFrame ? 1.0f : 0.0f);
    return Mix(capture, result(last), w, true);
  }
  size_t i = 0;
  while (i + 1 < keys.size() && keys[i + 1].t < T) ++i;
  const float span = std::max(keys[i + 1].t - keys[i].t, 1e-6f);
  return Mix(result(keys[i]), result(keys[i + 1]), Smoothstep((T - keys[i].t) / span), true);
}

// How much of a part's pose keys shows at take time T (0: the capture's own turn) - PosedDelta's weight.
float PoseInfluence(const std::vector<AnimPoseKey>& keys, float T) {
  if (keys.empty() || !isfinite(T)) return 0.0f;
  const AnimPoseKey& first = keys.front();
  const AnimPoseKey& last = keys.back();
  if (T < first.t) return first.ease > kHalfFrame ? Smoothstep(1.0f - (first.t - T) / first.ease) : (first.t - T <= kHalfFrame ? 1.0f : 0.0f);
  if (T > last.t) return last.ease > kHalfFrame ? Smoothstep(1.0f - (T - last.t) / last.ease) : (T - last.t <= kHalfFrame ? 1.0f : 0.0f);
  return 1.0f;
}

// A keyed bone: its delta track and its value in the stance's first frame; a posed part (#90) its pose keys
// and the clock they're in (take time = take_start + clip time).
struct Bound {
  const AnimKeyTrack* track;
  size_t at;
  Value home;
  const std::vector<AnimPoseKey>* poses = nullptr;
  float take_start = 0.0f;
};

// Steps 1, 4 and 6: the capture composed onto the stance at t, its pose keys applied (`posed`), before the
// loop and the blends.
Value Composed(const Bound& b, float t, bool posed = true) {
  const Value d = DeltaAt(*b.track, t);
  if (b.track->position) return {b.home[0] + d[0], b.home[1] + d[1], b.home[2] + d[2], 0.0f};
  if (posed && b.poses) return QMul(b.home, PosedDelta(*b.poses, b.take_start + t, d));
  return QMul(b.home, d);
}

// Step 9 on one bone's values (one per frame, frame 0 first): the loop's crossfade toward frame 0, then
// the blends from and to `home`; quaternions kept on one side so no key spins the long way round.
AnimKeys Finish(const AnimRecipe& r, const std::vector<Value>& values, const Value& home, bool position, int frames) {
  AnimKeys keys;
  const Value first = values.front();
  for (int f = 0; f <= frames; ++f) {
    const float t = r.length * static_cast<float>(f) / static_cast<float>(frames);
    Value v = Mix(values[f], first, LoopWeight(r, t), !position);
    v = Mix(home, v, BlendWeight(r, t), !position);
    keys.times.push_back(t);
    keys.values.push_back(v);
  }
  return keys;
}

// --- Step 8, the IK pass (#89) ------------------------------------------------------------------------
constexpr float kEase = 0.2f;       // a limb's reach eases from 0 over 0.2 s outside its keys
constexpr float kSinkCap = 0.25f;   // the hips sink at most this fraction of the hip height
constexpr float kReportOver = 0.03f;
// The grip's off arm (#112 addendum): kept to this share of its length - a slight bend, never locked straight - unless
// the start pose holds it straighter.
constexpr float kGripBend = 0.95f;
// The weapon's turn toward the off hand (#112 addendum 2), split about the forearm: its roll at most kGripRoll, the wrist
// at most kGripWrist; past them the hand moves. The give-way's start-pose share is full by a move of kGripFold of the
// arm's length. A grip within kGripSlack of the limit needs nothing (float noise at the start pose).
constexpr float kGripRoll = 80.0f * 3.14159265f / 180.0f;
constexpr float kGripWrist = 60.0f * 3.14159265f / 180.0f;
constexpr float kGripFold = 0.3f;
constexpr float kGripSlack = 1e-4f;
constexpr int kGripRounds = 4;  // the hand's moves, each after the weapon followed the forearm back to the wrist's caps

struct Limb {
  std::string name;
  bool hips = false;
  pose::Chain chain;      // hands and feet (the hips have none)
  int frame = -1;         // torso_g (hands), pelvis_g (feet): the frame the offset is in
  float length = 1.0f;    // the chain's rest length, or the hip height for the hips
  const std::vector<AnimTargetKey>* keys = nullptr;
  std::vector<Vec3> held;  // a pinned key's place in the world at its own time
  std::vector<Quat> held_turn;  // a foot's: a pinned key's turn of the foot in the world at its own time
};

Vec3 Lerp(const Vec3& a, const Vec3& b, float f) { return pose::Add(a, pose::Scale(pose::Sub(b, a), f)); }

// The limb's influence at take time T: interpolated between keys, eased in before the first and out after
// the last.
float ReachAt(const std::vector<AnimTargetKey>& keys, float T) {
  if (keys.empty() || !isfinite(T)) return 0.0f;
  if (T <= keys.front().t) return keys.front().reach * Smoothstep(1.0f - (keys.front().t - T) / kEase);
  if (T >= keys.back().t || keys.size() < 2) return keys.back().reach * Smoothstep(1.0f - (T - keys.back().t) / kEase);
  size_t i = 0;
  while (i + 1 < keys.size() && keys[i + 1].t < T) ++i;
  const float span = std::max(keys[i + 1].t - keys[i].t, 1e-6f);
  const float s = Smoothstep((T - keys[i].t) / span);
  return keys[i].reach + (keys[i + 1].reach - keys[i].reach) * s;
}

// A key's place on the body as it stands in `p` (the hips: from the stance's root in the world).
Vec3 BodyPlace(const Limb& L, const AnimTargetKey& k, const Pose& p, const Vec3& home) {
  const Vec3 off = pose::Scale({k.offset[0], k.offset[1], k.offset[2]}, L.length);
  if (L.hips) return pose::Add(home, off);
  return pose::Add(p.wp[L.chain.root], pose::Rotate(p.wq[L.frame], off));
}

Vec3 KeyPlace(const Limb& L, size_t i, const Pose& p, const Vec3& home) {
  return (*L.keys)[i].pinned && !L.hips ? L.held[i] : BodyPlace(L, (*L.keys)[i], p, home);
}

// The limb's target at take time T: between two keys their places interpolate; a pinned key holds its
// place in the world until the next, easing toward it over the last 0.2 s; outside the keys, the end's.
Vec3 PlaceAt(const Limb& L, float T, const Pose& p, const Vec3& home) {
  const std::vector<AnimTargetKey>& keys = *L.keys;
  if (T <= keys.front().t) return KeyPlace(L, 0, p, home);
  if (T >= keys.back().t || keys.size() < 2 || !isfinite(T)) return KeyPlace(L, keys.size() - 1, p, home);
  size_t i = 0;
  while (i + 1 < keys.size() && keys[i + 1].t < T) ++i;
  const float span = std::max(keys[i + 1].t - keys[i].t, 1e-6f);
  if (keys[i].pinned && !L.hips) {
    const float window = std::min(kEase, span);
    const float w = Smoothstep((T - (keys[i + 1].t - window)) / window);
    return w > 0.0f ? Lerp(L.held[i], KeyPlace(L, i + 1, p, home), w) : L.held[i];
  }
  return Lerp(KeyPlace(L, i, p, home), KeyPlace(L, i + 1, p, home), Smoothstep((T - keys[i].t) / span));
}

// A foot's held turn at take time T (#88, the owner's test: a held foot key held the ankle while the foot
// went on turning with the animation): a pinned key holds the foot's turn in the world as it was at the
// key, like its place - easing over the same window before the next key, toward its turn when it's pinned
// too, else back to the animation's; toward a pinned key from a free one over their span, as the place
// does. *w: how much of it applies (0: the animation's own turn).
Quat TurnAt(const Limb& L, float T, float* w) {
  const std::vector<AnimTargetKey>& keys = *L.keys;
  *w = 0.0f;
  if (L.held_turn.size() != keys.size()) return pose::kIdentity;
  const auto own = [&](size_t i) {
    *w = keys[i].pinned ? 1.0f : 0.0f;
    return L.held_turn[i];
  };
  if (T <= keys.front().t) return own(0);
  if (T >= keys.back().t || keys.size() < 2 || !isfinite(T)) return own(keys.size() - 1);
  size_t i = 0;
  while (i + 1 < keys.size() && keys[i + 1].t < T) ++i;
  const float span = std::max(keys[i + 1].t - keys[i].t, 1e-6f);
  if (keys[i].pinned) {
    const float window = std::min(kEase, span);
    const float u = Smoothstep((T - (keys[i + 1].t - window)) / window);
    if (keys[i + 1].pinned) {
      *w = 1.0f;
      return Mix(L.held_turn[i], L.held_turn[i + 1], u, true);
    }
    *w = 1.0f - u;
    return L.held_turn[i];
  }
  if (keys[i + 1].pinned) {
    *w = Smoothstep((T - keys[i].t) / span);
    return L.held_turn[i + 1];
  }
  return pose::kIdentity;
}

}  // namespace

bool AssembleAnimClip(const AnimRecipe& recipe, const std::string& library, const std::vector<AnimNode>& bones,
                      const AnimClip& stance, AnimClip* out, std::string* err, std::vector<AnimIkShort>* report) {
  if (report) report->clear();
  const auto found = recipe.keys.find(LowerName(library));
  if (found == recipe.keys.end() || found->second.empty()) {
    if (err) *err = "no keys for " + library;
    return false;
  }
  if (stance.nodes.empty()) {
    if (err) *err = "the stance " + recipe.stance + " has no frame 0";
    return false;
  }
  if (bones.empty() || recipe.length <= 0.0f) {
    if (err) *err = "no bones, or no length";
    return false;
  }
  AnimClip clip;
  clip.name = recipe.name;
  clip.length = recipe.length;
  clip.transtime = recipe.transtime;
  clip.animroot = "rootdummy";
  clip.nodes = bones;  // the library's tree, unkeyed: key_unkeyed_turns holds the untracked ones (#108)
  for (AnimNode& node : clip.nodes) {
    node.has_position = node.has_orientation = false;
    node.position = AnimKeys();
    node.orientation = AnimKeys();
  }
  const int frames = std::max(1, static_cast<int>(lroundf(recipe.length * 30.0f)));
  std::vector<Bound> bound;
  for (const AnimKeyTrack& track : found->second) {
    if (track.keys.times.empty()) continue;
    size_t at = 0;
    while (at < bones.size() && LowerName(bones[at].name) != LowerName(track.bone)) ++at;
    if (at == bones.size()) continue;
    bound.push_back({&track, at, StanceValue(stance, bones[at], track.position)});
  }
  if (bound.empty()) {
    if (err) *err = "none of its bones is in the library's tree";
    return false;
  }
  // Step 4's parts (#90): each posed part this body has takes its keys; one without a capture track joins
  // with an identity delta (the stance's turn, posed). A part the body lacks is skipped.
  const float pose_clock = isfinite(recipe.take_start) ? recipe.take_start : 0.0f;
  AnimKeyTrack unturned;
  unturned.keys.times = {0.0f};
  unturned.keys.values = {{0.0f, 0.0f, 0.0f, 1.0f}};
  for (const auto& part : recipe.poses) {
    if (part.second.empty()) continue;
    size_t at = 0;
    while (at < bones.size() && LowerName(bones[at].name) != part.first) ++at;
    if (at == bones.size()) continue;
    bool joined = false;
    for (Bound& b : bound) {
      if (b.at != at || b.track->position) continue;
      b.poses = &part.second;
      b.take_start = pose_clock;
      joined = true;
    }
    if (!joined) bound.push_back({&unturned, at, StanceValue(stance, bones[at], false), &part.second, pose_clock});
  }
  const auto frame_time = [&](int f) { return recipe.length * static_cast<float>(f) / static_cast<float>(frames); };
  const auto put = [&](size_t at, bool position, const AnimKeys& keys) {
    AnimNode& node = clip.nodes[at];
    if (position) {
      node.has_position = true;
      node.position = keys;
    } else {
      node.has_orientation = true;
      node.orientation = keys;
    }
  };
  // A node with no turn of its own holds the start pose's first-frame turn, at 0 and at the clip's length (#108;
  // 0.59.11): the engine keeps an unkeyed node's turn from the animation before - n5_rage, played from a combat
  // stance, kept that stance's 17-27 degree rootdummy turn and tilted the whole body. rootdummy holds its rest turn
  // instead: the bench solves with the root at rest and folds the stance's root turn into the root's children, so an
  // untracked child of the root (belt_g1, beside the pelvis) takes that fold too - rest^-1 * the stance's root * its
  // own. Every node then starts and ends on the stance - the cloak, the belt, the shoulder pads, the weapon and shield
  // points - as a keyed one does (S * delta, with no delta); 0.59.9's rest turn snapped them off it. The model's own
  // top node stays as it is, and so does a name the tree holds twice (a_da's two Impact nodes): the engine matches an
  // animation's nodes to the model's by name, so a turn keyed for one could land on the other. Both ways out below
  // end with it.
  const auto key_unkeyed_turns = [&]() {
    int root_at = -1;
    for (size_t b = 0; b < bones.size(); ++b) {
      if (LowerName(bones[b].name) == "rootdummy" && bones[b].parent >= 0) root_at = static_cast<int>(b);
    }
    const auto rest_of = [&](size_t b) {
      return bones[b].orientation.values.empty() ? Value{0.0f, 0.0f, 0.0f, 1.0f} : bones[b].orientation.values.front();
    };
    const auto twin = [&](size_t b) {
      for (size_t o = 0; o < bones.size(); ++o) {
        if (o != b && LowerName(bones[o].name) == LowerName(bones[b].name)) return true;
      }
      return false;
    };
    // The fold, while the root has no turn of its own (a recipe keys only its place).
    const bool fold = root_at >= 0 && !clip.nodes[root_at].has_orientation;
    const Value unturn = fold ? QMul(pose::QConj(rest_of(root_at)), StanceValue(stance, bones[root_at], false)) : Value{0.0f, 0.0f, 0.0f, 1.0f};
    for (size_t b = 0; b < bones.size(); ++b) {
      if (clip.nodes[b].has_orientation || bones[b].parent < 0 || twin(b)) continue;
      Value held = static_cast<int>(b) == root_at ? rest_of(b) : StanceValue(stance, bones[b], false);
      if (fold && bones[b].parent == root_at) held = pose::QNormalized(QMul(unturn, held));
      AnimKeys keys;
      keys.times = {0.0f, recipe.length};
      keys.values = {held, held};
      put(b, false, keys);
    }
  };

  // The limbs with targets that this body has (step 8 runs only for them).
  const auto find = [&](const char* name) {
    for (size_t i = 0; i < bones.size(); ++i) {
      if (LowerName(bones[i].name) == name) return static_cast<int>(i);
    }
    return -1;
  };
  const int root = find("rootdummy"), torso = find("torso_g"), pelvis = find("pelvis_g");
  const float hip_height = pose::RestHipHeight(bones);
  const float take_start = isfinite(recipe.take_start) ? recipe.take_start : 0.0f;  // a hand-edited nan: 0
  // The two-handed grip (#112): a body without the right hand or the left arm has none.
  const int rhand = find("rhand_g");
  const pose::Chain left_arm = pose::FindChain(bones, "lbicep_g", "lforearm_g", "lhand_g");
  const pose::Chain right_arm = pose::FindChain(bones, "rbicep_g", "rforearm_g", "rhand_g");
  const bool grip = recipe.grip && rhand >= 0 && left_arm.root >= 0 && torso >= 0;
  std::vector<Limb> limbs;
  for (const std::string& name : AnimTargetLimbs()) {
    const auto keys = recipe.targets.find(name);
    if (keys == recipe.targets.end() || keys->second.empty()) continue;
    if (grip && name == "left_hand") continue;  // the grip holds the left hand: its target does nothing (the grip wins)
    Limb L;
    L.name = name;
    L.keys = &keys->second;
    if (name == "hips") {
      if (root < 0 || bones[root].parent < 0) continue;
      L.hips = true;
      L.length = hip_height;
    } else {
      const bool hand = name.find("hand") != std::string::npos;
      const char* side = name.compare(0, 4, "left") == 0 ? "l" : "r";
      const std::string s(side);
      L.chain = hand ? pose::FindChain(bones, (s + "bicep_g").c_str(), (s + "forearm_g").c_str(), (s + "hand_g").c_str())
                     : pose::FindChain(bones, (s + "thigh_g").c_str(), (s + "shin_g").c_str(), (s + "foot_g").c_str());
      L.frame = hand ? torso : pelvis;
      if (L.chain.root < 0 || L.frame < 0) continue;
      L.length = pose::ChainLength(bones, L.chain);
      if (L.length <= 1e-5f) continue;
    }
    limbs.push_back(L);
  }

  // A posed pelvis (#90) keeps the feet where the capture put them: the IK pass runs for it too.
  const std::vector<AnimPoseKey>* pelvis_keys = nullptr;
  if (pelvis >= 0) {
    const auto found_pelvis = recipe.poses.find("pelvis_g");
    if (found_pelvis != recipe.poses.end() && !found_pelvis->second.empty()) pelvis_keys = &found_pelvis->second;
  }
  bool any_poses = false;
  for (const auto& part : recipe.poses) any_poses = any_poses || !part.second.empty();
  // The two-handed grip (#112): the left hand held where the stance holds it against the right hand - its place and turn
  // in rhand_g's frame, from the stance's first frame on this body - and the off arm's limit: the straighter of
  // kGripBend of its length and its span in the stance, so the clip's ends (the stance) never move.
  Vec3 grip_place = {0.0f, 0.0f, 0.0f};
  Quat grip_turn = {0.0f, 0.0f, 0.0f, 1.0f};
  float grip_limit = 0.0f;
  std::array<Quat, 6> grip_arm{};  // the start pose's arm turns: lbicep, lforearm, lhand, rbicep, rforearm, rhand
  if (grip) {
    Pose st = pose::RestPose(bones);
    for (size_t b = 0; b < bones.size(); ++b) {
      if (bones[b].parent < 0) continue;
      st.lq[b] = pose::QNormalized(StanceValue(stance, bones[b], false));
      if (static_cast<int>(b) == root) {
        const Value h = StanceValue(stance, bones[b], true);
        st.lp[b] = {h[0], h[1], h[2]};
      }
    }
    pose::Forward(bones, &st);
    grip_place = pose::Rotate(pose::QConj(st.wq[rhand]), pose::Sub(st.wp[left_arm.end], st.wp[rhand]));
    grip_turn = QMul(pose::QConj(st.wq[rhand]), st.wq[left_arm.end]);
    grip_limit = std::max(kGripBend * pose::ChainLength(bones, left_arm), pose::Length(pose::Sub(st.wp[left_arm.end], st.wp[left_arm.root])));
    const int arm_at[6] = {left_arm.root, left_arm.mid, left_arm.end, right_arm.root, right_arm.mid, right_arm.end};
    for (int k = 0; k < 6; ++k) {
      if (arm_at[k] >= 0) grip_arm[k] = st.lq[arm_at[k]];
    }
  }

  // The report: each limb's worst shortfall while its goal had full weight (the grip's: the weapon hand's largest move).
  std::map<std::string, AnimIkShort> worst;
  const auto record = [&](const std::string& limb, float shortfall, float T) {
    AnimIkShort& w = worst[limb];
    w.limb = limb;
    if (shortfall > w.metres) {
      w.metres = shortfall;
      w.t = T;
    }
  };
  const auto finish_report = [&]() {
    if (!report) return;
    for (const auto& w : worst) {
      if (w.second.metres > kReportOver) report->push_back(w.second);
    }
  };

  // The grip on the finished frames (#112; addenda 1-2) - after Step 9's loop and blends, which turn each bone on its
  // own and so would pull the hands apart. Each frame where the grip lies beyond the off arm's limit from the left
  // shoulder (the owner's p2 / t3):
  //  1. the weapon turns first - along the shortest turn that carries the grip toward the shoulder, as far as needed but
  //     no further than its split about the forearm allows: kGripRoll of the forearm's roll, kGripWrist of the wrist;
  //  2. the hand moves only for the rest, toward the shoulder, with the turned weapon - the right arm solved from its
  //     take's arm mixed toward its arm as solved on the frame before, by how far it moves (a nearly straight arm folding
  //     far has no bend of its own to keep: its elbow popped);
  //  3. the turn's twist goes to the forearm's roll, so the wrist keeps only the swing.
  // Then the left arm takes the grip, place and turn, solved from its arm as solved on the frame before (the start pose's
  // on the first) on this frame's torso - never from wherever the take's free arm was. Each solve bends toward the start
  // pose's bend, carried on the frame's torso: the elbow's side carries from frame to frame (a bend taken fresh each
  // frame flips where the grip lines up with it: the polearm's strike), and the start pose's own frames solve to it.
  // The arms' turns are written back per frame, each on the same side as the one before; the right arm's only where it
  // gave way.
  const auto hold_grip = [&]() {
    if (!grip) return;
    const std::vector<int> in_clip = pose::ClipIndex(clip, bones);
    const int arm[6] = {left_arm.root, left_arm.mid, left_arm.end, right_arm.root, right_arm.mid, right_arm.end};
    std::vector<std::vector<Value>> held(6);
    bool gave_way = false;
    const float right_length = right_arm.root >= 0 ? pose::ChainLength(bones, right_arm) : 0.0f;
    // A chain's bend - its mid off the root-end line - or `fallback` for a straight one.
    const auto bend_of = [&](const Pose& p, const pose::Chain& c, const Vec3& fallback) {
      const Vec3 line = pose::Normalized(pose::Sub(p.wp[c.end], p.wp[c.root]));
      Vec3 bend = pose::Sub(p.wp[c.mid], p.wp[c.root]);
      bend = pose::Sub(bend, pose::Scale(line, pose::Dot(bend, line)));
      return pose::Length(bend) > 1e-3f ? bend : fallback;
    };
    // `p` with one arm's turns (k0: 0 the left, 3 the right) mixed toward `turns` by w.
    const auto arm_toward = [&](const Pose& p, int k0, const std::array<Quat, 6>& turns, float w) {
      Pose r = p;
      for (int k = k0; k < k0 + 3; ++k) {
        if (arm[k] >= 0) r.lq[arm[k]] = pose::QNormalized(Mix(p.lq[arm[k]], turns[k], w, true));
      }
      pose::Forward(bones, &r);
      return r;
    };
    // The arms as solved on the frame before (the start pose's before the first), and whether the right arm gave way.
    std::array<Quat, 6> last = grip_arm;
    bool last_gave = false;
    // A turn's twist about `axis` (its swing is the rest), and a turn's angle.
    const auto twist_of = [](const Quat& q, const Vec3& axis) {
      const float d = q[0] * axis[0] + q[1] * axis[1] + q[2] * axis[2];
      const Quat tw = {axis[0] * d, axis[1] * d, axis[2] * d, q[3]};
      const float n = sqrtf(tw[0] * tw[0] + tw[1] * tw[1] + tw[2] * tw[2] + tw[3] * tw[3]);
      return n > 1e-9f ? Quat{tw[0] / n, tw[1] / n, tw[2] / n, tw[3] / n} : Quat{0.0f, 0.0f, 0.0f, 1.0f};
    };
    const auto angle_of = [](const Quat& q) { return 2.0f * acosf(std::min(1.0f, fabsf(q[3]))); };
    for (int f = 0; f <= frames; ++f) {
      const float t = frame_time(f), T = take_start + t;
      Pose p = pose::PoseAt(clip, in_clip, bones, t);
      const Pose take = p;
      const Vec3 shoulder = p.wp[left_arm.root], down = pose::Rotate(p.wq[torso], {0.0f, -1.0f, 0.0f});
      const Pose posed = arm_toward(arm_toward(p, 0, grip_arm, 1.0f), 3, grip_arm, 1.0f);  // the start pose's arms here
      bool gave = false;
      Quat weapon = p.wq[rhand];
      const auto excess_at = [&](const Vec3& at, const Quat& turn) {
        return pose::Length(pose::Sub(pose::Add(at, pose::Rotate(turn, grip_place)), shoulder)) - grip_limit;
      };
      float excess = excess_at(p.wp[rhand], weapon);
      if (excess > kGripSlack && right_arm.root >= 0) {
        gave_way = gave = true;
        // 1. The weapon turns first.
        const Vec3 d = pose::Normalized(pose::Rotate(weapon, grip_place)), e = pose::Normalized(pose::Sub(shoulder, p.wp[rhand]));
        const Vec3 across = pose::Cross(d, e);
        const float full = atan2f(pose::Length(across), pose::Dot(d, e));
        if (pose::Length(across) > 1e-6f && full > 1e-4f) {
          const Vec3 axis = pose::Normalized(across), fore = pose::Normalized(pose::Sub(p.wp[rhand], p.wp[right_arm.mid]));
          const auto within = [&](float a) {
            const Quat q = pose::AxisAngleQ(axis, a), tw = twist_of(q, fore);
            return angle_of(tw) <= kGripRoll && angle_of(QMul(q, pose::QConj(tw))) <= kGripWrist;
          };
          const auto excess_turned = [&](float a) { return excess_at(p.wp[rhand], QMul(pose::AxisAngleQ(axis, a), weapon)); };
          float lo = 0.0f, hi = full;
          if (!within(full)) {
            for (int i = 0; i < 30; ++i) {
              const float m = 0.5f * (lo + hi);
              if (within(m)) lo = m; else hi = m;
            }
            hi = lo;
          }
          float turn = hi;  // the cap along this turn; where less brings the grip to the limit, the least that does
          if (excess_turned(turn) <= 0.0f) {
            lo = 0.0f;
            for (int i = 0; i < 30; ++i) {
              const float m = 0.5f * (lo + turn);
              if (excess_turned(m) > 0.0f) lo = m; else turn = m;
            }
          }
          weapon = pose::QNormalized(QMul(pose::AxisAngleQ(axis, turn), weapon));
          excess = excess_at(p.wp[rhand], weapon);
        }
        // 2. The hand moves for the rest. Folding the arm swings the forearm under the weapon; where the wrist would
        //    pass its caps for it, the weapon follows the forearm back to them, and the hand moves again for what that
        //    opens - up to kGripRounds, while each round at least halves what's left. Where it doesn't (a long grip: the
        //    forearm swings the grip as far as the hand moved - a polearm on a_dfa ran to 1.2 m), the round keeps the
        //    weapon instead: the hand reaches the grip with it, and the wrist passes its caps on that frame.
        const Vec3 roll_axis = pose::Normalized(p.lp[rhand]);  // the forearm's own axis, in its frame
        const auto capped = [&](const Quat& wrist) {
          const Quat change = QMul(wrist, pose::QConj(take.lq[rhand]));
          Quat tw = twist_of(change, roll_axis), sw = QMul(change, pose::QConj(tw));
          const float ta = angle_of(tw), sa = angle_of(sw);
          if (ta > kGripRoll) tw = pose::QNormalized(Mix(Quat{0.0f, 0.0f, 0.0f, 1.0f}, tw, kGripRoll / ta, true));
          if (sa > kGripWrist) sw = pose::QNormalized(Mix(Quat{0.0f, 0.0f, 0.0f, 1.0f}, sw, kGripWrist / sa, true));
          return pose::QNormalized(QMul(QMul(sw, tw), take.lq[rhand]));
        };
        for (int round = 0; round < kGripRounds && excess > kGripSlack; ++round) {
          const Vec3 in = pose::Normalized(pose::Sub(shoulder, pose::Add(p.wp[rhand], pose::Rotate(weapon, grip_place))));
          const Vec3 to = pose::Add(p.wp[rhand], pose::Scale(in, excess));
          const float share = Smoothstep(pose::Length(pose::Sub(to, take.wp[rhand])) / (kGripFold * right_length));
          const Pose from = round > 0 ? p : last_gave ? arm_toward(p, 3, last, share) : p;
          Pose now = from;
          pose::SolveTwoBone(bones, from, &now, right_arm, to, bend_of(posed, right_arm, down), pose::kLimbDamping, &weapon);
          pose::Forward(bones, &now);
          Pose wrist_kept = now;
          wrist_kept.lq[rhand] = capped(pose::QNormalized(QMul(pose::QConj(now.wq[right_arm.mid]), weapon)));
          pose::Forward(bones, &wrist_kept);
          const float left = excess_at(wrist_kept.wp[rhand], wrist_kept.wq[rhand]);
          if (left > 0.5f * excess) {  // the caps chase the grip: keep the weapon, the hand there reaches it
            p = now;
            excess = excess_at(p.wp[rhand], weapon);
            break;
          }
          p = wrist_kept;
          weapon = p.wq[rhand];
          excess = left;
        }
        if (pose::Length(pose::Sub(p.wp[rhand], take.wp[rhand])) > 1e-4f) record("grip", pose::Length(pose::Sub(p.wp[rhand], take.wp[rhand])), T);
        // 3. The twist to the forearm's roll: the wrist's change from the take's, split about the forearm's own axis
        //    (in its frame, toward the hand); the forearm rolls by it, the wrist keeps the swing.
        const Quat wrist = pose::QNormalized(QMul(pose::QConj(p.wq[right_arm.mid]), weapon));
        const Quat roll = twist_of(QMul(wrist, pose::QConj(take.lq[rhand])), pose::Normalized(p.lp[rhand]));
        p.lq[right_arm.mid] = pose::QNormalized(QMul(p.lq[right_arm.mid], roll));
        p.lq[rhand] = pose::QNormalized(QMul(pose::QConj(roll), wrist));
        pose::Forward(bones, &p);
      }
      // The left arm: the grip, from the start pose's left arm on this frame's torso.
      const Vec3 goal = pose::Add(p.wp[rhand], pose::Rotate(p.wq[rhand], grip_place));
      Quat hand = pose::QNormalized(QMul(p.wq[rhand], grip_turn));
      const Pose from = arm_toward(p, 0, last, 1.0f);
      Pose now = from;
      record("left_hand", pose::SolveTwoBone(bones, from, &now, left_arm, goal, bend_of(posed, left_arm, down), pose::kLimbDamping, &hand), T);
      p = now;
      for (int k = 0; k < 6; ++k) {
        if (arm[k] >= 0) last[k] = p.lq[arm[k]];
      }
      last_gave = gave;
      for (int k = 0; k < 6; ++k) {
        if (arm[k] >= 0) held[k].push_back(p.lq[arm[k]]);
      }
    }
    for (int k = 0; k < 6; ++k) {
      if (arm[k] < 0 || (k >= 3 && !gave_way)) continue;
      AnimKeys keys;
      for (int f = 0; f <= frames; ++f) {
        Value q = held[k][f];
        if (f > 0) {
          const Value& a = keys.values.back();
          if (a[0] * q[0] + a[1] * q[1] + a[2] * q[2] + a[3] * q[3] < 0.0f) q = {-q[0], -q[1], -q[2], -q[3]};
        }
        keys.times.push_back(frame_time(f));
        keys.values.push_back(q);
      }
      put(static_cast<size_t>(arm[k]), false, keys);
    }
  };

  if (limbs.empty() && !pelvis_keys) {
    // Steps 1, 4 and 6, then 9: each track on its own (phase 1's clip, exactly, without pose keys).
    for (const Bound& b : bound) {
      std::vector<Value> values;
      for (int f = 0; f <= frames; ++f) values.push_back(Composed(b, frame_time(f)));
      put(b.at, b.track->position, Finish(recipe, values, b.home, b.track->position, frames));
    }
    key_unkeyed_turns();
    hold_grip();
    finish_report();
    *out = clip;
    return true;
  }

  // The composed pose at clip time t (every keyed bone, the rest at rest), world transforms included.
  const pose::Pose rest = pose::RestPose(bones);
  // posed false: the capture alone; skip: a part whose pose keys are left out (the pelvis's, #90).
  const auto composed_pose = [&](float t, bool posed = true, int skip = -1) {
    Pose p = rest;
    for (const Bound& b : bound) {
      const Value v = Composed(b, t, posed && static_cast<int>(b.at) != skip);
      if (b.track->position) p.lp[b.at] = {v[0], v[1], v[2]};
      else p.lq[b.at] = pose::QNormalized(v);
    }
    pose::Forward(bones, &p);
    return p;
  };
  Limb* hips = nullptr;
  for (Limb& L : limbs) {
    if (L.hips) hips = &L;
  }
  // The stance's first-frame root in the world: the hips' targets are offsets from it.
  Vec3 home = {0.0f, 0.0f, 0.0f};
  if (root >= 0 && bones[root].parent >= 0) {
    const Pose p0 = composed_pose(0.0f);
    const int up = bones[root].parent;
    const Value h = StanceValue(stance, bones[root], true);
    home = pose::Add(p0.wp[up], pose::Rotate(p0.wq[up], {h[0], h[1], h[2]}));
  }
  // The hips toward their target by its reach (the root's world place, back into its parent's frame).
  const auto move_hips = [&](Pose* p, float T) {
    if (!hips) return 0.0f;
    const float r = ReachAt(*hips->keys, T);
    if (r <= 0.0f) return 0.0f;
    const int up = bones[root].parent;
    const Vec3 world = Lerp(p->wp[root], PlaceAt(*hips, T, *p, home), r);
    p->lp[root] = pose::Rotate(pose::QConj(p->wq[up]), pose::Sub(world, p->wp[up]));
    pose::Forward(bones, p);
    return r;
  };
  const Limb* keyed_limb[2] = {nullptr, nullptr};  // the feet's limbs (left, right), when they have keys
  for (const Limb& L : limbs) {
    if (L.name == "left_foot") keyed_limb[0] = &L;
    if (L.name == "right_foot") keyed_limb[1] = &L;
  }
  const pose::Chain legs[2] = {pose::FindChain(bones, "lthigh_g", "lshin_g", "lfoot_g"),
                               pose::FindChain(bones, "rthigh_g", "rshin_g", "rfoot_g")};
  const char* leg_names[2] = {"left_foot", "right_foot"};
  // Whether a limb's place at T is fixed in the world: a held (pinned) key's, or easing toward another.
  const auto held_at = [&](const Limb& L, float T) {
    const std::vector<AnimTargetKey>& keys = *L.keys;
    if (T <= keys.front().t) return keys.front().pinned;
    if (T >= keys.back().t || keys.size() < 2) return keys.back().pinned;
    size_t i = 0;
    while (i + 1 < keys.size() && keys[i + 1].t < T) ++i;
    if (!keys[i].pinned) return false;
    const float window = std::min(kEase, std::max(keys[i + 1].t - keys[i].t, 1e-6f));
    return T <= keys[i + 1].t - window || keys[i + 1].pinned;
  };
  // The hips and the legs at take time T, on `now` (the composed pose `before`): the hips toward their
  // target; each foot where the capture planted it, blended toward its target (with `targets`); the legs
  // solved when the hips moved or a foot has a target. A leg whose goal is fixed in the world (a planted
  // foot, a held key) and falls short lowers the root, and both legs solve again - a solve always follows
  // a sink. A foot target from the hip joint never sinks the hips: it moves with them, so sinking can't
  // help it reach (it stays short, reported).
  struct Stood {
    float hips_reach = 0.0f, sunk = 0.0f;
    float shortfall[2] = {0.0f, 0.0f}, reach[2] = {0.0f, 0.0f};
    bool solved[2] = {false, false};
  };
  // Whether a chain's root or mid (an upper arm or forearm, a thigh or shin) is posed at T (#90).
  const auto chain_posed = [&](const pose::Chain& c, float T) {
    for (int b : {c.root, c.mid}) {
      const auto found_part = recipe.poses.find(LowerName(bones[b].name));
      if (found_part != recipe.poses.end() && PoseInfluence(found_part->second, T) > 0.0f) return true;
    }
    return false;
  };
  // The chain's end turn in the world with its root and mid unposed (`bare`: the capture alone) and its own
  // turn kept: while the chain's target has reach, IK wins on the chain, so their pose keys have no effect
  // (the final review's I3), on `p`'s parent turn and end turn.
  const auto chain_free_end = [&](const pose::Chain& c, const Pose& p, const Pose& bare) {
    Quat w = pose::QNormalized(QMul(p.wq[bones[c.root].parent], bare.lq[c.root]));
    w = pose::QNormalized(QMul(w, bare.lq[c.mid]));
    return pose::QNormalized(QMul(w, p.lq[c.end]));
  };
  // `unpelvised`: the same frame without the pelvis's pose keys - a posed pelvis leaves each foot where the
  // rest of the pose puts it, its place and its turn in the world (#90: pose keys turn parts; they don't move
  // the body), continuous in every key's influence (the final review's C1); `bare`: without any pose keys.
  const auto stand = [&](Pose* now, const Pose& before, const Pose& unpelvised, const Pose& bare, float T, bool targets) {
    Stood s;
    s.hips_reach = move_hips(now, T);
    const float pelvis_w = pelvis_keys ? PoseInfluence(*pelvis_keys, T) : 0.0f;
    for (int pass = 0; pass < 3; ++pass) {
      float fixed_short = 0.0f;
      for (int side = 0; side < 2; ++side) {
        const pose::Chain& leg = legs[side];
        if (leg.root < 0) continue;
        const Limb* L = targets ? keyed_limb[side] : nullptr;
        const float r = L ? ReachAt(*L->keys, T) : 0.0f;
        if (r <= 0.0f && s.hips_reach <= 0.0f && s.sunk <= 0.0f && pelvis_w <= 0.0f) continue;
        const Pose& own = pelvis_w > 0.0f ? unpelvised : before;
        const Vec3 planted = own.wp[leg.end];
        const Vec3 goal = r > 0.0f ? Lerp(planted, PlaceAt(*L, T, *now, home), r) : planted;
        const Vec3 knee_way = pose::Rotate(before.wq[leg.root], {0.0f, 1.0f, 0.0f});
        // The foot keeps the pose's turn - the leg's own keys inert under its target, by the reach - and a
        // held key's turn on top, by the reach.
        Quat own_foot = own.wq[leg.end];
        if (r > 0.0f && chain_posed(leg, T)) own_foot = Mix(own_foot, chain_free_end(leg, own, bare), r, true);
        float held = 0.0f;
        const Quat turn = r > 0.0f ? TurnAt(*L, T, &held) : pose::kIdentity;
        const Quat foot = held > 0.0f ? Mix(own_foot, turn, held * r, true) : own_foot;
        // Damping by the reach: a planted foot (reach 0) lands exactly; a target eases near straight.
        s.shortfall[side] = pose::SolveTwoBone(bones, before, now, leg, goal, knee_way, pose::kLimbDamping * r, &foot);
        s.reach[side] = r;
        s.solved[side] = true;
        if (r <= 0.0f || held_at(*L, T)) fixed_short = std::max(fixed_short, s.shortfall[side]);
      }
      pose::Forward(bones, now);
      if (pass == 2 || fixed_short <= 1e-4f || root < 0 || bones[root].parent < 0) break;
      const float drop = std::min(fixed_short, kSinkCap * hip_height - s.sunk);
      if (drop <= 1e-6f) break;
      const int up = bones[root].parent;  // the root lowered in the world, back into its parent's frame
      const Vec3 world = pose::Sub(now->wp[root], {0.0f, 0.0f, drop});
      now->lp[root] = pose::Rotate(pose::QConj(now->wq[up]), pose::Sub(world, now->wp[up]));
      pose::Forward(bones, now);
      s.sunk += drop;
    }
    return s;
  };
  // A pinned key's place (and a foot's, its turn): on the body as it stands at the key's own time - after
  // the hips and the legs on their planted feet, sinking included, as the bench shows it there.
  for (Limb& L : limbs) {
    if (L.hips) continue;
    for (const AnimTargetKey& k : *L.keys) {
      const bool foot = L.frame == pelvis;
      if (!k.pinned) {
        L.held.push_back({0.0f, 0.0f, 0.0f});
        if (foot) L.held_turn.push_back(pose::kIdentity);
        continue;
      }
      const float t = std::max(0.0f, std::min(recipe.length, k.t - take_start));
      const Pose before = composed_pose(t);
      Pose p = before;
      stand(&p, before, pelvis_keys ? composed_pose(t, true, pelvis) : before, any_poses ? composed_pose(t, false) : before, k.t, false);
      L.held.push_back(BodyPlace(L, k, p, home));
      if (foot) L.held_turn.push_back(p.wq[L.chain.end]);  // the foot's turn there, held with the place
    }
  }

  // Each frame: the composed pose, the hips and the legs, the hands. The report after the last solve.
  std::vector<std::vector<Value>> turns(bones.size());
  std::vector<Value> root_places;
  std::vector<bool> solved(bones.size(), false);
  bool root_moved = false;
  for (int f = 0; f <= frames; ++f) {
    const float t = frame_time(f), T = take_start + t;
    const Pose before = composed_pose(t);
    Pose now = before;
    const Pose bare = any_poses ? composed_pose(t, false) : before;
    const Stood s = stand(&now, before, pelvis_keys ? composed_pose(t, true, pelvis) : before, bare, T, true);
    if (s.hips_reach > 0.0f || s.sunk > 0.0f) root_moved = true;
    for (int side = 0; side < 2; ++side) {
      if (!s.solved[side]) continue;
      const pose::Chain& leg = legs[side];
      solved[leg.root] = solved[leg.mid] = solved[leg.end] = true;
      if (s.reach[side] <= 0.0f || s.reach[side] >= 0.999f) record(leg_names[side], s.shortfall[side], T);
    }
    // The hips' own gap: their target against where the root ended (the legs held it back).
    if (hips && s.hips_reach >= 0.999f) record("hips", pose::Length(pose::Sub(PlaceAt(*hips, T, now, home), now.wp[root])), T);
    // The hands: blended toward their targets on the body as it now stands.
    const Pose moved = now;
    for (const Limb& L : limbs) {
      if (L.hips || L.frame != torso) continue;
      const float r = ReachAt(*L.keys, T);
      if (r <= 0.0f) continue;
      const Vec3 goal = Lerp(moved.wp[L.chain.end], PlaceAt(L, T, moved, home), r);
      const Vec3 elbow_way = pose::Rotate(moved.wq[torso], {0.0f, -1.0f, 0.0f});
      // The hand keeps the pose's turn, the arm's own keys inert under its target (by the reach).
      Quat hand = moved.wq[L.chain.end];
      if (chain_posed(L.chain, T)) hand = Mix(hand, chain_free_end(L.chain, moved, bare), r, true);
      const float shortfall = pose::SolveTwoBone(bones, moved, &now, L.chain, goal, elbow_way, pose::kLimbDamping * r, &hand);
      if (r >= 0.999f) record(L.name, shortfall, T);
      solved[L.chain.root] = solved[L.chain.mid] = solved[L.chain.end] = true;
    }
    for (size_t b = 0; b < bones.size(); ++b) turns[b].push_back(now.lq[b]);
    if (root >= 0) root_places.push_back({now.lp[root][0], now.lp[root][1], now.lp[root][2], 0.0f});
  }

  // Step 9 on every keyed bone (a bone the IK never solved keeps the capture's exact values) and on
  // the bones the IK solved without a track of their own (home: their rest).
  std::vector<bool> written(bones.size() * 2, false);  // [bone * 2 + position]
  for (const Bound& b : bound) {
    const bool position = b.track->position;
    std::vector<Value> values;
    const bool from_ik = position ? (b.at == static_cast<size_t>(root) && root_moved) : solved[b.at];
    for (int f = 0; f <= frames; ++f) {
      values.push_back(from_ik ? (position ? root_places[f] : turns[b.at][f]) : Composed(b, frame_time(f)));
    }
    put(b.at, position, Finish(recipe, values, b.home, position, frames));
    written[b.at * 2 + (position ? 1 : 0)] = true;
  }
  for (size_t b = 0; b < bones.size(); ++b) {
    if (solved[b] && !written[b * 2]) {
      const Value home = bones[b].orientation.values.empty() ? Value{0.0f, 0.0f, 0.0f, 1.0f} : bones[b].orientation.values.front();
      put(b, false, Finish(recipe, turns[b], home, false, frames));
    }
  }
  if (root >= 0 && root_moved && !written[root * 2 + 1]) {
    const Value home = bones[root].position.values.empty() ? Value{0.0f, 0.0f, 0.0f, 0.0f} : bones[root].position.values.front();
    put(root, true, Finish(recipe, root_places, home, true, frames));
  }
  key_unkeyed_turns();
  hold_grip();
  // No key spins the long way round: each orientation on the same side as the one before.
  for (AnimNode& node : clip.nodes) {
    if (!node.has_orientation) continue;
    for (size_t k = 1; k < node.orientation.values.size(); ++k) {
      const Value& a = node.orientation.values[k - 1];
      Value& q = node.orientation.values[k];
      if (a[0] * q[0] + a[1] * q[1] + a[2] * q[2] + a[3] * q[3] < 0.0f) q = {-q[0], -q[1], -q[2], -q[3]};
    }
  }
  finish_report();
  *out = clip;
  return true;
}

}  // namespace data
