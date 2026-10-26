#include "data/vfx_model.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
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
      flag("random", e.random), flag("inherit", e.inherit), "  inheritvel 0", flag("inherit_local", e.inherit_local),
      flag("splat", e.splat), flag("inherit_part", e.inherit_part), "  renderorder 0", "  spawntype 0",
      "  update " + e.update,
      "  render " + e.render,
      "  blend " + e.blend,
      "  texture " + e.texture,
      "  xgrid " + std::to_string(e.grid), "  ygrid " + std::to_string(e.grid), "  loop 0",
      "  deadspace " + Num(std::min(std::max(e.deadspace, 0.0f), kVfxDeadspaceMax)), flag("twosidedtex", e.twosidedtex),
      "  blastRadius 0", "  blastLength 0",
      "  position " + Num(e.position[0]) + " " + Num(e.position[1]) + " " + Num(e.position[2]),
      "  orientation " + Num(e.orientation[0]) + " " + Num(e.orientation[1]) + " " + Num(e.orientation[2]) + " " +
          Num(e.orientation[3]),
      "  colorStart " + Num(e.color_start[0]) + " " + Num(e.color_start[1]) + " " + Num(e.color_start[2]),
      "  colorEnd " + Num(e.color_end[0]) + " " + Num(e.color_end[1]) + " " + Num(e.color_end[2]),
      "  alphaStart " + Num(e.alpha_start), "  alphaEnd " + Num(e.alpha_end),
      "  sizeStart " + Num(e.size_start), "  sizeEnd " + Num(e.size_end),
      "  sizeStart_y 0", "  sizeEnd_y 0",
      "  frameStart 0", "  frameEnd " + std::to_string(e.frame_end),
      "  birthrate " + Num(keyed_birthrate ? 0.0f : e.birthrate),
      "  lifeExp " + Num(e.life),
      "  mass " + Num(e.mass), "  spread " + Num(e.spread), "  particleRot " + Num(e.particle_rot),
      "  velocity " + Num(e.velocity), "  randvel " + Num(e.randvel), "  fps " + Num(e.fps),
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

const char* const kAnims[] = {"impact", "duration", "cessation"};

std::string Row(const std::vector<float>& values) {
  std::string out;
  for (size_t i = 0; i < values.size(); ++i) out += (i ? " " : "") + Num(values[i]);
  return out;
}

void ReadAll(const std::vector<std::string>& words, std::vector<float>* out) {
  out->clear();
  for (const std::string& w : words) out->push_back(ToFloat(w));
}

// A node's or an emitter's key lines in a draft: the animations that key it, in order.
std::string DraftKeys(const std::vector<VfxKeys>& keys) {
  std::string out;
  for (const char* anim : kAnims) {
    for (const VfxKeys& k : keys) {
      if (k.anim != anim) continue;
      if (!k.position.empty()) out += std::string(anim) + "_position = " + Row(k.position) + "\n";
      if (!k.orientation.empty()) out += std::string(anim) + "_orientation = " + Row(k.orientation) + "\n";
    }
  }
  return out;
}

// A draft line that is a key list ("<anim>_position" / "<anim>_orientation").
bool DraftKeyLine(const std::string& key, const std::vector<std::string>& words, std::vector<VfxKeys>* keys) {
  for (const char* anim : kAnims) {
    const std::string a(anim);
    if (key == a + "_position") {
      ReadAll(words, &FindKeys(keys, a, true)->position);
      return true;
    }
    if (key == a + "_orientation") {
      ReadAll(words, &FindKeys(keys, a, true)->orientation);
      return true;
    }
  }
  return false;
}

// The last key's time in one animation, over the effect's nodes and emitters.
float LastKeyTime(const VfxEffect& effect, const std::string& anim) {
  float last = 0.0f;
  const auto scan = [&](const std::vector<VfxKeys>& keys) {
    for (const VfxKeys& k : keys) {
      if (k.anim != anim) continue;
      for (size_t i = 0; i + 3 < k.position.size(); i += 4) last = std::max(last, k.position[i]);
      for (size_t i = 0; i + 4 < k.orientation.size(); i += 5) last = std::max(last, k.orientation[i]);
    }
  };
  for (const VfxNode& n : effect.nodes) scan(n.keys);
  for (const VfxEmitter& e : effect.emitters) scan(e.keys);
  return last;
}

}  // namespace

VfxKeys* FindKeys(std::vector<VfxKeys>* list, const std::string& anim, bool create) {
  for (VfxKeys& k : *list) {
    if (k.anim == anim) return &k;
  }
  if (!create) return nullptr;
  list->emplace_back();
  list->back().anim = anim;
  return &list->back();
}

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
  // The model's own names for the effect's nodes and emitters (unique to it), and its parents
  // before its children: the order a model is written in (a parent never written - a loop,
  // a missing one - leaves its child under the root).
  const size_t node_count = effect.nodes.size(), total = node_count + effect.emitters.size();
  std::vector<std::string> names(total);
  std::map<std::string, std::string> by_name;
  for (size_t k = 0; k < total; ++k) {
    if (k < node_count) {
      std::string clean = VfxResRef(effect.nodes[k].name, 20);
      names[k] = model + "_n" + std::to_string(k) + "_" + (clean.empty() ? std::string("n") : clean);
      by_name.emplace(effect.nodes[k].name, names[k]);
    } else {
      names[k] = NodeName(model, effect.emitters[k - node_count], k - node_count);
      by_name.emplace(effect.emitters[k - node_count].name, names[k]);
    }
  }
  const auto parent_of = [&](size_t k) {
    const std::string& parent = k < node_count ? effect.nodes[k].parent : effect.emitters[k - node_count].parent;
    const auto it = parent.empty() ? by_name.end() : by_name.find(parent);
    return it == by_name.end() ? model : it->second;
  };
  std::vector<size_t> order;
  std::vector<bool> placed(total, false);
  std::set<std::string> written = {model};
  for (bool progress = true; progress && order.size() < total;) {
    progress = false;
    for (size_t k = 0; k < total; ++k) {
      if (placed[k] || !written.count(parent_of(k))) continue;
      placed[k] = progress = true;
      order.push_back(k);
      written.insert(names[k]);
    }
  }
  std::set<size_t> orphans;
  for (size_t k = 0; k < total; ++k) {
    if (!placed[k]) {
      order.push_back(k);
      orphans.insert(k);
    }
  }
  const auto parent_line = [&](size_t k) { return orphans.count(k) ? model : parent_of(k); };
  const auto turn = [](const float o[4]) {
    return Num(o[0]) + " " + Num(o[1]) + " " + Num(o[2]) + " " + Num(o[3]);
  };

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
  for (size_t k : order) {
    if (k < node_count) {
      const VfxNode& n = effect.nodes[k];
      lines.push_back("node dummy " + names[k]);
      lines.push_back("  parent " + parent_line(k));
      lines.push_back("  position " + Num(n.position[0]) + " " + Num(n.position[1]) + " " + Num(n.position[2]));
      lines.push_back("  orientation " + turn(n.orientation));
      lines.push_back("endnode");
      continue;
    }
    const VfxEmitter& e = effect.emitters[k - node_count];
    // An emitter that switches off is keyed in the impact; F's always-on ones are too.
    const bool keyed = e.emit_seconds > 0.0f || !effect.duration;
    for (const std::string& line : EmitterLines(e, names[k], parent_line(k), keyed)) lines.push_back(line);
  }
  lines.push_back("endmodelgeom " + model);
  // The impact: long enough for every emitter's emission (and, F, what it emitted to fade),
  // for the effect's own length and for its keys; the duration and cessation as long as the
  // effect has them (2 s / 1 s if not given) and their keys.
  float impact = 0.2f;
  for (const VfxEmitter& e : effect.emitters) {
    const float on = e.emit_seconds > 0.0f ? e.emit_seconds : (effect.duration ? 0.0f : effect.seconds);
    impact = std::max(impact, on + 0.01f + (effect.duration ? 0.0f : e.life));
  }
  impact = std::max({impact, effect.lengths[0], LastKeyTime(effect, "impact")});
  const float duration = std::max(effect.lengths[1] > 0.0f ? effect.lengths[1] : 2.0f, LastKeyTime(effect, "duration"));
  const float cessation =
      std::max(effect.lengths[2] > 0.0f ? effect.lengths[2] : 1.0f, LastKeyTime(effect, "cessation"));
  const auto key_lines = [&](const std::vector<VfxKeys>& keys, const char* anim) {
    for (const VfxKeys& k : keys) {
      if (k.anim != anim) continue;
      if (k.position.size() >= 4) {
        lines.push_back("  positionkey " + std::to_string(k.position.size() / 4));
        for (size_t i = 0; i + 3 < k.position.size(); i += 4) {
          lines.push_back("    " + Num(k.position[i]) + " " + Num(k.position[i + 1]) + " " + Num(k.position[i + 2]) + " " +
                          Num(k.position[i + 3]));
        }
      }
      if (k.orientation.size() >= 5) {
        lines.push_back("  orientationkey " + std::to_string(k.orientation.size() / 5));
        for (size_t i = 0; i + 4 < k.orientation.size(); i += 5) {
          lines.push_back("    " + Num(k.orientation[i]) + " " + turn(&k.orientation[i + 1]));
        }
      }
    }
  };
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
    for (size_t k : order) {
      if (k < node_count) {
        lines.push_back("node dummy " + names[k]);
        lines.push_back("  parent " + parent_line(k));
        key_lines(effect.nodes[k].keys, name);
        lines.push_back("endnode");
        continue;
      }
      const VfxEmitter& e = effect.emitters[k - node_count];
      lines.push_back("node emitter " + names[k]);
      lines.push_back("  parent " + parent_line(k));
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
      key_lines(e.keys, name);
      lines.push_back("endnode");
    }
    lines.push_back(std::string("doneanim ") + name + " " + model);
  };
  anim("impact", impact, true, false);
  if (effect.duration) {
    anim("duration", duration, false, false);
    anim("cessation", cessation, false, true);
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
  out += "lengths = " + Num(effect.lengths[0]) + " " + Num(effect.lengths[1]) + " " + Num(effect.lengths[2]) + "\n";
  for (const VfxNode& n : effect.nodes) {
    out += "\n[node " + n.name + "]\n";
    if (!n.parent.empty()) out += "parent = " + n.parent + "\n";
    out += "position = " + Num(n.position[0]) + " " + Num(n.position[1]) + " " + Num(n.position[2]) + "\n";
    out += "orientation = " + Num(n.orientation[0]) + " " + Num(n.orientation[1]) + " " + Num(n.orientation[2]) + " " +
           Num(n.orientation[3]) + "\n";
    out += DraftKeys(n.keys);
  }
  for (const VfxEmitter& e : effect.emitters) {
    out += "\n[emitter " + e.name + "]\n";
    out += "update = " + e.update + "\nrender = " + e.render + "\nblend = " + e.blend + "\ntexture = " + e.texture + "\n";
    out += "position = " + Num(e.position[0]) + " " + Num(e.position[1]) + " " + Num(e.position[2]) + "\n";
    out += "orientation = " + Num(e.orientation[0]) + " " + Num(e.orientation[1]) + " " + Num(e.orientation[2]) + " " +
           Num(e.orientation[3]) + "\n";
    if (!e.parent.empty()) out += "parent = " + e.parent + "\n";
    out += "birthrate = " + Num(e.birthrate) + "\nemit_seconds = " + Num(e.emit_seconds) + "\nlife = " + Num(e.life) + "\n";
    out += "velocity = " + Num(e.velocity) + "\nrandvel = " + Num(e.randvel) + "\nspread = " + Num(e.spread) + "\n";
    out += "mass = " + Num(e.mass) + "\nsize = " + Num(e.size_start) + " " + Num(e.size_end) + "\n";
    out += "color_start = " + Num(e.color_start[0]) + " " + Num(e.color_start[1]) + " " + Num(e.color_start[2]) + "\n";
    out += "color_end = " + Num(e.color_end[0]) + " " + Num(e.color_end[1]) + " " + Num(e.color_end[2]) + "\n";
    out += "alpha = " + Num(e.alpha_start) + " " + Num(e.alpha_end) + "\n";
    out += "area = " + Num(e.xsize) + " " + Num(e.ysize) + "\n";
    out += "grid = " + std::to_string(e.grid) + "\nframe_end = " + std::to_string(e.frame_end) + "\n";
    out += "fps = " + Num(e.fps) + "\nparticle_rot = " + Num(e.particle_rot) + "\n";
    out += "blur_length = " + Num(e.blur_length) + "\nbounce_co = " + Num(e.bounce_co) + "\n";
    out += "deadspace = " + Num(e.deadspace) + "\n";
    std::string flags;
    if (e.inherit) flags += " inherit";
    if (e.splat) flags += " splat";
    if (e.bounce) flags += " bounce";
    if (e.random) flags += " random";
    if (e.inherit_local) flags += " inherit_local";
    if (e.inherit_part) flags += " inherit_part";
    if (e.twosidedtex) flags += " twosidedtex";
    out += "flags =" + flags + "\n";
    out += DraftKeys(e.keys);
  }
  return out;
}

bool ReadVfxDraft(const std::string& text, VfxEffect* effect, std::string* error) {
  VfxEffect result;
  int emitter = -1, node = -1;  // the section being read (indices: the lists grow)
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
      if (Lower(inner).rfind("emitter", 0) == 0) {
        result.emitters.emplace_back();
        emitter = static_cast<int>(result.emitters.size()) - 1;
        node = -1;
        result.emitters.back().name = Trim(inner.substr(7));
        if (result.emitters.back().name.empty()) result.emitters.back().name = "emitter";
      } else if (Lower(inner).rfind("node", 0) == 0) {
        result.nodes.emplace_back();
        node = static_cast<int>(result.nodes.size()) - 1;
        emitter = -1;
        result.nodes.back().name = Trim(inner.substr(4));
        if (result.nodes.back().name.empty()) result.nodes.back().name = "node";
      } else {
        if (error) *error = "line " + std::to_string(line_number) + ": unknown section [" + inner + "]";
        return false;
      }
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
    if (node >= 0) {
      VfxNode& n = result.nodes[node];
      if (key == "parent") n.parent = value;
      else if (key == "position") ReadFloats(words, 0, n.position, 3);
      else if (key == "orientation") ReadFloats(words, 0, n.orientation, 4);
      else DraftKeyLine(key, words, &n.keys);
      continue;
    }
    if (emitter < 0) {
      if (key == "name") result.name = value;
      else if (key == "type") result.duration = Lower(value) != "f";
      else if (key == "seconds") result.seconds = ToFloat(value);
      else if (key == "attach") result.attach = Lower(value);
      else if (key == "lengths") ReadFloats(words, 0, result.lengths, 3);
      continue;
    }
    VfxEmitter* e = &result.emitters[emitter];
    if (key == "update") e->update = value;
    else if (key == "render") e->render = value;
    else if (key == "blend") e->blend = value;
    else if (key == "texture") e->texture = value;
    else if (key == "parent") e->parent = value;
    else if (key == "position") ReadFloats(words, 0, e->position, 3);
    else if (key == "orientation") ReadFloats(words, 0, e->orientation, 4);
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
    else if (key == "fps") e->fps = ToFloat(value);
    else if (key == "particle_rot") e->particle_rot = ToFloat(value);
    else if (key == "blur_length") e->blur_length = ToFloat(value);
    else if (key == "bounce_co") e->bounce_co = ToFloat(value);
    else if (key == "deadspace") e->deadspace = std::min(std::max(ToFloat(value), 0.0f), kVfxDeadspaceMax);
    else if (key == "flags") {
      for (const std::string& word : words) {
        const std::string f = Lower(word);
        e->inherit = e->inherit || f == "inherit";
        e->splat = e->splat || f == "splat";
        e->bounce = e->bounce || f == "bounce";
        e->random = e->random || f == "random";
        e->inherit_local = e->inherit_local || f == "inherit_local";
        e->inherit_part = e->inherit_part || f == "inherit_part";
        e->twosidedtex = e->twosidedtex || f == "twosidedtex";
      }
    } else DraftKeyLine(key, words, &e->keys);
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
  // The geometry's nodes (any kind: the emitters hang under dummies, meshes, lights...).
  struct Geo {
    std::string name, parent;
    float position[3] = {0, 0, 0};
    float orientation[4] = {0, 0, 0, 0};
    bool emitter = false;
    std::vector<VfxKeys> keys;
  };
  std::vector<Geo> geo;
  std::map<std::string, size_t> geo_at;  // lower-case name -> index
  int current = -1;                      // the geometry node being read / keyed
  int emitter = -1;                      // its emitter, if it is one
  std::string key_name;
  int key_rows = 0;                      // rows still to come; -1: until "endlist"
  std::vector<std::vector<float>> rows;
  bool has_duration = false;
  const auto finish_keys = [&]() {
    if (current >= 0 && !rows.empty()) {
      VfxEmitter* keyed = emitter >= 0 ? &result.emitters[emitter] : nullptr;
      if (key_name == "positionkey" || key_name == "orientationkey") {
        const size_t width = key_name == "positionkey" ? 4 : 5;
        VfxKeys* k = FindKeys(&geo[current].keys, anim, true);
        std::vector<float>& out = key_name == "positionkey" ? k->position : k->orientation;
        for (const auto& row : rows) {
          if (row.size() < width) continue;
          out.insert(out.end(), row.begin(), row.begin() + width);
        }
      } else if (keyed) {
        std::vector<std::pair<float, float>> keys;
        for (const auto& row : rows) {
          if (row.size() >= 2) keys.emplace_back(row[0], row[1]);
        }
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
      }
    }
    rows.clear();
    key_name.clear();
    key_rows = 0;
  };
  const auto numeric = [](const std::string& word) {
    return !word.empty() && (isdigit(static_cast<unsigned char>(word[0])) || word[0] == '-' || word[0] == '.');
  };
  while (std::getline(in, raw)) {
    const std::vector<std::string> w = Words(raw.substr(0, raw.find('#')));
    if (w.empty()) continue;
    const std::string first = Lower(w[0]);
    if (key_rows != 0) {
      if (first == "endlist") {
        finish_keys();
        continue;
      }
      if (numeric(w[0])) {
        std::vector<float> row;
        for (const std::string& word : w) row.push_back(ToFloat(word));
        rows.push_back(row);
        if (key_rows > 0 && --key_rows == 0) finish_keys();
        continue;
      }
      finish_keys();  // a list that ends without its rows or an endlist
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
      current = emitter = -1;
    } else if (where == Where::kAnim && first == "length" && w.size() >= 2) {
      const int slot = anim == "impact" ? 0 : anim == "duration" ? 1 : anim == "cessation" ? 2 : -1;
      if (slot >= 0) result.lengths[slot] = ToFloat(w[1]);
    } else if (first == "node" && w.size() >= 3) {
      current = emitter = -1;
      const auto found = geo_at.find(Lower(w[2]));
      if (where == Where::kGeometry) {
        geo.emplace_back();
        geo.back().name = w[2];
        current = static_cast<int>(geo.size()) - 1;
        geo_at.emplace(Lower(w[2]), geo.size() - 1);
        if (Lower(w[1]) == "emitter") {
          geo.back().emitter = true;
          result.emitters.emplace_back();
          emitter = static_cast<int>(result.emitters.size()) - 1;
          VfxEmitter& e = result.emitters.back();
          e.name = w[2];
          e.birthrate = 0.0f;
          e.size_start = e.size_end = 0.0f;
        }
      } else if (where == Where::kAnim && found != geo_at.end()) {
        current = static_cast<int>(found->second);
        if (geo[current].emitter) {
          for (size_t i = 0; i < result.emitters.size(); ++i) {
            if (Lower(result.emitters[i].name) == Lower(w[2])) emitter = static_cast<int>(i);
          }
        }
      }
    } else if (first == "endnode") {
      current = emitter = -1;
    } else if (where == Where::kAnim && current >= 0 && first.size() > 3 &&
               first.compare(first.size() - 3, 3, "key") == 0) {
      key_name = first;
      rows.clear();
      key_rows = w.size() >= 2 && numeric(w[1]) ? atoi(w[1].c_str()) : -1;
      if (key_rows == 0) key_name.clear();
    } else if (where == Where::kGeometry && current >= 0 && w.size() >= 2) {
      Geo& g = geo[current];
      if (first == "parent") g.parent = w[1];
      else if (first == "position") ReadFloats(w, 1, g.position, 3);
      else if (first == "orientation") ReadFloats(w, 1, g.orientation, 4);
      if (emitter < 0) continue;
      VfxEmitter* e = &result.emitters[emitter];
      const std::string& v = w[1];
      if (first == "update") e->update = v;
      else if (first == "render") e->render = v;
      else if (first == "blend") e->blend = v;
      else if (first == "texture") e->texture = v;
      else if (first == "position") ReadFloats(w, 1, e->position, 3);
      else if (first == "orientation") ReadFloats(w, 1, e->orientation, 4);
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
      else if (first == "fps") e->fps = ToFloat(v);
      else if (first == "particlerot") e->particle_rot = ToFloat(v);
      else if (first == "blurlength") e->blur_length = ToFloat(v);
      else if (first == "bounce_co") e->bounce_co = ToFloat(v);
      else if (first == "inherit") e->inherit = v != "0";
      else if (first == "splat") e->splat = v != "0";
      else if (first == "bounce") e->bounce = v != "0";
      else if (first == "random") e->random = v != "0";
      else if (first == "inherit_local") e->inherit_local = v != "0";
      else if (first == "inherit_part") e->inherit_part = v != "0";
      else if (first == "twosidedtex") e->twosidedtex = v != "0";
      else if (first == "deadspace") e->deadspace = ToFloat(v);  // radians, as the game reads it (%f)
    }
  }
  if (key_rows != 0) finish_keys();
  if (result.emitters.empty()) {
    if (error) *error = "no emitters in that model (an ASCII effect model of nwn5e's is needed)";
    return false;
  }
  // The root (the node whose parent is NULL - the effect's own) is the model; the nodes kept
  // are the emitters' ancestors under it, parents first, with their keys (0.42.3).
  const auto is_root = [&](const std::string& name) {
    const auto it = geo_at.find(Lower(name));
    return name.empty() || Lower(name) == "null" || (it != geo_at.end() && Lower(geo[it->second].parent) == "null");
  };
  std::set<std::string> keep;
  for (const Geo& g : geo) {
    if (!g.emitter) continue;
    std::string up = g.parent;
    for (int depth = 0; depth < 64 && !is_root(up) && geo_at.count(Lower(up)); ++depth) {
      const Geo& parent = geo[geo_at[Lower(up)]];
      if (!parent.emitter) keep.insert(Lower(parent.name));
      up = parent.parent;
    }
  }
  for (const Geo& g : geo) {
    if (g.emitter) {
      for (VfxEmitter& e : result.emitters) {
        if (Lower(e.name) != Lower(g.name)) continue;
        e.parent = is_root(g.parent) ? std::string() : g.parent;
        e.keys = g.keys;
      }
      continue;
    }
    if (!keep.count(Lower(g.name))) continue;
    VfxNode n;
    n.name = g.name;
    n.parent = is_root(g.parent) ? std::string() : g.parent;
    memcpy(n.position, g.position, sizeof(n.position));
    memcpy(n.orientation, g.orientation, sizeof(n.orientation));
    n.keys = g.keys;
    result.nodes.push_back(n);
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

// A node tree from its root (a header's offset from the model base): its nodes
// in this reader's terms (the header + 0x10: the name at +0x10, the children - an
// array of header offsets - at +0x38 with their count at +0x3C). Empty when there
// is no root.
std::vector<size_t> TreeNodes(const Bytes& m, uint32_t root) {
  std::vector<size_t> nodes;
  if (root == 0) return nodes;
  std::vector<size_t> stack = {kModelBase + root + 0x10};
  std::set<size_t> seen;
  while (!stack.empty() && nodes.size() < 4096) {
    const size_t node = stack.back();
    stack.pop_back();
    if (node + 0x60 > m.b.size() || !seen.insert(node).second) continue;
    nodes.push_back(node);
    const uint32_t children = m.U32(node + 0x38), count = m.U32(node + 0x3C);
    if (count > 1024 || kModelBase + children + 4ull * count > m.b.size()) continue;
    for (uint32_t i = 0; i < count; ++i) stack.push_back(kModelBase + m.U32(kModelBase + children + 4ull * i) + 0x10);
  }
  return nodes;
}

// Quaternions (x, y, z, w): a product and a vector turned by one.
void QMul(const float a[4], const float b[4], float out[4]) {
  const float r[4] = {a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
                      a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
                      a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
                      a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
  memcpy(out, r, sizeof(r));
}

void QRotate(const float q[4], const float v[3], float out[3]) {
  const float p[4] = {v[0], v[1], v[2], 0.0f};
  const float conj[4] = {-q[0], -q[1], -q[2], q[3]};
  float t[4], r[4];
  QMul(q, p, t);
  QMul(t, conj, r);
  memcpy(out, r, 3 * sizeof(float));
}

// A geometry node: its name, its parent (0: none - the root), where it sits under the parent
// (controller 8) and how it's turned (20, x y z w). Parents before children (0.42.3; 0.42.2
// composed them into one place - now the effect keeps the nodes, the game's motion is theirs).
struct GeoNode {
  size_t node = 0;
  size_t parent = 0;
  std::string name;
  float pos[3] = {0, 0, 0};
  float rot[4] = {0, 0, 0, 1};
};

std::vector<GeoNode> GeometryNodes(const Bytes& m, uint32_t root) {
  std::vector<GeoNode> out;
  if (root == 0) return out;
  std::set<size_t> seen;
  std::vector<std::pair<size_t, size_t>> stack = {{kModelBase + root + 0x10, 0}};  // node, parent
  while (!stack.empty() && out.size() < 4096) {
    const auto item = stack.back();
    stack.pop_back();
    if (item.first + 0x60 > m.b.size() || !seen.insert(item.first).second) continue;
    GeoNode g;
    g.node = item.first;
    g.parent = item.second;
    g.name = m.Str(item.first + 0x10, 32);
    std::vector<Controller> controllers;
    if (ReadControllers(m, item.first, &controllers)) {
      for (const Controller& c : controllers) {
        if (c.type == 8 && c.columns == 3 && c.values.size() >= 3) memcpy(g.pos, c.values.data(), sizeof(g.pos));
        if (c.type == 20 && c.columns == 4 && c.values.size() >= 4) memcpy(g.rot, c.values.data(), sizeof(g.rot));
      }
    }
    out.push_back(g);
    const uint32_t children = m.U32(item.first + 0x38), count = m.U32(item.first + 0x3C);
    if (count > 1024 || kModelBase + children + 4ull * count > m.b.size()) continue;
    for (uint32_t i = count; i-- > 0;) {
      stack.emplace_back(kModelBase + m.U32(kModelBase + children + 4ull * i) + 0x10, item.first);
    }
  }
  return out;
}

// A quaternion (x y z w) as axis and angle (angle 2 acos w in 0..2 pi: back to exactly the
// same quaternion, its sign too - the game slerps keys the short way round from there).
void AxisAngle(const float q[4], float out[4]) {
  const float w = std::max(-1.0f, std::min(1.0f, q[3]));
  const float s = std::sqrt(std::max(0.0f, 1.0f - w * w));
  out[3] = 2.0f * std::acos(w);
  for (int k = 0; k < 3; ++k) out[k] = s > 1e-6f ? q[k] / s : 0.0f;
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
    case 128: e->fps = v(0); break;  // PartEmitter +0x9c (controller = field offset - 28)
    case 132: e->frame_end = static_cast<int>(v(0)); break;
    case 160: e->particle_rot = v(0); break;  // +0xbc
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
  // The geometry's nodes (its header's root at +0x48): the emitters are read
  // there, not from an animation's copy of the node - which carries the same
  // emitter header but only the controllers that animation keys (0.42.0: the
  // first copy in the file was taken - vcm_hitsonc's cloud came out 1 m up with
  // default values, vdr_darkness's emitter all defaults).
  const std::vector<GeoNode> geometry = GeometryNodes(m, m.U32(kModelBase + 0x48));
  std::map<size_t, size_t> geo_at;  // node -> its index in `geometry`
  for (size_t i = 0; i < geometry.size(); ++i) geo_at[geometry[i].node] = i;
  const size_t root_node = geometry.empty() ? 0 : geometry[0].node;
  const auto parent_name = [&](const GeoNode& g) {
    return g.parent && g.parent != root_node && geo_at.count(g.parent) ? geometry[geo_at[g.parent]].name : std::string();
  };
  std::vector<size_t> nodes;  // each emitter's geometry node, parallel to result.emitters
  for (int pass = 0; pass < 2 && result.emitters.empty(); ++pass) {  // 2nd: any node (no tree read)
  for (const char* word : {"Fountain", "Single", "Explosion", "Lightning"}) {
    const std::string needle(word, strlen(word) + 1);
    for (size_t at = bytes.find(needle); at != std::string::npos; at = bytes.find(needle, at + 1)) {
      if (at < 0x18 + 0x60 + kModelBase) continue;
      const size_t emitter = at - 0x18, node = emitter - 0x60;
      if (pass == 0 && !geo_at.count(node)) continue;
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
      e.inherit_local = (flags & 0x100) != 0;
      e.inherit_part = (flags & 0x400) != 0;
      e.deadspace = m.F32(emitter);                   // node +0xa0 in the game
      e.twosidedtex = m.U32(emitter + 0xC8) != 0;    // node +0x168
      if (pass == 0) {  // under its parent, as the model has it
        const GeoNode& g = geometry[geo_at[node]];
        memcpy(e.position, g.pos, sizeof(e.position));
        float turn[4];
        AxisAngle(g.rot, turn);
        if (turn[3] > 1e-4f && (turn[0] || turn[1] || turn[2])) memcpy(e.orientation, turn, sizeof(turn));
        e.parent = parent_name(g);
      }
      result.emitters.push_back(e);
      nodes.push_back(node);
    }
  }
  }
  if (result.emitters.empty()) {
    if (error) *error = "no emitters found in that model";
    return false;
  }
  result.duration = bytes.find(std::string("duration", 9)) != std::string::npos;
  // The nodes the emitters hang under (their ancestors up to the root, which is the effect's
  // own node), parents first - the game moves and turns them (0.42.3).
  std::set<size_t> emitter_nodes(nodes.begin(), nodes.end()), ancestors;
  for (size_t node : nodes) {
    if (!geo_at.count(node)) continue;
    for (size_t up = geometry[geo_at[node]].parent; up && up != root_node && geo_at.count(up) && ancestors.insert(up).second;
         up = geometry[geo_at[up]].parent) {
    }
  }
  std::map<std::string, size_t> geo_by_name;  // lower-case name -> index in `geometry`
  for (size_t i = 0; i < geometry.size(); ++i) geo_by_name.emplace(Lower(geometry[i].name), i);
  for (const GeoNode& g : geometry) {
    if (!ancestors.count(g.node) || emitter_nodes.count(g.node)) continue;
    VfxNode n;
    n.name = g.name;
    n.parent = parent_name(g);
    memcpy(n.position, g.pos, sizeof(n.position));
    float turn[4];
    AxisAngle(g.rot, turn);
    if (turn[3] > 1e-4f && (turn[0] || turn[1] || turn[2])) memcpy(n.orientation, turn, sizeof(turn));
    result.nodes.push_back(n);
  }
  // The animations (their 64-byte name at the header's +8, the root node at
  // +0x48 as the geometry's, the length at +0x70) and the nodes of each.
  std::vector<std::pair<std::string, std::vector<size_t>>> anims;
  for (const char* name : {"impact", "duration", "cessation"}) {
    const std::string needle = std::string(1, '\0') + name + std::string(1, '\0');
    for (size_t at = bytes.find(needle); at != std::string::npos; at = bytes.find(needle, at + 1)) {
      if (at + 1 < 8 + kModelBase) continue;
      const size_t header = at + 1 - 8;
      const float length = m.F32(header + 0x70);
      if (length > 0.0f && length < 60.0f) {
        anims.emplace_back(name, TreeNodes(m, m.U32(header + 0x48)));
        result.lengths[std::string(name) == "impact" ? 0 : std::string(name) == "duration" ? 1 : 2] = length;
        break;
      }
    }
  }
  // Each animation's keys of those nodes and of the emitters: where they move (8: t x y z)
  // and how they turn (20: t + x y z w, kept as axis-angle). A single key equal to the
  // geometry's is no motion and isn't kept.
  for (const auto& a : anims) {
    for (const size_t copy : a.second) {
      const std::string name = Lower(m.Str(copy + 0x10, 32));
      const auto g = geo_by_name.find(name);
      if (g == geo_by_name.end()) continue;
      std::vector<VfxKeys>* keys = nullptr;
      for (VfxNode& n : result.nodes) {
        if (Lower(n.name) == name) keys = &n.keys;
      }
      for (VfxEmitter& e : result.emitters) {
        if (Lower(e.name) == name) keys = &e.keys;
      }
      std::vector<Controller> controllers;
      if (!keys || !ReadControllers(m, copy, &controllers)) continue;
      const GeoNode& base = geometry[g->second];
      for (const Controller& c : controllers) {
        const size_t rows = c.times.size();
        if (c.type == 8 && c.columns == 3 && c.values.size() >= rows * 3) {
          bool moves = rows >= 2;
          for (int k = 0; k < 3 && !moves; ++k) moves = std::fabs(c.values[k] - base.pos[k]) > 1e-4f;
          if (!moves) continue;
          std::vector<float>& out = FindKeys(keys, a.first, true)->position;
          for (size_t r = 0; r < rows; ++r) {
            out.push_back(c.times[r]);
            for (int k = 0; k < 3; ++k) out.push_back(c.values[r * 3 + k]);
          }
        } else if (c.type == 20 && c.columns == 4 && c.values.size() >= rows * 4) {
          bool turns = rows >= 2;
          for (int k = 0; k < 4 && !turns; ++k) turns = std::fabs(c.values[k] - base.rot[k]) > 1e-4f;
          if (!turns) continue;
          std::vector<float>& out = FindKeys(keys, a.first, true)->orientation;
          for (size_t r = 0; r < rows; ++r) {
            float turn[4];
            AxisAngle(&c.values[r * 4], turn);
            out.push_back(c.times[r]);
            for (int k = 0; k < 4; ++k) out.push_back(turn[k]);
          }
        }
      }
    }
  }
  // The animations' copies of each emitter node: a birthrate keyed off. Not the
  // cessation's (how a lasting effect ends - the game's own key every emitter off
  // there, which read as "on for 0.03 s") nor the looping duration's; the
  // impact's for a one-shot effect, or for a lasting one's emitter that is
  // otherwise off (birthrate 0: a burst at the start).
  for (size_t i = 0; i < result.emitters.size(); ++i) {
    VfxEmitter& e = result.emitters[i];
    const float geometry_rate = e.birthrate;
    for (const auto& a : anims) {
    const std::string& anim = a.first;
    if (anim == "cessation") continue;
    for (const size_t copy : a.second) {
      if (copy == nodes[i] || Lower(m.Str(copy + 0x10, 32)) != Lower(e.name)) continue;
      std::vector<Controller> controllers;
      if (!ReadControllers(m, copy, &controllers)) continue;
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
