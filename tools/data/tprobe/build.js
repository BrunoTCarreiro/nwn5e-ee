#!/usr/bin/env node
// The target-count marker's probe: glowing tally marks on one quad that our own vertex shader turns to face the camera
// every frame (#171, TP; tools\data\tprobe\README.md):
//   node tools\data\tprobe\build.js [--out <folder>]      (default: data\files)
//
// The owner, 2026-11-19, on the four-disc marker: "glowing tally marks that always face the camera, is that possible?"
// - then "go with the probe, white-gold on red". A particle faces the camera but isn't born while the game is paused
// (the TC diagnosis); a mesh is drawn at once, and a vertex shader needs no clock, so it turns while paused too. Two
// ways to face, for the owner to compare (rows 2211-2214, data\rows\visualeffects.tprobe.txt):
// - n5_tally_03.tga, n5_tally_07.tga - 256 x 128, 32-bit, bottom-up: white-gold bars (#fff2c0) in the picked glow's
//   red (VFX_DUR_GLOW_LIGHT_RED: progfx.2da row 408 AlphaLightRed, 1.0 0.4 0.4) on black - additive, so black adds
//   nothing; a five is four bars struck through diagonally. Drawn here from strokes (no font, no picture of anyone's);
// - n5_tprobe_s3 / _c3 (|||) and n5_tprobe_s7 / _c7 (a struck five and ||), each three files:
//   .mdl - an ASCII effect model: one 0.5 x 0.25 m quad, its node 0.55 m over the head node, made in the node's x-z
//          plane round its origin (the shader takes each corner's offset from there); no keys, no cessation;
//   .mtr - its material, found by the mesh's bitmap name: our vertex shader, the game's lit fragment shader (fslit),
//          texture0 its tally;
//   .txi - `blending additive` under the same name: the game takes a mesh's blend only from its bitmap's TXI (MV);
//          no `decal` - a decal mesh is drawn by the game's own unlit pair and its material dropped (SHP-8);
// - vsn5bill_s.shd (spherical: faces the camera fully, from above too) and vsn5bill_c.shd (cylindrical: stays upright,
//   turns about the world's up only) - the game's standard vertex setup by its include, then the quad laid along the
//   camera (shaderText, the maths in billboard()).
// The bytes follow from this file alone: the same on every run (check.js checks it), so the outputs are committed.
// Run from anywhere; it writes only these 16 files, and only those whose bytes changed.

"use strict";
const fs = require("fs");
const path = require("path");

// --- the probes ------------------------------------------------------------------------------------------------------
const PROBES = [
  { row: 2211, model: "n5_tprobe_s3", mode: "s", count: 3 },
  { row: 2212, model: "n5_tprobe_c3", mode: "c", count: 3 },
  { row: 2213, model: "n5_tprobe_s7", mode: "s", count: 7 },
  { row: 2214, model: "n5_tprobe_c7", mode: "c", count: 7 },
];
const SHADER = { s: "vsn5bill_s", c: "vsn5bill_c" };
const pad = (n) => String(n).padStart(2, "0");
const tallyName = (n) => "n5_tally_" + pad(n);

// --- the art ---------------------------------------------------------------------------------------------------------
const TEX_W = 256;
const TEX_H = 128;
const CORE = [255, 242, 192];  // white-gold, #fff2c0
// The picked glow's colour: VFX_DUR_GLOW_LIGHT_RED (visualeffects.2da row 412) is ProgFX_Duration 408, progfx.2da's
// AlphaLightRed: Param2-4 = 1.0 0.4 0.4.
const GLOW_RGB = [1.0, 0.4, 0.4];
const GLOW = GLOW_RGB.map((c) => Math.round(255 * c));
const GLOW_PEAK = 0.9;   // the glow's brightness at a stroke's edge
const GLOW_SIGMA = 7.5;  // px: its soft fall-off, exp(-(e / sigma)^2) e px out from the edge
const GLOW_REACH = 18;   // px: and nothing past this (a smooth cut over its last 4 px)
const HALF = 5;          // px: half a stroke's width (10 px bars: 2 cm on the 0.5 m quad)
const MARGIN = 7;        // px: black all round, so filtering and the texture's repeat bring in nothing
const PITCH = 20;        // px between bars
const GROUP_GAP = 34;    // px from a five's last bar to the next bar
const BAR_Y0 = 30;       // px: a bar's centreline, from the bottom (v up)
const BAR_Y1 = 98;
const STRIKE_OVER = 6;   // px: a strike starts and ends this far past its five's first and last bar
const STRIKE_Y0 = 43;    // px: its low (left) and high (right) end
const STRIKE_Y1 = 85;

// A count's strokes, px, y up from the bottom: upright bars, and a rising strike over each five's four bars - centred.
function layout(n) {
  const bars = [], strikes = [];
  let x = 0;
  for (let left = n; left > 0;) {
    if (left >= 5) {
      const xs = [x, x + PITCH, x + 2 * PITCH, x + 3 * PITCH];
      for (const bx of xs) bars.push({ x0: bx, y0: BAR_Y0, x1: bx, y1: BAR_Y1 });
      strikes.push({ x0: xs[0] - STRIKE_OVER, y0: STRIKE_Y0, x1: xs[3] + STRIKE_OVER, y1: STRIKE_Y1 });
      x = xs[3] + GROUP_GAP;
      left -= 5;
    } else {
      bars.push({ x0: x, y0: BAR_Y0, x1: x, y1: BAR_Y1 });
      x += PITCH;
      left -= 1;
    }
  }
  const lo = Math.min(...bars.map((b) => b.x0), ...strikes.map((s) => s.x0));
  const hi = Math.max(...bars.map((b) => b.x1), ...strikes.map((s) => s.x1));
  const shift = TEX_W / 2 - (lo + hi) / 2;
  const move = (s) => ({ x0: s.x0 + shift, y0: s.y0, x1: s.x1 + shift, y1: s.y1 });
  return { bars: bars.map(move), strikes: strikes.map(move) };
}

// The distance from (x, y) to a stroke's centreline (a segment), px.
function distance(s, x, y) {
  const dx = s.x1 - s.x0, dy = s.y1 - s.y0, len2 = dx * dx + dy * dy;
  let t = len2 > 0 ? ((x - s.x0) * dx + (y - s.y0) * dy) / len2 : 0;
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  return Math.hypot(s.x0 + t * dx - x, s.y0 + t * dy - y);
}

const clamp01 = (v) => (v < 0 ? 0 : v > 1 ? 1 : v);
const smoothstep = (a, b, v) => {
  const t = clamp01((v - a) / (b - a));
  return t * t * (3 - 2 * t);
};
// The glow's brightness e px out from a stroke's edge.
const glowAt = (e) => GLOW_PEAK * Math.exp(-(e / GLOW_SIGMA) * (e / GLOW_SIGMA)) * (1 - smoothstep(GLOW_REACH - 4, GLOW_REACH, e));

// The texture's pixels, bottom row first (v up): [r, g, b, a] each 0-255. The strokes' union: its core white-gold (an
// antialiased edge), round it the red glow, black past it; alpha 255 everywhere - additive, the colour is the light.
function picture(n) {
  const { bars, strikes } = layout(n);
  const strokes = [...bars, ...strikes];
  const rows = [];
  for (let py = 0; py < TEX_H; ++py) {
    const row = [];
    for (let px = 0; px < TEX_W; ++px) {
      const x = px + 0.5, y = py + 0.5;
      let d = Infinity;
      for (const s of strokes) d = Math.min(d, distance(s, x, y));
      const cover = clamp01(HALF - d + 0.5);
      const glow = glowAt(Math.max(0, d - HALF)) * (1 - cover);
      row.push([...CORE.map((c, i) => Math.round(c * cover + GLOW[i] * glow)), 255]);
    }
    rows.push(row);
  }
  return rows;
}

// 32-bit uncompressed, bottom-up (descriptor 0x08: 8 alpha bits, origin bottom left), as the game's own TGAs.
function tga(rows) {
  const head = Buffer.alloc(18);
  head[2] = 2;
  head.writeUInt16LE(TEX_W, 12);
  head.writeUInt16LE(TEX_H, 14);
  head[16] = 32;
  head[17] = 0x08;
  const body = Buffer.alloc(TEX_W * TEX_H * 4);
  let o = 0;
  for (let y = 0; y < TEX_H; ++y) {
    for (const [r, g, b, a] of rows[y]) {
      body[o++] = b;
      body[o++] = g;
      body[o++] = r;
      body[o++] = a;
    }
  }
  return Buffer.concat([head, body]);
}

// --- the shader ------------------------------------------------------------------------------------------------------
// Its inputs, from the game's own shaders (inc_common, inc_transform; read in memory, values only) and the engine's
// GLRender::SetUniforms (nwmain-linux): m_mv - the model-view matrix, view x model, where the model matrix is the Gob's
// transform (its attachment, following the head node, upright on the world's axes) times each part's own position and
// turn, pushed as the engine draws it (Gob::PartDraw); m_view - the camera's view matrix alone; m_proj - the
// projection; vPos - the vertex's position in its node. inc_transform's TransformMainCoordinates (in ApplyStandardShader)
// writes vPosView = m_mv vPos and gl_Position = m_proj vPosView; the lines here write them again for the turned quad.
// View space is the GL one: x right on screen, y up, the camera looking down -z.
function shaderText(mode) {
  const name = SHADER[mode];
  const kind = mode === "s" ? "spherical: faces the camera fully, from above too" : "cylindrical: stays upright, turns about the world's up";
  return [
    `// ${name}.shd - nwn5e's camera-facing quad (nwn5e-ee #171, TP), ${kind}`,
    "// Made by nwn5e-ee tools/data/tprobe/build.js (shaderText): change it there and rebuild, not here. A probe's material,",
    `// n5_tprobe_${mode}<count>.mtr, pairs this vertex shader with the game's lit fragment shader (fslit). The game's standard`,
    "// vertex setup first (its own include, by name), then the quad turned to the camera on every frame - a vertex shader",
    "// needs no clock, so it turns while the game is paused:",
    "// - its centre: the mesh node's own origin, m_mv (the model-view matrix: the node's place in the world - the head",
    "//   attachment's with it - then the camera's) times (0, 0, 0, 1);",
    "// - its corners: each vertex's offset from that origin, its x across and its z up (the quad is made in its node's",
    "//   x-z plane), laid along the camera's right and up in view space, at the model's own scale;",
    "// - right: level with the world (the world's up, +z, is m_view's third column in view space) and across the screen;",
    `//   up: ${mode === "s" ? "the screen's up, so the quad lies flat to the view" : "the world's up, so the quad stands upright and turns about it only"};`,
    "// - the normal toward the camera, and no fog or keyhole on it: an additive glow fogged would add the fog's colour",
    "//   as a box, and the game's own glows (drawn unlit, decal) have neither.",
    "",
    "#define SHADER_TYPE 1",
    "#define FOG 1",
    "#define KEYHOLING 1",
    "#define NORMAL_MAP 0",
    "#define ENVIRONMENT_MAP 0",
    "#define LIGHTING 1",
    "",
    '#include "inc_standard"',
    "",
    "void main ()",
    "{",
    "    ApplyStandardShader();",
    "    vec3 centre = (m_mv * vec4(0.0, 0.0, 0.0, 1.0)).xyz;",
    "    float scale = length(m_mv[0].xyz);",
    "    vec3 worldUp = normalize(m_view[2].xyz);",
    "    vec3 right = cross(worldUp, vec3(0.0, 0.0, 1.0));",
    "    float across = length(right);",
    "    right = across > 0.0001 ? right / across : vec3(1.0, 0.0, 0.0);",
    mode === "s" ? "    vec3 up = cross(vec3(0.0, 0.0, 1.0), right);" : "    vec3 up = worldUp;",
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
    "",
  ].join("\n");
}

// The shader's lines in JS, for the check: a corner `vpos` ([x, y, z, 1] in its node) under `mv` and `view` (column-major
// 4 x 4, as GLSL's m[col][row] = a[col * 4 + row]) -> { pos: vPosView, normal: vVertexNormal }.
function billboard(mode, mv, view, vpos) {
  const len = (v) => Math.hypot(v[0], v[1], v[2]);
  const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  const centre = [mv[12], mv[13], mv[14]];
  const scale = len([mv[0], mv[1], mv[2]]);
  const w = [view[8], view[9], view[10]], wl = len(w);
  const worldUp = w.map((c) => c / wl);
  let right = cross(worldUp, [0, 0, 1]);
  const across = len(right);
  right = across > 0.0001 ? right.map((c) => c / across) : [1, 0, 0];
  const up = mode === "s" ? cross([0, 0, 1], right) : worldUp;
  const pos = [0, 1, 2].map((i) => centre[i] + scale * (vpos[0] * right[i] + vpos[2] * up[i]));
  return { pos, normal: cross(right, up) };
}

// --- the model -------------------------------------------------------------------------------------------------------
const QUAD_W = 0.5;     // metres across
const QUAD_H = 0.25;    // and up: the texture's 2:1
const CENTRE_M = 0.55;  // its centre over the head node, as the four-disc marker's (0.425 m clear above it)

const num = (v) => {
  const s = String(Math.round(v * 10000) / 10000);
  return s === "-0" ? "0" : s;
};

function model(p) {
  const name = p.model;
  const hw = QUAD_W / 2, hh = QUAD_H / 2;
  const corners = [[-1, -1], [1, -1], [1, 1], [-1, 1]];  // (across, up): bottom left, bottom right, top right, top left
  const lines = [
    "#MAXMODEL ASCII",
    `# nwn5e (tools/data/tprobe/build.js): #171's probe (TP) - ${p.count} as glowing tally marks on one quad that its`,
    `# vertex shader ${SHADER[p.mode]} turns to the camera (${p.mode === "s" ? "spherical" : "cylindrical"}); nwn5e's own art ${tallyName(p.count)}.tga, additive. No keys.`,
    "newmodel " + name,
    "setsupermodel " + name + " NULL",
    "classification effect",
    "setanimationscale 1",
    "beginmodelgeom " + name,
    "node dummy " + name,
    "  parent NULL",
    "endnode",
    "node trimesh " + name + "_quad",
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
    ...corners.map(([a, u]) => "    " + [a * hw, 0, u * hh].map(num).join(" ")),
    "  tverts 4",
    ...corners.map(([a, u]) => "    " + num((a + 1) / 2) + " " + num((u + 1) / 2) + " 0"),
    "  faces 2",
    "    0 1 2 1 0 1 2 0",
    "    0 2 3 1 0 2 3 0",
    "endnode",
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

const materialText = (p) => `customshadervs ${SHADER[p.mode]}\ncustomshaderfs fslit\ntexture0 ${tallyName(p.count)}\n`;
const TXI = "blending additive\n";

// Every file, by name: the bytes this file makes.
function build() {
  const files = new Map();
  for (const n of [...new Set(PROBES.map((p) => p.count))]) files.set(tallyName(n) + ".tga", tga(picture(n)));
  for (const p of PROBES) {
    files.set(p.model + ".mdl", model(p));
    files.set(p.model + ".mtr", Buffer.from(materialText(p), "latin1"));
    files.set(p.model + ".txi", Buffer.from(TXI, "latin1"));
  }
  for (const mode of ["s", "c"]) files.set(SHADER[mode] + ".shd", Buffer.from(shaderText(mode), "latin1"));
  return files;
}

// Whether a file on disk already holds `bytes`: a text file whatever its line ends - git's autocrlf checks the committed
// ones out with CRLF, as every model in data\files, and the game reads either - a texture exactly.
function same(name, onDisk, bytes) {
  if (/\.(mdl|txi|mtr|shd)$/i.test(name)) {
    const lf = (b) => b.toString("latin1").replace(/\r\n/g, "\n");
    return lf(onDisk) === lf(bytes);
  }
  return Buffer.compare(onDisk, bytes) === 0;
}

module.exports = { build, layout, picture, billboard, shaderText, same, PROBES, SHADER, TEX_W, TEX_H, CORE, GLOW, GLOW_PEAK,
  GLOW_REACH, HALF, MARGIN, QUAD_W, QUAD_H, CENTRE_M };

if (require.main === module) {
  const args = process.argv.slice(2);
  const i = args.indexOf("--out");
  const out = i >= 0 && args[i + 1] ? path.resolve(args[i + 1]) : path.resolve(__dirname, "..", "..", "..", "data", "files");
  if (!fs.existsSync(out)) {
    console.log("no folder " + out);
    process.exit(1);
  }
  const files = build();
  let written = 0;
  for (const [name, bytes] of files) {
    const at = path.join(out, name);
    if (fs.existsSync(at) && same(name, fs.readFileSync(at), bytes)) {
      console.log("unchanged " + name);
      continue;
    }
    fs.writeFileSync(at, bytes);
    console.log("written   " + name);
    ++written;
  }
  console.log(written + " written, " + (files.size - written) + " unchanged, in " + out);
}
