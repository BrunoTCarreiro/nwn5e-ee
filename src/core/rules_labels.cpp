// C8 and SK1 (port of 1.69's rules_labels.cpp; docs/EE-PORT.md E3a / E3b,
// EP4): in faithful mode the game shows 2024 names — the three NWN saves as
// "Constitution / Dexterity / Wisdom" (character sheet, combat log "... Save",
// effect and spell texts), the skills by their 2024 names — swapped in memory
// as the game looks the strings up (CTlkTable::FetchInternal, which every
// string lookup goes through, the combat log's save label included);
// dialog.tlk is not changed. NWN mode keeps NWN's labels.

#include <stdint.h>

#include "config.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "rules/saves.h"
#include "rules/skills.h"

namespace {

using FetchInternalFn = int (*)(void* table, uint32_t strref, void* result, int flag);
using ExoStringAssignFn = void* (*)(void* exo_string, const char* text);

FetchInternalFn g_fetch_internal = nullptr;
ExoStringAssignFn g_assign = nullptr;
int g_logged = 0;

const char* Label2024(uint32_t strref) {
  const char* label = g_config.saves == rules::RuleVariant::kFaithful ? rules::Faithful2024SaveLabel(strref) : nullptr;
  if (!label && g_config.skills == rules::RuleVariant::kFaithful) label = rules::Faithful2024SkillLabelForStrref(strref);
  return label;
}

// STR_RES: the text is a CExoString at +0 (its copy operator and
// GetSimpleString read it there).
int FetchInternal(void* table, uint32_t strref, void* result, int flag) {
  const int found = g_fetch_internal(table, strref, result, flag);
  if (!result) return found;
  const char* label = Label2024(strref);
  if (!label) return found;
  g_assign(static_cast<uint8_t*>(result) + ee::kStrResText, label);
  if (g_config.log_attacks && g_logged < 30) {
    ++g_logged;
    Log("label: string %u shown as \"%s\"", strref, label);
  }
  return found;
}

}  // namespace

void InstallLabelRules() {
  if (g_config.saves != rules::RuleVariant::kFaithful && g_config.skills != rules::RuleVariant::kFaithful) return;
  g_assign = reinterpret_cast<ExoStringAssignFn>(EngineExport(ee::kExoStringAssignChars));
  if (!g_assign) {
    Log("SKIP labels: CExoString::operator=(const char*) is not exported");
    return;
  }
  HookExport("CTlkTable::FetchInternal (2024 save and skill labels)", ee::kTlkFetchInternal,
             reinterpret_cast<void*>(&FetchInternal), reinterpret_cast<void**>(&g_fetch_internal));
}
