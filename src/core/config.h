#pragma once

#include "rules/modes.h"

// Rule switches from nwn5e.ini [rules]; every rule can be turned off to fall
// back to the game's own behaviour. Rules with a faithful (2024) and an
// NWN-mode version read `mode` as their default and their own key as an
// override (PLAN.md "Modes"). Keys arrive here as their rules are ported to
// EE (docs/EE-PORT.md); the 1.69 build's config.h lists the rest.
struct Config {
  rules::RuleVariant mode = rules::RuleVariant::kFaithful;

  // E0 — 2024 critical hits (on/off rules, as on 1.69).
  bool crit_nat20_only = true;        // threat only on a natural 20
  bool crit_no_confirmation = true;   // a threat is a critical hit, no second roll
  bool crit_double_damage = true;     // x2 multiplier for every weapon
  bool crit_dice_only = true;         // crits roll damage dice twice, flat bonuses once
  bool crits_against_players = true;  // no difficulty rule turning creatures' crits on players into hits
  bool log_attacks = true;            // write each rule decision to nwn5e.log

  // [debug] — test switches, not rules.
  bool debug_every_hit_crits = false;   // every hit is a critical (crit immunity still applies)
  bool debug_combat_breakdown = false;  // the engine's own per-term attack and damage breakdown
};

extern Config g_config;
void ConfigLoad(const wchar_t* directory);
