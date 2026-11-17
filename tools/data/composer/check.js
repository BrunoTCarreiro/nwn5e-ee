#!/usr/bin/env node
// The Composer's project generator, its pure parts (project.js, and floatdisk.js's texts as build.js calls them),
// checked without the game or the reader - only the VFX Bench's core.js, from nwn5e-tools, for a spell mesh's draft:
//   node tools\data\composer\check.js [--tools <nwn5e-tools>]
"use strict";
const assert = require("assert");
const fs = require("fs");
const path = require("path");
const P = require("./project.js");
const F = require("../floatdisk/floatdisk.js");

const argv = process.argv.slice(2);
const option = (name, fallback) => {
  const i = argv.indexOf("--" + name);
  return i >= 0 && i + 1 < argv.length ? argv[i + 1] : fallback;
};
const TOOLS = path.resolve(option("tools", process.env.NWN5E_TOOLS || path.join(__dirname, "..", "..", "..", "..", "nwn5e-tools")));

let count = 0;
function check(name, fn) {
  try {
    fn();
    count++;
  } catch (e) {
    console.error("FAIL " + name + ": " + e.message);
    process.exitCode = 1;
  }
}
const near = (a, b, eps = 1e-4) => assert.ok(Math.abs(a - b) <= eps, `${a} is not ${b}`);
const says = (message) => (e) => {
  assert.strictEqual(e.message, message);
  return true;
};
const lines = (text) => text.split("\n");
const block = (text, head) => { const l = lines(text), i = l.indexOf(head); assert.ok(i >= 0, "no " + head); return l.slice(i, l.indexOf("endnode", i) + 1); };
const tok = (s) => [...s.matchAll(/"([^"]*)"|(\S+)/g)].map((m) => (m[1] !== undefined ? m[1] : m[2]));
const area = (points) => points.reduce((a, p, i) => { const q = points[(i + 1) % points.length]; return a + (p[0] * q[1] - q[0] * p[1]) / 2; }, 0);
const BASE = path.resolve("C:\\nwn5e\\tools\\data\\composer");
const CIRCLE = { shape: "circle", diameter: 1.2, height: 0 };
const LO = [-0.4, -0.45, 0], HI = [0.4, 0.45, 0.6];
const MESH = { name: "j01_Box", bitmap: "plc_x", verts: [-0.2, -0.3, 0, 0.2, 0.3, 0, 0, 0, 0.5], tverts: [0, 0, 1, 0, 0, 1], faces: [0, 1, 2],
  diffuse: [1, 1, 1], ambient: [1, 1, 1], selfillum: [0, 0, 0], alpha: 1 };

// The spec's example (the addendum's "The project file"), and a second variant of other parts.
const CRATES = {
  _about: ["A test project."],
  kind: "placeable",
  name: "crates",
  guide: { shape: "circle", diameter: 1.2, height: 0 },
  placeable: { copy: 2, use: [0, -0.7], walk: "blocks" },
  variants: [
    { name: "1", model: "plc_n5crates_1", row: 14100, parts: [
      { note: "a crate", model: "plc_a03", at: [0, 0, 0], turn: 15, scale: 0.5 },
    ] },
    { name: "big", model: "plc_whatever", parts: [
      { file: "parts/x.mdl", at: [0.1, 0, 0] },
      { model: ["WSWLS_B_011", "wswls_m_011", "wswls_t_011"], at: [0, 0, 0.2], tilt: [0, 90] },
      { model: "plc_b06", at: [0, 0.2, 0] },
    ] },
  ],
};
const copy = (o) => JSON.parse(JSON.stringify(o));

// --- loading --------------------------------------------------------------------------------------------------------
check("project.js loads in a page (no require, no module, no Buffer)", () => {
  const self = {};
  for (const file of [path.join(__dirname, "..", "floatdisk", "floatdisk.js"), path.join(__dirname, "project.js")]) {
    new Function("self", "require", "module", "Buffer", fs.readFileSync(file, "utf8"))(self);
  }
  const Q = self.ComposerProject;
  assert.strictEqual(typeof Q.readProject, "function");
  assert.strictEqual(Q.MARK, P.MARK);
  // It works there, with the page's FloatDisk: a project of game models reads, and its text is the same.
  const json = copy(CRATES);
  json.variants[1].parts.shift();  // an own file needs Node's path
  assert.deepStrictEqual(Q.readProject(json, "", self.FloatDisk), P.readProject(json, "", F));
  assert.strictEqual(Q.projectText(Q.newProject("crates", "placeable")), P.projectText(P.newProject("crates", "placeable")));
  assert.deepStrictEqual(Q.outline(CIRCLE), P.outline(CIRCLE));
});

// --- reading a project ------------------------------------------------------------------------------------------------
check("readProject: a placeable and a spell mesh", () => {
  const p = P.readProject(CRATES, BASE, F);
  assert.strictEqual(p.kind, "placeable");
  assert.strictEqual(p.name, "crates");
  assert.deepStrictEqual(p.guide, { shape: "circle", diameter: 1.2, width: null, length: null, height: 0 });
  assert.deepStrictEqual(p.placeable, { copy: 2, use: [0, -0.7], walk: "blocks" });
  // A variant's model is always its name's, never the file's (F15); a row only where the file has one.
  assert.deepStrictEqual(p.variants.map((v) => [v.name, v.model, v.row, v.parts.length]), [["1", "plc_n5crates_1", 14100, 1], ["big", "plc_n5crates_2", null, 3]]);
  // The parts through F.readPart: a game model, an own file from the base folder, a weapon's pieces.
  assert.deepStrictEqual(p.variants[0].parts[0], F.readPart(CRATES.variants[0].parts[0], "x", BASE));
  assert.deepStrictEqual(p.variants[1].parts[0].source, { kind: "file", file: path.join(BASE, "parts", "x.mdl"), given: "parts/x.mdl" });
  assert.deepStrictEqual(p.variants[1].parts[1].source, { kind: "models", models: ["wswls_b_011", "wswls_m_011", "wswls_t_011"] });
  assert.deepStrictEqual(p.variants[1].parts[1].tilt, [0, 90]);
  // The defaults: no placeable settings is walk-through, no copy, no use point; a rectangle; one variant has no _1.
  const q = P.readProject({ kind: "placeable", name: "b2", guide: { shape: "rect", width: 2, length: 1 }, variants: [{ name: "1", parts: [] }] }, BASE, F);
  assert.deepStrictEqual(q.placeable, { copy: null, use: null, walk: "through" });
  assert.deepStrictEqual(q.guide, { shape: "rect", diameter: null, width: 2, length: 1, height: 0 });
  assert.deepStrictEqual(q.variants, [{ name: "1", model: "plc_n5b2", row: null, parts: [] }]);  // an empty variant reads (build.js refuses it)
  // A spell mesh: no placeable settings, no rows; its guide may sit below the ground.
  const m = P.readProject({ kind: "spellmesh", name: "web", guide: { shape: "none", height: -0.25 }, placeable: { walk: "blocks" },
    variants: [{ name: "1", row: 14100, parts: [{ model: "plc_b06", at: [0, 0, 0] }] }] }, BASE, F);
  assert.strictEqual(m.placeable, null);
  assert.deepStrictEqual(m.guide, { shape: "none", diameter: null, width: null, length: null, height: -0.25 });
  assert.deepStrictEqual(m.variants.map((v) => [v.model, v.row]), [["n5m_web", null]]);
  // A new project reads.
  for (const kind of P.KINDS) {
    const n = P.readProject(P.newProject("crates", kind), BASE, F);
    assert.deepStrictEqual(n.variants, [{ name: "1", model: P.modelName(kind, "crates", 1, 1), row: null, parts: [] }]);
  }
});

check("readProject: each error names its field", () => {
  const bad = (edit, expected) => {
    const j = copy(CRATES);
    edit(j);
    assert.throws(() => P.readProject(j, BASE, F), typeof expected === "string" ? says(expected) : expected);
  };
  bad((j) => { j.kind = "door"; }, "kind: placeable or spellmesh");
  bad((j) => { j.name = "Crates"; }, "name: 1-8 letters or digits, lower-case");
  bad((j) => { j.name = "abcdefghi"; }, "name: 1-8 letters or digits, lower-case");
  bad((j) => { j.guide.shape = "star"; }, "guide.shape: circle, rect or none");
  bad((j) => { delete j.guide; }, "guide.shape: circle, rect or none");
  bad((j) => { j.guide.diameter = 0; }, "guide.diameter: a number above 0");
  bad((j) => { j.guide = { shape: "rect", length: 1 }; }, "guide.width: a number above 0");
  bad((j) => { j.guide.height = "low"; }, "guide.height: a number (metres)");
  bad((j) => { j.placeable.walk = "block"; }, "placeable.walk: through or blocks");
  bad((j) => { j.placeable.use = [0]; }, "placeable.use: [x, y] in metres, or null");
  bad((j) => { j.placeable.copy = -1; }, "placeable.copy: a placeables.2da row (a whole number), or null");
  bad((j) => { j.variants = []; }, "variants: a list of 1 to 9 variants");
  bad((j) => { j.variants[1].name = "1"; }, "variants[1].name: 1-4 letters or digits, unique");
  bad((j) => { j.variants[1].name = "large"; }, "variants[1].name: 1-4 letters or digits, unique");
  bad((j) => { j.variants[0].row = 14099; }, "variants[0].row: 14100-14199");
  bad((j) => { j.variants[0].row = 14200; }, "variants[0].row: 14100-14199");
  bad((j) => { j.variants[0].row = 14100.5; }, "variants[0].row: 14100-14199");
  bad((j) => { j.variants[1].row = 14100; }, /^Error: variants\[1\]\.row: 14100 is variants\[0\]'s too/);
  bad((j) => { j.variants[1].parts = {}; }, "variants[1].parts: a list of parts");
  bad((j) => { j.variants[0].parts = [{ model: "a", at: [0, 0, 0] }, { model: "b", at: [0, 0, 0] }, { model: "c", at: [0, 0, 0], scale: 0 }]; },
    "variants[0].parts[2].scale: a number above 0");
  bad((j) => { j.variants[1].parts[2] = { at: [0, 0, 0] }; }, /^Error: variants\[1\]\.parts\[2\]: either "model"/);
  bad((j) => { j.variants[1].parts[0].file = "x.obj"; }, /^Error: variants\[1\]\.parts\[0\]\.file: /);
});

// --- names, rows ----------------------------------------------------------------------------------------------------
check("model names and labels fit (16 characters)", () => {
  assert.strictEqual(P.modelName("placeable", "crates", 1, 1), "plc_n5crates");
  assert.strictEqual(P.modelName("placeable", "crates", 2, 3), "plc_n5crates_2");
  assert.strictEqual(P.modelName("spellmesh", "web", 1, 1), "n5m_web");
  assert.strictEqual(P.modelName("spellmesh", "web", 9, 9), "n5m_web_9");
  assert.strictEqual(P.labelOf("crates", 1, 1), "N5_Crates");
  assert.strictEqual(P.labelOf("crates", 2, 2), "N5_Crates_2");
  // The longest: an 8-letter name at its 9th variant; a 10th variant is refused (F16).
  for (const kind of P.KINDS) {
    for (let n = 1; n <= 9; n++) for (let k = 1; k <= n; k++) assert.ok(F.RESREF.test(P.modelName(kind, "abcdefgh", k, n)), P.modelName(kind, "abcdefgh", k, n));
  }
  assert.strictEqual(P.modelName("placeable", "abcdefgh", 9, 9).length, 16);
  const ten = { kind: "placeable", name: "abcdefgh", guide: CIRCLE, variants: Array.from({ length: 10 }, (_, k) => ({ name: String(k + 1), parts: [] })) };
  assert.throws(() => P.readProject(ten, BASE, F), says("variants: a list of 1 to 9 variants"));
  ten.variants.pop();
  assert.strictEqual(P.readProject(ten, BASE, F).variants[8].model, "plc_n5abcdefgh_9");
  assert.ok(P.NAME.test("abcdefgh") && P.NAME.test("crates2") && !P.NAME.test("abcdefghi") && !P.NAME.test("Crates") && !P.NAME.test("a_b"));
  assert.ok(P.VARIANT.test("1") && P.VARIANT.test("big") && !P.VARIANT.test("large") && !P.VARIANT.test(""));
});

check("freeRows: the lowest free, own rows kept, never a taken one", () => {
  assert.deepStrictEqual(P.ROWS, [14100, 14199]);
  assert.deepStrictEqual(P.freeRows([], 2), [14100, 14101]);
  assert.deepStrictEqual(P.freeRows([14100, 14102], 3), [14101, 14103, 14104]);
  assert.deepStrictEqual(P.freeRows(new Set([14100]), 3, [14105, 14101]), [14105, 14101, 14102]);  // its own first, then new ones past them
  assert.deepStrictEqual(P.freeRows([14001, 14002, 14003], 1), [14100]);  // the disk's rows are below the range
  const all = Array.from({ length: 100 }, (_, i) => 14100 + i);
  assert.deepStrictEqual(P.freeRows(all.slice(0, 99), 1), [14199]);
  assert.throws(() => P.freeRows(all, 1), says("the Composer's placeable rows (14100-14199) are all taken"));
  assert.throws(() => P.freeRows(all.slice(0, 99), 2), says("the Composer's placeable rows (14100-14199) are all taken"));
  // What's taken: the rows a rows file sets (a value other than ****) and the rows a table uses (a model, not USER).
  const text = [F.MARK, "# why", "       Label  ModelName  Static", "14100  N5_A   plc_a      1", "14101  ****   ****       ****",
    '14102  "a b"  ****  ****', "+      N5_C   plc_c      1", "14103  ****   ****       **** # 1", "14104", ""].join("\r\n");
  assert.deepStrictEqual(P.rowsUsed(text), [14100, 14102]);
  const table = { columns: ["Label", "ModelName", "Static"], rows: new Map([[13, { Label: "LootBag1", ModelName: "PLC_B04", Static: "1" }],
    [14100, { Label: "****", ModelName: "USER", Static: "1" }], [14101, { Label: "X", ModelName: "plc_x", Static: "1" }],
    [14102, { Label: "****", ModelName: "****", Static: "****" }], [14103, { Label: "****", ModelName: "user", Static: "1" }]]) };
  assert.deepStrictEqual(P.tableRowsUsed(table), [13, 14101]);
});

// --- the guide ------------------------------------------------------------------------------------------------------
check("outline: a circle's 16 points, a rectangle's 4, none", () => {
  const c = P.outline(CIRCLE);
  assert.strictEqual(c.length, 16);
  assert.deepStrictEqual([c[0], c[4], c[8], c[12]], [[0.6, 0], [0, 0.6], [-0.6, 0], [0, -0.6]]);  // the first at +x, counter-clockwise
  for (const [x, y] of c) near(Math.hypot(x, y), 0.6, 1e-5);
  assert.ok(area(c) > 0);
  assert.strictEqual(P.outline({ shape: "circle", diameter: 1, height: 0 }, 8).length, 8);
  const r = P.outline({ shape: "rect", width: 2, length: 1, height: 0 });
  assert.deepStrictEqual(r, [[1, 0.5], [-1, 0.5], [-1, -0.5], [1, -0.5]]);  // width along x, length along y
  assert.ok(area(r) > 0);
  assert.deepStrictEqual(P.outline({ shape: "none", height: 0 }), []);
  assert.deepStrictEqual(P.outline(null), []);
});

check("defaultUse: 0.1 m past the guide's front, at -y", () => {
  assert.deepStrictEqual(P.defaultUse({ shape: "circle", diameter: 1, height: 0 }), [0, -0.6]);
  assert.deepStrictEqual(P.defaultUse({ shape: "circle", diameter: 1.2, height: 0.5 }), [0, -0.7]);
  assert.deepStrictEqual(P.defaultUse({ shape: "rect", width: 3, length: 1, height: 0 }), [0, -0.6]);  // half the length, not the width
  assert.deepStrictEqual(P.defaultUse({ shape: "none", height: 0 }), [0, -0.5]);
  assert.deepStrictEqual(P.defaultUse(null), [0, -0.5]);
});

check("newScale: within 0.4 of the guide, never above 1", () => {
  assert.strictEqual(P.newScale([0, 0, 0], [2, 1, 1], { shape: "circle", diameter: 1, height: 0 }), 0.2);  // 0.4 x 1 m over 2 m
  assert.strictEqual(P.newScale([-0.1, -0.1, 0], [0.1, 0.1, 3], { shape: "circle", diameter: 1, height: 0 }), 1);  // small already: never above 1
  assert.strictEqual(P.newScale([0, 0, 0], [1, 3, 0], { shape: "rect", width: 2, length: 1.5, height: 0 }), 0.267);  // the longer side: 0.4 x 2 / 3
  assert.strictEqual(P.newScale([0, 0, 0], [0.9, 0.5, 0.3], { shape: "circle", diameter: 0.9, height: 0.9 }), 0.4);  // the disk's: 0.36 m
  assert.strictEqual(P.newScale([0, 0, 0], [5, 5, 5], { shape: "none", height: 0 }), 1);
  assert.strictEqual(P.newScale([0, 0, 0], [5, 5, 5], null), 1);
  assert.strictEqual(P.newScale([0, 0, 0], [0, 0, 1], CIRCLE), 1);  // no footprint
  assert.ok(P.newScale([0, 0, 0], [1e6, 1, 1], CIRCLE) > 0);  // never 0, a scale must be above 0
});

// --- the walkmesh ---------------------------------------------------------------------------------------------------
check("a walk-through pwk: its use point, no ground mesh", () => {
  const guide = { shape: "circle", diameter: 1, height: 0 };
  const text = F.pileWalkmeshText("plc_n5crates", LO, HI, { mark: P.MARK, use: [P.defaultUse(guide)], block: null });
  const l = lines(text);
  assert.deepStrictEqual(l.slice(0, 3), ["#MAXDOOR ASCII", P.MARK, "# model: plc_n5crates_pwk"]);
  assert.deepStrictEqual(block(text, "node dummy n5crates_pwk_use01").slice(1, 3), ["  parent plc_n5crates_pwk", "  position 0 -0.6 0"]);
  assert.ok(!l.some((x) => x.startsWith("node trimesh") || x.startsWith("node dummy n5crates_pwk_use02")));
  assert.ok(F.carriesMark(Buffer.from(F.crlf(text)), P.MARK) && !F.carriesMark(Buffer.from(text)));
  // Use points as given, numbered from 01.
  const two = F.pileWalkmeshText("plc_n5crates", LO, HI, { mark: P.MARK, use: [[0.2, -0.7], [0, 0.7]] });
  assert.strictEqual(block(two, "node dummy n5crates_pwk_use01")[2], "  position 0.2 -0.7 0");
  assert.strictEqual(block(two, "node dummy n5crates_pwk_use02")[2], "  position 0 0.7 0");
});

check("a blocking pwk: the outline as faces of material 7, then the use point", () => {
  const ring = P.outline(CIRCLE);
  const text = F.pileWalkmeshText("plc_n5crates_2", LO, HI, { mark: P.MARK, use: [[0, -0.7]], block: ring });
  const wg = block(text, "node trimesh PLC_N5CRATES_2_wg");  // as the game's PLC_A03.pwk: PLC_A03_wg under PLC_A03_pwk
  assert.deepStrictEqual(wg.slice(1, 5), ["  parent plc_n5crates_2_pwk", "  position 0 0 0", "  orientation 0 0 0 0", "  bitmap NULL"]);
  const vi = wg.indexOf("  verts 16");
  assert.ok(vi > 0, "no verts 16");
  const verts = wg.slice(vi + 1, vi + 17).map((s) => s.trim().split(/\s+/).map(Number));
  verts.forEach((v, i) => assert.deepStrictEqual(v, [ring[i][0], ring[i][1], 0]));
  const fi = wg.indexOf("  faces 14");
  assert.strictEqual(fi, vi + 17, "faces 14 after the verts");
  const used = new Set();
  for (const f of wg.slice(fi + 1, fi + 15)) {
    assert.match(f, /^    \d+ \d+ \d+  1  0 0 0  7$/);
    const [a, b, c] = f.trim().split(/\s+/).slice(0, 3).map(Number);
    assert.ok(area([verts[a], verts[b], verts[c]]) > 0, f + " is not counter-clockwise from above");
    [a, b, c].forEach((k) => used.add(k));
  }
  assert.strictEqual(used.size, 16);
  assert.strictEqual(wg[fi + 15], "endnode");
  const l = lines(text);
  assert.ok(l.indexOf("node trimesh PLC_N5CRATES_2_wg") < l.indexOf("node dummy n5crates_2_pwk_use01"));
  assert.strictEqual(block(text, "node dummy n5crates_2_pwk_use01")[2], "  position 0 -0.7 0");
  // A rectangle's: 4 corners, 2 faces. Fewer than 3 points is no outline.
  const rect = block(F.pileWalkmeshText("plc_n5b", LO, HI, { mark: P.MARK, use: [[0, -0.6]], block: P.outline({ shape: "rect", width: 1, length: 1, height: 0 }) }), "node trimesh PLC_N5B_wg");
  assert.ok(rect.includes("  verts 4") && rect.includes("  faces 2"));
  assert.throws(() => F.pileWalkmeshText("plc_n5b", LO, HI, { block: [[0, 0], [1, 0]] }), /outline/);
});

// --- the model and the rows ----------------------------------------------------------------------------------------
check("a placeable's model and rows carry the Composer's mark; the rows copy the source row", () => {
  const model = F.pileModelText("plc_n5crates", [MESH], LO, HI, ["about"], P.MARK);
  assert.deepStrictEqual(lines(model).slice(0, 3), ["#MAXMODEL ASCII", P.MARK, "# about"]);
  assert.ok(F.carriesMark(Buffer.from(F.crlf(model)), P.MARK) && !F.carriesMark(Buffer.from(model)));
  // The row copied: the card's, else the first placeable part's (the lowest row whose ModelName is its model), else 189.
  const table = { columns: ["Label", "StrRef", "ModelName", "SoundAppType", "Static"], rows: new Map([
    [189, { Label: "TreasureSml", StrRef: "63232", ModelName: "PLC_C07", SoundAppType: "21", Static: "1" }],
    [2, { Label: "Box1", StrRef: "5", ModelName: "PLC_A03", SoundAppType: "1", Static: "1" }],
    [40, { Label: "LootBag2", StrRef: "6", ModelName: "PLC_B04", SoundAppType: "23", Static: "1" }],
    [13, { Label: "LootBag1", StrRef: "5656", ModelName: "PLC_B04", SoundAppType: "23", Static: "1" }]]) };
  const project = (first, second, copyRow = null) => P.readProject({ kind: "placeable", name: "crates", guide: CIRCLE,
    placeable: { copy: copyRow, use: null, walk: "through" }, variants: [{ name: "1", parts: first }, { name: "2", parts: second }] }, BASE, F);
  const at = [0, 0, 0];
  const own = { file: "parts/x.mdl", at }, weapon = { model: ["wswls_b_011", "wswls_m_011", "wswls_t_011"], at };
  assert.strictEqual(P.sourceRow(project([own, weapon], [{ model: "plc_b04", at }, { model: "plc_a03", at }]), table), 13);
  assert.strictEqual(P.sourceRow(project([{ model: "plc_a03", at }], [{ model: "plc_b04", at }]), table), 2);
  assert.strictEqual(P.sourceRow(project([{ model: "plc_a03", at }], [], 40), table), 40);
  assert.strictEqual(P.sourceRow(project([own, { model: "plc_zz9", at }], []), table), P.SOURCE_ROW);
  assert.strictEqual(P.SOURCE_ROW, 189);
  const rows = F.rowsText(table, 13, [{ row: 14100, label: P.labelOf("crates", 1, 2), model: "plc_n5crates_1" },
    { row: 14101, label: P.labelOf("crates", 2, 2), model: "plc_n5crates_2" }], { labelColumn: "Label", modelColumn: "ModelName", mark: P.MARK, about: ["why"] });
  assert.deepStrictEqual(lines(rows).slice(0, 2), [P.MARK, "# why"]);
  assert.ok(F.carriesMark(Buffer.from(F.crlf(rows)), P.MARK) && !F.carriesMark(Buffer.from(rows)));
  const l = lines(rows).filter((x) => x && !x.startsWith("#"));
  assert.deepStrictEqual(tok(l[0]), table.columns);
  assert.deepStrictEqual(tok(l[1]), ["14100", "N5_Crates_1", "5656", "plc_n5crates_1", "23", "1"]);
  assert.deepStrictEqual(tok(l[2]), ["14101", "N5_Crates_2", "5656", "plc_n5crates_2", "23", "1"]);
});

check("a spell mesh's draft reads back with its meshes as planes", () => {
  global.window = global.window || {};  // core.js runs in the page and under node
  const core = require(path.join(TOOLS, "vfx_bench", "public", "core.js"));
  const glow = Object.assign({}, MESH, { name: "j02_Glow", bitmap: "", diffuse: [0.5, 0.2, 0.1], selfillum: [0.3, 0.3, 0.9], alpha: 0.5 });
  const meshes = F.uniqueNames([MESH, glow], []);
  const text = P.draftText(core, "n5m_web", meshes);
  assert.strictEqual(lines(text)[0], P.MARK);
  assert.ok(F.carriesMark(Buffer.from(F.crlf(text)), P.MARK));
  const fx = core.readDraft(F.crlf(text).replace(/\r/g, ""));
  assert.deepStrictEqual([fx.name, fx.duration, fx.seconds, fx.attach], ["n5m_web", true, 2, "root"]);
  assert.deepStrictEqual(fx.emitters, []);
  assert.strictEqual(fx.planes.length, 2);
  fx.planes.forEach((pl, i) => {
    const m = meshes[i];
    assert.strictEqual(pl.name, m.name);
    assert.strictEqual(pl.bitmap, m.bitmap || "null");
    assert.deepStrictEqual(pl.faces, m.faces);
    pl.verts.forEach((v, k) => near(v, m.verts[k], 1e-5));
    pl.tverts.forEach((v, k) => near(v, m.tverts[k], 1e-5));
    [...pl.diffuse, ...pl.selfillum, pl.alpha].forEach((v, k) => near(v, [...m.diffuse, ...m.selfillum, m.alpha][k], 1e-6));  // read as float32, as the game does
  });
});

// --- the project file -----------------------------------------------------------------------------------------------
check("projectText: a part a line, and a round trip", () => {
  const json = copy(CRATES);
  json.variants[1] = { parts: [], name: "2" };
  json.variants[0] = { parts: json.variants[0].parts, row: 14100, name: "1", model: "plc_n5crates_1" };  // keys in any order
  json.variants[0].parts.push({ scale: 1, at: [0.25, -0.1, 0.4], model: "plc_b06", note: "on top", extra: 1 });
  const text = P.projectText(json);
  assert.strictEqual(text, [
    "{",
    '  "_about": [',
    '    "A test project."',
    "  ],",
    '  "kind": "placeable",',
    '  "name": "crates",',
    '  "guide": { "shape": "circle", "diameter": 1.2, "height": 0 },',
    '  "placeable": { "copy": 2, "use": [0, -0.7], "walk": "blocks" },',
    '  "variants": [',
    '    { "name": "1", "model": "plc_n5crates_1", "row": 14100, "parts": [',
    '      { "note": "a crate", "model": "plc_a03", "at": [0, 0, 0], "turn": 15, "scale": 0.5 },',
    '      { "note": "on top", "model": "plc_b06", "at": [0.25, -0.1, 0.4], "scale": 1, "extra": 1 }',
    "    ] },",
    '    { "name": "2", "parts": [] }',
    "  ]",
    "}",
    "",
  ].join("\n"));
  assert.deepStrictEqual(JSON.parse(text), json);
  assert.strictEqual(P.projectText(JSON.parse(text)), text);
  // A new project: its guide, the placeable's settings, one empty variant.
  const pile = P.projectText(P.newProject("crates", "placeable"));
  assert.ok(pile.includes('\n  "guide": { "shape": "circle", "diameter": 1, "height": 0 },\n'));
  assert.ok(pile.includes('\n  "placeable": { "copy": null, "use": null, "walk": "through" },\n'));
  assert.ok(pile.includes('\n  "variants": [\n    { "name": "1", "parts": [] }\n  ]\n}\n'));
  const mesh = P.projectText(P.newProject("web", "spellmesh"));
  assert.ok(mesh.includes('\n  "guide": { "shape": "circle", "diameter": 0.5, "height": 0 },\n') && !mesh.includes("placeable\""));
  assert.ok(lines(mesh)[1] === '  "_about": [' && !mesh.includes("\r"));
  assert.deepStrictEqual(Object.keys(P.newProject("web", "spellmesh")), ["_about", "kind", "name", "guide", "variants"]);
  assert.throws(() => P.newProject("Bad-Name", "placeable"), /^Error: name: /);
  assert.throws(() => P.newProject("crates", "door"), /^Error: kind: /);
});

console.log(process.exitCode ? `composer check: FAILED (${count} passed)` : `composer check: ${count} checks pass`);
