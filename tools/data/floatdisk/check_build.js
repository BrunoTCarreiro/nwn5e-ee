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
  if (/^(c_n5fd_|plc_n5fd_)/.test(f) || f === "n5t_forcedisk.tga" || /^n5[ps]_[0-9a-f]{12}\.tga$/.test(f)) fs.copyFileSync(path.join(REAL_FILES, f), path.join(FILES, f));
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
const GENERATED_FILE = /^data\/files\/n5[ps]_[0-9a-f]{12}\.tga$/;  // a bake or a shape's soft texture

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
    assert.deepStrictEqual(made.map(([, f]) => path.posix.basename(f, ".tga")).sort(), generated(build(["--check", "--json"])).sort());
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
      c.stages.medium = [{ shape: { type: "dome", radius: 0.3, colour }, at: [0, 0, 0] }, { model: "ptm_lantern03", at: [0.2, 0, 0], scale: 0.2 }];
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

  check("the real worktree's data\\files and the real build's vfxedit\\saved\\ are as they were", () => {
    assert.deepStrictEqual(realState(), REAL);
  });
} finally {
  fs.rmSync(root, { recursive: true, force: true });
}

console.log(failed ? `build check: ${failed} failed` : "build check: all pass");
process.exitCode = failed ? 1 : 0;
