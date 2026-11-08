# nwn5e-ee — how we work

D&D 2024 (SRD 5.2) for Neverwinter Nights: Enhanced Edition, as an x64 DLL
(README.md; the design and decisions: docs/PLAN.md).

## Tracking: GitHub issues

- Work is tracked in **GitHub issues** on `BrunoTCarreiro/nwn5e-ee` and the
  **nwn5e board** (https://github.com/users/BrunoTCarreiro/projects/2):
  Todo → In progress → Needs test → Done.
- **docs/PLAN.md stays the design and decision record**: briefs, the owner's
  decisions (quoted, dated), what was built and why, acceptances. Issues track
  the work and name their PLAN.md section.
- **Labels:** `type:` feature | follow-up | bug | research | epic; `area:` one
  or two of combat, conditions, skills, stealth, grapple, character, classes,
  spells, rest, actions, interface, creatures, balance, tools, engine;
  `needs-decision` (waiting for the owner's pick); `needs-test` (built, waiting
  for the owner's in-game test).
- **Milestones** are the plan's phases (Phase 1 … Phase 9) and Tools.
- **Epics** hold native sub-issues, one per part (e.g. the Monk's parts).
- Open = not accepted yet; closed = accepted by the owner (or dropped, with a
  comment saying why).

## An issue's life

1. **Before building**, the work has an issue (open one if it's new): what,
   status, the PLAN.md section, what it depends on.
2. **A decision is needed:** explain it info first in chat (the issue, how it
   is now, the options, a recommendation), no pop-up, and wait for the owner's
   pick. Record the pick in PLAN.md (quoted) and as an issue comment; remove
   `needs-decision`.
3. **Starting:** board → In progress.
4. **Building:** commits name the issue (`feat: 0.58.0 - Monk part 4 (#29)`);
   the version (`kVersion`, src/core/dllmain.cpp) is bumped by the game
   session on `main`, once per install ("Two sessions, two working copies").
5. **Built:** comment on the issue with the version it ships in, what changed, the files,
   the test matrix and caveats; add `needs-test`; board → Needs test. Print
   the test matrix in chat too, and add its rows to the test register (below).
6. **The owner tests** and says "check the logs"; fixes go on the same issue
   (a new version and a comment each). Each result goes in the register.
7. **Accepted** (the owner says so): note it in PLAN.md (`*Accepted (owner,
   date: "...")*`), remove `needs-test`, close the issue (board → Done).
8. **Follow-ups found on the way** get their own issue (`type:follow-up`),
   linked from the one they came from.
9. **Never auto-advance:** finishing an issue isn't permission to start the
   next one; the owner picks.

## gh commands

```
gh issue list --repo BrunoTCarreiro/nwn5e-ee --state open [--milestone "Phase 3 — Character model and classes"] [--label needs-test]
gh issue view <n> --repo BrunoTCarreiro/nwn5e-ee
gh issue comment <n> --repo BrunoTCarreiro/nwn5e-ee --body-file <file>
gh issue edit <n> --repo BrunoTCarreiro/nwn5e-ee --add-label needs-test   (or --remove-label)
gh issue close <n> --repo BrunoTCarreiro/nwn5e-ee --comment "Accepted (owner, <date>)"
```

The board (project 2): find an issue's item with
`gh project item-list 2 --owner BrunoTCarreiro --format json --limit 300`,
then set its Status:

```
gh project item-edit --id <item id> --project-id PVT_kwHOAFhW084BpSF9 --field-id PVTSSF_lAHOAFhW084BpSF9zhnuadA --single-select-option-id <option>
```

| Status | option id |
|---|---|
| Todo | 6d2aff54 |
| In progress | 0bccb716 |
| Needs test | 0d6a4f6b |
| Done | 8d151b8d |

A new issue goes on the board with
`gh project item-add 2 --owner BrunoTCarreiro --url <issue url>`.

## The test register

Decided (owner, 2026-11-08, #147: "instead of having ephemeral test
matrices let's have an excel file where we keep all the tests, passed and
open so that I can just look at them any time and filter to only show the
open ones").

- `docs/tests.csv` holds every test, one row each: `id, status, issue,
  feature, version, setup, do, expect, note, added, result_date, stream,
  source`. Status: Open, Passed, Failed, Superseded (a retest or a newer
  row replaces it), Dropped.
- `python -I scripts/tests_sheet.py` writes `TESTS.xlsx` at this checkout's
  root (git-ignored), the owner's view: newest issue first, a filter on
  every column, open on the Open tests. It's rebuilt after every change to
  the register; an edit made in the spreadsheet is lost at the next
  rebuild, so results come in chat.
- **Edit it only with** 
  (a result, dated today) and  (rows in the
  same columns). Both rebuild the sheet, and both keep a cell's line breaks.
- **One writer:** the game session, on `main`. The animation session sends
  its rows with "anim ready" and its results by message; the game session
  adds them.
- **When:**
  - A "Built" comment's matrix: its rows, Open.
  - The owner's result ("NO-1 passes", a log read): Passed or Failed, with
    the owner's words in the note.
  - A retest row: the old row becomes Superseded, its note naming the retest.
  - An accepted issue: its rows still Open become Passed.

## Rules that always hold

- **Public repo, SRD only.** Rules text in commits, issues and comments is SRD
  5.2. Player's Handbook material lives only in the git-ignored `private/`
  layer (docs/PRIVATE-LAYER.md), which is never read into or quoted in public
  places. Game files (and art made from them), logs and saves are never
  committed or attached.
- **Every rule has two modes:** faithful (2024) and NWN mode (and why NWN does
  it that way). New features with no NWN counterpart are faithful only.
- **Git:** commit only your own paths, in one step (below); never `git add -A`;
  one git command per call; the owner pushes.
- **Builds:** never build into `build\` while nwmain.exe runs - build into a
  scratch folder (`NWN5E_OUT`) and install once the game is closed; one
  installer, the game session (below). After a launch check, close the game
  completely (and check it's gone).
- **Checks before "built":** the unit tests (test.bat), a mutation check of
  new rule tests (each deliberate break must fail a test), and a launch check
  for engine changes.

## Two sessions, two working copies

Decided (owner, 2026-11-02, #123: "we need a better way tio isolate the two
session so you don't have to be waiting for eachother", then "yes, go with
1 + 2"; before, 2026-10-30, one checkout by rules). Two sessions, each in its
own git working copy of this repo:

- **The game session** ("mechanics dev") works here, in `nwn5e-ee` on `main`.
  This checkout's `build\` is the one the game runs, and the bench saves its
  projects and recipes under `build\animedit`.
- **The animation session** ("animation dev") works in the worktree
  `..\nwn5e-ee-anim` on branch `anim`, with its own `build\` for test.bat and
  its builds. It never builds into this checkout's `build\`. The private layer
  lives only here: it builds and tests with `NWN5E_PRIVATE=<this
  checkout>\private` (build.bat and test.bat default to `private\`).
- **Merging.** The animation session commits on `anim` and keeps it current
  with `main` (`git merge main` in its worktree). When a change is ready for
  the game, it messages "anim ready at <commit>". The game session runs `git
  merge anim` here when its tree allows; its own uncommitted files can stay.
  Only this checkout moves `main`: moved from the worktree, this tree would
  look as if it had reverted the change.
- **Version and installs.** `kVersion` (src/core/dllmain.cpp) is changed only
  by the game session, on `main`, once per install: one number covers
  everything merged since the last install. `anim` never edits dllmain.cpp.
  The game session builds from `main`'s committed tree (`git diff --quiet
  HEAD -- src build.bat` before and after the build), launch-checks and
  installs. An install carrying animation changes is reported to the animation
  session, which gives the owner its tests.
- **No shared lines for new work.**
  - build.bat compiles every `src\core`, `src\data` and `src\rules` source, so
    a new file needs no build.bat line. Names are unique across those folders
    (one obj folder); build.bat stops on a duplicate.
  - Animation installs hang off `InstallAnimLayers()` (src/core/anim_layers.cpp),
    so there are no new animation lines in dllmain.cpp.
  - A game rule that plays an animation calls the animation session's API
    in one line (e.g. `RequestFlourishClip`, src/core/anim_flourishes.h).
    Which clip plays, its timing and swaps live in the animation files. A new
    hook in a game file is announced, with the lines, before it lands.
- **The anim helper** (nwn5e-tools) builds from this checkout by default, so
  its core stamp matches the installed DLL. `NWN5E_EE=<worktree>` builds one
  from `anim` for bench work before a merge. core_stamp.bat hashes the raw
  bytes: the same file with CRLF gives another stamp.

- **Who owns what.** Since 2026-11-01 (owner: "this sessions deals with
  animations (within and without the game) the other one should not touch
  anything related to the animations this one deals with. The other session
  should not ask me to test "!test anim" for example") the tools stream is the
  **animation session**: everything about animations, in the game and out of
  it - the nwn5e-tools repo (the benches, the helper, the dev layer behind its
  seam), `tools\`, the animation code (`src/data/anim_*`, `src/core/anim_*`,
  `tests/data/anim_*`,
  `tests/data/fixtures/*.recipe`), the shipped recipes (`data/anims/`), the
  animation test profile (`data/tests/anims.txt`), docs/TOOLS-PLAN.md, the
  bench's docs (docs/BENCH-*, docs/research), and the animation issues - their
  test matrices and the owner's animation tests. The game session: the rest of
  `src\`, `tests\`, `data\`, `scripts\`, `test.bat` and docs/PLAN.md. It
  doesn't edit the animation files, write animation test steps or ask the
  owner to test an animation: a game rule that plays an animation (Rage
  starting n5_rage) is the game's, how the animation looks is the animation
  session's. Shared - announced to the other session before editing, left
  alone by it until merged: `build.bat`, `test.bat`, `src/core/config.*`,
  `nwn5e.ini`, README.md, CLAUDE.md.
- **One installer.** The game session builds `main`, installs and
  launch-checks (above). It checks for itself whether the game is running
  (`tasklist` for nwmain.exe) instead of asking (owner, 2026-11-05: "instead
  of asking me to tell you when the game's closed, can't you just check?"):
  it installs once nwmain.exe isn't running, checks again right before the
  copy and compares every file after it. While the game runs it says the
  build is ready and installs when it's closed; "game closed" from the owner
  still works, and the animation session passes it on.
- **The front desk** (owner, 2026-11-03: "go with B"). The owner talks to the
  game session; it routes animation topics to the animation session. The
  animation window is for bench work and animation design.
  - **Forwarding:** animation topics go to the animation session with the
    owner's words quoted verbatim, and its answer is read back to the owner,
    whole for decisions (the info-first explanation it writes).
  - **Test matrices:** the animation session sends its matrix with each "anim
    ready"; the game session gives the owner one matrix per install, covering
    both streams.
  - **"check the logs":** after an install carrying animation changes, the
    game session reads its own lines and asks the animation session to read
    the animation lines (`flourish:`, `look:`, recipes), then reads both back.
  - **Lost messages:** anything that must not be lost also goes on its issue -
    a session that isn't running misses a message.
- **Commit in one step.** `git commit --only <your paths>` (the message by
  `-F -`), never a plain `git commit` after `git add`: a file the other
  session staged would go in with yours. A new file needs `git add <it>`
  first (`--only` takes only paths git knows). Before it, `git status
  --short` and each shared file's diff checked to be only yours.
- **Separate plan files.** docs/PLAN.md is the game's design and decision
  record; the tools stream's notes are in docs/TOOLS-PLAN.md (the tools session
  moves its paragraphs out of PLAN.md). Neither edits the other's file; a
  pointer to it is a line in your own.
- **Tools work behind the seam.** Beyond its animation files, the animation
  session changes nwn5e-ee's `src\` only through the dev-layer seam; anything
  else it must change there is announced first (the files named).
- **Launch checks and logs.** `build\nwn5e.log` and `Documents\Neverwinter
  Nights\logs\*` hold the owner's last test session: backed up before any
  launch and restored after it. Only your own launch's nwmain PID is killed.
- **Talk.** Installs, versions and shared files are agreed by cross-session
  message, never assumed from what is on disk.
