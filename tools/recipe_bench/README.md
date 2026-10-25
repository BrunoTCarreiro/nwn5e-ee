# Animation Recipe Bench

A local web app to make nwn5e animation recipes with a live preview: three figures
play the base animation, the recipe and the animation it mixes toward, and change as
you move the sliders. Save writes the recipe into the game's recipes folder; the next
game start builds it, and `!animedit` or `!anim` plays it.

## Run it

Double-click `recipe_bench.bat` (or `node tools/recipe_bench/server.js`). It opens
http://127.0.0.1:8765/ in the browser; close the window to stop it. It needs
Node.js 18 or later and nothing else.

The game has to have been started once with nwn5e 0.24.2 or later: at each start
nwn5e copies the animation libraries it reads (from your own game files) to
`build\animedit\libraries\`. The bench reads them there and saves recipes to
`build\animedit\recipes\<name>.recipe`.

Options: `--build <folder>` (nwn5e's build folder; default `../../build`, or the
`NWN5E_BUILD` variable), `--port <n>` (default 8765), `--no-open`.

## Files

- `server.js` - serves the page and the two folders, on 127.0.0.1 only.
- `public/core.js` - the library reader and the recipe maths, a line-for-line port
  of `src/data/anim_recipe.cpp`: keep the two in step.
- `public/app.js`, `index.html`, `style.css` - the page.
- `check.js` - runs core.js on a folder of libraries:
  `node tools/recipe_bench/check.js build/animedit/libraries`.
