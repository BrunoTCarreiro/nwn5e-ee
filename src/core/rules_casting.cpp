// SK7 (port of 1.69's rules_casting.cpp; docs/EE-PORT.md E3b), `concentration`
// faithful: no Concentration checks on casting. 2024 casting is not
// interrupted by damage (only keeping a concentration spell takes a
// Constitution save — later), restrained casters cast normally, and there is
// no defensive casting (casting draws no opportunity attacks, D2).
// EE rolls Concentration on damage while casting (OnApplyDamage, only for a
// spell whose spells.2da UseConcentration is 1) and when casting while
// entangled (AIActionCastSpell, only when it is not 0): faithful sets every
// spell's UseConcentration to 0 in memory after spells.2da loads (the file is
// unchanged). The third check, defensive casting, goes with its mode
// (rules_nwn_skills.cpp refuses it). NWN mode: NWN's.

#include <stdint.h>

#include "config.h"
#include "creature.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"

namespace {

using LoadFn = void (*)(void* spells);
using GetSpellFn = void* (*)(const void* spells, int id);

LoadFn g_load = nullptr;
GetSpellFn g_get_spell = nullptr;

void NoConcentration(void* spells) {
  if (!spells || !g_get_spell) return;
  const int count = Field<int32_t>(spells, ee::kSpellArrayCount);
  if (count <= 0 || count > 100000) return;
  int changed = 0;
  for (int id = 0; id < count; ++id) {
    void* spell = g_get_spell(spells, id);
    if (!spell || Field<int32_t>(spell, ee::kSpellUseConcentration) == 0) continue;
    SetField<int32_t>(spell, ee::kSpellUseConcentration, 0);
    ++changed;
  }
  Log("concentration: %d of %d spells no longer need Concentration while casting (faithful)", changed, count);
}

void Load(void* spells) {
  g_load(spells);
  NoConcentration(spells);
}

}  // namespace

void InstallCastingRules() {
  if (g_config.concentration != rules::RuleVariant::kFaithful) return;
  g_get_spell = reinterpret_cast<GetSpellFn>(EngineExport(ee::kGetSpell));
  if (!g_get_spell) {
    Log("SKIP concentration: CNWSpellArray::GetSpell is not exported");
    return;
  }
  HookExport("CNWSpellArray::Load (no Concentration while casting)", ee::kLoadSpellArray,
             reinterpret_cast<void*>(&Load), reinterpret_cast<void**>(&g_load));
  // Normally the rules load after nwn5e starts; if they already have, now.
  void** rules = reinterpret_cast<void**>(EngineExport(ee::kRules));
  if (rules && *rules) NoConcentration(Field<void*>(*rules, ee::kRulesSpells));
}
