// DH: see anim_death_debris.h.

#include "anim_death_debris.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "vfx_model.h"

namespace data {
namespace {

constexpr size_t kModelBase = 12;       // a compiled model's offsets count from the end of its 12-byte file header
constexpr size_t kSupermodelAt = 0xA8;  // its supermodel's name in the model header (64 bytes)
constexpr size_t kGeometryAt = 0x48;    // its geometry's root node (as an animation header's own tree, +0x48)
constexpr uint32_t kBirthrate = 88;     // an emitter's birthrate controller
// Models in a supermodel chain, the model's own counted: nwn5e's own cap (FindAnimClip's rule). The engine's
// FindAnimation has none - at the chain's end it falls back to the last model's "default" - but no game chain is deeper
// than 2, and no "default" there detonates.
constexpr int kChainMost = 8;

std::string Lower(std::string text) {
  for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return text;
}

std::vector<std::string> Words(const std::string& line) {
  std::vector<std::string> words;
  size_t at = 0;
  while (at < line.size()) {
    while (at < line.size() && isspace(static_cast<unsigned char>(line[at]))) ++at;
    const size_t start = at;
    while (at < line.size() && !isspace(static_cast<unsigned char>(line[at]))) ++at;
    if (at > start) words.push_back(line.substr(start, at - start));
  }
  return words;
}

// Compiled: its first 4 bytes 0 (an ASCII model starts with text), as every reader tells them.
bool IsCompiled(const std::string& bytes) {
  return bytes.size() >= 4 && bytes[0] == '\0' && bytes[1] == '\0' && bytes[2] == '\0' && bytes[3] == '\0';
}

// --- A compiled model, bounds-checked (0 / "" past the file).
uint32_t U32(const std::string& b, size_t at) {
  uint32_t v = 0;
  if (at + 4 <= b.size()) memcpy(&v, b.data() + at, 4);
  return v;
}
uint16_t U16(const std::string& b, size_t at) {
  uint16_t v = 0;
  if (at + 2 <= b.size()) memcpy(&v, b.data() + at, 2);
  return v;
}
float F32(const std::string& b, size_t at) {
  float v = 0.0f;
  if (at + 4 <= b.size()) memcpy(&v, b.data() + at, 4);
  return v;
}
// A NUL-ended printable string of at most `max` bytes ("" if not one) - the VFX reader's rule, so names compare alike.
std::string Str(const std::string& b, size_t at, size_t max) {
  std::string out;
  for (size_t i = 0; i < max && at + i < b.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(b[at + i]);
    if (c == 0) return out;
    if (c < 32 || c > 126) return std::string();
    out += static_cast<char>(c);
  }
  return std::string();
}

// A node tree from its root (an offset from the model base): each node in the readers' terms (its header + 0x10: the
// name at +0x10, the children - header offsets - at +0x38, their count +0x3C). 4,096 nodes at most, each once.
std::vector<size_t> Tree(const std::string& b, uint32_t root) {
  std::vector<size_t> nodes;
  if (root == 0) return nodes;
  std::vector<size_t> stack = {kModelBase + root + 0x10};
  std::set<size_t> seen;
  while (!stack.empty() && nodes.size() < 4096) {
    const size_t node = stack.back();
    stack.pop_back();
    if (node + 0x60 > b.size() || !seen.insert(node).second) continue;
    nodes.push_back(node);
    const uint32_t children = U32(b, node + 0x38), count = U32(b, node + 0x3C);
    if (count > 1024 || kModelBase + children + 4ull * count > b.size()) continue;
    for (uint32_t i = 0; i < count; ++i) stack.push_back(kModelBase + U32(b, kModelBase + children + 4ull * i) + 0x10);
  }
  return nodes;
}

// A node's controller `type`: its rows' first values (none when it hasn't one, or its arrays don't fit). Its keys at
// +0x44 (count +0x48), 12 bytes each - type, rows (+4), the first time's and value's index (+6, +8), columns (+10) - its
// values at +0x50 (count +0x54), offsets from the model base.
std::vector<float> Controller(const std::string& b, size_t node, uint32_t type) {
  const uint32_t keys = U32(b, node + 0x44), key_count = U32(b, node + 0x48);
  const uint32_t data = U32(b, node + 0x50), data_count = U32(b, node + 0x54);
  if (key_count == 0 || key_count > 96 || data_count > 65536) return {};
  if (kModelBase + keys + 12ull * key_count > b.size() || kModelBase + data + 4ull * data_count > b.size()) return {};
  for (uint32_t k = 0; k < key_count; ++k) {
    const size_t at = kModelBase + keys + 12ull * k;
    if (U32(b, at) != type) continue;
    const uint32_t rows = U16(b, at + 4), value_at = U16(b, at + 8), columns = static_cast<uint8_t>(b[at + 10]) & 0x0F;
    if (rows == 0 || columns == 0 || value_at + rows * columns > data_count) return {};
    std::vector<float> values;
    for (uint32_t r = 0; r < rows; ++r) values.push_back(F32(b, kModelBase + data + 4ull * (value_at + r * columns)));
    return values;
  }
  return {};
}

bool AboveZero(const std::vector<float>& values) {
  for (float v : values) {
    if (v > 0.0f) return true;
  }
  return false;
}

// A compiled model's emitter's geometry birthrate (its node by name, any case): the controller's first value; 0 without.
float CompiledBirthrate(const std::string& b, const std::string& lower_name) {
  for (size_t node : Tree(b, U32(b, kModelBase + kGeometryAt))) {
    if (Lower(Str(b, node + 0x10, 32)) != lower_name) continue;
    const std::vector<float> values = Controller(b, node, kBirthrate);
    return values.empty() ? 0.0f : values[0];
  }
  return 0.0f;
}

// The nodes a compiled animation (its header's offset in the file) keys the birthrate of above 0, lower case.
std::set<std::string> CompiledBirths(const std::string& b, size_t header) {
  std::set<std::string> births;
  for (size_t node : Tree(b, U32(b, header + 0x48))) {
    if (AboveZero(Controller(b, node, kBirthrate))) births.insert(Lower(Str(b, node + 0x10, 32)));
  }
  return births;
}

// The nodes the first newanim block of `anim` (any case) in an ASCII model keys the birthrate of above 0: a
// "birthratekey" line in a node's lines ("node <kind> <name>" .. "endnode"), its rows ("t v") counted on the line or
// running to an "endlist".
std::set<std::string> AsciiBirths(const std::string& text, const char* anim) {
  std::set<std::string> births;
  bool inside = false, done = false;
  std::string node;
  int rows = 0;  // rows of a birthratekey still to read; -1: to its endlist
  for (size_t at = 0; at < text.size() && !done;) {
    size_t end = text.find('\n', at);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(at, end - at);
    line = line.substr(0, line.find('#'));
    const std::vector<std::string> w = Words(line);
    at = end + 1;
    if (w.empty()) continue;
    const std::string key = Lower(w[0]);
    if (rows != 0) {
      const bool number = isdigit(static_cast<unsigned char>(w[0][0])) || w[0][0] == '-' || w[0][0] == '.';
      if (number) {
        if (w.size() >= 2 && atof(w[1].c_str()) > 0.0) births.insert(node);
        if (rows > 0) --rows;
        continue;
      }
      rows = 0;
      if (key == "endlist") continue;
    }
    if (key == "newanim") {
      if (inside) done = true;
      inside = !done && w.size() >= 2 && Lower(w[1]) == anim;
      node.clear();
    } else if (!inside) {
      continue;
    } else if (key == "doneanim") {
      done = true;
    } else if (key == "node" && w.size() >= 3) {
      node = Lower(w[2]);
    } else if (key == "endnode") {
      node.clear();
    } else if (key == "birthratekey" && !node.empty()) {
      rows = w.size() >= 2 ? std::max(0, std::min(65536, atoi(w[1].c_str()))) : -1;
    }
  }
  return births;
}

// Its supermodel, lower case: a compiled model's header name (printable, NUL-ended), an ASCII model's first
// "setsupermodel <model> <supermodel>" line; "" for none ("NULL") or one that can't be read.
std::string Supermodel(const std::string& bytes) {
  std::string name;
  if (IsCompiled(bytes)) {
    const size_t at = kModelBase + kSupermodelAt;
    for (size_t i = 0; i < 64 && at + i < bytes.size(); ++i) {
      const unsigned char c = static_cast<unsigned char>(bytes[at + i]);
      if (c == 0) break;
      if (c < 32 || c > 126) return std::string();
      name += static_cast<char>(c);
    }
  } else {
    for (size_t at = 0; at < bytes.size();) {
      size_t end = bytes.find('\n', at);
      if (end == std::string::npos) end = bytes.size();
      const std::string line = bytes.substr(at, end - at);
      at = end + 1;
      const std::vector<std::string> w = Words(line.substr(0, line.find('#')));
      if (w.size() >= 3 && Lower(w[0]) == "setsupermodel") {
        name = w[2];
        break;
      }
    }
  }
  name = Lower(name);
  return name == "null" ? std::string() : name;
}

// The first animation of that name (any case) - the one the engine plays - or null.
const ModelAnimation* First(const std::vector<ModelAnimation>& anims, const char* name) {
  for (const ModelAnimation& a : anims) {
    if (Lower(a.name) == name) return &a;
  }
  return nullptr;
}

int Detonates(const ModelAnimation* anim) {
  int count = 0;
  if (anim) {
    for (const ModelEvent& e : anim->events) count += Lower(e.name) == "detonate" ? 1 : 0;
  }
  return count;
}

// One of "die" / "dead" as the model plays it: its detonates, the model it is from ("" not found) and the nodes it keys
// the birthrate of.
struct Played {
  int detonates = 0;
  std::string from;
  std::set<std::string> births;
};

std::string Describe(const char* anim, const Played& p) {
  if (p.from.empty()) return std::string("no ") + anim;
  return std::string(anim) + " " + std::to_string(p.detonates) + (p.detonates == 1 ? " detonate" : " detonates") + " (" + p.from + ")";
}

}  // namespace

bool ReadModelDeathParts(const std::string& bytes, ModelDeathParts* parts, std::string* error) {
  ModelDeathParts result;
  std::vector<ModelAnimation> anims;
  std::string why;
  const bool read = ReadModelAnimations(bytes, &anims, &why);
  if (read) {
    const bool compiled = IsCompiled(bytes);
    const ModelAnimation* die = First(anims, "die");
    const ModelAnimation* dead = First(anims, "dead");
    result.die = die != nullptr;
    result.dead = dead != nullptr;
    result.die_detonates = Detonates(die);
    result.dead_detonates = Detonates(dead);
    result.supermodel = Supermodel(bytes);
    if (compiled) {
      if (die) result.die_births = CompiledBirths(bytes, die->header);
      if (dead) result.dead_births = CompiledBirths(bytes, dead->header);
    } else {
      if (die) result.die_births = AsciiBirths(bytes, "die");
      if (dead) result.dead_births = AsciiBirths(bytes, "dead");
    }
    // The emitters: the model's geometry's, through the readers the VFX Bench uses (a model with none reads as none). The
    // update exactly "Explosion", as the engine compares it.
    VfxEffect effect;
    std::string unused;
    if (compiled ? ReadVfxBinaryModel(bytes, &effect, &unused) : ReadVfxAsciiModel(bytes, &effect, &unused)) {
      for (const VfxEmitter& e : effect.emitters) {
        if (e.update != "Explosion") continue;
        ++result.explosions;
        if (e.chunk.empty()) continue;
        ++result.chunk_explosions;
        ChunkEmitter c;
        c.name = Lower(e.name);
        c.chunk = Lower(e.chunk);
        // Compiled: read here (the VFX reader gives an emitter with no birthrate controller a draft's 10). ASCII: its
        // "birthrate" line (0 without one).
        c.birthrate = compiled ? CompiledBirthrate(bytes, c.name) : e.birthrate;
        result.chunk_emitters.push_back(c);
      }
    }
  }
  if (parts) *parts = result;
  if (error) *error = read ? std::string() : why;
  return read;
}

bool HasOwnDeathDebris(const ModelBytesLookup& lookup, const std::string& model, std::string* why) {
  const std::string name = Lower(model);
  std::string bytes, error;
  ModelDeathParts parts;
  if (!lookup || name.empty() || !lookup(name, &bytes)) {
    if (why) *why = (name.empty() ? "(no name)" : name) + ": no such model - no death debris of its own";
    return false;
  }
  if (!ReadModelDeathParts(bytes, &parts, &error)) {
    if (why) *why = name + ": not read (" + error + ") - no death debris of its own";
    return false;
  }
  Played die, dead;
  if (parts.die) die = {parts.die_detonates, name, parts.die_births};
  if (parts.dead) dead = {parts.dead_detonates, name, parts.dead_births};
  // An animation it hasn't: its supermodel chain's, the nearest first. A link missing or unread, or a loop, ends it.
  std::set<std::string> seen = {name};
  std::string next = parts.supermodel;
  for (int depth = 1; depth < kChainMost && !next.empty() && (die.from.empty() || dead.from.empty()); ++depth) {
    std::string super_bytes, super_error;
    ModelDeathParts super;
    if (!seen.insert(next).second || !lookup(next, &super_bytes) || !ReadModelDeathParts(super_bytes, &super, &super_error)) break;
    if (die.from.empty() && super.die) die = {super.die_detonates, next, super.die_births};
    if (dead.from.empty() && super.dead) dead = {super.dead_detonates, next, super.dead_births};
    next = super.supermodel;
  }
  // Each chunk emitter: set off (the "die" or "dead" with a detonate) with something to throw (its birthrate, or that
  // animation's key of it), and its chunk model there.
  const bool set_off = die.detonates > 0 || dead.detonates > 0;
  int throwing = 0, idle = 0;
  std::string missing;
  std::map<std::string, bool> there;  // chunk model -> found
  for (const ChunkEmitter& e : parts.chunk_emitters) {
    const auto fires = [&](const Played& p) { return p.detonates > 0 && (e.birthrate > 0.0f || p.births.count(e.name) > 0); };
    if (!fires(die) && !fires(dead)) {
      idle += set_off ? 1 : 0;  // (with no detonate none is set off: said once, by the counts before)
      continue;
    }
    auto found = there.find(e.chunk);
    if (found == there.end()) {
      std::string chunk_bytes;
      found = there.emplace(e.chunk, lookup(e.chunk, &chunk_bytes)).first;
      if (!found->second) missing += (missing.empty() ? "" : ", ") + e.chunk;
    }
    if (found->second) ++throwing;
  }
  const bool own = throwing > 0;
  if (why) {
    *why = name + ": " + Describe("die", die) + ", " + Describe("dead", dead) + "; " + std::to_string(parts.explosions) +
           (parts.explosions == 1 ? " Explosion emitter, " : " Explosion emitters, ") + std::to_string(parts.chunk_explosions) +
           " with chunks, " + std::to_string(throwing) + " throwing" +
           (idle ? ", " + std::to_string(idle) + (idle == 1 ? " throws nothing" : " throw nothing") : std::string()) +
           (missing.empty() ? std::string() : ", chunk model " + missing + " missing") + " -> " +
           (own ? "own debris" : "no debris of its own");
  }
  return own;
}

}  // namespace data
