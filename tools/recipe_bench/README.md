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

1. Drop the recording's `output_data/mediapipe_body_3d_xyz.csv` on the drop zone
   (with `output_data/recording_parameters.json` for its frame rate; else 30).
2. Pick what it is for - `n5_rage` (played as a Rage starts), `n5_cridle` (the
   crouched idle while hiding), `n5_shove` (a player's Shove swing) or a name of
   your own (play it with `!anim <name>`). Each sets sensible blends, a loop or a
   fitted length.
3. Set the **standing** window (the T-pose at the take's start: the floor, the
   facing and the body's neutral are found there), trim the take, smooth it if it
   jitters, and watch the body you pick play it with the capture's points over it.
4. **Save for every body**: the take is retargeted to all eight body libraries and
   written as one keys recipe; the next game start builds it.

Capture habits: two or more cameras; a T-pose at each take's start, facing
forward, feet flat; moves performed on the spot (travel is stripped); a prop of
the weapon's length; a clap or a stomp at the contact moment of a swing.

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
  into made-up captures and back (the joints within a few cm), the axes found
  from a standing pose, and a real recording retargeted to every body:
  `node tools/recipe_bench/mocap_check.js build/animedit/libraries [recording.csv]`.
- `check.js` - runs core.js on a folder of libraries:
  `node tools/recipe_bench/check.js build/animedit/libraries`.
