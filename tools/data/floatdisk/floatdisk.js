// The Floating Disk generator's pure parts (#39; build.js does the reading and writing, check.js tests these): the
// composition, the maths that places the junk, the disk's mesh and texture, the Composer's shapes (a dome, a sphere, a
// cylinder, a cone, a disc and a ring: their meshes and generated texture), a moving texture's maths, material and
// shader (M2), the game files' text (the creature and pile models, the piles' use points, the 2da rows), a compiled
// model's node tree, and an ASCII model made readable by the mesh reader. Nothing here touches the disk or the game.
// Plain UMD: Node's require, or a page's self.FloatDisk (the Composer). modelNodes, carriesMark and sameText use Buffer
// and stay Node-only; tgaBytes and diskTexture give a Buffer in Node and a Uint8Array in a page.
(function (root) {
"use strict";

const MARK = "# made by nwn5e-ee tools/data/floatdisk/build.js from composition.json - edit that and rebuild, not this file";
const TEXTURE_MARK = "nwn5e floatdisk build.js";  // the bench library's copy of the disk's texture: its TGA image ID
const STAGES = ["empty", "small", "medium", "large"];
const PILES = ["small", "medium", "large"];
const SUFFIX = { empty: "empty", small: "small", medium: "med", large: "large" };
const RESREF = /^[a-z0-9_]{1,16}$/;

// The game's rows each new row copies (every column but its label and model), and nwn5e's rows (stable: an appearance
// is saved on the creature). appearance.2da and placeables.2da keep 1000-14999 for their users (blank, NAME
// USER_RESERVED / ModelName USER); a stage's pile has its creature's number.
const SUPERMODEL = "c_invsguy";  // appearance 298, Invisible_Human_Male: the walk, the idles, the nodes
const SOURCE_ROWS = { appearance: 298, placeables: 189 };  // 189 TreasureSml
// Where the disk's appearance rows differ from 298's: TARGETABLE 1, so a click opens its inventory (298's 0 hides it);
// FOOTSTEPTYPE -1, no footsteps, as the game's floating creatures (Lantern Archon 103, Will-o-Wisp, Shadow, Allip).
const APPEARANCE_OVERRIDES = { TARGETABLE: "1", FOOTSTEPTYPE: "-1" };
const ROWS = {
  appearance: { empty: 14000, small: 14001, medium: 14002, large: 14003 },
  placeables: { small: 14001, medium: 14002, large: 14003 },
};
const LABELS = {
  appearance: { empty: "N5_FloatDisk_Empty", small: "N5_FloatDisk_Small", medium: "N5_FloatDisk_Medium", large: "N5_FloatDisk_Large" },
  placeables: { small: "N5_FloatDiskPile_Small", medium: "N5_FloatDiskPile_Medium", large: "N5_FloatDiskPile_Large" },
};
const creatureModel = (stage) => "c_n5fd_" + SUFFIX[stage];
const pileModel = (stage) => "plc_n5fd_" + SUFFIX[stage];

// --- the composition --------------------------------------------------------------------------------------------------
const isNum = (v) => typeof v === "number" && Number.isFinite(v);

// A part's own model file (the owner's piles, 2026-11-16): relative to `baseDir` (tools\data\floatdisk), a .mdl - ASCII
// (what NeverBlender exports) or compiled. Returns its absolute path.
function resolvePartFile(baseDir, file) {
  if (typeof file !== "string" || !file.trim()) throw new Error("a part's file: a path to a .mdl, relative to tools\\data\\floatdisk");
  if (!/\.mdl$/i.test(file.trim())) throw new Error(`a part's file: ${file} - a .mdl model (ASCII or compiled)`);
  return require("path").resolve(baseDir, file.trim());  // Node only: a page has no files of its own
}

// --- a dyed part's colours (a PLT texture baked from the game's palettes) ----------------------------------------------
const PLT_LAYERS = ["skin", "hair", "metal1", "metal2", "cloth1", "cloth2", "leather1", "leather2", "tattoo1", "tattoo2"];  // a PLT's layer ids 0-9
const PLT_PALETTES = { skin: "pal_skin01", hair: "pal_hair01", metal1: "pal_armor01", metal2: "pal_armor02", cloth1: "pal_cloth01",
  cloth2: "pal_cloth01", leather1: "pal_leath01", leather2: "pal_leath01", tattoo1: "pal_tattoo01", tattoo2: "pal_tattoo01" };
const PLT_FIELDS = ["metal1", "metal2", "leather1", "leather2", "cloth1", "cloth2"];  // the card's, in the owner's order
const PLT_LABELS = { metal1: "Metal 1", metal2: "Metal 2", leather1: "Leather 1", leather2: "Leather 2", cloth1: "Cloth 1", cloth2: "Cloth 2" };
const PLT_DEFAULTS = { skin: 0, hair: 0, metal1: 0, metal2: 3, cloth1: 22, cloth2: 23, leather1: 3, leather2: 2, tattoo1: 0, tattoo2: 0 };
const PLT_MAX = 175;
const BAKED = /^n5p_[0-9a-f]{12}$/;

// The ten layers' colours: PLT_DEFAULTS with `c`'s over them. `c`: undefined, or { metal1: 0-175, ... } (PLT_FIELDS only).
function readColours(c, where) {
  if (c === undefined || c === null) return Object.assign({}, PLT_DEFAULTS);
  const ok = c && typeof c === "object" && !Array.isArray(c) && Object.keys(c).every((k) => PLT_FIELDS.includes(k) && Number.isInteger(c[k]) && c[k] >= 0 && c[k] <= PLT_MAX);
  if (!ok) throw new Error(`${where}: { "metal1": 0-${PLT_MAX}, ... } - metal1, metal2, leather1, leather2, cloth1 or cloth2, each a palette number 0-${PLT_MAX}`);
  return Object.assign({}, PLT_DEFAULTS, c);
}
// The ten layers' colours the bake and its name use: `c`'s, each layer it leaves out (undefined or null, or no `c`) its
// default - one rule for bakePlt and bakedName, so a name always matches its pixels.
function fullColours(c) {
  const out = {};
  for (const l of PLT_LAYERS) out[l] = c && c[l] !== undefined && c[l] !== null ? c[l] : PLT_DEFAULTS[l];
  return out;
}
// A part's tint (M1, #39; the owner, 2026-11-18: "for the existing textures I'd like to add tinting options to them as
// right now when using a texture there's no way to change the color"): [r, g, b], each 0 to 1 -> its own copy; white
// ([1, 1, 1]) when it has none (undefined or null) - white is no tint, and no writer writes it (isWhite).
const isWhite = (t) => Array.isArray(t) && t.length === 3 && t.every((c) => c === 1);
function readTint(t, where) {
  if (t === undefined || t === null) return [1, 1, 1];
  if (!Array.isArray(t) || t.length !== 3 || !t.every((c) => isNum(c) && c >= 0 && c <= 1)) throw new Error(`${where}.tint: [r, g, b], each 0 to 1`);
  return t.slice();
}
// The PLT_FIELDS whose colour differs from the defaults (the card's order), or undefined when none does.
function cleanColours(c) {
  if (!c) return undefined;
  const out = {};
  for (const k of PLT_FIELDS) if (c[k] !== undefined && c[k] !== PLT_DEFAULTS[k]) out[k] = c[k];
  return Object.keys(out).length ? out : undefined;
}

// A PLT file (a Buffer or Uint8Array): "PLT V1  ", u32 width at 16, u32 height at 20, then a [shade, layer] byte pair a
// pixel, the picture's bottom row first -> { width, height, shade, layer } (Uint8Arrays in the file's order).
function readPlt(bytes) {
  const u = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
  if (u.length < 24 || String.fromCharCode(...u.subarray(0, 8)) !== "PLT V1  ") throw new Error('not a PLT (no "PLT V1  " header)');
  const u32 = (o) => (u[o] | (u[o + 1] << 8) | (u[o + 2] << 16) | (u[o + 3] << 24)) >>> 0;
  const w = u32(16), h = u32(20), need = 24 + 2 * w * h;
  if (u.length < need) throw new Error(`a PLT cut short: ${w} x ${h} needs ${need} bytes, it has ${u.length}`);
  const shade = new Uint8Array(w * h), layer = new Uint8Array(w * h);
  for (let i = 0; i < w * h; i++) { shade[i] = u[24 + 2 * i]; layer[i] = u[25 + 2 * i]; }
  return { width: w, height: h, shade, layer };
}

// A PLT painted from the game's palettes ({ name: { width, height, data } }, decoded top-down) in `colours` (a layer
// left out or undefined is its default: fullColours) -> { width, height, data: Uint8ClampedArray RGBA, rows top-down }.
// A layer id past 9 is skin.
function bakePlt(plt, palettes, given) {
  const { width, height, shade, layer } = plt;
  const colours = fullColours(given);
  const data = new Uint8ClampedArray(width * height * 4);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const i = (height - 1 - y) * width + x;
      const L = PLT_LAYERS[layer[i]] || "skin", name = PLT_PALETTES[L], pal = palettes[name];
      if (!pal) throw new Error(`${name}: a palette the bake needs`);
      const row = Math.min(colours[L], pal.height - 1);
      const from = (row * pal.width + shade[i]) * 4, to = (y * width + x) * 4;
      data[to] = pal.data[from]; data[to + 1] = pal.data[from + 1]; data[to + 2] = pal.data[from + 2]; data[to + 3] = 255;
    }
  }
  return { width, height, data };
}

function cyrb53(str, seed = 0) {
  let h1 = 0xdeadbeef ^ seed, h2 = 0x41c6ce57 ^ seed;
  for (let i = 0, ch; i < str.length; i++) { ch = str.charCodeAt(i); h1 = Math.imul(h1 ^ ch, 2654435761); h2 = Math.imul(h2 ^ ch, 1597334677); }
  h1 = Math.imul(h1 ^ (h1 >>> 16), 2246822507); h1 ^= Math.imul(h2 ^ (h2 >>> 13), 3266489909);
  h2 = Math.imul(h2 ^ (h2 >>> 16), 2246822507); h2 ^= Math.imul(h1 ^ (h1 >>> 13), 3266489909);
  return 4294967296 * (2097151 & h2) + (h1 >>> 0);
}
// A PLT baked in these colours is a texture of its own: n5p_ and 12 hex digits of its name and ten colours (fullColours).
function bakedName(plt, colours) {
  const full = fullColours(colours);
  return "n5p_" + cyrb53(`${plt.toLowerCase()}:${PLT_LAYERS.map((l) => full[l]).join(",")}`).toString(16).padStart(14, "0").slice(-12);
}

// A top-down RGBA picture as an uncompressed TGA, rows bottom-up as the game's (a Buffer in Node, else a Uint8Array):
// 24-bit, or with `alpha` 32-bit BGRA with 8 alpha bits (a shape's soft texture).
function tgaBytes(image, { alpha = false } = {}) {
  const { width: w, height: h, data } = image, depth = alpha ? 4 : 3;
  const bytes = new Uint8Array(18 + w * h * depth);
  bytes.set([0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, w & 255, w >> 8, h & 255, h >> 8, alpha ? 32 : 24, alpha ? 8 : 0]);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const from = ((h - 1 - y) * w + x) * 4, to = 18 + (y * w + x) * depth;
      bytes[to] = data[from + 2]; bytes[to + 1] = data[from + 1]; bytes[to + 2] = data[from];
      if (alpha) bytes[to + 3] = data[from + 3];
    }
  }
  return typeof Buffer === "function" ? Buffer.from(bytes.buffer, bytes.byteOffset, bytes.length) : bytes;
}

// Meshes with each PLT-only bitmap (isPlt(lower-case name)) renamed for its colours -> { meshes (copies), bakes: [{ name,
// plt, colours: only what isn't a default }] }, each name once in the meshes' order.
function bakeBitmaps(meshes, colours, isPlt) {
  const bakes = [], seen = new Set();
  const out = meshes.map((m) => {
    const plt = String(m.bitmap || "").toLowerCase();
    if (!plt || !isPlt(plt)) return Object.assign({}, m);
    const name = bakedName(plt, colours);
    if (!seen.has(name)) { seen.add(name); bakes.push({ name, plt, colours: cleanColours(colours) || {} }); }
    return Object.assign({}, m, { bitmap: name });
  });
  return { meshes: out, bakes };
}

// One part of a composition (or of a Composer project: tools\data\composer\project.js) read and checked -> { source:
// { kind: "model", model } | { kind: "models", models } (a weapon's pieces) | { kind: "file", file (absolute), given }
// | { kind: "shape", shape (readShape's: every key) }, at, turn, scale, tilt, note, colours (the ten layers'), tint
// (readTint's: white without one), motion (readMotion's, M2: still without one) }. `where` names it in an error
// (stages.small[0]); `baseDir` resolves its file. A shape has no scale (its radius and height size it), no dyes and no
// tint (its colour is on the shape): its colours are the defaults, its tint white. Any part may carry a motion, a shape
// too: it moves the part's textured meshes.
function readPart(p, where, baseDir) {
  if (!p || typeof p !== "object" || Array.isArray(p)) throw new Error(`${where}: a part ({ "model": ..., "at": [x, y, z] })`);
  const hasModel = p.model !== undefined, hasFile = p.file !== undefined, hasShape = p.shape !== undefined;
  if ([hasModel, hasFile, hasShape].filter(Boolean).length !== 1) {
    throw new Error(`${where}: either "model" (a game model), "file" (your own .mdl) or "shape" (a dome and the like) - just one`);
  }
  let source;
  if (hasModel) {
    const list = Array.isArray(p.model) ? p.model : [p.model];
    const models = list.map((m) => String(m || "").toLowerCase());
    if (!list.length || list.length > 4 || !models.every((m) => RESREF.test(m))) throw new Error(`${where}.model: a model's resref, or a list of them (a weapon's pieces)`);
    source = models.length === 1 ? { kind: "model", model: models[0] } : { kind: "models", models };
  } else if (hasFile) {
    let file;
    try { file = resolvePartFile(baseDir, p.file); } catch (e) { throw new Error(`${where}.file: ${e.message}`); }
    source = { kind: "file", file, given: String(p.file) };
  } else {
    source = { kind: "shape", shape: readShape(p.shape, `${where}.shape`) };
    if (p.scale !== undefined) throw new Error(`${where}.scale: a shape has no scale - its radius and height size it`);
    if (p.colours !== undefined) throw new Error(`${where}.colours: a shape has no dyes - its colour is on the shape`);
    if (p.tint !== undefined && p.tint !== null) throw new Error(`${where}.tint: a shape has no tint - its Colour is its colour`);
  }
  if (!Array.isArray(p.at) || p.at.length !== 3 || !p.at.every(isNum)) throw new Error(`${where}.at: [x, y, z] in metres`);
  const turn = p.turn === undefined ? 0 : p.turn, scale = p.scale === undefined ? 1 : p.scale;
  const tilt = p.tilt === undefined ? [0, 0] : p.tilt;
  if (!isNum(turn)) throw new Error(`${where}.turn: degrees`);
  if (!isNum(scale) || scale <= 0) throw new Error(`${where}.scale: a number above 0`);
  if (!Array.isArray(tilt) || tilt.length !== 2 || !tilt.every(isNum)) throw new Error(`${where}.tilt: [degrees about x, degrees about y]`);
  return { source, at: p.at, turn, scale, tilt, note: p.note || "", colours: readColours(p.colours, `${where}.colours`), tint: readTint(p.tint, where),
    motion: readMotion(p.motion, where) };
}

// composition.json's object -> { disk, stages: { empty: [], small: [part], ... } }, each part as readPart gives it.
// `baseDir` resolves a part's file.
function readComposition(c, baseDir) {
  const d = (c && c.disk) || {};
  const disk = {
    diameter: d.diameter, thickness: d.thickness, height: d.height, sides: d.sides === undefined ? 40 : d.sides,
    texture: d.texture === undefined ? "n5t_forcedisk" : String(d.texture).toLowerCase(),
    colour: d.colour, rim: d.rim, alpha: d.alpha, selfillum: d.selfillum === undefined ? [0, 0, 0] : d.selfillum,
  };
  for (const k of ["diameter", "thickness", "height", "alpha"]) if (!isNum(disk[k]) || disk[k] <= 0) throw new Error(`disk.${k}: a number above 0`);
  if (!Number.isInteger(disk.sides) || disk.sides < 8 || disk.sides > 96) throw new Error("disk.sides: a whole number, 8 to 96");
  for (const k of ["colour", "rim", "selfillum"]) {
    if (!Array.isArray(disk[k]) || disk[k].length !== 3 || !disk[k].every((v) => isNum(v) && v >= 0 && v <= 1)) throw new Error(`disk.${k}: [r, g, b], each 0 to 1`);
  }
  if (disk.alpha > 1) throw new Error("disk.alpha: 0 to 1");
  if (!RESREF.test(disk.texture)) throw new Error("disk.texture: a resref (a-z 0-9 _, up to 16)");
  const stages = { empty: [] };
  for (const stage of PILES) {
    const list = ((c && c.stages) || {})[stage];
    if (!Array.isArray(list)) throw new Error(`stages.${stage}: a list of parts`);
    stages[stage] = list.map((p, i) => readPart(p, `stages.${stage}[${i}]`, baseDir));
  }
  return { disk, stages };
}
// A part's source as one string (a shape's as it's written: cleanShape's), and its name on the page (dome 4.57 m).
const partKey = (part) => (part.source.kind === "model" ? "model:" + part.source.model
  : part.source.kind === "models" ? "models:" + part.source.models.join("+")
    : part.source.kind === "shape" ? "shape:" + inline(cleanShape(part.source.shape)) : "file:" + part.source.file.toLowerCase());
const partLabel = (part) => (part.source.kind === "model" ? part.source.model
  : part.source.kind === "models" ? part.source.models.join(" + ")
    : part.source.kind === "shape" ? `${part.source.shape.type} ${num(part.source.shape.radius)} m` : part.source.given);

// A list of modelSpaceMeshes results (a weapon's pieces) as one model: the meshes in order, the box their union.
function mergeModels(list) {
  const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
  for (const m of list) for (let k = 0; k < 3; k++) { lo[k] = Math.min(lo[k], m.lo[k]); hi[k] = Math.max(hi[k], m.hi[k]); }
  return { meshes: [].concat(...list.map((m) => m.meshes)), lo, hi };
}

// A value as JSON on one line: [a, b], { "k": v } ({} when empty, an undefined key left out), else JSON.stringify's. The
// one writer for composition.json and the Composer's projects (project.js's projectText calls it).
function inline(v) {
  if (Array.isArray(v)) return "[" + v.map(inline).join(", ") + "]";
  if (v && typeof v === "object") {
    const keys = Object.keys(v).filter((k) => v[k] !== undefined);
    return keys.length ? "{ " + keys.map((k) => JSON.stringify(k) + ": " + inline(v[k])).join(", ") + " }" : "{}";
  }
  return JSON.stringify(v);
}
// composition.json's text in its own layout (the Composer saves it): two-space indents, _about a string a line, the disk
// a key a line, each stage a part a line (note, model | file | shape, at, turn, scale, tilt, colours, tint - never white,
// which is no tint - motion - its non-zero keys, cleanMotion's, never a still one - then any other key); arrays and a
// shape inline.
function compositionText(c) {
  const PART_KEYS = ["note", "model", "file", "shape", "at", "turn", "scale", "tilt", "colours", "tint", "motion"];
  const part = (p) => {
    const written = (k) => p[k] !== undefined && !(k === "tint" && isWhite(p[k])) && !(k === "motion" && !hasMotion(p[k]));
    const keys = PART_KEYS.filter(written).concat(Object.keys(p).filter((k) => !PART_KEYS.includes(k) && written(k)));
    return "{ " + keys.map((k) => JSON.stringify(k) + ": " + inline(k === "motion" ? cleanMotion(p[k]) : p[k])).join(", ") + " }";
  };
  const block = (o, indent, line) => Object.keys(o).filter((k) => o[k] !== undefined).map((k, i, a) => indent + line(k, o[k]) + (i < a.length - 1 ? "," : ""));
  const top = Object.keys(c).filter((k) => c[k] !== undefined).map((k) => {
    const v = c[k], key = JSON.stringify(k) + ": ";
    if (k === "_about" && Array.isArray(v)) return [key + "[", ...v.map((s, i) => "    " + JSON.stringify(s) + (i < v.length - 1 ? "," : "")), "  ]"];
    if (k === "disk") return [key + "{", ...block(v, "    ", (kk, vv) => JSON.stringify(kk) + ": " + inline(vv)), "  }"];
    if (k === "stages") {
      return [key + "{",
        ...Object.keys(v).flatMap((stage, i, a) => {
          const parts = v[stage];
          const rows = parts.map((p, j) => "      " + part(p) + (j < parts.length - 1 ? "," : ""));
          const tail = i < a.length - 1 ? "]," : "]";
          return [`    ${JSON.stringify(stage)}: [`, ...rows, "    " + tail];
        }), "  }"];
    }
    return [key + inline(v)];
  });
  const lines = ["{"];
  top.forEach((chunk, i) => {
    chunk.forEach((l, j) => lines.push((j === 0 ? "  " : "") + l + (j === chunk.length - 1 && i < top.length - 1 ? "," : "")));
  });
  lines.push("}", "");
  return lines.join("\n");
}

// --- maths ----------------------------------------------------------------------------------------------------------
function quat(o) {  // axis-angle (x y z radians, as a model writes it) -> quaternion (x y z w)
  const len = Math.hypot(o[0], o[1], o[2]), half = (o[3] || 0) / 2;
  if (!len || !o[3]) return [0, 0, 0, 1];
  const s = Math.sin(half) / len;
  return [o[0] * s, o[1] * s, o[2] * s, Math.cos(half)];
}
function axisAngle(q) {  // quaternion -> axis-angle; no turn -> 0 0 0 0
  const w = Math.max(-1, Math.min(1, q[3])), s = Math.sqrt(Math.max(0, 1 - w * w)), angle = 2 * Math.acos(w);
  if (s < 1e-6 || angle < 1e-6) return [0, 0, 0, 0];
  return [q[0] / s, q[1] / s, q[2] / s, angle];
}
function qrot(q, v) {
  const c1 = [q[1] * v[2] - q[2] * v[1], q[2] * v[0] - q[0] * v[2], q[0] * v[1] - q[1] * v[0]];
  const c2 = [q[1] * c1[2] - q[2] * c1[1], q[2] * c1[0] - q[0] * c1[2], q[0] * c1[1] - q[1] * c1[0]];
  return [v[0] + 2 * (q[3] * c1[0] + c2[0]), v[1] + 2 * (q[3] * c1[1] + c2[1]), v[2] + 2 * (q[3] * c1[2] + c2[2])];
}
const clamp01 = (v) => Math.max(0, Math.min(1, v));
const smooth = (e0, e1, x) => { const t = clamp01((x - e0) / (e1 - e0)); return t * t * (3 - 2 * t); };  // smoothstep: the textures' soft edges
const round = (v) => Math.round(v * 1e5) / 1e5;  // 0.01 mm
function num(v) {  // a model's number: up to 5 decimals, no exponent, no -0
  const r = round(v);
  return Object.is(r, -0) || r === 0 ? "0" : String(r);
}

// A mesh's transparencyhint (T1b; the owner, 2026-11-18: n5_zot's see-through meshes cut away in game - "go"): the game's
// word for a see-through mesh, kept as nwn5e-ee's vfx_model.cpp keeps one - a whole number, 0 to HINT_MAX (the game's own
// models' range): text by its leading digits, base 10, as the game's parser and vfx_model.cpp's strtol read an integer
// ("1.7" is 1, "+2" 2; not a number 0) - before them only what C's isspace skips (space, \t, \n, \v, \f, \r: a no-break
// space stops it, 0); a number by its whole part (not finite 0); below 0 is 0, above HINT_MAX the most. The bench's
// core.js hintValue keeps the same rule (the Composer's check.js compares them).
const HINT_MAX = 5;
function hintValue(v) {
  const digits = typeof v === "number" ? null : /^[ \t\n\v\f\r]*([+-]?\d+)/.exec(String(v));
  const n = typeof v === "number" ? (Number.isFinite(v) ? Math.trunc(v) : 0) : digits ? parseInt(digits[1], 10) : 0;
  return Math.min(HINT_MAX, Math.max(0, n));
}
// What both writers write for a mesh - meshLines (a placeable's model, the Floating Disk's) and a draft's plane (the
// Composer's project.js draftText, build.js's drafts): its own transparencyhint (every mesh the generator gives carries
// one: a model's its model's, a shape's and the disk's from their see-through-ness), else - a mesh without one - 1 when
// it's see-through (transparent), else 0.
function meshHint(m) {
  const own = m.transparencyhint;
  return typeof own === "number" && Number.isFinite(own) ? hintValue(own) : m.transparent ? 1 : 0;
}

// A read model (a draft's nodes and planes, as core.readDraft gives them) -> its drawn meshes with every vertex in the
// model's space (each mesh's own turn and place, then each parent's; a parent the draft doesn't have is the root). A
// render-0 mesh (a damage box, a shadow mesh) is left out. { meshes: [{ name, bitmap, verts, tverts, faces, diffuse,
// ambient, selfillum, alpha, transparencyhint (the model's own, by hintValue; a plane without one 0) }], lo, hi }
function modelSpaceMeshes(fx, label) {
  const parents = new Map();
  for (const n of fx.nodes || []) if (!parents.has(n.name)) parents.set(n.name, n);
  for (const p of fx.planes || []) if (!parents.has(p.name)) parents.set(p.name, p);
  const place = (v, item) => {
    let out = v;
    for (let o = item, depth = 0; o && depth < 64; depth++) {
      const r = qrot(quat(o.orientation || [0, 0, 0, 0]), out), at = o.position || [0, 0, 0];
      out = [r[0] + at[0], r[1] + at[1], r[2] + at[2]];
      o = o.parent ? parents.get(o.parent) : null;
    }
    return out;
  };
  const meshes = [];
  const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
  for (const p of fx.planes || []) {
    if (p.render === false || !p.faces || !p.faces.length) continue;
    const verts = [];
    for (let i = 0; i + 2 < p.verts.length; i += 3) {
      const v = place([p.verts[i], p.verts[i + 1], p.verts[i + 2]], p);
      verts.push(...v);
      for (let k = 0; k < 3; k++) { lo[k] = Math.min(lo[k], v[k]); hi[k] = Math.max(hi[k], v[k]); }
    }
    const bitmap = String(p.bitmap || "").toLowerCase();
    meshes.push({ name: p.name, bitmap: bitmap === "null" ? "" : bitmap, verts, tverts: (p.tverts || []).slice(), faces: p.faces.slice(),
      diffuse: p.diffuse || [0.8, 0.8, 0.8], ambient: p.ambient || [0.2, 0.2, 0.2], selfillum: p.selfillum || [0, 0, 0],
      alpha: p.alpha === undefined ? 1 : p.alpha, transparencyhint: hintValue(p.transparencyhint === undefined ? 0 : p.transparencyhint) });
  }
  if (!meshes.length) throw new Error(`${label}: no mesh the game draws`);
  return { meshes, lo, hi };
}

// Meshes tinted (M1): copies with their ambient, diffuse and selfillum each times `tint` ([r, g, b]) a channel, through
// round - the game lights a textured mesh as texture x (selfillum + ambient x the area's + diffuse x the lights), so the
// material tints the texture (a dyed part over its dyes). A channel at 1 keeps its numbers as they are, so white (or no
// tint) gives the same numbers, unrounded: nothing written today changes. Everything else of a mesh is kept (its
// transparencyhint too); a mesh without a material keeps none.
function tintMeshes(meshes, tint) {
  const t = Array.isArray(tint) && tint.length === 3 ? tint : [1, 1, 1];
  const times = (c) => c.map((v, k) => (t[k] === 1 ? v : round(v * t[k]) + 0));  // + 0: never -0
  return meshes.map((m) => {
    const out = Object.assign({}, m);
    for (const k of ["ambient", "diffuse", "selfillum"]) if (Array.isArray(m[k])) out[k] = times(m[k]);
    return out;
  });
}

// A draft plane's material as the game draws it (M1's review): the game's model writer (nwn5e-ee's WriteVfxModel,
// src/data/vfx_model.cpp; tests/data/vfx_model_test.cpp pins it) writes a plane whose ambient and diffuse are both the
// draft's defaults (VfxPlane's 0.2 and 0.8 - a mesh without them has them - each compared as C's %g of a float writes
// it) as "ambient 1 1 1" / "diffuse 1 1 1", as nwn5e always wrote planes: the game draws such a plane at full, never at
// 0.8. -> a copy at full ambient and diffuse, else the mesh as it is. The bench's core.js drawnMaterial keeps the same rule
// (the Composer's check.js compares them).
const PLANE_AMBIENT = [0.2, 0.2, 0.2], PLANE_DIFFUSE = [0.8, 0.8, 0.8];
const asWritten = (v) => Number(Math.fround(v).toPrecision(6));  // %g: a float's six significant digits
const atDefault = (c, d) => !Array.isArray(c) || (c.length >= 3 && [0, 1, 2].every((k) => isNum(c[k]) && asWritten(c[k]) === d[k]));
function drawnMaterial(m) {
  if (!atDefault(m.ambient, PLANE_AMBIENT) || !atDefault(m.diffuse, PLANE_DIFFUSE)) return m;
  return Object.assign({}, m, { ambient: [1, 1, 1], diffuse: [1, 1, 1] });
}

// A part's meshes where the composition puts it: scaled, tilted (about x, then y), turned (about z), then moved so the
// middle of its footprint (its box from above) is at at.x, at.y and its lowest point at.z above `floor` (the disk's
// top), and tinted by its tint (tintMeshes; a part without one: white) - one place, so both generators and the
// Composer's stage get it. `planes`: its meshes become a draft's planes (a spell mesh's), which the game draws by
// drawnMaterial - a tinted part's meshes at the draft's defaults are lifted to full first, so the tint multiplies what the
// game draws (not 0.2 0.2 0.1998 for a tint of 1 1 0.999, which the game would draw as it is, darker than untinted);
// untinted they're left as they are (the game lifts them; every draft keeps its bytes). A placeable's meshes never: its
// model is written as it is (meshLines). { meshes (named prefix + its number + "_" + the mesh's name), lo, hi, reach (how
// far from the centre it reaches in x y) }
function placePart(part, model, index, floor, prefix, planes) {
  const s = part.scale, rad = Math.PI / 180;
  const qx = [Math.sin(part.tilt[0] * rad / 2), 0, 0, Math.cos(part.tilt[0] * rad / 2)];
  const qy = [0, Math.sin(part.tilt[1] * rad / 2), 0, Math.cos(part.tilt[1] * rad / 2)];
  const qz = [0, 0, Math.sin(part.turn * rad / 2), Math.cos(part.turn * rad / 2)];
  const turned = model.meshes.map((m) => {
    const out = [];
    for (let i = 0; i + 2 < m.verts.length; i += 3) out.push(qrot(qz, qrot(qy, qrot(qx, [m.verts[i] * s, m.verts[i + 1] * s, m.verts[i + 2] * s]))));
    return out;
  });
  const box = [[Infinity, Infinity, Infinity], [-Infinity, -Infinity, -Infinity]];
  for (const vs of turned) for (const v of vs) for (let j = 0; j < 3; j++) { box[0][j] = Math.min(box[0][j], v[j]); box[1][j] = Math.max(box[1][j], v[j]); }
  const move = [part.at[0] - (box[0][0] + box[1][0]) / 2, part.at[1] - (box[0][1] + box[1][1]) / 2, floor + part.at[2] - box[0][2]];
  const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
  let reach = 0;
  const meshes = model.meshes.map((m, k) => {
    const verts = [];
    for (const v of turned[k]) {
      const w = [v[0] + move[0], v[1] + move[1], v[2] + move[2]];
      for (let j = 0; j < 3; j++) { lo[j] = Math.min(lo[j], w[j]); hi[j] = Math.max(hi[j], w[j]); }
      reach = Math.max(reach, Math.hypot(w[0], w[1]));
      verts.push(round(w[0]), round(w[1]), round(w[2]));
    }
    return Object.assign({}, m, { name: prefix + String(index + 1).padStart(2, "0") + "_" + m.name, verts, tverts: m.tverts.map(round) });
  });
  const lifted = planes && Array.isArray(part.tint) && !isWhite(part.tint) ? meshes.map(drawnMaterial) : meshes;
  return { meshes: tintMeshes(lifted, part.tint), lo, hi, reach };
}

// The same meshes moved by dz (the piles: the disk's height down, their base on the ground).
const lowered = (meshes, dz) => meshes.map((m) => Object.assign({}, m, { verts: m.verts.map((v, i) => (i % 3 === 2 ? round(v - dz) : v)) }));

// Node names a model can take: letters, digits and _ (a name from someone's model may have others), at most `max`
// characters, each its own whatever the case and none of `taken` (the supermodel's: a mesh named like one of its nodes
// would be moved by its animations).
function uniqueNames(meshes, taken, max = 24) {
  const seen = new Set([...(taken || [])].map((n) => n.toLowerCase()));
  return meshes.map((m) => {
    const base = String(m.name).replace(/[^A-Za-z0-9_]/g, "_").slice(0, max);
    let n = base;
    for (let k = 2; seen.has(n.toLowerCase()); k++) n = base.slice(0, max - 1 - String(k).length) + "_" + k;
    seen.add(n.toLowerCase());
    return Object.assign({}, m, { name: n });
  });
}

// --- the disk -------------------------------------------------------------------------------------------------------
// A flat round slab: the top cap, the rim and the bottom cap, `sides` round, its top at disk.height. One mesh, textured
// across its caps from above (u 0.5 + x / D, v 0.5 + y / D; the rim at radius 0.97 of the texture, where its bright edge
// band is); faces counter-clockwise seen from outside.
function diskMesh(disk) {
  const r = disk.diameter / 2, top = disk.height, bottom = disk.height - disk.thickness, n = disk.sides, k = 0.97;
  const verts = [], tverts = [], faces = [];
  const vertex = (x, y, z, u, v) => { verts.push(round(x), round(y), round(z)); tverts.push(round(u), round(v)); return verts.length / 3 - 1; };
  const uv = (c, s) => [0.5 + 0.5 * k * c, 0.5 + 0.5 * k * s];
  const ring = (z) => Array.from({ length: n }, (_, i) => {
    const a = (2 * Math.PI * i) / n, c = Math.cos(a), s = Math.sin(a), t = uv(c, s);
    return vertex(r * c, r * s, z, t[0], t[1]);
  });
  const topCentre = vertex(0, 0, top, 0.5, 0.5), topRing = ring(top);
  for (let i = 0; i < n; i++) faces.push(topCentre, topRing[i], topRing[(i + 1) % n]);
  const rimTop = ring(top), rimBottom = ring(bottom);  // their own vertices: a sharp edge
  for (let i = 0; i < n; i++) {
    const j = (i + 1) % n;
    faces.push(rimTop[i], rimBottom[i], rimBottom[j], rimTop[i], rimBottom[j], rimTop[j]);
  }
  const bottomCentre = vertex(0, 0, bottom, 0.5, 0.5), bottomRing = ring(bottom);
  for (let i = 0; i < n; i++) faces.push(bottomCentre, bottomRing[(i + 1) % n], bottomRing[i]);
  return { name: "forcedisk", bitmap: disk.texture, verts, tverts, faces, diffuse: disk.colour, ambient: disk.colour.map((c) => round(c * 0.4)),
    selfillum: disk.selfillum, alpha: disk.alpha, transparent: true, transparencyhint: 1 };
}

// The disk's texture: soft force-blue, a faint shimmer, a brighter rim band; its alpha the texture's see-through-ness
// (the mesh's own alpha on top). 128 x 128, 32-bit, rows bottom-up as the game's TGAs are (descriptor 0x08). `id`: the
// TGA's image ID ("" for the shipped copy, as nwn5e's other TGAs; TEXTURE_MARK for the bench library's).
function diskTexture(disk, id = "") {
  const size = 128, idLen = id.length;
  const bytes = new Uint8Array(18 + idLen + size * size * 4);
  bytes[0] = idLen; bytes[2] = 2; bytes[12] = size & 255; bytes[13] = size >> 8; bytes[14] = size & 255; bytes[15] = size >> 8; bytes[16] = 32; bytes[17] = 0x08;
  for (let i = 0; i < idLen; i++) bytes[18 + i] = id.charCodeAt(i) & 255;  // latin1
  const body = bytes.subarray(18 + idLen);
  const mix = (a, b, t) => a + (b - a) * t;
  for (let row = 0; row < size; row++) {
    for (let col = 0; col < size; col++) {
      const x = (col + 0.5) / size * 2 - 1, y = (row + 0.5) / size * 2 - 1, d = Math.min(1, Math.hypot(x, y)), a = Math.atan2(y, x);
      const shimmer = 0.06 * Math.sin(d * 22 + 3 * a) * Math.sin(d * 9 - 2 * a) + 0.04 * Math.sin(d * 37);
      const edge = smooth(0.84, 0.95, d);
      const rgb = [0, 1, 2].map((c) => Math.max(0, Math.min(1, mix(disk.colour[c] * (0.85 + 0.25 * d), disk.rim[c], edge) + shimmer)));
      const alpha = Math.max(0, Math.min(1, mix(0.62 + 0.2 * d, 1, edge) + shimmer * 0.5));
      const o = (row * size + col) * 4;
      body[o] = Math.round(rgb[2] * 255); body[o + 1] = Math.round(rgb[1] * 255); body[o + 2] = Math.round(rgb[0] * 255); body[o + 3] = Math.round(alpha * 255);
    }
  }
  return typeof Buffer === "function" ? Buffer.from(bytes.buffer, bytes.byteOffset, bytes.length) : bytes;
}

// --- the Composer's shapes ------------------------------------------------------------------------------------------
// A part may be a shape the generator makes instead of a model (the owner, 2026-11-17: "a translucent sphere so the
// player could see the affected area"): { "type": "dome", "radius": <m> }, and the rest only where it differs from
// shapeDefaults. Its mesh is made here from numbers, and so is its soft texture (n5s_ and a hash of its colour): both
// original.
// S5 adds the rest (the design's list): a sphere (globes, Resilient Sphere), an open cylinder (Moonbeam's and Flame
// Strike's columns), a cone (cone spells, tilted onto their side), a disc (a glyph's place) and a ring (a zone's edge on
// the ground). A disc and a ring are flat (SHAPE_FLAT): no height, on the ground.
// TR (the owner, 2026-11-18, on Elemental Shield's flame ring: "it looks like a flame texture on a truncated upside down
// cone" - "yes" to a Top radius): a cylinder's Top radius makes it a truncated cone either way up - wider flares out,
// narrower leans in; at its default, the Radius, the cylinder as before.
const SHAPE_TYPES = ["dome", "sphere", "cylinder", "cone", "disc", "ring"];
const SHAPE_LABELS = { dome: "Dome (half sphere)", sphere: "Sphere", cylinder: "Cylinder (open)", cone: "Cone (point up)", disc: "Disc (flat, on the ground)",
  ring: "Ring (flat band)" };
const SHAPE_KEYS = ["type", "radius", "height", "inner", "top", "segments", "sides", "mapping", "repeat_around", "repeat_up", "colour", "alpha", "glow",
  "texture", "blend"];  // the order they're written in
const SOLID = () => ["radius", "height", "segments", "sides"];
const SHAPE_FIELDS = { dome: SOLID(), sphere: SOLID(), cylinder: ["radius", "top", "height", "segments", "sides"], cone: SOLID(),  // a type's own fields, the card's order
  disc: ["radius", "segments", "sides"], ring: ["radius", "inner", "segments", "sides"] };
const SHAPE_FLAT = ["disc", "ring"];
const LOOK_FIELDS = ["mapping", "repeat_around", "repeat_up", "colour", "alpha", "glow", "texture", "blend"];  // every type's
// A field's card label and bounds: what readShape enforces, and what the page shows.
const SHAPE_INFO = {
  radius: { label: "Radius (m)", min: 0.05, max: 100, step: 0.05 },
  height: { label: "Height (m)", min: 0.05, max: 100, step: 0.05 },
  inner: { label: "Inner radius (m)", min: 0.01, max: 100, step: 0.05 },  // a ring's, below its radius too (readShape)
  top: { label: "Top radius (m)", min: 0.01, max: 100, step: 0.05,  // a cylinder's (TR)
    title: "The radius at its top: the same as Radius is a cylinder; wider flares out (a lampshade upside down), narrower leans in" },
  segments: { label: "Smoothness", min: 8, max: 96, step: 4, whole: true, title: "Segments round it; a dome's rings up it are a quarter of that, a sphere's half" },
  sides: { label: "Sides", choices: [["both", "Both: seen from inside too"], ["outside", "Outside only"]] },
  mapping: { label: "Mapping", choices: [["wrap", "Wrapped: round it and along it"], ["above", "From above: laid on from overhead"]] },
  repeat_around: { label: "Repeat around", min: 1, max: 32, step: 1, whole: true, title: "How many times the texture goes round it" },
  repeat_up: { label: "Repeat up", min: 1, max: 16, step: 1, whole: true,
    title: "How many times the texture repeats from its base to its top (a flat one's: from its rim in)" },
  colour: { label: "Colour", colour: true },
  alpha: { label: "Opacity", min: 0.02, max: 1, step: 0.05,
    title: "Its alpha: at 1 it's as solid as its texture (Soft and library textures still show through where their own alpha does); the game's globes go to about 0.3" },
  glow: { label: "Glow", min: 0, max: 1, step: 0.05, title: "How much it lights itself (selfillum): at 1 it looks the same by day and night" },
  // Soft or none, or a library texture's name (the game's own, named: nothing generated or copied).
  texture: { label: "Texture", choices: [["soft", "Soft: brighter at its base (a flat one's rim)"], ["none", "None: its colour only"]],
    title: "Soft is made for Wrapped; a texture from the library is tinted by its Colour - at Glow 1 lit by itself, the same by day and night; lower, the scene's light comes in" },
  // M1b (the design's addendum (b); the owner, 2026-11-18: "fxpa_globe works. it's faint, you can see the sparkles" - "go
  // with your rec on the blend"): how the game draws its Soft texture - Additive by a TXI beside it (SOFT_TXI), as the
  // game's own faint effect meshes (fxpa_Globe, fxpa_shield ...): no 0.2 alpha cut, nothing behind hidden; Normal as
  // before (a shape saved without it). A library texture follows its own TXI; None has no texture to carry one.
  blend: { label: "Blend", choices: [["additive", "Additive: glows, adds light, faint and see-through as the game's globes"], ["normal", "Normal: blended over what's behind"]],
    title: "Additive adds its light: it glows, never darkens, black is nothing, overlaps brighten - a faint one is a low Opacity or a darker Colour. Soft only: a library texture follows its own TXI" },
};
const SHAPE_TEXTURE = /^n5s_[0-9a-f]{12}$/;  // a soft texture's name: made, never picked
// An additive soft texture's TXI, written beside its TGA wherever the texture is written (files.js shapeWrites, the
// bench's bake for Play): the game's keyword and value, as nwn5e's n5t_mark*.txi and the game's fxpa_Globe.txi say it.
const SOFT_TXI = "blending additive\n";
// Whether a TXI's text (a string or bytes) says "blending additive" - a line of it, the keyword and its value in any case,
// spaces or tabs round them, either line end: the game's additive textures (13 of its 734 TXIs). The bench's texture
// route says so for a texture it serves (its stage draws such a mesh additively).
function txiAdditive(text) {
  if (text === undefined || text === null) return false;
  const s = typeof text === "string" ? text : Array.from(text instanceof Uint8Array ? text : new Uint8Array(text), (c) => String.fromCharCode(c)).join("");
  return /^[ \t]*blending[ \t]+additive[ \t]*\r?$/im.test(s);
}
const orList = (list) => (list.length > 1 ? list.slice(0, -1).join(", ") + " or " + list[list.length - 1] : list.join(""));

// A type's defaults, in SHAPE_KEYS' order: a dome as tall as its radius; a sphere, a cylinder and a cone twice that (a
// sphere's a ball); a disc and a ring no height; a ring's inner edge 0.85 of its radius, to the millimetre; a cylinder's
// top its radius (TR: a cylinder - never written, so a Top left at its default follows the Radius as a Height does).
// Blend Normal (M1b): a shape saved before it keeps its look - the page writes Additive on the shapes it adds
// (composer.js partFrom).
function shapeDefaults(type, radius) {
  const out = { type, radius };
  if (!SHAPE_FLAT.includes(type)) out.height = type === "dome" ? radius : 2 * radius;
  if (type === "ring") out.inner = Math.round(0.85 * radius * 1000) / 1000;
  if (type === "cylinder") out.top = radius;
  return Object.assign(out, { segments: 32, sides: "both", mapping: "wrap", repeat_around: 1, repeat_up: 1,
    colour: [0.6, 0.8, 1], alpha: 0.35, glow: 1, texture: "soft", blend: "normal" });
}
// Whether `v` is a right value for field `k` (by SHAPE_INFO's bounds), and the error naming the field when it isn't.
function shapeFieldOk(k, v) {
  const info = SHAPE_INFO[k];
  // A library texture's name: never n5s_ (those are made, not picked), nor null (the game reads "bitmap null" as none).
  if (k === "texture") return info.choices.some((c) => c[0] === v) || (typeof v === "string" && RESREF.test(v) && !v.startsWith("n5s_") && v !== "null");
  if (info.colour) return Array.isArray(v) && v.length === 3 && v.every((c) => isNum(c) && c >= 0 && c <= 1);
  if (info.choices) return info.choices.some((c) => c[0] === v);
  return isNum(v) && v >= info.min && v <= info.max && (!info.whole || Number.isInteger(v));
}
function shapeFieldError(where, k) {
  const info = SHAPE_INFO[k];
  if (k === "texture") return `${where}.texture: soft, none or a texture's name (lower-case letters, digits and _, at most 16)`;
  if (k === "inner") return `${where}.inner: a number from ${info.min} to below the radius`;
  if (info.colour) return `${where}.${k}: [r, g, b], each 0 to 1`;
  if (info.choices) return `${where}.${k}: ${orList(info.choices.map((c) => c[0]))}`;
  return `${where}.${k}: ${info.whole ? "a whole number" : "a number"} from ${info.min} to ${info.max}${/\(m\)$/.test(info.label) ? " (metres)" : ""}`;
}
// A shape as a part gives it -> the full shape: shapeDefaults(type, radius) with its keys over it (its colour copied).
// `where` names it in an error (stages.small[0].shape); each error names its field. A key not its type's is refused (a
// disc's height, a dome's inner, a cone's top); a ring's inner is below its radius.
function readShape(s, where) {
  if (!s || typeof s !== "object" || Array.isArray(s)) throw new Error(`${where}: a shape ({ "type": "dome", "radius": <metres> })`);
  if (!SHAPE_TYPES.includes(s.type)) throw new Error(`${where}.type: ${orList(SHAPE_TYPES)}`);
  const own = ["type", ...SHAPE_FIELDS[s.type], ...LOOK_FIELDS];
  const given = Object.keys(s).filter((k) => s[k] !== undefined);
  for (const k of given) {
    if (!own.includes(k)) throw new Error(`${where}.${k}: not a ${s.type}'s - ${orList(SHAPE_KEYS.filter((f) => own.includes(f)))}`);
  }
  if (s.radius === undefined) throw new Error(shapeFieldError(where, "radius"));
  for (const k of SHAPE_KEYS) if (k !== "type" && given.includes(k) && !shapeFieldOk(k, s[k])) throw new Error(shapeFieldError(where, k));
  const out = shapeDefaults(s.type, s.radius);
  for (const k of given) out[k] = k === "colour" ? s[k].slice() : s[k];
  if (out.type === "ring" && !(out.inner < out.radius)) throw new Error(shapeFieldError(where, "inner"));
  return out;
}
// A shape as it's written (`s` a right shape, whole or not): type and radius, then each other key whose value differs
// from shapeDefaults' (a colour by its three numbers), in SHAPE_KEYS' order. Under From above the repeats are left out:
// they mean nothing there. Its blend is written only when Additive (Normal is the default), whatever its texture: under a
// library texture or None it means nothing, but Soft again keeps it.
function cleanShape(s) {
  const d = shapeDefaults(s.type, s.radius), out = { type: s.type, radius: s.radius };
  for (const k of SHAPE_KEYS) {
    const v = s[k];
    if (k === "type" || k === "radius" || v === undefined) continue;
    if (s.mapping === "above" && (k === "repeat_around" || k === "repeat_up")) continue;
    const same = k === "colour" ? Array.isArray(v) && v.length === 3 && v.every((c, i) => c === d.colour[i]) : v === d[k];
    if (!same) out[k] = k === "colour" ? v.slice() : v;
  }
  return out;
}

// A soft texture's colour as its name and its picture both take it: each channel to 0.001. One rule, so a name fixes
// its bytes (a file of that name is rewritten as the generator's).
const softColour = (colour) => colour.map((c) => Math.round(c * 1000) / 1000);
// A soft texture's name: n5s_ and 12 hex digits of its colour (softColour's) and its blend (M1b) - one per colour and
// blend, the same in a page. Normal's (or no blend given) hashes its colour alone, as before M1b, so every name made
// then stays; Additive's hashes ":additive" after it - its own name, so a blend changed never leaves the game a TXI
// beside a texture that no longer wants one (or none beside one that does). DC: a clamped one (`clamp`: its mesh's v
// within one repeat, on a still shape - stillTextures) hashes ":clamp2" after that, its own name for the same reason
// (its TXI says clamp 2: softTxi); an unclamped one is named as before DC.
function shapeTextureName({ colour, blend, clamp }) {
  return "n5s_" + cyrb53("soft:" + softColour(colour).map(String).join(",") + (blend === "additive" ? ":additive" : "") + (clamp ? ":clamp2" : ""))
    .toString(16).padStart(14, "0").slice(-12);
}
// The soft texture, made for Wrapped: its colour (softColour's, as its name has it), brighter and more opaque at the
// ground ring (v 0) and fading towards the top (v 1), with a faint shimmer of whole turns in u, so it wraps round
// seamlessly. 128 x 128 -> { width, height, data: Uint8ClampedArray RGBA, rows top-down } (as bakePlt gives a picture;
// tgaBytes with alpha writes it).
function shapeTexture({ colour: given }) {
  const colour = softColour(given), size = 128, data = new Uint8ClampedArray(size * size * 4);
  for (let y = 0; y < size; y++) {
    const v = 1 - (y + 0.5) / size, ground = 1 - smooth(0, 0.2, v);  // 1 at the ground ring, 0 from a fifth up
    for (let x = 0; x < size; x++) {
      const u = (x + 0.5) / size;
      const shimmer = 0.05 * Math.sin(2 * Math.PI * 3 * u + 7 * v) * Math.sin(2 * Math.PI * 5 * u - 3 * v);
      const o = (y * size + x) * 4;
      for (let c = 0; c < 3; c++) data[o + c] = Math.round(clamp01(colour[c] * (0.75 + 0.25 * (1 - v)) + 0.35 * ground + shimmer) * 255);
      data[o + 3] = Math.round(clamp01(0.3 + 0.5 * (1 - v) * (1 - v) + 0.2 * ground + 0.5 * shimmer) * 255);
    }
  }
  return { width: size, height: size, data };
}

// A shape's profile, turned round z to make it: its stations from its base onwards, each a ring { r, z, v } or a point a
// segment { pole: true, z, v } (r 0), `v` its wrapped v at one repeat. Each goes on from the one before it so its
// outside is the profile's outward side; a flat one's runs in from its rim, so its outside is its top. `n` segments,
// `m = max(2, round(n / 4))` rings to a quarter turn.
// - dome: m rings up from the ground ring (latitude (PI / 2) j / m), then its top; v j / m, then 1.
// - sphere: the dome's rings mirrored below - its bottom point, 2m - 1 rings, its top point; its middle at h / 2, v
//   from the bottom (0) to the top (1).
// - cylinder: an open tube, a ring of its radius at 0 (v 0) and one of its top (TR: its radius by default) at h (v 1) -
//   a wider top flares out, a narrower one leans in. cone: a ring at 0 (v 0) to a point at h (v 1), its base open.
// - disc: its rim (v 0) to a point at its centre (v 1), flat at 0. ring: its rim (v 0) to its inner edge (v 1). A flat
//   one's v runs from the rim in, so the soft texture glows at its rim - the area's edge - as a dome's at its ground ring
//   (the coordinator's ruling on S5).
// A cylinder's top (TR; its radius when not given, as readShape fills it) and a shape's widest radius - its radius, or a
// cylinder's top when wider: what From above lays the picture across and its box spans.
const topOf = (shape) => (isNum(shape.top) ? shape.top : shape.radius);
const widestOf = (shape) => (shape.type === "cylinder" ? Math.max(shape.radius, topOf(shape)) : shape.radius);
function profileOf(shape) {
  const r = shape.radius, h = shape.height, n = shape.segments, m = Math.max(2, Math.round(n / 4));
  const latitude = (phi, z0, up, v) => ({ r: r * Math.cos(phi), z: z0 + up * Math.sin(phi), v });
  if (shape.type === "dome") {
    return [...Array.from({ length: m }, (_, j) => latitude((Math.PI / 2) * j / m, 0, h, j / m)), { pole: true, z: h, v: 1 }];
  }
  if (shape.type === "sphere") {
    return [{ pole: true, z: 0, v: 0 }, ...Array.from({ length: 2 * m - 1 }, (_, k) => latitude(-Math.PI / 2 + (Math.PI / 2) * (k + 1) / m, h / 2, h / 2, (k + 1) / (2 * m))),
      { pole: true, z: h, v: 1 }];
  }
  if (shape.type === "cylinder") return [{ r, z: 0, v: 0 }, { r: topOf(shape), z: h, v: 1 }];
  if (shape.type === "cone") return [{ r, z: 0, v: 0 }, { pole: true, z: h, v: 1 }];
  if (shape.type === "disc") return [{ r, z: 0, v: 0 }, { pole: true, z: 0, v: 1 }];
  if (shape.type === "ring") return [{ r, z: 0, v: 0 }, { r: shape.inner, z: 0, v: 1 }];
  throw new Error(`shapeModel: no ${shape.type}`);
}
// One side of a shape: its profile (profileOf) turned round z, n segments - a ring's vertex i at angle 2 PI i / n, u i / n,
// the seam's twice (0 and n); a pole's vertex i at its segment's middle, u (i + 0.5) / n - its faces counter-clockwise
// seen from outside. -> { verts, tverts, faces }, its vertices and UVs rounded as diskMesh's, never -0 (so the mesh
// through JSON is the same mesh). `uv(x, y, u, v)`: a vertex's UV from its position and its wrapped (u, v) at one repeat.
function shapeSide(shape, uv) {
  const n = shape.segments, verts = [], tverts = [], faces = [];
  const tidy = (v) => round(v) + 0;  // -0 + 0 is 0
  const vertex = (x, y, z, u, v) => { verts.push(tidy(x), tidy(y), tidy(z)); tverts.push(...uv(x, y, u, v).map(tidy)); return verts.length / 3 - 1; };
  const profile = profileOf(shape);
  const stations = profile.map((s) => (s.pole ? Array.from({ length: n }, (_, i) => vertex(0, 0, s.z, (i + 0.5) / n, s.v))
    : Array.from({ length: n + 1 }, (_, i) => {
      const theta = (2 * Math.PI * i) / n;
      return vertex(s.r * Math.cos(theta), s.r * Math.sin(theta), s.z, i / n, s.v);
    })));
  for (let j = 0; j + 1 < stations.length; j++) {
    const a = stations[j], b = stations[j + 1];
    for (let i = 0; i < n; i++) {
      if (profile[j].pole) faces.push(a[i], b[i + 1], b[i]);  // from a point: its segment's triangle
      else if (profile[j + 1].pole) faces.push(a[i], a[i + 1], b[i]);  // to a point
      else faces.push(a[i], a[i + 1], b[i + 1], a[i], b[i + 1], b[i]);
    }
  }
  return { verts, tverts, faces };
}
// Both sides: a copy of the vertices with the inside's faces (each outside face's first, third and second) - the inside
// first, then the outside. -> { verts, tverts, faces, groups }: the inside's faces in smoothing group 2, the outside's in
// 1 - the sides share their positions, and a loader merging normals by position within a group would cancel them.
function bothSides(side) {
  const count = side.verts.length / 3, faces = [], half = side.faces.length / 3;
  for (let f = 0; f < side.faces.length; f += 3) faces.push(side.faces[f], side.faces[f + 2], side.faces[f + 1]);
  for (const v of side.faces) faces.push(v + count);
  return { verts: side.verts.concat(side.verts), tverts: side.tverts.concat(side.tverts), faces,
    groups: new Array(half).fill(2).concat(new Array(half).fill(1)) };
}
// A shape (readShape's) as a model, in the model's space as modelSpaceMeshes gives one (its base at z 0, centred on x 0
// y 0) -> { meshes: [{ name (its type), bitmap, verts, tverts, faces, groups (each face's smoothing group: meshLines),
// diffuse, ambient, selfillum, alpha, transparent, transparencyhint (1 when transparent, else 0), additive (M1b: true
// for a Soft Additive shape - its texture's TXI makes the game add its light; the bench's stage draws it so - else not
// there) }], lo, hi, textures: [{ name, colour, blend }] (the soft texture to make - with SOFT_TXI beside it when its blend
// is additive - or none) }.
// - Its UVs: Wrapped, u round it from +x and v from its base up (a disc's and a ring's from the rim in), times its
//   repeats; From above, laid on from overhead, its width the picture's square ((0.5 + x / 2r, 0.5 + y / 2r): r its
//   radius, a ring's outer, a cylinder's the larger of its radius and its top - TR: every UV within [0, 1]).
// - Its material: soft, the texture's colour lit by its glow; none, its own colour lit by its glow; a library texture
//   (M1, the owner's Q3), tinted by its colour and lit by itself as far as its glow goes - selfillum its colour x glow,
//   ambient and diffuse its colour x (1 - glow), each through round: at Glow 1 the texture x its colour in any light (the
//   game clamps a mesh's light at 1, so ambient and diffuse on top washed a pale colour out to white by day), at Glow 0
//   lit by the scene as a placeable is. See-through (its own alpha isn't known here).
// - Its box: a flat one's (a disc, a ring) is 0 high; a flared cylinder's as wide as its top (widestOf).
function shapeModel(shape) {
  const r = widestOf(shape);
  const uv = shape.mapping === "above" ? (x, y) => [0.5 + x / (2 * r), 0.5 + y / (2 * r)]
    : (x, y, u, v) => [u * shape.repeat_around, v * shape.repeat_up];
  const side = shapeSide(shape, uv);
  const { verts, tverts, faces, groups } = shape.sides === "both" ? bothSides(side) : Object.assign({ groups: new Array(side.faces.length / 3).fill(1) }, side);
  const soft = shape.texture === "soft", none = shape.texture === "none", colour = shape.colour, glow = shape.glow;
  const bitmap = soft ? shapeTextureName(shape) : none ? "" : shape.texture;
  const transparent = !none || shape.alpha < 1;
  const times = (k) => colour.map((c) => round(c * k) + 0);  // + 0: never -0
  const lit = soft ? [1, 1, 1] : none ? colour.slice() : times(1 - glow);  // its ambient and diffuse
  const blend = shape.blend === "additive" ? "additive" : "normal";
  const mesh = { name: shape.type, bitmap, verts, tverts, faces, groups,
    diffuse: lit, ambient: lit.slice(),
    selfillum: soft ? [glow, glow, glow] : times(glow),
    alpha: shape.alpha, transparent, transparencyhint: transparent ? 1 : 0 };  // T1b: see-through in game when it is
  if (soft && blend === "additive") mesh.additive = true;
  return { meshes: [mesh], lo: [-r, -r, 0], hi: [r, r, SHAPE_FLAT.includes(shape.type) ? 0 : shape.height],
    textures: soft ? [{ name: bitmap, colour: colour.slice(), blend }] : [] };
}

// --- textures that move (M2) ----------------------------------------------------------------------------------------
// The owner, 2026-11-18: "for the animated textures: 1. scrolling, scaling (breathing), rotating" - "the textures on the
// meshes". A part's `motion` { scroll: [u, v] (repeats a second along u, then v), turn (degrees a second), breathe (how
// much bigger and smaller the picture gets, 0 to 0.9), rate (breaths a second, while it breathes) } moves every textured
// mesh of the part - a shape's too; absent or still, nothing written changes. NWN:EE has no UV-motion setting (the
// design's "Texture motion"), so a moving texture reaches the game as an EE material: the mesh's bitmap becomes
// n5u_<12 hex digits> (motionName), and n5u_<...>.mtr - the game attaches a material by its mesh's texture name - names
// our vertex shader (vsn5uvmove.shd, motionShaderText) with the game's lit fragment shader and gives the motion as its
// parameters (motionMaterialText). Data files: no DLL change. The stage moves its UVs by the same maths (uvMotion).
const MOTION_KEYS = ["scroll", "turn", "breathe", "rate"];
const MOTION_RATE = 0.5;  // a breath every 2 s, when a motion gives no rate
// A field's card label(s) and bounds: what readMotion enforces, and what the page shows (the design's sketch).
const MOTION_INFO = {
  scroll: { labels: ["Scroll round", "Scroll up"], min: -10, max: 10, step: 0.05,
    title: "Repeats a second: + round it counter-clockwise seen from above (u), + up it (v)" },
  turn: { label: "Turn (°/s)", min: -720, max: 720, step: 5, title: "Degrees a second: + counter-clockwise seen from above, round the texture's middle" },
  breathe: { label: "Breathe", min: 0, max: 0.9, step: 0.05, title: "How much bigger and smaller the picture gets (0 to 0.9) - the mesh stays as it is" },
  rate: { label: "Breaths (/s)", min: 0.05, max: 5, step: 0.05, title: "How often it breathes: breaths a second (with Breathe above 0)" },
};
const MOTION_ERRORS = { scroll: "[u, v], each a number from -10 to 10 (repeats a second)", turn: "a number from -720 to 720 (degrees a second)",
  breathe: "a number from 0 to 0.9 (how much bigger and smaller it gets)", rate: "a number from 0.05 to 5 (breaths a second)" };
const MOTION_TEXTURE = /^n5u_[0-9a-f]{12}$/;  // a moving texture's material: made, never picked
const MOTION_SHADER = "vsn5uvmove";  // our vertex shader, data\files\vsn5uvmove.shd (committed; made.txt lists it)
// The material's parameters, in its order: the shader's uniforms of these names (a material's "parameter float <name>
// <value>" is a uniform of that name, as the game's inc_material reads Specularity).
const MOTION_PARAMS = ["n5ScrollU", "n5ScrollV", "n5Turn", "n5Breathe", "n5Rate", "n5PivotU", "n5PivotV"];
const inMotionRange = (v, k) => isNum(v) && v >= MOTION_INFO[k].min && v <= MOTION_INFO[k].max;
// A part's motion -> { scroll: [u, v] (its own copy), turn, breathe, rate }: absent (undefined or null) or a key left out,
// 0 - the rate MOTION_RATE. `where` names it in an error (stages.small[0]); each error names its field.
function readMotion(m, where) {
  const out = { scroll: [0, 0], turn: 0, breathe: 0, rate: MOTION_RATE };
  if (m === undefined || m === null) return out;
  if (typeof m !== "object" || Array.isArray(m)) {
    throw new Error(`${where}.motion: { "scroll": [u, v], "turn": <degrees a second>, "breathe": 0-0.9, "rate": <breaths a second> }`);
  }
  for (const k of Object.keys(m)) {
    const v = m[k];
    if (v === undefined) continue;
    if (!MOTION_KEYS.includes(k)) throw new Error(`${where}.motion.${k}: not a motion's - ${orList(MOTION_KEYS)}`);
    const ok = k === "scroll" ? Array.isArray(v) && v.length === 2 && v.every((c) => inMotionRange(c, "scroll")) : inMotionRange(v, k);
    if (!ok) throw new Error(`${where}.motion.${k}: ${MOTION_ERRORS[k]}`);
    out[k] = k === "scroll" ? v.slice() : v;
  }
  return out;
}
const moves = (v) => isNum(v) && v !== 0;
// Whether a motion (read or as given) moves anything: it scrolls, turns or breathes - a rate alone moves nothing.
function hasMotion(m) {
  if (!m || typeof m !== "object" || Array.isArray(m)) return false;
  return (Array.isArray(m.scroll) && m.scroll.some(moves)) || moves(m.turn) || moves(m.breathe);
}
// A motion as it's written: its non-zero keys in MOTION_KEYS' order - the rate only while it breathes (meaningless
// without) and not at its default, MOTION_RATE (a default is never written) - or undefined when it doesn't move (nothing
// is written).
function cleanMotion(m) {
  if (!hasMotion(m)) return undefined;
  const out = {};
  if (Array.isArray(m.scroll) && m.scroll.some(moves)) out.scroll = m.scroll.slice();
  if (moves(m.turn)) out.turn = m.turn;
  if (moves(m.breathe)) {
    out.breathe = m.breathe;
    if (m.rate !== undefined && m.rate !== null && m.rate !== MOTION_RATE) out.rate = m.rate;
  }
  return out;
}
// A mesh's pivot (the design's call 5): the middle of its UV box, [(umin + umax) / 2, (vmin + vmax) / 2], through round -
// From above's middle is the shape's centre (a glyph turns and breathes round it), a Wrapped shape with repeats turns round
// the middle of its repeats. No UVs: the picture's middle. Never -0.
function uvPivot(tverts) {
  const t = tverts || [], lo = [Infinity, Infinity], hi = [-Infinity, -Infinity];
  for (let i = 0; i + 1 < t.length; i += 2) {
    for (let k = 0; k < 2; k++) if (isNum(t[i + k])) { lo[k] = Math.min(lo[k], t[i + k]); hi[k] = Math.max(hi[k], t[i + k]); }
  }
  if (!(lo[0] <= hi[0] && lo[1] <= hi[1])) return [0.5, 0.5];
  return [round((lo[0] + hi[0]) / 2) + 0, round((lo[1] + hi[1]) / 2) + 0];
}
// The UVs at time t (seconds) as a matrix [a, b, c, d, e, f] - u' = a u + b v + c, v' = d u + e v + f - round the pivot p:
// s = 1 + breathe x sin(2 pi rate t), th = -turn x t x pi / 180, uv' = R(th) (uv - p) / s + p - scroll x t. So + scroll
// moves the picture along +u / +v, + turn turns it counter-clockwise (From above, seen from above), and breathing grows
// and shrinks the picture, not the mesh. The numbers are the material's (motionNumbers: the motion and the pivot each to
// 0.001), so the stage moves exactly as the game's shader is told to. No motion: [1, 0, 0, 0, 1, 0] exactly. Never -0.
function uvMotion(motion, pivot, t) {
  const [su, sv, turn, breathe, rate, pu, pv] = motionNumbers(motion, pivot);
  if (!su && !sv && !turn && !breathe) return [1, 0, 0, 0, 1, 0];
  const s = 1 + breathe * Math.sin(2 * Math.PI * rate * t), th = (-turn * t * Math.PI) / 180;
  const a = Math.cos(th) / s, b = -Math.sin(th) / s, d = Math.sin(th) / s, e = Math.cos(th) / s;
  return [a, b, pu - a * pu - b * pv - su * t, d, e, pv - d * pu - e * pv - sv * t].map((v) => v + 0);
}
// A moving texture's seven numbers, as its name and its material both take them (one rule, so a name fixes its bytes):
// MOTION_PARAMS' order, each to 0.001, the rate MOTION_RATE while it doesn't breathe (it means nothing then).
const to3 = (v) => Math.round(v * 1000) / 1000 + 0;
function motionNumbers(motion, pivot) {
  const m = readMotion(motion, "material"), breathe = to3(m.breathe);
  return [to3(m.scroll[0]), to3(m.scroll[1]), to3(m.turn), breathe, breathe ? to3(m.rate) : MOTION_RATE, to3(pivot[0]), to3(pivot[1])];
}
// A moving texture's material name - the mesh's bitmap: n5u_ and 12 hex digits of its texture (lower-case), its motion
// and its pivot (motionNumbers), as shapeTextureName makes an n5s_ name. One per texture, motion and pivot - and clamp
// (W1: motionClamps'): a clamped one hashes ":clamp" after them, so a clamped and an unclamped motion never share a name
// (the copy and its TXI go with the clamped one only); an unclamped one is named as before W1.
function motionName(texture, motion, pivot, clamp = false) {
  return "n5u_" + cyrb53(`uv:${String(texture).toLowerCase()}:${motionNumbers(motion, pivot).join(",")}${clamp ? ":clamp" : ""}`).toString(16).padStart(14, "0").slice(-12);
}
// --- W1: a breathing texture's edges clamped (the design's addendum (c); TMO-2, the owner, 2026-11-18: "when it shrank
// the texture being tiled and wrapping would show a bit at the bottom edge of the dome" - "go with 1 for the wrap") ----
// The game repeats a texture past UV 1 (which a scroll needs); a breath below 1 scales the UVs past the picture's edge, so
// the next repeat shows at the mesh's edge. So a moving texture that breathes and doesn't scroll, on a mesh whose UVs stay
// within one repeat (a shape From above or at Repeat 1 / 1, a game part whose UVs don't tile), is clamped: its picture
// copied under its n5u_ name with a TXI of the source's own lines and the game's clamp line (clampedTxi), the material
// naming the copy - no texture0 (motionMaterialText) - the copy route TMO-1 proved in game (2026-11-18: the copy domes
// moved and stayed see-through). Its edge pixels stretch instead. Unproven until TMO-5: that the game honours a TXI's
// clamp on a mesh (14 of its 15 clamp TXIs are the GUI's); if not, the fallback is a breath that only grows.
// How far a UV may be past 0..1 and still be within one repeat (a mesh's UVs as the generators round them, to 0.00001).
const UV_REPEAT_EPSILON = 0.001;
// The game's clamp line: "clamp" and a number - 1, 2 or 3 in its 15 TXIs that clamp (gui_chrsht_* and gui_slider 1,
// tdt01_testc 2, the GUI's tabs, icons and loadscreens 3: 10 of them). Read as two bits (1 u, 2 v: inferred from the
// values, not seen), 3 clamps both ways - and it's the game's own commonest line either way.
const CLAMP_TXI = "clamp 3";
// The rule: whether a motion (read or as given; readMotion's errors) on a mesh with these UVs (`tverts`: u v u v ...) is
// clamped - it breathes and doesn't scroll (each as the material takes it: motionNumbers, to 0.001), and every UV is
// within 0..1 (UV_REPEAT_EPSILON either side). No UVs: it repeats (nothing known within one repeat).
function motionClamps(motion, tverts) {
  const [su, sv, , breathe] = motionNumbers(motion, [0, 0]);
  if (!breathe || su || sv) return false;
  const t = tverts || [];
  if (!t.length) return false;
  return Array.from(t).every((v) => isNum(v) && v >= -UV_REPEAT_EPSILON && v <= 1 + UV_REPEAT_EPSILON);
}
// A TXI's text (a string or bytes; none: "") as written under a clamped material's name: the source texture's own lines
// as they are - its blending, its procedure (proceduretype, numx, numy, fps) ... - in its own line ends (CRLF the game's,
// LF ours), any clamp line of its own left out and its blank lines at the end dropped, then CLAMP_TXI and a line end.
// DC: through copiedTxi - its decal line dropped too (a decal mesh is drawn unlit, its material skipped, so our material
// would read whatever the mesh before it left: the SHP-8 diagnosis), and the clamp the union of the source's own and 3
// (the game's clamp TXIs say 1, 2 or 3: clamp 3 always, as before).
const latin1 = (text) => (text === undefined || text === null ? "" : typeof text === "string" ? text
  : Array.from(text instanceof Uint8Array ? text : new Uint8Array(text), (c) => String.fromCharCode(c)).join(""));
const CLAMP_LINE = /^[ \t]*clamp[ \t]+(\S+)/i;
const clampedTxi = (source) => copiedTxi(source, 3);
// Whether a TXI (a string or bytes) clamps: a "clamp" line whose value isn't 0 (any case, spaces or tabs, either line
// end; DC: its value as C's atoi reads it - txiClampBits). The bench's texture route says so of a clamped material's copy
// (its stage draws it clamped).
const txiClamps = (text) => txiClampBits(text) !== 0;
// Its material, n5u_<...>.mtr ({ texture, motion, pivot, clamp? }: moveTextures' material): our vertex shader with the
// game's lit fragment shader (the pair the game's vslit / fslit are), the real texture as texture0 - left out with
// `copy`, the route where the texture is copied under the material's own name (TMO-1 passed 2026-11-18: route A,
// texture0, and the copy route too); a clamped material (W1) is on the copy route, so `copy` is its clamp unless given -
// and a "parameter float" line a MOTION_PARAMS name. LF line ends.
function motionMaterialText({ texture, motion, pivot, clamp = false }, { copy = !!clamp } = {}) {
  const n = motionNumbers(motion, pivot);
  return ["customshadervs " + MOTION_SHADER, "customshaderfs fslit", ...(copy ? [] : ["texture0 " + String(texture).toLowerCase()]),
    ...MOTION_PARAMS.map((p, i) => `parameter float ${p} ${n[i]}`), ""].join("\n");
}
// The shader, vsn5uvmove.shd: original text (nothing of the game's copied; its include named), LF - written with the
// generators' line ends (crlf) into data\files, where it's committed, and beside what needs it. Its first line is its
// mark: files.js owns a vsn5uvmove.shd carrying it (isMotionShader).
const MOTION_SHADER_MARK = "// vsn5uvmove.shd - nwn5e's moving mesh textures (nwn5e-ee #39): a texture scrolled, turned and breathing with the game's clock";
function motionShaderText() {
  return [
    MOTION_SHADER_MARK,
    "// Made by nwn5e-ee tools/data/floatdisk/floatdisk.js (motionShaderText): change it there and rebuild, not here. A moving",
    "// texture's material, n5u_<12 hex digits>.mtr, pairs this vertex shader with the game's lit fragment shader (fslit) and",
    "// gives the motion below as its parameters. The game's standard vertex setup first (its own include, by name); then the",
    "// texture's coordinates moved round the pivot p: uv' = R(-turn t) (uv - p) / s + p - scroll t, s = 1 + breathe",
    "// sin(2 pi rate t), t the game's world clock in seconds (floatdisk.js uvMotion: the same maths). t itself is a float:",
    "// late in a game day (near 2880 s) it holds to about a quarter of a millisecond, the motion's own limit. The scroll,",
    "// the turn and the breath are each taken within one repeat, one turn and one breath, so the coordinates stay small and",
    "// lose nothing more on top of that: the same picture, as the texture repeats.",
    "",
    "#define SHADER_TYPE 1",
    "#define FOG 1",
    "#define KEYHOLING 1",
    "#define NORMAL_MAP 0",
    "#define ENVIRONMENT_MAP 0",
    "#define LIGHTING 1",
    "",
    '#include "inc_standard"',
    '#include "inc_uniforms"',
    "",
    ...MOTION_PARAMS.map((p) => `uniform float ${p};`),
    "",
    "void main ()",
    "{",
    "    ApplyStandardShader();",
    "    float t = float(worldtimerTimeOfDay) * 0.001;",
    "    float s = 1.0 + n5Breathe * sin(6.28318531 * fract(n5Rate * t));",
    "    float a = radians(-mod(n5Turn * t, 360.0));",
    "    float du = (vVertexTexCoords.x - n5PivotU) / s;",
    "    float dv = (vVertexTexCoords.y - n5PivotV) / s;",
    "    vVertexTexCoords = vec2(cos(a) * du - sin(a) * dv + n5PivotU - fract(n5ScrollU * t),",
    "        sin(a) * du + cos(a) * dv + n5PivotV - fract(n5ScrollV * t));",
    "}",
    "",
  ].join("\n");
}
// Whether a text (a string or bytes) is the shader the generators write, this version's or an earlier one's: its first line
// is MOTION_SHADER_MARK (either line end).
function isMotionShader(text) {
  if (text === undefined || text === null) return false;
  const head = typeof text === "string" ? text : String.fromCharCode(...(text instanceof Uint8Array ? text : new Uint8Array(text)).subarray(0, 512));
  return head.split("\n", 1)[0].replace(/\r$/, "") === MOTION_SHADER_MARK;
}
// Meshes with each textured one's bitmap renamed for the motion (motionName: its texture, the motion, its pivot - uvPivot
// of its UVs - and whether it's clamped, W1's motionClamps of its UVs) -> { meshes (copies), materials: [{ name, texture,
// motion (readMotion's), pivot, clamp: true (a clamped one's only) }] }, each name once in the meshes' order. An
// untextured mesh, and one naming a moving texture's material already, as it is. No motion: the same meshes, no
// materials. `clamp` false: the rule off, every one repeats (uvprobe: TMO-1's domes as they were written). Called after a
// part's bakes and soft textures are named (files.js motionMaterials), so its texture - texture0, or the copy's picture -
// is the real one.
function moveTextures(meshes, motion, { clamp = true } = {}) {
  const m = readMotion(motion, "moveTextures");
  if (!hasMotion(m)) return { meshes, materials: [] };
  const materials = [], seen = new Set();
  const out = meshes.map((mesh) => {
    const texture = String(mesh.bitmap || "").toLowerCase();
    if (!texture || texture === "null" || MOTION_TEXTURE.test(texture)) return Object.assign({}, mesh);
    const pivot = uvPivot(mesh.tverts), clamped = clamp && motionClamps(m, mesh.tverts), name = motionName(texture, m, pivot, clamped);
    if (!seen.has(name)) { seen.add(name); materials.push(Object.assign({ name, texture, motion: m, pivot }, clamped ? { clamp: true } : {})); }
    return Object.assign({}, mesh, { bitmap: name });
  });
  return { meshes: out, materials };
}

// --- DC: a library texture copied without its decal mark; flipbooks and soft textures clamped in v (#39) --------------
// SHP-8 (the owner, 2026-11-19): a flame ring (Cylinder (open), Top radius, fxpa_flame02, a green Colour) showed "a top very
// thin color at the edge on the rim of the cylinder" (on the stage and in game), and "when played in game the tinting
// doesn't show"; "Go" to the fix. The engine (nwmain, read in the SHP-8 diagnosis):
// - a TXI's `decal` (Material::ParseField) makes the game draw the mesh unlit: PartTriMesh::Mat() sets its colour to (1, 1,
//   1, alpha) and skips its material, RenderFlat turns lighting off, so the shader is vsvt / fst - texture x alpha - and a
//   shape's Colour (its selfillum, ambient and diffuse) never shows. 21 of the game's 734 TXIs say it: fxpa_flame02,
//   fxpa_symbl_003, fxpa_web, fxpa_fern, its 7 flipbooks, c_air_skin ...;
// - a flipbook's frame is a texture of its own (CreateTexture -> ImageGetSubImage), drawn repeating both ways unless its TXI
//   clamps (SetTextureParameters: clamp bit 1 WRAP_S, 2 WRAP_T, 4 WRAP_R), so a mesh whose v runs to 1 samples the frame's
//   wrapped bottom row at its top rim: the thin line. Our soft textures (alpha 77 at the top row, 253 at the bottom) show
//   it fainter.
// So a Composer shape whose part doesn't move (a moving one's texture goes through its material: M2, W1 - clampedTxi drops
// decal there) gives the game, under our own names:
// - a library texture whose TXI says decal, or a flipbook (decal or not) on a mesh whose v stays within one repeat: a copy
//   of its picture, n5l_<12 hex digits> (copyName: its texture and the clamp added - one copy per distinct result, never
//   per colour), with a TXI of its own lines minus decal - its flipbook and blending kept - plus clamp 2 (v) when the
//   mesh's v stays within [0, 1] (copiedTxi). U keeps repeating (round a ring, Repeat around);
// - its soft texture, when its mesh's v stays within [0, 1]: a name of its own (shapeTextureName's clamp) with a TXI
//   saying clamp 2 (softTxi).
// A game part's or your own file's meshes are drawn as their models have them; the game's own effects are never copied.
const CLAMP_V = 2;  // the game's clamp bits: 1 u (WRAP_S), 2 v (WRAP_T), 4 r (WRAP_R) - SetTextureParameters
const COPY_TEXTURE = /^n5l_[0-9a-f]{12}$/;  // a library texture's copy: made, never picked
// A TXI value as C's atoi reads it: its leading whole number (after spaces or tabs), else 0.
const atoi = (v) => { const m = /^[ \t]*([+-]?\d+)/.exec(String(v === undefined ? "" : v)); return m ? parseInt(m[1], 10) : 0; };
const DECAL_WORD = /^[ \t]*decal(?:[ \t]|$)/i;  // a decal line, its value or none: every one is dropped from a copy
// Whether a TXI (a string or bytes) says decal: a "decal" line whose value isn't 0 as atoi reads it (any case, either
// line end) - what the game draws unlit.
function txiDecal(text) {
  return latin1(text).split(/\r?\n/).some((l) => { const m = /^[ \t]*decal[ \t]+(\S+)/i.exec(l); return !!m && atoi(m[1]) !== 0; });
}
// A TXI's clamp bits: every "clamp" line's value (atoi) together (1 u, 2 v, 4 r); 0 when none clamps.
function txiClampBits(text) {
  let bits = 0;
  for (const l of latin1(text).split(/\r?\n/)) { const m = CLAMP_LINE.exec(l); if (m) bits |= Math.max(0, atoi(m[1])); }
  return bits;
}
// A TXI's words: each line's first word (lower-case) -> the word after it, the first line naming it (the bench's
// composer_server.js txiWords, the same reading).
function txiWords(text) {
  const out = {};
  for (const line of latin1(text).split(/\r?\n/)) {
    const m = /^[ \t]*([A-Za-z_][A-Za-z0-9_]*)[ \t]+(\S+)/.exec(line);
    if (m && !(m[1].toLowerCase() in out)) out[m[1].toLowerCase()] = m[2];
  }
  return out;
}
// A TXI's flipbook: "proceduretype cycle" with numx and numy (whole, 1 or more; 1 when left out) and fps (0 or more; 0
// when left out) -> { numx, numy, fps }, or null (no cycle, or a sheet of one frame). The bench's texture route says it
// (X-Cycle) by this rule.
function txiCycle(text) {
  const w = txiWords(text);
  if (String(w.proceduretype || "").toLowerCase() !== "cycle") return null;
  const whole = (v) => (v === undefined ? 1 : Number(v)), numx = whole(w.numx), numy = whole(w.numy), fps = w.fps === undefined ? 0 : Number(w.fps);
  if (!Number.isInteger(numx) || !Number.isInteger(numy) || numx < 1 || numy < 1 || numx * numy < 2 || !Number.isFinite(fps) || fps < 0) return null;
  return { numx, numy, fps };
}
// A copy's TXI (`source` its texture's TXI, a string or bytes; none: ""): the source's own lines as they are but every
// decal line - its procedure (proceduretype, numx, numy, fps), its blending ... - in its own line ends (CRLF the game's,
// LF ours), its clamp line left out and its blank lines at the end dropped; then one clamp line, the union of its own clamp
// and `clamp` (CLAMP_V, W1's 3), when that isn't 0, and a line end. Nothing left: "".
function copiedTxi(source, clamp = 0) {
  const s = latin1(source), eol = /\r\n/.test(s) ? "\r\n" : "\n", bits = (clamp | txiClampBits(s)) >>> 0;
  const lines = s ? s.split(/\r?\n/).filter((l) => !CLAMP_LINE.test(l) && !DECAL_WORD.test(l)) : [];
  while (lines.length && !lines[lines.length - 1].trim()) lines.pop();
  if (bits) lines.push("clamp " + bits);
  return lines.length ? [...lines, ""].join(eol) : "";
}
// A soft texture's TXI ({ blend, clamp }): an Additive one's "blending additive" (SOFT_TXI), a clamped one's "clamp 2",
// both lines for both, LF; null for a Normal unclamped one (no TXI, as before DC).
function softTxi({ blend, clamp } = {}) {
  const lines = [...(blend === "additive" ? ["blending additive"] : []), ...(clamp ? ["clamp " + CLAMP_V] : [])];
  return lines.length ? lines.join("\n") + "\n" : null;
}
// Whether a mesh's v stays within one repeat: every v of its UVs (`tverts`: u v u v ...) within 0..1, UV_REPEAT_EPSILON
// either side (its u may tile - round a ring, Repeat around). No UVs: no.
function vClamps(tverts) {
  const t = Array.from(tverts || []);
  if (t.length < 2) return false;
  for (let i = 1; i < t.length; i += 2) if (!isNum(t[i]) || t[i] < -UV_REPEAT_EPSILON || t[i] > 1 + UV_REPEAT_EPSILON) return false;
  return true;
}
// A copy's name: n5l_ and 12 hex digits of its texture (lower-case) and the clamp added (0 or CLAMP_V) - its TXI is the
// texture's own lines minus decal with that clamp, so the name fixes its bytes, and a shape's colour isn't in it.
function copyName(texture, clamp) {
  return "n5l_" + cyrb53(`copy:${String(texture).toLowerCase()}:${clamp ? clamp : 0}`).toString(16).padStart(14, "0").slice(-12);
}
// Whether the game must have a library texture copied: its TXI (`info`: { decal, cycle } - txiDecal, txiCycle - of it;
// null when not known) says decal, or it's a flipbook on a mesh whose v stays within one repeat (`clampV`).
const copied = (info, clampV) => !!info && (!!info.decal || (!!info.cycle && !!clampV));
// What DC does with a shape's textured meshes (`shape` readShape's; `motion` its part's, read or as given): "soft" (its
// soft texture), "library" (a library texture), or null - none (no texture), a part that moves (its material's route) or
// no shape.
function copyKind(shape, motion) {
  if (!shape || typeof shape !== "object" || hasMotion(motion)) return null;
  return shape.texture === "soft" ? "soft" : shape.texture && shape.texture !== "none" ? "library" : null;
}
// The rule, one place (the generators: files.js shapeTextures / copyTextures; the bench's Play in game: composer.js): a
// shape part's meshes (`shape`, its part's `motion`, `meshes` placed) as the game must be given them, `info(texture)` ->
// { decal, cycle } | null its library texture's TXI. -> { meshes (copies - the given ones untouched), softs: [{ name,
// colour, blend, clamp: true }] (clamped soft textures to make), copies: [{ name, texture, clamp }] (library textures to
// copy) }, each name once. A soft mesh whose v stays within one repeat names its soft texture clamped; a library one the
// game must have copied (`copied`) names its copy, clamp CLAMP_V within one repeat, else 0. Anything else as it is.
function stillTextures(shape, motion, meshes, info) {
  const kind = copyKind(shape, motion), softs = [], copies = [], seen = new Set();
  if (!kind) return { meshes, softs, copies };
  const soft = kind === "soft" ? shapeTextureName(shape) : null;
  const out = (meshes || []).map((m) => {
    const t = String(m.bitmap || "").toLowerCase();
    if (!t || t === "null") return m;
    const clampV = vClamps(m.tverts);
    if (kind === "soft") {
      if (t !== soft || !clampV) return m;
      const blend = shape.blend === "additive" ? "additive" : "normal", name = shapeTextureName({ colour: shape.colour, blend, clamp: true });
      if (!seen.has(name)) { seen.add(name); softs.push({ name, colour: shape.colour.slice(), blend, clamp: true }); }
      return Object.assign({}, m, { bitmap: name });
    }
    if (t !== String(shape.texture).toLowerCase() || !copied(typeof info === "function" ? info(t) : null, clampV)) return m;
    const clamp = clampV ? CLAMP_V : 0, name = copyName(t, clamp);
    if (!seen.has(name)) { seen.add(name); copies.push({ name, texture: t, clamp }); }
    return Object.assign({}, m, { bitmap: name });
  });
  return { meshes: out, softs, copies };
}

// --- a compiled model's node tree ---------------------------------------------------------------------------------
// [{ name, parent ("" for the root), position, rotation (quaternion x y z w), flags }], parents before children, in the
// file's order - the geometry's nodes with their places (controller 8) and turns (20) as the model has them (Torlack's
// binary MDL notes; vfx_model.cpp's GeometryNodes reads them alike).
function modelNodes(bytes) {
  const BASE = 12;
  if (!Buffer.isBuffer(bytes) || bytes.length < 0x200 || bytes.readUInt32LE(0) !== 0) throw new Error("not a compiled model");
  const u32 = (o) => bytes.readUInt32LE(o);
  const str = (o, n) => bytes.toString("latin1", o, o + n).replace(/\0[\s\S]*$/, "");
  const out = [];
  const seen = new Set();
  const visit = (rel, parent, depth) => {
    const node = BASE + rel + 0x10;  // as vfx_model.cpp counts: the name at +0x10, children at +0x38, flags at +0x5C
    if (depth > 64 || node + 0x60 > bytes.length || seen.has(node)) return;
    seen.add(node);
    const name = str(node + 0x10, 32);
    const position = [0, 0, 0], rotation = [0, 0, 0, 1];
    const keys = BASE + u32(node + 0x44), count = u32(node + 0x48), data = BASE + u32(node + 0x50);
    for (let i = 0; i < count && i < 256 && keys + 12 * i + 12 <= bytes.length; i++) {
      const k = keys + 12 * i, type = bytes.readInt32LE(k), at = bytes.readInt16LE(k + 8), columns = bytes[k + 10] & 0x0f;
      const read = (n) => Array.from({ length: n }, (_, j) => bytes.readFloatLE(data + 4 * (at + j)));
      if (type === 8 && columns === 3) position.splice(0, 3, ...read(3));
      if (type === 20 && columns === 4) rotation.splice(0, 4, ...read(4));
    }
    out.push({ name, parent, position, rotation, flags: u32(node + 0x5C) });
    const children = u32(node + 0x38), n = u32(node + 0x3C);
    for (let i = 0; i < n && i < 1024; i++) visit(u32(BASE + children + 4 * i), name, depth + 1);
  };
  visit(u32(BASE + 0x48), "", 0);
  if (!out.length) throw new Error("no nodes in that model");
  return out;
}

// --- the game files' text ------------------------------------------------------------------------------------------
function meshLines(m, parent) {
  const lines = [`node trimesh ${m.name}`, `  parent ${parent}`, "  position 0 0 0", "  orientation 0 0 0 0", "  wirecolor 1 1 1",
    `  ambient ${m.ambient.map(num).join(" ")}`, `  diffuse ${m.diffuse.map(num).join(" ")}`, "  specular 0 0 0", "  shininess 1",
    `  bitmap ${m.bitmap || "NULL"}`, "  render 1", "  shadow 0", "  beaming 0", "  inheritcolor 0", `  alpha ${num(m.alpha)}`,
    `  transparencyhint ${meshHint(m)}`, `  selfillumcolor ${m.selfillum.map(num).join(" ")}`, "  tilefade 0", "  rotatetexture 0"];
  const count = m.verts.length / 3;
  lines.push(`  verts ${count}`);
  for (let i = 0; i < count; i++) lines.push(`    ${num(m.verts[3 * i])} ${num(m.verts[3 * i + 1])} ${num(m.verts[3 * i + 2])}`);
  const faces = m.faces.length / 3;
  lines.push(`  faces ${faces}`);
  for (let f = 0; f < faces; f++) {
    const [a, b, c] = [m.faces[3 * f], m.faces[3 * f + 1], m.faces[3 * f + 2]];
    const group = m.groups && m.groups[f] !== undefined ? m.groups[f] : 1;  // the mesh's own (a shape's inside 2), else 1
    lines.push(`    ${a} ${b} ${c} ${group} ${a} ${b} ${c} 0`);  // the UVs the vertex's own
  }
  lines.push(`  tverts ${count}`);
  for (let i = 0; i < count; i++) lines.push(`    ${num(m.tverts[2 * i] || 0)} ${num(m.tverts[2 * i + 1] || 0)} 0`);
  lines.push("endnode");
  return lines;
}
function dummyLines(name, parent, position, axis) {
  return [`node dummy ${name}`, `  parent ${parent}`, `  position ${position.map(num).join(" ")}`,
    `  orientation ${(axis || [0, 0, 0, 0]).map(num).join(" ")}`, "endnode"];
}
const header = (name, supermodel, classification, about, mark = MARK) => ["#MAXMODEL ASCII", mark, ...about.map((a) => "# " + a),
  `newmodel ${name}`, `setsupermodel ${name} ${supermodel}`, `classification ${classification}`, "setanimationscale 1",
  `beginmodelgeom ${name}`, `node dummy ${name}`, "  parent NULL", "endnode"];

// A stage's creature model: supermodel c_invsguy (its walk and idles), every node of c_invsguy's as a dummy where it is
// (`nodes`: modelNodes(c_invsguy) - rootdummy, the body's parts, Impact, head, the hands, headconjure, handconjure: the
// engine's and the effects' places, moved by the supermodel's animations as on the invisible man), then the meshes - the
// junk, then the disk - straight under the model's root, so the disk floats level and steady while rootdummy walks.
function creatureModelText(name, nodes, meshes, about) {
  const lines = header(name, SUPERMODEL, "character", about);
  const root = nodes[0].name;
  for (const n of nodes.slice(1)) {
    lines.push(...dummyLines(n.name, n.parent === root ? name : n.parent, n.position, axisAngle(n.rotation)));
  }
  for (const m of meshes) lines.push(...meshLines(m, name));
  lines.push(`endmodelgeom ${name}`, "", `donemodel ${name}`, "");
  return lines.join("\n");
}

// A pile's placement nodes, as the game's junk placeables have them (PLC_B04 LootBag1, PLC_C07 TreasureSml...): named
// after the model without "plc_" (B04_hand ...): where a user's hand goes (in front, past the pile's edge), the head and
// its hit, the impact and the ground - scaled to the pile's size and height as theirs are.
function pileNodes(name, lo, hi) {
  const short = name.replace(/^plc_/, "");
  const h = Math.max(0.05, hi[2]), depth = Math.max(Math.abs(lo[1]), Math.abs(hi[1]));
  return [
    { name: short + "_hand", position: [0, -(depth + 0.35), Math.min(0.35, 0.6 * h)] },
    { name: short + "_head", position: [0, 0, h + 0.4] },
    { name: short + "_head_hit", position: [0, 0, 0.85 * h] },
    { name: short + "_impact", position: [0, 0, 0.4 * h] },
    { name: short + "_ground", position: [0, 0, 0] },
  ];
}
// `mark`: whose it is (MARK; the Composer's projects pass theirs, tools\data\composer\project.js).
function pileModelText(name, meshes, lo, hi, about, mark = MARK) {
  const lines = header(name, "NULL", "character", about, mark);
  for (const m of meshes) lines.push(...meshLines(m, name));
  for (const n of pileNodes(name, lo, hi)) lines.push(...dummyLines(n.name, name, n.position));
  lines.push(`endmodelgeom ${name}`, "", `donemodel ${name}`, "");
  return lines.join("\n");
}
// Its walkmesh file (.pwk), as the game's junk has it: no walkable faces, only the use points a creature walks to -
// `use` [[x, y], ...] (<short>_pwk_use01, _use02 ...), by default two, behind and in front, past the pile's edge.
// `block` [[x, y], ...], a convex outline counter-clockwise from above (a Composer placeable's Blocks): before them, a
// ground mesh of it that nothing walks through, as the game's crates and barrels have (PLC_A03.pwk: PLC_A03_wg under
// PLC_A03_pwk, its faces material 7, non-walkable).
function pileWalkmeshText(name, lo, hi, { mark = MARK, use = null, block = null } = {}) {
  const short = name.replace(/^plc_/, ""), depth = Math.max(Math.abs(lo[1]), Math.abs(hi[1])) + 0.1;
  const lines = ["#MAXDOOR ASCII", mark, `# model: ${name}_pwk`];
  if (block) {
    if (!Array.isArray(block) || block.length < 3) throw new Error("a blocking outline: 3 points or more");
    lines.push(`node trimesh ${name.toUpperCase()}_wg`, `  parent ${name}_pwk`, "  position 0 0 0", "  orientation 0 0 0 0", "  bitmap NULL",
      `  verts ${block.length}`, ...block.map((p) => `    ${num(p[0])} ${num(p[1])} 0`), `  faces ${block.length - 2}`);
    for (let i = 1; i + 1 < block.length; i++) lines.push(`    0 ${i} ${i + 1}  1  0 0 0  7`);  // a fan; smoothing group 1, material 7
    lines.push("endnode");
  }
  const points = use ? use.map((p) => [p[0], p[1], 0]) : [[0, -depth, 0], [0, depth, 0]];
  points.forEach((at, i) => lines.push(...dummyLines(`${short}_pwk_use${String(i + 1).padStart(2, "0")}`, name + "_pwk", at)));
  lines.push("");
  return lines.join("\n");
}

// nwn5e's rows (data/README.md) copying one row of the game's `table` ({ columns, rows: Map(row -> { column: value }) })
// into `entries` [{ row, label, model }]: every column named (a row inside the table changes only those), the label
// and model columns changed, `overrides` { column: value } for every entry too. `mark`: whose it is.
function rowsText(table, sourceRow, entries, { labelColumn, modelColumn, overrides = {}, about = [], mark = MARK }) {
  const source = table.rows.get(sourceRow);
  if (!source) throw new Error(`the game's table has no row ${sourceRow}`);
  const columns = table.columns;
  for (const c of [labelColumn, modelColumn, ...Object.keys(overrides)]) if (!columns.includes(c)) throw new Error(`the game's table has no column ${c}`);
  const cell = (v) => {
    const s = v === undefined || v === "" ? "****" : String(v);
    return /\s/.test(s) ? `"${s}"` : s;
  };
  const table2 = [["", ...columns]];
  for (const e of entries) {
    const row = Object.assign({}, source, overrides, { [labelColumn]: e.label, [modelColumn]: e.model });
    table2.push([String(e.row), ...columns.map((c) => cell(row[c]))]);
  }
  const widths = table2[0].map((_, k) => Math.max(...table2.map((r) => r[k].length)));
  const lines = [mark, ...about.map((a) => "# " + a)];
  table2.forEach((r) => lines.push(r.map((v, k) => (k === r.length - 1 ? v : v.padEnd(widths[k]))).join("  ").trimEnd()));
  lines.push("");
  return lines.join("\n");
}

// --- someone's ASCII model, made readable by the mesh reader --------------------------------------------------------
// The reader (vfx_model.cpp ReadVfxAsciiModel) takes trimesh nodes whose UVs are one per vertex. NeverBlender's models
// (and the game's ASCII ones) give each face corner its own UV index and have danglymesh / skin / animmesh nodes too.
// This keeps the geometry (up to endmodelgeom; the animations dropped): every drawn mesh kind as a trimesh in its rest
// pose, a vertex for each (vertex, UV) pair a face uses, its material; every other node a dummy where it is (the
// hierarchy kept). A node scaled other than 1 is refused (apply the scale before exporting). -> ASCII text.
const MESH_KINDS = new Set(["trimesh", "danglymesh", "skin", "animmesh"]);
function readableAscii(text, label) {
  const lines = String(text).split(/\r?\n/);
  let name = null, inGeom = false, cur = null, list = null, left = 0;
  const nodes = [];
  const words = (s) => s.replace(/#.*$/, "").trim().split(/\s+/).filter(Boolean);
  for (const raw of lines) {
    const w = words(raw);
    if (!w.length) continue;
    if (left > 0 && /^[-+.\d]/.test(w[0])) {
      if (cur && list) cur[list].push(w.map(Number));
      left--;
      continue;
    }
    left = 0;
    list = null;
    const k = w[0].toLowerCase();
    if (!inGeom) {
      if (k === "newmodel" && w[1]) name = w[1];
      if (k === "beginmodelgeom") inGeom = true;
      continue;
    }
    if (k === "endmodelgeom") break;
    if (k === "node" && w.length >= 3) {
      cur = { kind: w[1].toLowerCase(), name: w[2], parent: "NULL", position: [0, 0, 0], orientation: [0, 0, 0, 0], scale: 1, bitmap: "",
        diffuse: null, ambient: null, selfillum: null, alpha: null, render: true, transparencyhint: 0, verts: [], tverts: [], faces: [] };
      nodes.push(cur);
      continue;
    }
    if (k === "endnode") { cur = null; continue; }
    if (!cur) continue;
    const nums = w.slice(1).map(Number);
    if (k === "parent" && w[1]) cur.parent = w[1];
    else if (k === "position" && nums.length >= 3) cur.position = nums.slice(0, 3);
    else if (k === "orientation" && nums.length >= 4) cur.orientation = nums.slice(0, 4);
    else if (k === "scale" && nums.length >= 1) cur.scale = nums[0];
    else if ((k === "bitmap" || (k === "texture0" && !cur.bitmap)) && w[1]) cur.bitmap = w[1].toLowerCase() === "null" ? "" : w[1];
    else if (k === "diffuse" && nums.length >= 3) cur.diffuse = nums.slice(0, 3);
    else if (k === "ambient" && nums.length >= 3) cur.ambient = nums.slice(0, 3);
    else if (k === "selfillumcolor" && nums.length >= 3) cur.selfillum = nums.slice(0, 3);
    else if (k === "alpha" && nums.length >= 1) cur.alpha = nums[0];
    else if (k === "render" && w[1]) cur.render = w[1] !== "0";
    else if (k === "transparencyhint" && w[1]) cur.transparencyhint = hintValue(w[1]);  // as the game reads it (T1b)
    else if ((k === "verts" || k === "tverts" || k === "faces") && Number.isInteger(nums[0])) { list = k; left = nums[0]; }
    else if (Number.isInteger(nums[0]) && nums.length === 1) { list = null; left = nums[0]; }  // another list (weights, constraints, ...): skipped
  }
  if (!name) throw new Error(`${label}: no "newmodel" line - not an ASCII model`);
  if (!inGeom) throw new Error(`${label}: no "beginmodelgeom" - not an ASCII model`);
  const out = ["#MAXMODEL ASCII", `newmodel ${name}`, `setsupermodel ${name} NULL`, "classification effect", "setanimationscale 1", `beginmodelgeom ${name}`];
  let drawn = 0;
  for (const n of nodes) {
    if (Math.abs(n.scale - 1) > 1e-4) throw new Error(`${label}: node ${n.name} is scaled ${n.scale} - apply the scale in Blender before exporting`);
    const parent = n.parent;
    if (!MESH_KINDS.has(n.kind) || !n.verts.length || !n.faces.length) {
      out.push(`node dummy ${n.name}`, `  parent ${parent}`, `  position ${n.position.join(" ")}`, `  orientation ${n.orientation.join(" ")}`, "endnode");
      continue;
    }
    // One vertex per (vertex, UV) pair a face uses; faces as "v1 v2 v3 group t1 t2 t3 material" (3 numbers: the UVs the
    // vertex's own).
    const index = new Map(), verts = [], tverts = [], faces = [];
    for (const f of n.faces) {
      if (f.length < 3) continue;
      const vs = f.slice(0, 3), ts = f.length >= 7 ? f.slice(4, 7) : vs;
      for (let c = 0; c < 3; c++) {
        const v = vs[c], t = ts[c];
        if (!n.verts[v]) throw new Error(`${label}: mesh ${n.name}: a face uses vertex ${v} of ${n.verts.length}`);
        const key = v + "/" + t;
        if (!index.has(key)) {
          index.set(key, verts.length);
          verts.push(n.verts[v].slice(0, 3));
          const uv = n.tverts[t] || [0, 0];
          tverts.push([uv[0] || 0, uv[1] || 0]);
        }
        faces.push(index.get(key));
      }
    }
    drawn++;
    out.push(`node trimesh ${n.name}`, `  parent ${parent}`, `  position ${n.position.join(" ")}`, `  orientation ${n.orientation.join(" ")}`,
      `  bitmap ${n.bitmap || "NULL"}`, `  render ${n.render ? 1 : 0}`);
    if (n.diffuse) out.push(`  diffuse ${n.diffuse.join(" ")}`);
    if (n.ambient) out.push(`  ambient ${n.ambient.join(" ")}`);
    if (n.selfillum) out.push(`  selfillumcolor ${n.selfillum.join(" ")}`);
    if (n.alpha !== null) out.push(`  alpha ${n.alpha}`);
    if (n.transparencyhint) out.push(`  transparencyhint ${n.transparencyhint}`);  // T1b: the mesh reader keeps it
    out.push(`  verts ${verts.length}`, ...verts.map((v) => `    ${v.join(" ")}`));
    out.push(`  faces ${faces.length / 3}`);
    for (let i = 0; i < faces.length; i += 3) out.push(`    ${faces[i]} ${faces[i + 1]} ${faces[i + 2]} 1 ${faces[i]} ${faces[i + 1]} ${faces[i + 2]} 0`);
    out.push(`  tverts ${tverts.length}`, ...tverts.map((t) => `    ${t[0]} ${t[1]} 0`), "endnode");
  }
  if (!drawn) throw new Error(`${label}: no mesh in its geometry (trimesh, danglymesh, skin or animmesh with faces)`);
  out.push(`endmodelgeom ${name}`, `donemodel ${name}`, "");
  return out.join("\n");
}

// A texture beside a part's own file: the entries of its folder (`listing`) that are <name>.tga / .dds / .txi, any case.
// { image: the tga or dds (tga first) or null, txi: or null }
function texturesBeside(name, listing) {
  const find = (ext) => listing.find((f) => f.toLowerCase() === (name + ext).toLowerCase()) || null;
  return { image: find(".tga") || find(".dds"), txi: find(".txi") };
}

// --- whose a file is --------------------------------------------------------------------------------------------------
// A text file of ours carries MARK on its first line, or on its second after the format's own first line (#MAXMODEL
// ASCII, #MAXDOOR ASCII); a draft for the bench likewise. `mark`: whose (MARK, or a Composer project's).
function carriesMark(bytes, mark = MARK) {
  const lines = Buffer.from(bytes).toString("latin1").split(/\r?\n/, 2);
  return lines[0] === mark || (/^#MAX(MODEL|DOOR) ASCII$/.test(lines[0]) && lines[1] === mark);
}
const sameText = (a, b) => Buffer.from(a).toString("latin1").replace(/\r\n/g, "\n") === Buffer.from(b).toString("latin1").replace(/\r\n/g, "\n");
const crlf = (text) => text.replace(/\r?\n/g, "\r\n");

const api = {
  MARK, TEXTURE_MARK, STAGES, PILES, SUFFIX, RESREF, SUPERMODEL, SOURCE_ROWS, APPEARANCE_OVERRIDES, ROWS, LABELS,
  creatureModel, pileModel,
  PLT_LAYERS, PLT_PALETTES, PLT_FIELDS, PLT_LABELS, PLT_DEFAULTS, PLT_MAX, BAKED,
  readColours, fullColours, cleanColours, readPlt, bakePlt, bakedName, tgaBytes, bakeBitmaps, isWhite, readTint, tintMeshes, drawnMaterial,
  resolvePartFile, readPart, readComposition, partKey, partLabel, quat, axisAngle, qrot, round, num, HINT_MAX, hintValue, meshHint, modelSpaceMeshes, placePart,
  lowered, uniqueNames, diskMesh, diskTexture, modelNodes, creatureModelText, pileNodes, pileModelText,
  pileWalkmeshText, rowsText, readableAscii, texturesBeside, carriesMark, sameText, crlf, mergeModels, inline, compositionText,
  SHAPE_TYPES, SHAPE_LABELS, SHAPE_KEYS, SHAPE_FIELDS, SHAPE_FLAT, LOOK_FIELDS, SHAPE_INFO, SHAPE_TEXTURE,
  shapeDefaults, readShape, cleanShape, shapeModel, shapeTexture, shapeTextureName, SOFT_TXI, txiAdditive,
  MOTION_KEYS, MOTION_RATE, MOTION_INFO, MOTION_TEXTURE, MOTION_SHADER, MOTION_PARAMS, MOTION_SHADER_MARK, readMotion, hasMotion, cleanMotion,
  uvPivot, uvMotion, motionName, motionMaterialText, motionShaderText, isMotionShader, moveTextures,
  UV_REPEAT_EPSILON, CLAMP_TXI, motionClamps, clampedTxi, txiClamps,
  CLAMP_V, COPY_TEXTURE, txiDecal, txiClampBits, txiCycle, copiedTxi, softTxi, vClamps, copyName, copied, copyKind, stillTextures,
};
if (typeof module === "object" && module.exports) module.exports = api;
else root.FloatDisk = api;
})(typeof self !== "undefined" ? self : this);
