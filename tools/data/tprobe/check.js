#!/usr/bin/env node
// The tally-marker probe's generator check (#171, TP): node tools\data\tprobe\check.js
//
// Builds the probe twice in memory (build.js's build()) and checks, without writing anything:
// - the files: the tally textures n5_tally_03 / _07, the four probes n5_tprobe_s3 / c3 / s7 / c7 (each a model, its
//   material and its TXI) and the two vertex shaders vsn5bill_s / vsn5bill_c; names under 17 letters; the same bytes
//   on both builds;
// - the art: 256 x 128, 32-bit, bottom-up with 8 alpha bits, alpha 255 everywhere (additive: the colour is the light);
//   a black border all round (filtering and the repeat bleed nothing); every pixel inside a stroke the white-gold core,
//   every pixel of the glow the picked glow's red (VFX_DUR_GLOW_LIGHT_RED's colour, progfx.2da row 408, 1.0 0.4 0.4) at
//   most GLOW_PEAK bright, black past its reach; the layout: as many upright bars as the count less its fives, a
//   diagonal strike over each five's four bars, every count 1-11 inside the picture's margin (the full set later);
// - each TXI: `blending additive` only - no `decal` (a decal mesh is drawn unlit by the game's own pair, its material
//   dropped: SHP-8 / MV);
// - each material: our vertex shader (s or c), the game's lit fragment shader fslit, texture0 its tally - nothing else;
// - each model: nwn5e's mark, one trimesh whose bitmap is the model's name (the material's and the TXI's), its node
//   0.55 m over the head node, the quad 0.5 x 0.25 m in the node's x-z plane centred on its origin, u right and v up,
//   counter-clockwise seen from -y; unlit by the material (ambient, diffuse, specular 0, selfillum 1); impact and
//   duration only (no cessation, never one of length 0), no keys, no emitter;
// - each shader: nwn5e's mark, the defines the game's lit vertex shader takes, its include, the standard setup first,
//   then the billboard lines exactly (the spherical and the cylindrical differ in the one `up` line); plain ASCII;
// - the billboard's maths (build.js billboard, the shader's lines in JS) over cameras all round and from above: the
//   spherical quad lies in the view plane (faces the camera fully, also from above), the cylindrical one stands upright
//   on the world's up and turns about it only, facing the camera; both centred on the node's origin wherever the head
//   is, at the model's scale, counter-clockwise on screen (the front face), never a NaN;
// - data\rows\visualeffects.tprobe.txt: rows 2211-2214 naming the four probes, type D, on the head, OrientWithObject 0;
// - build.js loads nothing but Node's fs and path (our own art: no font, canvas or image);
// - data\files holds exactly these bytes (text whatever its line ends: git's autocrlf checks them out CRLF).
// Exit 0: all pass; 1: a failure (each printed).

"use strict";
const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "..", "..", "..");
const FILES = path.join(ROOT, "data", "files");
const ROWS = path.join(ROOT, "data", "rows", "visualeffects.tprobe.txt");
let failures = 0;
const fail = (what) => {
  ++failures;
  console.log("FAIL " + what);
};
const ok = (what) => console.log("ok   " + what);

let B;
try {
  B = require("./build.js");
} catch (e) {
  fail("build.js loads: " + e.message);
  console.log("\n" + failures + " failed");
  process.exit(1);
}

const PROBES = [
  { row: 2211, model: "n5_tprobe_s3", mode: "s", count: 3, tally: "n5_tally_03" },
  { row: 2212, model: "n5_tprobe_c3", mode: "c", count: 3, tally: "n5_tally_03" },
  { row: 2213, model: "n5_tprobe_s7", mode: "s", count: 7, tally: "n5_tally_07" },
  { row: 2214, model: "n5_tprobe_c7", mode: "c", count: 7, tally: "n5_tally_07" },
];
const SHADERS = { s: "vsn5bill_s", c: "vsn5bill_c" };
const W = 256, H = 128;
const CORE = [255, 242, 192];  // #fff2c0
const GLOW = [255, 102, 102];  // progfx.2da 408 AlphaLightRed: 1.0 0.4 0.4

// --- the files, and the same bytes twice --------------------------------------------------------------------------
const first = B.build();
const second = B.build();
const expected = ["n5_tally_03.tga", "n5_tally_07.tga", "vsn5bill_s.shd", "vsn5bill_c.shd"];
for (const p of PROBES) for (const ext of [".mdl", ".mtr", ".txi"]) expected.push(p.model + ext);
const got = [...first.keys()].sort();
if (JSON.stringify(got) !== JSON.stringify(expected.slice().sort())) fail("the files: " + got.join(" "));
else ok(expected.length + " files: n5_tally_03 / _07, n5_tprobe_s3 / c3 / s7 / c7 (.mdl .mtr .txi), vsn5bill_s / _c");
const longNames = got.filter((n) => n.replace(/\.[^.]+$/, "").length > 16);
if (longNames.length) fail("names over 16 letters: " + longNames.join(" "));
let same = first.size === second.size;
for (const [name, bytes] of first) same = same && second.has(name) && Buffer.compare(bytes, second.get(name)) === 0;
if (same) ok("deterministic: two builds give the same bytes");
else fail("two builds differ");

// --- the art ---------------------------------------------------------------------------------------------------------
function readTga(bytes) {
  const head = { idLength: bytes[0], colorMapType: bytes[1], imageType: bytes[2], width: bytes.readUInt16LE(12),
    height: bytes.readUInt16LE(14), bpp: bytes[16], descriptor: bytes[17] };
  const px = [];  // rows from the bottom (v up), each [r, g, b, a]
  if (head.imageType === 2 && head.bpp === 32) {
    for (let y = 0; y < head.height; ++y) {
      const fileRow = head.descriptor & 0x20 ? head.height - 1 - y : y;
      const row = [];
      for (let x = 0; x < head.width; ++x) {
        const o = 18 + head.idLength + (fileRow * head.width + x) * 4;
        row.push([bytes[o + 2], bytes[o + 1], bytes[o], bytes[o + 3]]);
      }
      px.push(row);
    }
  }
  return { head, px, size: bytes.length };
}

// The layout's own statement: what a count must draw. Bars upright, strikes across a five's four bars.
function checkLayout(n) {
  const L = B.layout(n);
  const what = [];
  const fives = Math.floor(n / 5);
  if (!L || !Array.isArray(L.bars) || !Array.isArray(L.strikes)) return ["no layout"];
  if (L.bars.length !== n - fives) what.push(L.bars.length + " bars, not " + (n - fives));
  if (L.strikes.length !== fives) what.push(L.strikes.length + " strikes, not " + fives);
  if (what.length) return what;
  for (const b of L.bars) if (b.x0 !== b.x1 || !(b.y1 > b.y0)) what.push("a bar not upright");
  const xs = L.bars.map((b) => b.x0);
  for (let i = 1; i < xs.length; ++i) if (!(xs[i] > xs[i - 1])) what.push("bars not left to right");
  L.strikes.forEach((s, k) => {
    const gate = xs.slice(k * 4, k * 4 + 4);
    if (!(s.x0 < gate[0] && s.x1 > gate[3])) what.push("strike " + k + " doesn't cross its four bars");
    if (!(s.y1 > s.y0)) what.push("strike " + k + " doesn't rise left to right");
    const bar = L.bars[k * 4];
    if (!(s.y0 > bar.y0 && s.y1 < bar.y1)) what.push("strike " + k + " leaves its bars' height");
  });
  // Bars outside a five stand apart from a five's strike.
  if (fives && L.bars.length > fives * 4) {
    const after = L.bars[fives * 4].x0, strikeEnd = L.strikes[fives - 1].x1;
    if (!(after - strikeEnd > 2 * B.HALF)) what.push("the bar after a five touches its strike");
  }
  const reach = B.HALF + B.GLOW_REACH;
  const lo = Math.min(...L.bars.map((b) => b.x0), ...L.strikes.map((s) => s.x0)) - reach;
  const hi = Math.max(...L.bars.map((b) => b.x0), ...L.strikes.map((s) => s.x1)) + reach;
  const bottom = Math.min(...L.bars.map((b) => b.y0)) - reach, top = Math.max(...L.bars.map((b) => b.y1)) + reach;
  if (lo < B.MARGIN || hi > W - B.MARGIN || bottom < B.MARGIN || top > H - B.MARGIN) {
    what.push(`its glow leaves the ${B.MARGIN} px margin (x ${lo.toFixed(1)}..${hi.toFixed(1)}, y ${bottom}..${top})`);
  }
  if (Math.abs((lo + hi) / 2 - W / 2) > 0.5) what.push("not centred");
  return what;
}
const layoutBad = [];
if (!(B.MARGIN >= 6)) layoutBad.push("a black border of only " + B.MARGIN + " px");
for (let n = 1; n <= 11; ++n) for (const w of checkLayout(n)) layoutBad.push(n + ": " + w);
if (layoutBad.length) fail("layout: " + layoutBad.join("; "));
else ok("layout: counts 1-11 - n less its fives upright bars, a rising strike over each five's four, inside the margin, centred");

const near = (a, b, tol) => Math.abs(a - b) <= tol;
for (const tally of ["n5_tally_03", "n5_tally_07"]) {
  const n = Number(tally.slice(-2));
  const bytes = first.get(tally + ".tga");
  if (!bytes) continue;
  const { head, px, size } = readTga(bytes);
  const problems = [];
  if (head.imageType !== 2 || head.colorMapType !== 0 || head.idLength !== 0) problems.push("not an uncompressed true-colour TGA");
  if (head.width !== W || head.height !== H) problems.push("size " + head.width + " x " + head.height);
  if (head.bpp !== 32 || head.descriptor !== 0x08) problems.push("bpp " + head.bpp + " descriptor " + head.descriptor);
  if (size !== 18 + W * H * 4) problems.push("length " + size);
  if (problems.length) {
    fail(tally + ".tga: " + problems.join(", "));
    continue;
  }
  const L = B.layout(n);
  const segs = [...L.bars, ...L.strikes];
  const dist = (x, y) => Math.min(...segs.map((s) => {
    const dx = s.x1 - s.x0, dy = s.y1 - s.y0, l2 = dx * dx + dy * dy;
    let t = l2 > 0 ? ((x - s.x0) * dx + (y - s.y0) * dy) / l2 : 0;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return Math.hypot(s.x0 + t * dx - x, s.y0 + t * dy - y);
  }));
  let notOpaque = 0, borderLit = 0, coreWrong = 0, core = 0, glowWrong = 0, glow = 0, beyondLit = 0, tooBright = 0;
  for (let y = 0; y < H; ++y) {
    for (let x = 0; x < W; ++x) {
      const p = px[y][x];
      if (p[3] !== 255) ++notOpaque;
      const lit = p[0] || p[1] || p[2];
      if ((x < B.MARGIN || y < B.MARGIN || x >= W - B.MARGIN || y >= H - B.MARGIN) && lit) ++borderLit;
      const d = dist(x + 0.5, y + 0.5);
      if (d <= B.HALF - 0.75) {
        ++core;
        if (p[0] !== CORE[0] || p[1] !== CORE[1] || p[2] !== CORE[2]) ++coreWrong;
      } else if (d >= B.HALF + 0.75 && d < B.HALF + B.GLOW_REACH) {
        if (!lit) continue;
        ++glow;
        // the red glow's own hue: g and b 0.4 of r (rounding allowed), never brighter than its peak
        if (!near(p[1], p[0] * 0.4, 1) || !near(p[2], p[0] * 0.4, 1)) ++glowWrong;
        if (p[0] > Math.round(255 * B.GLOW_PEAK)) ++tooBright;
      } else if (d >= B.HALF + B.GLOW_REACH && lit) {
        ++beyondLit;
      }
    }
  }
  const what = [];
  if (notOpaque) what.push(notOpaque + " pixels not alpha 255");
  if (borderLit) what.push(borderLit + " lit pixels in the " + B.MARGIN + " px border");
  if (core < 500 * n / 3) what.push("only " + core + " core pixels");
  if (coreWrong) what.push(coreWrong + " core pixels not #fff2c0");
  if (glow < 1500 * n / 3) what.push("only " + glow + " glow pixels");
  if (glowWrong) what.push(glowWrong + " glow pixels not the picked glow's red");
  if (tooBright) what.push(tooBright + " glow pixels brighter than its peak");
  if (beyondLit) what.push(beyondLit + " lit pixels past the glow's reach");
  // every bar has its core all the way up, at its own x; every strike crosses its four bars' cores
  for (const b of L.bars) {
    for (let y = Math.ceil(b.y0); y < b.y1; y += 4) {
      const p = px[Math.floor(y)][Math.floor(b.x0)];
      if (p[0] !== CORE[0] || p[1] !== CORE[1] || p[2] !== CORE[2]) {
        what.push("a bar's core missing at " + Math.floor(b.x0) + "," + Math.floor(y));
        break;
      }
    }
  }
  for (const s of L.strikes) {
    for (let t = 0; t <= 1; t += 0.05) {
      const x = s.x0 + t * (s.x1 - s.x0), y = s.y0 + t * (s.y1 - s.y0);
      const p = px[Math.floor(y)][Math.floor(x)];
      if (p[0] !== CORE[0] || p[1] !== CORE[1] || p[2] !== CORE[2]) {
        what.push("a strike's core missing at " + Math.floor(x) + "," + Math.floor(y));
        break;
      }
    }
  }
  if (what.length) fail(tally + ".tga (" + n + "): " + what.join("; "));
  else ok(tally + ".tga (" + n + "): #fff2c0 bars" + (L.strikes.length ? ", a struck five" : "") + ", the picked glow's red round them, black border, " + core + " core / " + glow + " glow px");
}
if (Buffer.compare(first.get("n5_tally_03.tga") || Buffer.alloc(0), first.get("n5_tally_07.tga") || Buffer.alloc(0)) === 0) fail("3 and 7 the same picture");

// --- the TXIs and materials ------------------------------------------------------------------------------------------
const txt = (name) => (first.get(name) || Buffer.alloc(0)).toString("latin1");
const txiBad = PROBES.filter((p) => txt(p.model + ".txi") !== "blending additive\n");
if (txiBad.length) fail("TXI not exactly `blending additive` (no decal): " + txiBad.map((p) => p.model).join(" "));
else ok("4 TXIs: blending additive, no decal (a decal would drop the material)");
const mtrBad = PROBES.filter((p) => txt(p.model + ".mtr") !== `customshadervs ${SHADERS[p.mode]}\ncustomshaderfs fslit\ntexture0 ${p.tally}\n`);
if (mtrBad.length) fail("material not our vertex shader + fslit + its tally: " + mtrBad.map((p) => p.model).join(" "));
else ok("4 materials: vsn5bill_s / _c, fslit, texture0 n5_tally_03 / _07");

// --- the models ------------------------------------------------------------------------------------------------------
const failuresBeforeModels = failures;
for (const p of PROBES) {
  const text = txt(p.model + ".mdl");
  const lines = text.split("\n");
  const what = [];
  if (/[^\x09\x0a\x20-\x7e]/.test(text)) what.push("not plain ASCII with LF");
  if (lines[0] !== "#MAXMODEL ASCII") what.push("first line");
  if (!lines[1] || !lines[1].startsWith("# nwn5e (tools/data/tprobe/build.js)")) what.push("no nwn5e mark");
  if (!text.includes("\nnewmodel " + p.model + "\n") || !text.includes("\ndonemodel " + p.model + "\n")) what.push("newmodel / donemodel");
  if (!text.includes("\nclassification effect\n")) what.push("not an effect");
  if ((text.match(/\nnode trimesh /g) || []).length !== 1) what.push("not one trimesh");
  if (/\nnode (emitter|light) /.test(text)) what.push("an emitter or light");
  const mesh = text.slice(text.indexOf("\nnode trimesh "), text.indexOf("\nendnode", text.indexOf("\nnode trimesh ")));
  const field = (k) => {
    const m = new RegExp("\\n  " + k + " ([^\\n]*)").exec(mesh);
    return m ? m[1] : null;
  };
  if (field("bitmap") !== p.model) what.push("bitmap " + field("bitmap") + " (the material and TXI are found by it)");
  if (field("parent") !== p.model) what.push("parent");
  for (const [k, v] of [["ambient", "0 0 0"], ["diffuse", "0 0 0"], ["specular", "0 0 0"], ["selfillumcolor", "1 1 1"],
                        ["alpha", "1"], ["render", "1"], ["shadow", "0"], ["position", "0 0 0.55"],  // as the discs'
                        ["orientation", "0 0 0 0"]]) {
    if (field(k) !== v) what.push(k + " " + field(k));
  }
  const vi = mesh.indexOf("\n  verts 4\n"), ti = mesh.indexOf("\n  tverts 4\n"), fi = mesh.indexOf("\n  faces 2\n");
  if (vi < 0 || ti < 0 || fi < 0) {
    what.push("verts / tverts / faces");
  } else {
    const nums = (at, n) => mesh.slice(at).split("\n").slice(2, 2 + n).map((l) => l.trim().split(/\s+/).map(Number));
    const v = nums(vi, 4), t = nums(ti, 4), f = nums(fi, 2);
    const hw = B.QUAD_W / 2, hh = B.QUAD_H / 2;
    if (Math.abs(B.QUAD_W - 0.5) > 1e-9 || Math.abs(B.QUAD_H - 0.25) > 1e-9) what.push("quad " + B.QUAD_W + " x " + B.QUAD_H);
    if (v.some((c) => c.length !== 3 || c[1] !== 0 || Math.abs(Math.abs(c[0]) - hw) > 1e-6 || Math.abs(Math.abs(c[2]) - hh) > 1e-6)) {
      what.push("verts not the quad's corners in the x-z plane, centred on the node");
    }
    // u right with x, v up with z
    v.forEach((c, i) => {
      if (!t[i] || t[i][0] !== (c[0] > 0 ? 1 : 0) || t[i][1] !== (c[2] > 0 ? 1 : 0)) what.push("tvert " + i + " not u right, v up");
    });
    // each face counter-clockwise seen from -y (the side the shader turns to the camera): (b - a) x (c - a) along -y
    for (const face of f) {
      const [a, b, c] = face.slice(0, 3).map((i) => v[i]);
      if (!a || !b || !c) continue;
      const e1 = [b[0] - a[0], b[2] - a[2]], e2 = [c[0] - a[0], c[2] - a[2]];
      if (!(e1[0] * e2[1] - e1[1] * e2[0] > 0)) what.push("a face not counter-clockwise from the front");
      if (face.slice(4, 7).join(" ") !== face.slice(0, 3).join(" ")) what.push("a face's tverts not its verts");
    }
  }
  for (const anim of ["impact", "duration"]) if (!text.includes("\nnewanim " + anim + " " + p.model + "\n")) what.push("no " + anim);
  if (/\bnewanim\s+cessation\b/i.test(text)) what.push("has a cessation (a removed marker would linger)");
  if ((text.match(/\nnewanim /g) || []).length !== 2) what.push("not exactly impact and duration");
  if (/key\s+\d/i.test(text)) what.push("keyed (a key would wait for the clock while paused)");
  if (what.length) fail(p.model + ".mdl: " + what.join("; "));
}
if (failures === failuresBeforeModels) ok("4 models: one 0.5 x 0.25 m quad, 0.55 m over the head node, bitmap = its material, no cessation, no keys");

// --- the shaders -----------------------------------------------------------------------------------------------------
const BODY = {
  common: [
    "void main ()",
    "{",
    "    ApplyStandardShader();",
    "    vec3 centre = (m_mv * vec4(0.0, 0.0, 0.0, 1.0)).xyz;",
    "    float scale = length(m_mv[0].xyz);",
    "    vec3 worldUp = normalize(m_view[2].xyz);",
    "    vec3 right = cross(worldUp, vec3(0.0, 0.0, 1.0));",
    "    float across = length(right);",
    "    right = across > 0.0001 ? right / across : vec3(1.0, 0.0, 0.0);",
    "@up",
    "    vPosView = centre + scale * (vPos.x * right + vPos.z * up);",
    "    gl_Position = m_proj * vec4(vPosView, 1.0);",
    "    vVertexNormal = cross(right, up);",
    "#if FOG == 1",
    "    fFogFragCoord = 0.0;",
    "#endif",
    "#if KEYHOLING == 1",
    "    fFragCamOffset = 0.0;",
    "#endif",
    "}",
  ],
  s: "    vec3 up = cross(vec3(0.0, 0.0, 1.0), right);",
  c: "    vec3 up = worldUp;",
};
const DEFINES = ["#define SHADER_TYPE 1", "#define FOG 1", "#define KEYHOLING 1", "#define NORMAL_MAP 0", "#define ENVIRONMENT_MAP 0",
  "#define LIGHTING 1", "", '#include "inc_standard"', ""];
for (const mode of ["s", "c"]) {
  const name = SHADERS[mode];
  const text = txt(name + ".shd");
  const lines = text.split("\n");
  const what = [];
  if (/[^\x0a\x20-\x7e]/.test(text) || text.includes("`")) what.push("not plain ASCII with LF (or a backtick)");
  if (!lines[0].startsWith("// " + name + ".shd - nwn5e's camera-facing quad")) what.push("no mark on its first line");
  const start = lines.indexOf("#define SHADER_TYPE 1");
  if (start < 0 || lines.slice(0, start).some((l) => l && !l.startsWith("//"))) what.push("its header isn't comments");
  const want = [...DEFINES, ...BODY.common.map((l) => (l === "@up" ? BODY[mode] : l)), ""];
  const have = lines.slice(start);
  if (JSON.stringify(have) !== JSON.stringify(want)) {
    const at = want.findIndex((l, i) => have[i] !== l);
    what.push("line " + (start + at + 1) + ": " + JSON.stringify(have[at]) + ", not " + JSON.stringify(want[at]));
  }
  if (what.length) fail(name + ".shd: " + what.join("; "));
  else ok(name + ".shd: the lit vertex setup, then the " + (mode === "s" ? "spherical" : "cylindrical") + " billboard's lines");
}

// --- the billboard's maths -------------------------------------------------------------------------------------------
// Column-major 4 x 4 (GLSL's m[col][row] = a[col * 4 + row]).
const mul = (a, b) => {
  const o = new Array(16).fill(0);
  for (let c = 0; c < 4; ++c) for (let r = 0; r < 4; ++r) for (let k = 0; k < 4; ++k) o[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
  return o;
};
const apply = (m, v) => [0, 1, 2, 3].map((r) => m[r] * v[0] + m[4 + r] * v[1] + m[8 + r] * v[2] + m[12 + r] * v[3]);
const sub = (a, b) => a.map((x, i) => x - b[i]);
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const crs = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const nrm = (a) => a.map((x) => x / Math.hypot(...a));
const translate = (x, y, z) => [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1];
const scaling = (s) => [s, 0, 0, 0, 0, s, 0, 0, 0, 0, s, 0, 0, 0, 0, 1];
// A camera at `eye` looking at `at`, the world's up +z (as the game's: no roll) - the view matrix and its inverse.
function lookAt(eye, at) {
  const f = nrm(sub(at, eye));
  let s = crs(f, [0, 0, 1]);
  s = Math.hypot(...s) < 1e-9 ? [1, 0, 0] : nrm(s);
  const u = crs(s, f);
  const view = [s[0], u[0], -f[0], 0, s[1], u[1], -f[1], 0, s[2], u[2], -f[2], 0, -dot(s, eye), -dot(u, eye), dot(f, eye), 1];
  return view;
}
const PROJ = (() => {  // a perspective projection, 60 degrees, 16:9
  const f = 1 / Math.tan(Math.PI / 6), a = 16 / 9, n = 0.1, fa = 100;
  return [f / a, 0, 0, 0, 0, f, 0, 0, 0, 0, (fa + n) / (n - fa), -1, 0, 0, (2 * fa * n) / (n - fa), 0];
})();
const corners = [[-B.QUAD_W / 2, 0, -B.QUAD_H / 2, 1], [B.QUAD_W / 2, 0, -B.QUAD_H / 2, 1], [B.QUAD_W / 2, 0, B.QUAD_H / 2, 1],
  [-B.QUAD_W / 2, 0, B.QUAD_H / 2, 1]];
const maths = { s: [], c: [] };
let views = 0;
for (const head of [[10, 20, 1.7], [-3.5, 7.25, 0.4], [0, 0, 0]]) {
  for (const scale of [1, 1.3]) {
    for (let yaw = 0; yaw < 360; yaw += 30) {
      for (const pitch of [5, 30, 60, 85, 90]) {
        for (const distance of [3, 12, 30]) {
          const target = [head[0], head[1], head[2] + B.CENTRE_M * scale];
          const yr = (yaw * Math.PI) / 180, pr = (pitch * Math.PI) / 180;
          const eye = [target[0] + distance * Math.cos(pr) * Math.cos(yr), target[1] + distance * Math.cos(pr) * Math.sin(yr),
            target[2] + distance * Math.sin(pr)];
          const view = pitch === 90 ? (() => {  // straight down: the camera's up its yaw (lookAt can't tell)
            const s = [-Math.sin(yr), Math.cos(yr), 0], u = [-Math.cos(yr), -Math.sin(yr), 0], f = [0, 0, -1];
            return [s[0], u[0], -f[0], 0, s[1], u[1], -f[1], 0, s[2], u[2], -f[2], 0, -dot(s, eye), -dot(u, eye), dot(f, eye), 1];
          })() : lookAt(eye, target);
          // the model: the Gob at the head (turned to the world's axes), its scale, the quad's node 0.55 m up
          const model = mul(mul(translate(...head), scaling(scale)), translate(0, 0, B.CENTRE_M));
          const mv = mul(view, model);
          const nodeView = apply(mv, [0, 0, 0, 1]).slice(0, 3);
          ++views;
          for (const mode of ["s", "c"]) {
            const out = corners.map((c) => B.billboard(mode, mv, view, c));
            const bad = maths[mode];
            if (out.some((o) => [...o.pos, ...o.normal].some((x) => !Number.isFinite(x)))) {
              bad.push(`NaN at yaw ${yaw} pitch ${pitch}`);
              continue;
            }
            const centre = [0, 1, 2].map((i) => out.reduce((sum, o) => sum + o.pos[i], 0) / 4);
            if (Math.hypot(...sub(centre, nodeView)) > 1e-9) bad.push(`centre off the node at yaw ${yaw} pitch ${pitch}`);
            const width = Math.hypot(...sub(out[1].pos, out[0].pos)), height = Math.hypot(...sub(out[3].pos, out[0].pos));
            if (Math.abs(width - B.QUAD_W * scale) > 1e-9 || (mode === "s" || pitch < 90) && Math.abs(height - B.QUAD_H * scale) > 1e-9) {
              bad.push(`size ${width.toFixed(3)} x ${height.toFixed(3)} at scale ${scale}`);
            }
            if (mode === "s") {
              // in the view plane: every corner at the centre's depth, the normal straight at the camera, upright on screen
              if (out.some((o) => Math.abs(o.pos[2] - centre[2]) > 1e-9)) bad.push(`not in the view plane at yaw ${yaw} pitch ${pitch}`);
              if (Math.hypot(...sub(out[0].normal, [0, 0, 1])) > 1e-9) bad.push(`normal not at the camera at yaw ${yaw} pitch ${pitch}`);
              if (Math.abs(out[3].pos[0] - out[0].pos[0]) > 1e-9 || !(out[3].pos[1] > out[0].pos[1])) bad.push(`not upright on screen at yaw ${yaw} pitch ${pitch}`);
            } else if (pitch < 90) {
              // upright in the world: its vertical edge along the world's up; its across edge level; facing the camera
              const inv = (p) => {  // view -> world, by the view's transpose (it's a rotation and a move)
                const q = sub(p, [view[12], view[13], view[14]]);
                return [0, 1, 2].map((i) => view[i * 4] * q[0] + view[i * 4 + 1] * q[1] + view[i * 4 + 2] * q[2]);
              };
              const w = out.map((o) => inv(o.pos));
              const vertical = sub(w[3], w[0]), across = sub(w[1], w[0]);
              if (Math.hypot(vertical[0], vertical[1]) > 1e-9 || !(vertical[2] > 0)) bad.push(`not upright in the world at yaw ${yaw} pitch ${pitch}`);
              if (Math.abs(across[2]) > 1e-9) bad.push(`its across edge not level at yaw ${yaw} pitch ${pitch}`);
              const toEye = sub(eye, inv(centre)), normalWorld = crs(nrm(across), nrm(vertical));
              if (Math.abs(normalWorld[2]) > 1e-9) bad.push(`its normal not level at yaw ${yaw} pitch ${pitch}`);
              if (!(dot(normalWorld, toEye) > 0)) bad.push(`facing away at yaw ${yaw} pitch ${pitch}`);
              if (Math.hypot(...sub(nrm(out[0].normal), nrm(crs(sub(out[1].pos, out[0].pos), sub(out[3].pos, out[0].pos))))) > 1e-9) {
                bad.push(`the shader's normal not the face's at yaw ${yaw} pitch ${pitch}`);
              }
            }
            // counter-clockwise on screen (the face the game draws), unless edge-on (cylindrical from straight above)
            if (mode === "s" || pitch < 90) {
              const s = out.map((o) => {
                const c = apply(PROJ, [...o.pos, 1]);
                return [c[0] / c[3], c[1] / c[3]];
              });
              const ccw = (a, b, c) => (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
              if (!(ccw(s[0], s[1], s[2]) > 0 && ccw(s[0], s[2], s[3]) > 0)) bad.push(`clockwise on screen at yaw ${yaw} pitch ${pitch}`);
            }
          }
        }
      }
    }
  }
}
for (const mode of ["s", "c"]) {
  const bad = maths[mode];
  const kind = mode === "s" ? "spherical" : "cylindrical";
  if (bad.length) fail(`billboard (${kind}): ${bad.length} of ${views} views - ${[...new Set(bad)].slice(0, 4).join("; ")}`);
  else ok(`billboard (${kind}): ${views} views - ${mode === "s" ? "in the view plane, facing the camera, upright on screen" : "upright on the world's up, level, facing the camera"}, centred on the node, counter-clockwise`);
}

// --- the rows --------------------------------------------------------------------------------------------------------
{
  const what = [];
  if (!fs.existsSync(ROWS)) {
    what.push("missing");
  } else {
    const lines = fs.readFileSync(ROWS, "latin1").split(/\r?\n/).filter((l) => l.trim() && !l.trim().startsWith("#"));
    if (lines[0] !== "Label Type_FD OrientWithGround Imp_HeadCon_Node OrientWithObject") what.push("header " + JSON.stringify(lines[0]));
    const rows = lines.slice(1).map((l) => l.trim().split(/\s+/));
    if (rows.length !== PROBES.length) what.push(rows.length + " rows");
    PROBES.forEach((p, i) => {
      const r = rows[i] || [];
      if (Number(r[0]) !== p.row || r[2] !== "D" || r[3] !== "0" || r[4] !== p.model || r[5] !== "0" || r.length !== 6) {
        what.push("row " + p.row + " " + JSON.stringify(r.join(" ")));
      }
    });
  }
  if (what.length) fail("data\\rows\\visualeffects.tprobe.txt: " + what.join("; "));
  else ok("data\\rows\\visualeffects.tprobe.txt: 2211-2214 name the four probes, type D, on the head, OrientWithObject 0");
}

// --- our own art: no font, canvas or image library -------------------------------------------------------------------
const source = fs.readFileSync(path.join(__dirname, "build.js"), "utf8");
const required = [...source.matchAll(/require\(\s*["']([^"']+)["']\s*\)/g)].map((m) => m[1]);
const foreign = required.filter((r) => r !== "fs" && r !== "path");
if (foreign.length) fail("build.js loads " + foreign.join(", ") + " (only fs and path: the tallies are drawn from its own shapes)");
else ok("build.js loads only fs and path: its own strokes");

// --- data\files holds exactly these bytes ----------------------------------------------------------------------------
const stale = [];
for (const [name, bytes] of first) {
  const at = path.join(FILES, name);
  if (!fs.existsSync(at)) stale.push(name + " (missing)");
  else if (!B.same(name, fs.readFileSync(at), bytes)) stale.push(name + " (differs)");
}
if (stale.length) fail("data\\files out of step with the generator (node tools\\data\\tprobe\\build.js): " + stale.join(", "));
else ok("data\\files holds the generator's " + first.size + " files");

console.log(failures ? "\n" + failures + " failed" : "\nall passed");
process.exit(failures ? 1 : 0);
