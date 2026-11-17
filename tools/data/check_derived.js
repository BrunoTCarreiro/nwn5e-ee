#!/usr/bin/env node
// build_derived.js end to end (D1), on temporary copies only - needs the game, prints "skip" without it:
//   node tools\data\check_derived.js [--game <folder>] [--tools <nwn5e-tools>]
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
function madeAtHead(generator) {
  const rel = `tools/data/${generator}/made.txt`;
  if (!headFiles(`tools/data/${generator}`).includes(rel)) return [];
  return checkedOut(rel).toString("utf8").split(/\r?\n/).map((l) => l.trim()).filter((l) => l.startsWith("data/files/"));
}
const FLOATDISK_AT_HEAD = [...markedAtHead(F.MARK), ...madeAtHead("floatdisk")].map((rel) => path.posix.basename(rel));
const EXPECTED = new Map();  // name -> HEAD's data\files copy, as checked out
for (const rel of [...markedAtHead(F.MARK), ...madeAtHead("floatdisk"), ...markedAtHead(P.MARK), ...madeAtHead("composer")]) {
  EXPECTED.set(path.posix.basename(rel), checkedOut(rel));
}
// The Floating Disk's files. The six models made of the game's meshes are never committed (the owner, 2026-11-17: "go
// with 1, remove the six files from main"): git-ignored, built at build time. The rest is made from scratch, committed.
const FLOATDISK = ["c_n5fd_empty.mdl", "c_n5fd_large.mdl", "c_n5fd_med.mdl", "c_n5fd_small.mdl", "n5t_forcedisk.tga",
  "plc_n5fd_large.mdl", "plc_n5fd_large.pwk", "plc_n5fd_med.mdl", "plc_n5fd_med.pwk", "plc_n5fd_small.mdl", "plc_n5fd_small.pwk"];
const GAME_DERIVED = ["c_n5fd_large.mdl", "c_n5fd_med.mdl", "c_n5fd_small.mdl", "plc_n5fd_large.mdl", "plc_n5fd_med.mdl", "plc_n5fd_small.mdl"];
const AT_HEAD = new Set(headFiles("data/files").map((rel) => path.posix.basename(rel)));
// .gitignore's rules for a data\files name (--no-index: the rule, whether or not the file is tracked).
const ignored = (name) => spawnSync("git", ["check-ignore", "-q", "--no-index", "data/files/" + name], { cwd: REPO, windowsHide: true }).status === 0;
// The six as last committed - the parent of the commit that removed them - while the recipe and the generator are what
// they were then: the build must give them back byte for byte. null before they were removed; not compared once either
// changed.
function lastCommitted() {
  const gone = git(["log", "-1", "--format=%H", "--diff-filter=D", "HEAD", "--", "data/files/c_n5fd_small.mdl"], "utf8").trim();
  if (!gone) return null;
  const blobs = (rev) => ["composition.json", "floatdisk.js", "build.js", "reader.js", "files.js"]
    .map((f) => git(["rev-parse", `${rev}:tools/data/floatdisk/${f}`], "utf8").trim()).join(" ");
  if (blobs(gone + "^") !== blobs("HEAD")) return { rev: gone.slice(0, 7), changed: true };
  return { rev: gone.slice(0, 7), changed: false, files: new Map(GAME_DERIVED.map((n) => [n, git(["cat-file", "--filters", `${gone}^:data/files/${n}`])])) };
}
const LAST = lastCommitted();
let NAMES = [...EXPECTED.keys()].sort();  // what a build makes: HEAD's, and the git-ignored ones (the first build says)

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
  check("HEAD's data\\files: the Floating Disk's 5 made from scratch committed, its 6 game-derived models not, git-ignored", () => {
    assert.deepStrictEqual(FLOATDISK_AT_HEAD.sort(), FLOATDISK.filter((f) => !GAME_DERIVED.includes(f)));
    for (const f of GAME_DERIVED) {
      assert.ok(!AT_HEAD.has(f), f + " is committed at HEAD");
      assert.ok(ignored(f), f + " isn't git-ignored");
    }
    // A Composer placeable's model (built from the game's parts) and a baked PLT texture are ignored too; the original
    // art - the empty disk, a walkmesh, the disk's texture, a project's own .pwk, nwn5e's other files - isn't.
    for (const f of ["plc_n5crates.mdl", "plc_n5crates_1.mdl", "PLC_N5CRATES_2.MDL", "n5p_2cd3d725dedb.tga"]) assert.ok(ignored(f), f + " isn't git-ignored");
    for (const f of ["c_n5fd_empty.mdl", "plc_n5fd_small.pwk", "n5t_forcedisk.tga", "plc_n5crates_1.pwk", "n5_bleed.mdl", "n5t_mark01.tga"]) {
      assert.ok(!ignored(f), f + " is git-ignored");
    }
    console.log(`     ${EXPECTED.size} files at HEAD made by a generator (${EXPECTED.size - FLOATDISK_AT_HEAD.length} by the Composer's placeable projects)` +
      (LAST ? `; the six last committed at ${LAST.rev}^${LAST.changed ? " - the recipe or the generator changed since: not compared" : ""}` : ""));
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
    // What's built but not committed: git-ignored, and a generator's.
    for (const name of NAMES.filter((n) => !EXPECTED.has(n))) {
      assert.ok(ignored(name), `${name}: built, not committed at HEAD, and not git-ignored`);
      const got = fs.readFileSync(path.join(OUT, name));
      assert.ok(F.carriesMark(got, F.MARK) || F.carriesMark(got, P.MARK), name + " carries no generator's mark");
    }
    if (LAST && !LAST.changed) {
      for (const [name, bytes] of LAST.files) assert.ok(fs.readFileSync(path.join(OUT, name)).equals(bytes), `${name} isn't ${LAST.rev}^'s, as checked out`);
    }
    assert.match(r.out, /the Floating Disk \(tools\/data\/floatdisk\/composition\.json\): 11 files; its rows agree/);
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
        assert.match(r.err, /a recipe or its generator changed without its rows/);
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
      [{ game: path.join(root, "no-game") }, /^build_derived: no game at .*no-game \(no data\\nwn_base\.key\) - pass --game <folder>/],
      [{ tools: path.join(root, "no-tools") }, /^build_derived: nwn5e-tools isn't at .*no-tools .* - .*--tools <folder>/],
      [{ reader: path.join(root, "no-mdlmesh.exe") }, /^build_derived: the mesh reader isn't there .*no-mdlmesh\.exe.* - pass --reader <mdlmesh\.exe>/],
    ]) {
      for (const args of [[], ["--check"]]) {
        const r = derive(["--out", nowhere, ...args], over);
        assert.strictEqual(r.code, 2, `${JSON.stringify(over)} ${args}: exit ${r.code} - ${r.err.trim()}`);
        const lines = r.err.split(/\r?\n/).filter(Boolean);
        assert.strictEqual(lines.length, 1, lines.length + " lines: " + r.err.trim());
        assert.match(lines[0], pattern);
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

  check("the real worktree is untouched (composition.json's hash, data\\files' and data\\rows' mtimes)", () => {
    assert.deepStrictEqual(worktreeState(), WORKTREE);
  });
} finally {
  fs.rmSync(root, { recursive: true, force: true });
}

console.log(failed ? `derived check: ${failed} failed` : "derived check: all pass");
process.exitCode = failed ? 1 : 0;
