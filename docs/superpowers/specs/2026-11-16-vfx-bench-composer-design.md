# The VFX Bench's Composer: building the Floating Disk's piles

The design for a Composer mode in nwn5e-tools' VFX Bench, where the owner builds the Floating Disk's four looks (#39) by
hand. Written 2026-11-16, after the disk's phase 2 landed (anim b9e4fab, 0906eb3, 3d49e05; merged into main at 0516247,
installed with 0.99.56).

## Intent and why

The owner, 2026-11-16, on the piles phase 1 and 2 composed from the game's placeables:

- "those piles aren't bad, you can use them for now, but I'll create new ones"
- "can I build the piles in one of the benches?"

The proposal he answered with **"go, after phase 2 lands"**:

> The **VFX Bench** is the natural place for it. Since VF-3 its stage draws real game models with their textures, beside
> the figure for scale, and it already has undo and a library panel. A **Composer** mode there would work like this:
> - **Pick parts** from a searchable list of the game's placeable and item models (bags, crates, barrels, books, bones,
>   weapons...), with a thumbnail of each. Your own `.mdl` files would be in the list too.
> - **Place them** on the disk by dragging, with handles to turn and scale each part and to stack them. Snap to the
>   disk's top or to the part below.
> - **Four stage tabs** (empty, small, medium, large), with an onion-skin view of the previous stage so the piles grow.
> - **Save** writes `composition.json` and runs the generator. The disk models, the containers and the rows rebuild,
>   ready for the next install.
>
> Limits: DDS-only and env-mapped textures draw flat on the bench; the bench's lighting is simpler than the game's, so
> the final check is always in game. Effort: about one to two sessions.

Where things stand:

- **The generator** (nwn5e-ee `tools\data\floatdisk\`): `composition.json` lists each pile's parts (a game model or the
  owner's own `.mdl`, with `at`, `turn`, `scale`, `tilt`, `note`). `build.js` reads each part through `mdlmesh` (the
  repo's own model reader, `src\data\vfx_model.cpp`), places it with `floatdisk.js`'s pure maths and writes 13 files: the
  four creature models, the three pile placeables and their `.pwk`, the disk's texture, and the two rows files. It runs
  in 1.3 s and refuses any file it didn't make.
- **Editing it today** means typing numbers into `composition.json`, rebuilding, and looking at screenshots made by a
  script in the job's tmp folder (`shoot2.mjs`). There is no way to see a change while making it.
- **The VFX Bench** draws a chunk model's real meshes with their textures and materials (012dc3a, e6c132c), beside the
  2 m figure. It has the shared frame, undo (`common/history.js`) and a library drawer. But it draws only what the
  library holds: the parts' models and textures aren't in it (phase 1 report, "What the bench drew badly").

## Goals

1. **See the pile while building it**, on the bench's stage, beside the figure, placed exactly as the generator will
   place it.
2. **Pick parts from the game** (placeables and items) and from the owner's own `.mdl` files, by a search with
   thumbnails.
3. **Place, turn, scale and stack** parts with handles on the stage. A part rests on the disk or on the part below it.
4. **Four stage tabs** with the previous stage shown as a see-through ghost, so each pile grows from the last.
5. **Save and rebuild in one press**: `composition.json` written and the generator run, its result shown.
6. **Undo** for every change, as in the rest of the bench.

## Non-goals

- Changing what the generator makes. The models' layout, the rows, the disk's mesh and the files' names stay as phase 2
  made them. Only how the generator is fed and how it reports change.
- Editing a part's own mesh or texture: that's Blender's job (the owner's `.mdl` files).
- Other compositions than the Floating Disk's. The mode opens `tools\data\floatdisk\composition.json`.
- The game's lighting, environment maps and specular on the bench.
- Committing, merging or installing from the bench.
- A hover motion for the disk (phase 2's trade-off stands).

## Anti-goals (a failure even if it works)

- The stage placing a part anywhere other than where the generator puts it.
- Save leaving `composition.json` and the built files out of step: a failed build leaves both as they were.
- Writing anything outside the chosen checkout's `composition.json`, the generator's own marked files and the bench's
  own cache. Nothing in the main checkout (`nwn5e-ee`) unless the owner points the bench there.
- The effect mode changing in any way the owner would notice.
- Losing work silently: leaving the Composer or the page with unsaved changes asks first, and Save refuses a
  `composition.json` that changed on disk since it was opened.

## Constraints

- **Code:** plain browser JavaScript in UMD modules, no build step, no framework, no new npm dependency, nothing to
  install. Node 18 as now.
- **The frame:** the shared frame's rules hold (the page never scrolls, the stage stays in view, at least 1100 x 700).
- **Two repos:**
  - the generator and its docs change on nwn5e-ee branch `anim` (the worktree `nwn5e-ee-anim`);
  - the bench changes on nwn5e-tools `main`.
- **Game files are never committed.** The thumbnails cached from the game's models live in the bench's build folder
  (`build\vfxedit\composer\`), which git ignores.
- **`spell_bench\game_data.js`** (the game's KEY/BIF reader) is used as it is, as build.js already uses it. Another job is
  editing the Spell & Attack Bench: the Composer only calls `resources`, `resourceNames` and `gameTable`.

## Approaches

1. **(Picked) A Composer mode in the VFX Bench, fed by the generator's own code.**
   - The bench's server reads each part through the generator's reader (mdlmesh, moved into a `reader.js` the
     generator also uses). The page places parts with the generator's own `floatdisk.js`. Save runs the generator.
   - For: one reader and one placement for the stage and the files, so the stage can't drift from what gets built. It
     reuses the stage, the figure, the frame, undo and the drawer. It's what the owner approved.
   - Against: the bench now loads code from an nwn5e-ee checkout, so the two repos must stay in step (the server's
     stale check covers it).
2. **A separate Pile Bench** with its own server and page.
   - For: no change to the VFX Bench.
   - Against: a second stage, frame wiring, figure and undo to keep in step. The owner asked for "one of the benches".
3. **Composing in the game** through dev-layer chat commands that place parts on a disk.
   - For: the real look at every step.
   - Against: no thumbnails, no undo, slow round trips, and engine work behind the seam.

Two routes for the meshes were weighed and rejected inside approach 1:

- **A JavaScript port of the binary model reader** (floatdisk.js already reads a compiled model's node tree): a second
  reader to keep in step with vfx_model.cpp, and the bench might show a mesh the generator reads differently.
- **The dev layer copying every placeable into the bench's library** (as it copies the chunk models): it needs the game
  run, a library format change, and ~1,300 models copied on every export.

## What the owner sees

```
+-----------------------------------------------------------------------------------------------------------------+
| VFX Bench [Open]  Composer: Floating Disk (#39)  [Save and rebuild] [Undo] [Redo] [Back to effects]  status (o) |
+--------------------+------------------------------------------------------------+-------------------------------+
| PARTS              | [Empty] [Small 2] [Medium 7] [Large 12]                    | PART: LootBag2                |
| Placeables | Items |                                                            |  plc_b05 - 0.32 x 0.32 x 0.36 |
|  | Yours           |                                 o  <- lift                 |  67 triangles                 |
| [bag             ] |              (ghost of Small)  /|\                         |  Note [a big sack          ]  |
| +----+ LootBag1    |                .-----------.  [###]                        |  x [ 0.19] y [-0.08]          |
| |    | plc_b04     |     ___......-'  the pile  '-......___    figure (2 m)     |  Height [0   ] [Drop it]      |
| +----+             |    (_____________ the disk ____________)         O         |  Turn [-25] Scale [0.48]      |
| +----+ LootBag2    |          `--- turn ring ---o scale              /|\        |  Tilt x [0] y [0]             |
| |    | plc_b05     |                                                 / \        |  [Duplicate] [Remove]         |
| +----+             |                                                            +-------------------------------+
| ...                |                                                            | MEDIUM (33-66%): 7 parts,     |
|                    |                                                            | 615 triangles, 0.36 m tall,   |
| Your files:        |                                                            | its top 1.26 m up. Past the   |
| tools\data\float-  |                                                            | rim: none. [Copy Small's in]  |
| disk\parts [Reload]|                                                            | Saving to nwn5e-ee-anim (anim)|
+--------------------+------------------------------------------------------------+-------------------------------+
| [x] Figure  Backdrop [Dark] [x] Onion skin (Small)  [x] Rests on what's below  View: [3/4] [Side] [Top] [Front] |
| On Medium: [crate] [big sack] [small bag] [bedroll] [book] [coins] [bottle]                                     |
+-----------------------------------------------------------------------------------------------------------------+
```

**Opening it.** A **Composer** button in the bar, beside New and Duplicate, opens the composition (`?compose=1` does the
same for the checks). The bar keeps Undo, Redo and the status. The effect's own fields (Name, Type, Seconds, Play in
game, Stop, New, Duplicate) step aside, Save becomes **Save and rebuild**, and **Back to effects** returns to the effect
as it was. Unsaved changes are asked about first.

**The stage.** The disk is at the middle of the ground grid, at its height (0.9 m), and the 2 m figure stands 0.85 m to
its left and 0.15 m behind, facing +y, as in phase 1's pictures. The disk's front is +y. Drag on empty space to turn the
view, right-drag to move it, wheel to zoom (down to 0.6 m in the Composer), double-click to reset. The view buttons are
phase 1's 3/4 and side cameras, plus top and front.

**The stage tabs** (along the stage's top): Empty, Small, Medium, Large, each with its part count. The selected stage's
parts are drawn as they'll be built. With **Onion skin** on (the default), the previous stage's parts show as pale
see-through ghosts, so Medium is built over Small and Large over Medium. **Copy Small's parts in** (on Medium; Medium's
on Large) starts a bigger pile from the smaller one.

**The Empty tab is the disk.** Its card holds the disk's diameter, thickness, height, sides, colour, rim colour,
see-through (alpha) and glow (selfillum). The disk is the same on every stage. Its texture's name stays
`n5t_forcedisk`.

**The parts list** (the drawer):

- **Placeables:** every model `placeables.2da` names that the game has (1,268), by the row's label and the model's
  name ("LootBag1 · plc_b04").
- **Items:** what a dropped item is drawn with:
  - the weapons, each as its three pieces (726: "longsword 011", the pieces `wswls_b_011`, `_m_011`, `_t_011`);
  - the shields and other single-model items (about 130);
  - the helmets (35);
  - the loose item models (`it_*`, and `gi_*` but the markers).
  Armour isn't there: the game draws it on a body. The body-bag armour placeables (`gi_armor01-04`) are in Placeables.
- **Yours:** the `.mdl` files in `tools\data\floatdisk\parts\` of the checkout, with **Reload**. The note under it says
  where the folder is and points to README.md's units and axes.

The search box matches the label, the model's name and the kind. Each entry has a thumbnail, drawn the first time it
scrolls into view and kept for next time. Click an entry to add it on the disk's centre, resting on whatever is there,
and selected; or drag it onto the stage to put it where it lands.

**Handling a part.** Click a part to select it: its box is outlined and its handles appear.

- **Drag the part itself** to move it across the disk.
- **The ring** round its foot turns it. Shift turns in 15 degree steps.
- **The square on the ring** scales it.
- **The arrow above it** lifts or lowers it.

With **Rests on what's below** on (the default), a part moved or added comes to rest on the highest thing under it: the
disk's top or another part. That's how parts stack. **Drop it** rests the selected part again after a lift. With it off,
a move keeps the part's height.

The part's card has every field as a number: its place (x, y, height), turn, scale and tilt (about x and y), and its note
(which composition.json keeps). It also holds **Duplicate** (Ctrl+D) and **Remove** (Delete). The arrow keys nudge the
selected part 1 cm, Esc deselects. A part that reaches past the disk's rim lights the rim amber, and the stage card says by
how much, as the generator's summary does.

**The parts strip** under the stage lists the stage's parts in order, by note, so a small part hidden under another can
be clicked.

**Save and rebuild** writes `composition.json` and runs the generator in the checkout, then shows its summary in the
status line ("c_n5fd_med.mdl, plc_n5fd_med.mdl rebuilt, 11 unchanged") and in the Composer card. If the generator
refuses (a model the game doesn't have, a texture it can't find), its message is shown, and neither `composition.json`
nor any built file changes.

**For the game to show it:**

1. Save and rebuild in the bench. The checkout's `composition.json` and its rebuilt files now differ from git.
2. Tell the animation session the piles are saved. It commits them on `anim` and sends "anim ready at <commit>".
3. The game session merges `anim` and installs when the game is closed. The next game start shows the new piles.

**Undo** covers every change in the Composer: a drag is one step, a field is one step when left, an add or a remove is
one step. Undoing a change made on another stage shows that stage. Opening or leaving the Composer starts the history
again, and Save isn't undone.

## How it works

**The generator** (nwn5e-ee `tools\data\floatdisk\`):

- **`floatdisk.js`** loads in a page as well as in Node (`window.FloatDisk`): the same `placePart`, `diskMesh` and
  `diskTexture` for the stage and the files. `diskTexture` builds its bytes without Node's Buffer and gives a Buffer
  only under Node.
- A part's **`model`** may be a list: a weapon's pieces, read as one model.
- **`compositionText(c)`** writes composition.json in its own layout (a part a line), so a Save without changes leaves
  the file as it is. The one exception is the `0.0` in the large stage, written `0`.
- **`reader.js`** (new) holds what build.js did to read a part and find a texture (`ensureReader`, `readPart`,
  `findTexture`). build.js uses it, and so does the bench's server.
- **`build.js --json`** prints its result as one JSON line (the stages' summary and each file's state) for the bench.
  Without it, build.js prints what it does today.

**The bench's server** (`vfx_bench/composer_server.js`, routes under `/api/composer/`):

- It finds the checkout and reports its folder, branch and composition.
- It lists the parts (the catalog), built once per run from the game's tables and archives.
- It reads a part's meshes through `reader.js`, in the model's own space, kept in memory per model (an own file by its
  path and time).
- It serves a part's textures from the game (or from beside an own file), with what the page needs to draw each one.
- It keeps the thumbnails (`build\vfxedit\composer\thumbs\<key>.png`) and serves the checkout's `floatdisk.js` to the
  page.
- **Save** checks the composition with `readComposition`, writes it to a temporary file, and runs `node build.js --json
  --composition <temp>` in the checkout (spawned: the checkout's current code, its own refusals). Only when that
  succeeds does it replace `composition.json`.
- The checkout's `floatdisk.js` and `reader.js` join the server's stale-code list: a changed generator asks for a bench
  restart, as a changed bench does.

**The page:**

- **`public/composer.js`** (pure, Node-checked) holds:
  - the composition in the page;
  - each stage's meshes, placed by `FloatDisk.placePart`;
  - the stage's summary;
  - picking a part under the pointer;
  - resting a part on what's below;
  - the handles' maths;
  - the snapshot for undo.
- **`public/composer_page.js`** is the mode itself: the drawer, the cards, the tabs, the strip and the stage's pointer.
- **`view.js`** draws the parts and the disk through the chunk models' path: textured and shaded, untextured in their
  material, see-through blended. It also gains the stage's pointer handlers (a grab takes the drag, as the Mocap Bench's
  handles do) and a ray from the pointer.

## Calls I made

1. **The meshes come from the generator's own reader, through the bench's server.** build.js's part reading moves into
   `reader.js`, which build.js and the bench's server both require. Each model is read once per server run and kept in
   memory. *Why:* one reader means the stage shows the meshes the files get, including mdlmesh's handling of render 0,
   parents and the owner's ASCII models. It's quick: `build.js --check` reads today's 14 different models, c_invsguy
   and two tables in 1.3 s.
2. **The page places parts with the generator's own `floatdisk.js`**, served from the checkout and made loadable in a
   page. *Why:* the footprint centring, the lowest point on the disk, turn after tilt, the rounding: the same code, so
   "what you see is what's built" holds by construction. A check compares the page's summary with `build.js --json` for
   the real composition.
3. **The part list:** placeables by their `placeables.2da` rows; items as the models a dropped item is drawn with;
   yours from `tools\data\floatdisk\parts\`, the folder README.md already names. *Why:* the placeables table is the
   game's own list of things to put in a world, and all of phase 1's parts are in it (the dag_ bottles and pots, the
   tnp_ barrel, the gi_ treasure and armour are placeable rows). Armour is drawn on a body, so it isn't an item part.
4. **A weapon is one part made of its three pieces.** composition.json's `model` may be a list
   (`["wswls_b_011", "wswls_m_011", "wswls_t_011"]`). The list offers each number all three pieces share, and a weapon
   is added lying flat. *Why:* placed one by one, each piece's footprint would be centred on its own `at` and the
   weapon would come apart. The pieces share an origin, as the game assembles them (to be confirmed in Task 2's check).
5. **Thumbnails are drawn once by the page and kept by the server** as PNGs in `build\vfxedit\composer\thumbs\`, drawn
   lazily as entries scroll into view, four at a time. *Why:* about 2,200 entries can't all be drawn live, and a server
   can't draw without a browser or an install. The build folder is git-ignored, so pictures of game models are never
   committed.
6. **Textures:**
   - The game's TGA is served by name, `ovr\` first as the game does. A texture beside the owner's file comes next.
   - A texture's alpha cuts out where clear (the chunk path's 0.2), except a texture whose TXI names an environment map.
     There the alpha is shine (`plc_gold`'s TXI is one), so it draws opaque, its colour without the shine.
   - A texture with no TGA (DDS-only, PLT) draws flat, in the mesh's material. The status says which.
   - *Why:* the approved limits, made concrete with what the game files show. Q1 asks whether to go further.
7. **Save writes into the anim worktree.** The bench finds it by `--ee <checkout>`, else `..\nwn5e-ee-anim` beside
   nwn5e-tools when it holds the composition, else `..\nwn5e-ee`. The Composer card names the folder and its branch (read
   from the worktree's HEAD, no git command) and warns when the branch isn't `anim`. *Why:*
   - the composition and the generator are committed on `anim`, and this line owns the tools;
   - the main checkout is the game session's working copy, and files written there would sit uncommitted in its tree;
   - the generator touches only its marked files and `composition.json`, so the animation session's other work in the
     worktree is safe.
8. **The bench never runs git.** After Save, the status says what's next: the animation session commits, the game
   session merges and installs. *Why:* the project's commit rules (one command a call, `--only`, sessions own their
   trees) belong to the sessions, not to a button.
9. **Save is all or nothing, and checks the file first.**
   - The page sends the hash of the `composition.json` text it opened. If the file changed on disk since, Save refuses
     and offers Reload or Save anyway.
   - The composition goes to a temporary file, and the generator runs on that. `composition.json` is replaced only after
     the generator succeeded (it already checks every file before writing any).
   - *Why:* the anim session (or a merge) may change the file while the bench is open, and a refused build must not
     leave the composition saying something the files don't.
10. **The handles follow the Mocap Bench's pattern.** A grab on a handle or on the selected part takes the drag; anywhere
    else the drag turns the view. The handles:
    - the part's body moves it on the plane of its foot;
    - the ring turns it about its footprint's centre (which is `at`);
    - the square scales it by the change in distance from that centre;
    - the arrow lifts it along the vertical through its centre.

    Tilt is in the card only. *Why:* the four handles cover what the owner named (place, turn, scale, stack). Tilt is
    rare: phase 1 used none. Picking, the ray and the handles' maths are pure functions, checked in Node.
11. **Resting on what's below** is on by default. A moved, added or dropped part's height is found from its own
    vertices and centres against the other parts' triangles from above: the highest surface under any of them, or the
    disk's top. *Why:* it is what "snap to the disk's top or to the part below" means for a heap. Phase 1's stacked parts
    were placed by hand with such heights (`at` z 0.31 on the barrel). A point of another part poking up between the
    moving part's vertices can be missed by up to that gap; the arrow handle fixes it.
12. **The empty stage edits the disk**: diameter, thickness, height, sides, colour, rim, alpha and glow, and its texture
    shows the change live (`diskTexture` in the page). The texture's name isn't offered. *Why:* the empty stage has
    nothing else, and the disk is in composition.json. A new texture name would leave the old file behind in made.txt
    for nothing.
13. **A new part** starts at the disk's centre (or where it's dropped from the list), resting, at a scale that keeps its
    footprint's longer side within 0.35 m (never above 1). Its note is the list's label. A weapon starts lying flat.
    *Why:* most placeables are far bigger than a disk (a crate is 0.75 m), and phase 1 used scales of 0.26 to 0.85. The
    note keeps composition.json readable.
14. **Rounding:** places to 1 mm, turns and tilts to 0.1 degree, scales to 0.001. *Why:* a drag makes numbers like
    0.15000000000000002, which would make composition.json unreadable and every save a diff.
15. **Undo snapshots hold the composition only**, not the tab. Undoing or redoing a change made on another stage
    switches to that stage. *Why:* a tab switch isn't an edit, but an undo the owner can't see would look like nothing
    happened.
16. **No copy of an unsaved composition in the browser's storage**, unlike the effect mode's draft. *Why:* the file is
    the truth, and a stale browser copy of it would compete with a newer file. Leaving or reloading with unsaved changes
    asks instead.
17. **The generator's changes are kept small and checked to change nothing it writes today:**
    - `floatdisk.js` loads in a page;
    - model lists;
    - `compositionText`;
    - `reader.js`;
    - `--json`.

    `build.js --check` prints the same before and after, and a full run on a copy of the committed files reports all 13
    files unchanged.

## Open questions (only the owner can answer)

- **Q1. Should the bench read DDS textures?** I recommend yes.
  - **Today:** the approved plan draws a DDS-only texture flat. That includes your large pile's barrel (`tno01_post`).
  - **What the game files show:** EE ships most part textures as both TGA and DDS, and the DDS is often the bigger one.
    `plc_gold`'s TGA is 8 x 8 against a 64 x 64 DDS; `gi_000`'s is 64 against 256; `tic01_books01`'s 128 against 512.
    So the bench would show some parts blurrier than the game does.
  - **The cost:** reading DDS (Bioware's DXT1 / DXT5) is about 80 lines and one more task (Task 11), without changing
    anything else.
- **Q2. A Play in game for a stage?** I recommend yes, if you want quicker in-game looks.
  - **What it would do:** the bench sends the stage's pile to the running game as an effect beside your character,
    through the dev layer's Play in game. You'd see the real textures, lighting and the disk's transparency without the
    commit, merge and install round. The effect writer already carries meshes.
  - **The cost:** one more task (Task 12) and a first check in the game. It isn't part of what you approved; without it,
    the in-game look comes after the install.

## Limits the owner should expect

- **Lighting:** the stage shades each face by one soft light from above (the chunk path's). There's no game lighting,
  specular, environment map or self-illumination on textured meshes. The disk shows its texture at its alpha, not its
  glow.
- **Flat textures:** DDS-only and PLT textures draw flat (unless Q1), and environment-mapped ones draw without their
  shine.
- **Render 0:** meshes mdlmesh reads as render 0 aren't drawn, on the bench or in the files. Phase 1 found some DAG models
  where that looks wrong (a bottle's cap).
- **The final check is in game**, after the install (or Q2's Play in game).

## Checks

- **The generator's Node check** (`tools\data\floatdisk\check.js`, no game needed):
  - `floatdisk.js` loads without `require`;
  - the disk's texture is the same bytes in a page;
  - a part's model may be a list;
  - `compositionText` gives back the committed file;
  - the pieces of a list are read as one model.
- **The generator against the game** (`tools\data\floatdisk\check_build.js`, new; it works on a temporary copy):
  - `build.js --check` output identical before and after;
  - `--json` parses and matches the text;
  - a full run on a temporary copy of the committed files reports 13 unchanged;
  - a weapon composition builds.
- **The page's pure parts** (`vfx_bench/check_composer.js`):
  - the composition in and out;
  - placement equal to `placePart`'s;
  - the stage's summary;
  - picking;
  - resting on a part and on the disk;
  - the four handles' maths;
  - lying a weapon flat;
  - the new part's scale;
  - the ghost list;
  - rounding;
  - the stage an undo shows;
  - the ray and the projection round trip.
- **The server** (`vfx_bench/check_composer_server.js`, against the game, on a temporary copy of the checkout, never the
  real worktree):
  - the checkout and its branch;
  - the catalog's kinds;
  - a part's meshes equal `reader.js`'s;
  - a weapon's three pieces;
  - an own file;
  - textures and their flags;
  - the thumbnail cache;
  - Save writes and rebuilds;
  - a refused build changes nothing;
  - a changed file is refused;
  - a foreign page is refused;
  - the stale guard;
  - the page's summary equals `build.js --json`'s.
- **The browser** (`vfx_bench/composer_check.mjs`, headless Chrome on a temporary build and checkout):
  - the mode opens and draws Medium's parts textured;
  - tabs and onion skin;
  - the disk card;
  - pick, move, rest on a part, turn, scale and lift;
  - adding from the list (by click and by drag);
  - Delete, Ctrl+D and the nudges;
  - undo and redo across stages;
  - Save;
  - Back to effects;
  - no page scroll;
  - no page errors.
- **The existing checks stay green:** `check_chunks.js` (the shared mesh path), `browser_check.mjs` (the effect mode
  untouched), `check.js`, `check_area.js`, `check_parts.js`, `check_effect.js`, `check_beats.js`, and
  `common/checks/run.js`.

## Build order

1. The generator: `floatdisk.js` in a page, model lists, `compositionText`.
2. The generator: `reader.js`, `build.js --json`.
3. The server's Composer routes: the checkout, the catalog, parts, textures, thumbnails.
4. The server's Save.
5. The page's pure parts (`public/composer.js`).
6. The stage: the shared mesh path, the pointer handlers, the ray.
7. The mode: the frame's parts, the tabs, the onion skin, the disk card, the part card, undo.
8. The parts list and thumbnails.
9. The handles.
10. Save in the page.
11. (Q1) DDS textures.
12. (Q2) Play in game for a stage.
13. The READMEs, TOOLS-PLAN and the owner's tests.

Each step leaves the bench and the generator working with their checks green. **Effort:** about two sessions for tasks
1-10 and 13; Q1 and Q2 add a few hours each.

---

## Addendum 2026-11-16: a general Composer

The owner reviewed this design and changed its scope. Where this addendum and the sections above disagree, the
addendum wins. The sections above stay as they were written.

### The owner's words, in order

1. "let's do something here, instead of buiding something specific for the disk, let's build something more versatile
   that will allow other things to be built. The disk doesn't need to be rendered. at most a circle, other shapes can be
   used as needed. Regarding your questions, sure, yes to both"
2. Asked what the other things should become in game: "I think placeables and meshes for spells"
3. Choosing between a mode in the VFX Bench (A) and a separate Model Bench (B), and approving the outputs below: "go
   with A and those outputs"

### What changes

- **A project, not the disk's composition.**
  - Parts come from the game's placeables and items and from the owner's `.mdl` files, as before.
  - They are placed, turned, scaled and stacked as before.
- **Variants, not stages.** A project holds one or more versions of a model. The onion skin shows the previous variant,
  and **Copy the previous one in** starts from it.
- **A guide on the ground:**
  - a circle, a rectangle or none, sized by numbers, at a height;
  - drawn as an outline, never part of the model;
  - parts reaching past it light it amber, as the rim did.

  "Other shapes" are guide outlines, not solid shapes to build with (the coordinator's reading, which the owner didn't
  correct).
- **No disk is drawn.** The Floating Disk is one project: a 0.9 m circle guide at 0.9 m with four variants. Its
  generator still adds the disk mesh and writes the creature looks, the piles and the rows. build.js and its 13 files
  are unchanged.
- **Q1 and Q2 are yes.**
  - DDS textures are read.
  - **Play in game** shows the selected variant of any kind beside the character.
- **Approach A stands:** the Composer is a mode of the VFX Bench.

Superseded above:

- the four fixed stage tabs (now the project's variants);
- the Empty tab's disk card (now the guide card; the disk's look stays in composition.json);
- the rim drawn on the disk (now the guide's outline);
- the open questions (both answered yes);
- "Other compositions than the Floating Disk's" under non-goals.

Everything else stands: the reader, the placement, the parts list, thumbnails, handles, resting, Save's all-or-nothing,
undo and the checks.

### What the owner sees

```
+-----------------------------------------------------------------------------------------------------------------+
| VFX Bench [Open]  Composer: [crates (placeable) v] [New project]  [Save and build] [Play in game] [Stop] [Undo] |
| [Redo] [Back to effects]   status ...                                                                    (o)    |
+--------------------+------------------------------------------------------------+-------------------------------+
| PARTS              | [1] [2] [3] [+ variant]                                    | GUIDE                         |
| Placeables | Items |                                                            |  Shape [Circle v]             |
|  | Yours           |                    .------------.                          |  Diameter [1.2]  Height [0]   |
| [crate           ] |                 .-'   the parts  '-.                       +-------------------------------+
| +----+ Box1        |                (    ( guide circle ) )                     | PLACEABLE                     |
| |    | plc_a03     |                 '-.______________.-'                       |  plc_n5crates_2, row 14101    |
| +----+             |                         x  <- use point     O              |  Copies row 2 (Box1)          |
| ...                |                                            /|\  figure     |  Use point x [0] y [-0.7]     |
|                    |                                            / \  at the use |  (o) Walk-through ( ) Blocks  |
|                    |                                                 point      +-------------------------------+
|                    |                                                            | PART: Box1 (card as before)   |
+--------------------+------------------------------------------------------------+-------------------------------+
| [x] Figure  Backdrop [Dark] [x] Onion skin (1)  [x] Rests on what's below  View: [3/4] [Side] [Top] [Front]     |
| On 2: [crate] [crate] [sack]                                                                                    |
+-----------------------------------------------------------------------------------------------------------------+
```

**Projects.**
- The bar's project list holds the Floating Disk (#39) and every project in the checkout's `tools\data\composer\`.
- **New project** asks for a name (up to 8 letters and digits) and a kind, Placeable or Spell mesh. It writes the file
  with one empty variant.
- Opening a project starts the undo history again, as opening an effect does.

**Variants.**
- The tabs along the stage's top are the project's variants. **+ variant** adds one; a variant's card renames or removes
  it.
- The Floating Disk's variants are fixed to its four stages. Its Empty takes no parts: the generator makes it, the disk
  alone.

**The guide card.**
- Its fields are the shape (circle, rectangle, none), its diameter (or width and length) and its height.
- The parts' heights are measured from the guide's height (the generator's `at` z), and resting stops on it.
- For the Floating Disk, the guide's diameter and height are `disk.diameter` and `disk.height`. The disk's other
  settings (colour, rim, see-through, glow, thickness, sides) stay in composition.json and aren't shown.

**The figure** stands where it helps:
- a placeable's figure stands on the use point, facing the placeable;
- the Floating Disk's and a spell mesh's figure stands 0.85 m to the left, as before.

**Save and build** builds per kind (below) and says what it wrote. **Play in game** sends the selected variant to the
running game beside the character (the dev layer's Play in game). **Stop in game** removes it.

### The kinds and what they build

**Placeable.** Each variant becomes one of the game's placeables.

- **The model:**
  - `data\files\plc_n5<name>.mdl`, or `plc_n5<name>_<k>.mdl` with more than one variant;
  - laid out as the game's junk placeables are (the pile models' layout: the parts' meshes, the `_hand`, `_head`,
    `_head_hit`, `_impact` and `_ground` nodes);
  - its base on the ground at the guide's height.
- **The walkmesh file** (`.pwk`, the game's `#MAXDOOR ASCII` as its own placeables have) holds:
  - **the use point**, where a creature walks to use it. It defaults to 0.1 m past the guide's front, and the owner can
    drag it on the stage or type it. A placeable's front is -y: the game's own use points and hands are there (PLC_A03's
    use point at y -0.91, PLC_B04's hand at -0.5).
  - **with Blocks,** a ground mesh of the guide's outline, faces material 7 (non-walkable), named `<MODEL>_wg`, as the
    game's crates and barrels block (PLC_A03, PLC_H07).
  - **with Walk-through** (the default), no ground mesh, as the game's junk has none.
- **The row:**
  - in `data\rows\placeables.<name>.txt`, one row a variant, in nwn5e's rows format;
  - each row is a copy of a game row with Label (`N5_<Name>_<k>`) and ModelName changed;
  - the copied row defaults to the first placeable part's own row (a pile of crates sounds like a crate), else 189
    (TreasureSml, as the disk's piles);
  - the card can name another.
- **Row numbers come from 14100-14199,** given at the first save and kept in the project after.
  - Checked free: in `data\rows` (only the disk's 14001-14003 now), in `data\looks` (no placeables rows), in the game's
    own table (1000-14999 are its users' blank rows) and in the main checkout's served table (14001-14003 only).
  - Checking every folder is the visualeffects lesson: a row taken in two files makes the launch check skip one of them
    whole.
- The files go to the checkout's `data\files` and `data\rows`. They reach the game as the disk's did: the animation
  session commits, the game session merges and installs. The UTP blueprint isn't ours.

**Spell mesh.** Each variant becomes a mesh for spells, `n5m_<name>[_<k>]`.

- **Where it's saved:** in the bench's `vfxedit\saved\` as a draft of its meshes. The game writes its model
  (`saved\n5m_<name>.mdl`) when it runs, as it does for every saved draft.
  - The saved folder is registered with the game as a resource folder (dev\vfx_editor.cpp, `RegisterFolder`), so the
    game finds the model by its name.
  - The bench lists it in `vfxedit\meshes.txt`, its own file.
  - Not the library: the export rewrites `library\chunks.txt` whole on every run (`vfx_editor.cpp`), so a mesh put there
    would drop out of the list.
  - Yours (the effects) doesn't list the meshes; the Composer does.
- **Use (a), as a chunk:** an emitter's **Chunk model** may name it.
  - The stage draws it through the chunk path: the server lists the meshes with the library's chunks and serves their
    drafts the same way.
  - In the game the emitter throws it, once the game has written its model.
- **Use (b), as a mesh beat** in an effect: **+ mesh** under the timeline (beside **+ light**) adds the mesh to the open
  effect.
  - Its row on the timeline and its card work like a light's:
    - where it hangs, its place, turn and scale;
    - Starts after, On for, Fade in, Fade out.
  - **Refresh from the mesh** brings in later edits of the mesh. The effect holds a copy of the mesh: an effect model
    must carry its own meshes.
- **The mesh's origin** is the guide's centre at its height. A spell mesh's guide may sit below 0, so a chunk can turn
  about its middle.

**Floating Disk.** Saved through its own generator as this design planned: composition.json written by
`compositionText`, then `build.js`.

### Can a mesh fade in an effect? Checked

- **The game can.**
  - 22 of the game's effect models key a mesh's alpha (controller 128, one value) in their animations, for example:
    - vdr_globemin, vdr_globemax and vdr_globe01;
    - vdr_magearmor2;
    - vdr_web;
    - vdr_stone01;
    - vdr_eleshield;
    - vdr_freemove.

    They key it in the impact, the duration and the cessation.
  - The game's chunk models fx_rockchunklg / med / sml fade their rock 1 to 0 in their impact.
  - Read from the game's own files, through the binary layout `vfx_model.cpp` uses.
- **nwn5e's writer can't yet:**
  - `WriteVfxModel` puts a plane in the geometry only, never in an animation;
  - `VfxPlane` has no keys;
  - the draft has no key lines for planes.
- **So the plan adds them** (Task A4, on anim):
  - a plane may carry alpha keys per animation (`<anim>_alpha = t a ...`, as a light's `<anim>_radius`);
  - the writer writes them as `alphakey` on the plane's node in that animation;
  - a plane also keeps, for the bench only, the mesh it came from, its scale, and its Starts after and On for.

  The bench makes the keys as it makes a light's (`parts.lightKeys`, with alpha in place of the radius).
- **Until the game session installs a DLL with Task A4,** a mesh beat shows for the whole effect in the game, from its
  start and with no fade. Its timing works on the bench at once.
  - Its scale is right either way: the bench writes the mesh's vertices already scaled, so an older DLL draws it at its
    size.
  - The DLL's reader skips plane keys it doesn't know.

### The project file

**Where projects live:** `<checkout>\tools\data\composer\<name>.json`, committed with what they build. The owner's own
`.mdl` parts for any project go in `tools\data\composer\parts\`. The Floating Disk's `tools\data\floatdisk\parts\` is
listed too.

**Kind `placeable`:**

```json
{
  "_about": ["..."],
  "kind": "placeable",
  "name": "crates",
  "guide": { "shape": "circle", "diameter": 1.2, "height": 0 },
  "placeable": { "copy": 2, "use": [0, -0.7], "walk": "blocks" },
  "variants": [
    { "name": "1", "model": "plc_n5crates_1", "row": 14100, "parts": [
      { "note": "a crate", "model": "plc_a03", "at": [0, 0, 0], "turn": 15, "scale": 0.5 }
    ] }
  ]
}
```

**Kind `spellmesh`:** the same, without `placeable` and `row`. Its models are `n5m_<name>[_<k>]`.

**The parts** are composition.json's (`note`, `model` (or a list: a weapon), `file`, `at`, `turn`, `scale`, `tilt`).
`at` z is measured from the guide's height. `guide.shape` is `circle` (`diameter`), `rect` (`width` along x, `length`
along y) or `none`.

**The Floating Disk stays in composition.json, read through an adapter in the page:**
- a circle guide from `disk.diameter` at `disk.height`;
- variants Empty, Small, Medium, Large from `stages`;
- written back by `compositionText` with only those changed.

*Why an adapter, not a migration:* the generator, its README, its checks and the owner's knowledge of composition.json
stay as they are, and build.js and its 13 files don't change.

### The generator for projects

`tools\data\composer\build.js <project.json>` with the same options and refusals as the disk's build.js (`--check`,
`--json`, `--tools`, `--game`, `--reader`, plus `--saved <folder>` for spell meshes).

- It reads parts through `reader.js` and places them with `floatdisk.js`'s `placePart`, so a project and the disk share
  one placement.
- Its pure parts are `tools\data\composer\project.js`. It loads in a page as `floatdisk.js` does: the bench uses the same
  outline, use point and model names. It holds:
  - reading a project;
  - the model names;
  - the free rows;
  - the guide's outline;
  - the use point;
  - the project file's text.
- It writes only files carrying its own mark line.
- `floatdisk.js` gains optional arguments (a mark, use points, a blocking outline) whose defaults leave the disk's 13
  files byte for byte (`check_build.js`).

### Calls I made (the addendum's)

1. **Projects live in the checkout, beside what they build** (`tools\data\composer\`). *Why:* a placeable's model and
   rows are committed; their source must be too, as composition.json is the disk's.
2. **The Floating Disk is read through an adapter, not migrated.** *Why:* the approved "keep build.js and its 13 files
   unchanged" holds trivially, and composition.json stays the file README.md documents.
3. **Spell meshes are saved in `vfxedit\saved\`, not the library.** *Why:* the export rewrites `library\chunks.txt` whole.
   The saved folder is already one the game reads models from by name, which is exactly what a chunk or a Play in game
   needs.
4. **A mesh beat is a group of the effect's planes, named as copies are** (`n5m_x`, `n5m_x2`... by beats.js), each
   carrying `mesh`, `scale`, `start_seconds`, `emit_seconds` and alpha keys. *Why:*
   - planes are the effect format's meshes already, and grouping by name is how beats already work;
   - the vertices are written scaled, so a DLL without A4 still draws the right size.
5. **A mesh beat's timing reuses the lights' key maker** (`lightKeys`), alpha for radius. With no fade out of its own, it
   goes out at the effect's end. *Why:* one rule for timed things; the owner already knows how lights behave.
6. **Placeable rows: 14100-14199,** given at the first save, kept in the project. *Why:* free in all four places checked,
   next to nwn5e's 14000s, clear of the game's BD_RESERVED rows (5000, 7001, 9002, 11003, 13004).
7. **A placeable's row copies a game row** (the first placeable part's, else 189), with Label and ModelName changed.
   *Why:* sound, shadow, reflection and the rest come from a real placeable of its kind. The UTP isn't ours.
8. **Blocks makes a 16-sided outline for a circle guide** (4 corners for a rectangle). *Why:* the game's own blockers are
   simple quads; 16 sides follow a circle within 2 % of its radius. None with Blocks is refused at save.
9. **Model names:**
   - `plc_n5<name>` and `n5m_<name>`, with `_<k>` past one variant;
   - a name is up to 8 letters and digits, so every model fits 16 characters;
   - a name the game already has is refused.

   *Why:* nwn5e's `n5` keeps them clear of the game's names (as `plc_n5fd_*`).
10. **A new part's scale** keeps its footprint within 0.4 of the guide's diameter (or longer side), never above 1. With
    no guide it's 1. *Why:* this is the 0.35 m rule above (0.4 x 0.9 m), now for any size of guide.
11. **Play in game sends what would be built.** For the Floating Disk that's the creature model's meshes (the parts and
    the disk). For the other kinds it's the variant's meshes. All of it goes 0.85 m beside the character. *Why:* the
    in-game look should be the built look; the disk's texture ships with the install.
12. **DDS first.** When the game has a texture as DDS and TGA, the bench draws the DDS, the copy the game draws (often the
    bigger one). The same loader serves the meshes in the effect mode (chunks and mesh beats). The particles keep the
    library's textures.

### Limits (added)

- **Mesh beats in the game** fade only after an install carrying Task A4. Before that they show for the whole effect.
- **A mesh beat is a copy.** It follows an edited spell mesh only after **Refresh from the mesh**.
- **A spell mesh is a chunk in the game** only once the game has written its model, when it runs after the save.
- **Blocks** is the guide's outline, not the parts' shape. A guide much bigger than the parts blocks more than they
  cover.
- **Lighting and shine:** the stage's lighting and the environment-map limit stand. DDS is now read.

### Size

17 tasks in the plan's addendum, about **4 sessions**: more than the 2½ the owner was told.

| part | about |
|---|---|
| the Composer as first planned (now with projects and variants) | 2 sessions |
| DDS and Play in game | ½ |
| the project generator (placeables, spell meshes, rows, use point, walkmesh) and the project format | ¾ |
| mesh beats: the writer and draft on anim (C++, test.bat), core.js, the effect editor | ¾ |

---

## Addendum 2026-11-17: PLT parts baked

The owner found the helmets untextured, on the stage and in the game. This addendum adds dyed parts to the Composer. Where
it and the sections above disagree, it wins; they stay as they were written.

### The owner's words, in order

1. "helms don't have textures? Not in the bench and not in-game"
2. After the explanation - a PLT is NWN's dyeable texture: each pixel is a shade and a layer, painted from an item's or a
   creature's colours; placeables and effects have no colours, so the game binds no texture; helm_001/002/003/011/012
   have their texture only as a PLT - he picked: "go with A, per-part colours".

**Decision A** (also a #39 comment):
- a part whose texture exists only as a PLT is baked: painted with the game's palettes into an ordinary texture of its
  own, shipped beside the model (placeables, spell meshes, the Floating Disk);
- the VFX Bench draws the same baked picture;
- each such part gets colour fields in its part card (Metal 1, Metal 2, Leather 1, Leather 2, Cloth 1, Cloth 2, as palette
  numbers) with neutral defaults;
- test-first.

### What the game's files say

Read 2026-11-17 through `spell_bench\game_data.js` (probes in the job's tmp folder, nothing kept).

- **A PLT** (helm_001, helm_002, helm_011, pmh0_chest001 decoded):
  - the header `PLT V1  `, then u32 10 (its layers), u32 0, the width (u32 at 16) and the height (u32 at 20);
  - then a 2-byte pixel each: its **shade** (0 dark to 255 light), then its **layer** (0-9);
  - 24 + 2 x width x height bytes in all;
  - **the rows run bottom-up**, as a TGA's with descriptor 0x08. The game has 17 PLTs also as small TGAs (pmh0_*,
    pmd0_*). For 15 of them, the PLT's shade matches the picture's brightness with the PLT's first row at the picture's
    bottom (correlation 0.46-0.88, against -0.36-0.37 the other way up); the other two are too flat to tell.
- **The layers:** 0 skin, 1 hair, 2 metal 1, 3 metal 2, 4 cloth 1, 5 cloth 2, 6 leather 1, 7 leather 2, 8 tattoo 1,
  9 tattoo 2 (NWN's order). helm_001 uses metal 1 and 2; helm_011 metal 2, cloth 1 and leather 1 and 2.
- **The palettes:** seven TGAs, each 256 x 176, 32-bit, bottom-up, no DDS copies: pal_skin01, pal_hair01, pal_armor01,
  pal_armor02, pal_cloth01, pal_leath01, pal_tattoo01. pal_cloth01, pal_leath01 and pal_tattoo01 are the same file.
  - **A colour number is a row, counted from the top as the picture is seen.** The first 64 rows are the original
    game's (its special colours at 56-63: 61 is alpha 0 everywhere); the 1.69 colours 64-175 are under them. Skin 0 is a
    light skin, cloth 0 a cream.
  - **A shade is a column**, 0 dark to 255 light.
  - **Which palette a layer takes:** skin, hair and tattoo their own; metal 1 pal_armor01; metal 2 pal_armor02; cloth 1
    and 2 pal_cloth01; leather 1 and 2 pal_leath01. This is the documented mapping. The files can't prove metal 2's:
    pal_armor02 holds paler, opaque versions of pal_armor01's hues.
  - **The alpha is shine, not see-through.** pal_armor01's falls as the shade rises (silver: 181 at shade 64, 71 at 192,
    0 at 255), and the cloth and leather rows 44-59 carry it too. A helmet that light makes see-through would be absurd:
    it's the environment map's share.
- **The parts:** of the parts list's 2,171 entries, each read through reader.js as the stage reads it, **34 have a
  texture the game has only as a PLT**:
  - 33 of the 35 helmets, through 24 PLTs (helm_001 to helm_034; some helmets share one: helm_006 draws helm_003's);
  - one loose item, gi_cloak01;
  - no placeable and no weapon.

  Those PLTs use metal 1 (23 of them), metal 2 (24), cloth 1 (9), cloth 2 (7), leather 1 and 2 (2 each); none uses skin,
  hair or tattoo, none has a TXI, and none has a shade-255 pixel. 23 are 128 x 128, one 256, one 512.

### What the owner sees

- **On the stage, in the parts list's thumbnails and in Play in game, a dyed part is textured**: by default steel with a
  darker steel trim, brown leather, dark grey cloth.
- **A dyed part's card gains Colours**: Metal 1, Metal 2, Leather 1, Leather 2, Cloth 1, Cloth 2.
  - Each is a palette number 0-175, with a swatch of its colour beside it.
  - The stage re-bakes as he changes one; each field is one undo step.
  - A note says the part is dyed (its PLT's name) and that colours left at the defaults aren't written.
- **Save** writes only the colours that differ from the defaults, in the part's line: `"colours": { "metal1": 5 }`. Its
  status names the baked textures.
- **In the game:** the built models name the baked textures, and the textures are made from his own game into the bench
  build's `vfxedit\saved\`, which the dev layer serves at every start. After a Save, the next start shows them (with the
  models from the next install); Play in game shows them at once.

### How it works

- **floatdisk.js** (pure, Node and page): the PLT constants (layers, palettes, the six fields, the defaults);
  `readColours` (readPart reads a part's `colours` with it); `cleanColours`; `readPlt`; `bakePlt`; `bakedName`;
  `tgaBytes`; `bakeBitmaps` (a part's meshes with their dyed textures renamed).
- **reader.js** (Node): `image()` gives the PLT's bytes; `pltOnly(name)`; `bake(plt, colours)` from the game's PLT and
  palettes.
- **files.js**: `bakeParts` (both generators rename and bake through it); the ownership kind `baked`; `shipTextures`
  leaves baked names alone.
- **Both build.js:** `--saved <folder>` takes the baked textures; the summary lists them (`baked` in `--json`).
- **composer_server.js:**
  - the texture route serves a PLT-only texture as itself (`X-Texture: plt`), the page bakes it;
  - a part's textures say `draw: "plt"`;
  - the route also looks in `vfxedit\saved\`, where the baked textures are;
  - `POST /api/composer/bake` bakes Play in game's textures into `vfxedit\saved\`;
  - Save gives every kind's build `--saved`;
  - the thumbnails move to `thumbs\v2\`.
- **The page:**
  - composer.js's placement renames the dyed textures as the generators do;
  - a loader ahead of the game's bakes each baked name in the page;
  - the part card's Colours;
  - Play in game sends its bakes first.

### Calls I made (this addendum's)

1. **The bake has one implementation:** floatdisk.js paints, reader.js reads the game for it. *Why:*
   - floatdisk.js already loads in the page and in both generators, is served at `/composer/floatdisk.js` and sits on
     the server's stale list: no new route, script tag or stale entry;
   - reader.js already owns "a texture's pictures from the game", ovr\ first.
2. **The name:** `n5p_` and 12 hex digits of a 53-bit string hash (cyrb53) of the PLT's name and its ten layers'
   colours. helm_001 at the defaults is `n5p_2cd3d725dedb`. *Why:*
   - a resref is 16 characters: no room for a PLT's name and six numbers;
   - the hash is plain JavaScript, the same in Node and the page;
   - other colours give another name, so the game never shows a picture it cached under the old ones, and parts with the
     same PLT and colours share one file;
   - `n5p` is free: no game resource and no nwn5e file starts with it.
3. **Baked textures live in the bench build's `vfxedit\saved\`, for every kind, never in a repo.** *Why:*
   - a baked texture is a picture made from the game's PLT and palettes, and CLAUDE.md's rule is "Game files (and art
     made from them) ... are never committed";
   - `saved\` is the dev layer's NWN5EVFXS folder, registered at every start (with `vfx_bench` on, the default) and
     watched for changes: the game finds a texture there by name, as it finds a saved spell mesh's model;
   - so the models in `data\files` name them and nothing else is installed.

   "Shipped beside the model" is read as "made beside where the game reads the bench's models", not `data\files`. Q3
   asks the owner.
4. **Whose a baked file is: its name says.** `n5p_` and 12 hex digits are made only by the bake, and the name fixes what's
   in it. It isn't listed in made.txt (nothing is in a repo). Old ones stay in `saved\`, the bench's own folder.
5. **A build writes baked textures only into a `--saved` it's given.** A dyed part without one is refused, and nothing is
   written. *Why:* a default would let a check, or a bench not yet updated, write the real build's `saved\`. The bench
   passes it; a build by hand says what to add.
6. **Colours in the files:** a part's `colours` holds only the fields that differ from the defaults, and no key when none
   do. readPart reads it: the six names only, whole numbers 0-175, anything else refused with its place. *Why:* a part at
   the defaults keeps its line as it was; an unknown name or a number out of range is a typo.
7. **The defaults:** Metal 1 0 (steel: the game's own colour 0), Metal 2 3 (a darker steel trim), Leather 1 3 and
   Leather 2 2 (browns), Cloth 1 22 and Cloth 2 23 (dark grey, charcoal). Skin, hair and tattoo stay 0: no PLT-only part
   uses them, and the card doesn't offer them. *Why:* picked from bakes of helm_001, 002, 011, 021 and gi_cloak01 side by
   side; the owner asked for neutral.
8. **The bake:**
   - each pixel from its layer's palette, the row its colour and the column its shade;
   - the PLT's first row is the picture's bottom;
   - RGB only, opaque: the palettes' alpha is shine, which a plain texture would turn into see-through;
   - written as a 24-bit TGA, rows bottom-up as the game's;
   - a layer past 9 is painted as skin and a colour past 175 as 175 (neither occurs in the game's files).
9. **The page bakes**, from the PLT and the palettes the texture route serves (a PLT-only texture as itself, the palettes
   as TGAs). A loader ahead of the game's gives each baked name its picture. *Why:* a colour typed re-bakes at once, the
   thumbnails bake the same way, and it's the generators' own function.
10. **Play in game:** before its request, the page sends the variant's bakes (PLT and colours) to `POST
    /api/composer/bake`. The server bakes them through reader.js into `vfxedit\saved\`, then the play goes as now. *Why:*
    the same file a Save would make, in a folder the game reads by name and watches; the dev layer doesn't change.
11. **The Floating Disk's generator** renames the dyed textures in the creature and pile models, writes the baked
    textures into `--saved`, and lists them in its summary. Its 13 repo files stay 13. The models of a stage holding a
    dyed part change: the owner's Large, with helm_001.
12. **Thumbnails move to `thumbs\v2\`.** *Why:* the 34 dyed entries' thumbnails were drawn flat and would stay so. v2
    redraws every entry once, as it scrolls into view. The first folder is the bench's own cache, left as it is.
13. **No change to the dev layer or the game's code.**

Superseded above: call 6's "PLT ... draws flat" and the limit "DDS-only and PLT textures draw flat" (DDS went in A14).
A texture the game doesn't have at all still draws flat.

### Limits (added)

- **Matte metal:** a baked texture shows the palette's colour without the shine the game gives a dyed item.
- **Metal 2's palette** is the documented one (pal_armor02); the files can't prove it. If the game's helmets show metal 2
  otherwise, it's one line to change.
- **Only with the dev layer:** a build without it (a public one) has no `saved\` folder, and dyed parts draw untextured
  there (Q3).
- **Play in game** relies on the game noticing a texture written into `saved\` while it runs (the folder's watch). If it
  doesn't, the next Play or start shows it.
- **Other default colours** rename every default-coloured texture: the models change at their next Save.

### Checks (added)

- **The pure parts** (`tools\data\floatdisk\check.js`, no game): a part's colours; cleanColours; readPlt; bakePlt on a
  2 x 2 PLT and synthetic palettes; the name's known answers; the TGA's bytes; bakeBitmaps; compositionText's colours;
  the same name and bytes in a page.
- **The generators against the game** (both check_build.js, on copies): a dyed part baked into `--saved` and named by
  the models, not in `data\` or made.txt; without `--saved`, refused and nothing written; the real build's `saved\`
  untouched.
- **The server** (`check_composer_server.js`): the PLT served as itself; Play's bake, byte for byte the page's bake of the
  served PLT and palettes; a baked texture served from `saved\`; Save bakes into the temporary build.
- **The page** (`check_composer.js`, `composer_check.mjs`): the dyed meshes renamed for their colours; the default
  colours dropped; a helmet drawn baked; a colour typed re-bakes, one undo step; the page's bake equals reader.js's; Play
  bakes before it plays; a dyed thumbnail in its colours.

### Size

Four tasks in the plan's addendum (P1-P4), **about one session**: the bake and its checks (anim), the generators
(anim), the server and the page (nwn5e-tools).

### Open question (only the owner can answer)

- **Q3. Where do baked textures live?** I recommend (a).
  - **(a) Planned:** in your build's `vfxedit\saved\`, made from your own game. Your game shows them; the repo holds no
    game art. A build without the dev layer has none, and its dyed parts draw untextured until a release step bakes
    them on the player's machine.
  - **(b)** In `data\files`, committed and installed as the disk's texture is. Works in any build, but it commits art
    made from the game's files, against the repo's rule.
  - Note: the disk's committed models (`data\files\c_n5fd_*`, `plc_n5fd_*`) already carry the game's placeable meshes.
