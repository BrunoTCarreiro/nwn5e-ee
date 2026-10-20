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

  // E1 — attacks and AC (on/off rules, as on 1.69).
  bool attack_proficiency = true;  // attack rolls use the proficiency bonus instead of the 3e BAB
  bool creature_ac_by_cr = true;   // non-player creatures use the expected AC for their CR
  bool extra_attack = true;        // PCs attack per 2024 Extra Attack; no 3e -5 on later attacks
  bool advantage_disadvantage = true;  // situational modifiers become 2024 advantage/disadvantage
  bool retarget_remaining_attacks = true;  // after a kill, the round's remaining attacks go to the nearest enemy
  int ranged_retarget_range = 40;          // metres a ranged retarget looks (capped at the weapon's range)
  // R1: faithful removes NWN's racial attack/AC bonuses vs specific enemies.
  rules::RuleVariant racial_combat_bonuses = rules::RuleVariant::kFaithful;
  // D1: faithful = no off-hand penalties, one off-hand attack (Light weapons,
  // double weapon or Dual Wielder), no Strength on its damage unless negative
  // or with the Two-Weapon Fighting style. nwn/off = NWN's rules.
  rules::RuleVariant two_weapon_fighting = rules::RuleVariant::kFaithful;
  // E1: faithful = two-handed melee adds Strength once; nwn/off = NWN's 1.5 x.
  rules::RuleVariant two_handed_strength = rules::RuleVariant::kFaithful;

  // [debug] — test switches, not rules.
  bool debug_every_hit_crits = false;    // every hit is a critical (crit immunity still applies)
  bool debug_combat_breakdown = false;   // the engine's own per-term attack and damage breakdown
  int debug_attacks_per_round = 0;       // >0: player characters make this many main-hand attacks
  int debug_creature_ac = 0;             // >0: every non-player creature has this AC (forces misses)
  int debug_creature_hp_multiplier = 0;  // >1: a creature's hit points x this when a character first attacks it
  int debug_creature_min_hp = 0;         // >0: ... and at least this many
  bool debug_attack_trace = false;       // log round setup and every PC attack roll
};

extern Config g_config;
void ConfigLoad(const wchar_t* directory);
