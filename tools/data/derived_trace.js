// What a generator reads (docs/TOOLS-PLAN.md, "Faster checks": the owner, 2026-11-23, "yes, do 1 to 3 after the install"):
// build_derived.js's cache (derived_cache.js) keys each generator's output by every file it read, so this watches Node's
// fs and child_process while one runs and lists what it touched:
//   - a file's bytes read (readFileSync, openSync for reading, readFile, createReadStream, copyFileSync's and cpSync's
//     source);
//   - a path looked at (existsSync, statSync, lstatSync, accessSync): whether it was there, a file or a folder;
//   - a folder listed (readdirSync): its names;
//   - a program run (execFileSync, spawnSync, execFile, spawn; execSync and exec by their command): its file, and any
//     argument naming a file, as read (mdlmesh reads the files it's given);
//   - what it wrote (writeFileSync, appendFileSync, openSync for writing, rmSync, unlinkSync, renameSync, copyFileSync's
//     and cpSync's target) and the folders it made (mkdtempSync, mkdirSync of one that wasn't there): a read, look or
//     listing of its own file after it wrote it, or of anything in a folder it made, is no input - one before is.
// In this process: `const T = require("./derived_trace.js"); const s = T.begin(); ...; T.end(s)`.
// In a child: NODE_OPTIONS="--require <this file>" with N5_TRACE_DIR=<folder> - installed before anything runs, it
// traces the whole process and at exit writes what it saw, and every module it loaded, into a file of its own there
// (derived_cache.js reads them). A child's own children inherit both and write theirs.
"use strict";
const fs = require("fs");
const path = require("path");
const cp = require("child_process");

const real = { statSync: fs.statSync, readdirSync: fs.readdirSync, writeFileSync: fs.writeFileSync };
const scopes = new Set();
let installed = false;

function abs(p) {
  if (p === undefined || p === null || typeof p === "number") return null;
  if (Buffer.isBuffer(p)) p = p.toString();
  else if (p instanceof URL) p = require("url").fileURLToPath(p);
  if (typeof p !== "string" || !p) return null;
  return path.resolve(p);
}

function kindOf(p) {
  try {
    const s = real.statSync(p);
    return s.isDirectory() ? "dir" : s.isFile() ? "file" : "other";
  } catch (e) {
    return null;
  }
}

// One run's record. A read, look or listing counts only before the run itself wrote that file or made the folder it's
// in: what it reads back of its own isn't an input, but a file it reads and then rewrites is.
class Scope {
  constructor() {
    this.reads = new Map();   // key -> { path, size, mtimeMs } as it was when first read
    this.probes = new Map();  // key -> { path, kind: "file" | "dir" | "other" | null }
    this.dirs = new Map();    // key -> { path, names }
    this.execs = new Map();   // key -> { path }
    this.written = new Set(); // files it wrote, renamed or removed
    this.made = [];           // folders it made (mkdtemp, or a mkdir of one that wasn't there)
    this.children = 0;        // node processes it started (each writes a trace of its own)
  }
  own(key) {
    return this.written.has(key) || this.made.some((d) => key === d || key.startsWith(d + path.sep));
  }
  add(kind, at, extra) {
    const key = at.toLowerCase();
    if (kind === "write") return void this.written.add(key);
    if (kind === "made") return void this.made.push(key);
    if (this.own(key)) return;
    if (kind === "read") {
      if (this.reads.has(key)) return;
      try {
        const s = real.statSync(at);
        if (s.isFile()) this.reads.set(key, { path: at, size: s.size, mtimeMs: s.mtimeMs });
      } catch (e) { /* not there: an error the generator sees */ }
    } else if (kind === "probe") {
      if (!this.probes.has(key)) this.probes.set(key, { path: at, kind: extra });
    } else if (kind === "dir") {
      if (!this.dirs.has(key)) this.dirs.set(key, { path: at, names: extra });
    } else if (kind === "exec") {
      if (!this.execs.has(key)) this.execs.set(key, { path: at });
    }
  }
  // What it read, looked at, listed and ran; and the folders it made (another process of the same run - a generator's
  // own child - may read in them: derived_cache.js drops those too).
  result() {
    const v = (m) => [...m.values()];
    return { reads: v(this.reads), probes: v(this.probes), dirs: v(this.dirs), execs: v(this.execs), made: [...this.made],
      children: this.children };
  }
}

function note(kind, p, extra) {
  if (!scopes.size) return;
  const at = typeof p === "string" && p.startsWith("<") ? p : abs(p);
  if (!at) return;
  for (const s of scopes) s.add(kind, at, extra);
}

function wrap(obj, name, fn) {
  const orig = obj[name];
  if (typeof orig !== "function") return;
  const wrapped = function (...args) {
    return fn.call(this, orig.bind(obj), args);
  };
  Object.defineProperties(wrapped, Object.getOwnPropertyDescriptors(orig));  // (util.promisify.custom and the like)
  obj[name] = wrapped;
}

const readOnly = (flags) => (flags === undefined ? true : typeof flags === "number" ? (flags & 3) === 0 : /^rs?$/.test(String(flags)));

function install() {
  if (installed) return;
  installed = true;
  wrap(fs, "existsSync", (orig, args) => {
    const r = orig(...args);
    if (scopes.size) note("probe", args[0], r ? kindOf(abs(args[0]) || "") : null);
    return r;
  });
  for (const name of ["statSync", "lstatSync", "accessSync"]) {
    wrap(fs, name, (orig, args) => {
      try {
        return orig(...args);
      } finally {
        if (scopes.size && abs(args[0])) note("probe", args[0], kindOf(abs(args[0])));
      }
    });
  }
  for (const name of ["readFileSync", "readFile", "createReadStream"]) {
    wrap(fs, name, (orig, args) => {
      note("read", args[0]);
      return orig(...args);
    });
  }
  wrap(fs, "openSync", (orig, args) => {
    note(readOnly(args[1]) ? "read" : "write", args[0]);
    return orig(...args);
  });
  wrap(fs, "readdirSync", (orig, args) => {
    const r = orig(...args);
    if (scopes.size) note("dir", args[0], r.map((x) => String(typeof x === "string" || Buffer.isBuffer(x) ? x : x.name)).sort());
    return r;
  });
  for (const name of ["writeFileSync", "appendFileSync", "rmSync", "unlinkSync", "rmdirSync"]) {
    wrap(fs, name, (orig, args) => {
      note("write", args[0]);
      return orig(...args);
    });
  }
  wrap(fs, "mkdirSync", (orig, args) => {
    const was = scopes.size ? kindOf(abs(args[0]) || "") : "dir";
    const r = orig(...args);
    if (!was) note("made", args[0]);
    return r;
  });
  wrap(fs, "mkdtempSync", (orig, args) => {
    const made = orig(...args);
    note("made", made);
    return made;
  });
  wrap(fs, "copyFileSync", (orig, args) => {
    note("read", args[0]);
    note("write", args[1]);
    return orig(...args);
  });
  wrap(fs, "cpSync", (orig, args) => {
    const walk = (p) => {
      if (kindOf(p) !== "dir") return note("read", p);
      const names = real.readdirSync(p).map(String).sort();
      note("dir", p, names);
      for (const f of names) walk(path.join(p, f));
    };
    if (scopes.size && abs(args[0])) walk(abs(args[0]));
    const was = scopes.size ? kindOf(abs(args[1]) || "") : "dir";
    note(was ? "write" : kindOf(abs(args[0]) || "") === "dir" ? "made" : "write", args[1]);
    return orig(...args);
  });
  wrap(fs, "renameSync", (orig, args) => {
    note("write", args[0]);
    note("write", args[1]);
    return orig(...args);
  });
  // A program run: its file (node itself aside - its version is in the key; its own run is traced on its own), and
  // any argument naming a file, as read.
  const ran = (file, args) => {
    if (!scopes.size || typeof file !== "string") return;
    if (path.resolve(file).toLowerCase() !== process.execPath.toLowerCase()) note("exec", /[\\/]/.test(file) ? file : "<path>:" + file);
    else for (const s of scopes) s.children++;  // a node child: its own trace is expected beside this one's
    for (const a of Array.isArray(args) ? args : []) {
      if (typeof a === "string" && /[\\/]/.test(a) && kindOf(path.resolve(a)) === "file") note("read", a);
    }
  };
  for (const name of ["execFileSync", "spawnSync", "execFile", "spawn"]) {
    wrap(cp, name, (orig, args) => {
      ran(args[0], args[1]);
      return orig(...args);
    });
  }
  for (const name of ["execSync", "exec"]) {
    wrap(cp, name, (orig, args) => {
      note("exec", "<shell>:" + String(args[0]));
      return orig(...args);
    });
  }
}

function begin() {
  install();
  const s = new Scope();
  scopes.add(s);
  return s;
}

function end(s) {
  scopes.delete(s);
  return s.result();
}

module.exports = { install, begin, end };
// Wrapped as soon as this loads (a module loaded after it that keeps `const { execFileSync } = require("child_process")`
// keeps the wrapper): build_derived.js loads it first. Nothing is recorded while no run is traced.
install();

// Preloaded into a child (NODE_OPTIONS --require, N5_TRACE_DIR set): trace the whole process, write it out at exit.
if (process.env.N5_TRACE_DIR && !global.__n5TraceWhole) {
  global.__n5TraceWhole = true;
  const s = begin();
  process.on("exit", () => {
    const out = end(s);
    out.modules = Object.keys(require.cache).filter((m) => path.isAbsolute(m) && m !== __filename);
    try {
      real.writeFileSync(path.join(process.env.N5_TRACE_DIR, `trace-${process.pid}-${Date.now()}.json`), JSON.stringify(out));
    } catch (e) { /* no trace: derived_cache.js doesn't keep that run */ }
  });
}
