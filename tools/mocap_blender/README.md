# Cleaning a mocap take in Blender

A round trip between nwn5e's Animation Recipe Bench (`tools/recipe_bench`) and
Blender (written for 5.2), for the hand clean-up the bench can't do (issue #77):

```
FreeMoCap recording  ->  bench: Export points for Blender  ->  Blender: clean
   ->  Blender: Export  ->  bench: trim, blends, Save for every body  ->  game
```

The take travels as **points** (where each joint is, frame by frame), not as a
rig: the bench turns points into NWN's eight body types. In Blender each point
is an empty named `mc.<point>` (`mc.left_ankle`, `mc.right_wrist` ...), keyed
on every frame, with a stick figure joining them.

## Once: install the add-on

1. Blender: **Edit › Preferences › Add-ons**.
2. The drop-down arrow at the top right › **Install from Disk…** › pick
   `tools\mocap_blender\nwn5e_mocap_points.py` › tick **nwn5e mocap points**.

(Or open the file in the **Scripting** tab and press **Run Script**: the menu
entries last until Blender closes.)

## 1. Out of the bench

In the bench's **Mocap** tab drop the recording's folder. Check the green
(standing) and shaded (kept) parts of the graph - the sliders under it move
them - then press **Export points for Blender**: `<take>_points.csv` lands in
your downloads. It is the take as the bench cleaned it - an even 30 frames a
second, glitches removed, upright, facing forward, in metres - from the start
of the standing part to the end of the kept part, both marked, and the bench's
planted feet (they come back as they went).

## 2. Into Blender

1. **File › New › General**; select everything (**A**) and delete it (**X**).
2. **File › Import › nwn5e mocap points (.csv)** › pick the file.
3. Front view (**numpad 1**) faces the performer; **Home** frames it; **Space**
   plays the kept part (the timeline's **Start** / **End**). The take arrives
   in a collection called `mocap <file>`.

**The standing part** sits before the kept part, between the timeline markers
`standing` and `standing end`. Leave its keys in: the bench measures every
turn from it. Smoothing it along with the rest is fine.

## 3. Open the Graph Editor

The quick way: the **Animation** workspace tab at the top, hover its Dope Sheet
(the lower editor) and press **Ctrl+Tab** - it turns into the Graph Editor;
**Ctrl+Space** makes it fill the window (again to go back). It shows the keys
of the **selected** points: X, Y and Z location, a key per frame. Select
points by clicking them in the viewport or in the Outliner (Shift-click adds).
To go to a frame, type it in the timeline's current-frame box.

Every tool below is in the Graph Editor's **Key** menu and works on the
**selected keys**. After running one, its settings are in the panel at the
bottom left (**Adjust Last Operation**) - change them and watch the result.
**Ctrl+Z** undoes.

Work in this order - **spikes, then planted feet, then smoothing**: a spike
smoothed first is smeared over the frames around it instead of removed, and
smoothing last also softens the step where a flattened foot starts and stops.

## 4. A bad moment: a twitch or a spike

1. Select the point that jumps (e.g. `mc.right_ankle`).
2. In the Graph Editor find the spike; box-select (**B**) its few keys.
3. Either **X › Delete Keyframes** - the curve bridges the gap - or **Key ›
   Blend › Blend to Neighbor** and drag left or right to pull them toward the
   keys before or after.

## 5. A foot that slides while it should stand still

Leave it to the bench: mark the foot planted on the **L** / **R** rows under its
motion graph (it suggests them) and it stays put on every body - that body's leg
bends to hold it. A foot pinned here comes back as points, and NWN's longer legs
slide it again; the bench's lock doesn't.

Only a foot the capture itself drags - a slip the performer didn't make - is
worth fixing here:

1. Select the foot's points: `mc.left_heel`, `mc.left_foot_index`,
   `mc.left_ankle`.
2. In the Graph Editor's channel list click **X Location**, then Ctrl-click
   **Y Location** - so the height (Z) isn't flattened.
3. Box-select (**B**) the keys over the frames the foot is planted.
4. **Key › Blend › Scale Average** and set **Scale Factor** to **0**: the
   foot stays put on the floor. (1 leaves it as it was; 0.5 halves the slide.)
5. If the step in or out now pops, the smoothing (step 6) softens it.

## 6. Jitter: smooth the whole take

Two webcams at 13 frames a second jitter.

1. Outliner: right-click the `mocap …` collection › **Select Objects**.
2. Graph Editor: hover it, **A** (all keys).
3. **Key › Smooth › Butterworth Smooth**. In its panel, **Frequency Cutoff
   (Hz)**: lower is smoother. Start at **5** when the take has a fast strike
   (a recording at 13-15 a second holds nothing real above about 6 Hz, so 5
   loses none of the move), **3** for a slow one (a stance, a flourish), 2
   for very slow. Below 5, a strike's snap goes soft: leave the striking
   arm's keys out of the selection around it.
4. Play it. Too smooth: Ctrl+Z and a higher cutoff.

To smooth only part of the take, box-select (**B**) just those frames' keys
first. **Smooth (Gaussian)** is the other smoother (its **Sigma** and **Filter
Width**).

## 7. A pose to fix by hand

Go to the frame, select the point, move it (**G**), key it (**I** › Location).
The neighbouring frames are untouched - smooth a few frames around it if it
pops. The take's hands don't turn (their tracking failed): moving and keying
`mc.left_index` / `mc.left_pinky` gives a hand its turn.

## 8. Back to the bench

1. To trim tighter, narrow the timeline's **Start** / **End** first: the kept
   part follows them.
2. **File › Export › nwn5e mocap points (.csv)** - it suggests
   `<take>_points_clean.csv`. Every frame goes, the standing part and the kept
   part marked.
3. **File › Save** the .blend too, to come back to it.
4. Drop the CSV on the bench's Mocap tab. The standing and keep sliders and the
   planted feet come back from its marks, and its glitch filter starts **off** (it would "fix" a
   point you moved on purpose). Blend, pick the animation it's for, **Save for
   every body**; the next game start builds it.

## Files

- `nwn5e_mocap_points.py` - the add-on (import, export).
- `check.py` - checks it without a window: imports a marked points CSV, edits
  a key, exports, compares, then narrows the range and exports again -
  `blender --background --factory-startup --python tools/mocap_blender/check.py -- <points.csv> <out folder>`.
