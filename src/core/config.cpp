#include "config.h"

#include <windows.h>
#include <stdio.h>

#include "log.h"

Config g_config;

namespace {

bool ReadBool(const wchar_t* ini, const wchar_t* key, bool fallback, const wchar_t* section = L"rules") {
  return GetPrivateProfileIntW(section, key, fallback ? 1 : 0, ini) != 0;
}

// A faithful / nwn / off key (PLAN.md "Modes"); empty or missing -> fallback.
rules::RuleVariant ReadVariant(const wchar_t* ini, const wchar_t* key, rules::RuleVariant fallback) {
  wchar_t wide[32] = {};
  GetPrivateProfileStringW(L"rules", key, L"", wide, 32, ini);
  char text[32] = {};
  for (int i = 0; i < 31 && wide[i]; ++i) text[i] = wide[i] < 128 ? static_cast<char>(wide[i]) : '?';
  return rules::ParseRuleVariant(text, fallback);
}

const char* VariantName(rules::RuleVariant variant) {
  switch (variant) {
    case rules::RuleVariant::kFaithful:
      return "faithful";
    case rules::RuleVariant::kNwn:
      return "nwn";
    default:
      return "off";
  }
}

}  // namespace

void ConfigLoad(const wchar_t* directory) {
  wchar_t ini[MAX_PATH];
  swprintf_s(ini, L"%s\\nwn5e.ini", directory);
  g_config.mode = ReadVariant(ini, L"mode", g_config.mode);
  g_config.crit_nat20_only = ReadBool(ini, L"crit_nat20_only", g_config.crit_nat20_only);
  g_config.crit_no_confirmation = ReadBool(ini, L"crit_no_confirmation", g_config.crit_no_confirmation);
  g_config.crit_double_damage = ReadBool(ini, L"crit_double_damage", g_config.crit_double_damage);
  g_config.crit_dice_only = ReadBool(ini, L"crit_dice_only", g_config.crit_dice_only);
  g_config.crits_against_players = ReadBool(ini, L"crits_against_players", g_config.crits_against_players);
  g_config.log_attacks = ReadBool(ini, L"log_attacks", g_config.log_attacks);
  g_config.debug_every_hit_crits = ReadBool(ini, L"every_hit_crits", g_config.debug_every_hit_crits, L"debug");
  g_config.debug_combat_breakdown = ReadBool(ini, L"combat_breakdown", g_config.debug_combat_breakdown, L"debug");
  Log("config: mode=%s crit_nat20_only=%d crit_no_confirmation=%d crit_double_damage=%d crit_dice_only=%d "
      "crits_against_players=%d log_attacks=%d",
      VariantName(g_config.mode), g_config.crit_nat20_only, g_config.crit_no_confirmation,
      g_config.crit_double_damage, g_config.crit_dice_only, g_config.crits_against_players, g_config.log_attacks);
  if (g_config.debug_every_hit_crits) Log("config: [debug] every_hit_crits=1 - every hit is a critical");
  if (g_config.debug_combat_breakdown) Log("config: [debug] combat_breakdown=1 - the engine's own attack breakdown");
}
