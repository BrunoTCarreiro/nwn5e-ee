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
  g_config.attack_proficiency = ReadBool(ini, L"attack_proficiency", g_config.attack_proficiency);
  g_config.creature_ac_by_cr = ReadBool(ini, L"creature_ac_by_cr", g_config.creature_ac_by_cr);
  g_config.extra_attack = ReadBool(ini, L"extra_attack", g_config.extra_attack);
  g_config.advantage_disadvantage = ReadBool(ini, L"advantage_disadvantage", g_config.advantage_disadvantage);
  g_config.retarget_remaining_attacks =
      ReadBool(ini, L"retarget_remaining_attacks", g_config.retarget_remaining_attacks);
  g_config.ranged_retarget_range = GetPrivateProfileIntW(L"rules", L"ranged_retarget_range", 40, ini);
  g_config.racial_combat_bonuses = ReadVariant(ini, L"racial_combat_bonuses", g_config.mode);
  g_config.two_weapon_fighting = ReadVariant(ini, L"two_weapon_fighting", g_config.mode);
  g_config.two_handed_strength = ReadVariant(ini, L"two_handed_strength", g_config.mode);
  g_config.debug_every_hit_crits = ReadBool(ini, L"every_hit_crits", g_config.debug_every_hit_crits, L"debug");
  g_config.debug_combat_breakdown = ReadBool(ini, L"combat_breakdown", g_config.debug_combat_breakdown, L"debug");
  g_config.debug_attacks_per_round = GetPrivateProfileIntW(L"debug", L"attacks_per_round", 0, ini);
  g_config.debug_creature_ac = GetPrivateProfileIntW(L"debug", L"creature_ac", 0, ini);
  g_config.debug_creature_hp_multiplier = GetPrivateProfileIntW(L"debug", L"creature_hp_multiplier", 0, ini);
  g_config.debug_creature_min_hp = GetPrivateProfileIntW(L"debug", L"creature_min_hp", 0, ini);
  g_config.debug_attack_trace = ReadBool(ini, L"attack_trace", g_config.debug_attack_trace, L"debug");
  Log("config: mode=%s crit_nat20_only=%d crit_no_confirmation=%d crit_double_damage=%d crit_dice_only=%d "
      "crits_against_players=%d log_attacks=%d",
      VariantName(g_config.mode), g_config.crit_nat20_only, g_config.crit_no_confirmation,
      g_config.crit_double_damage, g_config.crit_dice_only, g_config.crits_against_players, g_config.log_attacks);
  Log("config: attack_proficiency=%d creature_ac_by_cr=%d extra_attack=%d advantage_disadvantage=%d",
      g_config.attack_proficiency, g_config.creature_ac_by_cr, g_config.extra_attack,
      g_config.advantage_disadvantage);
  Log("config: retarget_remaining_attacks=%d ranged_retarget_range=%d m", g_config.retarget_remaining_attacks,
      g_config.ranged_retarget_range);
  Log("config: racial_combat_bonuses=%s two_weapon_fighting=%s two_handed_strength=%s",
      VariantName(g_config.racial_combat_bonuses), VariantName(g_config.two_weapon_fighting),
      VariantName(g_config.two_handed_strength));
  if (g_config.debug_every_hit_crits) Log("config: [debug] every_hit_crits=1 - every hit is a critical");
  if (g_config.debug_combat_breakdown) Log("config: [debug] combat_breakdown=1 - the engine's own attack breakdown");
  if (g_config.debug_attacks_per_round > 0) {
    Log("config: [debug] attacks_per_round=%d - player characters make this many main-hand attacks",
        g_config.debug_attacks_per_round);
  }
  if (g_config.debug_creature_ac > 0) {
    Log("config: [debug] creature_ac=%d - every non-player creature has this AC", g_config.debug_creature_ac);
  }
  if (g_config.debug_creature_hp_multiplier > 1 || g_config.debug_creature_min_hp > 0) {
    Log("config: [debug] creature_hp_multiplier=%d creature_min_hp=%d - applied when your party first attacks a "
        "creature", g_config.debug_creature_hp_multiplier, g_config.debug_creature_min_hp);
  }
  if (g_config.debug_attack_trace) Log("config: [debug] attack_trace=1 - round setup and PC attack rolls logged");
}
