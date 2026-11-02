# Clip Versions Saved Together Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A clip keeps its two-handed and polearm versions (#112) as part of itself; one Save writes the clip and
every version it has, from the same take and edits; a switch previews each version on the doll.

**Architecture:**
- `project.versions` holds, per kind, whether the clip has that version and its follow share.
- `versionProject(project, kind)` (project.js, pure) builds a version's project: name, grip, follow, no versions.
- The page rebases its keys onto the version's start pose (`onStance`, as the version button does), solves it on the
  eight bodies and builds its recipe.
- `/api/save` takes the versions' recipes beside the clip's. Each is checked on every body with the game's code;
  either all are written or none.

**Tech Stack:** Node / browser JS (nwn5e-tools/mocap_bench). No DLL change.

**Spec:** GitHub issue BrunoTCarreiro/nwn5e-ee#117 and its decision comment (owner, 2026-11-02: "go with your
recommendations, 117 first").

## Global Constraints

- Bench only (nwn5e-tools); no nwn5e-ee source change, so no install.
- Older projects open unchanged: no versions.
- The version kinds and their start poses are `VERSIONS` (#112): two-handed on 2hreadyr (`_2h`, follow 0.5), polearm on
  plreadyr (`_pl`, follow 0.25). A version's name must fit 16 characters (`versionName`).
- The separate-project buttons (#112) stay as the way to make a version with its own edits (the issue's "not now").
  Save says when a version's recipe replaces one of theirs.
- Save writes nothing unless every recipe (the clip's and each version's) passes on every body.

## Review Focus

1. A version whose name doesn't fit: its box is refused with the reason, and Save still writes the clip.
2. A refused version recipe: nothing is written, the clip included, and the status names the version.
3. An edit made while Save builds the versions: Save reports and writes what it built (as today); the page keeps the
   edit.
4. The preview switched to a version, then that version turned off: the preview goes back to the clip.
5. A saved separate project of a version's name: Save says its recipe is replaced.

---

### Task 1: The model

**Files:** `mocap_bench/public/core/project.js`, test `mocap_bench/checks/project_check.js`.

**Produces:**
- `project.versions = { twohand: { on, follow }, polearm: { on, follow } }`, a top-level key ordered after `clip`.
  Validated: `on` true only for `true`, `follow` a number clamped 0..1 or the kind's default. Absent means both off.
- `versionProject(project, kind)` returns `{ project, stance }` or `{ error }`. The project is a clone with
  `name = versionName(...)`, `slot = ""`, `clip.grip = true`, `clip.gripFollow` = the version's follow, and no
  `versions`; its stance is still the clip's (the page rebases). `stance` is the version's start pose.
- Checks:
  - `versionsDefault`: new and older projects have both off with defaults 0.5 / 0.25; clamping; unknown kinds
    dropped; the key order.
  - `versionProject`: name, grip, follow, slot, no versions, the stance returned; over 16 characters gives an
    error.

### Task 2: The server saves the versions with the clip

**Files:** `mocap_bench/server.js`, test `mocap_bench/checks/server_check.js`.

- `/api/save` body may carry `versions: [{ name, recipe }]`. Each name must be the project's name with a version
  suffix, and its recipe must be named so; else 400.
- Every recipe is assembled on every body; any refusal gives 422 with `version` naming it, and nothing is written.
- On success it writes the project, the clip's recipe, then each version's recipe. The response adds
  `versions: [{ name, recipePath, replaced }]`.
- Check `saveWritesVersions`:
  - a save with two versions writes three recipes;
  - a misnamed version gives 400, nothing written;
  - a version the core refuses gives 422, nothing written.

### Task 3: The page

**Files:** `mocap_bench/public/app.js`, `mocap_bench/public/inspector.js`, `mocap_bench/public/core/pipeline.js` (no
change expected), test `mocap_bench/browser_check.mjs`.

- **Inspector:** the clip's "two-handed" field shows a row per kind:
  - a box "save the two-handed version (`<name>_2h`)" (disabled with the reason when the name doesn't fit);
  - its "weapon follows the arms" slider while on;
  - a "show" select (the clip / each version that's on);
  - the separate-project buttons, renamed "separate two-handed project".
  The clip's own grip box and slider stay.
- **`versionFor(project, kind)`:** `versionProject` + `onStance(project with the clip's stance, version stance)`,
  cached by the project's text and the kind.
- **Preview:** while `state.showing` is a kind that's on, `refresh` runs the version's project; the gear follows the
  shown start pose. Turning the shown version off shows the clip.
- **Save:**
  - after the clip's solve, each version that's on is built (`versionFor`), solved on the eight bodies, and its
    recipe built;
  - the post carries them;
  - the status names the versions saved and any separate project whose recipe was replaced;
  - Save's notes list the clip's issues, then each version's prefixed with its label.
- **Browser checks:**
  - `versionsSaveTogether`: tick both and Save. Three recipes are written; the versions name their start pose and
    `grip = 1` / `grip_follow`; the clip's recipe has no grip.
  - `versionPreview`: show the polearm version (the preview's recipe names plreadyr, gear a halberd); turn it off
    (the clip shown).
  - `twoHandedVersion` follows the renamed buttons.
- The suite: checks/run.js and browser_check pass; deliberate breaks for each rule; commit per task.
