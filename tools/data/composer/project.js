// The Composer's projects (nwn5e-ee #39; the owner, 2026-11-16: "let's build something more versatile that will allow
// other things to be built" - "placeables and meshes for spells"), their pure parts: reading a project file
// (tools\data\composer\<name>.json, README.md), the model names and labels, the free rows, the guide's outline, the use
// point, a new part's scale, a spell mesh's draft and the project file's text. build.js beside this file builds a
// project; the VFX Bench's Composer edits one, with these same parts. Nothing here touches the disk or the game.
// Plain UMD: Node's require, or a page's self.ComposerProject. Nothing is required when it loads: floatdisk.js (F) and the
// bench's core.js are passed in where they're needed - but for projectText, which writes with floatdisk.js's inline (the
// one JSON writer, composition.json's too), found when it runs: the page's self.FloatDisk, else Node's require.
(function (root) {
"use strict";

const MARK = "# made by nwn5e-ee tools/data/composer/build.js from a Composer project - change it in the VFX Bench and save, not this file";
const KINDS = ["placeable", "spellmesh"];
const NAME = /^[a-z0-9]{1,8}$/;  // so a model, plc_n5<name>_<k>, fits a resref's 16 characters
const VARIANT = /^[a-z0-9]{1,4}$/;
const MAX_VARIANTS = 9;  // _<k> one digit: an 8-letter name's 9th placeable is 16 characters (pre-flight F16)
// The placeables.2da rows the Composer gives (free in data\rows, data\looks, the game's table - its users' blank rows -
// and the served table; clear of the game's BD_RESERVED rows), and the row a placeable copies without a better one.
const ROWS = [14100, 14199];
const SOURCE_ROW = 189;  // TreasureSml, as the Floating Disk's piles
const SHAPES = ["circle", "rect", "none"];
const WALKS = ["through", "blocks"];

const isNum = (v) => typeof v === "number" && Number.isFinite(v);
const round5 = (v) => { const r = Math.round(v * 1e5) / 1e5; return r === 0 ? 0 : r; };  // 0.01 mm, no -0

// --- reading a project ------------------------------------------------------------------------------------------------
// guide { shape: circle | rect | none, diameter (circle), width along x, length along y (rect), height } -> the same with
// every key (null where the shape has none; height 0 when left out).
function readGuide(g) {
  if (!g || typeof g !== "object" || !SHAPES.includes(g.shape)) throw new Error("guide.shape: circle, rect or none");
  const height = g.height === undefined ? 0 : g.height;
  if (!isNum(height)) throw new Error("guide.height: a number (metres)");
  const size = (k) => {
    if (!isNum(g[k]) || g[k] <= 0) throw new Error(`guide.${k}: a number above 0`);
    return g[k];
  };
  return { shape: g.shape, diameter: g.shape === "circle" ? size("diameter") : null, width: g.shape === "rect" ? size("width") : null,
    length: g.shape === "rect" ? size("length") : null, height };
}

// A project's JSON -> { kind, name, guide, placeable: { copy, use, walk } | null, variants: [{ name, model, row, parts }] }.
// Each part through F.readPart (floatdisk.js: the Floating Disk's parts), an own file from `baseDir`. A variant's model is
// always its place's name (modelName), never the file's (pre-flight F15); its row (a placeable's) null until it has one.
// Every error names its field. An empty variant reads (a new project has one); build.js refuses to build it.
function readProject(json, baseDir, F) {
  const j = json && typeof json === "object" ? json : {};
  if (!KINDS.includes(j.kind)) throw new Error("kind: " + KINDS.join(" or "));
  if (typeof j.name !== "string" || !NAME.test(j.name)) throw new Error("name: 1-8 letters or digits, lower-case");
  const guide = readGuide(j.guide);
  let placeable = null;
  if (j.kind === "placeable") {
    const s = j.placeable === undefined || j.placeable === null ? {} : j.placeable;
    if (typeof s !== "object" || Array.isArray(s)) throw new Error('placeable: { "copy", "use", "walk" }');
    const copy = s.copy === undefined ? null : s.copy, use = s.use === undefined ? null : s.use, walk = s.walk === undefined ? "through" : s.walk;
    if (copy !== null && (!Number.isInteger(copy) || copy < 0)) throw new Error("placeable.copy: a placeables.2da row (a whole number), or null");
    if (use !== null && (!Array.isArray(use) || use.length !== 2 || !use.every(isNum))) throw new Error("placeable.use: [x, y] in metres, or null");
    if (!WALKS.includes(walk)) throw new Error("placeable.walk: through or blocks");
    placeable = { copy, use, walk };
  }
  if (!Array.isArray(j.variants) || !j.variants.length || j.variants.length > MAX_VARIANTS) {
    throw new Error(`variants: a list of 1 to ${MAX_VARIANTS} variants`);
  }
  const names = new Set(), rows = new Map(), count = j.variants.length;
  const variants = j.variants.map((v, k) => {
    const where = `variants[${k}]`;
    if (!v || typeof v !== "object") throw new Error(`${where}: a variant ({ "name": "1", "parts": [] })`);
    if (typeof v.name !== "string" || !VARIANT.test(v.name) || names.has(v.name)) throw new Error(`${where}.name: 1-4 letters or digits, unique`);
    names.add(v.name);
    let row = null;
    if (j.kind === "placeable" && v.row !== undefined && v.row !== null) {
      if (!Number.isInteger(v.row) || v.row < ROWS[0] || v.row > ROWS[1]) throw new Error(`${where}.row: ${ROWS[0]}-${ROWS[1]}`);
      if (rows.has(v.row)) throw new Error(`${where}.row: ${v.row} is variants[${rows.get(v.row)}]'s too - a row a variant`);
      rows.set(v.row, k);
      row = v.row;
    }
    if (!Array.isArray(v.parts)) throw new Error(`${where}.parts: a list of parts`);
    const parts = v.parts.map((p, i) => F.readPart(p, `${where}.parts[${i}]`, baseDir));
    return { name: v.name, model: modelName(j.kind, j.name, k + 1, count), row, parts };
  });
  return { kind: j.kind, name: j.name, guide, placeable, variants };
}

// --- names ------------------------------------------------------------------------------------------------------------
// The k-th variant's model (k from 1): plc_n5<name> / n5m_<name>, with _<k> when the project has more than one.
const modelName = (kind, name, k, count) => (kind === "spellmesh" ? "n5m_" : "plc_n5") + name + (count > 1 ? "_" + k : "");
// Its placeables.2da Label: N5_<Name>, N5_<Name>_<k>.
const labelOf = (name, k, count) => "N5_" + name.charAt(0).toUpperCase() + name.slice(1) + (count > 1 ? "_" + k : "");

// --- rows -------------------------------------------------------------------------------------------------------------
// The rows an nwn5e rows file sets (data\README.md): a line starting with a row number with a value other than ****.
function rowsUsed(text) {
  const out = new Set();
  for (const line of String(text).split(/\r?\n/)) {
    const t = [];
    for (const m of line.matchAll(/"([^"]*)"|(\S+)/g)) {
      if (m[2] !== undefined && m[2].startsWith("#")) break;  // a comment
      t.push(m[1] !== undefined ? m[1] : m[2]);
    }
    if (/^\d+$/.test(t[0] || "") && t.slice(1).some((v) => v !== "****")) out.add(Number(t[0]));
  }
  return [...out].sort((a, b) => a - b);
}
// The rows a table ({ columns, rows: Map(row -> { column: value }) }: the game's placeables.2da) uses: a ModelName
// neither **** nor USER (its users' blank rows are USER).
function tableRowsUsed(table) {
  const out = [];
  for (const [row, v] of table.rows) if (!["****", "user", ""].includes(String(v.ModelName || "").toLowerCase())) out.push(row);
  return out.sort((a, b) => a - b);
}
// `count` rows for a project's variants: its `own` rows kept, in order, then the lowest of ROWS neither `taken` (a list
// or a Set) nor its own.
function freeRows(taken, count, own = []) {
  const out = own.filter((r) => Number.isInteger(r)).slice(0, count);
  const busy = new Set([...taken, ...out]);
  for (let r = ROWS[0]; r <= ROWS[1] && out.length < count; r++) if (!busy.has(r)) out.push(r);
  if (out.length < count) throw new Error(`the Composer's placeable rows (${ROWS[0]}-${ROWS[1]}) are all taken`);
  return out;
}
// The game's row a placeable's rows copy (all but Label and ModelName): the card's (placeable.copy), else the first
// placeable part's (the first part, variant by variant, that is a game model some placeables.2da row names - the lowest
// such row: a pile of crates sounds like a crate), else SOURCE_ROW. `project` as readProject gives it; `table` the game's
// placeables.2da.
function sourceRow(project, table) {
  if (project.placeable && project.placeable.copy !== null && project.placeable.copy !== undefined) return project.placeable.copy;
  const rowOf = new Map();
  for (const [row, v] of [...table.rows].sort((a, b) => a[0] - b[0])) {
    const m = String(v.ModelName || "").toLowerCase();
    if (m && !rowOf.has(m)) rowOf.set(m, row);
  }
  for (const v of project.variants) for (const p of v.parts) if (p.source.kind === "model" && rowOf.has(p.source.model)) return rowOf.get(p.source.model);
  return SOURCE_ROW;
}

// --- the guide --------------------------------------------------------------------------------------------------------
// The guide's outline on the ground, [[x, y], ...] counter-clockwise from above: a circle's `sides` points at its radius
// (the first at +x), a rectangle's 4 corners (the first at +x +y), none without a guide.
function outline(guide, sides = 16) {
  if (guide && guide.shape === "circle") {
    const r = guide.diameter / 2;
    return Array.from({ length: sides }, (_, i) => [round5(r * Math.cos((2 * Math.PI * i) / sides)), round5(r * Math.sin((2 * Math.PI * i) / sides))]);
  }
  if (guide && guide.shape === "rect") {
    const x = round5(guide.width / 2), y = round5(guide.length / 2);
    return [[x, y], [-x, y], [-x, -y], [x, -y]];
  }
  return [];
}
// Where a creature stands to use a placeable by default: 0.1 m past the guide's front, at -y (a placeable's front: the
// game's own use points and hands are there - PLC_A03's use point at y -0.91, PLC_B04's hand at -0.5).
function defaultUse(guide) {
  const front = guide && guide.shape === "circle" ? guide.diameter / 2 : guide && guide.shape === "rect" ? guide.length / 2 : null;
  return front === null ? [0, -0.5] : [0, -round5(front + 0.1)];
}
// A new part's scale: its footprint (lo, hi: its box at scale 1) within 0.4 of the guide's diameter or longer side, never
// above 1 (the Floating Disk's 0.35 m, 0.4 x 0.9 m, for any guide); 1 without a guide. Rounded 0.001, never 0.
function newScale(lo, hi, guide) {
  const span = guide && guide.shape === "circle" ? guide.diameter : guide && guide.shape === "rect" ? Math.max(guide.width, guide.length) : null;
  const foot = Math.max(hi[0] - lo[0], hi[1] - lo[1]);
  if (span === null || !(foot > 0)) return 1;
  return Math.max(0.001, Math.round(Math.min(1, (0.4 * span) / foot) * 1000) / 1000);
}
// How far a placed part ({ lo, hi, reach }: placePart's) reaches past the guide, in metres: a circle's farthest vertex
// past its radius; a rectangle's farthest footprint corner past its edges (the distance from the rectangle); 0 without.
function past(guide, placed) {
  if (guide && guide.shape === "circle") return Math.max(0, placed.reach - guide.diameter / 2);
  if (guide && guide.shape === "rect") {
    const over = (k, half) => Math.max(0, Math.max(Math.abs(placed.lo[k]), Math.abs(placed.hi[k])) - half);
    return Math.hypot(over(0, guide.width / 2), over(1, guide.length / 2));
  }
  return 0;
}

// --- a spell mesh's draft ---------------------------------------------------------------------------------------------
// A variant's meshes (placed and lowered, their names unique) as the VFX Bench's draft of a mesh: MARK, then one plane a
// mesh, written by the bench's own core.js (`core`). LF; build.js writes it CRLF, as the bench writes drafts.
function draftText(core, model, meshes) {
  const planes = meshes.map((m) => core.plane({ name: m.name, bitmap: m.bitmap || "null", verts: m.verts, tverts: m.tverts, faces: m.faces,
    diffuse: m.diffuse, ambient: m.ambient, selfillum: m.selfillum, alpha: m.alpha }));
  return MARK + "\n" + core.writeDraft(core.effect({ name: model, duration: true, seconds: 2, attach: "root", planes }));
}

// --- the project file -------------------------------------------------------------------------------------------------
const ABOUT = {
  placeable: ["A Composer project (nwn5e-ee #39): the VFX Bench's Composer edits it, tools/data/composer/build.js builds it (README.md there).",
    "A placeable: each variant is one of the game's placeables - data/files/<model>.mdl and .pwk, and its row in",
    "data/rows/placeables.<name>.txt (14100-14199, given at the first save)."],
  spellmesh: ["A Composer project (nwn5e-ee #39): the VFX Bench's Composer edits it, tools/data/composer/build.js builds it (README.md there).",
    "A spell mesh: each variant is a mesh for spells, n5m_<name>[_<k>] - a draft in the bench's vfxedit/saved, the chunk an emitter",
    "throws or a mesh beat in an effect."],
};
// A new project's JSON: a circle guide on the ground (1 m for a placeable, 0.5 m for a spell mesh), a placeable's
// settings, one empty variant.
function newProject(name, kind) {
  if (!KINDS.includes(kind)) throw new Error("kind: " + KINDS.join(" or "));
  if (typeof name !== "string" || !NAME.test(name)) throw new Error("name: 1-8 letters or digits, lower-case");
  const p = { _about: ABOUT[kind].slice(), kind, name, guide: { shape: "circle", diameter: kind === "placeable" ? 1 : 0.5, height: 0 } };
  if (kind === "placeable") p.placeable = { copy: null, use: null, walk: "through" };
  p.variants = [{ name: "1", parts: [] }];
  return p;
}

// A project's text in composition.json's layout: two-space indents, _about a string a line, each variant a line opening
// its parts and each part a line (note, model | file | shape, at, turn, scale, tilt, then any other key), the rest inline;
// LF, a final newline.
const PART_KEYS = ["note", "model", "file", "shape", "at", "turn", "scale", "tilt"];
const VARIANT_KEYS = ["name", "model", "row"];
const ordered = (o, first) => first.filter((k) => o[k] !== undefined).concat(Object.keys(o).filter((k) => !first.includes(k) && o[k] !== undefined));
// A value on one line: floatdisk.js's inline ({} when empty, an undefined key left out), the page's or Node's.
function floatDisk() {
  if (root.FloatDisk) return root.FloatDisk;
  if (typeof require === "function") return require("../floatdisk/floatdisk.js");
  throw new Error("project.js's projectText needs floatdisk.js (self.FloatDisk): load it first");
}
const inline = (v) => floatDisk().inline(v);
function projectText(json) {
  const pair = (k, v) => JSON.stringify(k) + ": " + v;
  const part = (p) => "{ " + ordered(p, PART_KEYS).map((k) => pair(k, inline(p[k]))).join(", ") + " }";
  const variant = (v, last) => {
    const head = ordered(v, VARIANT_KEYS).filter((k) => k !== "parts").map((k) => pair(k, inline(v[k])));
    const parts = Array.isArray(v.parts) ? v.parts : [];
    const tail = last ? "" : ",";
    if (!parts.length) return ["    { " + head.concat(pair("parts", "[]")).join(", ") + " }" + tail];
    return ["    { " + head.concat(pair("parts", "[")).join(", "), ...parts.map((p, i) => "      " + part(p) + (i < parts.length - 1 ? "," : "")), "    ] }" + tail];
  };
  const top = Object.keys(json).filter((k) => json[k] !== undefined).map((k) => {
    const v = json[k];
    if (k === "_about" && Array.isArray(v)) return [pair(k, "["), ...v.map((s, i) => "    " + JSON.stringify(s) + (i < v.length - 1 ? "," : "")), "  ]"];
    if (k === "variants" && Array.isArray(v)) return [pair(k, "["), ...v.flatMap((x, i) => variant(x, i === v.length - 1)), "  ]"];
    return [pair(k, inline(v))];
  });
  const lines = ["{"];
  top.forEach((chunk, i) => {
    chunk.forEach((l, j) => lines.push((j === 0 ? "  " : "") + l + (j === chunk.length - 1 && i < top.length - 1 ? "," : "")));
  });
  lines.push("}", "");
  return lines.join("\n");
}

const api = {
  MARK, KINDS, NAME, VARIANT, MAX_VARIANTS, ROWS, SOURCE_ROW,
  readProject, modelName, labelOf, rowsUsed, tableRowsUsed, freeRows, sourceRow, outline, defaultUse, newScale, past, draftText,
  newProject, projectText,
};
if (typeof module === "object" && module.exports) module.exports = api;
else root.ComposerProject = api;
})(typeof self !== "undefined" ? self : this);
