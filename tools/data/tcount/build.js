#!/usr/bin/env node
// The target-count marker's art and models (#171, TC; tools\data\tcount\README.md):
//   node tools\data\tcount\build.js [--out <folder>]      (default: data\files)
//
// 11 markers, count 1 to 11, each three files:
// - n5_tcnt_NN.tga - 128 x 128, 32-bit, bottom-up as the game's own: a dark-red disc (clear outside it) and a bold
//   white number with a dark outline, drawn here from hand-made digit shapes (lines and circular arcs, stroked): no
//   font, no picture of anyone's. 10 and 11 use narrow digits so both fit the disc;
// - n5_tcnt_NN.txi - `decal 1`: the game draws the mesh unlit (texture x 1), the same by day and night;
// - n5_tcnt_NN.mdl - an ASCII effect model: four one-sided discs facing out round the head (0.4 m, their centres 0.2 m
//   from its axis and 0.55 m over the head node), so one always faces the camera within 45 degrees, none is ever seen
//   mirrored (the game culls a back face) and, upright on the world's axes, none upside down. Geometry because the
//   window runs paused and the engine animates no particle while paused (the TC diagnosis). One inert emitter (no
//   particle, never drawn) lists the model in the VFX Bench, whose library lists models with an emitter.
// The bytes follow from this file alone: the same on every run (check.js checks it), so the outputs are committed.
// Run from anywhere; it writes only the 33 files, and only those whose bytes changed.

"use strict";
const fs = require("fs");
const path = require("path");

const COUNTS = 11;
const SIZE = 128;
const C = SIZE / 2;
const DISC_RADIUS = 61;  // px: a 122 px disc, 3 px clear to the texture's edge (no bleed when filtered)
const RIM_PX = 3;        // a darker ring at the disc's edge
const OUTLINE_PX = 4;    // the dark outline round the white strokes
const SCALE = 58;        // px per glyph unit: a digit's centrelines are 1 unit tall (69 px with the stroke)
const STROKE = 0.095;    // half the stroke's width, glyph units (11 px strokes: bold)
const GAP = 0.22;        // between two digits' boxes, glyph units
const DISC = [124, 18, 22];
const RIM = [70, 8, 12];
const OUTLINE = [18, 6, 6];
const WHITE = [255, 255, 255];

const FACE_M = 0.4;     // each disc's square, metres
const OUT_M = 0.2;      // its plane from the head's axis
const CENTRE_M = 0.55;  // its centre over the head node (0.35 m clear above it)

const pad = (n) => String(n).padStart(2, "0");
const nameOf = (n) => "n5_tcnt_" + pad(n);

// --- the digits: centrelines in a box 1 unit tall (y up), stroked round ------------------------------------------
const unit = (x, y) => {
  const l = Math.sqrt(x * x + y * y);
  return [x / l, y / l];
};
const E = [1, 0], N = [0, 1], W = [-1, 0], S = [0, -1];
const D155 = unit(-0.9063, 0.4226), D325 = unit(0.8192, -0.5736), D205 = unit(-0.9063, -0.4226);
const D210 = unit(-0.866, -0.5), D120 = unit(-0.5, 0.866);
const line = (x1, y1, x2, y2) => ({ kind: "line", a: [x1, y1], b: [x2, y2] });
// An arc of radius r round (cx, cy), counter-clockwise from direction `from` to `to`.
const arc = (cx, cy, r, from, to) => ({ kind: "arc", c: [cx, cy], r, from, to });
const ring = (cx, cy, r) => ({ kind: "ring", c: [cx, cy], r });
const at = (cx, cy, r, d) => [cx + r * d[0], cy + r * d[1]];

const GLYPHS = {
  "0": { w: 0.56, parts: [line(0, 0.28, 0, 0.72), line(0.56, 0.28, 0.56, 0.72), arc(0.28, 0.72, 0.28, E, W), arc(0.28, 0.28, 0.28, W, E)] },
  "1": { w: 0.56, parts: [line(0.32, 0, 0.32, 1), line(0.32, 1, 0.06, 0.8), line(0.08, 0, 0.56, 0)] },
  "2": {
    w: 0.56,
    parts: [arc(0.28, 0.72, 0.28, D325, D155), line(...at(0.28, 0.72, 0.28, D325), 0, 0), line(0, 0, 0.56, 0)],
  },
  "3": { w: 0.56, parts: [arc(0.28, 0.765, 0.235, S, D155), arc(0.28, 0.265, 0.265, D205, N), line(0.14, 0.53, 0.28, 0.53)] },
  "4": { w: 0.56, parts: [line(0.4, 1, 0, 0.3), line(0, 0.3, 0.56, 0.3), line(0.4, 0, 0.4, 1)] },
  "5": {
    w: 0.56,
    parts: [line(0.08, 1, 0.52, 1), line(0.08, 1, 0.08, 0.58), line(0.08, 0.58, ...at(0.27, 0.31, 0.29, D120)),
            arc(0.27, 0.31, 0.29, D210, D120)],
  },
  "6": { w: 0.56, parts: [ring(0.28, 0.28, 0.28), line(0.46, 1, 0, 0.3)] },
  "7": { w: 0.56, parts: [line(0, 1, 0.56, 1), line(0.56, 1, 0.18, 0)] },
  "8": { w: 0.56, parts: [ring(0.28, 0.765, 0.235), ring(0.28, 0.265, 0.265)] },
  "9": { w: 0.56, parts: [ring(0.28, 0.72, 0.28), line(0.1, 0, 0.56, 0.7)] },  // the 6 turned round, drawn upright only
  // Narrow ones for two digits (10, 11): both fit the disc at the single digits' height.
  "1n": { w: 0.44, parts: [line(0.22, 0, 0.22, 1), line(0.22, 1, 0, 0.82), line(0.04, 0, 0.4, 0)] },
  "0n": { w: 0.44, parts: [line(0, 0.22, 0, 0.78), line(0.44, 0.22, 0.44, 0.78), arc(0.22, 0.78, 0.22, E, W), arc(0.22, 0.22, 0.22, W, E)] },
};

const glyphsOf = (n) => (n < 10 ? [String(n)] : String(n).split("").map((d) => d + "n"));

const cross = (a, b) => a[0] * b[1] - a[1] * b[0];

// The distance from (x, y) to a part's centreline, glyph units.
function distance(part, x, y) {
  if (part.kind === "line") {
    const [ax, ay] = part.a, [bx, by] = part.b;
    const dx = bx - ax, dy = by - ay;
    const len2 = dx * dx + dy * dy;
    let t = len2 > 0 ? ((x - ax) * dx + (y - ay) * dy) / len2 : 0;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    const px = ax + t * dx - x, py = ay + t * dy - y;
    return Math.sqrt(px * px + py * py);
  }
  const v = [x - part.c[0], y - part.c[1]];
  const len = Math.sqrt(v[0] * v[0] + v[1] * v[1]);
  if (part.kind === "ring") return Math.abs(len - part.r);
  const sweep = cross(part.from, part.to);
  let inside;
  if (sweep > 1e-9) inside = cross(part.from, v) >= 0 && cross(v, part.to) >= 0;
  else if (sweep < -1e-9) inside = cross(part.from, v) >= 0 || cross(v, part.to) >= 0;
  else inside = cross(part.from, v) >= 0;  // a half circle
  if (inside) return Math.abs(len - part.r);
  const ends = [at(part.c[0], part.c[1], part.r, part.from), at(part.c[0], part.c[1], part.r, part.to)];
  return Math.min(...ends.map(([ex, ey]) => Math.sqrt((x - ex) * (x - ex) + (y - ey) * (y - ey))));
}

const clamp01 = (v) => (v < 0 ? 0 : v > 1 ? 1 : v);
const lerp = (a, b, t) => a.map((v, i) => v + (b[i] - v) * t);

// The texture's pixels, top row first: [r, g, b, a] each 0-255.
function picture(n) {
  const glyphs = glyphsOf(n).map((key) => GLYPHS[key]);
  const width = glyphs.reduce((sum, g) => sum + g.w, 0) + GAP * (glyphs.length - 1);
  const parts = [];
  let x0 = 0;
  for (const g of glyphs) {
    for (const p of g.parts) parts.push({ p, x0 });
    x0 += g.w + GAP;
  }
  const rows = [];
  for (let py = 0; py < SIZE; ++py) {
    const row = [];
    for (let px = 0; px < SIZE; ++px) {
      const cx = px + 0.5 - C, cy = C - (py + 0.5);
      const dc = Math.sqrt(cx * cx + cy * cy);
      const gx = cx / SCALE + width / 2, gy = cy / SCALE + 0.5;
      let d = Infinity;
      for (const { p, x0: ox } of parts) d = Math.min(d, distance(p, gx - ox, gy));
      const dPx = d * SCALE;
      const white = clamp01(STROKE * SCALE - dPx + 0.5);
      const outline = clamp01(STROKE * SCALE + OUTLINE_PX - dPx + 0.5);
      const cover = clamp01(DISC_RADIUS - dc + 0.5);
      const rim = clamp01(dc - (DISC_RADIUS - RIM_PX) + 0.5);
      let colour = lerp(DISC, RIM, rim);
      colour = lerp(colour, OUTLINE, outline);
      colour = lerp(colour, WHITE, white);
      row.push([...colour.map((v) => Math.round(v)), Math.round(cover * 255)]);
    }
    rows.push(row);
  }
  return rows;
}

// 32-bit uncompressed, bottom-up (descriptor 0x08: 8 alpha bits, origin bottom left), as the game's own TGAs.
function tga(rows) {
  const head = Buffer.alloc(18);
  head[2] = 2;
  head.writeUInt16LE(SIZE, 12);
  head.writeUInt16LE(SIZE, 14);
  head[16] = 32;
  head[17] = 0x08;
  const body = Buffer.alloc(SIZE * SIZE * 4);
  let o = 0;
  for (let y = SIZE - 1; y >= 0; --y) {
    for (const [r, g, b, a] of rows[y]) {
      body[o++] = b;
      body[o++] = g;
      body[o++] = r;
      body[o++] = a;
    }
  }
  return Buffer.concat([head, body]);
}

// --- the model -----------------------------------------------------------------------------------------------------
const num = (v) => {
  const s = String(Math.round(v * 10000) / 10000);
  return s === "-0" ? "0" : s;
};

// A disc facing out along (nx, ny): seen from that side, u runs to the viewer's right (z x out) and v up.
function face(name, side, nx, ny) {
  const h = FACE_M / 2;
  const rx = -ny, ry = nx;  // the viewer's right
  const cx = OUT_M * nx, cy = OUT_M * ny;
  const corners = [[-1, -1], [1, -1], [1, 1], [-1, 1]];  // (right, up): bottom left, bottom right, top right, top left
  return [
    "node trimesh " + name + "_" + side,
    "  parent " + name,
    "  ambient 0 0 0",
    "  diffuse 0 0 0",
    "  specular 0 0 0",
    "  shininess 1",
    "  bitmap " + name,
    "  render 1",
    "  shadow 0",
    "  beaming 0",
    "  inheritcolor 0",
    "  alpha 1",
    "  transparencyhint 0",
    "  selfillumcolor 1 1 1",
    "  tilefade 0",
    "  rotatetexture 0",
    "  position 0 0 " + num(CENTRE_M),
    "  orientation 0 0 0 0",
    "  verts 4",
    ...corners.map(([r, u]) => "    " + [cx + r * h * rx, cy + r * h * ry, u * h].map(num).join(" ")),
    "  tverts 4",
    ...corners.map(([r, u]) => "    " + num((r + 1) / 2) + " " + num((u + 1) / 2) + " 0"),
    "  faces 2",
    "    0 1 2 1 0 1 2 0",
    "    0 2 3 1 0 2 3 0",
    "endnode",
  ];
}

// No particle ever (birthrate 0: a Single emitter makes none; lifeExp 0: the game draws none): it only lists the model
// in the VFX Bench (its library takes models with an emitter).
function anchor(name) {
  return [
    "node emitter " + name + "_anchor",
    "  parent " + name,
    "  p2p 0", "  p2p_sel 1", "  affectedByWind 0", "  m_isTinted 0", "  bounce 0", "  random 0",
    "  inherit 1", "  inheritvel 0", "  inherit_local 0", "  splat 0", "  inherit_part 0",
    "  renderorder 0", "  spawntype 0",
    "  update Single",
    "  render Normal",
    "  blend Normal",
    "  texture " + name,
    "  xgrid 1", "  ygrid 1", "  loop 0", "  deadspace 0", "  twosidedtex 0",
    "  blastRadius 0", "  blastLength 0",
    "  position 0 0 " + num(CENTRE_M),
    "  orientation 0 0 0 0",
    "  colorStart 1 1 1", "  colorEnd 1 1 1", "  alphaStart 1", "  alphaEnd 1",
    "  sizeStart 0", "  sizeEnd 0", "  sizeStart_y 0", "  sizeEnd_y 0",
    "  frameStart 0", "  frameEnd 0",
    "  birthrate 0",
    "  lifeExp 0",
    "  mass 0", "  spread 0", "  particleRot 0", "  velocity 0", "  randvel 0", "  fps 0",
    "  xsize 0", "  ysize 0", "  bounce_co 0", "  blurlength 10",
    "  lightningDelay 0", "  lightningRadius 0", "  lightningScale 0",
    "  p2p_bezier2 0", "  p2p_bezier3 0", "  combinetime 0", "  drag 0", "  grav 0", "  threshold 0",
    "endnode",
  ];
}

function model(n) {
  const name = nameOf(n);
  const lines = [
    "#MAXMODEL ASCII",
    "# nwn5e (tools/data/tcount/build.js): the target-count marker (#171) - " + n + " on four discs facing out round the",
    "# head, upright, drawn at once (paused too); nwn5e's own art " + name + ".tga, unlit (its TXI's decal). No keys.",
    "newmodel " + name,
    "setsupermodel " + name + " NULL",
    "classification effect",
    "setanimationscale 1",
    "beginmodelgeom " + name,
    "node dummy " + name,
    "  parent NULL",
    "endnode",
    ...face(name, "n", 0, 1),
    ...face(name, "e", 1, 0),
    ...face(name, "s", 0, -1),
    ...face(name, "w", -1, 0),
    ...anchor(name),
    "endmodelgeom " + name,
  ];
  // No cessation, and never one of length 0 (#171, TC-1): the client re-attaches a model every frame while its cessation
  // runs (never, paused) and lingers 3 s after; with none a removed marker is detached and deleted the next frame.
  for (const [anim, length] of [["impact", 0.25], ["duration", 1]]) {
    lines.push("", "newanim " + anim + " " + name, "  length " + num(length), "  transtime 0.25", "  animroot " + name,
               "node dummy " + name, "  parent NULL", "endnode", "doneanim " + anim + " " + name);
  }
  lines.push("", "donemodel " + name, "");
  return Buffer.from(lines.join("\n"), "latin1");
}

// Every file, by name: the bytes this file makes.
function build() {
  const files = new Map();
  for (let n = 1; n <= COUNTS; ++n) {
    const name = nameOf(n);
    files.set(name + ".tga", tga(picture(n)));
    files.set(name + ".txi", Buffer.from("decal 1\n", "latin1"));
    files.set(name + ".mdl", model(n));
  }
  return files;
}

// Whether a file on disk already holds `bytes`: a text file (.mdl, .txi) whatever its line ends - git's autocrlf
// checks the committed ones out with CRLF, as every model in data\files, and the game reads either - a texture exactly.
function same(name, onDisk, bytes) {
  if (/\.(mdl|txi)$/i.test(name)) {
    const lf = (b) => b.toString("latin1").replace(/\r\n/g, "\n");
    return lf(onDisk) === lf(bytes);
  }
  return Buffer.compare(onDisk, bytes) === 0;
}

module.exports = { build, picture, same, nameOf, COUNTS, SIZE, DISC_RADIUS, OUTLINE_PX, FACE_M, OUT_M, CENTRE_M };

if (require.main === module) {
  const args = process.argv.slice(2);
  const i = args.indexOf("--out");
  const out = i >= 0 && args[i + 1] ? path.resolve(args[i + 1]) : path.resolve(__dirname, "..", "..", "..", "data", "files");
  if (!fs.existsSync(out)) {
    console.log("no folder " + out);
    process.exit(1);
  }
  let written = 0;
  for (const [name, bytes] of build()) {
    const at = path.join(out, name);
    if (fs.existsSync(at) && same(name, fs.readFileSync(at), bytes)) {
      console.log("unchanged " + name);
      continue;
    }
    fs.writeFileSync(at, bytes);
    console.log("written   " + name);
    ++written;
  }
  console.log(written + " written, " + (COUNTS * 3 - written) + " unchanged, in " + out);
}
