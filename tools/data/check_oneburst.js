#!/usr/bin/env node
// oneburst.js without the game (LD fix round 1) - models built here, nothing of the game's; it needs only the mesh reader
// (mdlmesh, built when missing, as build_derived.js builds it):
//   node tools\data\check_oneburst.js
// check_derived.js checks the 16 against the game's own models (it needs the game).
"use strict";
const assert = require("assert");
const path = require("path");
const { spawnSync } = require("child_process");
const R = require("./floatdisk/reader.js");
const O = require("./oneburst.js");

const REPO = path.resolve(__dirname, "..", "..");
const READER = R.ensureReader({ repo: REPO });

let failed = 0;
function check(name, fn) {
  try {
    fn();
    console.log("ok   " + name);
  } catch (e) {
    failed++;
    console.log("FAIL " + name + ": " + e.message);
  }
}

// An ASCII model built here: an Explosion emitter, an animation with no name (newanim alone) before "die" and "dead".
const MODEL = Buffer.from([
  "newmodel plc_chk", "setsupermodel plc_chk NULL", "classification Character", "beginmodelgeom plc_chk",
  "node dummy plc_chk", "  parent NULL", "endnode", "node emitter chunky1", "  parent plc_chk", "  update Explosion",
  "  chunkName plc_chunk_w01", "endnode", "endmodelgeom plc_chk",
  "newanim", "  length 0.5", "  event 0.1 snd_a", "doneanim",
  "newanim die plc_chk", "  length 0.167", "  event 0.033 detonate", "  event 0.033 detonate", "doneanim die plc_chk",
  "newanim dead plc_chk", "  length 0.033", "  event 0.0 detonate", "  event 0.0 snd_dead", "doneanim dead plc_chk",
  "donemodel plc_chk", ""].join("\r\n"), "latin1");
const names = (r) => r.anims.map((a) => a.name);
const events = (a) => a.events.map((e) => e.name + " " + e.time).join(", ");

try {
  check("readModels: an animation mdlmesh prints with no name keeps its place - the events after it land on their own animation", () => {
    const [r] = O.readModels(READER, [{ bytes: MODEL }]);
    assert.ok(!r.error, r.error);
    assert.deepStrictEqual(names(r), ["", "die", "dead"]);
    assert.strictEqual(events(r.anims[0]), "snd_a 0.1");
    assert.strictEqual(events(r.anims[1]), "detonate 0.033, detonate 0.033");
    assert.strictEqual(events(r.anims[2]), "detonate 0, snd_dead 0");
    assert.strictEqual(O.deadDetonates(r), 1);
    assert.ok(O.bursts(r), "no Explosion emitter read");
  });

  check("derive: the copy re-read - its \"dead\" without the detonate, every other animation the game's; skips said", () => {
    const res = (name, type) => (type === 2002 && name === "plc_chk" ? MODEL : null);
    const d = O.derive({ models: ["plc_chk", "plc_none"], res, reader: READER });
    assert.deepStrictEqual(d.skipped, [{ model: "plc_none", why: "not in the game" }]);
    assert.strictEqual(d.files.length, 1);
    assert.deepStrictEqual([d.files[0].name, d.files[0].dropped, d.files[0].form], ["plc_chk.mdl", 1, "ascii"]);
    assert.strictEqual(d.files[0].bytes.toString("latin1"), MODEL.toString("latin1").replace("  event 0.0 detonate\r\n", ""));
  });

  check("verifyCopy: a copy that isn't the game's model minus its \"dead\"'s detonates throws, naming the model", () => {
    const [game] = O.readModels(READER, [{ bytes: MODEL }]);
    const good = JSON.parse(JSON.stringify(game));
    good.anims[2].events = good.anims[2].events.filter((e) => e.name !== "detonate");
    O.verifyCopy("plc_chk", game, good);  // the right copy: no throw
    const broken = (what, edit) => {
      const c = JSON.parse(JSON.stringify(good));
      edit(c);
      assert.throws(() => O.verifyCopy("plc_chk", game, c), /^Error: one burst: plc_chk: /, what + ": not refused");
    };
    broken("a detonate left in dead", (c) => c.anims[2].events.push({ time: 0, name: "Detonate" }));
    broken("dead's other event lost", (c) => { c.anims[2].events = []; });
    broken("die's events changed", (c) => { c.anims[1].events.pop(); });
    broken("the unnamed animation's events changed", (c) => { c.anims[0].events = []; });
    broken("an animation lost", (c) => c.anims.pop());
    broken("an animation's length changed", (c) => { c.anims[1].length = 0.2; });
    broken("an emitter changed", (c) => { c.emitters = []; });
    broken("the form changed", (c) => { c.form = "compiled"; });
    broken("unreadable", (c) => { c.error = "cut short"; });
    assert.throws(() => O.verifyCopy("plc_chk", game, JSON.parse(JSON.stringify(game))), /a detonate left in its "dead"/);
  });

  check("mdlmesh: a mode with the wrong number of arguments is a usage error (exit 2), not a file to read", () => {
    for (const args of [["--drop-events", "dead", "detonate", "only_in.mdl"], ["--drop-events"], ["--model", "draft.vfx"], ["--anims"]]) {
      const r = spawnSync(READER, args, { encoding: "latin1", windowsHide: true });
      assert.strictEqual(r.status, 2, `${args.join(" ")}: exit ${r.status} - ${(r.stdout + r.stderr).trim()}`);
      assert.match(r.stderr, /^usage: mdlmesh /, args.join(" "));
    }
  });
} finally {
  console.log(failed ? `oneburst check: ${failed} failed` : "oneburst check: all pass");
  process.exitCode = failed ? 1 : 0;
}
