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
  g_config.opportunity_attacks = ReadVariant(ini, L"opportunity_attacks", g_config.mode);
  g_config.conditions = ReadVariant(ini, L"conditions", g_config.mode);
  g_config.weapon_mastery = ReadVariant(ini, L"weapon_mastery", g_config.mode);
  g_config.saves = ReadVariant(ini, L"saves", g_config.mode);
  g_config.skills = ReadVariant(ini, L"skills", g_config.mode);
  g_config.perception = ReadVariant(ini, L"perception", g_config.mode);
  g_config.hiding_needs_15 = ReadBool(ini, L"hiding_needs_15", g_config.hiding_needs_15);
  g_config.found_up_close = ReadBool(ini, L"found_up_close", g_config.found_up_close);
  g_config.alert_creatures_search = ReadBool(ini, L"alert_creatures_search", g_config.alert_creatures_search);
  g_config.spell_attacks = ReadVariant(ini, L"spell_attacks", g_config.mode);
  g_config.concentration = ReadVariant(ini, L"concentration", g_config.mode);
  g_config.knockdown = ReadVariant(ini, L"knockdown", g_config.mode);
  g_config.initiative = ReadVariant(ini, L"initiative", g_config.mode);
  g_config.nwn_only_skills = ReadVariant(ini, L"nwn_only_skills", g_config.mode);
  g_config.progression = ReadVariant(ini, L"progression", g_config.mode);
  g_config.multiclass = ReadVariant(ini, L"multiclass", g_config.mode);
  g_config.alignment = ReadVariant(ini, L"alignment", g_config.mode);
  g_config.hit_points = ReadVariant(ini, L"hit_points", g_config.mode);
  g_config.feat_cadence = ReadVariant(ini, L"feat_cadence", g_config.mode);
  g_config.ability_scores = ReadVariant(ini, L"ability_scores", g_config.mode);
  g_config.class_features = ReadVariant(ini, L"class_features", g_config.mode);
  g_config.species = ReadVariant(ini, L"species", g_config.mode);
  g_config.feats = ReadVariant(ini, L"feats", g_config.mode);
  g_config.starting_equipment = ReadVariant(ini, L"starting_equipment", g_config.mode);
  g_config.size = ReadVariant(ini, L"size", g_config.mode);
  g_config.finesse = ReadVariant(ini, L"finesse", g_config.mode);
  g_config.rests = ReadVariant(ini, L"rests", g_config.mode);
  g_config.rest_clock = ReadBool(ini, L"rest_clock", g_config.rest_clock);
  g_config.dying = ReadVariant(ini, L"dying", g_config.mode);
  g_config.death_save_aura_vfx =
      GetPrivateProfileIntW(L"rules", L"death_save_aura_vfx", g_config.death_save_aura_vfx, ini);
  g_config.death_save_stable_vfx =
      GetPrivateProfileIntW(L"rules", L"death_save_stable_vfx", g_config.death_save_stable_vfx, ini);
  g_config.death_save_success_vfx =
      GetPrivateProfileIntW(L"rules", L"death_save_success_vfx", g_config.death_save_success_vfx, ini);
  g_config.death_save_failure_vfx =
      GetPrivateProfileIntW(L"rules", L"death_save_failure_vfx", g_config.death_save_failure_vfx, ini);
  g_config.death_save_nat1_vfx =
      GetPrivateProfileIntW(L"rules", L"death_save_nat1_vfx", g_config.death_save_nat1_vfx, ini);
  g_config.death_save_nat20_vfx =
      GetPrivateProfileIntW(L"rules", L"death_save_nat20_vfx", g_config.death_save_nat20_vfx, ini);
  g_config.death_save_marker = ReadBool(ini, L"death_save_marker", g_config.death_save_marker);
  g_config.short_rest_vfx = GetPrivateProfileIntW(L"rules", L"short_rest_vfx", g_config.short_rest_vfx, ini);
  g_config.short_rest_vfx_scale =
      GetPrivateProfileIntW(L"rules", L"short_rest_vfx_scale", g_config.short_rest_vfx_scale, ini);
  g_config.served_data = ReadBool(ini, L"served_data", g_config.served_data);
  g_config.resume_attack_after_potion =
      ReadBool(ini, L"resume_attack_after_potion", g_config.resume_attack_after_potion);
  g_config.debug_every_hit_crits = ReadBool(ini, L"every_hit_crits", g_config.debug_every_hit_crits, L"debug");
  g_config.debug_combat_breakdown = ReadBool(ini, L"combat_breakdown", g_config.debug_combat_breakdown, L"debug");
  g_config.debug_attacks_per_round = GetPrivateProfileIntW(L"debug", L"attacks_per_round", 0, ini);
  g_config.debug_creature_ac = GetPrivateProfileIntW(L"debug", L"creature_ac", 0, ini);
  g_config.debug_creature_hp_multiplier = GetPrivateProfileIntW(L"debug", L"creature_hp_multiplier", 0, ini);
  g_config.debug_creature_min_hp = GetPrivateProfileIntW(L"debug", L"creature_min_hp", 0, ini);
  g_config.debug_attack_trace = ReadBool(ini, L"attack_trace", g_config.debug_attack_trace, L"debug");
  g_config.debug_players_poisoned = ReadBool(ini, L"players_poisoned", g_config.debug_players_poisoned, L"debug");
  g_config.debug_served_data_probe =
      ReadBool(ini, L"served_data_probe", g_config.debug_served_data_probe, L"debug");
  g_config.debug_nui_test = ReadBool(ini, L"nui_test", g_config.debug_nui_test, L"debug");
  g_config.debug_client_nui_test = ReadBool(ini, L"client_nui_test", g_config.debug_client_nui_test, L"debug");
  // test_commands (give_command: its earlier name, still read).
  g_config.debug_test_commands = ReadBool(ini, L"test_commands", g_config.debug_test_commands, L"debug") ||
                                 ReadBool(ini, L"give_command", false, L"debug");
  Log("config: mode=%s crit_nat20_only=%d crit_no_confirmation=%d crit_double_damage=%d crit_dice_only=%d "
      "crits_against_players=%d log_attacks=%d",
      VariantName(g_config.mode), g_config.crit_nat20_only, g_config.crit_no_confirmation,
      g_config.crit_double_damage, g_config.crit_dice_only, g_config.crits_against_players, g_config.log_attacks);
  Log("config: attack_proficiency=%d creature_ac_by_cr=%d extra_attack=%d advantage_disadvantage=%d",
      g_config.attack_proficiency, g_config.creature_ac_by_cr, g_config.extra_attack,
      g_config.advantage_disadvantage);
  Log("config: retarget_remaining_attacks=%d ranged_retarget_range=%d m", g_config.retarget_remaining_attacks,
      g_config.ranged_retarget_range);
  Log("config: opportunity_attacks=%s resume_attack_after_potion=%d conditions=%s weapon_mastery=%s",
      VariantName(g_config.opportunity_attacks), g_config.resume_attack_after_potion,
      VariantName(g_config.conditions), VariantName(g_config.weapon_mastery));
  Log("config: racial_combat_bonuses=%s two_weapon_fighting=%s two_handed_strength=%s",
      VariantName(g_config.racial_combat_bonuses), VariantName(g_config.two_weapon_fighting),
      VariantName(g_config.two_handed_strength));
  Log("config: saves=%s skills=%s perception=%s spell_attacks=%s concentration=%s knockdown=%s initiative=%s "
      "nwn_only_skills=%s",
      VariantName(g_config.saves), VariantName(g_config.skills), VariantName(g_config.perception),
      VariantName(g_config.spell_attacks), VariantName(g_config.concentration), VariantName(g_config.knockdown),
      VariantName(g_config.initiative), VariantName(g_config.nwn_only_skills));
  Log("config: progression=%s multiclass=%s alignment=%s hit_points=%s feat_cadence=%s ability_scores=%s "
      "class_features=%s species=%s feats=%s starting_equipment=%s size=%s finesse=%s rests=%s rest_clock=%d "
      "dying=%s served_data=%d",
      VariantName(g_config.progression), VariantName(g_config.multiclass), VariantName(g_config.alignment),
      VariantName(g_config.hit_points), VariantName(g_config.feat_cadence), VariantName(g_config.ability_scores),
      VariantName(g_config.class_features), VariantName(g_config.species), VariantName(g_config.feats),
      VariantName(g_config.starting_equipment), VariantName(g_config.size), VariantName(g_config.finesse),
      VariantName(g_config.rests), g_config.rest_clock, VariantName(g_config.dying), g_config.served_data);
  if (g_config.perception == rules::RuleVariant::kFaithful) {
    Log("config: hiding_needs_15=%d found_up_close=%d alert_creatures_search=%d", g_config.hiding_needs_15,
        g_config.found_up_close, g_config.alert_creatures_search);
  }
  if (g_config.debug_players_poisoned) {
    Log("config: [debug] players_poisoned=1 - player characters count as poisoned for ability checks");
  }
  if (g_config.debug_every_hit_crits) Log("config: [debug] every_hit_crits=1 - every hit is a critical");
  if (g_config.debug_nui_test) Log("config: [debug] nui_test=1 - the NUI test window opens on arrival");
  if (g_config.debug_client_nui_test) Log("config: [debug] client_nui_test=1 - the client test window opens on the main menu");
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
