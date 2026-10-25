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

}  // namespace

std::string WriteAnimRecipe(const AnimRecipe& r) {
  return "# nwn5e animation recipe (!animedit) - key = value\n"
         "name = " + r.name + "\nbase = " + r.base + "\nmix = " + r.mix + "\namount = " + Num(r.amount) +
         "\nspeed = " + Num(r.speed) + "\nstart = " + Num(r.start) + "\nend = " + Num(r.end) + "\n";
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

AnimClip DeriveAnimClip(const AnimRecipe& recipe, const AnimClip& base, const AnimClip* mix) {
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
  return out;
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
