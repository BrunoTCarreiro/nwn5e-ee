#include "data/vfx_model.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

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
      float top = 0.0f, last_on = 0.0f;
      for (const auto& k : keys) {
        if (k.second > 0.0f) last_on = k.first;
        top = std::max(top, k.second);
      }
      if (top > 0.0f) keyed->birthrate = std::max(keyed->birthrate, top);
      if (keys.back().second == 0.0f && last_on > 0.0f) keyed->emit_seconds = last_on;
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

}  // namespace data
