#!/usr/bin/env node
// The target-count marker's generator check (#171, TC): node tools\data\tcount\check.js
//
// Builds the 11 markers twice in memory (build.js's build()) and checks, without writing anything:
// - the files: n5_tcnt_01..11, each a .tga, a .txi and a .mdl, names under 16 letters; the same bytes on both builds;
// - each texture: 128 x 128, 32-bit, bottom-up with 8 alpha bits (as the game's own TGAs); alpha 0 outside the disc,
//   255 inside it; the dark-red disc; a white number whose every pixel has the dark outline round it (no white pixel
//   within 2 px of the disc's red); as many separate white shapes as the number has digits (10 and 11: two, inside the
//   disc); all 11 different;
// - each TXI: `decal 1` (drawn unlit - the same by day and night);
// - each model: nwn5e's mark, its texture on four outward-facing discs (one trimesh each) and an inert emitter;
// - the generator draws its own art: build.js loads nothing but Node's fs and path (no font, no canvas, no image);
// - data\files holds exactly these bytes (committed art never goes stale) - the models and TXIs whatever their line
//   ends (git's autocrlf checks them out with CRLF), the textures byte for byte.
// Exit 0: all pass; 1: a failure (each printed).

"use strict";
const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "..", "..", "..");
const FILES = path.join(ROOT, "data", "files");
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
  process.exit(1);
}

const COUNTS = 11;
const pad = (n) => String(n).padStart(2, "0");
const names = Array.from({ length: COUNTS }, (_, i) => "n5_tcnt_" + pad(i + 1));

// --- the files, and the same bytes twice --------------------------------------------------------------------------
const first = B.build();
const second = B.build();
const expected = [];
for (const n of names) for (const ext of [".tga", ".txi", ".mdl"]) expected.push(n + ext);
const got = [...first.keys()].sort();
if (JSON.stringify(got) !== JSON.stringify(expected.slice().sort())) fail("the files: " + got.join(" "));
else ok("33 files: n5_tcnt_01..11 .tga .txi .mdl");
if (names.some((n) => n.length > 16)) fail("a name over 16 letters");
let same = true;
for (const [name, bytes] of first) same = same && second.has(name) && Buffer.compare(bytes, second.get(name)) === 0;
if (same) ok("deterministic: two builds give the same bytes");
else fail("two builds differ");

// --- the textures ---------------------------------------------------------------------------------------------------
function readTga(bytes) {
  const head = {
    idLength: bytes[0], colorMapType: bytes[1], imageType: bytes[2],
    width: bytes.readUInt16LE(12), height: bytes.readUInt16LE(14), bpp: bytes[16], descriptor: bytes[17],
  };
  const px = [];  // top-down rows of [r, g, b, a]
  if (head.imageType === 2 && head.bpp === 32) {
    for (let y = 0; y < head.height; ++y) {
      const fileRow = head.descriptor & 0x20 ? y : head.height - 1 - y;
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

const SIZE = 128;
const C = SIZE / 2;
const R = B.DISC_RADIUS;
const isWhite = (p) => p[0] >= 250 && p[1] >= 250 && p[2] >= 250 && p[3] === 255;
const isRed = (p) => p[3] > 0 && p[0] >= 100 && p[1] <= 30 && p[2] <= 30;

function components(px, test) {
  const seen = px.map((row) => row.map(() => false));
  let count = 0;
  for (let y = 0; y < SIZE; ++y) {
    for (let x = 0; x < SIZE; ++x) {
      if (seen[y][x] || !test(px[y][x])) continue;
      ++count;
      const stack = [[x, y]];
      seen[y][x] = true;
      while (stack.length) {
        const [cx, cy] = stack.pop();
        for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
          const nx = cx + dx, ny = cy + dy;
          if (nx < 0 || ny < 0 || nx >= SIZE || ny >= SIZE || seen[ny][nx] || !test(px[ny][nx])) continue;
          seen[ny][nx] = true;
          stack.push([nx, ny]);
        }
      }
    }
  }
  return count;
}

for (let i = 0; i < COUNTS; ++i) {
  const n = i + 1;
  const name = names[i];
  const bytes = first.get(name + ".tga");
  if (!bytes) {
    fail(name + ".tga missing");
    continue;
  }
  const { head, px, size } = readTga(bytes);
  const problems = [];
  if (head.imageType !== 2 || head.colorMapType !== 0 || head.idLength !== 0) problems.push("not an uncompressed true-colour TGA");
  if (head.width !== SIZE || head.height !== SIZE) problems.push("size " + head.width + " x " + head.height);
  if (head.bpp !== 32 || head.descriptor !== 0x08) problems.push("bpp " + head.bpp + " descriptor " + head.descriptor);
  if (size !== 18 + SIZE * SIZE * 4) problems.push("length " + size);
  if (problems.length) {
    fail(name + ".tga: " + problems.join(", "));
    continue;
  }
  let outsideOpaque = 0, insideClear = 0, white = 0, red = 0, whiteNearRed = 0, whiteOutOfDisc = 0;
  for (let y = 0; y < SIZE; ++y) {
    for (let x = 0; x < SIZE; ++x) {
      const p = px[y][x];
      const d = Math.sqrt((x + 0.5 - C) * (x + 0.5 - C) + (y + 0.5 - C) * (y + 0.5 - C));
      if (d > R + 1 && p[3] !== 0) ++outsideOpaque;
      if (d < R - 1 && p[3] !== 255) ++insideClear;
      if (isRed(p)) ++red;
      if (!isWhite(p)) continue;
      ++white;
      if (d > R - B.OUTLINE_PX - 2) ++whiteOutOfDisc;
      for (let dy = -2; dy <= 2; ++dy) {
        for (let dx = -2; dx <= 2; ++dx) {
          const q = px[y + dy] && px[y + dy][x + dx];
          if (q && isRed(q)) ++whiteNearRed;
        }
      }
    }
  }
  const digits = String(n).length;
  const shapes = components(px, isWhite);
  const what = [];
  if (outsideOpaque) what.push(outsideOpaque + " pixels outside the disc not clear");
  if (insideClear) what.push(insideClear + " pixels inside the disc not opaque");
  if (red < 3000) what.push("only " + red + " dark-red disc pixels");
  if (white < 400) what.push("only " + white + " white pixels");
  if (whiteNearRed) what.push(whiteNearRed + " white pixels with the disc's red within 2 px (no outline)");
  if (whiteOutOfDisc) what.push(whiteOutOfDisc + " white pixels too near the disc's edge");
  if (shapes !== digits) what.push(shapes + " white shapes for " + digits + " digit(s)");
  if (what.length) fail(name + ".tga (" + n + "): " + what.join("; "));
  else ok(name + ".tga (" + n + "): disc, outlined white number, " + shapes + " shape(s), " + white + " white px");
}
const tgas = names.map((n) => first.get(n + ".tga")).filter(Boolean);
let distinct = true;
for (let a = 0; a < tgas.length; ++a) for (let b = a + 1; b < tgas.length; ++b) distinct = distinct && Buffer.compare(tgas[a], tgas[b]) !== 0;
if (distinct && tgas.length === COUNTS) ok("11 distinct textures");
else fail("the textures aren't 11 distinct ones");

// --- the TXIs -------------------------------------------------------------------------------------------------------
const txiBad = names.filter((n) => !first.get(n + ".txi") || first.get(n + ".txi").toString("latin1") !== "decal 1\n");
if (txiBad.length) fail("TXI not `decal 1`: " + txiBad.join(" "));
else ok("11 TXIs: decal 1 (unlit)");

// --- the models -----------------------------------------------------------------------------------------------------
for (const name of names) {
  const bytes = first.get(name + ".mdl");
  if (!bytes) {
    fail(name + ".mdl missing");
    continue;
  }
  const text = bytes.toString("latin1");
  const lines = text.split("\n");
  const what = [];
  if (/[^\x09\x0a\x20-\x7e]/.test(text)) what.push("not plain ASCII with LF");
  if (lines[0] !== "#MAXMODEL ASCII") what.push("first line");
  if (!lines[1] || !lines[1].startsWith("# nwn5e (tools/data/tcount/build.js)")) what.push("no nwn5e mark");
  if (!text.includes("\nnewmodel " + name + "\n") || !text.includes("\ndonemodel " + name + "\n")) what.push("newmodel / donemodel");
  const meshes = text.split("\nnode trimesh ").length - 1;
  const bitmaps = text.split("\n  bitmap " + name + "\n").length - 1;
  if (meshes !== 4 || bitmaps !== 4) what.push(meshes + " trimeshes, " + bitmaps + " naming " + name);
  if ((text.match(/\n  faces 2\n/g) || []).length !== 4) what.push("faces");
  if ((text.match(/\n  transparencyhint 0\n/g) || []).length !== 4) what.push("transparencyhint");
  const emitters = text.split("\nnode emitter ").length - 1;
  if (emitters !== 1 || !text.includes("\n  birthrate 0\n") || !text.includes("\n  lifeExp 0\n")) what.push("not one inert emitter");
  for (const anim of ["impact", "duration", "cessation"]) {
    if (!text.includes("\nnewanim " + anim + " " + name + "\n")) what.push("no " + anim);
  }
  if (/key\s+\d/i.test(text)) what.push("keyed (a key would wait for the clock while paused)");
  if (what.length) fail(name + ".mdl: " + what.join("; "));
}
if (!failures) ok("11 models: four outward discs, an inert emitter, no keys");

// --- our own art: no font, canvas or image library -------------------------------------------------------------------
const source = fs.readFileSync(path.join(__dirname, "build.js"), "utf8");
const required = [...source.matchAll(/require\(\s*["']([^"']+)["']\s*\)/g)].map((m) => m[1]);
const foreign = required.filter((r) => r !== "fs" && r !== "path");
if (foreign.length) fail("build.js loads " + foreign.join(", ") + " (only fs and path: the digits are drawn from its own shapes)");
else ok("build.js loads only fs and path: its own digit shapes");

// --- data\files holds exactly these bytes ---------------------------------------------------------------------------
const stale = [];
for (const [name, bytes] of first) {
  const at = path.join(FILES, name);
  if (!fs.existsSync(at)) stale.push(name + " (missing)");
  else if (!B.same(name, fs.readFileSync(at), bytes)) stale.push(name + " (differs)");  // text: either line end
}
if (stale.length) fail("data\\files out of step with the generator (node tools\\data\\tcount\\build.js): " + stale.join(", "));
else ok("data\\files holds the generator's 33 files byte for byte");

console.log(failures ? "\n" + failures + " failed" : "\nall passed");
process.exit(failures ? 1 : 0);
