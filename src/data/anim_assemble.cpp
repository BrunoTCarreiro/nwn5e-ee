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

// A keyed bone: its delta track and its value in the stance's first frame.
struct Bound {
  const AnimKeyTrack* track;
  size_t at;
  Value home;
};

// Steps 1 and 6: the capture composed onto the stance at t, before the loop and the blends.
Value Composed(const Bound& b, float t) {
  const Value d = DeltaAt(*b.track, t);
  if (b.track->position) return {b.home[0] + d[0], b.home[1] + d[1], b.home[2] + d[2], 0.0f};
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

struct Limb {
  std::string name;
  bool hips = false;
  pose::Chain chain;      // hands and feet (the hips have none)
  int frame = -1;         // torso_g (hands), pelvis_g (feet): the frame the offset is in
  float length = 1.0f;    // the chain's rest length, or the hip height for the hips
  const std::vector<AnimTargetKey>* keys = nullptr;
  std::vector<Vec3> held;  // a pinned key's place in the world at its own time
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
  clip.nodes = bones;  // the library's tree, unkeyed: bones without a track play rest
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
  std::vector<Limb> limbs;
  for (const std::string& name : AnimTargetLimbs()) {
    const auto keys = recipe.targets.find(name);
    if (keys == recipe.targets.end() || keys->second.empty()) continue;
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

  if (limbs.empty()) {
    // Steps 1 and 6, then 9: each track on its own (phase 1's clip, exactly).
    for (const Bound& b : bound) {
      std::vector<Value> values;
      for (int f = 0; f <= frames; ++f) values.push_back(Composed(b, frame_time(f)));
      put(b.at, b.track->position, Finish(recipe, values, b.home, b.track->position, frames));
    }
    *out = clip;
    return true;
  }

  // The composed pose at clip time t (every keyed bone, the rest at rest), world transforms included.
  const pose::Pose rest = pose::RestPose(bones);
  const auto composed_pose = [&](float t) {
    Pose p = rest;
    for (const Bound& b : bound) {
      const Value v = Composed(b, t);
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
  const auto stand = [&](Pose* now, const Pose& before, float T, bool targets) {
    Stood s;
    s.hips_reach = move_hips(now, T);
    for (int pass = 0; pass < 3; ++pass) {
      float fixed_short = 0.0f;
      for (int side = 0; side < 2; ++side) {
        const pose::Chain& leg = legs[side];
        if (leg.root < 0) continue;
        const Limb* L = targets ? keyed_limb[side] : nullptr;
        const float r = L ? ReachAt(*L->keys, T) : 0.0f;
        if (r <= 0.0f && s.hips_reach <= 0.0f && s.sunk <= 0.0f) continue;
        const Vec3 planted = before.wp[leg.end];
        const Vec3 goal = r > 0.0f ? Lerp(planted, PlaceAt(*L, T, *now, home), r) : planted;
        const Vec3 knee_way = pose::Rotate(before.wq[leg.root], {0.0f, 1.0f, 0.0f});
        // Damping by the reach: a planted foot (reach 0) lands exactly; a target eases near straight.
        s.shortfall[side] = pose::SolveTwoBone(bones, before, now, leg, goal, knee_way, pose::kLimbDamping * r);
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
  // A pinned key's place: on the body as it stands at the key's own time - after the hips and the legs on
  // their planted feet, sinking included, as the bench shows it there.
  for (Limb& L : limbs) {
    if (L.hips) continue;
    for (const AnimTargetKey& k : *L.keys) {
      if (!k.pinned) {
        L.held.push_back({0.0f, 0.0f, 0.0f});
        continue;
      }
      const float t = std::max(0.0f, std::min(recipe.length, k.t - take_start));
      const Pose before = composed_pose(t);
      Pose p = before;
      stand(&p, before, k.t, false);
      L.held.push_back(BodyPlace(L, k, p, home));
    }
  }

  // Each frame: the composed pose, the hips and the legs, the hands. The report after the last solve.
  std::map<std::string, AnimIkShort> worst;  // limb -> its worst shortfall while its goal had full weight
  const auto record = [&](const std::string& limb, float shortfall, float T) {
    AnimIkShort& w = worst[limb];
    w.limb = limb;
    if (shortfall > w.metres) {
      w.metres = shortfall;
      w.t = T;
    }
  };
  std::vector<std::vector<Value>> turns(bones.size());
  std::vector<Value> root_places;
  std::vector<bool> solved(bones.size(), false);
  bool root_moved = false;
  for (int f = 0; f <= frames; ++f) {
    const float t = frame_time(f), T = take_start + t;
    const Pose before = composed_pose(t);
    Pose now = before;
    const Stood s = stand(&now, before, T, true);
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
      const float shortfall = pose::SolveTwoBone(bones, moved, &now, L.chain, goal, elbow_way, pose::kLimbDamping * r);
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
  // No key spins the long way round: each orientation on the same side as the one before.
  for (AnimNode& node : clip.nodes) {
    if (!node.has_orientation) continue;
    for (size_t k = 1; k < node.orientation.values.size(); ++k) {
      const Value& a = node.orientation.values[k - 1];
      Value& q = node.orientation.values[k];
      if (a[0] * q[0] + a[1] * q[1] + a[2] * q[2] + a[3] * q[3] < 0.0f) q = {-q[0], -q[1], -q[2], -q[3]};
    }
  }
  if (report) {
    for (const auto& w : worst) {
      if (w.second.metres > kReportOver) report->push_back(w.second);
    }
  }
  *out = clip;
  return true;
}

}  // namespace data
