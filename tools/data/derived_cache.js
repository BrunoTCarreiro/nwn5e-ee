// build_derived.js's cache (docs/TOOLS-PLAN.md, "Faster checks"; the owner, 2026-11-23: "yes, do 1 to 3 after the
// install"). Each generator's output is kept, keyed by everything it read the run that made it, and reused while all of
// that is as it was; anything changed - a game file, a recipe, the generator's code, nwn5e-tools' code it loads, the mesh
// reader - and it runs again. The first build after a game update or a generator change does the full work.
//
//   const DC = require("./derived_cache.js");
//   const cache = DC.open({ dir, mode, anchors: { repo, tools, game, reader }, log });
//   const result = cache.unit({ id, key, run: (ctx) => ..., child, modules, inputs });
//
// - **What a run read** (derived_trace.js): every file's bytes it read, every path it looked at (there or not, a file or
//   a folder), every folder it listed, every program it ran (the mesh reader) and the files it named, every module it
//   loaded - not what it read back of its own (a file it wrote first, anything in a folder it made). A generator run
//   as a child (child: true) is traced whole, its own children with it (ctx.env: NODE_OPTIONS and N5_TRACE_DIR to
//   pass on); one run in this process (child: false) is traced while run() runs, and its code is `modules` and every
//   module they load (module.children, all the way down). `inputs` adds what it reads through someone else (the game's
//   keys, read once per process; the reader). ctx.exclude(folder): a scratch folder that's no input; ctx.alias(copy,
//   folder, seen): a scratch copy of a folder (build_derived's of data\rows), its files read as that folder's.
// - **Kept as** <dir>\u\<the unit's key>\<n>.json - each input by where it is (under the repo, nwn5e-tools, the game,
//   or the reader itself - so a copy of the repo, as check_derived.js makes, finds the same entries), with its size and
//   time and, outside the game, its SHA-1 - and the output's bytes in <dir>\b\<SHA-1>. A key holds the last 12 runs.
// - **Reused when** every input is as it was: the same size and time, or (outside the game) the same bytes; every path
//   looked at there or not as it was; every folder's names the same. The game's files by size and time only (some are
//   gigabytes); a game update changes both.
// - **The key:** the unit's id and arguments (paths by where they are, as above), Node's version, the environment the
//   generators read (NWN5E_BUILD, NWN5E_MDLMESH, NWN5E_ANIM_HELPER, NWN_GAME_DIR, NWN5E_TOOLS) and the cache's own code
//   and build_derived.js's.
// - **Modes:** "on" (reuse a hit, keep a miss), "off" (no cache), "fresh" (run all, keep them: build.bat clean),
//   "verify" (run all and fail where a hit's output differs from the run's - the check that the cache gives the same
//   bytes).
// - Never an error of the generator's kept: a run that throws is thrown again, nothing kept.
"use strict";
const crypto = require("crypto");
const fs = require("fs");
const os = require("os");
const path = require("path");
const T = require("./derived_trace.js");

const VERSION = 1;
const KEEP = 12;  // runs kept per key
const HASH_LIMIT = 256 << 20;  // a file this big outside the game: by size and time only
const ENV = ["NWN5E_BUILD", "NWN5E_MDLMESH", "NWN5E_ANIM_HELPER", "NWN_GAME_DIR", "NWN5E_TOOLS"];
const OWN_CODE = [__filename, require.resolve("./derived_trace.js"), require.resolve("./build_derived.js")];

const sha1 = (bytes) => crypto.createHash("sha1").update(bytes).digest("hex");
const namesOf = (names) => sha1(names.map((n) => String(n).toLowerCase()).sort().join("\n"));  // a folder's names, as one

function kindOf(p) {
  try {
    const s = fs.statSync(p);
    return s.isDirectory() ? "dir" : s.isFile() ? "file" : "other";
  } catch (e) {
    return null;
  }
}

function open({ dir, mode = "on", anchors = {}, log = () => {} }) {
  const on = mode !== "off";
  // Anchors: a path under one is kept as "<name>:<relative, lower case, />" - a file anchor (the reader) exactly.
  const list = Object.entries(anchors).filter(([, p]) => p).map(([name, p]) => [name, path.resolve(p).toLowerCase()])
    .sort((a, b) => b[1].length - a[1].length);
  const anchored = (p) => {
    if (p.startsWith("<")) return p;
    const low = path.resolve(p).toLowerCase();
    for (const [name, at] of list) {
      if (low === at) return name + ":";
      if (low.startsWith(at + path.sep)) return name + ":" + low.slice(at.length + 1).split(path.sep).join("/");
    }
    return "abs:" + low;
  };
  const resolve = (a) => {
    const i = a.indexOf(":");
    const name = a.slice(0, i), rel = a.slice(i + 1);
    if (name === "abs") return rel;
    const at = anchors[name];
    if (!at) return null;
    return rel ? path.join(path.resolve(at), ...rel.split("/")) : path.resolve(at);
  };
  const inGame = (a) => a.startsWith("game:");
  const ownCode = OWN_CODE.filter((f) => fs.existsSync(f)).map((f) => sha1(fs.readFileSync(f))).join(",");
  const stats = { reused: 0, built: 0, missed: [] };

  // The arguments as the key sees them: a path by where it is.
  const keyed = (v) => {
    if (typeof v === "string" && path.isAbsolute(v)) return anchored(v);
    if (Array.isArray(v)) return v.map(keyed);
    if (v && typeof v === "object") return Object.fromEntries(Object.entries(v).map(([k, x]) => [k, keyed(x)]));
    return v;
  };
  const unitKey = (id, key) => sha1(JSON.stringify({ v: VERSION, id, key: keyed(key), node: process.version, arch: process.arch,
    env: ENV.map((e) => process.env[e] || ""), own: ownCode }));

  // --- outputs: Buffers kept as blobs ---------------------------------------------------------------------------------
  const blobs = path.join(dir, "b"), units = path.join(dir, "u");
  const putFile = (file, bytes) => {
    const tmp = `${file}.${process.pid}.${crypto.randomBytes(4).toString("hex")}.tmp`;
    fs.writeFileSync(tmp, bytes);
    fs.renameSync(tmp, file);
  };
  const encode = (v) => {
    if (Buffer.isBuffer(v)) {
      const h = sha1(v), file = path.join(blobs, h);
      let ok = false;
      try { ok = fs.statSync(file).size === v.length && sha1(fs.readFileSync(file)) === h; } catch (e) { /* not there */ }
      if (!ok) putFile(file, v);  // (written again when damaged)
      return { $blob: h, size: v.length };
    }
    if (Array.isArray(v)) return v.map(encode);
    if (v && typeof v === "object") return Object.fromEntries(Object.entries(v).map(([k, x]) => [k, encode(x)]));
    return v;
  };
  const decode = (v) => {
    if (v && typeof v === "object" && typeof v.$blob === "string") {
      const bytes = fs.readFileSync(path.join(blobs, v.$blob));
      if (bytes.length !== v.size || sha1(bytes) !== v.$blob) throw new Error("blob " + v.$blob + " damaged");
      return bytes;
    }
    if (Array.isArray(v)) return v.map(decode);
    if (v && typeof v === "object") return Object.fromEntries(Object.entries(v).map(([k, x]) => [k, decode(x)]));
    return v;
  };
  // The same output? (verify) - compared as the bytes they hold.
  const same = (a, b) => JSON.stringify(shape(a)) === JSON.stringify(shape(b));
  const shape = (v) => (Buffer.isBuffer(v) ? { $bytes: sha1(v) } : Array.isArray(v) ? v.map(shape)
    : v && typeof v === "object" ? Object.fromEntries(Object.entries(v).map(([k, x]) => [k, shape(x)])) : v);

  // --- inputs ----------------------------------------------------------------------------------------------------------
  // A trace (or several: a child's processes) -> the record's inputs. Throws Changed when an input changed while it ran.
  // `aliases`: [{ from, to, seen }] - a scratch copy's files read as the files they copy (`seen`: each one's size and
  // time when copied, by its lower-case path).
  function inputsOf(traces, extra, exclude, aliases) {
    const out = new Map();  // anchored -> entry
    const under = (low, d) => low === d || low.startsWith(d + path.sep);
    const skip = [dir, ...exclude].map((p) => path.resolve(p).toLowerCase());
    for (const t of traces) skip.push(...(t.made || []));  // a folder one of its processes made: another's reads in it
    const real = (p) => {  // -> [the path an alias gives, the copy's seen stat or undefined]
      const low = path.resolve(p).toLowerCase();
      for (const x of aliases) {
        if (under(low, x.from)) {
          const to = path.join(x.to, low.slice(x.from.length));
          return [to, x.seen.get(to.toLowerCase()) || null];
        }
      }
      return [p, undefined];
    };
    const dropped = (p) => skip.some((s) => under(path.resolve(p).toLowerCase(), s));
    const file = (given, seenGiven) => {
      if (given.startsWith("<")) return out.set(given, { t: "tool", a: given });  // a program by name: the system's
      const [p, aliasSeen] = real(given);
      const seen = aliasSeen === undefined ? seenGiven : aliasSeen;
      if (dropped(p)) return;
      const a = anchored(p);
      if (out.has(a) && out.get(a).t === "file") return;
      let st;
      try {
        st = fs.statSync(p);
      } catch (e) {
        return out.set(a, { t: "probe", a, kind: null });
      }
      if (!st.isFile()) return out.set(a, { t: "probe", a, kind: st.isDirectory() ? "dir" : "other" });
      if (seen && (seen.size !== st.size || seen.mtimeMs !== st.mtimeMs)) throw new Changed(p);
      const hash = inGame(a) || st.size > HASH_LIMIT ? null : sha1(fs.readFileSync(p));
      const again = fs.statSync(p);
      if (again.size !== st.size || again.mtimeMs !== st.mtimeMs) throw new Changed(p);
      out.set(a, { t: "file", a, size: st.size, mtimeMs: st.mtimeMs, sha1: hash });
    };
    for (const t of traces) {
      for (const r of t.reads || []) file(r.path, r);
      for (const m of t.modules || []) file(m, null);
      for (const e of t.execs || []) file(e.path, null);
    }
    for (const p of extra) {
      if (kindOf(p) !== "dir") file(p, null);
      else if (!dropped(p)) out.set(anchored(p), { t: "dir", a: anchored(p), names: namesOf(fs.readdirSync(p)) });
    }
    for (const t of traces) {
      for (const d of t.dirs || []) {
        const [p] = real(d.path);
        if (dropped(p)) continue;
        const a = anchored(p);
        if (!out.has(a) || out.get(a).t === "probe") out.set(a, { t: "dir", a, names: namesOf(d.names) });
      }
      for (const x of t.probes || []) {
        const [p] = real(x.path);
        if (dropped(p)) continue;
        const a = anchored(p);
        if (!out.has(a)) out.set(a, { t: "probe", a, kind: x.kind });
      }
    }
    return [...out.values()].sort((x, y) => (x.a < y.a ? -1 : x.a > y.a ? 1 : 0));
  }
  class Changed extends Error {}

  // Is every input as it was? -> null (yes) or what changed.
  function stale(inputs) {
    for (const x of inputs) {
      const p = x.t === "tool" ? null : resolve(x.a);
      if (x.t === "tool") continue;
      if (!p) return x.a + " (no such place now)";
      if (x.t === "probe") {
        if (kindOf(p) !== x.kind) return x.a + (x.kind ? " is gone" : " is there now");
      } else if (x.t === "dir") {
        let names;
        try {
          names = fs.readdirSync(p);
        } catch (e) {
          return x.a + " is gone";
        }
        if (namesOf(names) !== x.names) return x.a + "'s files changed";
      } else {
        let st;
        try {
          st = fs.statSync(p);
        } catch (e) {
          return x.a + " is gone";
        }
        if (st.size === x.size && st.mtimeMs === x.mtimeMs) continue;
        if (!x.sha1 || st.size !== x.size || sha1(fs.readFileSync(p)) !== x.sha1) return x.a + " changed";
      }
    }
    return null;
  }

  // The closure of these modules' code: each, and every module it loaded (module.children), all the way down.
  function codeOf(entries) {
    const seen = new Set(), outList = [];
    const walk = (m) => {
      if (!m || seen.has(m.filename)) return;
      seen.add(m.filename);
      outList.push(m.filename);
      for (const c of m.children || []) walk(c);
    };
    for (const e of entries) walk(require.cache[path.resolve(e)]);
    return outList;
  }

  // `run(ctx)`: ctx.env (a child's: to add to its environment), ctx.exclude(folder) (its scratch: no input),
  // ctx.alias(from, to, seen) (a scratch copy of `to`, read as `to`; `seen`: Map(lower-case path in `to` -> its
  // { size, mtimeMs } when copied)), ctx.input(path) (one more input: a file the caller reads for it, there or not).
  function unit({ id, key, run, child = false, modules = [], inputs = [] }) {
    const exclude = [], aliases = [];
    inputs = [...inputs];
    const ctx = { env: {}, exclude: (p) => exclude.push(p), input: (p) => inputs.push(p),
      alias: (from, to, seen) => aliases.push({ from: path.resolve(from).toLowerCase(), to: path.resolve(to), seen }) };
    if (!on) return run(ctx);
    const k = unitKey(id, key), at = path.join(units, k);
    let hit = null, why = "never built";
    if (mode === "on" || mode === "verify") {
      const records = fs.existsSync(at) ? fs.readdirSync(at).filter((f) => f.endsWith(".json"))
        .map((f) => ({ f: path.join(at, f), t: fs.statSync(path.join(at, f)).mtimeMs })).sort((a, b) => b.t - a.t) : [];
      for (const { f } of records) {
        let rec;
        try {
          rec = JSON.parse(fs.readFileSync(f, "utf8"));
        } catch (e) {
          continue;
        }
        const s = stale(rec.inputs);
        if (s) {
          if (why === "never built") why = s;
          continue;
        }
        try {
          hit = { result: decode(rec.result), file: f };
        } catch (e) {
          fs.rmSync(f, { force: true });  // a blob gone: a miss
          continue;
        }
        break;
      }
    }
    if (hit && mode === "on") {
      const now = new Date();
      try { fs.utimesSync(hit.file, now, now); } catch (e) { /* only its place in the list */ }
      stats.reused++;
      return hit.result;
    }
    // Run it, traced.
    let result, traces;
    if (child) {
      const traceDir = fs.mkdtempSync(path.join(os.tmpdir(), "n5_trace-"));
      try {
        const pre = `--require ${JSON.stringify(require.resolve("./derived_trace.js"))}`;
        ctx.env = { NODE_OPTIONS: [process.env.NODE_OPTIONS, pre].filter(Boolean).join(" "), N5_TRACE_DIR: traceDir };
        result = run(ctx);
        traces = fs.readdirSync(traceDir).filter((f) => f.startsWith("trace-")).map((f) => JSON.parse(fs.readFileSync(path.join(traceDir, f), "utf8")));
        exclude.push(traceDir);
      } finally {
        fs.rmSync(traceDir, { recursive: true, force: true });
      }
    } else {
      const s = T.begin();
      try {
        result = run(ctx);
      } finally {
        traces = [T.end(s)];
      }
      traces[0].modules = codeOf(modules);
    }
    if (hit && mode === "verify" && !same(hit.result, result)) {
      throw new Error(`the cache's ${id} differs from a fresh run - its kept output in ${hit.file}`);
    }
    stats.built++;
    stats.missed.push(`${id} (${hit ? "verified" : mode === "fresh" ? "fresh" : why})`);
    // One trace for the run (the child's own, or this process's) and one for each node process it started: one missing
    // (a run in this process starts its children untraced) leaves what it read unknown - not kept.
    const want = 1 + traces.reduce((n, t) => n + (t.children || 0), 0);
    if (traces.length !== want) {
      log(`build_derived: cache: ${id} not kept - ${want - traces.length} of its node processes left no trace`);
      return result;
    }
    let recordInputs;
    try {
      recordInputs = inputsOf(traces, inputs, exclude, aliases);
    } catch (e) {
      if (e instanceof Changed) {
        log(`build_derived: cache: ${id} not kept - ${anchored(e.message)} changed while it ran`);
        return result;
      }
      throw e;
    }
    fs.mkdirSync(at, { recursive: true });
    fs.mkdirSync(blobs, { recursive: true });
    putFile(path.join(at, `${Date.now()}-${process.pid}-${crypto.randomBytes(3).toString("hex")}.json`),
      JSON.stringify({ v: VERSION, id, inputs: recordInputs, result: encode(result) }));
    // The last KEEP runs of this key.
    const all = fs.readdirSync(at).filter((f) => f.endsWith(".json")).map((f) => ({ f: path.join(at, f), t: fs.statSync(path.join(at, f)).mtimeMs }))
      .sort((a, b) => b.t - a.t);
    for (const { f } of all.slice(KEEP)) fs.rmSync(f, { force: true });
    return result;
  }

  // Blobs no record names, a day old (a run writing one now names it soon), deleted.
  function gc() {
    if (!on || !fs.existsSync(blobs)) return;
    const named = new Set();
    const walk = (v) => {
      if (v && typeof v === "object") {
        if (typeof v.$blob === "string") named.add(v.$blob);
        else for (const x of Object.values(v)) walk(x);
      }
    };
    for (const k of fs.existsSync(units) ? fs.readdirSync(units) : []) {
      for (const f of fs.readdirSync(path.join(units, k))) {
        try {
          walk(JSON.parse(fs.readFileSync(path.join(units, k, f), "utf8")).result);
        } catch (e) { /* a record being written */ }
      }
    }
    const old = Date.now() - 24 * 3600 * 1000;
    for (const b of fs.readdirSync(blobs)) {
      const f = path.join(blobs, b);
      if (!named.has(b) && fs.statSync(f).mtimeMs < old) fs.rmSync(f, { force: true });
    }
  }

  return { unit, gc, stats, mode, dir };
}

module.exports = { open, VERSION };
