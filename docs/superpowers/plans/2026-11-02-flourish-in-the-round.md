# Flourishes Inside the Combat Round (#121) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rage's flourish and Intimidating Presence's play inside a combat round without cutting a swing or dropping the attack order. Each waits for the creature's current swing, then plays while its round pauses for the clip's length, as the engine pauses for an equip or a flinch. No attack is lost.

**Architecture:**
- **A pure decision** (src/data/anim_flourish): each tick, wait, play now, pause and play, wait for the next round, or drop.
- **An engine side** (src/core/anim_flourish): holds one request per creature and decides on a creature-update listener.
  - It pauses the round with the engine's own pause calls, without PauseCombatRound's flinch.
  - It sets the animation directly (CNWSCreature::SetAnimation), never a script PlayAnimation. A script PlayAnimation runs ClearAllActions, which cuts the swing, drops the target and empties the round.
- **One-line hooks** in rules_barbarian.cpp call it.

**Tech Stack:** C++17 DLL (MSVC, build.bat), tests in test.bat (tests/data/*.cpp), engine offsets in src/core/ee_layout.h.

**Spec:** issue #121 (its body and comments: the owner's report, "B and C, do not trim it, I'll tailor the animations to fit the budget", "C with A meanwhile, start #121 next", the game session's BA-12 finding). The engine facts come from the read-only research of 2026-11-02 on the owner's nwmain.exe 8193.37-17 (RVAs in the comments added to ee_layout.h).

## Global Constraints

- **Ownership:** animation files are this session's: src/data/anim_flourish.*, src/core/anim_flourish.*, tests/data/anim_flourish_test.cpp. Shared or game files need the game session's ack before any edit:
  - ee_layout.h: new constants;
  - rules_barbarian.cpp: the hooks;
  - build.bat: the new sources;
  - dllmain.cpp: the install call and kVersion.
- **Builds:**
  - The DLL is built in a scratch folder (`NWN5E_OUT`), never into build\ while nwmain runs.
  - The game session installs; kVersion is the next one after HEAD's, read just before the commit.
  - test.bat passes, with deliberate breaks for every new rule.
- **Engine behaviour (from the research, verified statically):**
  - **Never a script PlayAnimation for a flourish.** It runs ClearAllActions.
  - **A swing is the creature's own round pause.** Round +0x2F1C = 1, +0x2F20 = its id, animation +0xD8 = 9.
  - **CheckActionLength(by, ms)** refuses when the round is paused by another creature, or when RoundLength − Overlap − Timer + 1000 < ms.
  - **StartCombatRound clears any pause.**
  - **At a pause's end the engine sets animation 1 (ready) itself.**
- **Animation IDs:**
  - Rage: VICTORY2 = 45 (victorymg, swapped for n5_rage / _2h / _pl on part-based bodies);
  - Intimidating Presence, meanwhile: VICTORY3 = 46 (victoryth, 1.47 s on a_ba), or the owner's n5_presence once built (owner: C later).
- **Commits:** nwn5e-ee, `git commit --only <paths> -F -`, ending with `Claude-Session: https://claude.ai/code/session_01B9aC6onRCyqDh7qEdmA7n9`.

## Review Focus

1. **A flourish requested during the creature's own swing:** it waits. The swing isn't cut, and the attack order (+0x90 attack action, +0x724 target) survives.
2. **Requested after the round's last swing, when the pause doesn't fit:** it waits for the next round's start and plays then, not never.
3. **Requested out of combat, or with no combat round:** it plays at once without a pause, and the animation is reset after the clip so a later one re-sends.
4. **The creature dies, or Rage ends, while waiting:** the request drops and the victorymg swap is restored.
5. **The round paused by an enemy's swing:** the flourish waits, and never pauses over another creature's pause.

---

### Task 1: The decision (pure)

**Files:**
- Create: `src/data/anim_flourish.h`, `src/data/anim_flourish.cpp`
- Test: `tests/data/anim_flourish_test.cpp`

**Interfaces (Produces):**
- `struct FlourishTick { bool alive, wanted, has_round, round_started, paused, moving, fits; int64_t now_ms, not_before_ms, give_up_ms; };`
- `enum class FlourishStep { Wait, PlayNow, PauseAndPlay, Drop };`
- `FlourishStep DecideFlourish(const FlourishTick&)`, applied in this order:
  1. Drop when not alive, not wanted, or past give_up.
  2. Wait before not_before.
  3. PlayNow when there is no round, or it isn't started.
  4. Wait while paused or moving.
  5. PauseAndPlay when it fits.
  6. Otherwise Wait (the next round's start clears the pause and the timer).
- `int FlourishPauseMs(float clip_seconds)`: the clip plus a 75 ms pad, so the engine's "ready" at the pause's end comes after the clip.

- [ ] **Step 1: Write the failing tests** in tests/data/anim_flourish_test.cpp, in the existing tests' style (see anim_recipe_test.cpp's registration):
  - `flourishDropsWhenDeadOrUnwanted`, `flourishDropsPastGiveUp`;
  - `flourishWaitsForTheSwapGap`;
  - `flourishPlaysAtOnceOutOfCombat` (no round; round not started);
  - `flourishWaitsWhilePaused` (its own swing or an enemy's), `flourishWaitsWhileMoving`;
  - `flourishPausesWhenItFits`, `flourishWaitsWhenItDoesNotFit`;
  - `flourishPauseCoversTheClip` (1.47 s → 1545 ms; 3.4667 s → 3542 ms).
- [ ] **Step 2: Run test.bat.** Expected: they fail to compile (no anim_flourish.h).
- [ ] **Step 3: Implement** anim_flourish.h/.cpp in src/data (namespace data).
- [ ] **Step 4: Run test.bat.** All pass.
- [ ] **Step 5: Breaks**, each CAUGHT: the drop order; the swap gap; out of combat; paused; moving; fits; the pad.
- [ ] **Step 6: Commit** — `feat: the flourish's decision - wait for the swing, pause the round, or play at once (#121, 1/3)`.

### Task 2: The engine side

**Files (needing the game session's ack where marked):**
- Create: `src/core/anim_flourish.h`, `src/core/anim_flourish.cpp`
- Modify (ack): `src/core/ee_layout.h` (new constants), `build.bat` (the two sources), `src/core/dllmain.cpp` (install call)

**Interfaces:**
- Consumes: `data::DecideFlourish`, `data::FlourishPauseMs` (Task 1).
- Produces: `void RequestFlourish(void* creature, int engine_animation, float clip_seconds, const char* swap_from, const std::string& swap_to, bool (*wanted)(void*))`.
  - `swap_from` is null for no swap, `swap_to` the replacement's name, `wanted` null for always.
  - `bool InstallFlourish()` registers the listener.
- Produces, in ee_layout.h, with their RVA notes:
  - round: `kRoundTimer` +0x2F0C, `kRoundLength` +0x2F10, `kRoundOverlap` +0x2F14;
  - object: `kObjectAnimationSpeed` +0xDC (float);
  - animation IDs `kAnimationSwing` 9, `kAnimationVictory2` 45, `kAnimationVictory3` 46.
  - The rest (round +0x2F04/+0x2F1C/+0x2F20/+0x2F24, creature +0x880/+0x700/+0x724/+0x90, object +0xD8) already exists; reuse those names.

- [ ] **Step 1: The request store and the listener.** One request per creature id, holding:
  - the animation, the clip's ms, swap_from and swap_to;
  - not_before = now + 100 ms (the swap reaches the client first);
  - give_up = now + 12 s game time;
  - `wanted`.
- [ ] **Step 2: The swap.**
  - When the request is made: `ReplaceObjectAnimation(swap_from → swap_to)` through a script chunk, never a PlayAnimation.
  - At clip + 0.6 s after it plays, or at once when the request drops: back to itself.
  - A new request for the same creature replaces the old one, restoring its swap first.
- [ ] **Step 3: Each tick**, build a FlourishTick from:
  - creature alive; `wanted(creature)`;
  - the round pointer; RoundStarted; Paused;
  - moving: the current action +0x90 is a move, or the animation is a moving one: 2-4 or 78/79/84-87, as AnimationStationary;
  - fits: CheckActionLength(self, pause_ms) when not paused;
  - game time.

  Then act on the step:
  - **PlayNow:** write +0xDC = 1.0f, SetAnimation(anim); at clip end, SetAnimation(0) if +0xD8 still holds it.
  - **PauseAndPlay:** SetRoundPaused(1, self), SetPauseTimer(pause_ms, 0), DecrementRoundLength(pause_ms, 0), +0xDC = 1.0f, SetAnimation(anim).
  - **Drop:** restore the swap.
- [ ] **Step 4: Logs**, one line per stage (requested / waiting why / played how / dropped why), with paused, by, pause timer, Timer, length and +0xD8.
- [ ] **Step 5: Install and build.**
  - Announce the ee_layout.h, build.bat and dllmain.cpp edits to the game session and wait for its ack.
  - Then add `InstallFlourish()` to dllmain's installs and the two sources to build.bat.
  - Build in a scratch folder; run test.bat.
- [ ] **Step 6: Commit** — `feat: the flourish helper - waits for the swing, pauses the round, sets the animation directly (#121, 2/3)`.

### Task 3: The hooks, the version, the game test

**Files:** Modify (ack): `src/core/rules_barbarian.cpp` (RageFlourish's call site in StartRage; Intimidating Presence's chunk), `src/core/dllmain.cpp` (kVersion).

- [ ] **Step 1: Send the game session the exact lines and wait for its ack.**
  - **StartRage:** the script string from `RageFlourish` loses both `PlayAnimation` calls and the swap and restore. In their place, after `RunScriptChunk`, `RequestFlourish(creature, kAnimationVictory2, length of the chosen n5_rage recipe (or victorymg's 1.734 s when not built), "victorymg" when part-based else nullptr, name, &IsRaging)`.
  - **Intimidating Presence:** the chunk loses `PlayAnimation(ANIMATION_FIREFORGET_TAUNT, 1.0);`. After it, `RequestFlourish(creature, kAnimationVictory3, 1.47f, part-based and n5_presence built ? "victoryth" : nullptr, "n5_presence", nullptr)`. The owner's later clip (C) then needs no code change.
- [ ] **Step 2: Bump kVersion and build in a scratch folder.** Hand the build to the game session to install on "game closed"; the launch check is in its log (the per-stage lines on a Rage).
- [ ] **Step 3: The owner's test,** then "check the logs". The matrix:
  1. Rage with no enemy near: plays at once, n5_rage whole.
  2. Rage mid-fight on the default Bonus Action at a round's start: plays before the first swing, which then follows; no swing cut; the target kept.
  3. Rage mid-round right after a swing: waits for it, then plays; the next swing after the clip.
  4. Rage after the round's last swing: plays at the next round's start.
  5. Intimidating Presence mid-fight: victoryth, the same rules.
  6. A two-handed hold: n5_rage_2h if built, else n5_rage.
- [ ] **Step 4: Commit** — `feat: <version> - Rage's and Intimidating Presence's flourishes wait for the swing and pause the round; no swing cut, no target lost (#121, 3/3)`. Record in TOOLS-PLAN and #121; the board goes to Needs test.

### After the tasks

A fresh reviewer (the most capable model) on the whole range, with the plan's Review Focus; one fix pass under TDD.
