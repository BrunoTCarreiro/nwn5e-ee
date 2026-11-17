// The Floating Disk generator's pure parts (#39; build.js does the reading and writing, check.js tests these): the
// composition, the maths that places the junk, the disk's mesh and texture, the Composer's shapes (a dome: its mesh and
// generated texture), the game files' text (the creature and pile models, the piles' use points, the 2da rows), a
// compiled model's node tree, and an ASCII model made readable by the mesh reader. Nothing here touches the disk or the
// game.
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
// | { kind: "shape", shape (readShape's: every key) }, at, turn, scale, tilt, note, colours (the ten layers') }. `where`
// names it in an error (stages.small[0]); `baseDir` resolves its file. A shape has no scale (its radius and height size
// it) and no dyes (its colour is on the shape): its colours are the defaults.
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
  }
  if (!Array.isArray(p.at) || p.at.length !== 3 || !p.at.every(isNum)) throw new Error(`${where}.at: [x, y, z] in metres`);
  const turn = p.turn === undefined ? 0 : p.turn, scale = p.scale === undefined ? 1 : p.scale;
  const tilt = p.tilt === undefined ? [0, 0] : p.tilt;
  if (!isNum(turn)) throw new Error(`${where}.turn: degrees`);
  if (!isNum(scale) || scale <= 0) throw new Error(`${where}.scale: a number above 0`);
  if (!Array.isArray(tilt) || tilt.length !== 2 || !tilt.every(isNum)) throw new Error(`${where}.tilt: [degrees about x, degrees about y]`);
  return { source, at: p.at, turn, scale, tilt, note: p.note || "", colours: readColours(p.colours, `${where}.colours`) };
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
// a key a line, each stage a part a line (note, model | file | shape, at, turn, scale, tilt, colours, then any other
// key); arrays and a shape inline.
function compositionText(c) {
  const PART_KEYS = ["note", "model", "file", "shape", "at", "turn", "scale", "tilt", "colours"];
  const part = (p) => {
    const keys = PART_KEYS.filter((k) => p[k] !== undefined).concat(Object.keys(p).filter((k) => !PART_KEYS.includes(k) && p[k] !== undefined));
    return "{ " + keys.map((k) => JSON.stringify(k) + ": " + inline(p[k])).join(", ") + " }";
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

// A read model (a draft's nodes and planes, as core.readDraft gives them) -> its drawn meshes with every vertex in the
// model's space (each mesh's own turn and place, then each parent's; a parent the draft doesn't have is the root). A
// render-0 mesh (a damage box, a shadow mesh) is left out. { meshes: [{ name, bitmap, verts, tverts, faces, diffuse,
// ambient, selfillum, alpha }], lo, hi }
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
      alpha: p.alpha === undefined ? 1 : p.alpha });
  }
  if (!meshes.length) throw new Error(`${label}: no mesh the game draws`);
  return { meshes, lo, hi };
}

// A part's meshes where the composition puts it: scaled, tilted (about x, then y), turned (about z), then moved so the
// middle of its footprint (its box from above) is at at.x, at.y and its lowest point at.z above `floor` (the disk's
// top). { meshes (named prefix + its number + "_" + the mesh's name), lo, hi, reach (how far from the centre it reaches
// in x y) }
function placePart(part, model, index, floor, prefix) {
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
  return { meshes, lo, hi, reach };
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
    selfillum: disk.selfillum, alpha: disk.alpha, transparent: true };
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
const SHAPE_TYPES = ["dome"];
const SHAPE_LABELS = { dome: "Dome (half sphere)" };
const SHAPE_KEYS = ["type", "radius", "height", "segments", "sides", "mapping", "repeat_around", "repeat_up", "colour", "alpha", "glow", "texture"];  // the order they're written in
const SHAPE_FIELDS = { dome: ["radius", "height", "segments", "sides"] };  // a type's own fields, the card's order
const LOOK_FIELDS = ["mapping", "repeat_around", "repeat_up", "colour", "alpha", "glow", "texture"];  // every type's
// A field's card label and bounds: what readShape enforces, and what the page shows.
const SHAPE_INFO = {
  radius: { label: "Radius (m)", min: 0.05, max: 100, step: 0.05 },
  height: { label: "Height (m)", min: 0.05, max: 100, step: 0.05 },
  segments: { label: "Smoothness", min: 8, max: 96, step: 4, whole: true, title: "Segments round it; the rings up it are a quarter of that" },
  sides: { label: "Sides", choices: [["both", "Both: seen from inside too"], ["outside", "Outside only"]] },
  mapping: { label: "Mapping", choices: [["wrap", "Wrapped: round it and up it"], ["above", "From above: laid on from overhead"]] },
  repeat_around: { label: "Repeat around", min: 1, max: 32, step: 1, whole: true, title: "How many times the texture goes round it" },
  repeat_up: { label: "Repeat up", min: 1, max: 16, step: 1, whole: true, title: "How many times the texture repeats from the ground to the top" },
  colour: { label: "Colour", colour: true },
  alpha: { label: "Opacity", min: 0.02, max: 1, step: 0.05, title: "Its alpha: 1 is solid; the game's globes go to about 0.3" },
  glow: { label: "Glow", min: 0, max: 1, step: 0.05, title: "How much it lights itself (selfillum): at 1 it looks the same by day and night" },
  // Soft or none, or a library texture's name (the game's own, named: nothing generated or copied).
  texture: { label: "Texture", choices: [["soft", "Soft: brighter at the ground"], ["none", "None: its colour only"]],
    title: "Soft is made for Wrapped; a texture from the library is tinted by Colour" },
};
const SHAPE_TEXTURE = /^n5s_[0-9a-f]{12}$/;  // a soft texture's name: made, never picked
const orList = (list) => (list.length > 1 ? list.slice(0, -1).join(", ") + " or " + list[list.length - 1] : list.join(""));

function shapeDefaults(type, radius) {
  return { type, radius, height: radius, segments: 32, sides: "both", mapping: "wrap", repeat_around: 1, repeat_up: 1,
    colour: [0.6, 0.8, 1], alpha: 0.35, glow: 1, texture: "soft" };
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
  if (info.colour) return `${where}.${k}: [r, g, b], each 0 to 1`;
  if (info.choices) return `${where}.${k}: ${orList(info.choices.map((c) => c[0]))}`;
  return `${where}.${k}: ${info.whole ? "a whole number" : "a number"} from ${info.min} to ${info.max}${/\(m\)$/.test(info.label) ? " (metres)" : ""}`;
}
// A shape as a part gives it -> the full shape: shapeDefaults(type, radius) with its keys over it (its colour copied).
// `where` names it in an error (stages.small[0].shape); each error names its field.
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
  return out;
}
// A shape as it's written (`s` a right shape, whole or not): type and radius, then each other key whose value differs
// from shapeDefaults' (a colour by its three numbers), in SHAPE_KEYS' order. Under From above the repeats are left out:
// they mean nothing there.
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

// A soft texture's name: n5s_ and 12 hex digits of its colour (to 0.001) - one per colour, the same in a page.
function shapeTextureName({ colour }) {
  return "n5s_" + cyrb53("soft:" + colour.map((c) => String(Math.round(c * 1000) / 1000)).join(",")).toString(16).padStart(14, "0").slice(-12);
}
// The soft texture, made for Wrapped: its colour, brighter and more opaque at the ground ring (v 0) and fading towards
// the top (v 1), with a faint shimmer of whole turns in u, so it wraps round seamlessly. 128 x 128 -> { width, height,
// data: Uint8ClampedArray RGBA, rows top-down } (as bakePlt gives a picture; tgaBytes with alpha writes it).
function shapeTexture({ colour }) {
  const size = 128, data = new Uint8ClampedArray(size * size * 4);
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

// One side of a dome: rings up it and n vertices at its top, its faces counter-clockwise seen from outside.
// -> { verts, tverts, faces }, its vertices and UVs rounded as diskMesh's, never -0 (so the mesh through JSON is the
// same mesh). `uv(x, y, u, v)`: a vertex's UV from its position and its wrapped (u, v) at one repeat.
function domeSide(shape, uv) {
  const r = shape.radius, h = shape.height, n = shape.segments, m = Math.max(2, Math.round(n / 4));
  const verts = [], tverts = [], faces = [];
  const tidy = (v) => round(v) + 0;  // -0 + 0 is 0
  const vertex = (x, y, z, u, v) => { verts.push(tidy(x), tidy(y), tidy(z)); tverts.push(...uv(x, y, u, v).map(tidy)); return verts.length / 3 - 1; };
  const ring = [];
  for (let j = 0; j < m; j++) {  // ring j at latitude (PI / 2) j / m; vertex i at angle 2 PI i / n, the seam's twice (0 and n)
    const phi = (Math.PI / 2) * j / m, c = Math.cos(phi), s = Math.sin(phi);
    ring.push(Array.from({ length: n + 1 }, (_, i) => {
      const theta = (2 * Math.PI * i) / n;
      return vertex(r * c * Math.cos(theta), r * c * Math.sin(theta), h * s, i / n, j / m);
    }));
  }
  const top = Array.from({ length: n }, (_, i) => vertex(0, 0, h, (i + 0.5) / n, 1));
  for (let j = 0; j < m - 1; j++) {
    for (let i = 0; i < n; i++) {
      const a = ring[j][i], b = ring[j][i + 1], c = ring[j + 1][i + 1], d = ring[j + 1][i];
      faces.push(a, b, c, a, c, d);
    }
  }
  for (let i = 0; i < n; i++) faces.push(ring[m - 1][i], ring[m - 1][i + 1], top[i]);
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
// diffuse, ambient, selfillum, alpha, transparent }], lo, hi, textures: [{ name, colour }] (the soft texture to make, or
// none) }.
// - Its UVs: Wrapped, u round it from +x and v from the ground ring up, times its repeats; From above, laid on from
//   overhead, its width the picture's square ((0.5 + x / 2r, 0.5 + y / 2r)).
// - Its material: soft, the texture's colour lit by its glow; none, its own colour; a library texture, tinted by its
//   colour and see-through (its own alpha isn't known here).
function shapeModel(shape) {
  const r = shape.radius;
  const uv = shape.mapping === "above" ? (x, y) => [0.5 + x / (2 * r), 0.5 + y / (2 * r)]
    : (x, y, u, v) => [u * shape.repeat_around, v * shape.repeat_up];
  const side = domeSide(shape, uv);
  const { verts, tverts, faces, groups } = shape.sides === "both" ? bothSides(side) : Object.assign({ groups: new Array(side.faces.length / 3).fill(1) }, side);
  const soft = shape.texture === "soft", colour = shape.colour, glow = shape.glow;
  const bitmap = soft ? shapeTextureName(shape) : shape.texture === "none" ? "" : shape.texture;
  const mesh = { name: shape.type, bitmap, verts, tverts, faces, groups,
    diffuse: soft ? [1, 1, 1] : colour.slice(), ambient: soft ? [1, 1, 1] : colour.slice(),
    selfillum: soft ? [glow, glow, glow] : colour.map((c) => round(c * glow)),
    alpha: shape.alpha, transparent: shape.texture !== "none" || shape.alpha < 1 };
  return { meshes: [mesh], lo: [-r, -r, 0], hi: [r, r, shape.height], textures: soft ? [{ name: bitmap, colour: colour.slice() }] : [] };
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
    `  transparencyhint ${m.transparent ? 1 : 0}`, `  selfillumcolor ${m.selfillum.map(num).join(" ")}`, "  tilefade 0", "  rotatetexture 0"];
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
    else if (k === "transparencyhint" && w[1]) cur.transparencyhint = Number(w[1]) || 0;
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
  readColours, fullColours, cleanColours, readPlt, bakePlt, bakedName, tgaBytes, bakeBitmaps,
  resolvePartFile, readPart, readComposition, partKey, partLabel, quat, axisAngle, qrot, round, num, modelSpaceMeshes, placePart,
  lowered, uniqueNames, diskMesh, diskTexture, modelNodes, creatureModelText, pileNodes, pileModelText,
  pileWalkmeshText, rowsText, readableAscii, texturesBeside, carriesMark, sameText, crlf, mergeModels, inline, compositionText,
  SHAPE_TYPES, SHAPE_LABELS, SHAPE_KEYS, SHAPE_FIELDS, LOOK_FIELDS, SHAPE_INFO, SHAPE_TEXTURE,
  shapeDefaults, readShape, cleanShape, shapeModel, shapeTexture, shapeTextureName,
};
if (typeof module === "object" && module.exports) module.exports = api;
else root.FloatDisk = api;
})(typeof self !== "undefined" ? self : this);
