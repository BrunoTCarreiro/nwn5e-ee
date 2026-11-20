// DH: see anim_death_debris.h.

#include "anim_death_debris.h"

#include <ctype.h>
#include <string.h>

#include <set>
#include <string>
#include <vector>

#include "vfx_model.h"

namespace data {
namespace {

constexpr size_t kModelBase = 12;       // a compiled model's offsets count from the end of its 12-byte file header
constexpr size_t kSupermodelAt = 0xA8;  // its supermodel's name in the model header (64 bytes)
constexpr int kChainMost = 8;           // models in a supermodel chain, the model's own counted (FindAnimClip's)

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

// One of "die" / "dead" as the model plays it: its detonates and the model it is from ("" not found).
struct Played {
  int detonates = 0;
  std::string from;
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
    const ModelAnimation* die = First(anims, "die");
    const ModelAnimation* dead = First(anims, "dead");
    result.die = die != nullptr;
    result.dead = dead != nullptr;
    result.die_detonates = Detonates(die);
    result.dead_detonates = Detonates(dead);
    result.supermodel = Supermodel(bytes);
    // The emitters: the model's geometry's, through the readers the VFX Bench uses (a model with none reads as none).
    VfxEffect effect;
    std::string unused;
    if (IsCompiled(bytes) ? ReadVfxBinaryModel(bytes, &effect, &unused) : ReadVfxAsciiModel(bytes, &effect, &unused)) {
      for (const VfxEmitter& e : effect.emitters) {
        if (Lower(e.update) != "explosion") continue;
        ++result.explosions;
        if (!e.chunk.empty()) ++result.chunk_explosions;
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
  if (parts.die) die = {parts.die_detonates, name};
  if (parts.dead) dead = {parts.dead_detonates, name};
  // An animation it hasn't: its supermodel chain's, the nearest first. A link missing or unread, or a loop, ends it.
  std::set<std::string> seen = {name};
  std::string next = parts.supermodel;
  for (int depth = 1; depth < kChainMost && !next.empty() && (die.from.empty() || dead.from.empty()); ++depth) {
    std::string super_bytes, super_error;
    ModelDeathParts super;
    if (!seen.insert(next).second || !lookup(next, &super_bytes) || !ReadModelDeathParts(super_bytes, &super, &super_error)) break;
    if (die.from.empty() && super.die) die = {super.die_detonates, next};
    if (dead.from.empty() && super.dead) dead = {super.dead_detonates, next};
    next = super.supermodel;
  }
  const bool own = parts.chunk_explosions > 0 && (die.detonates > 0 || dead.detonates > 0);
  if (why) {
    *why = name + ": " + Describe("die", die) + ", " + Describe("dead", dead) + "; " + std::to_string(parts.explosions) +
           (parts.explosions == 1 ? " Explosion emitter, " : " Explosion emitters, ") + std::to_string(parts.chunk_explosions) +
           " with chunks -> " + (own ? "own debris" : "no debris of its own");
  }
  return own;
}

}  // namespace data
