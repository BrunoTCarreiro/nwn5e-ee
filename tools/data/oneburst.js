// One burst (LD; the owner, 2026-11-20: "For the lectern A" - the Academy's lectern "fountained a huge amount of debris"
// when broken). A broken placeable's debris is its model's own "die" animation: its detonate events set off its Explosion
// emitters (chunks, a smoke puff). The client then loops "dead" until the server deletes the object, 2 s later - and 16
// of the game's placeable models have a detonate in that "dead" too, so every loop throws the debris again: a fountain.
// build_derived.js serves copies of those 16 - the game's own model, read from the player's game at build time, with the
// "dead" animation's detonate events taken out and nothing else changed - under their own names in the build's
// data\files, which nwn5e serves over the game's (as it serves the held torch's flame: the game's model changed in place,
// written under its own name). Built, never committed: art made from the game's files (.gitignore names the 16).
//
//   const O = require("./oneburst.js");
//   O.derive({ models: O.MODELS, res, reader })   -> { files: [{ name, bytes, dropped, form }], skipped: [{ model, why }] }
//   O.fountains({ res, reader, twoDa })           -> { fountains: [names], idle: [names] }   (the rule, from the game)
//   O.readModels(reader, [{ name, bytes }])       -> what mdlmesh --anims reads of each
//
// `res`: spell_bench\game_data.js's resources(game) - (name, type) -> the game's bytes or null; `reader`: mdlmesh.exe
// (floatdisk\reader.js ensureReader), which reads and changes them through src\data\vfx_model.cpp's ReadModelAnimations
// and DropAnimationEvents. tools\data\README.md, "One burst".
"use strict";
const fs = require("fs");
const os = require("os");
const path = require("path");
const { execFileSync } = require("child_process");

// The game's placeable models (placeables.2da's ModelName) whose looping "dead" has a detonate event and which have an
// Explosion emitter it sets off (fountains, below; check_derived.js fails when the game's say otherwise). Each keeps the
// form the game has it in: plc_c10 and pwc_mage_001 are ASCII there, the rest compiled.
const MODELS = ["plc_c03", "plc_c04", "plc_c10", "plc_h10", "plc_i02", "plc_k02", "plc_q03", "plc_x0_fgs", "plc_x0_grw",
  "plc_x0_tds", "plc_x0_wds", "pwc_mage_001", "px2_b04", "px2_c10", "px2_e01", "px2_g02"];
const ANIM = "dead", EVENT = "detonate";
const MDL = 2002, TWODA = 2017;
const lower = (s) => String(s).toLowerCase();

// mdlmesh --anims over models' bytes, a few dozen a run (the command line's length) -> one entry a model, in order:
// { form: "compiled" | "ascii", anims: [{ name, length, header, eventAt, events: [{ time, name }] }], emitters: [{ name,
// update }], error } - error set (and the rest empty) for one it can't read.
function readModels(reader, list) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "n5_oneburst-"));
  try {
    const out = [];
    for (let at = 0; at < list.length; at += 32) {
      const files = list.slice(at, at + 32).map((m, i) => {
        const file = path.join(dir, `${at + i}.mdl`);
        fs.writeFileSync(file, m.bytes);
        return file;
      });
      const text = execFileSync(reader, ["--anims", ...files], { encoding: "latin1", maxBuffer: 256 << 20, windowsHide: true });
      const byFile = new Map(files.map((f) => [f, { form: "", anims: [], emitters: [], error: "" }]));
      let cur = null;
      for (const line of text.split(/\r?\n/)) {
        let m;
        if ((m = line.match(/^# model (.+) (compiled|ascii)$/))) {
          cur = byFile.get(m[1]);
          if (cur) cur.form = m[2];
        } else if ((m = line.match(/^# model (.+) unreadable (.*)$/))) {
          cur = byFile.get(m[1]);
          if (cur) cur.error = m[2] || "unreadable";
          cur = null;
        } else if (cur && (m = line.match(/^anim (\S*) length (\S+) header (\d+) events (\d+) (\d+)$/))) {  // a name may be "" (newanim alone)
          cur.anims.push({ name: m[1], length: Number(m[2]), header: Number(m[3]), eventAt: Number(m[4]), count: Number(m[5]), events: [] });
        } else if (cur && (m = line.match(/^event (\d+) (\S+) (\S+)$/))) {
          const a = cur.anims[Number(m[1])];
          if (a) a.events.push({ time: Number(m[2]), name: m[3] });
        } else if (cur && (m = line.match(/^emitter (\S+) (\S*)$/))) {
          cur.emitters.push({ name: m[1], update: m[2] });
        }
      }
      for (const f of files) {
        const r = byFile.get(f);
        if (!r.form && !r.error) r.error = "mdlmesh said nothing of it";
        out.push(r);
      }
    }
    return out;
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
}

// A model as read: its "dead"'s detonates, and whether an Explosion emitter is there for them to set off.
const deadDetonates = (r) => r.anims.filter((a) => lower(a.name) === ANIM).reduce((n, a) => n + a.events.filter((e) => lower(e.name) === EVENT).length, 0);
const bursts = (r) => r.emitters.some((e) => lower(e.update) === "explosion");

// A copy as read (readModels) against the game's model as read: readable, the same form and emitters, the same animations
// (name, length, header) with the same events - but the "dead"'s, which are the game's without its detonates, none left.
// Throws "one burst: <model>: ..." when it isn't.
function verifyCopy(model, game, copy) {
  const fail = (what) => {
    throw new Error(`one burst: ${model}: ${what} - nothing written`);
  };
  const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
  if (game.error || copy.error) fail(`the copy or the game's model can't be read back (${copy.error || game.error})`);
  if (copy.form !== game.form) fail(`the copy is ${copy.form}, the game's ${game.form}`);
  if (!same(copy.emitters, game.emitters)) fail("the copy's emitters aren't the game's");
  if (copy.anims.length !== game.anims.length) fail(`the copy has ${copy.anims.length} animations, the game's ${game.anims.length}`);
  game.anims.forEach((g, k) => {
    const c = copy.anims[k];
    if (!same([c.name, c.length, c.header], [g.name, g.length, g.header])) fail(`the copy's animation ${k} isn't the game's ("${g.name}")`);
    if (lower(g.name) !== ANIM) {
      if (!same(c.events, g.events)) fail(`the copy's "${g.name}" events aren't the game's`);
      return;
    }
    if (c.events.some((e) => lower(e.name) === EVENT)) fail(`the copy has a ${EVENT} left in its "${ANIM}"`);
    if (!same(c.events, g.events.filter((e) => lower(e.name) !== EVENT))) fail(`the copy's "${ANIM}" isn't the game's without its ${EVENT}s`);
  });
}

// The copies: each of `models` as the game has it, its "dead"'s detonates taken out (mdlmesh --drop-events: vfx_model.cpp's
// DropAnimationEvents - in place, every other byte as it was), under its own name, then read back beside the game's and
// checked (verifyCopy). A model the game hasn't, or whose "dead" has no detonate now (a game update), is skipped and said;
// a model the reader can't read or change, or a copy that isn't the game's model minus its "dead"'s detonates, stops it
// (throws).
function derive({ models, res, reader }) {
  const files = [], skipped = [], games = [];
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "n5_oneburst-"));
  try {
    for (const model of models) {
      const bytes = res(model, MDL);
      if (!bytes) {
        skipped.push({ model, why: "not in the game" });
        continue;
      }
      const from = path.join(dir, model + ".game.mdl"), to = path.join(dir, model + ".mdl");
      fs.writeFileSync(from, bytes);
      let said;
      try {
        said = execFileSync(reader, ["--drop-events", ANIM, EVENT, from, to], { encoding: "latin1", windowsHide: true });
      } catch (e) {
        throw new Error(`one burst: ${model}: the reader couldn't change it - ${String(e.stderr || e.message).trim()}`);
      }
      const dropped = Number((said.match(/^dropped (\d+)/m) || [])[1] || 0);
      if (!dropped) {
        skipped.push({ model, why: `the game's own has no ${EVENT} in its "${ANIM}" now - served as the game has it` });
        continue;
      }
      files.push({ name: model + ".mdl", bytes: fs.readFileSync(to), dropped, form: bytes.readUInt32LE(0) === 0 ? "compiled" : "ascii" });
      games.push({ model, bytes });
    }
    // Each copy read back beside the game's (one mdlmesh run for them all): a bad copy stops the build.
    const read = readModels(reader, [].concat(...games.map((g, i) => [{ bytes: g.bytes }, { bytes: files[i].bytes }])));
    games.forEach((g, i) => verifyCopy(g.model, read[2 * i], read[2 * i + 1]));
    return { files, skipped };
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
}

// The rule, from the game: each model placeables.2da names (once) whose "dead" has a detonate. -> { fountains: those with
// an Explosion emitter (what a detonate sets off), sorted; idle: those without one - their detonate sets off nothing }.
// `twoDa`: game_data.js's (text -> { columns, rows }).
// Only a model's own "dead" is looked at: one it would inherit from its supermodel isn't. None does today - of the 192
// placeable models with a supermodel, 191 have no "dead" of their own and none of their supermodels' chains has a
// "dead" with a detonate (LD fix round 1, read 2026-11-20) - so a game update that adds one isn't caught here.
function fountains({ res, reader, twoDa }) {
  const table = res("placeables", TWODA);
  if (!table) throw new Error("the game has no placeables.2da");
  const names = new Set();
  for (const row of twoDa(table.toString("latin1")).rows.values()) {
    const m = row.ModelName;
    if (m && m !== "****") names.add(lower(m));
  }
  // Only a model with "detonate" anywhere in it can have one in its "dead": the rest aren't read.
  const list = [];
  for (const name of [...names].sort()) {
    const bytes = res(name, MDL);
    if (bytes && bytes.toString("latin1").toLowerCase().includes(EVENT)) list.push({ name, bytes });
  }
  const read = readModels(reader, list);
  const out = { fountains: [], idle: [] };
  list.forEach((m, i) => {
    if (read[i].error || !deadDetonates(read[i])) return;
    (bursts(read[i]) ? out.fountains : out.idle).push(m.name);
  });
  return out;
}

module.exports = { MODELS, ANIM, EVENT, readModels, derive, verifyCopy, fountains, deadDetonates, bursts };
