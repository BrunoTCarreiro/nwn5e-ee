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
  bool hiding_needs_15 = false;  // off since 0.28.0 (owner: "hiding should succeed by default")
  // SK5 B: an enemy finds a hidden creature in front of it within 2 m with a
  // clear line between them (2024 hides you only out of sight).
  bool found_up_close = true;
  // SK5 C: a creature in combat searches (an active d20 Perception each 6 s)
  // instead of using passive Perception.
  bool alert_creatures_search = true;
  // SK5 D (owner, 2026-10-25: "hiding should succeed by default, enemy
  // investigation rolls should test it"): a creature that isn't searching
  // can't see a hider (it can hear one, and comes to look - stealth_watch.cpp);
  // only searching ones (investigating, detect mode, alert in combat) roll
  // Perception to find it. Up close (B) still finds it.
  bool hiders_need_search = true;
  // SK5 E (owner, 2026-10-25: vision cones, "Go with option 2"): with
  // hiders_need_search, a calm creature does see a hider inside its vision cone
  // (shown as a filled cone while hiding - stealth_watch.cpp; 60 / 90 / 120 / 150 models),
  // this many degrees centred on its facing (its passive Perception against the
  // Stealth total); outside, not at all; searching ones see all round. 0 = calm
  // creatures see no hider (D as it was); 360 = all round.
  int vision_cone = 90;  // owner, 2026-10-25: "Vision cones should be 90 degrees"
  // SK5 F (owner, 2026-10-25: "once discovered the PC leaves stealth mode"): a
  // hostile that sees a hiding player character takes it out of stealth mode.
  bool found_ends_hiding = true;
  // SK5 G (owner, 2026-10-25: environmental light - "do the sensible
  // distances"): the light where a hiding player character stands, the 2024
  // way - dim -5 to be seen, dark unseen (darkvision within 18 m: dim). Bright:
  // within 4 m of a lit placeable, or the inner half of a light effect, or
  // carrying a light, or a bright area; dim: within 9 m of one, or an area of
  // moderate light; else dark (stealth_watch.cpp). Replaces EE's night terms.
  bool stealth_light = true;
  // With stealth_light (owner, 2026-10-25: "change the stealth vfx, make it
  // dependent on the light level, 90% visible on light, 70% in dim and 50% in
  // dark" - "do not make the ghostly effect pulse"): a hiding player character
  // is see-through by its light, steadily (stealth_ghost.cpp).
  bool stealth_ghost = true;
  // With stealth_ghost (owner, 2026-10-25: "there might be an issue with the
  // transparency levels where the player might not see their own character in
  // high levels of stealth. Is it possible to add any kind of outline to
  // characters?" - "go with A"): a hiding character is outlined in EE's own
  // highlight glow (stealth_ghost.cpp).
  bool stealth_outline = true;
  // Henchmen and stealth (owner, 2026-10-25: "If they're following the PC they
  // should also enter stealth mode. If they have been asked to stand their
  // ground they should not"): while the player character hides its henchmen do
  // too, unless set to Stand your ground; they leave when it stops
  // (stealth_watch.cpp). Both rule modes.
  bool henchmen_follow_stealth = true;
  // Light sources (owner, 2026-10-25: "putting off a light source is a good
  // one"): a lit brazier, lamp or torch bracket can be put out - used, attacked,
  // or hit by a cold / water / wind spell - and lit again by using it; hostiles
  // that see it go out come to look (light_sources.cpp).
  bool light_sources = true;
  // Darker maps (owner: "the maps ... seem to be a bit too bright right now"):
  // the areas' sun and moon light at map_light % of the module's (100 = its
  // own; !maplight sets it in game), and with map_tile_dim each tile's main
  // lights one step darker (light_sources.cpp).
  // With light_sources: a tile's fill (main) lights go off when every light on
  // it and the 8 tiles round it is out (owner: "let's try B").
  bool fill_follows_lights = true;
  // A held torch's flame rises whatever the torch's angle (owner: "a torch held
  // sideways will spew flames sideways as well"; light_sources.cpp).
  bool torch_flame_up = true;
  int torch_flame_life_ms = 500;  // its particles' life (the game's: 2000)
  int torch_flame_rise = 14;      // how fast they rise: mass -rise / 100
  int map_light = 70;
  bool map_tile_dim = true;
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
  // Carrying (owner, 2026-10-26: "use the 2024 encumberance"): faithful = the
  // 2024 carrying capacity - Strength x 15 lb. with no penalty (NWN slows from
  // about Str x 9.5 and stops the run), more overloaded: Speed about 5 feet
  // (data/rows/encumbrance.if_carry2024.txt, ruleset.if_carry2024.txt);
  // nwn / off = NWN's encumbrance table.
  rules::RuleVariant carrying = rules::RuleVariant::kFaithful;
  // Armour (owner, 2026-10-27: "make sure the NWN armor AC matches 2024"):
  // faithful = NWN's armour tiers read as the 2024 armour they name (padded ...
  // plate) - the 2024 AC and Dexterity cap for players and henchmen
  // (rules_armor.cpp); nwn / off = NWN's armour AC.
  rules::RuleVariant armor = rules::RuleVariant::kFaithful;
  // The quickbar (owner, 2026-10-27: "do the 1-9 hook plus auto-placing new
  // actions"): faithful = keys 1-9 press quickbar slots 1-9 as F1-F9 do, and
  // nwn5e's class actions (Rage, Brutal Strike's options, Second Wind, the
  // Shoves ...) go on empty slots when a character gains them (rules_quickbar.cpp);
  // nwn / off = NWN's quickbar (a new feature: faithful only).
  rules::RuleVariant quickbar = rules::RuleVariant::kFaithful;
  // Finesse: faithful = a Finesse weapon (dagger, rapier, scimitar, short
  // sword...) uses the better of Str and Dex for attack and damage, no feat
  // needed; nwn / off = NWN's Weapon Finesse feat (Dex to attack).
  rules::RuleVariant finesse = rules::RuleVariant::kFaithful;
  // Phase 5a: faithful = Hit Dice, a Short Rest (HUD button, !shortrest)
  // spending them out of combat, NWN's Rest as the Long Rest (all or nothing,
  // the Hit Dice back); nwn / off = NWN's rest.
  rules::RuleVariant rests = rules::RuleVariant::kFaithful;
  // Rests move the clock: a Long Rest 8 hours, a Short Rest 1 (to test; the
  // owner keeps it still for now).
  bool rest_clock = false;
  // Phase 5b: faithful = a player character with a henchman standing makes
  // death saves at 0 HP (else the module's dying: the campaigns' instant
  // death), henchmen lie at 1 HP until healed - a spell, a kit, a Short Rest
  // (SoU's handling), the temple summons them, Exhaustion from coming back from 0 HP;
  // nwn / off = NWN's (the module's scripts).
  rules::RuleVariant dying = rules::RuleVariant::kFaithful;
  // What a down player looks like (0.20.0): visualeffects.2da rows, 0 none -
  // an aura while making death saves (512 VFX_DUR_AURA_PULSE_RED_WHITE) and
  // while Stable (513 ..._BLUE_WHITE), a flash per roll (199
  // VFX_IMP_HEAD_HOLY a success, 203 VFX_IMP_HEAD_EVIL a failure, 217
  // VFX_IMP_DEATH_L a natural 1, 68 VFX_IMP_HEALING_M a natural 20); and the
  // death-save marker over it (success bars left, failure bars right).
  int death_save_aura_vfx = 512;
  int death_save_stable_vfx = 513;
  int death_save_success_vfx = 199;
  int death_save_failure_vfx = 203;
  int death_save_nat1_vfx = 217;
  int death_save_nat20_vfx = 68;
  bool death_save_marker = true;
  // The exhausted look (owner, 2026-10-24): Exhaustion 1-2 the tired idle
  // (pausetrd), 3-5 also the injured walk (walkinj) - EE's
  // ReplaceObjectAnimation on the player character.
  bool exhaustion_look = true;
  // A wounded player character bleeds (owner, 2026-10-24: "just do 1 bleeding
  // effect, mild, with a small puddle when the PC is under 10%"; no dripping
  // since 0.22.8): a small puddle of blood under bleed_percent of its hit
  // points (visualeffects 1570, low_hp_look.cpp).
  bool low_hp_bleed = true;
  int bleed_percent = 10;
  // Animation recipes (anim_packs.cpp): new animations built at
  // startup from the game's own, served as packs the body skeletons point at.
  bool animation_recipes = true;
  // A hiding creature crouches (owner, 2026-10-25: a crouch walk - "go, use
  // stealth mode as the trigger"): in stealth mode the idle and the walk are
  // nwn5e's n5_cridle / n5_crwalk recipes (stance_look.cpp). Needs
  // animation_recipes.
  bool stealth_crouch = true;
  // A walk-limited character (stealth / detect mode, heavy load) walks
  // backwards with the keyboard (drive_fix.cpp): the engine's AddDriveAction
  // dropped the backward key for it, so the server never stepped it back while
  // its client did - the position pulled back, a stutter (the unmodded game too).
  bool drive_backward_fix = true;
  // A held weapon's walk / run arm layer left playing over an idle (its stop
  // lost, e.g. turning while backing up) is stopped (anim_layers.cpp).
  bool stuck_layer_fix = true;
  // Another creature's radial menu leaves out your self-only class feats
  // (Rage, Reckless Attack ...) instead of greying them (radial_class_feats.cpp).
  bool radial_hide_self_feats = true;
  // The VFX Bench (nwn5e-tools/vfx_bench): the game's effects exported for it once,
  // its Play in game / Stop requests played (the dev layer's vfx_editor.cpp; no effect without it).
  bool vfx_bench = true;
  // Who has noticed a hiding player character (owner, 2026-10-25: "build your
  // recommendation" for showing enemies' sight): an eye over each hostile that
  // sees it, an ear over one that only hears it (stealth_watch.cpp).
  bool stealth_marker = true;
  // The range rings (owner, 2026-10-25: "next up we'll add the range
  // indicators"): while a player character hides, the nearest stealth_rings
  // hostiles get a ring at how far they see and one at how far they hear
  // (stealth_watch.cpp). 0: none.
  int stealth_rings = 6;
  // 2024 rules (perception=faithful; owner: "for the hearing detection, do
  // option 2"): a hostile that hears a hiding player character but doesn't see
  // it walks over to where the sound came from and searches (stealth_watch.cpp).
  bool heard_investigates = true;
  // The Short Rest's visual on each resting character (owner, 2026-10-23: "I
  // like the look the health potion effect has"; then "Use
  // VFX_IMP_RESTORATION_LESSER, it looks like a good placeholder"): a
  // visualeffects.2da row (221 VFX_IMP_RESTORATION_LESSER; 69 the Cure Light
  // Wounds potion's; 0 none) at a scale in percent.
  int short_rest_vfx = 221;
  int short_rest_vfx_scale = 100;
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
const wchar_t* ConfigPath();  // nwn5e.ini's full path (for a setting changed in game)
