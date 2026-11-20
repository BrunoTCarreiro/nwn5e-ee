#!/usr/bin/env node
// The target-count marker's final look (#171, TF; tools\data\tally\README.md): the count as tally marks facing the camera
// from every angle - black bars on the spell's glow:
//   node tools\data\tally\build.js [--out <folder>]      (default: data\files)
//
// The owner, 2026-11-20, after the probe (TP, tools\data\tprobe: one quad our vertex shader turns to the camera every
// frame, drawn and turning while paused): the face-on (spherical) style - "camera level for max readability for every
// angle" - and "for now let's keep your generated style, but can we do it black on blue/orange/violet glow for magic
// missile, scorching ray and eldritch blast respectively?". An additive glow can't draw black (black adds nothing), so
// each marker is two quads, both turned to the camera by our vertex shader vsn5bill_sl:
// - the bars: black (#010101 - never 0 0 0, the owner: "so they don't get seen as transparent") with the strokes' alpha,
//   clear elsewhere; alpha-blended (bitmap n5_tfb_NN: its TXI `blending default`, its material `transparency 1` - drawn
//   with the blended meshes, a pixel at alpha 0.2 or under cut). One per count, the same for the three spells.
// - the glow: the spell's colour round the strokes, **black where the bars are opaque**; additive (bitmap n5_tfg_<ss>NN:
//   its TXI `blending additive`). Made 1 cm in front of the bars (its node's -y: the shader moves it that far toward the
//   eye, along the eye's line through the centre, so it covers the bars' picture exactly).
// The engine draws every blended mesh before every additive one (draw bucket 3, then 4 - the TF reading, docs/
// TOOLS-PLAN.md), so the glow always comes second; nearer the camera, the bars' depth never cuts it; black where they
// are, it leaves them black. Should the order ever turn round, the result is the same but for the antialiased rim
// (check.js composites both).
// The files (data\files): for the 11 counts and the 3 spells (ss: mm Magic Missile, sr Scorching Ray, eb Eldritch Blast) -
// - n5_tf_<ss>NN.mdl: an ASCII effect model, the two quads (0.5 x 0.25 m, in their node's x-z plane round its origin),
//   the node 0.55 m over the head node; no keys, no emitter, no cessation;
// - n5_tfb_NN.mtr / .txi and n5_tfbp_NN.tga / .txi (256 x 128, 32-bit): the bars' material, its TXI (the blend), the
//   picture and the picture's TXI;
// - n5_tfg_<ss>NN.mtr / .txi and n5_tfgp_<ss>NN.tga / .txi (256 x 128, 32-bit, alpha 255): the glow's;
// - vsn5bill_sl.shd: the probe's spherical billboard (vsn5bill_s) with the layer's nudge toward the eye.
// Every TXI says `downsamplemax 0` and `downsamplemin 0`: never shrunk by the game's texture quality (the owner).
// The layout is the probe's (layout(): bars, a five four bars struck through), drawn here from strokes - no font, no
// picture of anyone's. The bytes follow from this file alone: the same on every run (check.js checks it), so the outputs
// are committed. Run from anywhere; it writes only these 210 files, and only those whose bytes changed.

"use strict";
const fs = require("fs");
const path = require("path");

// --- the spells ------------------------------------------------------------------------------------------------------
// Their glows: clear, saturated colours, each with one channel full (the brightest a colour can add).
const STYLES = [
  { code: "mm", spell: "Magic Missile", colour: [0x3a, 0x7b, 0xff] },   // blue, #3a7bff
  { code: "sr", spell: "Scorching Ray", colour: [0xff, 0x8a, 0x1e] },   // orange, #ff8a1e
  { code: "eb", spell: "Eldritch Blast", colour: [0xa0, 0x40, 0xff] },  // violet, #a040ff
];
const COUNTS = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11];
const FIRST_ROW = 2215;  // data/rows/visualeffects.tally.txt; src/data/anim_target_count.h kTargetTallyFirstRow
const SHADER = "vsn5bill_sl";
const pad = (n) => String(n).padStart(2, "0");
const modelName = (s, n) => `n5_tf_${s.code}${pad(n)}`;
const glowBitmap = (s, n) => `n5_tfg_${s.code}${pad(n)}`;
const glowPicture = (s, n) => `n5_tfgp_${s.code}${pad(n)}`;
const barsBitmap = (n) => `n5_tfb_${pad(n)}`;
const barsPicture = (n) => `n5_tfbp_${pad(n)}`;

// --- the art ---------------------------------------------------------------------------------------------------------
// The probe's numbers (tools\data\tprobe\build.js), kept: bars 10 px wide, 68 px tall, 20 px apart; a five's strike
// rising from 6 px before its first bar to 6 px past its fourth; the glow's fall-off and reach; the clear border.
const TEX_W = 256;
const TEX_H = 128;
const BAR_RGB = [1, 1, 1];  // #010101: black to the eye, never 0 0 0 (the owner's ask)
const GLOW_PEAK = 0.9;      // the glow's brightness at a stroke's edge
const GLOW_SIGMA = 7.5;     // px: its soft fall-off, exp(-(e / sigma)^2) e px out from the edge
const GLOW_REACH = 18;      // px: and nothing past this (a smooth cut over its last 4 px)
const HALF = 5;             // px: half a stroke's width (10 px bars: 2 cm on the 0.5 m quad)
const MARGIN = 7;           // px: clear all round, so filtering and the texture's repeat bring in nothing
const PITCH = 20;           // px between bars
const GROUP_GAP = 34;       // px from a five's last bar to the next bar
const BAR_Y0 = 30;          // px: a bar's centreline, from the bottom (v up)
const BAR_Y1 = 98;
const STRIKE_OVER = 6;      // px: a strike starts and ends this far past its five's first and last bar
const STRIKE_Y0 = 43;       // px: its low (left) and high (right) end
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

// Each pixel's cover by the strokes' union (0..1, an antialiased edge) and the glow there (0..1: none under a stroke's
// opaque core, the probe's fall-off past its edge), bottom row first (v up). The same for a count's bars and its glows.
const coverCache = new Map();
function coverage(n) {
  if (coverCache.has(n)) return coverCache.get(n);
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
      const alpha = Math.round(255 * cover);
      // Where the bars are opaque the glow is black, exactly: drawn over them, it must add nothing there.
      const glow = alpha === 255 ? 0 : glowAt(Math.max(0, d - HALF)) * (1 - cover);
      row.push({ alpha, glow });
    }
    rows.push(row);
  }
  coverCache.set(n, rows);
  return rows;
}

// The bars' picture: #010101 everywhere, alpha the strokes' cover.
const barsPixels = (n) => coverage(n).map((row) => row.map((p) => [...BAR_RGB, p.alpha]));
// A spell's glow picture: its colour times the glow, alpha 255 (additive: the colour is the light).
const glowPixels = (s, n) => coverage(n).map((row) => row.map((p) => [...s.colour.map((c) => Math.round(c * p.glow)), 255]));

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
// The probe's spherical billboard (vsn5bill_s; its inputs read from the game's inc_common / inc_transform and the engine's
// GLRender::SetUniforms - tools\data\tprobe\README.md): m_mv the model-view matrix (the node's place in the world, the
// head attachment's with it, then the camera's), m_view the camera's alone, m_proj the projection, vPos the vertex in its
// node; view space the GL one (x right on screen, y up, the camera at the origin looking down -z). One line more: each
// vertex's y in its node (the quad's front is -y) moves it along the eye's line through the centre - toward the eye for
// -y - so a layer made at y -0.01 lies 1 cm nearer the camera, its picture exactly over one made at y 0.
function shaderText() {
  return [
    `// ${SHADER}.shd - nwn5e's camera-facing layered quad (nwn5e-ee #171, TF): spherical, faces the camera fully, from above too`,
    "// Made by nwn5e-ee tools/data/tally/build.js (shaderText): change it there and rebuild, not here. The target-count",
    "// marker's materials (n5_tfb_NN, n5_tfg_<ss>NN) pair this vertex shader with the game's lit fragment shader (fslit).",
    "// The game's standard vertex setup first (its own include, by name), then the quad turned to the camera on every",
    "// frame - a vertex shader needs no clock, so it turns while the game is paused:",
    "// - its centre: the mesh node's own origin, m_mv (the model-view matrix: the node's place in the world - the head",
    "//   attachment's with it - then the camera's) times (0, 0, 0, 1);",
    "// - its corners: each vertex's offset from that origin, its x across and its z up (the quad is made in its node's",
    "//   x-z plane), laid along the camera's right and up in view space, at the model's own scale;",
    "// - right: level with the world (the world's up, +z, is m_view's third column in view space) and across the screen;",
    "//   up: the screen's up, so the quad lies flat to the view;",
    "// - its layer: the vertex's y moves it along the eye's line through the centre (the node's -y toward the eye), so the",
    "//   glow, made 1 cm in front of the bars, lies that much nearer the camera, its picture exactly over theirs;",
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
    "    vec3 up = cross(vec3(0.0, 0.0, 1.0), right);",
    "    float eyeDistance = length(centre);",
    "    vec3 toEye = eyeDistance > 0.0001 ? -centre / eyeDistance : vec3(0.0, 0.0, 1.0);",
    "    vPosView = centre + scale * (vPos.x * right + vPos.z * up - vPos.y * toEye);",
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
function billboard(mv, view, vpos) {
  const len = (v) => Math.hypot(v[0], v[1], v[2]);
  const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  const centre = [mv[12], mv[13], mv[14]];
  const scale = len([mv[0], mv[1], mv[2]]);
  const w = [view[8], view[9], view[10]], wl = len(w);
  const worldUp = w.map((c) => c / wl);
  let right = cross(worldUp, [0, 0, 1]);
  const across = len(right);
  right = across > 0.0001 ? right.map((c) => c / across) : [1, 0, 0];
  const up = cross([0, 0, 1], right);
  const eyeDistance = len(centre);
  const toEye = eyeDistance > 0.0001 ? centre.map((c) => -c / eyeDistance) : [0, 0, 1];
  const pos = [0, 1, 2].map((i) => centre[i] + scale * (vpos[0] * right[i] + vpos[2] * up[i] - vpos[1] * toEye[i]));
  return { pos, normal: cross(right, up) };
}

// --- the models ------------------------------------------------------------------------------------------------------
const QUAD_W = 0.5;      // metres across
const QUAD_H = 0.25;     // and up: the pictures' 2:1
const CENTRE_M = 0.55;   // the quads' centre over the head node, as the probe's and the four discs'
const GLOW_FRONT = 0.01; // metres: the glow in front of the bars (its node's -y), nearer the camera

const num = (v) => {
  const s = String(Math.round(v * 10000) / 10000);
  return s === "-0" ? "0" : s;
};

function quad(node, parent, bitmap, y) {
  const hw = QUAD_W / 2, hh = QUAD_H / 2;
  const corners = [[-1, -1], [1, -1], [1, 1], [-1, 1]];  // (across, up): bottom left, bottom right, top right, top left
  return [
    "node trimesh " + node,
    "  parent " + parent,
    "  ambient 0 0 0",
    "  diffuse 0 0 0",
    "  specular 0 0 0",
    "  shininess 1",
    "  bitmap " + bitmap,
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
    ...corners.map(([a, u]) => "    " + [a * hw, y, u * hh].map(num).join(" ")),
    "  tverts 4",
    ...corners.map(([a, u]) => "    " + num((a + 1) / 2) + " " + num((u + 1) / 2) + " 0"),
    "  faces 2",
    "    0 1 2 1 0 1 2 0",
    "    0 2 3 1 0 2 3 0",
    "endnode",
  ];
}

function model(s, n) {
  const name = modelName(s, n);
  const lines = [
    "#MAXMODEL ASCII",
    `# nwn5e (tools/data/tally/build.js): #171's target-count marker (TF) - ${n} as black tally marks on ${s.spell}'s`,
    `# glow, two quads its vertex shader ${SHADER} turns to the camera: the bars (${barsBitmap(n)}, alpha-blended) and the`,
    `# glow 1 cm in front (${glowBitmap(s, n)}, additive, black where the bars are). nwn5e's own art. No keys.`,
    "newmodel " + name,
    "setsupermodel " + name + " NULL",
    "classification effect",
    "setanimationscale 1",
    "beginmodelgeom " + name,
    "node dummy " + name,
    "  parent NULL",
    "endnode",
    ...quad(name + "_bars", name, barsBitmap(n), 0),
    ...quad(name + "_glow", name, glowBitmap(s, n), -GLOW_FRONT),
    "endmodelgeom " + name,
  ];
  // Only impact and duration, never a cessation and never one of length 0 (#171, TC-1): the client re-attaches a model
  // every frame while its cessation runs (never, paused) and keeps it 3 s after; with none a removed marker is detached
  // and deleted the next frame.
  for (const [anim, length] of [["impact", 0.25], ["duration", 1]]) {
    lines.push("", "newanim " + anim + " " + name, "  length " + num(length), "  transtime 0.25", "  animroot " + name,
               "node dummy " + name, "  parent NULL", "endnode", "doneanim " + anim + " " + name);
  }
  lines.push("", "donemodel " + name, "");
  return Buffer.from(lines.join("\n"), "latin1");
}

// The materials, found by the meshes' bitmap names: our vertex shader, the game's lit fragment shader (ambient, diffuse,
// specular 0 and selfillum 1: the picture exactly, day and night), texture0 the layer's picture. The bars' also
// `transparency 1`: never opaque (Material::GetIsOpaque), so drawn with the blended meshes - a pixel at alpha 0.2 or
// under cut, the rest blended - not as an opaque box whatever the picture's alpha is taken for.
const barsMaterial = (n) => `customshadervs ${SHADER}\ncustomshaderfs fslit\ntexture0 ${barsPicture(n)}\ntransparency 1\n`;
const glowMaterial = (s, n) => `customshadervs ${SHADER}\ncustomshaderfs fslit\ntexture0 ${glowPicture(s, n)}\n`;
// Never shrunk: the game's texture-quality setting downsamples a texture unless its TXI says these (the owner,
// 2026-11-20: "add the downsample lines to my textures too for now"). On every TXI here, and each picture gets a TXI of
// its own with them alone.
const KEEP_SIZE = "downsamplemax 0\ndownsamplemin 0\n";
// The TXIs under the bitmap names - the only place the engine takes a mesh's blend from (MV): the glow's additive; the
// bars' says so plainly (the engine reads only additive and punchthrough: anything else keeps its alpha blend). No
// decal on either: a decal mesh is drawn by the game's own unlit pair and its material dropped (SHP-8).
const GLOW_TXI = "blending additive\n" + KEEP_SIZE;
const BARS_TXI = "blending default\n" + KEEP_SIZE;
// The pictures' own TXIs (texture0's, read by the texture itself - its size, filter, clamp; never the blend): the
// downsample lines alone.
const PICTURE_TXI = KEEP_SIZE;

// Every file, by name: the bytes this file makes.
function build() {
  const files = new Map();
  for (const n of COUNTS) {
    files.set(barsPicture(n) + ".tga", tga(barsPixels(n)));
    files.set(barsPicture(n) + ".txi", Buffer.from(PICTURE_TXI, "latin1"));
    files.set(barsBitmap(n) + ".mtr", Buffer.from(barsMaterial(n), "latin1"));
    files.set(barsBitmap(n) + ".txi", Buffer.from(BARS_TXI, "latin1"));
  }
  for (const s of STYLES) {
    for (const n of COUNTS) {
      files.set(modelName(s, n) + ".mdl", model(s, n));
      files.set(glowPicture(s, n) + ".tga", tga(glowPixels(s, n)));
      files.set(glowPicture(s, n) + ".txi", Buffer.from(PICTURE_TXI, "latin1"));
      files.set(glowBitmap(s, n) + ".mtr", Buffer.from(glowMaterial(s, n), "latin1"));
      files.set(glowBitmap(s, n) + ".txi", Buffer.from(GLOW_TXI, "latin1"));
    }
  }
  files.set(SHADER + ".shd", Buffer.from(shaderText(), "latin1"));
  return files;
}

// Whether a file on disk already holds `bytes`: a text file whatever its line ends - git's autocrlf checks the committed
// ones out with CRLF, as every model in data\files, and the game reads either - a picture exactly.
function same(name, onDisk, bytes) {
  if (/\.(mdl|txi|mtr|shd)$/i.test(name)) {
    const lf = (b) => b.toString("latin1").replace(/\r\n/g, "\n");
    return lf(onDisk) === lf(bytes);
  }
  return Buffer.compare(onDisk, bytes) === 0;
}

module.exports = { build, layout, billboard, shaderText, same, STYLES, COUNTS, FIRST_ROW, SHADER, TEX_W, TEX_H, BAR_RGB,
  GLOW_PEAK, GLOW_REACH, HALF, MARGIN, QUAD_W, QUAD_H, CENTRE_M, GLOW_FRONT };

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
    if (fs.existsSync(at) && same(name, fs.readFileSync(at), bytes)) continue;
    fs.writeFileSync(at, bytes);
    console.log("written   " + name);
    ++written;
  }
  console.log(written + " written, " + (files.size - written) + " unchanged, in " + out);
}
