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

  // E2 — opportunity attacks, potions.
  // D2: faithful = only moving away provokes (no casting, potion, ranged,
  // unarmed or Disarm AoOs; Spring Attack no longer protects; only creatures
  // run away from take it). nwn/off = NWN's AoOs.
  rules::RuleVariant opportunity_attacks = rules::RuleVariant::kFaithful;
  // Owner request: after drinking a potion or using Second Wind mid-fight, a
  // player character goes back to attacking (NWN drops the attack order on
  // item and feat use).
  bool resume_attack_after_potion = true;
  // CN1–CN8: conditions. faithful = 2024 conditions (restrained, helpless,
  // stunned, poisoned, frightened, concealment, deafened; PLAN.md); nwn =
  // NWN's, plus the both-mode parts (stunned / paralysed / asleep targets give
  // advantage); off = NWN's.
  rules::RuleVariant conditions = rules::RuleVariant::kFaithful;
  // D3 / D4: faithful = 2024 slots chosen with !mastery or filled by use, one
  // swap per long rest; nwn = mastery classes master every proficient weapon;
  // Weapon Focus adds its weapon in both. off = no mastery.
  rules::RuleVariant weapon_mastery = rules::RuleVariant::kFaithful;

  // E3 — saves, skills and checks.
  // C1–C8: the three NWN saves as 2024 Con / Dex / Wis saves, 2024 spell DCs,
  // fixed DCs converted; faithful = no monster save proficiency, no automatic
  // 1 / 20, Aura of Protection; nwn = NWN's flat feat bonuses, natural 20s,
  // Spell Focus +1/+2/+3; off = NWN's 3e saves and DCs.
  rules::RuleVariant saves = rules::RuleVariant::kFaithful;
  // SK1–SK3: faithful = the 2024 skills (proficiency and Expertise instead of
  // ranks, !skills, 2024 names; scripts read the values scaled to the 3e
  // range); nwn / off = NWN's.
  rules::RuleVariant skills = rules::RuleVariant::kFaithful;
  // SK5: faithful = Stealth rolled once per hide, passive Perception unless
  // searching, no size / in-combat / Favored Enemy terms; nwn / off = NWN's.
  rules::RuleVariant perception = rules::RuleVariant::kFaithful;
  // SK5 A (with perception=faithful): hiding needs a Stealth total of 15 (the
  // 2024 Hide action); below it the creature is not hidden.
  bool hiding_needs_15 = true;
  // SK5 B: an enemy finds a hidden creature in front of it within 2 m with a
  // clear line between them (2024 hides you only out of sight).
  bool found_up_close = true;
  // SK5 C: a creature in combat searches (an active d20 Perception each 6 s)
  // instead of using passive Perception.
  bool alert_creatures_search = true;
  // SK10: faithful = spell attacks are PB + casting ability against full AC,
  // with advantage / disadvantage; nwn / off = NWN's touch attacks.
  rules::RuleVariant spell_attacks = rules::RuleVariant::kFaithful;
  // SK7: faithful = casting is not interrupted by damage, no Concentration
  // check while entangled, no defensive casting mode; nwn / off = NWN's.
  rules::RuleVariant concentration = rules::RuleVariant::kFaithful;
  // SK6: faithful = Knockdown is a 2024 Shove, Disarm an ordinary attack; nwn / off = NWN's.
  rules::RuleVariant knockdown = rules::RuleVariant::kFaithful;
  // SK9: faithful = d20 + Dex (+ PB with Improved Initiative = Alert),
  // advantage when invisible; nwn / off = NWN's.
  rules::RuleVariant initiative = rules::RuleVariant::kFaithful;
  // SK8: faithful = no parry mode, Taunt does nothing, stores keep their
  // prices, Use Magic Device lifts no item limits; nwn / off = NWN's.
  rules::RuleVariant nwn_only_skills = rules::RuleVariant::kFaithful;

  // E4 — the character model.
  // CM1: faithful = 2024 XP table, level cap 20, script SetXP read in NWN's
  // table; nwn / off = NWN's.
  rules::RuleVariant progression = rules::RuleVariant::kFaithful;
  // CM2: faithful = no multiclass XP penalty, a new class needs 13 in the
  // primary abilities, up to 8 classes (EP16); nwn / off = NWN's (3 classes).
  rules::RuleVariant multiclass = rules::RuleVariant::kFaithful;
  // CM14: faithful = alignment is descriptive (no class or item alignment
  // limits); nwn / off = NWN's.
  rules::RuleVariant alignment = rules::RuleVariant::kFaithful;
  // CM3: faithful = 2024 hit points (max die at level 1, then the fixed
  // average, 2024 dice); nwn / off = NWN's.
  rules::RuleVariant hit_points = rules::RuleVariant::kFaithful;
  // CM7: faithful = feats at 2024's class levels, general feats only at level
  // 1, Ability Score Improvement as a feat, no ability +1 every 4 levels;
  // nwn / off = NWN's (the ASI rows hidden).
  rules::RuleVariant feat_cadence = rules::RuleVariant::kFaithful;
  // CM4 / CM5: faithful = no racial ability adjustments, 27-point buy to 15,
  // NWN's level +1s not counted, !background and !asi; nwn / off = NWN's.
  rules::RuleVariant ability_scores = rules::RuleVariant::kFaithful;
  // CM9 / CM10: faithful = the 2024 class features, the SRD subclass at
  // class level 3, no prestige classes; nwn / off = NWN's.
  rules::RuleVariant class_features = rules::RuleVariant::kFaithful;
  // CM6: faithful = the 2024 species — their names (Half-Orc as Orc) and
  // texts, "Species" for Race; nwn / off = NWN's races.
  rules::RuleVariant species = rules::RuleVariant::kFaithful;
  // CM8: faithful = only the SRD 5.2 feats nwn5e has built can be chosen
  // (Alert, Ability Score Improvement, the Fighting Styles); nwn / off = NWN's.
  rules::RuleVariant feats = rules::RuleVariant::kFaithful;
  // C2: faithful = the 2024 starting kits as the creation screen's packages,
  // and the background's kit when a new character enters; nwn / off = NWN's.
  rules::RuleVariant starting_equipment = rules::RuleVariant::kFaithful;
  // Creature size: faithful = size limits no weapon or shield (two or more
  // sizes larger: two-handed), no 3e size modifiers to AC, attack or
  // unarmed damage, and the 2024 Heavy rule (Disadvantage below Str / Dex
  // 13); nwn / off = NWN's size rules.
  rules::RuleVariant size = rules::RuleVariant::kFaithful;
  // Finesse: faithful = a Finesse weapon (dagger, rapier, scimitar, short
  // sword...) uses the better of Str and Dex for attack and damage, no feat
  // needed; nwn / off = NWN's Weapon Finesse feat (Dex to attack).
  rules::RuleVariant finesse = rules::RuleVariant::kFaithful;
  // CM0: serve nwn5e's 2da rows and strings (data\rows, data\strings.txt).
  bool served_data = true;

  // [debug] — test switches, not rules.
  bool debug_every_hit_crits = false;    // every hit is a critical (crit immunity still applies)
  bool debug_combat_breakdown = false;   // the engine's own per-term attack and damage breakdown
  int debug_attacks_per_round = 0;       // >0: player characters make this many main-hand attacks
  int debug_creature_ac = 0;             // >0: every non-player creature has this AC (forces misses)
  int debug_creature_hp_multiplier = 0;  // >1: a creature's hit points x this when a character first attacks it
  int debug_creature_min_hp = 0;         // >0: ... and at least this many
  bool debug_attack_trace = false;       // log round setup and every PC attack roll
  bool debug_players_poisoned = false;   // SK11 test: player characters count as poisoned for ability checks
  bool debug_served_data_probe = false;  // CM0 test: a hidden probe row in feat.2da, checked after load
  bool debug_nui_test = false;           // NUI test: the test window opens when a player character arrives
  bool debug_client_nui_test = false;    // B1: nwn5e's client test window opens on the main menu
  bool debug_test_commands = false;      // !give <blueprint>, !feat add / remove: for testing
};

extern Config g_config;
void ConfigLoad(const wchar_t* directory);
