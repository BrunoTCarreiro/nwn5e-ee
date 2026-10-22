// C8, SK1 and CM0 (port of 1.69's rules_labels.cpp; docs/EE-PORT.md E3a /
// E3b / E4a, EP4): nwn5e's own strings (data\strings.txt), and in faithful
// mode the game shows 2024 names — the three NWN saves as
// "Constitution / Dexterity / Wisdom" (character sheet, combat log "... Save",
// effect and spell texts), the skills by their 2024 names — swapped in memory
// as the game looks the strings up (CTlkTable::FetchInternal, which every
// string lookup goes through, the combat log's save label included);
// dialog.tlk is not changed. NWN mode keeps NWN's labels.
//
// C1 (docs/CHARGEN.md): the creation screen's 2024 words and the species
// texts (rules/species.h) — the game's string answered with nwn5e's, each
// under its own key (species, ability_scores).

#include <stdint.h>

#include <string>

#include "config.h"
#include "ee_layout.h"
#include "hooks.h"
#include "log.h"
#include "data_serving.h"
#include "rules/saves.h"
#include "rules/skills.h"
#include "rules/species.h"

bool ClassDescriptionNote(uint32_t strref, std::string* note);  // rules_progression.cpp

namespace {

using FetchInternalFn = int (*)(void* table, uint32_t strref, void* result, int flag);
using ExoStringAssignFn = void* (*)(void* exo_string, const char* text);

FetchInternalFn g_fetch_internal = nullptr;
ExoStringAssignFn g_assign = nullptr;
int g_logged = 0;

bool Faithful(rules::StringRule rule) {
  switch (rule) {
    case rules::StringRule::kSpecies:
      return g_config.species == rules::RuleVariant::kFaithful;
    case rules::StringRule::kAbilityScores:
      return g_config.ability_scores == rules::RuleVariant::kFaithful;
    case rules::StringRule::kStartingEquipment:  // nwn5e's builds (C2) in the list: the step reads Builds
      return g_config.starting_equipment == rules::RuleVariant::kFaithful;
  }
  return false;
}

const char* Label2024(uint32_t strref) {
  const char* label = g_config.saves == rules::RuleVariant::kFaithful ? rules::Faithful2024SaveLabel(strref) : nullptr;
  if (!label && g_config.skills == rules::RuleVariant::kFaithful) label = rules::Faithful2024SkillLabelForStrref(strref);
  if (!label) {
    const auto swap = rules::CreationStringSwap2024(strref);
    if (swap && Faithful(swap->rule)) label = DataString(swap->nwn5e);
  }
  return label;
}

// STR_RES: the text is a CExoString at +0 (its copy operator and
// GetSimpleString read it there).
int FetchInternal(void* table, uint32_t strref, void* result, int flag) {
  // nwn5e's own strings (CM0): ids 16000000+ are not in dialog.tlk (the engine
  // would warn "invalid strref" and give nothing); served without asking it.
  if (const char* text = result ? DataString(strref) : nullptr) {
    g_assign(static_cast<uint8_t*>(result) + ee::kStrResText, text);
    return 1;
  }
  const int found = g_fetch_internal(table, strref, result, flag);
  if (!result) return found;
  // A class the Class page refuses: its description says why (rules_progression.cpp).
  std::string note;
  if (ClassDescriptionNote(strref, &note)) {
    const char* text = *reinterpret_cast<const char* const*>(static_cast<uint8_t*>(result) + ee::kStrResText);
    const std::string described = std::string(text ? text : "") + "\n\n" + note;
    g_assign(static_cast<uint8_t*>(result) + ee::kStrResText, described.c_str());
    return found;
  }
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
  if (g_config.saves != rules::RuleVariant::kFaithful && g_config.skills != rules::RuleVariant::kFaithful &&
      !HaveDataStrings()) {
    return;
  }
  g_assign = reinterpret_cast<ExoStringAssignFn>(EngineExport(ee::kExoStringAssignChars));
  if (!g_assign) {
    Log("SKIP labels: CExoString::operator=(const char*) is not exported");
    return;
  }
  HookExport("CTlkTable::FetchInternal (2024 labels, nwn5e's strings)", ee::kTlkFetchInternal,
             reinterpret_cast<void*>(&FetchInternal), reinterpret_cast<void**>(&g_fetch_internal));
}
