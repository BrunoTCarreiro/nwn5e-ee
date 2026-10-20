// C1–C3, C8 (port of 1.69's rules_saves.cpp; docs/EE-PORT.md E3a): the three
// NWN save values become 2024 saves. Fortitude, Reflex and Will are read as
// the Constitution, Dexterity and Wisdom saves: ability modifier +
// proficiency bonus when proficient + magic effects (+ Aura of Protection in
// faithful mode, + NWN's flat feat/racial and blueprint bonuses in NWN mode) —
// rules::SaveValue. The character sheet, scripts and the save roll read these
// functions, so all follow. `saves=off` keeps NWN's 3e values.
// EE fix over 1.69: the save roll asks for base values (no effects; it adds
// them itself), and 1.69 added Aura of Protection only outside base asks, so
// the aura never reached a real roll; here it does.

#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "chat.h"
#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "log_once.h"
#include "rules/nwn_conversion.h"
#include "rules/saves.h"
#include "saves.h"

namespace {

using SaveFn = char (*)(void* stats, int base_only);
using ClassSaveBonusFn = uint8_t (*)(void* character_class, uint8_t level);
using FactionFn = void* (*)(void* creature);

SaveFn g_get_fort = nullptr;
SaveFn g_get_reflex = nullptr;
SaveFn g_get_will = nullptr;

struct Api {
  ClassSaveBonusFn fort_bonus = reinterpret_cast<ClassSaveBonusFn>(EngineExport(ee::kGetFortSaveBonus));
  ClassSaveBonusFn reflex_bonus = reinterpret_cast<ClassSaveBonusFn>(EngineExport(ee::kGetRefSaveBonus));
  ClassSaveBonusFn will_bonus = reinterpret_cast<ClassSaveBonusFn>(EngineExport(ee::kGetWillSaveBonus));
  FactionFn faction = reinterpret_cast<FactionFn>(EngineExport(ee::kGetFaction));
  void** rules = reinterpret_cast<void**>(EngineExport(ee::kRules));
  // The hooked getters (through the hooks), for !saves and SaveBonusFor.
  SaveFn fort = reinterpret_cast<SaveFn>(EngineExport(ee::kGetFortSavingThrow));
  SaveFn reflex = reinterpret_cast<SaveFn>(EngineExport(ee::kGetReflexSavingThrow));
  SaveFn will = reinterpret_cast<SaveFn>(EngineExport(ee::kGetWillSavingThrow));
};

const Api& E() {
  static const Api api;
  return api;
}

LogOnce g_save_logged(2000);

int Ability(void* stats, rules::Ability ability) { return engine::AbilityModifier(stats, static_cast<int>(ability)); }

int SaveFeat(rules::NwnSave save) {
  switch (save) {
    case rules::NwnSave::kFortitude:
      return ee::kFeatGreatFortitude;
    case rules::NwnSave::kReflex:
      return ee::kFeatLightningReflexes;
    default:
      return ee::kFeatIronWill;
  }
}

bool HasFeat(void* stats, int feat) { return engine::HasFeat(stats, feat); }

// NWN mode: NWN's flat feat, racial and blueprint bonuses for this save, as
// the engine's own save functions add them.
int NwnExtras(void* stats, rules::NwnSave save) {
  const int charisma = Ability(stats, rules::Ability::kCharisma);
  int extras = 0;
  if (HasFeat(stats, ee::kFeatDivineGrace) && charisma > 0) extras += charisma;
  if (HasFeat(stats, SaveFeat(save))) extras += 2;
  if (HasFeat(stats, ee::kFeatLucky)) extras += 1;
  if (HasFeat(stats, ee::kFeatLuckOfHeroes)) extras += 1;
  if (HasFeat(stats, ee::kFeatDarkBlessing)) extras += charisma;
  switch (save) {
    case rules::NwnSave::kFortitude:
      if (HasFeat(stats, 388)) extras += 1;
      if (HasFeat(stats, 583)) extras += 4;
      extras += Field<int8_t>(stats, ee::kStatsFortBlueprintBonus);
      break;
    case rules::NwnSave::kReflex:
      if (HasFeat(stats, 386)) extras += 1;
      if (HasFeat(stats, 585)) extras += 4;
      extras += Field<int8_t>(stats, ee::kStatsReflexBlueprintBonus);
      break;
    case rules::NwnSave::kWill:
      if (HasFeat(stats, 380)) extras += 1;
      if (HasFeat(stats, 388)) extras += 1;
      if (HasFeat(stats, 695)) extras += 4;
      extras += Field<int8_t>(stats, ee::kStatsWillBlueprintBonus);
      break;
  }
  return extras;
}

// C2 NWN mode: any of the creature's classes has a "good" table for this save.
bool HasGoodNwnTable(const std::vector<rules::NwnClassSlot>& slots, rules::NwnSave save) {
  if (!E().rules || !*E().rules) return false;
  uint8_t* classes = Field<uint8_t*>(*E().rules, ee::kRulesClasses);
  const ClassSaveBonusFn save_bonus = save == rules::NwnSave::kFortitude ? E().fort_bonus
                                      : save == rules::NwnSave::kReflex  ? E().reflex_bonus
                                                                         : E().will_bonus;
  if (!classes || !save_bonus) return false;
  for (const rules::NwnClassSlot& slot : slots) {
    if (slot.nwn_class_id < 0 || slot.nwn_class_id > 255) continue;
    void* character_class = classes + slot.nwn_class_id * ee::kClassStride;
    if (rules::IsGoodNwnSaveTable(static_cast<int8_t>(save_bonus(character_class, 1)))) return true;
  }
  return false;
}

int PaladinLevels(void* stats) {
  int levels = 0;
  for (const rules::NwnClassSlot& slot : ReadClassSlots(stats)) {
    if (rules::ClassFromNwnId(slot.nwn_class_id) == rules::CharacterClass::kPaladin) levels += slot.levels;
  }
  return levels;
}

bool Conscious(void* creature) { return !engine::IsDeadOrDying(creature) && !IncapacitatedByCondition(creature); }

int AuraFrom(void* paladin, float distance_m) {
  void* stats = StatsOf(paladin);
  if (!stats) return 0;
  return rules::AuraOfProtectionBonus(PaladinLevels(stats), Ability(stats, rules::Ability::kCharisma),
                                      Conscious(paladin), distance_m);
}

struct AuraSearch {
  void* creature;
  void* faction;
  int best;
};

void ConsiderAura(void* context, uint32_t, void* other) {
  auto* s = static_cast<AuraSearch*>(context);
  if (!s->faction || !E().faction || E().faction(other) != s->faction) return;
  s->best = std::max(s->best, AuraFrom(other, std::max(0.0f, engine::EdgeDistance(s->creature, other))));
}

// C3 faithful: the best Aura of Protection covering this creature (its own,
// or an ally paladin's within 3 m). Auras don't stack.
int AuraOfProtection(void* stats) {
  void* creature = CreatureOf(stats);
  if (!creature) return 0;
  AuraSearch search{creature, E().faction ? E().faction(creature) : nullptr, AuraFrom(creature, 0.0f)};
  engine::ForEachOtherCreatureInArea(creature, &ConsiderAura, &search);
  return search.best;
}

const char* SaveName(rules::NwnSave save) {
  return save == rules::NwnSave::kFortitude ? "Fortitude (Con)"
         : save == rules::NwnSave::kReflex  ? "Reflex (Dex)"
                                            : "Will (Wis)";
}

int ConvertSave(rules::NwnSave save, SaveFn original, void* stats, int base_only) {
  const int engine_value = static_cast<int8_t>(original(stats, base_only));
  const rules::RuleVariant variant = g_config.saves;
  if (variant == rules::RuleVariant::kOff || !stats) return engine_value;
  const CreatureFacts facts = ReadCreatureFacts(stats);
  if (!facts.plausible) return engine_value;
  const std::vector<rules::NwnClassSlot> slots = ReadClassSlots(stats);
  rules::SaveFacts save_facts;
  save_facts.save = save;
  save_facts.ability_modifier = Ability(stats, rules::AbilityForNwnSave(save));
  save_facts.first_class = slots.empty() ? std::nullopt : rules::ClassFromNwnId(slots[0].nwn_class_id);
  save_facts.character = CountsAsCharacter(stats, facts);
  save_facts.proficiency_bonus =
      rules::ProficiencyBonusForCreature(save_facts.character, facts.level, facts.challenge_rating);
  save_facts.save_feat = HasFeat(stats, SaveFeat(save));
  // C1: the spell behind this save asks for another ability (faithful).
  const std::optional<rules::Ability> spell_ability = SaveAbilityOverride(stats);
  if (spell_ability && *spell_ability != rules::AbilityForNwnSave(save)) {
    save_facts.ability = spell_ability;
    save_facts.ability_modifier = Ability(stats, *spell_ability);
    save_facts.save_feat = false;  // Great Fortitude & co. are for their own save
  }
  save_facts.good_nwn_table = !save_facts.character && HasGoodNwnTable(slots, save);
  save_facts.effects = base_only ? 0 : engine_value - static_cast<int8_t>(original(stats, 1));
  if (variant == rules::RuleVariant::kFaithful && (!base_only || SaveRollInProgressFor(stats))) {
    save_facts.aura = AuraOfProtection(stats);
  }
  if (variant == rules::RuleVariant::kNwn) save_facts.nwn_extras = NwnExtras(stats, save);
  const int value = std::clamp(rules::SaveValue(save_facts, variant), -127, 127);
  if (g_config.log_attacks &&
      g_save_logged.First(LogOnce::Key(ObjectIdOf(stats) ^ (static_cast<uint32_t>(save) << 28), engine_value, value))) {
    Log("save: object 0x%08X %s %s%s%s: 3e %+d -> 5e %+d (ability %+d, %s, effects %+d%s)", ObjectIdOf(stats),
        KindOf(facts), SaveName(save), base_only ? " base" : "", save_facts.ability ? " as the spell's ability" : "",
        engine_value, value, save_facts.ability_modifier, save_facts.character ? "character" : "creature",
        save_facts.effects, save_facts.aura ? ", aura" : (save_facts.nwn_extras ? ", NWN extras" : ""));
  }
  return value;
}

char GetFortSavingThrow(void* stats, int base_only) {
  return static_cast<char>(ConvertSave(rules::NwnSave::kFortitude, g_get_fort, stats, base_only));
}
char GetReflexSavingThrow(void* stats, int base_only) {
  return static_cast<char>(ConvertSave(rules::NwnSave::kReflex, g_get_reflex, stats, base_only));
}
char GetWillSavingThrow(void* stats, int base_only) {
  return static_cast<char>(ConvertSave(rules::NwnSave::kWill, g_get_will, stats, base_only));
}

int EngineSave(SaveFn getter, void* stats) { return getter ? static_cast<int8_t>(getter(stats, 0)) : 0; }

// C8: all six 2024 saves. Con / Dex / Wis are the hooked Fortitude / Reflex /
// Will values; Str / Int / Cha (no NWN save asks for them) are ability
// modifier + proficiency bonus when proficient (+ Aura of Protection in
// faithful mode).
std::string DescribeSaves(void* stats) {
  if (g_config.saves == rules::RuleVariant::kOff) return "nwn5e: 2024 saves are off (saves=off in nwn5e.ini).";
  const CreatureFacts facts = ReadCreatureFacts(stats);
  const std::vector<rules::NwnClassSlot> slots = ReadClassSlots(stats);
  const std::optional<rules::CharacterClass> first =
      slots.empty() ? std::nullopt : rules::ClassFromNwnId(slots[0].nwn_class_id);
  const bool character = CountsAsCharacter(stats, facts);
  const int pb = rules::ProficiencyBonusForCreature(character, facts.level, facts.challenge_rating);
  const bool faithful = g_config.saves == rules::RuleVariant::kFaithful;
  const int aura = faithful ? AuraOfProtection(stats) : 0;
  const auto class_proficient = [&](rules::Ability ability) {
    return character && first && rules::HasSaveProficiency(*first, ability);
  };
  struct Row {
    int value;
    bool proficient;
  };
  const auto engine_save = [&](SaveFn getter, rules::NwnSave save, rules::Ability ability) {
    const bool feat = faithful && character && HasFeat(stats, SaveFeat(save));
    const bool good = !faithful && !character && HasGoodNwnTable(slots, save);
    return Row{EngineSave(getter, stats), class_proficient(ability) || feat || good};
  };
  const auto plain_save = [&](rules::Ability ability) {
    const bool proficient = class_proficient(ability);
    return Row{rules::SavingThrowBonus(Ability(stats, ability), proficient, pb, aura), proficient};
  };
  const Row rows[6] = {plain_save(rules::Ability::kStrength),
                       engine_save(E().reflex, rules::NwnSave::kReflex, rules::Ability::kDexterity),
                       engine_save(E().fort, rules::NwnSave::kFortitude, rules::Ability::kConstitution),
                       plain_save(rules::Ability::kIntelligence),
                       engine_save(E().will, rules::NwnSave::kWill, rules::Ability::kWisdom),
                       plain_save(rules::Ability::kCharisma)};
  static const char* const kNames[6] = {"Str", "Dex", "Con", "Int", "Wis", "Cha"};
  std::string text = faithful ? "nwn5e: saves (2024): " : "nwn5e: saves (2024, NWN mode): ";
  char part[32];
  for (int i = 0; i < 6; ++i) {
    snprintf(part, sizeof(part), "%s%s %+d%s", i ? ", " : "", kNames[i], rows[i].value, rows[i].proficient ? "*" : "");
    text += part;
  }
  text += "   (* proficient)";
  return text;
}

std::vector<std::string> SavesCommand(void* creature, const std::string&) {
  void* stats = creature ? StatsOf(creature) : nullptr;
  if (!stats) return {};
  return {DescribeSaves(stats)};
}

}  // namespace

void InstallSaveRules() {
  if (g_config.saves == rules::RuleVariant::kOff) return;
  HookExport("CNWSCreatureStats::GetFortSavingThrow (2024 Con save)", ee::kGetFortSavingThrow,
             reinterpret_cast<void*>(&GetFortSavingThrow), reinterpret_cast<void**>(&g_get_fort));
  HookExport("CNWSCreatureStats::GetReflexSavingThrow (2024 Dex save)", ee::kGetReflexSavingThrow,
             reinterpret_cast<void*>(&GetReflexSavingThrow), reinterpret_cast<void**>(&g_get_reflex));
  HookExport("CNWSCreatureStats::GetWillSavingThrow (2024 Wis save)", ee::kGetWillSavingThrow,
             reinterpret_cast<void*>(&GetWillSavingThrow), reinterpret_cast<void**>(&g_get_will));
  AddChatCommand("!saves", &SavesCommand);
}

int SaveBonusFor(void* stats, rules::Ability ability) {
  switch (ability) {
    case rules::Ability::kDexterity:
      return EngineSave(E().reflex, stats);
    case rules::Ability::kConstitution:
      return EngineSave(E().fort, stats);
    case rules::Ability::kWisdom:
      return EngineSave(E().will, stats);
    default:
      break;
  }
  const CreatureFacts facts = ReadCreatureFacts(stats);
  const std::vector<rules::NwnClassSlot> slots = ReadClassSlots(stats);
  const std::optional<rules::CharacterClass> first =
      slots.empty() ? std::nullopt : rules::ClassFromNwnId(slots[0].nwn_class_id);
  const bool character = CountsAsCharacter(stats, facts);
  const int pb = rules::ProficiencyBonusForCreature(character, facts.level, facts.challenge_rating);
  const int aura = g_config.saves == rules::RuleVariant::kFaithful ? AuraOfProtection(stats) : 0;
  const bool proficient = character && first && rules::HasSaveProficiency(*first, ability);
  return rules::SavingThrowBonus(Ability(stats, ability), proficient, pb, aura);
}
