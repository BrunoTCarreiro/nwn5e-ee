#!/usr/bin/env node
// The particle mask's shader (#184's research spike; the owner, 2026-11-23: "is there a way to make a "mask" effect? an
// effect that makes particles from other effects disappear", then "run the research spike now"): fsn5pmask.shd, made
// at build time from the player's own game - its particle fragment shader, fsparticle.shd, with a mask added - into the
// build's data\files (build_derived.js calls derive). Never committed: it is the game's shader text.
//
// - Why the game's own: an emitter whose texture is nwn5e's material n5_pmask (data\files\n5_pmask.mtr: the game's
//   vsparticle by name, this shader, the game's flame texture by name) draws with this instead of fsparticle. Made from
//   fsparticle itself, it draws exactly as the game's particles do - the same texture, colour, alpha cut, soft edges and
//   fog - and follows a game update that changes them.
// - What is added, and nothing else changed: the game's scriptable uniforms (its own include, by name), the view
//   position's varying when the game's defines haven't declared it, the mask's three functions before main, and one
//   call to it just before main's last line (gl_FragColor = FragmentColor). The mask is off while scriptableVec1.w is 0 -
//   every uniform's default - and then returns at once: the shader draws exactly as fsparticle.
// - The mask (src/rules/particle_mask.h packs its uniforms; rules::ParticleMasked is the same test on the CPU, checked
//   against the cover rule's line): the fragment's place in the area, from its view position by the inverse view matrix
//   (as the game's own target highlight finds it); dropped - its colour cleared and discarded - when the line from the
//   origin (scriptableVec1.xyz) to it passes through one of the boxes (scriptableVec1.w of them, two vectors each from
//   scriptableVec2: the centre and the box's +X's x, the half sizes and its +X's y).
// - Anchors: the game's text must have one "void main" line and one "gl_FragColor = FragmentColor;" line. If it hasn't
//   (a game update rewrote it), nothing is made and the build says so; !maskproof then says the shader is missing.
//
//   node tools\data\particle_mask.js --out <folder> [--game <folder>] [--tools <nwn5e-tools>]   (by hand; build.bat runs
//   build_derived.js, which calls derive)
"use strict";
const fs = require("fs");
const path = require("path");

const NAME = "fsn5pmask";
const SOURCE = "fsparticle";
const TYPE_SHD = 2069;  // the game's shaders' resource type (base_shaders.bif)
const MARK = "// fsn5pmask.shd - nwn5e's masked particle shader (nwn5e-ee #184's spike), made at build time by tools/data/particle_mask.js";

const HEADER = [
  MARK,
  "// from the player's own game's fsparticle.shd (below, as the game has it) with nwn5e's mask added: the lines marked",
  "// nwn5e. Not committed anywhere: it is the game's text. Change particle_mask.js and rebuild, not this.",
  "",
].join("\n");

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
  "// nwn5e: whether the line from o to p meets the box: a its centre and its +X's x, b its half sizes and its +X's y.",
  "bool N5Shadowed(vec3 o, vec3 p, vec4 a, vec4 b)",
  "{",
  "    vec2 xa = vec2(a.w, b.w);",
  "    vec2 ya = vec2(-xa.y, xa.x);",
  "    vec2 ro = o.xy - a.xy;",
  "    vec2 rp = p.xy - a.xy;",
  "    vec3 lo = vec3(dot(ro, xa), dot(ro, ya), o.z - a.z);",
  "    vec3 lp = vec3(dot(rp, xa), dot(rp, ya), p.z - a.z);",
  "    vec3 d = lp - lo;",
  "    float t0 = 0.0;",
  "    float t1 = 1.0;",
  "    return N5Slab(lo.x, d.x, b.x, t0, t1) && N5Slab(lo.y, d.y, b.y, t0, t1) && N5Slab(lo.z, d.z, b.z, t0, t1);",
  "}",
  "",
  "// nwn5e: off (scriptableVec1.w 0, the default) it returns at once; on, a fragment behind or inside a box is dropped.",
  "void N5ParticleMask(inout vec4 colour)",
  "{",
  "    if (scriptableVec1.w < 0.5)",
  "        return;",
  "    vec3 p = (m_view_inv * vec4(vPosView, 1.0)).xyz;",
  "    vec3 o = scriptableVec1.xyz;",
  "    float n = scriptableVec1.w;",
  "    bool hidden = N5Shadowed(o, p, scriptableVec2, scriptableVec3)",
  "        || (n > 1.5 && N5Shadowed(o, p, scriptableVec4, scriptableVec5))",
  "        || (n > 2.5 && N5Shadowed(o, p, scriptableVec6, scriptableVec7))",
  "        || (n > 3.5 && N5Shadowed(o, p, scriptableVec8, scriptableVec9))",
  "        || (n > 4.5 && N5Shadowed(o, p, scriptableVec10, scriptableVec11))",
  "        || (n > 5.5 && N5Shadowed(o, p, scriptableVec12, scriptableVec13))",
  "        || (n > 6.5 && N5Shadowed(o, p, scriptableVec14, scriptableVec15));",
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

// The added lines taken out again: the game's text back (particle_mask_check.js: nothing else changed).
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

module.exports = { NAME, SOURCE, TYPE_SHD, MARK, CALL, patch, unpatch, derive };

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
