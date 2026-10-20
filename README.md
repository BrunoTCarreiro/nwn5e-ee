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

Test switches (`[debug]`, off by default): `every_hit_crits=1` makes every
hit a critical; `combat_breakdown=1` turns on the engine's own per-term
attack and damage breakdown; `attacks_per_round=N` gives player characters N
main-hand attacks; `creature_ac=N` gives every monster AC N;
`creature_hp_multiplier=N` / `creature_min_hp=N` give a monster N times / at
least N hit points when your party first attacks it; `attack_trace=1` logs
every PC attack roll with its round's attack counts.

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
