# The private layer

The public ruleset is **SRD 5.2 only** (CC-BY-4.0). Player's Handbook (and
other non-SRD) material is personal use: the owner adds and tests it,
reworded, in a private layer that never reaches this repo. It moves here only
if that material is released under an open license (OGL / CC).

## Where it lives

`private/` at the repo root — listed in `.gitignore`, and its own git
repository (local, or a private remote). Layout:

| Folder | Holds | Built by |
|---|---|---|
| `private/src/` | hooks and installers (engine code, like `src/core`) | `build.bat` |
| `private/rules/` | rule math (no engine code, like `src/rules`) | `build.bat`, `test.bat` |
| `private/tests/` | unit tests (`TEST(...)` from `tests/check.h`) | `test.bat` |
| `private/docs/` | briefs and decisions for private material | — |
| `private/data/` | served rows (`rows/<table>.txt`) and strings (`strings.txt`), merged after the public ones (data/README.md) | `build.bat` copies it (not with `public`) |

Name every private source file `private_*.cpp`: the build puts all objects in
one folder, so a private `core.cpp` would overwrite the public one.

## How it is built

- `build.bat` compiles `private/src/*.cpp` and `private/rules/*.cpp` when
  they exist and defines `NWN5E_PRIVATE`; `dllmain.cpp` then calls
  `InstallPrivateRules()` (defined in `private/src`) after the public
  installers, and the log says `private layer: compiled in`.
- `build.bat public` leaves the layer out. **Build any DLL that leaves this
  machine that way**; a DLL built with the layer is personal use like its
  source.
- `test.bat` adds `private/rules` and `private/tests` when they exist.

## Rules for the boundary

- Public code holds mechanisms, never private content: no PHB names, numbers
  or text in `src/`, `docs/`, `tests/`, commit messages or the public
  `nwn5e.ini`. Where private code needs a hook, the public side gets a
  generic one (a registration call, a data folder) that names nothing.
- Private code may include any public header; public code includes nothing
  from `private/`.
- Private settings go in `build\nwn5e.ini` only (git-ignored), read by
  private code; the public `nwn5e.ini` template doesn't list them.
- Logs and screenshots from a private build stay local (they are already
  ignored here).
- Private material keeps the two modes: each rule says what it does in
  faithful and in NWN mode, like the public ones.
