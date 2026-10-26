#!/usr/bin/env node
// VFX Bench - a local web app to make and edit visual effects for nwn5e (owner,
// 2026-10-26: "can you come up with a webapp to edit and create new vfx? Kind of
// like the one for animations").
//
// Start it with vfx_bench.bat (or `node tools/vfx_bench/server.js`); it opens
// http://127.0.0.1:8766/ in the browser. It reads what nwn5e copies out for it
// (build\vfxedit\library\: the game's effects as drafts, their particle textures,
// nwn5e's own effects), saves drafts to build\vfxedit\saved\ (the game writes
// each one's model, and !vfxedit's Load lists them), and asks a running game to
// play or stop an effect on the player's character (build\vfxedit\bench\).
// Only this machine can reach it (127.0.0.1); no dependencies.
//
// options: --build <folder>  nwn5e's build folder (default: ../../build, or NWN5E_BUILD)
//          --port <n>        (default 8766)
//          --no-open         don't open the browser
"use strict";
const http = require("http");
const fs = require("fs");
const path = require("path");
const { exec } = require("child_process");
const core = require("./public/core.js");

const args = process.argv.slice(2);
const option = (name, fallback) => {
  const i = args.indexOf("--" + name);
  return i >= 0 && args[i + 1] ? args[i + 1] : fallback;
};
const BUILD = path.resolve(option("build", process.env.NWN5E_BUILD || path.join(__dirname, "..", "..", "build")));
const PORT = Number(option("port", "8766"));
const OPEN = !args.includes("--no-open");
const EDITOR = path.join(BUILD, "vfxedit");
const LIBRARY = path.join(EDITOR, "library");
const SAVED = path.join(EDITOR, "saved");
const BENCH = path.join(EDITOR, "bench");
const PUBLIC = path.join(__dirname, "public");
const FILES = ["index.html", "app.js", "core.js", "sim.js", "view.js", "style.css"];
const STATIC = Object.fromEntries([["/", "index.html"], ...FILES.map((f) => ["/" + f, f])]);
const TYPES = { ".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8", ".css": "text/css; charset=utf-8" };
const NAME = /^[a-z0-9_]{1,16}$/;  // a resref: the model's file name
const LIBRARY_NAME = /^[a-z0-9_]{1,32}$/;
const URL_BASE = `http://127.0.0.1:${PORT}/`;
const ALIVE_MS = 6000;  // the game writes bench\alive.txt every 2 s while a character is in play
// The code this server runs, from when it started: a bench updated since needs a restart.
const STARTED = Date.now();
const CODE = [__filename, path.join(PUBLIC, "core.js")];
let serial = Date.now() % 100000;

function list(folder, test) {
  try {
    return fs.readdirSync(folder, { withFileTypes: true }).filter((e) => e.isFile() && test(e.name)).map((e) => e.name).sort();
  } catch (e) {
    return [];
  }
}

function stale() {
  return CODE.some((file) => { try { return fs.statSync(file).mtimeMs > STARTED; } catch (e) { return false; } });
}

function readIndex() {
  let text = "";
  try { text = fs.readFileSync(path.join(LIBRARY, "index.txt"), "utf8"); } catch (e) { return []; }
  const rows = [];
  for (const line of text.split(/\r?\n/)) {
    if (!line || line.startsWith("#")) continue;
    const [row, label, model, attach, type] = line.split("\t");
    if (!model || !LIBRARY_NAME.test(model)) continue;
    rows.push({ row: Number(row), label, model, attach: attach || "root", type: type === "F" ? "F" : "D" });
  }
  return rows;
}

function stem(file) { return file.replace(/\.[^.]+$/, "").toLowerCase(); }

function game() {
  let alive = false, library = 0;
  try { alive = Date.now() - fs.statSync(path.join(BENCH, "alive.txt")).mtimeMs < ALIVE_MS; } catch (e) { /* not yet */ }
  try { library = Math.round(fs.statSync(path.join(LIBRARY, "index.txt")).mtimeMs); } catch (e) { /* not exported yet */ }
  let status = null;
  try {
    const [n, ...rest] = fs.readFileSync(path.join(BENCH, "status.txt"), "utf8").trim().split("\t");
    status = { serial: Number(n), text: rest.join("\t") };
  } catch (e) { /* none yet */ }
  return { alive, status, library };  // library: when the game last copied its effects out
}

function state() {
  const saved = list(SAVED, (n) => /\.vfx$/i.test(n)).map((f) => {
    const name = stem(f);
    let model = "none";
    try {
      const draft = fs.statSync(path.join(SAVED, f)).mtimeMs;
      model = fs.statSync(path.join(SAVED, name + ".mdl")).mtimeMs >= draft ? "written" : "older";
    } catch (e) { /* no model yet */ }
    return { name, model };
  });
  return {
    stale: stale(),
    build: BUILD,
    libraryFolder: LIBRARY,
    savedFolder: SAVED,
    exported: fs.existsSync(path.join(LIBRARY, "index.txt")),
    game: readIndex(),
    nwn5e: list(path.join(LIBRARY, "nwn5e"), (n) => /\.vfx$/i.test(n)).map(stem),
    saved,
    textures: list(path.join(LIBRARY, "textures"), (n) => /\.tga$/i.test(n)).map(stem),
    link: game(),
  };
}

function send(res, status, body, type) {
  res.writeHead(status, { "Content-Type": type || "application/json; charset=utf-8", "Cache-Control": "no-store" });
  res.end(typeof body === "string" || Buffer.isBuffer(body) ? body : JSON.stringify(body));
}

function readBody(req, limit) {
  return new Promise((resolve, reject) => {
    let size = 0;
    const chunks = [];
    req.on("data", (c) => {
      size += c.length;
      if (size > limit) { reject(new Error("too large")); req.destroy(); } else chunks.push(c);
    });
    req.on("end", () => resolve(Buffer.concat(chunks).toString("utf8")));
    req.on("error", reject);
  });
}

// A file written whole: the game reads it when its time changes, never half of it.
function writeWhole(file, text) {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  const temp = file + ".tmp";
  fs.writeFileSync(temp, text);
  fs.renameSync(temp, file);
}

function fromPage(req) {
  // Writes come from this page only: another site's page could post plain text here too, but its Origin differs.
  const origin = String(req.headers.origin || "");
  return origin === `http://127.0.0.1:${PORT}` || origin === `http://localhost:${PORT}`;
}

async function draftBody(req) {
  if (!String(req.headers["content-type"] || "").startsWith("text/plain")) throw new Error("send the draft as text/plain");
  const text = (await readBody(req, 262144)).replace(/\r?\n/g, "\n");
  const effect = core.readDraft(text);  // throws when it doesn't read, as the game would refuse it
  return { text, effect };
}

async function handle(req, res) {
  // Only pages served from here may call it (a web page elsewhere can't aim at it by a made-up host name).
  const host = String(req.headers.host || "");
  if (host !== `127.0.0.1:${PORT}` && host !== `localhost:${PORT}`) return send(res, 403, { error: "wrong host" });
  const url = new URL(req.url, URL_BASE);

  if (req.method === "GET" && STATIC[url.pathname]) {
    const file = STATIC[url.pathname];
    return fs.readFile(path.join(PUBLIC, file), (err, data) =>
      err ? send(res, 404, { error: "missing " + file }) : send(res, 200, data, TYPES[path.extname(file)]));
  }
  if (req.method === "GET" && url.pathname === "/api/state") return send(res, 200, state());
  if (req.method === "GET" && url.pathname === "/api/game") return send(res, 200, game());

  if (req.method === "GET" && url.pathname === "/api/draft") {
    const source = url.searchParams.get("source");
    const name = String(url.searchParams.get("name") || "").toLowerCase();
    const folder = source === "saved" ? SAVED : source === "nwn5e" ? path.join(LIBRARY, "nwn5e") : source === "game" ? path.join(LIBRARY, "game") : null;
    if (!folder || !LIBRARY_NAME.test(name)) return send(res, 400, { error: "bad source or name" });
    const file = list(folder, (n) => n.toLowerCase() === name + ".vfx")[0];
    if (!file) return send(res, 404, { error: "no draft " + name });
    return send(res, 200, fs.readFileSync(path.join(folder, file), "utf8"), "text/plain; charset=utf-8");
  }

  if (req.method === "GET" && url.pathname.startsWith("/api/texture/")) {
    const name = decodeURIComponent(url.pathname.slice("/api/texture/".length)).toLowerCase();
    if (!LIBRARY_NAME.test(name)) return send(res, 400, { error: "bad texture name" });
    const folder = path.join(LIBRARY, "textures");
    const file = list(folder, (n) => n.toLowerCase() === name + ".tga")[0];
    if (!file) return send(res, 404, { error: "no texture " + name });
    return fs.readFile(path.join(folder, file), (err, data) =>
      err ? send(res, 500, { error: err.message }) : send(res, 200, data, "application/octet-stream"));
  }

  if (req.method === "POST" && url.pathname === "/api/save") {
    if (!fromPage(req)) return send(res, 403, { error: "saves come from the bench's own page" });
    let draft;
    try { draft = await draftBody(req); } catch (e) { return send(res, 400, { error: e.message }); }
    const name = core.resref(draft.effect.name);
    if (!NAME.test(name) || name !== draft.effect.name) {
      return send(res, 400, { error: "a name is 1-16 lower-case letters, digits or _" });
    }
    const file = path.join(SAVED, name + ".vfx");
    const replaced = fs.existsSync(file);
    // The page's text as it is: this server's copy of the draft code may be older than the page's.
    writeWhole(file, draft.text);
    console.log(`saved ${file}${replaced ? " (replaced)" : ""}`);
    return send(res, 200, { name, path: file, replaced, state: state() });
  }

  if (req.method === "POST" && (url.pathname === "/api/play" || url.pathname === "/api/stop")) {
    if (!fromPage(req)) return send(res, 403, { error: "requests come from the bench's own page" });
    const play = url.pathname === "/api/play";
    let text = "";
    if (play) {
      try { text = (await draftBody(req)).text; } catch (e) { return send(res, 400, { error: e.message }); }
    }
    const n = ++serial;
    writeWhole(path.join(BENCH, "request.vfx"), `request = ${play ? "play" : "stop"} ${n}\n` + text);
    return send(res, 200, { serial: n, link: game() });
  }

  return send(res, 404, { error: "not found" });
}

function openBrowser() {
  if (!OPEN) return;
  const command = process.platform === "win32" ? `start "" "${URL_BASE}"` : process.platform === "darwin" ? `open ${URL_BASE}` : `xdg-open ${URL_BASE}`;
  exec(command, () => {});
}

const server = http.createServer((req, res) => {
  handle(req, res).catch((e) => {
    console.error(e);
    if (!res.headersSent) send(res, 500, { error: e.message });
  });
});
server.on("error", (e) => {
  if (e.code === "EADDRINUSE") {
    console.log(`Port ${PORT} is taken - the bench is probably running already. Opening ${URL_BASE}`);
    openBrowser();
    process.exitCode = 0;
    return;
  }
  console.error(e.message);
  process.exitCode = 1;
});
server.listen(PORT, "127.0.0.1", () => {
  const s = state();
  console.log("VFX Bench - " + URL_BASE);
  console.log("  library: " + LIBRARY + " (" + s.game.length + " game effects, " + s.textures.length + " textures)");
  console.log("  saved:   " + SAVED);
  if (!s.exported) console.log("  No library yet: start the game once with nwn5e 0.42.0 or later, then press Reload in the page.");
  console.log("Close this window (or Ctrl+C) to stop it.");
  openBrowser();
});
