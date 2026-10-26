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

The preview is an approximation of the game's emitters (update, render and blend
modes, spread, gravity, spawn area, life, size / colour / opacity over life, texture
sheets, bounce, splat, particles that follow the character). Not drawn: Lightning
emitters. Not in the effect at all (the in-game editor's too): an emitter node's own
movement in the game's models - their rings that circle the character stand still
here and in what Play in game and Save make. When in doubt, Play in game.

Mouse on the stage: drag to turn, right-drag (or shift-drag) to move, wheel to zoom,
double-click to reset. Space pauses, R restarts. `?open=game:<model>&warm=<s>` opens
an effect already run that far.

## Files

- `server.js` - serves the page and the build folder's `vfxedit\` parts, on 127.0.0.1 only.
- `public/core.js` - the draft format, a line-for-line port of `src/data/vfx_model.cpp`'s
  WriteVfxDraft / ReadVfxDraft (keep the two in step), and the TGA reader.
- `public/sim.js` - the particles; `public/view.js` - the WebGL stage; `public/app.js`,
  `index.html`, `style.css` - the page.
- `check.js` - reads and writes back every draft nwn5e wrote (they must come out byte for
  byte) and decodes every texture: `node tools/vfx_bench/check.js build/vfxedit`.
