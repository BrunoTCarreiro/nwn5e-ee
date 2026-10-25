#include "data/anim_recipe.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <sstream>

namespace data {
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

// --- The crouch step: vectors, quaternions (x y z w), a pose ------------------------

using Vec3 = std::array<float, 3>;
using Quat = std::array<float, 4>;
constexpr Quat kIdentity = {0.0f, 0.0f, 0.0f, 1.0f};
constexpr float kPi = 3.14159265f;

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

Quat QMul(const Quat& a, const Quat& b) {
  return {a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
          a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
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
// The shortest turn taking direction a to direction b.
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

// Local transforms (the clip's keys at t, else the bone's rest) and world ones.
struct Pose {
  std::vector<Vec3> lp, wp;
  std::vector<Quat> lq, wq;
};

std::vector<int> ClipIndex(const AnimClip& clip, const std::vector<AnimNode>& bones) {
  std::vector<int> index(bones.size(), -1);
  for (size_t i = 0; i < bones.size(); ++i) {
    const std::string name = Lower(bones[i].name);
    for (size_t k = 0; k < clip.nodes.size(); ++k) {
      if (Lower(clip.nodes[k].name) == name) {
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

struct Leg {
  int thigh, shin, foot;
};

// The thigh and shin turned so the ankle reaches where it was in `before`,
// the knee in the plane it bent in; the foot keeps its turn in the world.
void SolveLeg(const std::vector<AnimNode>& bones, const Pose& before, Pose* now, const Leg& leg) {
  const Vec3 hip = now->wp[leg.thigh];
  const Vec3 target = before.wp[leg.foot];
  const float l1 = Length(now->lp[leg.shin]), l2 = Length(now->lp[leg.foot]);
  const Vec3 to = Sub(target, hip);
  const float reach = Length(to);
  if (reach < 1e-5f || l1 < 1e-5f || l2 < 1e-5f) return;
  const Vec3 u = Scale(to, 1.0f / reach);
  const float d = std::max(fabsf(l1 - l2) + 1e-4f, std::min(l1 + l2 - 1e-4f, reach));
  // The knee bends the way it did - off the old hip-ankle line (against the new
  // one a straight front leg reads as bent backward: the knee flipped) - and a
  // nearly straight leg the way its thigh faces.
  const Vec3 old_line = Normalized(Sub(before.wp[leg.foot], before.wp[leg.thigh]));
  Vec3 bend = Sub(before.wp[leg.shin], before.wp[leg.thigh]);
  bend = Sub(bend, Scale(old_line, Dot(bend, old_line)));
  const Vec3 facing = Normalized(Rotate(before.wq[leg.thigh], Vec3{0.0f, 1.0f, 0.0f}));
  Vec3 pole = Add(bend, Scale(facing, 0.05f));
  pole = Sub(pole, Scale(u, Dot(pole, u)));
  if (Length(pole) < 1e-6f) return;
  pole = Normalized(pole);
  const float a = (l1 * l1 - l2 * l2 + d * d) / (2.0f * d);
  const float h = sqrtf(std::max(0.0f, l1 * l1 - a * a));
  const Vec3 knee = Add(hip, Add(Scale(u, a), Scale(pole, h)));
  const Quat up = now->wq[bones[leg.thigh].parent];
  const Quat thigh = QNormalized(QMul(FromTo(Rotate(now->wq[leg.thigh], now->lp[leg.shin]), Sub(knee, hip)), now->wq[leg.thigh]));
  Quat shin = QNormalized(QMul(thigh, now->lq[leg.shin]));
  shin = QNormalized(QMul(FromTo(Rotate(shin, now->lp[leg.foot]), Sub(target, knee)), shin));
  now->lq[leg.thigh] = QNormalized(QMul(QConj(up), thigh));
  now->lq[leg.shin] = QNormalized(QMul(QConj(thigh), shin));
  now->lq[leg.foot] = QNormalized(QMul(QConj(shin), before.wq[leg.foot]));
}

bool Under(const std::vector<AnimNode>& bones, int bone, int ancestor) {
  for (int at = bone; at >= 0; at = bones[at].parent) {
    if (at == ancestor) return true;
  }
  return false;
}

}  // namespace

std::string WriteAnimRecipe(const AnimRecipe& r) {
  return "# nwn5e animation recipe (!animedit) - key = value\n"
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
  const int root = find("rootdummy"), torso = find("torso_g");
  int head = find("head_g"), pelvis = find("pelvis_g");
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
        bones[leg.foot].parent == leg.shin && bones[leg.thigh].parent >= 0) {
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

}  // namespace data
