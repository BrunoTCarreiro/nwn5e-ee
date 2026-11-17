#!/usr/bin/env node
// The Floating Disk generator's pure parts (floatdisk.js), checked without the game or the reader:
//   node tools\data\floatdisk\check.js
"use strict";
const assert = require("assert");
const path = require("path");
const F = require("./floatdisk.js");

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
const nearAll = (a, b, eps) => { assert.strictEqual(a.length, b.length); a.forEach((v, i) => near(v, b[i], eps)); };
const BASE = path.resolve("C:\\nwn5e\\tools\\data\\floatdisk");
const DISK = { diameter: 0.9, thickness: 0.025, height: 0.9, sides: 40, colour: [0.42, 0.62, 0.95], rim: [0.8, 0.92, 1], alpha: 0.6,
  selfillum: [0.25, 0.42, 0.75], texture: "n5t_forcedisk" };
const comp = (small) => ({ disk: DISK, stages: { small, medium: [], large: [] } });

// --- a part's own file -------------------------------------------------------------------------------------------------
check("a part's file resolves beside the generator", () => {
  assert.strictEqual(F.resolvePartFile(BASE, "parts/my_pile.mdl"), path.join(BASE, "parts", "my_pile.mdl"));
  assert.strictEqual(F.resolvePartFile(BASE, "parts\\My Pile.MDL"), path.join(BASE, "parts", "My Pile.MDL"));
  assert.strictEqual(F.resolvePartFile(BASE, "../piles/a.mdl"), path.resolve(BASE, "..", "piles", "a.mdl"));
  assert.strictEqual(F.resolvePartFile(BASE, "D:\\art\\pile.mdl"), path.resolve("D:\\art\\pile.mdl"));
  assert.strictEqual(F.resolvePartFile(BASE, "  parts/a.mdl "), path.join(BASE, "parts", "a.mdl"));
});
check("a part's file must be a .mdl", () => {
  assert.throws(() => F.resolvePartFile(BASE, "parts/pile.blend"), /\.mdl/);
  assert.throws(() => F.resolvePartFile(BASE, ""), /\.mdl/);
  assert.throws(() => F.resolvePartFile(BASE, 3), /\.mdl/);
});
check("a part names a game model or a file, not both", () => {
  const c = F.readComposition(comp([{ model: "PLC_B04", at: [0, 0, 0] }, { file: "parts/x.mdl", at: [0.1, 0, 0], turn: 30, scale: 2 }]), BASE);
  assert.deepStrictEqual(c.stages.small[0].source, { kind: "model", model: "plc_b04" });
  assert.deepStrictEqual(c.stages.small[1].source, { kind: "file", file: path.join(BASE, "parts", "x.mdl"), given: "parts/x.mdl" });
  assert.strictEqual(F.partKey(c.stages.small[0]), "model:plc_b04");
  assert.strictEqual(F.partLabel(c.stages.small[1]), "parts/x.mdl");
  assert.deepStrictEqual(c.stages.empty, []);
  assert.throws(() => F.readComposition(comp([{ model: "plc_b04", file: "a.mdl", at: [0, 0, 0] }]), BASE), /either/);
  assert.throws(() => F.readComposition(comp([{ at: [0, 0, 0] }]), BASE), /either/);
  assert.throws(() => F.readComposition(comp([{ file: "a.obj", at: [0, 0, 0] }]), BASE), /stages\.small\[0\]\.file/);
  assert.throws(() => F.readComposition(comp([{ model: "plc_b04", at: [0, 0] }]), BASE), /at/);
  assert.throws(() => F.readComposition(comp([{ model: "plc_b04", at: [0, 0, 0], scale: 0 }]), BASE), /scale/);
});

// --- maths ----------------------------------------------------------------------------------------------------------
check("axis-angle and quaternions round-trip", () => {
  const q = F.quat([0, 0, 1, Math.PI / 2]);
  nearAll(F.qrot(q, [1, 0, 0]), [0, 1, 0]);
  nearAll(F.axisAngle(q), [0, 0, 1, Math.PI / 2]);
  assert.deepStrictEqual(F.axisAngle([0, 0, 0, 1]), [0, 0, 0, 0]);
  assert.deepStrictEqual(F.quat([0, 0, 0, 0]), [0, 0, 0, 1]);
  assert.strictEqual(F.num(-0.000001), "0");
  assert.strictEqual(F.num(1.234567), "1.23457");
  assert.strictEqual(F.num(1e-7), "0");
});
const cube = () => {  // a 1 m cube on the origin's floor (z 0..1), one mesh
  const verts = [];
  for (const z of [0, 1]) for (const y of [-0.5, 0.5]) for (const x of [-0.5, 0.5]) verts.push(x + 2, y, z);  // off-centre in x
  return { meshes: [{ name: "Box", bitmap: "plc_x", verts, tverts: new Array(16).fill(0), faces: [0, 1, 2], diffuse: [1, 1, 1],
    ambient: [1, 1, 1], selfillum: [0, 0, 0], alpha: 1 }] };
};
check("a part sits on the floor, its footprint centred where it's put", () => {
  const part = { at: [0.1, -0.2, 0.05], turn: 90, scale: 0.5, tilt: [0, 0] };
  const placed = F.placePart(part, cube(), 2, 0.9, "j");
  assert.strictEqual(placed.meshes[0].name, "j03_Box");
  near(placed.lo[2], 0.95);
  near(placed.hi[2], 1.45);
  near((placed.lo[0] + placed.hi[0]) / 2, 0.1);
  near((placed.lo[1] + placed.hi[1]) / 2, -0.2);
  near(placed.hi[0] - placed.lo[0], 0.5);
});
check("a scene's meshes come into the model's space through their parents", () => {
  const fx = { nodes: [{ name: "turned", parent: "", position: [1, 0, 0], orientation: [0, 0, 1, Math.PI / 2] }],
    planes: [{ name: "m", parent: "turned", position: [1, 0, 0], orientation: [0, 0, 0, 0], verts: [0, 0, 0], tverts: [0, 0], faces: [0, 0, 0], bitmap: "NULL" },
      { name: "hidden", parent: "", position: [0, 0, 0], verts: [0, 0, 0], faces: [0, 0, 0], render: false }] };
  const m = F.modelSpaceMeshes(fx, "t");
  assert.strictEqual(m.meshes.length, 1);  // render 0 left out
  nearAll(m.meshes[0].verts, [1, 1, 0]);
  assert.strictEqual(m.meshes[0].bitmap, "");
  assert.throws(() => F.modelSpaceMeshes({ planes: [] }, "none"), /no mesh/);
});
check("a pile is its junk lowered by the disk's height", () => {
  const out = F.lowered([{ verts: [1, 2, 1.0, 3, 4, 1.5] }], 0.9);
  nearAll(out[0].verts, [1, 2, 0.1, 3, 4, 0.6]);
});
check("mesh names are their own and none of the supermodel's", () => {
  const names = F.uniqueNames([{ name: "Rootdummy" }, { name: "a b.c" }, { name: "a_b_c" }, { name: "x".repeat(40) }, { name: "x".repeat(40) }], ["rootdummy"], 24)
    .map((m) => m.name);
  assert.deepStrictEqual(names.slice(0, 3), ["Rootdummy_2", "a_b_c", "a_b_c_2"]);
  assert.ok(names.every((n) => n.length <= 24));
  assert.notStrictEqual(names[3].toLowerCase(), names[4].toLowerCase());
});

// --- the disk -------------------------------------------------------------------------------------------------------
check("the disk: 40 sides, its top at its height, see-through", () => {
  const d = F.diskMesh(DISK);
  assert.strictEqual(d.verts.length / 3, 4 * 40 + 2);
  assert.strictEqual(d.faces.length / 3, 4 * 40);
  near(Math.max(...d.verts.filter((_, i) => i % 3 === 2)), 0.9);
  near(Math.min(...d.verts.filter((_, i) => i % 3 === 2)), 0.875);
  near(Math.max(...d.verts.filter((_, i) => i % 3 === 0)), 0.45);
  assert.strictEqual(d.transparent, true);
  assert.strictEqual(d.alpha, 0.6);
});
check("the disk's texture: a plain 128 x 128 32-bit TGA, rows bottom-up", () => {
  const t = F.diskTexture(DISK);
  assert.strictEqual(t[0], 0);  // no image ID: as nwn5e's other TGAs
  assert.strictEqual(t[2], 2);
  assert.strictEqual(t.readUInt16LE(12), 128);
  assert.strictEqual(t[16], 32);
  assert.strictEqual(t[17], 0x08);
  assert.strictEqual(t.length, 18 + 128 * 128 * 4);
  const library = F.diskTexture(DISK, F.TEXTURE_MARK);
  assert.strictEqual(library.toString("latin1", 18, 18 + library[0]), F.TEXTURE_MARK);
  assert.ok(library.slice(18 + library[0]).equals(t.slice(18)));
});

// --- a compiled model's nodes ---------------------------------------------------------------------------------------
check("a compiled model's node tree: names, parents, places, turns", () => {
  const b = Buffer.alloc(12 + 0x400);
  const at = (rel) => 12 + rel;
  b.writeUInt32LE(0, 0);
  b.writeUInt32LE(0x400, 4);
  b.writeUInt32LE(0x100, at(0x48));  // the geometry's root node
  const node = (rel, name, children, keys) => {
    const n = at(rel) + 0x10;
    b.write(name, n + 0x10, "latin1");
    b.writeUInt32LE(children ? 0x280 : 0, n + 0x38);
    b.writeUInt32LE(children ? children.length : 0, n + 0x3c);
    if (keys) { b.writeUInt32LE(0x300, n + 0x44); b.writeUInt32LE(2, n + 0x48); b.writeUInt32LE(0x340, n + 0x50); }
    b.writeUInt32LE(1, n + 0x5c);
  };
  node(0x100, "c_test", [0x180, 0x200]);
  node(0x180, "rootdummy", null, true);
  node(0x200, "Impact", null, false);
  b.writeUInt32LE(0x180, at(0x280));
  b.writeUInt32LE(0x200, at(0x284));
  const key = (i, type, data, cols) => { const k = at(0x300) + 12 * i; b.writeInt32LE(type, k); b.writeInt16LE(1, k + 4); b.writeInt16LE(0, k + 6); b.writeInt16LE(data, k + 8); b[k + 10] = cols; };
  key(0, 8, 1, 3);
  key(1, 20, 4, 4);
  [0, 0.1, 0.2, 1.2, 0, 0, Math.SQRT1_2, Math.SQRT1_2].forEach((v, i) => b.writeFloatLE(v, at(0x340) + 4 * i));
  const nodes = F.modelNodes(b);
  assert.deepStrictEqual(nodes.map((n) => [n.name, n.parent]), [["c_test", ""], ["rootdummy", "c_test"], ["Impact", "c_test"]]);
  nearAll(nodes[1].position, [0.1, 0.2, 1.2]);
  nearAll(nodes[1].rotation, [0, 0, Math.SQRT1_2, Math.SQRT1_2]);
  nearAll(nodes[2].rotation, [0, 0, 0, 1]);
  assert.throws(() => F.modelNodes(Buffer.from("#MAXMODEL ASCII\n".padEnd(600))), /not a compiled model/);
});

// --- the game files' text ------------------------------------------------------------------------------------------
const NODES = [{ name: "c_invsguy", parent: "", position: [0, 0, 0], rotation: [0, 0, 0, 1] },
  { name: "rootdummy", parent: "c_invsguy", position: [0, 0.02, 1.2], rotation: [0, 0, 0, 1] },
  { name: "lforearm", parent: "rootdummy", position: [0, 0, -0.18], rotation: [0, Math.sin(0.0349), 0, Math.cos(0.0349)] },
  { name: "headconjure", parent: "c_invsguy", position: [0, -0.15, 2.4], rotation: [0, 0, 0, 1] }];
const lines = (text) => text.split("\n");
const block = (text, head) => { const l = lines(text), i = l.indexOf(head); assert.ok(i >= 0, "no " + head); return l.slice(i, l.indexOf("endnode", i) + 1); };
check("a creature model: c_invsguy's supermodel and nodes, the meshes under the root", () => {
  const mesh = { name: "j01_Box", bitmap: "plc_x", verts: [0, 0, 1, 1, 0, 1, 0, 1, 1], tverts: [0, 0, 1, 0, 0, 1], faces: [0, 1, 2], diffuse: [1, 1, 1],
    ambient: [1, 1, 1], selfillum: [0, 0, 0], alpha: 1 };
  const text = F.creatureModelText("c_n5fd_small", NODES, [mesh, F.diskMesh(DISK)], ["about"]);
  const l = lines(text);
  assert.strictEqual(l[0], "#MAXMODEL ASCII");
  assert.strictEqual(l[1], F.MARK);
  assert.ok(l.includes("setsupermodel c_n5fd_small c_invsguy"));
  assert.ok(l.includes("classification character"));
  assert.deepStrictEqual(block(text, "node dummy rootdummy").slice(1, 3), ["  parent c_n5fd_small", "  position 0 0.02 1.2"]);
  assert.strictEqual(block(text, "node dummy lforearm")[1], "  parent rootdummy");
  assert.ok(/^  orientation 0 1 0 0\.069/.test(block(text, "node dummy lforearm")[3]));
  assert.strictEqual(block(text, "node dummy headconjure")[1], "  parent c_n5fd_small");
  assert.ok(!l.includes("node dummy c_invsguy"));
  const box = block(text, "node trimesh j01_Box");
  assert.ok(box.includes("  parent c_n5fd_small") && box.includes("  bitmap plc_x") && box.includes("  transparencyhint 0"));
  assert.ok(box.includes("    0 1 2 1 0 1 2 0"));
  const disk = block(text, "node trimesh forcedisk");
  assert.ok(disk.includes("  parent c_n5fd_small") && disk.includes("  alpha 0.6") && disk.includes("  transparencyhint 1"));
  assert.ok(l.indexOf("node trimesh forcedisk") > l.indexOf("node trimesh j01_Box"));  // the disk last
  assert.ok(l.includes("donemodel c_n5fd_small") && !l.some((x) => x.startsWith("newanim")));
});
check("a pile model: its use and effect nodes as the game's junk has them", () => {
  const mesh = { name: "j01_Box", bitmap: "plc_x", verts: [-0.2, -0.3, 0, 0.2, 0.3, 0, 0, 0, 0.5], tverts: [], faces: [0, 1, 2], diffuse: [1, 1, 1],
    ambient: [1, 1, 1], selfillum: [0, 0, 0], alpha: 1 };
  const text = F.pileModelText("plc_n5fd_med", [mesh], [-0.2, -0.3, 0], [0.2, 0.3, 0.5], []);
  assert.ok(lines(text).includes("setsupermodel plc_n5fd_med NULL"));
  for (const n of ["hand", "head", "head_hit", "impact", "ground"]) assert.strictEqual(block(text, "node dummy n5fd_med_" + n)[1], "  parent plc_n5fd_med");
  const hand = F.pileNodes("plc_n5fd_med", [-0.2, -0.3, 0], [0.2, 0.3, 0.5])[0];
  assert.ok(hand.position[1] < -0.3);
  const pwk = F.pileWalkmeshText("plc_n5fd_med", [-0.2, -0.3, 0], [0.2, 0.3, 0.5]);
  assert.strictEqual(lines(pwk)[0], "#MAXDOOR ASCII");
  assert.deepStrictEqual(block(pwk, "node dummy n5fd_med_pwk_use01").slice(1, 3), ["  parent plc_n5fd_med_pwk", "  position 0 -0.4 0"]);
  assert.ok(F.carriesMark(Buffer.from(pwk)) && F.carriesMark(Buffer.from(text)));
});
check("rows: the game's row copied whole, its label and model changed", () => {
  const table = { columns: ["LABEL", "NAME", "RACE", "PORTRAIT", "TARGETABLE"], rows: new Map([[298, { LABEL: "Invisible_Human_Male", NAME: "invsguy",
    RACE: "c_invsguy", PORTRAIT: "po hu", TARGETABLE: "0" }]]) };
  const text = F.rowsText(table, 298, [{ row: 14000, label: "N5_A", model: "c_a" }, { row: 14001, label: "N5_B", model: "c_b" }],
    { labelColumn: "LABEL", modelColumn: "RACE", overrides: { TARGETABLE: "1" }, about: ["why"] });
  const l = lines(text).filter((x) => x && !x.startsWith("#"));
  const tok = (s) => [...s.matchAll(/"([^"]*)"|(\S+)/g)].map((m) => (m[1] !== undefined ? m[1] : m[2]));
  assert.strictEqual(lines(text)[0], F.MARK);
  assert.deepStrictEqual(tok(l[0]), table.columns);
  assert.deepStrictEqual(tok(l[1]), ["14000", "N5_A", "invsguy", "c_a", "po hu", "1"]);
  assert.deepStrictEqual(tok(l[2]), ["14001", "N5_B", "invsguy", "c_b", "po hu", "1"]);
  assert.throws(() => F.rowsText(table, 1, [], { labelColumn: "LABEL", modelColumn: "RACE" }), /no row 1/);
  assert.throws(() => F.rowsText(table, 298, [], { labelColumn: "Label", modelColumn: "RACE" }), /no column Label/);
});

// --- someone's ASCII model -------------------------------------------------------------------------------------------
const NEVERBLENDER = [
  "# Exported from blender", "filedependancy pile.blend", "newmodel my_pile", "setsupermodel my_pile null", "classification placeable",
  "beginmodelgeom my_pile", "node dummy my_pile", "  parent NULL", "endnode",
  "node trimesh Sack", "  parent my_pile", "  position 0 0 0.1", "  orientation 0 0 1 1.5708", "  scale 1.0", "  bitmap my_sack", "  diffuse 0.8 0.8 0.8",
  "  verts 3", "    0 0 0", "    1 0 0", "    0 1 0",
  "  tverts 4", "    0 0 0", "    1 0 0", "    0 1 0", "    0.5 0.5 0",
  "  faces 2", "    0 1 2 1 0 1 2 1", "    0 2 1 1 3 2 1 1",
  "endnode",
  "node danglymesh Strap", "  parent Sack", "  bitmap NULL", "  verts 3", "    0 0 1", "    1 0 1", "    0 1 1", "  faces 1", "    0 1 2 1 0 0 0 1",
  "  constraints 3", "    0", "    0", "    0", "  period 1", "endnode",
  "node aabb wok", "  parent my_pile", "  verts 3", "    0 0 0", "    1 0 0", "    0 1 0", "  faces 1", "    0 1 2 1 0 0 0 1",
  "  aabb -1 -1 0 1 1 0 0", "endnode",
  "endmodelgeom my_pile",
  "newanim default my_pile", "  length 1", "node trimesh Sack", "  parent my_pile", "endnode", "doneanim default my_pile",
  "donemodel my_pile",
].join("\r\n");
check("an ASCII model: one vertex per (vertex, UV) pair, every mesh kind a trimesh", () => {
  const text = F.readableAscii(NEVERBLENDER, "parts/my_pile.mdl");
  const sack = block(text, "node trimesh Sack");
  assert.ok(sack.includes("  orientation 0 0 1 1.5708") && sack.includes("  position 0 0 0.1") && sack.includes("  bitmap my_sack"));
  assert.ok(sack.includes("  verts 4"));  // vertex 0 with UV 0 and with UV 3
  assert.ok(sack.includes("    0 1 2 1 0 1 2 0") && sack.includes("    3 2 1 1 3 2 1 0"));
  assert.ok(sack.includes("    0.5 0.5 0"));
  const strap = block(text, "node trimesh Strap");
  assert.ok(strap.includes("  parent Sack") && strap.includes("  bitmap NULL") && strap.includes("  verts 3"));
  assert.ok(block(text, "node dummy wok").includes("  parent my_pile"));  // the walkmesh not drawn
  assert.ok(!/newanim/.test(text));
});
check("an ASCII model: a scaled node, no mesh or no model refused", () => {
  assert.throws(() => F.readableAscii(NEVERBLENDER.replace("scale 1.0", "scale 0.5"), "x"), /scaled 0\.5/);
  assert.throws(() => F.readableAscii("newmodel a\nbeginmodelgeom a\nnode dummy a\n  parent NULL\nendnode\nendmodelgeom a\n", "x"), /no mesh/);
  assert.throws(() => F.readableAscii("hello", "x"), /not an ASCII model/);
});
check("textures beside a file: by name, any case, TGA first", () => {
  assert.deepStrictEqual(F.texturesBeside("my_sack", ["My_Sack.DDS", "my_sack.tga", "my_sack.txi", "pile.mdl"]), { image: "my_sack.tga", txi: "my_sack.txi" });
  assert.deepStrictEqual(F.texturesBeside("my_sack", ["MY_SACK.dds"]), { image: "MY_SACK.dds", txi: null });
  assert.deepStrictEqual(F.texturesBeside("x", ["y.tga"]), { image: null, txi: null });
});

// --- whose a file is --------------------------------------------------------------------------------------------------
check("ours: the mark on the first line, or the second after the format's", () => {
  assert.ok(F.carriesMark(Buffer.from(F.MARK + "\r\nLABEL\r\n")));
  assert.ok(F.carriesMark(Buffer.from("#MAXMODEL ASCII\r\n" + F.MARK + "\r\nnewmodel a")));
  assert.ok(!F.carriesMark(Buffer.from("#MAXMODEL ASCII\nnewmodel a\n" + F.MARK)));
  assert.ok(!F.carriesMark(Buffer.from("# someone's rows\nLABEL\n")));
  assert.ok(F.sameText(Buffer.from("a\r\nb\r\n"), Buffer.from("a\nb\n")));
  assert.strictEqual(F.crlf("a\nb\r\nc"), "a\r\nb\r\nc");
});
check("the shipped disk rows: clickable, and silent underfoot like the game's floating creatures", () => {
  // TARGETABLE 1: the disk's inventory opens on a click (row 298's 0 would hide it). FOOTSTEPTYPE -1: the disk floats,
  // so its walk and run make no footsteps (the Lantern Archon 103, Will-o-Wisp, Shadow and Allip are -1; 298 is 0).
  const fs = require("fs");
  const file = path.join(__dirname, "..", "..", "..", "data", "rows", "appearance.floatdisk.txt");
  const tok = (s) => [...s.matchAll(/"([^"]*)"|(\S+)/g)].map((m) => (m[1] !== undefined ? m[1] : m[2]));
  const l = fs.readFileSync(file, "utf8").split(/\r?\n/).filter((x) => x.trim() && !x.startsWith("#"));
  const head = tok(l[0]);
  const rows = l.slice(1).map((x) => tok(x));
  assert.strictEqual(rows.length, F.STAGES.length);
  for (const r of rows) {
    assert.strictEqual(r[head.indexOf("TARGETABLE") + 1], "1", `row ${r[0]} TARGETABLE`);
    assert.strictEqual(r[head.indexOf("FOOTSTEPTYPE") + 1], "-1", `row ${r[0]} FOOTSTEPTYPE`);
  }
  assert.deepStrictEqual(F.APPEARANCE_OVERRIDES, { TARGETABLE: "1", FOOTSTEPTYPE: "-1" });
});
check("the stages' names fit a resref", () => {
  for (const s of F.STAGES) assert.ok(F.RESREF.test(F.creatureModel(s)));
  for (const s of F.PILES) assert.ok(F.RESREF.test(F.pileModel(s)) && F.ROWS.placeables[s] === F.ROWS.appearance[s]);
});

// --- the Composer's page and the weapon's pieces ---------------------------------------------------------------------
check("floatdisk.js loads in a page (no require, no module, no Buffer)", () => {
  const src = require("fs").readFileSync(path.join(__dirname, "floatdisk.js"), "utf8");
  const self = {};
  new Function("self", "require", "module", "Buffer", src)(self);
  assert.strictEqual(typeof self.FloatDisk.placePart, "function");
});
check("the disk's texture: the same bytes in a page", () => {
  const src = require("fs").readFileSync(path.join(__dirname, "floatdisk.js"), "utf8");
  const self = {};
  new Function("self", "require", "module", "Buffer", src)(self);
  const page = self.FloatDisk.diskTexture(DISK);
  assert.ok(page instanceof Uint8Array && !Buffer.isBuffer(page));
  assert.ok(Buffer.from(page).equals(F.diskTexture(DISK)));
  const library = self.FloatDisk.diskTexture(DISK, F.TEXTURE_MARK);
  assert.ok(Buffer.from(library).equals(F.diskTexture(DISK, F.TEXTURE_MARK)));
});
check("a part's model may be a list (a weapon's pieces)", () => {
  const c = F.readComposition(comp([{ model: ["WSWLS_B_011", "wswls_m_011", "wswls_t_011"], at: [0, 0, 0] }, { model: ["plc_b04"], at: [0, 0, 0] }]), BASE);
  assert.deepStrictEqual(c.stages.small[0].source, { kind: "models", models: ["wswls_b_011", "wswls_m_011", "wswls_t_011"] });
  assert.strictEqual(F.partKey(c.stages.small[0]), "models:wswls_b_011+wswls_m_011+wswls_t_011");
  assert.strictEqual(F.partLabel(c.stages.small[0]), "wswls_b_011 + wswls_m_011 + wswls_t_011");
  assert.deepStrictEqual(c.stages.small[1].source, { kind: "model", model: "plc_b04" });
  assert.throws(() => F.readComposition(comp([{ model: [], at: [0, 0, 0] }]), BASE), /a list of them/);
  assert.throws(() => F.readComposition(comp([{ model: ["a", "b", "c", "d", "e"], at: [0, 0, 0] }]), BASE), /a list of them/);
});
check("mergeModels: a weapon's pieces as one model", () => {
  const a = cube(), b = cube();
  b.meshes[0].verts = b.meshes[0].verts.map((v, i) => (i % 3 === 2 ? v + 1 : v));
  const merged = F.mergeModels([{ meshes: a.meshes, lo: [1.5, -0.5, 0], hi: [2.5, 0.5, 1] }, { meshes: b.meshes, lo: [1.5, -0.5, 1], hi: [2.5, 0.5, 2] }]);
  assert.strictEqual(merged.meshes.length, 2);
  near(merged.lo[2], 0);
  near(merged.hi[2], 2);
  const placed = F.placePart({ at: [0, 0, 0], turn: 0, scale: 0.5, tilt: [0, 0] }, merged, 0, 0.9, "j");
  near(placed.meshes[1].verts[2] - placed.meshes[0].verts[2], 0.5);
});
const COMPOSITION_TEXT = require("fs").readFileSync(path.join(__dirname, "composition.json"), "utf8").replace(/\r\n/g, "\n");
check("compositionText gives back composition.json (0.0 and 1.0 written 0 and 1)", () => {
  assert.strictEqual(F.compositionText(JSON.parse(COMPOSITION_TEXT)),
    COMPOSITION_TEXT.replace("[0.8, 0.92, 1.0]", "[0.8, 0.92, 1]").replace("[0.0, 0.32, 0]", "[0, 0.32, 0]"));
});
check("compositionText: a part's keys in order, tilt only when given, an own file", () => {
  const out = F.compositionText({ _about: ["a"], disk: { diameter: 0.9 }, stages: {
    small: [{ scale: 1, at: [0, 0, 0], model: "plc_b04", note: "n", extra: 1 }, { file: "parts/x.mdl", at: [0, 0, 0], tilt: [0, 90], turn: 5 }], medium: [], large: [] } });
  const l = out.split("\n");
  assert.ok(l.includes('      { "note": "n", "model": "plc_b04", "at": [0, 0, 0], "scale": 1, "extra": 1 },'));
  assert.ok(l.includes('      { "file": "parts/x.mdl", "at": [0, 0, 0], "turn": 5, "tilt": [0, 90] }'));
  assert.ok(out.endsWith("}\n") && !out.includes("\r"));
});

// --- the Composer's arguments (tools\data\composer\build.js) -----------------------------------------------------------
check("the new arguments' defaults give today's text", () => {
  // A guard: the mark, the use points and the blocking outline the Composer passes are optional; left out or spelled
  // out as their defaults, the disk's piles, walkmeshes, rows and ownership test are what they were.
  const mesh = { name: "j01_Box", bitmap: "plc_x", verts: [-0.2, -0.3, 0, 0.2, 0.3, 0, 0, 0, 0.5], tverts: [0, 0, 1, 0, 0, 1], faces: [0, 1, 2],
    diffuse: [1, 1, 1], ambient: [1, 1, 1], selfillum: [0, 0, 0], alpha: 1 };
  const lo = [-0.2, -0.3, 0], hi = [0.2, 0.3, 0.5];
  const model = F.pileModelText("plc_n5fd_med", [mesh], lo, hi, ["about"]);
  assert.strictEqual(F.pileModelText("plc_n5fd_med", [mesh], lo, hi, ["about"], F.MARK), model);
  const pwk = F.pileWalkmeshText("plc_n5fd_med", lo, hi);
  assert.strictEqual(pwk, ["#MAXDOOR ASCII", F.MARK, "# model: plc_n5fd_med_pwk",
    "node dummy n5fd_med_pwk_use01", "  parent plc_n5fd_med_pwk", "  position 0 -0.4 0", "  orientation 0 0 0 0", "endnode",
    "node dummy n5fd_med_pwk_use02", "  parent plc_n5fd_med_pwk", "  position 0 0.4 0", "  orientation 0 0 0 0", "endnode", ""].join("\n"));
  assert.strictEqual(F.pileWalkmeshText("plc_n5fd_med", lo, hi, {}), pwk);
  assert.strictEqual(F.pileWalkmeshText("plc_n5fd_med", lo, hi, { mark: F.MARK, use: null, block: null }), pwk);
  const table = { columns: ["Label", "ModelName", "Static"], rows: new Map([[189, { Label: "TreasureSml", ModelName: "PLC_C07", Static: "1" }]]) };
  const entries = [{ row: 14001, label: "N5_A", model: "plc_a" }];
  const rows = F.rowsText(table, 189, entries, { labelColumn: "Label", modelColumn: "ModelName", about: ["why"] });
  assert.strictEqual(F.rowsText(table, 189, entries, { labelColumn: "Label", modelColumn: "ModelName", about: ["why"], mark: F.MARK }), rows);
  for (const bytes of [Buffer.from(model), Buffer.from(pwk), Buffer.from(rows), Buffer.from("# someone's rows\n")]) {
    assert.strictEqual(F.carriesMark(bytes, F.MARK), F.carriesMark(bytes));
  }
  assert.ok(F.carriesMark(Buffer.from(rows)) && !F.carriesMark(Buffer.from("# someone's rows\n")));
});

// --- files.js's writing (build_derived.js --check: dry) -----------------------------------------------------------------
check("writeAll dry: the states and the ownership rule as written, nothing written", () => {
  const fs = require("fs");
  const os = require("os");
  const W = require("./files.js");
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "n5_fdfiles-"));
  try {
    const model = (body) => Buffer.from("#MAXMODEL ASCII\r\n" + F.MARK + "\r\n" + body + "\r\n", "latin1");
    const mine = path.join(dir, "a.mdl"), same = path.join(dir, "b.mdl"), fresh = path.join(dir, "sub", "c.tga");
    fs.writeFileSync(mine, model("old"));
    fs.writeFileSync(same, model("kept"));
    const writes = [{ file: mine, bytes: model("new"), kind: "text" }, { file: same, bytes: model("kept"), kind: "text" },
      { file: fresh, bytes: Buffer.from([1, 2, 3]), kind: "binary" }];
    const owned = W.ownership({ repo: dir, manifest: new Set(), mark: F.MARK });
    const manifest = new Set();
    const states = [["rebuilt", mine], ["unchanged", same], ["new", fresh]];
    assert.deepStrictEqual(W.writeAll(writes, owned, { repo: dir, manifest, dry: true }), states);
    assert.ok(fs.readFileSync(mine).equals(model("old")), "dry rewrote a.mdl");
    assert.ok(!fs.existsSync(path.dirname(fresh)), "dry made a folder");
    assert.strictEqual(manifest.size, 0, "dry noted a texture in made.txt's list");
    // A file in the way it didn't make: refused, dry or not, nothing written.
    const theirs = path.join(dir, "d.mdl");
    fs.writeFileSync(theirs, "#MAXMODEL ASCII\r\n# someone's model\r\n");
    for (const dry of [true, false]) {
      assert.throws(() => W.writeAll([...writes, { file: theirs, bytes: model("d"), kind: "text" }], owned, { repo: dir, manifest, dry }),
        /^Error: d\.mdl is there and wasn't made by this generator: nothing written/);
    }
    assert.ok(fs.readFileSync(mine).equals(model("old")) && !fs.existsSync(fresh));
    // Without it, the same writes are written, as they were before dry.
    assert.deepStrictEqual(W.writeAll(writes, owned, { repo: dir, manifest }), states);
    assert.ok(fs.readFileSync(mine).equals(model("new")) && fs.readFileSync(fresh).equals(Buffer.from([1, 2, 3])));
    assert.deepStrictEqual([...manifest], ["sub/c.tga"]);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

console.log(process.exitCode ? `floatdisk check: FAILED (${count} passed)` : `floatdisk check: ${count} checks pass`);
