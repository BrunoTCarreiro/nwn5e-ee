#!/usr/bin/env node
// Animation Recipe Bench - a local web app for nwn5e's animation recipes (owner,
// 2026-10-25: "the saving closing and opening the game just to get a preview is
// not great. could we get a webapp working" - "don't make it an artifact, create
// an actual webapp I can run locally").
//
// Start it with recipe_bench.bat (or `node tools/recipe_bench/server.js`); it
// opens http://127.0.0.1:8765/ in the browser. It reads the animation libraries
// nwn5e copies out at each game start (build\animedit\libraries\) and saves
// recipes straight into build\animedit\recipes\, where the next game start builds
// them. Only this machine can reach it (127.0.0.1); no dependencies.
//
// options: --build <folder>  nwn5e's build folder (default: ../../build, or NWN5E_BUILD)
//          --port <n>        (default 8765)
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
const PORT = Number(option("port", "8765"));
const OPEN = !args.includes("--no-open");
const LIBRARIES = path.join(BUILD, "animedit", "libraries");
const RECIPES = path.join(BUILD, "animedit", "recipes");
const NWN5E_RECIPES = path.join(BUILD, "data", "anims");
const PUBLIC = path.join(__dirname, "public");
const STATIC = { "/": "index.html", "/index.html": "index.html", "/app.js": "app.js", "/core.js": "core.js", "/style.css": "style.css" };
const TYPES = { ".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8", ".css": "text/css; charset=utf-8" };
const NAME = /^[a-z0-9_]{1,32}$/;
const RECIPE_FILE = /\.recipe$|\.recipe.*\.txt$/i;  // as nwn5e reads them (anim_packs.cpp)
const URL_BASE = `http://127.0.0.1:${PORT}/`;
// The code this server runs, from when it started: a bench updated since needs a restart
// (owner, 2026-10-25: two recipes saved through a server older than crouch and lean lost both).
const STARTED = Date.now();
const CODE = [__filename, path.join(PUBLIC, "core.js")];

function list(folder, test) {
  try {
    return fs.readdirSync(folder, { withFileTypes: true }).filter((e) => e.isFile() && test(e.name)).map((e) => e.name).sort();
  } catch (e) {
    return [];
  }
}

// The recipes in a folder, one per name: the newest file wins, as in the game.
function recipesIn(folder, source) {
  const byName = new Map();
  for (const file of list(folder, (n) => RECIPE_FILE.test(n))) {
    try {
      const full = path.join(folder, file);
      const recipe = core.readRecipe(fs.readFileSync(full, "utf8"));
      const name = recipe.name.toLowerCase();
      const modified = fs.statSync(full).mtimeMs;
      const seen = byName.get(name);
      if (!seen || seen.modified < modified) byName.set(name, { name, file, source, modified });
    } catch (e) {
      // not a recipe: the game skips it too
    }
  }
  return [...byName.values()].sort((a, b) => a.name.localeCompare(b.name));
}

function stale() {
  return CODE.some((file) => { try { return fs.statSync(file).mtimeMs > STARTED; } catch (e) { return false; } });
}

function state() {
  return {
    stale: stale(),
    build: BUILD,
    librariesFolder: LIBRARIES,
    recipesFolder: RECIPES,
    libraries: list(LIBRARIES, (n) => /\.mdl$/i.test(n)).map((n) => n.slice(0, -4).toLowerCase()).filter((n) => NAME.test(n)),
    recipes: [...recipesIn(RECIPES, "yours"), ...recipesIn(NWN5E_RECIPES, "nwn5e")],
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

  if (req.method === "GET" && url.pathname.startsWith("/api/library/")) {
    const name = decodeURIComponent(url.pathname.slice("/api/library/".length)).toLowerCase();
    if (!NAME.test(name)) return send(res, 400, { error: "bad library name" });
    const file = list(LIBRARIES, (n) => n.toLowerCase() === name + ".mdl")[0];
    if (!file) return send(res, 404, { error: "no library " + name });
    return fs.readFile(path.join(LIBRARIES, file), (err, data) =>
      err ? send(res, 500, { error: err.message }) : send(res, 200, data, "application/octet-stream"));
  }

  if (req.method === "GET" && url.pathname === "/api/recipe") {
    const folder = url.searchParams.get("source") === "nwn5e" ? NWN5E_RECIPES : RECIPES;
    const file = url.searchParams.get("file") || "";
    if (path.basename(file) !== file || !RECIPE_FILE.test(file) || !list(folder, (n) => n === file).length) {
      return send(res, 404, { error: "no such recipe" });
    }
    return send(res, 200, fs.readFileSync(path.join(folder, file), "utf8"), "text/plain; charset=utf-8");
  }

  if (req.method === "POST" && url.pathname === "/api/recipe") {
    // Saves come from this page only: another site's page could post plain text here too, but its Origin differs.
    const origin = String(req.headers.origin || "");
    if (origin !== `http://127.0.0.1:${PORT}` && origin !== `http://localhost:${PORT}`) return send(res, 403, { error: "saves come from the bench's own page" });
    if (!String(req.headers["content-type"] || "").startsWith("text/plain")) return send(res, 415, { error: "send the recipe as text/plain" });
    let recipe, text;
    try {
      text = await readBody(req, 16384);
      recipe = core.readRecipe(text);
    } catch (e) {
      return send(res, 400, { error: e.message });
    }
    recipe.name = recipe.name.toLowerCase();
    if (!NAME.test(recipe.name) || !text.split(/\r?\n/).some((l) => /^\s*name\s*=\s*[a-z0-9_]{1,32}\s*$/.test(l))) {
      return send(res, 400, { error: "a name is 1-32 lower-case letters, digits or _" });
    }
    fs.mkdirSync(RECIPES, { recursive: true });
    const file = recipe.name + ".recipe";
    const replaced = fs.existsSync(path.join(RECIPES, file));
    // The page's text as it is: this server's own copy of the recipe code may be older
    // than the page's and would drop settings it doesn't know.
    fs.writeFileSync(path.join(RECIPES, file), text.replace(/\r?\n/g, "\n"));
    const overridesNwn5e = recipesIn(NWN5E_RECIPES, "nwn5e").some((r) => r.name === recipe.name);
    console.log(`saved ${path.join(RECIPES, file)}${replaced ? " (replaced)" : ""}`);
    return send(res, 200, { file, path: path.join(RECIPES, file), replaced, overridesNwn5e, state: state() });
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
  console.log("Animation Recipe Bench - " + URL_BASE);
  console.log("  libraries: " + LIBRARIES + " (" + s.libraries.length + ")");
  console.log("  recipes:   " + RECIPES);
  if (!s.libraries.length) console.log("  No libraries yet: start the game once with nwn5e 0.24.2 or later, then press Reload in the page.");
  console.log("Close this window (or Ctrl+C) to stop it.");
  openBrowser();
});
