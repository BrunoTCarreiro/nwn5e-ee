#!/usr/bin/env node
// The particle mask's shader (#184; the owner, 2026-11-23: "is there a way to make a "mask" effect? an effect that makes
// particles from other effects disappear", then "run the research spike now", "maskproof seems good!" and "wire it, lets
// test it out"): fsn5pmask.shd, made at build time from the player's own game - its particle fragment shader,
// fsparticle.shd, with a mask added - into the build's data\files (build_derived.js calls derive). Never committed: it
// is the game's shader text.
//
// - Why the game's own: an emitter whose texture is one of nwn5e's masked materials (data\files\n5_pmask.mtr, the
//   proof's; the spells' n5x_*.mtr, spell_mask.js: the game's vsparticle by name, this shader, a game texture by name)
//   draws with this instead of fsparticle. Made from fsparticle itself, it draws exactly as the game's particles do -
//   the same texture, colour, alpha cut, soft edges and fog - and follows a game update that changes them.
// - What is added, and nothing else changed: the game's scriptable uniforms (its own include, by name), the view
//   position's varying when the game's defines haven't declared it, the mask's functions before main, and one call to
//   it just before main's last line (gl_FragColor = FragmentColor). The mask is off while scriptableVec1.w is 0 - every
//   uniform's default - and then returns at once: the shader draws exactly as fsparticle.
// - The mask (src/rules/particle_mask.h packs its uniforms and says their layout; rules::ParticleMasked is the same test
//   on the CPU, checked there against the cover rule's line): the fragment's place in the area, from its view position
//   by the inverse view matrix (as the game's own target highlight finds it). Within the reach (scriptableVec1.w, across
//   the ground from the origin scriptableVec1.xyz) it is dropped - its colour cleared and discarded - when it lies past
//   the wall in its direction (the ring: 32 directions' nearness, scriptableVec2..9, taken in proportion between the two
//   either side of it) or the line from the origin to it passes through one of the four boxes (scriptableVec10..16, 7
//   numbers each back to back - BOX_NUMBERS below writes where each is read).
// - Anchors: the game's text must have one "void main" line and one "gl_FragColor = FragmentColor;" line. If it hasn't
//   (a game update rewrote it), nothing is made and the build says so; the masked materials then have no shader to
//   draw with and !maskproof says it's missing.
//
//   node tools\data\particle_mask.js --out <folder> [--game <folder>] [--tools <nwn5e-tools>]   (by hand; build.bat runs
//   build_derived.js, which calls derive)
"use strict";
const fs = require("fs");
const path = require("path");

const NAME = "fsn5pmask";
const SOURCE = "fsparticle";
const TYPE_SHD = 2069;  // the game's shaders' resource type (base_shaders.bif)
const MARK = "// fsn5pmask.shd - nwn5e's masked particle shader (nwn5e-ee #184), made at build time by tools/data/particle_mask.js";

const HEADER = [
  MARK,
  "// from the player's own game's fsparticle.shd (below, as the game has it) with nwn5e's mask added: the lines marked",
  "// nwn5e. Not committed anywhere: it is the game's text. Change particle_mask.js and rebuild, not this.",
  "",
].join("\n");

// The layout (src/rules/particle_mask.h): the ring's 32 directions in scriptableVec2..9, 4 a vector; four boxes' 7
// numbers each back to back from scriptableVec10 - number n of box j (7j + n of the 28) in scriptableVec<10 + that / 4>,
// component that % 4: its centre x y z, half sizes x y z, the angle of its +X.
const RING_DIRECTIONS = 32, RING_FIRST = 2, BOX_FIRST = 10, BOXES = 4, BOX_SIZE = 7;
const BOX_NUMBERS = (j) => Array.from({ length: BOX_SIZE }, (_, n) => {
  const at = BOX_SIZE * j + n;
  return `scriptableVec${BOX_FIRST + Math.floor(at / 4)}.${"xyzw"[at % 4]}`;
});
const boxCall = (j) => {
  const v = BOX_NUMBERS(j);
  return `N5Box(o, p, vec3(${v[0]}, ${v[1]}, ${v[2]}), vec3(${v[3]}, ${v[4]}, ${v[5]}), ${v[6]})`;
};
const STEP = (2 * Math.PI / RING_DIRECTIONS).toFixed(8);  // a direction's angle, radians

const DECLARATIONS = [
  "// nwn5e: the particle mask (tools/data/particle_mask.js; src/rules/particle_mask.h packs its uniforms).",
  "#include \"inc_scriptable\"",
  "#if POSITION_VIEW != 1",
  "varying highp vec3 vPosView;",
  "#endif",
  "",
  "// nwn5e: one slab of a box's frame - the line's part inside it narrows [t0, t1].",
  "bool N5Slab(float o, float d, float h, inout float t0, inout float t1)",
  "{",
  "    if (abs(d) < 1e-6)",
  "        return abs(o) <= h;",
  "    float u = (-h - o) / d;",
  "    float v = (h - o) / d;",
  "    t0 = max(t0, min(u, v));",
  "    t1 = min(t1, max(u, v));",
  "    return t0 <= t1;",
  "}",
  "",
  "// nwn5e: whether the line from o to p meets the box: c its centre, h its half sizes, a the angle of its +X (none when",
  "// it has no half size across).",
  "bool N5Box(vec3 o, vec3 p, vec3 c, vec3 h, float a)",
  "{",
  "    if (h.x <= 0.0)",
  "        return false;",
  "    vec2 xa = vec2(cos(a), sin(a));",
  "    vec2 ya = vec2(-xa.y, xa.x);",
  "    vec2 ro = o.xy - c.xy;",
  "    vec2 rp = p.xy - c.xy;",
  "    vec3 lo = vec3(dot(ro, xa), dot(ro, ya), o.z - c.z);",
  "    vec3 lp = vec3(dot(rp, xa), dot(rp, ya), p.z - c.z);",
  "    vec3 d = lp - lo;",
  "    float t0 = 0.0;",
  "    float t1 = 1.0;",
  "    return N5Slab(lo.x, d.x, h.x, t0, t1) && N5Slab(lo.y, d.y, h.y, t0, t1) && N5Slab(lo.z, d.z, h.z, t0, t1);",
  "}",
  "",
  `// nwn5e: direction k's wall nearness (1 / metres; 0: none) - scriptableVec${RING_FIRST}..${RING_FIRST + RING_DIRECTIONS / 4 - 1}, 4 a vector.`,
  "float N5Near(float k)",
  "{",
  "    float s = floor(k / 4.0);",
  "    float c = k - 4.0 * s;",
  `    vec4 v = scriptableVec${RING_FIRST};`,
  ...Array.from({ length: RING_DIRECTIONS / 4 - 1 }, (_, i) => `    if (s > ${i}.5) v = scriptableVec${RING_FIRST + i + 1};`),
  "    if (c < 0.5) return v.x;",
  "    if (c < 1.5) return v.y;",
  "    if (c < 2.5) return v.z;",
  "    return v.w;",
  "}",
  "",
  "// nwn5e: off (scriptableVec1.w 0, the default) it returns at once; on, a fragment within the reach that lies past the",
  "// wall in its direction, or behind or inside a box, is dropped.",
  "void N5ParticleMask(inout vec4 colour)",
  "{",
  "    if (scriptableVec1.w <= 0.0)",
  "        return;",
  "    vec3 p = (m_view_inv * vec4(vPosView, 1.0)).xyz;",
  "    vec3 o = scriptableVec1.xyz;",
  "    vec2 d = p.xy - o.xy;",
  "    float r = length(d);",
  "    if (r > scriptableVec1.w)",
  "        return;",
  "    bool hidden = false;",
  "    if (r > 0.01)",
  "    {",
  `        float f = atan(d.y, d.x) / ${STEP};`,
  "        if (f < 0.0)",
  `            f += ${RING_DIRECTIONS}.0;`,
  "        float k0 = floor(f);",
  "        float t = f - k0;",
  `        if (k0 > ${RING_DIRECTIONS - 1}.5)`,
  `            k0 -= ${RING_DIRECTIONS}.0;`,
  "        float k1 = k0 + 1.0;",
  `        if (k1 > ${RING_DIRECTIONS - 1}.5)`,
  "            k1 = 0.0;",
  "        hidden = r * mix(N5Near(k0), N5Near(k1), t) > 1.0;",
  "    }",
  "    hidden = hidden",
  ...Array.from({ length: BOXES }, (_, j) => `        || ${boxCall(j)}${j === BOXES - 1 ? ";" : ""}`),
  "    if (hidden)",
  "    {",
  "        colour = vec4(0.0);",
  "        discard;",
  "    }",
  "}",
  "",
].join("\n");

const CALL = "    N5ParticleMask(FragmentColor); // nwn5e: the particle mask";

// The game's fsparticle text -> { text } with the mask added, or { error } (an anchor missing or doubled).
function patch(source) {
  const eol = source.includes("\r\n") ? "\r\n" : "\n";
  const lines = source.split(/\r?\n/);
  const mains = lines.map((l, i) => (/^\s*void\s+main\s*\(/.test(l) ? i : -1)).filter((i) => i >= 0);
  const outs = lines.map((l, i) => (/^\s*gl_FragColor\s*=\s*FragmentColor\s*;\s*$/.test(l) ? i : -1)).filter((i) => i >= 0);
  if (mains.length !== 1) return { error: `${SOURCE}.shd has ${mains.length} "void main" lines, not one` };
  if (outs.length !== 1) return { error: `${SOURCE}.shd has ${outs.length} "gl_FragColor = FragmentColor;" lines, not one` };
  if (outs[0] < mains[0]) return { error: `${SOURCE}.shd sets gl_FragColor before its main` };
  if (/\bN5ParticleMask\b|\bscriptableVec1\b/.test(source)) return { error: `${SOURCE}.shd already has a mask or the scriptable uniforms` };
  const out = [
    ...HEADER.split("\n"),
    ...lines.slice(0, mains[0]),
    ...DECLARATIONS.split("\n"),
    ...lines.slice(mains[0], outs[0]),
    CALL,
    ...lines.slice(outs[0]),
  ];
  return { text: out.join(eol) };
}

// The added lines taken out again: the game's text back (check_particle_mask.js: nothing else changed).
function unpatch(text) {
  const eol = text.includes("\r\n") ? "\r\n" : "\n";
  const lines = text.split(/\r?\n/);
  const header = HEADER.split("\n"), decl = DECLARATIONS.split("\n");
  if (lines.slice(0, header.length).join("\n") !== header.join("\n")) return null;
  const rest = lines.slice(header.length);
  const at = rest.findIndex((l, i) => rest.slice(i, i + decl.length).join("\n") === decl.join("\n"));
  if (at < 0) return null;
  const body = [...rest.slice(0, at), ...rest.slice(at + decl.length)].filter((l) => l !== CALL);
  return body.join(eol);
}

// For build_derived.js: `res` the game's resources (spell_bench\game_data.js resources(game)). -> { name, bytes } or
// { skipped: why }.
function derive({ res }) {
  const source = res(SOURCE, TYPE_SHD);
  if (!source) return { skipped: `the game has no ${SOURCE}.shd` };
  const made = patch(source.toString("latin1"));
  if (made.error) return { skipped: made.error };
  return { name: NAME + ".shd", bytes: Buffer.from(made.text, "latin1") };
}

module.exports = { NAME, SOURCE, TYPE_SHD, MARK, CALL, RING_DIRECTIONS, RING_FIRST, BOX_FIRST, BOXES, BOX_NUMBERS, patch, unpatch, derive };

if (require.main === module) {
  const argv = process.argv.slice(2);
  const option = (name, fallback) => {
    const i = argv.indexOf("--" + name);
    return i >= 0 && i + 1 < argv.length ? argv[i + 1] : fallback;
  };
  const out = option("out", "");
  if (!out) {
    console.error("particle_mask: --out <folder> [--game <folder>] [--tools <nwn5e-tools>]");
    process.exit(1);
  }
  const repo = path.resolve(__dirname, "..", "..");
  const tools = path.resolve(option("tools", process.env.NWN5E_TOOLS || path.join(repo, "..", "nwn5e-tools")));
  const game = require(path.join(tools, "anim_helper", "helper.js")).findGameDir({ game: option("game", ""), buildDir: path.join(repo, "build") });
  const made = derive({ res: require(path.join(tools, "spell_bench", "game_data.js")).resources(game) });
  if (made.skipped) {
    console.error("particle_mask: not made - " + made.skipped);
    process.exit(1);
  }
  fs.writeFileSync(path.join(path.resolve(out), made.name), made.bytes);
  console.log(`particle_mask: written ${path.join(path.resolve(out), made.name)}`);
}
