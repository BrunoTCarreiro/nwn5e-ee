// #144 (owner, 2026-11-08: "I'd go for engine work as it allows for more flexibility with effects later by allowing
// nodes to follow body parts"): visual effects that follow a body part.
//
// The client pins every model it attaches - a visual effect's among them - with Gob::AttachToObject(target, node
// name, mode): it finds the target model's node by name (GetPart, vtable +0x230) and gives the model an attach
// behaviour that keeps it on that node every frame (mode 0 CAurBehaviorAttach: the node's position and turning;
// 3 CAurBehaviorAttachNoOrientation: its position only; 1 / 2 / 4: translate, rotate, GUI scale). The game itself
// names only the head, the chest ("impact") and the feet ("root").
//
// A model that a visualeffects.2da row sends to a node (NWN5E_Node, data/effect_nodes.h) is attached to that node
// instead, when the target's model has it (otherwise where the game puts it: a creature without hands, a
// placeable). NWN5E_NodeUpright 1 attaches it by position only (a flame stays upright as the hand turns); otherwise
// it turns with the node. Only a plain attach (mode 0 or 3) changes mode. AttachToPart (a node already found) is
// treated the same. The rows are read from the served visualeffects.2da at the first rules load.
//
// 0.95.7: after a position-only attach (a row without OrientWithObject) the client turns the effect's root part to
// the world's axes (Gob::SetPartPosition("root", its position, identity), right after the attach). For an effect
// this hook turned to mode 0 that call is skipped (data::SkipsWorldAlignment), or it would start on the world's
// axes and turn from there.
//
// The VFX bench (the dev layer, through DevSeam::set_effect_node) sets a model's node at run time, before its
// row's: its preview goes on the hand while it plays.
//
// `!vfxnode <row> [seconds]` (debug_test_commands) puts a row's visual effect on you; `!vfxnode off` ends it.

#include <windows.h>

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>
#include <vector>

#include "chat.h"
#include "config.h"
#include "creature.h"
#include "data/effect_nodes.h"
#include "data/twoda.h"
#include "data_serving.h"
#include "dev_seam.h"
#include "ee_layout.h"
#include "effects.h"
#include "hooks.h"
#include "log.h"
#include "rules_loaded.h"

namespace {

constexpr const char* kAttachToObject = "?AttachToObject@Gob@@UEAAPEAXPEAVCAurObject@@PEBDH@Z";
constexpr const char* kAttachToPart = "?AttachToPart@Gob@@UEAAPEAXPEAVCAurObject@@PEAVCAurPart@@H@Z";
constexpr const char* kGetModelName = "?GetModelName@Gob@@UEAAPEBDXZ";
constexpr const char* kGetPart = "?GetPart@Gob@@UEAAPEAVCAurPart@@PEBD@Z";
constexpr const char* kSetPartPosition = "?SetPartPosition@Gob@@UEAAHPEBDVVector@@VQuaternion@@@Z";
constexpr const char* kRttiGob = ".?AVGob@@";
constexpr int kModelNameSlot = 0x18 / 8;  // Gob vtable: GetModelName
constexpr int kAttachSlot = 0x98 / 8;     // Gob vtable: AttachToObject
constexpr int kGetPartSlot = 0x230 / 8;   // CAurObject / Gob vtable: GetPart(name) - AttachToObject's own lookup
constexpr int kSetPartPositionSlot = 0x130 / 8;  // Gob vtable: SetPartPosition - the client's world alignment
constexpr uint64_t kAlignmentWindowMs = 1000;    // the alignment follows the attach in the same call
constexpr int kSkipLogCap = 16;
// AttachToObject's lookup of the node on the target: call qword ptr [rax + 0x230].
constexpr uint8_t kGetPartCall[] = {0xff, 0x90, 0x30, 0x02, 0x00, 0x00};
constexpr size_t kOwnLogCap = 64;       // nwn5e's models (n5...) and every move, logged once each
constexpr size_t kGameLogCap = 24;      // the game's effect models (v...): how it attaches them
constexpr int kDefaultSeconds = 30;
constexpr int kMaxSeconds = 600;
constexpr const char* kTestTag = "n5_vfxnode";

using AttachToObjectFn = void* (*)(void* self, void* target, const char* node, int mode);
using AttachToPartFn = void* (*)(void* self, void* target, void* part, int mode);
using ModelNameFn = const char* (*)(void* gob);
using GetPartFn = void* (*)(void* object, const char* name);
// Vector and Quaternion by value: by a pointer to a copy (x64). The quaternion is w, x, y, z.
using SetPartPositionFn = int (*)(void* self, const char* part, const float* position, const float* turn);

AttachToObjectFn g_attach_to_object = nullptr;
AttachToPartFn g_attach_to_part = nullptr;
SetPartPositionFn g_set_part_position = nullptr;
std::mutex g_turning_mutex;
std::map<void*, uint64_t> g_turning;  // effects turned to mode 0, when: their world alignment is skipped once
std::atomic<int> g_skip_logs{0};
std::shared_mutex g_table_mutex;    // g_rows, g_overrides
data::EffectNodes g_rows;           // the served visualeffects.2da's, read at the first rules load
data::EffectNodeOverrides g_overrides;  // the VFX bench's (DevSeam::set_effect_node): before the rows
std::atomic<bool> g_loaded{false};
std::mutex g_log_mutex;
std::set<std::string> g_logged_own;   // models already logged
std::set<std::string> g_logged_game;
std::atomic<bool> g_own_full{false};
std::atomic<bool> g_game_full{false};

const char* ModelNameOf(void* gob) {
  const uintptr_t* vtable = *static_cast<const uintptr_t* const*>(gob);
  return vtable ? reinterpret_cast<ModelNameFn>(vtable[kModelNameSlot])(gob) : nullptr;
}

void* PartOf(void* object, const char* name) {
  const uintptr_t* vtable = *static_cast<const uintptr_t* const*>(object);
  return vtable ? reinterpret_cast<GetPartFn>(vtable[kGetPartSlot])(object, name) : nullptr;
}

std::string Lower(const char* text) {
  std::string out = text ? text : "";
  for (char& c : out) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return out;
}

// An effect turned to mode 0: the client's world alignment right after its attach is skipped (stale ones forgotten).
void RememberTurning(void* self) {
  const uint64_t now = GetTickCount64();
  std::lock_guard<std::mutex> lock(g_turning_mutex);
  for (auto it = g_turning.begin(); it != g_turning.end();) {
    it = now - it->second > kAlignmentWindowMs ? g_turning.erase(it) : std::next(it);
  }
  g_turning[self] = now;
}

bool TakeTurning(void* self) {
  std::lock_guard<std::mutex> lock(g_turning_mutex);
  const auto found = g_turning.find(self);
  if (found == g_turning.end()) return false;
  const bool fresh = GetTickCount64() - found->second <= kAlignmentWindowMs;
  g_turning.erase(found);
  return fresh;
}

// The first attach of each of nwn5e's models (n5...), of every model moved, and of the game's effect models (v...),
// capped apart: the game's node and mode, and what happened.
void Note(const std::string& model, const char* game_node, int game_mode, const std::string& what, bool own) {
  std::atomic<bool>& full = own ? g_own_full : g_game_full;
  if (full.load(std::memory_order_relaxed)) return;
  std::set<std::string>& logged = own ? g_logged_own : g_logged_game;
  const size_t cap = own ? kOwnLogCap : kGameLogCap;
  std::lock_guard<std::mutex> lock(g_log_mutex);
  if (logged.size() >= cap) {
    full = true;
    Log("effect nodes: %zu %s models logged - no more", cap, own ? "of nwn5e's (or moved)" : "of the game's");
    return;
  }
  if (!logged.insert(model).second) return;
  Log("effect nodes: %s attached at \"%s\" (mode %d)%s", model.c_str(), game_node ? game_node : "(a node found first)",
      game_mode, what.c_str());
}

// The node of `target` that `self` goes on instead, with *node / *mode for the attach; nullptr when no row moves its
// model or the target's model lacks that node (the game's attach then).
void* Redirect(void* self, void* target, const char* game_node, int game_mode, std::string* node, int* mode) {
  if (!self || !target) return nullptr;
  const char* name = ModelNameOf(self);
  if (!name || !*name) return nullptr;
  const std::string model = Lower(name);
  data::EffectNode where;
  bool found = false;
  {
    std::shared_lock<std::shared_mutex> lock(g_table_mutex);
    found = data::FindEffectNode(g_rows, g_overrides, model, &where);
  }
  if (!found) {
    const bool own = model.compare(0, 2, "n5") == 0;
    if (own || model[0] == 'v') Note(model, game_node, game_mode, "", own);
    return nullptr;
  }
  void* part = PartOf(target, where.node.c_str());
  if (!part) {
    Note(model, game_node, game_mode, " - stays: the target's model has no \"" + where.node + "\"", true);
    return nullptr;
  }
  *node = where.node;
  *mode = data::EffectAttachMode(where, game_mode);
  if (data::SkipsWorldAlignment(game_mode, *mode)) RememberTurning(self);
  Note(model, game_node, game_mode, " - moved to \"" + where.node + "\" (mode " + std::to_string(*mode) + ")",
       true);
  return part;
}

void* AttachToObject(void* self, void* target, const char* node, int mode) {
  std::string moved;
  int moved_mode = mode;
  if (Redirect(self, target, node, mode, &moved, &moved_mode)) {
    return g_attach_to_object(self, target, moved.c_str(), moved_mode);
  }
  return g_attach_to_object(self, target, node, mode);
}

void* AttachToPart(void* self, void* target, void* part, int mode) {
  std::string moved;
  int moved_mode = mode;
  if (void* other = Redirect(self, target, nullptr, mode, &moved, &moved_mode)) {
    return g_attach_to_part(self, target, other, moved_mode);
  }
  return g_attach_to_part(self, target, part, mode);
}

// The client's world alignment after a position-only attach, skipped for an effect turned to mode 0.
int SetPartPosition(void* self, const char* part, const float* position, const float* turn) {
  if (self && data::IsWorldAlignment(part, turn) && TakeTurning(self)) {
    if (g_skip_logs.fetch_add(1) < kSkipLogCap) {
      Log("effect nodes: %s turns with its node from the start (the game's world alignment skipped)",
          Lower(ModelNameOf(self)).c_str());
    }
    return 1;
  }
  return g_set_part_position(self, part, position, turn);
}

void LoadNodes(void* /*rules*/, bool /*reload*/) {
  if (g_loaded.load()) return;  // served once, at start
  std::string bytes, why, error;
  data::TwoDA table;
  if (!ReadGameResource("visualeffects", ee::kResourceType2da, &bytes, &why)) {
    Log("SKIP effect nodes: visualeffects.2da not read (%s)", why.c_str());
    return;
  }
  if (!data::ParseTwoDA(bytes, &table, &error)) {
    Log("SKIP effect nodes: visualeffects.2da not parsed (%s)", error.c_str());
    return;
  }
  data::EffectNodes nodes = data::EffectNodesFrom(table);
  for (const std::string& line : nodes.refused) Log("effect nodes: %s", line.c_str());
  std::string list;
  for (const auto& entry : nodes.models) {
    if (list.size() > 400) {
      list += " ...";
      break;
    }
    list += (list.empty() ? "" : ", ") + entry.first + " -> " + entry.second.node + (entry.second.upright ? " (upright)" : "");
  }
  Log("effect nodes: %zu models follow a node%s%s", nodes.models.size(), list.empty() ? "" : ": ", list.c_str());
  {
    std::unique_lock<std::shared_mutex> lock(g_table_mutex);
    g_rows = std::move(nodes);
  }
  g_loaded = true;
}

// DevSeam::set_effect_node (dev_seam.h): the VFX bench's preview model on a node, before its row's.
bool SetEffectNode(const char* model, const char* node, bool upright) {
  const std::string name = Lower(model);
  const std::string where = Lower(node);
  if (name.empty() || (!where.empty() && !data::ValidNodeName(where))) {
    Log("effect nodes: the bench's node \"%s\" for \"%s\" refused (an empty model or a bad node name)", where.c_str(),
        name.c_str());
    return false;
  }
  {
    std::unique_lock<std::shared_mutex> lock(g_table_mutex);
    if (where.empty()) {
      g_overrides.erase(name);
    } else {
      data::EffectNode set;
      set.node = where;
      set.upright = upright;
      g_overrides[name] = set;
    }
  }
  {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    g_logged_own.erase(name);  // its next attach is logged again
  }
  if (where.empty()) {
    Log("effect nodes: the bench's node for %s cleared", name.c_str());
  } else {
    Log("effect nodes: the bench puts %s on \"%s\"%s", name.c_str(), where.c_str(), upright ? " (upright)" : "");
  }
  return true;
}

std::vector<std::string> VfxNodeCommand(void* creature, const std::string& args) {
  const uint32_t id = ObjectIdOf(StatsOf(creature));
  std::string text = Lower(args.c_str());
  while (!text.empty() && text.back() == ' ') text.pop_back();
  if (text == "off") {
    const std::string code = std::string("effect e = GetFirstEffect(OBJECT_SELF); while (GetIsEffectValid(e)) { "
                                         "if (GetEffectTag(e) == \"") + kTestTag + "\") RemoveEffect(OBJECT_SELF, e); "
                                         "e = GetNextEffect(OBJECT_SELF); }";
    RunScriptChunk(code.c_str(), id);
    return {"nwn5e: !vfxnode effects removed."};
  }
  char* end = nullptr;
  const long row = strtol(text.c_str(), &end, 10);
  if (text.empty() || row < 0 || row > 65535 || (*end != '\0' && *end != ' ')) {
    return {"nwn5e: !vfxnode <visualeffects row> [seconds] - puts that visual effect on you (default 30 s); "
            "!vfxnode off ends them. The node tests: 1845 right hand (upright), 1846 left hand (turning with it), "
            "1847 head."};
  }
  int seconds = *end == ' ' ? atoi(end + 1) : kDefaultSeconds;
  if (seconds <= 0) seconds = kDefaultSeconds;
  if (seconds > kMaxSeconds) seconds = kMaxSeconds;
  const std::string code = "ApplyEffectToObject(DURATION_TYPE_TEMPORARY, TagEffect(EffectVisualEffect(" +
                           std::to_string(row) + "), \"" + kTestTag + "\"), OBJECT_SELF, " + std::to_string(seconds) +
                           ".0);";
  RunScriptChunk(code.c_str(), id);
  Log("effect nodes: 0x%08X !vfxnode %ld for %d s", id, row, seconds);
  return {"nwn5e: visual effect " + std::to_string(row) + " on you for " + std::to_string(seconds) +
          " s (!vfxnode off ends it)."};
}

}  // namespace

void InstallVfxNodes() {
  const uintptr_t attach = EngineExport(kAttachToObject);
  const uintptr_t vtable = FindVtable("effect nodes: Gob's vtable", kRttiGob);
  const CodeRange body = FunctionAt(attach);
  const bool anchored = attach && vtable && VtableSlot(vtable, kAttachSlot) == attach &&
                        VtableSlot(vtable, kModelNameSlot) == EngineExport(kGetModelName) &&
                        VtableSlot(vtable, kGetPartSlot) == EngineExport(kGetPart) && body.Valid() &&
                        ContainsBytes(body.begin, body.end, kGetPartCall, sizeof(kGetPartCall));
  if (!anchored) {
    Log("SKIP effect nodes: Gob's attach call isn't as read (vtable slots 0x18 / 0x98 / 0x230) - effects stay where "
        "the game puts them");
    return;
  }
  if (!HookExport("Gob::AttachToObject (an effect on the node its row names)", kAttachToObject,
                  reinterpret_cast<void*>(&AttachToObject), reinterpret_cast<void**>(&g_attach_to_object)) ||
      !HookExport("Gob::AttachToPart (an effect on the node its row names)", kAttachToPart,
                  reinterpret_cast<void*>(&AttachToPart), reinterpret_cast<void**>(&g_attach_to_part))) {
    return;
  }
  if (VtableSlot(vtable, kSetPartPositionSlot) != EngineExport(kSetPartPosition) ||
      !HookExport("Gob::SetPartPosition (a turning effect not aligned to the world's axes)", kSetPartPosition,
                  reinterpret_cast<void*>(&SetPartPosition), reinterpret_cast<void**>(&g_set_part_position))) {
    g_set_part_position = nullptr;
    Log("SKIP effect nodes: Gob::SetPartPosition isn't as read (vtable +0x130) - a turning effect starts on the "
        "world's axes");
  }
  OnRulesLoaded(&LoadNodes);
  g_dev_seam.set_effect_node = &SetEffectNode;
  if (g_config.debug_test_commands) AddChatCommand("!vfxnode", &VfxNodeCommand);
}
