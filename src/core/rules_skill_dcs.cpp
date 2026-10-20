// SK3 (port of 1.69's rules_skill_dcs.cpp; docs/EE-PORT.md E3d, EP11), with
// `skills` faithful: NWN's fixed skill DCs on the 2024 scale. Skill totals are
// 2024's (the GetSkillRank hook, SK2), so every engine check that compares one
// with a 3e DC uses the converted DC — 10 + (DC - 10) / 2, a DC of 1 or less
// kept (rules::ConvertNwnSkillDc). EE has no DC getters, so per check:
// - scripts' GetIsSkillSuccessful: the DC is converted on the script stack
//   before the engine reads it (so its combat log line shows it);
// - the engine actions that read the DC after the skill (unlock, lock, the
//   trap actions, Tumble against opportunity attacks, Spellcraft identify,
//   the Examine Trap window, scripts' "can the lock be picked"), and Search
//   (its DC sits in a register at the skill call): the skill value is shifted
//   by DC - converted DC while that check reads it — the same outcome — and
//   their combat log line is set back to the 2024 skill and DC (EP11);
// - Set Trap's DC and a heal kit's poison / disease DCs: converted as the
//   engine reads them from traps.2da / poison.2da / disease.2da;
// - Pick Pocket and Animal Empathy DCs: their ruleset values.
// Lore identification compares the total with SkillVsItemCost.2da rows (a
// skill value, not a DC): the 2024 total is scaled to the 3e range there (2 x,
// as for scripts, SK3 a). A trap a character sets stores its DCs as the trap's
// base + the setter's skill: that skill counts twice, so the later conversion
// lands right. Owner, 2026-10-19: no trap synergy, no Rogue-only traps; "take
// 20" out of combat stays NWN's (EP11).
//
// SK11 (`conditions` faithful): GetIsSkillSuccessful's d20 has disadvantage
// when the creature is poisoned or frightened (its fear's source in sight); a
// deafened one listening or a blinded one spotting or searching rolls a 1.
// Search's die: a blinded searcher rolls 0, a poisoned or frightened one the
// lower of two.

#include <windows.h>

#include <intrin.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>

#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "dice.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "ruleset.h"
#include "rules/skills.h"
#include "skill_rank.h"

namespace {

using ActionFn = uint32_t (*)(void* object, void* node);
using VmCommandFn = int (*)(void* commands, int command, int parameters);
using SpellCastFn = void (*)(void* creature, uint32_t spell, uint8_t a, uint16_t b);
using CounterSpellFn = void (*)(void* object, void* spell, void* message);
using ExamineGuiFn = int (*)(void* message, void* player, uint32_t trap, void* creature, int a);
using UpdateTrapCheckFn = void (*)(void* creature);
using BroadcastSkillDataFn = void (*)(void* creature, void* message);
using GetIntEntryFn = int (*)(void* table, int row, const void* column, int* out);
using PopIntFn = int (*)(void* vm, int* value);
using PopObjectFn = int (*)(void* vm, uint32_t* value);
using PushIntFn = int (*)(void* vm, int value);
using PushObjectFn = int (*)(void* vm, uint32_t value);
using GetGameObjectFn = void* (*)(void* app, uint32_t id);
using CastFn = void* (*)(void* object);
using MessageGetFn = int (*)(void* message, int index);
using MessageSetFn = void (*)(void* message, int index, int value);
using GetSpellFn = void* (*)(const void* spells, int id);
using GetLevelFn = uint8_t (*)(void* stats, int subtract_negative);
using GetBlindFn = int (*)(void* creature);

struct Api {
  PopIntFn pop_int = reinterpret_cast<PopIntFn>(EngineExport(ee::kStackPopInteger));
  PopObjectFn pop_object = reinterpret_cast<PopObjectFn>(EngineExport(ee::kStackPopObject));
  PushIntFn push_int = reinterpret_cast<PushIntFn>(EngineExport(ee::kStackPushInteger));
  PushObjectFn push_object = reinterpret_cast<PushObjectFn>(EngineExport(ee::kStackPushObject));
  void** vm = reinterpret_cast<void**>(EngineExport(ee::kVirtualMachine));
  GetGameObjectFn game_object = reinterpret_cast<GetGameObjectFn>(EngineExport(ee::kGetGameObject));
  void** app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  MessageGetFn message_get = reinterpret_cast<MessageGetFn>(EngineExport(ee::kMessageGetInteger));
  MessageSetFn message_set = reinterpret_cast<MessageSetFn>(EngineExport(ee::kMessageSetInteger));
  GetSpellFn get_spell = reinterpret_cast<GetSpellFn>(EngineExport(ee::kGetSpell));
  void** rules = reinterpret_cast<void**>(EngineExport(ee::kRules));
  GetLevelFn level = reinterpret_cast<GetLevelFn>(EngineExport(ee::kGetLevel));
  GetBlindFn blind = reinterpret_cast<GetBlindFn>(EngineExport(ee::kGetBlind));
};

const Api& E() {
  static const Api api;
  return api;
}

// --- Where the engine's checks are (found at load) --------------------------

enum class Kind { kNone, kUnlock, kLock, kDisarm, kFlag, kRecover, kExamine };

struct ActionHook {
  Kind kind;
  const char* export_name;
  const char* what;
  int skill;
  ActionFn original = nullptr;
  CodeRange code;
};
ActionHook g_actions[] = {
    {Kind::kUnlock, ee::kAIActionUnlockObject, "Open Lock: unlock", ee::kSkillOpenLock},
    {Kind::kLock, ee::kAIActionLockObject, "Open Lock: lock", ee::kSkillOpenLock},
    {Kind::kDisarm, ee::kAIActionDisarmTrap, "Disarm Trap", ee::kSkillDisableTrap},
    {Kind::kFlag, ee::kAIActionFlagTrap, "Flag Trap", ee::kSkillDisableTrap},
    {Kind::kRecover, ee::kAIActionRecoverTrap, "Recover Trap", ee::kSkillDisableTrap},
    {Kind::kExamine, ee::kAIActionExamineTrap, "Examine Trap", ee::kSkillDisableTrap},
};

CodeRange g_search_code;        // UpdateTrapCheck
CodeRange g_aoo_code;           // BroadcastAttackOfOpportunity (Tumble)
CodeRange g_spellcast_code;     // BroadcastSpellCast (Spellcraft identify)
CodeRange g_counterspell_code;  // BroadcastCounterSpellData (Spellcraft identify)
CodeRange g_examine_gui_code;   // SendServerToPlayerExamineGui_TrapData
CodeRange g_heal_code;          // AIActionHeal
CodeRange g_settrap_code;       // AIActionSetTrap
uintptr_t g_lore_caller = 0;
uintptr_t g_settrap_caller = 0;
uintptr_t g_door_action_caller = 0;
uintptr_t g_skill_check_d20 = 0;
std::vector<uintptr_t> g_search_dice;

VmCommandFn g_skill_success_command = nullptr;
VmCommandFn g_door_action_command = nullptr;
SpellCastFn g_spell_cast = nullptr;
CounterSpellFn g_counter_spell = nullptr;
ExamineGuiFn g_examine_gui = nullptr;
UpdateTrapCheckFn g_update_trap_check = nullptr;
BroadcastSkillDataFn g_broadcast_skill_data = nullptr;
GetIntEntryFn g_get_int_entry = nullptr;
ActionFn g_animal_empathy = nullptr;

// --- What the engine's thread is checking ------------------------------------

struct Action {
  Kind kind = Kind::kNone;
  uint32_t target = ee::kObjectInvalid;
  const ActionHook* hook = nullptr;
};
struct LogFix {
  bool active = false;
  CodeRange code;
  int shift = 0;
  int dc = 0;
};
struct Check {
  void* creature = nullptr;
  int skill = -1;
};
thread_local Action t_action;
thread_local LogFix t_log_fix;
thread_local Check t_check;  // GetIsSkillSuccessful
thread_local void* t_searcher = nullptr;
thread_local int t_spell_id = -1;
thread_local const void* t_counter_spell = nullptr;
thread_local uint32_t t_examine_trap = ee::kObjectInvalid;
thread_local uint32_t t_door_target = ee::kObjectInvalid;
thread_local int t_settrap_skill = INT_MIN;
thread_local int t_empathy_hd = -1;
LogOnce g_logged(400);
int g_check_lines = 0;

bool ConditionsFaithful() { return g_config.conditions == rules::RuleVariant::kFaithful; }
uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }

void* ServerApp() {
  return E().app_manager && *E().app_manager ? Field<void*>(*E().app_manager, ee::kAppManagerServerApp) : nullptr;
}

void* Cast(void* object, size_t slot) {
  const auto cast = (*reinterpret_cast<CastFn* const*>(object))[slot / sizeof(void*)];
  return cast ? cast(object) : nullptr;
}

// A door's, placeable's or trigger's lock and trap DCs (0 where it has none).
struct Dcs {
  bool known = false;
  int unlock = 0;
  int lock = 0;
  int detect = 0;
  int disarm = 0;
};

Dcs DcsOf(uint32_t id) {
  Dcs dcs;
  void* app = ServerApp();
  void* object = app && E().game_object && id != ee::kObjectInvalid ? E().game_object(app, id) : nullptr;
  if (!object) return dcs;
  switch (Field<uint8_t>(object, ee::kGameObjectType)) {
    case ee::kObjectTypeDoor:
      if (void* door = Cast(object, ee::kVtableAsDoor)) {
        dcs = {true, Field<uint8_t>(door, ee::kDoorUnlockDc), Field<uint8_t>(door, ee::kDoorLockDc),
               Field<uint8_t>(door, ee::kDoorDetectDc), Field<uint8_t>(door, ee::kDoorDisarmDc)};
      }
      break;
    case ee::kObjectTypePlaceable:
      if (void* placeable = Cast(object, ee::kVtableAsPlaceable)) {
        dcs = {true, Field<uint8_t>(placeable, ee::kPlaceableUnlockDc), Field<uint8_t>(placeable, ee::kPlaceableLockDc),
               Field<uint8_t>(placeable, ee::kPlaceableDetectDc), Field<uint8_t>(placeable, ee::kPlaceableDisarmDc)};
      }
      break;
    case ee::kObjectTypeTrigger:
      if (void* trigger = Cast(object, ee::kVtableAsTrigger)) {
        dcs = {true, 0, 0, Field<int32_t>(trigger, ee::kTriggerDetectDc), Field<int32_t>(trigger, ee::kTriggerDisarmDc)};
      }
      break;
    default:
      break;
  }
  return dcs;
}

// The DC an action compares with (as the engine forms it after the skill).
int ActionDc(Kind kind, const Dcs& dcs) {
  int dc = 0;
  switch (kind) {
    case Kind::kUnlock: dc = dcs.unlock; break;
    case Kind::kLock: dc = dcs.lock; break;
    case Kind::kDisarm: dc = dcs.disarm; break;
    case Kind::kFlag: dc = dcs.disarm - 5; break;
    case Kind::kRecover: dc = dcs.disarm + 10; break;
    case Kind::kExamine: dc = dcs.disarm - 7; break;
    default: break;
  }
  return dc < 1 ? 1 : dc;
}

int SpellDcOf(const void* spell) { return spell ? Field<uint8_t>(spell, ee::kSpellInnateLevel) + 15 : INT_MIN; }

const void* SpellRow(int id) {
  if (id < 0 || !E().rules || !*E().rules || !E().get_spell) return nullptr;
  void* spells = Field<void*>(*E().rules, ee::kRulesSpells);
  return spells ? E().get_spell(spells, id) : nullptr;
}

void LogConversion(const char* what, int dc, int converted, const char* how) {
  if (g_config.log_attacks && g_logged.First(LogOnce::Key(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(what)),
                                                           dc, converted))) {
    Log("skill DC: %s: 3e %d -> 2024 %d%s", what, dc, converted, how);
  }
}

// --- The skill value at each check -------------------------------------------

int SkillAtCheck(const SkillQuery& query, int value) {
  if (query.base_only) return value;
  const uintptr_t caller = query.caller;
  if (caller == g_lore_caller && query.skill == ee::kSkillLore) {
    const int scaled = rules::ScriptSkillValue2024(value);
    LogConversion("Lore: identify an item", value, scaled, " (a skill value, scaled to the 3e table)");
    return scaled;
  }
  if (caller == g_settrap_caller && query.skill == ee::kSkillSetTrap) {
    t_settrap_skill = value;  // counted again in the trap's stored DCs
    return value;
  }
  int dc = INT_MIN;
  const char* what = nullptr;
  const CodeRange* log_code = nullptr;
  if (t_action.hook && t_action.hook->code.Contains(caller) && query.skill == t_action.hook->skill) {
    dc = ActionDc(t_action.kind, DcsOf(t_action.target));
    what = t_action.hook->what;
    log_code = &t_action.hook->code;
  } else if (g_search_code.Contains(caller) && query.skill == ee::kSkillSearch) {
    CONTEXT context;  // the trap's detect DC is in edi at the call (D)
    if (CallerContext(caller, &context)) dc = static_cast<int32_t>(context.Rdi);
    what = "Search";
  } else if (caller == g_door_action_caller && query.skill == ee::kSkillOpenLock) {
    const Dcs dcs = DcsOf(t_door_target);
    if (dcs.known) dc = dcs.unlock < 1 ? 1 : dcs.unlock;
    what = "scripts: can the lock be picked (skill + 20)";
  } else if (g_spellcast_code.Contains(caller) && query.skill == ee::kSkillSpellcraft) {
    dc = SpellDcOf(SpellRow(t_spell_id));
    what = "Spellcraft: identify a spell";
  } else if (g_counterspell_code.Contains(caller) && query.skill == ee::kSkillSpellcraft) {
    dc = SpellDcOf(t_counter_spell);
    what = "Spellcraft: identify a spell (counterspell)";
  } else if (g_aoo_code.Contains(caller) && query.skill == ee::kSkillTumble) {
    dc = ee::kTumbleDc;
    what = "Tumble against attacks of opportunity";
    log_code = &g_aoo_code;
  } else if (g_examine_gui_code.Contains(caller) && query.skill == ee::kSkillDisableTrap) {
    const Dcs dcs = DcsOf(t_examine_trap);
    if (dcs.known) dc = dcs.disarm;
    what = "Examine Trap window";
  }
  if (dc == INT_MIN) return value;
  const int converted = rules::ConvertNwnSkillDc(dc);
  const int shift = dc - converted;
  if (log_code) t_log_fix = LogFix{true, *log_code, shift, converted};
  LogConversion(what, dc, converted, "");
  return value + shift;
}

// --- Wrappers that tell the checks apart ---------------------------------------

template <int kIndex>
uint32_t ActionWrapper(void* object, void* node) {
  const Action outer = t_action;
  t_action.kind = g_actions[kIndex].kind;
  t_action.hook = &g_actions[kIndex];
  t_action.target = node ? Field<uint32_t>(node, ee::kActionFirstParam) : ee::kObjectInvalid;
  const uint32_t result = g_actions[kIndex].original(object, node);
  t_action = outer;
  return result;
}

constexpr ActionFn kActionWrappers[] = {&ActionWrapper<0>, &ActionWrapper<1>, &ActionWrapper<2>, &ActionWrapper<3>,
                                        &ActionWrapper<4>, &ActionWrapper<5>};

void SpellCast(void* creature, uint32_t spell, uint8_t a, uint16_t b) {
  const int outer = t_spell_id;
  t_spell_id = static_cast<int>(spell);
  g_spell_cast(creature, spell, a, b);
  t_spell_id = outer;
}

void CounterSpell(void* object, void* spell, void* message) {
  const void* outer = t_counter_spell;
  t_counter_spell = spell;
  g_counter_spell(object, spell, message);
  t_counter_spell = outer;
}

int ExamineGui(void* message, void* player, uint32_t trap, void* creature, int a) {
  const uint32_t outer = t_examine_trap;
  t_examine_trap = trap;
  const int result = g_examine_gui(message, player, trap, creature, a);
  t_examine_trap = outer;
  return result;
}

void UpdateTrapCheck(void* creature) {
  void* outer = t_searcher;
  t_searcher = creature;
  g_update_trap_check(creature);
  t_searcher = outer;
}

uint32_t AnimalEmpathy(void* creature, void* node) {
  const int outer = t_empathy_hd;
  void* target = node ? CreatureById(Field<uint32_t>(node, ee::kActionFirstParam)) : nullptr;
  void* stats = target ? StatsOf(target) : nullptr;
  t_empathy_hd = stats && E().level ? E().level(stats, 0) : -1;
  const uint32_t result = g_animal_empathy(creature, node);
  t_empathy_hd = outer;
  return result;
}

// The scripts' GetIsSkillSuccessful(object, skill, DC): the DC converted on
// the stack (2024 skills only; the rest keep NWN's totals and DCs).
int SkillSuccessCommand(void* commands, int command, int parameters) {
  void* vm = E().vm ? *E().vm : nullptr;
  uint32_t object = ee::kObjectInvalid;
  int skill = 0;
  int dc = 0;
  if (vm && E().pop_object(vm, &object) && E().pop_int(vm, &skill) && E().pop_int(vm, &dc)) {
    const int converted = rules::Skill2024ForNwnSkill(skill) ? rules::ConvertNwnSkillDc(dc) : dc;
    if (converted != dc) LogConversion("GetIsSkillSuccessful (scripts)", dc, converted, "");
    E().push_int(vm, converted);
    E().push_int(vm, skill);
    E().push_object(vm, object);
    t_check = Check{CreatureById(object), skill};
  }
  const int result = g_skill_success_command(commands, command, parameters);
  t_check = Check();
  return result;
}

// The scripts' GetIsDoorActionPossible / GetIsPlaceableObjectActionPossible
// (object, action): the object is noted for its unlock test.
int DoorActionCommand(void* commands, int command, int parameters) {
  void* vm = E().vm ? *E().vm : nullptr;
  uint32_t object = ee::kObjectInvalid;
  int action = 0;
  const uint32_t outer = t_door_target;
  if (vm && E().pop_object(vm, &object) && E().pop_int(vm, &action)) {
    E().push_int(vm, action);
    E().push_object(vm, object);
    t_door_target = object;
  }
  const int result = g_door_action_command(commands, command, parameters);
  t_door_target = outer;
  return result;
}

// The combat log's skill line: back to the 2024 skill and DC after a shift.
void BroadcastSkillData(void* creature, void* message) {
  const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
  if (t_log_fix.active && t_log_fix.code.Contains(caller) && message && E().message_get && E().message_set) {
    E().message_set(message, ee::kSkillMessageTotal, E().message_get(message, ee::kSkillMessageTotal) - t_log_fix.shift);
    E().message_set(message, ee::kSkillMessageDc, t_log_fix.dc);
    t_log_fix.active = false;
  }
  g_broadcast_skill_data(creature, message);
}

std::string ColumnName(const void* exo_string) {
  const char* text = exo_string ? Field<const char*>(exo_string, 0) : nullptr;
  return text ? std::string(text) : std::string();
}

// Set Trap's DC and stored DCs, a heal kit's DCs, as the engine reads them.
int GetIntEntry(void* table, int row, const void* column, int* out) {
  const int found = g_get_int_entry(table, row, column, out);
  const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
  if (!found || !out || (!g_heal_code.Contains(caller) && !g_settrap_code.Contains(caller))) return found;
  const std::string name = ColumnName(column);
  if (g_heal_code.Contains(caller) && (name == "Save_DC" || name == "Subs_Save")) {
    const int converted = rules::ConvertNwnSkillDc(*out);
    LogConversion("Heal kit: poison / disease", *out, converted, "");
    *out = converted;
  } else if (g_settrap_code.Contains(caller) && name == "SetDC") {
    const int converted = rules::ConvertNwnSkillDc(*out);
    LogConversion("Set Trap", *out, converted, "");
    *out = converted;
    t_settrap_skill = INT_MIN;  // read again at this check's skill call
  } else if (g_settrap_code.Contains(caller) && (name == "DetectDCMod" || name == "DisarmDCMod") &&
             t_settrap_skill != INT_MIN) {
    *out += t_settrap_skill;  // + the setter's skill once more: base + 2 x skill, as on 1.69
  }
  return found;
}

// --- Ruleset values ------------------------------------------------------------

int ConvertedDc(int engine_value) { return rules::ConvertNwnSkillDc(engine_value); }
int NoRogueOnlyTraps(int) { return 1000; }
int NoSynergy(int) { return 0; }
// Animal Empathy's DC is the target's level + this value.
int EmpathyDcBase(int engine_value) {
  if (t_empathy_hd < 0) return engine_value;
  return rules::ConvertNwnSkillDc(t_empathy_hd + engine_value) - t_empathy_hd;
}

// --- SK11: conditions on the dice ------------------------------------------------

bool Blind(void* creature) { return E().blind && E().blind(creature) != 0; }

bool CheckDice(uintptr_t caller, int count, int sides, int* result) {
  if (!ConditionsFaithful()) return false;
  void* creature = nullptr;
  const char* sense = nullptr;
  bool search = false;
  if (caller == g_skill_check_d20 && t_check.creature) {
    creature = t_check.creature;
    const bool deaf = effects::HasType(creature, ee::kEffectTypeDeaf);
    if (t_check.skill == ee::kSkillListen && deaf) sense = "deafened";
    if ((t_check.skill == ee::kSkillSpot || t_check.skill == ee::kSkillSearch) && Blind(creature)) sense = "blinded";
  } else if (t_searcher && !g_search_dice.empty()) {
    for (uintptr_t die : g_search_dice) search = search || die == caller;
    if (!search) return false;
    creature = t_searcher;
    if (Blind(creature)) sense = "blinded";
  } else {
    return false;
  }
  const std::string worse = CheckDisadvantage(creature);
  if (!sense && worse.empty()) return false;
  const int first = RollEngineDice(count, sides);
  int kept = first;
  int second = 0;
  if (sense) {
    kept = search ? 0 : 1;
  } else {
    second = RollEngineDice(count, sides);
    kept = second < first ? second : first;
  }
  if (g_config.log_attacks && g_check_lines < 200) {
    ++g_check_lines;
    Log("skill check: 0x%08X %s: d%d %d%s%s -> %d (%s)", IdOf(creature), search ? "Search" : "GetIsSkillSuccessful",
        sides, first, second ? " / " : "", second ? std::to_string(second).c_str() : "", kept,
        sense ? (std::string(sense) + ": fails what needs that sense").c_str() : ("disadvantage: " + worse).c_str());
  }
  *result = kept;
  return true;
}

// --- Install -------------------------------------------------------------------------

std::vector<uintptr_t> CallsIn(const char* function, const char* target) {
  return FindCallsTo(FunctionAt(EngineExport(function)), EngineExport(target));
}

uintptr_t OneCall(const char* function, const char* target, const char* what) {
  const std::vector<uintptr_t> calls = CallsIn(function, target);
  if (calls.size() != 1) {
    Log("SKIP skill DC (%s): %zu calls found (expected 1)", what, calls.size());
    return 0;
  }
  return calls[0] + kCallLength;
}

template <typename Fn>
bool Hook(const char* what, const char* export_name, Fn detour, Fn* original) {
  return HookExport(what, export_name, reinterpret_cast<void*>(detour), reinterpret_cast<void**>(original));
}

}  // namespace

void InstallSkillDcRules() {
  if (g_config.skills != rules::RuleVariant::kFaithful) return;
  int hooks = 0;
  for (size_t i = 0; i < sizeof(g_actions) / sizeof(g_actions[0]); ++i) {
    g_actions[i].code = FunctionAt(EngineExport(g_actions[i].export_name));
    char what[96];
    sprintf_s(what, "%s (2024 DC)", g_actions[i].what);
    hooks += Hook(what, g_actions[i].export_name, kActionWrappers[i], &g_actions[i].original) ? 1 : 0;
  }
  g_search_code = FunctionAt(EngineExport(ee::kUpdateTrapCheck));
  g_aoo_code = FunctionAt(EngineExport(ee::kBroadcastAttackOfOpportunity));
  g_spellcast_code = FunctionAt(EngineExport(ee::kBroadcastSpellCast));
  g_counterspell_code = FunctionAt(EngineExport(ee::kBroadcastCounterSpellData));
  g_examine_gui_code = FunctionAt(EngineExport(ee::kExamineGuiTrapData));
  g_heal_code = FunctionAt(EngineExport(ee::kAIActionHeal));
  g_settrap_code = FunctionAt(EngineExport(ee::kAIActionSetTrap));
  g_lore_caller = SkillRankCallerIn(ee::kUseLoreOnItem);
  g_door_action_caller = SkillRankCallerIn(ee::kExecuteCommandGetIsDoorActionPossible);
  // Set Trap: its own skill (the Disable Trap synergy call asks base ranks).
  for (uintptr_t call : CallsIn(ee::kAIActionSetTrap, ee::kGetSkillRank)) {
    if (!g_settrap_caller) g_settrap_caller = call + kCallLength;
  }
  g_skill_check_d20 = OneCall(ee::kExecuteCommandGetIsSkillSuccessful, ee::kRollDice, "GetIsSkillSuccessful d20");
  for (uintptr_t call : CallsIn(ee::kUpdateTrapCheck, ee::kRollDice)) g_search_dice.push_back(call + kCallLength);
  hooks += Hook("CNWSCreature::UpdateTrapCheck (Search: 2024 DC, conditions)", ee::kUpdateTrapCheck, &UpdateTrapCheck,
                &g_update_trap_check) ? 1 : 0;
  hooks += Hook("CNWSCreature::BroadcastSpellCast (Spellcraft: 2024 DC)", ee::kBroadcastSpellCast, &SpellCast,
                &g_spell_cast) ? 1 : 0;
  hooks += Hook("CNWSObject::BroadcastCounterSpellData (Spellcraft: 2024 DC)", ee::kBroadcastCounterSpellData,
                &CounterSpell, &g_counter_spell) ? 1 : 0;
  hooks += Hook("CNWSMessage::SendServerToPlayerExamineGui_TrapData (2024 DC)", ee::kExamineGuiTrapData, &ExamineGui,
                &g_examine_gui) ? 1 : 0;
  hooks += Hook("CNWSCreature::AIActionAnimalEmpathy (2024 DC)", ee::kAIActionAnimalEmpathy, &AnimalEmpathy,
                &g_animal_empathy) ? 1 : 0;
  hooks += Hook("script GetIsSkillSuccessful (2024 DC)", ee::kExecuteCommandGetIsSkillSuccessful, &SkillSuccessCommand,
                &g_skill_success_command) ? 1 : 0;
  hooks += Hook("script GetIsDoorActionPossible (2024 lock DC)", ee::kExecuteCommandGetIsDoorActionPossible,
                &DoorActionCommand, &g_door_action_command) ? 1 : 0;
  hooks += Hook("CNWSCreature::BroadcastSkillData (the log line's 2024 skill and DC)", ee::kBroadcastSkillData,
                &BroadcastSkillData, &g_broadcast_skill_data) ? 1 : 0;
  hooks += Hook("C2DA::GetINTEntry (Set Trap and heal kit DCs)", ee::kTwoDaGetIntEntry, &GetIntEntry,
                &g_get_int_entry) ? 1 : 0;
  AddSkillAdjuster(&SkillAtCheck);
  AnswerRulesetInt("PICKPOCKET_HOSTILE_CREATURE_DC", "skills: Pick Pocket DC on the 2024 scale", &ConvertedDc);
  AnswerRulesetInt("PICKPOCKET_NEUTRAL_CREATURE_DC", "skills: Pick Pocket DC on the 2024 scale", &ConvertedDc);
  AnswerRulesetInt("ANIMAL_EMPATHY_ANIMAL_DC", "skills: Animal Empathy DC on the 2024 scale", &EmpathyDcBase);
  AnswerRulesetInt("ANIMAL_EMPATHY_BEAST_DC", "skills: Animal Empathy DC on the 2024 scale", &EmpathyDcBase);
  AnswerRulesetInt("MAX_NON_ROGUE_DISARM_LEVEL", "skills: no Rogue-only traps", &NoRogueOnlyTraps);
  AnswerRulesetInt("MAX_NON_ROGUE_DETECT_LEVEL", "skills: no Rogue-only traps", &NoRogueOnlyTraps);
  AnswerRulesetInt("SKILL_DISABLE_TRAP_SYNERGY_BONUS", "skills: no trap synergy", &NoSynergy);
  AnswerRulesetInt("SKILL_SETTRAP_SYNERGY_BONUS", "skills: no trap synergy", &NoSynergy);
  if (ConditionsFaithful()) AddDiceFilter(&CheckDice);
  Log("skill DCs: %d hooks, %zu search dice, Lore %s, Set Trap %s (faithful)", hooks, g_search_dice.size(),
      g_lore_caller ? "found" : "missing", g_settrap_caller ? "found" : "missing");
}
