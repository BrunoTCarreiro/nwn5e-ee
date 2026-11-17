// The generators' file plumbing (#39), shared by the Floating Disk's build.js and the Composer's
// (tools\data\composer\build.js): their options, loading nwn5e-tools, a repo file's path, the textures a part's own file
// brings, and writing only files that are the generator's own - one ownership rule for both. Node only.
//
//   const W = require("./files.js");
//   W.option(argv, "game", "")   W.loadTools({ tools, game, build })   W.shipTextures(placed, rd)
//   W.writeAll(writes, W.ownership({ repo, manifest, mark, kinds }), { repo, manifest })   W.writeManifest(file, manifest, header)
"use strict";
const fs = require("fs");
const path = require("path");
const F = require("./floatdisk.js");

// --- options and nwn5e-tools ------------------------------------------------------------------------------------------
// `--<name> <value>` in argv, else `fallback`.
function option(argv, name, fallback) {
  const i = argv.indexOf("--" + name);
  return i >= 0 && i + 1 < argv.length ? argv[i + 1] : fallback;
}
function need(file, what) {
  if (!fs.existsSync(file)) throw new Error(`${what} isn't there: ${file}`);
  return file;
}
// What the nwn5e-tools folder gives: the VFX Bench's core.js, spell_bench's game data reader, and the game's folder
// (anim_helper's findGameDir: `game`, else NWN_GAME_DIR, nwn5e.ini's [loader] game_dir, the Steam folder). Called when a
// generator's main starts, so a bad --tools is an error like any other, JSON included.
function loadTools({ tools, game, build }) {
  global.window = global.window || {};  // core.js runs in the page and under node
  const core = require(need(path.join(tools, "vfx_bench", "public", "core.js"), "nwn5e-tools' VFX Bench core (--tools)"));
  const gameData = require(need(path.join(tools, "spell_bench", "game_data.js"), "nwn5e-tools' game data reader (--tools)"));
  const helper = require(need(path.join(tools, "anim_helper", "helper.js"), "nwn5e-tools' anim helper (--tools)"));
  return { core, gameData, game: helper.findGameDir({ game, buildDir: build }) };
}

// --- paths ------------------------------------------------------------------------------------------------------------
// A file in the repo by its path from the repo's root ("data/files/x.tga"); null outside it.
function repoPath(repo, file) {
  const rel = path.relative(repo, file);
  return rel.startsWith("..") || path.isAbsolute(rel) ? null : rel.split(path.sep).join("/");
}
// A file as the generators name it in what they print: from the repo's root, else in full.
const shown = (repo, file) => repoPath(repo, file) || file;

// --- textures ---------------------------------------------------------------------------------------------------------
// The textures that the placed parts' own files bring and the game doesn't have: each mesh's, looked up in the game,
// then beside its file (reader.js's findTexture). `placed`: [{ part, meshes }] (placePart's, with the part). -> Map(name
// -> its copies beside a file: the image, and its .txi), in the parts' order. A texture found nowhere stops the build.
function shipTextures(placed, rd) {
  const ship = new Map();
  for (const x of placed) {
    if (x.part.source.kind !== "file") continue;
    for (const m of x.meshes) {
      if (!m.bitmap || ship.has(m.bitmap)) continue;
      const found = rd.findTexture(m.bitmap, [path.dirname(x.part.source.file)]);
      if (!found) throw new Error(`${F.partLabel(x.part)}: texture ${m.bitmap} is neither in the game nor beside the file (${m.bitmap}.tga / .dds)`);
      if (found.where === "beside") ship.set(m.bitmap, found.copies);
    }
  }
  return ship;
}

// --- whose a file is, and writing -------------------------------------------------------------------------------------
// made.txt: the files a generator wrote in the repo that can't carry its mark (textures), by their repo paths.
function readManifest(file) {
  if (!fs.existsSync(file)) return new Set();
  return new Set(fs.readFileSync(file, "utf8").split(/\r?\n/).map((l) => l.trim()).filter((l) => l && !l.startsWith("#")));
}
// It rewritten when its list changed: the `header` lines, then the paths sorted. Made only once it has one.
function writeManifest(file, manifest, header) {
  const body = [...header, ...[...manifest].sort()].join("\n") + "\n";
  const had = fs.existsSync(file);
  if ((had || manifest.size) && (!had || fs.readFileSync(file, "utf8").replace(/\r\n/g, "\n") !== body)) fs.writeFileSync(file, body);
}
// The rule for a file already where a write { file, bytes, kind } goes: "text" (a model, .pwk, rows file or draft) is
// ours when it carries `mark`; a kind in `kinds` by its own test (w, old); any other (a texture in the repo) when made.txt
// (`manifest`) lists it. -> (w, old) => true when it's ours.
function ownership({ repo, manifest, mark, kinds = {} }) {
  return (w, old) => {
    if (w.kind === "text") return F.carriesMark(old, mark);
    if (kinds[w.kind]) return kinds[w.kind](w, old);
    return repoPath(repo, w.file) !== null && manifest.has(repoPath(repo, w.file));
  };
}
// Every write checked before any is written: a file there that isn't ours (`owned`, as ownership gives it) and isn't
// what would be written stops it all; then each written unless unchanged (a text the same but for its line ends is).
// -> [[ "new" | "rebuilt" | "unchanged", file ]], `manifest` gaining the binary files written in the repo. `dry`: the
// same checks and states, nothing written and the manifest left as it is (tools\data\build_derived.js --check).
function writeAll(writes, owned, { repo, manifest, dry = false }) {
  for (const w of writes) {
    if (!fs.existsSync(w.file)) continue;
    const old = fs.readFileSync(w.file);
    if (!owned(w, old) && !old.equals(w.bytes)) {
      throw new Error(`${shown(repo, w.file)} is there and wasn't made by this generator: nothing written - move it away or rename it`);
    }
  }
  const results = [];
  for (const w of writes) {
    let state = "new";
    if (fs.existsSync(w.file)) {
      const old = fs.readFileSync(w.file);
      state = old.equals(w.bytes) || (w.kind === "text" && F.sameText(old, w.bytes)) ? "unchanged" : "rebuilt";
    }
    if (state !== "unchanged" && !dry) {
      fs.mkdirSync(path.dirname(w.file), { recursive: true });
      fs.writeFileSync(w.file, w.bytes);
    }
    if (!dry && w.kind === "binary" && repoPath(repo, w.file) !== null) manifest.add(repoPath(repo, w.file));
    results.push([state, w.file]);
  }
  return results;
}

module.exports = { option, loadTools, repoPath, shown, shipTextures, readManifest, writeManifest, ownership, writeAll };
