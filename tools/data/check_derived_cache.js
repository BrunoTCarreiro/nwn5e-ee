#!/usr/bin/env node
// derived_cache.js and derived_trace.js on temporary folders - no game, no generator ("Faster checks", docs/TOOLS-PLAN.md):
//   node tools\data\check_derived_cache.js
// A fake repo, game and reader, and units that read them as the generators do - in this process, and as a child (with a
// child of its own) - each run twice to see it reused, then each kind of input changed to see it run again: a file's
// bytes (and the same bytes with a new time: reused), a path that wasn't there, a folder's names, a module's code, a game
// file by size and time, the reader; what a run wrote and read back is no input, a file it read before rewriting is; a
// scratch copy read as the folder it copies; a copy of the repo elsewhere finds the same entries; an error isn't kept;
// "off", "fresh" and "verify"; a damaged or missing blob is a miss; an input changed while it ran isn't kept.
"use strict";
const DC = require("./derived_cache.js");  // (first, as build_derived.js loads it: its tracer wraps child_process for what follows)
const assert = require("assert");
const fs = require("fs");
const os = require("os");
const path = require("path");
const { spawnSync } = require("child_process");

const root = fs.mkdtempSync(path.join(os.tmpdir(), "n5_cachecheck-"));
const REPO = path.join(root, "repo"), GAME = path.join(root, "game"), TOOLS = path.join(root, "tools"), CACHE = path.join(root, "cache");
const READER = path.join(root, "reader.exe");
const put = (file, text) => {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, text);
};
const later = (file) => {  // the same bytes, a new time
  const t = new Date(fs.statSync(file).mtimeMs + 5000);
  fs.utimesSync(file, t, t);
};
put(path.join(REPO, "recipe.txt"), "recipe 1");
put(path.join(REPO, "rows", "a.txt"), "row a");
put(path.join(REPO, "rows", "b.txt"), "row b");
put(path.join(GAME, "data", "nwn_base.key"), "key 1");
put(path.join(GAME, "data", "base.bif"), "bif 1");
put(path.join(TOOLS, "lib.js"), "module.exports = { k: 1 };\n");
put(READER, "reader 1");
// The in-process unit's code: a module reading the recipe, the game's bif, probing an ovr file, listing rows\, and
// writing then reading back a scratch file of its own.
const UNIT = path.join(REPO, "unit.js");
put(UNIT, `"use strict";
const fs = require("fs"), os = require("os"), path = require("path");
const lib = require(${JSON.stringify(path.join(TOOLS, "lib.js"))});
module.exports = (repo, game) => {
  const scratch = fs.mkdtempSync(path.join(os.tmpdir(), "n5_cachecheck_unit-"));
  fs.writeFileSync(path.join(scratch, "mine.txt"), "mine");
  fs.writeFileSync(path.join(repo, "mine.txt"), "mine");  // (one beside the inputs, read back and left: no input)
  const mine = fs.readFileSync(path.join(scratch, "mine.txt"), "utf8") + fs.readFileSync(path.join(repo, "mine.txt"), "utf8");
  fs.rmSync(scratch, { recursive: true, force: true });
  return { recipe: fs.readFileSync(path.join(repo, "recipe.txt"), "utf8"), bif: fs.readFileSync(path.join(game, "data", "base.bif")),
    ovr: fs.existsSync(path.join(game, "ovr", "x.mdl")), rows: fs.readdirSync(path.join(repo, "rows")).length, k: lib.k, mine };
};
`);
// The child unit: a script reading a rows file through a scratch copy (and rewriting that copy, as a generator writes
// its rows), and running a child of its own that reads the recipe's neighbour and a file the child wrote in a folder it
// made (as spiritweapon\build.js's Composer run writes into its scratch).
const CHILD = path.join(REPO, "child.js"), GRAND = path.join(REPO, "grand.js");
put(path.join(REPO, "neighbour.txt"), "neighbour 1");
put(GRAND, `const fs = require("fs"), path = require("path");
fs.readdirSync(path.dirname(process.argv[2]));
process.stdout.write(fs.readFileSync(${JSON.stringify(path.join(REPO, "neighbour.txt"))}, "utf8") + fs.readFileSync(process.argv[2], "utf8"));\n`);
put(CHILD, `"use strict";
const fs = require("fs"), os = require("os"), path = require("path"), { spawnSync } = require("child_process");
const [rows, out] = process.argv.slice(2);
const made = fs.mkdtempSync(path.join(os.tmpdir(), "n5_cachecheck_child-"));
fs.writeFileSync(path.join(made, "given.txt"), "");
const grand = spawnSync(process.execPath, [${JSON.stringify(GRAND)}, path.join(made, "given.txt")], { encoding: "utf8" }).stdout;
fs.rmSync(made, { recursive: true, force: true });
const a = fs.readFileSync(path.join(rows, "a.txt"), "utf8");
fs.writeFileSync(path.join(rows, "a.txt"), a + " (rewritten)");
fs.writeFileSync(out, JSON.stringify({ a, grand }));
`);

let failed = 0;
function check(name, fn) {
  try {
    fn();
    console.log("ok   " + name);
  } catch (e) {
    failed++;
    console.log("FAIL " + name + ": " + (e.stack || e.message).split("\n").slice(0, 3).join(" | "));
  }
}

const open = (mode = "on", repo = REPO) => DC.open({ dir: CACHE, mode, anchors: { repo, tools: TOOLS, game: GAME, reader: READER } });
// One in-process run -> { result, reused } ("ran" when it ran).
function local(mode = "on", repo = REPO) {
  const cache = open(mode, repo);
  const file = path.join(repo, "unit.js");
  delete require.cache[file];
  const result = cache.unit({ id: "unit", key: { n: 1 }, modules: [file], inputs: [path.join(GAME, "data", "nwn_base.key"), READER],
    run: () => require(file)(repo, GAME) });
  return { result, reused: cache.stats.reused === 1, why: cache.stats.missed.join("; ") };
}
// One child run: rows\ copied into a scratch folder first, as build_derived.js copies data\rows.
function child(mode = "on") {
  const cache = open(mode);
  const result = cache.unit({ id: "child", key: { n: 2 }, child: true, inputs: [READER], run: (ctx) => {
    const stage = fs.mkdtempSync(path.join(os.tmpdir(), "n5_cachecheck_stage-"));
    ctx.exclude(stage);
    try {
      const seen = new Map();
      fs.mkdirSync(path.join(stage, "rows"));
      for (const f of fs.readdirSync(path.join(REPO, "rows"))) {
        const st = fs.statSync(path.join(REPO, "rows", f));
        seen.set(path.join(REPO, "rows", f).toLowerCase(), { size: st.size, mtimeMs: st.mtimeMs });
        fs.copyFileSync(path.join(REPO, "rows", f), path.join(stage, "rows", f));
      }
      ctx.alias(path.join(stage, "rows"), path.join(REPO, "rows"), seen);
      const out = path.join(stage, "out.json");
      const r = spawnSync(process.execPath, [CHILD, path.join(stage, "rows"), out], { encoding: "utf8", env: Object.assign({}, process.env, ctx.env) });
      if (r.status !== 0) throw new Error("child: " + r.stderr);
      return JSON.parse(fs.readFileSync(out, "utf8"));
    } finally {
      fs.rmSync(stage, { recursive: true, force: true });
    }
  } });
  return { result, reused: cache.stats.reused === 1, why: cache.stats.missed.join("; ") };
}
const ran = (r, why) => {
  assert.ok(!r.reused, "reused, not ran");
  if (why) assert.match(r.why, why);
};
const reused = (r) => assert.ok(r.reused, "ran: " + r.why);

try {
  check("in this process: run, then reused; a file it wrote and read back is no input", () => {
    const a = local();
    ran(a, /never built/);
    assert.deepStrictEqual([a.result.recipe, a.result.bif.toString(), a.result.ovr, a.result.rows, a.result.k, a.result.mine],
      ["recipe 1", "bif 1", false, 2, 1, "minemine"]);
    fs.writeFileSync(path.join(REPO, "mine.txt"), "someone else's");
    const b = local();
    reused(b);
    assert.ok(Buffer.isBuffer(b.result.bif) && b.result.bif.equals(a.result.bif), "the Buffer comes back a Buffer, the same bytes");
    assert.deepStrictEqual(Object.assign({}, b.result, { bif: null }), Object.assign({}, a.result, { bif: null }));
  });
  check("a file's bytes changed: run again; the same bytes with a new time: reused", () => {
    fs.writeFileSync(path.join(REPO, "recipe.txt"), "recipe 2");
    const a = local();
    ran(a, /repo:recipe\.txt changed/);
    assert.strictEqual(a.result.recipe, "recipe 2");
    later(path.join(REPO, "recipe.txt"));
    reused(local());
  });
  check("a path that wasn't there is: run again; a folder's names changed: run again", () => {
    put(path.join(GAME, "ovr", "x.mdl"), "over");
    const a = local();
    ran(a, /game:ovr\/x\.mdl is there now/);
    assert.strictEqual(a.result.ovr, true);
    put(path.join(REPO, "rows", "c.txt"), "row c");
    const b = local();
    ran(b, /repo:rows's files changed/);
    assert.strictEqual(b.result.rows, 3);
    reused(local());
  });
  check("a module's code changed (one the unit loads): run again", () => {
    put(path.join(TOOLS, "lib.js"), "module.exports = { k: 2 };\n");
    delete require.cache[path.join(TOOLS, "lib.js")];
    const a = local();
    ran(a, /tools:lib\.js changed/);
    assert.strictEqual(a.result.k, 2);
    reused(local());
  });
  check("a game file: by size and time - its time alone changed runs it again; the reader's bytes: run again", () => {
    later(path.join(GAME, "data", "base.bif"));
    ran(local(), /game:data\/base\.bif changed/);
    fs.writeFileSync(path.join(GAME, "data", "nwn_base.key"), "key 2");
    ran(local(), /game:data\/nwn_base\.key changed/);
    fs.writeFileSync(READER, "reader 2");
    ran(local(), /reader: changed/);
    reused(local());
  });
  check("a copy of the repo elsewhere (as check_derived.js makes): the same entries, reused; its own change runs it", () => {
    const copy = path.join(root, "copy");
    fs.cpSync(REPO, copy, { recursive: true });
    reused(local("on", copy));
    fs.writeFileSync(path.join(copy, "recipe.txt"), "recipe 3");
    const a = local("on", copy);
    ran(a);
    assert.strictEqual(a.result.recipe, "recipe 3");
  });
  check("a child: run, then reused - its child's reads, a scratch copy read (then rewritten) as the folder it copies", () => {
    const a = child();
    ran(a, /never built/);
    assert.deepStrictEqual(a.result, { a: "row a", grand: "neighbour 1" });
    reused(child());
    fs.writeFileSync(path.join(REPO, "rows", "a.txt"), "row a2");
    const b = child();
    ran(b, /repo:rows\/a\.txt changed/);
    assert.strictEqual(b.result.a, "row a2");
    fs.writeFileSync(path.join(REPO, "rows", "b.txt"), "row b2");  // not read by it
    reused(child());
    fs.writeFileSync(path.join(REPO, "neighbour.txt"), "neighbour 2");
    const c = child();
    ran(c, /repo:neighbour\.txt changed/);
    assert.strictEqual(c.result.grand, "neighbour 2");
    reused(child());
  });
  check("a node process of the run that leaves no trace - one this process starts, or one that never loads the tracer: not kept", () => {
    for (const [id, child, start] of [["in this process", false, () => spawnSync(process.execPath, ["-e", "1"])],
      ["a child's", true, (ctx) => spawnSync(process.execPath, ["-e", `require("child_process").spawnSync(process.execPath, ["--version"])`],
        { env: Object.assign({}, process.env, ctx.env) })]]) {
      let n = 0;
      const run = (ctx) => (start(ctx), ++n);
      open().unit({ id, key: {}, child, run });
      const again = open();
      again.unit({ id, key: {}, child, run });
      assert.strictEqual(again.stats.reused, 0, `${id}: kept`);
      assert.strictEqual(n, 2);
    }
  });
  check("an error isn't kept: thrown again, and the next run runs", () => {
    const cache = open();
    assert.throws(() => cache.unit({ id: "bad", key: {}, run: () => { throw new Error("refused"); } }), /refused/);
    let n = 0;
    open().unit({ id: "bad", key: {}, run: () => ++n });
    assert.strictEqual(n, 1);
  });
  check("off: runs, keeps nothing; fresh: runs and keeps; verify: runs, and fails where a kept output differs", () => {
    const before = fs.readdirSync(path.join(CACHE, "u")).length;
    let n = 0;
    const once = (mode, value) => {
      const cache = open(mode);
      const r = cache.unit({ id: "modes", key: {}, run: () => (++n, value) });
      return { r, cache };
    };
    once("off", 1);
    assert.strictEqual(fs.readdirSync(path.join(CACHE, "u")).length, before, "off kept something");
    once("fresh", 1);
    once("fresh", 1);
    assert.strictEqual(n, 3, "fresh didn't run");
    const { cache } = once("on", 1);
    assert.strictEqual(cache.stats.reused, 1, "fresh didn't keep");
    once("verify", 1);
    assert.strictEqual(n, 4, "verify didn't run");
    assert.throws(() => once("verify", 2), /differs from a fresh run/);
  });
  check("a damaged or missing blob: a miss, the unit runs again", () => {
    local();
    const blobs = path.join(CACHE, "b");
    for (const b of fs.readdirSync(blobs)) fs.writeFileSync(path.join(blobs, b), "damaged");
    const a = local();
    ran(a);
    assert.strictEqual(a.result.bif.toString(), "bif 1");
    reused(local());
    for (const b of fs.readdirSync(blobs)) fs.rmSync(path.join(blobs, b));
    ran(local());
  });
  check("an input changed while it ran - by another process, or rewritten by the run itself: not kept", () => {
    const file = path.join(REPO, "recipe.txt");
    for (const [id, change] of [["under it", () => spawnSync(process.execPath, ["-e", `require("fs").writeFileSync(${JSON.stringify(file)}, "changed under it")`])],
      ["its own", () => fs.writeFileSync(file, "rewritten by the run")]]) {
      let n = 0;
      open().unit({ id, key: {}, run: () => {
        fs.readFileSync(file, "utf8");
        change();
        return ++n;
      } });
      const again = open();
      again.unit({ id, key: {}, run: () => ++n });
      assert.strictEqual(again.stats.reused, 0, `${id}: kept a run whose input changed while it ran`);
      assert.strictEqual(n, 2);
    }
  });
} finally {
  fs.rmSync(root, { recursive: true, force: true });
}
console.log(failed ? `derived cache check: ${failed} failed` : "derived cache check: all pass");
process.exitCode = failed ? 1 : 0;
