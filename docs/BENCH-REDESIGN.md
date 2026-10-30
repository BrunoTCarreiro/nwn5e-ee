# The Mocap Bench - a redesign

Status: approved by the owner (2026-10-30: "approved, write the phase 1 plan"); epic #86, phases #87-#93. Phase 1 built
in 0.58.0 and awaiting the owner's test (#87; what it leaves in the shipped DLL: #95). The current tool is the
Animation Recipe Bench, which moved on 2026-10-30 with the other dev tools from this repo's
`tools/` to the `nwn5e-tools` repo beside it (#85): it is now `nwn5e-tools/recipe_bench/`.
The research behind this is in `docs/research/reports/Mocap cleanup and editing workflow.md`.

## Why

The bench began on 2026-10-25 as a live preview for recipes, which are animations derived
from NWN's own. Over five days it grew a Mocap tab one request at a time: RTMPose input,
the Blender round trip, a sticky preview, foot locks, the timeline, the knee rule, the
neutral stance, rough-capture repair, drawn feet, pose keys, and mixing legs and torso.
Each step fixed a real problem, but almost every one added its own special control.

Today the Mocap tab has:

- one long card of about 50 controls;
- 19 paragraphs of explanation;
- vector math in four copies and forward kinematics in five;
- two clocks that disagree once a take is fitted to a length;
- a full re-solve on every input, and a redraw that rewrites the page's controls every
  frame;
- edits that live only in browser storage, so a saved animation can't be reopened.

No mocap animation has been confirmed playing in game yet. #74 and #77-#84 are all still
waiting for a test.

The owner wants to "adjust speed, position in space with IK, keying certain poses". He
wants "something that gives me more control over the timing and position of the various
body parts to make sure poses are timed correctly and impactful". He says "keying and
correcting placements with the current 3 sliders is cumbersome", and adds: "I don't know
much about mocap data cleaning and editing ... I might be taking the wrong path."

The research says the path is right: professional animators make exactly those three
edits on top of mocap. What makes their tools efficient is the structure:

- the capture is cleaned automatically once, then never touched;
- every edit lives above it;
- IK targets ease in and out of a hold ("reach");
- timing is one speed curve anchored to the hit.

It also says the capture's weak point at 13-27 fps is the few frames around a strike.
Those frames carry the impact, so the artist re-poses them, and the tool should make that
quick.

The owner's decisions:

- *(2026-10-30)* He approved this picture: "sounds right, write the design doc".
- *(2026-10-30, asked whether mixes of his captures with NWN's animations are for his own
  game or for everyone)* "they should ship with nwn5e".
- *(2026-10-30, #85)* "I think we can actually remove the benches from the nwn5e code.
  these are development tools". Asked whether that meant the tool folders only or the
  DLL's dev-only parts too, he answered "b, go with your recommendation":
  - the folders moved to `nwn5e-tools` straight away;
  - the DLL's dev-only parts come out in this redesign's first phase.

## Goals

- **Take to game quickly.** A simple move goes from a recorded take to a test in game in
  minutes.
- **Direct manipulation.** Pick a body part on the doll and drag it. Numbers are there
  for exactness, not as the way in.
- **Few concepts, one timeline.** A project starts from a take, or from an NWN animation.
  Edits sit on tracks on one timeline, anchored to a few marked moments. Save writes a
  recipe.
- **Nothing destructive.** The capture stays as it was. Every edit can be changed, muted
  or deleted. There's undo.
- **Re-editable.** Work is saved as a project file. Reopen it, change it, save again.
- **Ships with nwn5e.** Every recipe the bench writes can go into nwn5e's public repo.
  It holds only the owner's own data and names NWN's animations without copying them.
  The game assembles it at startup from each player's own files.
- **What you see is what the game plays.** The bench's preview and the game's assembly
  are the same code.
- **Visible.** See the capture against the doll, and the original against the edit (a
  ghost). See where feet and hands are held, and where the capture was poor.
- **Keep what works.** Everything the current core does correctly carries over:
  - the retarget onto all eight bodies;
  - the knee rule;
  - foot locks;
  - the neutral stance and the blends into it;
  - rough-capture repair;
  - mixing legs and torso;
  - the crouch and the lean.

## Non-goals

- **A general animation package.** That means:
  - no other skeletons, no faces;
  - no physics;
  - no full-body IK with a balance solver;
  - no learned cleanup or retargeting.
- **Mixing animations live in the game.** That was decided under #84: recipes are
  assembled once, at startup.
- **Reloading recipes without a restart.** That would be a separate game issue if wanted.
- **Hosting.** The bench stays a local app.

## Anti-goals (a failure even if it works)

- A rail of settings that grows with every new problem. A new need is a tool on the
  timeline or a field in the inspector, never another permanent panel.
- Two ways to do the same edit, or two clocks.
- A preview that shows something different from what the game plays.
- Edits that live only in the browser.
- NWN data inside a recipe meant to ship.
- Walls of explanation in the page. One line of help per selection; the README holds the
  rest.
- Losing correctness the current core has: no backward knees, no sliding locked feet, the
  hips' real height, a_dfa's turned pelvis.

## Constraints

- **Where things live** (#85).
  - **nwn5e (this repo) keeps only what players need.** The shipped DLL reads and
    assembles recipes at startup, and that is all it does for animations. The shared C++
    core (see Shipping) lives here, because the game needs it.
  - **Everything for making things lives in `nwn5e-tools`:**
    - the bench;
    - the helper program, built there from this repo's core sources in the checkout
      beside it;
    - a **dev add-on**, loaded into the game on the owner's machine only.
  - **The dev add-on holds the DLL's dev-only parts.** These are:
    - the `!anim` browser and `!animtrace`;
    - the VFX editor's plumbing (`!vfxedit`, the watched folder, play in game);
    - the VFX bench's export.

    `!animedit` is retired outright, because the bench replaces it.
  - **The tools read the game's own files from the install** (its data archives), not
    from an export the shipped DLL writes. Nothing of the game's goes into either repo.
- **Local and dependency-free to run.** The bench runs on 127.0.0.1, is started by a
  `.bat`, and needs Node with no npm installs. It works offline. Saves are accepted only
  from the page's own origin, and the server checks the Host header. It is never hosted
  or published as an artifact.
- **Public repo, SRD only.** Game files, and anything made from them, never go in the
  repo. So:
  - A recipe meant to ship names every NWN animation and pose it uses (stance, bases,
    sources) and never copies their keys. That includes the stance it blends into, and
    the stance the capture is measured from.
  - The bench reads the body libraries from the player's own install.
  - Projects live under nwn5e's `build\` (git-ignored).
- **The game reads recipes at startup.** Each test needs a restart. The newest file of a
  name wins, and the owner's copy overrides nwn5e's. The game keeps reading today's two
  kinds, formula recipes and keys recipes, so nothing already made breaks.
- **NWN skeleton facts the design relies on:**
  - rootdummy carries torso_g and pelvis_g side by side, and only rootdummy has position
    keys;
  - there is no toe bone;
  - the weapon layers use animroot torso_g;
  - bind poses differ per library (a_dfa's pelvis is turned 22°), and halfling shins are
    about 10% shorter;
  - the ready stances turn rootdummy by 17-24°.
- **Process** (nwn5e-ee/CLAUDE.md):
  - every phase has an issue;
  - decisions go info-first and are quoted in PLAN.md;
  - every check needs deliberate breaks it catches;
  - the owner does the in-game test;
  - nothing auto-advances;
  - phases that change the DLL bump its version and go through test.bat.

## The workflow, as the owner sees it

1. **Open a project.** Pick a recording from the list (the server reads the FreeMoCap
   recordings folder) or drop its files. Or start from an NWN animation instead (the
   crouch walk starts from `walk`). Name it or pick a slot: n5_shove, n5_rage, n5_cridle,
   or a name of your own.
2. **Clean (automatic, for a take).** The bench cleans the take and shows a short report:
   the real frame rate, lost stretches, the floor, the standing part. Poor stretches are
   marked red on the timeline. Three things can be changed:
   - one smoothing control, from "steady" to "sharp";
   - the standing part;
   - the kept part.

   The standing and kept parts are dragged as spans on the timeline's overview strip.
3. **Mark the moments.** The bench suggests the hit (the fastest hands or feet) and each
   foot's contacts (as lanes). It can also mark a recovery moment, where control
   returns. Drag them where they belong. The animation starts and ends at the kept part's
   ends, and the blends into and out of the stance overlay them, as today.
4. **Edit.** Scrub to a frame, click a body part on the doll, and drag:
   - **A hand, foot or the hips:** an IK target. The limb follows, and the target is keyed
     at the marker. Holding a hand or foot somewhere for a stretch is a pin. Feet in
     contact are pinned automatically from the contact lanes.
   - **Any other part:** turn it. That's a pose key, a push on top of the capture. For
     frames the capture smeared, switch the key to replace instead.
   - **Timing,** on the time track:
     - drag the hit;
     - "hold at the hit" for N frames;
     - speed a stretch up or down;
     - set the overall length and where the hit lands.
   - **Another source:** take the legs, the torso or the whole body from an NWN
     animation, at a weight. Weight 1 replaces; less blends, like the old "mix toward".
     It shows as a block on the timeline that you can slide and stretch.
   - **Crouch and lean,** as before, in the clip's inspector.
5. **Check.** Compare against the ghost of the unedited result, and the capture's points
   drawn on the doll's own proportions. Readouts show foot slide and out-of-reach limbs
   on any of the eight bodies. Camera presets cover front, side, 3/4, the stance views and
   the game's camera pitch.
6. **Save.** Save writes the project and the recipe. Then restart the game and play it:
   `!anim <name>`, or the slot's trigger.

## The screen

One page, filling the window. There's no scrolling rail and no sticky-preview layout
tricks.

```
+--------------------------------------------------------------+------------------+
| project: n5_shove [slot]  take: 2026-10-29_21-20-50  body: a_ba  [Save]  status  |
+--------------------------------------------------------------+------------------+
|                                                              |  INSPECTOR       |
|                         STAGE                                |  (the selection: |
|        the doll, the ghost, the capture's points,            |   a key, a part, |
|        pins and targets, the picked part's handle            |   a contact, a   |
|                                                              |   source block,  |
|  [front] [side] [3/4] [stance] [game]   ghost [x] points [x] |   the take, the  |
|                                                              |   clip)          |
+--------------------------------------------------------------+------------------+
| > || |< < > >|   animation 0.40 s · frame 12 · take 8.31 s     snap: marker/frame|
| OVERVIEW [standing]    [kept.......]   (red: poor capture)   (the whole take)    |
| TIME     |   x1.3   |  hit +3  |   x0.8   |  (animation seconds ticked along it)  |
| CONTACT  L foot [=====]      [=======]    R foot [==========]   hands (if any)   |
| TARGETS  L hand  o        o              R foot      o                           |
| POSES    torso   <>    <>      R foot <>  <>                                     |
| SOURCES  legs: nwreadyr [--------------]                                         |
+----------------------------------------------------------------------------------+
```

- **Stage.** It keeps the current canvas renderer: sticks, drawn feet, shadows, rings
  under pinned feet.
  - **Picking:** the nearest bone on screen.
  - **Dragging:** moves or turns in the plane facing the camera. To work in depth,
    change the view.
  - **Precise numbers:** in the inspector.
- **Inspector.** It shows only the selection's few settings, and replaces the rail:
  - a pose key: push or replace, ease, the exact turn;
  - a contact: auto, always or never;
  - a source block: parts, weight, offset, speed, loop, seam;
  - the take: smoothing, re-suggest;
  - the clip: stance, blends, loop, return to the spot or stay in place, length, blend
    time, crouch, lean.
- **Timeline.** There's one ruler, and it is **take time** (for an NWN base, the base
  animation's time). *(Builder's call.)*
  - The ruler is zoomed to the kept part. The overview strip above shows the whole take:
    the standing part, the kept part and poor stretches. Drag on it to set the spans, or
    to scroll and zoom.
  - Every edit is keyed in take time, so every edit sits on this ruler.
  - The animation's own time, as it will play, is shown, not used as a second ruler:
    - ticks along the time track;
    - the transport readout;
    - playback, which runs in animation time and moves the marker along take time. During
      a hold the marker stands still while the animation clock runs.
  - Speed changes show as shaded stretches labelled ×1.3 or ×0.8. Holds show as a badge
    on their moment ("+3").
  - Dragging the hit picks which take moment is the hit. Where it lands in the animation
    is a field in its inspector, beside the overall length.
  - Rows appear only when they hold something, apart from overview, time and contacts.
  - Everything snaps to the marker and to whole frames.
- **Keys:**

  | Key | Does |
  |---|---|
  | Space | play / pause |
  | ← → | step a frame |
  | Delete | delete the selection |
  | Ctrl+Z / Ctrl+Y | undo / redo |
  | W / E | move / turn |
  | K | key here |
  | 1-5 | camera presets |

## The model

A project is one JSON file, `build\animedit\projects\<name>.json`.

```
base      a take (recording folder or files, the clock, standing [t0,t1], kept [t0,t1])
          or an NWN animation by name (kept [t0,t1] on its own time)
clean     smoothing (steady..sharp), the clean-up version it was made with   (a take only)
clip      name (= the project's and the recipe's), slot, stance (by name), blend in/out,
          loop, place (returns | in place), transtime, crouch, lean
moments   hit, recover                                              (take time)
contacts  per limb: intervals [t0,t1] with auto|always|never        (take time)
targets   per limb (hands, feet, hips): keys { t, offset, pinned, reach }   (take time)
poses     per part: keys { t, turn (deg about the stance axes), push|replace, ease }
time      time keys (animation t -> take t), monotone; holds, speed spans, the hit's
          landing time and the length are all made of them
sources   { parts: legs|torso|whole, from: NWN animation by name, weight, offset, speed,
            loop, seam }
balance   the hips follow the weight (on|off)
```

- **One clock for edits.** Every edit is keyed in take time, so a key belongs to a moment
  of the performance and stays with it when the timing changes. The time track maps the
  animation's time to take time.
- **The capture is never stored in the project.** It is recomputed from the take and the
  cleaning settings. A project remembers which clean-up version made it. When a newer
  clean-up would change the take, the bench says so on opening rather than shifting the
  edits silently.
- **The hips move only through their IK target.** Pose keys turn parts; they don't move
  the body. *(Builder's call.)*
- **Targets carry to all eight bodies by proportion.** *(Builder's call.)* A target is
  stored as an offset in the authoring body's own measure, then mapped onto each body:
  - a hand: from its shoulder, in the torso's frame, as a fraction of the arm's length;
  - a foot: from its hip joint, in the pelvis's frame, as a fraction of the leg's length;
  - the hips: from the stance's home, as a fraction of the hip height, the same rule as
    today's root travel.

  A pin holds wherever the limb was when it began, worked out on each body, as foot locks
  do today. A per-body readout flags any body where a target ends up out of reach.

## Shipping: the assembled recipe

Save writes one new kind of recipe, `kind = assembled` (the name is the builder's), for
every project. It holds only the owner's data:

- **The capture, per body, as turns relative to the named stance.** The bench's solve
  (the clean-up, the retarget, the knee rule and the legs' neutral) runs in the bench
  only, because it reads the take. Its result is stored as each bone's change from the
  stance's pose, never as absolute keys. So the stance's own pose is not in the file.
- **Names:** the stance, an NWN base, the sources, and any NWN animation used.
- **Every edit**, as in the project: moments, contacts, targets, poses, time, sources,
  balance, clip settings.

The game assembles each recipe at startup, for each body, from the player's own files,
in this order:

1. The capture composed onto the stance (or the NWN base).
2. The sources spliced in, carrying an animation's root turn into the hips and torso.
3. The time track.
4. Pose keys.
5. Crouch and lean.
6. The root.
7. The balance.
8. One IK pass: pins, targets, knee and elbow direction, extension damping, the hips
   sinking.
9. The blends into and out of the stance, and the loop.

**One implementation.** *(Builder's call; the biggest call in this design.)* Steps 1-9
live once, in C++, in the DLL's sources (`src/data/`). The same code is built twice:

- into the DLL, which assembles recipes at startup;
- into a small helper program the bench's server runs. It is built in `nwn5e-tools` from
  this repo's sources in the checkout beside it. The page sends it the project and the
  capture; it returns poses.

So the preview is the game's own math, and the two can't drift. The helper and the DLL
carry a build stamp; the bench refuses to save if they differ. It's also cached per
stage: moving a pose key re-runs only the steps after pose keys.

The alternative, two implementations (JavaScript for the preview, C++ for the game) kept
in step by checks, is how formula recipes work today. It was rejected because every
feature would be written twice and preview-vs-game drift is the failure that matters
most here.

The old kinds stay readable: formula recipes (the crouch walk) and keys recipes (n5_shove
as saved today). The new bench can open a formula recipe as a project: its base is the
animation, mix toward is a whole-body source with a weight, and crouch and lean carry
over as clip settings. The game's existing formula path is untouched.

## Clean-up, from the research (the bench, for a take)

- **The clock.** Real frame times from FreeMoCap's timestamps, resampled to 30 a second,
  as today.
- **Outliers.** A Hampel test on each point's distance from a short smoothed track. Only
  isolated spikes that snap back are dropped, so a fast strike is never mistaken for a
  glitch.
- **Gaps.** Up to 6-10 frames are filled with a cubic. Longer ones are held and marked red.
  Today's repair of lost limbs and lost heels and toes stays.
- **Left/right swaps.** At each frame, compare the crossed and uncrossed pairing of the
  left and right points with the frame before, and relabel when crossed fits better.
- **Filtering.**
  - A zero-phase Butterworth per joint group: the trunk lower, the hands and feet higher.
  - Never above 0.25 × the real frame rate.
  - Around strikes (speed peaks of the wrists and ankles), crossfade over 2-3 frames to a
    light filter so the peak survives.
  - The one smoothing control moves all the cutoffs together.
- **Floor and contacts.** The floor comes from the standing part, as today. A contact is
  height plus speed, with hysteresis and a minimum duration. For a kick, a contact
  releases as soon as the foot speeds up. For an NWN base or legs source, contacts come
  from where its feet stand still, as #84 does.
- **Capture habits** go in the README, not the tool:
  - fixed exposure and hard light;
  - every camera holding a steady 30 a second or more;
  - stand still at both ends;
  - full speed;
  - one camera side-on at hip height.

## What goes, and where it went

| Today | After |
|---|---|
| The Mocap rail (about 50 controls) | The inspector (the selection's settings) and the timeline |
| Frames-a-second box | Gone: the timestamps give the clock |
| Smoothing σ, Remove glitches | One steady-to-sharp control; outliers removed automatically |
| Standing and kept sliders | Spans dragged on the overview strip |
| Suggest standing and trim, Suggest feet | Automatic on opening; "re-suggest" in the take's inspector |
| Lock planted feet, the plant rows | Contact lanes (on by default) |
| Blend in/out, loop, stance, Show the stance | The clip's inspector, plus a "stance" camera preset |
| Returns / in place, keep the sway | One setting in the clip's inspector |
| Fit to a length, contact at, fastest hands | The time track: the length, and where the hit lands |
| Key tolerance | Fixed in the code |
| The mix panel (#84) | Source blocks on the timeline, with their inspector |
| Pose panel and three sliders (#83) | Dragging on the doll; exact numbers in the inspector |
| Export points for Blender, re-import, the Blender add-on | Retired; deleting them needs the owner's OK |
| localStorage sessions | Project files |
| Library chips, Copy text, the recipe text box | Gone |
| The Recipe tab (base, mix toward, amount, speed, keep, crouch, lean, holding) | Folded in: a project can start from an NWN animation. Mix toward is a whole-body source with a weight; speed is the time track; keep is the kept span. Crouch and lean are clip settings. Holding (the weapon-layer preview) stays a preview toggle |
| Four copies of the math, two leg solvers, two foot-still detectors | One of each: the C++ core for everything after the solve, one JS math module for the bench's own work |
| The DLL's library export to `build\animedit` | Gone: the bench reads the libraries from the install |
| `!animedit` (the in-game recipe window) | Retired: the bench does it |
| `!anim`'s browser, `!animtrace`, the VFX editor plumbing (in the shipped DLL) | The dev add-on in `nwn5e-tools`, on the owner's machine only |

Code that carries over, re-read and tidied rather than rewritten:

- **Bench, JavaScript:** the library reader, the take readers (RTMPose, MediaPipe), the
  solve, the knee rule, the legs' neutral and rough-capture repair.
- **C++ core:** the halves, pose values, the leg solver, the crouch step and the balance
  move in. The crouch step and the formula recipes' code are already there.

## Build order

Each phase is a sub-issue of one epic. Each has its own checks, deliberate breaks,
headless-browser check, test matrix and the owner's test. The new bench lives beside the
old one in `nwn5e-tools` (`mocap_bench/`, `mocap_bench.bat`, port 8766) until it does
everything the owner uses. Only then, with his OK, is the old one deleted.

1. **The split and the shared core.** *(DLL and tools.)*
   - **The split:**
     - the dev-only parts leave the shipped DLL for the dev add-on, and `!animedit` is
       retired;
     - the tools read the game's files from the install;
     - the VFX bench keeps working throughout: its checks pass, and play-in-game works
       through the add-on;
     - a check that the shipped DLL registers no dev commands and writes no exports.
   - **The assembled recipe:**
     - the format, its reader, and steps 1, 6 and 9: the capture composed onto the named
       stance, the root, the blends and the loop;
     - the helper program and the build stamp;
     - test.bat checks, including a recipe round trip.
   - **Acceptance:**
     - one of the owner's takes, converted, plays in game the same as today's baked keys
       recipe of it, within the key tolerance;
     - the VFX bench works as before.
   - *Built 0.58.0 (#87).* The dev layer is `nwn5e-tools/dev` behind `g_dev_seam`; the helper
     is `nwn5e-tools/anim_helper`; the assembled recipe is `src/data/anim_assemble`. A take
     converted both ways (`recipe_bench/convert_assembled.js` / `compare_assembled.js`)
     matches today's keys recipe within 0.84° / 1.1 mm on all eight bodies, fitted or not.
2. **The bench's foundation.**
   - The server: recordings, projects, the helper.
   - The project model, with undo and redo.
   - Clean-up v2 with checks; the solve carried over.
   - The stage, with the ghost and the points on the doll's proportions.
   - The timeline: overview, moments, contacts.
   - The inspector; Save.
   - Acceptance, in two parts. *(Builder's call.)*
     - Fed today's cleaned frames and plants, the new bench's result must match today's
       within the key tolerance. Today's clean-up stays in the checks as that reference.
     - Clean-up v2 changes the result on purpose, so it is judged by its own checks
       (spike vs strike, gaps, swaps, cutoffs) and by the owner looking at the same take
       in both benches side by side.
3. **IK.** *(Core and bench.)* Targets and pins for hands, feet and hips, dragged on the
   doll. Reach eased in and out, poles, extension damping, the hips sinking.
4. **Poses.** *(Core and bench.)* Turning a part by dragging; push and replace; the ease;
   numbers in the inspector.
5. **Timing.** *(Core and bench.)* First settle the engine facts below. Then:
   - time keys;
   - hold at the hit, with a slight drift;
   - speed a stretch;
   - the length and where the hit lands;
   - removing dead frames.
6. **Sources and NWN bases.** *(Core and bench.)*
   - The legs, torso and whole-body blocks with weights; #84 carried over.
   - The balance.
   - Projects starting from an NWN animation; crouch and lean.
   - Opening a formula recipe as a project.
7. **Retire.** With the owner's OK:
   - delete the old bench and the Blender add-on;
   - the README;
   - switch the `.bat`.

   The owner's saved keys and formula recipes keep working in game.

**Later, not in this plan:**

- hands held to the body (a chin guard, hands on hips: a hand stored relative to the body
  part it touches);
- reducing a take to a few editable poses;
- a loop finder;
- an "exaggerate" control;
- quality readouts per clip.

## Engine facts to settle before the timing phase

The research flags these. Each is a small check in game or in the engine, not a design
choice.

- **transtime:** what it blends. If it's the crossfade into the clip, a short attack needs
  a shorter one.
- **Root movement:** whether movement on rootdummy moves the creature or plays in place.
- **Key interpolation:** confirm the engine interpolates keys by nlerp.
- **Slot timing:** whether a slot's trigger needs the hit at a set time. For example,
  n5_shove replaces nwstab's swing, and the game resolves the shove on its own clock. If
  so, the slot presets the hit's landing time and the length.
- **Startup cost:** the time added when every recipe is assembled for eight bodies.

## Checks

- **The C++ core (test.bat).** Each step on fixtures, including:
  - the capture composed onto a stance and back;
  - pins holding;
  - knees never backward;
  - the time track monotone with holds;
  - sources splicing with the root turn carried;
  - the balance;
  - reach easing;
  - crouch and lean as today.
- **The bench (Node).** Carry over the mocap_check tests that still apply: retarget round
  trips, RTMPose, knees, the legs' neutral. Add:
  - clean-up (spike vs strike, gaps, swaps, cutoffs against the frame rate);
  - undo;
  - a project round trip;
  - a recipe that ships: written, read back by the core, no NWN keys in it;
  - the helper's build stamp.
- **Browser checks (headless Chrome).** They go into the repo
  (`nwn5e-tools/mocap_bench/browser_check.mjs`) so every phase reruns them.
- **The split.** The shipped DLL has no dev commands and writes no exports. The dev
  add-on, when present, brings them back. The VFX bench's checks still pass.
- **Deliberate breaks** for every check, each one caught.

## What the builder decides

The code structure inside these lines, and the exact numbers (thresholds, ease shapes,
damping). Also:

- the drawing details, and shortcut keys beyond the ones above;
- the helper's protocol (a long-running process on stdin/stdout is the expected shape);
- the recipe's text layout.

And these specific checks and decisions:

- **Unfiltered points:** check whether this FreeMoCap version can hand over unfiltered
  points. `output_data` holds only filtered CSVs. If it can't, FreeMoCap's own smoothing
  could be turned off in its settings so the bench filters at the true rate.
- **No import of old work:** old browser sessions and Blender-edited points files aren't
  imported. The old bench stays until the owner agrees to delete it, so nothing is lost.
- **Who builds the DLL parts:** game work goes on in the same repo, so the DLL phases are
  planned and committed like any game issue, with version bumps.
- **How the dev add-on gets into the game:** for example, nwn5e loads it only when the
  file sits beside it, and the shipped build has no such file. It needs the same hooks
  the DLL uses.
- **Reading the install:** finding the game install (the Steam path by default,
  overridable), and reading the body libraries and effects from its data archives.

## Open questions

None left for the owner. *(The Recipe tab: folded in, because recipes must ship:
2026-10-30.)*
