// The moment a spell takes effect (spell_impact.h; 0.87.0, from rules_concentration.cpp's 0.86).

#include "spell_impact.h"

#include <ctype.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "creature.h"
#include "creature_update.h"
#include "ee_layout.h"
#include "effects.h"
#include "engine.h"
#include "hooks.h"
#include "log.h"
#include "script_hooks.h"

namespace {

constexpr const char* kVarScripts = "nwn5e_impact_scripts";  // the script bridge's answer (once a game)

using GetObjectFn = void* (*)(void* app, uint32_t id);
using GetSpellFn = void* (*)(const void* spells, int id);
using SpellNameFn = void* (*)(void* spell, void* out);
using FreeTextFn = void (*)(void* text);
using LoadModuleFinishFn = uint32_t (*)(void* module);

struct ExoText {
  char* text = nullptr;
  uint32_t length = 0;
  uint32_t capacity = 0;
};

struct Watch {
  std::unordered_set<int> spells;
  SpellImpactListener listener = nullptr;
};

std::vector<Watch> g_watches;
std::vector<int> g_watched;  // every watched spell, ascending, once
std::vector<GameStartListener> g_starts;
std::unordered_map<int, std::string> g_impact;  // a watched spell's ImpactScript, lower case
std::unordered_set<std::string> g_names;        // the same names: a script's first look
bool g_read = false;
bool g_installed = false;
bool g_failed = false;
void** g_app_manager = nullptr;
void** g_rules = nullptr;
GetObjectFn g_get_item = nullptr;
GetSpellFn g_get_spell = nullptr;
SpellNameFn g_spell_name = nullptr;
FreeTextFn g_free_text = nullptr;
LoadModuleFinishFn g_load_module_finish = nullptr;

uint32_t IdOf(const void* object) { return object ? Field<uint32_t>(object, ee::kObjectId) : ee::kObjectInvalid; }

bool IsPlayer(void* creature) { return creature && Field<int32_t>(creature, ee::kCreatureIsPlayerCharacter) != 0; }

std::string Lower(std::string text) {
  for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return text;
}

// The script bridge reads each watched spell's ImpactScript (spells.2da, as the module serves it).
void ReadImpactScripts(void* creature) {
  g_read = true;
  std::string code = "string s = \"\";";
  for (int spell : g_watched) {
    code += " s += Get2DAString(\"spells\", \"ImpactScript\", " + std::to_string(spell) + ") + \";\";";
  }
  code += " SetLocalString(OBJECT_SELF, \"" + std::string(kVarScripts) + "\", s);";
  RunScriptChunk(code.c_str(), IdOf(creature));
  const std::string names = engine::GetLocalString(creature, kVarScripts);
  size_t at = 0;
  for (int spell : g_watched) {
    const size_t end = names.find(';', at);
    if (end == std::string::npos) break;
    const std::string name = Lower(names.substr(at, end - at));
    if (!name.empty() && name != "****") {
      g_impact[spell] = name;
      g_names.insert(name);
    }
    at = end + 1;
  }
  Log("spell impacts: %zu of %zu watched spells' impact scripts read (#37, #38)", g_impact.size(), g_watched.size());
}

// Every script about to run: a watched spell's own impact script, run for its caster.
void ScriptStarting(uint32_t self, const char* name, uint32_t length) {
  if (g_names.empty() || !name || length == 0 || length > 32) return;
  char lower[33];  // every script runs through here: its name first, the creature only for an impact script's
  for (uint32_t i = 0; i < length; ++i) lower[i] = static_cast<char>(tolower(static_cast<unsigned char>(name[i])));
  lower[length] = 0;
  if (g_names.find(lower) == g_names.end()) return;
  void* creature = CreatureById(self);
  if (!creature) return;
  const int spell = Field<int32_t>(creature, ee::kObjectLastSpellId);
  const auto impact = g_impact.find(spell);
  if (impact == g_impact.end() || impact->second != lower) return;
  for (const Watch& watch : g_watches) {
    if (watch.spells.count(spell)) watch.listener(creature, spell);
  }
}

void Update(void* creature) {
  if (!g_read && !g_watched.empty() && IsPlayer(creature) && engine::AreaOf(creature)) ReadImpactScripts(creature);
}

uint32_t LoadModuleFinish(void* module) {
  const uint32_t result = g_load_module_finish(module);
  g_impact.clear();
  g_names.clear();
  g_read = false;
  for (GameStartListener listener : g_starts) listener();
  return result;
}

bool Install() {
  if (g_installed) return true;
  if (g_failed) return false;
  g_app_manager = reinterpret_cast<void**>(EngineExport(ee::kAppManager));
  g_rules = reinterpret_cast<void**>(EngineExport(ee::kRules));
  g_get_item = reinterpret_cast<GetObjectFn>(EngineExport(ee::kGetItemByGameObjectId));
  g_get_spell = reinterpret_cast<GetSpellFn>(EngineExport(ee::kGetSpell));
  g_spell_name = reinterpret_cast<SpellNameFn>(EngineExport(ee::kSpellGetNameText));
  g_free_text = reinterpret_cast<FreeTextFn>(EngineExport(ee::kExoStringDestructor));
  const bool found = g_app_manager && g_rules && g_get_item && g_get_spell && g_spell_name && g_free_text;
  const bool hooked =
      found && AddScriptStartListener(&ScriptStarting) && AddCreatureUpdateListener(&Update) &&
      HookExport("CNWSModule::LoadModuleFinish (a game started: the last one's spell state cleared, #37 #38)",
                 ee::kModuleLoadFinish, reinterpret_cast<void*>(&LoadModuleFinish),
                 reinterpret_cast<void**>(&g_load_module_finish));
  if (!hooked) {
    g_failed = true;
    Log("SKIP spell impacts: not mapped or hooked - Concentration and material components do nothing");
    return false;
  }
  g_installed = true;
  return true;
}

}  // namespace

bool AddSpellImpactListener(const std::vector<int>& spells, SpellImpactListener listener) {
  if (!listener || !Install()) return false;
  Watch watch;
  watch.spells.insert(spells.begin(), spells.end());
  watch.listener = listener;
  g_watches.push_back(watch);
  for (int spell : spells) g_watched.push_back(spell);
  std::sort(g_watched.begin(), g_watched.end());
  g_watched.erase(std::unique(g_watched.begin(), g_watched.end()), g_watched.end());
  return true;
}

bool AddGameStartListener(GameStartListener listener) {
  if (!listener || !Install()) return false;
  g_starts.push_back(listener);
  return true;
}

int SpellCastItemBase(void* caster) {
  const uint32_t item_id = caster ? Field<uint32_t>(caster, ee::kCreatureLastSpellCastItem) : ee::kObjectInvalid;
  if (item_id == ee::kObjectInvalid || !g_get_item || !g_app_manager || !*g_app_manager) return -1;
  void* app = Field<void*>(*g_app_manager, ee::kAppManagerServerApp);
  void* item = app ? g_get_item(app, item_id) : nullptr;
  return item ? Field<int32_t>(item, ee::kItemBaseItem) : -1;
}

std::string SpellNameOf(int spell) {
  void* spells = g_rules && *g_rules ? Field<void*>(*g_rules, ee::kRulesSpells) : nullptr;
  void* entry = spells && g_get_spell ? g_get_spell(spells, spell) : nullptr;
  if (!entry || !g_spell_name || !g_free_text) return "spell " + std::to_string(spell);
  ExoText text;
  g_spell_name(entry, &text);
  std::string name = text.text ? text.text : "";
  g_free_text(&text);
  return name.empty() ? "spell " + std::to_string(spell) : name;
}
