// C3–C7 (port of 1.69's rules_save_roll.cpp; docs/EE-PORT.md E3a), with
// `saves` on:
// - C5 spell DC: 8 + proficiency bonus + casting modifier (class casters),
//   the 2024 scroll DCs for spells cast from items, 8 + proficiency bonus +
//   Charisma for class-less abilities (EE's class-feat and polymorph
//   abilities: the engine's own modifier); NWN mode adds Spell Focus as
//   +1/+2/+3. Each DC handed out is remembered with its caster.
// - C6: any other DC reaching the save roll (traps, poisons, item powers,
//   scripted monster abilities) is converted to the 2024 scale.
// - C4 (faithful): natural 1s and 20s on saves are not automatic — the 1 by
//   the server option it is (in memory; settings.tml is not written), the 20
//   by one anchored patch (EP12).
// - C7 (faithful): the save roll's own feat and racial bonuses ("+2 vs X")
//   are dropped (every HasFeat it asks before its d20); item and spell effects
//   stay.
// - C3 species: advantage for dwarves vs poison, halflings vs fear, elves vs
//   mind spells (charm), gnomes on Will (Wisdom) saves; Halfling Luck rerolls
//   a natural 1. Both modes.
// - CN1–CN3 (`conditions`): stunned (both modes), paralysed and sleeping
//   (faithful) creatures fail Strength and Dexterity saves — the d20 comes up
//   1 and the result is a failure, told to the player; a restrained creature
//   (faithful) rolls Dexterity saves with disadvantage.
// - C1 (faithful): a spell whose 2024 save is on another ability than NWN's
//   save rolls on that ability (the save getters substitute it); EE's combat
//   log can only name Fortitude / Reflex / Will, so the player is told the
//   ability in a chat line (EP13).
// - The Fighter's Indomitable (F5, class_features faithful): a failed save is
//   rolled again with the Fighter level added (the DC lowered by it, so the
//   engine's second save line shows the lower DC), uses in rules_fighter.cpp.

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <string>

#include "chat.h"
#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "dice.h"
#include "ee_layout.h"
#include "engine.h"
#include "feats.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "rules/core.h"
#include "rules/d20.h"
#include "rules/nwn_conversion.h"
#include "rules/saves.h"
#include "saves.h"

int IndomitableReroll(void* creature);  // rules_fighter.cpp (Fighter F5)
int ClassSaveAdvantage(void* creature, rules::Ability ability);  // rules_barbarian.cpp (B5 Danger Sense, B2 Rage)
bool IndomitableMightSucceeds(void* creature, int dc);           // rules_barbarian.cpp (B13)
void NoteSaveForced(uint32_t by, void* creature);                // rules_barbarian.cpp (B3: keeps a Rage going)

namespace {

using SpellDcFn = int (*)(void* creature, int spell_id);
using SaveRollFn = uint8_t (*)(void* creature, uint8_t save, uint16_t dc, uint8_t type, uint32_t versus, int feedback,
                               uint16_t feat, int from_combat);
using VmCommandFn = int (*)(void* commands, int command, int parameters);
using GetSpellFn = void* (*)(const void* spells, int id);
using CastingModFn = int8_t (*)(void* stats, uint8_t slot);
using GetGameObjectFn = void* (*)(void* app, uint32_t id);
using AsObjectFn = void* (*)(void* object);
using GetServerInfoFn = void* (*)(void* app);

SpellDcFn g_calculate_spell_save_dc = nullptr;
SaveRollFn g_saving_throw_roll = nullptr;
VmCommandFn g_vm_save_command = nullptr;
VmCommandFn g_vm_reflex_damage_command = nullptr;

struct Api {
  GetSpellFn get_spell = reinterpret_cast<GetSpellFn>(EngineExport(ee::kGetSpell));
  CastingModFn casting_mod = reinterpret_cast<CastingModFn>(EngineExport(ee::kGetSpellcastingMod));
  GetGameObjectFn game_object = reinterpret_cast<GetGameObjectFn>(EngineExport(ee::kGetGameObject));
  GetServerInfoFn server_info = reinterpret_cast<GetServerInfoFn>(EngineExport(ee::kGetServerInfo));
  void** rules = reinterpret_cast<void**>(EngineExport(ee::kRules));
  void** app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
};

const Api& E() {
  static const Api api;
  return api;
}

CodeRange g_save_roll_code;  // SavingThrowRoll's code, for its d20 and feat checks
uintptr_t g_save_d20_caller = 0;
LogOnce g_dc_logged(2000);
int g_save_log_lines = 0;

uint32_t IdOf(const void* object) { return Field<uint32_t>(object, ee::kObjectId); }
bool Faithful() { return g_config.saves == rules::RuleVariant::kFaithful; }

void* ServerApp() {
  return E().app_manager && *E().app_manager ? Field<void*>(*E().app_manager, ee::kAppManagerServerApp) : nullptr;
}

const void* SpellRow(int spell_id) {
  if (!E().rules || !*E().rules || !E().get_spell || spell_id < 0) return nullptr;
  void* spells = Field<void*>(*E().rules, ee::kRulesSpells);
  return spells ? E().get_spell(spells, spell_id) : nullptr;
}

// DCs handed out by CalculateSpellSaveDC, so the save roll can tell them from
// fixed DCs (C6 converts only the latter).
struct SpellDc {
  uint32_t caster;
  int dc;
};
SpellDc g_spell_dcs[32] = {};
int g_next_spell_dc = 0;

void RememberSpellDc(uint32_t caster, int dc) {
  g_spell_dcs[g_next_spell_dc] = {caster, dc};
  g_next_spell_dc = (g_next_spell_dc + 1) % 32;
}

bool IsSpellDc(uint32_t caster, int dc) {
  for (const SpellDc& entry : g_spell_dcs) {
    if (entry.caster == caster && entry.dc == dc && caster != 0) return true;
  }
  return false;
}

// The save being rolled, for the d20 and the getters.
struct SaveContext {
  bool active = false;
  void* creature = nullptr;
  int save = 0;    // 1 Fortitude, 2 Reflex, 3 Will
  int type = 0;    // NWN save type
  int spell = -1;  // the spell whose script asked for this save, or -1
  std::optional<rules::Ability> ability;  // C1: the 2024 ability that spell uses
  ConditionSave condition;                // CN1–CN3
};
SaveContext g_save;

// The spell of the script running the current FortitudeSave / ReflexSave /
// WillSave / GetReflexAdjustedDamage command (-1 outside one).
int g_script_spell = -1;
// SK6: the ability of the save being rolled when not NWN's default for it (a
// Shove's Strength save), set around that one roll.
std::optional<rules::Ability> g_forced_ability;

// The spell id of an object, as GetSpellId reads it (an area of effect keeps
// its own); -1 when unknown.
int SpellOfObject(uint32_t id) {
  void* app = ServerApp();
  void* object = app && E().game_object ? E().game_object(app, id) : nullptr;
  if (!object) return -1;
  const auto as_object = (*reinterpret_cast<AsObjectFn* const*>(object))[ee::kVtableAsNwsObject / sizeof(void*)];
  void* nws_object = as_object ? as_object(object) : nullptr;
  return nws_object ? Field<int32_t>(nws_object, ee::kObjectLastSpellId) : -1;
}

int WithScriptSpell(VmCommandFn original, void* commands, int command, int parameters) {
  const int outer = g_script_spell;
  g_script_spell = SpellOfObject(Field<uint32_t>(commands, ee::kVmCommandsObjectSelf));
  const int result = original(commands, command, parameters);
  g_script_spell = outer;
  return result;
}

int VmSaveCommand(void* commands, int command, int parameters) {
  return WithScriptSpell(g_vm_save_command, commands, command, parameters);
}

int VmReflexDamageCommand(void* commands, int command, int parameters) {
  return WithScriptSpell(g_vm_reflex_damage_command, commands, command, parameters);
}

int SpellFocusBonus(void* stats, int school) {
  if (school < 1 || school > 8) return 0;
  if (engine::HasFeat(stats, 0x262 + school - 1)) return 6;
  if (engine::HasFeat(stats, 0x189 + school - 1)) return 4;
  if (engine::HasFeat(stats, school == 1 ? 0x23 : 0xA6 + school - 2)) return 2;
  return 0;
}

// The 2024 save DC of `spell_id` cast by `creature` (C5), given the engine's
// own DC; false when the creature's numbers are not known.
bool SpellDc2024(void* creature, int spell_id, int engine_dc, int* dc, int* pb_out, const char** source) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  const void* spell = SpellRow(spell_id);
  if (!stats || !spell) return false;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.plausible) return false;
  const int pb = rules::ProficiencyBonusForCreature(CountsAsCharacter(stats, facts), facts.level,
                                                    facts.challenge_rating);
  const int innate = Field<uint8_t>(spell, ee::kSpellInnateLevel);
  const int slot = Field<uint8_t>(creature, ee::kCreatureCastingSlot);
  *source = "class";
  if (Field<int32_t>(creature, ee::kCreatureCastingFromItem) != 0) {
    *dc = rules::SpellScrollDc(innate);
    *source = "item";
  } else if (slot == 0xFE ||
             (slot == 0xFF && Field<uint16_t>(creature, ee::kCreatureCastingFeat) == 0xFFFF &&
              Field<int32_t>(creature, ee::kCreaturePolymorphed) == 0)) {
    *dc = rules::SaveDifficultyClass(engine::AbilityModifier(stats, static_cast<int>(rules::Ability::kCharisma)), pb);
    *source = "ability";  // class-less or innate ability: 8 + PB + Cha
  } else if (slot == 0xFF) {
    // EE's class-feat and polymorph abilities: the engine's own modifier (its DC is 10 + modifier + innate level).
    *dc = rules::SaveDifficultyClass(engine_dc - 10 - innate, pb);
    *source = "feat / polymorph ability";
  } else {
    const int modifier = E().casting_mod ? E().casting_mod(stats, static_cast<uint8_t>(slot)) : 0;
    *dc = rules::SaveDifficultyClass(modifier, pb);
    if (g_config.saves == rules::RuleVariant::kNwn) {
      *dc += rules::NwnModeSpellFocusDcBonus(SpellFocusBonus(stats, Field<uint8_t>(spell, ee::kSpellSchool)));
    }
  }
  if (pb_out) *pb_out = pb;
  return true;
}

int CalculateSpellSaveDc(void* creature, int spell_id) {
  const int engine_dc = g_calculate_spell_save_dc(creature, spell_id);
  if (g_config.saves == rules::RuleVariant::kOff || !creature) return engine_dc;
  int dc = 0;
  int pb = 0;
  const char* source = "";
  if (!SpellDc2024(creature, spell_id, engine_dc, &dc, &pb, &source)) return engine_dc;
  RememberSpellDc(IdOf(creature), dc);
  if (g_config.log_attacks && g_dc_logged.First(LogOnce::Key(IdOf(creature), engine_dc, dc))) {
    Log("spell DC: object 0x%08X %s spell %d (%s): 3e %d -> 5e %d (PB %d)", IdOf(creature),
        KindOf(ReadCreatureFacts(StatsOf(creature))), spell_id, source, engine_dc, dc, pb);
  }
  return dc;
}

const char* AbilityName(rules::Ability ability) {
  static const char* const kNames[] = {"Strength", "Dexterity", "Constitution", "Intelligence", "Wisdom", "Charisma"};
  return kNames[static_cast<int>(ability)];
}

std::string SpellNote(int spell, const std::optional<rules::Ability>& ability) {
  if (spell < 0) return std::string();
  return ", spell " + std::to_string(spell) + (ability ? std::string(" uses ") + AbilityName(*ability) : std::string());
}

// C4 faithful: a natural 1 is not an automatic failure — EE's server option
// (CServerInfo +0x148), set in memory before each roll (its message is
// written later, for combat saves).
void NaturalOnesNotAutomatic() {
  void* app = ServerApp();
  void* info = app && E().server_info ? E().server_info(app) : nullptr;
  if (info && Field<int32_t>(info, ee::kServerInfoNaturalOnesFailSaves) != 0) {
    SetField<int32_t>(info, ee::kServerInfoNaturalOnesFailSaves, 0);
    Log("save roll: natural 1 not an automatic failure (faithful; the game option, in memory)");
  }
}

void TellIfPlayer(uint32_t id, const std::string& line) {
  void* creature = CreatureById(id);
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (stats && ReadCreatureFacts(stats).is_player) TellPlayer(id, line);
}

uint8_t SavingThrowRoll(void* creature, uint8_t save, uint16_t dc, uint8_t type, uint32_t versus, int feedback,
                        uint16_t feat, int from_combat) {
  if (g_config.saves == rules::RuleVariant::kOff || !creature) {
    return g_saving_throw_roll(creature, save, dc, type, versus, feedback, feat, from_combat);
  }
  if (Faithful()) NaturalOnesNotAutomatic();
  NoteSaveForced(versus, creature);
  const int engine_dc = dc;
  // A save asked for by a spell's script uses that spell's DC (GetSpellSaveDC,
  // already 2024 — persistent areas of effect pass themselves as the
  // opponent, so the caster check alone misses them).
  const bool spell_dc = g_script_spell >= 0 || IsSpellDc(versus, engine_dc);
  const int new_dc = spell_dc ? engine_dc : rules::ConvertNwnFixedDc(engine_dc);
  const SaveContext outer = g_save;
  g_save = SaveContext();
  g_save.active = true;
  g_save.creature = creature;
  g_save.save = save;
  g_save.type = type;
  g_save.spell = g_script_spell;
  if (Faithful() && g_script_spell >= 0) g_save.ability = rules::SaveAbilityForNwnSpell(g_script_spell);
  if (Faithful() && g_forced_ability) g_save.ability = g_forced_ability;
  if (save >= 1 && save <= 3) {
    const rules::Ability ability =
        g_save.ability.value_or(rules::AbilityForNwnSave(static_cast<rules::NwnSave>(save - 1)));
    g_save.condition = ConditionsForSave(creature, ability);
  }
  const SaveContext context = g_save;
  uint8_t result = g_saving_throw_roll(creature, save, static_cast<uint16_t>(std::clamp(new_dc, 1, 0xFFFF)), type,
                                       versus, feedback, feat, from_combat);
  // Fighter F5 Indomitable: a failed save is rolled again, + the Fighter level.
  if (result == 0 && !context.condition.auto_fail) {
    const int bonus = IndomitableReroll(creature);
    if (bonus > 0) {
      g_save = context;
      TellIfPlayer(IdOf(creature),
                   "nwn5e: Indomitable - the failed save is rolled again, +" + std::to_string(bonus) + ".");
      result = g_saving_throw_roll(creature, save, static_cast<uint16_t>(std::clamp(new_dc - bonus, 1, 0xFFFF)),
                                   type, versus, feedback, feat, from_combat);
    }
  }
  // Barbarian B13 Indomitable Might: a failed Strength save succeeds when the Strength score meets the DC.
  if (result == 0 && !context.condition.auto_fail && save >= 1 && save <= 3 &&
      context.ability.value_or(rules::AbilityForNwnSave(static_cast<rules::NwnSave>(save - 1))) ==
          rules::Ability::kStrength &&
      IndomitableMightSucceeds(creature, new_dc)) {
    result = 1;
    TellIfPlayer(IdOf(creature), "nwn5e: Indomitable Might - your Strength score meets the DC: the save succeeds.");
  }
  g_save = outer;
  const uint32_t creature_id = IdOf(creature);
  if (context.condition.auto_fail && result != 2) {
    result = 0;  // a failure; the engine's own line may still read "success" when its total beat the DC
    TellIfPlayer(creature_id, "nwn5e: that save fails automatically (" + context.condition.note + ")");
  }
  if (context.ability && save >= 1 && save <= 3 &&
      *context.ability != rules::AbilityForNwnSave(static_cast<rules::NwnSave>(save - 1))) {
    static const char* const kNwnNames[] = {"Fortitude", "Reflex", "Will"};
    const std::string line = std::string("nwn5e: that ") + kNwnNames[save - 1] + " save was a " +
                             AbilityName(*context.ability) + " save (the spell's ability).";
    TellIfPlayer(creature_id, line);
    if (versus != creature_id) TellIfPlayer(versus, line);
  }
  if (g_config.log_attacks && g_save_log_lines < 300) {
    ++g_save_log_lines;
    Log("save roll: object 0x%08X save %d type %d vs 0x%08X: DC %d%s -> %d%s, %s%s%s", creature_id, save, type, versus,
        engine_dc, spell_dc ? " (spell)" : "", new_dc, SpellNote(context.spell, context.ability).c_str(),
        result == 1 ? "success" : (result == 2 ? "immune" : "failure"),
        context.condition.auto_fail ? ", automatic failure: " : "",
        context.condition.auto_fail ? context.condition.note.c_str() : "");
  }
  return result;
}

bool SpeciesAdvantage(void* stats, int save, int type) {
  const int race = engine::RaceOf(stats);
  return (race == ee::kRaceDwarf && type == ee::kSaveTypePoison) ||
         (race == ee::kRaceHalfling && type == ee::kSaveTypeFear) ||
         (race == ee::kRaceElf && type == ee::kSaveTypeMindSpells) || (race == ee::kRaceGnome && save == 3);
}

// The save roll's d20.
bool SaveD20(uintptr_t caller, int count, int sides, int* result) {
  if (caller != g_save_d20_caller || !g_save.active || count != 1 || sides != 20) return false;
  if (g_save.condition.auto_fail) {  // CN2/CN3: the wrapper makes it a failure
    *result = 1;
    return true;
  }
  void* stats = StatsOf(g_save.creature);
  int kept = RollEngineDice(1, 20);
  const bool species = stats && SpeciesAdvantage(stats, g_save.save, g_save.type);
  const bool class_feature =
      g_save.save >= 1 && g_save.save <= 3 &&
      ClassSaveAdvantage(g_save.creature, g_save.ability.value_or(rules::AbilityForNwnSave(
                                              static_cast<rules::NwnSave>(g_save.save - 1)))) > 0;
  const rules::RollMode mode =
      rules::ResolveRollMode((species ? 1 : 0) + (class_feature ? 1 : 0), g_save.condition.disadvantage);
  const bool advantage = mode == rules::RollMode::kAdvantage;
  const bool disadvantage = mode == rules::RollMode::kDisadvantage;
  if (advantage || disadvantage) {
    const int second = RollEngineDice(1, 20);
    if (advantage ? second > kept : second < kept) kept = second;
  }
  bool lucky = false;
  if (kept == 1 && engine::RaceOf(stats) == ee::kRaceHalfling) {
    kept = RollEngineDice(1, 20);  // Halfling Luck: reroll a natural 1
    lucky = true;
  }
  if ((species || class_feature || disadvantage || lucky) && g_config.log_attacks && g_save_log_lines < 300) {
    ++g_save_log_lines;
    Log("save d20: object 0x%08X save %d type %d:%s%s%s%s%s -> %d", IdOf(g_save.creature), g_save.save, g_save.type,
        species ? " species advantage" : "", class_feature ? " class advantage (Danger Sense / Rage)" : "",
        g_save.condition.disadvantage > 0 ? " disadvantage (" : "",
        g_save.condition.disadvantage > 0 ? (g_save.condition.note + ")").c_str() : "",
        lucky ? " Halfling Luck reroll" : "", kept);
  }
  *result = kept;
  return true;
}

// C7 (faithful): every HasFeat the save roll asks before its d20 is a feat or
// racial save bonus ("+2 vs X"); answered 0 there (1.69 redirected the same 34 calls).
bool NoSaveFeatBonus(void* stats, int feat, uintptr_t caller, int* answer) {
  (void)stats;
  (void)feat;
  if (!g_save_roll_code.Contains(caller) || caller >= g_save_d20_caller) return false;
  *answer = 0;
  return true;
}

}  // namespace

void RememberSaveDc2024(uint32_t caster, int dc) { RememberSpellDc(caster, dc); }

std::optional<rules::Ability> SaveAbilityOverride(void* stats) {
  if (!g_save.active || !g_save.ability || !g_save.creature) return std::nullopt;
  return StatsOf(g_save.creature) == stats ? g_save.ability : std::nullopt;
}

bool SaveRollInProgressFor(void* stats) { return g_save.active && g_save.creature && StatsOf(g_save.creature) == stats; }

void SetForcedSaveAbility(std::optional<rules::Ability> ability) { g_forced_ability = ability; }

bool SpellAttackCasting(void* creature, int* bonus, const char** source) {
  if (!creature) return false;
  const int spell_id = Field<int32_t>(creature, ee::kObjectLastSpellId);
  static const auto unhooked = reinterpret_cast<SpellDcFn>(EngineExport(ee::kCalculateSpellSaveDC));
  const SpellDcFn engine = g_calculate_spell_save_dc ? g_calculate_spell_save_dc : unhooked;
  const int engine_dc = engine ? engine(creature, spell_id) : 10;
  int dc = 0;
  if (!SpellDc2024(creature, spell_id, engine_dc, &dc, nullptr, source)) return false;
  *bonus = dc - 8;  // 2024: attack bonus = PB + ability, DC = 8 + the same (scrolls: their table, the same rule)
  return true;
}

void InstallSaveRollRules() {
  if (g_config.saves == rules::RuleVariant::kOff) return;
  HookExport("CNWSCreature::CalculateSpellSaveDC (2024 spell DC)", ee::kCalculateSpellSaveDC,
             reinterpret_cast<void*>(&CalculateSpellSaveDc), reinterpret_cast<void**>(&g_calculate_spell_save_dc));
  HookExport("script save commands (the spell behind a save)", ee::kExecuteCommandSavingThrow,
             reinterpret_cast<void*>(&VmSaveCommand), reinterpret_cast<void**>(&g_vm_save_command));
  HookExport("GetReflexAdjustedDamage command (the spell behind its save)", ee::kExecuteCommandGetReflexAdjustedDamage,
             reinterpret_cast<void*>(&VmReflexDamageCommand), reinterpret_cast<void**>(&g_vm_reflex_damage_command));
  if (!HookExport("CNWSCreature::SavingThrowRoll (DCs, species advantage, conditions)", ee::kSavingThrowRoll,
                  reinterpret_cast<void*>(&SavingThrowRoll), reinterpret_cast<void**>(&g_saving_throw_roll))) {
    return;
  }
  g_save_roll_code = FunctionAt(EngineExport(ee::kSavingThrowRoll));
  const std::vector<uintptr_t> d20 = FindCallsTo(g_save_roll_code, EngineExport(ee::kRollDice));
  if (d20.size() == 1) {
    g_save_d20_caller = d20[0] + kCallLength;
    AddDiceFilter(&SaveD20);
  } else {
    Log("SKIP save d20 (species advantage, conditions, Halfling Luck): %zu RollDice calls in SavingThrowRoll",
        d20.size());
  }
  if (!Faithful()) return;
  static const uint8_t kJe[2] = {0x74, 0x4E};
  static const uint8_t kNops[2] = {0x90, 0x90};
  PatchAnchored("save roll: natural 20 not automatic (faithful, EP12)", ee::kSavingThrowRoll, ee::kSaveNatural20Anchor,
                sizeof(ee::kSaveNatural20Anchor), ee::kSaveNatural20JumpOffset, kJe, kNops, sizeof(kNops));
  if (g_save_d20_caller && AddFeatFilter(&NoSaveFeatBonus)) {
    Log("save roll: the feat and racial save bonuses before its d20 are dropped (faithful, C7)");
  }
}
