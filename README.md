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
| Critical threat only on a natural 20 | `crit_nat20_only` | built (0.1.0) |
| No critical confirmation roll; no "Threat Roll" in the combat log | `crit_no_confirmation` | built (0.1.0) |
| x2 critical multiplier for every weapon | `crit_double_damage` | built (0.1.0) |
| Crits roll the damage dice twice, flat bonuses (Strength, flat effect bonuses) once | `crit_dice_only` | built (0.1.0) |
| Creatures can crit your party at every difficulty (B7) | `crits_against_players` | built (0.1.0) |

Test switches (`[debug]`, off by default): `every_hit_crits=1` makes every
hit a critical; `combat_breakdown=1` turns on the engine's own per-term
attack and damage breakdown.

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
