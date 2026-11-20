#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "check.h"
#include "data/anim_death_debris.h"
#include "data/game_resources.h"
#include "data/twoda.h"
#include "data/vfx_model.h"

// DH (#172): does a placeable's model already throw debris of its own when it breaks (data/anim_death_debris.h)? The
// game session plays DE's material debris only when it doesn't. Synthetic models (built here, nothing of the game's),
// then the game's own placeables where the game is (NWN_GAME_DIR, or Steam's folder; read in memory, nothing kept).

using namespace data;

namespace {

bool Has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

// A controller of a node built here: its type, columns, its rows' times and values.
struct Ctrl {
  uint32_t type;
  int columns;
  std::vector<float> times, values;
};

// An animation's birthrate keys (controller 88) of one node, by its name: t v rows.
struct BirthKeys {
  const char* node;
  std::vector<float> rows;
};

// A compiled placeable model laid out as the game's (the readers' terms: a node is its header + 0x10, offsets from byte
// 12): a root dummy whose children are the emitters (each with its position and birthrate controllers - 12 bytes a key:
// type, rows, the first time's and value's index, columns - and its update, render, texture and chunk in the emitter
// header at the node + 0x60), animations with their events (36 bytes an event: its time, then a 32-byte name) and their
// own node trees (a node keying an emitter's birthrate), listed in the model header (+0x78, count +0x7C), and its
// supermodel's name (+0xA8).
struct PlcModel {
  static constexpr size_t kBase = 12;
  std::string b = std::string(0x8000, '\0');
  size_t free_at = 0x100;
  size_t root = 0;
  std::vector<size_t> kids, anims;
  explicit PlcModel(const char* name, const char* supermodel = "NULL") {
    Puts(kBase + 8, name);
    Puts(kBase + 0xA8, supermodel);
    root = Node(name, 0x1, {{8, 3, {0}, {0, 0, 0}}});
    Put32(kBase + 0x48, Off(root));
  }
  static uint32_t Off(size_t node) { return static_cast<uint32_t>(node - 0x10 - kBase); }
  size_t Alloc(size_t n) {
    const size_t at = free_at;
    free_at += (n + 15) & ~static_cast<size_t>(15);
    return at;
  }
  void Put32(size_t at, uint32_t v) { memcpy(&b[at], &v, 4); }
  void Put16(size_t at, uint16_t v) { memcpy(&b[at], &v, 2); }
  void PutF(size_t at, float v) { memcpy(&b[at], &v, 4); }
  void Puts(size_t at, const char* text) { memcpy(&b[at], text, strlen(text)); }
  size_t Node(const char* name, uint32_t flags, const std::vector<Ctrl>& ctrls) {
    const size_t at = Alloc(0x300);
    Puts(at + 0x10, name);
    Put32(at + 0x5C, flags);
    size_t numbers = 0;
    for (const Ctrl& c : ctrls) numbers += c.times.size() + c.values.size();
    const size_t keys = Alloc(12 * ctrls.size() + 4), values = Alloc(4 * numbers + 4);
    Put32(at + 0x44, static_cast<uint32_t>(keys - kBase));
    Put32(at + 0x48, static_cast<uint32_t>(ctrls.size()));
    Put32(at + 0x50, static_cast<uint32_t>(values - kBase));
    uint16_t used = 0;
    for (size_t i = 0; i < ctrls.size(); ++i) {
      const size_t k = keys + 12 * i;
      Put32(k, ctrls[i].type);
      Put16(k + 4, static_cast<uint16_t>(ctrls[i].times.size()));
      Put16(k + 6, used);
      for (float t : ctrls[i].times) PutF(values + 4 * used++, t);
      Put16(k + 8, used);
      for (float v : ctrls[i].values) PutF(values + 4 * used++, v);
      b[k + 10] = static_cast<char>(ctrls[i].columns);
    }
    Put32(at + 0x54, used);
    return at;
  }
  void Children(size_t at, const std::vector<size_t>& children) {
    const size_t array = Alloc(4 * children.size() + 4);
    Put32(at + 0x38, static_cast<uint32_t>(array - kBase));
    Put32(at + 0x3C, static_cast<uint32_t>(children.size()));
    for (size_t i = 0; i < children.size(); ++i) Put32(array + 4 * i, Off(children[i]));
  }
  // An emitter under the root: its update ("Explosion", "Fountain" ...), chunk model (nullptr: none) and birthrate (< 0:
  // no birthrate controller at all).
  void Emitter(const char* name, const char* update, const char* chunk, float birthrate = 2.0f) {
    std::vector<Ctrl> ctrls = {{8, 3, {0}, {0, 0, 0}}};
    if (birthrate >= 0.0f) ctrls.push_back({88, 1, {0}, {birthrate}});
    const size_t at = Node(name, 0x5, ctrls);
    Puts(at + 0x60 + 0x18, update);
    Puts(at + 0x60 + 0x38, "Normal");
    Puts(at + 0x60 + 0x78, "fxpa_smoke01");
    if (chunk) Puts(at + 0x60 + 0xB8, chunk);
    kids.push_back(at);
  }
  // An animation (its own root dummy, and under it a node for each of `births`) with its events.
  void Anim(const char* name, const std::vector<std::pair<float, const char*>>& events, const std::vector<BirthKeys>& births = {}) {
    const size_t header = Alloc(0x100);
    Puts(header + 8, name);
    const size_t top = Node(name, 0x1, {});
    std::vector<size_t> keyed;
    for (const BirthKeys& k : births) {
      Ctrl c{88, 1, {}, {}};
      for (size_t r = 0; r + 1 < k.rows.size(); r += 2) {
        c.times.push_back(k.rows[r]);
        c.values.push_back(k.rows[r + 1]);
      }
      keyed.push_back(Node(k.node, 0x5, {c}));
    }
    if (!keyed.empty()) Children(top, keyed);
    Put32(header + 0x48, Off(top));
    b[header + 0x6C] = 5;
    PutF(header + 0x70, 0.167f);
    if (!events.empty()) {
      const size_t array = Alloc(36 * events.size());
      for (size_t i = 0; i < events.size(); ++i) {
        PutF(array + 36 * i, events[i].first);
        Puts(array + 36 * i + 4, events[i].second);
      }
      Put32(header + 0xB8, static_cast<uint32_t>(array - kBase));
      Put32(header + 0xBC, static_cast<uint32_t>(events.size()));
      Put32(header + 0xC0, static_cast<uint32_t>(events.size()));
    }
    anims.push_back(header);
  }
  // The model's bytes: the root's children and the animations' list written.
  std::string Bytes() {
    PlcModel copy = *this;
    copy.Children(root, kids);
    const size_t list = copy.Alloc(4 * anims.size() + 4);
    copy.Put32(kBase + 0x78, static_cast<uint32_t>(list - kBase));
    copy.Put32(kBase + 0x7C, static_cast<uint32_t>(anims.size()));
    for (size_t i = 0; i < anims.size(); ++i) copy.Put32(list + 4 * i, static_cast<uint32_t>(anims[i] - kBase));
    return copy.b;
  }
};

// The lectern's shape (PLC_K02), compiled: chunks and a smoke puff (both Explosion), "die" with two detonates and a sound,
// `dead` its events.
std::string Lectern(const std::vector<std::pair<float, const char*>>& die, const std::vector<std::pair<float, const char*>>& dead,
                    const char* chunk = "PLC_Chunk_W01", const char* update = "Explosion") {
  PlcModel m("plc_test");
  m.Emitter("ChunkyWood1", update, chunk);
  m.Emitter("fire!1", "Explosion", nullptr);
  m.Anim("damage", {});
  m.Anim("die", die);
  m.Anim("dead", dead);
  m.Anim("default", {});
  return m.Bytes();
}

// The same as an ASCII model (PLC_C10's form): `emitters` its geometry's emitter nodes, `anims` its newanim blocks.
std::string AsciiModel(const std::string& supermodel, const std::string& emitters, const std::string& anims) {
  return "# Exported from somewhere\r\nnewmodel PLC_T10\r\nsetsupermodel PLC_T10 " + supermodel +
         "\r\nclassification Character\r\nbeginmodelgeom PLC_T10\r\nnode dummy PLC_T10\r\n  parent NULL\r\nendnode\r\n" + emitters +
         "endmodelgeom PLC_T10\r\n\r\n" + anims + "donemodel PLC_T10\r\n";
}

std::string AsciiEmitter(const std::string& name, const std::string& update, const std::string& chunk_line,
                         const std::string& birthrate = "2") {
  return "node emitter " + name + "\r\n  parent PLC_T10\r\n  update " + update + "\r\n  render Normal\r\n" + chunk_line +
         "  birthrate " + birthrate + "\r\nendnode\r\n";
}

// A newanim block: its events, then `nodes` (an animation's node lines, e.g. AsciiBirthKeys).
std::string AsciiAnim(const std::string& name, const std::string& events, const std::string& nodes = "") {
  return "newanim " + name + " PLC_T10\r\n  length 0.0333333\r\n  transtime 0.25\r\n  animroot PLC_T10\r\n" + events +
         "node dummy PLC_T10\r\n  parent NULL\r\nendnode\r\n" + nodes + "doneanim " + name + " PLC_T10\r\n\r\n";
}

// An animation's node keying an emitter's birthrate: "birthratekey <rows>" and its t v rows ("" rows: an endlist list).
std::string AsciiBirthKeys(const std::string& emitter, const std::vector<std::string>& rows, bool counted = true) {
  std::string out = "node emitter " + emitter + "\r\n  parent PLC_T10\r\n  birthratekey" +
                    (counted ? " " + std::to_string(rows.size()) : std::string()) + "\r\n";
  for (const std::string& r : rows) out += "    " + r + "\r\n";
  return out + (counted ? "" : "  endlist\r\n") + "endnode\r\n";
}

// Models handed out by name, as the game's resource manager would - with the chunk models the tests throw (any bytes: a
// chunk model only has to be there), unless `chunks` is false.
ModelBytesLookup Models(std::map<std::string, std::string> models, bool chunks = true) {
  if (chunks) {
    for (const char* chunk : {"plc_chunk_w01", "plc_chunk_w02"}) models.emplace(chunk, "chunk");
  }
  return [models](const std::string& name, std::string* bytes) {
    const auto found = models.find(name);
    if (found == models.end()) return false;
    *bytes = found->second;
    return true;
  };
}

bool Own(const std::string& bytes, std::string* why = nullptr) { return HasOwnDeathDebris(Models({{"plc_test", bytes}}), "plc_test", why); }

}  // namespace

// The rule's yes: a "die" whose detonate sets off an Explosion emitter with a chunk model - compiled and ASCII, the
// animation and event names in any case. Breaks caught: "die" not read (only "dead"), a detonate or an animation name
// taken case-sensitively, the chunk name not read, the smoke puff taken for the chunks (or the chunks for a puff), the
// parts' counts wrong.
TEST(DeathDebrisChunkDetonateInDie) {
  const std::string compiled = Lectern({{0.033f, "detonate"}, {0.033f, "Detonate"}, {0.1f, "snd_crash"}}, {});
  ModelDeathParts parts;
  std::string error, why;
  CHECK_EQ(ReadModelDeathParts(compiled, &parts, &error), true);
  CHECK_EQ(parts.die && parts.dead && parts.die_detonates == 2 && parts.dead_detonates == 0, true);
  CHECK_EQ(parts.explosions, 2);
  CHECK_EQ(parts.chunk_explosions, 1);
  CHECK_EQ(parts.supermodel.empty(), true);
  CHECK_EQ(Own(compiled, &why), true);
  printf("    %s\n", why.c_str());
  CHECK_EQ(Has(why, "plc_test") && Has(why, "own debris"), true);
  // ASCII, the names in other cases.
  const std::string ascii = AsciiModel("NULL",
                                       AsciiEmitter("ChunkyWood1", "Explosion", "  chunkName PLC_Chunk_W01\r\n") +
                                           AsciiEmitter("fire!1", "Explosion", ""),
                                       AsciiAnim("DIE", "  event 0.0333333 DETONATE\r\n") + AsciiAnim("dead", ""));
  ModelDeathParts text;
  CHECK_EQ(ReadModelDeathParts(ascii, &text, &error), true);
  CHECK_EQ(text.die && text.dead && text.die_detonates == 1 && text.dead_detonates == 0, true);
  CHECK_EQ(text.explosions == 2 && text.chunk_explosions == 1 && text.supermodel.empty(), true);
  CHECK_EQ(Own(ascii), true);
}

// "dead" alone (the Cot's, the Birdbath's and Flag3's shape in the game's own files: a "die" with no events, the
// detonates in "dead") counts too. Breaks caught: only "die" looked at.
TEST(DeathDebrisChunkDetonateInDeadOnly) {
  const std::string compiled = Lectern({}, {{0.033f, "detonate"}});
  ModelDeathParts parts;
  std::string error;
  CHECK_EQ(ReadModelDeathParts(compiled, &parts, &error), true);
  CHECK_EQ(parts.die && parts.die_detonates == 0 && parts.dead && parts.dead_detonates == 1, true);
  CHECK_EQ(Own(compiled), true);
  const std::string ascii = AsciiModel("NULL", AsciiEmitter("ChunkyWood1", "Explosion", "  chunkName PLC_Chunk_W02\r\n"),
                                       AsciiAnim("die", "") + AsciiAnim("Dead", "  event 0.0 detonate\r\n"));
  CHECK_EQ(ReadModelDeathParts(ascii, &parts, &error) && parts.dead_detonates == 1 && parts.chunk_explosions == 1, true);
  CHECK_EQ(Own(ascii), true);
}

// A detonate that sets off no chunks: only a smoke puff (an Explosion emitter with no chunk - empty, or an ASCII model's
// "chunkName NULL" or bare "chunkName"), or a chunk model on an emitter that isn't an Explosion one (a detonate doesn't
// set off a Fountain or a Single) -> no debris of its own. Breaks caught: any Explosion emitter taken for chunks; any
// emitter with a chunk taken, whatever its update; "NULL" read as a chunk model.
TEST(DeathDebrisPuffOnly) {
  ModelDeathParts parts;
  std::string error, why;
  const std::string puff = Lectern({{0.033f, "detonate"}}, {}, nullptr);
  CHECK_EQ(ReadModelDeathParts(puff, &parts, &error), true);
  CHECK_EQ(parts.die_detonates == 1 && parts.explosions == 2 && parts.chunk_explosions == 0, true);
  CHECK_EQ(Own(puff, &why), false);
  printf("    %s\n", why.c_str());
  CHECK_EQ(Has(why, "no debris of its own"), true);
  for (const char* update : {"Fountain", "Single"}) {
    const std::string fountain = Lectern({{0.033f, "detonate"}}, {{0.033f, "detonate"}}, "PLC_Chunk_W01", update);
    CHECK_EQ(ReadModelDeathParts(fountain, &parts, &error) && parts.explosions == 1 && parts.chunk_explosions == 0, true);
    CHECK_EQ(Own(fountain), false);
  }
  for (const char* line : {"  chunkName NULL\r\n", "  chunkName\r\n", "  chunkName null\r\n", ""}) {
    const std::string ascii = AsciiModel("NULL", AsciiEmitter("fire!1", "Explosion", line), AsciiAnim("die", "  event 0.0 detonate\r\n"));
    CHECK_EQ(ReadModelDeathParts(ascii, &parts, &error) && parts.explosions == 1 && parts.chunk_explosions == 0, true);
    CHECK_EQ(Own(ascii), false);
  }
  const std::string chunks_fountain =
      AsciiModel("NULL", AsciiEmitter("ChunkyWood1", "Fountain", "  chunkName PLC_Chunk_W01\r\n"), AsciiAnim("die", "  event 0.0 detonate\r\n"));
  CHECK_EQ(ReadModelDeathParts(chunks_fountain, &parts, &error) && parts.explosions == 0 && parts.chunk_explosions == 0, true);
  CHECK_EQ(Own(chunks_fountain), false);
}

// No detonate where it plays: a "die" with only a sound, a detonate in another animation ("damage", "default"), no
// "die" or "dead" at all, a second "die" listed after the first (the engine plays the first of a name) -> no debris of
// its own, though the chunk emitter is there. Breaks caught: any event taken for a detonate; any animation's detonate
// counted; the last "die" of a name taken.
TEST(DeathDebrisNoDetonateOrNoDie) {
  ModelDeathParts parts;
  std::string error;
  const std::string sound = Lectern({{0.1f, "snd_crash"}}, {{0.0f, "snd_dead"}});
  CHECK_EQ(ReadModelDeathParts(sound, &parts, &error) && parts.die && parts.die_detonates == 0 && parts.dead_detonates == 0, true);
  CHECK_EQ(parts.chunk_explosions, 1);
  CHECK_EQ(Own(sound), false);
  PlcModel other("plc_test");
  other.Emitter("ChunkyWood1", "Explosion", "PLC_Chunk_W01");
  other.Anim("damage", {{0.0f, "detonate"}});
  other.Anim("default", {{0.0f, "detonate"}});
  other.Anim("die", {});
  CHECK_EQ(ReadModelDeathParts(other.Bytes(), &parts, &error) && parts.die && !parts.dead && parts.die_detonates == 0, true);
  CHECK_EQ(Own(other.Bytes()), false);
  PlcModel none("plc_test");
  none.Emitter("ChunkyWood1", "Explosion", "PLC_Chunk_W01");
  none.Anim("default", {});
  std::string why;
  CHECK_EQ(ReadModelDeathParts(none.Bytes(), &parts, &error) && !parts.die && !parts.dead && parts.chunk_explosions == 1, true);
  CHECK_EQ(Own(none.Bytes(), &why), false);
  printf("    %s\n", why.c_str());
  PlcModel twice("plc_test");
  twice.Emitter("ChunkyWood1", "Explosion", "PLC_Chunk_W01");
  twice.Anim("die", {});
  twice.Anim("die", {{0.033f, "detonate"}});
  CHECK_EQ(ReadModelDeathParts(twice.Bytes(), &parts, &error) && parts.die && parts.die_detonates == 0, true);
  CHECK_EQ(Own(twice.Bytes()), false);
  const std::string ascii = AsciiModel("NULL", AsciiEmitter("ChunkyWood1", "Explosion", "  chunkName PLC_Chunk_W01\r\n"),
                                       AsciiAnim("damage", "  event 0.0 detonate\r\n") + AsciiAnim("default", ""));
  CHECK_EQ(ReadModelDeathParts(ascii, &parts, &error) && !parts.die && !parts.dead && parts.chunk_explosions == 1, true);
  CHECK_EQ(Own(ascii), false);
}

// Not a model, or not there: false and why, no crash - nothing, text with no newmodel, a compiled model cut inside an
// animation's events or with a count past the file, garbage, a name the lookup hasn't, no lookup at all. Breaks caught:
// a cut model read anyway (out of bounds), a model not there taken for one with debris, an empty why.
TEST(DeathDebrisNotAModel) {
  ModelDeathParts parts;
  std::string error, why;
  CHECK_EQ(ReadModelDeathParts("", &parts, &error), false);
  CHECK_EQ(error.empty(), false);
  CHECK_EQ(ReadModelDeathParts("hello\r\nworld\r\n", &parts, &error), false);
  const std::string whole = Lectern({{0.033f, "detonate"}}, {});
  std::vector<ModelAnimation> anims;
  CHECK_EQ(ReadModelAnimations(whole, &anims, &error) && anims.size() == 4, true);
  if (anims.size() != 4) return;
  const std::string cut = whole.substr(0, anims[1].event_at + 10);
  CHECK_EQ(ReadModelDeathParts(cut, &parts, &error), false);
  CHECK_EQ(Own(cut, &why), false);
  CHECK_EQ(why.empty(), false);
  std::string over = whole;
  const uint32_t many = 100000;
  memcpy(&over[anims[1].header + 0xBC], &many, 4);
  CHECK_EQ(Own(over), false);
  std::string garbage(0x900, '\0');
  for (size_t i = 0; i < garbage.size(); ++i) garbage[i] = static_cast<char>((i * 131 + 7) % 251);
  garbage[0] = garbage[1] = garbage[2] = garbage[3] = '\0';
  CHECK_EQ(Own(garbage), false);
  CHECK_EQ(HasOwnDeathDebris(Models({}), "plc_none", &why), false);
  printf("    %s\n", why.c_str());
  CHECK_EQ(Has(why, "plc_none"), true);
  CHECK_EQ(HasOwnDeathDebris(ModelBytesLookup(), "plc_test", &why), false);
  CHECK_EQ(HasOwnDeathDebris(Models({{"plc_test", whole}}), "", nullptr), false);
}

// The engine makes an ExplosionEmitter only of a node whose update is exactly "Explosion" (MdlNodeEmitter::
// InternalCreateInstance compares it case-sensitively): an ASCII "update explosion" or "EXPLOSION" is no Explosion
// emitter - a detonate sets nothing off. (A compiled model's update is found as "Explosion" itself.) Breaks caught: the
// update compared in any case.
TEST(DeathDebrisExplosionIsCaseSensitive) {
  ModelDeathParts parts;
  std::string error;
  for (const char* update : {"explosion", "EXPLOSION"}) {
    const std::string ascii = AsciiModel("NULL", AsciiEmitter("ChunkyWood1", update, "  chunkName PLC_Chunk_W01\r\n"),
                                         AsciiAnim("die", "  event 0.0 detonate\r\n"));
    CHECK_EQ(ReadModelDeathParts(ascii, &parts, &error), true);
    CHECK_EQ(parts.explosions == 0 && parts.chunk_explosions == 0 && parts.chunk_emitters.empty(), true);
    CHECK_EQ(Own(ascii), false);
  }
  const std::string exact = AsciiModel("NULL", AsciiEmitter("ChunkyWood1", "Explosion", "  chunkName PLC_Chunk_W01\r\n"),
                                       AsciiAnim("die", "  event 0.0 detonate\r\n"));
  CHECK_EQ(ReadModelDeathParts(exact, &parts, &error) && parts.explosions == 1 && parts.chunk_explosions == 1, true);
  CHECK_EQ(Own(exact), true);
  CHECK_EQ(Own(Lectern({{0.033f, "detonate"}}, {}, "PLC_Chunk_W01", "explosion")), false);
}

// An emitter counts only when it throws something: its birthrate above 0, or the animation that sets it off (the "die"
// or "dead" with the detonate - its own or its supermodel's) keying its birthrate above 0 - and when its chunk model is
// there. The game's: plc_x0_cha's and plc_x0_ice's chunk emitter has birthrate 0 and isn't keyed (nothing thrown);
// plc_x0_jar's "die" keys its chunk emitter's birthrate; plc_trgegg's PLC_Chunk_Egg isn't in the game. Breaks caught:
// the birthrate not looked at; a compiled emitter with no birthrate controller taken for one with a rate (the VFX
// reader's default, 10); a key to 0 taken for one that fires it; a key in another animation ("damage", or a "die" with no
// detonate while "dead" detonates) taken; an inherited animation's keys not matched to the model's own emitter by name;
// the chunk model's presence not checked; the parts' emitters read wrong.
TEST(DeathDebrisOnlyEmittersThatThrow) {
  using Events = std::vector<std::pair<float, const char*>>;
  const Events detonate = {{0.033f, "detonate"}}, quiet = {{0.1f, "snd_crash"}};
  const auto model = [&](float birthrate, const std::vector<BirthKeys>& die_keys, const std::vector<BirthKeys>& dead_keys = {},
                         bool die_detonates = true, bool dead_detonates = false, const std::vector<BirthKeys>& damage_keys = {}) {
    PlcModel m("plc_test");
    m.Emitter("ChunkyWood1", "Explosion", "PLC_Chunk_W01", birthrate);
    m.Emitter("fire!1", "Explosion", nullptr, 10.0f);
    m.Anim("damage", {}, damage_keys);
    m.Anim("die", die_detonates ? detonate : quiet, die_keys);
    m.Anim("dead", dead_detonates ? detonate : Events(), dead_keys);
    return m.Bytes();
  };
  ModelDeathParts parts;
  std::string error, why;
  CHECK_EQ(ReadModelDeathParts(model(2.0f, {}), &parts, &error), true);
  CHECK_EQ(parts.chunk_emitters.size(), 1u);
  if (parts.chunk_emitters.size() == 1) {
    CHECK_EQ(parts.chunk_emitters[0].name == "chunkywood1" && parts.chunk_emitters[0].chunk == "plc_chunk_w01", true);
    CHECK_EQ(parts.chunk_emitters[0].birthrate == 2.0f, true);
  }
  CHECK_EQ(Own(model(2.0f, {})), true);
  // Birthrate 0, or none at all, and not keyed: nothing thrown.
  CHECK_EQ(Own(model(0.0f, {}), &why), false);
  printf("    %s\n", why.c_str());
  CHECK_EQ(Has(why, "throws nothing"), true);
  CHECK_EQ(ReadModelDeathParts(model(-1.0f, {}), &parts, &error) && parts.chunk_emitters.size() == 1 &&
               parts.chunk_emitters[0].birthrate == 0.0f,
           true);
  CHECK_EQ(Own(model(-1.0f, {}), &why), false);
  // The "die" that detonates keys it up: thrown. Keyed only to 0: not.
  const std::string keyed = model(0.0f, {{"ChunkyWood1", {0.0f, 0.0f, 0.033f, 3.0f}}});
  CHECK_EQ(ReadModelDeathParts(keyed, &parts, &error) && parts.die_births.count("chunkywood1") == 1 && parts.dead_births.empty(), true);
  CHECK_EQ(Own(keyed, &why), true);
  printf("    %s\n", why.c_str());
  CHECK_EQ(Own(model(0.0f, {{"ChunkyWood1", {0.0f, 0.0f}}})), false);
  // Keyed where it doesn't fire: in "damage"; in a "die" with no detonate while "dead" detonates. Keyed in the "dead"
  // that detonates: thrown.
  CHECK_EQ(Own(model(0.0f, {}, {}, true, false, {{"ChunkyWood1", {0.0f, 3.0f}}})), false);
  CHECK_EQ(Own(model(0.0f, {{"ChunkyWood1", {0.0f, 3.0f}}}, {}, false, true)), false);
  const std::string dead_keyed = model(0.0f, {}, {{"chunkywood1", {0.0f, 3.0f}}}, false, true);
  CHECK_EQ(ReadModelDeathParts(dead_keyed, &parts, &error) && parts.dead_births.count("chunkywood1") == 1, true);
  CHECK_EQ(Own(dead_keyed), true);
  // Its chunk model not there: nothing thrown.
  CHECK_EQ(HasOwnDeathDebris(Models({{"plc_test", model(2.0f, {})}}, false), "plc_test", &why), false);
  printf("    %s\n", why.c_str());
  CHECK_EQ(Has(why, "missing"), true);
  // ASCII: its "birthrate" line, and "birthratekey" lists in the die's emitter node (counted rows, or to an endlist).
  const auto ascii = [](const std::string& birthrate, const std::string& nodes) {
    return AsciiModel("NULL", AsciiEmitter("ChunkyWood1", "Explosion", "  chunkName PLC_Chunk_W01\r\n", birthrate),
                      AsciiAnim("die", "  event 0.0333333 detonate\r\n", nodes));
  };
  CHECK_EQ(ReadModelDeathParts(ascii("0", ""), &parts, &error) && parts.chunk_emitters.size() == 1 &&
               parts.chunk_emitters[0].birthrate == 0.0f && parts.chunk_emitters[0].chunk == "plc_chunk_w01",
           true);
  CHECK_EQ(Own(ascii("0", "")), false);
  CHECK_EQ(Own(ascii("3", "")), true);
  const std::string ascii_keyed = ascii("0", AsciiBirthKeys("ChunkyWood1", {"0.0 0", "0.0333333 3"}));
  CHECK_EQ(ReadModelDeathParts(ascii_keyed, &parts, &error) && parts.die_births.count("chunkywood1") == 1, true);
  CHECK_EQ(Own(ascii_keyed), true);
  CHECK_EQ(Own(ascii("0", AsciiBirthKeys("ChunkyWood1", {"0.0 0", "0.0333333 3"}, false))), true);
  CHECK_EQ(Own(ascii("0", AsciiBirthKeys("ChunkyWood1", {"0.0 0", "0.0333333 0"}))), false);
  // The first "die" of the name is the one played: a second one's key isn't.
  const std::string second = AsciiModel("NULL", AsciiEmitter("ChunkyWood1", "Explosion", "  chunkName PLC_Chunk_W01\r\n", "0"),
                                        AsciiAnim("die", "  event 0.0333333 detonate\r\n") +
                                            AsciiAnim("die", "", AsciiBirthKeys("ChunkyWood1", {"0.0 3"})));
  CHECK_EQ(ReadModelDeathParts(second, &parts, &error) && parts.die_births.empty(), true);
  CHECK_EQ(Own(second), false);
  // An inherited "die" keys the model's own emitter by name.
  PlcModel base("plc_base");
  base.Anim("die", detonate, {{"ChunkyWood2", {0.0f, 4.0f}}});
  PlcModel other("plc_base");
  other.Anim("die", detonate, {{"ChunkyWood9", {0.0f, 4.0f}}});
  PlcModel child("plc_test", "plc_base");
  child.Emitter("ChunkyWood2", "Explosion", "PLC_Chunk_W02", 0.0f);
  CHECK_EQ(HasOwnDeathDebris(Models({{"plc_test", child.Bytes()}, {"plc_base", base.Bytes()}}), "plc_test", nullptr), true);
  CHECK_EQ(HasOwnDeathDebris(Models({{"plc_test", child.Bytes()}, {"plc_base", other.Bytes()}}), "plc_test", nullptr), false);
}

// An animation the model hasn't is its supermodel chain's, as the engine plays it; the emitters are the model's own.
// The game's flags: tnp_flag_d03 has its chunk emitters but no animation, its "die" (2 detonates) is tnp_flag_c01's.
// Breaks caught: the supermodel not followed; the supermodel's "die" taken over the model's own; the supermodel's
// emitters taken for the model's; a chain of two not followed; a missing link or a loop (a -> b -> a) crashing or
// hanging; the chain followed past eight models (nwn5e's own cap, FindAnimClip's - the engine's FindAnimation has none,
// and no game chain is deeper than 2).
TEST(DeathDebrisFromTheSupermodel) {
  PlcModel base("plc_base");
  base.Emitter("ChunkyWood1", "Explosion", "PLC_Chunk_W01");
  base.Anim("die", {{0.033f, "detonate"}, {0.033f, "detonate"}});
  base.Anim("dead", {});
  PlcModel child("plc_test", "plc_base");
  child.Emitter("ChunkyWood2", "Explosion", "PLC_Chunk_W02");
  child.Emitter("fire!1", "Explosion", nullptr);
  ModelDeathParts parts;
  std::string error, why;
  CHECK_EQ(ReadModelDeathParts(child.Bytes(), &parts, &error), true);
  CHECK_EQ(parts.supermodel == "plc_base" && !parts.die && !parts.dead && parts.chunk_explosions == 1, true);
  CHECK_EQ(HasOwnDeathDebris(Models({{"plc_test", child.Bytes()}, {"plc_base", base.Bytes()}}), "PLC_Test", &why), true);
  printf("    %s\n", why.c_str());
  CHECK_EQ(Has(why, "plc_base"), true);
  // Its own "die" (no detonate) wins over the supermodel's.
  PlcModel own_die = child;
  own_die.Anim("die", {{0.1f, "snd_crash"}});
  CHECK_EQ(HasOwnDeathDebris(Models({{"plc_test", own_die.Bytes()}, {"plc_base", base.Bytes()}}), "plc_test", nullptr), false);
  // The supermodel's emitters aren't the model's: a child with only a puff.
  PlcModel puff("plc_test", "plc_base");
  puff.Emitter("fire!1", "Explosion", nullptr);
  CHECK_EQ(HasOwnDeathDebris(Models({{"plc_test", puff.Bytes()}, {"plc_base", base.Bytes()}}), "plc_test", nullptr), false);
  // Two deep: child -> plc_mid (no die, no dead) -> plc_base.
  PlcModel mid("plc_mid", "PLC_BASE");
  mid.Anim("default", {});
  PlcModel deep("plc_test", "plc_mid");
  deep.Emitter("ChunkyWood2", "Explosion", "PLC_Chunk_W02");
  CHECK_EQ(HasOwnDeathDebris(Models({{"plc_test", deep.Bytes()}, {"plc_mid", mid.Bytes()}, {"plc_base", base.Bytes()}}), "plc_test", &why),
           true);
  CHECK_EQ(Has(why, "plc_base"), true);
  // An ASCII child (setsupermodel) of a compiled supermodel.
  const std::string ascii = AsciiModel("plc_base", AsciiEmitter("ChunkyWood1", "Explosion", "  chunkName PLC_Chunk_W01\r\n"), "");
  CHECK_EQ(ReadModelDeathParts(ascii, &parts, &error) && parts.supermodel == "plc_base" && !parts.die, true);
  CHECK_EQ(HasOwnDeathDebris(Models({{"plc_test", ascii}, {"plc_base", base.Bytes()}}), "plc_test", nullptr), true);
  // A missing link, a loop: false, and it ends.
  CHECK_EQ(HasOwnDeathDebris(Models({{"plc_test", deep.Bytes()}}), "plc_test", nullptr), false);
  PlcModel a("plc_test", "plc_loop");
  a.Emitter("ChunkyWood1", "Explosion", "PLC_Chunk_W01");
  PlcModel b("plc_loop", "plc_test");
  CHECK_EQ(HasOwnDeathDebris(Models({{"plc_test", a.Bytes()}, {"plc_loop", b.Bytes()}}), "plc_test", nullptr), false);
  // Eight models at most, the model's own counted: its "die" 7 supermodels up is played, 8 up is not.
  const auto chain = [&](int up) {
    std::map<std::string, std::string> models;
    for (int i = 0; i < up; ++i) {
      const std::string name = i == 0 ? "plc_test" : "plc_s" + std::to_string(i);
      const std::string next = "plc_s" + std::to_string(i + 1);
      PlcModel m(name.c_str(), next.c_str());
      if (i == 0) m.Emitter("ChunkyWood1", "Explosion", "PLC_Chunk_W01");
      models[name] = m.Bytes();
    }
    PlcModel top(("plc_s" + std::to_string(up)).c_str());
    top.Anim("die", {{0.033f, "detonate"}});
    models["plc_s" + std::to_string(up)] = top.Bytes();
    return HasOwnDeathDebris(Models(models), "plc_test", nullptr);
  };
  CHECK_EQ(chain(1), true);
  CHECK_EQ(chain(7), true);
  CHECK_EQ(chain(8), false);
}

// --- The game's own placeables (only where the game is; read in memory, nothing kept). ---
namespace {

std::string GameFolder() {
  const char* given = getenv("NWN_GAME_DIR");
  return given && *given ? given : "X:\\SteamLibrary\\steamapps\\common\\Neverwinter Nights";
}

// The game's models as the tests read them (its keys, ovr\ over them) - and as nwn5e serves them: LD's one-burst copies
// (tools/data/oneburst.js's MODELS: the game's model with its "dead" detonates taken out, built from the game at install)
// in place of the game's own.
struct GameModels {
  GameResources resources;
  std::set<std::string> one_burst;
  explicit GameModels(const std::wstring& dir) : resources(dir, GameResources::EeKeyFiles(), {dir + L"\\ovr"}) {
    std::ifstream in("tools/data/oneburst.js", std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    const std::string js = text.str();
    const size_t start = js.find("const MODELS = [");
    const size_t end = start == std::string::npos ? std::string::npos : js.find(']', start);
    for (size_t at = start; end != std::string::npos && (at = js.find('"', at)) != std::string::npos && at < end;) {
      const size_t close = js.find('"', at + 1);
      one_burst.insert(js.substr(at + 1, close - at - 1));
      at = close + 1;
    }
  }
  ModelBytesLookup Game() {
    return [this](const std::string& name, std::string* bytes) {
      std::string source;
      return resources.Read(name, kResourceTypeMdl, bytes, &source);
    };
  }
  ModelBytesLookup Served() {
    return [this](const std::string& name, std::string* bytes) {
      std::string source;
      if (!resources.Read(name, kResourceTypeMdl, bytes, &source)) return false;
      if (one_burst.count(name)) DropAnimationEvents(bytes, "dead", "detonate");
      return true;
    };
  }
};

}  // namespace

// The anchors: the Academy's bench (PLC_X01), lectern (plc_k02) and brazier (PLC_I05) throw their own chunks; the Cot,
// the Birdbath and Flag3 (plc_h10, plc_i02, plc_q03) do from the game's files - their detonates are in "dead" - and
// don't as nwn5e serves them (LD took those out; the owner: "B for the cot, birdbath and flag" - DE's debris); the lectern
// still does (its "die" is untouched). A candle's smoke puff (plc_cndl01), a candle with no debris (ptm_candle06) and
// "new:Brazier" (tnp_fire_001, no "die") have none; the flag tnp_flag_d03 has, by tnp_flag_c01's "die".
TEST(DeathDebrisGameAnchors) {
  const std::string game = GameFolder();
  if (!std::ifstream(game + "\\data\\nwn_base.key")) {
    printf("    (no game at %s: skipped)\n", game.c_str());
    return;
  }
  GameModels models(std::wstring(game.begin(), game.end()));
  CHECK_EQ(models.one_burst.size(), 16u);
  const ModelBytesLookup from_game = models.Game(), served = models.Served();
  for (const char* model : {"plc_k02", "PLC_X01", "PLC_I05"}) {
    std::string why;
    const bool own = HasOwnDeathDebris(from_game, model, &why);
    printf("    %s\n", why.c_str());
    CHECK_EQ(own, true);
    CHECK_EQ(HasOwnDeathDebris(served, model, nullptr), true);
  }
  for (const char* model : {"plc_h10", "plc_i02", "plc_q03"}) {
    std::string why;
    CHECK_EQ(HasOwnDeathDebris(from_game, model, nullptr), true);
    const bool own = HasOwnDeathDebris(served, model, &why);
    printf("    served %s\n", why.c_str());
    CHECK_EQ(own, false);
  }
  for (const char* model : {"plc_cndl01", "ptm_candle06", "tnp_fire_001"}) {
    std::string why;
    const bool own = HasOwnDeathDebris(served, model, &why);
    printf("    %s\n", why.c_str());
    CHECK_EQ(own, false);
  }
  std::string why;
  CHECK_EQ(HasOwnDeathDebris(served, "tnp_flag_d03", &why), true);
  printf("    %s\n", why.c_str());
  CHECK_EQ(Has(why, "tnp_flag_c01"), true);
}

// Debris that shows (the review): the chandelier plc_x0_cha and plc_x0_ice - their one chunk emitter (ChunkyWood94) has
// birthrate 0 and "die" doesn't key it - and plc_trgegg, plc_trgnst (PLC_Chunk_Egg) and px2_e01 (PX2_S01), whose chunk
// models aren't in the game, throw nothing: no debris of their own, from the game's files and as served. plc_x0_jar,
// plc_x0_myt, pwc_mage_001 and pwc_scrt_001, whose "die" keys their chunk emitters' birthrate, do; and px2_b02 / px2_d04,
// one of whose chunk models (PLC_Chunk_M02) isn't there, throw their others.
TEST(DeathDebrisGameDebrisThatShows) {
  const std::string game = GameFolder();
  if (!std::ifstream(game + "\\data\\nwn_base.key")) {
    printf("    (no game at %s: skipped)\n", game.c_str());
    return;
  }
  GameModels models(std::wstring(game.begin(), game.end()));
  const ModelBytesLookup from_game = models.Game(), served = models.Served();
  for (const char* model : {"plc_x0_cha", "plc_x0_ice", "plc_trgegg", "plc_trgnst", "px2_e01"}) {
    std::string why;
    const bool own = HasOwnDeathDebris(served, model, &why);
    printf("    %s\n", why.c_str());
    CHECK_EQ(own, false);
    CHECK_EQ(HasOwnDeathDebris(from_game, model, nullptr), false);
  }
  for (const char* model : {"plc_x0_jar", "plc_x0_myt", "pwc_mage_001", "pwc_scrt_001", "px2_b02", "px2_d04"}) {
    std::string why;
    const bool own = HasOwnDeathDebris(served, model, &why);
    printf("    %s\n", why.c_str());
    CHECK_EQ(own, true);
  }
}

// Every placeables.2da model: the recon's classification read from each model's own animations (it followed no
// supermodel) - "die" with detonate and chunk emitters 525, "die" with a smoke puff only 181, "die" with no debris 71, no
// "die" 491, 36 not in the game - and the rule's count. By the models' own animations and chunk names alone 528: the 525
// and the three whose detonates are only in "dead" (the Cot, the Birdbath, Flag3). As the game plays them (an animation
// a model hasn't is its supermodel's) 151 more - models with chunk emitters of their own and no "die" whose supermodel's
// "die" detonates: 72 flags and banners (tnp_flag_*, dag_*flag*, pwc_bann_*: tnp_flag_a01 / b01 / c01 / e01 ...), 59
// trees (tnp_tree_a01's), 14 balconies, 4 windows, a scaffold and a bench - less the five whose chunks throw nothing or
// aren't there (fix round 1: plc_x0_cha, plc_x0_ice, plc_trgegg, plc_trgnst, px2_e01): 674 from the game's files, 671
// as served (the three lose theirs to LD's copies).
TEST(DeathDebrisGamePlaceablesCount) {
  const std::string game = GameFolder();
  if (!std::ifstream(game + "\\data\\nwn_base.key")) {
    printf("    (no game at %s: skipped)\n", game.c_str());
    return;
  }
  GameModels models(std::wstring(game.begin(), game.end()));
  std::string bytes, source, error;
  TwoDA placeables;
  CHECK_EQ(models.resources.Read("placeables", kResourceType2DA, &bytes, &source) && ParseTwoDA(bytes, &placeables, &error), true);
  size_t column = placeables.columns.size();
  for (size_t c = 0; c < placeables.columns.size(); ++c) {
    if (placeables.columns[c] == "ModelName") column = c;
  }
  CHECK_EQ(column < placeables.columns.size(), true);
  if (column == placeables.columns.size()) return;
  std::set<std::string> names;
  for (const std::vector<std::string>& row : placeables.rows) {
    if (column >= row.size() || row[column] == kEmpty) continue;
    std::string name = row[column];
    for (char& c : name) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    names.insert(name);
  }
  int missing = 0, unread = 0, debris = 0, puff = 0, none = 0, no_die = 0, own_rule = 0, rule_game = 0, rule_served = 0, inherited = 0;
  const ModelBytesLookup from_game = models.Game(), served = models.Served();
  for (const std::string& name : names) {
    if (!from_game(name, &bytes)) {
      ++missing;
      continue;
    }
    ModelDeathParts p;
    if (!ReadModelDeathParts(bytes, &p, &error)) {
      ++unread;
      printf("    %s unread: %s\n", name.c_str(), error.c_str());
      continue;
    }
    if (!p.die) ++no_die;
    else if (p.die_detonates > 0 && p.explosions > 0) ++(p.chunk_explosions > 0 ? debris : puff);
    else ++none;
    if (p.chunk_explosions > 0 && (p.die_detonates > 0 || p.dead_detonates > 0)) ++own_rule;
    const bool g = HasOwnDeathDebris(from_game, name, nullptr), s = HasOwnDeathDebris(served, name, nullptr);
    rule_game += g;
    rule_served += s;
    if (s && !(p.chunk_explosions > 0 && (p.die_detonates > 0 || p.dead_detonates > 0))) ++inherited;
  }
  printf("    %d models (%d not in the game, %d unread): die + chunk debris %d, die + puff only %d, die with no debris %d, no die %d\n",
         static_cast<int>(names.size()), missing, unread, debris, puff, none, no_die);
  printf("    the rule: own animations only %d; as played (supermodels) from the game %d, as served %d (%d by a supermodel's die)\n",
         own_rule, rule_game, rule_served, inherited);
  CHECK_EQ(static_cast<int>(names.size()), 1304);
  CHECK_EQ(missing, 36);
  CHECK_EQ(unread, 0);
  CHECK_EQ(debris, 525);
  CHECK_EQ(puff, 181);
  CHECK_EQ(none, 71);
  CHECK_EQ(no_die, 491);
  CHECK_EQ(own_rule, 528);
  CHECK_EQ(rule_game, 674);
  CHECK_EQ(rule_served, 671);
  CHECK_EQ(inherited, 151);
}
