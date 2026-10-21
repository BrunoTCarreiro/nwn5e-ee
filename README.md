# nwn5e-ee

D&D 2024 rules (SRD 5.2) for **Neverwinter Nights: Enhanced Edition**
(64-bit, Windows, single player). This is the EE port of
[nwn5e](https://github.com/BrunoTCarreiro/nwn5e), which targets Diamond 1.69;
why and how it moves is in [docs/EE-RECON.md](docs/EE-RECON.md) (route B),
and the full plan is in [docs/PLAN.md](docs/PLAN.md).

`nwn5e.exe` starts EE's `bin\win32\nwmain.exe` with `nwn5e.dll` loaded; the
DLL replaces engine rule functions in memory. EE's executable exports its
engine functions by name, so the DLL finds them by name rather than by fixed
addresses, and logs any that a game update removes. Nothing in the game
folder is modified: starting EE from Steam is the unmodified game.

## Status

Route B was proven in game with a spike (0.0.2, 2026-10-20: a hook by
exported name made every hit threaten). The port follows
[docs/EE-PORT.md](docs/EE-PORT.md), step by step (E0–E4). Rules so far, each
switchable in `nwn5e.ini`:

| Rule | Setting | State |
|---|---|---|
| Critical threat only on a natural 20 | `crit_nat20_only` | verified in game (0.2.0, 2026-10-20: sheet threat 19–20 → 20; a natural 19 was a hit, natural 20s crits) |
| No critical confirmation roll; no "Threat Roll" in the combat log | `crit_no_confirmation` | verified in game (0.1.0 / 0.1.1, 2026-10-20: every crit "*critical hit*" with no Threat Roll, on rolls of 5, 7, 14) |
| x2 critical multiplier for every weapon | `crit_double_damage` | verified in game (0.1.1, 2026-10-20: d8 ×3 weapon — crits 15 and 19 = 2d8 + 3, were up to 33; the sheet's main-hand ×3 → ×2, which EE computes apart). The weapon's own description still shows its base multiplier (the client's data) |
| Crits roll the damage dice twice, flat bonuses (Strength, flat effect bonuses) once | `crit_dice_only` | verified in game for Strength (19 = 2 × 8 + 3; doubled Strength could reach 22); flat effect bonuses await a crit with such an effect |
| Creatures can crit your party at every difficulty (B7) | `crits_against_players` | verified in game (0.1.0, 2026-10-20: goblins' crits on the player at Easy stayed critical hits) |
| Attack rolls use the proficiency bonus instead of the 3e BAB; the sheet shows the same | `attack_proficiency` | verified in game (0.2.0, 2026-10-20: PC 3e +4 → +5 with PB 2, goblins −2 → +0; the sheet's attack line read "+6/+6/+6" with `attacks_per_round=3` — the bonus the rolls used, no −5 per attack) |
| Monsters use the 2024 expected AC for their CR; characters keep gear AC | `creature_ac_by_cr` | verified in game (0.2.0: goblin CR 1/8, AC 7 → 13) |
| PCs and henchmen attack per 2024 Extra Attack; no 3e −5 on any attack (iterative, off-hand, opportunity, Haste, Flurry) | `extra_attack` | no −5 verified in game (0.2.0: 3 attacks a round and attacks of opportunity all at the same bonus, with `attacks_per_round=3`); the class-based count awaits a level-5 character |
| Henchmen use the player-character rules (decision 11) | — (always) | built (0.2.0) |
| Advantage / disadvantage replace NWN's situational bonuses; ranged attacks with an enemy within 5 ft have disadvantage (B1–B6) | `advantage_disadvantage` | verified in game (0.2.0: flanking +2 removed, normal roll; a bow shot with a goblin adjacent had disadvantage, lower d20 kept; a goblin not seeing its target had disadvantage) |
| After a kill, the round's remaining attacks go to the nearest enemy; ranged only to one already in the fight, within 40 m (decision 14) | `retarget_remaining_attacks`, `ranged_retarget_range` | melee verified in game (0.2.0: 2 of 3 kills moved the round on; once the round's last two attacks were lost — watching); ranged awaits a test |
| No racial bonuses vs specific enemies (R1) | `racial_combat_bonuses` | built (0.2.0) |
| Two-weapon fighting: no penalties, one off-hand attack, off-hand damage without Strength (D1) | `two_weapon_fighting` | built (0.2.0); seen in game: longsword + battleaxe (no Light weapon) → no off-hand attack, the penalties answered 0 |
| Two-handed weapons add Strength once (E1) | `two_handed_strength` | built (0.2.0) |
| Opportunity attacks: only moving away provokes (no casting, potion, ranged, unarmed or Disarm AoOs), only from creatures you run away from; Spring Attack no longer protects (D2) | `opportunity_attacks` | built (0.3.0) |
| After drinking a potion or using Second Wind mid-fight, a player character goes back to attacking (owner request; EE drops the order too) | `resume_attack_after_potion` | Second Wind added in 0.5.3 (owner's test of 0.5.2: the target was lost); potions fixed in 0.4.4: 0.3.0-0.4.3 queued the attack at the drink and EE then dropped the drink (owner's test: no potion drunk mid-fight, and after the fight it sent the character at a nearby hostile); now the attack is given once the drink is over, only while still in combat, the fallback enemy one it can reach; verified in game (0.4.4, 2026-10-20: three potions drunk, two after a kill with no attack, one mid-fight whose target had died - no enemy in reach, no attack); going back to a target still standing awaits a test |
| Conditions (CN1–CN8): restrained, helpless targets (advantage; crits from within 5 ft), poisoned, frightened without fleeing, concealment as disadvantage, deafness without spell failure, no coup de grace | `conditions` | built (0.3.0) |
| Weapon mastery, who (D3): 2024 slots by class, `!mastery`, filled by use, one swap per long rest; Weapon Focus adds its weapon | `weapon_mastery` | built (0.3.0); a slot filled by use, with its chat message, seen in game (0.3.1) |
| Weapon mastery effects (D4): Vex, Sap, Graze, Slow, Topple, Cleave | `weapon_mastery` | built (0.3.0) |
| Push (D4, faithful only): a hit pushes a creature up to Large 10 ft straight away, stopped by walls and placeables, in 0.1 s (owner, 0.5.0; was 0.15 s); it falls and its round pauses; if another enemy is now closer, the attacker turns to it | `weapon_mastery` | verified in game (0.3.1, 2026-10-20, warhammer vs goblins: 3 m slides in the open, shorter at walls and a placeable, none against a wall; the fall held, no walking back; 10 turns to the closer goblin, all kept; an attack order the engine dropped mid-chase was reissued); Huge creatures (not pushed) await a test |
| 2024 saves (C1–C3): Fortitude / Reflex / Will = Con / Dex / Wis saves (ability + PB when proficient + magic; Aura of Protection); `!saves` lists all six; the sheet and the combat log name them Constitution / Dexterity / Wisdom | `saves` | built (0.4.0) |
| Save rolls (C4–C7): 2024 spell DCs (8 + PB + casting ability; scroll DCs), fixed DCs converted, no automatic natural 1 / 20, no feat "+2 vs X" save bonuses, species advantages and Halfling Luck, stunned / paralysed / asleep fail Str and Dex saves; a spell's own save ability (chat line) | `saves`, `conditions` | built (0.4.0) |
| 2024 skills (SK1–SK2, SK3 a): proficiency / Expertise instead of ranks, `!skills`, 2024 names; scripts read 2 x | `skills` | built (0.4.0) |
| Concentration (SK7): no interruption by damage, no check while entangled, no defensive casting mode | `concentration` | built (0.4.0) |
| 3e-only skills (SK8): no parry mode, Taunt does nothing, no Appraise prices, Use Magic Device lifts no item limits | `nwn_only_skills` | built (0.4.0) |
| Initiative (SK9): d20 + Dex (+ PB with Improved Initiative = Alert), advantage when invisible, Halfling Luck | `initiative` | built (0.4.0) |
| Spell attacks (SK10): PB + casting ability against full AC, advantage / disadvantage, a natural 20 crits without confirmation | `spell_attacks` | built (0.4.1) |
| Skill DCs (SK3) and conditions on checks (SK11): NWN's fixed DCs converted to the 2024 scale (locks, traps, Search, scripts' checks, Tumble, Spellcraft, heal kits, Pick Pocket, Animal Empathy; the combat log shows the 2024 skill and DC); no trap synergy, no Rogue-only traps; poisoned / frightened checks have disadvantage, blinded / deafened ones fail what needs that sense | `skills`, `conditions` | built (0.4.2) |
| Stealth and Perception (SK5): Stealth rolled once per hide (15 to hide), passive Perception unless searching (creatures in combat search), no size / in-combat / Favored Enemy terms, found up close (2 m in front, clear line); hidden = advantage on initiative | `perception`, `hiding_needs_15`, `found_up_close`, `alert_creatures_search` | built (0.4.3) |
| Served data and strings (CM0): nwn5e's 2da rows merged into the game's own tables at start-up (build\served, priority 15M: below modules and haks), its strings served by the string-table hook | `served_data` | built (0.5.0); probe OK at launch |
| Levels and XP (CM1): 2024 XP table, level cap 20, scripts' SetXP read in NWN's table | `progression` | built (0.5.0) |
| Multiclassing (CM2): no XP penalty, 13s in the primary abilities, up to 8 classes (EP16), no prestige classes with class_features | `multiclass` | built (0.5.0); the 4th class awaits the in-game test |
| Alignment (CM14): no class or item alignment limits | `alignment` | built (0.5.0) |
| Hit points (CM3): max die at level 1, then the average, 2024 dice | `hit_points` | built (0.5.0) |
| Feat cadence (CM7): feats at 2024 levels, general feat at level 1 only, ASI rows, no ability page every 4 levels (EP15 b: three patches, all or none) | `feat_cadence` | built (0.5.0) |
| Ability scores and backgrounds (CM4, CM5, CM12, CM13): no racial ability adjustments; point buy 27 to 15 (2 points from 13), Recommended scores legal; NWN's +1 every 4 levels taken back; !background (Acolyte, Criminal, Sage, Soldier: +2/+1 or +1/+1/+1, two skills, Origin feat); !asi; henchmen pick their own; legacy characters get their 2024 ASIs | `ability_scores` | built (0.5.1) |
| Character creation, Background step (CM5, docs/CHARGEN.md): the creation hub's Alignment step is a real Background page — the four SRD backgrounds, then the ability split, with Recommended; alignment set to neutral | `ability_scores` | built (0.6.0) |
| Class features frame (CM9, CM10): `!features` lists your 2024 class features and what nwn5e does with each; feats a class level gives (Second Wind) are added by nwn5e, also to characters made earlier, and taken back in NWN mode | `class_features` | built (0.5.2) |
| Fighter (F0–F7, C1–C5): Fighting Styles (Archery +2 ranged, Defense +1 AC in armour, Great Weapon Fighting 1–2 count as 3, Two-Weapon Fighting); Second Wind 1d10 + level (2 / 3 / 4 uses); Champion crits on 19–20 (18–20 from 15); Remarkable Athlete (initiative advantage); Tactical Shift; Indomitable; Studied Attacks; Heroic Rally | `class_features` | built (0.5.2); Second Wind verified in game (0.5.2, 2026-10-21: granted at level 1, on the radial once listed in cls_feat_fight with OnMenu 1) |
| Shove (SK6): Knockdown makes no attack roll and no damage; the target's better Str / Dex save vs 8 + Str + PB, at most one size larger; Disarm is an ordinary attack | `knockdown` | built (0.4.1) |

Test switches (`[debug]`, off by default): `every_hit_crits=1` makes every
hit a critical; `combat_breakdown=1` turns on the engine's own per-term
attack and damage breakdown; `attacks_per_round=N` gives player characters N
main-hand attacks; `creature_ac=N` gives every monster AC N;
`creature_hp_multiplier=N` / `creature_min_hp=N` give a monster N times / at
least N hit points when your party first attacks it; `attack_trace=1` logs
every PC attack roll with its round's attack counts; `nui_test=1` opens
nwn5e's NUI test window when your character arrives (`!nui` opens it any
time: nwn5e's own windows, the base for the Fighting Style picker);
`client_nui_test=1` opens nwn5e's client window test on the main menu (EE's
own NUI with no game loaded: the base of nwn5e's creation and level-up
steps).

What is carried over from the 1.69 build, unchanged:

| Folder | Holds |
|---|---|
| `src/rules`, `src/data` | the 2024 rule math and the 2da reader/writer (no engine code) |
| `tests/` | their unit tests (`test.bat`) |
| `data/`, `scripts/` | served 2da rows, strings and NWScript sources |
| `docs/PLAN.md` | the plan and every decision so far |

The engine side (`src/core`) is rebuilt for EE one rule at a time.

## Build and run

Needs Visual Studio 2019 Build Tools (C++, x64) and NWN: EE from Steam.

    build.bat        rem build\nwn5e.exe + build\nwn5e.dll
    test.bat         rem the rule unit tests, no game needed
    build\nwn5e.exe  rem starts EE with the DLL; keep Steam running

The game folder is the Steam default unless `build\nwn5e.ini` sets
`[loader] game_dir`. The DLL writes `build\nwn5e.log`.

## Legal

The ruleset is the System Reference Document 5.2 (CC-BY-4.0) only. No game
files, Player's Handbook text or other non-SRD material are in this
repository ([docs/PRIVATE-LAYER.md](docs/PRIVATE-LAYER.md)). Neverwinter
Nights is a trademark of its owners; this project is unofficial.
