#!/usr/bin/env node
// #172's destruction effects' check (DE):
//   node tools\data\debris\check.js [--tools <nwn5e-tools>] [--reader <mdlmesh.exe>] [--game <folder>]
//
// Builds the effects twice in memory (build.js's build(), through the VFX Bench's core.js) and checks, writing nothing:
// - the eleven effects: n5_db_<wood|stone|metal|cloth|glass>_<s|l> then n5_db_light, rows 2248-2258 in that order, names
//   under 17 letters starting n5_ (the bench's nwn5e tab lists served n5_*.mdl with emitters); one-shots (F) on the root;
//   the same drafts on both builds;
// - the game's recipes, stated here and compared emitter by emitter (each from the game model named): the chunks
//   ("ChunkyWood", PLC_X01) - Explosion, life 0.75 s, velocity 8 (0.3), mass 3, bouncing at 0.5, inherit, spread 1.222
//   from 89 x 40 cm (the glass shards' 2.967 from the bottles' 5 x 5 cm and the crystal ball's 22 x 24 cm); the rocks
//   (vwp_m_chunk_rock "medrock": life 4 s, velocity 3, spread 4.189, mass 2, bouncing at 0.2); the dust puff ("fire!":
//   10, size 4 -> 1, life 2.5 s, turned 90 degrees about x) in each material's own colours; metal's sparks (vwp_spark);
//   glass's glint (px2_d01 fire!175);
// - each material's chunks: wood plc_chunk_w01 / w02; stone plc_chunk_s01 and fx_rockchunksml (large: also s02 and
//   fx_rockchunkmed); metal and cloth plc_chunk_m01; glass dag_chunk_g01 (large: also px2_gls); every emitter a burst at
//   0; a small one's emitters 0.25-0.7 m up, a large one's adding more from 0.9 m up and throwing more;
// - the light's death: an ember burst (vim_explflame's sparks, slower), rising embers (Lighten, mass below 0, yellow to
//   deep orange, on 1.2 s), the smoke column (the brazier's fire!06, alpha 0.3, on 2 s), no chunk; one light - the game's
//   flame light's colour and reach (vim_exp2flame: 0.945 0.741 0.42, 5 m) on at once (fading off), lighting its
//   surroundings (not ambient only - the game's is: ours on purpose), its colour keys from it to black at 1.5 s, never
//   brighter on the way; each on the light's point - the burst, the embers and the light within 5 cm of it, the smoke
//   10 cm over it (fix round 1: a burst or embers lifted 1 m passed before);
// - data\rows\visualeffects.debris.txt: the 11 rows naming the models in the four root columns, F, OrientWithGround 0;
// - tools\data\vfx holds exactly the drafts, and the bench reads each back byte for byte (core.readDraft / writeDraft);
// - data\files holds exactly the models the game's writer makes from them (mdlmesh --model; text whatever its line ends)
//   - and no other n5_db_ file;
// - on the bench's stage (its own sim.js and view.js): each chunk emitter bursts its count at once; by the effect's end
//   nothing is left (one-shots: no particle outlives the model); the light's death's embers rise, its smoke climbs past
//   1 m, its light is warm at 0 and black from 1.5 s;
// - with the game (skipped without one): every chunk model the effects throw is the game's - read from its install as
//   the bench's library export reads a chunk (mdlmesh: compiled through ReadVfxBinaryModel with any mesh, ASCII through
//   ReadVfxAsciiModel) - and the stage draws it as its real meshes (view.js chunkModel / chunkMeshes: triangles, each
//   mesh's texture, one copy per particle where the particle is), never as the stand-in cube.
// Exit 0: all pass; 1: a failure (each printed).

"use strict";
const fs = require("fs");
const os = require("os");
const path = require("path");
const { execFileSync } = require("child_process");

const ROOT = path.resolve(__dirname, "..", "..", "..");
const FILES = path.join(ROOT, "data", "files");
const DRAFTS = path.join(ROOT, "tools", "data", "vfx");
const ROWS = path.join(ROOT, "data", "rows", "visualeffects.debris.txt");
let failures = 0;
const fail = (what) => {
  ++failures;
  console.log("FAIL " + what);
};
const ok = (what) => console.log("ok   " + what);
const lf = (file) => fs.readFileSync(file, "utf8").replace(/\r/g, "");
const near = (a, b, tol = 1e-4) => Math.abs(a - b) <= tol;
const nearV = (a, b, tol = 1e-4) => Array.isArray(a) && a.length === b.length && a.every((x, i) => near(x, b[i], tol));

let B, T;
try {
  B = require("./build.js");
  T = B.tools(process.argv.slice(2));
} catch (e) {
  fail("build.js and nwn5e-tools' core.js load (--tools): " + e.message);
  console.log("\n" + failures + " failed");
  process.exit(1);
}
const core = T.core;

// --- what the effects are, stated here -------------------------------------------------------------------------------
const MODELS = ["n5_db_wood_s", "n5_db_wood_l", "n5_db_stone_s", "n5_db_stone_l", "n5_db_metal_s", "n5_db_metal_l",
  "n5_db_cloth_s", "n5_db_cloth_l", "n5_db_glass_s", "n5_db_glass_l", "n5_db_light"];
const FIRST_ROW = 2248;
const CHUNKS = {
  n5_db_wood_s: ["plc_chunk_w01", "plc_chunk_w02"], n5_db_wood_l: ["plc_chunk_w01", "plc_chunk_w02"],
  n5_db_stone_s: ["fx_rockchunksml", "plc_chunk_s01"], n5_db_stone_l: ["fx_rockchunkmed", "fx_rockchunksml", "plc_chunk_s01", "plc_chunk_s02"],
  n5_db_metal_s: ["plc_chunk_m01"], n5_db_metal_l: ["plc_chunk_m01"], n5_db_cloth_s: ["plc_chunk_m01"], n5_db_cloth_l: ["plc_chunk_m01"],
  n5_db_glass_s: ["dag_chunk_g01"], n5_db_glass_l: ["dag_chunk_g01", "px2_gls"], n5_db_light: [],
};
const DUST = {
  wood: [[0.443, 0.388, 0.247], [0.639, 0.608, 0.482]], stone: [[0.475, 0.439, 0.365], [0.82, 0.796, 0.71]],
  metal: [[0.608, 0.569, 0.486], [0.82, 0.816, 0.796]], cloth: [[0.475, 0.455, 0.408], [0.82, 0.804, 0.745]],
  glass: [[0.518, 0.576, 0.608], [0.82, 0.816, 0.796]],
};
const WARM = [0.945, 0.741, 0.42];
// A recipe's fields as the game has them; `area` [x, y] cm.
const same = (e, want) => Object.entries(want).filter(([k, v]) => (Array.isArray(v) ? !nearV(e[k], v) : typeof v === "number" ? !near(e[k], v) : e[k] !== v))
  .map(([k, v]) => k + " " + JSON.stringify(e[k]) + " not " + JSON.stringify(v));
const CHUNK = { update: "Explosion", render: "Normal", blend: "Normal", life: 0.75, velocity: 8, randvel: 0.3, mass: 3, bounce: true, bounce_co: 0.5, inherit: true };
const ROCK = { update: "Explosion", life: 4, velocity: 3, randvel: 0.5, spread: 4.189, mass: 2, particle_rot: 1, bounce: true, bounce_co: 0.2, xsize: 20, ysize: 20 };
const PUFF = { update: "Explosion", render: "Normal", blend: "Normal", texture: "fxpa_smoke01", grid: 4, frame_end: 24, fps: 10, life: 2.5,
  velocity: 2, randvel: 0.5, spread: 2.967, mass: 0.09, particle_rot: 0.5, size_start: 4, size_end: 1, alpha_start: 1, alpha_end: 0,
  orientation: [1, 0, 0, 1.5708], inherit: true };
const SPARK = { update: "Explosion", render: "Motion_Blur", blend: "Lighten", texture: "spark", life: 3, velocity: 3, randvel: 1,
  spread: 6.283, mass: 1.5, size_start: 0.015, size_end: 0.01, color_start: [0.996, 1, 0.824], color_end: [0.788, 0.463, 0.039],
  alpha_start: 1, alpha_end: 0.02, bounce: true, bounce_co: 0.3, blur_length: 0.07, inherit_local: true };
const GLINT = { update: "Explosion", render: "Normal", blend: "Lighten", texture: "fxpa_strk_b", birthrate: 10, life: 1, velocity: 2,
  spread: 2.967, size_start: 0.5, size_end: 0, color_start: [1, 1, 1], color_end: [0, 0.682, 1], alpha_start: 1, alpha_end: 0 };

// --- the effects, twice ----------------------------------------------------------------------------------------------
let built = [];
{
  const what = [];
  try {
    built = B.build(core);
    const again = B.build(core);
    if (JSON.stringify(built.map((e) => e.draft)) !== JSON.stringify(again.map((e) => e.draft))) what.push("two builds differ");
  } catch (e) {
    what.push("build() throws: " + e.message);
  }
  const names = built.map((e) => e.name);
  if (JSON.stringify(names) !== JSON.stringify(MODELS)) what.push("the effects " + names.join(" ") + ", not " + MODELS.join(" "));
  built.forEach((e, i) => {
    if (e.row !== FIRST_ROW + i) what.push(e.name + " row " + e.row);
    if (!/^n5_[a-z0-9_]{1,13}$/.test(e.name)) what.push(e.name + " not an n5_ resref under 17 letters");
    if (e.fx.duration || e.fx.attach !== "root") what.push(e.name + " not a one-shot (F) on the root");
  });
  if (what.length) fail("the effects: " + what.join("; "));
  else ok("11 effects, rows 2248-2258 in the call's order, n5_ resrefs, one-shots on the root; the same drafts twice");
}
const byName = new Map(built.map((e) => [e.name, e]));

// --- the game's recipes ---------------------------------------------------------------------------------------------
{
  const what = [];
  for (const name of MODELS.slice(0, 10)) {
    const e = byName.get(name);
    if (!e) continue;
    const material = name.split("_")[2], large = name.endsWith("_l");
    const thrown = new Set(), problems = [];
    let count = 0, low = Infinity, high = 0, spark = false, glint = false, puffs = 0;
    for (const x of e.fx.emitters) {
      const z = x.position[2];
      low = Math.min(low, z);
      high = Math.max(high, z);
      if (x.start_seconds || x.emit_seconds) problems.push(x.name + " not a burst at 0");
      if (x.chunk) {
        thrown.add(x.chunk);
        count += x.birthrate;
        if (x.chunk.startsWith("fx_rockchunk")) problems.push(...same(x, ROCK).map((s) => x.name + " " + s));
        else {
          problems.push(...same(x, CHUNK).map((s) => x.name + " " + s));
          const want = x.chunk === "dag_chunk_g01" ? { spread: 2.967, xsize: 5, ysize: 5 } : x.chunk === "px2_gls" ? { spread: 2.967, xsize: 22, ysize: 24 }
            : { spread: 1.222, xsize: 89, ysize: 40 };
          problems.push(...same(x, want).map((s) => x.name + " " + s));
        }
      } else if (x.texture === "fxpa_smoke01") {
        ++puffs;
        problems.push(...same(x, Object.assign({}, PUFF, { color_start: DUST[material][0], color_end: DUST[material][1] })).map((s) => x.name + " " + s));
        const area = material === "glass" ? { birthrate: 5, xsize: 51, ysize: 51 } : { birthrate: 10, xsize: 100, ysize: 99 };
        problems.push(...same(x, area).map((s) => x.name + " " + s));
      } else if (x.texture === "spark") {
        spark = true;
        problems.push(...same(x, SPARK).map((s) => x.name + " " + s));
      } else if (x.texture === "fxpa_strk_b") {
        glint = true;
        problems.push(...same(x, GLINT).map((s) => x.name + " " + s));
      } else problems.push(x.name + " an emitter of no recipe (" + x.texture + ")");
    }
    if ([...thrown].sort().join(" ") !== CHUNKS[name].join(" ")) problems.push("throws " + [...thrown].sort().join(" "));
    if (spark !== (material === "metal")) problems.push(spark ? "sparks off metal" : "no sparks");
    if (glint !== (material === "glass")) problems.push(glint ? "a glint off glass" : "no glint");
    if (puffs < 1 || (large && puffs < 2)) problems.push(puffs + " puffs");
    if (e.fx.lights.length || (e.fx.planes || []).length) problems.push("a light or a plane");
    if (!large && (low < 0.25 || high > 0.7)) problems.push("small, emitters at " + low + "-" + high + " m");
    if (large) {
      const small = byName.get(name.replace(/_l$/, "_s"));
      const smallCount = small ? small.fx.emitters.reduce((n, x) => n + (x.chunk ? x.birthrate : 0), 0) : 0;
      if (high < 0.9) problems.push("large, its highest emitter at " + high + " m");
      if (!(count > smallCount)) problems.push("large, " + count + " chunks, the small one " + smallCount);
    }
    if (problems.length) what.push(name + ": " + problems.join("; "));
  }
  if (what.length) fail("the debris recipes: " + what.join(" | "));
  else ok("10 debris effects on the game's recipes: chunks (ChunkyWood), rocks (medrock), the material's dust (fire!), metal's sparks (vwp_spark), glass's glint (px2_d01); each material's chunks; bursts at 0; small 0.25-0.7 m, large higher and more");
}
{
  const e = byName.get("n5_db_light"), what = [];
  if (e) {
    const em = (n) => e.fx.emitters.find((x) => x.name === n);
    const burst = em("burst"), embers = em("embers"), smoke = em("smoke");
    if (!burst || !embers || !smoke || e.fx.emitters.length !== 3) what.push("not burst, embers and smoke");
    if (burst) what.push(...same(burst, { update: "Explosion", render: "Motion_Blur", blend: "Lighten", texture: "fxpa_strk_b", spread: 6.283,
      color_start: [0.925, 0.592, 0.165], color_end: [1, 0.706, 0.071], blur_length: 0.8 }).map((s) => "burst " + s));
    if (burst && !(burst.velocity < 10 && burst.life > 0.1 && burst.life <= 0.5)) what.push("burst not slower and longer than the flame's hit");
    if (embers) what.push(...same(embers, { update: "Fountain", blend: "Lighten", texture: "fxpa_flare", color_start: [1, 0.871, 0], color_end: [0.92, 0.44, 0.11],
      alpha_start: 1, alpha_end: 0, emit_seconds: 1.2 }).map((s) => "embers " + s));
    if (embers && !(embers.mass < 0 && embers.velocity > 0 && embers.velocity < 3 && embers.size_end < embers.size_start)) what.push("embers don't drift up, shrinking");
    if (smoke) what.push(...same(smoke, { update: "Fountain", blend: "Normal", texture: "fxpa_smoke01", grid: 4, frame_end: 24, fps: 10, birthrate: 20, life: 3,
      velocity: 1, randvel: 0.2, spread: 0.175, mass: 0.035, size_start: 1, size_end: 2, color_start: [0.376, 0.376, 0.376],
      color_end: [0.176, 0.176, 0.176], alpha_start: 0.3, alpha_end: 0, emit_seconds: 2 }).map((s) => "smoke " + s));
    if (e.fx.emitters.some((x) => x.chunk)) what.push("a chunk");
    // Where each sits (fix round 1, the review: a burst or embers lifted 1 m passed): on the light's point - the burst,
    // the embers and the glow within 5 cm of it, the smoke 10 cm over it; nothing under a node that could move them.
    const at = (item, z) => item && Array.isArray(item.position) && near(item.position[0], 0, 0.05) && near(item.position[1], 0, 0.05) &&
      near(item.position[2], z, 0.05) && !item.parent;
    for (const [item, z, name] of [[burst, 0, "burst"], [embers, 0, "embers"], [smoke, 0.1, "smoke"], [e.fx.lights[0], 0, "light"]]) {
      if (item && !at(item, z)) what.push(name + " at " + JSON.stringify(item.position) + (item.parent ? " under " + item.parent : "") + ", not 0 0 " + z);
    }
    if ((e.fx.nodes || []).length) what.push("nodes");
    const l = e.fx.lights[0];
    if (e.fx.lights.length !== 1 || !l) what.push(e.fx.lights.length + " lights");
    else {
      what.push(...same(l, { color: WARM, radius: 5, multiplier: 1, fading: false, ambientonly: false, shadow: false }).map((s) => "light " + s));
      const keys = (l.keys || []).find((k) => k.anim === "impact"), c = keys && keys.color;
      if (!c || c.length < 8 || c.length % 4) what.push("no colour keys");
      else {
        if (!near(c[0], 0) || !nearV(c.slice(1, 4), WARM)) what.push("its keys don't start at its colour");
        const last = c.length - 4;
        if (!near(c[last], 1.5) || c.slice(last + 1).some((v) => v !== 0)) what.push("not black at 1.5 s");
        for (let i = 4; i < c.length; i += 4) {
          if (!(c[i] > c[i - 4])) what.push("key times not rising");
          for (let k = 1; k <= 3; ++k) if (c[i + k] > c[i - 4 + k]) what.push("brighter at " + c[i] + " s");
        }
      }
    }
  } else what.push("missing");
  if (what.length) fail("n5_db_light: " + what.join("; "));
  else ok("n5_db_light: an ember burst, rising embers, the brazier's smoke column on 2 s, the game's flame light on at once fading to black at 1.5 s - each on the light's point (the smoke 10 cm over it)");
}

// --- the rows file ----------------------------------------------------------------------------------------------------
{
  const what = [];
  const lines = lf(ROWS).split("\n").filter((l) => l.trim() && !l.trim().startsWith("#"));
  const header = lines[0].trim().split(/\s+/);
  if (header.join(" ") !== "Label Type_FD OrientWithGround Imp_Root_S_Node Imp_Root_M_Node Imp_Root_L_Node Imp_Root_H_Node") what.push("header " + header.join(" "));
  const rows = lines.slice(1).map((l) => l.trim().split(/\s+/));
  if (rows.length !== 11) what.push(rows.length + " rows");
  rows.forEach((w, i) => {
    const name = MODELS[i];
    const label = i === 10 ? "NWN5E_LIGHT_DEATH" : "NWN5E_DEBRIS_" + name.split("_")[2].toUpperCase() + (name.endsWith("_l") ? "_L" : "_S");
    const want = [String(FIRST_ROW + i), label, "F", "0", name, name, name, name];
    if (w.join(" ") !== want.join(" ")) what.push("row " + (i + 1) + ": " + w.join(" "));
  });
  if (what.length) fail("visualeffects.debris.txt: " + what.join("; "));
  else ok("visualeffects.debris.txt: rows 2248-2258, F, flat, each model in the four root columns");
}

// --- the drafts and the models --------------------------------------------------------------------------------------
{
  const what = [];
  for (const e of built) {
    const file = path.join(DRAFTS, e.name + ".vfx");
    if (!fs.existsSync(file)) what.push(e.name + ".vfx missing");
    else if (lf(file) !== e.draft) what.push(e.name + ".vfx not the build's");
    if (core.writeDraft(core.readDraft(e.draft)) !== e.draft) what.push(e.name + ".vfx not read back byte for byte by the bench");
  }
  const strays = fs.readdirSync(DRAFTS).filter((f) => /^n5_db_/i.test(f) && !MODELS.includes(f.replace(/\.vfx$/i, "")));
  if (strays.length) what.push("other drafts: " + strays.join(" "));
  if (what.length) fail("tools/data/vfx: " + what.join("; "));
  else ok("tools/data/vfx: the 11 drafts, each the build's and read back by the bench byte for byte");
}
let reader = null;
try {
  const R = require("../floatdisk/reader.js");
  reader = R.ensureReader({ repo: ROOT, reader: T.option("reader", process.env.NWN5E_MDLMESH || ""), log: () => {} });
} catch (e) {
  fail("mdlmesh (the game's model writer and reader): " + e.message);
}
if (reader) {
  const what = [];
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "n5_debris-"));
  try {
    for (const e of built) {
      const draft = path.join(tmp, e.name + ".vfx");
      fs.writeFileSync(draft, e.draft);
      const file = path.join(FILES, e.name + ".mdl");
      if (!fs.existsSync(file)) what.push(e.name + ".mdl missing");
      else if (lf(file) !== B.modelOf(reader, draft, e.name)) what.push(e.name + ".mdl not the game's writer's from its draft");
    }
  } finally {
    fs.rmSync(tmp, { recursive: true, force: true });
  }
  const strays = fs.readdirSync(FILES).filter((f) => /^n5_db_/i.test(f) && !MODELS.map((m) => m + ".mdl").includes(f.toLowerCase()));
  if (strays.length) what.push("other files: " + strays.join(" "));
  if (what.length) fail("data/files: " + what.join("; "));
  else ok("data/files: the 11 models, each the game's writer's from its draft (mdlmesh --model), no other n5_db_ file");
}

// --- the bench's stage ------------------------------------------------------------------------------------------------
let sim = null, V = null;
try {
  sim = require(path.join(T.dir, "vfx_bench", "public", "sim.js"));
  V = require(path.join(T.dir, "vfx_bench", "public", "view.js"));
} catch (e) {
  fail("the bench's sim.js and view.js load: " + e.message);
}
const play = (draft) => {
  const s = new sim.Sim();
  s.setEffect(core.readDraft(draft));
  s.restart();
  return s;
};
const stepTo = (s, t) => { while (s.time < t - 1e-9) s.step(Math.min(1 / 30, t - s.time)); };
if (sim) {
  const what = [];
  for (const e of built) {
    const s = play(e.draft);
    stepTo(s, 0.04);
    s.fx.emitters.forEach((x, i) => {
      if (String(x.update).toLowerCase() !== "explosion") return;
      if (s.states[i].particles.length !== Math.trunc(x.birthrate)) what.push(e.name + " " + x.name + " burst " + s.states[i].particles.length + ", not " + Math.trunc(x.birthrate));
    });
    stepTo(s, s.anims().impact + 0.05);
    const left = s.states.reduce((n, st) => n + st.particles.length, 0);
    if (left) what.push(e.name + ": " + left + " particles outlive its " + s.anims().impact + " s");
  }
  {
    const e = byName.get("n5_db_light");
    const s = play(e.draft);
    stepTo(s, 0.8);
    const embers = s.states[s.fx.emitters.findIndex((x) => x.name === "embers")].particles;
    if (!embers.length || !(embers.reduce((z, p) => z + p.pos[2], 0) / embers.length > 0.2)) what.push("the embers don't rise");
    stepTo(s, 2.6);
    const smoke = s.states[s.fx.emitters.findIndex((x) => x.name === "smoke")].particles;
    if (!smoke.length || !(Math.max(...smoke.map((p) => p.pos[2])) > 1)) what.push("the smoke doesn't climb past 1 m");
    const light = (t) => (s.lightsAt(t)[0] || {}).color || [];
    if (!nearV(light(0), WARM)) what.push("its light at 0 " + JSON.stringify(light(0)));
    const mid = light(0.75);
    if (!(mid[0] > 0 && mid[0] < WARM[0] && mid[2] < mid[0])) what.push("its light at 0.75 s " + JSON.stringify(mid));
    for (const t of [1.5, 2, 4]) if (light(t).some((v) => v > 1e-6)) what.push("its light at " + t + " s " + JSON.stringify(light(t)));
  }
  if (what.length) fail("on the bench's stage: " + what.join("; "));
  else ok("on the bench's stage (sim.js): every chunk emitter bursts its count at once, nothing outlives an effect; the embers rise, the smoke climbs past 1 m, the light warm at 0 and black from 1.5 s");
}

// --- with the game: the chunks are the game's, drawn as their meshes ----------------------------------------------------
if (sim && V && reader) {
  let game = null, res = null;
  try {
    const W = require("../floatdisk/files.js");
    const tools = W.loadTools({ tools: T.dir, game: T.option("game", ""), build: path.join(ROOT, "build") });
    game = tools.game;
    if (game && fs.existsSync(path.join(game, "data", "nwn_base.key"))) res = tools.gameData.resources(game);
  } catch (e) {
    game = null;
  }
  if (!res) console.log("skip the game's chunk models (no game: --game <folder>, NWN_GAME_DIR or Steam's folder)");
  else {
    const what = [], seen = [];
    const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "n5_debris-"));
    try {
      const names = [...new Set(Object.values(CHUNKS).flat())].sort();
      const models = new Map();
      for (const name of names) {
        const bytes = res(name, 2002);
        if (!bytes) { what.push(name + " not in the game"); continue; }
        const file = path.join(tmp, name + ".mdl");
        fs.writeFileSync(file, bytes);
        let text;
        try {
          text = execFileSync(reader, [file], { encoding: "utf8", windowsHide: true }).replace(/\r/g, "");
        } catch (e) {
          what.push(name + " not read: " + String(e.stderr || e.message).trim());
          continue;
        }
        const model = V.chunkModel(core.readDraft(text));
        if (!model || !model.triangles || !model.meshes.length || model.meshes.some((m) => !m.bitmap)) {
          what.push(name + " no textured triangles (" + (model ? model.triangles : 0) + ")");
          continue;
        }
        models.set(name, model);
        seen.push(name + " " + model.triangles + " (" + [...new Set(model.meshes.map((m) => String(m.bitmap).toLowerCase()))].join(" ") + ")");
      }
      // Each effect on the stage a moment after its burst: every chunk particle drawn as its model's triangles, where it is.
      for (const e of built) {
        const s = play(e.draft);
        stepTo(s, 0.1);
        for (const entry of s.snapshot()) {
          const x = entry.emitter;
          if (!x.chunk || !models.has(x.chunk)) continue;
          const model = models.get(x.chunk), meshes = V.chunkMeshes(model, entry);
          const floats = meshes.reduce((n, m) => n + m.data.length, 0);
          if (floats !== entry.particles.length * model.triangles * 27) what.push(e.name + " " + x.name + ": " + floats + " floats for " + entry.particles.length + " particles");
          for (const m of meshes) {
            for (let i = 0; i + 8 < m.data.length; i += 9) {
              if (![0, 1, 2].every((k) => Number.isFinite(m.data[i + k]))) { what.push(e.name + " " + x.name + " a vertex not a number"); break; }
            }
          }
          const far = entry.particles.some((p, k) => {
            const m = meshes[0], base = k * model.meshes[0].triangles * 27;
            return m && Math.hypot(m.data[base] - p.pos[0], m.data[base + 1] - p.pos[1], m.data[base + 2] - p.pos[2]) > 1.5;
          });
          if (far) what.push(e.name + " " + x.name + " a copy away from its particle");
        }
      }
    } finally {
      fs.rmSync(tmp, { recursive: true, force: true });
    }
    if (what.length) fail("the game's chunk models on the stage: " + what.join("; "));
    else ok("the game's chunk models (" + game + "), read as the library export reads them and drawn as their meshes on the stage: " + seen.join(", "));
  }
}

console.log(failures ? "\n" + failures + " failed" : "\nall passed");
process.exit(failures ? 1 : 0);
