#!/usr/bin/env node
// The Composer's project generator (nwn5e-ee #39; the owner, 2026-11-16: "let's build something more versatile that
// will allow other things to be built" - "placeables and meshes for spells"). A project (README.md; project.js reads it)
// says what each of its variants is made of - the game's placeables and items, or your own .mdl files, each placed,
// turned, tilted and scaled on a guide - and this builds its kind's files from it:
//
// - a placeable: per variant data\files\<model>.mdl (laid out as the game's junk placeables are, its base on the ground
//   at the guide's height) and <model>.pwk (its use point; with Blocks, the guide's outline as ground nothing walks
//   through); one row a variant in data\rows\placeables.<name>.txt (14100-14199), each a game row's copy; the textures
//   your own files' parts bring that the game doesn't have, into data\files (listed in made.txt beside this file);
// - a spell mesh: per variant a VFX Bench draft of its meshes, <saved>\<model>.vfx (the bench's vfxedit\saved: the game
//   writes its model when it runs), with your own files' textures beside it.
//
// The parts are read and placed as the Floating Disk's (..\floatdisk\reader.js and floatdisk.js), through the player's
// own game and nwn5e-ee's mesh reader; nothing of the game's is kept in the repo.
//
//   node tools\data\composer\build.js <project.json> [options]
//     --check             read and place everything, print the summary, write nothing (a placeable's variant may have no
//                         row yet: the VFX Bench gives it one at Save)
//     --json              the summary as one line of JSON on stdout (what the VFX Bench reads); an error as { "error" }
//     --saved <folder>    where a spell mesh's drafts go (needed to build one)
//     --game <folder>     the game (default: NWN_GAME_DIR, nwn5e.ini's [loader] game_dir, the Steam folder)
//     --tools <folder>    nwn5e-tools (default: NWN5E_TOOLS, else beside this repo)
//     --reader <exe>      the mesh reader (default: NWN5E_MDLMESH, else build\floatdisk\mdlmesh.exe, built if needed)
//     --files <folder>    where the models and textures go (default: data\files)
//     --rows <folder>     where the rows files go (default: data\rows)
//
// An own file's path (a part's "file") is relative to this folder, whatever folder the project comes from. It never
// overwrites a file it didn't make: a model, .pwk, rows file or draft must carry project.js's MARK line; a texture in
// this repo must be in made.txt, one in <saved> named by this project's marked draft there. Every file is checked
// before any is written.
"use strict";
const fs = require("fs");
const path = require("path");
const F = require("../floatdisk/floatdisk.js");
const R = require("../floatdisk/reader.js");
const P = require("./project.js");

const REPO = path.resolve(__dirname, "..", "..", "..");
const MANIFEST = path.join(__dirname, "made.txt");
const LOOKS = path.join(REPO, "data", "looks");

// --- options ------------------------------------------------------------------------------------------------------
const argv = process.argv.slice(2);
const VALUED = ["saved", "game", "tools", "reader", "files", "rows"];
const option = (name, fallback) => {
  const i = argv.indexOf("--" + name);
  return i >= 0 && i + 1 < argv.length ? argv[i + 1] : fallback;
};
const PROJECT = argv.find((a, i) => !a.startsWith("--") && !(i > 0 && argv[i - 1].startsWith("--") && VALUED.includes(argv[i - 1].slice(2))));
const CHECK = argv.includes("--check");
const JSON_OUT = argv.includes("--json");
const TOOLS = path.resolve(option("tools", process.env.NWN5E_TOOLS || path.join(REPO, "..", "nwn5e-tools")));
const BUILD = path.resolve(process.env.NWN5E_BUILD || path.join(TOOLS, "..", "nwn5e-ee", "build"));
const FILES = path.resolve(option("files", path.join(REPO, "data", "files")));
const ROWS_DIR = path.resolve(option("rows", path.join(REPO, "data", "rows")));
const SAVED = option("saved", "") ? path.resolve(option("saved", "")) : null;
const READER = option("reader", process.env.NWN5E_MDLMESH || "");

function need(file, what) {
  if (!fs.existsSync(file)) throw new Error(`${what} isn't there: ${file}`);
  return file;
}
// What the nwn5e-tools folder gives, loaded when main starts (a bad --tools is an error like any other, JSON included).
let core, gameData, GAME;
function setup() {
  global.window = global.window || {};  // core.js runs in the page and under node
  core = require(need(path.join(TOOLS, "vfx_bench", "public", "core.js"), "nwn5e-tools' VFX Bench core (--tools)"));
  gameData = require(need(path.join(TOOLS, "spell_bench", "game_data.js"), "nwn5e-tools' game data reader (--tools)"));
  const helper = require(need(path.join(TOOLS, "anim_helper", "helper.js"), "nwn5e-tools' anim helper (--tools)"));
  GAME = helper.findGameDir({ game: option("game", ""), buildDir: BUILD });
}

// --- writing ----------------------------------------------------------------------------------------------------------
// A file in this repo by its path from the repo's root ("data/files/x.tga"); null outside it.
const repoPath = (file) => {
  const rel = path.relative(REPO, file);
  return rel.startsWith("..") || path.isAbsolute(rel) ? null : rel.split(path.sep).join("/");
};
const shown = (file) => repoPath(file) || file;
const listing = (dir) => (dir && fs.existsSync(dir) ? fs.readdirSync(dir).sort() : []);
function readManifest() {
  if (!fs.existsSync(MANIFEST)) return new Set();
  return new Set(fs.readFileSync(MANIFEST, "utf8").split(/\r?\n/).map((l) => l.trim()).filter((l) => l && !l.startsWith("#")));
}
// Each write { file, bytes, kind }: "text" (a model, .pwk, rows or draft: ours when it carries P.MARK), "binary" (a
// texture in this repo: ours when made.txt lists it), "saved" (a texture beside a spell mesh's draft: ours when one of
// this project's marked drafts there names it, `named`); outside the repo an identical one is anyone's. Every one checked
// before any is written; -> [[ "new" | "rebuilt" | "unchanged", file ]], `manifest` gaining the binary files written.
function writeAll(writes, manifest, named) {
  const owned = (w, old) => {
    if (w.kind === "text") return F.carriesMark(old, P.MARK);
    if (w.kind === "saved") return named.has(path.parse(w.file).name.toLowerCase());
    return repoPath(w.file) !== null && manifest.has(repoPath(w.file));
  };
  for (const w of writes) {
    if (!fs.existsSync(w.file)) continue;
    const old = fs.readFileSync(w.file);
    if (!owned(w, old) && !old.equals(w.bytes)) {
      throw new Error(`${shown(w.file)} is there and wasn't made by this generator: nothing written - move it away or rename it`);
    }
  }
  const results = [];
  for (const w of writes) {
    let state = "new";
    if (fs.existsSync(w.file)) {
      const old = fs.readFileSync(w.file);
      state = old.equals(w.bytes) || (w.kind === "text" && F.sameText(old, w.bytes)) ? "unchanged" : "rebuilt";
    }
    if (state !== "unchanged") {
      fs.mkdirSync(path.dirname(w.file), { recursive: true });
      fs.writeFileSync(w.file, w.bytes);
    }
    if (w.kind === "binary" && repoPath(w.file) !== null) manifest.add(repoPath(w.file));
    results.push([state, w.file]);
  }
  return results;
}

// --- main -----------------------------------------------------------------------------------------------------------
function main() {
  if (!PROJECT) throw new Error("which project? node tools/data/composer/build.js <project.json> [--check] [--json] [--saved <folder>] ...");
  setup();
  const file = path.resolve(PROJECT);
  let json;
  try {
    json = JSON.parse(fs.readFileSync(file, "utf8").replace(/^\uFEFF/, ""));
  } catch (e) {
    throw new Error(`${file}: ${e.code === "ENOENT" ? "no such file" : "not a project's JSON - " + e.message}`);
  }
  // An own file's path from this folder, wherever the project comes from (the bench builds a temporary copy: F2).
  const project = P.readProject(json, __dirname, F);
  const { kind, name, guide } = project;
  const placeable = kind === "placeable";
  const settings = project.placeable;

  // What can be refused before the game is read.
  project.variants.forEach((v, k) => { if (!v.parts.length) throw new Error(`variants[${k}].parts: at least one part`); });
  if (placeable && settings.walk === "blocks" && guide.shape === "none") {
    throw new Error("placeable.walk: Blocks needs a guide (a circle or a rectangle) - its outline is the ground it blocks");
  }
  if (placeable && !CHECK) {
    project.variants.forEach((v, k) => {
      if (v.row === null) throw new Error(`variants[${k}].row: none yet - ${P.ROWS[0]}-${P.ROWS[1]} (the VFX Bench gives one at Save)`);
    });
  }
  if (!placeable && !CHECK && !SAVED) throw new Error("--saved <folder>: where a spell mesh's drafts go (the bench's vfxedit\\saved)");
  if (!fs.existsSync(path.join(GAME, "data", "nwn_base.key"))) throw new Error("no game at " + GAME + " (--game <folder>)");
  const res = gameData.resources(GAME);
  const table = placeable ? gameData.gameTable(GAME, "placeables") : null;

  // A placeable's rows: free everywhere but its own rows file - nwn5e's other rows files and looks, and the game's table
  // (its users' blank rows are USER). A row set in two files makes the launch check skip one of them whole.
  if (placeable) {
    const own = `placeables.${name}.txt`;
    const takenBy = new Map();
    const scan = (dir, keep) => {
      for (const f of listing(dir)) {
        if (!keep(f.toLowerCase())) continue;
        for (const r of P.rowsUsed(fs.readFileSync(path.join(dir, f), "latin1"))) if (!takenBy.has(r)) takenBy.set(r, shown(path.join(dir, f)));
      }
    };
    scan(ROWS_DIR, (f) => f !== own && f.endsWith(".txt") && f.split(".")[0] === "placeables");
    scan(LOOKS, (f) => f.split(".")[0] === "placeables");
    for (const r of P.tableRowsUsed(table)) if (!takenBy.has(r)) takenBy.set(r, "the game's placeables.2da");
    project.variants.forEach((v, k) => {
      if (v.row !== null && takenBy.has(v.row)) throw new Error(`variants[${k}].row: ${v.row} is taken - ${takenBy.get(v.row)} sets it`);
    });
  }
  // A model name the game has is refused, unless it's ours (the game's copy, or the file here, carries the mark).
  for (const v of project.variants) {
    const theirs = res(v.model, 2002);
    if (!theirs) continue;
    const here = placeable ? path.join(FILES, v.model + ".mdl") : SAVED ? path.join(SAVED, v.model + ".vfx") : null;
    if (F.carriesMark(theirs, P.MARK) || (here && fs.existsSync(here) && F.carriesMark(fs.readFileSync(here), P.MARK))) continue;
    throw new Error(`${v.model}: the game has a model of that name - rename the project`);
  }

  // The parts, read once each (a game model, a weapon's pieces or a file of yours: reader.js).
  const reader = R.ensureReader({ repo: REPO, reader: READER, log: JSON_OUT ? console.error : console.log });
  const rd = R.create({ game: GAME, core, gameData, reader });
  const models = new Map();
  try {
    for (const v of project.variants) for (const p of v.parts) if (!models.has(F.partKey(p))) models.set(F.partKey(p), rd.part(p));
  } finally {
    rd.close();
  }

  // Each variant: its parts placed on the guide's height, as the stage shows them; then lowered by it, so the model's
  // origin is the guide's centre at its height (a placeable's base on the ground).
  const h = guide.height;
  const built = project.variants.map((v) => {
    const parts = v.parts.map((p, i) => Object.assign(F.placePart(p, models.get(F.partKey(p)), i, h, "j"), { part: p }));
    const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
    for (const x of parts) for (let k = 0; k < 3; k++) { lo[k] = Math.min(lo[k], x.lo[k]); hi[k] = Math.max(hi[k], x.hi[k]); }
    const meshes = F.uniqueNames(F.lowered([].concat(...parts.map((x) => x.meshes)), h), []);
    return { v, parts, meshes, lo, hi, low: [lo[0], lo[1], lo[2] - h], high: [hi[0], hi[1], hi[2] - h] };
  });

  // Textures: a game model's are the game's; one of your own files' is looked up (the game, then beside the file).
  const ship = new Map();  // texture name -> its copies beside a file
  for (const b of built) {
    for (const x of b.parts) {
      if (x.part.source.kind !== "file") continue;
      for (const m of x.meshes) {
        if (!m.bitmap || ship.has(m.bitmap)) continue;
        const found = rd.findTexture(m.bitmap, [path.dirname(x.part.source.file)]);
        if (!found) throw new Error(`${F.partLabel(x.part)}: texture ${m.bitmap} is neither in the game nor beside the file (${m.bitmap}.tga / .dds)`);
        if (found.where === "beside") ship.set(m.bitmap, found.copies);
      }
    }
  }

  // The summary: per variant what the VFX Bench's stage shows (A2's stage fields: the parts, meshes, triangles, how tall
  // above the guide, its top as the stage has it, and each part's size, height and reach past the guide).
  const round3 = (v) => Math.round(v * 1000) / 1000;
  const say = JSON_OUT ? () => {} : console.log;
  const variants = built.map(({ v, parts, meshes, hi }) => {
    const triangles = meshes.reduce((n, m) => n + m.faces.length / 3, 0);
    say(`${v.model} (variant ${v.name}${placeable ? ", row " + (v.row === null ? "none yet" : v.row) : ""}): ${parts.length} part${parts.length === 1 ? "" : "s"}, ` +
      `${meshes.length} meshes, ${triangles} triangles; ${(hi[2] - h).toFixed(2)} m tall`);
    const list = parts.map((x) => {
      const over = P.past(guide, x);
      say(`  ${F.partLabel(x.part).padEnd(16)} ${(x.hi[0] - x.lo[0]).toFixed(2)} x ${(x.hi[1] - x.lo[1]).toFixed(2)} x ${(x.hi[2] - x.lo[2]).toFixed(2)} m, ` +
        `${(x.lo[2] - h).toFixed(2)}..${(x.hi[2] - h).toFixed(2)} above the guide` + (over > 0.005 ? `, reaches ${(over * 100).toFixed(0)} cm past it` : ""));
      return { label: F.partLabel(x.part), size: [0, 1, 2].map((k) => round3(x.hi[k] - x.lo[k])), from: round3(x.lo[2] - h), to: round3(x.hi[2] - h), past: round3(over) };
    });
    return { name: v.name, model: v.model, row: v.row, parts: parts.length, meshes: meshes.length, triangles, tall: round3(hi[2] - h), top: round3(hi[2]), list };
  });
  for (const [texture, copies] of ship) say(`  texture ${texture}: not in the game - shipped from ${copies.join(", ")}`);

  // The files.
  const writes = [];
  const text = (to, body) => writes.push({ file: to, bytes: Buffer.from(F.crlf(body), "latin1"), kind: "text" });
  const from = `tools/data/composer/${name}.json`;
  const count = built.length;
  if (placeable) {
    for (const b of built) {
      const v = b.v;
      text(path.join(FILES, v.model + ".mdl"), F.pileModelText(v.model, b.meshes, b.low, b.high, [
        `The Composer's project ${name} (${from}), variant ${v.name}: ${b.parts.length} part${b.parts.length === 1 ? "" : "s"}, its base on the`,
        `ground at the guide's height. Laid out as the game's junk placeables (PLC_B04, PLC_C07). Placeables row ${v.row}.`], P.MARK));
      text(path.join(FILES, v.model + ".pwk"), F.pileWalkmeshText(v.model, b.low, b.high, { mark: P.MARK, use: [settings.use ?? P.defaultUse(guide)],
        block: settings.walk === "blocks" ? P.outline(guide) : null }));
    }
    for (const [texture, copies] of ship) {
      for (const c of copies) writes.push({ file: path.join(FILES, texture + path.extname(c).toLowerCase()), bytes: fs.readFileSync(c), kind: "binary" });
    }
    const source = P.sourceRow(project, table);
    const label = (table.rows.get(source) || {}).Label;
    text(path.join(ROWS_DIR, `placeables.${name}.txt`), F.rowsText(table, source,
      built.map((b, k) => ({ row: b.v.row, label: P.labelOf(name, k + 1, count), model: b.v.model })),
      { labelColumn: "Label", modelColumn: "ModelName", mark: P.MARK, about: [
        `placeables.2da: the Composer's project ${name} (${from}), a row a variant.`,
        `Each row is the game's row ${source}${label && label !== "****" ? " (" + label + ")" : ""} with its Label and ModelName changed.`] }));
  } else {
    const dir = SAVED || path.resolve("saved");  // --check without --saved: nothing is written
    for (const b of built) {
      writes.push({ file: path.join(dir, b.v.model + ".vfx"), bytes: Buffer.from(F.crlf(P.draftText(core, b.v.model, b.meshes)), "latin1"), kind: "text" });
    }
    for (const [texture, copies] of ship) {
      for (const c of copies) writes.push({ file: path.join(dir, texture + path.extname(c).toLowerCase()), bytes: fs.readFileSync(c), kind: "saved" });
    }
  }

  // The marked files this project made under another model name (its variants counted 1 and then 2, or back): listed,
  // never deleted (F15). And, beside a spell mesh's drafts, the textures its marked drafts name (ours to rewrite).
  const outDir = placeable ? FILES : SAVED;
  const mine = new RegExp(`^${placeable ? "plc_n5" : "n5m_"}${name}(_[1-9])?\\.(${placeable ? "mdl|pwk" : "vfx"})$`, "i");
  const writing = new Set(writes.map((w) => path.resolve(w.file).toLowerCase()));
  const unused = [], named = new Set();
  for (const f of listing(outDir)) {
    const at = path.join(outDir, f);
    if (!mine.test(f) || !F.carriesMark(fs.readFileSync(at), P.MARK)) continue;
    if (!placeable) {
      let planes = [];
      try { planes = core.readDraft(fs.readFileSync(at, "latin1").replace(/\r/g, "")).planes; } catch (e) { /* a draft that doesn't read names nothing */ }
      for (const pl of planes) named.add(String(pl.bitmap).toLowerCase());
    }
    if (!writing.has(path.resolve(at).toLowerCase())) unused.push(shown(at));
  }

  const result = { kind, check: CHECK, variants, files: [], unused, shipped: [...ship].map(([texture, copies]) => ({ texture, from: copies })), wouldWrite: writes.length };
  const finish = () => {
    for (const f of unused) say(`no longer used: ${f} (made by this generator for ${name} under an earlier name) - delete it if nothing needs it`);
    if (JSON_OUT) console.log(JSON.stringify(result));
  };
  if (CHECK) {
    say(`--check: nothing written (${writes.length} files would be)`);
    finish();
    return;
  }
  const manifest = readManifest();
  const results = writeAll(writes, manifest, named);
  for (const [state, to] of results) {
    say(state.padEnd(9) + " " + shown(to));
    result.files.push({ state, file: shown(to) });
  }
  const body = ["# made by nwn5e-ee tools/data/composer/build.js: the files it wrote in this repo for the Composer's projects that can't",
    "# carry its mark (textures) - it may rewrite these; it refuses any other file in its way", ...[...manifest].sort()].join("\n") + "\n";
  const had = fs.existsSync(MANIFEST);
  if ((had || manifest.size) && (!had || fs.readFileSync(MANIFEST, "utf8").replace(/\r\n/g, "\n") !== body)) fs.writeFileSync(MANIFEST, body);
  finish();
}

try {
  main();
} catch (e) {
  if (JSON_OUT) console.log(JSON.stringify({ error: e.message }));
  console.error("composer: " + e.message);
  process.exit(1);
}
