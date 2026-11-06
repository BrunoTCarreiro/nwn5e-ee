// #37 Concentration (docs/PLAN.md "Phase 4"; owner, 2026-11-06: "37, all recommendations except 4A, do not have a
// end concentration item in the quickbar, add a button for that in the resources panel. Maybe clicking the
// \"concentrating: <spell>\" element"), `concentration` faithful (SK7's key: its other half, 0.9.39, took NWN's
// Concentration checks while casting away). SRD 5.2: a spell whose duration is "Concentration, up to ..." lasts while
// its caster keeps Concentration - one at a time; damage calls for a Constitution save (DC 10 or half the damage,
// at most 30); Incapacitated or dead ends it, so does choosing to; Rage ends it (B2).
// - Which spells (1A): rules::IsConcentrationSpell - SRD 5.2's Concentration spells among NWN's rows. Every caster
//   (2A): players, henchmen, enemies. A potion's spell needs none (3A); a scroll's or a wand's does. NWN's durations
//   stay (5A: #39 gives each spell the SRD's).
// - The cast: CVirtualMachine::RunScript runs a spell's impact script with the caster as OBJECT_SELF and the spell
//   in its last-spell field (+0x218, GetSpellId's); a start listener (script_hooks.h) sees a Concentration spell's
//   own ImpactScript (spells.2da's, read once by the script bridge) begin - before its effects land, so the
//   caster's earlier Concentration (the same spell's too) ends first.
// - What holds it: every effect the caster made with that spell (CGameEffect creator +0x18, spell id +0x1C) on any
//   object of the caster's area, and its areas of effect (CNWSAreaOfEffectObject creator +0x330, spell +0x314) -
//   their own effects carry the caster too. Ending it removes those effects (a script per object: RemoveEffect) and
//   destroys those areas. When none are left (the spell's own duration ran out) the Concentration is over.
// - Damage (OnApplyDamage's listeners): the engine's saving throw, Fortitude (the 2024 save hook answers it as
//   Constitution), DC rules::ConcentrationSaveDc; a failure ends it at the caster's next update (not inside the
//   damage's own handling), as do Incapacitated (IncapacitatedByCondition), down (dying or dead) and Rage.
// - 4 (the owner's): the resource panel's line "Concentrating: <spell>"; a click on it ends it (no action). The
//   combat log has each save's roll and an nwn5e line for its outcome.
// NWN mode: no Concentration (NWN's).

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "chat.h"
#include "condition_effects.h"
#include "config.h"
#include "creature.h"
#include "creature_update.h"
#include "damage_listeners.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "resource_panel.h"
#include "saves.h"
#include "script_hooks.h"
#include "rules/concentration.h"

bool BarbarianRaging(void* creature);  // rules_barbarian.cpp (B2: Rage ends Concentration)

namespace {

constexpr const char* kVarHeld = "nwn5e_concentration";             // the spell + 1 (0: none) - the panel, a load
constexpr const char* kVarScripts = "nwn5e_concentration_scripts";  // the script bridge's answer (once)
constexpr ULONGLONG kOverCheckMs = 1000;   // how often a held spell is looked for
constexpr ULONGLONG kSettleMs = 3000;      // after the cast: its effects may still be on their way (DelayCommand)

using GetObjectFn = void* (*)(void* app, uint32_t id);
using GetSpellFn = void* (*)(const void* spells, int id);
using SpellNameFn = void* (*)(void* spell, void* out);
using FreeTextFn = void (*)(void* text);
using SaveRollFn = uint8_t (*)(void* creature, uint8_t save, uint16_t dc, uint8_t type, uint32_t versus, int feedback,
                               uint16_t feat, int from_combat);
using AsObjectFn = void* (*)(void* game_object);

struct ExoText {
  char* text = nullptr;
  uint32_t length = 0;
  uint32_t capacity = 0;
};

struct Held {
  int spell = -1;
  ULONGLONG since = 0;
  ULONGLONG next_look = 0;
  std::string end;  // a reason to end it at the caster's next update ("" none)
};

std::unordered_map<uint32_t, Held> g_held;  // by caster
std::unordered_map<int, std::string> g_impact;  // a Concentration spell's ImpactScript, lower case
std::unordered_set<std::string> g_impact_names;  // the same names: a script's first look
bool g_impact_read = false;
void** g_app_manager = nullptr;
void** g_rules = nullptr;
GetObjectFn g_get_object = nullptr;
GetObjectFn g_get_item = nullptr;
GetObjectFn g_get_aoe = nullptr;
GetSpellFn g_get_spell = nullptr;
SpellNameFn g_spell_name = nullptr;
FreeTextFn g_free_text = nullptr;
SaveRollFn g_save_roll = nullptr;
int g_logged = 0;

bool Faithful() { return g_config.concentration == rules::RuleVariant::kFaithful; }

uint32_t IdOf(const void* object) { return object ? Field<uint32_t>(object, ee::kObjectId) : ee::kObjectInvalid; }

bool IsPlayer(void* creature) { return creature && Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) != 0; }

void* ServerApp() {
  return g_app_manager && *g_app_manager ? Field<void*>(*g_app_manager, ee::kAppManagerServerApp) : nullptr;
}

std::string Lower(std::string text) {
  for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return text;
}

std::string SpellName(int spell) {
  void* spells = g_rules && *g_rules ? Field<void*>(*g_rules, ee::kRulesSpells) : nullptr;
  void* entry = spells && g_get_spell ? g_get_spell(spells, spell) : nullptr;
  if (!entry || !g_spell_name || !g_free_text) return "spell " + std::to_string(spell);
  ExoText text;
  g_spell_name(entry, &text);
  std::string name = text.text ? text.text : "";
  g_free_text(&text);
  return name.empty() ? "spell " + std::to_string(spell) : name;
}

// The base item of the item a spell was cast from (-1: none).
int CastItemBase(void* creature) {
  const uint32_t item_id = Field<uint32_t>(creature, ee::kCreatureLastSpellCastItem);
  if (item_id == ee::kObjectInvalid || !g_get_item) return -1;
  void* app = ServerApp();
  void* item = app ? g_get_item(app, item_id) : nullptr;
  return item ? Field<int32_t>(item, ee::kItemBaseItem) : -1;
}

bool HasEffectOf(const void* object, uint32_t caster, int spell) {
  void* const* list = Field<void* const*>(object, ee::kObjectEffects);
  const int count = Field<int32_t>(object, ee::kObjectEffectCount);
  if (!list || count <= 0 || count > 4096) return false;
  for (int i = 0; i < count; ++i) {
    const void* effect = list[i];
    if (effect && Field<uint32_t>(effect, ee::kEffectCreator) == caster &&
        Field<int32_t>(effect, ee::kEffectSpellId) == spell) {
      return true;
    }
  }
  return false;
}

// The objects of the caster's area that hold its spell (its effects, its areas of effect); with `remove`, the
// effects removed and the areas destroyed. The count found.
int Sweep(void* caster, int spell, bool remove) {
  void* area = engine::AreaOf(caster);
  void* app = ServerApp();
  if (!area || !app || !g_get_object) return 0;
  const uint32_t* ids = Field<const uint32_t*>(area, ee::kAreaObjectIds);
  const int count = Field<int32_t>(area, ee::kAreaObjectCount);
  if (!ids || count <= 0 || count > 100000) return 0;
  const uint32_t caster_id = IdOf(caster);
  std::vector<uint32_t> holders, areas;
  for (int i = 0; i < count; ++i) {
    void* object = g_get_object(app, ids[i]);
    if (!object) continue;
    if (Field<uint8_t>(object, ee::kGameObjectType) == ee::kObjectTypeAreaOfEffect) {
      void* aoe = g_get_aoe ? g_get_aoe(app, ids[i]) : nullptr;
      if (aoe && Field<uint32_t>(aoe, ee::kAoeCreator) == caster_id &&
          (Field<int32_t>(aoe, ee::kAoeEffectSpellId) == spell || Field<int32_t>(aoe, ee::kObjectLastSpellId) == spell)) {
        areas.push_back(ids[i]);
      }
      continue;
    }
    const AsObjectFn as_object = (*reinterpret_cast<AsObjectFn* const*>(object))[ee::kVtableAsNwsObject / 8];
    void* nws = as_object ? as_object(object) : nullptr;
    if (nws && HasEffectOf(nws, caster_id, spell)) holders.push_back(ids[i]);
  }
  if (remove) {
    char code[512];
    for (uint32_t id : holders) {
      sprintf_s(code,
                "object c = StringToObject(\"%x\"); effect e = GetFirstEffect(OBJECT_SELF); while (GetIsEffectValid(e)) "
                "{ if (GetEffectCreator(e) == c && GetEffectSpellId(e) == %d) RemoveEffect(OBJECT_SELF, e); "
                "e = GetNextEffect(OBJECT_SELF); }",
                caster_id, spell);
      RunScriptChunk(code, id);
    }
    for (uint32_t id : areas) RunScriptChunk("DestroyObject(OBJECT_SELF);", id);
  }
  return static_cast<int>(holders.size() + areas.size());
}

// The caster's Concentration (restored from its local variable after a load); null: none.
Held* Find(void* creature) {
  const uint32_t id = IdOf(creature);
  auto found = g_held.find(id);
  if (found != g_held.end()) return &found->second;
  const int stored = engine::GetLocalInt(creature, kVarHeld);
  if (stored <= 0) return nullptr;
  Held& held = g_held[id];
  held.spell = stored - 1;
  held.since = 0;  // settled long ago
  return &held;
}

void End(void* creature, const std::string& why) {
  const uint32_t id = IdOf(creature);
  auto found = g_held.find(id);
  if (found == g_held.end()) return;
  const int spell = found->second.spell;
  g_held.erase(found);
  engine::SetLocalInt(creature, kVarHeld, 0);
  const int swept = Sweep(creature, spell, true);
  const std::string name = SpellName(spell);
  if (IsPlayer(creature)) TellPlayer(id, "nwn5e: Concentration on " + name + " ends - " + why + ".");
  Log("concentration: 0x%08X %s ends - %s (%d holder(s) cleared)", id, name.c_str(), why.c_str(), swept);
}

void Begin(void* creature, int spell) {
  const uint32_t id = IdOf(creature);
  if (Find(creature)) End(creature, "you cast " + SpellName(spell));
  Held& held = g_held[id];
  held = Held();
  held.spell = spell;
  held.since = GetTickCount64();
  held.next_look = held.since + kSettleMs;
  engine::SetLocalInt(creature, kVarHeld, spell + 1);
  if (g_logged < 40) {
    ++g_logged;
    Log("concentration: 0x%08X concentrates on %s (spell %d)", id, SpellName(spell).c_str(), spell);
  }
}

// The script bridge reads each Concentration spell's ImpactScript once (spells.2da, as the module serves it).
void ReadImpactScripts(void* creature) {
  g_impact_read = true;
  std::string code = "string s = \"\";";
  for (int spell : rules::ConcentrationSpells()) {
    code += " s += Get2DAString(\"spells\", \"ImpactScript\", " + std::to_string(spell) + ") + \";\";";
  }
  code += " SetLocalString(OBJECT_SELF, \"" + std::string(kVarScripts) + "\", s);";
  RunScriptChunk(code.c_str(), IdOf(creature));
  const std::string names = engine::GetLocalString(creature, kVarScripts);
  size_t at = 0;
  for (int spell : rules::ConcentrationSpells()) {
    const size_t end = names.find(';', at);
    if (end == std::string::npos) break;
    const std::string name = Lower(names.substr(at, end - at));
    if (!name.empty() && name != "****") {
      g_impact[spell] = name;
      g_impact_names.insert(name);
    }
    at = end + 1;
  }
  Log("concentration: %zu of %zu Concentration spells' impact scripts read", g_impact.size(),
      rules::ConcentrationSpells().size());
}

// A script starts (script_hooks.h): a Concentration spell's own impact script, run for its caster.
void ScriptStarting(uint32_t self, const char* name, uint32_t length) {
  if (g_impact_names.empty() || !name || length == 0 || length > 32) return;
  char lower[33];  // every script runs through here: its name first, the creature only for an impact script's
  for (uint32_t i = 0; i < length; ++i) lower[i] = static_cast<char>(tolower(static_cast<unsigned char>(name[i])));
  lower[length] = 0;
  if (g_impact_names.find(lower) == g_impact_names.end()) return;
  void* creature = CreatureById(self);
  if (!creature) return;
  const int spell = Field<int32_t>(creature, ee::kObjectLastSpellId);
  const auto impact = g_impact.find(spell);
  if (impact == g_impact.end() || impact->second.size() != length ||
      _strnicmp(impact->second.c_str(), name, length) != 0) {
    return;
  }
  if (!rules::ItemCastNeedsConcentration(CastItemBase(creature))) {  // 3A: a potion's
    if (g_logged < 40) {
      ++g_logged;
      Log("concentration: 0x%08X %s from a potion - no Concentration", self, SpellName(spell).c_str());
    }
    return;
  }
  Begin(creature, spell);
}

// OnApplyDamage's listener: a concentrating creature that took damage saves (Constitution).
void Damaged(uint32_t creator, void* target, int damage) {
  if (damage <= 0 || !target || !g_save_roll) return;
  Held* held = Find(target);
  if (!held || !held->end.empty()) return;
  const int dc = rules::ConcentrationSaveDc(damage);
  const uint32_t id = IdOf(target);
  // A Constitution save whatever the spell's own save is (C1's table would answer for the creature's last spell):
  // the DC on the 2024 scale, remembered for the creature itself.
  RememberSaveDc2024(id, dc);
  SetForcedSaveAbility(rules::Ability::kConstitution);
  const uint8_t result =
      g_save_roll(target, static_cast<uint8_t>(ee::kSaveFortitude), static_cast<uint16_t>(dc), 0, id, 1, 0, 1);
  SetForcedSaveAbility(std::nullopt);
  const std::string name = SpellName(held->spell);
  (void)creator;
  if (result != 0) {
    if (IsPlayer(target)) {
      TellPlayer(id, "nwn5e: Concentration on " + name + " kept (Constitution save, DC " + std::to_string(dc) + ").");
    }
    Log("concentration: 0x%08X keeps %s - %d damage, DC %d made", id, name.c_str(), damage, dc);
    return;
  }
  held->end = "a failed Constitution save (DC " + std::to_string(dc) + ", " + std::to_string(damage) + " damage)";
  Log("concentration: 0x%08X loses %s - %d damage, DC %d failed", id, name.c_str(), damage, dc);
}

// Every creature update: a concentrating one's endings, and whether its spell is still there.
void Update(void* creature) {
  if (!g_impact_read && IsPlayer(creature) && engine::AreaOf(creature)) ReadImpactScripts(creature);
  if (g_held.empty()) return;
  const auto found = g_held.find(IdOf(creature));  // (after a load the panel's provider or a hit restores it)
  if (found == g_held.end()) return;
  Held* held = &found->second;
  if (!held->end.empty()) return End(creature, held->end);
  if (engine::IsDeadOrDying(creature)) return End(creature, "you're down");
  if (IncapacitatedByCondition(creature)) return End(creature, "you're Incapacitated");
  if (BarbarianRaging(creature)) return End(creature, "you rage");
  const ULONGLONG now = GetTickCount64();
  if (now < held->next_look) return;
  held->next_look = now + kOverCheckMs;
  if (Sweep(creature, held->spell, false) > 0) return;
  const uint32_t id = IdOf(creature);
  Log("concentration: 0x%08X %s is over (none of its effects left here)", id, SpellName(held->spell).c_str());
  g_held.erase(id);
  engine::SetLocalInt(creature, kVarHeld, 0);
}

// 4: the resource panel's line, and its click.
bool ConcentrationHudPool(void* creature, rules::HudPool* pool) {
  if (!Faithful()) return false;
  const Held* held = Find(creature);
  if (!held) return false;
  const std::string name = SpellName(held->spell);
  pool->name = "Concentrating: " + name;
  pool->line = true;
  pool->click = "concentration";
  pool->tooltip = "Concentrating on " + name + ". It ends if you cast another Concentration spell, fail a "
                  "Constitution save when you take damage (DC 10, or half the damage if higher), are Incapacitated "
                  "or rage.\n\nClick to end it now.";
  return true;
}

void PanelClicked(void* creature) {
  if (Find(creature)) End(creature, "you ended it");
}

}  // namespace

void InstallConcentration() {
  if (!Faithful()) {
    Log("concentration: NWN's - no Concentration on spells (key concentration)");
    return;
  }
  g_app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  g_rules = reinterpret_cast<void**>(EngineExport(ee::kRules));
  g_get_object = reinterpret_cast<GetObjectFn>(EngineExport(ee::kGetGameObject));
  g_get_item = reinterpret_cast<GetObjectFn>(EngineExport(ee::kGetItemByGameObjectId));
  g_get_aoe = reinterpret_cast<GetObjectFn>(EngineExport(ee::kGetAreaOfEffectById));
  g_get_spell = reinterpret_cast<GetSpellFn>(EngineExport(ee::kGetSpell));
  g_spell_name = reinterpret_cast<SpellNameFn>(EngineExport(ee::kSpellGetNameText));
  g_free_text = reinterpret_cast<FreeTextFn>(EngineExport(ee::kExoStringDestructor));
  g_save_roll = reinterpret_cast<SaveRollFn>(EngineExport(ee::kSavingThrowRoll));
  const bool found = g_app_manager && g_rules && g_get_object && g_get_item && g_get_aoe && g_get_spell &&
                     g_spell_name && g_free_text && g_save_roll;
  const bool hooked = found && AddScriptStartListener(&ScriptStarting) && AddDamageListener(&Damaged) &&
                      AddCreatureUpdateListener(&Update);
  if (!hooked) {
    Log("SKIP concentration #37: not mapped or hooked - spells keep NWN's (no Concentration)");
    return;
  }
  AddHudPool(&ConcentrationHudPool);
  AddHudClick("concentration", &PanelClicked);
  Log("concentration: SRD 5.2's (%zu of NWN's spells) - one at a time, a Constitution save on damage (DC 10 or half, "
      "at most 30), Incapacitated, down or raging ends it; a potion's needs none; the resource panel's line ends it "
      "on a click (#37)",
      rules::ConcentrationSpells().size());
}
