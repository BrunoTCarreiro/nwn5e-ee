#!/usr/bin/env node
// #172's destruction effects (DE; tools\data\debris\README.md): material-matched debris for a breakable whose model has
// none of its own, and a lit light's death -
//   node tools\data\debris\build.js [--tools <nwn5e-tools>] [--reader <mdlmesh.exe>]
//
// The owner, 2026-11-20 ("yes on all"), after the destruction recon: a placeable's debris is its own model's "die" (its
// Explosion emitters throw the game's chunk models on a detonate event); 71 breakable models have a "die" with no debris,
// 181 a smoke puff only, 491 no "die" at all; a lit light's light is cut at once. Eleven one-shot effects, each a VFX
// Bench draft and the model the game plays:
// - debris: wood, stone, metal (with sparks), cloth (dust and scraps) and glass, small and large - n5_db_<material>_<s|l>;
// - a lit light's death: ember sparks, a short rising smoke column and a warm light fading out over 1.5 s - n5_db_light.
// Every emitter starts from one of the game's own (each recipe below names the model and emitter it was read from, read
// out of the player's install - nothing of the game's is kept here); the chunks are the game's chunk models, thrown by
// name. The debris sits at the breakable's own location (its base, on the floor), so a small one's emitters are lifted
// to where the game's small pieces have theirs (0.25-0.7 m) and a large one adds the same again 0.6 m higher (the
// bookshelf's and the armoire's upper emitters); the light's death sits at its light's point, its emitters at 0.
//
// The drafts go to tools\data\vfx\<name>.vfx, written by the VFX Bench's own draft writer (nwn5e-tools'
// vfx_bench\public\core.js writeDraft - byte for byte the game's WriteVfxDraft, which tests/data/anim_debris_test.cpp
// checks); the models to data\files\<name>.mdl, written by the game's own model writer (src\data\vfx_model.cpp
// WriteVfxModel) through tools\data\floatdisk's mdlmesh --model (built from this checkout when missing or stale) - the
// Detect Magic way. The same bytes on every run (check.js checks it); it writes only these 22 files, and only those whose
// bytes changed (text compared without CRs: git checks them out CRLF).
//
//   --tools <folder>    nwn5e-tools (default: NWN5E_TOOLS, else beside this repo)
//   --reader <exe>      mdlmesh (default: NWN5E_MDLMESH, else build\floatdisk\mdlmesh.exe, built if needed)

"use strict";
const fs = require("fs");
const path = require("path");
const { execFileSync } = require("child_process");

const REPO = path.resolve(__dirname, "..", "..", "..");
const DRAFTS = path.join(REPO, "tools", "data", "vfx");
const FILES = path.join(REPO, "data", "files");

// --- what the call names (src/data/anim_debris.h) --------------------------------------------------------------------
const MATERIALS = ["wood", "stone", "metal", "cloth", "glass"];  // core's DebrisMaterial order
const SIZES = ["s", "l"];                                          // DebrisSize: Small, Large
const FIRST_ROW = 2248;                                            // kDebrisFirstRow; the light's death after the ten
const debrisModel = (m, s) => `n5_db_${MATERIALS[m]}_${SIZES[s]}`;
const LIGHT_MODEL = "n5_db_light";
const MODELS = [...MATERIALS.flatMap((_, m) => SIZES.map((__, s) => debrisModel(m, s))), LIGHT_MODEL];
const rowOf = (name) => FIRST_ROW + MODELS.indexOf(name);

// --- the game's recipes ------------------------------------------------------------------------------------------------
// The chunks: the game's placeable "die" debris - "ChunkyWood" on every one of its 525 models with chunks, here PLC_X01's
// (the bench) ChunkyWood190 / 191: an Explosion on the die's detonate, life 0.75 s, velocity 8 (randvel 0.3), spread
// 1.222, mass 3, bouncing at 0.5, from an 89 x 40 cm patch, carried with the model (inherit), the game's texture and
// sheet (a chunk draws no sprite: the model is the particle). particle_rot (its tumble) 5 or 2.9 as the game gives each.
const CHUNK = {
  update: "Explosion", render: "Normal", blend: "Normal", texture: "fxpa_cloud02", grid: 5, frame_end: 24,
  life: 0.75, velocity: 8, randvel: 0.3, spread: 1.222, mass: 3, size_start: 0.1, size_end: 0.1, alpha_start: 1, alpha_end: 1,
  xsize: 89, ysize: 40, bounce: true, bounce_co: 0.5, blur_length: 10, inherit: true,
};
// The rocks: VFX_COM_CHUNK_STONE's vwp_m_chunk_rock, its "medrock" (fx_rockchunkmed: they lie 4 s, thrown wide).
const ROCK = {
  update: "Explosion", render: "Normal", blend: "Normal", texture: "NULL", grid: 1, frame_end: 0,
  life: 4, velocity: 3, randvel: 0.5, spread: 4.189, mass: 2, particle_rot: 1, size_start: 0.1, size_end: 0.1,
  alpha_start: 1, alpha_end: 1, xsize: 20, ysize: 20, bounce: true, bounce_co: 0.2, blur_length: 10,
};
// The dust puff: the game's die smoke ("fire!" on the same models; PLC_X01's fire!117) - a burst of 10, size 4 -> 1 m,
// fading from full, life 2.5 s, turned on its side (90 degrees about x), its colour the material's (DUST below).
const PUFF = {
  update: "Explosion", render: "Normal", blend: "Normal", texture: "fxpa_smoke01", grid: 4, frame_end: 24, fps: 10,
  orientation: [1, 0, 0, 1.5708], birthrate: 10, life: 2.5, velocity: 2, randvel: 0.5, spread: 2.967, mass: 0.09,
  particle_rot: 0.5, size_start: 4, size_end: 1, alpha_start: 1, alpha_end: 0, xsize: 100, ysize: 99, blur_length: 10,
  inherit: true, twosidedtex: true,
};
// Each material's dust, as the game's own pieces of it tint their puff (colour start -> end).
const DUST = {
  wood: [[0.443, 0.388, 0.247], [0.639, 0.608, 0.482]],    // PLC_X01 (Bench) fire!117
  stone: [[0.475, 0.439, 0.365], [0.82, 0.796, 0.71]],      // PLC_B10 (Sarco1) fire!20
  metal: [[0.608, 0.569, 0.486], [0.82, 0.816, 0.796]],     // PLC_I05 (Brazier) fire!51 - the game's common grey puff
                                                            // (the bolt of cloth's fire!49, the campfire's, the candelabra's
                                                            // and the mirror's too): not a metal tint; its sparks tell it
  cloth: [[0.475, 0.455, 0.408], [0.82, 0.804, 0.745]],     // PLC_L03 (Sacks) fire!82
  glass: [[0.518, 0.576, 0.608], [0.82, 0.816, 0.796]],     // px2_d01 (CrystalBall) fire!165 - with its 5 and 51 x 51 cm
};
// Metal's sparks: VFX_COM_SPARKS_PARRY's vwp_spark "spark" - bright specks streaking (motion blur, added light), falling
// and bouncing for 3 s, white-yellow to orange.
const SPARK = {
  update: "Explosion", render: "Motion_Blur", blend: "Lighten", texture: "spark", grid: 1, frame_end: 0,
  birthrate: 10, life: 3, velocity: 3, randvel: 1, spread: 6.283, mass: 1.5, size_start: 0.015, size_end: 0.01,
  color_start: [0.996, 1, 0.824], color_end: [0.788, 0.463, 0.039], alpha_start: 1, alpha_end: 0.02,
  bounce: true, bounce_co: 0.3, blur_length: 0.07, inherit_local: true,
};
// Glass's glint: the crystal ball's (px2_d01) fire!175 - white streaks turning ice blue, gone in 1 s (one frame: the
// texture is a single streak).
const GLINT = {
  update: "Explosion", render: "Normal", blend: "Lighten", texture: "fxpa_strk_b", grid: 1, frame_end: 0,
  birthrate: 10, life: 1, velocity: 2, randvel: 0.5, spread: 2.967, mass: 0.09, particle_rot: 0.5, size_start: 0.5,
  size_end: 0, color_start: [1, 1, 1], color_end: [0, 0.682, 1], alpha_start: 1, alpha_end: 0, xsize: 14, ysize: 14,
  blur_length: 10, inherit: true,
};

// One emitter: a recipe with this effect's own name, height (z, metres above the effect's point) and changes.
const at = (recipe, name, z, more = {}) => Object.assign({}, recipe, { name, position: [0, 0, z] }, more);
const chunk = (name, model, z, birthrate, particle_rot, more = {}) =>
  at(CHUNK, name, z, Object.assign({ chunk: model, birthrate, particle_rot }, more));
const rock = (name, model, z, birthrate) => at(ROCK, name, z, { chunk: model, birthrate });
const puff = (material, name, z, more = {}) =>
  at(PUFF, name, z, Object.assign({ color_start: DUST[material][0], color_end: DUST[material][1] }, more));

// --- the debris ----------------------------------------------------------------------------------------------------------
// Small: the game's own small piece of that material, its emitters' heights rounded. Large: the small one's, then the
// same kind of debris again 0.6 m higher with the larger chunk where the game has one (the bookshelf, the armoire and
// the sarcophagus throw from 1.1-1.3 m).
const DEBRIS = {
  wood: {  // PLC_X01 (Bench): a puff, W02 x 1, W01 x 2
    s: () => [puff("wood", "dust", 0.45), chunk("w02", "plc_chunk_w02", 0.5, 1, 5), chunk("w01", "plc_chunk_w01", 0.6, 2, 2.9)],
    up: () => [puff("wood", "dust_up", 1.05), chunk("w02_up", "plc_chunk_w02", 1.1, 2, 5), chunk("w01_up", "plc_chunk_w01", 1.2, 2, 2.9)],
  },
  stone: {  // PLC_B10 (Sarco1): a puff, S01 x 3, S02 x 3 (above); vwp_m_chunk_rock's rocks
    s: () => [puff("stone", "dust", 0.45), chunk("s01", "plc_chunk_s01", 0.6, 3, 5), rock("gravel", "fx_rockchunksml", 0.5, 4)],
    up: () => [puff("stone", "dust_up", 1.05), chunk("s02_up", "plc_chunk_s02", 1.2, 3, 2.9), rock("rocks_up", "fx_rockchunkmed", 1.1, 2)],
  },
  metal: {  // PLC_I05 (Brazier): a puff, M01 x 3 (its S01 is its stone bowl's); vwp_spark's sparks
    s: () => [puff("metal", "dust", 0.5), chunk("m01", "plc_chunk_m01", 0.7, 3, 2.9), at(SPARK, "sparks", 0.6)],
    up: () => [puff("metal", "dust_up", 1.1), chunk("m01_up", "plc_chunk_m01", 1.3, 3, 2.9), at(SPARK, "sparks_up", 1.2, { birthrate: 15 })],
  },
  cloth: {  // PLC_I03 (BltOfCloth): a puff alone; PLC_L03 (Sacks, large): a puff and M01 x 3 - the scraps
    s: () => [puff("cloth", "dust", 0.25), chunk("scraps", "plc_chunk_m01", 0.4, 1, 2.9)],
    l: () => [puff("cloth", "dust", 0.3), chunk("scraps", "plc_chunk_m01", 0.45, 3, 2.9)],
    up: () => [puff("cloth", "dust_up", 0.9)],
  },
  glass: {  // dag_btl* (the bottles): dag_chunk_g01 x 12 from a 5 x 5 cm patch, spread 2.967; px2_d01 (CrystalBall): PX2_GLS x 4,
            // its glint and its blue-grey puff
    s: () => [chunk("shards", "dag_chunk_g01", 0.37, 12, 5, { spread: 2.967, xsize: 5, ysize: 5 }), at(GLINT, "glint", 0.33),
      puff("glass", "dust", 0.38, { birthrate: 5, xsize: 51, ysize: 51 })],
    l: () => [chunk("gls", "px2_gls", 0.4, 4, 5, { spread: 2.967, xsize: 22, ysize: 24 }),
      chunk("shards", "dag_chunk_g01", 0.37, 12, 5, { spread: 2.967, xsize: 5, ysize: 5 }), at(GLINT, "glint", 0.33),
      puff("glass", "dust", 0.38, { birthrate: 5, xsize: 51, ysize: 51 })],
    up: () => [chunk("gls_up", "px2_gls", 1.0, 4, 5, { spread: 2.967, xsize: 22, ysize: 24 }),
      chunk("shards_up", "dag_chunk_g01", 1.0, 12, 5, { spread: 2.967, xsize: 5, ysize: 5 }),
      puff("glass", "dust_up", 1.0, { birthrate: 5, xsize: 51, ysize: 51 })],
  },
};

// --- a lit light's death -------------------------------------------------------------------------------------------------
// At the light's own point (the game session places it there), its emitters at 0:
// - a burst of ember sparks: VFX_IMP_FLAME_S's vim_explflame "sparks" (orange streaks every way, motion-blurred, added
//   light), slower and shorter than a flame's hit (velocity 10 -> 5, life 0.1 -> 0.25 s, 30 -> 20);
// - rising embers: VFX_IMP_FLAME_M's vim_exp2flame "sparks01" (fxpa_flare, added light, from yellow), drifting up as
//   vim_explflame's smoke rises (mass -0.1) instead of spitting (velocity 7 -> 1.2), smaller and shrinking (0.1 -> 0.04
//   m), cooling to the deep orange of the game's fire light (vim_strkfire's 0.92 0.44 0.11), 12 a second for 1.2 s;
// - the smoke column: the brazier's own (PLC_I05 fire!06: 20 a second, rising 1 m/s in a narrow column, growing 1 -> 2
//   m, grey to dark, life 3 s), thicker as the fire chokes (alpha 0.1 -> 0.3 at the start), on for 2 s;
// - the glow: VFX_IMP_FLAME_M's light colour and reach (vim_exp2flame AuroraLight01: 0.945 0.741 0.42, 5 m) in place of
//   the light the client cuts as the placeable dies - on at once (fadingLight off), cooling to an ember red as it fades,
//   dark at 1.5 s. Unlike the game's (ambient-only, not dynamic) it lights its surroundings and creatures (ambientonly
//   off, dynamic on - core.light's own), as the placeable's light did: a choice, not the game's value.
// Every emitter and the glow sit on the light's point (the smoke 10 cm over it); check.js and the tests pin them.
function lightDeath(core) {
  const emitters = [
    core.emitter({ name: "burst", update: "Explosion", render: "Motion_Blur", blend: "Lighten", texture: "fxpa_strk_b", grid: 1,
      frame_end: 0, birthrate: 20, life: 0.25, velocity: 5, randvel: 1, spread: 6.283, mass: 0, size_start: 0.12, size_end: 0,
      color_start: [0.925, 0.592, 0.165], color_end: [1, 0.706, 0.071], alpha_start: 1, alpha_end: 0.7, blur_length: 0.8,
      deadspace: 0.175, inherit: true }),
    core.emitter({ name: "embers", update: "Fountain", render: "Normal", blend: "Lighten", texture: "fxpa_flare", grid: 1,
      frame_end: 0, birthrate: 12, emit_seconds: 1.2, life: 1.5, velocity: 1.2, randvel: 0.6, spread: 0.8, mass: -0.1,
      size_start: 0.1, size_end: 0.04, color_start: [1, 0.871, 0], color_end: [0.92, 0.44, 0.11], alpha_start: 1, alpha_end: 0,
      xsize: 30, ysize: 30, blur_length: 10, inherit_local: true }),
    core.emitter({ name: "smoke", update: "Fountain", render: "Normal", blend: "Normal", texture: "fxpa_smoke01", grid: 4,
      frame_end: 24, fps: 10, position: [0, 0, 0.1], birthrate: 20, emit_seconds: 2, life: 3, velocity: 1, randvel: 0.2,
      spread: 0.175, mass: 0.035, particle_rot: 1.5, size_start: 1, size_end: 2, color_start: [0.376, 0.376, 0.376],
      color_end: [0.176, 0.176, 0.176], alpha_start: 0.3, alpha_end: 0, xsize: 20, ysize: 20, blur_length: 10, inherit: true }),
  ];
  const warm = [0.945, 0.741, 0.42];
  const glow = core.light({ name: "glow", position: [0, 0, 0], color: warm, radius: 5, multiplier: 1, fading: false });
  core.findKeys(glow.keys, "impact", true).color = [0, ...warm, 0.5, 0.85, 0.55, 0.25, 1, 0.5, 0.25, 0.08, 1.5, 0, 0, 0];
  return core.effect({ name: LIGHT_MODEL, duration: false, seconds: 2, attach: "root", lengths: [0, 2, 1], emitters, lights: [glow] });
}

// --- the effects ---------------------------------------------------------------------------------------------------------
// [{ name, row, fx }] in the call's order: the ten debris effects, then the light's death.
function effects(core) {
  const out = [];
  MATERIALS.forEach((material, m) => {
    SIZES.forEach((size, s) => {
      const d = DEBRIS[material];
      const list = size === "s" ? d.s() : [...(d.l || d.s)(), ...d.up()];
      const fx = core.effect({ name: debrisModel(m, s), duration: false, seconds: 1, attach: "root", lengths: [0, 2, 1],
        emitters: list.map((e) => core.emitter(e)) });
      out.push({ name: fx.name, row: rowOf(fx.name), fx });
    });
  });
  out.push({ name: LIGHT_MODEL, row: rowOf(LIGHT_MODEL), fx: lightDeath(core) });
  return out;
}

// The drafts' text: [{ name, row, fx, draft }].
function build(core) {
  return effects(core).map((e) => Object.assign(e, { draft: core.writeDraft(e.fx) }));
}

// A draft's model through the game's writer (mdlmesh --model), LF.
function modelOf(reader, draftFile, name) {
  return execFileSync(reader, ["--model", draftFile, name], { encoding: "utf8", windowsHide: true }).replace(/\r/g, "");
}

// Write `text` to `file` unless it holds it already (compared without CRs). -> "new" | "rebuilt" | "unchanged".
function writeText(file, text) {
  const old = fs.existsSync(file) ? fs.readFileSync(file, "utf8").replace(/\r/g, "") : null;
  if (old === text) return "unchanged";
  fs.writeFileSync(file, text);
  return old === null ? "new" : "rebuilt";
}

// What nwn5e-tools gives (the bench's core.js), and mdlmesh.
function tools(argv) {
  const W = require("../floatdisk/files.js");
  const option = (name, fallback) => W.option(argv, name, fallback);
  const dir = path.resolve(option("tools", process.env.NWN5E_TOOLS || path.join(REPO, "..", "nwn5e-tools")));
  global.window = global.window || {};
  const corePath = path.join(dir, "vfx_bench", "public", "core.js");
  if (!fs.existsSync(corePath)) throw new Error("nwn5e-tools' VFX Bench core (--tools) isn't there: " + corePath);
  return { dir, core: require(corePath), option };
}

function main() {
  const argv = process.argv.slice(2);
  const { core, option } = tools(argv);
  const R = require("../floatdisk/reader.js");
  const reader = R.ensureReader({ repo: REPO, reader: option("reader", process.env.NWN5E_MDLMESH || "") });
  for (const e of build(core)) {
    const draftFile = path.join(DRAFTS, e.name + ".vfx");
    const said = [writeText(draftFile, e.draft)];
    said.push(writeText(path.join(FILES, e.name + ".mdl"), modelOf(reader, draftFile, e.name)));
    console.log(`${e.row} ${e.name}: draft ${said[0]}, model ${said[1]} (${e.fx.emitters.length} emitters${e.fx.lights.length ? ", a light" : ""})`);
  }
}

module.exports = { MATERIALS, SIZES, FIRST_ROW, MODELS, LIGHT_MODEL, CHUNK, ROCK, PUFF, DUST, SPARK, GLINT, debrisModel, rowOf,
  effects, build, modelOf, tools };
if (require.main === module) {
  try {
    main();
  } catch (e) {
    console.error(e.message);
    process.exit(1);
  }
}
