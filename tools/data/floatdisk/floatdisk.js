// The Floating Disk generator's pure parts (#39; build.js does the reading and writing, check.js tests these): the
// composition, the maths that places the junk, the disk's mesh and texture, the game files' text (the creature and pile
// models, the piles' use points, the 2da rows), a compiled model's node tree, and an ASCII model made readable by the
// mesh reader. Nothing here touches the disk or the game.
"use strict";
const path = require("path");

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
  return path.resolve(baseDir, file.trim());
}

// composition.json's object -> { disk, stages: { empty: [], small: [part], ... } }; a part { source: { kind: "model",
// model } | { kind: "file", file (absolute), given }, at, turn, scale, tilt, note }. `baseDir` resolves a part's file.
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
    stages[stage] = list.map((p, i) => {
      const where = `stages.${stage}[${i}]`;
      const hasModel = p.model !== undefined, hasFile = p.file !== undefined;
      if (hasModel === hasFile) throw new Error(`${where}: either "model" (a game model) or "file" (your own .mdl), not both or neither`);
      let source;
      if (hasModel) {
        const model = String(p.model || "").toLowerCase();
        if (!RESREF.test(model)) throw new Error(`${where}.model: a model's resref`);
        source = { kind: "model", model };
      } else {
        let file;
        try { file = resolvePartFile(baseDir, p.file); } catch (e) { throw new Error(`${where}.file: ${e.message}`); }
        source = { kind: "file", file, given: String(p.file) };
      }
      if (!Array.isArray(p.at) || p.at.length !== 3 || !p.at.every(isNum)) throw new Error(`${where}.at: [x, y, z] in metres`);
      const turn = p.turn === undefined ? 0 : p.turn, scale = p.scale === undefined ? 1 : p.scale;
      const tilt = p.tilt === undefined ? [0, 0] : p.tilt;
      if (!isNum(turn)) throw new Error(`${where}.turn: degrees`);
      if (!isNum(scale) || scale <= 0) throw new Error(`${where}.scale: a number above 0`);
      if (!Array.isArray(tilt) || tilt.length !== 2 || !tilt.every(isNum)) throw new Error(`${where}.tilt: [degrees about x, degrees about y]`);
      return { source, at: p.at, turn, scale, tilt, note: p.note || "" };
    });
  }
  return { disk, stages };
}
const partKey = (part) => (part.source.kind === "model" ? "model:" + part.source.model : "file:" + part.source.file.toLowerCase());
const partLabel = (part) => (part.source.kind === "model" ? part.source.model : part.source.given);

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
  const size = 128, idBytes = Buffer.from(id, "latin1");
  const head = Buffer.alloc(18);
  head[0] = idBytes.length; head[2] = 2; head.writeUInt16LE(size, 12); head.writeUInt16LE(size, 14); head[16] = 32; head[17] = 0x08;
  const body = Buffer.alloc(size * size * 4);
  const mix = (a, b, t) => a + (b - a) * t;
  const smooth = (e0, e1, x) => { const t = Math.max(0, Math.min(1, (x - e0) / (e1 - e0))); return t * t * (3 - 2 * t); };
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
  return Buffer.concat([head, idBytes, body]);
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
    lines.push(`    ${a} ${b} ${c} 1 ${a} ${b} ${c} 0`);  // smoothing group 1; the UVs the vertex's own
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
const header = (name, supermodel, classification, about) => ["#MAXMODEL ASCII", MARK, ...about.map((a) => "# " + a),
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
function pileModelText(name, meshes, lo, hi, about) {
  const lines = header(name, "NULL", "character", about);
  for (const m of meshes) lines.push(...meshLines(m, name));
  for (const n of pileNodes(name, lo, hi)) lines.push(...dummyLines(n.name, name, n.position));
  lines.push(`endmodelgeom ${name}`, "", `donemodel ${name}`, "");
  return lines.join("\n");
}
// Its walkmesh file (.pwk), as the game's junk has it: no walkable faces, only the two use points a creature walks to
// (behind and in front, past the pile's edge).
function pileWalkmeshText(name, lo, hi) {
  const short = name.replace(/^plc_/, ""), depth = Math.max(Math.abs(lo[1]), Math.abs(hi[1])) + 0.1;
  const lines = ["#MAXDOOR ASCII", MARK, `# model: ${name}_pwk`];
  [[0, -depth, 0], [0, depth, 0]].forEach((at, i) => lines.push(...dummyLines(`${short}_pwk_use0${i + 1}`, name + "_pwk", at)));
  lines.push("");
  return lines.join("\n");
}

// nwn5e's rows (data/README.md) copying one row of the game's `table` ({ columns, rows: Map(row -> { column: value }) })
// into `entries` [{ row, label, model }]: every column named (a row inside the table changes only those), the label
// and model columns changed, `overrides` { column: value } for every entry too.
function rowsText(table, sourceRow, entries, { labelColumn, modelColumn, overrides = {}, about = [] }) {
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
  const lines = [MARK, ...about.map((a) => "# " + a)];
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
// ASCII, #MAXDOOR ASCII); a draft for the bench likewise.
function carriesMark(bytes) {
  const lines = Buffer.from(bytes).toString("latin1").split(/\r?\n/, 2);
  return lines[0] === MARK || (/^#MAX(MODEL|DOOR) ASCII$/.test(lines[0]) && lines[1] === MARK);
}
const sameText = (a, b) => Buffer.from(a).toString("latin1").replace(/\r\n/g, "\n") === Buffer.from(b).toString("latin1").replace(/\r\n/g, "\n");
const crlf = (text) => text.replace(/\r?\n/g, "\r\n");

module.exports = {
  MARK, TEXTURE_MARK, STAGES, PILES, SUFFIX, RESREF, SUPERMODEL, SOURCE_ROWS, ROWS, LABELS, creatureModel, pileModel,
  resolvePartFile, readComposition, partKey, partLabel, quat, axisAngle, qrot, round, num, modelSpaceMeshes, placePart,
  lowered, uniqueNames, diskMesh, diskTexture, modelNodes, creatureModelText, pileNodes, pileModelText,
  pileWalkmeshText, rowsText, readableAscii, texturesBeside, carriesMark, sameText, crlf,
};
