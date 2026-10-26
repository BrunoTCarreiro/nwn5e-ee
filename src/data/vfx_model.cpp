#include "data/vfx_model.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <algorithm>
#include <sstream>

namespace data {
namespace {

std::string Num(float value) {
  char text[32];
  snprintf(text, sizeof(text), "%g", value);
  return text;
}

std::string Lower(std::string text) {
  for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return text;
}

std::string Trim(const std::string& text) {
  const size_t first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

std::vector<std::string> Words(const std::string& text) {
  std::vector<std::string> words;
  std::istringstream in(text);
  std::string word;
  while (in >> word) words.push_back(word);
  return words;
}

float ToFloat(const std::string& text) { return static_cast<float>(atof(text.c_str())); }

void ReadFloats(const std::vector<std::string>& words, size_t from, float* out, int count) {
  for (int i = 0; i < count && from + i < words.size(); ++i) out[i] = ToFloat(words[from + i]);
}

std::vector<std::string> EmitterLines(const VfxEmitter& e, const std::string& node, const std::string& parent,
                                      bool keyed_birthrate) {
  const auto flag = [](const char* name, bool on) { return std::string("  ") + name + (on ? " 1" : " 0"); };
  return {
      "node emitter " + node,
      "  parent " + parent,
      "  p2p 0", "  p2p_sel 1", "  affectedByWind 0", "  m_isTinted 0", flag("bounce", e.bounce),
      flag("random", e.random), flag("inherit", e.inherit), "  inheritvel 0", "  inherit_local 0",
      flag("splat", e.splat), "  inherit_part 0", "  renderorder 0", "  spawntype 0",
      "  update " + e.update,
      "  render " + e.render,
      "  blend " + e.blend,
      "  texture " + e.texture,
      "  xgrid " + std::to_string(e.grid), "  ygrid " + std::to_string(e.grid), "  loop 0", "  deadspace 0",
      "  twosidedtex 0", "  blastRadius 0", "  blastLength 0",
      "  position " + Num(e.position[0]) + " " + Num(e.position[1]) + " " + Num(e.position[2]),
      "  orientation 0 0 0 0",
      "  colorStart " + Num(e.color_start[0]) + " " + Num(e.color_start[1]) + " " + Num(e.color_start[2]),
      "  colorEnd " + Num(e.color_end[0]) + " " + Num(e.color_end[1]) + " " + Num(e.color_end[2]),
      "  alphaStart " + Num(e.alpha_start), "  alphaEnd " + Num(e.alpha_end),
      "  sizeStart " + Num(e.size_start), "  sizeEnd " + Num(e.size_end),
      "  sizeStart_y 0", "  sizeEnd_y 0",
      "  frameStart 0", "  frameEnd " + std::to_string(e.frame_end),
      "  birthrate " + Num(keyed_birthrate ? 0.0f : e.birthrate),
      "  lifeExp " + Num(e.life),
      "  mass " + Num(e.mass), "  spread " + Num(e.spread), "  particleRot 0",
      "  velocity " + Num(e.velocity), "  randvel " + Num(e.randvel), "  fps 0",
      "  xsize " + Num(e.xsize), "  ysize " + Num(e.ysize), "  bounce_co " + Num(e.bounce_co),
      "  blurlength " + Num(e.blur_length),
      "  lightningDelay 0", "  lightningRadius 0", "  lightningScale 0",
      "  p2p_bezier2 0", "  p2p_bezier3 0", "  combinetime 0", "  drag 0", "  grav 0", "  threshold 0",
      "endnode",
  };
}

// Birthrate keys (time, value): the most it reaches, and when it switches off
// for good - where it reaches 0 after its last positive key (the game's models
// key it down to a negative value: 491's sticky drops 150 -> -150 from 0.033 s
// to 0.3 s, off at 0.17 s); a drop to 0 within 0.011 s is nwn5e's own "off"
// key (on until `emit`, 0 at emit + 0.01), read as `emit`. 0 when it stays on.
void ReadBirthrateKeys(const std::vector<std::pair<float, float>>& keys, float* top, float* off) {
  *top = 0.0f;
  *off = 0.0f;
  int last_on = -1;
  for (size_t i = 0; i < keys.size(); ++i) {
    if (keys[i].second > 0.0f) last_on = static_cast<int>(i);
    *top = std::max(*top, keys[i].second);
  }
  if (last_on < 0 || last_on + 1 >= static_cast<int>(keys.size())) return;  // never on, or on to the end
  const auto& on = keys[last_on];
  const auto& next = keys[last_on + 1];
  if (next.second == 0.0f) {
    *off = next.first - on.first <= 0.011f ? on.first : next.first;
  } else {
    *off = on.first + (next.first - on.first) * on.second / (on.second - next.second);
  }
}

std::string NodeName(const std::string& model, const VfxEmitter& e, size_t index) {
  std::string clean = VfxResRef(e.name, 20);
  if (clean.empty()) clean = "e";
  return model + "_" + std::to_string(index) + "_" + clean;
}

}  // namespace

std::string VfxResRef(const std::string& text, size_t max) {
  std::string out;
  for (char c : text) {
    if (out.size() >= max) break;
    const unsigned char u = static_cast<unsigned char>(c);
    if (isalnum(u) || c == '_') out += static_cast<char>(tolower(u));
  }
  return out;
}

std::string WriteVfxModel(const VfxEffect& effect, const std::string& model) {
  std::vector<std::string> lines = {
      "#MAXMODEL ASCII",
      "# nwn5e VFX editor (!vfxedit): " + effect.name,
      "newmodel " + model,
      "setsupermodel " + model + " NULL",
      "classification effect",
      "setanimationscale 1",
      "beginmodelgeom " + model,
      "node dummy " + model,
      "  parent NULL",
      "endnode",
  };
  for (size_t i = 0; i < effect.emitters.size(); ++i) {
    const VfxEmitter& e = effect.emitters[i];
    // An emitter that switches off is keyed in the impact; F's always-on ones are too.
    const bool keyed = e.emit_seconds > 0.0f || !effect.duration;
    for (const std::string& line : EmitterLines(e, NodeName(model, e, i), model, keyed)) lines.push_back(line);
  }
  lines.push_back("endmodelgeom " + model);
  // The impact: long enough for every emitter's emission (and, F, what it emitted to fade).
  float impact = 0.2f;
  for (const VfxEmitter& e : effect.emitters) {
    const float on = e.emit_seconds > 0.0f ? e.emit_seconds : (effect.duration ? 0.0f : effect.seconds);
    impact = std::max(impact, on + 0.01f + (effect.duration ? 0.0f : e.life));
  }
  const auto anim = [&](const char* name, float length, bool keys_on, bool keys_off) {
    lines.push_back("");
    lines.push_back(std::string("newanim ") + name + " " + model);
    lines.push_back("  length " + Num(length));
    lines.push_back("  transtime 0.25");
    lines.push_back("  animroot " + model);
    // Explosion emitters burst on the impact's detonate event (the game's own models have it).
    if (std::string(name) == "impact") lines.push_back("  event 0 detonate");
    lines.push_back("node dummy " + model);
    lines.push_back("  parent NULL");
    lines.push_back("endnode");
    for (size_t i = 0; i < effect.emitters.size(); ++i) {
      const VfxEmitter& e = effect.emitters[i];
      lines.push_back("node emitter " + NodeName(model, e, i));
      lines.push_back("  parent " + model);
      if (keys_off) {
        lines.push_back("  birthratekey 1");
        lines.push_back("    0 0");
      } else if (keys_on && (e.emit_seconds > 0.0f || !effect.duration)) {
        const float on = e.emit_seconds > 0.0f ? e.emit_seconds : effect.seconds;
        lines.push_back("  birthratekey 3");
        lines.push_back("    0 " + Num(e.birthrate));
        lines.push_back("    " + Num(on) + " " + Num(e.birthrate));
        lines.push_back("    " + Num(on + 0.01f) + " 0");
      }
      lines.push_back("endnode");
    }
    lines.push_back(std::string("doneanim ") + name + " " + model);
  };
  anim("impact", impact, true, false);
  if (effect.duration) {
    anim("duration", 2.0f, false, false);
    anim("cessation", 1.0f, false, true);
  }
  lines.push_back("");
  lines.push_back("donemodel " + model);
  std::string text;
  for (const std::string& line : lines) text += line + "\n";
  return text;
}

std::string WriteVfxDraft(const VfxEffect& effect) {
  std::string out = "# nwn5e VFX draft (!vfxedit) - key = value; one [emitter <name>] section per emitter\n";
  out += "name = " + effect.name + "\n";
  out += std::string("type = ") + (effect.duration ? "D" : "F") + "\n";
  out += "seconds = " + Num(effect.seconds) + "\n";
  out += "attach = " + effect.attach + "\n";
  for (const VfxEmitter& e : effect.emitters) {
    out += "\n[emitter " + e.name + "]\n";
    out += "update = " + e.update + "\nrender = " + e.render + "\nblend = " + e.blend + "\ntexture = " + e.texture + "\n";
    out += "position = " + Num(e.position[0]) + " " + Num(e.position[1]) + " " + Num(e.position[2]) + "\n";
    out += "birthrate = " + Num(e.birthrate) + "\nemit_seconds = " + Num(e.emit_seconds) + "\nlife = " + Num(e.life) + "\n";
    out += "velocity = " + Num(e.velocity) + "\nrandvel = " + Num(e.randvel) + "\nspread = " + Num(e.spread) + "\n";
    out += "mass = " + Num(e.mass) + "\nsize = " + Num(e.size_start) + " " + Num(e.size_end) + "\n";
    out += "color_start = " + Num(e.color_start[0]) + " " + Num(e.color_start[1]) + " " + Num(e.color_start[2]) + "\n";
    out += "color_end = " + Num(e.color_end[0]) + " " + Num(e.color_end[1]) + " " + Num(e.color_end[2]) + "\n";
    out += "alpha = " + Num(e.alpha_start) + " " + Num(e.alpha_end) + "\n";
    out += "area = " + Num(e.xsize) + " " + Num(e.ysize) + "\n";
    out += "grid = " + std::to_string(e.grid) + "\nframe_end = " + std::to_string(e.frame_end) + "\n";
    out += "blur_length = " + Num(e.blur_length) + "\nbounce_co = " + Num(e.bounce_co) + "\n";
    std::string flags;
    if (e.inherit) flags += " inherit";
    if (e.splat) flags += " splat";
    if (e.bounce) flags += " bounce";
    if (e.random) flags += " random";
    out += "flags =" + flags + "\n";
  }
  return out;
}

bool ReadVfxDraft(const std::string& text, VfxEffect* effect, std::string* error) {
  VfxEffect result;
  VfxEmitter* e = nullptr;
  std::istringstream in(text);
  std::string raw;
  int line_number = 0;
  while (std::getline(in, raw)) {
    ++line_number;
    const std::string line = Trim(raw.substr(0, raw.find('#')));
    if (line.empty()) continue;
    if (line.front() == '[') {
      const std::string inner = Trim(line.substr(1, line.find(']') == std::string::npos ? std::string::npos
                                                                                          : line.find(']') - 1));
      if (Lower(inner).rfind("emitter", 0) != 0) {
        if (error) *error = "line " + std::to_string(line_number) + ": unknown section [" + inner + "]";
        return false;
      }
      result.emitters.emplace_back();
      e = &result.emitters.back();
      e->name = Trim(inner.substr(7));
      if (e->name.empty()) e->name = "emitter";
      continue;
    }
    const size_t eq = line.find('=');
    if (eq == std::string::npos) {
      if (error) *error = "line " + std::to_string(line_number) + ": no '='";
      return false;
    }
    const std::string key = Lower(Trim(line.substr(0, eq)));
    const std::string value = Trim(line.substr(eq + 1));
    const std::vector<std::string> words = Words(value);
    if (!e) {
      if (key == "name") result.name = value;
      else if (key == "type") result.duration = Lower(value) != "f";
      else if (key == "seconds") result.seconds = ToFloat(value);
      else if (key == "attach") result.attach = Lower(value);
      continue;
    }
    if (key == "update") e->update = value;
    else if (key == "render") e->render = value;
    else if (key == "blend") e->blend = value;
    else if (key == "texture") e->texture = value;
    else if (key == "position") ReadFloats(words, 0, e->position, 3);
    else if (key == "birthrate") e->birthrate = ToFloat(value);
    else if (key == "emit_seconds") e->emit_seconds = ToFloat(value);
    else if (key == "life") e->life = ToFloat(value);
    else if (key == "velocity") e->velocity = ToFloat(value);
    else if (key == "randvel") e->randvel = ToFloat(value);
    else if (key == "spread") e->spread = ToFloat(value);
    else if (key == "mass") e->mass = ToFloat(value);
    else if (key == "size") {
      float size[2] = {e->size_start, e->size_end};
      ReadFloats(words, 0, size, 2);
      e->size_start = size[0];
      e->size_end = size[1];
    } else if (key == "color_start") ReadFloats(words, 0, e->color_start, 3);
    else if (key == "color_end") ReadFloats(words, 0, e->color_end, 3);
    else if (key == "alpha") {
      float alpha[2] = {e->alpha_start, e->alpha_end};
      ReadFloats(words, 0, alpha, 2);
      e->alpha_start = alpha[0];
      e->alpha_end = alpha[1];
    } else if (key == "area") {
      float area[2] = {e->xsize, e->ysize};
      ReadFloats(words, 0, area, 2);
      e->xsize = area[0];
      e->ysize = area[1];
    } else if (key == "grid") e->grid = std::max(1, atoi(value.c_str()));
    else if (key == "frame_end") e->frame_end = std::max(0, atoi(value.c_str()));
    else if (key == "blur_length") e->blur_length = ToFloat(value);
    else if (key == "bounce_co") e->bounce_co = ToFloat(value);
    else if (key == "flags") {
      for (const std::string& word : words) {
        const std::string f = Lower(word);
        e->inherit = e->inherit || f == "inherit";
        e->splat = e->splat || f == "splat";
        e->bounce = e->bounce || f == "bounce";
        e->random = e->random || f == "random";
      }
    }
  }
  *effect = result;
  return true;
}

bool ReadVfxAsciiModel(const std::string& text, VfxEffect* effect, std::string* error) {
  VfxEffect result;
  result.emitters.clear();
  std::istringstream in(text);
  std::string raw;
  enum class Where { kOutside, kGeometry, kAnim } where = Where::kOutside;
  std::string anim;
  VfxEmitter* e = nullptr;          // geometry: the emitter being read
  VfxEmitter* keyed = nullptr;      // an animation's emitter node
  std::vector<std::string> names;   // node names, parallel to emitters
  std::string key_name;
  int key_rows = 0;
  std::vector<std::pair<float, float>> keys;
  bool has_duration = false;
  const auto finish_keys = [&]() {
    if (!keyed || keys.empty()) return;
    if (key_name == "birthratekey" && anim == "impact") {
      float top = 0.0f, off = 0.0f;
      ReadBirthrateKeys(keys, &top, &off);
      if (top > 0.0f) keyed->birthrate = std::max(keyed->birthrate, top);
      if (off > 0.0f) keyed->emit_seconds = off;
    } else if ((key_name == "sizestartkey" || key_name == "sizeendkey") && keyed->size_start == 0.0f) {
      float top = 0.0f;
      for (const auto& k : keys) top = std::max(top, k.second);
      if (top > 0.0f) {
        keyed->size_start = top;
        keyed->size_end = top;
      }
    }
    keys.clear();
    key_name.clear();
    key_rows = 0;
  };
  while (std::getline(in, raw)) {
    const std::vector<std::string> w = Words(raw.substr(0, raw.find('#')));
    if (w.empty()) continue;
    const std::string first = Lower(w[0]);
    if (key_rows > 0 && w.size() >= 2 && (isdigit(static_cast<unsigned char>(w[0][0])) || w[0][0] == '-' ||
                                           w[0][0] == '.')) {
      keys.emplace_back(ToFloat(w[0]), ToFloat(w[1]));
      if (--key_rows == 0) finish_keys();
      continue;
    }
    if (first == "newmodel" && w.size() >= 2) result.name = w[1];
    else if (first == "beginmodelgeom") where = Where::kGeometry;
    else if (first == "endmodelgeom") where = Where::kOutside;
    else if (first == "newanim" && w.size() >= 2) {
      where = Where::kAnim;
      anim = Lower(w[1]);
      if (anim == "duration") has_duration = true;
    } else if (first == "doneanim") {
      where = Where::kOutside;
      keyed = nullptr;
    } else if (first == "node" && w.size() >= 3) {
      e = nullptr;
      keyed = nullptr;
      if (Lower(w[1]) != "emitter") continue;
      if (where == Where::kGeometry) {
        result.emitters.emplace_back();
        e = &result.emitters.back();
        names.push_back(w[2]);
        e->name = w[2];
        e->birthrate = 0.0f;
        e->size_start = e->size_end = 0.0f;
      } else if (where == Where::kAnim) {
        for (size_t i = 0; i < names.size(); ++i) {
          if (Lower(names[i]) == Lower(w[2])) keyed = &result.emitters[i];
        }
      }
    } else if (first == "endnode") {
      e = nullptr;
      keyed = nullptr;
    } else if (where == Where::kAnim && keyed && w.size() >= 2 && first.size() > 3 &&
               first.compare(first.size() - 3, 3, "key") == 0) {
      key_name = first;
      key_rows = atoi(w[1].c_str());
      keys.clear();
    } else if (where == Where::kGeometry && e && w.size() >= 2) {
      const std::string& v = w[1];
      if (first == "update") e->update = v;
      else if (first == "render") e->render = v;
      else if (first == "blend") e->blend = v;
      else if (first == "texture") e->texture = v;
      else if (first == "position") ReadFloats(w, 1, e->position, 3);
      else if (first == "colorstart") ReadFloats(w, 1, e->color_start, 3);
      else if (first == "colorend") ReadFloats(w, 1, e->color_end, 3);
      else if (first == "alphastart") e->alpha_start = ToFloat(v);
      else if (first == "alphaend") e->alpha_end = ToFloat(v);
      else if (first == "sizestart") e->size_start = ToFloat(v);
      else if (first == "sizeend") e->size_end = ToFloat(v);
      else if (first == "birthrate") e->birthrate = ToFloat(v);
      else if (first == "lifeexp") e->life = ToFloat(v);
      else if (first == "mass") e->mass = ToFloat(v);
      else if (first == "spread") e->spread = ToFloat(v);
      else if (first == "velocity") e->velocity = ToFloat(v);
      else if (first == "randvel") e->randvel = ToFloat(v);
      else if (first == "xsize") e->xsize = ToFloat(v);
      else if (first == "ysize") e->ysize = ToFloat(v);
      else if (first == "xgrid") e->grid = std::max(1, atoi(v.c_str()));
      else if (first == "frameend") e->frame_end = std::max(0, atoi(v.c_str()));
      else if (first == "blurlength") e->blur_length = ToFloat(v);
      else if (first == "bounce_co") e->bounce_co = ToFloat(v);
      else if (first == "inherit") e->inherit = v != "0";
      else if (first == "splat") e->splat = v != "0";
      else if (first == "bounce") e->bounce = v != "0";
      else if (first == "random") e->random = v != "0";
    }
  }
  if (result.emitters.empty()) {
    if (error) *error = "no emitters in that model (an ASCII effect model of nwn5e's is needed)";
    return false;
  }
  result.duration = has_duration;
  *effect = result;
  return true;
}

namespace {

// Binary model reading, bounds-checked.
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
  // A NUL-terminated printable string of at most `max` bytes ("" if not one).
  std::string Str(size_t at, size_t max) const {
    std::string out;
    for (size_t i = 0; i < max && at + i < b.size(); ++i) {
      const char c = b[at + i];
      if (c == 0) return out;
      if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126) return "";
      out += c;
    }
    return "";
  }
};

constexpr size_t kModelBase = 12;  // offsets in a model are from the end of the 12-byte file header

struct Controller {
  uint32_t type;
  std::vector<float> times;
  std::vector<float> values;  // rows x columns
  int columns;
};

// A node's controllers; false when the arrays don't make sense there (not a node).
bool ReadControllers(const Bytes& m, size_t node, std::vector<Controller>* out) {
  const uint32_t keys = m.U32(node + 0x44), key_count = m.U32(node + 0x48);
  const uint32_t data = m.U32(node + 0x50), data_count = m.U32(node + 0x54);
  if (key_count == 0 || key_count > 96 || data_count > 8192) return false;
  if (kModelBase + keys + 12ull * key_count > m.b.size() || kModelBase + data + 4ull * data_count > m.b.size()) return false;
  for (uint32_t k = 0; k < key_count; ++k) {
    const size_t at = kModelBase + keys + 12ull * k;
    Controller c;
    c.type = m.U32(at);
    const int rows = m.U16(at + 4), time_at = m.U16(at + 6), value_at = m.U16(at + 8);
    c.columns = m.U8(at + 10) & 0x0F;
    if (c.type > 600 || rows <= 0 || rows > 512 || c.columns <= 0 || c.columns > 4) return false;
    if (time_at + rows > static_cast<int>(data_count) || value_at + rows * c.columns > static_cast<int>(data_count)) {
      return false;
    }
    for (int r = 0; r < rows; ++r) c.times.push_back(m.F32(kModelBase + data + 4ull * (time_at + r)));
    for (int v = 0; v < rows * c.columns; ++v) c.values.push_back(m.F32(kModelBase + data + 4ull * (value_at + v)));
    out->push_back(c);
  }
  return true;
}

void ApplyController(const Controller& c, VfxEmitter* e) {
  const auto v = [&](int i) { return i < static_cast<int>(c.values.size()) ? c.values[i] : 0.0f; };
  switch (c.type) {
    case 8: e->position[0] = v(0); e->position[1] = v(1); e->position[2] = v(2); break;
    case 80: e->alpha_end = v(0); break;
    case 84: e->alpha_start = v(0); break;
    case 88: e->birthrate = v(0); break;
    case 92: e->bounce_co = v(0); break;
    case 96: e->color_end[0] = v(0); e->color_end[1] = v(1); e->color_end[2] = v(2); break;
    case 108: e->color_start[0] = v(0); e->color_start[1] = v(1); e->color_start[2] = v(2); break;
    case 132: e->frame_end = static_cast<int>(v(0)); break;
    case 144: e->life = v(0); break;
    case 148: e->mass = v(0); break;
    case 164: e->randvel = v(0); break;
    case 168: e->size_start = v(0); break;
    case 172: e->size_end = v(0); break;
    case 184: e->spread = v(0); break;
    case 192: e->velocity = v(0); break;
    case 196: e->xsize = v(0); break;
    case 200: e->ysize = v(0); break;
    case 204: e->blur_length = v(0); break;
    default: break;
  }
}

}  // namespace

bool ReadVfxBinaryModel(const std::string& bytes, VfxEffect* effect, std::string* error) {
  const Bytes m{bytes};
  if (bytes.size() < 0x200 || m.U32(0) != 0) {
    if (error) *error = "not a compiled model";
    return false;
  }
  VfxEffect result;
  result.name = m.Str(kModelBase + 8, 64);
  std::vector<size_t> nodes;  // each emitter's geometry node, parallel to result.emitters
  for (const char* word : {"Fountain", "Single", "Explosion", "Lightning"}) {
    const std::string needle(word, strlen(word) + 1);
    for (size_t at = bytes.find(needle); at != std::string::npos; at = bytes.find(needle, at + 1)) {
      if (at < 0x18 + 0x60 + kModelBase) continue;
      const size_t emitter = at - 0x18, node = emitter - 0x60;
      const std::string name = m.Str(node + 0x10, 32);
      const std::string render = m.Str(emitter + 0x38, 32);
      if (name.empty() || render.empty()) continue;
      bool seen = false;
      for (const VfxEmitter& e : result.emitters) seen = seen || e.name == name;
      std::vector<Controller> controllers;
      if (seen || !ReadControllers(m, node, &controllers)) continue;
      VfxEmitter e;
      e.name = name;
      e.update = word;
      e.render = render;
      e.blend = m.Str(emitter + 0x58, 32);
      if (e.blend.empty()) e.blend = "Normal";
      e.texture = m.Str(emitter + 0x78, 64);
      e.grid = std::max<int>(1, static_cast<int>(m.U32(emitter + 0x0C)));
      const uint32_t flags = m.U32(emitter + 0xD4);
      e.bounce = (flags & 0x010) != 0;
      e.random = (flags & 0x020) != 0;
      e.inherit = (flags & 0x040) != 0;
      e.splat = (flags & 0x200) != 0;
      for (const Controller& c : controllers) ApplyController(c, &e);
      result.emitters.push_back(e);
      nodes.push_back(node);
    }
  }
  if (result.emitters.empty()) {
    if (error) *error = "no emitters found in that model";
    return false;
  }
  result.duration = bytes.find(std::string("duration", 9)) != std::string::npos;
  // The animations (their 64-byte name at the header's +8, the length at +0x70),
  // by where they start: an animation's copies of the nodes follow its header
  // (0.42.0, the game's vdr_eleshield: impact 0x100, duration 0xa6c, cessation
  // 0x1344, Ring01's copies at 0x55c / 0xec0 / 0x1798).
  std::vector<std::pair<size_t, std::string>> anims;
  for (const char* name : {"impact", "duration", "cessation"}) {
    const std::string needle = std::string(1, '\0') + name + std::string(1, '\0');
    for (size_t at = bytes.find(needle); at != std::string::npos; at = bytes.find(needle, at + 1)) {
      if (at + 1 < 8 + kModelBase) continue;
      const size_t header = at + 1 - 8;
      const float length = m.F32(header + 0x70);
      if (length > 0.0f && length < 60.0f) anims.emplace_back(header, name);
    }
  }
  const auto anim_of = [&](size_t node) {
    size_t best = 0;
    std::string name;
    for (const auto& a : anims) {
      if (a.first < node && a.first >= best) {
        best = a.first;
        name = a.second;
      }
    }
    return name;
  };
  // The animations' copies of each emitter node: a birthrate keyed off. Not the
  // cessation's (how a lasting effect ends - the game's own key every emitter off
  // there, which read as "on for 0.03 s") nor the looping duration's; the
  // impact's for a one-shot effect, or for a lasting one's emitter that is
  // otherwise off (birthrate 0: a burst at the start).
  for (size_t i = 0; i < result.emitters.size(); ++i) {
    VfxEmitter& e = result.emitters[i];
    const float geometry_rate = e.birthrate;
    const std::string needle = e.name + std::string(1, '\0');
    for (size_t at = bytes.find(needle); at != std::string::npos; at = bytes.find(needle, at + 1)) {
      if (at < 0x10 + kModelBase || at - 0x10 == nodes[i]) continue;
      const std::string anim = anim_of(at - 0x10);
      if (anim == "cessation") continue;
      std::vector<Controller> controllers;
      if (!ReadControllers(m, at - 0x10, &controllers)) continue;
      for (const Controller& c : controllers) {
        if (c.type != 88 || c.times.size() < 2) continue;
        std::vector<std::pair<float, float>> keys;
        for (size_t r = 0; r < c.times.size(); ++r) keys.emplace_back(c.times[r], c.values[r]);
        float top = 0.0f, off = 0.0f;
        ReadBirthrateKeys(keys, &top, &off);
        if (top > 0.0f) e.birthrate = std::max(e.birthrate, top);
        if (off > 0.0f && anim != "duration" && (!result.duration || geometry_rate == 0.0f)) e.emit_seconds = off;
      }
    }
  }
  // The impact animation's length (its header: the 64-byte name at +8, the
  // length at +0x70): how long an always-on emitter of a one-shot effect runs.
  const std::string impact("impact", 7);
  for (size_t at = bytes.find(impact); at != std::string::npos; at = bytes.find(impact, at + 1)) {
    if (at < 8 + kModelBase || bytes[at - 1] != 0) continue;
    const float length = m.F32(at - 8 + 0x70);
    if (length > 0.0f && length < 60.0f) {
      if (!result.duration) result.seconds = length;
      break;
    }
  }
  *effect = result;
  return true;
}

bool ReadModelNodePosition(const std::string& bytes, const std::string& node_name, float position[3]) {
  if (node_name.empty() || bytes.size() < 0x200 || bytes[0] != 0) return false;
  const Bytes m{bytes};
  std::string lower(bytes.size(), '\0');
  std::transform(bytes.begin(), bytes.end(), lower.begin(), [](char c) { return static_cast<char>(tolower(c)); });
  std::string needle = node_name + std::string(1, '\0');
  std::transform(needle.begin(), needle.end(), needle.begin(), [](char c) { return static_cast<char>(tolower(c)); });
  for (size_t at = lower.find(needle); at != std::string::npos; at = lower.find(needle, at + 1)) {
    if (at < 0x10 + kModelBase || (at > 0 && lower[at - 1] != 0)) continue;  // a whole name, where a node's would be
    std::vector<Controller> controllers;
    if (!ReadControllers(m, at - 0x10, &controllers)) continue;
    for (const Controller& c : controllers) {
      if (c.type != 8 || c.columns < 3 || c.values.size() < 3) continue;
      position[0] = c.values[0];
      position[1] = c.values[1];
      position[2] = c.values[2];
      return true;
    }
  }
  return false;
}

namespace {

// The value of `key` in `[section]` of an INI-like text ("" if none).
std::string IniValue(const std::string& text, const std::string& section, const std::string& key) {
  std::istringstream in(text);
  std::string line;
  bool inside = false;
  const auto lower = [](std::string s) {
    for (char& c : s) c = static_cast<char>(tolower(c));
    return s;
  };
  const std::string want_section = "[" + lower(section) + "]", want_key = lower(key);
  while (std::getline(in, line)) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
    const size_t first = line.find_first_not_of(" \t");
    if (first == std::string::npos) continue;
    line = line.substr(first);
    if (line[0] == '[') {
      inside = lower(line) == want_section;
      continue;
    }
    if (!inside) continue;
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string name = line.substr(0, eq);
    while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) name.pop_back();
    if (lower(name) != want_key) continue;
    std::string value = line.substr(eq + 1);
    const size_t v = value.find_first_not_of(" \t");
    return v == std::string::npos ? "" : value.substr(v);
  }
  return "";
}

}  // namespace

std::string TilesetTileModel(const std::string& set_text, int id) {
  return id < 0 ? "" : IniValue(set_text, "TILE" + std::to_string(id), "Model");
}

float TilesetTransition(const std::string& set_text) {
  const std::string value = IniValue(set_text, "GENERAL", "Transition");
  return value.empty() ? 0.0f : static_cast<float>(atof(value.c_str()));
}

int PatchEmitter(std::string* bytes, const std::string& emitter_name, const std::vector<std::pair<uint32_t, float>>& values,
                 uint32_t clear_flags) {
  if (!bytes || emitter_name.empty() || bytes->size() < 0x200 || (*bytes)[0] != 0) return 0;
  const Bytes m{*bytes};
  const std::string needle = emitter_name + std::string(1, '\0');
  int changed = 0;
  for (size_t at = bytes->find(needle); at != std::string::npos; at = bytes->find(needle, at + 1)) {
    if (at < 0x10 + kModelBase || (*bytes)[at - 1] != 0) continue;
    const size_t node = at - 0x10;
    std::vector<Controller> controllers;
    if (!ReadControllers(m, node, &controllers)) continue;
    const uint32_t keys = m.U32(node + 0x44), key_count = m.U32(node + 0x48), data = m.U32(node + 0x50);
    bool touched = false;
    for (uint32_t k = 0; k < key_count; ++k) {
      const size_t key = kModelBase + keys + 12ull * k;
      const uint32_t type = m.U32(key);
      const int rows = m.U16(key + 4), value_at = m.U16(key + 8), columns = m.U8(key + 10) & 0x0F;
      for (const auto& value : values) {
        if (value.first != type || columns != 1) continue;
        for (int r = 0; r < rows; ++r) {
          const size_t where = kModelBase + data + 4ull * (value_at + r);
          if (where + 4 <= bytes->size()) memcpy(&(*bytes)[where], &value.second, 4);
        }
        touched = true;
      }
    }
    // The geometry node's emitter header follows it (update name at +0x18): its flags.
    const size_t emitter = node + 0x60;
    const std::string update = m.Str(emitter + 0x18, 32);
    if (clear_flags && (update == "Fountain" || update == "Single" || update == "Explosion" || update == "Lightning")) {
      const uint32_t flags = m.U32(emitter + 0xD4) & ~clear_flags;
      memcpy(&(*bytes)[emitter + 0xD4], &flags, 4);
      touched = true;
    }
    if (touched) ++changed;
  }
  return changed;
}

std::vector<uint8_t> TileFillLightsOff(int width, int height, const std::vector<TileLight>& lights, bool strict) {
  if (width <= 0 || height <= 0 || width > 64 || height > 64) return {};
  const size_t tiles = static_cast<size_t>(width) * height;
  std::vector<int> near(tiles, 0), lit(tiles, 0);
  for (const TileLight& light : lights) {
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        const int x = light.x + dx, y = light.y + dy;
        if (x < 0 || y < 0 || x >= width || y >= height) continue;
        ++near[static_cast<size_t>(y) * width + x];
        if (light.lit) ++lit[static_cast<size_t>(y) * width + x];
      }
    }
  }
  std::vector<uint8_t> off(tiles, 0);
  for (size_t i = 0; i < tiles; ++i) off[i] = (strict || near[i] > 0) && lit[i] == 0 ? 1 : 0;
  return off;
}

void TilePointInArea(const float local[3], int tile_x, int tile_y, int orientation, int height, float transition,
                     float out[3]) {
  float x = local[0], y = local[1];
  switch (((orientation % 4) + 4) % 4) {
    case 1: { const float t = x; x = -y; y = t; break; }
    case 2: x = -x; y = -y; break;
    case 3: { const float t = x; x = y; y = -t; break; }
    default: break;
  }
  out[0] = (static_cast<float>(tile_x) + 0.5f) * 10.0f + x;
  out[1] = (static_cast<float>(tile_y) + 0.5f) * 10.0f + y;
  out[2] = static_cast<float>(height) * transition + local[2];
}

}  // namespace data
