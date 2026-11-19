#!/usr/bin/env node
// build_derived.js end to end (D1), on temporary copies only - needs the game, prints "skip" without it:
//   node tools\data\check_derived.js [--game <folder>] [--tools <nwn5e-tools>]
// A dyed part's baked texture (n5p_*) and a shape's soft texture (n5s_*) - build output, both git-ignored - come out
// with their models, the same bytes build after build.
// The copy is <tmp>\n5_derived-*\: build_derived.js and the generators' code (*.js, *.bat, *.cpp) from this worktree;
// everything else under tools\data\floatdisk and \composer - the recipes (composition.json, the projects), made.txt -
// and data\rows, data\looks from HEAD as git checks them out (git cat-file --filters), never the worktree's: the owner's
// uncommitted Composer edits are not the baseline. data\files is empty; the build's folder is <copy>\build\data\files.
// The real worktree's data\ and tools\data\ are never written - the last check says so.
"use strict";
const assert = require("assert");
const crypto = require("crypto");
const fs = require("fs");
const os = require("os");
const path = require("path");
const { spawnSync } = require("child_process");
const F = require("./floatdisk/floatdisk.js");
const P = require("./composer/project.js");
const R = require("./floatdisk/reader.js");

const REPO = path.resolve(__dirname, "..", "..");
const argv = process.argv.slice(2);
const option = (name, fallback) => {
  const i = argv.indexOf("--" + name);
  return i >= 0 && i + 1 < argv.length ? argv[i + 1] : fallback;
};
const TOOLS = path.resolve(option("tools", process.env.NWN5E_TOOLS || path.join(REPO, "..", "nwn5e-tools")));
const GAME = require(path.join(TOOLS, "anim_helper", "helper.js")).findGameDir({ game: option("game", ""), buildDir: path.join(TOOLS, "..", "nwn5e-ee", "build") });
if (!fs.existsSync(path.join(GAME, "data", "nwn_base.key"))) {
  console.log("skip: no game at " + GAME);
  process.exit(0);
}
const READER = R.ensureReader({ repo: REPO });
const sha1 = (bytes) => crypto.createHash("sha1").update(bytes).digest("hex");

// --- the real worktree, before ------------------------------------------------------------------------------------------
// composition.json's hash; data\files', data\rows', and tools\data\floatdisk's and \composer's files' mtimes and sizes.
function worktreeState() {
  const s = { "composition.json sha1": sha1(fs.readFileSync(path.join(REPO, "tools", "data", "floatdisk", "composition.json"))) };
  for (const dir of [["data", "files"], ["data", "rows"], ["tools", "data", "floatdisk"], ["tools", "data", "composer"]]) {
    const at = path.join(REPO, ...dir);
    for (const f of fs.readdirSync(at)) {
      const st = fs.statSync(path.join(at, f));
      if (st.isFile()) s[dir.join("/") + "/" + f] = st.mtimeMs + ":" + st.size;
    }
  }
  return s;
}
const WORKTREE = worktreeState();

// --- HEAD -----------------------------------------------------------------------------------------------------------------
function git(args, encoding = "buffer") {
  const r = spawnSync("git", args, { cwd: REPO, encoding, maxBuffer: 256 << 20, windowsHide: true });
  if (r.status !== 0) throw new Error(`git ${args.join(" ")}: ${String(r.stderr).trim()}`);
  return r.stdout;
}
const headFiles = (dir) => git(["ls-tree", "-r", "--name-only", "HEAD", "--", dir + "/"], "utf8").split(/\r?\n/).filter(Boolean);
const checkedOut = (rel) => git(["cat-file", "--filters", "HEAD:" + rel]);  // HEAD's copy as git checks it out here
const stored = (rel) => git(["show", "HEAD:" + rel]);  // HEAD's blob as git stores it
const lf = (bytes) => Buffer.from(bytes.toString("latin1").replace(/\r\n/g, "\n"), "latin1");

// HEAD's data\files that a generator made: a model or walkmesh carrying its mark, a texture its made.txt lists.
function markedAtHead(mark) {
  const r = spawnSync("git", ["grep", "-l", "-F", "-e", mark, "HEAD", "--", "data/files/"], { cwd: REPO, encoding: "utf8", windowsHide: true });
  if (r.status === 1) return [];
  if (r.status !== 0) throw new Error("git grep: " + r.stderr.trim());
  return r.stdout.split(/\r?\n/).filter(Boolean).map((l) => l.replace(/^HEAD:/, "")).filter((rel) => F.carriesMark(stored(rel), mark));
}
// The moving textures' shader (M2), data/files/vsn5uvmove.shd, is in the Floating Disk's made.txt but committed whole -
// original text, which build.bat's copy of data\ takes into the build; build_derived writes it only beside a moving
// texture's material - so it's neither one of the disk's files nor one a build of the committed recipes must make.
const SHADER = F.MOTION_SHADER + ".shd";
function madeAtHead(generator) {
  const rel = `tools/data/${generator}/made.txt`;
  if (!headFiles(`tools/data/${generator}`).includes(rel)) return [];
  return checkedOut(rel).toString("utf8").split(/\r?\n/).map((l) => l.trim()).filter((l) => l.startsWith("data/files/") && path.posix.basename(l) !== SHADER);
}
const FLOATDISK_AT_HEAD = [...markedAtHead(F.MARK), ...madeAtHead("floatdisk")].map((rel) => path.posix.basename(rel));
const EXPECTED = new Map();  // name -> HEAD's data\files copy, as checked out
for (const rel of [...markedAtHead(F.MARK), ...madeAtHead("floatdisk"), ...markedAtHead(P.MARK), ...madeAtHead("composer")]) {
  EXPECTED.set(path.posix.basename(rel), checkedOut(rel));
}
// The Floating Disk's files. Its models made from the game's files are never committed (the owner, 2026-11-17: "go with
// 1, remove the six files from main", then "go with your recs for the three questions"): the six of the game's meshes,
// and c_n5fd_empty.mdl - its disk original, but carrying c_invsguy's node names and positions. Git-ignored, built at
// build time. The rest is committed: the .pwk files and the texture, made from scratch.
const FLOATDISK = ["c_n5fd_empty.mdl", "c_n5fd_large.mdl", "c_n5fd_med.mdl", "c_n5fd_small.mdl", "n5t_forcedisk.tga",
  "plc_n5fd_large.mdl", "plc_n5fd_large.pwk", "plc_n5fd_med.mdl", "plc_n5fd_med.pwk", "plc_n5fd_small.mdl", "plc_n5fd_small.pwk"];
const GAME_DERIVED = ["c_n5fd_empty.mdl", "c_n5fd_large.mdl", "c_n5fd_med.mdl", "c_n5fd_small.mdl", "plc_n5fd_large.mdl", "plc_n5fd_med.mdl",
  "plc_n5fd_small.mdl"];
const AT_HEAD = new Set(headFiles("data/files").map((rel) => path.posix.basename(rel)));
// .gitignore's rules for a data\files name (--no-index: the rule, whether or not the file is tracked).
const ignored = (name) => spawnSync("git", ["check-ignore", "-q", "--no-index", "data/files/" + name], { cwd: REPO, windowsHide: true }).status === 0;
// Each game-derived model as last committed - the parent of the commit that removed it - while the recipe and the
// generator are what they were then: the build must give it back byte for byte. [{ name, rev, bytes }], bytes null once
// either changed (not compared); a model not removed yet isn't listed.
function lastCommitted() {
  const blobs = (rev) => ["composition.json", "floatdisk.js", "build.js", "reader.js", "files.js"]
    .map((f) => git(["rev-parse", `${rev}:tools/data/floatdisk/${f}`], "utf8").trim()).join(" ");
  const now = blobs("HEAD"), out = [];
  for (const name of GAME_DERIVED) {
    const gone = git(["log", "-1", "--format=%H", "--diff-filter=D", "HEAD", "--", "data/files/" + name], "utf8").trim();
    if (!gone) continue;
    out.push({ name, rev: gone.slice(0, 7), bytes: blobs(gone + "^") === now ? git(["cat-file", "--filters", `${gone}^:data/files/${name}`]) : null });
  }
  return out;
}
const LAST = lastCommitted();
let NAMES = [...EXPECTED.keys()].sort();  // what a build makes: HEAD's, and the git-ignored ones (the first build says)
// The game's reading, as the generators have it (reader.js without its build.js): a dyed part's baked texture.
global.window = global.window || {};  // core.js runs in the page and under node
const CORE = require(path.join(TOOLS, "vfx_bench", "public", "core.js"));
const GAME_DATA = require(path.join(TOOLS, "spell_bench", "game_data.js"));
const RD = R.create({ game: GAME, core: CORE, gameData: GAME_DATA, reader: READER });
// The baked textures HEAD's composition gives: each part's meshes read through reader.js, its dyed ones named by
// floatdisk.js's bakeBitmaps - the names, sorted.
const headComposition = () => F.readComposition(JSON.parse(checkedOut("tools/data/floatdisk/composition.json").toString("utf8")),
  path.join(REPO, "tools", "data", "floatdisk"));
function diskBakes() {
  const comp = headComposition();
  const names = new Set();
  for (const stage of F.PILES) {
    for (const p of comp.stages[stage]) for (const b of F.bakeBitmaps(RD.part(p).meshes, p.colours, RD.pltOnly).bakes) names.add(b.name);
  }
  return [...names].sort();
}
// The shapes' soft textures HEAD's composition gives (floatdisk.js's shapeModel: its `textures`) - the names, sorted.
function diskShapes() {
  const comp = headComposition();
  const names = new Set();
  for (const stage of F.PILES) {
    for (const p of comp.stages[stage]) if (p.source.kind === "shape") for (const t of F.shapeModel(p.source.shape).textures) names.add(t.name);
  }
  return [...names].sort();
}

// --- the copy -------------------------------------------------------------------------------------------------------------
const root = fs.mkdtempSync(path.join(os.tmpdir(), "n5_derived-"));
const DATA = path.join(root, "data"), FILES = path.join(DATA, "files"), ROWS = path.join(DATA, "rows");
const COMPOSER = path.join(root, "tools", "data", "composer");
const OUT = path.join(root, "build", "data", "files");
const put = (rel, bytes) => {
  const to = path.join(root, ...rel.split("/"));
  fs.mkdirSync(path.dirname(to), { recursive: true });
  fs.writeFileSync(to, bytes);
};
for (const rel of [...headFiles("tools/data/floatdisk"), ...headFiles("tools/data/composer")]) {
  put(rel, /\.(js|bat|cpp)$/i.test(rel) ? fs.readFileSync(path.join(REPO, ...rel.split("/"))) : checkedOut(rel));
}
if (fs.existsSync(path.join(__dirname, "build_derived.js"))) put("tools/data/build_derived.js", fs.readFileSync(path.join(__dirname, "build_derived.js")));
for (const rel of [...headFiles("data/rows"), ...headFiles("data/looks")]) put(rel, checkedOut(rel));
fs.mkdirSync(FILES, { recursive: true });

// build_derived.js in the copy: the game, nwn5e-tools and the reader given (`over` replaces one; null leaves it out).
function derive(args, over = {}) {
  const given = Object.assign({ game: GAME, tools: TOOLS, reader: READER }, over);
  const pre = [].concat(...Object.entries(given).filter(([, v]) => v !== null).map(([k, v]) => ["--" + k, v]));
  const r = spawnSync(process.execPath, [path.join(root, "tools", "data", "build_derived.js"), ...pre, ...args],
    { encoding: "utf8", maxBuffer: 64 << 20, windowsHide: true });
  return { code: r.status, out: r.stdout || "", err: r.stderr || "" };
}
const done = (r) => assert.strictEqual(r.code, 0, "build_derived.js exited " + r.code + ": " + r.err.trim());
// Its per-file lines: name -> written | unchanged | would write.
const states = (r) => Object.fromEntries(r.out.split(/\r?\n/).map((l) => l.match(/^(written|unchanged|would write)\s+(\S+)$/)).filter(Boolean).map((m) => [m[2], m[1]]));
const total = (r) => r.out.trim().split(/\r?\n/).pop();
function snapshot(...dirs) {
  const s = {};
  const walk = (dir) => {
    if (!fs.existsSync(dir)) return;
    for (const f of fs.readdirSync(dir)) {
      const at = path.join(dir, f), st = fs.statSync(at);
      if (st.isDirectory()) walk(at);
      else s[at] = st.mtimeMs + ":" + sha1(fs.readFileSync(at));
    }
  };
  dirs.forEach(walk);
  return s;
}
const repoCopy = () => snapshot(path.join(root, "tools"), DATA);  // the copy's own tools\ and data\
const project = (name, json) => {
  const file = path.join(COMPOSER, name + ".json");
  fs.writeFileSync(file, P.projectText(Object.assign({ _about: ["a check's project"], name }, json)));
  return file;
};
const CIRCLE = { shape: "circle", diameter: 1, height: 0.3 };
const THROUGH = { copy: null, use: null, walk: "through" };

// --- the checks -----------------------------------------------------------------------------------------------------------
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
  check("HEAD's data\\files: the Floating Disk's 4 made from scratch committed, its 7 game-derived models not, git-ignored", () => {
    assert.deepStrictEqual(FLOATDISK_AT_HEAD.sort(), FLOATDISK.filter((f) => !GAME_DERIVED.includes(f)));
    for (const f of GAME_DERIVED) {
      assert.ok(!AT_HEAD.has(f), f + " is committed at HEAD");
      assert.ok(ignored(f), f + " isn't git-ignored");
    }
    // A Composer placeable's model (built from the game's parts), a baked PLT texture and a shape's soft texture (build
    // output: its colour's, rebuilt every build) are ignored too; the original art - a walkmesh, the disk's texture, a
    // project's own .pwk, nwn5e's other files - isn't.
    for (const f of ["plc_n5crates.mdl", "plc_n5crates_1.mdl", "PLC_N5CRATES_2.MDL", "n5p_2cd3d725dedb.tga", "n5s_8c757fb28f99.tga"]) {
      assert.ok(ignored(f), f + " isn't git-ignored");
    }
    for (const f of ["plc_n5fd_small.pwk", "n5t_forcedisk.tga", "plc_n5crates_1.pwk", "n5_bleed.mdl", "n5t_mark01.tga"]) {
      assert.ok(!ignored(f), f + " is git-ignored");
    }
    const say = (list) => [...new Set(list.map((x) => x.rev))].map((rev) => list.filter((x) => x.rev === rev).map((x) => x.name).join(", ") + ` at ${rev}^`).join("; ");
    const compared = LAST.filter((x) => x.bytes), not = LAST.filter((x) => !x.bytes);
    console.log(`     ${EXPECTED.size} files at HEAD made by a generator (${EXPECTED.size - FLOATDISK_AT_HEAD.length} by the Composer's placeable projects)` +
      (compared.length ? `; as last committed, compared: ${say(compared)}` : "") +
      (not.length ? `; not compared (the recipe or the generator changed since): ${say(not)}` : ""));
  });

  check("the committed recipes build HEAD's data\\files byte for byte, and the git-ignored ones, into --out only", () => {
    const before = repoCopy();
    const r = derive(["--out", OUT]);
    done(r);
    NAMES = Object.keys(states(r)).sort();
    assert.ok(Object.values(states(r)).every((s) => s === "written"), JSON.stringify(states(r)));
    assert.deepStrictEqual(fs.readdirSync(OUT).sort(), NAMES);
    for (const name of [...EXPECTED.keys(), ...GAME_DERIVED]) assert.ok(NAMES.includes(name), name + " wasn't built");
    for (const [name, bytes] of EXPECTED) {
      const got = fs.readFileSync(path.join(OUT, name));
      assert.ok(got.equals(bytes), `${name} isn't HEAD's data/files/${name} as checked out`);
      const text = F.carriesMark(got, F.MARK) || F.carriesMark(got, P.MARK);
      assert.ok((text ? lf(got) : got).equals(stored("data/files/" + name)), `${name} isn't git show HEAD:data/files/${name}${text ? " (CRLF as LF)" : ""}`);
    }
    // What's built but not committed: git-ignored, and a generator's - a model carrying its mark, or a dyed part's baked
    // texture, a shape's soft texture or a moving texture's material (n5p_ / n5s_ / n5u_ and 12 hex digits: only the
    // generators make such names) that a built model names. The shader (M2) is committed whole: the generators' own.
    const stem = (name) => name.replace(/\.(tga|dds|txi|mtr)$/, "");  // (W1: a clamped material's copy and TXI too)
    const named = (name) => F.BAKED.test(stem(name)) || F.SHAPE_TEXTURE.test(stem(name)) || F.MOTION_TEXTURE.test(stem(name));
    for (const name of NAMES.filter((n) => !EXPECTED.has(n))) {
      if (name === SHADER) {
        assert.ok(F.isMotionShader(fs.readFileSync(path.join(OUT, name))), name + " isn't the generators' shader");
        continue;
      }
      assert.ok(ignored(name), `${name}: built, not committed at HEAD, and not git-ignored`);
      const got = fs.readFileSync(path.join(OUT, name));
      if (named(name)) {
        const by = NAMES.filter((m) => m.endsWith(".mdl") && fs.readFileSync(path.join(OUT, m), "latin1").includes(`\r\n  bitmap ${stem(name)}\r\n`));
        assert.ok(by.length, name + ": no built model names it");
      } else {
        assert.ok(F.carriesMark(got, F.MARK) || F.carriesMark(got, P.MARK), name + " carries no generator's mark");
      }
    }
    for (const { name, rev, bytes } of LAST) {
      if (bytes) assert.ok(fs.readFileSync(path.join(OUT, name)).equals(bytes), `${name} isn't ${rev}^'s, as checked out`);
    }
    // The Floating Disk's: its 4 committed files, its 7 models, a texture a dyed part its recipe holds (the owner's
    // Large: helm_001 at the defaults, n5p_2cd3d725dedb), and a soft texture a shape part its recipe holds.
    const bakes = diskBakes(), shapes = diskShapes(), disk = [...FLOATDISK, ...bakes.map((b) => b + ".tga"), ...shapes.map((s) => s + ".tga")];
    console.log(`     the Floating Disk's dyed parts baked: ${bakes.join(", ") || "none"}; its shapes' textures: ${shapes.join(", ") || "none"}`);
    for (const name of disk) assert.ok(NAMES.includes(name), name + " wasn't built");
    assert.match(r.out, new RegExp(`the Floating Disk \\(tools/data/floatdisk/composition\\.json\\): ${disk.length} files; its rows agree`));
    assert.match(total(r), new RegExp(`^build_derived: ${NAMES.length} files in build/data/files: ${NAMES.length} written, 0 unchanged$`));
    assert.deepStrictEqual(repoCopy(), before, "the copy's tools\\ or data\\ changed");
  });

  check("a second build: every file unchanged, none touched", () => {
    const before = snapshot(OUT);
    const r = derive(["--out", OUT]);
    done(r);
    assert.ok(Object.values(states(r)).every((s) => s === "unchanged") && Object.keys(states(r)).length === NAMES.length, JSON.stringify(states(r)));
    assert.match(total(r), new RegExp(`: 0 written, ${NAMES.length} unchanged$`));
    assert.deepStrictEqual(snapshot(OUT), before);
  });

  check("--check writes nothing and says what it would write", () => {
    const before = repoCopy();
    const fresh = path.join(root, "build2", "files");
    const r = derive(["--out", fresh, "--check"]);
    done(r);
    assert.deepStrictEqual(Object.keys(states(r)).sort(), NAMES);
    assert.ok(Object.values(states(r)).every((s) => s === "would write"), JSON.stringify(states(r)));
    assert.match(total(r), new RegExp(`^build_derived: --check, nothing written: ${NAMES.length} files in build2/files: ${NAMES.length} would be written, 0 unchanged$`));
    assert.ok(!fs.existsSync(path.join(root, "build2")), "--check made the --out folder");
    assert.deepStrictEqual(repoCopy(), before);
    // A file changed in --out: --check says it would write that one, and leaves it as it is.
    const pwk = path.join(OUT, "plc_n5fd_small.pwk");
    fs.writeFileSync(pwk, Buffer.concat([EXPECTED.get("plc_n5fd_small.pwk"), Buffer.from("# an edit\r\n")]));  // still carries the mark
    const edited = snapshot(OUT);
    const c = derive(["--out", OUT, "--check"]);
    done(c);
    assert.deepStrictEqual(Object.entries(states(c)).filter(([, s]) => s !== "unchanged"), [["plc_n5fd_small.pwk", "would write"]]);
    assert.deepStrictEqual(snapshot(OUT), edited);
    // The build rewrites it as HEAD has it.
    const b = derive(["--out", OUT]);
    done(b);
    assert.deepStrictEqual(Object.entries(states(b)).filter(([, s]) => s !== "unchanged"), [["plc_n5fd_small.pwk", "written"]]);
    assert.ok(fs.readFileSync(pwk).equals(EXPECTED.get("plc_n5fd_small.pwk")));
  });

  check("rows that disagree with what the recipe gives: exit 1, which and why, nothing written", () => {
    const file = path.join(ROWS, "placeables.floatdisk.txt"), appearance = path.join(ROWS, "appearance.floatdisk.txt");
    const kept = fs.readFileSync(file), keptAppearance = fs.readFileSync(appearance);
    try {
      fs.writeFileSync(file, kept.toString("latin1").replace("N5_FloatDiskPile_Medium", "N5_FloatDiskPile_Mediun"));
      const before = snapshot(OUT), copy = repoCopy();
      for (const args of [[], ["--check"]]) {
        const r = derive(["--out", OUT, ...args]);
        assert.strictEqual(r.code, 1, "exit " + r.code);
        assert.match(r.err, /^build_derived: the rows don't match their recipes - nothing written:\r?\n  data\/rows\/placeables\.floatdisk\.txt: line 6 differs from what tools\/data\/floatdisk\/composition\.json gives\r?\n/);
        // Why it may be: a recipe changed without its rows, or a game update changed the game's tables the rows copy;
        // and what to do: rebuild and commit them, or skip for a build that won't be installed.
        assert.match(r.err, /a recipe or its generator changed without its rows, or a game update changed the game's appearance\.2da or placeables\.2da/);
        assert.match(r.err, /\nrebuild the rows and commit them /);
        assert.match(r.err, /\nor, for a build that won't be installed, NWN5E_DERIVED=skip builds without the game-derived models/);
        assert.ok(!/^(written|would write) /m.test(r.out), "it said it wrote: " + r.out);
      }
      assert.deepStrictEqual(snapshot(OUT), before);
      assert.deepStrictEqual(repoCopy(), copy, "the rows were rewritten");
      // A rows file missing likewise.
      fs.writeFileSync(file, kept);
      fs.rmSync(appearance);
      const m = derive(["--out", OUT]);
      assert.strictEqual(m.code, 1, "exit " + m.code);
      assert.match(m.err, /\n  data\/rows\/appearance\.floatdisk\.txt: missing - tools\/data\/floatdisk\/composition\.json gives one/);
      assert.ok(!fs.existsSync(appearance));
      assert.deepStrictEqual(snapshot(OUT), before);
    } finally {
      fs.writeFileSync(file, kept);
      fs.writeFileSync(appearance, keptAppearance);
    }
  });

  check("a placeable project: its committed files byte for byte; its recipe edited without its rows stops the build", () => {
    const pile = (row, scale) => ({ kind: "placeable", guide: CIRCLE, placeable: THROUGH, variants: [{ name: "1", row, parts: [{ model: "plc_b04", at: [0, 0, 0], scale }] }] });
    const file = project("chkdrv", pile(14150, 1));
    const own = ["plc_n5chkdrv.mdl", "plc_n5chkdrv.pwk"];
    try {
      // "Committed": the Composer's build.js into the copy's data\files and data\rows, as the VFX Bench's Save does.
      const g = spawnSync(process.execPath, [path.join(COMPOSER, "build.js"), file, "--json", "--game", GAME, "--tools", TOOLS, "--reader", READER],
        { encoding: "utf8", maxBuffer: 64 << 20, windowsHide: true });
      assert.strictEqual(g.status, 0, "composer build.js: " + (g.stdout + g.stderr).trim());
      const r = derive(["--out", OUT]);
      done(r);
      assert.deepStrictEqual(Object.entries(states(r)).filter(([, s]) => s !== "unchanged").sort(), own.map((f) => [f, "written"]));
      for (const f of own) assert.ok(fs.readFileSync(path.join(OUT, f)).equals(fs.readFileSync(path.join(FILES, f))), f + " isn't the committed copy");
      assert.match(r.out, /the Composer's project chkdrv \(tools\/data\/composer\/chkdrv\.json\): 2 files; its rows agree/);
      // Its row changed in the recipe, the rows file not: exit 1, nothing written.
      project("chkdrv", pile(14151, 1));
      const before = snapshot(OUT);
      const e = derive(["--out", OUT]);
      assert.strictEqual(e.code, 1, "exit " + e.code);
      assert.match(e.err, /\n  data\/rows\/placeables\.chkdrv\.txt: line \d+ differs from what tools\/data\/composer\/chkdrv\.json gives\r?\n/);
      assert.deepStrictEqual(snapshot(OUT), before);
      // An edit that keeps its rows (the part's scale) builds: its model written.
      project("chkdrv", pile(14150, 0.5));
      const k = derive(["--out", OUT]);
      done(k);
      assert.strictEqual(states(k)["plc_n5chkdrv.mdl"], "written");
    } finally {
      fs.rmSync(file, { force: true });
      fs.rmSync(path.join(ROWS, "placeables.chkdrv.txt"), { force: true });
      for (const f of own) for (const dir of [FILES, OUT]) fs.rmSync(path.join(dir, f), { force: true });
    }
  });

  check("a dyed part's baked texture comes out with its model into --out, the bake's bytes, the same on a second build", () => {
    // A project of two helmets: helm_001 at the defaults - the bake the owner's Large makes too: two generators make it,
    // alike, and it's written once - and helm_011 in leather 1 7.
    const DEF = "n5p_2cd3d725dedb", LEATHER = F.bakedName("helm_011", { leather1: 7 });
    const file = project("chkdye", { kind: "placeable", guide: CIRCLE, placeable: THROUGH, variants: [{ name: "1", row: 14152, parts: [
      { model: "helm_001", at: [0, 0, 0] }, { model: "helm_011", at: [0.3, 0, 0], colours: { leather1: 7 } }] }] });
    const own = ["plc_n5chkdye.mdl", "plc_n5chkdye.pwk", DEF + ".tga", LEATHER + ".tga"];
    const disks = NAMES.includes(DEF + ".tga");
    try {
      // "Committed": the Composer's build.js into the copy's data\files and data\rows, as the VFX Bench's Save does - its
      // bakes beside the model in data\files (git-ignored), none in made.txt.
      const g = spawnSync(process.execPath, [path.join(COMPOSER, "build.js"), file, "--json", "--game", GAME, "--tools", TOOLS, "--reader", READER],
        { encoding: "utf8", maxBuffer: 64 << 20, windowsHide: true });
      assert.strictEqual(g.status, 0, "composer build.js: " + (g.stdout + g.stderr).trim());
      for (const f of own) assert.ok(fs.existsSync(path.join(FILES, f)), f + " isn't in the copy's data\\files");
      const made = path.join(COMPOSER, "made.txt");
      assert.ok(!fs.existsSync(made) || !/n5p_/.test(fs.readFileSync(made, "utf8")), "made.txt lists a bake");
      const r = derive(["--out", OUT]);
      done(r);
      assert.deepStrictEqual(Object.entries(states(r)).filter(([, s]) => s !== "unchanged").map(([f]) => f).sort(),
        own.filter((f) => !(disks && f === DEF + ".tga")).sort());
      assert.match(r.out, /the Composer's project chkdye \(tools\/data\/composer\/chkdye\.json\): 4 files; its rows agree/);
      const mdl = fs.readFileSync(path.join(OUT, "plc_n5chkdye.mdl"), "latin1");
      for (const name of [DEF, LEATHER]) assert.ok(mdl.includes(`\r\n  bitmap ${name}\r\n`), "plc_n5chkdye.mdl doesn't name " + name);
      for (const [name, plt, colours] of [[DEF, "helm_001", F.readColours()], [LEATHER, "helm_011", F.readColours({ leather1: 7 })]]) {
        assert.ok(fs.readFileSync(path.join(OUT, name + ".tga")).equals(RD.bake(plt, colours).bytes), name + " isn't reader.js's bake");
      }
      // A second build: every file unchanged, none touched.
      const before = snapshot(OUT);
      const s = derive(["--out", OUT]);
      done(s);
      assert.ok(Object.values(states(s)).every((x) => x === "unchanged"), JSON.stringify(states(s)));
      assert.deepStrictEqual(snapshot(OUT), before);
      // A file of a baked name in --out is the bake's (only the bake makes the name, and the name fixes its bytes).
      fs.writeFileSync(path.join(OUT, LEATHER + ".tga"), "not a bake");
      const w = derive(["--out", OUT]);
      done(w);
      assert.deepStrictEqual(Object.entries(states(w)).filter(([, x]) => x !== "unchanged"), [[LEATHER + ".tga", "written"]]);
    } finally {
      fs.rmSync(file, { force: true });
      fs.rmSync(path.join(ROWS, "placeables.chkdye.txt"), { force: true });
      for (const f of own) fs.rmSync(path.join(FILES, f), { force: true });
      for (const f of own.filter((x) => !(disks && x === DEF + ".tga"))) fs.rmSync(path.join(OUT, f), { force: true });
    }
  });

  check("a spell mesh's project is skipped, and it says so", () => {
    const file = project("chkspm", { kind: "spellmesh", guide: { shape: "circle", diameter: 0.5, height: 0 }, variants: [{ name: "1", parts: [{ model: "plc_b06", at: [0, 0, 0] }] }] });
    try {
      const r = derive(["--out", OUT]);
      done(r);
      assert.match(r.out, /^build_derived: skipped tools\/data\/composer\/chkspm\.json - a spell mesh: its drafts live in the VFX Bench's saved folder/m);
      assert.ok(!fs.readdirSync(OUT).some((f) => /^n5m_/i.test(f)));
      assert.deepStrictEqual(Object.keys(states(r)).sort(), NAMES);
    } finally {
      fs.rmSync(file, { force: true });
    }
  });

  check("a project never built (the VFX Bench's New project: no rows file, no row) is skipped and said so; one with rows still stops it", () => {
    const file = path.join(COMPOSER, "chknew.json"), rows = path.join(ROWS, "placeables.chknew.txt");
    const fresh = () => P.newProject("chknew", "placeable");  // what New project writes: one empty variant, no row
    try {
      fs.writeFileSync(file, P.projectText(fresh()));
      const before = snapshot(OUT), copy = repoCopy();
      for (const args of [[], ["--check"]]) {
        const r = derive(["--out", OUT, ...args]);
        done(r);
        assert.match(r.out, /^build_derived: skipped tools\/data\/composer\/chknew\.json - never built: no data\/rows\/placeables\.chknew\.txt and no variant with a row/m);
        assert.deepStrictEqual(Object.keys(states(r)).sort(), NAMES);
      }
      assert.deepStrictEqual(snapshot(OUT), before);
      assert.deepStrictEqual(repoCopy(), copy);
      // A variant with a row: it has rows, so it's built - and refused (no parts yet).
      const withRow = fresh();
      withRow.variants[0].row = 14175;
      fs.writeFileSync(file, P.projectText(withRow));
      const a = derive(["--out", OUT]);
      assert.strictEqual(a.code, 1, "a row: exit " + a.code);
      assert.match(a.err, /^build_derived: the Composer's project chknew \(tools\/data\/composer\/chknew\.json\): variants\[0\]\.parts: at least one part/);
      // A rows file, no row in the recipe: built - and refused.
      fs.writeFileSync(file, P.projectText(fresh()));
      fs.writeFileSync(rows, P.MARK + "\r\n       Label     ModelName\r\n14176  N5_Chknew plc_n5chknew\r\n");
      const b = derive(["--out", OUT]);
      assert.strictEqual(b.code, 1, "a rows file: exit " + b.code);
      assert.match(b.err, /^build_derived: the Composer's project chknew \(tools\/data\/composer\/chknew\.json\): /);
      assert.deepStrictEqual(snapshot(OUT), before);
    } finally {
      fs.rmSync(file, { force: true });
      fs.rmSync(rows, { force: true });
    }
  });

  check("a refused build: exit 1, the generator's reason, nothing written", () => {
    const file = project("chknope", { kind: "placeable", guide: CIRCLE, placeable: THROUGH, variants: [{ name: "1", row: 14160, parts: [{ model: "plc_nope", at: [0, 0, 0] }] }] });
    try {
      const before = snapshot(OUT), copy = repoCopy();
      const r = derive(["--out", OUT]);
      assert.strictEqual(r.code, 1, "exit " + r.code);
      assert.match(r.err, /^build_derived: the Composer's project chknope \(tools\/data\/composer\/chknope\.json\): plc_nope: no such model in the game/);
      assert.deepStrictEqual(snapshot(OUT), before);
      assert.deepStrictEqual(repoCopy(), copy);
    } finally {
      fs.rmSync(file, { force: true });
    }
  });

  check("a file in --out it didn't make is refused, nothing written", () => {
    const pwk = path.join(OUT, "plc_n5fd_med.pwk");
    fs.writeFileSync(pwk, "#MAXDOOR ASCII\r\n# someone's walkmesh\r\n");
    try {
      const before = snapshot(OUT);
      for (const args of [[], ["--check"]]) {
        const r = derive(["--out", OUT, ...args]);
        assert.strictEqual(r.code, 1, "exit " + r.code);
        assert.strictEqual(r.err.trim(), "build_derived: build/data/files/plc_n5fd_med.pwk is there and wasn't made by this generator: nothing written - move it away or rename it");
      }
      assert.deepStrictEqual(snapshot(OUT), before);
    } finally {
      fs.writeFileSync(pwk, EXPECTED.get("plc_n5fd_med.pwk"));
    }
  });

  check("a prerequisite missing: exit 2, one line saying which and how to fix it, nothing written", () => {
    const nowhere = path.join(root, "build3", "files");
    for (const [over, pattern] of [
      [{ game: path.join(root, "no-game") }, /^build_derived: no game at .*no-game \(no data\\nwn_base\.key\) - pass --game <folder>, or set NWN_GAME_DIR, or game_dir under \[loader\] in .*nwn5e\.ini/],
      [{ tools: path.join(root, "no-tools") }, /^build_derived: nwn5e-tools isn't at .*no-tools .* - .*--tools <folder>/],
      [{ reader: path.join(root, "no-mdlmesh.exe") }, /^build_derived: the mesh reader isn't there .*no-mdlmesh\.exe.* - pass --reader <mdlmesh\.exe>/],
    ]) {
      for (const args of [[], ["--check"]]) {
        const r = derive(["--out", nowhere, ...args], over);
        assert.strictEqual(r.code, 2, `${JSON.stringify(over)} ${args}: exit ${r.code} - ${r.err.trim()}`);
        const lines = r.err.split(/\r?\n/).filter(Boolean);
        assert.strictEqual(lines.length, 1, lines.length + " lines: " + r.err.trim());
        assert.match(lines[0], pattern);
        // The game: the nwn5e.ini it read named in full (the generators' build folder's, as findGameDir reads it).
        const ini = path.join(path.resolve(process.env.NWN5E_BUILD || path.join(TOOLS, "..", "nwn5e-ee", "build")), "nwn5e.ini");
        if (over.game) assert.ok(lines[0].includes(" in " + ini), "the line doesn't name " + ini + ": " + lines[0]);
      }
    }
    assert.ok(!fs.existsSync(path.join(root, "build3")));
  });

  check("no --out, or --out in the repo's data\\: exit 1, nothing written", () => {
    const copy = repoCopy();
    const none = derive([]);
    assert.strictEqual(none.code, 1, "exit " + none.code);
    assert.match(none.err, /^build_derived: --out <folder>: /);
    for (const into of [FILES, DATA, path.join(DATA, "files", "sub")]) {
      const r = derive(["--out", into]);
      assert.strictEqual(r.code, 1, into + ": exit " + r.code);
      assert.match(r.err, /^build_derived: --out .* is in this repo's data\\ - /);
    }
    assert.deepStrictEqual(repoCopy(), copy);
    assert.ok(!fs.existsSync(path.join(DATA, "files", "sub")));
  });

  check("a placeable project's dome: its model and its shape texture come out into --out, the same on a second build", () => {
    // A 1 m dome in gold: a shape part (no game model), its soft texture n5s_a3caa0709dda generated from its colour -
    // beside the model in data\files, git-ignored and owned by its name as a bake is.
    const GOLD = "n5s_a3caa0709dda", colour = [1, 0.85, 0.4];
    // (DC: Repeat up 2 - its v tiles, so its soft texture is its colour's own, no TXI, as before DC; a still dome within
    // one repeat is clamped: DC's check below.)
    const file = project("chkdome", { kind: "placeable", guide: CIRCLE, placeable: THROUGH, variants: [{ name: "1", row: 14153, parts: [
      { shape: { type: "dome", radius: 1, colour, repeat_up: 2 }, at: [0, 0, 0] }] }] });
    const own = ["plc_n5chkdome.mdl", "plc_n5chkdome.pwk", GOLD + ".tga"];
    try {
      // "Committed": the Composer's build.js into the copy's data\files and data\rows, as the VFX Bench's Save does - its
      // shape texture beside the model, git-ignored (build output, as its model), not in made.txt.
      const g = spawnSync(process.execPath, [path.join(COMPOSER, "build.js"), file, "--json", "--game", GAME, "--tools", TOOLS, "--reader", READER],
        { encoding: "utf8", maxBuffer: 64 << 20, windowsHide: true });
      assert.strictEqual(g.status, 0, "composer build.js: " + (g.stdout + g.stderr).trim());
      for (const f of own) assert.ok(fs.existsSync(path.join(FILES, f)), f + " isn't in the copy's data\\files");
      for (const f of ["plc_n5chkdome.mdl", GOLD + ".tga"]) assert.ok(ignored(f), f + " isn't git-ignored");
      const made = path.join(COMPOSER, "made.txt");
      assert.ok(!fs.existsSync(made) || !/n5s_/.test(fs.readFileSync(made, "utf8")), "made.txt lists a shape's texture");
      const r = derive(["--out", OUT]);
      done(r);
      assert.deepStrictEqual(Object.entries(states(r)).filter(([, s]) => s !== "unchanged").sort(), own.map((f) => [f, "written"]).sort());
      assert.match(r.out, /the Composer's project chkdome \(tools\/data\/composer\/chkdome\.json\): 3 files; its rows agree/);
      assert.ok(fs.readFileSync(path.join(OUT, "plc_n5chkdome.mdl"), "latin1").includes(`\r\n  bitmap ${GOLD}\r\n`), "plc_n5chkdome.mdl doesn't name " + GOLD);
      assert.ok(fs.readFileSync(path.join(OUT, GOLD + ".tga")).equals(F.tgaBytes(F.shapeTexture({ colour }), { alpha: true })), GOLD + " isn't the generator's");
      // A second build: both unchanged, none touched.
      const before = snapshot(OUT);
      const s = derive(["--out", OUT]);
      done(s);
      assert.deepStrictEqual([states(s)["plc_n5chkdome.mdl"], states(s)[GOLD + ".tga"]], ["unchanged", "unchanged"]);
      assert.ok(Object.values(states(s)).every((x) => x === "unchanged"), JSON.stringify(states(s)));
      assert.deepStrictEqual(snapshot(OUT), before);
      // A file of a shape texture's name in --out is the generator's (only it makes n5s_ names, and the name fixes the
      // bytes): rewritten.
      fs.writeFileSync(path.join(OUT, GOLD + ".tga"), "not a texture");
      const w = derive(["--out", OUT]);
      done(w);
      assert.deepStrictEqual(Object.entries(states(w)).filter(([, x]) => x !== "unchanged"), [[GOLD + ".tga", "written"]]);
    } finally {
      fs.rmSync(file, { force: true });
      fs.rmSync(path.join(ROWS, "placeables.chkdome.txt"), { force: true });
      for (const f of own) for (const dir of [FILES, OUT]) fs.rmSync(path.join(dir, f), { force: true });
    }
  });

  check("a placeable project's Additive dome (M1b): its soft texture's TXI comes out into --out beside it, owned by its name; a second build the same", () => {
    // The 1 m gold dome, Additive: its own soft texture (n5s_438a32a94ebf - the blend hashed in) and its TXI ("blending
    // additive": the game's additive path), both git-ignored build output beside the model.
    const ADD = "n5s_438a32a94ebf", colour = [1, 0.85, 0.4];
    assert.strictEqual(F.shapeTextureName({ colour, blend: "additive" }), ADD);
    const file = project("chkdomea", { kind: "placeable", guide: CIRCLE, placeable: THROUGH, variants: [{ name: "1", row: 14154, parts: [
      { shape: { type: "dome", radius: 1, colour, alpha: 0.1, blend: "additive", repeat_up: 2 }, at: [0, 0, 0] }] }] });  // (DC: Repeat up 2, as above)
    const own = ["plc_n5chkdomea.mdl", "plc_n5chkdomea.pwk", ADD + ".tga", ADD + ".txi"];
    try {
      const g = spawnSync(process.execPath, [path.join(COMPOSER, "build.js"), file, "--json", "--game", GAME, "--tools", TOOLS, "--reader", READER],
        { encoding: "utf8", maxBuffer: 64 << 20, windowsHide: true });
      assert.strictEqual(g.status, 0, "composer build.js: " + (g.stdout + g.stderr).trim());
      for (const f of own) assert.ok(fs.existsSync(path.join(FILES, f)), f + " isn't in the copy's data\\files");
      assert.ok(ignored(ADD + ".txi"), ADD + ".txi isn't git-ignored");
      const r = derive(["--out", OUT]);
      done(r);
      assert.deepStrictEqual(Object.entries(states(r)).filter(([, s]) => s !== "unchanged").sort(), own.map((f) => [f, "written"]).sort());
      assert.match(r.out, /the Composer's project chkdomea \(tools\/data\/composer\/chkdomea\.json\): 4 files; its rows agree/);
      assert.strictEqual(fs.readFileSync(path.join(OUT, ADD + ".txi"), "latin1"), "blending additive\n");
      assert.ok(fs.readFileSync(path.join(OUT, ADD + ".tga")).equals(F.tgaBytes(F.shapeTexture({ colour }), { alpha: true })), ADD + ".tga isn't the generator's");
      const s = derive(["--out", OUT]);
      done(s);
      assert.ok(Object.values(states(s)).every((x) => x === "unchanged"), JSON.stringify(states(s)));
      // A TXI of a soft texture's name in --out is the generator's: rewritten, never refused as someone else's.
      fs.writeFileSync(path.join(OUT, ADD + ".txi"), "blending punchthrough\r\n");
      const w = derive(["--out", OUT]);
      done(w);
      assert.deepStrictEqual(Object.entries(states(w)).filter(([, x]) => x !== "unchanged"), [[ADD + ".txi", "written"]]);
    } finally {
      fs.rmSync(file, { force: true });
      fs.rmSync(path.join(ROWS, "placeables.chkdomea.txt"), { force: true });
      for (const f of own) for (const dir of [FILES, OUT]) fs.rmSync(path.join(dir, f), { force: true });
    }
  });

  check("a placeable project's moving dome (M2): its material and the shader come out into --out beside its model, owned by name and mark; a second build the same", () => {
    // The 1 m gold dome scrolling up: its model names n5u_<...> (its soft texture's, the motion's, its pivot's material);
    // the material (git-ignored build output, as its texture) and the shader it names (committed whole in data\files)
    // come out beside it.
    const GOLD = "n5s_a3caa0709dda", colour = [1, 0.85, 0.4], motion = { scroll: [0, 0.25] }, pivot = [0.5, 0.5], MAT = F.motionName(GOLD, motion, pivot);
    const file = project("chkuv", { kind: "placeable", guide: CIRCLE, placeable: THROUGH, variants: [{ name: "1", row: 14155, parts: [
      { shape: { type: "dome", radius: 1, colour }, at: [0, 0, 0], motion }] }] });
    const own = ["plc_n5chkuv.mdl", "plc_n5chkuv.pwk", GOLD + ".tga", MAT + ".mtr", SHADER];
    try {
      const g = spawnSync(process.execPath, [path.join(COMPOSER, "build.js"), file, "--json", "--game", GAME, "--tools", TOOLS, "--reader", READER],
        { encoding: "utf8", maxBuffer: 64 << 20, windowsHide: true });
      assert.strictEqual(g.status, 0, "composer build.js: " + (g.stdout + g.stderr).trim());
      for (const f of own) assert.ok(fs.existsSync(path.join(FILES, f)), f + " isn't in the copy's data\\files");
      assert.ok(ignored(MAT + ".mtr") && !ignored(SHADER), "the material git-ignored, the shader not");
      const r = derive(["--out", OUT]);
      done(r);
      assert.deepStrictEqual(Object.entries(states(r)).filter(([, s]) => s !== "unchanged").sort(), own.map((f) => [f, "written"]).sort());
      assert.match(r.out, /the Composer's project chkuv \(tools\/data\/composer\/chkuv\.json\): 5 files; its rows agree/);
      assert.ok(fs.readFileSync(path.join(OUT, "plc_n5chkuv.mdl"), "latin1").includes(`\r\n  bitmap ${MAT}\r\n`), "plc_n5chkuv.mdl doesn't name " + MAT);
      assert.strictEqual(fs.readFileSync(path.join(OUT, MAT + ".mtr"), "latin1"), F.motionMaterialText({ texture: GOLD, motion, pivot }));
      assert.ok(fs.readFileSync(path.join(OUT, SHADER)).equals(fs.readFileSync(path.join(REPO, "data", "files", SHADER))), SHADER + " isn't the checkout's, byte for byte");
      const s = derive(["--out", OUT]);
      done(s);
      assert.ok(Object.values(states(s)).every((x) => x === "unchanged"), JSON.stringify(states(s)));
      // In --out, a file of the material's name is the generator's, and so is a shader carrying the shader's first line (an
      // earlier version): both rewritten. Someone's vsn5uvmove.shd stops it all, nothing written.
      fs.writeFileSync(path.join(OUT, MAT + ".mtr"), "customshadervs vslit\r\n");
      fs.writeFileSync(path.join(OUT, SHADER), F.MOTION_SHADER_MARK + "\r\n// an earlier version\r\n");
      const w = derive(["--out", OUT]);
      done(w);
      assert.deepStrictEqual(Object.entries(states(w)).filter(([, x]) => x !== "unchanged").sort(), [[MAT + ".mtr", "written"], [SHADER, "written"]].sort());
      fs.writeFileSync(path.join(OUT, SHADER), "// someone's own shader\r\n");
      const before = snapshot(OUT);
      const x = derive(["--out", OUT]);
      assert.strictEqual(x.code, 1, "build_derived.js exited " + x.code);
      assert.match(x.err, /vsn5uvmove\.shd is there and wasn't made by this generator: nothing written/);
      assert.deepStrictEqual(snapshot(OUT), before);
    } finally {
      fs.rmSync(file, { force: true });
      fs.rmSync(path.join(ROWS, "placeables.chkuv.txt"), { force: true });
      for (const f of own) for (const dir of [FILES, OUT]) fs.rmSync(path.join(dir, f), { force: true });
    }
  });

  check("W1: a placeable project's breathing From-above dome (a library texture) is clamped - its material, the game texture's copy and its TXI (+ clamp 3) come out into --out beside its model, owned by the n5u_ name; a second build the same", () => {
    // fxpa_symbl_003 From above, Turn 30, Breathe 0.2 (TMO-2's dome): the model names the clamped material (its own name),
    // which names its copy - the game's picture copied under the material's name, with its TXI's lines and the game's clamp
    // line - all git-ignored build output (art made from the game's files: never committed), written beside the model.
    const SYM = "fxpa_symbl_003", motion = { turn: 30, breathe: 0.2 }, pivot = [0.5, 0.5], MAT = F.motionName(SYM, motion, pivot, true);
    const file = project("chkclmp", { kind: "placeable", guide: CIRCLE, placeable: THROUGH, variants: [{ name: "1", row: 14162, parts: [
      { shape: { type: "dome", radius: 1, mapping: "above", texture: SYM, colour: [1, 0.502, 0] }, at: [0, 0, 0], motion }] }] });
    const own = ["plc_n5chkclmp.mdl", "plc_n5chkclmp.pwk", MAT + ".mtr", MAT + ".tga", MAT + ".txi", SHADER];
    try {
      const g = spawnSync(process.execPath, [path.join(COMPOSER, "build.js"), file, "--json", "--game", GAME, "--tools", TOOLS, "--reader", READER],
        { encoding: "utf8", maxBuffer: 64 << 20, windowsHide: true });
      assert.strictEqual(g.status, 0, "composer build.js: " + (g.stdout + g.stderr).trim());
      for (const f of own) assert.ok(fs.existsSync(path.join(FILES, f)), f + " isn't in the copy's data\\files");
      for (const f of [MAT + ".mtr", MAT + ".tga", MAT + ".txi"]) assert.ok(ignored(f), f + " isn't git-ignored");
      const r = derive(["--out", OUT]);
      done(r);
      assert.deepStrictEqual(Object.entries(states(r)).filter(([, s]) => s !== "unchanged").sort(), own.map((f) => [f, "written"]).sort());
      assert.match(r.out, /the Composer's project chkclmp \(tools\/data\/composer\/chkclmp\.json\): 6 files; its rows agree/);
      assert.ok(fs.readFileSync(path.join(OUT, "plc_n5chkclmp.mdl"), "latin1").includes(`\r\n  bitmap ${MAT}\r\n`), "plc_n5chkclmp.mdl doesn't name " + MAT);
      assert.strictEqual(fs.readFileSync(path.join(OUT, MAT + ".mtr"), "latin1"), F.motionMaterialText({ texture: SYM, motion, pivot, clamp: true }));
      const sym = RD.image(SYM, []);
      assert.ok(fs.readFileSync(path.join(OUT, MAT + ".tga")).equals(sym.tga), MAT + ".tga isn't the game's " + SYM);
      assert.strictEqual(fs.readFileSync(path.join(OUT, MAT + ".txi"), "latin1"), F.clampedTxi(sym.txi));
      const s = derive(["--out", OUT]);
      done(s);
      assert.ok(Object.values(states(s)).every((x) => x === "unchanged"), JSON.stringify(states(s)));
      // In --out, a copy or TXI of the material's name is the generator's (the name fixes its bytes): rewritten.
      fs.writeFileSync(path.join(OUT, MAT + ".txi"), "clamp 0\r\n");
      const w = derive(["--out", OUT]);
      done(w);
      assert.deepStrictEqual(Object.entries(states(w)).filter(([, x]) => x !== "unchanged"), [[MAT + ".txi", "written"]]);
    } finally {
      fs.rmSync(file, { force: true });
      fs.rmSync(path.join(ROWS, "placeables.chkclmp.txt"), { force: true });
      for (const f of own) for (const dir of [FILES, OUT]) fs.rmSync(path.join(dir, f), { force: true });
    }
  });

  check("DC: a placeable project's still glyph dome (fxpa_symbl_003 From above: decal) and Soft dome come out into --out - the glyph's copy (n5l_: the game's picture, its TXI minus decal + clamp 2), the clamped soft texture and its TXI (clamp 2), owned by their names; a second build the same", () => {
    // TIN-1's glyph, orange, still: its model names the copy - the game's picture under our own name with its TXI's lines
    // but decal, + clamp 2 - git-ignored build output (the game's art: never committed); a Soft dome beside it names its
    // clamped soft texture (its own n5s_ name, its TXI clamp 2).
    const SYM = "fxpa_symbl_003", COPY = F.copyName(SYM, 2), colour = [1, 0.85, 0.4], SOFT = F.shapeTextureName({ colour, clamp: true });
    const file = project("chkdc", { kind: "placeable", guide: CIRCLE, placeable: THROUGH, variants: [{ name: "1", row: 14163, parts: [
      { shape: { type: "dome", radius: 1, mapping: "above", texture: SYM, colour: [1, 0.502, 0] }, at: [0, 0, 0] },
      { shape: { type: "dome", radius: 0.5, colour }, at: [2, 0, 0] }] }] });
    const own = ["plc_n5chkdc.mdl", "plc_n5chkdc.pwk", COPY + ".tga", COPY + ".txi", SOFT + ".tga", SOFT + ".txi"];
    try {
      const g = spawnSync(process.execPath, [path.join(COMPOSER, "build.js"), file, "--json", "--game", GAME, "--tools", TOOLS, "--reader", READER],
        { encoding: "utf8", maxBuffer: 64 << 20, windowsHide: true });
      assert.strictEqual(g.status, 0, "composer build.js: " + (g.stdout + g.stderr).trim());
      for (const f of own) assert.ok(fs.existsSync(path.join(FILES, f)), f + " isn't in the copy's data\\files");
      for (const f of [COPY + ".tga", COPY + ".txi", SOFT + ".txi"]) assert.ok(ignored(f), f + " isn't git-ignored");
      const r = derive(["--out", OUT]);
      done(r);
      assert.deepStrictEqual(Object.entries(states(r)).filter(([, s]) => s !== "unchanged").sort(), own.map((f) => [f, "written"]).sort());
      assert.match(r.out, /the Composer's project chkdc \(tools\/data\/composer\/chkdc\.json\): 6 files; its rows agree/);
      const mdl = fs.readFileSync(path.join(OUT, "plc_n5chkdc.mdl"), "latin1");
      assert.ok(mdl.includes(`\r\n  bitmap ${COPY}\r\n`) && mdl.includes(`\r\n  bitmap ${SOFT}\r\n`) && !mdl.includes(`\r\n  bitmap ${SYM}\r\n`), "plc_n5chkdc.mdl doesn't name " + COPY + " and " + SOFT);
      const sym = RD.image(SYM, []);
      assert.ok(fs.readFileSync(path.join(OUT, COPY + ".tga")).equals(sym.tga), COPY + ".tga isn't the game's " + SYM);
      assert.strictEqual(fs.readFileSync(path.join(OUT, COPY + ".txi"), "latin1"), F.copiedTxi(sym.txi, 2));
      assert.strictEqual(fs.readFileSync(path.join(OUT, SOFT + ".txi"), "latin1"), "clamp 2\n");
      const s = derive(["--out", OUT]);
      done(s);
      assert.ok(Object.values(states(s)).every((x) => x === "unchanged"), JSON.stringify(states(s)));
      // In --out, a copy or its TXI of its name is the generator's (the name fixes its bytes): rewritten.
      fs.writeFileSync(path.join(OUT, COPY + ".txi"), "decal 1\r\n");
      const w = derive(["--out", OUT]);
      done(w);
      assert.deepStrictEqual(Object.entries(states(w)).filter(([, x]) => x !== "unchanged"), [[COPY + ".txi", "written"]]);
    } finally {
      fs.rmSync(file, { force: true });
      fs.rmSync(path.join(ROWS, "placeables.chkdc.txt"), { force: true });
      for (const f of own) for (const dir of [FILES, OUT]) fs.rmSync(path.join(dir, f), { force: true });
    }
  });

  check("the real worktree is untouched (composition.json's hash, data\\files' and data\\rows' mtimes)", () => {
    assert.deepStrictEqual(worktreeState(), WORKTREE);
  });
} finally {
  RD.close();
  fs.rmSync(root, { recursive: true, force: true });
}

console.log(failed ? `derived check: ${failed} failed` : "derived check: all pass");
process.exitCode = failed ? 1 : 0;
