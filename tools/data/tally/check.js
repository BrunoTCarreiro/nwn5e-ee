#!/usr/bin/env node
// The final target-count marker's generator check (#171, TF): node tools\data\tally\check.js
//
// Builds the marker twice in memory (build.js's build()) and checks, without writing anything:
// - the files: 11 bars pictures n5_tfbp_NN, 33 glow pictures n5_tfgp_<ss>NN (ss: mm Magic Missile, sr Scorching Ray, eb
//   Eldritch Blast), each with a TXI, 33 models n5_tf_<ss>NN, the bars' bitmaps n5_tfb_NN and the glows' n5_tfg_<ss>NN
//   (each a material and a TXI), the vertex shader vsn5bill_sl; names under 17 letters; the same bytes on both builds;
// - the layout: counts 1-11 as the probe (TP, tools/data/tprobe) lays them out - n less its fives upright bars, a rising
//   strike over each five's four, inside the picture's margin, centred - stated here in its numbers (and, while the
//   probe's generator is there, the same as its);
// - the bars pictures: 256 x 128, 32-bit, bottom-up with 8 alpha bits; every pixel #010101 (the owner: never pure black,
//   "so they don't get seen as transparent" - no pixel with alpha over 0 is 0 0 0); alpha 255 inside a stroke, 0 outside
//   it and in the border, an antialiased edge between; every bar and strike there;
// - the glow pictures: the same size, alpha 255 everywhere (additive: the colour is the light); the style's colour
//   (Magic Missile #3a7bff, Scorching Ray #ff8a1e, Eldritch Blast #a040ff) at most GLOW_PEAK bright, round every stroke,
//   black past its reach and in the border - and **black wherever the bars are opaque**: the glow is drawn after the
//   bars and over them, and black adds nothing, so the bars stay black; on the bars' antialiased rim at most the peak
//   times what the bar leaves uncovered;
// - the two layers as the game composites them (the engine draws a blended mesh before an additive one: draw buckets 3
//   then 4), and the other way round, over night, grass, snow and sky: a bar's core #010101 on every one, the glow round
//   it; the two orders differ only on the antialiased rim;
// - each TXI: the glow's `blending additive`, the bars' `blending default` (alpha-blended: the engine reads only
//   additive and punchthrough) - no decal (a decal mesh is drawn by the game's own pair, its material dropped); every
//   TXI `downsamplemax 0` and `downsamplemin 0` (never shrunk by the game's texture quality: the owner), each picture
//   a TXI of its own with those alone;
// - each material: our vertex shader vsn5bill_sl, the game's lit fragment shader fslit, texture0 its picture; the bars'
//   also `transparency 1` (drawn with the blended meshes, cut at alpha 0.2 - never as an opaque box);
// - each model: nwn5e's mark, two trimeshes - the bars (bitmap n5_tfb_NN, y 0) and the glow (n5_tfg_<ss>NN, y -0.01: 1
//   cm nearer the camera) - each a 0.5 x 0.25 m quad in its node's x-z plane round its origin, 0.55 m over the head node,
//   u right and v up, counter-clockwise from -y; lit by itself (ambient, diffuse, specular 0, selfillum 1), alpha 1;
//   impact and duration only (no cessation, never one of length 0), no keys, no emitter;
// - the shader: nwn5e's mark, the defines the game's lit vertex shader takes, its include, the standard setup first, then
//   the spherical billboard's lines with the layer's nudge toward the eye, exactly; plain ASCII;
// - the billboard's maths (build.js billboard, the shader's lines in JS) over cameras all round and from above: each layer
//   in the view plane, facing the camera, upright on the screen, centred on the node, at the model's scale; the glow
//   nearer the camera than the bars at every corner; its picture the bars' scaled about the same centre on the screen
//   (the nudge is along the eye's line through the centre), by under 1% of the quad from 2 m out; counter-clockwise on
//   the screen; never a NaN;
// - data\rows\visualeffects.tally.txt: rows 2215-2247 naming the 33 models, type D, on the head, OrientWithObject 0;
// - build.js loads nothing but Node's fs and path (our own art: no font, canvas or image);
// - data\files holds exactly these bytes (text whatever its line ends: git's autocrlf checks them out CRLF).
// Exit 0: all pass; 1: a failure (each printed).

"use strict";
const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "..", "..", "..");
const FILES = path.join(ROOT, "data", "files");
const ROWS = path.join(ROOT, "data", "rows", "visualeffects.tally.txt");
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

// --- what the marker is, stated here -----------------------------------------------------------------------------------
const STYLES = [
  { code: "mm", spell: "Magic Missile", colour: [0x3a, 0x7b, 0xff] },
  { code: "sr", spell: "Scorching Ray", colour: [0xff, 0x8a, 0x1e] },
  { code: "eb", spell: "Eldritch Blast", colour: [0xa0, 0x40, 0xff] },
];
const FIRST_ROW = 2215;
const W = 256, H = 128;
const BAR_RGB = [1, 1, 1];  // #010101
const SHADER = "vsn5bill_sl";
const pad = (n) => String(n).padStart(2, "0");
const COUNTS = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11];
const model = (s, n) => `n5_tf_${s.code}${pad(n)}`;
const glowBitmap = (s, n) => `n5_tfg_${s.code}${pad(n)}`;
const glowPicture = (s, n) => `n5_tfgp_${s.code}${pad(n)}`;
const barsBitmap = (n) => `n5_tfb_${pad(n)}`;
const barsPicture = (n) => `n5_tfbp_${pad(n)}`;

// --- the files, and the same bytes twice --------------------------------------------------------------------------
const first = B.build();
const second = B.build();
const expected = [SHADER + ".shd"];
for (const n of COUNTS) expected.push(barsPicture(n) + ".tga", barsPicture(n) + ".txi", barsBitmap(n) + ".mtr", barsBitmap(n) + ".txi");
for (const s of STYLES) {
  for (const n of COUNTS) expected.push(model(s, n) + ".mdl", glowPicture(s, n) + ".tga", glowPicture(s, n) + ".txi", glowBitmap(s, n) + ".mtr", glowBitmap(s, n) + ".txi");
}
const got = [...first.keys()].sort();
if (JSON.stringify(got) !== JSON.stringify(expected.slice().sort())) {
  const extra = got.filter((n) => !expected.includes(n)), missing = expected.filter((n) => !got.includes(n));
  fail("the files: " + got.length + " (extra: " + extra.join(" ") + "; missing: " + missing.join(" ") + ")");
} else {
  ok(expected.length + " files: 33 models, 44 pictures (11 bars, 33 glows) each with its TXI, 44 materials and their TXIs, vsn5bill_sl");
}
const longNames = got.filter((n) => n.replace(/\.[^.]+$/, "").length > 16);
if (longNames.length) fail("names over 16 letters: " + longNames.join(" "));
let same = first.size === second.size;
for (const [name, bytes] of first) same = same && second.has(name) && Buffer.compare(bytes, second.get(name)) === 0;
if (same) ok("deterministic: two builds give the same bytes");
else fail("two builds differ");

// --- the layout ------------------------------------------------------------------------------------------------------
// The probe's numbers (TP): bars 10 px wide from y 30 to 98, 20 px apart, 34 px from a five's last bar to the next; a
// five's strike from 6 px before its first bar at y 43 to 6 px past its fourth at y 85; the group centred on 128.
const LAYOUT = { HALF: 5, PITCH: 20, GROUP_GAP: 34, BAR_Y0: 30, BAR_Y1: 98, STRIKE_OVER: 6, STRIKE_Y0: 43, STRIKE_Y1: 85 };
function statedLayout(n) {
  const bars = [], strikes = [];
  let x = 0;
  for (let left = n; left > 0;) {
    if (left >= 5) {
      const xs = [0, 1, 2, 3].map((k) => x + k * LAYOUT.PITCH);
      for (const bx of xs) bars.push({ x0: bx, y0: LAYOUT.BAR_Y0, x1: bx, y1: LAYOUT.BAR_Y1 });
      strikes.push({ x0: xs[0] - LAYOUT.STRIKE_OVER, y0: LAYOUT.STRIKE_Y0, x1: xs[3] + LAYOUT.STRIKE_OVER, y1: LAYOUT.STRIKE_Y1 });
      x = xs[3] + LAYOUT.GROUP_GAP;
      left -= 5;
    } else {
      bars.push({ x0: x, y0: LAYOUT.BAR_Y0, x1: x, y1: LAYOUT.BAR_Y1 });
      x += LAYOUT.PITCH;
      left -= 1;
    }
  }
  const lo = Math.min(...bars.map((b) => b.x0), ...strikes.map((s) => s.x0));
  const hi = Math.max(...bars.map((b) => b.x1), ...strikes.map((s) => s.x1));
  const shift = W / 2 - (lo + hi) / 2;
  const move = (s) => ({ x0: s.x0 + shift, y0: s.y0, x1: s.x1 + shift, y1: s.y1 });
  return { bars: bars.map(move), strikes: strikes.map(move) };
}
function checkLayout(n) {
  const L = B.layout(n);
  const what = [];
  const fives = Math.floor(n / 5);
  if (!L || !Array.isArray(L.bars) || !Array.isArray(L.strikes)) return ["no layout"];
  if (JSON.stringify(L) !== JSON.stringify(statedLayout(n))) what.push("not the probe's layout");
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
  });
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
{
  const bad = [];
  if (B.HALF !== LAYOUT.HALF) bad.push("bars " + 2 * B.HALF + " px wide, not 10");
  if (B.MARGIN !== 7) bad.push("a clear border of " + B.MARGIN + " px, not the probe's 7");
  // The probe's glow: 0.9 at a stroke's edge, nothing past 18 px.
  if (B.GLOW_PEAK !== 0.9 || B.GLOW_REACH !== 18) bad.push("the glow's peak " + B.GLOW_PEAK + " and reach " + B.GLOW_REACH + ", not the probe's 0.9 and 18");
  for (const n of COUNTS) for (const w of checkLayout(n)) bad.push(n + ": " + w);
  // The probe's generator, while it's there: the same strokes for every count.
  const probe = path.join(__dirname, "..", "tprobe", "build.js");
  let compared = false;
  if (fs.existsSync(probe)) {
    const P = require(probe);
    for (const n of COUNTS) if (JSON.stringify(P.layout(n)) !== JSON.stringify(B.layout(n))) bad.push(n + ": not tools/data/tprobe's layout");
    compared = true;
  }
  if (bad.length) fail("layout: " + bad.join("; "));
  else ok("layout: counts 1-11 - the probe's (n less its fives upright bars, a rising strike over each five's four), inside the margin, centred" + (compared ? "; the same as tools/data/tprobe's" : ""));
}

// --- the pictures ----------------------------------------------------------------------------------------------------
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
function headProblems(name, t) {
  const p = [];
  if (t.head.imageType !== 2 || t.head.colorMapType !== 0 || t.head.idLength !== 0) p.push("not an uncompressed true-colour TGA");
  if (t.head.width !== W || t.head.height !== H) p.push("size " + t.head.width + " x " + t.head.height);
  if (t.head.bpp !== 32 || t.head.descriptor !== 0x08) p.push("bpp " + t.head.bpp + " descriptor " + t.head.descriptor);
  if (t.size !== 18 + W * H * 4) p.push("length " + t.size);
  return p;
}
const distanceTo = (L) => {
  const segs = [...L.bars, ...L.strikes];
  return (x, y) => Math.min(...segs.map((s) => {
    const dx = s.x1 - s.x0, dy = s.y1 - s.y0, l2 = dx * dx + dy * dy;
    let t = l2 > 0 ? ((x - s.x0) * dx + (y - s.y0) * dy) / l2 : 0;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return Math.hypot(s.x0 + t * dx - x, s.y0 + t * dy - y);
  }));
};
const inBorder = (x, y) => x < B.MARGIN || y < B.MARGIN || x >= W - B.MARGIN || y >= H - B.MARGIN;

// The bars: #010101 everywhere, alpha the stroke's cover.
const bars = new Map();  // n -> px
{
  const bad = [];
  for (const n of COUNTS) {
    const bytes = first.get(barsPicture(n) + ".tga");
    if (!bytes) continue;
    const t = readTga(bytes);
    const head = headProblems(barsPicture(n), t);
    if (head.length) {
      bad.push(barsPicture(n) + ": " + head.join(", "));
      continue;
    }
    bars.set(n, t.px);
    const L = B.layout(n), dist = distanceTo(L);
    let notGrey = 0, pureBlack = 0, coreClear = 0, outsideSeen = 0, borderSeen = 0, rimWrong = 0, core = 0;
    for (let y = 0; y < H; ++y) {
      for (let x = 0; x < W; ++x) {
        const p = t.px[y][x];
        if (p[0] !== BAR_RGB[0] || p[1] !== BAR_RGB[1] || p[2] !== BAR_RGB[2]) ++notGrey;
        if (p[3] > 0 && p[0] === 0 && p[1] === 0 && p[2] === 0) ++pureBlack;
        if (inBorder(x, y) && p[3]) ++borderSeen;
        const d = dist(x + 0.5, y + 0.5);
        if (d <= B.HALF - 0.75) {
          ++core;
          if (p[3] !== 255) ++coreClear;
        } else if (d >= B.HALF + 0.75) {
          if (p[3]) ++outsideSeen;
        } else if (p[3] === 0 && d < B.HALF - 0.25 || p[3] === 255 && d > B.HALF + 0.25) {
          ++rimWrong;
        }
      }
    }
    const what = [];
    if (notGrey) what.push(notGrey + " pixels not #010101");
    if (pureBlack) what.push(pureBlack + " pixels with alpha over 0 pure black (0 0 0)");
    if (core < 500 * n / 3) what.push("only " + core + " core pixels");
    if (coreClear) what.push(coreClear + " core pixels not opaque");
    if (outsideSeen) what.push(outsideSeen + " pixels past the stroke not clear");
    if (borderSeen) what.push(borderSeen + " pixels in the border not clear");
    if (rimWrong) what.push(rimWrong + " rim pixels not between");
    for (const s of [...L.bars, ...L.strikes]) {
      for (let k = 0; k <= 20; ++k) {
        const x = s.x0 + (k / 20) * (s.x1 - s.x0), y = s.y0 + (k / 20) * (s.y1 - s.y0);
        if (t.px[Math.floor(y)][Math.floor(x)][3] !== 255) {
          what.push("a stroke missing at " + Math.floor(x) + "," + Math.floor(y));
          break;
        }
      }
    }
    if (what.length) bad.push(barsPicture(n) + " (" + n + "): " + what.join("; "));
  }
  if (bad.length) fail("bars pictures: " + bad.join(" | "));
  else ok("11 bars pictures: 256 x 128, #010101 everywhere (never 0 0 0 where seen), opaque strokes, clear elsewhere and in the border");
  const distinct = new Set(COUNTS.map((n) => (first.get(barsPicture(n) + ".tga") || Buffer.alloc(0)).toString("base64")));
  if (distinct.size !== COUNTS.length) fail("bars pictures: " + distinct.size + " distinct of 11");
}

// The glows: the style's colour round the strokes, black under the opaque bars.
const glows = new Map();  // code + n -> px
{
  const bad = [];
  for (const s of STYLES) {
    const top = s.colour.indexOf(255);  // the channel at full strength (each colour has one)
    for (const n of COUNTS) {
      const name = glowPicture(s, n);
      const bytes = first.get(name + ".tga");
      if (!bytes) continue;
      const t = readTga(bytes);
      const head = headProblems(name, t);
      if (head.length) {
        bad.push(name + ": " + head.join(", "));
        continue;
      }
      glows.set(s.code + n, t.px);
      const L = B.layout(n), dist = distanceTo(L), barPx = bars.get(n);
      let notOpaque = 0, borderLit = 0, hueWrong = 0, tooBright = 0, beyondLit = 0, underBars = 0, rimBright = 0, ring = 0, ringDark = 0, brightest = 0;
      for (let y = 0; y < H; ++y) {
        for (let x = 0; x < W; ++x) {
          const p = t.px[y][x];
          if (p[3] !== 255) ++notOpaque;
          const lit = p[0] || p[1] || p[2];
          if (inBorder(x, y) && lit) ++borderLit;
          if (barPx && barPx[y][x][3] === 255 && lit) ++underBars;
          // On the bars' antialiased rim the glow fades in as the bar fades out: at most the peak times what the bar
          // leaves uncovered (1 - its alpha), so the edge never shows the glow over the bar.
          if (barPx && lit && p[top] > 255 * B.GLOW_PEAK * (1 - barPx[y][x][3] / 255) + 1) ++rimBright;
          const d = dist(x + 0.5, y + 0.5);
          if (lit) {
            const g = p[top] / 255;
            if (s.colour.some((c, i) => Math.abs(p[i] - c * g) > 1.01)) ++hueWrong;
            if (p[top] > Math.round(255 * B.GLOW_PEAK)) ++tooBright;
            brightest = Math.max(brightest, p[top]);
          }
          if (d >= B.HALF + B.GLOW_REACH && lit) ++beyondLit;
          if (d >= B.HALF + 1 && d <= B.HALF + 4) {  // just outside a stroke: the ring that outlines it
            ++ring;
            if (p[top] < 0.5 * 255 * B.GLOW_PEAK) ++ringDark;
          }
        }
      }
      const what = [];
      if (notOpaque) what.push(notOpaque + " pixels not alpha 255");
      if (borderLit) what.push(borderLit + " lit pixels in the border");
      if (underBars) what.push(underBars + " lit pixels under the opaque bars (they'd not stay black)");
      if (rimBright) what.push(rimBright + " pixels on the bars' rim brighter than the peak times what the bar leaves uncovered");
      if (hueWrong) what.push(hueWrong + " pixels not " + s.spell + "'s colour");
      if (tooBright) what.push(tooBright + " pixels brighter than the peak");
      if (beyondLit) what.push(beyondLit + " lit pixels past the glow's reach");
      if (ring < 300 * n / 3 || ringDark) what.push("the ring round the strokes: " + ringDark + " of " + ring + " under half the peak");
      if (brightest < 0.95 * 255 * B.GLOW_PEAK) what.push("its brightest " + brightest);
      if (what.length) bad.push(name + " (" + n + "): " + what.join("; "));
    }
  }
  if (bad.length) fail("glow pictures: " + bad.join(" | "));
  else ok("33 glow pictures: 256 x 128, alpha 255, each spell's colour (#3a7bff, #ff8a1e, #a040ff) round the strokes at most " + B.GLOW_PEAK + ", black under the opaque bars, past the reach and in the border");
  if (JSON.stringify(B.STYLES.map((s) => [s.code, s.colour])) !== JSON.stringify(STYLES.map((s) => [s.code, s.colour]))) {
    fail("build.js's styles: " + JSON.stringify(B.STYLES));
  }
}

// --- the two layers as the game composites them ------------------------------------------------------------------------
// The engine (nwmain-linux, the TF reading; docs/TOOLS-PLAN.md): the bars, alpha-blended (SRC_ALPHA, ONE_MINUS_SRC_ALPHA,
// a pixel at alpha 0.2 or under discarded), in the dynamic blended bucket (3); the glow, additive (SRC_ALPHA, ONE, no
// depth written), in the additive bucket (4) - drawn after, every frame. The glow lies nearer the camera, so the bars'
// depth never cuts it. Over each background, then the other order (should it ever be): a bar's core stays #010101.
{
  const BACKGROUNDS = { night: [0.02, 0.03, 0.05], grass: [0.25, 0.35, 0.15], snow: [1, 1, 1], sky: [0.55, 0.75, 0.95] };
  const bad = [];
  let pixels = 0, rim = 0;
  for (const s of STYLES) {
    for (const n of COUNTS) {
      const bp = bars.get(n), gp = glows.get(s.code + n);
      if (!bp || !gp) continue;
      for (const [label, bg] of Object.entries(BACKGROUNDS)) {
        let coreWrong = 0, clearWrong = 0;
        for (let y = 0; y < H; ++y) {
          for (let x = 0; x < W; ++x) {
            const a = bp[y][x][3] / 255;
            const barsFirst = [0, 1, 2].map((i) => {
              const under = a > 0.2 ? bg[i] * (1 - a) + (BAR_RGB[i] / 255) * a : bg[i];
              return Math.min(1, under + gp[y][x][i] / 255);
            });
            const glowFirst = [0, 1, 2].map((i) => {
              const lit = Math.min(1, bg[i] + gp[y][x][i] / 255);
              return a > 0.2 ? lit * (1 - a) + (BAR_RGB[i] / 255) * a : lit;
            });
            ++pixels;
            if (bp[y][x][3] === 255) {
              if (barsFirst.some((c) => Math.abs(c - 1 / 255) > 1e-9) || glowFirst.some((c) => Math.abs(c - 1 / 255) > 1e-9)) ++coreWrong;
            } else if (bp[y][x][3] === 0) {
              if (barsFirst.some((c, i) => Math.abs(c - glowFirst[i]) > 1e-9)) ++clearWrong;
            } else {
              ++rim;  // the antialiased edge: the only pixels the order can change
            }
          }
        }
        if (coreWrong) bad.push(`${s.code}${pad(n)} on ${label}: ${coreWrong} core pixels not #010101`);
        if (clearWrong) bad.push(`${s.code}${pad(n)} on ${label}: ${clearWrong} clear pixels differ by the order`);
      }
    }
  }
  if (bad.length) fail("composited: " + bad.slice(0, 6).join("; ") + (bad.length > 6 ? " ..." : ""));
  else ok(`composited over night, grass, snow and sky, bars then glow (the engine's order) and glow then bars: every bar core #010101, the glow round it; the orders differ only on the rim (${rim} of ${pixels} pixels)`);
}

// --- the TXIs and materials ------------------------------------------------------------------------------------------
const txt = (name) => (first.get(name) || Buffer.alloc(0)).toString("latin1");
{
  // Never shrunk (the owner, 2026-11-20: "add the downsample lines to my textures too for now"): every TXI here says
  // downsamplemax 0 and downsamplemin 0; each picture has a TXI of its own with those alone.
  const KEEP = "downsamplemax 0\ndownsamplemin 0\n";
  const bad = [];
  for (const n of COUNTS) {
    if (txt(barsBitmap(n) + ".txi") !== "blending default\n" + KEEP) bad.push(barsBitmap(n) + ".txi");
    if (txt(barsPicture(n) + ".txi") !== KEEP) bad.push(barsPicture(n) + ".txi");
    if (txt(barsBitmap(n) + ".mtr") !== `customshadervs ${SHADER}\ncustomshaderfs fslit\ntexture0 ${barsPicture(n)}\ntransparency 1\n`) bad.push(barsBitmap(n) + ".mtr");
    for (const s of STYLES) {
      if (txt(glowBitmap(s, n) + ".txi") !== "blending additive\n" + KEEP) bad.push(glowBitmap(s, n) + ".txi");
      if (txt(glowPicture(s, n) + ".txi") !== KEEP) bad.push(glowPicture(s, n) + ".txi");
      if (txt(glowBitmap(s, n) + ".mtr") !== `customshadervs ${SHADER}\ncustomshaderfs fslit\ntexture0 ${glowPicture(s, n)}\n`) bad.push(glowBitmap(s, n) + ".mtr");
    }
  }
  // and no TXI (or picture) without them, whatever else build() makes
  for (const [name] of first) {
    if (/\.txi$/.test(name) && (!/(^|\n)downsamplemax 0\n/.test(txt(name)) || !/(^|\n)downsamplemin 0\n/.test(txt(name)))) bad.push(name + " without the downsample lines");
    if (/\.tga$/.test(name) && !first.has(name.replace(/\.tga$/, ".txi"))) bad.push(name + " without a TXI");
  }
  if (bad.length) fail("materials / TXIs: " + bad.join(" "));
  else ok("88 TXIs, each with downsamplemax 0 and downsamplemin 0 (the glows' blending additive, the bars' blending default - no decal; the 44 pictures' those alone) and 44 materials (vsn5bill_sl + fslit + texture0 its picture; the bars' transparency 1)");
}

// --- the models ------------------------------------------------------------------------------------------------------
{
  const failuresBefore = failures;
  for (const s of STYLES) {
    for (const n of COUNTS) {
      const name = model(s, n);
      const text = txt(name + ".mdl");
      const lines = text.split("\n");
      const what = [];
      if (/[^\x0a\x20-\x7e]/.test(text)) what.push("not plain ASCII with LF");
      if (lines[0] !== "#MAXMODEL ASCII") what.push("first line");
      if (!lines[1] || !lines[1].startsWith("# nwn5e (tools/data/tally/build.js)")) what.push("no nwn5e mark");
      if (!text.includes("\nnewmodel " + name + "\n") || !text.includes("\ndonemodel " + name + "\n")) what.push("newmodel / donemodel");
      if (!text.includes("\nsetsupermodel " + name + " NULL\n")) what.push("supermodel");
      if (!text.includes("\nclassification effect\n")) what.push("not an effect");
      if (!text.includes("\nnode dummy " + name + "\n  parent NULL\nendnode\n")) what.push("its root");
      if ((text.match(/\nnode trimesh /g) || []).length !== 2) what.push("not two trimeshes");
      if (/\nnode (emitter|light|danglymesh|skin|animmesh|aabb) /.test(text)) what.push("an emitter, light or other node");
      const layers = [{ node: name + "_bars", bitmap: barsBitmap(n), y: 0 }, { node: name + "_glow", bitmap: glowBitmap(s, n), y: -0.01 }];
      for (const layer of layers) {
        const at = text.indexOf("\nnode trimesh " + layer.node + "\n");
        if (at < 0) {
          what.push("no trimesh " + layer.node);
          continue;
        }
        const mesh = text.slice(at, text.indexOf("\nendnode", at));
        const field = (k) => {
          const m = new RegExp("\\n  " + k + " ([^\\n]*)").exec(mesh);
          return m ? m[1] : null;
        };
        for (const [k, v] of [["parent", name], ["bitmap", layer.bitmap], ["ambient", "0 0 0"], ["diffuse", "0 0 0"], ["specular", "0 0 0"],
                              ["selfillumcolor", "1 1 1"], ["alpha", "1"], ["render", "1"], ["shadow", "0"], ["transparencyhint", "0"],
                              ["position", "0 0 0.55"], ["orientation", "0 0 0 0"]]) {
          if (field(k) !== v) what.push(layer.node + " " + k + " " + field(k));
        }
        const vi = mesh.indexOf("\n  verts 4\n"), ti = mesh.indexOf("\n  tverts 4\n"), fi = mesh.indexOf("\n  faces 2\n");
        if (vi < 0 || ti < 0 || fi < 0) {
          what.push(layer.node + " verts / tverts / faces");
          continue;
        }
        const nums = (at2, k) => mesh.slice(at2).split("\n").slice(2, 2 + k).map((l) => l.trim().split(/\s+/).map(Number));
        const v = nums(vi, 4), t = nums(ti, 4), f = nums(fi, 2);
        if (v.some((c) => c.length !== 3 || Math.abs(c[1] - layer.y) > 1e-9 || Math.abs(Math.abs(c[0]) - 0.25) > 1e-9 || Math.abs(Math.abs(c[2]) - 0.125) > 1e-9)) {
          what.push(layer.node + " verts not a 0.5 x 0.25 m quad in the x-z plane at y " + layer.y);
        }
        v.forEach((c, i) => {
          if (!t[i] || t[i][0] !== (c[0] > 0 ? 1 : 0) || t[i][1] !== (c[2] > 0 ? 1 : 0)) what.push(layer.node + " tvert " + i + " not u right, v up");
        });
        for (const face of f) {
          const [a, b, c] = face.slice(0, 3).map((i) => v[i]);
          if (!a || !b || !c) continue;
          const e1 = [b[0] - a[0], b[2] - a[2]], e2 = [c[0] - a[0], c[2] - a[2]];
          if (!(e1[0] * e2[1] - e1[1] * e2[0] > 0)) what.push(layer.node + " a face not counter-clockwise from the front");
          if (face.slice(4, 7).join(" ") !== face.slice(0, 3).join(" ")) what.push(layer.node + " a face's tverts not its verts");
        }
      }
      for (const anim of ["impact", "duration"]) if (!text.includes("\nnewanim " + anim + " " + name + "\n")) what.push("no " + anim);
      if (/\bnewanim\s+cessation\b/i.test(text)) what.push("has a cessation (a removed marker would linger)");
      if ((text.match(/\nnewanim /g) || []).length !== 2) what.push("not exactly impact and duration");
      if (/\n\s*length 0\s*\n/.test(text)) what.push("an animation of length 0");
      if (/key\s+\d/i.test(text)) what.push("keyed (a key would wait for the clock while paused)");
      if (what.length) fail(name + ".mdl: " + what.join("; "));
    }
  }
  if (failures === failuresBefore) ok("33 models: the bars (y 0) and the glow (y -0.01) quads, 0.5 x 0.25 m, 0.55 m over the head node, each bitmap its material, no cessation, no keys, no emitter");
}

// --- the shader ------------------------------------------------------------------------------------------------------
{
  const WANT = [
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
  ];
  const text = txt(SHADER + ".shd");
  const lines = text.split("\n");
  const what = [];
  if (/[^\x0a\x20-\x7e]/.test(text) || text.includes("`")) what.push("not plain ASCII with LF (or a backtick)");
  if (!lines[0].startsWith("// " + SHADER + ".shd - nwn5e's camera-facing layered quad")) what.push("no mark on its first line");
  const start = lines.indexOf("#define SHADER_TYPE 1");
  if (start < 0 || lines.slice(0, start).some((l) => l && !l.startsWith("//"))) what.push("its header isn't comments");
  const have = lines.slice(start);
  if (JSON.stringify(have) !== JSON.stringify(WANT)) {
    const at = WANT.findIndex((l, i) => have[i] !== l);
    what.push("line " + (start + at + 1) + ": " + JSON.stringify(have[at]) + ", not " + JSON.stringify(WANT[at]));
  }
  if (what.length) fail(SHADER + ".shd: " + what.join("; "));
  else ok(SHADER + ".shd: the lit vertex setup, then the spherical billboard's lines with the layer's nudge toward the eye");
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
const scaling = (k) => [k, 0, 0, 0, 0, k, 0, 0, 0, 0, k, 0, 0, 0, 0, 1];
function lookAt(eye, at) {
  const f = nrm(sub(at, eye));
  let sv = crs(f, [0, 0, 1]);
  sv = Math.hypot(...sv) < 1e-9 ? [1, 0, 0] : nrm(sv);
  const u = crs(sv, f);
  return [sv[0], u[0], -f[0], 0, sv[1], u[1], -f[1], 0, sv[2], u[2], -f[2], 0, -dot(sv, eye), -dot(u, eye), dot(f, eye), 1];
}
const PROJ = (() => {  // a perspective projection, 60 degrees, 16:9
  const f = 1 / Math.tan(Math.PI / 6), a = 16 / 9, near = 0.1, far = 100;
  return [f / a, 0, 0, 0, 0, f, 0, 0, 0, 0, (far + near) / (near - far), -1, 0, 0, (2 * far * near) / (near - far), 0];
})();
const screen = (p) => {
  const c = apply(PROJ, [...p, 1]);
  return [c[0] / c[3], c[1] / c[3]];
};
{
  const layerCorners = (y) => [[-0.25, y, -0.125, 1], [0.25, y, -0.125, 1], [0.25, y, 0.125, 1], [-0.25, y, 0.125, 1]];
  const bad = [];
  let views = 0, worstAlign = 0, leastGap = Infinity;
  for (const head of [[10, 20, 1.7], [-3.5, 7.25, 0.4], [0, 0, 0]]) {
    for (const scale of [1, 1.3]) {
      for (let yaw = 0; yaw < 360; yaw += 30) {
        for (const pitch of [5, 30, 60, 85, 90]) {
          for (const [distance, aim] of [[2, [0, 0, 0]], [12, [0, 0, 0]], [30, [0, 0, 0]], [2, [0.6, -0.4, 0.3]], [12, [-2.5, 1.5, -1]]]) {
            // The camera `distance` from the marker, looking at it - or past it (`aim`): the marker off the middle of
            // the screen, where the eye's line through it isn't the view's axis.
            const target = [head[0], head[1], head[2] + 0.55 * scale];
            const yr = (yaw * Math.PI) / 180, pr = (pitch * Math.PI) / 180;
            const eye = [target[0] + distance * Math.cos(pr) * Math.cos(yr), target[1] + distance * Math.cos(pr) * Math.sin(yr),
              target[2] + distance * Math.sin(pr)];
            const view = pitch === 90 ? (() => {  // straight down: the camera's up its yaw
              const sv = [-Math.sin(yr), Math.cos(yr), 0], u = [-Math.cos(yr), -Math.sin(yr), 0], f = [0, 0, -1];
              return [sv[0], u[0], -f[0], 0, sv[1], u[1], -f[1], 0, sv[2], u[2], -f[2], 0, -dot(sv, eye), -dot(u, eye), dot(f, eye), 1];
            })() : lookAt(eye, [0, 1, 2].map((i) => target[i] + aim[i]));
            const mdl = mul(mul(translate(...head), scaling(scale)), translate(0, 0, 0.55));
            const mv = mul(view, mdl);
            const nodeView = apply(mv, [0, 0, 0, 1]).slice(0, 3);
            ++views;
            const out = {};
            for (const [label, y] of [["bars", 0], ["glow", -0.01]]) {
              out[label] = layerCorners(y).map((c) => B.billboard(mv, view, c));
              const o = out[label];
              const at = `yaw ${yaw} pitch ${pitch} distance ${distance}`;
              if (o.some((q) => [...q.pos, ...q.normal].some((x) => !Number.isFinite(x)))) {
                bad.push(`${label}: NaN at ${at}`);
                continue;
              }
              const centre = [0, 1, 2].map((i) => o.reduce((sum, q) => sum + q.pos[i], 0) / 4);
              const width = Math.hypot(...sub(o[1].pos, o[0].pos)), height = Math.hypot(...sub(o[3].pos, o[0].pos));
              if (Math.abs(width - 0.5 * scale) > 1e-9 || Math.abs(height - 0.25 * scale) > 1e-9) bad.push(`${label}: size ${width.toFixed(4)} x ${height.toFixed(4)} at ${at}`);
              if (o.some((q) => Math.abs(q.pos[2] - centre[2]) > 1e-6 * distance)) bad.push(`${label}: not in a plane parallel to the view at ${at}`);
              if (Math.hypot(...sub(o[0].normal, [0, 0, 1])) > 1e-9) bad.push(`${label}: normal not at the camera at ${at}`);
              if (Math.abs(o[3].pos[0] - o[0].pos[0]) > 1e-9 || !(o[3].pos[1] > o[0].pos[1])) bad.push(`${label}: not upright on screen at ${at}`);
              if (label === "bars" && Math.hypot(...sub(centre, nodeView)) > 1e-9) bad.push(`bars: centre off the node at ${at}`);
              const sc = o.map((q) => screen(q.pos));
              const ccw = (a, b, c) => (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
              if (!(ccw(sc[0], sc[1], sc[2]) > 0 && ccw(sc[0], sc[2], sc[3]) > 0)) bad.push(`${label}: clockwise on screen at ${at}`);
            }
            if (!out.bars || !out.glow || out.bars.length !== 4 || out.glow.length !== 4) continue;
            // The glow nearer the camera at every corner (view depth -z smaller), by about the nudge along the eye's line.
            const dist = Math.hypot(...nodeView);
            for (let k = 0; k < 4; ++k) {
              const gap = -out.bars[k].pos[2] - -out.glow[k].pos[2];
              leastGap = Math.min(leastGap, gap / scale);
              if (!(gap >= 0.0075 * scale)) bad.push(`the glow not nearer at corner ${k}, ${yaw} / ${pitch} / ${distance}: ${gap.toFixed(5)}`);
            }
            // The glow's centre on the eye's line through the bars' (the same point on the screen), and its picture the
            // bars' scaled about it by distance / (distance - nudge): over the bars within a hundredth of its size.
            const gc = [0, 1, 2].map((i) => out.glow.reduce((sum, q) => sum + q.pos[i], 0) / 4);
            const bs = screen(nodeView), gs = screen(gc);
            if (Math.hypot(bs[0] - gs[0], bs[1] - gs[1]) > 1e-9) bad.push(`the glow's centre off the bars' on screen at ${yaw} / ${pitch} / ${distance}`);
            const want = dist / (dist - 0.01 * scale);
            for (let k = 0; k < 4; ++k) {
              const b = screen(out.bars[k].pos), g = screen(out.glow[k].pos);
              const scaled = [bs[0] + (b[0] - bs[0]) * want, bs[1] + (b[1] - bs[1]) * want];
              const size = Math.hypot(b[0] - bs[0], b[1] - bs[1]);
              const off = Math.hypot(g[0] - scaled[0], g[1] - scaled[1]), apart = Math.hypot(g[0] - b[0], g[1] - b[1]);
              if (off > 1e-6 * Math.max(1, size)) bad.push(`the glow's corner ${k} not the bars' scaled at ${yaw} / ${pitch} / ${distance}`);
              worstAlign = Math.max(worstAlign, apart / size);
            }
          }
        }
      }
    }
  }
  if (worstAlign > 0.01) bad.push(`the glow off the bars by ${(100 * worstAlign).toFixed(2)}% of the quad's size`);
  if (bad.length) fail(`billboard: ${bad.length} problems over ${views} views - ${[...new Set(bad)].slice(0, 4).join("; ")}`);
  else ok(`billboard: ${views} views - both layers in the view plane, facing the camera, upright on screen, counter-clockwise; the glow nearer at every corner (at least ${(100 * leastGap).toFixed(2)} cm a metre of scale), its picture over the bars' within ${(100 * worstAlign).toFixed(2)}% of the quad`);
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
    if (rows.length !== 33) what.push(rows.length + " rows");
    let i = 0;
    STYLES.forEach((s, k) => {
      for (const n of COUNTS) {
        const r = rows[i++] || [];
        const want = [String(FIRST_ROW + 11 * k + n - 1), `NWN5E_TALLY_${s.code.toUpperCase()}_${n}`, "D", "0", model(s, n), "0"];
        if (JSON.stringify(r) !== JSON.stringify(want)) what.push(JSON.stringify(r.join(" ")) + " not " + JSON.stringify(want.join(" ")));
      }
    });
  }
  if (what.length) fail("data\\rows\\visualeffects.tally.txt: " + what.slice(0, 4).join("; "));
  else ok("data\\rows\\visualeffects.tally.txt: 2215-2247 - Magic Missile, Scorching Ray, Eldritch Blast, 1-11 each - type D, on the head, OrientWithObject 0");
}

// --- our own art: no font, canvas or image library -------------------------------------------------------------------
{
  const source = fs.readFileSync(path.join(__dirname, "build.js"), "utf8");
  const required = [...source.matchAll(/require\(\s*["']([^"']+)["']\s*\)/g)].map((m) => m[1]);
  const foreign = required.filter((r) => r !== "fs" && r !== "path");
  if (foreign.length) fail("build.js loads " + foreign.join(", ") + " (only fs and path: the tallies are drawn from its own shapes)");
  else ok("build.js loads only fs and path: its own strokes");
}

// --- data\files holds exactly these bytes ----------------------------------------------------------------------------
{
  const stale = [];
  for (const [name, bytes] of first) {
    const at = path.join(FILES, name);
    if (!fs.existsSync(at)) stale.push(name + " (missing)");
    else if (!B.same(name, fs.readFileSync(at), bytes)) stale.push(name + " (differs)");
  }
  if (stale.length) fail("data\\files out of step with the generator (node tools\\data\\tally\\build.js): " + stale.slice(0, 8).join(", ") + (stale.length > 8 ? ` ... ${stale.length}` : ""));
  else ok("data\\files holds the generator's " + first.size + " files");
}

console.log(failures ? "\n" + failures + " failed" : "\nall passed");
process.exit(failures ? 1 : 0);
