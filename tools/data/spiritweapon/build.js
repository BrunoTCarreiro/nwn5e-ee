#!/usr/bin/env node
// #39's Spiritual Weapon, option C's look (the owner, 2026-11-22: "weapon appears, but doesn't look very "spiritual", is it
// possible to customize the look and idle animation?", then "A, make the draft look"): the force is an effect of nwn5e's
// own on an invisible marker, three VFX Bench drafts and the models the game plays -
//   node tools\data\spiritweapon\build.js [--tools <nwn5e-tools>] [--reader <mdlmesh.exe>] [--from-drafts]
//
// - n5_swp_idle (visualeffects 4180, NWN5E_SPIRIT_WEAPON; a lasting effect on a marker at ground level - plc_invisobj -
//   so the model carries the blade's height itself): a spectral blade about 0.9 m long hanging point down round 1.2 m up
//   - translucent blue-white, added light, a soft glow round it and faint motes drifting off - hovering in its loop: a bob
//   of 5 cm either way every 2.67 s, a gentle sway every 4 s and one slow turn about its long axis in 16 s, the loop's last
//   key its first (seamless); it fades in over its start (0.5 s) and out over its end (0.25 s: quick, as a strike's swing
//   takes its place).
// - n5_swp_fly (spells.2da 1279's ProjModel - the flight, the game session's row; no visualeffects row): the blade in
//   flight, point first along the model's +y (the game's own ammunition, wamar_001 and wambo_001, fly with their points at
//   +y: ProjOrientation "path"), 1.2 m over the model's root (the marker casts it from its origin at ground level to a
//   ground point), a short streak left behind it. A projectile model plays no keyed animation (the game's own vpr_* have
//   none): its emitters carry their rates in the geometry - a lasting effect's always-on ones (WriteVfxModel's rule).
// - n5_swp_hit (4182, NWN5E_SPIRIT_WEAPON_STRIKE; a one-shot at the force's spot, the location turned toward the
//   creature): a swing in front of the force (the model's +y, a creature's front): the blade sweeping 72 degrees in 0.45 s
//   (fast, slowing as it ends: past the middle of its arc - pointing at the creature - at 0.15 s, when the game session's
//   flash or sparks land), its tip 0.915 m out (an arc of about 1.15 m), edge first, leaving a bright arc at its tip and a
//   fainter fan along it, fading by 0.8 s. It carries its own blade: the idle hides while it plays (the game session's
//   0.6 s - the idle's end fading out as this one's blade comes, its start fading in as this one's goes). Its blade's fade
//   starting at 0 makes the impact start on its keys (transtime 0: no 0.25 s blend from the rest pose).
//
// The blade's mesh is made here (a diamond-section blade tapering to its point, a box crossguard, a four-sided grip and
// a small pommel - original, nothing of the game's) and so are its two textures: n5_swp_blade (its colour; its TXI
// "blending additive" - the game draws a mesh adding its light only by its texture's TXI, M1b - and "clamp 2", the Floating Disk's) and
// n5_swp_glow (a soft round glow for the particles - added light by their emitters' blend, Lighten; a particle's never
// by a TXI). The drafts go to tools\data\vfx\<name>.vfx, written by the VFX Bench's own draft writer (nwn5e-tools'
// vfx_bench\public\core.js writeDraft); the models to data\files\<name>.mdl by the game's own model writer
// (src\data\vfx_model.cpp WriteVfxModel) through mdlmesh --model, the textures beside them - the debris way
// (tools\data\debris\build.js). The same bytes on every run (check.js checks it); it writes only these files, and only
// those whose bytes changed (text compared without CRs: git checks them out CRLF).
//
// Refining (the owner's): open one in the bench's nwn5e tab, change it and Save - the draft lands in
// build\vfxedit\saved\<name>.vfx; copied over tools\data\vfx\<name>.vfx, --from-drafts builds the models from the drafts
// as they are (the recipe here no longer rewrites them).
//   --tools <folder>    nwn5e-tools (default: NWN5E_TOOLS, else beside this repo)
//   --reader <exe>      mdlmesh (default: NWN5E_MDLMESH, else build\floatdisk\mdlmesh.exe, built if needed)
"use strict";
const fs = require("fs");
const path = require("path");
const D = require("../debris/build.js");  // its tools() and modelOf()

const REPO = path.resolve(__dirname, "..", "..", "..");
const DRAFTS = path.join(REPO, "tools", "data", "vfx");
const FILES = path.join(REPO, "data", "files");

const IDLE = "n5_swp_idle", FLY = "n5_swp_fly", HIT = "n5_swp_hit";
const MODELS = [IDLE, FLY, HIT];
// (the flight's row is spells.2da's - 1279, the game session's: a projectile model, no visualeffects row)
const ROWS = { [IDLE]: 4180, [FLY]: null, [HIT]: 4182 };
const LABELS = { [IDLE]: "NWN5E_SPIRIT_WEAPON", [FLY]: null, [HIT]: "NWN5E_SPIRIT_WEAPON_STRIKE" };
const FLY_PROJECTILE_ROW = 1279;  // spells.2da nwn5e_Spiritual_Weapon_Flight: Proj 1, ProjModel n5_swp_fly, homing, path
const BLADE_TEX = "n5_swp_blade", GLOW_TEX = "n5_swp_glow";

// Write `text` to `file` unless it holds it already (compared without CRs). -> "new" | "rebuilt" | "unchanged".
function writeText(file, text) {
  const old = fs.existsSync(file) ? fs.readFileSync(file, "utf8").replace(/\r/g, "") : null;
  if (old === text) return "unchanged";
  fs.writeFileSync(file, text);
  return old === null ? "new" : "rebuilt";
}

// --- small maths ---------------------------------------------------------------------------------------------------------
const r6 = (v) => Math.round(v * 1e6) / 1e6 + 0;  // (written as the draft writes them: %g of a float - kept short)
const norm = (v) => { const l = Math.hypot(...v); return v.map((x) => x / l); };
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
// Quaternions [x, y, z, w].
const qAxis = (axis, angle) => { const a = norm(axis), s = Math.sin(angle / 2); return [a[0] * s, a[1] * s, a[2] * s, Math.cos(angle / 2)]; };
const qMul = (a, b) => [
  a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
  a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
  a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
  a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]];
// A rotation matrix's columns (where x, y, z go) as a quaternion.
function qFromBasis(x, y, z) {
  const m = [[x[0], y[0], z[0]], [x[1], y[1], z[1]], [x[2], y[2], z[2]]], tr = m[0][0] + m[1][1] + m[2][2];
  let q;
  if (tr > 0) {
    const s = Math.sqrt(tr + 1) * 2;
    q = [(m[2][1] - m[1][2]) / s, (m[0][2] - m[2][0]) / s, (m[1][0] - m[0][1]) / s, 0.25 * s];
  } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
    const s = Math.sqrt(1 + m[0][0] - m[1][1] - m[2][2]) * 2;
    q = [0.25 * s, (m[0][1] + m[1][0]) / s, (m[0][2] + m[2][0]) / s, (m[2][1] - m[1][2]) / s];
  } else if (m[1][1] > m[2][2]) {
    const s = Math.sqrt(1 + m[1][1] - m[0][0] - m[2][2]) * 2;
    q = [(m[0][1] + m[1][0]) / s, 0.25 * s, (m[1][2] + m[2][1]) / s, (m[0][2] - m[2][0]) / s];
  } else {
    const s = Math.sqrt(1 + m[2][2] - m[0][0] - m[1][1]) * 2;
    q = [(m[0][2] + m[2][0]) / s, (m[1][2] + m[2][1]) / s, 0.25 * s, (m[1][0] - m[0][1]) / s];
  }
  return norm(q);
}
// As the draft keeps a turn: axis x y z and the angle (radians), the game's axis-angle; none: 0 0 0 0.
function axisAngle(q) {
  let [x, y, z, w] = q;
  if (w < 0) { x = -x; y = -y; z = -z; w = -w; }
  const s = Math.hypot(x, y, z);
  if (s < 1e-9) return [0, 0, 0, 0];
  return [r6(x / s), r6(y / s), r6(z / s), r6(2 * Math.atan2(s, w))];
}

// --- the blade's mesh (its own frame: +y along it, the point at +y; x across its width; z its thickness) --------------
// 0.915 m: the blade 0.66 m (a diamond section, 11 cm wide and 2.4 cm thick at the guard, tapering, a point from 0.5 m),
// a crossguard 28 x 4.4 x 4 cm, a grip 18 cm (3.2 cm square), a pommel 3 cm round - centred on its middle (BLADE_MID), so the
// node it hangs from is its middle. UVs: u across (0 and 1 the edges, 0.5 the ridge), v along (the grip 0 - 0.2, the
// guard 0.2 - 0.27, the blade 0.27 - 1), as bladeTexture paints them.
const BLADE_LENGTH = 0.66, GRIP_LOW = -0.2, POMMEL_AT = -0.225, POMMEL_R = 0.03;
const BLADE_LO = POMMEL_AT - POMMEL_R, BLADE_MID = (BLADE_LENGTH + BLADE_LO) / 2;
function bladeMesh() {
  const verts = [], tverts = [], faces = [];
  const vert = (p, uv) => { verts.push(r6(p[0]), r6(p[1] - BLADE_MID), r6(p[2])); tverts.push(r6(uv[0]), r6(uv[1])); return verts.length / 3 - 1; };
  const quad = (a, b, c, d) => faces.push(a, b, c, a, c, d);
  // The blade: rings of four (edge +x, ridge +z, edge -x, ridge -z), then its point.
  const vOf = (y) => 0.27 + 0.73 * (y / BLADE_LENGTH);
  const rings = [[0, 0.055, 0.012], [0.2, 0.05, 0.01], [0.4, 0.044, 0.007], [0.5, 0.038, 0.005]];
  const ringIds = rings.map(([y, w, t]) => [vert([w, y, 0], [1, vOf(y)]), vert([0, y, t], [0.5, vOf(y)]), vert([-w, y, 0], [0, vOf(y)]), vert([0, y, -t], [0.5, vOf(y)])]);
  for (let r = 0; r + 1 < ringIds.length; r++) {
    const a = ringIds[r], b = ringIds[r + 1];
    for (let k = 0; k < 4; k++) quad(a[k], a[(k + 1) % 4], b[(k + 1) % 4], b[k]);
  }
  const tip = vert([0, BLADE_LENGTH, 0], [0.5, 1]), last = ringIds[ringIds.length - 1];
  for (let k = 0; k < 4; k++) faces.push(last[k], last[(k + 1) % 4], tip);
  const base = ringIds[0];
  faces.push(base[0], base[3], base[2], base[0], base[2], base[1]);  // (closed at the guard)
  // A box: its 8 corners and 6 sides, u across its x (0 - 1) and v in [v0, v1] along its y.
  const box = (x0, x1, y0, y1, z0, z1, v0, v1) => {
    const c = [];
    for (const y of [y0, y1]) for (const z of [z0, z1]) for (const x of [x0, x1]) c.push(vert([x, y, z], [x === x0 ? 0 : 1, y === y0 ? v0 : v1]));
    // c: index = (y ? 4 : 0) + (z ? 2 : 0) + (x ? 1 : 0)
    quad(c[0], c[1], c[3], c[2]);  // y0 end
    quad(c[4], c[6], c[7], c[5]);  // y1 end
    quad(c[0], c[4], c[5], c[1]);  // z0 side
    quad(c[2], c[3], c[7], c[6]);  // z1 side
    quad(c[0], c[2], c[6], c[4]);  // x0 side
    quad(c[1], c[5], c[7], c[3]);  // x1 side
  };
  box(-0.14, 0.14, -0.022, 0.022, -0.02, 0.02, 0.2, 0.27);    // the crossguard
  box(-0.016, 0.016, GRIP_LOW, -0.022, -0.016, 0.016, 0.05, 0.2);  // the grip
  // The pommel: an octahedron.
  const pc = [0, POMMEL_AT, 0], R = POMMEL_R;
  const p = [[R, 0, 0], [-R, 0, 0], [0, R, 0], [0, -R, 0], [0, 0, R], [0, 0, -R]].map((d, i) => vert([pc[0] + d[0], pc[1] + d[1], pc[2] + d[2]], [i % 2, 0.02]));
  for (const [a, b] of [[0, 4], [4, 1], [1, 5], [5, 0]]) faces.push(p[a], p[b], p[2], p[b], p[a], p[3]);
  return { verts, tverts, faces };
}

// --- its textures (pictures: { width, height, data: RGBA rows top-down }, as floatdisk.js tgaBytes takes them) ---------
const clamp01 = (v) => Math.max(0, Math.min(1, v));
// The blade's colour, added light: the blade pale blue-white, brightest at its edges and ridge and toward its point; the
// guard a bright band; the grip and pommel a deeper blue - all of it light (the alpha full: an added mesh adds its colour).
function bladeTexture() {
  const width = 32, height = 128, data = new Uint8ClampedArray(width * height * 4);
  for (let y = 0; y < height; y++) {
    const v = 1 - (y + 0.5) / height;  // (rows top-down: the first row is v 1, the point)
    for (let x = 0; x < width; x++) {
      const u = (x + 0.5) / width, o = (y * width + x) * 4;
      let rgb;
      if (v >= 0.27) {
        const along = (v - 0.27) / 0.73, edge = Math.max(0, 1 - Math.min(u, 1 - u) / 0.12), ridge = Math.max(0, 1 - Math.abs(u - 0.5) / 0.08);
        const shine = 0.42 + 0.12 * along + 0.5 * edge * edge + 0.3 * ridge;
        rgb = [0.55 * shine + 0.25 * edge, 0.75 * shine + 0.2 * edge, 1.0 * shine + 0.1 * edge];
      } else if (v >= 0.2) {
        const band = 1 - Math.abs((v - 0.235) / 0.035);
        rgb = [0.5 + 0.35 * band, 0.7 + 0.25 * band, 1];
      } else {
        const wrap = 0.5 + 0.5 * Math.cos(2 * Math.PI * (v / 0.025));  // a wound grip's bands
        rgb = [0.2 + 0.08 * wrap, 0.3 + 0.1 * wrap, 0.48 + 0.12 * wrap];
      }
      for (let c = 0; c < 3; c++) data[o + c] = Math.round(clamp01(rgb[c]) * 255);
      data[o + 3] = 255;
    }
  }
  return { width, height, data };
}
// A soft round glow, white at its heart going pale blue: its alpha (1 - r)^2, its light the same - for the particles.
function glowTexture() {
  const size = 64, data = new Uint8ClampedArray(size * size * 4);
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const r = Math.min(1, Math.hypot((x + 0.5) / size - 0.5, (y + 0.5) / size - 0.5) / 0.5), fall = (1 - r) * (1 - r), o = (y * size + x) * 4;
      const heart = Math.max(0, 1 - r / 0.35);
      const rgb = [0.72 + 0.28 * heart, 0.86 + 0.14 * heart, 1];
      for (let c = 0; c < 3; c++) data[o + c] = Math.round(clamp01(rgb[c] * (0.35 + 0.65 * fall)) * 255);
      data[o + 3] = Math.round(clamp01(fall) * 255);
    }
  }
  return { width: size, height: size, data };
}
// [{ name, bytes (a TGA with alpha), txi (text | null) }]
function textures() {
  const F = require("../floatdisk/floatdisk.js");
  return [
    { name: BLADE_TEX, bytes: F.tgaBytes(bladeTexture(), { alpha: true }), txi: F.softTxi({ blend: "additive", clamp: true }) },
    { name: GLOW_TEX, bytes: F.tgaBytes(glowTexture(), { alpha: true }), txi: null },
  ];
}

// --- the effects -----------------------------------------------------------------------------------------------------
const BLUE = [0.55, 0.75, 1], PALE = [0.82, 0.93, 1];
// The blade as a plane of the draft (the mesh above, its textures; lit by itself: selfillum 1, so the area's light
// doesn't dim it).
function bladePlane(core, more) {
  const m = bladeMesh();
  return core.plane(Object.assign({ name: "blade", bitmap: BLADE_TEX, verts: m.verts, tverts: m.tverts, faces: m.faces, selfillum: [1, 1, 1] }, more));
}
// A soft glow round the blade: added light, the glow texture, born along it (its area: 3 x 80 cm in its own x / y - turned
// so its y runs along the blade) and carried with it (inherit).
const halo = (core, more) => core.emitter(Object.assign({ name: "glow", update: "Fountain", render: "Normal", blend: "Lighten", texture: GLOW_TEX,
  grid: 1, frame_end: 0, birthrate: 10, life: 1.2, velocity: 0, randvel: 0, spread: 0, mass: 0, size_start: 0.35, size_end: 0.5,
  color_start: BLUE, color_end: [0.45, 0.65, 1], alpha_start: 0.35, alpha_end: 0, xsize: 3, ysize: 80, inherit: true }, more));

// The idle (4180): 16 s of hover - the bob (6 of 2.67 s), the sway (4 degrees about x every 4 s, 3 about y every 8 s:
// none at the loop's start and end) and one turn about the blade - keyed every 0.25 s.
const LOOP = 16, KEY_STEP = 0.25, HOVER_Z = 1.2, BOB = 0.05;
function hoverAt(t) {
  const z = HOVER_Z + BOB * Math.sin((2 * Math.PI * 6 * t) / LOOP);
  const sway = (2 * Math.PI * 4 * t) / LOOP;
  const tilt = qMul(qAxis([1, 0, 0], (4 * Math.PI / 180) * Math.sin(sway)), qAxis([0, 1, 0], (3 * Math.PI / 180) * Math.sin(sway / 2)));
  const spin = qAxis([0, 0, 1], (2 * Math.PI * t) / LOOP);
  return { pos: [0, 0, r6(z)], q: qMul(tilt, spin) };
}
function idle(core) {
  const hover = core.node({ name: "hover", position: [0, 0, HOVER_Z] });
  const position = [], orientation = [];
  for (let k = 0; k * KEY_STEP <= LOOP + 1e-9; k++) {
    const t = r6(k * KEY_STEP), at = hoverAt(t);
    position.push(t, ...at.pos);
    orientation.push(t, ...(k === 0 || k * KEY_STEP >= LOOP - 1e-9 ? [0, 0, 0, 0] : axisAngle(at.q)));
  }
  hover.keys = [{ anim: "duration", position, orientation }];
  // The blade hangs point down from its middle (its +y turned to -z: a quarter turn back about x).
  const blade = bladePlane(core, { parent: "hover", orientation: [-1, 0, 0, 1.570796] });
  blade.keys = [{ anim: "impact", position: [], orientation: [], alpha: [0, 0, 0.5, 1] }, { anim: "cessation", position: [], orientation: [], alpha: [0, 1, 0.25, 0] }];
  const along = [1, 0, 0, 1.570796];  // (an emitter's area turned so its y runs down the hanging blade)
  const emitters = [
    halo(core, { parent: "hover", orientation: along }),
    core.emitter({ name: "motes", parent: "hover", orientation: along, update: "Fountain", render: "Normal", blend: "Lighten", texture: GLOW_TEX,
      grid: 1, frame_end: 0, birthrate: 6, life: 1.8, velocity: 0.12, randvel: 0.08, spread: 6.283, mass: -0.02, size_start: 0.05,
      size_end: 0.015, color_start: PALE, color_end: [0.5, 0.75, 1], alpha_start: 0.9, alpha_end: 0, xsize: 4, ysize: 80 }),
  ];
  return core.effect({ name: IDLE, duration: true, seconds: 8, attach: "root", lengths: [0.5, LOOP, 0.25], nodes: [hover], emitters, planes: [blade] });
}

// The flight (spells.2da 1279's projectile): the blade along +y at FLY_Z over the model's root, a short streak left in
// the world behind it.
const FLY_Z = 1.2;
function fly(core) {
  const blade = bladePlane(core, { position: [0, 0, FLY_Z] });
  const emitters = [
    halo(core, { position: [0, 0, FLY_Z], birthrate: 12, life: 0.5, alpha_start: 0.35, xsize: 3, ysize: 75 }),
    core.emitter({ name: "streak", position: [0, 0, FLY_Z], update: "Fountain", render: "Normal", blend: "Lighten", texture: GLOW_TEX, grid: 1, frame_end: 0,
      birthrate: 60, life: 0.2, velocity: 0, randvel: 0, spread: 0, mass: 0, size_start: 0.22, size_end: 0.04, color_start: PALE,
      color_end: BLUE, alpha_start: 0.45, alpha_end: 0, xsize: 2, ysize: 70 }),
  ];
  return core.effect({ name: FLY, duration: true, seconds: 2, attach: "root", lengths: [0, 2, 1], emitters, planes: [blade] });
}

// The strike (4182): from its pivot 1.3 m up, 15 cm in front, the blade sweeps from up and right to down and left in
// front of it - 72 degrees in 0.45 s (fast, slowing as it ends: sin(pi/2 t / 0.45) of the way, so past the middle of its arc
// at 0.15 s), the edge first - and fades by 0.8 s.
const PIVOT = [0, 0.15, 1.3], SWING_FROM = norm([0.55, 0.75, 0.35]), SWING_TO = norm([-0.45, 0.85, -0.25]), SWING_TIME = 0.45;
const SLASH_AT = 0.15;  // the middle of the arc: the blade pointing at the creature (the game session's flash or sparks)
function swingAt(t) {
  const n = norm(cross(SWING_FROM, SWING_TO)), total = Math.acos(dot(SWING_FROM, SWING_TO));
  const s = Math.min(1, t / SWING_TIME), f = Math.sin((Math.PI / 2) * s);
  const lead = norm(cross(n, SWING_FROM));  // where the edge goes first
  const start = qFromBasis(lead, SWING_FROM, cross(lead, SWING_FROM));
  return qMul(qAxis(n, total * f), start);
}
function hit(core) {
  const start = axisAngle(swingAt(0));
  const swing = core.node({ name: "swing", position: PIVOT, orientation: start });
  const orientation = [];
  for (let k = 0; k <= 10; k++) {
    const t = r6((k * SWING_TIME) / 10);
    orientation.push(t, ...axisAngle(swingAt(t)));
  }
  swing.keys = [{ anim: "impact", position: [], orientation }];
  // The blade from its pommel at the pivot, out along the swing node's +y.
  const blade = bladePlane(core, { parent: "swing", position: [0, r6(BLADE_MID - BLADE_LO), 0] });
  blade.keys = [{ anim: "impact", position: [], orientation: [], alpha: [0, 0, 0.05, 1, 0.5, 1, 0.8, 0] }];
  const emitters = [
    core.emitter({ name: "edge", parent: "swing", position: [0, 0.75, 0], update: "Fountain", render: "Normal", blend: "Lighten", texture: GLOW_TEX,
      grid: 1, frame_end: 0, birthrate: 240, emit_seconds: 0.47, life: 0.2, velocity: 0, randvel: 0, spread: 0, mass: 0,
      size_start: 0.18, size_end: 0.05, color_start: [0.95, 0.98, 1], color_end: BLUE, alpha_start: 1, alpha_end: 0, xsize: 1, ysize: 30 }),
    core.emitter({ name: "fan", parent: "swing", position: [0, 0.55, 0], update: "Fountain", render: "Normal", blend: "Lighten", texture: GLOW_TEX,
      grid: 1, frame_end: 0, birthrate: 90, emit_seconds: 0.47, life: 0.3, velocity: 0, randvel: 0, spread: 0, mass: 0,
      size_start: 0.25, size_end: 0.1, color_start: BLUE, color_end: [0.4, 0.6, 1], alpha_start: 0.35, alpha_end: 0, xsize: 2, ysize: 60 }),
  ];
  return core.effect({ name: HIT, duration: false, seconds: 0.9, attach: "root", lengths: [0.9, 2, 1], nodes: [swing], emitters, planes: [blade] });
}

// [{ name, row, label, fx }] in the rows' order.
function effects(core) {
  return [idle(core), fly(core), hit(core)].map((fx) => ({ name: fx.name, row: ROWS[fx.name], label: LABELS[fx.name], fx }));
}
// The drafts' text: [{ name, row, label, fx, draft }].
function build(core) {
  return effects(core).map((e) => Object.assign(e, { draft: core.writeDraft(e.fx) }));
}

function main() {
  const argv = process.argv.slice(2);
  const { core, option } = D.tools(argv);
  const R = require("../floatdisk/reader.js");
  const reader = R.ensureReader({ repo: REPO, reader: option("reader", process.env.NWN5E_MDLMESH || "") });
  const fromDrafts = argv.includes("--from-drafts");
  for (const e of build(core)) {
    const draftFile = path.join(DRAFTS, e.name + ".vfx");
    const said = [fromDrafts ? "as it is" : writeText(draftFile, e.draft)];
    said.push(writeText(path.join(FILES, e.name + ".mdl"), D.modelOf(reader, draftFile, e.name)));
    console.log(`${e.row ? "visualeffects " + e.row : "spells.2da " + FLY_PROJECTILE_ROW + "'s projectile"} ${e.name}: draft ${said[0]}, model ${said[1]}`);
  }
  for (const t of textures()) {
    const file = path.join(FILES, t.name + ".tga"), old = fs.existsSync(file) ? fs.readFileSync(file) : null;
    if (!old || !old.equals(t.bytes)) fs.writeFileSync(file, t.bytes);
    let txi = "";
    if (t.txi !== null) txi = ", its TXI " + writeText(path.join(FILES, t.name + ".txi"), t.txi);
    console.log(`${t.name}.tga: ${!old ? "new" : old.equals(t.bytes) ? "unchanged" : "rebuilt"}${txi}`);
  }
}

module.exports = { MODELS, ROWS, LABELS, FLY_PROJECTILE_ROW, IDLE, FLY, HIT, BLADE_TEX, GLOW_TEX, LOOP, KEY_STEP, HOVER_Z, BOB, FLY_Z,
  PIVOT, SWING_FROM, SWING_TO, SWING_TIME, SLASH_AT, BLADE_LENGTH, BLADE_LO, BLADE_MID, bladeMesh, bladeTexture, glowTexture, textures, hoverAt, swingAt, axisAngle, effects, build };
if (require.main === module) {
  try {
    main();
  } catch (e) {
    console.error(e.message);
    process.exitCode = 1;
  }
}
