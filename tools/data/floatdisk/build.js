#!/usr/bin/env node
// Floating Disk's four looks (#39; the owner, 2026-11-16: "I'm imagining an actual disk and there would be 4 stages to
// it: empty - just the disk; 0-33%, 33%-66%, 66%-99% of the weight they would have 3 different meshes of increasing size
// of random junk on top" - then "go with all your recommendations, let them compose it. If I don't like the
// compositions, is there a way to compose them myself?"). This is that way: composition.json beside this file says what
// each pile is made of - the game's own placeable models, each placed, turned, tilted and scaled on the disk's top - and
// this generator rebuilds everything from it. A part: { "model": a placeable's model (placeables.2da ModelName), "at":
// [x, y, z] - x to the disk's right, y to its front (where a creature faces), metres from its centre: where the middle
// of the part's footprint goes, and z how high its lowest point sits above the disk's top (0: on the disk; on another
// part: that part's height there), "turn": degrees about the vertical (counter-clockwise from above), "scale" (1: the
// game's size), and, optional, "tilt": [degrees about x, degrees about y] (leaning it, before the turn) }.
//
// Phase 1 (this file): each stage (empty, small, medium, large) as a VFX Bench draft - the disk's mesh plus the parts'
// meshes, their vertices already where the composition puts them (each mesh's own node chain applied, then the part's
// scale, tilt, turn and place), with their textures and materials - written to the bench's saved folder as
// n5_floatdisk_<stage>.vfx, and the disk's texture (n5t_forcedisk.tga, made here) to the bench's library textures.
// Phase 2 (not yet): the creature models (supermodel c_invsguy) and the piles as placeables.
//
// Nothing of the game's is kept in the repo: the parts are read from the player's own game at build time (its KEY /
// BIF archives and ovr\, through nwn5e-tools' spell_bench\game_data.js) and their meshes through nwn5e-ee's own model
// reader (src\data\vfx_model.cpp ReadVfxBinaryModel, every mesh kind) compiled into a small console tool - `mdlmesh`,
// which prints a model's meshes as a draft.
//
//   node tools\data\floatdisk\build.js [options]
//     --composition <file>  default: composition.json beside this file
//     --game <folder>       the game (default: NWN_GAME_DIR, nwn5e.ini's [loader] game_dir, the Steam folder)
//     --build <folder>      nwn5e's build folder whose vfxedit\ the bench uses (default: NWN5E_BUILD, else
//                           ..\nwn5e-ee\build beside nwn5e-tools - the bench's own default)
//     --tools <folder>      nwn5e-tools (default: NWN5E_TOOLS, else beside this repo)
//     --reader <exe>        the mdlmesh tool (default: NWN5E_MDLMESH)
//     --out <folder>        where the drafts go (default: <build>\vfxedit\saved)
//     --textures <folder>   where the disk's texture goes (default: <build>\vfxedit\library\textures)
//     --check               read and place everything, print the summary, write nothing
//
// It never overwrites a file it didn't make: a draft must carry the MARK line, the texture the TEXTURE_MARK in its TGA
// image ID - anything else of that name stops it.
"use strict";
const fs = require("fs");
const os = require("os");
const path = require("path");
const { execFileSync } = require("child_process");

const MARK = "# made by nwn5e-ee tools/data/floatdisk/build.js from composition.json - edit that and rebuild, not this file";
const TEXTURE_MARK = "nwn5e floatdisk build.js";
const STAGES = ["empty", "small", "medium", "large"];
const RESREF = /^[a-z0-9_]{1,16}$/;

// --- options ------------------------------------------------------------------------------------------------------
const argv = process.argv.slice(2);
const option = (name, fallback) => {
  const i = argv.indexOf("--" + name);
  return i >= 0 && i + 1 < argv.length ? argv[i + 1] : fallback;
};
const CHECK = argv.includes("--check");
const TOOLS = path.resolve(option("tools", process.env.NWN5E_TOOLS || path.join(__dirname, "..", "..", "..", "..", "nwn5e-tools")));
const BUILD = path.resolve(option("build", process.env.NWN5E_BUILD || path.join(TOOLS, "..", "nwn5e-ee", "build")));
const OUT = path.resolve(option("out", path.join(BUILD, "vfxedit", "saved")));
const TEXTURES = path.resolve(option("textures", path.join(BUILD, "vfxedit", "library", "textures")));
const COMPOSITION = path.resolve(option("composition", path.join(__dirname, "composition.json")));
const READER = option("reader", process.env.NWN5E_MDLMESH || "");

function need(file, what) {
  if (!fs.existsSync(file)) throw new Error(`${what} isn't there: ${file}`);
  return file;
}
global.window = global.window || {};  // core.js runs in the page and under node
const core = require(need(path.join(TOOLS, "vfx_bench", "public", "core.js"), "nwn5e-tools' VFX Bench core (--tools)"));
const gameData = require(need(path.join(TOOLS, "spell_bench", "game_data.js"), "nwn5e-tools' game data reader (--tools)"));
const helper = require(need(path.join(TOOLS, "anim_helper", "helper.js"), "nwn5e-tools' anim helper (--tools)"));
const GAME = helper.findGameDir({ game: option("game", ""), buildDir: BUILD });

// --- the composition --------------------------------------------------------------------------------------------------
const isNum = (v) => typeof v === "number" && Number.isFinite(v);
function readComposition(file) {
  const c = JSON.parse(fs.readFileSync(file, "utf8"));
  const d = c.disk || {};
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
  if (disk.texture && !RESREF.test(disk.texture)) throw new Error("disk.texture: a resref (a-z 0-9 _, up to 16) or \"\" for none");
  const stages = { empty: [] };
  for (const stage of ["small", "medium", "large"]) {
    const list = (c.stages || {})[stage];
    if (!Array.isArray(list)) throw new Error(`stages.${stage}: a list of parts`);
    stages[stage] = list.map((p, i) => {
      const where = `stages.${stage}[${i}]`;
      const model = String(p.model || "").toLowerCase();
      if (!RESREF.test(model)) throw new Error(`${where}.model: a model's resref`);
      if (!Array.isArray(p.at) || p.at.length !== 3 || !p.at.every(isNum)) throw new Error(`${where}.at: [x, y, z] in metres`);
      const turn = p.turn === undefined ? 0 : p.turn, scale = p.scale === undefined ? 1 : p.scale;
      const tilt = p.tilt === undefined ? [0, 0] : p.tilt;
      if (!isNum(turn)) throw new Error(`${where}.turn: degrees`);
      if (!isNum(scale) || scale <= 0) throw new Error(`${where}.scale: a number above 0`);
      if (!Array.isArray(tilt) || tilt.length !== 2 || !tilt.every(isNum)) throw new Error(`${where}.tilt: [degrees about x, degrees about y]`);
      return { model, at: p.at, turn, scale, tilt, note: p.note || "" };
    });
  }
  return { disk, stages };
}

// --- maths ----------------------------------------------------------------------------------------------------------
function quat(o) {  // the draft's axis-angle as a quaternion (x y z w)
  const len = Math.hypot(o[0], o[1], o[2]), half = (o[3] || 0) / 2;
  if (!len || !o[3]) return [0, 0, 0, 1];
  const s = Math.sin(half) / len;
  return [o[0] * s, o[1] * s, o[2] * s, Math.cos(half)];
}
function qrot(q, v) {
  const c1 = [q[1] * v[2] - q[2] * v[1], q[2] * v[0] - q[0] * v[2], q[0] * v[1] - q[1] * v[0]];
  const c2 = [q[1] * c1[2] - q[2] * c1[1], q[2] * c1[0] - q[0] * c1[2], q[0] * c1[1] - q[1] * c1[0]];
  return [v[0] + 2 * (q[3] * c1[0] + c2[0]), v[1] + 2 * (q[3] * c1[1] + c2[1]), v[2] + 2 * (q[3] * c1[2] + c2[2])];
}
const round = (v) => Math.round(v * 1e5) / 1e5;  // 0.01 mm: the drafts stay small

// --- the parts: the game's models, their meshes in the model's own space ---------------------------------------------
// { meshes: [{ name, bitmap, verts (model space), tverts, faces, diffuse, ambient, selfillum, alpha }], lo, hi }
function readModel(model, res, tmp) {
  const bytes = res(model, 2002);
  if (!bytes) throw new Error(`${model}: no such model in the game (${GAME})`);
  const file = path.join(tmp, model + ".mdl");
  fs.writeFileSync(file, bytes);
  let text;
  try {
    text = execFileSync(READER, [file], { encoding: "utf8", maxBuffer: 64 << 20, windowsHide: true });
  } catch (e) {
    throw new Error(`${model}: the reader couldn't read it - ${String(e.stderr || e.message).trim()}`);
  }
  const fx = core.readDraft(text.replace(/\r/g, ""));
  // Each mesh's vertices through its own turn and place, then its parents' up to the model's root (a parent the draft
  // doesn't have is the root, as the bench's chunk models do).
  const parents = new Map();
  for (const n of fx.nodes) if (!parents.has(n.name)) parents.set(n.name, n);
  for (const p of fx.planes) if (!parents.has(p.name)) parents.set(p.name, p);
  const place = (v, item) => {
    let out = v;
    for (let o = item, depth = 0; o && depth < 64; depth++) {
      const r = qrot(quat(o.orientation), out);
      out = [r[0] + o.position[0], r[1] + o.position[1], r[2] + o.position[2]];
      o = o.parent ? parents.get(o.parent) : null;
    }
    return out;
  };
  const meshes = [];
  const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
  for (const p of fx.planes) {
    if (p.render === false || !p.faces.length) continue;  // not drawn in the game (its damage box, a shadow mesh)
    const verts = [];
    for (let i = 0; i + 2 < p.verts.length; i += 3) {
      const v = place([p.verts[i], p.verts[i + 1], p.verts[i + 2]], p);
      verts.push(...v);
      for (let k = 0; k < 3; k++) { lo[k] = Math.min(lo[k], v[k]); hi[k] = Math.max(hi[k], v[k]); }
    }
    meshes.push({ name: p.name, bitmap: String(p.bitmap || "").toLowerCase(), verts, tverts: p.tverts.slice(), faces: p.faces.slice(),
      diffuse: p.diffuse, ambient: p.ambient, selfillum: p.selfillum, alpha: p.alpha });
  }
  if (!meshes.length) throw new Error(`${model}: no mesh the game draws`);
  return { meshes, lo, hi };
}

// A part's meshes where the composition puts it: scaled, tilted (about x, then y), turned (about z), then moved so the
// middle of its footprint (its box from above) is at at.x, at.y and its lowest point at.z above the disk's top. Planes
// for the draft, and its box on the disk.
function placePart(part, model, index, disk, prefix) {
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
  const move = [part.at[0] - (box[0][0] + box[1][0]) / 2, part.at[1] - (box[0][1] + box[1][1]) / 2, disk.height + part.at[2] - box[0][2]];
  const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
  let reach = 0;  // how far from the disk's centre it reaches, in x y
  const planes = model.meshes.map((m, k) => {
    const verts = [];
    for (const v of turned[k]) {
      const w = [v[0] + move[0], v[1] + move[1], v[2] + move[2]];
      for (let j = 0; j < 3; j++) { lo[j] = Math.min(lo[j], w[j]); hi[j] = Math.max(hi[j], w[j]); }
      reach = Math.max(reach, Math.hypot(w[0], w[1]));
      verts.push(round(w[0]), round(w[1]), round(w[2]));
    }
    return core.plane({ name: (prefix + String(index + 1).padStart(2, "0") + "_" + m.name).slice(0, 31), bitmap: m.bitmap || "null",
      verts, tverts: m.tverts.map(round), faces: m.faces, diffuse: m.diffuse, ambient: m.ambient, selfillum: m.selfillum, alpha: m.alpha });
  });
  return { planes, lo, hi, reach };
}

// --- the disk -------------------------------------------------------------------------------------------------------
// A flat round slab: the top cap, the rim and the bottom cap, `sides` round, its top at `height`. One mesh, textured
// across its caps from above (u 0.5 + x / D, v 0.5 + y / D; the rim at radius 0.97 of the texture, where its bright edge
// band is); faces counter-clockwise seen from outside.
function diskPlanes(disk) {
  const r = disk.diameter / 2, top = disk.height, bottom = disk.height - disk.thickness, n = disk.sides, k = 0.97;
  const verts = [], tverts = [], faces = [];
  const vertex = (x, y, z, u, v) => { verts.push(round(x), round(y), round(z)); tverts.push(round(u), round(v)); return verts.length / 3 - 1; };
  const uv = (c, s, f) => [0.5 + 0.5 * k * f * c, 0.5 + 0.5 * k * f * s];
  const ring = (z) => Array.from({ length: n }, (_, i) => {
    const a = (2 * Math.PI * i) / n, c = Math.cos(a), s = Math.sin(a), t = uv(c, s, 1);
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
  return [core.plane({ name: "forcedisk", bitmap: disk.texture || "null", verts, tverts, faces,
    diffuse: disk.colour, ambient: disk.colour.map((c) => round(c * 0.4)), selfillum: disk.selfillum, alpha: disk.alpha })];
}

// The disk's texture: soft force-blue, a faint shimmer, a brighter rim band; its alpha the texture's see-through-ness
// (the mesh's own alpha on top). 128 x 128, 32-bit, rows bottom-up as the game's TGAs are, TEXTURE_MARK as its image ID.
function diskTexture(disk) {
  const size = 128, id = Buffer.from(TEXTURE_MARK, "latin1");
  const head = Buffer.alloc(18);
  head[0] = id.length; head[2] = 2; head.writeUInt16LE(size, 12); head.writeUInt16LE(size, 14); head[16] = 32; head[17] = 0x08;
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
      const o = (row * size + col) * 4;  // row 0 is the bottom row (descriptor 0x08)
      body[o] = Math.round(rgb[2] * 255); body[o + 1] = Math.round(rgb[1] * 255); body[o + 2] = Math.round(rgb[0] * 255); body[o + 3] = Math.round(alpha * 255);
    }
  }
  return Buffer.concat([head, id, body]);
}

// --- writing ----------------------------------------------------------------------------------------------------------
// The file only if it's absent or ours; returns "new" / "rebuilt" / "unchanged".
function writeOwn(file, bytes, isOurs) {
  if (fs.existsSync(file)) {
    const old = fs.readFileSync(file);
    if (!isOurs(old)) throw new Error(`${file} is there and wasn't made by this generator: not overwritten - move it away or rename the stage`);
    if (old.equals(bytes)) return "unchanged";
    fs.writeFileSync(file, bytes);
    return "rebuilt";
  }
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, bytes);
  return "new";
}
const draftIsOurs = (bytes) => bytes.toString("latin1").split(/\r?\n/, 1)[0] === MARK;
const textureIsOurs = (bytes) => bytes.length > 18 && bytes.toString("latin1", 18, 18 + bytes[0]) === TEXTURE_MARK;

// --- main -----------------------------------------------------------------------------------------------------------
function main() {
  const comp = readComposition(COMPOSITION);
  if (!READER || !fs.existsSync(READER)) {
    throw new Error("the mdlmesh reader isn't given or isn't there (--reader <mdlmesh.exe> or NWN5E_MDLMESH): " + (READER || "none"));
  }
  if (!fs.existsSync(path.join(GAME, "data", "nwn_base.key"))) throw new Error("no game at " + GAME + " (--game <folder>)");
  const res = gameData.resources(GAME);
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "n5_floatdisk-"));
  const models = new Map();
  try {
    for (const stage of STAGES) for (const p of comp.stages[stage]) if (!models.has(p.model)) models.set(p.model, readModel(p.model, res, tmp));
  } finally {
    fs.rmSync(tmp, { recursive: true, force: true });
  }
  const disk = diskPlanes(comp.disk);
  const results = [];
  for (const stage of STAGES) {
    const parts = comp.stages[stage].map((p, i) => Object.assign(placePart(p, models.get(p.model), i, comp.disk, "j"), { part: p }));
    const planes = [].concat(...parts.map((x) => x.planes), disk);  // the disk last: see-through, drawn over the junk
    const seen = new Set();  // each mesh's name its own (a model can name two meshes alike)
    for (const p of planes) {
      let n = p.name;
      for (let k = 2; seen.has(n.toLowerCase()); k++) n = p.name.slice(0, 28) + "_" + k;
      p.name = n;
      seen.add(n.toLowerCase());
    }
    const name = "n5_floatdisk_" + stage;
    const fx = core.effect({ name, duration: true, seconds: 2, attach: "root", planes });
    const text = MARK + "\n" + core.writeDraft(fx);
    core.readDraft(text);  // it reads back
    results.push({ stage, name, parts, text, planes });
  }
  // The summary: per stage its parts, triangles, how high the pile goes and how far it reaches.
  const r = comp.disk.diameter / 2;
  for (const { stage, name, parts, planes } of results) {
    const tris = planes.reduce((n, p) => n + p.faces.length / 3, 0);
    const top = parts.reduce((m, x) => Math.max(m, x.hi[2]), comp.disk.height);
    console.log(`${name}: ${parts.length} part${parts.length === 1 ? "" : "s"}, ${planes.length} meshes, ${tris} triangles; ` +
      `the pile ${(top - comp.disk.height).toFixed(2)} m tall, its top ${top.toFixed(2)} m above the ground`);
    for (const x of parts) {
      const over = x.reach - r;
      console.log(`  ${x.part.model.padEnd(16)} ${(x.hi[0] - x.lo[0]).toFixed(2)} x ${(x.hi[1] - x.lo[1]).toFixed(2)} x ${(x.hi[2] - x.lo[2]).toFixed(2)} m, ` +
        `${(x.lo[2] - comp.disk.height).toFixed(2)}..${(x.hi[2] - comp.disk.height).toFixed(2)} above the disk` +
        (over > 0.005 ? `, reaches ${(over * 100).toFixed(0)} cm past the rim` : ""));
    }
  }
  if (CHECK) { console.log("--check: nothing written"); return; }
  // The drafts (CRLF, as the bench's server writes them) and the texture: every one checked before any is written.
  const writes = results.map(({ name, text }) => ({ file: path.join(OUT, name + ".vfx"),
    bytes: Buffer.from(text.replace(/\n/g, "\r\n"), "latin1"), isOurs: draftIsOurs }));
  if (comp.disk.texture) writes.push({ file: path.join(TEXTURES, comp.disk.texture + ".tga"), bytes: diskTexture(comp.disk), isOurs: textureIsOurs });
  for (const w of writes) {
    if (fs.existsSync(w.file) && !w.isOurs(fs.readFileSync(w.file))) {
      throw new Error(`${w.file} is there and wasn't made by this generator: nothing written - move it away or rename the stage`);
    }
  }
  for (const w of writes) console.log(writeOwn(w.file, w.bytes, w.isOurs).padEnd(9), w.file);
}

try {
  main();
} catch (e) {
  console.error("floatdisk: " + e.message);
  process.exit(1);
}
