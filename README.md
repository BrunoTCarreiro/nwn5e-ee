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

**Spike (0.0.2) — route B verified in game (2026-10-20).** The DLL hooks
`CNWSCreatureStats::GetCriticalHitRoll` by its exported name so every hit
threatens a critical hit, and logs each attack roll. In The Prelude, all 9
hits logged a threat (rolls of 3 to 20, with a weapon that threatens on 20
only); 6 of the player's 8 became critical hits after the confirmation roll.
No 2024 rules yet.

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
