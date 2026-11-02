#include "data/anim_recipe.h"

#include "data/anim_pose.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <sstream>

namespace data {

using namespace pose;  // the crouch step's pose math (anim_pose.h)

namespace {

constexpr size_t kBase = 12;  // offsets in a compiled model are from the end of the 12-byte file header

std::string Lower(std::string text) {
  for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return text;
}

std::string Trim(const std::string& text) {
  const size_t first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

std::string Num(float value) {
  char text[32];
  snprintf(text, sizeof(text), "%.6g", value);
  return text;
}

struct Bytes {
  const std::string& b;
  uint32_t U32(size_t at) const {
    if (at + 4 > b.size()) return 0;
    uint32_t v;
    memcpy(&v, b.data() + at, 4);
    return v;
  }
  uint16_t U16(size_t at) const {
    if (at + 2 > b.size()) return 0;
    uint16_t v;
    memcpy(&v, b.data() + at, 2);
    return v;
  }
  uint8_t U8(size_t at) const { return at < b.size() ? static_cast<uint8_t>(b[at]) : 0; }
  float F32(size_t at) const {
    if (at + 4 > b.size()) return 0.0f;
    float v;
    memcpy(&v, b.data() + at, 4);
    return v;
  }
  std::string Str(size_t at, size_t max) const {
    std::string out;
    for (size_t i = 0; i < max && at + i < b.size(); ++i) {
      const char c = b[at + i];
      if (c == 0) break;
      out += c;
    }
    return out;
  }
};

// A node's position (8) and orientation (20) keys. Compressed orientations
// (columns flag 0x10) aren't read: the node keeps no keys of its own.
void ReadTracks(const Bytes& m, size_t node, AnimNode* out) {
  const uint32_t keys = m.U32(node + 0x54), key_count = m.U32(node + 0x58);
  const uint32_t data = m.U32(node + 0x60), data_count = m.U32(node + 0x64);
  if (key_count > 96 || data_count > 65536) return;
  for (uint32_t k = 0; k < key_count; ++k) {
    const size_t at = kBase + keys + 12ull * k;
    const uint32_t type = m.U32(at);
    const int rows = m.U16(at + 4), time_at = m.U16(at + 6), value_at = m.U16(at + 8);
    const uint8_t raw_columns = m.U8(at + 10);
    const int columns = raw_columns & 0x0F;
    if ((type != 8 && type != 20) || (raw_columns & 0x10) || rows <= 0) continue;
    if (type == 8 && columns != 3) continue;
    if (type == 20 && columns != 4) continue;
    if (time_at + rows > static_cast<int>(data_count) || value_at + rows * columns > static_cast<int>(data_count)) continue;
    AnimKeys& track = type == 8 ? out->position : out->orientation;
    (type == 8 ? out->has_position : out->has_orientation) = true;
    for (int r = 0; r < rows; ++r) {
      track.times.push_back(m.F32(kBase + data + 4ull * (time_at + r)));
      std::array<float, 4> v = {0.0f, 0.0f, 0.0f, 1.0f};
      for (int c = 0; c < columns; ++c) v[c] = m.F32(kBase + data + 4ull * (value_at + r * columns + c));
      track.values.push_back(v);
    }
  }
}

bool WalkNodes(const Bytes& m, size_t node, int parent, int depth, std::vector<AnimNode>* out) {
  if (depth > 64 || out->size() > 1024 || node + 0x70 > m.b.size()) return false;
  AnimNode n;
  n.name = m.Str(node + 0x20, 32);
  n.parent = parent;
  ReadTracks(m, node, &n);
  const int index = static_cast<int>(out->size());
  out->push_back(n);
  const uint32_t children = m.U32(node + 0x48), count = m.U32(node + 0x4C);
  if (count > 256 || kBase + children + 4ull * count > m.b.size()) return count == 0;
  for (uint32_t c = 0; c < count; ++c) {
    if (!WalkNodes(m, kBase + m.U32(kBase + children + 4ull * c), index, depth + 1, out)) return false;
  }
  return true;
}

}  // namespace

// Shared with anim_assemble.cpp (declared in anim_recipe.h).
std::array<float, 4> Mix(const std::array<float, 4>& a, std::array<float, 4> b, float f, bool quaternion) {
  std::array<float, 4> out;
  if (quaternion) {
    float dot = 0.0f;
    for (int i = 0; i < 4; ++i) dot += a[i] * b[i];
    if (dot < 0.0f) {
      for (float& v : b) v = -v;
    }
  }
  for (int i = 0; i < 4; ++i) out[i] = a[i] + (b[i] - a[i]) * f;
  if (quaternion) {
    float n = 0.0f;
    for (float v : out) n += v * v;
    n = sqrtf(n);
    if (n > 1e-6f) {
      for (float& v : out) v /= n;
    } else {
      out = a;
    }
  }
  return out;
}

std::array<float, 4> Sample(const AnimKeys& keys, float t, bool quaternion) {
  if (keys.times.empty()) return {0.0f, 0.0f, 0.0f, 1.0f};
  if (t <= keys.times.front()) return keys.values.front();
  for (size_t i = 1; i < keys.times.size(); ++i) {
    if (t <= keys.times[i]) {
      const float span = std::max(keys.times[i] - keys.times[i - 1], 1e-6f);
      return Mix(keys.values[i - 1], keys.values[i], (t - keys.times[i - 1]) / span, quaternion);
    }
  }
  return keys.values.back();
}

namespace {

// ASCII orientations are axis + angle.
std::string AxisAngle(const std::array<float, 4>& q) {
  const float w = std::max(-1.0f, std::min(1.0f, q[3]));
  const float angle = 2.0f * acosf(w);
  const float s = sqrtf(std::max(0.0f, 1.0f - w * w));
  if (s < 1e-6f) return "1 0 0 0";
  return Num(q[0] / s) + " " + Num(q[1] / s) + " " + Num(q[2] / s) + " " + Num(angle);
}

const AnimNode* FindNode(const AnimClip& clip, const std::string& name) {
  for (const AnimNode& n : clip.nodes) {
    if (Lower(n.name) == Lower(name)) return &n;
  }
  return nullptr;
}

}  // namespace

// Shared with anim_assemble.cpp (declared in anim_recipe.h).
std::array<float, 4> QMul(const std::array<float, 4>& a, const std::array<float, 4>& b) {
  return {a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
          a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
}

namespace {

bool Under(const std::vector<AnimNode>& bones, int bone, int ancestor) {
  for (int at = bone; at >= 0; at = bones[at].parent) {
    if (at == ancestor) return true;
  }
  return false;
}

}  // namespace

namespace {

// A keys or assembled recipe's tracks: a line per library, bone and channel.
std::string KeyLines(const AnimRecipe& r) {
  std::string text;
  for (const auto& library : r.keys) {
    for (const AnimKeyTrack& track : library.second) {
      text += "keys " + library.first + " " + track.bone + (track.position ? " p" : " o");
      for (size_t k = 0; k < track.keys.times.size(); ++k) {
        const auto& v = track.keys.values[k];
        text += " " + Num(track.keys.times[k]) + " " + Num(v[0]) + " " + Num(v[1]) + " " + Num(v[2]);
        if (!track.position) text += " " + Num(v[3]);
      }
      text += "\n";
    }
  }
  return text;
}

// An assembled recipe's IK targets (#89): take_start, then a line per key, the limbs in their order and
// each limb's keys by time. Nothing without targets (the recipe as phase 1 wrote it).
std::string TargetLines(const AnimRecipe& r) {
  if (r.targets.empty() && r.take_start == 0.0f) return "";
  std::string text = "take_start = " + Num(r.take_start) +
                     "\n# target <limb> <take time> <x y z> <reach> [pin]: the place in the limb's own measure\n";
  for (const std::string& limb : AnimTargetLimbs()) {
    const auto found = r.targets.find(limb);
    if (found == r.targets.end()) continue;
    std::vector<AnimTargetKey> keys = found->second;
    std::stable_sort(keys.begin(), keys.end(), [](const AnimTargetKey& a, const AnimTargetKey& b) { return a.t < b.t; });
    for (const AnimTargetKey& k : keys) {
      text += "target " + limb + " " + Num(k.t) + " " + Num(k.offset[0]) + " " + Num(k.offset[1]) + " " +
              Num(k.offset[2]) + " " + Num(k.reach) + (k.pinned ? " pin" : "") + "\n";
    }
  }
  return text;
}

// A `target` line's words after "target"; false (and the error) on anything but a known limb, five
// finite numbers and an optional "pin".
bool ReadTargetLine(const std::string& words_text, int line_number, AnimRecipe* r, std::string* error) {
  std::istringstream words(words_text);
  std::string limb;
  words >> limb;
  const auto bad = [&](const std::string& why) {
    if (error) *error = "line " + std::to_string(line_number) + ": " + why;
    return false;
  };
  const std::vector<std::string>& limbs = AnimTargetLimbs();
  if (std::find(limbs.begin(), limbs.end(), Lower(limb)) == limbs.end()) {
    return bad("a target's limb is left_hand, right_hand, left_foot, right_foot or hips (not '" + limb + "')");
  }
  float v[5];
  for (float& x : v) {
    std::string word;
    if (!(words >> word)) return bad("a target line is 'target <limb> <t> <x> <y> <z> <reach> [pin]'");
    char* end = nullptr;
    x = strtof(word.c_str(), &end);
    if (end == word.c_str() || *end != '\0' || !isfinite(x)) return bad("'" + word + "' isn't a finite number");
  }
  std::string tail;
  bool pinned = false;
  if (words >> tail) {
    if (tail != "pin") return bad("after a target's reach only 'pin' may follow (not '" + tail + "')");
    pinned = true;
  }
  if (words >> tail) return bad("too many words on a target line");
  AnimTargetKey k;
  k.t = v[0];
  k.offset = {v[1], v[2], v[3]};
  k.reach = std::max(0.0f, std::min(1.0f, v[4]));
  k.pinned = pinned;
  r->targets[Lower(limb)].push_back(k);
  return true;
}

// An assembled recipe's pose keys (#90): a line per key, the parts in their order and each part's keys by
// time. Nothing without poses.
std::string PoseLines(const AnimRecipe& r) {
  std::string text;
  for (const std::string& part : AnimPoseParts()) {
    const auto found = r.poses.find(part);
    if (found == r.poses.end() || found->second.empty()) continue;
    std::vector<AnimPoseKey> keys = found->second;
    std::stable_sort(keys.begin(), keys.end(), [](const AnimPoseKey& a, const AnimPoseKey& b) { return a.t < b.t; });
    if (text.empty()) {
      text = "# pose <part> <take time> <x y z> <push|replace> <ease>: degrees about the part's stance axes, X then Y then Z\n";
    }
    for (const AnimPoseKey& k : keys) {
      text += "pose " + part + " " + Num(k.t) + " " + Num(k.turn[0]) + " " + Num(k.turn[1]) + " " + Num(k.turn[2]) + " " +
              (k.replace ? "replace" : "push") + " " + Num(k.ease) + "\n";
    }
  }
  return text;
}

// A `pose` line's words after "pose"; false (and the error) on anything but a pose part, four finite
// numbers, push or replace, and a finite ease (clamped to 0 - 2).
bool ReadPoseLine(const std::string& words_text, int line_number, AnimRecipe* r, std::string* error) {
  std::istringstream words(words_text);
  std::string part;
  words >> part;
  const auto bad = [&](const std::string& why) {
    if (error) *error = "line " + std::to_string(line_number) + ": " + why;
    return false;
  };
  const std::vector<std::string>& parts = AnimPoseParts();
  if (std::find(parts.begin(), parts.end(), Lower(part)) == parts.end()) {
    return bad("a pose's part is a body part a pose can turn (torso_g, lforearm_g ...; not '" + part + "')");
  }
  const auto number = [&](float* x) {
    std::string word;
    if (!(words >> word)) return bad("a pose line is 'pose <part> <t> <x> <y> <z> <push|replace> <ease>'");
    char* end = nullptr;
    *x = strtof(word.c_str(), &end);
    if (end == word.c_str() || *end != '\0' || !isfinite(*x)) return bad("'" + word + "' isn't a finite number");
    return true;
  };
  AnimPoseKey k;
  if (!number(&k.t) || !number(&k.turn[0]) || !number(&k.turn[1]) || !number(&k.turn[2])) return false;
  std::string mode;
  if (!(words >> mode)) return bad("a pose line is 'pose <part> <t> <x> <y> <z> <push|replace> <ease>'");
  if (mode != "push" && mode != "replace") return bad("a pose is push or replace (not '" + mode + "')");
  k.replace = mode == "replace";
  float ease = 0.0f;
  if (!number(&ease)) return false;
  k.ease = std::max(0.0f, std::min(2.0f, ease));
  std::string tail;
  if (words >> tail) return bad("too many words on a pose line");
  r->poses[Lower(part)].push_back(k);
  return true;
}

}  // namespace

const std::vector<std::string>& AnimPoseParts() {
  static const std::vector<std::string> parts = {
      "torso_g",  "neck_g",  "head_g",  "pelvis_g", "lbicep_g", "lforearm_g", "lhand_g", "rbicep_g",
      "rforearm_g", "rhand_g", "lthigh_g", "lshin_g", "lfoot_g", "rthigh_g", "rshin_g", "rfoot_g"};
  return parts;
}

std::string VariantName(const std::string& name, WeaponGrip grip, const std::function<bool(const std::string&)>& built) {
  const auto pick = [&](const char* suffix) {
    const std::string variant = name + suffix;
    return variant.size() <= 16 && built && built(variant) ? variant : std::string();
  };
  if (grip == WeaponGrip::kPolearm) {
    const std::string pl = pick("_pl");
    if (!pl.empty()) return pl;
  }
  if (grip != WeaponGrip::kOneHand) {
    const std::string two = pick("_2h");
    if (!two.empty()) return two;
  }
  return name;
}

std::string VariantName(const std::string& name, bool two_handed, const std::function<bool(const std::string&)>& built) {
  return VariantName(name, two_handed ? WeaponGrip::kTwoHand : WeaponGrip::kOneHand, built);
}

const std::vector<std::string>& AnimTargetLimbs() {
  static const std::vector<std::string> limbs = {"left_hand", "right_hand", "left_foot", "right_foot", "hips"};
  return limbs;
}

std::string WriteAnimRecipe(const AnimRecipe& r) {
  if (r.kind == AnimRecipeKind::kAssembled) {
    return "# nwn5e animation recipe (Animation Recipe Bench: assembled) - key = value\n"
           "name = " + r.name + "\nkind = assembled\nstance = " + r.stance + "\nlength = " + Num(r.length) +
           "\ntranstime = " + Num(r.transtime) + "\nblend_in = " + Num(r.blend_in) + "\nblend_out = " +
           Num(r.blend_out) + "\nloop = " + (r.loop ? "1" : "0") + "\nloop_blend = " + Num(r.loop_blend) +
           (r.grip ? "\ngrip = 1" : "") + (r.grip && r.grip_follow < 1.0f ? "\ngrip_follow = " + Num(r.grip_follow) : std::string()) +
           "\nsource = " + r.source + "\n" + TargetLines(r) + PoseLines(r) +
           "# keys <library> <bone> o <time x y z w> ...: the turn from the stance's first frame (L = S * delta)"
           "\n# keys <library> rootdummy p <time x y z> ...: the offset from the stance's first-frame root\n" +
           KeyLines(r);
  }
  if (r.keyed || r.kind == AnimRecipeKind::kKeys) {
    return "# nwn5e animation recipe (Animation Recipe Bench: Mocap mode) - key = value\n"
           "name = " + r.name + "\nkind = keys\nlength = " + Num(r.length) + "\ntranstime = " +
           Num(r.transtime) + "\nsource = " + r.source +
           "\n# keys <library> <bone> o <time x y z w> ... | p <time x y z> ...\n" + KeyLines(r);
  }
  return "# nwn5e animation recipe - key = value\n"
         "name = " + r.name + "\nbase = " + r.base + "\nmix = " + r.mix + "\namount = " + Num(r.amount) +
         "\nspeed = " + Num(r.speed) + "\nstart = " + Num(r.start) + "\nend = " + Num(r.end) +
         "\ncrouch = " + Num(r.crouch) + "\nlean = " + Num(r.lean) + "\n";
}

bool ReadAnimRecipe(const std::string& text, AnimRecipe* recipe, std::string* error) {
  AnimRecipe r;
  std::istringstream in(text);
  std::string raw;
  int line_number = 0;
  while (std::getline(in, raw)) {
    ++line_number;
    const std::string line = Trim(raw.substr(0, raw.find('#')));
    if (line.empty()) continue;
    if (line.compare(0, 5, "keys ") == 0) {  // keys <library> <bone> o|p <time values> ...
      std::istringstream words(line.substr(5));
      std::string library, bone, kind;
      words >> library >> bone >> kind;
      if (library.empty() || bone.empty() || (kind != "o" && kind != "p")) {
        if (error) *error = "line " + std::to_string(line_number) + ": a keys line is 'keys <library> <bone> o|p ...'";
        return false;
      }
      AnimKeyTrack track;
      track.bone = bone;
      track.position = kind == "p";
      const int columns = track.position ? 3 : 4;
      float time = 0.0f;
      while (words >> time) {
        std::array<float, 4> v = {0.0f, 0.0f, 0.0f, 1.0f};
        for (int c = 0; c < columns; ++c) {
          if (!(words >> v[c])) {
            if (error) *error = "line " + std::to_string(line_number) + ": a key cut short";
            return false;
          }
        }
        if (!track.keys.times.empty() && time < track.keys.times.back()) {
          if (error) *error = "line " + std::to_string(line_number) + ": keys out of time order";
          return false;
        }
        track.keys.times.push_back(time);
        track.keys.values.push_back(v);
      }
      if (!track.keys.times.empty()) r.keys[Lower(library)].push_back(track);
      continue;
    }
    if (line.compare(0, 7, "target ") == 0) {  // target <limb> <t> <x> <y> <z> <reach> [pin]
      if (!ReadTargetLine(line.substr(7), line_number, &r, error)) return false;
      continue;
    }
    if (line.compare(0, 5, "pose ") == 0) {  // pose <part> <t> <x> <y> <z> <push|replace> <ease>
      if (!ReadPoseLine(line.substr(5), line_number, &r, error)) return false;
      continue;
    }
    const size_t eq = line.find('=');
    if (eq == std::string::npos) {
      if (error) *error = "line " + std::to_string(line_number) + ": no '='";
      return false;
    }
    const std::string key = Lower(Trim(line.substr(0, eq)));
    const std::string value = Trim(line.substr(eq + 1));
    if (key == "name") r.name = value;
    else if (key == "base") r.base = value;
    else if (key == "mix") r.mix = value;
    else if (key == "amount") r.amount = static_cast<float>(atof(value.c_str()));
    else if (key == "speed") r.speed = static_cast<float>(atof(value.c_str()));
    else if (key == "start") r.start = static_cast<float>(atof(value.c_str()));
    else if (key == "end") r.end = static_cast<float>(atof(value.c_str()));
    else if (key == "crouch") r.crouch = std::max(0.0f, std::min(0.6f, static_cast<float>(atof(value.c_str()))));
    else if (key == "lean") r.lean = std::max(-45.0f, std::min(90.0f, static_cast<float>(atof(value.c_str()))));
    else if (key == "kind") {
      const std::string kind = Lower(value);
      r.kind = kind == "keys" ? AnimRecipeKind::kKeys
               : kind == "assembled" ? AnimRecipeKind::kAssembled
                                     : AnimRecipeKind::kFormula;
      r.keyed = r.kind == AnimRecipeKind::kKeys;
    }
    else if (key == "length") r.length = static_cast<float>(atof(value.c_str()));
    else if (key == "transtime") r.transtime = std::max(0.0f, static_cast<float>(atof(value.c_str())));
    else if (key == "source") r.source = value;
    else if (key == "stance") r.stance = value;
    else if (key == "blend_in") r.blend_in = std::max(0.0f, static_cast<float>(atof(value.c_str())));
    else if (key == "blend_out") r.blend_out = std::max(0.0f, static_cast<float>(atof(value.c_str())));
    else if (key == "loop") r.loop = atoi(value.c_str()) != 0;
    else if (key == "loop_blend") r.loop_blend = std::max(0.0f, static_cast<float>(atof(value.c_str())));
    else if (key == "grip") r.grip = atoi(value.c_str()) != 0;
    else if (key == "grip_follow") r.grip_follow = std::max(0.0f, std::min(1.0f, static_cast<float>(atof(value.c_str()))));
    else if (key == "take_start") {
      r.take_start = static_cast<float>(atof(value.c_str()));
      if (!isfinite(r.take_start)) {
        if (error) *error = "line " + std::to_string(line_number) + ": take_start isn't a finite number";
        return false;
      }
    }
  }
  for (auto& limb : r.targets) {  // each limb's keys by time (stable: a file's order breaks ties)
    std::stable_sort(limb.second.begin(), limb.second.end(),
                     [](const AnimTargetKey& a, const AnimTargetKey& b) { return a.t < b.t; });
  }
  for (auto& part : r.poses) {  // each part's keys by time
    std::stable_sort(part.second.begin(), part.second.end(), [](const AnimPoseKey& a, const AnimPoseKey& b) { return a.t < b.t; });
  }
  if (r.kind == AnimRecipeKind::kAssembled) {
    if (r.stance.empty()) {
      if (error) *error = "an assembled recipe needs a stance (the animation its turns are relative to)";
      return false;
    }
    if (r.name.empty() || r.keys.empty() || r.length <= 0.0f) {
      if (error) *error = "an assembled recipe needs a name, a length and keys";
      return false;
    }
    r.base.clear();
    r.mix.clear();
    *recipe = r;
    return true;
  }
  if (r.keyed) {
    if (r.name.empty() || r.keys.empty() || r.length <= 0.0f) {
      if (error) *error = "a keys recipe needs a name, a length and keys";
      return false;
    }
    r.base.clear();
    *recipe = r;
    return true;
  }
  if (r.name.empty() || r.base.empty()) {
    if (error) *error = "a recipe needs a name and a base";
    return false;
  }
  *recipe = r;
  return true;
}

bool ReadAnimLibrary(const std::string& bytes, AnimLibrary* library, std::string* error) {
  const Bytes m{bytes};
  if (bytes.size() < 0x200 || m.U32(0) != 0) {
    if (error) *error = "not a compiled model";
    return false;
  }
  AnimLibrary result;
  result.name = m.Str(kBase + 8, 64);
  result.supermodel = m.Str(kBase + 0xA8, 64);
  if (Lower(result.supermodel) == "null") result.supermodel.clear();
  if (!WalkNodes(m, kBase + m.U32(kBase + 0x48), -1, 0, &result.bones) || result.bones.empty()) {
    if (error) *error = "its bone tree couldn't be read";
    return false;
  }
  const uint32_t clips = m.U32(kBase + 0x78), count = m.U32(kBase + 0x7C);
  if (count > 4096 || kBase + clips + 4ull * count > bytes.size()) {
    if (error) *error = "its animation list couldn't be read";
    return false;
  }
  for (uint32_t i = 0; i < count; ++i) result.clip_names.push_back(Lower(m.Str(kBase + m.U32(kBase + clips + 4ull * i) + 8, 64)));
  *library = result;
  return true;
}

bool ReadAnimClip(const std::string& bytes, const std::string& name, AnimClip* clip) {
  const Bytes m{bytes};
  if (bytes.size() < 0x200 || m.U32(0) != 0) return false;
  const uint32_t clips = m.U32(kBase + 0x78), count = m.U32(kBase + 0x7C);
  if (count > 4096 || kBase + clips + 4ull * count > bytes.size()) return false;
  for (uint32_t i = 0; i < count; ++i) {
    const size_t anim = kBase + m.U32(kBase + clips + 4ull * i);
    if (Lower(m.Str(anim + 8, 64)) != Lower(name)) continue;
    AnimClip result;
    result.name = m.Str(anim + 8, 64);
    result.length = m.F32(anim + 0x70);
    result.transtime = m.F32(anim + 0x74);
    result.animroot = m.Str(anim + 0x78, 64);
    if (!WalkNodes(m, kBase + m.U32(anim + 0x48), -1, 0, &result.nodes) || result.nodes.empty()) return false;
    *clip = result;
    return true;
  }
  return false;
}

#ifndef NWN5E_CORE_STAMP
#define NWN5E_CORE_STAMP "unstamped"
#endif
const char* AnimCoreStamp() { return NWN5E_CORE_STAMP; }

bool FindAnimClip(const AnimLibraryLookup& lookup, const std::string& library, const std::string& name,
                  AnimClip* clip) {
  std::string current = Lower(library);
  for (int depth = 0; depth < 8 && !current.empty(); ++depth) {
    const LoadedAnimLibrary* lib = lookup(current);
    if (!lib) return false;
    const std::vector<std::string>& names = lib->library.clip_names;
    if (std::find(names.begin(), names.end(), Lower(name)) != names.end()) return ReadAnimClip(lib->bytes, name, clip);
    current = Lower(lib->library.supermodel);
  }
  return false;
}

AnimClip DeriveAnimClip(const AnimRecipe& recipe, const AnimClip& base, const AnimClip* mix,
                        const std::vector<AnimNode>* bones) {
  const float length = std::max(base.length, 0.001f);
  const float start = std::max(0.0f, std::min(recipe.start, length));
  float end = recipe.end > start ? std::min(recipe.end, length) : length;
  if (end <= start) end = length;
  const float speed = std::max(0.05f, recipe.speed);
  const bool mixing = mix && !recipe.mix.empty() && recipe.amount != 0.0f;
  const float mix_scale = mixing ? std::max(mix->length, 0.001f) / length : 1.0f;
  AnimClip out;
  out.name = recipe.name;
  out.length = (end - start) / speed;
  out.transtime = base.transtime;
  out.animroot = base.animroot;
  for (const AnimNode& node : base.nodes) {
    AnimNode n;
    n.name = node.name;
    n.parent = node.parent;
    const AnimNode* other = mixing ? FindNode(*mix, node.name) : nullptr;
    for (int track = 0; track < 2; ++track) {
      const bool quaternion = track == 1;
      const AnimKeys& keys = quaternion ? node.orientation : node.position;
      if (!(quaternion ? node.has_orientation : node.has_position) || keys.times.empty()) continue;
      const AnimKeys* toward = other && (quaternion ? other->has_orientation : other->has_position)
                                   ? (quaternion ? &other->orientation : &other->position)
                                   : nullptr;
      std::vector<float> times = {start};
      for (float t : keys.times) {
        if (t > start + 1e-4f && t < end - 1e-4f) times.push_back(t);
      }
      times.push_back(end);
      AnimKeys result;
      for (float t : times) {
        std::array<float, 4> v = Sample(keys, t, quaternion);
        if (toward) v = Mix(v, Sample(*toward, t * mix_scale, quaternion), recipe.amount, quaternion);
        result.times.push_back((t - start) / speed);
        result.values.push_back(v);
      }
      (quaternion ? n.orientation : n.position) = result;
      (quaternion ? n.has_orientation : n.has_position) = true;
    }
    out.nodes.push_back(n);
  }
  if (bones) ApplyCrouch(&out, *bones, recipe.crouch, recipe.lean);
  return out;
}

bool ApplyCrouch(AnimClip* clip, const std::vector<AnimNode>& bones, float crouch, float lean_degrees) {
  const std::vector<int> in_clip = ClipIndex(*clip, bones);
  const auto find = [&](const char* name) {
    for (size_t i = 0; i < bones.size(); ++i) {
      if (Lower(bones[i].name) == name && in_clip[i] >= 0) return static_cast<int>(i);
    }
    return -1;
  };
  // Only what hangs from the animroot (a weapon's walk layer: the torso up).
  const int top = find(Lower(clip->animroot).c_str());
  const auto inside = [&](int bone) { return bone >= 0 && (top < 0 || Under(bones, bone, top)); };
  int root = find("rootdummy"), torso = find("torso_g");
  int head = find("head_g"), pelvis = find("pelvis_g");
  if (!inside(root)) root = -1;
  if (!inside(torso)) torso = -1;
  if (!inside(pelvis)) pelvis = -1;
  if (head >= 0 && (torso < 0 || !Under(bones, head, torso))) head = -1;
  if (pelvis >= 0 && (torso < 0 || Under(bones, pelvis, torso) || Under(bones, torso, pelvis))) pelvis = -1;
  const bool crouching = crouch != 0.0f && root >= 0;
  const bool leaning = lean_degrees != 0.0f && torso >= 0;
  if (!crouching && !leaning) return false;
  const bool hinging = leaning && pelvis >= 0;  // the legs hang from the pelvis: solved again too
  std::vector<int> arms;  // they hang from the torso: turned back to swing as they did in the world
  for (const char* name : {"lbicep_g", "rbicep_g"}) {
    const int arm = find(name);
    if (leaning && arm >= 0 && arm != torso && Under(bones, arm, torso)) arms.push_back(arm);
  }
  std::vector<Leg> legs;
  for (const char* side : {"l", "r"}) {
    const Leg leg = {find((std::string(side) + "thigh_g").c_str()), find((std::string(side) + "shin_g").c_str()),
                     find((std::string(side) + "foot_g").c_str())};
    if (leg.thigh > 0 && leg.shin > 0 && leg.foot > 0 && bones[leg.shin].parent == leg.thigh &&
        bones[leg.foot].parent == leg.shin && bones[leg.thigh].parent >= 0 && inside(leg.thigh)) {
      legs.push_back(leg);
    }
  }
  // Keys every 1/30 s: a solved leg isn't a blend of its neighbours.
  std::vector<float> times;
  const float step = 1.0f / 30.0f;
  for (int k = 0; k * step < clip->length - 1e-4f; ++k) times.push_back(k * step);
  times.push_back(std::max(0.0f, clip->length));
  const float lean = lean_degrees * kPi / 180.0f;
  const Quat lean_turn = AxisAngleQ({1.0f, 0.0f, 0.0f}, -lean);  // +Z toward +Y: forward
  const Quat head_turn = AxisAngleQ({1.0f, 0.0f, 0.0f}, -lean * 0.3f);
  const Quat hip_turn = AxisAngleQ({1.0f, 0.0f, 0.0f}, -lean * 0.35f);
  std::vector<int> moved;
  if (crouching) moved.push_back(root);
  if (leaning) {
    moved.push_back(torso);
    if (head >= 0) moved.push_back(head);
    if (hinging) moved.push_back(pelvis);
    moved.insert(moved.end(), arms.begin(), arms.end());
  }
  if (crouching || hinging) {
    for (const Leg& leg : legs) moved.insert(moved.end(), {leg.thigh, leg.shin, leg.foot});
  }
  std::vector<AnimKeys> keys(bones.size());
  for (float t : times) {
    const Pose before = PoseAt(*clip, in_clip, bones, t);
    Pose now = before;
    if (leaning) {
      const int up = bones[torso].parent;
      now.lq[torso] = QNormalized(QMul(QConj(up >= 0 ? before.wq[up] : kIdentity), QMul(lean_turn, before.wq[torso])));
      if (hinging) {
        const int hip_up = bones[pelvis].parent;
        now.lq[pelvis] =
            QNormalized(QMul(QConj(hip_up >= 0 ? before.wq[hip_up] : kIdentity), QMul(hip_turn, before.wq[pelvis])));
      }
      Forward(bones, &now);
      if (head >= 0) {
        now.lq[head] = QNormalized(QMul(QConj(now.wq[bones[head].parent]), QMul(head_turn, before.wq[head])));
      }
      for (int arm : arms) now.lq[arm] = QNormalized(QMul(QConj(now.wq[bones[arm].parent]), before.wq[arm]));
    }
    if (crouching) {
      const int up = bones[root].parent;
      now.lp[root] = Add(now.lp[root], Rotate(QConj(up >= 0 ? before.wq[up] : kIdentity), {0.0f, 0.0f, -crouch}));
    }
    if (crouching || hinging) {
      Forward(bones, &now);
      for (const Leg& leg : legs) SolveLeg(bones, before, &now, leg);
    }
    for (int b : moved) {
      AnimKeys& k = keys[b];
      k.times.push_back(t);
      if (b == root && crouching) {
        k.values.push_back({now.lp[b][0], now.lp[b][1], now.lp[b][2], 1.0f});
      } else {
        Quat q = now.lq[b];
        if (!k.values.empty()) {  // no sign flip between keys: no spin the long way round
          const auto& last = k.values.back();
          if (last[0] * q[0] + last[1] * q[1] + last[2] * q[2] + last[3] * q[3] < 0.0f) q = {-q[0], -q[1], -q[2], -q[3]};
        }
        k.values.push_back(q);
      }
    }
  }
  for (int b : moved) {
    AnimNode& node = clip->nodes[in_clip[b]];
    if (b == root && crouching) {
      node.position = keys[b];
      node.has_position = true;
    } else {
      node.orientation = keys[b];
      node.has_orientation = true;
    }
  }
  return true;
}

std::vector<std::array<float, 3>> AnimWorldPositions(const AnimClip& clip, const std::vector<AnimNode>& bones, float t) {
  return PoseAt(clip, ClipIndex(clip, bones), bones, t).wp;
}

std::string WriteAnimPack(const std::string& pack, const std::string& library, const std::vector<AnimNode>& bones,
                          const std::vector<AnimClip>& clips) {
  std::vector<std::string> lines = {
      "#MAXMODEL ASCII",
      "# nwn5e animation recipes (core/anim_packs.cpp): built from the player's " + library,
      "newmodel " + pack,
      "setsupermodel " + pack + " " + library,
      "classification Character",
      "setanimationscale 1",
      "beginmodelgeom " + pack,
  };
  const auto node_name = [&](const std::vector<AnimNode>& nodes, int index) {
    return index == 0 ? pack : nodes[index].name;
  };
  for (size_t i = 0; i < bones.size(); ++i) {
    const AnimNode& b = bones[i];
    lines.push_back("node dummy " + node_name(bones, static_cast<int>(i)));
    lines.push_back("  parent " + (b.parent < 0 ? std::string("NULL") : node_name(bones, b.parent)));
    if (b.has_position && !b.position.values.empty()) {
      const auto& p = b.position.values.front();
      lines.push_back("  position " + Num(p[0]) + " " + Num(p[1]) + " " + Num(p[2]));
    }
    if (b.has_orientation && !b.orientation.values.empty()) {
      lines.push_back("  orientation " + AxisAngle(b.orientation.values.front()));
    }
    lines.push_back("endnode");
  }
  lines.push_back("endmodelgeom " + pack);
  for (const AnimClip& clip : clips) {
    lines.push_back("");
    lines.push_back("newanim " + clip.name + " " + pack);
    lines.push_back("  length " + Num(clip.length));
    lines.push_back("  transtime " + Num(clip.transtime));
    lines.push_back("  animroot " + (clip.animroot.empty() ? std::string("rootdummy") : clip.animroot));
    for (size_t i = 0; i < clip.nodes.size(); ++i) {
      const AnimNode& n = clip.nodes[i];
      lines.push_back("node dummy " + node_name(clip.nodes, static_cast<int>(i)));
      lines.push_back("  parent " + (n.parent < 0 ? std::string("NULL") : node_name(clip.nodes, n.parent)));
      if (n.has_position) {
        lines.push_back("  positionkey " + std::to_string(n.position.times.size()));
        for (size_t k = 0; k < n.position.times.size(); ++k) {
          const auto& p = n.position.values[k];
          lines.push_back("    " + Num(n.position.times[k]) + " " + Num(p[0]) + " " + Num(p[1]) + " " + Num(p[2]));
        }
      }
      if (n.has_orientation) {
        lines.push_back("  orientationkey " + std::to_string(n.orientation.times.size()));
        for (size_t k = 0; k < n.orientation.times.size(); ++k) {
          lines.push_back("    " + Num(n.orientation.times[k]) + " " + AxisAngle(n.orientation.values[k]));
        }
      }
      lines.push_back("endnode");
    }
    lines.push_back("doneanim " + clip.name + " " + pack);
  }
  lines.push_back("");
  lines.push_back("donemodel " + pack);
  std::string text;
  for (const std::string& line : lines) text += line + "\n";
  return text;
}

bool KeyedAnimClip(const AnimRecipe& recipe, const std::string& library, const std::vector<AnimNode>& bones,
                   AnimClip* clip) {
  const auto found = recipe.keys.find(Lower(library));
  if (!recipe.keyed || found == recipe.keys.end() || bones.empty()) return false;
  AnimClip out;
  out.name = recipe.name;
  out.length = recipe.length;
  out.transtime = recipe.transtime;
  out.animroot = "rootdummy";
  out.nodes = bones;  // the library's tree, unkeyed: bones the recipe doesn't key stay at rest
  for (AnimNode& node : out.nodes) {
    node.has_position = node.has_orientation = false;
    node.position = AnimKeys();
    node.orientation = AnimKeys();
  }
  int keyed = 0;
  for (const AnimKeyTrack& track : found->second) {
    for (AnimNode& node : out.nodes) {
      if (Lower(node.name) != Lower(track.bone)) continue;
      if (track.position) {
        node.has_position = true;
        node.position = track.keys;
      } else {
        node.has_orientation = true;
        node.orientation = track.keys;
      }
      ++keyed;
      break;
    }
  }
  if (!keyed) return false;
  *clip = out;
  return true;
}

}  // namespace data
