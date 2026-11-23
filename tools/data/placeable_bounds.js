// The placeables' bounds table (#166 part 2; owner, 2026-11-23: "A, and apply cover to all three" - the engine's sight
// knows only the tileset's walls and shut doors, so nwn5e tests its own lines against the placeables). For each
// placeables.2da row with a model: the model's visible geometry's bounds in model space (mdlmesh --bounds: every mesh
// drawn, through its nodes to the root) and, when the model has a walkmesh (<model>.pwk, ASCII), that walkmesh's
// footprint (x and y). Rows naming a model the game doesn't have (placeables.2da's "user" and "os_reserved"
// placeholders: some 15,000 rows) are counted, not listed. Built at build time from the player's own game by
// build_derived.js into the build's
// data\placeable_bounds.txt (git-ignored: the game's numbers, never committed); the DLL reads it at launch
// (core/object_cover.cpp). What it's used for: the visible bounds are each placeable's box for the sight lines and
// cover; the walkmesh footprint is recorded for the log and a later use - the engine's walking uses it, sight doesn't.
//   derive({ res, reader, twoDa }) -> { text, rows, models, misses, absent }
"use strict";
const fs = require("fs");
const os = require("os");
const path = require("path");
const { execFileSync } = require("child_process");

const MDL = 2002, PWK = 2053, TWODA = 2017;
const BATCH = 150;  // models a mdlmesh run (the command line's length)

// An ASCII walkmesh's footprint: every vertex moved by its node's position (walkmeshes aren't turned) -> [x0, x1, y0, y1]
// or null.
function walkFootprint(text) {
  const lines = String(text).split(/\r?\n/);
  let pos = [0, 0, 0], lo = null, hi = null;
  for (let i = 0; i < lines.length; i++) {
    const t = lines[i].trim().split(/\s+/);
    if (t[0] === "node") pos = [0, 0, 0];
    else if (t[0] === "position" && t.length >= 4) pos = [Number(t[1]), Number(t[2]), Number(t[3])];
    else if (t[0] === "verts" && t.length >= 2) {
      const n = Number(t[1]);
      for (let k = 1; k <= n && i + k < lines.length; k++) {
        const v = lines[i + k].trim().split(/\s+/).map(Number);
        if (v.length < 2 || v.some((x) => !Number.isFinite(x))) continue;
        const x = v[0] + pos[0], y = v[1] + pos[1];
        lo = lo ? [Math.min(lo[0], x), Math.min(lo[1], y)] : [x, y];
        hi = hi ? [Math.max(hi[0], x), Math.max(hi[1], y)] : [x, y];
      }
      i += n;
    }
  }
  return lo ? [lo[0], hi[0], lo[1], hi[1]] : null;
}

function derive({ res, reader, twoDa }) {
  const table = res("placeables", TWODA);
  if (!table) throw new Error("placeable bounds: the game has no placeables.2da");
  const byModel = new Map();  // model (lower case) -> [rows]
  for (const [row, r] of twoDa(table.toString("latin1")).rows) {
    const m = r.ModelName;
    if (!m || m === "****") continue;
    const key = m.toLowerCase();
    if (!byModel.has(key)) byModel.set(key, []);
    byModel.get(key).push(row);
  }
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "n5_bounds-"));
  const bounds = new Map(), why = new Map();
  try {
    const models = [...byModel.keys()].sort();
    for (let at = 0; at < models.length; at += BATCH) {
      const files = [];
      for (const model of models.slice(at, at + BATCH)) {
        const bytes = res(model, MDL);
        if (!bytes) {
          why.set(model, "not in the game");
          continue;
        }
        const file = path.join(dir, model + ".mdl");
        fs.writeFileSync(file, bytes);
        files.push({ model, file });
      }
      if (!files.length) continue;
      const said = execFileSync(reader, ["--bounds", ...files.map((f) => f.file)], { encoding: "latin1", windowsHide: true,
        maxBuffer: 16 << 20 });
      const byFile = new Map(files.map((f) => [path.resolve(f.file).toLowerCase(), f.model]));
      for (const line of said.split(/\r?\n/)) {
        let m = line.match(/^bounds (.+?) (-?[\d.]+) (-?[\d.]+) (-?[\d.]+) (-?[\d.]+) (-?[\d.]+) (-?[\d.]+) \d+$/);
        if (m) {
          const model = byFile.get(path.resolve(m[1]).toLowerCase());
          if (model) bounds.set(model, m.slice(2, 8).map(Number));
          continue;
        }
        m = line.match(/^# model (.+?) (nomesh|unreadable.*)$/);
        if (m) {
          const model = byFile.get(path.resolve(m[1]).toLowerCase());
          if (model) why.set(model, m[2] === "nomesh" ? "nothing drawn" : m[2]);
        }
      }
      for (const f of files) {
        fs.rmSync(f.file, { force: true });
        if (!bounds.has(f.model) && !why.has(f.model)) why.set(f.model, "the reader said nothing");
      }
    }
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
  const out = [
    "# nwn5e placeable bounds (#166 part 2): generated at build time by tools/data/placeable_bounds.js from this",
    "# machine's game - placeables.2da's models through mdlmesh --bounds - never committed (the game's numbers).",
    "# <placeables.2da row> <model> <x0> <x1> <y0> <y1> <z0> <z1>: the visible geometry in model space, metres;",
    "# then, with a walkmesh, \"walk <x0> <x1> <y0> <y1>\": its footprint. A model it couldn't read, or with nothing",
    "# drawn: \"# miss\". Rows naming a model the game doesn't have: counted on the last line (\"absent\").",
  ];
  let rows = 0, misses = 0, absent = 0;
  const f = (x) => (Math.round(x * 1000) / 1000).toString();
  for (const [model, list] of [...byModel].sort((a, b) => a[1][0] - b[1][0])) {
    const b = bounds.get(model);
    if (!b) {
      if (why.get(model) === "not in the game") {
        absent += list.length;
        continue;
      }
      for (const row of list) out.push(`# miss ${row} ${model} ${why.get(model) || "?"}`);
      misses += list.length;
      continue;
    }
    const pwk = res(model, PWK);
    const walk = pwk ? walkFootprint(pwk.toString("latin1")) : null;
    for (const row of list) {
      out.push(`${row} ${model} ${b.map(f).join(" ")}${walk ? " walk " + walk.map(f).join(" ") : ""}`);
      rows++;
    }
  }
  out.push(`# rows ${rows} misses ${misses} absent ${absent}`);
  return { text: out.join("\n") + "\n", rows, models: bounds.size, misses, absent };
}

module.exports = { derive, walkFootprint };
