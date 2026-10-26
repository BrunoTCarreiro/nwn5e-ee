# VFX Bench

A local web app to make and edit visual effects for nwn5e, with a live preview: pick
one of the game's effects (or nwn5e's, or start a new one), change its emitters with
sliders, colour pickers and a texture picker, and watch the particles change as you
go. **Play in game** shows the effect on your character in the running game - the
game's own renderer, the exact look. **Save** keeps it as a draft for the mod.

## Run it

Double-click `vfx_bench.bat` (or `node tools/vfx_bench/server.js`). It opens
http://127.0.0.1:8766/ in the browser; close the window to stop it. It needs Node.js
18 or later and nothing else. After an update to the bench, close its window and
start it again (the page says so when its window is older than its files).

The game has to have been started once with nwn5e 0.42.0 or later (`vfx_bench=1` in
nwn5e.ini, the default): 3 s into a start nwn5e copies the game's effects out for the
bench, once - `build\vfxedit\library\`: `index.txt` (the effects table's rows that have
emitters), `game\<model>.vfx` (each model's emitters as a draft), `textures\` (the
particle textures, from your own game files) and `nwn5e\` (nwn5e's own effects).
`!vfxexport` in the game's chat writes it again. An open page fills its lists in by
itself when the copy arrives.

Where an effect sits: the figure is the game's human male (pmh0, about 2 m), and an
effect is on one of three points - the feet (root), the chest (impact, 1.47 m) or
over the head (the head conjure point, 2.4 m: where the game's spell glows sit, about
40 cm above the head). An emitter's position is measured from that point; a new
emitter starts on it (0 0 0).

- **Play in game / Stop in game** work while a character is in play (the dot at the
  top says "Game: running"): the bench writes `build\vfxedit\bench\request.vfx`, the
  game plays it on your character as `!vfxedit`'s Preview on me does and answers in
  `bench\status.txt`.
- **Save** writes `build\vfxedit\saved\<name>.vfx`; the game writes its model
  (`<name>.mdl`) next to it when it runs, and `!vfxedit`'s Load lists it. To make it
  one of the mod's effects, ask Claude to add it.

Options: `--build <folder>` (nwn5e's build folder; default `../../build`, or the
`NWN5E_BUILD` variable), `--port <n>` (default 8766), `--no-open`.

The preview runs and draws particles by the game's own rules, read from its code
(PARTICLES.md, 0.42.2): births per frame (the fraction of a frame's births dropped, as
the game does), spread, speed, gravity, area, life, bounce and splat, the seven render
modes and three blends as the game builds them (back faces hidden), start -> end size,
colour and alpha (the "mid" values do nothing in the game), sheet frames at fps, spin.
An emitter's turn (orientation) is in the draft. Motion (0.42.3): an effect keeps the
nodes its emitters hang under (the game's dummies - and meshes or lights, kept as dummies)
and the keys that move and turn them and the emitters in its three animations - the start
(impact) once, the loop (duration) while applied, the end (cessation) as it's removed -
so a game effect's circling rings and wandering eyes play here, in Play in game and in
what Save makes. Particles leave from where the emitter is at that moment, and follow it
(inherit), the character's move (inherit_local) or the emitter's move (inherit_part).
The Nodes card lists them (Add, Remove, what each hangs under, its place and turn, Make it
spin); an emitter's "Its own motion" has Make it orbit (a new node turning round z with the
emitter out at a radius). Not here: Lightning emitters, the game's soft-particle fade and
fog, and the meshes of a game effect (Mage Armor's shields, Bigby's hands - 25 of the game's
spell effects have some) - only its particles. When in doubt, Play in game.

Two more of the game's emitter settings (0.42.4): "Seen from both sides" (twosidedtex - the
flat and particle-direction kinds otherwise show one side; 37 of the game's effects use it)
and "Dead space" (deadspace, radians: with a spread, a particle that would leave with its sheet
almost edge-on to the camera is turned toward it as it's born - so in the preview it depends on
where you look from, as in the game; 98 of the game's emitters use 0.17 or 0.21). The game
loops until that turn is done and above about 0.39 it can loop for ever, so the bench holds it
at 0.39.

Point to point (0.42.5): an emitter's "Goes to a target" sends its particles to a node under
it - Bezier along a curve from the emitter, arriving as they age out (the curve's two middle
points out along the emitter's z and back along the target's), or Gravity, pulled to it and
gone when they reach it. Choosing one makes the target (<emitter>_target, 1.5 m along the
emitter's x) if there isn't one; move it, or give it keys, in the Nodes card. 119 of the
game's emitters do it (Bigby's hands, Greater Dispelling's streaks). Save writes the target as
the reference node the game looks for; an emitter left without one is saved as not point to
point (the game would crash).

Mouse on the stage: drag to turn, right-drag (or shift-drag) to move, wheel to zoom,
double-click to reset. Space pauses, R restarts. `?open=game:<model>&warm=<s>` opens
an effect already run that far.

## Files

- `server.js` - serves the page and the build folder's `vfxedit\` parts, on 127.0.0.1 only.
- `public/core.js` - the draft format, a line-for-line port of `src/data/vfx_model.cpp`'s
  WriteVfxDraft / ReadVfxDraft (keep the two in step), and the TGA reader.
- `public/sim.js` - the particles as the game runs them; `public/view.js` - the WebGL stage,
  drawing them as the game does; `public/app.js`, `index.html`, `style.css` - the page.
- `PARTICLES.md` - what the game's particle code does (spawn, movement, drawing), with addresses.
- `check.js` - reads and writes back every draft nwn5e wrote (they must come out byte for
  byte) and decodes every texture: `node tools/vfx_bench/check.js build/vfxedit`.
