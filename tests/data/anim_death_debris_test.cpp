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

// A compiled placeable model laid out as the game's (the readers' terms: a node is its header + 0x10, offsets from byte
// 12): a root dummy whose children are the emitters (each with one controller, as the game's always have, and its update,
// render, texture and chunk in the emitter header at the node + 0x60), animations with their events (36 bytes an event:
// its time, then a 32-byte name) listed in the model header (+0x78, count +0x7C), and its supermodel's name (+0xA8).
struct PlcModel {
  static constexpr size_t kBase = 12;
  std::string b = std::string(0x8000, '\0');
  size_t free_at = 0x100;
  size_t root = 0;
  std::vector<size_t> kids, anims;
  explicit PlcModel(const char* name, const char* supermodel = "NULL") {
    Puts(kBase + 8, name);
    Puts(kBase + 0xA8, supermodel);
    root = Node(name, 0x1);
    Put32(kBase + 0x48, Off(root));
  }
  static uint32_t Off(size_t node) { return static_cast<uint32_t>(node - 0x10 - kBase); }
  size_t Alloc(size_t n) {
    const size_t at = free_at;
    free_at += (n + 15) & ~static_cast<size_t>(15);
    return at;
  }
  void Put32(size_t at, uint32_t v) { memcpy(&b[at], &v, 4); }
  void PutF(size_t at, float v) { memcpy(&b[at], &v, 4); }
  void Puts(size_t at, const char* text) { memcpy(&b[at], text, strlen(text)); }
  // A node with one controller: its position, 0 0 0 (type 8, one row of 3).
  size_t Node(const char* name, uint32_t flags) {
    const size_t at = Alloc(0x300);
    Puts(at + 0x10, name);
    Put32(at + 0x5C, flags);
    const size_t keys = Alloc(16), values = Alloc(16);
    Put32(at + 0x44, static_cast<uint32_t>(keys - kBase));
    Put32(at + 0x48, 1);
    Put32(at + 0x50, static_cast<uint32_t>(values - kBase));
    Put32(at + 0x54, 4);
    Put32(keys, 8);
    b[keys + 4] = 1;   // rows
    b[keys + 8] = 1;   // the values after the one time
    b[keys + 10] = 3;  // columns
    return at;
  }
  // An emitter under the root: its update ("Explosion", "Fountain" ...) and chunk model (nullptr: none).
  void Emitter(const char* name, const char* update, const char* chunk) {
    const size_t at = Node(name, 0x5);
    Puts(at + 0x60 + 0x18, update);
    Puts(at + 0x60 + 0x38, "Normal");
    Puts(at + 0x60 + 0x78, "fxpa_smoke01");
    if (chunk) Puts(at + 0x60 + 0xB8, chunk);
    kids.push_back(at);
  }
  // An animation (its own root dummy) with its events.
  void Anim(const char* name, const std::vector<std::pair<float, const char*>>& events) {
    const size_t header = Alloc(0x100);
    Puts(header + 8, name);
    Put32(header + 0x48, Off(Node(name, 0x1)));
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
    const size_t children = copy.Alloc(4 * kids.size() + 4);
    copy.Put32(root + 0x38, static_cast<uint32_t>(children - kBase));
    copy.Put32(root + 0x3C, static_cast<uint32_t>(kids.size()));
    for (size_t i = 0; i < kids.size(); ++i) copy.Put32(children + 4 * i, Off(kids[i]));
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

std::string AsciiEmitter(const std::string& name, const std::string& update, const std::string& chunk_line) {
  return "node emitter " + name + "\r\n  parent PLC_T10\r\n  update " + update + "\r\n  render Normal\r\n" + chunk_line +
         "  birthrate 2\r\nendnode\r\n";
}

std::string AsciiAnim(const std::string& name, const std::string& events) {
  return "newanim " + name + " PLC_T10\r\n  length 0.0333333\r\n  transtime 0.25\r\n  animroot PLC_T10\r\n" + events +
         "node dummy PLC_T10\r\n  parent NULL\r\nendnode\r\ndoneanim " + name + " PLC_T10\r\n\r\n";
}

// Models handed out by name, as the game's resource manager would.
ModelBytesLookup Models(const std::map<std::string, std::string>& models) {
  return [models](const std::string& name, std::string* bytes) {
    const auto found = models.find(name);
    if (found == models.end()) return false;
    *bytes = found->second;
    return true;
  };
}

bool Own(const std::string& bytes, std::string* why = nullptr) { return HasOwnDeathDebris(Models({{"plc_test", bytes}}), "plc_test", why); }

}  // namespace

// The rule's yes: a "die" whose detonate sets off an Explosion emitter with a chunk model - compiled and ASCII, any case.
// Breaks caught: "die" not read (only "dead"), a detonate or an animation name taken case-sensitively, the chunk name not
// read, the smoke puff taken for the chunks (or the chunks for a puff), the parts' counts wrong.
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
                                           AsciiEmitter("fire!1", "explosion", ""),
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

// An animation the model hasn't is its supermodel chain's, as the engine plays it; the emitters are the model's own.
// The game's flags: tnp_flag_d03 has its chunk emitters but no animation, its "die" (2 detonates) is tnp_flag_c01's.
// Breaks caught: the supermodel not followed; the supermodel's "die" taken over the model's own; the supermodel's
// emitters taken for the model's; a chain of two not followed; a missing link or a loop (a -> b -> a) crashing or
// hanging; the chain followed past eight models.
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

// Every placeables.2da model: the recon's classification read from each model's own animations (it followed no
// supermodel) - "die" with detonate and chunk emitters 525, "die" with a smoke puff only 181, "die" with no debris 71, no
// "die" 491, 36 not in the game - and the rule's count. By the models' own animations 528: the 525 and the three whose
// detonates are only in "dead" (the Cot, the Birdbath, Flag3). As the game plays them (an animation a model hasn't is
// its supermodel's) 679 from the game's files - 151 more, models with chunk emitters of their own and no "die" whose
// supermodel's "die" detonates: 72 flags and banners (tnp_flag_*, dag_*flag*, pwc_bann_*: tnp_flag_a01 / b01 / c01 /
// e01 ...), 59 trees (tnp_tree_a01's), 14 balconies, 4 windows, a scaffold and a bench - and 676 as served (the three
// lose theirs to LD's copies).
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
  CHECK_EQ(rule_game, 679);
  CHECK_EQ(rule_served, 676);
  CHECK_EQ(inherited, 151);
}
