// The generators' file plumbing (#39), shared by the Floating Disk's build.js and the Composer's
// (tools\data\composer\build.js): their options, loading nwn5e-tools, a repo file's path, a dyed part's baked textures,
// a shape's soft texture, a moving texture's material and shader (M2), the textures a part's own file brings, and
// writing only files that are the generator's own - one ownership rule for both. Node only.
//
//   const W = require("./files.js");
//   W.option(argv, "game", "")   W.loadTools({ tools, game, build })   W.bakeParts(placed, rd, baked)   W.shipTextures(placed, rd, baked)
//   W.bakedWrites(baked, dir)   W.bakedLines(baked, where)   W.bakedJson(baked)
//   W.shapeTextures(placed, made)   W.shapeWrites(made, dir)   W.shapeLines(made, where)   W.shapeJson(made)
//   W.motionMaterials(placed, materials)   W.motionWrites(materials, dir)   W.motionLines(materials, where)   W.motionJson(materials)
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

// --- dyed parts -------------------------------------------------------------------------------------------------------
// A dyed part's textures - the game has them only as a PLT - renamed for its colours and baked from the game's palettes
// (floatdisk.js's bakeBitmaps, reader.js's bake): each placed part's meshes replaced by their renamed copies (x.meshes),
// and each bake `baked` lacks added. `placed`: [{ part, meshes }] (placePart's, with its part). -> `baked`: Map(name ->
// { plt, colours (only what isn't a default), bytes (its TGA), label (the part's) }), in the parts' order.
function bakeParts(placed, rd, baked = new Map()) {
  for (const x of placed) {
    const b = F.bakeBitmaps(x.meshes, x.part.colours, rd.pltOnly);
    x.meshes = b.meshes;
    for (const k of b.bakes) {
      if (!baked.has(k.name)) baked.set(k.name, { plt: k.plt, colours: k.colours, bytes: rd.bake(k.plt, x.part.colours).bytes, label: F.partLabel(x.part) });
    }
  }
  return baked;
}
// The bakes as writes into `dir` (the models' folder, or a spell mesh's drafts'): <dir>\<name>.tga, kind "baked" - not
// noted in made.txt: whose it is, its name says (ownership).
const bakedWrites = (baked, dir) => [...baked].map(([name, b]) => ({ file: path.join(dir, name + ".tga"), bytes: b.bytes, kind: "baked" }));
// The summary's line a bake: its PLT and the part's six colours, in the card's order, and where it goes.
const bakedLines = (baked, where) => [...baked].map(([name, b]) => `  texture ${name}: ${b.plt} dyed with ` +
  F.PLT_FIELDS.map((k) => `${F.PLT_LABELS[k].toLowerCase()} ${b.colours[k] !== undefined ? b.colours[k] : F.PLT_DEFAULTS[k]}`).join(", ") + ` - baked into ${where}`);
// --json's: [{ texture, plt, colours (only what isn't a default) }].
const bakedJson = (baked) => [...baked].map(([texture, b]) => ({ texture, plt: b.plt, colours: b.colours }));

// --- a shape's soft texture -------------------------------------------------------------------------------------------
// The soft textures the placed shape parts name (floatdisk.js's shapeModel: `textures`, none for `none` or a library
// texture), each generated from its colour: original, nothing of the game's. Each `made` lacks is added. `placed`:
// [{ part, meshes }] (placePart's, with its part). -> `made`: Map(name -> { colour, blend (M1b: additive | normal), bytes
// (its 32-bit TGA), txi (an additive one's TXI, floatdisk.js's SOFT_TXI; null for normal), label (the part's) }), in the
// parts' order.
function shapeTextures(placed, made = new Map()) {
  for (const x of placed) {
    if (x.part.source.kind !== "shape") continue;
    for (const { name, colour, blend } of F.shapeModel(x.part.source.shape).textures) {
      if (made.has(name)) continue;
      made.set(name, { colour, blend, bytes: F.tgaBytes(F.shapeTexture({ colour }), { alpha: true }),
        txi: blend === "additive" ? Buffer.from(F.SOFT_TXI, "latin1") : null, label: F.partLabel(x.part) });
    }
  }
  return made;
}
// The soft textures as writes into `dir` (beside what names them: the models' folder, or a spell mesh's drafts'):
// <dir>\<name>.tga and, an additive one (M1b), <dir>\<name>.txi beside it - "blending additive", so the game draws it on
// its additive path - each kind "shape", not noted in made.txt: whose it is, its name says (ownership).
const shapeWrites = (made, dir) => [].concat(...[...made].map(([name, s]) => [{ file: path.join(dir, name + ".tga"), bytes: s.bytes, kind: "shape" },
  ...(s.txi ? [{ file: path.join(dir, name + ".txi"), bytes: s.txi, kind: "shape" }] : [])]));
// The summary's line a soft texture: its colour (an additive one's blend and its TXI), and where it goes.
const shapeLines = (made, where) => [...made].map(([name, s]) => `  texture ${name}: a shape's soft texture, colour ${s.colour.join(" ")}` +
  (s.txi ? `, additive (${name}.txi beside it: blending additive)` : "") + ` - written into ${where}`);
// --json's: [{ texture, colour, blend (only "additive": a Normal one's as before M1b) }].
const shapeJson = (made) => [...made].map(([texture, s]) => (s.txi ? { texture, colour: s.colour, blend: "additive" } : { texture, colour: s.colour }));

// --- a moving texture's material (M2) ---------------------------------------------------------------------------------
// The placed parts' moving textures: each part with a motion has its textured meshes renamed for it (floatdisk.js's
// moveTextures: the bitmap becomes the material's name, n5u_ and 12 hex digits) - each placed part's meshes replaced by
// their renamed copies (x.meshes) - and each material `materials` lacks added. Called after bakeParts and shipTextures,
// so a material's texture (texture0) is the real one and a shipped texture is found by its own name. `placed`: [{ part,
// meshes }] (placePart's, with its part). -> `materials`: Map(name -> { texture, motion (readMotion's), pivot, text (its
// .mtr: motionMaterialText, LF), label (the part's) }), in the parts' order.
function motionMaterials(placed, materials = new Map()) {
  for (const x of placed) {
    const moved = F.moveTextures(x.meshes, x.part.motion);
    x.meshes = moved.meshes;
    for (const m of moved.materials) {
      if (!materials.has(m.name)) materials.set(m.name, { texture: m.texture, motion: m.motion, pivot: m.pivot, text: F.motionMaterialText(m), label: F.partLabel(x.part) });
    }
  }
  return materials;
}
// The materials as writes into `dir` (beside what names them: the models' folder, or a spell mesh's drafts'):
// <dir>\<name>.mtr, kind "material" (ours by its name: only the generator makes n5u_ names, and the name fixes the
// bytes), and - when there's any - the shader they name, <dir>\vsn5uvmove.shd, kind "shader" (ours when made.txt lists
// it, as data\files' committed one, or when it's the generators' shader: floatdisk.js's isMotionShader), CRLF as the
// generators write text. Neither noted in made.txt.
function motionWrites(materials, dir) {
  if (!materials.size) return [];
  return [...[...materials].map(([name, m]) => ({ file: path.join(dir, name + ".mtr"), bytes: Buffer.from(m.text, "latin1"), kind: "material" })),
    { file: path.join(dir, F.MOTION_SHADER + ".shd"), bytes: Buffer.from(F.crlf(F.motionShaderText()), "latin1"), kind: "shader" }];
}
// A motion in words: "scrolls 0 0.25 a second, turns 30 degrees a second, breathes 0.3 at 0.5 a second" (what it does).
const motionWords = (m) => [m.scroll.some((v) => v !== 0) ? `scrolls ${m.scroll.join(" ")} a second` : null, m.turn ? `turns ${m.turn} degrees a second` : null,
  m.breathe ? `breathes ${m.breathe} at ${m.rate} a second` : null].filter(Boolean).join(", ");
// The summary's line a material: its texture, its motion and pivot, where it goes (and the shader beside it).
const motionLines = (materials, where) => [...materials].map(([name, m]) => `  material ${name}: ${m.texture} ${motionWords(m.motion)}, round ` +
  `${m.pivot.join(" ")} - written into ${where}, with ${F.MOTION_SHADER}.shd`);
// --json's: [{ material, texture, motion (its non-zero keys: cleanMotion's), pivot }].
const motionJson = (materials) => [...materials].map(([material, m]) => ({ material, texture: m.texture, motion: F.cleanMotion(m.motion), pivot: m.pivot }));

// --- textures ---------------------------------------------------------------------------------------------------------
// The textures that the placed parts' own files bring and the game doesn't have: each mesh's, looked up in the game,
// then beside its file (reader.js's findTexture); a baked one (`baked`, bakeParts' - renamed, so the game hasn't it) is
// passed over. `placed`: [{ part, meshes }] (placePart's, with the part). -> Map(name -> its copies beside a file: the
// image, and its .txi), in the parts' order. A texture found nowhere stops the build.
function shipTextures(placed, rd, baked = new Map()) {
  const ship = new Map();
  for (const x of placed) {
    if (x.part.source.kind !== "file") continue;
    for (const m of x.meshes) {
      if (!m.bitmap || ship.has(m.bitmap) || baked.has(m.bitmap)) continue;
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
// ours when it carries `mark`; "baked" (a dyed part's texture) when its name without .tga is a bake's (F.BAKED: only the
// bake makes such names, and the name fixes the bytes); "shape" (a shape's soft texture, or an additive one's TXI: M1b)
// likewise when it's a soft texture's (F.SHAPE_TEXTURE: only the generator makes them); "material" (M2: a moving
// texture's material - or, on the copy route, its texture and TXI under the material's name) likewise by its n5u_ name
// (F.MOTION_TEXTURE); "shader" (vsn5uvmove.shd) when made.txt lists it (data\files' committed one) or it's the
// generators' shader, this version's or an earlier one's (F.isMotionShader: its first line); a kind in `kinds` by its own
// test (w, old); any other (a texture in the repo) when made.txt (`manifest`) lists it. -> (w, old) => true when it's ours.
function ownership({ repo, manifest, mark, kinds = {} }) {
  const listed = (file) => repoPath(repo, file) !== null && manifest.has(repoPath(repo, file));
  return (w, old) => {
    if (w.kind === "text") return F.carriesMark(old, mark);
    if (w.kind === "baked") return F.BAKED.test(path.basename(w.file).replace(/\.tga$/i, ""));
    if (w.kind === "shape") return F.SHAPE_TEXTURE.test(path.basename(w.file).replace(/\.(tga|txi)$/i, ""));
    if (w.kind === "material") return /\.(mtr|tga|dds|txi)$/i.test(w.file) && F.MOTION_TEXTURE.test(path.basename(w.file).replace(/\.(mtr|tga|dds|txi)$/i, ""));
    if (w.kind === "shader") return listed(w.file) || F.isMotionShader(old);
    if (kinds[w.kind]) return kinds[w.kind](w, old);
    return listed(w.file);
  };
}
// Every write checked before any is written: a file there that isn't ours (`owned`, as ownership gives it) and isn't
// what would be written stops it all; then each written unless unchanged (a text - or the shader, M2: text too - the same
// but for its line ends is).
// -> [[ "new" | "rebuilt" | "unchanged", file ]], `manifest` gaining the binary files written in the repo. `dry`: the
// same checks and states, nothing written and the manifest left as it is (tools\data\build_derived.js --check).
const sameAs = (w, old) => old.equals(w.bytes) || ((w.kind === "text" || w.kind === "shader") && F.sameText(old, w.bytes));
function writeAll(writes, owned, { repo, manifest, dry = false }) {
  for (const w of writes) {
    if (!fs.existsSync(w.file)) continue;
    const old = fs.readFileSync(w.file);
    if (!owned(w, old) && !sameAs(w, old)) {
      throw new Error(`${shown(repo, w.file)} is there and wasn't made by this generator: nothing written - move it away or rename it`);
    }
  }
  const results = [];
  for (const w of writes) {
    let state = "new";
    if (fs.existsSync(w.file)) {
      const old = fs.readFileSync(w.file);
      state = sameAs(w, old) ? "unchanged" : "rebuilt";
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

module.exports = { option, loadTools, repoPath, shown, bakeParts, bakedWrites, bakedLines, bakedJson, shapeTextures, shapeWrites, shapeLines,
  shapeJson, motionMaterials, motionWrites, motionLines, motionJson, shipTextures, readManifest, writeManifest, ownership, writeAll };
