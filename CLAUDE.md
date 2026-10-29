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
   each change bumps the version (`kVersion`, src/core/dllmain.cpp).
5. **Built:** comment on the issue with the version, what changed, the files,
   the test matrix and caveats; add `needs-test`; board → Needs test. Print
   the test matrix in chat too.
6. **The owner tests** and says "check the logs"; fixes go on the same issue
   (a new version and a comment each).
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

## Rules that always hold

- **Public repo, SRD only.** Rules text in commits, issues and comments is SRD
  5.2. Player's Handbook material lives only in the git-ignored `private/`
  layer (docs/PRIVATE-LAYER.md), which is never read into or quoted in public
  places. Game files (and art made from them), logs and saves are never
  committed or attached.
- **Every rule has two modes:** faithful (2024) and NWN mode (and why NWN does
  it that way). New features with no NWN counterpart are faithful only.
- **Git:** stage by explicit path, never `git add -A`; one git command per
  call; the owner pushes.
- **Builds:** never build into `build\` while nwmain.exe runs - build into a
  scratch folder (`NWN5E_OUT`) and install once the game is closed. After a
  launch check, close the game completely (and check it's gone).
- **Checks before "built":** the unit tests (test.bat), a mutation check of
  new rule tests (each deliberate break must fail a test), and a launch check
  for engine changes.
