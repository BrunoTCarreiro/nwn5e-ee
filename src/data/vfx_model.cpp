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

// An emitter's blast radius (metres) or length (seconds) as every reader takes it - the draft, the ASCII model and the
// compiled one (the bench's core.js keeps the same rule): below 0, not a number, not finite or past 1000 is 0. The game's
// own reach 20 m and 4 s; an animation's copy of a compiled node can hold other bytes there.
float BlastValue(float v) { return std::isfinite(v) && v > 0.0f && v <= 1000.0f ? v : 0.0f; }

// KF: an animation's transtime as every reader keeps it - the draft, the ASCII model and the compiled one (the bench's
// core.js keeps the same rule): 0 to kVfxTranstimeMax (seconds), else not known (-1) - not a number, not finite, below 0
// or past it; -0 is 0.
float TranstimeValue(float v) { return std::isfinite(v) && v >= 0.0f && v <= kVfxTranstimeMax ? v + 0.0f : -1.0f; }

// An animation's slot in VfxEffect's lengths and transtimes (-1: not one of the three).
int AnimSlot(const std::string& anim) { return anim == "impact" ? 0 : anim == "duration" ? 1 : anim == "cessation" ? 2 : -1; }

// A plane's transparencyhint as every reader and both writers keep it (the bench's core.js hintValue and
// tools/data/floatdisk/floatdisk.js's hintValue keep the same rule - the Composer's check.js compares the two): 0 to
// kVfxTransparencyHintMax, the game's own models' range - below 0 is 0, above it the most.
int HintValue(long v) { return static_cast<int>(std::min<long>(std::max<long>(v, 0), kVfxTransparencyHintMax)); }

// From a draft's or an ASCII model's text, as the game's parser reads an integer: its leading digits, base 10 ("1.7" is
// 1, "+2" 2), after only what isspace skips (a no-break space stops it); not a number 0.
int ReadHint(const std::string& text) { return HintValue(strtol(text.c_str(), nullptr, 10)); }

void ReadFloats(const std::vector<std::string>& words, size_t from, float* out, int count) {
  for (int i = 0; i < count && from + i < words.size(); ++i) out[i] = ToFloat(words[from + i]);
}

// Three numbers as a draft or a model writes them ("r g b"): a plane's material is compared with its default this way,
// so a value that reads back the same writes the same (core.js compares the same text).
std::string Num3(const float v[3]) { return Num(v[0]) + " " + Num(v[1]) + " " + Num(v[2]); }

std::vector<std::string> EmitterLines(const VfxEmitter& e, const std::string& node, const std::string& parent,
                                      bool keyed_birthrate, bool has_target) {
  const auto flag = [](const char* name, bool on) { return std::string("  ") + name + (on ? " 1" : " 0"); };
  // Point to point only with a target (the game calls into a missing one); p2p_sel 1: bezier.
  const bool p2p = has_target && (e.p2p == "bezier" || e.p2p == "gravity");
  // A random frame each is frameStart + rand() % (frameEnd - frameStart + 1) in the game (chkRandom; nwn5e-tools
  // PARTICLES.md): a start one past the end divides by 0, so no random frame then (the draft keeps its flag).
  const bool random = e.random && e.frame_start != e.frame_end + 1;
  return {
      "node emitter " + node,
      "  parent " + parent,
      flag("p2p", p2p), flag("p2p_sel", !p2p || e.p2p == "bezier"), "  affectedByWind 0", "  m_isTinted 0", flag("bounce", e.bounce),
      flag("random", random), flag("inherit", e.inherit), "  inheritvel 0", flag("inherit_local", e.inherit_local),
      flag("splat", e.splat), flag("inherit_part", e.inherit_part), "  renderorder 0", "  spawntype 0",
      "  update " + e.update,
      "  render " + e.render,
      "  blend " + e.blend,
      "  texture " + e.texture,
      "  xgrid " + std::to_string(e.grid), "  ygrid " + std::to_string(e.grid_down ? e.grid_down : e.grid), "  loop 0",
      "  deadspace " + Num(std::min(std::max(e.deadspace, 0.0f), kVfxDeadspaceMax)), flag("twosidedtex", e.twosidedtex),
      // Its blast (the owner, 2026-11-17): 0 when none - the lines nwn5e always wrote, so a model without one keeps its bytes.
      "  blastRadius " + Num(std::max(0.0f, e.blast_radius)), "  blastLength " + Num(std::max(0.0f, e.blast_length)),
      "  position " + Num(e.position[0]) + " " + Num(e.position[1]) + " " + Num(e.position[2]),
      "  orientation " + Num(e.orientation[0]) + " " + Num(e.orientation[1]) + " " + Num(e.orientation[2]) + " " +
          Num(e.orientation[3]),
      "  colorStart " + Num(e.color_start[0]) + " " + Num(e.color_start[1]) + " " + Num(e.color_start[2]),
      "  colorEnd " + Num(e.color_end[0]) + " " + Num(e.color_end[1]) + " " + Num(e.color_end[2]),
      "  alphaStart " + Num(e.alpha_start), "  alphaEnd " + Num(e.alpha_end),
      "  sizeStart " + Num(e.size_start), "  sizeEnd " + Num(e.size_end),
      "  sizeStart_y 0", "  sizeEnd_y 0",
      "  frameStart " + std::to_string(e.frame_start), "  frameEnd " + std::to_string(e.frame_end),
      "  birthrate " + Num(keyed_birthrate ? 0.0f : e.birthrate),
      "  lifeExp " + Num(e.life),
      "  mass " + Num(e.mass), "  spread " + Num(e.spread), "  particleRot " + Num(e.particle_rot),
      "  velocity " + Num(e.velocity), "  randvel " + Num(e.randvel), "  fps " + Num(e.fps),
      "  xsize " + Num(e.xsize), "  ysize " + Num(e.ysize), "  bounce_co " + Num(e.bounce_co),
      "  blurlength " + Num(e.blur_length),
      "  lightningDelay " + Num(e.lightning_delay), "  lightningRadius " + Num(e.lightning_radius),
      "  lightningScale " + Num(e.lightning_scale),
      "  p2p_bezier2 " + Num(e.p2p_bezier2), "  p2p_bezier3 " + Num(e.p2p_bezier3),
      "  combinetime " + Num(e.combinetime), "  drag " + Num(e.drag), "  grav " + Num(e.grav),
      "  threshold " + Num(e.threshold),
      e.chunk.empty() ? std::string() : "  chunkName " + e.chunk,
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

// A node's name in a model nwn5e writes: <model>_<tag>_<the effect's name for it>, at most 31
// characters - the game keeps 31 of a node's name (MdlNode: strncpy 31 into a 32-byte field) and
// finds a "parent" by the whole name; one longer is cut, its children's parent isn't found and
// the game reads through a null pointer (MdlNode::InternalParseField+0x528 - the crash of
// 2026-10-26, Build from parts' "flare_gather_ringspin" nodes at 32-33 characters). The tag
// keeps it unique; the readable part gives way.
constexpr size_t kModelNodeNameMax = 31;
std::string ModelNodeName(const std::string& model, const std::string& tag, const std::string& readable) {
  const std::string prefix = model + "_" + tag + "_";
  const size_t room = prefix.size() < kModelNodeNameMax ? kModelNodeNameMax - prefix.size() : 0;
  std::string clean = VfxResRef(readable, room);
  if (clean.empty() && room) clean = "x";
  return (prefix + clean).substr(0, kModelNodeNameMax);
}

std::string NodeName(const std::string& model, const VfxEmitter& e, size_t index) {
  return ModelNodeName(model, std::to_string(index), e.name);
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

// A node's, an emitter's, a light's or a plane's key lines in a draft: the animations that key it, in order.
std::string DraftKeys(const std::vector<VfxKeys>& keys) {
  std::string out;
  for (const char* anim : kAnims) {
    for (const VfxKeys& k : keys) {
      if (k.anim != anim) continue;
      if (!k.position.empty()) out += std::string(anim) + "_position = " + Row(k.position) + "\n";
      if (!k.orientation.empty()) out += std::string(anim) + "_orientation = " + Row(k.orientation) + "\n";
      if (!k.color.empty()) out += std::string(anim) + "_color = " + Row(k.color) + "\n";  // #145, a light's
      if (!k.radius.empty()) out += std::string(anim) + "_radius = " + Row(k.radius) + "\n";
      if (!k.alpha.empty()) out += std::string(anim) + "_alpha = " + Row(k.alpha) + "\n";  // #39, a plane's
      // #39 (M4), a plane's scale: after its alpha, so every draft before it keeps its bytes.
      if (!k.scale.empty()) out += std::string(anim) + "_scale = " + Row(k.scale) + "\n";
      // KF, an emitter's own birthrate, alphaStart and alphaEnd: after the rest, the same way.
      if (!k.birthrate.empty()) out += std::string(anim) + "_birthrate = " + Row(k.birthrate) + "\n";
      if (!k.alpha_start.empty()) out += std::string(anim) + "_alpha_start = " + Row(k.alpha_start) + "\n";
      if (!k.alpha_end.empty()) out += std::string(anim) + "_alpha_end = " + Row(k.alpha_end) + "\n";
      // #179 Phase 3, an emitter's colour and size at birth and at the end: after the rest, the same way.
      if (!k.color_start.empty()) out += std::string(anim) + "_color_start = " + Row(k.color_start) + "\n";
      if (!k.color_end.empty()) out += std::string(anim) + "_color_end = " + Row(k.color_end) + "\n";
      if (!k.size_start.empty()) out += std::string(anim) + "_size_start = " + Row(k.size_start) + "\n";
      if (!k.size_end.empty()) out += std::string(anim) + "_size_end = " + Row(k.size_end) + "\n";
    }
  }
  return out;
}

// A draft line that is a key list ("<anim>_position" / "_orientation" / "_color" / "_radius" / "_alpha" / "_scale" /
// KF's "_birthrate" / "_alpha_start" / "_alpha_end" / #179's "_color_start" / "_color_end" / "_size_start" / "_size_end").
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
    if (key == a + "_color") {  // #145, a light's
      ReadAll(words, &FindKeys(keys, a, true)->color);
      return true;
    }
    if (key == a + "_radius") {
      ReadAll(words, &FindKeys(keys, a, true)->radius);
      return true;
    }
    if (key == a + "_alpha") {  // #39, a plane's
      ReadAll(words, &FindKeys(keys, a, true)->alpha);
      return true;
    }
    if (key == a + "_scale") {  // #39 (M4), a plane's
      ReadAll(words, &FindKeys(keys, a, true)->scale);
      return true;
    }
    if (key == a + "_birthrate") {  // KF, an emitter's
      ReadAll(words, &FindKeys(keys, a, true)->birthrate);
      return true;
    }
    if (key == a + "_alpha_start") {
      ReadAll(words, &FindKeys(keys, a, true)->alpha_start);
      return true;
    }
    if (key == a + "_alpha_end") {
      ReadAll(words, &FindKeys(keys, a, true)->alpha_end);
      return true;
    }
    if (key == a + "_color_start") {  // #179 Phase 3, an emitter's
      ReadAll(words, &FindKeys(keys, a, true)->color_start);
      return true;
    }
    if (key == a + "_color_end") {
      ReadAll(words, &FindKeys(keys, a, true)->color_end);
      return true;
    }
    if (key == a + "_size_start") {
      ReadAll(words, &FindKeys(keys, a, true)->size_start);
      return true;
    }
    if (key == a + "_size_end") {
      ReadAll(words, &FindKeys(keys, a, true)->size_end);
      return true;
    }
  }
  return false;
}

// The last key's time in one animation, over the effect's nodes, emitters and lights (#145: their colour and radius)
// and its planes' position, orientation, scale and alpha (#39: what the model keys of them).
float LastKeyTime(const VfxEffect& effect, const std::string& anim) {
  float last = 0.0f;
  const auto scan = [&](const std::vector<VfxKeys>& keys) {
    for (const VfxKeys& k : keys) {
      if (k.anim != anim) continue;
      for (size_t i = 0; i + 3 < k.position.size(); i += 4) last = std::max(last, k.position[i]);
      for (size_t i = 0; i + 4 < k.orientation.size(); i += 5) last = std::max(last, k.orientation[i]);
      for (size_t i = 0; i + 3 < k.color.size(); i += 4) last = std::max(last, k.color[i]);
      for (size_t i = 0; i + 1 < k.radius.size(); i += 2) last = std::max(last, k.radius[i]);
    }
  };
  for (const VfxNode& n : effect.nodes) scan(n.keys);
  for (const VfxEmitter& e : effect.emitters) {
    scan(e.keys);
    for (const VfxKeys& k : e.keys) {  // KF: its own birthrate, alphaStart and alphaEnd (what the model keys of them)
      if (k.anim != anim) continue;
      for (const std::vector<float>* list : {&k.birthrate, &k.alpha_start, &k.alpha_end, &k.size_start, &k.size_end}) {
        for (size_t i = 0; i + 1 < list->size(); i += 2) last = std::max(last, (*list)[i]);
      }
      for (const std::vector<float>* list : {&k.color_start, &k.color_end}) {  // #179 Phase 3 (t r g b rows)
        for (size_t i = 0; i + 3 < list->size(); i += 4) last = std::max(last, (*list)[i]);
      }
    }
  }
  for (const VfxLight& l : effect.lights) scan(l.keys);
  for (const VfxPlane& p : effect.planes) {
    for (const VfxKeys& k : p.keys) {
      if (k.anim != anim) continue;
      for (size_t i = 0; i + 3 < k.position.size(); i += 4) last = std::max(last, k.position[i]);  // M4
      for (size_t i = 0; i + 4 < k.orientation.size(); i += 5) last = std::max(last, k.orientation[i]);
      for (size_t i = 0; i + 1 < k.scale.size(); i += 2) last = std::max(last, k.scale[i]);
      for (size_t i = 0; i + 1 < k.alpha.size(); i += 2) last = std::max(last, k.alpha[i]);
    }
  }
  return last;
}

// #39 (M4): a plane's key lists in one animation as the model writes them - of each kind the first list there with a
// whole row (a draft has one list an animation); nullptr: none of that kind. A plane with none is in no animation.
struct PlaneKeys {
  const VfxKeys* move = nullptr;
  const VfxKeys* turn = nullptr;
  const VfxKeys* grow = nullptr;
  const VfxKeys* fade = nullptr;
  bool Any() const { return move || turn || grow || fade; }
};

PlaneKeys PlaneKeysIn(const VfxPlane& p, const std::string& anim) {
  PlaneKeys out;
  for (const VfxKeys& k : p.keys) {
    if (k.anim != anim) continue;
    if (!out.move && k.position.size() >= 4) out.move = &k;
    if (!out.turn && k.orientation.size() >= 5) out.turn = &k;
    if (!out.grow && k.scale.size() >= 2) out.grow = &k;
    if (!out.fade && k.alpha.size() >= 2) out.fade = &k;
  }
  return out;
}

// TT (#39; the owner, 2026-11-18, MBK-1: "the disk in-game still starts at ground level, rises to 3m and then drops
// down again"). How far an item's first key may be from its rest and still be at rest: 1e-4 - metres of a move or of a
// light's radius, radians of a turn, or of an alpha, a scale or a colour channel. Far below anything a 0.25 s blend
// could show (0.1 mm, 0.006 degrees, a ten-thousandth of the alpha); far above the noise a value picks up on its way
// from the bench to the model (a float's ~1e-7; the draft's %g keeps 6 digits, 1e-5 at most below 10), so a key the
// bench built at the rest counts as at rest. The binary reader's own tolerance for a key at the geometry's value (a
// single one within it is no motion, ReadVfxBinaryModel).
constexpr float kVfxRestTolerance = 1e-4f;

// A turn (axis x y z, angle in radians, as a model writes it) as a unit quaternion (x y z w), in doubles; no axis is
// no turn.
void TurnQuaternion(const float* turn, double q[4]) {
  const double n = std::sqrt(static_cast<double>(turn[0]) * turn[0] + static_cast<double>(turn[1]) * turn[1] +
                             static_cast<double>(turn[2]) * turn[2]);
  const double half = 0.5 * turn[3];
  const double s = n > 1e-12 ? std::sin(half) / n : 0.0;
  for (int k = 0; k < 3; ++k) q[k] = turn[k] * s;
  q[3] = n > 1e-12 ? std::cos(half) : 1.0;
}

// The angle between two turns (radians, 0 to pi), compared as turns, not as their numbers: the same turn written
// another way - the axis and the angle both negated, an axis not of length 1, a whole turn more, no angle about any
// axis - is 0. The turn from a to b (a's conjugate times b): its vector part sin(angle / 2) long, its w cos(angle / 2),
// either sign.
double TurnBetween(const float* a, const float* b) {
  double p[4], q[4];
  TurnQuaternion(a, p);
  TurnQuaternion(b, q);
  const double x = p[3] * q[0] - p[0] * q[3] - p[1] * q[2] + p[2] * q[1];
  const double y = p[3] * q[1] + p[0] * q[2] - p[1] * q[3] - p[2] * q[0];
  const double z = p[3] * q[2] - p[0] * q[1] + p[1] * q[0] - p[2] * q[3];
  const double w = p[3] * q[3] + p[0] * q[0] + p[1] * q[1] + p[2] * q[2];
  return 2.0 * std::atan2(std::sqrt(x * x + y * y + z * z), std::fabs(w));
}

// A key list's first row (the values after its time, `width` numbers a row) off its rest by more than
// kVfxRestTolerance in any value; a list without a whole row is written as no keys - not off.
bool FirstRowOffRest(const std::vector<float>& list, size_t width, const float* rest) {
  if (list.size() < width) return false;
  for (size_t c = 1; c < width; ++c) {
    if (std::fabs(list[c] - rest[c - 1]) > kVfxRestTolerance) return true;
  }
  return false;
}

// The same for a turn (t ax ay az angle a row), compared as a turn.
bool FirstTurnOffRest(const std::vector<float>& list, const float rest[4]) {
  return list.size() >= 5 && TurnBetween(&list[1], rest) > kVfxRestTolerance;
}

// A plane's first move, turn, scale or alpha key in an animation off its rest (PlaneKeysIn's lists): StartsOffRest's
// part for a plane, and KF's mesh beat (WriteVfxModel).
bool PlaneStartsOffRest(const VfxPlane& p, const std::string& anim) {
  const float as_placed = 1.0f;
  const PlaneKeys k = PlaneKeysIn(p, anim);
  return (k.move && FirstRowOffRest(k.move->position, 4, p.position)) ||
         (k.turn && FirstTurnOffRest(k.turn->orientation, p.orientation)) ||
         (k.grow && FirstRowOffRest(k.grow->scale, 2, &as_placed)) || (k.fade && FirstRowOffRest(k.fade->alpha, 2, &p.alpha));
}

// TT: whether an animation starts an item off its rest - the first key of what the model keys there against the item's
// own value in the geometry: a node's, an emitter's, a light's or a plane's position and orientation (a light's
// orientation is none: its node's is written 0 0 0 0), a plane's scale (1: as placed) and alpha (its material's), a
// light's colour and radius. Every key list the model writes of them (a plane's: of each kind the first,
// PlaneKeysIn); an emitter's birthrate keys are no pose. The game blends an animation in from the pose before it over
// its transtime (the compiled animation header's f32 at +0x74, after its length at +0x70) - an effect's impact from
// the rest pose, its geometry's - so a disc keyed from 3 m with its rest on the ground rose from the ground for 0.25 s
// before it fell (MBK-1). The game's own keyed effect nodes keep their rest at their first key (vim_protevil's
// OmenEmitter01: rest 0 0 11.11, its first key; vdr_bigby_int's hand meshes; fx_meteor's and fx_rainice's, impacts
// with transtime 0); of its effects' impacts 282 blend 0.25, 81 start at once (0) and 2 blend 0.15.
bool StartsOffRest(const VfxEffect& effect, const std::string& anim) {
  const float none[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  const auto moves = [&](const std::vector<VfxKeys>& keys, const float position[3], const float orientation[4]) {
    for (const VfxKeys& k : keys) {
      if (k.anim == anim && (FirstRowOffRest(k.position, 4, position) || FirstTurnOffRest(k.orientation, orientation))) {
        return true;
      }
    }
    return false;
  };
  for (const VfxNode& n : effect.nodes) {
    if (moves(n.keys, n.position, n.orientation)) return true;
  }
  for (const VfxEmitter& e : effect.emitters) {
    if (moves(e.keys, e.position, e.orientation)) return true;
  }
  for (const VfxLight& l : effect.lights) {
    if (moves(l.keys, l.position, none)) return true;
    for (const VfxKeys& k : l.keys) {
      if (k.anim == anim && (FirstRowOffRest(k.color, 4, l.color) || FirstRowOffRest(k.radius, 2, &l.radius))) {
        return true;
      }
    }
  }
  for (const VfxPlane& p : effect.planes) {
    if (PlaneStartsOffRest(p, anim)) return true;
  }
  return false;
}

// KF: an emitter keeps its own emission - its birthrate keys as read from a model (a list with a whole row in one of the
// effect's animations: what a draft carries - a placeable's animloop01 keys none of an effect's) are written in place of
// nwn5e's (VA-2's restore, an emit time's, a start's, a one-shot's, the cessation's off): in each animation its own list
// there, none where it has none - the game's controller holds the value it had (vdr_eleshield's Flame01: 100 -> 0 in the
// impact, none after: its flame stays out).
bool OwnEmission(const VfxEmitter& e) {
  for (const VfxKeys& k : e.keys) {
    if (k.birthrate.size() >= 2 && AnimSlot(k.anim) >= 0) return true;
  }
  return false;
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

std::vector<VfxAreaTile> VfxAreaTiles(const VfxEmitter& e) {
  const float x = std::min(std::max(e.xsize, 0.0f), kVfxAreaMax), y = std::min(std::max(e.ysize, 0.0f), kVfxAreaMax);
  const bool tiled = e.update == "Fountain" || e.update == "Explosion";
  const bool circle = e.area_shape == 1 && x > 0.0f;
  if (!tiled || (!circle && x <= kVfxAreaTileMax && y <= kVfxAreaTileMax)) return {{0.0f, 0.0f, e.xsize, e.ysize}};
  // How many pieces a length takes (a hair over the limit from rounding stays one).
  const auto pieces = [](float length) {
    return std::max(1, static_cast<int>(std::ceil(length / kVfxAreaTileMax - 0.0001f)));
  };
  std::vector<VfxAreaTile> out;
  if (!circle) {
    const int nx = pieces(x), ny = pieces(y);
    for (int i = 0; i < nx; ++i) {
      for (int j = 0; j < ny; ++j) out.push_back({-x / 2 + (i + 0.5f) * x / nx, -y / 2 + (j + 0.5f) * y / ny, x / nx, y / ny});
    }
    return out;
  }
  const float r = x / 2, half = r / std::sqrt(2.0f), side = 2 * half;  // the square in the circle
  const int n = pieces(side);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) out.push_back({-half + (i + 0.5f) * side / n, -half + (j + 0.5f) * side / n, side / n, side / n});
  }
  const int strips = e.area_edge == 1 ? 2 : 1;
  const float band = (r - half) / strips;
  for (int s = 0; s < strips; ++s) {
    const float mid = half + (s + 0.5f) * band;  // the strip's middle, out from the centre
    const float chord = 2 * std::sqrt(std::max(r * r - mid * mid, 0.0f));
    const int p = pieces(chord);
    for (int k = 0; k < p; ++k) {
      const float along = -chord / 2 + (k + 0.5f) * chord / p, wide = chord / p;
      out.push_back({along, mid, wide, band});
      out.push_back({along, -mid, wide, band});
      out.push_back({mid, along, band, wide});
      out.push_back({-mid, along, band, wide});
    }
  }
  return out;
}

namespace {

// #175: each emitter whose area is pieces (VfxAreaTiles) becomes their carrier - its name, place, turn, keys and
// children kept, no birth of its own - and the pieces its children: copies of it at their middles, each its piece of
// the area, its share (by area) of the birthrate and of its own birthrate keys, no move or turn of its own (the
// carrier's moves them). An Explosion's burst stays whole particles: shared by the largest remainders.
VfxEffect WithAreaTiles(const VfxEffect& effect) {
  VfxEffect out = effect;
  std::set<std::string> taken;
  for (const VfxNode& n : effect.nodes) taken.insert(n.name);
  for (const VfxEmitter& e : effect.emitters) taken.insert(e.name);
  std::vector<VfxEmitter> added;
  for (VfxEmitter& e : out.emitters) {
    const std::vector<VfxAreaTile> tiles = VfxAreaTiles(e);
    if (tiles.size() < 2) continue;
    float total = 0.0f;
    for (const VfxAreaTile& t : tiles) total += t.width * t.depth;
    std::vector<float> share(tiles.size()), rate(tiles.size());
    for (size_t i = 0; i < tiles.size(); ++i) {
      share[i] = total > 0.0f ? tiles[i].width * tiles[i].depth / total : 1.0f / tiles.size();
      rate[i] = e.birthrate * share[i];
    }
    if (e.update == "Explosion") {
      const int whole = static_cast<int>(std::lround(e.birthrate));
      int given = 0;
      std::vector<std::pair<float, size_t>> left;
      for (size_t i = 0; i < tiles.size(); ++i) {
        const float exact = whole * share[i];
        rate[i] = std::floor(exact);
        given += static_cast<int>(rate[i]);
        left.push_back({exact - rate[i], i});
      }
      std::stable_sort(left.begin(), left.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
      for (size_t k = 0; k < left.size() && given < whole; ++k, ++given) rate[left[k].second] += 1.0f;
    }
    for (size_t i = 0; i < tiles.size(); ++i) {
      VfxEmitter t = e;
      std::string name = e.name + "_t" + std::to_string(i + 1);
      while (taken.count(name)) name += "x";
      taken.insert(name);
      t.name = name;
      t.parent = e.name;
      t.position[0] = tiles[i].x / 100.0f;
      t.position[1] = tiles[i].y / 100.0f;
      t.position[2] = 0.0f;
      for (float& q : t.orientation) q = 0.0f;
      t.xsize = tiles[i].width;
      t.ysize = tiles[i].depth;
      t.area_shape = 0;
      t.area_edge = 0;
      t.birthrate = rate[i];
      for (VfxKeys& k : t.keys) {
        k.position.clear();
        k.orientation.clear();
        for (size_t v = 1; v < k.birthrate.size(); v += 2) k.birthrate[v] *= share[i];
      }
      added.push_back(t);
    }
    e.birthrate = 0.0f;
    for (VfxKeys& k : e.keys) {
      for (size_t v = 1; v < k.birthrate.size(); v += 2) k.birthrate[v] = 0.0f;
    }
  }
  out.emitters.insert(out.emitters.end(), added.begin(), added.end());
  return out;
}

}  // namespace

std::string WriteVfxModel(const VfxEffect& original, const std::string& model) {
  const VfxEffect tiled = WithAreaTiles(original);  // #175
  const VfxEffect& effect = tiled;
  // The model's own names for the effect's nodes and emitters (unique to it), and its parents
  // before its children: the order a model is written in (a parent never written - a loop,
  // a missing one - leaves its child under the root).
  const size_t node_count = effect.nodes.size(), total = node_count + effect.emitters.size();
  std::vector<std::string> names(total);
  std::map<std::string, std::string> by_name;
  for (size_t k = 0; k < total; ++k) {
    if (k < node_count) {
      names[k] = ModelNodeName(model, "n" + std::to_string(k), effect.nodes[k].name);
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
  // A node under an emitter is its point-to-point target: a reference node (0.42.5).
  std::set<std::string> emitter_names;
  for (const VfxEmitter& e : effect.emitters) emitter_names.insert(e.name);
  const auto is_target = [&](size_t k) {
    return k < node_count && !orphans.count(k) && emitter_names.count(effect.nodes[k].parent) > 0;
  };
  const auto has_target = [&](const VfxEmitter& e) {
    for (size_t k = 0; k < node_count; ++k) {
      if (effect.nodes[k].parent == e.name && is_target(k)) return true;
    }
    return false;
  };
  const auto turn = [](const float o[4]) {
    return Num(o[0]) + " " + Num(o[1]) + " " + Num(o[2]) + " " + Num(o[3]);
  };
  // #145: the lights - leaves (nothing hangs under them), under their parent (a node or an emitter of the effect, else
  // the model's root), named as the nodes are.
  std::vector<std::string> light_names;
  for (size_t i = 0; i < effect.lights.size(); ++i) {
    light_names.push_back(ModelNodeName(model, "l" + std::to_string(i), effect.lights[i].name));
  }
  const auto light_parent = [&](const VfxLight& l) {
    const auto it = l.parent.empty() ? by_name.end() : by_name.find(l.parent);
    return it == by_name.end() ? model : it->second;
  };
  // #161: the planes the same way (#39: the geometry's names and parents are their animations' too).
  std::vector<std::string> plane_names;
  for (size_t i = 0; i < effect.planes.size(); ++i) {
    plane_names.push_back(ModelNodeName(model, "p" + std::to_string(i), effect.planes[i].name));
  }
  const auto plane_parent = [&](const VfxPlane& p) {
    const auto it = p.parent.empty() ? by_name.end() : by_name.find(p.parent);
    return it == by_name.end() ? model : it->second;
  };

  std::vector<std::string> lines = {
      "#MAXMODEL ASCII",
      std::string(kVfxModelMark) + " VFX editor (!vfxedit): " + effect.name,  // nwn5e's mark (KF fix round 1)
      "newmodel " + model,
      "setsupermodel " + model + " NULL",
      "classification effect",
      "setanimationscale 1",
      "beginmodelgeom " + model,
      "node dummy " + model,
      "  parent NULL",
      "endnode",
  };
  // Bursts that start late (the owner's phases, 2026-11-14): each gets a detonate event 0.01 s after its start (one for
  // starts closer than kVfxBurstWindow), and every burst's rate is keyed on only round its own event.
  const auto is_burst = [](const VfxEmitter& e) { return Lower(e.update) == "explosion"; };
  std::vector<float> burst_events;
  for (const VfxEmitter& e : effect.emitters) {
    if (!is_burst(e) || e.start_seconds <= 0.0f) continue;
    const float at = e.start_seconds + 0.01f;
    bool near = false;
    for (float other : burst_events) near = near || std::fabs(other - at) < kVfxBurstWindow;
    if (!near) burst_events.push_back(at);
  }
  std::sort(burst_events.begin(), burst_events.end());
  const bool late_burst = !burst_events.empty();
  for (size_t k : order) {
    if (k < node_count) {
      const VfxNode& n = effect.nodes[k];
      lines.push_back((is_target(k) ? "node reference " : "node dummy ") + names[k]);
      lines.push_back("  parent " + parent_line(k));
      lines.push_back("  position " + Num(n.position[0]) + " " + Num(n.position[1]) + " " + Num(n.position[2]));
      lines.push_back("  orientation " + turn(n.orientation));
      if (is_target(k)) {
        lines.push_back("  refModel " + (n.ref_model.empty() ? std::string("fx_ref") : n.ref_model));
        lines.push_back("  reattachable 0");
      }
      lines.push_back("endnode");
      continue;
    }
    const VfxEmitter& e = effect.emitters[k - node_count];
    // An emitter that switches off is keyed in the impact; F's always-on ones are too, and one that starts late. KF: one
    // with its own emission (OwnEmission) keeps its rate here - its own keys say when it's on.
    const bool keyed = !OwnEmission(e) &&
                       (e.emit_seconds > 0.0f || !effect.duration || e.start_seconds > 0.0f || (late_burst && is_burst(e)));
    // DE-1 (#172): a burst the impact's detonate at 0 sets off keeps its count in the geometry, as every one of the game's
    // own does - the impact blends each controller in from the geometry's value over its transtime (InternalControl: field
    // = (1 - w) field + w key), and ExplosionEmitter::Update spends a detonate on int(birthrate) particles, none while that
    // is below 1. Keyed on from 0 (its impact's keys start at its count), the blend is then between equal values. A late
    // one keeps 0, so the detonate at 0 doesn't set it off.
    const bool burst_at_start = is_burst(e) && e.start_seconds <= 0.0f;
    for (const std::string& line : EmitterLines(e, names[k], parent_line(k), keyed && !burst_at_start, has_target(e))) {
      if (!line.empty()) lines.push_back(line);
    }
  }
  // A light node each, with its own flags (a new light's: dynamic, priority 3, fading, ambient-only off - it lights its
  // surroundings - as most of the game's own effect lights, #145 Task 0), affecting creatures, no flare.
  for (size_t i = 0; i < effect.lights.size(); ++i) {
    const VfxLight& l = effect.lights[i];
    lines.push_back("node light " + light_names[i]);
    lines.push_back("  parent " + light_parent(l));
    lines.push_back("  position " + Num(l.position[0]) + " " + Num(l.position[1]) + " " + Num(l.position[2]));
    lines.push_back("  orientation 0 0 0 0");
    lines.push_back("  wirecolor 1 1 1");
    lines.push_back(std::string("  ambientonly ") + (l.ambient_only ? "1" : "0"));
    lines.push_back(std::string("  isdynamic ") + (l.dynamic ? "1" : "0"));
    lines.push_back("  affectdynamic 1");
    lines.push_back(std::string("  shadow ") + (l.shadow ? "1" : "0"));
    lines.push_back("  lightpriority " + std::to_string(l.priority));
    lines.push_back(std::string("  fadingLight ") + (l.fading ? "1" : "0"));
    lines.push_back("  flareradius 0");
    lines.push_back("  multiplier " + Num(l.multiplier));
    lines.push_back("  radius " + Num(l.radius));
    lines.push_back("  color " + Num(l.color[0]) + " " + Num(l.color[1]) + " " + Num(l.color[2]));
    lines.push_back("endnode");
  }
  // #161: each plane a plain trimesh node, as the game's own effect models have them (Entangle's vines): its vertices,
  // UVs (the vertex's own index) and faces (smoothing group 1, material 1). Its material (the owner, 2026-11-16) where
  // it has one of its own; a plane with the defaults' ambient and diffuse - every one before the material was kept - is
  // written as nwn5e always wrote planes, full ambient and diffuse, so those effects keep their look in the game.
  const VfxPlane plane_defaults;
  for (size_t i = 0; i < effect.planes.size(); ++i) {
    const VfxPlane& p = effect.planes[i];
    lines.push_back("node trimesh " + plane_names[i]);
    lines.push_back("  parent " + plane_parent(p));
    lines.push_back("  position " + Num(p.position[0]) + " " + Num(p.position[1]) + " " + Num(p.position[2]));
    lines.push_back("  orientation " + Num(p.orientation[0]) + " " + Num(p.orientation[1]) + " " + Num(p.orientation[2]) + " " +
                    Num(p.orientation[3]));
    lines.push_back("  wirecolor 1 1 1");
    const bool own = Num3(p.ambient) != Num3(plane_defaults.ambient) || Num3(p.diffuse) != Num3(plane_defaults.diffuse);
    lines.push_back("  ambient " + (own ? Num3(p.ambient) : std::string("1 1 1")));
    lines.push_back("  diffuse " + (own ? Num3(p.diffuse) : std::string("1 1 1")));
    lines.push_back("  specular 0 0 0");
    lines.push_back("  shininess 1");
    lines.push_back("  bitmap " + (p.bitmap.empty() ? std::string("NULL") : p.bitmap));
    lines.push_back(std::string("  render ") + (p.render ? "1" : "0"));
    lines.push_back("  shadow 0");
    if (Num3(p.selfillum) != Num3(plane_defaults.selfillum)) lines.push_back("  selfillumcolor " + Num3(p.selfillum));
    if (Num(p.alpha) != Num(plane_defaults.alpha)) lines.push_back("  alpha " + Num(p.alpha));
    // Its transparencyhint (the owner, 2026-11-18: without one the game cuts a see-through mesh at alpha 0.2 and writes
    // its depth) only when it isn't 0, so every model before it keeps its bytes.
    if (HintValue(p.transparency_hint)) lines.push_back("  transparencyhint " + std::to_string(HintValue(p.transparency_hint)));
    const size_t verts = p.verts.size() / 3;
    lines.push_back("  verts " + std::to_string(verts));
    for (size_t v = 0; v < verts; ++v) lines.push_back("    " + Num(p.verts[3 * v]) + " " + Num(p.verts[3 * v + 1]) + " " + Num(p.verts[3 * v + 2]));
    const size_t uvs = std::min(verts, p.tverts.size() / 2);
    lines.push_back("  tverts " + std::to_string(uvs));
    for (size_t v = 0; v < uvs; ++v) lines.push_back("    " + Num(p.tverts[2 * v]) + " " + Num(p.tverts[2 * v + 1]) + " 0");
    const size_t faces = p.faces.size() / 3;
    lines.push_back("  faces " + std::to_string(faces));
    for (size_t f = 0; f < faces; ++f) {
      const std::string a = std::to_string(p.faces[3 * f]), b = std::to_string(p.faces[3 * f + 1]), c = std::to_string(p.faces[3 * f + 2]);
      lines.push_back("    " + a + " " + b + " " + c + " 1 " + a + " " + b + " " + c + " 1");
    }
    lines.push_back("endnode");
  }
  lines.push_back("endmodelgeom " + model);
  constexpr float kLightFadeOut = 0.4f;  // #145: how fast a light without an end of its own goes out
  // The impact: long enough for every emitter's emission (and, F, what it emitted to fade),
  // for the effect's own length and for its keys; the duration and cessation as long as the
  // effect has them (2 s / 1 s if not given) and their keys.
  float impact = 0.2f;
  for (const VfxEmitter& e : effect.emitters) {
    // When it goes off in the impact (a lasting one always on: when it comes on), from its start.
    const float s = e.start_seconds;
    const float on = e.emit_seconds > 0.0f ? s + e.emit_seconds : (effect.duration ? s : std::max(s, effect.seconds));
    impact = std::max(impact, on + 0.01f + (effect.duration ? 0.0f : e.life));
  }
  impact = std::max({impact, effect.lengths[0], LastKeyTime(effect, "impact")});
  const float duration = std::max(effect.lengths[1] > 0.0f ? effect.lengths[1] : 2.0f, LastKeyTime(effect, "duration"));
  const float cessation =
      std::max(effect.lengths[2] > 0.0f ? effect.lengths[2] : 1.0f, LastKeyTime(effect, "cessation"));
  // One key list: "  <word> <rows>", then its whole rows of `width` numbers, the time first (t x y z, t ax ay az angle,
  // t r g b, t m, t s, t a) - nothing without a whole row. Every node's, emitter's, light's and plane's lists.
  const auto list_lines = [&](const char* word, const std::vector<float>& values, size_t width) {
    if (values.size() < width) return;
    lines.push_back(std::string("  ") + word + " " + std::to_string(values.size() / width));
    for (size_t i = 0; i + width <= values.size(); i += width) {
      std::string row = "   ";
      for (size_t c = 0; c < width; ++c) row += " " + Num(values[i + c]);
      lines.push_back(row);
    }
  };
  const auto key_lines = [&](const std::vector<VfxKeys>& keys, const char* anim) {
    for (const VfxKeys& k : keys) {
      if (k.anim != anim) continue;
      list_lines("positionkey", k.position, 4);
      list_lines("orientationkey", k.orientation, 5);
      list_lines("colorkey", k.color, 4);  // #145, a light's
      list_lines("radiuskey", k.radius, 2);
    }
  };
  // TT: the impact starts on its keys (transtime 0) when an item's first key there is off its rest - else the game blends
  // 0.25 s from the rest pose into it (a dropped disc rose from the ground first). Every other impact, and every duration
  // and cessation (each blends from the animation before it), 0.25 as always.
  // KF: a transtime kept from the model read (a game effect's own: TT's review - a re-saved one wrote 0 where the game's
  // is 0.25) is written as it is, unless the impact has a mesh beat (a plane the bench placed: its mesh named) whose first
  // key there is off its rest - then 0, so a Drop in on a game effect's mesh beat starts on its keys (MBK-1) while every
  // game effect as read keeps its own (its planes never have a mesh). A transtime not known (-1, every draft of nwn5e's):
  // TT's rule.
  const bool impact_off_rest = StartsOffRest(effect, "impact");
  bool beat_off_rest = false;
  for (const VfxPlane& p : effect.planes) beat_off_rest = beat_off_rest || (!p.mesh.empty() && PlaneStartsOffRest(p, "impact"));
  const auto transtime = [&](const std::string& name) {
    const int slot = AnimSlot(name);
    const float kept = slot >= 0 ? effect.transtime[slot] : -1.0f;
    if (kept >= 0.0f && !(slot == 0 && beat_off_rest)) return Num(kept);
    return std::string(slot == 0 && impact_off_rest ? "0" : "0.25");
  };
  const auto anim = [&](const char* name, float length, bool keys_on, bool keys_off) {
    lines.push_back("");
    lines.push_back(std::string("newanim ") + name + " " + model);
    lines.push_back("  length " + Num(length));
    lines.push_back("  transtime " + transtime(name));
    lines.push_back("  animroot " + model);
    // Explosion emitters burst on the impact's detonate event (the game's own models have it); late bursts on their own.
    if (std::string(name) == "impact") {
      lines.push_back("  event 0 detonate");
      for (float at : burst_events) lines.push_back("  event " + Num(at) + " detonate");
    }
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
      // KF: of each of its own kinds the first list in this animation with a whole row (a draft has one an animation).
      const auto own = [&](std::vector<float> VfxKeys::*list) -> const std::vector<float>* {
        for (const VfxKeys& keys : e.keys) {
          if (keys.anim == name && (keys.*list).size() >= 2) return &(keys.*list);
        }
        return nullptr;
      };
      if (OwnEmission(e)) {
        // KF: its own birthrate keys here, and only them (none where it has none: the game's controller holds it).
        if (const std::vector<float>* rate = own(&VfxKeys::birthrate)) list_lines("birthratekey", *rate, 2);
      } else if (keys_off) {
        lines.push_back("  birthratekey 1");
        lines.push_back("    0 0");
      } else if (keys_on && late_burst && is_burst(e)) {
        // A burst in an effect with late ones: its rate on only round its own detonate event (0 s, or 0.01 s after
        // its start), so another's event doesn't set it off again.
        const float s = e.start_seconds;
        if (s > 0.0f) {
          lines.push_back("  birthratekey 5");
          lines.push_back("    0 0");
          lines.push_back("    " + Num(s) + " 0");
          lines.push_back("    " + Num(s + 0.01f) + " " + Num(e.birthrate));
          lines.push_back("    " + Num(s + 0.01f + kVfxBurstWindow) + " " + Num(e.birthrate));
          lines.push_back("    " + Num(s + 0.02f + kVfxBurstWindow) + " 0");
        } else {
          lines.push_back("  birthratekey 3");
          lines.push_back("    0 " + Num(e.birthrate));
          lines.push_back("    " + Num(kVfxBurstWindow) + " " + Num(e.birthrate));
          lines.push_back("    " + Num(kVfxBurstWindow + 0.01f) + " 0");
        }
      } else if (keys_on && e.start_seconds > 0.0f) {
        // The owner's phases (2026-11-14): off until its start, then on - a lasting effect's always-on one stays on (the
        // duration's restore key below keeps it), else off again after its On for (a one-shot's: at the effect's end).
        const float s = e.start_seconds;
        if (effect.duration && e.emit_seconds <= 0.0f) {
          lines.push_back("  birthratekey 3");
        } else {
          lines.push_back("  birthratekey 5");
        }
        lines.push_back("    0 0");
        lines.push_back("    " + Num(s) + " 0");
        lines.push_back("    " + Num(s + 0.01f) + " " + Num(e.birthrate));
        if (!effect.duration || e.emit_seconds > 0.0f) {
          const float off = e.emit_seconds > 0.0f ? s + e.emit_seconds : std::max(s + 0.01f, effect.seconds);
          lines.push_back("    " + Num(off) + " " + Num(e.birthrate));
          lines.push_back("    " + Num(off + 0.01f) + " 0");
        }
      } else if (keys_on && (e.emit_seconds > 0.0f || !effect.duration)) {
        const float on = e.emit_seconds > 0.0f ? e.emit_seconds : effect.seconds;
        lines.push_back("  birthratekey 3");
        lines.push_back("    0 " + Num(e.birthrate));
        lines.push_back("    " + Num(on) + " " + Num(e.birthrate));
        lines.push_back("    " + Num(on + 0.01f) + " 0");
      } else if (effect.duration && e.emit_seconds <= 0.0f) {
        // #161 VA-2: a lasting effect's emitter on again in its impact and duration - an area's copies run the cessation
        // (which turns it off) and then these again, and a birthrate left at 0 stays there (the game's own lasting
        // effects have no birthrate keys at all).
        lines.push_back("  birthratekey 1");
        lines.push_back("    0 " + Num(e.birthrate));
      }
      key_lines(e.keys, name);
      // KF: its own alphaStart and alphaEnd keys (controllers 84 and 80, the game's spelling) after its move and turn.
      if (const std::vector<float>* fade = own(&VfxKeys::alpha_start)) list_lines("alphaStartkey", *fade, 2);
      if (const std::vector<float>* fade = own(&VfxKeys::alpha_end)) list_lines("alphaEndkey", *fade, 2);
      // #179 Phase 3: its colour and size keys (controllers 108, 96, 168, 172: the game's colorStartkey, colorEndkey,
      // sizeStartkey, sizeEndkey) after them - of each kind the first list in this animation with a whole row.
      const auto whole = [&](std::vector<float> VfxKeys::*list, size_t width) -> const std::vector<float>* {
        for (const VfxKeys& keys : e.keys) {
          if (keys.anim == name && (keys.*list).size() >= width) return &(keys.*list);
        }
        return nullptr;
      };
      if (const std::vector<float>* c = whole(&VfxKeys::color_start, 4)) list_lines("colorStartkey", *c, 4);
      if (const std::vector<float>* c = whole(&VfxKeys::color_end, 4)) list_lines("colorEndkey", *c, 4);
      if (const std::vector<float>* z = whole(&VfxKeys::size_start, 2)) list_lines("sizeStartkey", *z, 2);
      if (const std::vector<float>* z = whole(&VfxKeys::size_end, 2)) list_lines("sizeEndkey", *z, 2);
      lines.push_back("endnode");
    }
    // #145: in the effect's last animation (a lasting one's cessation, a one-shot's impact) a light without radius keys
    // of its own there goes out with it - 0.4 s from its radius to 0, as the game's own effects end theirs dark (the
    // owner's test: else the game lights it again for a few seconds after Stop).
    const bool last = std::string(name) == (effect.duration ? "cessation" : "impact");
    for (size_t i = 0; i < effect.lights.size(); ++i) {  // #145
      const VfxLight& l = effect.lights[i];
      lines.push_back("node light " + light_names[i]);
      lines.push_back("  parent " + light_parent(l));
      key_lines(l.keys, name);
      bool own = false;
      for (const VfxKeys& k : l.keys) own = own || (k.anim == name && k.radius.size() >= 2);
      if (last && !own) {
        const float fade = std::min(kLightFadeOut, length);
        lines.push_back("  radiuskey 2");
        lines.push_back("    " + Num(effect.duration ? 0.0f : length - fade) + " " + Num(l.radius));
        lines.push_back("    " + Num(effect.duration ? fade : length) + " 0");
      } else if (effect.duration && !last && !own) {  // #161 VA-2: lit again after a cessation, as the emitters
        lines.push_back("  radiuskey 1");
        lines.push_back("    0 " + Num(l.radius));
      }
      lines.push_back("endnode");
    }
    // #39: a plane with keys in this animation keys them there, on a node of its geometry's name under its geometry's
    // parent: a mesh beat's move and turn (positionkey, orientationkey, as a node's - M4), its scale (scalekey, controller
    // 36, one value a row, as the game's own vdr_bigby_int keys its hand's - M4) and its fade (alphakey, controller 128, as
    // the game's own effect models key a mesh's). A plane without any is in no animation, as before - every model written
    // before keeps its bytes. Of each kind the first list in this animation with a whole row (PlaneKeysIn; a draft has one
    // list an animation). Its mesh, scale, timing and tint are the bench's, never written; unlike a light it gets no fade
    // of its own at the effect's end (its keys say it all).
    for (size_t i = 0; i < effect.planes.size(); ++i) {
      const PlaneKeys k = PlaneKeysIn(effect.planes[i], name);
      if (!k.Any()) continue;
      lines.push_back("node trimesh " + plane_names[i]);
      lines.push_back("  parent " + plane_parent(effect.planes[i]));
      if (k.move) list_lines("positionkey", k.move->position, 4);
      if (k.turn) list_lines("orientationkey", k.turn->orientation, 5);
      if (k.grow) list_lines("scalekey", k.grow->scale, 2);
      if (k.fade) list_lines("alphakey", k.fade->alpha, 2);
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
  // KF: each animation's transtime, only when known (read from a model) - every draft before them keeps its bytes.
  for (int slot = 0; slot < 3; ++slot) {
    if (effect.transtime[slot] >= 0.0f) out += std::string(kAnims[slot]) + "_transtime = " + Num(effect.transtime[slot]) + "\n";
  }
  if (effect.shake_type) {
    out += "shake = " + std::to_string(effect.shake_type) + " " + Num(effect.shake_delay) + " " + Num(effect.shake_duration) + "\n";
  }
  for (const VfxNode& n : effect.nodes) {
    out += "\n[node " + n.name + "]\n";
    if (!n.parent.empty()) out += "parent = " + n.parent + "\n";
    out += "position = " + Num(n.position[0]) + " " + Num(n.position[1]) + " " + Num(n.position[2]) + "\n";
    out += "orientation = " + Num(n.orientation[0]) + " " + Num(n.orientation[1]) + " " + Num(n.orientation[2]) + " " +
           Num(n.orientation[3]) + "\n";
    if (!n.ref_model.empty()) out += "ref_model = " + n.ref_model + "\n";
    out += DraftKeys(n.keys);
  }
  for (const VfxEmitter& e : effect.emitters) {
    out += "\n[emitter " + e.name + "]\n";
    out += "update = " + e.update + "\nrender = " + e.render + "\nblend = " + e.blend + "\ntexture = " + e.texture + "\n";
    if (!e.chunk.empty()) out += "chunk = " + e.chunk + "\n";
    out += "position = " + Num(e.position[0]) + " " + Num(e.position[1]) + " " + Num(e.position[2]) + "\n";
    out += "orientation = " + Num(e.orientation[0]) + " " + Num(e.orientation[1]) + " " + Num(e.orientation[2]) + " " +
           Num(e.orientation[3]) + "\n";
    if (!e.parent.empty()) out += "parent = " + e.parent + "\n";
    out += "birthrate = " + Num(e.birthrate) + "\nemit_seconds = " + Num(e.emit_seconds) + "\n";
    if (e.start_seconds > 0.0f) out += "start_seconds = " + Num(e.start_seconds) + "\n";  // the owner's phases (2026-11-14)
    out += "life = " + Num(e.life) + "\n";
    out += "velocity = " + Num(e.velocity) + "\nrandvel = " + Num(e.randvel) + "\nspread = " + Num(e.spread) + "\n";
    out += "mass = " + Num(e.mass) + "\nsize = " + Num(e.size_start) + " " + Num(e.size_end) + "\n";
    out += "color_start = " + Num(e.color_start[0]) + " " + Num(e.color_start[1]) + " " + Num(e.color_start[2]) + "\n";
    out += "color_end = " + Num(e.color_end[0]) + " " + Num(e.color_end[1]) + " " + Num(e.color_end[2]) + "\n";
    out += "alpha = " + Num(e.alpha_start) + " " + Num(e.alpha_end) + "\n";
    out += "area = " + Num(e.xsize) + " " + Num(e.ysize) + "\n";
    // #175: a circle and its edge only when set - every draft before them keeps its bytes.
    if (e.area_shape == 1) out += "area_shape = circle\n";
    if (e.area_edge == 1) out += "area_edge = fine\n";
    // Frames down and the start frame (the owner, 2026-11-17) only when set - every draft before them keeps its bytes.
    out += "grid = " + std::to_string(e.grid) + "\n";
    if (e.grid_down != 0 && e.grid_down != e.grid) out += "grid_down = " + std::to_string(e.grid_down) + "\n";
    out += "frame_end = " + std::to_string(e.frame_end) + "\n";
    if (e.frame_start != 0) out += "frame_start = " + std::to_string(e.frame_start) + "\n";
    out += "fps = " + Num(e.fps) + "\nparticle_rot = " + Num(e.particle_rot) + "\n";
    out += "blur_length = " + Num(e.blur_length) + "\nbounce_co = " + Num(e.bounce_co) + "\n";
    out += "deadspace = " + Num(e.deadspace) + "\n";
    // Its blast (the owner, 2026-11-17) only when set, each on its own - every draft before them keeps its bytes.
    if (e.blast_radius != 0.0f) out += "blast_radius = " + Num(e.blast_radius) + "\n";
    if (e.blast_length != 0.0f) out += "blast_length = " + Num(e.blast_length) + "\n";
    if (!e.p2p.empty()) {
      out += "p2p = " + e.p2p + "\np2p_bezier = " + Num(e.p2p_bezier2) + " " + Num(e.p2p_bezier3) + "\n";
      out += "combinetime = " + Num(e.combinetime) + "\ngrav = " + Num(e.grav) + "\ndrag = " + Num(e.drag) + "\n";
      out += "threshold = " + Num(e.threshold) + "\n";
    }
    if (Lower(e.update) == "lightning") {
      out += "lightning = " + Num(e.lightning_delay) + " " + Num(e.lightning_radius) + " " + Num(e.lightning_scale) + "\n";
    }
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
  // #145: the lights, after the emitters (core.js writes the same bytes).
  for (const VfxLight& l : effect.lights) {
    out += "\n[light " + l.name + "]\n";
    if (!l.parent.empty()) out += "parent = " + l.parent + "\n";
    out += "position = " + Num(l.position[0]) + " " + Num(l.position[1]) + " " + Num(l.position[2]) + "\n";
    out += "color = " + Num(l.color[0]) + " " + Num(l.color[1]) + " " + Num(l.color[2]) + "\n";
    out += "radius = " + Num(l.radius) + "\nbrightness = " + Num(l.multiplier) + "\n";
    if (l.shadow) out += "shadow = 1\n";
    if (l.ambient_only) out += "ambientonly = 1\n";  // its flags: only off a new light's
    if (!l.dynamic) out += "dynamic = 0\n";
    if (l.priority != 3) out += "priority = " + std::to_string(l.priority) + "\n";
    if (!l.fading) out += "fading = 0\n";
    out += DraftKeys(l.keys);
  }
  // #161: the planes, after the lights (core.js writes the same bytes): kept whole, never edited.
  const VfxPlane plane_defaults;
  for (const VfxPlane& p : effect.planes) {
    out += "\n[plane " + p.name + "]\n";
    if (!p.parent.empty()) out += "parent = " + p.parent + "\n";
    out += "position = " + Num(p.position[0]) + " " + Num(p.position[1]) + " " + Num(p.position[2]) + "\n";
    out += "orientation = " + Num(p.orientation[0]) + " " + Num(p.orientation[1]) + " " + Num(p.orientation[2]) + " " +
           Num(p.orientation[3]) + "\n";
    out += "bitmap = " + p.bitmap + "\n";
    // Its material (the owner, 2026-11-16): only what isn't the default, so every draft before it keeps its bytes.
    if (Num3(p.diffuse) != Num3(plane_defaults.diffuse)) out += "diffuse = " + Num3(p.diffuse) + "\n";
    if (Num3(p.ambient) != Num3(plane_defaults.ambient)) out += "ambient = " + Num3(p.ambient) + "\n";
    if (Num3(p.selfillum) != Num3(plane_defaults.selfillum)) out += "selfillum = " + Num3(p.selfillum) + "\n";
    if (Num(p.alpha) != Num(plane_defaults.alpha)) out += "alpha = " + Num(p.alpha) + "\n";
    if (!p.render) out += "render = 0\n";
    // Its transparencyhint (the owner, 2026-11-18) the same way: only when it isn't 0.
    if (HintValue(p.transparency_hint)) out += "transparencyhint = " + std::to_string(HintValue(p.transparency_hint)) + "\n";
    // #39, a mesh beat's (the bench's): the same rule - only what isn't the default, so every draft before it keeps its
    // bytes - its tint too (M4), then its keys.
    if (!p.mesh.empty()) out += "mesh = " + p.mesh + "\n";
    if (Num(p.scale) != Num(plane_defaults.scale)) out += "scale = " + Num(p.scale) + "\n";
    if (p.start_seconds > 0.0f) out += "start_seconds = " + Num(p.start_seconds) + "\n";
    if (p.emit_seconds > 0.0f) out += "emit_seconds = " + Num(p.emit_seconds) + "\n";
    if (Num3(p.tint) != Num3(plane_defaults.tint)) out += "tint = " + Num3(p.tint) + "\n";
    out += DraftKeys(p.keys);
    std::string list;
    for (float v : p.verts) list += " " + Num(v);
    out += "verts =" + list + "\n";
    list.clear();
    for (float v : p.tverts) list += " " + Num(v);
    out += "tverts =" + list + "\n";
    list.clear();
    for (int f : p.faces) list += " " + std::to_string(f);
    out += "faces =" + list + "\n";
  }
  return out;
}

bool ReadVfxDraft(const std::string& text, VfxEffect* effect, std::string* error) {
  VfxEffect result;
  int emitter = -1, node = -1, light = -1, plane = -1;  // the section being read (indices: the lists grow)
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
        node = light = -1;
        result.emitters.back().name = Trim(inner.substr(7));
        if (result.emitters.back().name.empty()) result.emitters.back().name = "emitter";
        plane = -1;
      } else if (Lower(inner).rfind("plane", 0) == 0) {  // #161
        result.planes.emplace_back();
        plane = static_cast<int>(result.planes.size()) - 1;
        emitter = node = light = -1;
        result.planes.back().name = Trim(inner.substr(5));
        if (result.planes.back().name.empty()) result.planes.back().name = "plane";
      } else if (Lower(inner).rfind("node", 0) == 0) {
        result.nodes.emplace_back();
        node = static_cast<int>(result.nodes.size()) - 1;
        emitter = light = plane = -1;
        result.nodes.back().name = Trim(inner.substr(4));
        if (result.nodes.back().name.empty()) result.nodes.back().name = "node";
      } else if (Lower(inner).rfind("light", 0) == 0) {  // #145
        result.lights.emplace_back();
        light = static_cast<int>(result.lights.size()) - 1;
        emitter = node = plane = -1;
        result.lights.back().name = Trim(inner.substr(5));
        if (result.lights.back().name.empty()) result.lights.back().name = "light";
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
    if (plane >= 0) {  // #161
      VfxPlane& p = result.planes[plane];
      if (key == "parent") p.parent = value;
      else if (key == "position") ReadFloats(words, 0, p.position, 3);
      else if (key == "orientation") ReadFloats(words, 0, p.orientation, 4);
      else if (key == "bitmap") p.bitmap = VfxResRef(value, 64);
      else if (key == "diffuse") ReadFloats(words, 0, p.diffuse, 3);  // its material (the owner, 2026-11-16)
      else if (key == "ambient") ReadFloats(words, 0, p.ambient, 3);
      else if (key == "selfillum") ReadFloats(words, 0, p.selfillum, 3);
      else if (key == "alpha") p.alpha = ToFloat(value);
      else if (key == "render") p.render = Trim(value) != "0";
      else if (key == "transparencyhint") p.transparency_hint = ReadHint(value);  // every reader's rule
      else if (key == "mesh") p.mesh = VfxResRef(value, 16);  // #39, a mesh beat's (the bench's)
      else if (key == "scale") p.scale = ToFloat(value);
      else if (key == "start_seconds") p.start_seconds = std::min(10.0f, std::max(0.0f, ToFloat(value)));  // as core.js reads a light's
      else if (key == "emit_seconds") p.emit_seconds = std::max(0.0f, ToFloat(value));
      else if (key == "tint") ReadFloats(words, 0, p.tint, 3);  // M4, as its material's colours are read
      else if (key == "verts" || key == "tverts") {
        std::vector<float>& list = key == "verts" ? p.verts : p.tverts;
        list.clear();
        for (const std::string& w : words) list.push_back(ToFloat(w));
      } else if (key == "faces") {
        p.faces.clear();
        for (const std::string& w : words) p.faces.push_back(std::max(0, atoi(w.c_str())));
      } else DraftKeyLine(key, words, &p.keys);
      continue;
    }
    if (light >= 0) {  // #145
      VfxLight& l = result.lights[light];
      if (key == "parent") l.parent = value;
      else if (key == "position") ReadFloats(words, 0, l.position, 3);
      else if (key == "color") ReadFloats(words, 0, l.color, 3);
      else if (key == "radius") l.radius = ToFloat(value);
      else if (key == "brightness") l.multiplier = ToFloat(value);
      else if (key == "shadow") l.shadow = Trim(value) != "0";
      else if (key == "ambientonly") l.ambient_only = Trim(value) != "0";
      else if (key == "dynamic") l.dynamic = Trim(value) != "0";
      else if (key == "priority") l.priority = atoi(Trim(value).c_str());
      else if (key == "fading") l.fading = Trim(value) != "0";
      else DraftKeyLine(key, words, &l.keys);
      continue;
    }
    if (node >= 0) {
      VfxNode& n = result.nodes[node];
      if (key == "parent") n.parent = value;
      else if (key == "position") ReadFloats(words, 0, n.position, 3);
      else if (key == "orientation") ReadFloats(words, 0, n.orientation, 4);
      else if (key == "ref_model") n.ref_model = VfxResRef(value, 16);
      else DraftKeyLine(key, words, &n.keys);
      continue;
    }
    if (emitter < 0) {
      if (key == "name") result.name = value;
      else if (key == "type") result.duration = Lower(value) != "f";
      else if (key == "seconds") result.seconds = ToFloat(value);
      else if (key == "attach") result.attach = Lower(value);
      else if (key == "lengths") ReadFloats(words, 0, result.lengths, 3);
      else if (key.size() > 10 && key.compare(key.size() - 10, 10, "_transtime") == 0 && AnimSlot(key.substr(0, key.size() - 10)) >= 0) {
        result.transtime[AnimSlot(key.substr(0, key.size() - 10))] = TranstimeValue(ToFloat(value));  // KF: every reader's rule
      } else if (key == "shake") {
        float shake[3] = {0.0f, 0.0f, 0.0f};
        ReadFloats(words, 0, shake, 3);
        result.shake_type = shake[0] == 1.0f || shake[0] == 2.0f ? static_cast<int>(shake[0]) : 0;
        result.shake_delay = std::max(0.0f, shake[1]);
        result.shake_duration = std::max(0.0f, shake[2]);
      }
      continue;
    }
    VfxEmitter* e = &result.emitters[emitter];
    if (key == "update") e->update = value;
    else if (key == "render") e->render = value;
    else if (key == "blend") e->blend = value;
    else if (key == "texture") e->texture = value;
    else if (key == "chunk") e->chunk = VfxResRef(value, 16);
    else if (key == "parent") e->parent = value;
    else if (key == "position") ReadFloats(words, 0, e->position, 3);
    else if (key == "orientation") ReadFloats(words, 0, e->orientation, 4);
    else if (key == "birthrate") e->birthrate = ToFloat(value);
    else if (key == "emit_seconds") e->emit_seconds = ToFloat(value);
    else if (key == "start_seconds") e->start_seconds = std::min(10.0f, std::max(0.0f, ToFloat(value)));
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
    } else if (key == "area_shape") e->area_shape = value == "circle" ? 1 : 0;  // #175
    else if (key == "area_edge") e->area_edge = value == "fine" ? 1 : 0;
    else if (key == "grid") e->grid = std::max(1, atoi(value.c_str()));
    else if (key == "grid_down") e->grid_down = std::max(0, atoi(value.c_str()));
    else if (key == "frame_end") e->frame_end = std::max(0, atoi(value.c_str()));
    else if (key == "frame_start") e->frame_start = std::max(0, atoi(value.c_str()));  // past frame_end too: as written
    else if (key == "fps") e->fps = ToFloat(value);
    else if (key == "particle_rot") e->particle_rot = ToFloat(value);
    else if (key == "blur_length") e->blur_length = ToFloat(value);
    else if (key == "bounce_co") e->bounce_co = ToFloat(value);
    else if (key == "deadspace") e->deadspace = std::min(std::max(ToFloat(value), 0.0f), kVfxDeadspaceMax);
    else if (key == "blast_radius") e->blast_radius = BlastValue(ToFloat(value));  // every reader's rule
    else if (key == "blast_length") e->blast_length = BlastValue(ToFloat(value));
    else if (key == "p2p") e->p2p = Lower(value) == "bezier" || Lower(value) == "gravity" ? Lower(value) : std::string();
    else if (key == "p2p_bezier") {
      float bezier[2] = {e->p2p_bezier2, e->p2p_bezier3};
      ReadFloats(words, 0, bezier, 2);
      e->p2p_bezier2 = bezier[0];
      e->p2p_bezier3 = bezier[1];
    } else if (key == "combinetime") e->combinetime = ToFloat(value);
    else if (key == "grav") e->grav = ToFloat(value);
    else if (key == "drag") e->drag = ToFloat(value);
    else if (key == "threshold") e->threshold = ToFloat(value);
    else if (key == "lightning") {
      float bolt[3] = {e->lightning_delay, e->lightning_radius, e->lightning_scale};
      ReadFloats(words, 0, bolt, 3);
      e->lightning_delay = bolt[0];
      e->lightning_radius = bolt[1];
      e->lightning_scale = bolt[2];
    }
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
  // Frames down the same as across (either line first): 0, as both model readers have it - then it follows the grid.
  for (VfxEmitter& e : result.emitters) {
    if (e.grid_down == e.grid) e.grid_down = 0;
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
    std::string name, parent, kind, ref_model;
    float position[3] = {0, 0, 0};
    float orientation[4] = {0, 0, 0, 0};
    bool emitter = false;
    bool p2p = false, p2p_sel = false;  // an emitter's flags (read in any order)
    std::vector<VfxKeys> keys;
    // #145: a light node's own (its keys of colour and radius go in `keys`).
    bool light = false;
    float color[3] = {1, 1, 1};
    float radius = 5.0f, multiplier = 1.0f;
    bool shadow = false;
    bool ambient_only = false, dynamic = true, fading = true;
    int priority = 3;
    // #161: a plain trimesh node, kept whole as a plane.
    bool mesh = false;
    std::string bitmap;
    std::vector<float> verts, tverts;
    std::vector<int> faces;
    VfxPlane material;  // its diffuse, ambient, selfillumcolor, alpha and render (the owner, 2026-11-16), transparencyhint
  };
  std::string list_kind;  // a trimesh's verts / tverts / faces rows being read
  int list_rows = 0;
  std::vector<Geo> geo;
  std::map<std::string, size_t> geo_at;  // lower-case name -> index
  int current = -1;                      // the geometry node being read / keyed
  int emitter = -1;                      // its emitter, if it is one
  std::string key_name;
  int key_rows = 0;                      // rows still to come; -1: until "endlist"
  std::vector<std::vector<float>> rows;
  bool has_duration = false;
  // KF fix round 1: a model nwn5e wrote itself (kVfxModelMark at the start of a comment line before its newmodel: the
  // writer's, the generators', the Short Rest's) is read as before KF - its emitters' birthrate keys are nwn5e's own
  // emission (VA-2's restore, an emit time's, a start's, the cessation's off: read into its fields, below), its transtimes
  // the writer's rule's, its planes' keys none (the draft it came from is nwn5e's) - so its library\nwn5e draft keeps its
  // bytes and a later edit (an On for, a Drop in) is written by nwn5e's own rules again.
  bool own_model = false, before_model = true;
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
      } else if (key_name == "colorkey" || key_name == "radiuskey") {  // #145, a light's
        const size_t width = key_name == "colorkey" ? 4 : 2;
        VfxKeys* k = FindKeys(&geo[current].keys, anim, true);
        std::vector<float>& out = key_name == "colorkey" ? k->color : k->radius;
        for (const auto& row : rows) {
          if (row.size() < width) continue;
          out.insert(out.end(), row.begin(), row.begin() + width);
        }
      } else if (!own_model && geo[current].mesh && (key_name == "alphakey" || key_name == "scalekey")) {
        // KF: a mesh's own fade and scale (a plane's keys), each list as read.
        VfxKeys* k = FindKeys(&geo[current].keys, anim, true);
        std::vector<float>& out = key_name == "alphakey" ? k->alpha : k->scale;
        for (const auto& row : rows) {
          if (row.size() >= 2) out.insert(out.end(), row.begin(), row.begin() + 2);
        }
      } else if (keyed) {
        // KF: an emitter's own birthrate, alphaStart and alphaEnd keys, each list as read (beside what is read from them
        // below, as before).
        if (!own_model && (key_name == "birthratekey" || key_name == "alphastartkey" || key_name == "alphaendkey")) {
          VfxKeys* k = FindKeys(&geo[current].keys, anim, true);
          std::vector<float>& out = key_name == "birthratekey" ? k->birthrate : key_name == "alphastartkey" ? k->alpha_start : k->alpha_end;
          for (const auto& row : rows) {
            if (row.size() >= 2) out.insert(out.end(), row.begin(), row.begin() + 2);
          }
        }
        // #179 Phase 3: its colour and size keys the same way (t r g b, t s).
        if (!own_model && (key_name == "colorstartkey" || key_name == "colorendkey" || key_name == "sizestartkey" || key_name == "sizeendkey")) {
          VfxKeys* k = FindKeys(&geo[current].keys, anim, true);
          const bool colour = key_name == "colorstartkey" || key_name == "colorendkey";
          const size_t width = colour ? 4 : 2;
          std::vector<float>& out = key_name == "colorstartkey" ? k->color_start : key_name == "colorendkey" ? k->color_end
                                    : key_name == "sizestartkey" ? k->size_start : k->size_end;
          for (const auto& row : rows) {
            if (row.size() >= width) out.insert(out.end(), row.begin(), row.begin() + width);
          }
        }
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
    if (before_model) {  // its first comment lines: nwn5e's mark (KF fix round 1)
      const std::string line = Trim(raw);
      if (!line.empty() && line[0] == '#') own_model = own_model || line.rfind(kVfxModelMark, 0) == 0;
      else if (!line.empty()) before_model = false;
    }
    const std::vector<std::string> w = Words(raw.substr(0, raw.find('#')));
    if (w.empty()) continue;
    const std::string first = Lower(w[0]);
    if (list_rows > 0) {  // #161: a trimesh's rows
      if (current >= 0 && numeric(w[0])) {
        Geo& g = geo[current];
        if (list_kind == "verts" && w.size() >= 3) {
          for (int k = 0; k < 3; ++k) g.verts.push_back(ToFloat(w[k]));
        } else if (list_kind == "tverts" && w.size() >= 2) {
          g.tverts.push_back(ToFloat(w[0]));
          g.tverts.push_back(ToFloat(w[1]));
        } else if (list_kind == "faces" && w.size() >= 3) {
          for (int k = 0; k < 3; ++k) g.faces.push_back(std::max(0, atoi(w[k].c_str())));
        }
        --list_rows;
        continue;
      }
      list_rows = 0;  // a list cut short
    }
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
      const int slot = AnimSlot(anim);
      if (slot >= 0) result.lengths[slot] = ToFloat(w[1]);
    } else if (where == Where::kAnim && first == "transtime" && w.size() >= 2) {  // KF: every reader's rule
      const int slot = AnimSlot(anim);
      if (slot >= 0 && !own_model) result.transtime[slot] = TranstimeValue(ToFloat(w[1]));
    } else if (first == "node" && w.size() >= 3) {
      current = emitter = -1;
      const auto found = geo_at.find(Lower(w[2]));
      if (where == Where::kGeometry) {
        geo.emplace_back();
        geo.back().name = w[2];
        geo.back().kind = Lower(w[1]);
        current = static_cast<int>(geo.size()) - 1;
        geo_at.emplace(Lower(w[2]), geo.size() - 1);
        geo.back().light = Lower(w[1]) == "light";  // #145
        geo.back().mesh = Lower(w[1]) == "trimesh";  // #161
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
      else if (first == "refmodel") g.ref_model = w[1];
      if (g.mesh) {  // #161
        if (first == "bitmap") g.bitmap = Lower(w[1]) == "null" ? std::string() : VfxResRef(w[1], 64);
        else if (first == "diffuse") ReadFloats(w, 1, g.material.diffuse, 3);
        else if (first == "ambient") ReadFloats(w, 1, g.material.ambient, 3);
        else if (first == "selfillumcolor") ReadFloats(w, 1, g.material.selfillum, 3);
        else if (first == "alpha") g.material.alpha = ToFloat(w[1]);
        else if (first == "render") g.material.render = w[1] != "0";
        else if (first == "transparencyhint") g.material.transparency_hint = ReadHint(w[1]);  // every reader's rule
        else if (first == "verts" || first == "tverts" || first == "faces") {
          list_kind = first;
          list_rows = std::max(0, std::min(65536, atoi(w[1].c_str())));
        }
        continue;
      }
      if (g.light) {  // #145
        if (first == "color") ReadFloats(w, 1, g.color, 3);
        else if (first == "radius") g.radius = ToFloat(w[1]);
        else if (first == "multiplier") g.multiplier = ToFloat(w[1]);
        else if (first == "shadow") g.shadow = w[1] != "0";
        else if (first == "ambientonly") g.ambient_only = w[1] != "0";
        else if (first == "isdynamic" || first == "ndynamictype") g.dynamic = w[1] != "0";  // either spelling
        else if (first == "lightpriority") g.priority = atoi(w[1].c_str());
        else if (first == "fadinglight") g.fading = w[1] != "0";
      }
      if (emitter < 0) continue;
      const auto on = [](const std::string& v) { return v != "0" && Lower(v) != "false"; };
      if (first == "p2p") g.p2p = on(w[1]);
      else if (first == "p2p_sel") g.p2p_sel = on(w[1]);
      VfxEmitter* e = &result.emitters[emitter];
      const std::string& v = w[1];
      if (first == "update") e->update = v;
      else if (first == "render") e->render = v;
      else if (first == "blend") e->blend = v;
      else if (first == "texture") e->texture = v;
      else if (first == "chunkname") e->chunk = Lower(v) == "null" ? std::string() : VfxResRef(v, 16);
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
      else if (first == "ygrid") e->grid_down = std::max(0, atoi(v.c_str()));  // 0 when it's xgrid: after the read
      else if (first == "frameend") e->frame_end = std::max(0, atoi(v.c_str()));
      else if (first == "framestart") e->frame_start = std::max(0, atoi(v.c_str()));
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
      else if (first == "blastradius") e->blast_radius = BlastValue(ToFloat(v));  // every reader's rule
      else if (first == "blastlength") e->blast_length = BlastValue(ToFloat(v));
      else if (first == "p2p_bezier2") e->p2p_bezier2 = ToFloat(v);
      else if (first == "p2p_bezier3") e->p2p_bezier3 = ToFloat(v);
      else if (first == "combinetime") e->combinetime = ToFloat(v);
      else if (first == "grav") e->grav = ToFloat(v);
      else if (first == "drag") e->drag = ToFloat(v);
      else if (first == "threshold") e->threshold = ToFloat(v);
      else if (first == "lightningdelay") e->lightning_delay = ToFloat(v);
      else if (first == "lightningradius") e->lightning_radius = ToFloat(v);
      else if (first == "lightningscale") e->lightning_scale = ToFloat(v);
    }
  }
  if (key_rows != 0) finish_keys();
  // Frames down the same as across (in either order in the model): 0, as a draft has it - a square sheet stays default.
  for (VfxEmitter& e : result.emitters) {
    if (e.grid_down == e.grid) e.grid_down = 0;
  }
  bool meshes = false;
  for (const Geo& g : geo) meshes = meshes || (g.mesh && !g.verts.empty() && !g.faces.empty());
  if (result.emitters.empty() && !meshes) {
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
    if (!g.emitter && !g.light && !(g.mesh && !g.verts.empty())) continue;  // #145: a light's ancestors too; #161 a plane's
    std::string up = g.parent;
    for (int depth = 0; depth < 64 && !is_root(up) && geo_at.count(Lower(up)); ++depth) {
      const Geo& parent = geo[geo_at[Lower(up)]];
      if (!parent.emitter) keep.insert(Lower(parent.name));
      up = parent.parent;
    }
  }
  // A point-to-point or Lightning emitter's target: its first reference child (0.42.5 / 0.42.7),
  // kept under it.
  std::set<std::string> targets;
  for (const Geo& g : geo) {
    bool lightning = false;
    for (const VfxEmitter& e : result.emitters) {
      if (Lower(e.name) == Lower(g.name)) lightning = Lower(e.update) == "lightning";
    }
    if (!g.emitter || (!g.p2p && !lightning)) continue;
    for (const Geo& child : geo) {
      if (child.kind == "reference" && Lower(child.parent) == Lower(g.name)) {
        targets.insert(Lower(child.name));
        break;
      }
    }
  }
  for (const Geo& g : geo) {
    if (g.emitter) {
      for (VfxEmitter& e : result.emitters) {
        if (Lower(e.name) != Lower(g.name)) continue;
        e.parent = is_root(g.parent) ? std::string() : g.parent;
        e.keys = g.keys;
        if (g.p2p) e.p2p = g.p2p_sel ? "bezier" : "gravity";
      }
      continue;
    }
    if (g.light) {  // #145
      VfxLight l;
      l.name = g.name;
      l.parent = is_root(g.parent) ? std::string() : g.parent;
      memcpy(l.position, g.position, sizeof(l.position));
      memcpy(l.color, g.color, sizeof(l.color));
      l.radius = g.radius;
      l.multiplier = g.multiplier;
      l.shadow = g.shadow;
      l.ambient_only = g.ambient_only;
      l.dynamic = g.dynamic;
      l.priority = g.priority;
      l.fading = g.fading;
      l.keys = g.keys;
      result.lights.push_back(l);
      continue;
    }
    if (g.mesh && !g.verts.empty() && !g.faces.empty()) {  // #161: a mesh with geometry is a plane (and a node too when
      VfxPlane p;                                          // something hangs under it)
      p.name = g.name;
      p.parent = is_root(g.parent) ? std::string() : g.parent;
      memcpy(p.position, g.position, sizeof(p.position));
      memcpy(p.orientation, g.orientation, sizeof(p.orientation));
      p.bitmap = g.bitmap;
      p.verts = g.verts;
      p.tverts = g.tverts;
      p.faces = g.faces;
      memcpy(p.diffuse, g.material.diffuse, sizeof(p.diffuse));
      memcpy(p.ambient, g.material.ambient, sizeof(p.ambient));
      memcpy(p.selfillum, g.material.selfillum, sizeof(p.selfillum));
      p.alpha = g.material.alpha;
      p.render = g.material.render;
      p.transparency_hint = g.material.transparency_hint;
      if (!own_model) p.keys = g.keys;  // KF: its own move, turn, fade and scale in the animations (nwn5e's own: none, as before)
      result.planes.push_back(p);
    }
    const bool target = targets.count(Lower(g.name)) > 0;
    if (!keep.count(Lower(g.name)) && !target) continue;
    VfxNode n;
    n.name = g.name;
    n.parent = is_root(g.parent) ? std::string() : g.parent;
    if (target) n.ref_model = g.ref_model.empty() ? "fx_ref" : g.ref_model;
    memcpy(n.position, g.position, sizeof(n.position));
    memcpy(n.orientation, g.orientation, sizeof(n.orientation));
    n.keys = g.keys;
    for (VfxKeys& k : n.keys) {  // KF: a mesh's fade and scale are its plane's, not its node's
      k.alpha.clear();
      k.scale.clear();
    }
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
    case 132: e->frame_end = std::max(0, static_cast<int>(v(0))); break;  // as the ASCII and draft readers: never below 0
    case 136: e->frame_start = std::max(0, static_cast<int>(v(0))); break;  // frameStart (vim_protevil's OmenEmitter01: 3)
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
    case 120: e->combinetime = v(0); break;  // +0x94 (0.42.5: point to point)
    case 124: e->drag = v(0); break;         // +0x98
    case 140: e->grav = v(0); break;         // +0xa8
    case 152: e->p2p_bezier2 = v(0); break;  // +0xb4
    case 156: e->p2p_bezier3 = v(0); break;  // +0xb8
    case 188: e->threshold = v(0); break;    // +0xd8
    case 208: e->lightning_delay = v(0); break;   // +0xec (0.42.7)
    case 212: e->lightning_radius = v(0); break;  // +0xf0
    case 216: e->lightning_scale = v(0); break;   // +0xf4
    default: break;
  }
}

}  // namespace

bool ReadVfxBinaryModel(const std::string& bytes, VfxEffect* effect, std::string* error, bool any_mesh) {
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
      e.chunk = VfxResRef(m.Str(emitter + 0xB8, 16), 16);  // node +0x158 (0.42.6); a resref: any case
      e.grid = std::max<int>(1, static_cast<int>(m.U32(emitter + 0x0C)));
      // y grid beside it (the game's sheets that aren't square: head_01's 8 x 4, frames 0..31; vim_rayblack's 2 x 1, 0..1):
      // the frames down, 0 when it's the grid (as a draft has a square sheet) - or no sheet's: 0, or past 64 (the game's
      // own go to 8).
      const uint32_t ygrid = m.U32(emitter + 0x10);
      e.grid_down = ygrid == 0 || ygrid > 64 || static_cast<int>(ygrid) == e.grid ? 0 : static_cast<int>(ygrid);
      const uint32_t flags = m.U32(emitter + 0xD4);
      e.bounce = (flags & 0x010) != 0;
      e.random = (flags & 0x020) != 0;
      e.inherit = (flags & 0x040) != 0;
      e.splat = (flags & 0x200) != 0;
      for (const Controller& c : controllers) ApplyController(c, &e);
      e.inherit_local = (flags & 0x100) != 0;
      e.inherit_part = (flags & 0x400) != 0;
      e.deadspace = m.F32(emitter);                   // node +0xa0 in the game
      // Its blast beside it (the game's parser: blastRadius node +0xa4, blastLength +0xa8), by every reader's rule.
      e.blast_radius = BlastValue(m.F32(emitter + 0x04));
      e.blast_length = BlastValue(m.F32(emitter + 0x08));
      e.twosidedtex = m.U32(emitter + 0xC8) != 0;    // node +0x168
      if (flags & 0x1) e.p2p = (flags & 0x2) ? "bezier" : "gravity";  // p2p, p2p_sel
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
  // #161: plain mesh nodes (flags 0x21: header | mesh - not skin, anim, dangly or walkmesh), kept whole as planes. With
  // any_mesh (a chunk model; the owner, 2026-11-16) skin (0x40), animated (0x80) and dangly (0x100) meshes too - each
  // starts with the same mesh header, its own vertices the rest pose - but never a walkmesh (0x200).
  const auto is_mesh = [&](uint32_t flags) {
    return any_mesh ? (flags & 0x20) != 0 && (flags & 0x200) == 0 : (flags & ~0x1u) == 0x20;
  };
  std::vector<size_t> plane_nodes;
  for (const GeoNode& g : geometry) {
    if (g.node != root_node && is_mesh(m.U32(g.node + 0x5C)) && m.U16(g.node + 0x60 + 0x1C0) > 0 && m.U32(g.node + 0x60 + 0x0C) > 0) {
      plane_nodes.push_back(g.node);  // with vertices and faces
    }
  }
  if (result.emitters.empty() && plane_nodes.empty()) {
    if (error) *error = "no emitters found in that model";
    return false;
  }
  result.duration = bytes.find(std::string("duration", 9)) != std::string::npos;
  // The nodes the emitters hang under (their ancestors up to the root, which is the effect's
  // own node), parents first - the game moves and turns them (0.42.3).
  std::set<size_t> emitter_nodes(nodes.begin(), nodes.end()), ancestors;
  // #145: the lights (node flags 0x002 at +0x5C) - their ancestors are kept as the emitters' are.
  std::vector<size_t> light_nodes;
  for (const GeoNode& g : geometry) {
    if (g.node != root_node && (m.U32(g.node + 0x5C) & 0x002)) light_nodes.push_back(g.node);
  }
  std::vector<size_t> hung = nodes;
  hung.insert(hung.end(), light_nodes.begin(), light_nodes.end());
  hung.insert(hung.end(), plane_nodes.begin(), plane_nodes.end());  // #161
  for (size_t node : hung) {
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
  // #145: each light - where it sits, its colour (76), radius (88) and brightness (the multiplier, 140), its shadow
  // (+0xB0), as the game's own effect lights keep them (Task 0).
  for (size_t node : light_nodes) {
    const GeoNode& g = geometry[geo_at[node]];
    VfxLight l;
    l.name = g.name;
    l.parent = parent_name(g);
    memcpy(l.position, g.pos, sizeof(l.position));
    std::vector<Controller> controllers;
    if (ReadControllers(m, node, &controllers)) {
      for (const Controller& c : controllers) {
        if (c.type == 76 && c.columns == 3 && c.values.size() >= 3) memcpy(l.color, c.values.data(), sizeof(l.color));
        if (c.type == 88 && c.columns == 1 && !c.values.empty()) l.radius = c.values[0];
        if (c.type == 140 && c.columns == 1 && !c.values.empty()) l.multiplier = c.values[0];
      }
    }
    l.shadow = m.U32(node + 0xB0) != 0;
    l.priority = static_cast<int>(m.U32(node + 0xA0));  // the review: its flags kept, so it lights as it did
    l.ambient_only = m.U32(node + 0xA4) != 0;
    l.dynamic = m.U32(node + 0xA8) != 0;
    l.fading = m.U32(node + 0xB8) != 0;
    result.lights.push_back(l);
  }
  // #161: each plane - the mesh header at the node +0x60: its texture at +0x78 (64), faces at +0x08 (count +0x0C, 32
  // bytes each, the vertex indices at +26 +28 +30), the vertices in the raw data (the file header's +4, past it) at
  // +0x1BC (count, 16 bits, +0x1C0), the first UVs at +0x1C4 (0xFFFFFFFF: none). A mesh that runs past the file is skipped.
  // Its material (the owner, 2026-11-16; Torlack's binary MDL notes, the mesh header): diffuse at +0x3C, ambient at
  // +0x48 (3 floats each), render at +0x6C (0: not drawn); and the node's selfillumcolor (controller 100, 3 values) and
  // alpha (128) - a mesh's: on an emitter 128 is its fps, on a light 100 its vertical displacement. Its transparencyhint
  // (the owner, 2026-11-18) at +0x70, an integer (Torlack's TransparencyHint; the game's parser keeps it beside shadow,
  // beaming and render, in the compiled header's order - over 581 textures used by both the game's ASCII and compiled
  // meshes, a non-zero hint at +0x70 goes with the ASCII ones' in 560).
  const size_t raw = kModelBase + m.U32(4);
  for (size_t node : plane_nodes) {
    const GeoNode& g = geometry[geo_at[node]];
    const size_t mh = node + 0x60;
    VfxPlane p;
    p.name = g.name;
    p.parent = parent_name(g);
    memcpy(p.position, g.pos, sizeof(p.position));
    float turn[4];
    AxisAngle(g.rot, turn);
    if (turn[3] > 1e-4f && (turn[0] || turn[1] || turn[2])) memcpy(p.orientation, turn, sizeof(turn));
    p.bitmap = VfxResRef(m.Str(mh + 0x78, 64), 64);
    p.flags = m.U32(node + 0x5C);
    for (int k = 0; k < 3; ++k) {
      p.diffuse[k] = m.F32(mh + 0x3C + 4 * k);
      p.ambient[k] = m.F32(mh + 0x48 + 4 * k);
    }
    p.render = m.U32(mh + 0x6C) != 0;
    p.transparency_hint = HintValue(static_cast<int32_t>(m.U32(mh + 0x70)));  // every reader's rule (signed: -1 is 0)
    std::vector<Controller> controllers;
    if (ReadControllers(m, node, &controllers)) {
      for (const Controller& c : controllers) {
        if (c.type == 100 && c.columns == 3 && c.values.size() >= 3) memcpy(p.selfillum, c.values.data(), sizeof(p.selfillum));
        if (c.type == 128 && c.columns == 1 && !c.values.empty()) p.alpha = c.values[0];
      }
    }
    const uint32_t v_at = m.U32(mh + 0x1BC), uv_at = m.U32(mh + 0x1C4), f_at = m.U32(mh + 0x08), f_count = m.U32(mh + 0x0C);
    const uint16_t v_count = m.U16(mh + 0x1C0);
    if (raw + v_at + 12ull * v_count > bytes.size() || f_count > 65536 || kModelBase + f_at + 32ull * f_count > bytes.size()) continue;
    for (uint32_t v = 0; v < 3u * v_count; ++v) p.verts.push_back(m.F32(raw + v_at + 4ull * v));
    if (uv_at != 0xFFFFFFFFu && raw + uv_at + 8ull * v_count <= bytes.size()) {
      for (uint32_t v = 0; v < 2u * v_count; ++v) p.tverts.push_back(m.F32(raw + uv_at + 4ull * v));
    }
    for (uint32_t f = 0; f < f_count; ++f) {
      const size_t at = kModelBase + f_at + 32ull * f;
      const uint16_t a = m.U16(at + 26), b = m.U16(at + 28), c = m.U16(at + 30);
      if (a >= v_count || b >= v_count || c >= v_count) continue;
      p.faces.insert(p.faces.end(), {a, b, c});
    }
    result.planes.push_back(p);
  }
  // A point-to-point emitter's target: its first child that is a reference node (header flags
  // 0x10; the game's SetEmitterTarget takes the first child with a reference object), kept
  // under the emitter with its reference model (0.42.5).
  for (size_t i = 0; i < result.emitters.size() && i < nodes.size(); ++i) {
    const bool lightning = Lower(result.emitters[i].update) == "lightning";
    if ((result.emitters[i].p2p.empty() && !lightning) || !geo_at.count(nodes[i])) continue;
    for (const GeoNode& g : geometry) {
      if (g.parent != nodes[i] || !(m.U32(g.node + 0x5C) & 0x10)) continue;
      VfxNode n;
      n.name = g.name;
      n.parent = result.emitters[i].name;
      memcpy(n.position, g.pos, sizeof(n.position));
      float turn[4];
      AxisAngle(g.rot, turn);
      if (turn[3] > 1e-4f && (turn[0] || turn[1] || turn[2])) memcpy(n.orientation, turn, sizeof(turn));
      n.ref_model = m.Str(g.node + 0x60, 64);
      if (n.ref_model.empty()) n.ref_model = "fx_ref";
      result.nodes.push_back(n);
      break;
    }
  }
  // The animations (their 64-byte name at the header's +8, the root node at +0x48 as the geometry's, the length at +0x70)
  // and the nodes of each. KF fix round 1: the model header's own list of them (+0x78: their headers' offsets, +0x7C their
  // count - as the game keeps them; anim_recipe.cpp's ReadAnimClip reads the same), the first of each name - no longer the
  // first "\0impact\0" in the file with a length in (0, 60), which took a node named impact for one (c_air's, a_gnome's:
  // garbage transtimes) and missed a real one 0 s long (vps_bugs' impact, the game's transtime 0). Such a one is read - its
  // transtime and its keys - its length not (0: the writer's own, as before). A model with no list has none.
  std::vector<std::pair<std::string, std::vector<size_t>>> anims;
  size_t impact_header = 0;  // the listed impact's, for a one-shot's seconds (below)
  const uint32_t anim_list = m.U32(kModelBase + 0x78), anim_count = m.U32(kModelBase + 0x7C);
  if (anim_count <= 4096 && kModelBase + anim_list + 4ull * anim_count <= bytes.size()) {
    for (const char* name : {"impact", "duration", "cessation"}) {
      for (uint32_t i = 0; i < anim_count; ++i) {
        const size_t header = kModelBase + m.U32(kModelBase + anim_list + 4ull * i);
        if (header + 0x80 > bytes.size() || Lower(m.Str(header + 8, 64)) != name) continue;
        const float length = m.F32(header + 0x70);
        if (!(length >= 0.0f && length < 60.0f)) continue;
        anims.emplace_back(name, TreeNodes(m, m.U32(header + 0x48)));
        if (length > 0.0f) result.lengths[AnimSlot(name)] = length;
        result.transtime[AnimSlot(name)] = TranstimeValue(m.F32(header + 0x74));  // KF: after its length, every reader's rule
        if (AnimSlot(name) == 0) impact_header = header;
        break;
      }
    }
  }
  // Each animation's keys of those nodes and of the emitters: where they move (8: t x y z)
  // and how they turn (20: t + x y z w, kept as axis-angle). A single key equal to the
  // geometry's is no motion and isn't kept. KF: an emitter's own birthrate (88), alphaStart (84) and alphaEnd (80), and a
  // plane's move and turn (as a node's), alpha (128) and scale (36) - each of those scalar lists as read, a single row
  // too (a value the animation sets).
  for (const auto& a : anims) {
    for (const size_t copy : a.second) {
      const std::string name = Lower(m.Str(copy + 0x10, 32));
      const auto g = geo_by_name.find(name);
      if (g == geo_by_name.end()) continue;
      std::vector<VfxKeys>* keys = nullptr;
      for (VfxNode& n : result.nodes) {
        if (Lower(n.name) == name) keys = &n.keys;
      }
      VfxEmitter* emitting = nullptr;  // KF
      for (VfxEmitter& e : result.emitters) {
        if (Lower(e.name) == name) {
          keys = &e.keys;
          emitting = &e;
        }
      }
      const VfxLight* lit = nullptr;  // #145
      for (VfxLight& l : result.lights) {
        if (Lower(l.name) == name) {
          keys = &l.keys;
          lit = &l;
          emitting = nullptr;
        }
      }
      VfxPlane* plane = nullptr;  // KF: a mesh's own keys (it may be a node too: its move and turn are kept for both)
      for (VfxPlane& p : result.planes) {
        if (Lower(p.name) == name) plane = &p;
      }
      std::vector<Controller> controllers;
      if ((!keys && !plane) || !ReadControllers(m, copy, &controllers)) continue;
      const GeoNode& base = geometry[g->second];
      // A scalar list (t v rows) into `out` of this animation's keys of `list`.
      const auto scalar = [&](std::vector<VfxKeys>* list, std::vector<float> VfxKeys::*field, const Controller& c) {
        std::vector<float>& out = FindKeys(list, a.first, true)->*field;
        for (size_t r = 0; r < c.times.size(); ++r) {
          out.push_back(c.times[r]);
          out.push_back(c.values[r]);
        }
      };
      for (const Controller& c : controllers) {
        const size_t rows = c.times.size();
        if (plane && c.columns == 1 && c.values.size() >= rows && (c.type == 128 || c.type == 36)) {
          scalar(&plane->keys, c.type == 128 ? &VfxKeys::alpha : &VfxKeys::scale, c);
          continue;
        }
        if (emitting && c.columns == 1 && c.values.size() >= rows && (c.type == 88 || c.type == 84 || c.type == 80)) {
          scalar(&emitting->keys, c.type == 88 ? &VfxKeys::birthrate : c.type == 84 ? &VfxKeys::alpha_start : &VfxKeys::alpha_end, c);
          continue;
        }
        // #179 Phase 3: its size (168 at birth, 172 at the end: one column) and colour (108, 96: three) keys, as read.
        if (emitting && c.columns == 1 && c.values.size() >= rows && (c.type == 168 || c.type == 172)) {
          scalar(&emitting->keys, c.type == 168 ? &VfxKeys::size_start : &VfxKeys::size_end, c);
          continue;
        }
        if (emitting && c.columns == 3 && c.values.size() >= rows * 3 && (c.type == 108 || c.type == 96)) {
          std::vector<float>& out = FindKeys(&emitting->keys, a.first, true)->*(c.type == 108 ? &VfxKeys::color_start : &VfxKeys::color_end);
          for (size_t r = 0; r < rows; ++r) {
            out.push_back(c.times[r]);
            out.insert(out.end(), c.values.begin() + r * 3, c.values.begin() + r * 3 + 3);
          }
          continue;
        }
        if (plane && (c.type == 8 || c.type == 20)) {  // its move and turn, as a node's below (a single row at rest: none)
          const bool turn = c.type == 20;
          const size_t width = turn ? 4 : 3;
          if (c.columns == static_cast<int>(width) && c.values.size() >= rows * width) {
            bool moves = rows >= 2;
            for (size_t k = 0; k < width && !moves; ++k) moves = std::fabs(c.values[k] - (turn ? base.rot[k] : base.pos[k])) > 1e-4f;
            if (moves) {
              std::vector<float>& out = turn ? FindKeys(&plane->keys, a.first, true)->orientation : FindKeys(&plane->keys, a.first, true)->position;
              for (size_t r = 0; r < rows; ++r) {
                out.push_back(c.times[r]);
                if (turn) {
                  float axis_angle[4];
                  AxisAngle(&c.values[r * 4], axis_angle);
                  out.insert(out.end(), axis_angle, axis_angle + 4);
                } else {
                  out.insert(out.end(), c.values.begin() + r * 3, c.values.begin() + r * 3 + 3);
                }
              }
            }
          }
        }
        if (!keys) continue;
        if (c.type == 8 && c.columns == 3 && c.values.size() >= rows * 3) {
          bool moves = rows >= 2;
          for (int k = 0; k < 3 && !moves; ++k) moves = std::fabs(c.values[k] - base.pos[k]) > 1e-4f;
          if (!moves) continue;
          std::vector<float>& out = FindKeys(keys, a.first, true)->position;
          for (size_t r = 0; r < rows; ++r) {
            out.push_back(c.times[r]);
            for (int k = 0; k < 3; ++k) out.push_back(c.values[r * 3 + k]);
          }
        } else if (lit && c.type == 76 && c.columns == 3 && c.values.size() >= rows * 3) {  // #145: a light's colour
          bool shifts = rows >= 2;
          for (int k = 0; k < 3 && !shifts; ++k) shifts = std::fabs(c.values[k] - lit->color[k]) > 1e-4f;
          if (!shifts) continue;
          std::vector<float>& out = FindKeys(keys, a.first, true)->color;
          for (size_t r = 0; r < rows; ++r) {
            out.push_back(c.times[r]);
            for (int k = 0; k < 3; ++k) out.push_back(c.values[r * 3 + k]);
          }
        } else if (lit && c.type == 88 && c.columns == 1 && c.values.size() >= rows) {  // #145: its radius
          if (rows < 2 && std::fabs(c.values[0] - lit->radius) <= 1e-4f) continue;
          std::vector<float>& out = FindKeys(keys, a.first, true)->radius;
          for (size_t r = 0; r < rows; ++r) {
            out.push_back(c.times[r]);
            out.push_back(c.values[r]);
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
  // The impact animation's length (its header: the 64-byte name at +8, the length at +0x70): how long an always-on
  // emitter of a one-shot effect runs - the listed impact's (KF fix round 1), when it has one.
  if (impact_header) {
    const float length = m.F32(impact_header + 0x70);
    if (length > 0.0f && length < 60.0f && !result.duration) result.seconds = length;
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

namespace {

// --- One burst (LD): a model's animations and their events.
constexpr size_t kEventSize = 36;     // a compiled event: its time (f32), then a 32-byte name
constexpr uint32_t kListMost = 4096;  // the most animations or events a list may have, as ReadVfxBinaryModel takes one

// Compiled: its first 4 bytes 0 (an ASCII model starts with text), as every reader tells them.
bool IsCompiled(const std::string& bytes) { return bytes.size() >= 4 && Bytes{bytes}.U32(0) == 0; }

bool ReadCompiledAnimations(const std::string& bytes, std::vector<ModelAnimation>* anims, std::string* error) {
  const Bytes m{bytes};
  if (bytes.size() < kModelBase + 0x80) {
    *error = "too short for a compiled model";
    return false;
  }
  const uint32_t list = m.U32(kModelBase + 0x78), count = m.U32(kModelBase + 0x7C);
  if (count > kListMost || kModelBase + list + 4ull * count > bytes.size()) {
    *error = "its animations' list runs past the file";
    return false;
  }
  for (uint32_t i = 0; i < count; ++i) {
    ModelAnimation a;
    a.header = kModelBase + m.U32(kModelBase + list + 4ull * i);
    if (a.header + 0xC4 > bytes.size()) {
      *error = "animation " + std::to_string(i) + "'s header runs past the file";
      return false;
    }
    a.name = m.Str(a.header + 8, 64);
    a.length = m.F32(a.header + 0x70);
    const uint32_t at = m.U32(a.header + 0xB8), n = m.U32(a.header + 0xBC);
    if (n > 0) {
      a.event_at = kModelBase + at;
      if (n > kListMost || a.event_at + kEventSize * n > bytes.size()) {
        *error = "the events of " + (a.name.empty() ? "animation " + std::to_string(i) : a.name) + " run past the file";
        return false;
      }
      for (uint32_t e = 0; e < n; ++e) a.events.push_back({m.F32(a.event_at + kEventSize * e), m.Str(a.event_at + kEventSize * e + 4, 32)});
    }
    anims->push_back(a);
  }
  return true;
}

// An ASCII model's lines, each with its own line end.
std::vector<std::string> LinesOf(const std::string& text) {
  std::vector<std::string> lines;
  for (size_t at = 0; at < text.size();) {
    const size_t end = text.find('\n', at);
    const size_t next = end == std::string::npos ? text.size() : end + 1;
    lines.push_back(text.substr(at, next - at));
    at = next;
  }
  return lines;
}

// Where an ASCII model's event line is: its line, its animation, its place among that animation's events.
struct EventLine {
  size_t line, anim, event;
};

// An ASCII model's newanim blocks: each one's name, its length and event lines (an animation's own, not a node's - between
// node and endnode), and where each event line is.
bool ReadAsciiAnimations(const std::vector<std::string>& lines, std::vector<ModelAnimation>* anims, std::vector<EventLine>* where,
                         std::string* error) {
  bool model = false;
  long inside = -1;
  int depth = 0;
  for (size_t i = 0; i < lines.size(); ++i) {
    const std::vector<std::string> w = Words(lines[i]);
    if (w.empty() || w[0][0] == '#') continue;
    const std::string key = Lower(w[0]);
    if (key == "newmodel") {
      model = true;
    } else if (key == "newanim") {
      anims->push_back(ModelAnimation());
      anims->back().name = w.size() > 1 ? w[1] : "";
      inside = static_cast<long>(anims->size()) - 1;
      depth = 0;
    } else if (key == "doneanim") {
      inside = -1;
    } else if (inside < 0) {
      continue;
    } else if (key == "node") {
      ++depth;
    } else if (key == "endnode") {
      depth = std::max(0, depth - 1);
    } else if (depth == 0 && key == "length" && w.size() > 1) {
      (*anims)[inside].length = ToFloat(w[1]);
    } else if (depth == 0 && key == "event" && w.size() > 2) {
      ModelAnimation& a = (*anims)[inside];
      where->push_back({i, static_cast<size_t>(inside), a.events.size()});
      a.events.push_back({ToFloat(w[1]), w[2]});
    }
  }
  if (!model) {
    *error = "not a model: neither compiled nor an ASCII one's newmodel";
    return false;
  }
  return true;
}

}  // namespace

bool ReadModelAnimations(const std::string& bytes, std::vector<ModelAnimation>* anims, std::string* error) {
  anims->clear();
  error->clear();
  std::vector<EventLine> where;
  const bool ok = IsCompiled(bytes) ? ReadCompiledAnimations(bytes, anims, error) : ReadAsciiAnimations(LinesOf(bytes), anims, &where, error);
  if (!ok) anims->clear();
  return ok;
}

int DropAnimationEvents(std::string* bytes, const std::string& anim, const std::string& event) {
  if (!bytes) return 0;
  std::vector<ModelAnimation> anims;
  std::string error;
  const std::string want_anim = Lower(anim), want_event = Lower(event);
  int dropped = 0;
  if (IsCompiled(*bytes)) {
    if (!ReadCompiledAnimations(*bytes, &anims, &error)) return 0;
    std::set<size_t> done;  // a header listed twice is changed once
    for (const ModelAnimation& a : anims) {
      if (Lower(a.name) != want_anim || a.events.empty() || !done.insert(a.header).second) continue;
      std::vector<std::string> kept;
      for (size_t e = 0; e < a.events.size(); ++e) {
        if (Lower(a.events[e].name) != want_event) kept.push_back(bytes->substr(a.event_at + kEventSize * e, kEventSize));
      }
      const size_t gone = a.events.size() - kept.size();
      if (gone == 0) continue;
      for (size_t e = 0; e < a.events.size(); ++e) {
        bytes->replace(a.event_at + kEventSize * e, kEventSize, e < kept.size() ? kept[e] : std::string(kEventSize, '\0'));
      }
      const uint32_t count = static_cast<uint32_t>(kept.size());
      const uint32_t offset = kept.empty() ? 0 : static_cast<uint32_t>(a.event_at - kModelBase);
      memcpy(&(*bytes)[a.header + 0xB8], &offset, 4);
      memcpy(&(*bytes)[a.header + 0xBC], &count, 4);
      memcpy(&(*bytes)[a.header + 0xC0], &count, 4);
      dropped += static_cast<int>(gone);
    }
    return dropped;
  }
  const std::vector<std::string> lines = LinesOf(*bytes);
  std::vector<EventLine> where;
  if (!ReadAsciiAnimations(lines, &anims, &where, &error)) return 0;
  std::set<size_t> drop;
  for (const EventLine& at : where) {
    if (Lower(anims[at.anim].name) == want_anim && Lower(anims[at.anim].events[at.event].name) == want_event) drop.insert(at.line);
  }
  if (drop.empty()) return 0;
  std::string out;
  out.reserve(bytes->size());
  for (size_t i = 0; i < lines.size(); ++i) {
    if (!drop.count(i)) out += lines[i];
  }
  *bytes = out;
  return static_cast<int>(drop.size());
}

// --- A spell's masked copy (#184).
std::vector<EmitterTexture> ReadEmitterTextures(const std::string& bytes) {
  std::vector<EmitterTexture> out;
  if (!IsCompiled(bytes) || bytes.size() < 0x200) return out;
  const Bytes m{bytes};
  std::vector<std::pair<size_t, std::string>> found;  // the update word's place, the word
  for (const char* word : {"Fountain", "Single", "Explosion", "Lightning"}) {
    const std::string needle(word, strlen(word) + 1);
    for (size_t at = bytes.find(needle); at != std::string::npos; at = bytes.find(needle, at + 1)) {
      if (at < 0x18 + 0x60 + kModelBase || bytes[at - 1] != 0) continue;
      found.emplace_back(at, word);
    }
  }
  std::sort(found.begin(), found.end());
  for (const auto& f : found) {
    const size_t emitter = f.first - 0x18, node = emitter - 0x60;
    if (emitter + 0xB8 > bytes.size()) continue;
    EmitterTexture e;
    e.node = m.Str(node + 0x10, 32);
    if (e.node.empty() || m.Str(emitter + 0x38, 32).empty()) continue;
    e.update = f.second;
    e.texture = m.Str(emitter + 0x78, 64);
    e.at = emitter + 0x78;
    out.push_back(e);
  }
  return out;
}

int MaskedModelCopy(std::string* bytes, const std::string& name, const std::map<std::string, std::string>& materials,
                    std::string* why) {
  std::string reason;
  if (!why) why = &reason;
  why->clear();
  if (!bytes || !IsCompiled(*bytes) || bytes->size() < 0x200) {
    *why = "not a compiled model";
    return -1;
  }
  const Bytes m{*bytes};
  const std::string own = m.Str(kModelBase + 8, 64);
  if (own.empty() || own.size() != name.size()) {
    *why = "its name \"" + own + "\" isn't as long as \"" + name + "\"";
    return -1;
  }
  for (const auto& material : materials) {
    if (material.second.empty() || material.second.size() > 31) {
      *why = "the material \"" + material.second + "\" isn't 1 to 31 characters";
      return -1;
    }
  }
  // The name's places: the model header's, the root node's, each animation's root and its root node's.
  std::vector<std::pair<size_t, size_t>> fields = {{kModelBase + 8, 64}};
  const uint32_t root = m.U32(kModelBase + 0x48);
  if (root == 0 || kModelBase + root + 0x40 > bytes->size()) {
    *why = "no root node";
    return -1;
  }
  fields.push_back({kModelBase + root + 0x20, 32});
  std::vector<ModelAnimation> anims;
  std::string error;
  if (!ReadCompiledAnimations(*bytes, &anims, &error)) {
    *why = error;
    return -1;
  }
  for (const ModelAnimation& a : anims) {
    fields.push_back({a.header + 0x78, 64});
    const uint32_t anim_root = m.U32(a.header + 0x48);
    if (anim_root == 0 || kModelBase + anim_root + 0x40 > bytes->size()) {
      *why = "the animation \"" + a.name + "\" has no root node";
      return -1;
    }
    fields.push_back({kModelBase + anim_root + 0x20, 32});
  }
  for (const auto& field : fields) {
    if (m.Str(field.first, field.second) != own) {
      *why = "a name field at " + std::to_string(field.first) + " isn't \"" + own + "\"";
      return -1;
    }
  }
  const std::string needle = own + std::string(1, '\0');
  int places = 0;
  for (size_t at = bytes->find(needle); at != std::string::npos; at = bytes->find(needle, at + 1)) ++places;
  std::set<size_t> distinct;
  for (const auto& field : fields) distinct.insert(field.first);
  if (places != static_cast<int>(distinct.size())) {
    *why = "\"" + own + "\" is in " + std::to_string(places) + " places, not the " + std::to_string(distinct.size()) +
           " a model keeps it in";
    return -1;
  }
  int changed = 0;
  for (const EmitterTexture& e : ReadEmitterTextures(*bytes)) {
    const auto found = materials.find(Lower(e.texture));
    if (found == materials.end()) continue;
    bytes->replace(e.at, 64, std::string(64, '\0'));
    bytes->replace(e.at, found->second.size(), found->second);
    ++changed;
  }
  for (size_t at : distinct) bytes->replace(at, name.size(), name);
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
