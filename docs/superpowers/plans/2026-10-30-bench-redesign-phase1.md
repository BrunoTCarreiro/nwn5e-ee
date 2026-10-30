# Bench redesign, phase 1: the split and the shared core - Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Take the dev-only parts out of the shipped nwn5e DLL into a dev layer that lives in nwn5e-tools, and add the "assembled" recipe kind. The game assembles it at startup with C++ that a helper program shares with the tools.

**Architecture:**
- **The dev layer.** It mirrors the private layer: `build.bat` compiles `nwn5e-tools\dev\*.cpp` into the DLL when that checkout is present and the build isn't `public`. `dllmain.cpp` calls `InstallDev()` under `#ifdef NWN5E_DEV`. Shipped code reaches the moved `!animtrace` only through a null-checked function-pointer seam.
- **The assembled recipe.** It extends the existing recipe text format. The capture is stored as per-bone turns relative to a named stance, plus root offsets from the stance's home. A pure `src/data` function composes it onto the stance and adds the blends and the loop.
- **The helper program.** It is built in nwn5e-tools from those same `src/data` files, plus an EE reader for the game archives. It extracts the body libraries for the tools, which replaces the DLL's export, and it assembles recipes.

**Tech Stack:** C++17 (VS2019 BuildTools `cl`, `/MT`), nwn5e's `tests/check.h` test framework, Node 18 (nwn5e-tools, no npm installs), Python 3 for one check script.

**Spec:** `docs/BENCH-REDESIGN.md` (approved 2026-10-30): "Where things live", "Shipping: the assembled recipe", "Build order" phase 1. Issues: epic #86, this phase #87, the tools' move #85.

## Global Constraints

- **Repos.** Two sibling checkouts: `C:\Users\Bruno\Desktop\Projects\nwn5e-ee` (public, the mod) and `C:\Users\Bruno\Desktop\Projects\nwn5e-tools` (private, the dev tools). Dev-only code goes in nwn5e-tools; the shipped DLL keeps only what players need.
- **The tools' location.** `build.bat` finds them at `%NWN5E_TOOLS%`, defaulting to `%~dp0..\nwn5e-tools`.
- **Shipped build.** `build.bat public` leaves out the private layer and the dev layer.
- **Build flags.** Keep build.bat's (`/O2 /MT /W3 /EHsc /std:c++17 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE`). No new third-party code.
- **build.bat's source list.** Every new `src\data` file is added to build.bat's explicit list (line 43) by hand. test.bat globs `src\data\*.cpp` and `tests\data\*.cpp`.
- **The game may be running.** Never build into `build\` while `nwmain.exe` runs: `set NWN5E_OUT=%TEMP%\nwn5e_out` first. Install into `build\` only when the owner says the game is closed. After any launch check of your own, `taskkill //F //IM nwmain.exe` and confirm with tasklist.
- **Shared checkout.** Another agent commits game work in nwn5e-ee:
  - `git status` before every commit, staging by explicit path, never `git add -A`;
  - never stage or commit files you didn't change;
  - one git command per call;
  - the owner pushes.
- **Commits.** Commit messages name `#87` and end with `Claude-Session: https://claude.ai/code/session_01B9aC6onRCyqDh7qEdmA7n9`.
- **Never open `nwn5e-ee\private\`.** If the build fails inside `private\` because a moved symbol is gone, stop and report it; it isn't yours to edit.
- **The recipe formats.** Keep the formula and keys formats as they are; the game still reads both. The assembled kind adds keys, and nothing else changes.
- **Key tolerances.** Key reduction is 0.75° / 4 mm, as today. The acceptance comparison allows 1.5° / 8 mm, since two reductions can stack. Clips run at 30 frames a second.
- **The eight bodies:** a_ba, a_fa, a_da, a_dfa, a_ba2, a_fa2, a_da2, a_dfa2. Their library chains give 10 compiled libraries: those eight plus a_ba_non_combat and a_ba_med_weap. a_ba_custom is ASCII, so it isn't compiled and isn't used.
- **The game install.** `X:\SteamLibrary\steamapps\common\Neverwinter Nights`, unless `nwn5e.ini` `[loader] game_dir` says otherwise.
  - Keys: `data\nwn_retail.key` (wins) then `data\nwn_base.key`, both "KEY V1", with BIFs "BIFFV1".
  - The engine also reads `%USERPROFILE%\Documents\Neverwinter Nights\override\` first.
- **Checks.** Every new test gets deliberate breaks, each one caught (CLAUDE.md "Checks before built").
- **Version.** One version bump for the phase (Task 11): `kVersion` in `src\core\dllmain.cpp`, the next minor after whatever is current at commit time (0.58.0 if still 0.57.x).
- **Git ignores.** Generated files never get committed: `nwn5e-tools\anim_helper\bin\` and `nwn5e-tools\anim_helper\cache\` go in nwn5e-tools' `.gitignore`.

## Review Focus

1. **The game loads a body model from somewhere the install reader doesn't look** (the user's Documents override, or a hak). The tools would then show a different skeleton from the game's. Task 3 reads the Documents override first, as the engine does, and its byte-identity step against the last export pins it.
2. **The helper isn't built, or the game folder is wrong.** The old bench must say exactly that, not show an empty body list (Task 4 step).
3. **An assembled recipe's stance doesn't exist on some body's library chain.** The game must skip that body with a log line, keep the other bodies, and not crash (Task 8 test, Task 9 log).
4. **An assembled recipe whose length runs past its last key.** The last key holds (Task 8 test).
5. **A take fitted to a length** (n5_shove's 2.6 s → 1 s). The converter must scale the blend lengths into output time, per segment of the warp, or the blends come out 2.6× too short (Task 10 test with a fitted take).

---

### Task 1: Read EE's game archives

**Files:**
- Modify: `src/data/game_resources.h`, `src/data/game_resources.cpp`
- Test: `tests/data/game_resources_test.cpp` (create, or extend if it exists)

**Interfaces:**
- Produces:
  - `data::GameResources(const std::wstring& game_directory, const std::vector<std::string>& key_files, const std::vector<std::wstring>& override_directories)`, where key files are listed winning-first.
  - `static std::vector<std::string> data::GameResources::EeKeyFiles()` returns `{"data/nwn_retail.key", "data/nwn_base.key"}`.
  - `constexpr uint16_t data::kResourceTypeMdl = 2002;`
  - The existing one-argument constructor keeps its 1.69 behaviour.

- [ ] **Step 1: Write the failing tests.** Each test writes a synthetic KEY V1 and BIFF V1 pair into a fresh `%TEMP%` folder: 22-byte key entries (resref[16], type u16, id u32, with the bif index in `id >> 20`), and 16-byte BIF entries.
  - `TEST(GameResourcesReadsAnEeKey)`: one key `data/a.key` listing `bodya.mdl` (type 2002) in `data/a.bif`, with bytes `"MDLBYTES"`. `Read("bodya", 2002, &b, &src)` is true, `b == "MDLBYTES"`, and `src == "a.key"`.
  - `TEST(GameResourcesFirstKeyWins)`: two keys both listing `bodya.mdl` with different bytes. The first key in the list wins.
  - `TEST(GameResourcesOverrideFolderWins)`: a loose `bodya.mdl` in an override directory beats both keys, and `src == "override"`.
  - `TEST(GameResourcesMissingIsFalse)`: an unknown resref gives false.
- [ ] **Step 2: Run `test.bat` and verify that these four fail.** Expected: the build fails because the constructor is unknown.
- [ ] **Step 3: Implement the new constructor in `game_resources.cpp`.** It stores the override directories and the key list. Lookup order: override directories in order, then the keys in order. The 1.69 constructor delegates to it with its old list and `<game>\override`.
- [ ] **Step 4: Run `test.bat`.** Expected: all pass, including the existing tests.
- [ ] **Step 5: Deliberate breaks.** Each must turn a test red:
  - reverse the key order;
  - skip the override directories;
  - read the bif index as `id >> 16`.

  Restore the code after each.
- [ ] **Step 6: Commit** `feat: game resources read EE's keys and the user's override folder (#87)`, staging `src/data/game_resources.h src/data/game_resources.cpp tests/data/game_resources_test.cpp`.

### Task 2: One supermodel-chain search, shared

**Files:**
- Modify: `src/data/anim_recipe.h`, `src/data/anim_recipe.cpp`, `src/core/anim_packs.cpp:154-200` (`g_libraries`, `LoadLibrary`, `FindClip`)
- Test: `tests/data/anim_recipe_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  struct LoadedAnimLibrary { std::string bytes; AnimLibrary library; };
  using AnimLibraryLookup = std::function<const LoadedAnimLibrary*(const std::string& lower_name)>;
  // The clip as `library` plays it: its own, else its supermodel chain's (depth 8). False if none.
  bool FindAnimClip(const AnimLibraryLookup& lookup, const std::string& library,
                    const std::string& name, AnimClip* clip);
  ```
- `anim_packs.cpp`'s `FindClip` becomes a one-line call to `data::FindAnimClip`, with a lookup over its `g_libraries`. Rename anim_packs' `LoadLibrary` to `LoadAnimLibrary`, because `LoadLibrary` is a `windows.h` macro.

- [ ] **Step 1: Write the failing tests.** Use the compiled-library fixtures that `AnimRecipeReadsACompiledLibrary` builds.
  - `TEST(AnimRecipeFindsAClipUpTheChain)`: library `child` (supermodel `parent`) has no clip `wave`, and `parent` has it. `FindAnimClip(lookup, "child", "wave", &c)` is true, with `c.name == "wave"`.
  - `TEST(AnimRecipeChainStopsAtDepth8)`: a 10-library cycle-free chain whose clip is only in the 10th gives false.
  - `TEST(AnimRecipeChainMissingLibrary)`: the lookup returns null for the supermodel, so the result is false.
- [ ] **Step 2: Run `test.bat`; verify the new tests fail** (undefined `FindAnimClip`).
- [ ] **Step 3: Implement `FindAnimClip`.** Move anim_packs' chain walk into it unchanged, and point anim_packs at it.
- [ ] **Step 4: Run `test.bat` and `build.bat` (with `NWN5E_OUT` if the game runs).** Expected: tests pass, and the DLL builds.
- [ ] **Step 5: Deliberate breaks:** depth 7 instead of 8; not following the supermodel. Each one is caught.
- [ ] **Step 6: Commit** `refactor: one supermodel-chain clip search in src/data (#87)`.

### Task 3: The helper program - extract the libraries, print the stamp

**Files (nwn5e-ee):**
- Create: `core_stamp.bat`. It prints the SHA-256 (`certutil -hashfile`) of these files concatenated in this order: `src\data\anim_recipe.h src\data\anim_recipe.cpp src\data\anim_assemble.h src\data\anim_assemble.cpp`. Any file that doesn't exist yet is skipped.
- Modify: `build.bat`. Call `core_stamp.bat`, pass `/DNWN5E_CORE_STAMP=\"<hash>\"` to the DLL compile, and write `%OUT%\core_stamp.txt`.

**Files (nwn5e-tools):**
- Create:
  - `anim_helper/main.cpp`;
  - `anim_helper/build_helper.bat`. It uses the same `vcvars64` and flags as nwn5e's build.bat, compiles `main.cpp` with `..\..\nwn5e-ee\src\data\{anim_recipe,game_resources,anim_assemble}.cpp` (anim_assemble once Task 8 exists; the bat skips missing files), takes the stamp from `..\..\nwn5e-ee\core_stamp.bat`, and writes `anim_helper\bin\nwn5e_anim_helper.exe`;
  - `anim_helper/README.md`;
  - `.gitignore` lines for `anim_helper/bin/` and `anim_helper/cache/`.

**Interfaces:**
- Produces the command lines that later tasks and the bench call:
  - `nwn5e_anim_helper stamp` prints the core stamp on one line.
  - `nwn5e_anim_helper extract-libraries <game_dir> <out_dir>`:
    - writes `<out_dir>\<library>.mdl` (raw bytes) for each compiled library reached from the eight bodies through their supermodel chains;
    - prints one line per library, `<name>\t<bytes>\t<source>`;
    - exits 1, with a message on stderr, if the game folder has no `data\nwn_base.key`.
  - The override directory is `%USERPROFILE%\Documents\Neverwinter Nights\override` when it exists.

- [ ] **Step 1: Build the helper** (`anim_helper\build_helper.bat`). Expected: the exe exists.
- [ ] **Step 2: Run `nwn5e_anim_helper stamp` and compare it with the DLL build's `core_stamp.txt`.** Expected: the same hash, from one build of each.
- [ ] **Step 3: Run `extract-libraries X:\SteamLibrary\steamapps\common\Neverwinter Nights anim_helper\cache\libraries`.** Expected: the 10 libraries listed in Global Constraints.
- [ ] **Step 4: Byte-identity against the last export.** `fc /b` each extracted file against `nwn5e-ee\build\animedit\libraries\<name>.mdl`, which the DLL's export left there from the last game run. Expected: all 10 identical. If one differs, the source column says where it came from; resolve it before going on (Review Focus 1).
- [ ] **Step 5: Deliberate break.** Point the override list at an empty folder that holds a copy of `a_ba.mdl` with one byte changed. Step 4 must then report a_ba as different. Restore.
- [ ] **Step 6: Commit nwn5e-ee** (`core_stamp.bat`, `build.bat`): `feat: a core stamp for the recipe core, written beside the DLL (#87)`.
- [ ] **Step 7: Commit nwn5e-tools** (`anim_helper/*`, `.gitignore`): `feat: nwn5e_anim_helper - extract the body libraries from the install, print the core stamp (#87)`.

### Task 4: The old recipe bench reads the libraries through the helper

**Files (nwn5e-tools):**
- Modify:
  - `recipe_bench/server.js`. On start, and on `/api/state` when the cache is missing, it runs `extract-libraries <game_dir> anim_helper\cache\libraries` and serves `/api/library/<name>` from that cache, not from `build\animedit\libraries`.
  - `recipe_bench/README.md`;
  - `recipe_bench/mocap_check.js` and `recipe_bench/check.js`: their usage comments name the cache folder.
- The game folder comes from `--game`, else `NWN_GAME_DIR`, else `[loader] game_dir` in `<build>\nwn5e.ini`, else the Steam default.

**Interfaces:**
- Consumes: Task 3's `extract-libraries`.
- `/api/state` keeps its shape, with `librariesFolder` pointing at the cache. It adds `gameDir`, and `libraryError` (a sentence or null).

- [ ] **Step 1: Bad helper path.** Start the server with `--helper C:\nope.exe`; GET `/api/state`. Expected: `libraryError` says the helper isn't built and names `anim_helper\build_helper.bat`, and `libraries` is `[]` (Review Focus 2).
- [ ] **Step 2: Bad game folder.** Start with `--game C:\nope`. Expected: `libraryError` names the folder and `data\nwn_base.key`.
- [ ] **Step 3: A normal start.** Expected: 10 libraries, and `/api/library/a_ba` serves the same bytes as the cache file.
- [ ] **Step 4: Run `node recipe_bench/mocap_check.js anim_helper/cache/libraries`.** Expected: `all ok`, 30 ok lines.
- [ ] **Step 5: Commit** `feat: the recipe bench reads the body libraries from the install through the helper (#87)`.

### Task 5: The dev layer

**Files (nwn5e-ee):**
- Create: `src/core/dev_seam.h`, `src/core/dev_seam.cpp` (add the .cpp to build.bat's `src\core` list).
- Modify:
  - `src/core/anim_layers.cpp` (:45, :155, :221), `src/core/drive_fix.cpp` (:41, :58), `src/core/stealth_ghost.cpp` (:69, :328): replace the direct `AnimTrace*` calls with the seam's.
  - `src/core/dllmain.cpp`: drop `InstallAnimTrace`, `InstallAnimBrowser` and `InstallVfxEditor` (and their declarations), and add `#ifdef NWN5E_DEV` `InstallDev();` where `InstallAnimEditor` was (:170).
  - `build.bat`: when not `public` and `%NWN5E_TOOLS%\dev\dev_layer.cpp` exists, add `%NWN5E_TOOLS%\dev\*.cpp`, `/DNWN5E_DEV`, and `/I` for `src\core` and `src\data`, and echo "dev layer: on".
  - `src/core/ee_layout.h:609-628`: these constants move to the dev layer.
- Delete (moved): `src/core/anim_trace.cpp`, `src/core/anim_browser.cpp`, `src/core/vfx_editor.cpp`.

**Files (nwn5e-tools):**
- Create: `dev/dev_layer.cpp`, `dev/anim_trace.cpp`, `dev/anim_browser.cpp`, `dev/vfx_editor.cpp`, `dev/README.md`.
- The moved files are copied as they are, with only include paths and the ee_layout constants changed. Their first-line comment names the nwn5e-ee commit they came from.

**Interfaces:**
- Produces:
  ```cpp
  struct DevSeam {
    void (*anim_play)(void* gob, const char* name, float speed, int flag, float value) = nullptr;
    void (*anim_drive)(void* creature, const float* target, int facing, int a, int keys, int c) = nullptr;
    void (*anim_client)(void* client_creature) = nullptr;
  };
  extern DevSeam g_dev_seam;   // all null without the dev layer
  void InstallDev();           // dev/dev_layer.cpp: installs the trace, the !anim browser, the VFX editor; fills g_dev_seam
  ```
- `!anim <name>` now plays `<name>` directly, by the same path as the window's "Play typed", which keeps its 16-character cut. `!anim` with no argument still opens the window. The `AnimationNote` export is dropped, because only `!animedit` used it.

- [ ] **Step 1: Build the dev DLL** (`build.bat`, with `NWN5E_OUT` if the game runs). Expected: "dev layer: on", and it links.
- [ ] **Step 2: Build the public DLL** (`build.bat public` into a second scratch folder). Expected: it links without the moved files.
- [ ] **Step 3: Run `test.bat`.** Expected: all pass.
- [ ] **Step 4: Launch check** (windowed, `__COMPAT_LAYER=RunAsInvoker`, the dev build installed only with the owner's OK, or launched from scratch):
  - the log shows the dev layer installed;
  - `nwn5e.log` has no errors from the moved parts.

  Close the game and check tasklist.
- [ ] **Step 5: Commit nwn5e-tools** (`dev/*`): `feat: the dev layer - !animtrace, the !anim browser (now !anim <name>), the VFX editor (#87)`.
- [ ] **Step 6: Commit nwn5e-ee**, staging the edited files and the three deletions by path: `feat: the shipped DLL calls the dev layer only through a seam; the dev parts moved to nwn5e-tools/dev (#87)`.

### Task 6: Retire !animedit and the library export; check the public DLL

**Files (nwn5e-ee):**
- Delete: `src/core/anim_editor.cpp` (and its build.bat entry).
- Modify:
  - `src/core/anim_packs.cpp`: remove `ExportLibraries` (:188-199) and its calls (:222, :294), the export-only "no recipes" branch (:220-225), `g_animation_names`, and `AnimationNames()` (:301).
  - `src/core/config.h` / `build\nwn5e.ini` template text (`animation_recipes`: no `!animedit`; `stealth_crouch`: `nwn5e-tools\recipe_bench`).
- Create: `tests/public_dll_check.py <dll>`.
  - It exits 1 if the DLL's bytes contain any of `!animtrace\0`, `!animedit\0`, `!vfxedit\0`, `!vfxexport\0`, `!anim\0`, `animedit\libraries` (UTF-16 or ASCII).
  - It exits 1 if they don't contain `NWN5E_CORE_STAMP`'s hash (read from the `core_stamp.txt` beside the DLL).
  - It prints what it found.

- [ ] **Step 1: Run `python tests/public_dll_check.py <public scratch>\nwn5e.dll` before the change.** Expected: FAIL, since `!animedit` is still there.
- [ ] **Step 2: Make the deletions above.** Rebuild public and dev.
- [ ] **Step 3: Check the public build.** Expected: PASS.
- [ ] **Step 4: Deliberate break.** Run the check on the dev build's DLL. Expected: FAIL, listing `!animtrace` etc.
- [ ] **Step 5: Run `test.bat`.** Expected: all pass.
- [ ] **Step 6: Commit** `feat: !animedit retired and the library export removed - the bench reads the install (#87)`.

### Task 7: The assembled recipe format

**Files:**
- Modify: `src/data/anim_recipe.h`, `src/data/anim_recipe.cpp` (`AnimRecipe`, `WriteAnimRecipe` :312, `ReadAnimRecipe` :337)
- Test: `tests/data/anim_recipe_test.cpp`

**Interfaces:**
- Produces new `AnimRecipe` fields:
  ```cpp
  enum class AnimRecipeKind { kFormula, kKeys, kAssembled };
  AnimRecipeKind kind = AnimRecipeKind::kFormula;   // `keyed` stays, true for kKeys only
  std::string stance;        // assembled: the named NWN animation whose first frame is the neutral pose
  double blend_in = 0, blend_out = 0, loop_blend = 0.25;   // seconds
  bool loop = false;
  ```
- The text form:

  | Key | Value |
  |---|---|
  | `kind` | `assembled` |
  | `stance` | the stance's name |
  | `blend_in` | seconds |
  | `blend_out` | seconds |
  | `loop` | 0 or 1 |
  | `loop_blend` | seconds |
  | `length` | seconds |
  | `transtime` | seconds |
  | `source` | free text |

  The tracks reuse `keys <library> <bone> o t x y z w ...` and `keys <library> rootdummy p t x y z ...`. In an assembled recipe an `o` track is the bone's turn relative to the stance's first-frame turn (`L = S * delta`), and the rootdummy `p` track is the offset from the stance's first-frame root position.

- [ ] **Step 1: Write the failing tests.**
  - `TEST(AnimRecipeAssembledRoundTrip)`: a recipe with `stance="1hreadyr"`, blend_in 0.1, blend_out 0.3, loop false, length 1.0, and one `o` and one `p` track for `a_ba`. After `WriteAnimRecipe` then `ReadAnimRecipe`, every field and key is equal (key times to 4 decimals, quaternions to 5, positions to 4).
  - `TEST(AnimRecipeAssembledNeedsAStance)`: no `stance` line, so the read fails and the error mentions "stance".
  - `TEST(AnimRecipeOldKindsUnchanged)`: the existing keys and formula fixtures still read as before (`kind` is kKeys / kFormula).
- [ ] **Step 2: Run `test.bat`; verify the new tests fail.**
- [ ] **Step 3: Implement** the fields, the parser keys and the writer.
- [ ] **Step 4: Run `test.bat`.** Expected: pass.
- [ ] **Step 5: Deliberate breaks:** the writer drops `blend_out`; the reader accepts a missing stance. Each one is caught.
- [ ] **Step 6: Commit** `feat: the assembled recipe format - turns relative to a named stance (#87)`.

### Task 8: Assemble a clip: the capture on the stance, the root, the blends, the loop

**Files:**
- Create: `src/data/anim_assemble.h`, `src/data/anim_assemble.cpp` (add to build.bat's `src\data` list)
- Test: `tests/data/anim_assemble_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  // One library's clip from an assembled recipe, sampled every 1/30 s for frames 0..round(length*30):
  // each keyed bone L(t) = S * delta(t); the root position = stanceRoot + offset(t); then blended
  // toward the stance's first frame (S, stanceRoot) over blend_in / blend_out, and the loop
  // crossfade over loop_blend. False (with err) when the recipe has no keys for `library` or the
  // stance clip has no frame 0.
  bool AssembleAnimClip(const AnimRecipe& recipe, const std::string& library,
                        const std::vector<AnimNode>& bones, const AnimClip& stance,
                        AnimClip* out, std::string* err);
  ```
- The blend weight at time t is `w = min(smoothstep(t / blend_in), smoothstep((length - t) / blend_out))`, where a zero blend length means weight 1. Turns use `q = Mix(S, L, w)`, the existing sign-flipped nlerp. The root is `p = home + (P - home) * w`.
- The loop weight is `lw = smoothstep((t - (length - loop_blend)) / loop_blend)`, toward frame 0's values. The loop applies before the blends, as in today's `mocap.js` retarget.
- The delta tracks are sampled with the existing `Sample` (nlerp), and hold their last key past their end.
- Bones with no track are left out of the clip; the game then uses rest, as `KeyedAnimClip` does.

- [ ] **Step 1: Write the failing tests** (a synthetic two-bone body plus a stance clip whose frame 0 turns bone `b1` by 30° about Z, with its root at (0, 0, 1)).
  - `TEST(AssembleIdentityDeltaIsTheStance)`: an identity delta and zero offset give `b1` 30° about Z and the root at (0, 0, 1) on every frame.
  - `TEST(AssembleComposesOnTheStance)`: a delta of 20° about X makes `L(0.5) == S * Rx(20°)`, within 1e-5.
  - `TEST(AssembleBlendsFromAndToTheStance)`: with blend_in 0.2 and blend_out 0.2 and a 20° delta, frame 0 equals S, and at 0.5 s the turn is `S * Rx(20)`. The last frame equals S, and the angle from S at t = 0.1 is 10° ± 0.5° (smoothstep(0.5) = 0.5).
  - `TEST(AssembleRootOffset)`: an offset of (0, 0.3, 0) gives a root at (0, 0.3, 1) mid-clip, and at the home position at the blended ends.
  - `TEST(AssembleLoopsBackToTheStart)`: with loop on, loop_blend 0.25 and a delta that changes over time, the last frame's turn equals frame 0's within 1e-4.
  - `TEST(AssembleHoldsThePastLastKey)`: keys end at 0.5 s but the length is 1.0, so frames after 0.5 s equal the 0.5 s value (Review Focus 4).
  - `TEST(AssembleNoKeysForLibrary)`: asking for `a_fa` when only `a_ba` has keys returns false, and the error names `a_fa`.
- [ ] **Step 2: Run `test.bat`; verify these fail.**
- [ ] **Step 3: Implement `AssembleAnimClip`.** Reuse anim_recipe's `Mix`, `Sample` and `QMul` by moving their declarations into `anim_recipe.h`; they stay defined once.
- [ ] **Step 4: Run `test.bat`.** Expected: pass.
- [ ] **Step 5: Deliberate breaks:** `delta * S` instead of `S * delta`; linear weight instead of smoothstep; the loop after the blends; no hold past the last key. Each one is caught.
- [ ] **Step 6: Rebuild the helper** (Task 3's bat now compiles anim_assemble). Confirm `stamp` still matches the DLL build's `core_stamp.txt`.
- [ ] **Step 7: Commit** `feat: assembled recipes compose onto the named stance, with the root, the blends and the loop (#87)`.

### Task 9: The game builds assembled recipes

**Files:**
- Modify: `src/core/anim_packs.cpp` (`GenerateAnimationPacks` :202, the per-library loop :259-290)
- Create: `tests/data/fixtures/n5_asmtiny.recipe`. Write it with Task 7's `WriteAnimRecipe` from a small one-off program or test: stance `pause1`, `a_ba` keys only, `lthigh_g` turned 20° about X for 1 s, blends 0.1 / 0.1.

**Interfaces:**
- Consumes:
  - Task 2's `FindAnimClip` (the stance on each library's chain);
  - the library's bones (its `AnimLibrary::bones`);
  - Task 8's `AssembleAnimClip`.
- For each pack library, an assembled recipe goes through `AssembleAnimClip` and gets no `_hold` weapon layers, as keyed recipes don't.
- If the stance isn't found on a library's chain, or there are no keys for it, the log gets one line, `recipe <name>: <library>: <reason>`, and the other libraries still build (Review Focus 3).

- [ ] **Step 1: Build the DLL** (dev, with `NWN5E_OUT` if the game runs). Expected: it builds.
- [ ] **Step 2: Check the end-to-end path with a hand-made recipe.** Copy `tests/data/fixtures/n5_asmtiny.recipe` into a scratch `animedit\recipes`. It's a tiny assembled recipe written by Task 7's writer: stance `pause1`, a_ba only, one bone turned 20°. Then launch check from scratch:
  - `nwn5e.log` shows "n5_asmtiny" built for a_ba;
  - the 7 other bodies each log "no keys";
  - no crash.

  Close the game.
- [ ] **Step 3: Commit** `feat: the game builds assembled recipes at startup (#87)`, staging `src/core/anim_packs.cpp` and `tests/data/fixtures/n5_asmtiny.recipe`.

### Task 10: Convert a take, assemble it, compare it with today's recipe

**Files (nwn5e-tools):**
- Modify: `anim_helper/main.cpp`. Add `assemble <libraries_dir> <recipe_file> <library>`, which prints JSON:
  ```json
  {"library":"a_ba","length":1.0,"fps":30,
   "nodes":[{"name":"rootdummy","position":[[x,y,z],...]},{"name":"lthigh_g","orientation":[[x,y,z,w],...]}]}
  ```
  It exits 1 with the error on stderr if `AssembleAnimClip` fails.
- Create: `recipe_bench/convert_assembled.js` and `recipe_bench/compare_assembled.js` (both use `public/mocap.js` and `public/core.js`).

**Interfaces:**
- `node recipe_bench/convert_assembled.js --take <recording folder> --start <s> --end <s> --stance <name> --name <base> [--fit <s> --contact <s> --at <s>] [--blend-in <s>] [--blend-out <s>] [--place return|inplace] [--libraries <dir>] [--out <dir>]`:
  - Writes `<out>/<base>k.recipe`: today's keys recipe from today's `MC.retarget`, with suggested plants locked.
  - Writes `<out>/<base>a.recipe`: the assembled one. It comes from the same `MC.retarget` run with blends 0 and loop off (the pre-blend tracks). Each `o` key becomes `conj(S) * L` per body, with S the stance's frame 0 on that body. The root keys become `P - home`.
  - Blend lengths in output time: when fitted, blend_in × (at / contact) and blend_out × ((fit − at) / (length − contact)). Otherwise they are unchanged.
  - The defaults are the n5_shove slot's: blend in 0.1, blend out 0.3, fit 1.0.
  - `--out` defaults to `../nwn5e-ee/build/animedit/recipes`, and `--libraries` to `anim_helper/cache/libraries`.
  - Names stay ≤ 16 characters, so `!anim` can play them.
- `node recipe_bench/compare_assembled.js --keys <k.recipe> --assembled <a.recipe> [--libraries <dir>] [--helper <exe>]`:
  - for each of the eight bodies, it runs `assemble` and samples the keys recipe at the same frames (core.js `keyedClip` + `sample`);
  - it prints the worst turn (degrees, with bone and time) and the worst root difference (mm) per body;
  - it exits 1 when any body is over 1.5° or 8 mm.

- [ ] **Step 1: Convert an unfitted take.** Run `convert_assembled.js` on take `C:\Users\Bruno\freemocap_data\recordings\2026-10-29_21-20-50_GMT` with `--start 7.15 --end 9.75 --stance 1hreadyr --name n5_asm --blend-in 0.1 --blend-out 0.3` and no `--fit`. Then run the comparison. Expected: every body ≤ 1.5° / 8 mm, exit 0.
- [ ] **Step 2: The same with `--fit 1.0`** (contact suggested by `MC.suggestContact`, at 0.5). Expected: ≤ 1.5° / 8 mm (Review Focus 5).
- [ ] **Step 3: Deliberate breaks.** Each must make step 1 or step 2 exceed the limits:
  - write `L * conj(S)` instead of `conj(S) * L`;
  - drop the blend scaling under fit;
  - write the absolute root instead of `P - home`.
- [ ] **Step 4: Commit** `feat: convert a take to an assembled recipe and compare it with today's keys recipe (#87)`.

### Task 11: Version, install, docs, hand over

**Files:**
- Modify (nwn5e-ee):
  - `src/core/dllmain.cpp` (`kVersion`, the next minor);
  - `docs/PLAN.md` (#87 paragraph: what was built, the numbers from Task 10, the follow-ups);
  - `docs/BENCH-REDESIGN.md` (phase 1 built);
  - README if it lists `!animedit` or the export.
- Modify (nwn5e-tools): `README.md` (the dev layer, the helper, the converter); `dev/README.md`.

- [ ] **Step 1: Bump `kVersion`; build the dev DLL into scratch; run `test.bat`; run the public check (Task 6) on a public build.** Expected: all green.
- [ ] **Step 2: Commit both repos.** nwn5e-ee: `feat: <version> - the dev layer split and assembled recipes (#87)`.
- [ ] **Step 3: Install when the owner says the game is closed.** Copy the scratch build's `nwn5e.dll` and `core_stamp.txt` into `build\`.
- [ ] **Step 4: Open one follow-up issue** (`type:follow-up`, linked from #87) for what phase 1 leaves in the shipped DLL:
  - `vfx_model`'s editor half (shared binary internals with the lighting code);
  - the empty `visualeffects` row 1600;
  - the `vfx_bench` config key;
  - the `!vfx` and `!light` test windows.
- [ ] **Step 5: Hand over on #87.** Comment with the version, files, numbers, the test matrix below and caveats; add `needs-test`; move the board item to Needs test; print the matrix in chat.

  | # | Do | Expect |
  |---|----|--------|
  | P1 | Start the game | `!anim n5_asmk` and `!anim n5_asma` (from Task 10, unfitted) look the same on your character |
  | P2 | Play `!anim n5_asmfk` and `!anim n5_asmfa` (the fitted pair) | They look the same |
  | P3 | `!animtrace 5`, then walk | The trace works as before (it comes from the dev layer now) |
  | P4 | `!animedit` | Nothing: retired |
  | P5 | VFX Bench: open it, then Play in game on an effect | It plays in game as before |
  | P6 | Recipe Bench: open it | The bodies are listed (read from the install), and a take retargets as before |
  | P7 | Your existing animations: Shove, the crouch walk in stealth | As before |
