#include "data/anim_assemble.h"

#include <ctype.h>
#include <math.h>

#include <algorithm>
#include <array>

namespace data {
namespace {

using Value = std::array<float, 4>;

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

}  // namespace

bool AssembleAnimClip(const AnimRecipe& recipe, const std::string& library, const std::vector<AnimNode>& bones,
                      const AnimClip& stance, AnimClip* out, std::string* err) {
  const auto found = recipe.keys.find(LowerName(library));
  if (found == recipe.keys.end() || found->second.empty()) {
    if (err) *err = recipe.name + " has no keys for " + library;
    return false;
  }
  if (stance.nodes.empty()) {
    if (err) *err = recipe.name + ": the stance " + recipe.stance + " has no frame 0 on " + library;
    return false;
  }
  if (bones.empty() || recipe.length <= 0.0f) {
    if (err) *err = recipe.name + ": no bones or no length for " + library;
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
  int keyed = 0;
  for (const AnimKeyTrack& track : found->second) {
    if (track.keys.times.empty()) continue;
    size_t at = 0;
    while (at < bones.size() && LowerName(bones[at].name) != LowerName(track.bone)) ++at;
    if (at == bones.size()) continue;
    const bool position = track.position;
    const Value home = StanceValue(stance, bones[at], position);
    // Step 1 (and 6 for the root): the capture on the stance, before the loop and the blends.
    const auto composed = [&](float t) -> Value {
      const Value d = DeltaAt(track, t);
      if (position) return {home[0] + d[0], home[1] + d[1], home[2] + d[2], 0.0f};
      return QMul(home, d);
    };
    const Value first = composed(0.0f);
    AnimKeys keys;
    for (int f = 0; f <= frames; ++f) {
      const float t = recipe.length * static_cast<float>(f) / static_cast<float>(frames);
      Value v = composed(t);
      // Step 9: the loop under the blends.
      v = Mix(v, first, LoopWeight(recipe, t), !position);
      v = Mix(home, v, BlendWeight(recipe, t), !position);
      keys.times.push_back(t);
      keys.values.push_back(v);
    }
    AnimNode& node = clip.nodes[at];
    if (position) {
      node.has_position = true;
      node.position = keys;
    } else {
      node.has_orientation = true;
      node.orientation = keys;
    }
    ++keyed;
  }
  if (!keyed) {
    if (err) *err = recipe.name + ": none of its bones for " + library + " is in the library's tree";
    return false;
  }
  *out = clip;
  return true;
}

}  // namespace data
