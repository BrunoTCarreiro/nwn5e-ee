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
//   in composer\*.json, each run with --json into a scratch folder of its own; their models, walkmeshes and textures are
//   then written into --out (build.bat: the build's data\files) - never into this repo's data\ (refused). A spell
//   mesh's project is skipped, and said so: its drafts live in the VFX Bench's saved folder, and the dev layer writes its
//   models when the game runs.
// - The rows stay the repo's: data\rows' appearance.floatdisk.txt, placeables.floatdisk.txt and each project's
//   placeables.<name>.txt are committed text that names the models. Each generator writes its rows over a copy of
//   data\rows, and they must come out unchanged: rows that differ (or are missing) stop the build, nothing written -
//   so it never ships models whose rows disagree.
// - In --out, the generators' one ownership rule (floatdisk\files.js): a model or .pwk there is overwritten only when it
//   carries its generator's mark, a texture only when that generator's made.txt lists it as data/files/<name>; any other
//   file in the way that differs stops it all. Every file is checked before any is written.
// - One line a file (written / unchanged), then a total. --check: the same builds and checks, in the scratch folders
//   only; nothing written into --out, each file said as it would be (would write / unchanged).
// - --game, --tools and --reader default as the generators': the game NWN_GAME_DIR, nwn5e.ini's [loader] game_dir, the
//   Steam folder; nwn5e-tools NWN5E_TOOLS, else beside this repo; the reader NWN5E_MDLMESH, else build\floatdisk\
//   mdlmesh.exe, built with floatdisk\build_mdlmesh.bat (VS 2019 Build Tools) when missing or older than its sources.
//
// Exit codes: 0 done; 2 a prerequisite missing - nwn5e-tools, the game, the mesh reader - one line saying which and how
// to fix it; 1 anything else (a refused build, rows that disagree, a file in the way). build.bat decides what 2 means.
"use strict";
const fs = require("fs");
const os = require("os");
const path = require("path");
const { spawnSync } = require("child_process");
const F = require("./floatdisk/floatdisk.js");
const R = require("./floatdisk/reader.js");
const W = require("./floatdisk/files.js");
const P = require("./composer/project.js");

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
    throw new Missing(`no game at ${game} (no data\\nwn_base.key) - pass --game <folder>, or set NWN_GAME_DIR or nwn5e.ini's [loader] game_dir`);
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
// The Floating Disk, then each Composer project in composer\*.json by name: a placeable's generator, a spell mesh's skip.
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
      skipped.push(file);
      continue;
    }
    const name = json && typeof json.name === "string" ? json.name : path.parse(f).name;
    list.push({ label: `the Composer's project ${name}`, script: path.join(composer, "build.js"), recipe: file, args: [file], mark: P.MARK,
      made: W.readManifest(path.join(composer, "made.txt")), again: `node tools\\data\\composer\\build.js ${shown(file).split("/").join("\\")} --files <a scratch folder>` });
  }
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
  for (const file of skipped) {
    say(`build_derived: skipped ${shown(file)} - a spell mesh: its drafts live in the VFX Bench's saved folder, and the dev layer writes its models when the game runs`);
  }

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
      "a recipe or its generator changed without its rows - rebuild them (each command rewrites its rows in data\\rows; --files keeps its models " +
      "out of data\\files; the VFX Bench's Save does the same) and commit them with the recipe:", ...again].join("\n"));
  }

  // The files into --out, under the generators' ownership rule: a model or .pwk carrying its generator's mark, a texture
  // its made.txt lists in data\files. Kind "texture", not files.js's "binary": nothing is noted in any made.txt.
  const writes = [], byName = new Map();
  for (const { gen, files } of built) {
    const owned = W.ownership({ repo: REPO, manifest: gen.made, mark: gen.mark, kinds: { texture: (w) => gen.made.has("data/files/" + path.basename(w.file)) } });
    for (const f of files) {
      const had = byName.get(f.name.toLowerCase());
      if (had) {
        if (!had.bytes.equals(f.bytes)) throw new Error(`${f.name}: ${had.gen.label} and ${gen.label} both make it, differently`);
        continue;
      }
      const w = { file: path.join(OUT, f.name), bytes: f.bytes, kind: F.carriesMark(f.bytes, gen.mark) ? "text" : "texture", owned, gen };
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
