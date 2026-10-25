// Runs the page's library logic (bodies, chains, clip lookup, bones) on the real libraries.
const fs = require("fs");
const path = require("path");
const A = require("./public/core.js");
const dir = process.argv[2];
const libs = new Map();
for (const f of fs.readdirSync(dir)) {
  if (!f.startsWith("a_") || !f.endsWith(".mdl")) continue;
  const b = fs.readFileSync(path.join(dir, f));
  const lib = A.readLibrary(b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength));
  libs.set(lib.name, lib);
  console.log(f, "->", lib.name, "super", JSON.stringify(lib.supermodel), "bones", lib.bones.length, "clips", lib.clips.size);
}
function chainNames(body) {
  const names = new Set();
  let current = body;
  for (let d = 0; d < 8 && current; d++) {
    const lib = libs.get(current);
    if (!lib) break;
    for (const n of lib.clips.keys()) names.add(n);
    current = lib.supermodel;
  }
  return names;
}
function findClip(body, name) {
  let current = body;
  for (let d = 0; d < 8 && current; d++) {
    const lib = libs.get(current);
    if (!lib) return null;
    if (lib.clips.has(name)) return [current, A.readClip(lib, name)];
    current = lib.supermodel;
  }
  return null;
}
const isSuper = new Set([...libs.values()].map((l) => l.supermodel));
const order = ["a_ba", "a_fa", "a_da", "a_dfa", "a_ba2", "a_fa2", "a_da2", "a_dfa2"];
console.log("bodies:", [...libs.keys()].filter((n) => !isSuper.has(n) || order.includes(n)).join(" "));
for (const body of order) {
  if (!libs.has(body)) continue;
  const names = chainNames(body);
  const p = findClip(body, "pausepsn"), q = findClip(body, "pause1"), w = findClip(body, "walk");
  console.log(body, "names", names.size, "pausepsn from", p && p[0], p && p[1].length.toFixed(2),
              "pause1 from", q && q[0], "walk from", w && w[0], w && w[1].length.toFixed(2));
}
const ba = libs.get("a_ba");
console.log("a_ba bones:", ba.bones.map((b) => b.name + "<" + (b.parent >= 0 ? ba.bones[b.parent].name : "-")).join(" "));
const r = { name: "n5_test", base: "pausepsn", mix: "pause1", amount: 0.5, speed: 1, start: 0, end: 0 };
const d = A.deriveClip(r, findClip("a_ba", "pausepsn")[1], findClip("a_ba", "pause1")[1]);
console.log("derived", d.length.toFixed(2), "nodes", d.nodes.length);
const fa = libs.get("a_fa");
if (fa) console.log("a_fa bone count", fa.bones.length, "first", fa.bones.slice(0, 6).map((b) => b.name).join(" "));
