# Animation Recipe Bench

A local web app to make nwn5e animation recipes with a live preview: three figures
play the base animation, the recipe and the animation it mixes toward, and change as
you move the sliders. Crouch drops the hips and re-bends the legs so the feet keep
their steps, Lean tips the upper body (nwn5e's own crouch walk and crouched idle,
`n5_crwalk` / `n5_cridle`, are made this way - open them to tune them). Save writes the recipe into the game's recipes folder; the next
game start builds it, and `!animedit` or `!anim` plays it.

## Run it

Double-click `recipe_bench.bat` (or `node tools/recipe_bench/server.js`). It opens
http://127.0.0.1:8765/ in the browser; close the window to stop it. It needs
Node.js 18 or later and nothing else. After an update to the bench, close its window
and start it again (the page says so when its window is older than its files).

The game has to have been started once with nwn5e 0.24.2 or later: at each start
nwn5e copies the animation libraries it reads (from your own game files) to
`build\animedit\libraries\`. The bench reads them there and saves recipes to
`build\animedit\recipes\<name>.recipe`.

Options: `--build <folder>` (nwn5e's build folder; default `../../build`, or the
`NWN5E_BUILD` variable), `--port <n>` (default 8765), `--no-open`.

## Mocap mode

The **Mocap** tab turns a [FreeMoCap](https://freemocap.org) recording into an
animation for nwn5e:

1. Drop the recording's folder on the drop zone (or **Pick a recording folder**),
   or its files: FreeMoCap's RTMPose output - `output_data/rtmpose_body_3d_xyz.csv`,
   the hand files `rtmpose_left_hand_3d_xyz.csv` / `..._right_...`, and
   `synchronized_videos/timestamps/<take>_timestamps.csv` - or the older
   MediaPipe one, `output_data/mediapipe_body_3d_xyz.csv` with
   `recording_parameters.json` for its frame rate. The rest of the folder is
   passed over. With the timestamps, the take is resampled to an even 30 a
   second (webcams run unevenly); a body file without them needs its frame rate
   set by hand. Glitches - a bone suddenly stretched or squashed by 30% for a
   frame - are dropped and filled (**Remove glitches**); hand files whose points
   don't sit on the wrists are left out (that hand then follows its forearm).
2. Pick what it is for - `n5_rage` (played as a Rage starts), `n5_cridle` (the
   crouched idle while hiding), `n5_shove` (a player's Shove swing) or a name of
   your own (play it with `!anim <name>`). Each sets sensible blends, a loop or a
   fitted length.
3. Set the **standing** window (standing still at the take's start: the floor,
   the facing and the body's neutral are found there), trim the take, smooth it if
   it jitters, and watch the body you pick play it with the capture's points over
   it. A new take gets a suggested standing window and trim (**Suggest standing
   and trim** again after changes); the graph under the sliders shows how fast
   the hands and feet move, the standing window, the part kept and the playing
   time.
4. **Planted feet**: the rows **L** and **R** under the graph are the stretches
   each foot stands still (suggested; **Suggest feet** again). Drag along a row
   to add one, drag an end to move it, click one to remove it. With **Lock
   planted feet**, a planted foot stays on its spot on every body - that body's
   leg bends to keep it there (a ring under the foot in the preview); red: out of
   the leg's reach on this body, the foot slides that much (listed under the
   rows). **The body**: *moves with its steps, back to its spot in the
   blend-out* (one-off animations), or *stays on its spot* (walk cycles, loops:
   the take's travel is taken out, the game moves the creature).
5. **Save for every body**: the take is retargeted to all eight body libraries and
   written as one keys recipe; the next game start builds it.

For hand clean-up - smoothing, a bad moment, a pose - **Export points for
Blender** writes the take as cleaned here (`<take>_points.csv`), from the standing
part to the end of the kept part, both marked, and the planted feet; clean it in
Blender with `tools/mocap_blender` (its README is the step-by-step) - it plays the
kept part - and drop the file Blender exports back here. A points file keeps its
own times, its marks set the sliders and the planted feet, and the glitch filter
starts off for it. (A sliding foot: lock it here - a fix in Blender slides again
on NWN's legs.)

Capture habits: two or more cameras; standing still (arms relaxed or a T-pose)
for a second or two at each take's start, facing forward, feet flat; moves
performed on the spot where you can (a one-off's steps travel and come back in
the blend-out; a walk cycle's travel is taken out); a prop of the weapon's length; a clap
or a stomp at the contact moment of a swing. The frame rate matters for fast
moves: resampled from 13 a second, a stab's peak lands about 29 cm short at the
hand, from 25 about 15 cm (mocap_check.js's info lines); slow moves (a flourish, a
stance) are fine at webcam rates.

The arms and legs point where the performer's point; the pelvis, torso, neck
and head turn by how much the performer's turn from the standing pose (which
maps to the body's own idle stance); the root follows the hips' height and their
sway over the feet. The pelvis's forward tilt isn't in the 33 points - it tips
with the torso (35%).

## Files

- `server.js` - serves the page and the two folders, on 127.0.0.1 only.
- `public/core.js` - the library reader and the recipe maths, a line-for-line port
  of `src/data/anim_recipe.cpp`: keep the two in step.
- `public/mocap.js` - Mocap mode: reading a capture, its axes and clean-up, the
  retarget, blends, loop, fit and key reduction. Saved recipes are read by
  `src/data/anim_recipe.cpp` (`kind = keys`).
- `public/app.js`, `index.html`, `style.css` - the page.
- `mocap_check.js` - checks Mocap mode on the real libraries: NWN animations turned
  into made-up captures and back (the joints within a few cm) - MediaPipe's layout
  and RTMPose's (a row a point a frame, hand files, uneven frame times, glitches,
  hand files off the wrists) - the axes found from a standing pose, and a real
  recording retargeted to every body:
  `node tools/recipe_bench/mocap_check.js build/animedit/libraries [recording.csv | recording folder]`.
- `check.js` - runs core.js on a folder of libraries:
  `node tools/recipe_bench/check.js build/animedit/libraries`.
