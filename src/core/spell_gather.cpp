// #166 / #184 decision A: NWN's own area spells' gathers leave out a creature behind Total Cover (spell_gather.h).
//
// The engine's command (nwmain 1.87.8193.35, read in its disassembly): ExecuteCommandGetObjectInShape(this, command,
// parameters) pops the shape (int), size (float), location (engine structure 2), line of sight (int), object filter
// (int) and origin (vector); command 0x80 (GetFirst) starts at CNWSArea::GetFirstObjectIndiceByX, 0x81 (GetNext) at the
// index after the one it returned last (kept in the area, +0x238); it scans on through the area's objects sorted by x,
// tests each against the shape (and ClearLineOfSight when asked), keeps the index of the one it returns, and pushes its
// id (OBJECT_INVALID at the end). The VM's pop and push of an engine structure each copy it (the implementer's
// CopyGameDefinedStructure), so the arguments popped here are pushed again, copied, for each call, and the location
// freed once (DestroyGameDefinedStructure). A creature skipped: the command run again as GetNext with the same
// arguments, which goes on from it.

#include "spell_gather.h"

#include <ctype.h>
#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "creature.h"
#include "data_serving.h"
#include "ee_layout.h"
#include "hooks.h"
#include "light_sources.h"
#include "log.h"
#include "object_cover.h"
#include "rules/area_origin.h"
#include "rules/spells2024.h"
#include "script_hooks.h"

namespace {

struct Vec3 {
  float x, y, z;
};
using VmCommandFn = int (*)(void* commands, int command, int parameters);
using PopIntFn = int (*)(void* vm, int* value);
using PopFloatFn = int (*)(void* vm, float* value);
using PopEngineFn = int (*)(void* vm, int type, void** value);
using PopVectorFn = int (*)(void* vm, Vec3* value);
using PopObjectFn = int (*)(void* vm, uint32_t* value);
using PushIntFn = int (*)(void* vm, int value);
using PushFloatFn = int (*)(void* vm, float value);
using PushEngineFn = int (*)(void* vm, int type, void* value);
using PushVectorFn = int (*)(void* vm, Vec3 value);
using PushObjectFn = int (*)(void* vm, uint32_t value);
using DestroyFn = void (*)(void* commands, int type, void* value);

constexpr const char* kShapeCommand = "?ExecuteCommandGetObjectInShape@CNWSVirtualMachineCommands@@QEAAHHH@Z";
constexpr const char* kPopFloat = "?StackPopFloat@CVirtualMachine@@QEAAHPEAM@Z";
constexpr const char* kPopEngine = "?StackPopEngineStructure@CVirtualMachine@@QEAAHHPEAPEAX@Z";
constexpr const char* kPopVector = "?StackPopVector@CVirtualMachine@@QEAAHPEAVVector@@@Z";
constexpr const char* kPushFloat = "?StackPushFloat@CVirtualMachine@@QEAAHM@Z";
constexpr const char* kPushEngine = "?StackPushEngineStructure@CVirtualMachine@@QEAAHHPEAX@Z";
constexpr const char* kPushVector = "?StackPushVector@CVirtualMachine@@QEAAHVVector@@@Z";
constexpr const char* kDestroy = "?DestroyGameDefinedStructure@CNWSVirtualMachineCommands@@UEAAXHPEAX@Z";
constexpr int kGetFirst = 0x80, kGetNext = 0x81;
constexpr int kLocation = 2;            // ENGINE_STRUCTURE_LOCATION
constexpr int kFilterCreature = 1;      // OBJECT_TYPE_CREATURE
constexpr int kPopFailed = -639;        // the command's own answer when an argument isn't there (0xFFFFFD81)
constexpr int kPushFailed = -638;       // and when its result can't be pushed (0xFFFFFD82)
constexpr int kMostSkips = 512;

struct Api {
  PopIntFn pop_int = reinterpret_cast<PopIntFn>(EngineExport(ee::kStackPopInteger));
  PopFloatFn pop_float = reinterpret_cast<PopFloatFn>(EngineExport(kPopFloat));
  PopEngineFn pop_engine = reinterpret_cast<PopEngineFn>(EngineExport(kPopEngine));
  PopVectorFn pop_vector = reinterpret_cast<PopVectorFn>(EngineExport(kPopVector));
  PopObjectFn pop_object = reinterpret_cast<PopObjectFn>(EngineExport(ee::kStackPopObject));
  PushIntFn push_int = reinterpret_cast<PushIntFn>(EngineExport(ee::kStackPushInteger));
  PushFloatFn push_float = reinterpret_cast<PushFloatFn>(EngineExport(kPushFloat));
  PushEngineFn push_engine = reinterpret_cast<PushEngineFn>(EngineExport(kPushEngine));
  PushVectorFn push_vector = reinterpret_cast<PushVectorFn>(EngineExport(kPushVector));
  PushObjectFn push_object = reinterpret_cast<PushObjectFn>(EngineExport(ee::kStackPushObject));
  DestroyFn destroy = reinterpret_cast<DestroyFn>(EngineExport(kDestroy));
  void** vm = reinterpret_cast<void**>(EngineExport(ee::kVirtualMachine));
  bool Whole() const {
    return pop_int && pop_float && pop_engine && pop_vector && pop_object && push_int && push_float && push_engine &&
           push_vector && push_object && destroy && vm;
  }
};

const Api& E() {
  static const Api api;
  return api;
}

struct Run {
  uint32_t self = 0;
  int spell = -1;  // -1: not an NWN area spell's impact script
};
std::vector<Run> g_runs;  // the scripts running now, the innermost last
std::unordered_map<std::string, std::vector<int>> g_scripts;  // an impact script (lower case) -> its NWN area spells
bool g_built = false;
VmCommandFn g_shape = nullptr;

void Build() {
  g_built = true;
  const std::vector<std::string> impact = GameTableColumn("spells", "ImpactScript");
  const std::vector<std::string> shapes = GameTableColumn("spells", "TargetShape");
  int spells = 0;
  for (size_t row = 0; row < impact.size(); ++row) {
    std::string name = impact[row];
    for (char& c : name) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    // nwn5e's own (its rows, its n5_ scripts) leave a creature out in their own code.
    const bool converted = rules::Spell2024Of(static_cast<int>(row)) != nullptr || name.rfind("n5", 0) == 0;
    if (name.empty() || !rules::NwnAreaSpell(row < shapes.size() ? shapes[row] : "", converted)) continue;
    g_scripts[name].push_back(static_cast<int>(row));
    ++spells;
  }
  Log("spell gather: %d of NWN's area spells (%zu impact scripts) gather without the creatures behind Total Cover", spells,
      g_scripts.size());
}

void Starts(uint32_t self, const char* name, uint32_t length) {
  Run run;
  run.self = self;
  if (name && length > 0 && length <= 16) {
    char lower[17];
    for (uint32_t i = 0; i < length; ++i) lower[i] = static_cast<char>(tolower(static_cast<unsigned char>(name[i])));
    lower[length] = '\0';
    if (!g_built) Build();
    const auto found = g_scripts.find(lower);
    void* object = found == g_scripts.end() ? nullptr : GameObjectById(self);
    if (object && Field<uint8_t>(object, ee::kGameObjectType) == ee::kObjectTypeCreature) {
      const int spell = Field<int32_t>(object, ee::kObjectLastSpellId);
      for (int one : found->second) {
        if (one == spell) run.spell = spell;
      }
    }
  }
  if (g_runs.size() > 256) g_runs.clear();  // (never: each start has its end)
  g_runs.push_back(run);
}

void Ends(uint32_t) {
  if (!g_runs.empty()) g_runs.pop_back();
}

std::string SpellWords(int spell) {
  static const std::vector<std::string> labels = GameTableColumn("spells", "Label");
  std::string label = static_cast<size_t>(spell) < labels.size() ? labels[static_cast<size_t>(spell)] : "";
  for (char& c : label) c = c == '_' ? ' ' : c;
  return (label.empty() ? std::string("spell") : label) + " (" + std::to_string(spell) + ")";
}

// The command, while an NWN area spell's impact script runs on its OBJECT_SELF: the creatures behind Total Cover from
// its point of origin skipped (the command run again as GetNext from each).
int ShapeCommand(void* commands, int command, int parameters) {
  const Run run = g_runs.empty() ? Run() : g_runs.back();
  if ((command != kGetFirst && command != kGetNext) || run.spell < 0 || !commands ||
      Field<uint32_t>(commands, ee::kVmCommandsObjectSelf) != run.self || !E().Whole() || !*E().vm) {
    return g_shape(commands, command, parameters);
  }
  void* vm = *E().vm;
  int shape = 0, sight = 0, filter = 0;
  float size = 0.0f;
  void* location = nullptr;
  Vec3 origin{0.0f, 0.0f, 0.0f};
  if (!E().pop_int(vm, &shape) || !E().pop_float(vm, &size) || !E().pop_engine(vm, kLocation, &location)) {
    return kPopFailed;
  }
  if (!E().pop_int(vm, &sight) || !E().pop_int(vm, &filter) || !E().pop_vector(vm, &origin)) {
    E().destroy(commands, kLocation, location);
    return kPopFailed;
  }
  int result = 0;
  int call = command;
  for (int skips = 0; skips <= kMostSkips; ++skips) {
    if (!E().push_vector(vm, origin) || !E().push_int(vm, filter) || !E().push_int(vm, sight) ||
        !E().push_engine(vm, kLocation, location) || !E().push_float(vm, size) || !E().push_int(vm, shape)) {
      result = kPopFailed;
      break;
    }
    result = g_shape(commands, call, parameters);
    if (result != 0) break;
    uint32_t id = ee::kObjectInvalid;
    if (!E().pop_object(vm, &id)) {
      result = kPushFailed;
      break;
    }
    const bool skip = id != ee::kObjectInvalid && (filter & kFilterCreature) && skips < kMostSkips && CreatureById(id) &&
                      LeftOutOfSpellGather(run.self, run.spell, id, SpellWords(run.spell));
    if (!skip) {
      if (!E().push_object(vm, id)) result = kPushFailed;
      break;
    }
    call = kGetNext;  // on from the one left out
  }
  E().destroy(commands, kLocation, location);
  return result;
}

}  // namespace

bool SpellGatherRunning(uint32_t self, int spell) {
  return !g_runs.empty() && g_runs.back().spell == spell && spell >= 0 && g_runs.back().self == self;
}

void InstallSpellGather() {
  if (!CoverObjectsOn()) {
    Log("spell gather: off (cover_objects=0, not the faithful rules, or no placeable bounds) - NWN's own area spells "
        "keep a creature behind Total Cover in, at +5 on a Dexterity save");
    return;
  }
  if (!E().Whole()) {
    Log("spell gather: off - the VM's stack functions aren't all exported");
    return;
  }
  const bool hooked = HookExport("script GetFirst/NextObjectInShape (#184 decision A: NWN's area spells, Total Cover)",
                                 kShapeCommand, reinterpret_cast<void*>(&ShapeCommand),
                                 reinterpret_cast<void**>(&g_shape));
  const bool heard = hooked && AddScriptStartListener(&Starts) && AddScriptEndListener(&Ends);
  Log("spell gather: %s - NWN's own area spells (their impact scripts' GetFirst/NextObjectInShape) leave out a creature "
      "behind Total Cover from the spell's point of origin, as nwn5e's do; Half and Three-Quarters stay in",
      heard ? "on" : "OFF (the command or the script listeners not hooked)");
}
