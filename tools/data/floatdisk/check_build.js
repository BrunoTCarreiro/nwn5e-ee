#!/usr/bin/env node
// The generator end to end (build.js, reader.js), on a temporary copy - needs the game, prints "skip" without it:
//   node tools\data\floatdisk\check_build.js [--game <folder>] [--tools <nwn5e-tools>]
// The copy is <tmp>\n5_fdbuild-*\tools\data\floatdisk (the folder) and <tmp>\...\data\files, data\rows (what it builds
// into: build.js's repo root is three folders up from itself), so the real worktree's data\ is never written - the last
// check says so, and that the real build's vfxedit\saved\ is as it was.
"use strict";
const assert = require("assert");
const crypto = require("crypto");
const fs = require("fs");
const os = require("os");
const path = require("path");
const { spawnSync } = require("child_process");

const REPO = path.resolve(__dirname, "..", "..", "..");
const argv = process.argv.slice(2);
const option = (name, fallback) => {
  const i = argv.indexOf("--" + name);
  return i >= 0 && i + 1 < argv.length ? argv[i + 1] : fallback;
};
const TOOLS = path.resolve(option("tools", process.env.NWN5E_TOOLS || path.join(REPO, "..", "nwn5e-tools")));
const READER = path.join(REPO, "build", "floatdisk", "mdlmesh.exe");
const GAME = require(path.join(TOOLS, "anim_helper", "helper.js")).findGameDir({ game: option("game", ""), buildDir: path.join(TOOLS, "..", "nwn5e-ee", "build") });
if (!fs.existsSync(path.join(GAME, "data", "nwn_base.key"))) {
  console.log("skip: no game at " + GAME);
  process.exit(0);
}

// --- the real worktree and build, before -------------------------------------------------------------------------------
// data\files' names, mtimes and sizes; the real build's vfxedit\saved\ listing (NWN5E_BUILD, else ..\nwn5e-ee\build beside
// nwn5e-tools, as build.js's --drafts has it).
const REAL_FILES = path.join(REPO, "data", "files");
const REAL_SAVED = path.join(path.resolve(process.env.NWN5E_BUILD || path.join(TOOLS, "..", "nwn5e-ee", "build")), "vfxedit", "saved");
const realState = () => ({
  files: Object.fromEntries(fs.readdirSync(REAL_FILES).map((f) => { const st = fs.statSync(path.join(REAL_FILES, f)); return [f, st.mtimeMs + ":" + st.size]; })),
  saved: fs.existsSync(REAL_SAVED) ? fs.readdirSync(REAL_SAVED).sort() : null,
});
const REAL = realState();

// --- the copy -----------------------------------------------------------------------------------------------------
const root = fs.mkdtempSync(path.join(os.tmpdir(), "n5_fdbuild-"));
const folder = path.join(root, "tools", "data", "floatdisk");
const FILES = path.join(root, "data", "files"), ROWS = path.join(root, "data", "rows");
fs.cpSync(__dirname, folder, { recursive: true });
fs.mkdirSync(FILES, { recursive: true });
fs.mkdirSync(ROWS, { recursive: true });
// The disk's files, and the generated textures beside them (a dyed part's bake, n5p_, and a shape's soft texture, n5s_,
// each with 12 hex digits: build output, git-ignored as the models).
for (const f of fs.readdirSync(REAL_FILES)) {
  if (/^(c_n5fd_|plc_n5fd_)/.test(f) || f === "n5t_forcedisk.tga" || /^n5[ps]_[0-9a-f]{12}\.(tga|txi)$/.test(f)) fs.copyFileSync(path.join(REAL_FILES, f), path.join(FILES, f));
}
for (const f of fs.readdirSync(path.join(REPO, "data", "rows"))) {
  if (/\.floatdisk\.txt$/.test(f)) fs.copyFileSync(path.join(REPO, "data", "rows", f), path.join(ROWS, f));
}

function build(args) {
  const r = spawnSync(process.execPath, [path.join(folder, "build.js"), "--game", GAME, "--tools", TOOLS, "--reader", READER,
    "--files", FILES, "--rows", ROWS, ...args], { encoding: "utf8", maxBuffer: 64 << 20, windowsHide: true });
  return { code: r.status, out: r.stdout || "", err: r.stderr || "" };
}
const json = (r) => {
  try {
    return JSON.parse(r.out.trim());
  } catch (e) {
    throw new Error("build.js printed no JSON" + (r.err ? " (" + r.err.trim() + ")" : ""));
  }
};
const snapshot = () => {
  const s = {};
  for (const dir of [FILES, ROWS]) {
    for (const f of fs.readdirSync(dir)) {
      const file = path.join(dir, f), st = fs.statSync(file);
      s[file] = st.mtimeMs + ":" + crypto.createHash("sha1").update(fs.readFileSync(file)).digest("hex");
    }
  }
  return s;
};
const withComposition = (name, edit) => {
  const c = JSON.parse(fs.readFileSync(path.join(folder, "composition.json"), "utf8"));
  edit(c);
  const file = path.join(folder, name);
  fs.writeFileSync(file, JSON.stringify(c));
  return file;
};
// The models made from the game's files - the six of its meshes and c_n5fd_empty - aren't committed (the owner,
// 2026-11-17; tools\data\README.md), nor are the generated textures - a dyed part's bakes, a shape's soft textures: a
// checkout without them - main after the merge, a fresh clone - gets them from one build first, so the full rebuild
// below still checks that every file comes out the same.
const F = require("./floatdisk.js");
const R = require("./reader.js");
const bakes = (r) => { try { return json(r).baked || []; } catch (e) { return []; } };
const shapes = (r) => { try { return json(r).shapes || []; } catch (e) { return []; } };
const generated = (r) => [...bakes(r), ...shapes(r)].map((b) => b.texture);  // the names of the textures it makes
const absent = [...F.STAGES.map((s) => F.creatureModel(s)), ...F.PILES.map((s) => F.pileModel(s))].map((m) => m + ".mdl")
  .concat(generated(build(["--check", "--json"])).map((t) => t + ".tga"))
  .filter((f) => !fs.existsSync(path.join(FILES, f)));
if (absent.length) {
  const r = build([]);
  console.log(`     ${absent.length} of the disk's models and generated textures aren't in this checkout (git-ignored, built at build time): ` +
    (r.code === 0 ? "built once first" : "the first build failed - " + r.err.trim()));
}
// A file's state lines (new / rebuilt / unchanged, then its path from the copy's root).
const stateLines = (r) => r.out.split(/\r?\n/).map((l) => l.match(/^(new|rebuilt|unchanged)\s+(\S+)$/)).filter(Boolean).map((m) => [m[1], m[2]]);
const GENERATED_FILE = /^data\/files\/n5[ps]_[0-9a-f]{12}\.(tga|txi)$/;  // a bake or a shape's soft texture (an additive one's TXI too: M1b)

// --- the checks ---------------------------------------------------------------------------------------------------
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

try {
  check("a full rebuild of the committed composition changes nothing (13 repo files unchanged, and its generated textures)", () => {
    const before = snapshot();
    const r = build([]);
    assert.strictEqual(r.code, 0, "build.js exited " + r.code + ": " + r.err.trim());
    const states = stateLines(r);
    const repo = states.filter(([, f]) => !GENERATED_FILE.test(f)), made = states.filter(([, f]) => GENERATED_FILE.test(f));
    assert.strictEqual(repo.length, 13, repo.length + " repo files reported");
    assert.ok(repo.every(([, f]) => f.startsWith("data/")), "outside data/: " + repo.map(([, f]) => f).join(", "));
    assert.ok(states.every(([s]) => s === "unchanged"), "not unchanged: " + states.filter(([s]) => s !== "unchanged").map(([s, f]) => s + " " + f).join(", "));
    // The generated textures are the composition's: its bakes, one a texture (the owner's Large dyes its helm_001: its
    // colours name it), and its shapes' soft textures.
    assert.deepStrictEqual(made.filter(([, f]) => f.endsWith(".tga")).map(([, f]) => path.posix.basename(f, ".tga")).sort(), generated(build(["--check", "--json"])).sort());
    assert.deepStrictEqual(snapshot(), before);
  });

  check("--json says what the text says", () => {
    const text = build(["--check"]).out;
    const j = json(build(["--check", "--json"]));
    const lines = text.split(/\r?\n/).map((l) => l.match(/^c_n5fd_(\w+): (\d+) parts?, (\d+) meshes, (\d+) triangles; the pile (\S+) m tall, its top (\S+) m/)).filter(Boolean);
    assert.strictEqual(lines.length, 4, lines.length + " stages in the text");
    assert.strictEqual(j.check, true);
    for (const m of lines) {
      const s = j.stages[m[1] === "med" ? "medium" : m[1]];
      assert.ok(s, "no stage " + m[1] + " in the JSON");
      assert.strictEqual(s.meshes, +m[3], m[1] + " meshes");
      assert.strictEqual(s.triangles, +m[4], m[1] + " triangles");
      assert.strictEqual(s.tall.toFixed(2), m[5], m[1] + " tall");
      assert.strictEqual(s.top.toFixed(2), m[6], m[1] + " top");
    }
  });

  check("a weapon's pieces are read as one, end to end", () => {
    const pieces = ["wswls_b_011", "wswls_m_011", "wswls_t_011"];
    const file = withComposition("weapon.json", (c) => { c.stages.medium = [{ model: pieces, at: [0, 0, 0], tilt: [0, 90] }]; });
    const r = build(["--composition", file, "--check", "--json"]);
    assert.strictEqual(r.code, 0, "build.js exited " + r.code + ": " + (json(r).error || r.err.trim()));
    const j = json(r);
    assert.strictEqual(j.stages.medium.parts, 1);
    assert.strictEqual(j.stages.medium.list.length, 1);
    // Read alone: the hilt's and the blade's boxes meet along the longest axis.
    const reader = require("./reader.js");
    const core = (global.window = global.window || {}, require(path.join(TOOLS, "vfx_bench", "public", "core.js")));
    const gameData = require(path.join(TOOLS, "spell_bench", "game_data.js"));
    const rd = reader.create({ game: GAME, core, gameData, reader: READER });
    try {
      const box = pieces.map((model) => rd.part({ source: { kind: "model", model } }));
      const all = rd.part({ source: { kind: "models", models: pieces } });
      const axis = [0, 1, 2].reduce((a, k) => (all.hi[k] - all.lo[k] > all.hi[a] - all.lo[a] ? k : a), 0);
      const [hilt, , blade] = box;
      const gap = Math.max(hilt.lo[axis], blade.lo[axis]) - Math.min(hilt.hi[axis], blade.hi[axis]);
      const mid = box[1];
      const gapMid = Math.max(hilt.lo[axis], mid.lo[axis]) - Math.min(hilt.hi[axis], mid.hi[axis]);
      console.log(`     axis ${"xyz"[axis]}: hilt ${hilt.lo[axis].toFixed(3)}..${hilt.hi[axis].toFixed(3)}, middle ${mid.lo[axis].toFixed(3)}..${mid.hi[axis].toFixed(3)}, ` +
        `blade ${blade.lo[axis].toFixed(3)}..${blade.hi[axis].toFixed(3)}; hilt-blade gap ${gap.toFixed(3)}, hilt-middle gap ${gapMid.toFixed(3)}`);
      assert.ok(gap <= 0.05, `the hilt and the blade are ${(gap * 100).toFixed(1)} cm apart along ${"xyz"[axis]}`);
    } finally {
      rd.close();
    }
  });

  check("a refused build writes nothing", () => {
    const before = snapshot();
    const file = withComposition("nope.json", (c) => { c.stages.small = [{ model: "plc_nope", at: [0, 0, 0] }]; });
    const r = build(["--composition", file, "--json"]);
    assert.strictEqual(r.code, 1, "build.js exited " + r.code);
    assert.match(json(r).error, /.*no such model/);
    assert.deepStrictEqual(snapshot(), before);
  });
  check("a file in the way it didn't make is refused, named from the repo's root", () => {
    // One ownership rule with the Composer's build.js (files.js): a repo file is named by its path from the root, as the
    // state lines and --json name it.
    const file = path.join(FILES, "plc_n5fd_small.pwk");
    const kept = fs.readFileSync(file);
    fs.writeFileSync(file, "#MAXDOOR ASCII\r\n# someone's walkmesh\r\n");
    try {
      const before = snapshot();
      const r = build(["--json"]);
      assert.strictEqual(r.code, 1, "build.js exited " + r.code);
      assert.strictEqual(json(r).error, "data/files/plc_n5fd_small.pwk is there and wasn't made by this generator: nothing written - move it away or rename it");
      assert.deepStrictEqual(snapshot(), before);
    } finally {
      fs.writeFileSync(file, kept);
    }
  });
  check("a file it made that changed is rebuilt", () => {
    const file = path.join(FILES, "plc_n5fd_small.pwk");
    const kept = fs.readFileSync(file);
    fs.writeFileSync(file, Buffer.concat([kept, Buffer.from("# an edit\r\n")]));  // still carries the mark
    const r = build(["--json"]);
    assert.strictEqual(r.code, 0, "build.js exited " + r.code + ": " + r.err.trim());
    const states = json(r).files.filter((f) => f.state !== "unchanged");
    assert.deepStrictEqual(states, [{ state: "rebuilt", file: "data/files/plc_n5fd_small.pwk" }]);
    assert.ok(fs.readFileSync(file).equals(kept), "rebuilt as it was");
  });
  check("a setup failure is still one JSON error line", () => {
    const r = spawnSync(process.execPath, [path.join(folder, "build.js"), "--check", "--json", "--tools", path.join(root, "no-such-tools")],
      { encoding: "utf8", windowsHide: true });
    assert.strictEqual(r.status, 1, "exit " + r.status);
    const lines = r.stdout.split(/\r?\n/).filter(Boolean);
    assert.strictEqual(lines.length, 1, lines.length + " lines on stdout: " + r.stdout.trim().slice(0, 200));
    assert.strictEqual(typeof JSON.parse(lines[0]).error, "string");
  });

  check("a dyed part is baked into data\\files beside the models; the models name it, made.txt doesn't list it", () => {
    // The check's own medium: helm_001 at the defaults, and with metal 1 5 - their names are the bake's rule, fixed.
    const DEF = "n5p_2cd3d725dedb", FIVE = "n5p_f15efdade06c";
    const file = withComposition("dyed.json", (c) => {
      c.stages.medium = [{ model: "helm_001", at: [0, 0, 0] }, { model: "helm_001", at: [0.2, 0, 0], colours: { metal1: 5 } }];
    });
    const core = (global.window = global.window || {}, require(path.join(TOOLS, "vfx_bench", "public", "core.js")));
    const gameData = require(path.join(TOOLS, "spell_bench", "game_data.js"));
    const rd = R.create({ game: GAME, core, gameData, reader: READER });
    try {
      // Every bake this recipe gives - the medium above and whatever the owner's other stages dye (the Large's helmet,
      // recoloured 2026-11-17: a recolour is a new name) - from the recipe itself, as check_derived.js's diskBakes: each
      // part read through reader.js, its PLT-only bitmaps named by floatdisk.js's bakeBitmaps (F.bakedName of the PLT and
      // the part's colours). In the build's order (the stages, their parts, their meshes), each name once, with its
      // part's ten colours and the stages that name it.
      const comp = F.readComposition(JSON.parse(fs.readFileSync(file, "utf8")), folder);
      const expected = new Map();  // name -> { plt, colours (only what isn't a default), full (the ten layers'), stages }
      for (const stage of F.STAGES) {
        for (const p of comp.stages[stage]) {
          for (const b of F.bakeBitmaps(rd.part(p).meshes, p.colours, rd.pltOnly).bakes) {
            if (!expected.has(b.name)) expected.set(b.name, { plt: b.plt, colours: b.colours, full: p.colours, stages: new Set() });
            expected.get(b.name).stages.add(stage);
          }
        }
      }
      assert.ok(expected.has(DEF) && expected.get(DEF).stages.has("medium"), "the recipe gives no " + DEF + " in the medium");
      assert.ok(expected.has(FIVE) && expected.get(FIVE).stages.has("medium"), "the recipe gives no " + FIVE + " in the medium");
      console.log("     the recipe's bakes: " + [...expected].map(([name, b]) => `${name} (${b.plt} ${JSON.stringify(b.colours)}, ${[...b.stages].join(" ")})`).join("; "));
      const made = fs.readFileSync(path.join(folder, "made.txt"));
      // --check: a summary line a bake, the part's six numbers in the card's order, where it goes.
      const c = build(["--composition", file, "--check"]);
      assert.strictEqual(c.code, 0, "build.js --check exited " + c.code + ": " + c.err.trim());
      const said = c.out.split(/\r?\n/);
      for (const [name, metal1] of [[DEF, 0], [FIVE, 5]]) {
        const line = `  texture ${name}: helm_001 dyed with metal 1 ${metal1}, metal 2 3, leather 1 3, leather 2 2, cloth 1 22, cloth 2 23 - baked into data/files`;
        assert.ok(said.includes(line), "no line: " + line + "\n" + c.out);
      }
      for (const [name, b] of expected) {
        const line = `  texture ${name}: ${b.plt} dyed with ` + F.PLT_FIELDS.map((k) => `${F.PLT_LABELS[k].toLowerCase()} ${b.full[k]}`).join(", ") + " - baked into data/files";
        assert.ok(said.includes(line), "no line: " + line + "\n" + c.out);
      }
      const had = fs.existsSync(path.join(FILES, DEF + ".tga"));  // an earlier build's bake may be there (copied above)
      const r = build(["--composition", file, "--json"]);
      assert.strictEqual(r.code, 0, "build.js exited " + r.code + ": " + r.err.trim());
      const j = json(r);
      assert.deepStrictEqual(j.baked, [...expected].map(([texture, b]) => ({ texture, plt: b.plt, colours: b.colours })));
      assert.deepStrictEqual(j.baked.slice(0, 2), [{ texture: DEF, plt: "helm_001", colours: {} }, { texture: FIVE, plt: "helm_001", colours: { metal1: 5 } }]);
      // Each stage's creature and pile name each of its bakes, and no longer the PLT it was baked from.
      for (const [name, b] of expected) {
        for (const stage of b.stages) {
          for (const m of [F.creatureModel(stage), F.pileModel(stage)].map((s) => s + ".mdl")) {
            const t = fs.readFileSync(path.join(FILES, m), "latin1");
            assert.ok(t.includes(`\r\n  bitmap ${name}\r\n`), `${m} doesn't name ${name}`);
            assert.ok(!t.toLowerCase().includes(`\r\n  bitmap ${b.plt}\r\n`), `${m} still names ${b.plt}`);
          }
        }
      }
      // Each in data\files at 24 bits, reader.js's bake of the game's PLT in its part's colours (helm_001's: 128 x 128, 18 +
      // 128 x 128 x 3 bytes).
      const states = Object.fromEntries(j.files.map((f) => [f.file, f.state]));
      assert.deepStrictEqual([states[`data/files/${DEF}.tga`], states[`data/files/${FIVE}.tga`]], [had ? "unchanged" : "new", "new"]);
      for (const [name, b] of expected) {
        const t = fs.readFileSync(path.join(FILES, name + ".tga"));
        if (b.plt === "helm_001") assert.strictEqual(t.length, 49170, name + ": " + t.length + " bytes");
        assert.strictEqual(t[16], 24, name + ": " + t[16] + " bits");
        const baked = rd.bake(b.plt, b.full);
        assert.strictEqual(baked.name, name);
        assert.ok(t.equals(baked.bytes), name + " isn't reader.js's bake");
      }
      // Nothing noted in made.txt, and no bake but in data\files.
      assert.ok(fs.readFileSync(path.join(folder, "made.txt")).equals(made), "made.txt changed");
      for (const dir of [ROWS, folder]) assert.ok(!fs.readdirSync(dir).some((f) => /^n5p_/i.test(f)), "a bake in " + dir);
      // Built again: every bake unchanged. A file of a baked name is the bake's (the name fixes its bytes): rewritten.
      const again = Object.fromEntries(json(build(["--composition", file, "--json"])).files.map((f) => [f.file, f.state]));
      assert.deepStrictEqual([...expected.keys()].map((name) => again[`data/files/${name}.tga`]), [...expected.keys()].map(() => "unchanged"));
      fs.writeFileSync(path.join(FILES, FIVE + ".tga"), "not a bake");
      const fixed = json(build(["--composition", file, "--json"])).files.filter((f) => f.state !== "unchanged");
      assert.deepStrictEqual(fixed, [{ state: "rebuilt", file: `data/files/${FIVE}.tga` }]);
      assert.ok(fs.readFileSync(path.join(FILES, FIVE + ".tga")).equals(rd.bake("helm_001", F.readColours({ metal1: 5 })).bytes));
    } finally {
      rd.close();
    }
  });

  check("a dome part: its mesh from its numbers, its soft texture beside the models and the drafts, owned by its name", () => {
    // The check's own medium: a 0.3 m dome in gold (floatdisk.js's shapeModel: no game model, no mesh reader), and the
    // game's lantern ptm_lantern03 (T1b: its meshes' own transparencyhint - inner panes 2, outer 1, the rest 0 - through
    // the mesh reader). --drafts into the copy's own folders, never the real build's.
    const GOLD = "n5s_a3caa0709dda", colour = [1, 0.85, 0.4];
    const file = withComposition("dome.json", (c) => {
      // (DC: Repeat up 2 - its v tiles, so its soft texture is its colour's own, as before DC; within one repeat a still
      // dome's is clamped, its own name and TXI: DC's check, composer check_build.)
      c.stages.medium = [{ shape: { type: "dome", radius: 0.3, colour, repeat_up: 2 }, at: [0, 0, 0] }, { model: "ptm_lantern03", at: [0.2, 0, 0], scale: 0.2 }];
    });
    const drafts = path.join(root, "build", "saved"), library = path.join(root, "build", "textures");
    const args = ["--composition", file, "--drafts", "--out", drafts, "--textures", library];
    const made = fs.readFileSync(path.join(folder, "made.txt"));
    // --check: a summary line for it, where it goes.
    const c = build(["--composition", file, "--check"]);
    assert.strictEqual(c.code, 0, "build.js --check exited " + c.code + ": " + c.err.trim());
    assert.ok(c.out.split(/\r?\n/).includes(`  texture ${GOLD}: a shape's soft texture, colour 1 0.85 0.4 - written into data/files`), "no summary line:\n" + c.out);
    // --json's shapes; written into data\files and beside the drafts, floatdisk.js's soft texture both.
    const had = fs.existsSync(path.join(FILES, GOLD + ".tga"));  // a build's copy from the real data\files (copied above)
    const r = build([...args, "--json"]);
    assert.strictEqual(r.code, 0, "build.js exited " + r.code + ": " + r.err.trim());
    const j = json(r);
    assert.deepStrictEqual(j.shapes, [{ texture: GOLD, colour }]);
    assert.deepStrictEqual(j.stages.medium.list.map((x) => x.label), ["dome 0.3 m", "ptm_lantern03"]);
    const states = Object.fromEntries(j.files.map((f) => [f.file, f.state]));
    assert.deepStrictEqual([states[`data/files/${GOLD}.tga`], states[`build/saved/${GOLD}.tga`]], [had ? "unchanged" : "new", "new"]);
    const bytes = F.tgaBytes(F.shapeTexture({ colour }), { alpha: true });
    for (const dir of [FILES, drafts]) assert.ok(fs.readFileSync(path.join(dir, GOLD + ".tga")).equals(bytes), `${dir}\\${GOLD}.tga isn't floatdisk.js's soft texture`);
    // The medium's creature and pile name it, the dome's mesh see-through; the medium's draft names it.
    for (const m of [F.creatureModel("medium"), F.pileModel("medium")].map((s) => s + ".mdl")) {
      const node = fs.readFileSync(path.join(FILES, m), "latin1").split("\r\nnode ").find((n) => n.includes(`\r\n  bitmap ${GOLD}\r\n`));
      assert.ok(node, m + " doesn't name " + GOLD);
      assert.ok(node.includes("\r\n  transparencyhint 1\r\n"), m + ": the dome's mesh isn't see-through");
    }
    const core = (global.window = global.window || {}, require(path.join(TOOLS, "vfx_bench", "public", "core.js")));
    const planes = core.readDraft(fs.readFileSync(path.join(drafts, "n5_floatdisk_medium.vfx"), "latin1").replace(/\r/g, "")).planes;
    assert.ok(planes.some((p) => p.bitmap === GOLD), "the medium's draft doesn't name " + GOLD);
    // T1b: the draft's planes see-through as the models' meshes are (meshHint): the dome's and the disk's 1.
    assert.deepStrictEqual(planes.filter((p) => p.bitmap === GOLD || p.name === "forcedisk").map((p) => [p.name, p.transparencyhint]),
      [["j01_dome", 1], ["forcedisk", 1]]);
    // Each draft plane's hint is its mesh's "transparencyhint" line in the medium's creature model, name by name - the
    // lantern's own 2 and 1 too, where a draft falling back to see-through-ness would write 0 or 1.
    const lines = Object.fromEntries(fs.readFileSync(path.join(FILES, F.creatureModel("medium") + ".mdl"), "latin1").split("\r\nnode trimesh ").slice(1)
      .map((n) => {
        const l = n.split("\r\n"), hint = l.filter((x) => x.startsWith("  transparencyhint "));
        assert.strictEqual(hint.length, 1, l[0] + ": " + hint.length + " transparencyhint lines");
        return [l[0], Number(hint[0].slice(19))];
      }));
    assert.deepStrictEqual(Object.fromEntries(planes.map((p) => [p.name, p.transparencyhint])), lines);
    const lantern = Object.entries(lines).filter(([name]) => /_lntrn3$/.test(name)).map(([, hint]) => hint);
    assert.deepStrictEqual([lantern.length, lantern.filter((x) => x === 2).length, lantern.filter((x) => x === 1).length], [34, 6, 6]);
    // Nothing noted in made.txt.
    assert.ok(fs.readFileSync(path.join(folder, "made.txt")).equals(made), "made.txt changed");
    // Built again: every file unchanged. A file of its name, either place, is the generator's (the name fixes its
    // bytes): rewritten.
    const again = json(build([...args, "--json"]));
    assert.ok(again.files.every((f) => f.state === "unchanged"), JSON.stringify(again.files.filter((f) => f.state !== "unchanged")));
    for (const dir of [FILES, drafts]) fs.writeFileSync(path.join(dir, GOLD + ".tga"), "not a texture");
    const fixed = json(build([...args, "--json"])).files.filter((f) => f.state !== "unchanged");
    assert.deepStrictEqual(fixed, [{ state: "rebuilt", file: `data/files/${GOLD}.tga` }, { state: "rebuilt", file: `build/saved/${GOLD}.tga` }]);
    for (const dir of [FILES, drafts]) assert.ok(fs.readFileSync(path.join(dir, GOLD + ".tga")).equals(bytes));
  });

  check("a soft-additive dome (M1b): its TXI beside its TGA in data\\files and beside the drafts, owned by its name; a Normal one's none; the names differ", () => {
    // The check's own medium: a 0.3 m gold dome, Additive, and a 0.2 m one in the same gold, Normal - two textures, one
    // TXI ("blending additive", floatdisk.js's SOFT_TXI: the game's additive path), each beside what names it.
    const GOLD = "n5s_a3caa0709dda", ADD = "n5s_438a32a94ebf", colour = [1, 0.85, 0.4];
    assert.strictEqual(F.shapeTextureName({ colour, blend: "additive" }), ADD);
    const file = withComposition("blend.json", (c) => {
      // (DC: both Repeat up 2, as the check above's.)
      c.stages.medium = [{ shape: { type: "dome", radius: 0.3, colour, blend: "additive", repeat_up: 2 }, at: [0, 0, 0] },
        { shape: { type: "dome", radius: 0.2, colour, repeat_up: 2 }, at: [0.4, 0, 0] }];
    });
    const drafts = path.join(root, "build", "saved"), library = path.join(root, "build", "textures");
    const args = ["--composition", file, "--drafts", "--out", drafts, "--textures", library];
    for (const dir of [FILES, drafts]) for (const f of [GOLD, ADD].flatMap((n) => [n + ".tga", n + ".txi"])) fs.rmSync(path.join(dir, f), { force: true });
    const made = fs.readFileSync(path.join(folder, "made.txt"));
    const c = build(["--composition", file, "--check"]);
    assert.strictEqual(c.code, 0, "build.js --check exited " + c.code + ": " + c.err.trim());
    assert.ok(c.out.split(/\r?\n/).includes(`  texture ${ADD}: a shape's soft texture, colour 1 0.85 0.4, additive (${ADD}.txi beside it: blending additive) - written into data/files`),
      "no summary line:\n" + c.out);
    const r = build([...args, "--json"]);
    assert.strictEqual(r.code, 0, "build.js exited " + r.code + ": " + r.err.trim());
    const j = json(r);
    assert.deepStrictEqual(j.shapes, [{ texture: ADD, colour, blend: "additive" }, { texture: GOLD, colour }]);
    const states = Object.fromEntries(j.files.filter((f) => /n5s_/.test(f.file)).map((f) => [f.file, f.state]));
    assert.deepStrictEqual(states, { [`data/files/${ADD}.tga`]: "new", [`data/files/${ADD}.txi`]: "new", [`data/files/${GOLD}.tga`]: "new",
      [`build/saved/${ADD}.tga`]: "new", [`build/saved/${ADD}.txi`]: "new", [`build/saved/${GOLD}.tga`]: "new" });
    const bytes = F.tgaBytes(F.shapeTexture({ colour }), { alpha: true });
    for (const dir of [FILES, drafts]) {
      assert.strictEqual(fs.readFileSync(path.join(dir, ADD + ".txi"), "latin1"), "blending additive\n", dir);
      for (const n of [ADD, GOLD]) assert.ok(fs.readFileSync(path.join(dir, n + ".tga")).equals(bytes), `${dir}\\${n}.tga isn't floatdisk.js's soft texture`);
      assert.ok(!fs.existsSync(path.join(dir, GOLD + ".txi")), `${dir}: a Normal soft texture got a TXI`);
    }
    // The medium's creature model and draft name each dome's own.
    const mdl = fs.readFileSync(path.join(FILES, F.creatureModel("medium") + ".mdl"), "latin1");
    for (const n of [ADD, GOLD]) assert.ok(mdl.includes(`\r\n  bitmap ${n}\r\n`), "the medium's model doesn't name " + n);
    const core = (global.window = global.window || {}, require(path.join(TOOLS, "vfx_bench", "public", "core.js")));
    const planes = core.readDraft(fs.readFileSync(path.join(drafts, "n5_floatdisk_medium.vfx"), "latin1").replace(/\r/g, "")).planes;
    assert.deepStrictEqual(planes.filter((p) => /^n5s_/.test(p.bitmap)).map((p) => p.bitmap), [ADD, GOLD]);
    assert.ok(fs.readFileSync(path.join(folder, "made.txt")).equals(made), "made.txt changed");
    // Built again: unchanged; a TXI of its name is the generator's - rewritten.
    const again = json(build([...args, "--json"]));
    assert.ok(again.files.every((f) => f.state === "unchanged"), JSON.stringify(again.files.filter((f) => f.state !== "unchanged")));
    for (const dir of [FILES, drafts]) fs.writeFileSync(path.join(dir, ADD + ".txi"), "blending punchthrough\r\n");
    const fixed = json(build([...args, "--json"])).files.filter((f) => f.state !== "unchanged");
    assert.deepStrictEqual(fixed, [{ state: "rebuilt", file: `data/files/${ADD}.txi` }, { state: "rebuilt", file: `build/saved/${ADD}.txi` }]);
  });

  // M2 (#39): a moving part's textured meshes name a material, n5u_<12 hex digits> (floatdisk.js motionName), written
  // beside what names them with the shader it names - vsn5uvmove.shd, committed in data\files and listed in made.txt (the
  // disk's own build, with no motion, never calls it unused).
  check("a moving dome part (M2): its material and the shader beside the models and the drafts, which name the material; the shader never unused", () => {
    const GOLD = "n5s_a3caa0709dda", colour = [1, 0.85, 0.4], motion = { turn: 30 }, pivot = [0.5, 0.5], MAT = F.motionName(GOLD, motion, pivot);
    const SHADER = "vsn5uvmove.shd", shaderBytes = Buffer.from(F.crlf(F.motionShaderText()), "latin1");
    const file = withComposition("uv.json", (c) => {
      c.stages.medium = [{ shape: { type: "dome", radius: 0.3, colour, mapping: "above" }, at: [0, 0, 0], motion }];
    });
    const drafts = path.join(root, "build", "saved"), library = path.join(root, "build", "textures");
    const args = ["--composition", file, "--drafts", "--out", drafts, "--textures", library];
    // The shader as the checkout commits it (made.txt lists it); the copy's data\files gets it as the real one has it.
    assert.ok(fs.readFileSync(path.join(folder, "made.txt"), "utf8").split(/\r?\n/).includes("data/files/" + SHADER), "made.txt doesn't list the shader");
    fs.copyFileSync(path.join(REAL_FILES, SHADER), path.join(FILES, SHADER));
    const made = fs.readFileSync(path.join(folder, "made.txt"));
    const plain = build([]);
    assert.strictEqual(plain.code, 0, plain.err);
    assert.ok(!/no longer used/.test(plain.out), "the disk's own build calls something unused:\n" + plain.out);
    const c = build(["--composition", file, "--check"]);
    assert.ok(c.out.split(/\r?\n/).includes(`  material ${MAT}: ${GOLD} turns 30 degrees a second, round 0.5 0.5 - written into data/files, with ${SHADER}`), "no summary line:\n" + c.out);
    const r = build([...args, "--json"]);
    assert.strictEqual(r.code, 0, "build.js exited " + r.code + ": " + r.err.trim());
    const j = json(r);
    assert.deepStrictEqual(j.materials, [{ material: MAT, texture: GOLD, motion, pivot }]);
    const states = Object.fromEntries(j.files.filter((f) => /n5u_|\.shd$/.test(f.file)).map((f) => [f.file, f.state]));
    assert.deepStrictEqual(states, { [`data/files/${MAT}.mtr`]: "new", [`data/files/${SHADER}`]: "unchanged", [`build/saved/${MAT}.mtr`]: "new", [`build/saved/${SHADER}`]: "new" });
    for (const dir of [FILES, drafts]) {
      assert.strictEqual(fs.readFileSync(path.join(dir, MAT + ".mtr"), "latin1"), F.motionMaterialText({ texture: GOLD, motion, pivot }), dir);
      assert.ok(fs.readFileSync(path.join(dir, SHADER)).equals(shaderBytes), dir);
      assert.ok(fs.existsSync(path.join(dir, GOLD + ".tga")), `${dir}: no ${GOLD}.tga (the material's texture0)`);
    }
    // The medium's creature and pile name the material, not the texture; so does its draft.
    for (const m of [F.creatureModel("medium"), F.pileModel("medium")].map((s) => s + ".mdl")) {
      const t = fs.readFileSync(path.join(FILES, m), "latin1");
      assert.ok(t.includes(`\r\n  bitmap ${MAT}\r\n`) && !t.includes(`\r\n  bitmap ${GOLD}\r\n`), m + " doesn't name " + MAT);
    }
    const core = (global.window = global.window || {}, require(path.join(TOOLS, "vfx_bench", "public", "core.js")));
    const planes = core.readDraft(fs.readFileSync(path.join(drafts, "n5_floatdisk_medium.vfx"), "latin1").replace(/\r/g, "")).planes;
    assert.ok(planes.some((p) => p.bitmap === MAT) && !planes.some((p) => p.bitmap === GOLD), planes.map((p) => p.bitmap).join(" "));
    assert.ok(fs.readFileSync(path.join(folder, "made.txt")).equals(made), "made.txt changed");
    assert.ok(!/no longer used/.test(build(["--composition", file]).out), "a moving build calls something unused");
    // Built again: unchanged.
    const again = json(build([...args, "--json"]));
    assert.ok(again.files.every((f) => f.state === "unchanged"), JSON.stringify(again.files.filter((f) => f.state !== "unchanged")));
  });

  // W1 (#39, the design's addendum (c); TMO-2): a breathing texture that doesn't scroll, on a mesh within one repeat, is
  // clamped - its material (its own name) names its copy, the picture copied under the material's name with a TXI of its
  // lines and the game's clamp line, beside the models in data\files and beside the drafts alike; made.txt doesn't list them.
  check("W1: a breathing From-above dome part is clamped - its copy and TXI (clamp 3) beside the models and the drafts, the material naming the copy; again unchanged", () => {
    const GOLD = "n5s_a3caa0709dda", colour = [1, 0.85, 0.4], motion = { breathe: 0.2 }, pivot = [0.5, 0.5], MAT = F.motionName(GOLD, motion, pivot, true);
    const file = withComposition("clamp.json", (c) => {
      c.stages.medium = [{ shape: { type: "dome", radius: 0.3, colour, mapping: "above" }, at: [0, 0, 0], motion }];
    });
    const drafts = path.join(root, "build", "saved"), library = path.join(root, "build", "textures");
    const args = ["--composition", file, "--drafts", "--out", drafts, "--textures", library];
    const made = fs.readFileSync(path.join(folder, "made.txt"));
    const r = build([...args, "--json"]);
    assert.strictEqual(r.code, 0, "build.js exited " + r.code + ": " + r.err.trim());
    const j = json(r);
    assert.deepStrictEqual(j.materials, [{ material: MAT, texture: GOLD, motion, pivot, clamp: true }]);
    const states = Object.fromEntries(j.files.filter((f) => f.file.includes(MAT)).map((f) => [f.file, f.state]));
    assert.deepStrictEqual(Object.keys(states).sort(), ["build/saved/", "data/files/"].flatMap((d) => [".mtr", ".tga", ".txi"].map((e) => d + MAT + e)).sort());
    const soft = F.tgaBytes(F.shapeTexture({ colour }), { alpha: true });
    for (const dir of [FILES, drafts]) {
      assert.strictEqual(fs.readFileSync(path.join(dir, MAT + ".mtr"), "latin1"), F.motionMaterialText({ texture: GOLD, motion, pivot, clamp: true }), dir);
      assert.ok(fs.readFileSync(path.join(dir, MAT + ".tga")).equals(soft), dir + ": the copy isn't the soft texture");
      assert.strictEqual(fs.readFileSync(path.join(dir, MAT + ".txi"), "latin1"), "clamp 3\n", dir);
    }
    for (const m of [F.creatureModel("medium"), F.pileModel("medium")].map((s) => s + ".mdl")) {
      assert.ok(fs.readFileSync(path.join(FILES, m), "latin1").includes(`\r\n  bitmap ${MAT}\r\n`), m + " doesn't name " + MAT);
    }
    assert.ok(fs.readFileSync(path.join(folder, "made.txt")).equals(made), "made.txt changed");
    assert.ok(!/no longer used/.test(build(["--composition", file]).out), "a clamped build calls something unused");
    const again = json(build([...args, "--json"]));
    assert.ok(again.files.every((f) => f.state === "unchanged"), JSON.stringify(again.files.filter((f) => f.state !== "unchanged")));
  });

  // DC (#39; SHP-8, the owner, 2026-11-19: "Go"): a still shape's library texture whose TXI says decal (or a flipbook on a
  // mesh within one repeat) is copied under its n5l_ name - its picture and a TXI of its lines minus decal, + clamp 2 within
  // one repeat - beside the models in data\files and beside the drafts alike; made.txt doesn't list them.
  check("DC: a still flame ring part (fxpa_flame02, Cylinder with a Top radius) names its copy - the game's TGA and a TXI minus decal + clamp 2 beside the models and the drafts; again unchanged", () => {
    global.window = global.window || {};
    const rd = R.create({ game: GAME, core: require(path.join(TOOLS, "vfx_bench", "public", "core.js")), gameData: require(path.join(TOOLS, "spell_bench", "game_data.js")), reader: null });
    const FLAME = "fxpa_flame02", flame = rd.image(FLAME, []);
    // (Its name hashes its key: the game texture's TXI text and its picture's bytes - floatdisk.js copyKey.)
    const C2 = F.copyName(FLAME, 2, F.copyKey(flame.txi, flame.dds || flame.tga));
    const file = withComposition("copy.json", (c) => {
      c.stages.medium = [{ shape: { type: "cylinder", radius: 0.3, height: 0.3, top: 0.35, texture: FLAME, colour: [0.137, 0.922, 0.314] }, at: [0, 0, 0] }];
    });
    const drafts = path.join(root, "build", "saved"), library = path.join(root, "build", "textures");
    const args = ["--composition", file, "--drafts", "--out", drafts, "--textures", library];
    const made = fs.readFileSync(path.join(folder, "made.txt"));
    const r = build([...args, "--json"]);
    assert.strictEqual(r.code, 0, "build.js exited " + r.code + ": " + r.err.trim());
    const j = json(r);
    assert.deepStrictEqual(j.copies, [{ copy: C2, texture: FLAME, clamp: 2 }]);
    const states = Object.fromEntries(j.files.filter((f) => f.file.includes(C2)).map((f) => [f.file, f.state]));
    assert.deepStrictEqual(Object.keys(states).sort(), ["build/saved/", "data/files/"].flatMap((d) => [".tga", ".txi"].map((e) => d + C2 + e)).sort());
    assert.ok(flame && flame.tga && F.txiDecal(flame.txi), "the game's fxpa_flame02: a TGA, a decal TXI");
    for (const dir of [FILES, drafts]) {
      assert.ok(fs.readFileSync(path.join(dir, C2 + ".tga")).equals(flame.tga), dir + ": the copy isn't the game's fxpa_flame02.tga");
      assert.strictEqual(fs.readFileSync(path.join(dir, C2 + ".txi"), "latin1"), F.copiedTxi(flame.txi, 2), dir);
    }
    for (const m of [F.creatureModel("medium"), F.pileModel("medium")].map((s) => s + ".mdl")) {
      const t = fs.readFileSync(path.join(FILES, m), "latin1");
      assert.ok(t.includes(`\r\n  bitmap ${C2}\r\n`) && !t.includes(`\r\n  bitmap ${FLAME}\r\n`), m + " doesn't name " + C2);
    }
    assert.ok(fs.readFileSync(path.join(folder, "made.txt")).equals(made), "made.txt changed");
    assert.ok(!/no longer used/.test(build(["--composition", file]).out), "a copying build calls something unused");
    const again = json(build([...args, "--json"]));
    assert.ok(again.files.every((f) => f.state === "unchanged"), JSON.stringify(again.files.filter((f) => f.state !== "unchanged")));
  });

  // MV (#39; the owner, 2026-11-19: "go"): a moving shape's texture0 is the still one's - a decal flipbook's copy, clamped
  // in v only while it moves sideways only - and its material has a TXI of its own, its texture's blending line (the engine
  // takes a mesh's blend only from its bitmap's TXI), beside the models and the drafts alike.
  check("MV: a flame ring part scrolling round (fxpa_flame02, Scroll round 0.5) - its material names the still ring's copy (clamp 2) as texture0, its own TXI (blending additive) beside it; the models and the drafts name the material; again unchanged", () => {
    global.window = global.window || {};
    const rd = R.create({ game: GAME, core: require(path.join(TOOLS, "vfx_bench", "public", "core.js")), gameData: require(path.join(TOOLS, "spell_bench", "game_data.js")), reader: null });
    const FLAME = "fxpa_flame02", flame = rd.image(FLAME, []), C2 = F.copyName(FLAME, 2, F.copyKey(flame.txi, flame.dds || flame.tga));
    const motion = { scroll: [0.5, 0] }, pivot = [0.5, 0.5], TXI = "blending additive\n", MAT = F.motionName(C2, motion, pivot, false, { side: true, txi: TXI });
    const file = withComposition("mv.json", (c) => {
      c.stages.medium = [{ shape: { type: "cylinder", radius: 0.3, height: 0.3, top: 0.35, texture: FLAME, colour: [0.137, 0.922, 0.314] }, at: [0, 0, 0], motion }];
    });
    const drafts = path.join(root, "build", "saved"), library = path.join(root, "build", "textures");
    const args = ["--composition", file, "--drafts", "--out", drafts, "--textures", library];
    const made = fs.readFileSync(path.join(folder, "made.txt"));
    const r = build([...args, "--json"]);
    assert.strictEqual(r.code, 0, "build.js exited " + r.code + ": " + r.err.trim());
    const j = json(r);
    assert.deepStrictEqual(j.copies, [{ copy: C2, texture: FLAME, clamp: 2 }]);
    assert.deepStrictEqual(j.materials, [{ material: MAT, texture: C2, motion, pivot, side: true, txi: TXI }]);
    for (const dir of [FILES, drafts]) {
      assert.strictEqual(fs.readFileSync(path.join(dir, MAT + ".mtr"), "latin1"), F.motionMaterialText({ texture: C2, motion, pivot }), dir);
      assert.strictEqual(fs.readFileSync(path.join(dir, MAT + ".txi"), "latin1"), TXI, dir);
      assert.ok(fs.readFileSync(path.join(dir, C2 + ".tga")).equals(flame.tga), dir + ": the copy isn't the game's fxpa_flame02.tga");
      assert.strictEqual(fs.readFileSync(path.join(dir, C2 + ".txi"), "latin1"), F.copiedTxi(flame.txi, 2), dir);
    }
    for (const m of [F.creatureModel("medium"), F.pileModel("medium")].map((s) => s + ".mdl")) {
      const t = fs.readFileSync(path.join(FILES, m), "latin1");
      assert.ok(t.includes(`\r\n  bitmap ${MAT}\r\n`) && !t.includes(`\r\n  bitmap ${FLAME}\r\n`) && !t.includes(`\r\n  bitmap ${C2}\r\n`), m + " doesn't name " + MAT);
    }
    assert.ok(fs.readFileSync(path.join(folder, "made.txt")).equals(made), "made.txt changed");
    assert.ok(!/no longer used/.test(build(["--composition", file]).out), "a moving copy's build calls something unused");
    const again = json(build([...args, "--json"]));
    assert.ok(again.files.every((f) => f.state === "unchanged"), JSON.stringify(again.files.filter((f) => f.state !== "unchanged")));
  });

  // AB (#39; the owner, 2026-11-23: a game weapon glowing as the generated blade does - "set it up and make 1 handed swords
  // the first weapon"): an Additive part on the disk - shortsword 012 (one texture, w_metal_tex, its TXI an environment
  // map) - lit by itself at its tint, naming its texture's copy made for added light; a breathing one (its UVs within one
  // repeat: W1) naming clamped materials whose copies are of that copy, its TXI's blending additive kept.
  check("AB: an Additive sword part - lit by itself at its tint, naming w_metal_tex's copy made for added light (its TXI blending additive) beside the models and the drafts; a breathing one's W1 copies of its copy (+ clamp 3); again unchanged", () => {
    global.window = global.window || {};
    const rd = R.create({ game: GAME, core: require(path.join(TOOLS, "vfx_bench", "public", "core.js")), gameData: require(path.join(TOOLS, "spell_bench", "game_data.js")), reader: null });
    const METAL = "w_metal_tex", metal = rd.image(METAL, []), A = F.copyName(METAL, 0, F.copyKey(metal.txi, metal.dds || metal.tga), true);
    const SWORD = ["wswss_b_012", "wswss_m_012", "wswss_t_012"], TINT = [0.72, 0.86, 1];
    const file = withComposition("ab.json", (c) => {
      c.stages.small = [{ model: SWORD, at: [0, 0, 0], scale: 0.3, tint: TINT, blend: "additive" }];
      c.stages.medium = [{ model: SWORD, at: [0, 0, 0], scale: 0.3, blend: "additive", motion: { breathe: 0.3 } }];
    });
    const drafts = path.join(root, "build", "saved"), library = path.join(root, "build", "textures");
    const args = ["--composition", file, "--drafts", "--out", drafts, "--textures", library];
    const r = build([...args, "--json"]);
    assert.strictEqual(r.code, 0, "build.js exited " + r.code + ": " + r.err.trim());
    const j = json(r);
    assert.deepStrictEqual(j.copies, [{ copy: A, texture: METAL, clamp: 0, additive: true }]);
    assert.ok(j.materials.length === 3 && j.materials.every((m) => m.texture === A && m.clamp === true), JSON.stringify(j.materials));
    for (const dir of [FILES, drafts]) {
      assert.ok(fs.readFileSync(path.join(dir, A + ".tga")).equals(metal.tga), dir + ": the copy isn't the game's w_metal_tex.tga");
      assert.strictEqual(fs.readFileSync(path.join(dir, A + ".txi"), "latin1"), F.additiveTxi(metal.txi), dir);
      for (const m of j.materials) {
        assert.strictEqual(fs.readFileSync(path.join(dir, m.material + ".txi"), "latin1"), F.clampedTxi(F.additiveTxi(metal.txi)), dir + " " + m.material);
        assert.ok(fs.readFileSync(path.join(dir, m.material + ".tga")).equals(metal.tga), dir + ": " + m.material + ".tga isn't the game's w_metal_tex.tga");
      }
    }
    // The small stage's creature: the copy, lit by itself at the tint (a model's material: written as it is).
    const small = fs.readFileSync(path.join(FILES, F.creatureModel("small") + ".mdl"), "latin1");
    assert.strictEqual(small.split(`\r\n  bitmap ${A}\r\n`).length - 1, 3, F.creatureModel("small") + " doesn't name " + A + " three times");
    assert.strictEqual(small.split("\r\n  selfillumcolor 0.72 0.86 1\r\n").length - 1, 3, "its selfillumcolor isn't the tint");
    assert.ok(!small.includes(`\r\n  bitmap ${METAL}\r\n`), "the game's own texture still named");
    const medium = fs.readFileSync(path.join(FILES, F.creatureModel("medium") + ".mdl"), "latin1");
    for (const m of j.materials) assert.ok(medium.includes(`\r\n  bitmap ${m.material}\r\n`), F.creatureModel("medium") + " doesn't name " + m.material);
    const again = json(build([...args, "--json"]));
    assert.ok(again.files.every((f) => f.state === "unchanged"), JSON.stringify(again.files.filter((f) => f.state !== "unchanged")));
  });

  // TX (#39; the owner, 2026-11-23: "cas we just use the mesh, change the texture to something white and tint that?"): a
  // Greyscale part on the disk - its texture's grey copy (its decoded pixels, each its luminance, its alpha kept) beside the
  // models and the drafts; a White one the plain white.
  check("TX: a Greyscale sword part names w_metal_tex's grey copy, a White one the plain white - beside the models and the drafts, the copy w_metal_tex in grey; again unchanged", () => {
    const core = (global.window = global.window || {}, require(path.join(TOOLS, "vfx_bench", "public", "core.js")));
    const rd = R.create({ game: GAME, core, gameData: require(path.join(TOOLS, "spell_bench", "game_data.js")), reader: null });
    const METAL = "w_metal_tex", metal = rd.image(METAL, []), G = F.copyName(METAL, 0, F.copyKey(metal.txi, metal.dds || metal.tga), { look: "greyscale" }), WN = F.whiteName();
    const SWORD = ["wswss_b_012", "wswss_m_012", "wswss_t_012"];
    const file = withComposition("tx.json", (c) => {
      c.stages.small = [{ model: SWORD, at: [0, 0, 0], scale: 0.3, tint: [0.72, 0.86, 1], texture: "greyscale" }];
      c.stages.medium = [{ model: SWORD, at: [0, 0, 0], scale: 0.3, texture: "white" }];
    });
    const drafts = path.join(root, "build", "saved"), library = path.join(root, "build", "textures");
    const args = ["--composition", file, "--drafts", "--out", drafts, "--textures", library];
    const r = build([...args, "--json"]);
    assert.strictEqual(r.code, 0, "build.js exited " + r.code + ": " + r.err.trim());
    const j = json(r);
    assert.deepStrictEqual(j.copies, [{ copy: G, texture: METAL, clamp: 0, look: "greyscale" }, { copy: WN, white: true }]);
    for (const dir of [FILES, drafts]) {
      assert.ok(fs.readFileSync(path.join(dir, G + ".tga")).equals(F.lookTga("greyscale", core.decodeTga(metal.tga))), dir + ": the copy isn't w_metal_tex in grey");
      assert.strictEqual(fs.readFileSync(path.join(dir, G + ".txi"), "latin1"), F.copiedTxi(metal.txi, 0), dir);
      assert.ok(fs.readFileSync(path.join(dir, WN + ".tga")).equals(F.tgaBytes(F.whiteTexture())), dir + ": not the plain white");
    }
    const small = fs.readFileSync(path.join(FILES, F.creatureModel("small") + ".mdl"), "latin1"), medium = fs.readFileSync(path.join(FILES, F.creatureModel("medium") + ".mdl"), "latin1");
    assert.deepStrictEqual([small.split(`\r\n  bitmap ${G}\r\n`).length - 1, medium.split(`\r\n  bitmap ${WN}\r\n`).length - 1], [3, 3]);
    const again = json(build([...args, "--json"]));
    assert.ok(again.files.every((f) => f.state === "unchanged"), JSON.stringify(again.files.filter((f) => f.state !== "unchanged")));
  });

  check("the real worktree's data\\files and the real build's vfxedit\\saved\\ are as they were", () => {
    assert.deepStrictEqual(realState(), REAL);
  });
} finally {
  fs.rmSync(root, { recursive: true, force: true });
}

console.log(failed ? `build check: ${failed} failed` : "build check: all pass");
process.exitCode = failed ? 1 : 0;
