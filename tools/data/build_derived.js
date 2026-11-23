#!/usr/bin/env node
// The game-derived files, built for a build (D1). nwn5e-ee's CLAUDE.md: "Game files (and art made from them), logs and
// saves are never committed or attached" - and the Floating Disk's models copy the game's own placeable meshes. The
// owner, 2026-11-17: "go with 1, remove the six files from main": the repo keeps the recipes (floatdisk\composition.json,
// the Composer's composer\<project>.json), the generators and original art; the models are built on the player's machine
// at build time, from the player's own game, into the build's data\files. This builds them (README.md beside this file):
//
//   node tools\data\build_derived.js --out <folder> [--game <folder>] [--tools <nwn5e-tools>] [--reader <mdlmesh.exe>] [--check]
//
// - The Floating Disk's generator (floatdisk\build.js) and the Composer's (composer\build.js) for every placeable project
//   in composer\*.json, each run with --json into a scratch folder of its own; their models, walkmeshes and textures - a
//   dyed part's baked texture (n5p_<12 hex digits>.tga, from the game's PLT and palettes) and a shape's soft texture
//   (n5s_<12 hex digits>.tga, from its colour and blend - an Additive one's n5s_<...>.txi beside it, M1b; all git-ignored)
//   and a moving texture's material (n5u_<12 hex digits>.mtr, M2, git-ignored; a clamped one's copy and TXI too, W1:
//   n5u_<...>.tga / .dds / .txi, its texture's picture under its name; an unclamped one's own TXI, MV: n5u_<...>.txi, its
//   texture's blending and map lines) and a still or moving shape's library texture's copy (DC, MV:
//   n5l_<12 hex digits>.tga / .dds and .txi, the game's picture with its TXI minus decal, git-ignored) with the shader it names (vsn5uvmove.shd,
//   committed whole in data\files, which build.bat copies anyway) - with the models that name them - are then written
//   into --out (build.bat: the build's data\files) - never into this repo's data\ (refused). Skipped, and said so: a
//   spell mesh's project (its drafts live in the VFX Bench's saved folder, and the dev layer writes its models when the
//   game runs), and a project never built - no data\rows\placeables.<name>.txt and no variant with a row, as the
//   bench's New project writes it - whose models nothing installed names.
// - #39's Spiritual Weapon look (the owner, 2026-11-23: "for these effects can I pick an existing weapon mesh instead of
//   using a custom one?", then "set it up and make 1 handed swords the first weapon, you can pick one at random"): the
//   force's three effects with its default look's weapon (spiritweapon\build.js DEFAULT_LOOK: the sword, the Composer's
//   swpsword.json - the game's shortsword 012) in place of the generated blade - n5_swp_idle, _fly and _hit, built from the
//   player's game (spiritweapon\build.js --look) over the committed fallback's copies in --out (the generated blade, which
//   build.bat copied there with the rest of data\files: a build without the game - NWN5E_DERIVED=skip - keeps it), with
//   the textures the weapon's meshes name (an Additive part's copies: n5l_ names). Owned there by their names and the
//   model writer's line for that name ("# nwn5e VFX editor (!vfxedit): <name>"), which the fallback carries too.
// - The rows stay the repo's: data\rows' appearance.floatdisk.txt, placeables.floatdisk.txt and each project's
//   placeables.<name>.txt are committed text that names the models. Each generator writes its rows over a copy of
//   data\rows, and they must come out unchanged: rows that differ (or are missing) stop the build, nothing written -
//   so it never ships models whose rows disagree. A recipe changed without its rows does that, and so does a game
//   update that changes the game's rows they copy.
// - In --out, the generators' one ownership rule (floatdisk\files.js): a model or .pwk there is overwritten only when it
//   carries its generator's mark, a texture only when that generator's made.txt lists it as data/files/<name>, a baked
//   texture, a shape's soft texture (or its TXI) or a moving texture's material by its name (only the generators make
//   them, and the name fixes the bytes), the moving textures' shader by its first line; any
//   other file in the way that differs stops it all. Every file is checked before any is written. Two generators may
//   make the same bake (or soft texture): written once.
// - One line a file (written / unchanged), then a total. --check: the same builds and checks, in the scratch folders
//   only; nothing written into --out, each file said as it would be (would write / unchanged).
// - --game, --tools and --reader default as the generators': the game NWN_GAME_DIR, nwn5e.ini's [loader] game_dir, the
//   Steam folder; nwn5e-tools NWN5E_TOOLS, else beside this repo; the reader NWN5E_MDLMESH, else build\floatdisk\
//   mdlmesh.exe, built with floatdisk\build_mdlmesh.bat (VS 2019 Build Tools) when missing or older than its sources.
//
// - One burst (LD; the owner, 2026-11-20: "For the lectern A"): the game's 16 placeable models whose looping "dead"
//   re-fires a detonate (oneburst.js's MODELS: the lectern's among them) - each the game's own model, read from the
//   player's game, with that animation's detonate events taken out (mdlmesh --drop-events) and every other byte as it
//   was, in the form the game has it (compiled, or ASCII for two) - written into --out under its own name, which nwn5e
//   serves over the game's. Owned there by its name (only this makes them; the game's model fixes the bytes); git-ignored.
//   One the game hasn't, or whose "dead" has no detonate now, is skipped and said.
//
// Exit codes: 0 done; 2 a prerequisite missing - nwn5e-tools, the game, the mesh reader - one line saying which and how
// to fix it; 1 anything else (a refused build, rows that disagree, a file in the way). build.bat stops on either, unless
// NWN5E_DERIVED=skip.
"use strict";
const fs = require("fs");
const os = require("os");
const path = require("path");
const { spawnSync } = require("child_process");
const F = require("./floatdisk/floatdisk.js");
const R = require("./floatdisk/reader.js");
const W = require("./floatdisk/files.js");
const P = require("./composer/project.js");
const O = require("./oneburst.js");

const REPO = path.resolve(__dirname, "..", "..");
const ROWS = path.join(REPO, "data", "rows");
const shown = (file) => W.shown(REPO, file);  // a repo file from the repo's root, else in full

// --- options --------------------------------------------------------------------------------------------------------------
const argv = process.argv.slice(2);
const option = (name, fallback) => W.option(argv, name, fallback);
const CHECK = argv.includes("--check");
const TOOLS = path.resolve(option("tools", process.env.NWN5E_TOOLS || path.join(REPO, "..", "nwn5e-tools")));
const BUILD = path.resolve(process.env.NWN5E_BUILD || path.join(TOOLS, "..", "nwn5e-ee", "build"));  // nwn5e.ini's, as the generators'

class Missing extends Error {}  // a prerequisite: exit 2

// --- the prerequisites ----------------------------------------------------------------------------------------------------
// nwn5e-tools (what floatdisk\files.js's loadTools needs), the game, the mesh reader: each missing one stops it, exit 2.
function prerequisites() {
  for (const need of [["vfx_bench", "public", "core.js"], ["spell_bench", "game_data.js"], ["anim_helper", "helper.js"]]) {
    if (!fs.existsSync(path.join(TOOLS, ...need))) {
      throw new Missing(`nwn5e-tools isn't at ${TOOLS} (no ${need.join("\\")}) - check it out beside this repo, or pass --tools <folder> (or set NWN5E_TOOLS)`);
    }
  }
  const game = require(path.join(TOOLS, "anim_helper", "helper.js")).findGameDir({ game: option("game", ""), buildDir: BUILD });
  if (!fs.existsSync(path.join(game, "data", "nwn_base.key"))) {
    const ini = path.join(BUILD, "nwn5e.ini");  // the one findGameDir read
    throw new Missing(`no game at ${game} (no data\\nwn_base.key) - pass --game <folder>, or set NWN_GAME_DIR, or game_dir under [loader] in ` +
      `${ini}${fs.existsSync(ini) ? "" : " (not there: the Steam folder is the default)"}`);
  }
  let reader;
  try {
    reader = R.ensureReader({ repo: REPO, reader: option("reader", process.env.NWN5E_MDLMESH || ""), log: console.log });
  } catch (e) {
    throw new Missing(`the mesh reader isn't there or doesn't build (${e.message.split(/\r?\n/)[0]}) - pass --reader <mdlmesh.exe>, ` +
      "or run tools\\data\\floatdisk\\build_mdlmesh.bat (VS 2019 Build Tools) to see why it doesn't build");
  }
  return { game, reader };
}

// --- the generators -------------------------------------------------------------------------------------------------------
// The Floating Disk, then each Composer project in composer\*.json by name: a placeable's generator; skipped, a spell
// mesh, and a project never built. -> { list, skipped: [{ file, why }] }
// A generator: { label, script, recipe, args, mark, made (its made.txt: the textures it wrote in data\files) }.
function generators() {
  const floatdisk = path.join(__dirname, "floatdisk"), composer = path.join(__dirname, "composer");
  const recipe = path.join(floatdisk, "composition.json");
  const list = [{ label: "the Floating Disk", script: path.join(floatdisk, "build.js"), recipe, args: ["--composition", recipe], mark: F.MARK,
    made: W.readManifest(path.join(floatdisk, "made.txt")), again: "node tools\\data\\floatdisk\\build.js --files <a scratch folder>" }];
  const skipped = [];
  const projects = fs.existsSync(composer) ? fs.readdirSync(composer).filter((f) => f.toLowerCase().endsWith(".json")).sort() : [];
  for (const f of projects) {
    const file = path.join(composer, f);
    let json;
    try {
      json = JSON.parse(fs.readFileSync(file, "utf8").replace(/^﻿/, ""));
    } catch (e) {
      throw new Error(`${shown(file)}: not a project's JSON - ${e.message}`);
    }
    if (json && json.kind === "spellmesh") {
      skipped.push({ file, why: "a spell mesh: its drafts live in the VFX Bench's saved folder, and the dev layer writes its models when the game runs" });
      continue;
    }
    // A project never built - the VFX Bench's New project writes it at once, one empty variant and no row - has no rows
    // file and no row: nothing installed names its models. Skipped; a project with either is built (and may be refused).
    const rowsFile = json && typeof json.name === "string" ? `placeables.${json.name}.txt` : null;
    const hasRow = json && Array.isArray(json.variants) && json.variants.some((v) => v && v.row !== undefined && v.row !== null);
    if (!hasRow && (!rowsFile || !fs.existsSync(path.join(ROWS, rowsFile)))) {
      skipped.push({ file, why: `never built: no data/rows/${rowsFile || "placeables.<name>.txt"} and no variant with a row ` +
        "(the VFX Bench gives rows at its first Save and build), so nothing installed uses its models" });
      continue;
    }
    const name = json && typeof json.name === "string" ? json.name : path.parse(f).name;
    list.push({ label: `the Composer's project ${name}`, script: path.join(composer, "build.js"), recipe: file, args: [file], mark: P.MARK,
      made: W.readManifest(path.join(composer, "made.txt")), again: `node tools\\data\\composer\\build.js ${shown(file).split("/").join("\\")} --files <a scratch folder>` });
  }
  // #39's Spiritual Weapon: its default look's three effects (spiritweapon\build.js --look; its spell mesh's project
  // built inside it, as above skipped on its own).
  const sw = require("./spiritweapon/build.js"), look = sw.DEFAULT_LOOK, swProject = path.join(composer, sw.LOOKS[look].project + ".json");
  if (!fs.existsSync(swProject)) skipped.push({ file: swProject, why: `the Spiritual Weapon's ${look}: its project isn't there - the generated blade stays` });
  else list.push({ label: `the Spiritual Weapon's ${look}`, script: path.join(__dirname, "spiritweapon", "build.js"),
    recipe: path.join(composer, sw.LOOKS[look].project + ".json"), args: ["--look", look], mark: null, swp: true, made: new Set(),
    again: `node tools\\data\\spiritweapon\\build.js --look ${look} --files <a scratch folder>` });
  return { list, skipped };
}

// The first line two texts differ on (line ends aside), from 1.
function firstDifference(a, b) {
  const x = a.toString("latin1").split(/\r?\n/), y = b.toString("latin1").split(/\r?\n/);
  let i = 0;
  while (i < x.length && i < y.length && x[i] === y[i]) i++;
  return i + 1;
}

// One generator into a scratch folder: files\ (empty) and rows\ (a copy of data\rows: its own rows come out unchanged
// when they agree, and the Composer sees every other rows file's rows as taken). -> { files: [{ name, bytes }], rows:
// [{ name, state: "agrees" | "differs" | "missing", line }] }. A refused build throws, with the generator's reason.
function run(gen, pre) {
  const stage = fs.mkdtempSync(path.join(os.tmpdir(), "n5_derived-"));
  try {
    const files = path.join(stage, "files"), rows = path.join(stage, "rows");
    fs.mkdirSync(files);
    fs.mkdirSync(rows);
    for (const f of fs.existsSync(ROWS) ? fs.readdirSync(ROWS) : []) {
      if (fs.statSync(path.join(ROWS, f)).isFile()) fs.copyFileSync(path.join(ROWS, f), path.join(rows, f));
    }
    const r = spawnSync(process.execPath, [gen.script, ...gen.args, "--json", "--game", pre.game, "--tools", TOOLS, "--reader", pre.reader,
      "--files", files, "--rows", rows], { encoding: "utf8", maxBuffer: 64 << 20, windowsHide: true });
    let j = null;
    try {
      j = JSON.parse((r.stdout || "").trim().split(/\r?\n/).pop());
    } catch (e) {
      // no JSON: the reason from stderr, below
    }
    if (r.status !== 0 || !j || j.error || !Array.isArray(j.files)) {
      const why = j && j.error ? j.error : String(r.stderr || r.stdout || (r.error && r.error.message) || "exit " + r.status).trim();
      throw new Error(`${gen.label} (${shown(gen.recipe)}): ${why.split(rows + path.sep).join("data/rows/")}`);
    }
    const out = { files: [], rows: [] };
    for (const x of j.files) {
      const file = path.resolve(x.file), dir = path.dirname(file), name = path.basename(file);
      if (dir === files) {
        out.files.push({ name, bytes: fs.readFileSync(file) });
      } else if (dir === rows) {
        const committed = path.join(ROWS, name);
        const state = x.state === "unchanged" ? "agrees" : fs.existsSync(committed) ? "differs" : "missing";
        out.rows.push({ name, state, line: state === "differs" ? firstDifference(fs.readFileSync(committed), fs.readFileSync(file)) : null });
      } else {
        throw new Error(`${gen.label}: it wrote ${x.file}, outside its scratch folder`);
      }
    }
    return out;
  } finally {
    fs.rmSync(stage, { recursive: true, force: true });
  }
}

// --- main -----------------------------------------------------------------------------------------------------------------
function main() {
  const given = option("out", "");
  if (!given) {
    throw new Error("--out <folder>: where the models, walkmeshes and textures go (build.bat: the build's data\\files) - " +
      "node tools\\data\\build_derived.js --out <folder> [--game <folder>] [--tools <nwn5e-tools>] [--reader <mdlmesh.exe>] [--check]");
  }
  const OUT = path.resolve(given);
  const inData = path.relative(path.join(REPO, "data"), OUT);
  if (!inData || (!inData.startsWith("..") && !path.isAbsolute(inData))) {
    throw new Error(`--out ${OUT} is in this repo's data\\ - the derived files go into a build's folder (build.bat: %OUT%\\data\\files), never the repo's`);
  }
  const pre = prerequisites();
  const { list, skipped } = generators();
  const say = console.log;
  for (const { file, why } of skipped) say(`build_derived: skipped ${shown(file)} - ${why}`);

  // Every generator built and its rows checked before anything is written.
  const built = [];
  const disagree = [], again = [];
  for (const gen of list) {
    const b = run(gen, pre);
    const bad = b.rows.filter((x) => x.state !== "agrees");
    say(`build_derived: ${gen.label} (${shown(gen.recipe)}): ${b.files.length} file${b.files.length === 1 ? "" : "s"}; ` +
      `its rows ${bad.length ? "disagree" : "agree"} (${b.rows.map((x) => "data/rows/" + x.name).join(", ") || "none"})`);
    for (const x of bad) {
      disagree.push(`  data/rows/${x.name}: ` + (x.state === "missing" ? `missing - ${shown(gen.recipe)} gives one` : `line ${x.line} differs from what ${shown(gen.recipe)} gives`));
    }
    if (bad.length) again.push("  " + gen.again);
    built.push({ gen, files: b.files });
  }
  if (disagree.length) {
    throw new Error(["the rows don't match their recipes - nothing written:", ...disagree,
      "either a recipe or its generator changed without its rows, or a game update changed the game's appearance.2da or placeables.2da " +
      "(the rows copy the game's rows).",
      "rebuild the rows and commit them (each command rewrites its rows in data\\rows; --files keeps its models out of data\\files; " +
      "the VFX Bench's Save does the same):", ...again,
      "or, for a build that won't be installed, NWN5E_DERIVED=skip builds without the game-derived models (build.bat)."].join("\n"));
  }

  // One burst (LD): the game's placeable models whose looping "dead" re-fires a detonate, without it, under their own
  // names. The game's resources as the generators read them (spell_bench\game_data.js: its keys, ovr\ over them).
  const res = require(path.join(TOOLS, "spell_bench", "game_data.js")).resources(pre.game);
  const burst = O.derive({ models: O.MODELS, res, reader: pre.reader });
  for (const { model, why } of burst.skipped) say(`build_derived: one burst: skipped ${model} - ${why}`);
  const forms = ["compiled", "ascii"].map((f) => [f, burst.files.filter((x) => x.form === f).length]).filter(([, n]) => n).map(([f, n]) => `${n} ${f}`);
  const taken = burst.files.reduce((n, x) => n + x.dropped, 0);
  say(`build_derived: one burst (tools/data/oneburst.js): ${burst.files.length} of the game's placeable models, their looping "${O.ANIM}" ` +
    `without its ${O.EVENT} (${taken} event${taken === 1 ? "" : "s"})${forms.length ? ", " + forms.join(", ") : ""}`);
  // #166 part 2: the placeables' bounds (tools/data/placeable_bounds.js) - the visible geometry of every placeables.2da
  // model and its walkmesh's footprint - into the build's data\placeable_bounds.txt beside data\files, for the DLL's own
  // sight lines and cover. The game's numbers: written into the build, never the repo.
  const bounds = require("./placeable_bounds.js").derive({ res, reader: pre.reader,
    twoDa: require(path.join(TOOLS, "spell_bench", "game_data.js")).twoDa });
  const boundsFile = path.join(path.dirname(OUT), "placeable_bounds.txt");
  if (!CHECK) {
    fs.mkdirSync(path.dirname(boundsFile), { recursive: true });  // (a build's data\ exists; a check's scratch --out may not yet)
    fs.writeFileSync(boundsFile, bounds.text);
  }
  say(`build_derived: placeable bounds (tools/data/placeable_bounds.js): ${bounds.rows} placeables.2da rows from ${bounds.models} ` +
    `models, ${bounds.misses} with a model it can't read or nothing drawn, ${bounds.absent} naming a model the game ` +
    `doesn't have (placeholders) - ${CHECK ? "would write" : "written"} ${boundsFile}`);
  const ONE_BURST = new Set(O.MODELS.map((m) => m.toLowerCase() + ".mdl"));
  built.push({ gen: { label: "one burst", oneBurst: true }, files: burst.files.map((x) => ({ name: x.name, bytes: x.bytes })) });

  // The files into --out, under the generators' ownership rule: a model or .pwk carrying its generator's mark, a texture
  // its made.txt lists in data\files, a baked texture by its name (files.js's "baked"), a shape's soft texture - or an
  // Additive one's TXI (M1b) - by its name (files.js's "shape"), a moving texture's material by its n5u_ name (M2:
  // files.js's "material"), a library texture's copy (DC: its picture and TXI) by its n5l_ name (files.js's "copy") and
  // the shader it names by its first line (files.js's "shader"). Kind "texture", not files.js's "binary": nothing is
  // noted in any made.txt.
  const isBake = (name) => /\.tga$/i.test(name) && F.BAKED.test(name.replace(/\.tga$/i, ""));
  const isShape = (name) => /\.(tga|txi)$/i.test(name) && F.SHAPE_TEXTURE.test(name.replace(/\.(tga|txi)$/i, ""));
  const isMaterial = (name) => /\.(mtr|tga|dds|txi)$/i.test(name) && F.MOTION_TEXTURE.test(name.replace(/\.(mtr|tga|dds|txi)$/i, ""));
  const isCopy = (name) => /\.(tga|dds|txi)$/i.test(name) && !!F.COPY_TEXTURE && F.COPY_TEXTURE.test(name.replace(/\.(tga|dds|txi)$/i, ""));
  const isShader = (name) => name.toLowerCase() === F.MOTION_SHADER + ".shd";
  // The Spiritual Weapon's three models (the generated blade's committed fallback, or a look built over it): the model
  // writer's mark line, "# nwn5e VFX editor (!vfxedit): <name>" (vfx_model.cpp WriteVfxModel).
  const SWP_MODEL = /^n5_swp_(idle|fly|hit)\.mdl$/i, SWP_MARK = "# nwn5e VFX editor (!vfxedit): ";
  // A one-burst copy (LD): kind "oneburst", owned by its name - one of oneburst.js's MODELS (only it makes them in a
  // build's data\files, and the game's model fixes the bytes).
  const writes = [], byName = new Map();
  for (const { gen, files } of built) {
    const owned = gen.oneBurst ? (w) => ONE_BURST.has(path.basename(w.file).toLowerCase())
      : W.ownership({ repo: REPO, manifest: gen.made, mark: gen.mark, kinds: { texture: (w) => gen.made.has("data/files/" + path.basename(w.file)),
        // (the Spiritual Weapon's: one of its three models, carrying the model writer's line for its own name)
        swpmodel: (w, old) => SWP_MODEL.test(path.basename(w.file)) && F.carriesMark(old, SWP_MARK + path.basename(w.file).replace(/\.mdl$/i, "")) } });
    for (const f of files) {
      const had = byName.get(f.name.toLowerCase());
      if (had) {
        if (!had.bytes.equals(f.bytes)) throw new Error(`${f.name}: ${had.gen.label} and ${gen.label} both make it, differently`);
        continue;
      }
      const kind = gen.oneBurst ? "oneburst" : gen.swp && SWP_MODEL.test(f.name) ? "swpmodel" : gen.mark && F.carriesMark(f.bytes, gen.mark) ? "text"
        : isBake(f.name) ? "baked" : isShape(f.name) ? "shape"
        : isMaterial(f.name) ? "material" : isCopy(f.name) ? "copy" : isShader(f.name) ? "shader" : "texture";
      const w = { file: path.join(OUT, f.name), bytes: f.bytes, kind, owned, gen };
      byName.set(f.name.toLowerCase(), w);
      writes.push(w);
    }
  }
  const results = W.writeAll(writes, (w, old) => w.owned(w, old), { repo: REPO, manifest: new Set(), dry: CHECK });
  let changed = 0;
  for (const [state, file] of results) {
    if (state !== "unchanged") changed++;
    say((state === "unchanged" ? "unchanged" : CHECK ? "would write" : "written").padEnd(CHECK ? 11 : 9) + " " + path.basename(file));
  }
  const count = `${results.length} file${results.length === 1 ? "" : "s"} in ${shown(OUT)}`;
  say(CHECK ? `build_derived: --check, nothing written: ${count}: ${changed} would be written, ${results.length - changed} unchanged`
    : `build_derived: ${count}: ${changed} written, ${results.length - changed} unchanged`);
}

try {
  main();
} catch (e) {
  console.error("build_derived: " + e.message);
  process.exitCode = e instanceof Missing ? 2 : 1;
}
