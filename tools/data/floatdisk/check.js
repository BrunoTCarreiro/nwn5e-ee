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

// --- the PLT bake (dyed parts) --------------------------------------------------------------------------------------------
function pltOf(w, h, pixels) {
  const b = Buffer.alloc(24 + 2 * w * h);
  b.write("PLT V1  ", 0, "latin1"); b.writeUInt32LE(10, 8); b.writeUInt32LE(0, 12); b.writeUInt32LE(w, 16); b.writeUInt32LE(h, 20);
  pixels.forEach(([shade, layer], i) => { b[24 + 2 * i] = shade; b[25 + 2 * i] = layer; });
  return b;
}
const PLT2 = pltOf(2, 2, [[10, 2], [20, 3], [30, 4], [40, 6]]);
function palette(id) {
  const data = new Uint8ClampedArray(256 * 176 * 4);
  for (let r = 0; r < 176; r++) for (let c = 0; c < 256; c++) data.set([r, c, id, 255 - id], (r * 256 + c) * 4);
  return { width: 256, height: 176, data };
}
const PALS = { pal_armor01: palette(1), pal_armor02: palette(2), pal_cloth01: palette(3), pal_leath01: palette(4), pal_skin01: palette(5),
  pal_hair01: palette(6), pal_tattoo01: palette(7) };
const DYE = () => F.readColours({ metal1: 7, metal2: 8, cloth1: 9, leather1: 11 });

check("a part's colours: the six fields as palette numbers, the defaults filled", () => {
  const read = (colours) => F.readComposition(comp([{ model: "helm_001", at: [0, 0, 0], colours }]), BASE).stages.small[0].colours;
  assert.deepStrictEqual(F.readComposition(comp([{ model: "helm_001", at: [0, 0, 0] }]), BASE).stages.small[0].colours, F.PLT_DEFAULTS);
  assert.deepStrictEqual(read({ metal1: 5, cloth2: 40 }), Object.assign({}, F.PLT_DEFAULTS, { metal1: 5, cloth2: 40 }));
  for (const bad of [{ metal1: 176 }, { metal1: 1.5 }, { skin: 3 }, []]) {
    assert.throws(() => read(bad), /stages\.small\[0\]\.colours: .*0-175/);
  }
});
check("cleanColours: only what differs from the defaults, in the card's order", () => {
  const c = F.cleanColours({ cloth2: 40, metal1: 0, metal2: 5 });
  assert.deepStrictEqual(c, { metal2: 5, cloth2: 40 });
  assert.deepStrictEqual(Object.keys(c), ["metal2", "cloth2"]);
  assert.strictEqual(F.cleanColours(F.PLT_DEFAULTS), undefined);
  assert.strictEqual(F.cleanColours(undefined), undefined);
});
check("readPlt: the header, the size, a shade and a layer a pixel, in the file's order", () => {
  for (const bytes of [PLT2, new Uint8Array(PLT2)]) {
    const p = F.readPlt(bytes);
    assert.deepStrictEqual([p.width, p.height, [...p.shade], [...p.layer]], [2, 2, [10, 20, 30, 40], [2, 3, 4, 6]]);
  }
  const v2 = Buffer.from(PLT2); v2.write("PLT V2  ", 0, "latin1");
  assert.throws(() => F.readPlt(v2), /not a PLT/);
  assert.throws(() => F.readPlt(PLT2.subarray(0, PLT2.length - 1)), /cut short: 2 x 2 needs 32 bytes, it has 31/);
});
check("bakePlt: each pixel its layer's palette, the row its colour, the column its shade; the first row at the bottom", () => {
  const img = F.bakePlt(F.readPlt(PLT2), PALS, DYE());
  assert.deepStrictEqual([img.width, img.height, [...img.data]], [2, 2, [9, 30, 3, 255, 11, 40, 4, 255, 7, 10, 1, 255, 8, 20, 2, 255]]);
  assert.ok(img.data instanceof Uint8ClampedArray);
  assert.deepStrictEqual([...F.bakePlt(F.readPlt(pltOf(1, 1, [[5, 12]])), PALS, DYE()).data], [0, 5, 5, 255]);
  assert.deepStrictEqual([...F.bakePlt(F.readPlt(pltOf(1, 1, [[5, 2]])), PALS, Object.assign({}, DYE(), { metal1: 200 })).data], [175, 5, 1, 255]);
  const { pal_armor02, ...fewer } = PALS;
  assert.throws(() => F.bakePlt(F.readPlt(PLT2), fewer, DYE()), /pal_armor02: a palette the bake needs/);
});
check("bakedName: n5p_ and 12 hex digits, one per PLT and colours", () => {
  assert.strictEqual(F.bakedName("helm_001", F.PLT_DEFAULTS), "n5p_2cd3d725dedb");
  assert.strictEqual(F.bakedName("HELM_001", {}), "n5p_2cd3d725dedb");
  assert.strictEqual(F.bakedName("helm_001", { metal1: 5 }), "n5p_f15efdade06c");
  assert.notStrictEqual(F.bakedName("helm_002", {}), F.bakedName("helm_001", {}));
  for (const n of [F.bakedName("helm_001", {}), F.bakedName("helm_002", { cloth1: 9 })]) assert.ok(F.BAKED.test(n), n);
});
check("tgaBytes: a 24-bit TGA, rows bottom-up as the game's", () => {
  const t = F.tgaBytes(F.bakePlt(F.readPlt(PLT2), PALS, DYE()));
  assert.ok(Buffer.isBuffer(t));
  assert.strictEqual(t.length, 30);
  assert.deepStrictEqual([...t.subarray(0, 18)], [0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 24, 0]);
  assert.deepStrictEqual([...t.subarray(18)], [1, 10, 7, 2, 20, 8, 3, 30, 9, 4, 40, 11]);
});
check("bakeBitmaps: a PLT-only texture named for its colours, the others kept, each bake once", () => {
  const meshes = ["helm_001", "plc_x", "HELM_001", ""].map((bitmap) => ({ name: "m", bitmap }));
  const out = F.bakeBitmaps(meshes, F.readColours({ metal1: 5 }), (b) => b === "helm_001");
  assert.deepStrictEqual(out.meshes.map((m) => m.bitmap), ["n5p_f15efdade06c", "plc_x", "n5p_f15efdade06c", ""]);
  assert.deepStrictEqual(out.bakes, [{ name: "n5p_f15efdade06c", plt: "helm_001", colours: { metal1: 5 } }]);
  assert.deepStrictEqual(meshes.map((m) => m.bitmap), ["helm_001", "plc_x", "HELM_001", ""]);
});
check("compositionText: a part's colours inline, after its known keys", () => {
  const text = F.compositionText({ stages: { small: [{ colours: { metal1: 5 }, model: "helm_001", at: [0, 0, 0], tilt: [0, 90] }] } });
  assert.ok(text.split("\n").includes('      { "model": "helm_001", "at": [0, 0, 0], "tilt": [0, 90], "colours": { "metal1": 5 } }'), text);
});
check("the bake in a page: the same name and bytes", () => {
  const src = require("fs").readFileSync(path.join(__dirname, "floatdisk.js"), "utf8");
  const self = {};
  new Function("self", "require", "module", "Buffer", src)(self);
  const P = self.FloatDisk;
  const t = P.tgaBytes(P.bakePlt(P.readPlt(new Uint8Array(PLT2)), PALS, DYE()));
  assert.ok(t instanceof Uint8Array && !Buffer.isBuffer(t));
  assert.ok(Buffer.from(t).equals(F.tgaBytes(F.bakePlt(F.readPlt(PLT2), PALS, DYE()))));
  assert.strictEqual(P.bakedName("helm_001", {}), F.bakedName("helm_001", {}));
});
check("bakePlt and bakedName: a colour left out or undefined is its default - one rule, so a name matches its pixels", () => {
  const plt = F.readPlt(PLT2), full = (c) => [...F.bakePlt(plt, PALS, F.readColours(c)).data];
  // Left out: the default's row, not row 0 (or black) - as the name has it.
  assert.deepStrictEqual([...F.bakePlt(plt, PALS, { metal1: 7 }).data], full({ metal1: 7 }));
  assert.deepStrictEqual([...F.bakePlt(plt, PALS, {}).data], full(undefined));
  assert.deepStrictEqual([...F.bakePlt(plt, PALS, undefined).data], full(undefined));
  // Undefined: the default too, in both.
  const gaps = Object.assign(F.readColours({ metal1: 7 }), { metal2: undefined, cloth1: undefined });
  assert.deepStrictEqual([...F.bakePlt(plt, PALS, gaps).data], full({ metal1: 7 }));
  assert.strictEqual(F.bakedName("helm_001", { metal1: undefined }), "n5p_2cd3d725dedb");
  assert.strictEqual(F.bakedName("helm_001", gaps), F.bakedName("helm_001", { metal1: 7 }));
});
check("one JSON writer (inline): {} when empty, an undefined key left out - compositionText's colours read back", () => {
  assert.strictEqual(F.inline({}), "{}");
  assert.strictEqual(F.inline({ metal1: 5, cloth1: undefined }), '{ "metal1": 5 }');
  assert.strictEqual(F.inline([1, { a: [0, 0.5] }, "x", null]), '[1, { "a": [0, 0.5] }, "x", null]');
  const text = F.compositionText({ stages: { small: [
    { model: "helm_001", at: [0, 0, 0], colours: {} },
    { model: "helm_002", at: [0, 0, 0], colours: { metal1: undefined, metal2: 5 } }] } });
  const l = text.split("\n");
  assert.ok(l.includes('      { "model": "helm_001", "at": [0, 0, 0], "colours": {} },'), text);
  assert.ok(l.includes('      { "model": "helm_002", "at": [0, 0, 0], "colours": { "metal2": 5 } }'), text);
  assert.deepStrictEqual(JSON.parse(text).stages.small.map((p) => p.colours), [{}, { metal2: 5 }]);
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

// --- the Composer's shapes: the dome ------------------------------------------------------------------------------------
const vertOf = (m, i) => [m.verts[3 * i], m.verts[3 * i + 1], m.verts[3 * i + 2]];
const uvOf = (m, i) => [m.tverts[2 * i], m.tverts[2 * i + 1]];
// Each triangle's (b - a) x (c - a) dotted with its centroid less `centre` (a dome's centre is the origin, a sphere's its
// middle): its sign says which way it faces, out (+) or in (-). `centre` "z": the normal's z (a flat shape's: up + or
// down -).
function normals(m, centre = [0, 0, 0]) {
  const out = [];
  for (let f = 0; f < m.faces.length; f += 3) {
    const [a, b, c] = [0, 1, 2].map((k) => vertOf(m, m.faces[f + k]));
    const u = [b[0] - a[0], b[1] - a[1], b[2] - a[2]], v = [c[0] - a[0], c[1] - a[1], c[2] - a[2]];
    const n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]];
    out.push(centre === "z" ? n[2] : [0, 1, 2].reduce((s, k) => s + n[k] * ((a[k] + b[k] + c[k]) / 3 - centre[k]), 0));
  }
  return out;
}
// Every vertex and UV rounded as diskMesh's (0.00001), and no -0: the mesh through JSON is the same mesh.
const rounded = (m) => assert.ok(m.verts.every((x) => x === F.round(x)) && m.tverts.every((t) => t === F.round(t)), "a vertex or UV not rounded");
const noNegativeZero = (model) => {
  assert.ok(!model.meshes.some((m) => m.verts.concat(m.tverts).some((x) => Object.is(x, -0))), "a -0 in the mesh");
  assert.deepStrictEqual(JSON.parse(JSON.stringify(model)), model);
};
const dome = (o) => Object.assign({ type: "dome", radius: 2 }, o);
const domeModel = (o) => F.shapeModel(F.readShape(dome(o), "s"));
const pageFloatDisk = () => {
  const self = {};
  new Function("self", "require", "module", "Buffer", require("fs").readFileSync(path.join(__dirname, "floatdisk.js"), "utf8"))(self);
  return self.FloatDisk;
};

check("a shape: read with its defaults, written without them", () => {
  const s = F.readShape({ type: "dome", radius: 4.57 }, "s");
  assert.deepStrictEqual(s, { type: "dome", radius: 4.57, height: 4.57, segments: 32, sides: "both", mapping: "wrap", repeat_around: 1, repeat_up: 1,
    colour: [0.6, 0.8, 1], alpha: 0.35, glow: 1, texture: "soft", blend: "normal" });
  assert.deepStrictEqual(F.shapeDefaults("dome", 4.57), s);
  assert.deepStrictEqual(F.cleanShape(s), { type: "dome", radius: 4.57 });
  const c = F.cleanShape(Object.assign({}, s, { colour: [1, 0.85, 0.4], height: 3 }));
  assert.deepStrictEqual(c, { type: "dome", radius: 4.57, height: 3, colour: [1, 0.85, 0.4] });
  assert.deepStrictEqual(Object.keys(c), ["type", "radius", "height", "colour"]);
  // Its colour is its own: copied in, copied out.
  const given = [1, 0.85, 0.4], read = F.readShape({ type: "dome", radius: 1, colour: given }, "s");
  assert.ok(read.colour !== given && F.cleanShape(read).colour !== read.colour);
  assert.ok(F.shapeDefaults("dome", 1).colour !== F.shapeDefaults("dome", 1).colour);
  // Whole or not: a key left out is its default.
  assert.deepStrictEqual(F.cleanShape({ type: "dome", radius: 1, sides: "outside", glow: 1 }), { type: "dome", radius: 1, sides: "outside" });
  // Mapping: written only when not wrap; the repeats when not 1, and never under From above (they mean nothing there).
  assert.deepStrictEqual(F.cleanShape(Object.assign({}, s, { repeat_up: 2, repeat_around: 3 })), { type: "dome", radius: 4.57, repeat_around: 3, repeat_up: 2 });
  const above = F.cleanShape(Object.assign({}, s, { mapping: "above", repeat_around: 3, repeat_up: 2, glow: 0.5 }));
  assert.deepStrictEqual(above, { type: "dome", radius: 4.57, mapping: "above", glow: 0.5 });
  assert.deepStrictEqual(Object.keys(above), ["type", "radius", "mapping", "glow"]);
  assert.deepStrictEqual(F.cleanShape(F.readShape({ type: "dome", radius: 1, texture: "fxpa_symbl_001" }, "s")), { type: "dome", radius: 1, texture: "fxpa_symbl_001" });
});
check("a shape's errors name the field", () => {
  const bad = (s, expected) => assert.throws(() => F.readShape(s, "s"), expected);
  const one = (o) => Object.assign({ type: "dome", radius: 1 }, o);
  bad({ type: "cube", radius: 1 }, /^Error: s\.type: dome/);
  for (const radius of [0, 101]) bad(one({ radius }), /^Error: s\.radius: a number from 0\.05 to 100 \(metres\)$/);
  bad({ type: "dome" }, /^Error: s\.radius: a number from 0\.05 to 100 \(metres\)$/);
  bad(one({ height: 0.04 }), /^Error: s\.height: a number from 0\.05 to 100 \(metres\)$/);
  for (const segments of [7, 97, 12.5]) bad(one({ segments }), /^Error: s\.segments: a whole number from 8 to 96$/);
  bad(one({ sides: "inside" }), /^Error: s\.sides: both or outside$/);
  for (const colour of [[1, 1], [2, 0, 0], "blue"]) bad(one({ colour }), /^Error: s\.colour: \[r, g, b\], each 0 to 1$/);
  bad(one({ alpha: 0 }), /^Error: s\.alpha: a number from 0\.02 to 1$/);
  bad(one({ glow: 1.5 }), /^Error: s\.glow: a number from 0 to 1$/);
  bad(one({ mapping: "below" }), /^Error: s\.mapping: wrap or above$/);
  for (const repeat_around of [0, 33, 1.5]) bad(one({ repeat_around }), /^Error: s\.repeat_around: a whole number from 1 to 32$/);
  for (const repeat_up of [0, 17]) bad(one({ repeat_up }), /^Error: s\.repeat_up: a whole number from 1 to 16$/);
  // A texture: soft, none or a library texture's name - never a made one (anything n5s_), nor null (the game reads
  // "bitmap null" as no texture), nor a name the game can't have.
  for (const texture of ["Glyph", "fx-glyph", "x".repeat(17), "", "n5s_8c757fb28f99", "n5s_mine", "n5s_", "null", 3]) {
    bad(one({ texture }), (e) => e.message === "s.texture: soft, none or a texture's name (lower-case letters, digits and _, at most 16)");
  }
  bad(one({ size: 2 }), /^Error: s\.size: not a dome's/);
  for (const s of [[], null, "dome"]) bad(s, /^Error: s: a shape/);
  // What reads: a library texture's name, the bounds themselves.
  assert.strictEqual(F.readShape(one({ texture: "fxpa_symbl_001" }), "s").texture, "fxpa_symbl_001");
  const edge = F.readShape({ type: "dome", radius: 0.05, height: 100, segments: 96, sides: "outside", mapping: "above", repeat_around: 32, repeat_up: 16,
    colour: [0, 1, 0], alpha: 0.02, glow: 0, texture: "none" }, "s");
  assert.deepStrictEqual([edge.radius, edge.height, edge.segments, edge.alpha, edge.glow], [0.05, 100, 96, 0.02, 0]);
});
check("a part may be a shape: its source, key and label; never a scale or dyes", () => {
  const part = F.readPart({ shape: { type: "dome", radius: 4.57 }, at: [0, 0, 0] }, "stages.small[0]", BASE);
  assert.deepStrictEqual(part.source, { kind: "shape", shape: F.shapeDefaults("dome", 4.57) });
  assert.strictEqual(part.scale, 1);
  assert.strictEqual(part.turn, 0);
  assert.strictEqual(F.partKey(part), 'shape:{ "type": "dome", "radius": 4.57 }');
  assert.strictEqual(F.partLabel(part), "dome 4.57 m");
  assert.throws(() => F.readPart({ shape: { type: "dome", radius: 4.57 }, at: [0, 0, 0], scale: 2 }, "stages.small[0]", BASE),
    /^Error: stages\.small\[0\]\.scale: a shape has no scale - its radius and height size it$/);
  for (const colours of [{ metal1: 5 }, {}]) {
    assert.throws(() => F.readPart({ shape: { type: "dome", radius: 4.57 }, at: [0, 0, 0], colours }, "stages.small[0]", BASE),
      /^Error: stages\.small\[0\]\.colours: a shape has no dyes - its colour is on the shape$/);
  }
  assert.throws(() => F.readPart({ model: "plc_b04", shape: { type: "dome", radius: 4.57 }, at: [0, 0, 0] }, "stages.small[0]", BASE), /either "model"/);
  assert.throws(() => F.readPart({ at: [0, 0, 0] }, "p", BASE),
    (e) => e.message === 'p: either "model" (a game model), "file" (your own .mdl) or "shape" (a dome and the like) - just one');
  assert.throws(() => F.readPart({ shape: { type: "dome", radius: 0 }, at: [0, 0, 0] }, "stages.small[0]", BASE), /^Error: stages\.small\[0\]\.shape\.radius: /);
  // The key is the shape as written: what differs from the defaults changes it, the repeats under From above don't.
  const key = (shape) => F.partKey(F.readPart({ shape, at: [0, 0, 0] }, "p", BASE));
  assert.strictEqual(key({ type: "dome", radius: 4.57, height: 3 }), 'shape:{ "type": "dome", "radius": 4.57, "height": 3 }');
  assert.strictEqual(key({ type: "dome", radius: 1, mapping: "above", repeat_up: 4 }), key({ type: "dome", radius: 1, mapping: "above" }));
  assert.strictEqual(F.partLabel(F.readPart({ shape: { type: "dome", radius: 0.5 }, at: [0, 0, 0] }, "p", BASE)), "dome 0.5 m");
  // In a composition, as any part.
  assert.deepStrictEqual(F.readComposition(comp([{ shape: { type: "dome", radius: 0.3 }, at: [0, 0, 0] }]), BASE).stages.small[0].source.shape,
    F.shapeDefaults("dome", 0.3));
});
check("the dome: 32 segments, 8 rings, both sides - 592 vertices, 960 triangles, the inside first", () => {
  const model = domeModel();
  assert.strictEqual(model.meshes.length, 1);
  const m = model.meshes[0];
  assert.strictEqual(m.name, "dome");
  assert.strictEqual(m.verts.length / 3, 592);
  assert.strictEqual(m.tverts.length / 2, 592);
  assert.strictEqual(m.faces.length / 3, 960);
  assert.deepStrictEqual(model.lo, [-2, -2, 0]);
  assert.deepStrictEqual(model.hi, [2, 2, 2]);
  normals(m).forEach((d, t) => assert.ok(t < 480 ? d < 0 : d > 0, `triangle ${t} faces ${d < 0 ? "in" : "out"}`));
  m.faces.forEach((v, k) => assert.ok(k < 3 * 480 ? v >= 0 && v < 296 : v >= 296 && v < 592, `triangle ${Math.floor(k / 3)} uses vertex ${v}`));
  for (let i = 0; i < 592; i++) {
    const [x, y, z] = vertOf(m, i);
    near((x * x + y * y) / 4 + (z * z) / 4, 1);
    assert.ok(z >= 0, `vertex ${i} below the ground`);
  }
  // Each side its own vertices: the inside's are the outside's, in the same order.
  assert.deepStrictEqual(m.verts.slice(0, 3 * 296), m.verts.slice(3 * 296));
  assert.deepStrictEqual(m.tverts.slice(0, 2 * 296), m.tverts.slice(2 * 296));
  rounded(m);
  noNegativeZero(model);
});
check("the dome's UVs: u round it from +x, v from the ground ring (0) to the top (1); Repeat around and up tile it, From above lays it on from overhead", () => {
  const m = domeModel().meshes[0];
  const outside = Array.from({ length: 296 }, (_, i) => 296 + i);
  const at = (mesh, p) => outside.filter((i) => vertOf(mesh, i).every((c, k) => Math.abs(c - p[k]) < 1e-4));
  assert.deepStrictEqual(at(m, [2, 0, 0]).map((i) => uvOf(m, i)), [[0, 0], [1, 0]]);
  const tops = Array.from({ length: 592 }, (_, i) => i).filter((i) => vertOf(m, i)[2] === 2);
  assert.strictEqual(tops.length, 64);
  for (const i of tops) {
    const [u, v] = uvOf(m, i), k = Math.round(u * 32 - 0.5);  // u rounded to 0.00001, as every UV
    assert.strictEqual(v, 1);
    assert.ok(Math.abs(u - (k + 0.5) / 32) <= 1e-5 && k >= 0 && k < 32, `top u ${u}`);
  }
  assert.deepStrictEqual(at(m, [1.41421, 0, 1.41421]).map((i) => uvOf(m, i)[1]), [0.5, 0.5]);
  assert.ok(m.tverts.every((t) => t >= 0 && t <= 1));
  // Wrapped, Repeat around 3 and up 2: the first ring's last vertex (the seam's second) at u 3, the top at v 2.
  const w = domeModel({ repeat_around: 3, repeat_up: 2 }).meshes[0];
  assert.deepStrictEqual(uvOf(w, 296 + 32), [3, 0]);
  assert.deepStrictEqual(at(w, [2, 0, 0]).map((i) => uvOf(w, i)), [[0, 0], [3, 0]]);
  const wTops = outside.filter((i) => vertOf(w, i)[2] === 2);
  assert.strictEqual(wTops.length, 32);
  for (const i of wTops) {
    const [u, v] = uvOf(w, i), k = Math.round((u / 3) * 32 - 0.5);  // u: (k + 0.5) / 32, three times round
    assert.strictEqual(v, 2);
    assert.ok(Math.abs(u - ((k + 0.5) / 32) * 3) <= 1e-5 && k >= 0 && k < 32, `top u ${u}`);
  }
  assert.deepStrictEqual(at(w, [1.41421, 0, 1.41421]).map((i) => uvOf(w, i)[1]), [1, 1]);
  rounded(w);
  // From above: the picture's square spans its width - the rim at angle 0 is (1, 0.5), at 90 degrees (0.5, 1), the top
  // (0.5, 0.5); the repeats don't apply.
  for (const o of [{ mapping: "above" }, { mapping: "above", repeat_around: 3, repeat_up: 2 }]) {
    const above = domeModel(o), a = above.meshes[0];
    rounded(a);
    noNegativeZero(above);
    assert.deepStrictEqual(at(a, [2, 0, 0]).map((i) => uvOf(a, i)), [[1, 0.5], [1, 0.5]]);
    assert.deepStrictEqual(uvOf(a, at(a, [0, 2, 0])[0]), [0.5, 1]);
    assert.ok(outside.filter((i) => vertOf(a, i)[2] === 2).every((i) => uvOf(a, i)[0] === 0.5 && uvOf(a, i)[1] === 0.5));
    for (let i = 0; i < 592; i++) {
      const [x, y] = vertOf(a, i), [u, v] = uvOf(a, i);
      near(u, 0.5 + x / 4);
      near(v, 0.5 + y / 4);
    }
    assert.deepStrictEqual(a.tverts.slice(0, 2 * 296), a.tverts.slice(2 * 296));  // the inside keeps its vertex's UV
  }
});
check("a lower dome, outside only, 8 segments: 26 vertices, 24 triangles, all facing out", () => {
  const model = F.shapeModel(F.readShape({ type: "dome", radius: 1, height: 0.4, segments: 8, sides: "outside" }, "s"));
  const m = model.meshes[0];
  assert.strictEqual(m.verts.length / 3, 26);
  assert.strictEqual(m.faces.length / 3, 24);
  assert.strictEqual(model.hi[2], 0.4);
  normals(m).forEach((d, t) => assert.ok(d > 0, `triangle ${t} faces in`));
  near(Math.max(...m.verts.filter((_, i) => i % 3 === 2)), 0.4);
  rounded(m);
  noNegativeZero(model);
});
check("the dome's material: its texture's colour lit by its glow (soft), its own colour (none), or a library texture tinted by it", () => {
  const soft = F.shapeModel(F.readShape({ type: "dome", radius: 1 }, "s")), m = soft.meshes[0];
  assert.deepStrictEqual(Object.keys(m), ["name", "bitmap", "verts", "tverts", "faces", "groups", "diffuse", "ambient", "selfillum", "alpha", "transparent",
    "transparencyhint"]);
  assert.deepStrictEqual([m.bitmap, m.diffuse, m.ambient, m.selfillum, m.alpha, m.transparent],
    ["n5s_8c757fb28f99", [1, 1, 1], [1, 1, 1], [1, 1, 1], 0.35, true]);
  assert.deepStrictEqual(soft.textures, [{ name: "n5s_8c757fb28f99", colour: [0.6, 0.8, 1], blend: "normal" }]);
  const none = F.shapeModel(F.readShape({ type: "dome", radius: 1, texture: "none", colour: [1, 0.85, 0.4], glow: 0.5, alpha: 1 }, "s")), n = none.meshes[0];
  assert.deepStrictEqual([n.bitmap, n.diffuse, n.ambient, n.selfillum, n.alpha, n.transparent],
    ["", [1, 0.85, 0.4], [1, 0.85, 0.4], [0.5, 0.425, 0.2], 1, false]);
  assert.deepStrictEqual(none.textures, []);
  assert.strictEqual(F.shapeModel(F.readShape({ type: "dome", radius: 1, texture: "none", alpha: 0.5 }, "s")).meshes[0].transparent, true);
  // A library texture: named as it is, tinted by its colour - lit by itself as far as its glow goes, by the scene the rest
  // (M1, the owner's Q3: diffuse and ambient its colour x (1 - glow), selfillum its colour x glow) - see-through (its own
  // alpha isn't known); nothing generated.
  const lib = F.shapeModel(F.readShape({ type: "dome", radius: 1, texture: "fxpa_symbl_001", colour: [1, 0.85, 0.4], glow: 0.5, alpha: 1 }, "s"));
  const l = lib.meshes[0];
  assert.deepStrictEqual([l.bitmap, l.diffuse, l.ambient, l.selfillum, l.alpha, l.transparent],
    ["fxpa_symbl_001", [0.5, 0.425, 0.2], [0.5, 0.425, 0.2], [0.5, 0.425, 0.2], 1, true]);
  assert.deepStrictEqual(lib.textures, []);
  // Soft in another colour: its own texture.
  assert.deepStrictEqual(F.shapeModel(F.readShape({ type: "dome", radius: 1, colour: [1, 0.85, 0.4] }, "s")).textures,
    [{ name: "n5s_a3caa0709dda", colour: [1, 0.85, 0.4], blend: "normal" }]);
});
check("a dome's two sides in their own smoothing groups (the inside 2, the outside 1); a mesh without groups is written 1, as before", () => {
  // The sides share their positions: in one group, a loader that merges normals by position would cancel them.
  const both = domeModel().meshes[0];
  assert.strictEqual(both.groups.length, 960);
  both.groups.forEach((g, t) => assert.strictEqual(g, t < 480 ? 2 : 1, `triangle ${t}'s group`));
  assert.deepStrictEqual(domeModel({ sides: "outside" }).meshes[0].groups, new Array(480).fill(1));
  // The model's text: each face's group as the mesh has it.
  const small = domeModel({ segments: 8 }), mesh = small.meshes[0];
  const text = F.pileModelText("plc_n5test", [mesh], small.lo, small.hi, []);
  const node = block(text, "node trimesh dome"), at = node.indexOf("  faces 48");
  assert.ok(at > 0, "no faces 48");
  node.slice(at + 1, at + 49).forEach((line, f) => {
    const [a, b, c] = mesh.faces.slice(3 * f, 3 * f + 3);
    assert.strictEqual(line, `    ${a} ${b} ${c} ${f < 24 ? 2 : 1} ${a} ${b} ${c} 0`);
  });
  // Placing and lowering keep them (a shape goes through placePart as any part's model).
  const placed = F.placePart({ at: [0, 0, 0], turn: 30, scale: 1, tilt: [0, 0] }, small, 0, 0.9, "s");
  assert.deepStrictEqual(placed.meshes[0].groups, mesh.groups);
  assert.deepStrictEqual(F.lowered(placed.meshes, 0.9)[0].groups, mesh.groups);
  // Without groups: 1 on every face, the bytes as before.
  const box = { name: "j01_Box", bitmap: "plc_x", verts: [0, 0, 0, 1, 0, 0, 0, 1, 0], tverts: [0, 0, 1, 0, 0, 1], faces: [0, 1, 2, 0, 2, 1], diffuse: [1, 1, 1],
    ambient: [1, 1, 1], selfillum: [0, 0, 0], alpha: 1 };
  const plain = block(F.pileModelText("plc_n5test", [box], [0, 0, 0], [1, 1, 0], []), "node trimesh j01_Box");
  assert.ok(plain.includes("    0 1 2 1 0 1 2 0") && plain.includes("    0 2 1 1 0 2 1 0"));
});
check("shapeTexture: 128 x 128, brighter and more opaque at the ground, seamless round it; its name n5s_ and a hash of its colour", () => {
  const t = F.shapeTexture({ colour: [0.6, 0.8, 1] });
  assert.deepStrictEqual([t.width, t.height, t.data.length], [128, 128, 65536]);
  assert.ok(t.data instanceof Uint8ClampedArray);
  const px = (x, y) => [...t.data.subarray((y * 128 + x) * 4, (y * 128 + x) * 4 + 4)];
  const mean = (y, f) => Array.from({ length: 128 }, (_, x) => f(px(x, y))).reduce((a, b) => a + b, 0) / 128;
  assert.ok(mean(127, (p) => p[3]) - mean(0, (p) => p[3]) > 100, "the ground ring's alpha");
  assert.ok(mean(127, (p) => p[0] + p[1] + p[2]) > mean(0, (p) => p[0] + p[1] + p[2]), "the ground ring's brightness");
  for (let y = 0; y < 128; y++) for (let c = 0; c < 4; c++) assert.ok(Math.abs(px(0, y)[c] - px(127, y)[c]) < 12, `row ${y} channel ${c} at the seam`);
  assert.strictEqual(F.shapeTextureName({ colour: [0.6, 0.8, 1] }), "n5s_8c757fb28f99");
  assert.strictEqual(F.shapeTextureName({ colour: [1, 0.85, 0.4] }), "n5s_a3caa0709dda");
  for (const n of ["n5s_8c757fb28f99", "n5s_a3caa0709dda"]) assert.ok(F.SHAPE_TEXTURE.test(n), n);
  assert.ok(!F.SHAPE_TEXTURE.test("n5p_2cd3d725dedb"));
  assert.strictEqual(F.tgaBytes(t, { alpha: true }).length, 65554);
});
check("tgaBytes with alpha: a 32-bit TGA, 8 alpha bits, rows bottom-up", () => {
  const img = { width: 2, height: 2, data: new Uint8ClampedArray([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160]) };
  const t = F.tgaBytes(img, { alpha: true });
  assert.strictEqual(t.length, 34);
  assert.deepStrictEqual([...t.subarray(0, 18)], [0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 32, 8]);
  assert.deepStrictEqual([...t.subarray(18)], [110, 100, 90, 120, 150, 140, 130, 160, 30, 20, 10, 40, 70, 60, 50, 80]);
  const plain = F.tgaBytes(img);
  assert.strictEqual(plain.length, 30);
  assert.strictEqual(plain[16], 24);
  assert.ok(F.tgaBytes(img, {}).equals(plain));
});
check("compositionText: a shape part on one line, its shape after note", () => {
  const text = F.compositionText({ stages: { small: [{ at: [0, 0, 0], shape: { type: "dome", radius: 4.57 }, note: "zone" }] } });
  assert.ok(text.split("\n").includes('      { "note": "zone", "shape": { "type": "dome", "radius": 4.57 }, "at": [0, 0, 0] }'), text);
});
check("shapes in a page: the same mesh, texture bytes and name", () => {
  const P = pageFloatDisk();
  assert.deepStrictEqual(P.SHAPE_TYPES, F.SHAPE_TYPES);
  for (const type of F.SHAPE_TYPES) {  // every type (S5)
    for (const o of [{}, { mapping: "above", texture: "none", sides: "outside" }, { repeat_around: 3, repeat_up: 2 }]) {
      const s = Object.assign({ type, radius: 2 }, o);
      assert.deepStrictEqual(P.shapeModel(P.readShape(s, "s")), F.shapeModel(F.readShape(s, "s")), `${type} ${JSON.stringify(o)}`);
    }
  }
  const page = P.tgaBytes(P.shapeTexture({ colour: [0.6, 0.8, 1] }), { alpha: true });
  assert.ok(page instanceof Uint8Array && !Buffer.isBuffer(page));
  assert.ok(Buffer.from(page).equals(F.tgaBytes(F.shapeTexture({ colour: [0.6, 0.8, 1] }), { alpha: true })));
  assert.strictEqual(P.shapeTextureName({ colour: [1, 0.85, 0.4] }), F.shapeTextureName({ colour: [1, 0.85, 0.4] }));
  assert.strictEqual(P.shapeTextureName({ colour: [0.6, 0.8, 1] }), "n5s_8c757fb28f99");
});
check("a soft texture's name fixes its bytes: the colour is painted as it's named, to 0.001", () => {
  const soft = (colour) => F.tgaBytes(F.shapeTexture({ colour }), { alpha: true });
  // [0.6001, 0.8, 1] is named as [0.6, 0.8, 1]: the same picture (a file of the name is rewritten as the generator's).
  assert.strictEqual(F.shapeTextureName({ colour: [0.6001, 0.8, 1] }), "n5s_8c757fb28f99");
  assert.ok(soft([0.6001, 0.8, 1]).equals(soft([0.6, 0.8, 1])), "one name, two pictures");
  // A colour named otherwise is painted otherwise.
  assert.notStrictEqual(F.shapeTextureName({ colour: [0.61, 0.8, 1] }), "n5s_8c757fb28f99");
  assert.ok(!soft([0.61, 0.8, 1]).equals(soft([0.6, 0.8, 1])));
});

// --- the Composer's shapes: sphere, cylinder, cone, disc and ring (S5) ---------------------------------------------------
// Each on its defaults at radius 1 - 32 segments, both sides - by the dome's rules: its base at z 0, centred, each side its
// own vertices, the inside first, counter-clockwise seen from outside (a flat one's outside its top), each ring's seam
// column twice. Wrapped: u round it from +x, v from its base up - a disc's and a ring's from the rim in, so the soft
// texture glows at the rim as a dome's at its ground ring (the coordinator's ruling on S5); From above as the dome's.
const shapeOf = (type, o) => F.shapeModel(F.readShape(Object.assign({ type, radius: 1 }, o), "s"));
const LOOK = { segments: 32, sides: "both", mapping: "wrap", repeat_around: 1, repeat_up: 1, colour: [0.6, 0.8, 1], alpha: 0.35, glow: 1, texture: "soft",
  blend: "normal" };
// Both sides of `type`'s mesh, the inside first: `count` vertices and `tris` triangles a side, the inside's facing in (each
// triangle's sign by `face`), each side its own vertices (the inside's the outside's, in the same order, UVs too), the
// inside in smoothing group 2; rounded, no -0. Outside only: one side, every triangle facing out. -> the mesh.
function twoSided(type, count, tris, face) {
  const model = shapeOf(type), m = model.meshes[0];
  assert.strictEqual(model.meshes.length, 1);
  assert.strictEqual(m.name, type);
  assert.deepStrictEqual([m.verts.length / 3, m.tverts.length / 2, m.faces.length / 3], [2 * count, 2 * count, 2 * tris]);
  face(m).forEach((d, t) => assert.ok(t < tris ? d < 0 : d > 0, `triangle ${t} faces ${d < 0 ? "in" : "out"}`));
  m.faces.forEach((v, k) => assert.ok(k < 3 * tris ? v >= 0 && v < count : v >= count && v < 2 * count, `triangle ${Math.floor(k / 3)} uses vertex ${v}`));
  assert.deepStrictEqual(m.verts.slice(0, 3 * count), m.verts.slice(3 * count));
  assert.deepStrictEqual(m.tverts.slice(0, 2 * count), m.tverts.slice(2 * count));
  assert.deepStrictEqual(m.groups, new Array(tris).fill(2).concat(new Array(tris).fill(1)));
  rounded(m);
  noNegativeZero(model);
  const out = shapeOf(type, { sides: "outside" }).meshes[0];
  assert.deepStrictEqual([out.verts.length / 3, out.faces.length / 3], [count, tris]);
  face(out).forEach((d, t) => assert.ok(d > 0, `outside only: triangle ${t} faces in`));
  assert.deepStrictEqual(out.groups, new Array(tris).fill(1));
  return m;
}
// The outside's vertices at point p (the second half of a mesh with both sides), and their UVs.
const outsideAt = (m, p) => { const half = m.verts.length / 6; return Array.from({ length: half }, (_, i) => half + i).filter((i) => vertOf(m, i).every((c, k) => Math.abs(c - p[k]) < 1e-4)); };
const uvsAt = (m, p) => outsideAt(m, p).map((i) => uvOf(m, i));
// A pole at p: a vertex a segment, each at its segment's middle u (times `around`), all at v `v`.
function poleAt(m, p, v, around = 1) {
  const at = outsideAt(m, p);
  assert.strictEqual(at.length, 32, `${at.length} vertices at ${p}`);
  at.forEach((i, k) => {
    const [u, w] = uvOf(m, i);
    assert.strictEqual(w, v, `pole ${p} v ${w}`);
    assert.ok(Math.abs(u - ((k + 0.5) / 32) * around) <= 1e-5, `pole ${p} u ${u}`);
  });
}
const zsOf = (m) => m.verts.filter((_, i) => i % 3 === 2);

check("the other shapes: six types, their labels and fields; a sphere's, a cylinder's and a cone's height twice the radius, a cylinder's top its radius, a flat one's none, a ring's inner 0.85 of it", () => {
  assert.deepStrictEqual(F.SHAPE_TYPES, ["dome", "sphere", "cylinder", "cone", "disc", "ring"]);
  assert.deepStrictEqual(F.SHAPE_LABELS, { dome: "Dome (half sphere)", sphere: "Sphere", cylinder: "Cylinder (open)", cone: "Cone (point up)",
    disc: "Disc (flat, on the ground)", ring: "Ring (flat band)" });
  assert.deepStrictEqual(F.SHAPE_KEYS, ["type", "radius", "height", "inner", "top", "segments", "sides", "mapping", "repeat_around", "repeat_up", "colour", "alpha", "glow",
    "texture", "blend"]);
  const solid = ["radius", "height", "segments", "sides"];
  assert.deepStrictEqual(F.SHAPE_FIELDS, { dome: solid, sphere: solid, cylinder: ["radius", "top", "height", "segments", "sides"], cone: solid,
    disc: ["radius", "segments", "sides"], ring: ["radius", "inner", "segments", "sides"] });
  assert.deepStrictEqual(F.SHAPE_FLAT, ["disc", "ring"]);
  assert.deepStrictEqual(F.SHAPE_INFO.inner, { label: "Inner radius (m)", min: 0.01, max: 100, step: 0.05 });
  // Smoothness's tooltip true of every type (the card shows one for each).
  assert.strictEqual(F.SHAPE_INFO.segments.title, "Segments round it; a dome's rings up it are a quarter of that, a sphere's half");
  // Opacity's true with a texture too (the final review): a Soft or library texture's own alpha still shows through at 1.
  assert.strictEqual(F.SHAPE_INFO.alpha.title,
    "Its alpha: at 1 it's as solid as its texture (Soft and library textures still show through where their own alpha does); the game's globes go to about 0.3");
  // The defaults, in SHAPE_KEYS' order; each read as its defaults and written as its type and radius alone.
  const want = {
    sphere: { type: "sphere", radius: 4.57, height: 9.14 }, cylinder: { type: "cylinder", radius: 4.57, height: 9.14, top: 4.57 }, cone: { type: "cone", radius: 4.57, height: 9.14 },
    disc: { type: "disc", radius: 4.57 }, ring: { type: "ring", radius: 4.57, inner: 3.885 },
  };
  for (const [type, own] of Object.entries(want)) {
    const d = F.shapeDefaults(type, 4.57), whole = Object.assign({}, own, LOOK);
    assert.deepStrictEqual(d, whole, type);
    assert.deepStrictEqual(Object.keys(d), Object.keys(whole), type);
    assert.deepStrictEqual(F.readShape({ type, radius: 4.57 }, "s"), d, type);
    assert.deepStrictEqual(F.cleanShape(d), { type, radius: 4.57 }, type);
    assert.strictEqual(F.partLabel(F.readPart({ shape: { type, radius: 4.57 }, at: [0, 0, 0] }, "p", BASE)), `${type} 4.57 m`);
  }
  // A height given is kept when it isn't the default; a flat shape has none, and only a ring has an inner.
  assert.deepStrictEqual(F.cleanShape(F.readShape({ type: "cylinder", radius: 1, height: 3 }, "s")), { type: "cylinder", radius: 1, height: 3 });
  assert.deepStrictEqual(F.cleanShape(F.readShape({ type: "cone", radius: 1, height: 2 }, "s")), { type: "cone", radius: 1 });
  const keys = "type, radius, segments, sides, mapping, repeat_around, repeat_up, colour, alpha, glow, texture or blend";
  assert.throws(() => F.readShape({ type: "disc", radius: 1, height: 1 }, "s"), (e) => e.message === `s.height: not a disc's - ${keys}`);
  assert.throws(() => F.readShape({ type: "ring", radius: 1, height: 1 }, "s"), /^Error: s\.height: not a ring's - type, radius, inner, segments, /);
  for (const type of ["dome", "sphere", "cylinder", "cone", "disc"]) assert.throws(() => F.readShape({ type, radius: 1, inner: 0.5 }, "s"), new RegExp(`^Error: s\\.inner: not a ${type}'s - `));
  assert.throws(() => F.readShape({ type: "cube", radius: 1 }, "s"), (e) => e.message === "s.type: dome, sphere, cylinder, cone, disc or ring");
});
check("a sphere: 1118 vertices and 1920 triangles, the inside first, its lowest point at 0, v from the bottom pole", () => {
  const m = twoSided("sphere", 559, 960, (x) => normals(x, [0, 0, 1])), model = shapeOf("sphere");
  assert.deepStrictEqual([model.lo, model.hi], [[-1, -1, 0], [1, 1, 2]]);
  for (let i = 0; i < 1118; i++) { const [x, y, z] = vertOf(m, i); near(x * x + y * y + (z - 1) * (z - 1), 1); }
  assert.deepStrictEqual([Math.min(...zsOf(m)), Math.max(...zsOf(m))], [0, 2]);
  // Wrapped at one repeat: v 0 at the bottom pole, 0.5 at the equator, 1 at the top - each ring's v its latitude's share
  // of the half turn; u round it from +x.
  poleAt(m, [0, 0, 0], 0);
  poleAt(m, [0, 0, 2], 1);
  assert.deepStrictEqual(uvsAt(m, [1, 0, 1]), [[0, 0.5], [1, 0.5]]);
  for (let i = 559; i < 1118; i++) {
    const [x, y, z] = vertOf(m, i);
    if (x !== 0 || y !== 0) near(uvOf(m, i)[1], 0.5 + Math.asin(Math.max(-1, Math.min(1, z - 1))) / Math.PI, 1e-3);
  }
  assert.ok(m.tverts.every((t) => t >= 0 && t <= 1));
  // Its height given: still standing on 0, its middle at half of it.
  const low = shapeOf("sphere", { height: 1 }), l = low.meshes[0];
  assert.deepStrictEqual(low.hi, [1, 1, 1]);
  for (let i = 0; i < l.verts.length / 3; i++) { const [x, y, z] = vertOf(l, i); near(x * x + y * y + 4 * (z - 0.5) * (z - 0.5), 1); }
});
check("a cylinder: an open tube, 132 vertices and 128 triangles, v up its side", () => {
  const m = twoSided("cylinder", 66, 64, (x) => normals(x)), model = shapeOf("cylinder");
  assert.deepStrictEqual([model.lo, model.hi], [[-1, -1, 0], [1, 1, 2]]);
  for (let i = 0; i < 132; i++) {
    const [x, y, z] = vertOf(m, i);
    near(x * x + y * y, 1);
    assert.ok(z === 0 || z === 2, `vertex ${i} at z ${z}`);
    assert.strictEqual(uvOf(m, i)[1], z / 2);  // v 0 at the ground, 1 at the top
  }
  assert.deepStrictEqual(uvsAt(m, [1, 0, 0]), [[0, 0], [1, 0]]);
  assert.deepStrictEqual(uvsAt(m, [1, 0, 2]), [[0, 1], [1, 1]]);
});
check("a cone: a ring to a point, 130 vertices and 64 triangles, the point at its height", () => {
  const m = twoSided("cone", 65, 32, (x) => normals(x)), model = shapeOf("cone");
  assert.deepStrictEqual([model.lo, model.hi], [[-1, -1, 0], [1, 1, 2]]);
  // Its ring on the ground at its radius (the seam's twice), its point a vertex a segment at its height; its base open.
  const half = Array.from({ length: 65 }, (_, i) => 65 + i), ground = half.filter((i) => vertOf(m, i)[2] === 0);
  assert.strictEqual(ground.length, 33);
  ground.forEach((i) => { const [x, y] = vertOf(m, i); near(x * x + y * y, 1); assert.strictEqual(uvOf(m, i)[1], 0); });
  poleAt(m, [0, 0, 2], 1);
  assert.deepStrictEqual(uvsAt(m, [1, 0, 0]), [[0, 0], [1, 0]]);
  assert.strictEqual(Math.max(...zsOf(m)), 2);
});
check("a disc: flat at 0, 130 vertices and 64 triangles, its outside facing up, v from the rim to the centre", () => {
  const m = twoSided("disc", 65, 32, (x) => normals(x, "z")), model = shapeOf("disc");
  assert.deepStrictEqual([model.lo, model.hi], [[-1, -1, 0], [1, 1, 0]]);
  assert.ok(zsOf(m).every((z) => z === 0), "a vertex off the ground");
  // Wrapped at one repeat, from the rim in: its rim at v 0 (where the soft texture glows), its centre a vertex a segment
  // at v 1.
  poleAt(m, [0, 0, 0], 1);
  assert.deepStrictEqual(uvsAt(m, [1, 0, 0]), [[0, 0], [1, 0]]);
  const rim = Array.from({ length: 65 }, (_, i) => 65 + i).filter((i) => vertOf(m, i)[0] !== 0 || vertOf(m, i)[1] !== 0);
  assert.strictEqual(rim.length, 33);
  rim.forEach((i) => { const [x, y] = vertOf(m, i); near(x * x + y * y, 1); assert.strictEqual(uvOf(m, i)[1], 0); });
});
check("a ring: a flat band, 132 vertices and 128 triangles, its inner edge 0.85 of the radius, v across the band", () => {
  const m = twoSided("ring", 66, 64, (x) => normals(x, "z")), model = shapeOf("ring");
  assert.deepStrictEqual([model.lo, model.hi], [[-1, -1, 0], [1, 1, 0]]);
  assert.ok(zsOf(m).every((z) => z === 0), "a vertex off the ground");
  // Its outer edge at the radius, its inner at 0.85 of it, 33 vertices each a side; wrapped at one repeat, from the rim
  // in: v 0 at the outer edge (where the soft texture glows), 1 at the inner.
  const edge = (r) => Array.from({ length: 66 }, (_, i) => 66 + i).filter((i) => { const [x, y] = vertOf(m, i); return Math.abs(Math.hypot(x, y) - r) < 1e-4; });
  assert.deepStrictEqual([edge(1).length, edge(0.85).length], [33, 33]);
  assert.ok(edge(1).every((i) => uvOf(m, i)[1] === 0) && edge(0.85).every((i) => uvOf(m, i)[1] === 1));
  assert.deepStrictEqual(uvsAt(m, [1, 0, 0]), [[0, 0], [1, 0]]);
  assert.deepStrictEqual(uvsAt(m, [0.85, 0, 0]), [[0, 1], [1, 1]]);
  // Its inner given: kept when it isn't the default (0.85 of the radius, to the millimetre); from 0.01 to below the radius.
  const narrow = shapeOf("ring", { inner: 0.5 }).meshes[0];
  assert.strictEqual(outsideAt(narrow, [0.5, 0, 0]).length, 2);
  assert.deepStrictEqual(F.cleanShape(F.readShape({ type: "ring", radius: 4.57, inner: 2 }, "s")), { type: "ring", radius: 4.57, inner: 2 });
  assert.deepStrictEqual(F.cleanShape(F.readShape({ type: "ring", radius: 4.57, inner: 3.885 }, "s")), { type: "ring", radius: 4.57 });
  for (const inner of [1, 0, 1.5, 0.005, -1, "0.5"]) {
    assert.throws(() => F.readShape({ type: "ring", radius: 1, inner }, "s"), /^Error: s\.inner: a number from 0\.01 to below the radius$/, `inner ${inner}`);
  }
  for (const inner of [0.01, 0.999]) assert.strictEqual(F.readShape({ type: "ring", radius: 1, inner }, "s").inner, inner);
  // The smallest ring's default inner still reads.
  assert.strictEqual(F.readShape({ type: "ring", radius: 0.05 }, "s").inner, 0.043);
});
// Each type Wrapped and From above (addendum (b): one check per type for each mapping), at radius 1.
// - Wrapped, Repeat around 3 and up 2: at each of its `seams` ([x, y, z] at angle 0, its v at one repeat) the seam's two
//   vertices, u 0 and 3, at twice that v; each of its `poles` a vertex a segment at its segment's middle u three times
//   round, at twice its v; every UV within [0, 3] x [0, 2].
// - From above (with the repeats or without: the same mesh): every vertex at (0.5 + x / 2, 0.5 + y / 2) from its own
//   place, the inside's as its vertex's - its `rim` at angle 0 (1, 0.5), at 90 degrees (0.5, 1), a pole (0.5, 0.5).
const MAPPED = {
  sphere: { wrap: "v from the bottom pole (0) through the equator (1) to the top (2)", seams: [[[1, 0, 1], 0.5]], poles: [[[0, 0, 0], 0], [[0, 0, 2], 1]], z: 1 },
  cylinder: { wrap: "v from the ground (0) to the top (2)", seams: [[[1, 0, 0], 0], [[1, 0, 2], 1]], poles: [], z: 0 },
  cone: { wrap: "v from the ground (0) to the point (2)", seams: [[[1, 0, 0], 0]], poles: [[[0, 0, 2], 1]], z: 0 },
  disc: { wrap: "v from the rim (0) to the centre (2)", seams: [[[1, 0, 0], 0]], poles: [[[0, 0, 0], 1]], z: 0 },
  ring: { wrap: "v from the outer edge (0) to the inner (2)", seams: [[[1, 0, 0], 0], [[0.85, 0, 0], 1]], poles: [], z: 0 },
};
for (const [type, t] of Object.entries(MAPPED)) {
  check(`a ${type} Wrapped, Repeat around 3 and up 2: u round it from +x to 3, ${t.wrap}`, () => {
    const model = shapeOf(type, { repeat_around: 3, repeat_up: 2 }), m = model.meshes[0];
    rounded(m);
    noNegativeZero(model);
    for (const [p, v] of t.seams) assert.deepStrictEqual(uvsAt(m, p), [[0, 2 * v], [3, 2 * v]], `the seam at ${p}`);
    for (const [p, v] of t.poles) poleAt(m, p, 2 * v, 3);
    for (let i = 0; i < m.tverts.length; i += 2) assert.ok(m.tverts[i] >= 0 && m.tverts[i] <= 3 && m.tverts[i + 1] >= 0 && m.tverts[i + 1] <= 2, `UV ${i / 2}`);
    assert.ok(m.tverts.some((x, i) => i % 2 === 0 && x === 3) && m.tverts.some((x, i) => i % 2 === 1 && x === 2), "the repeats not reached");
    // The repeats only tile the UVs: the same vertices and faces as at one repeat.
    const one = shapeOf(type).meshes[0];
    assert.deepStrictEqual([m.verts, m.faces], [one.verts, one.faces]);
  });
  check(`a ${type} From above: laid on from overhead, its rim at angle 0 (1, 0.5)${t.poles.length ? ", a pole (0.5, 0.5)" : ""}; the repeats don't apply`, () => {
    const model = shapeOf(type, { mapping: "above" }), m = model.meshes[0], count = m.verts.length / 3;
    rounded(m);
    noNegativeZero(model);
    assert.deepStrictEqual(shapeOf(type, { mapping: "above", repeat_around: 3, repeat_up: 2 }), model);
    for (let i = 0; i < count; i++) {
      const [x, y] = vertOf(m, i), [u, v] = uvOf(m, i);
      near(u, 0.5 + x / 2);
      near(v, 0.5 + y / 2);
    }
    assert.deepStrictEqual(m.tverts.slice(0, count), m.tverts.slice(count));  // the inside keeps its vertex's UV
    assert.deepStrictEqual(uvsAt(m, [1, 0, t.z]), [[1, 0.5], [1, 0.5]]);
    assert.deepStrictEqual(uvOf(m, outsideAt(m, [0, 1, t.z])[0]), [0.5, 1]);
    for (const [p] of t.poles) assert.ok(outsideAt(m, p).every((i) => uvOf(m, i)[0] === 0.5 && uvOf(m, i)[1] === 0.5), `the pole at ${p}`);
    if (type === "ring") assert.deepStrictEqual(uvsAt(m, [0.85, 0, 0]), [[0.925, 0.5], [0.925, 0.5]]);  // its r the outer radius
  });
}

// --- the Cylinder's Top radius (TR; the owner, 2026-11-18, on Elemental Shield's flame ring - "it looks like a flame
// texture on a truncated upside down cone" - "yes" to a Top radius that makes the Cylinder a truncated cone either way up) -
// A cylinder's profile runs from (radius, 0) to (top, height): at top = radius the cylinder before it, byte for byte; wider
// flares out (vdr_eleshield's Cylinder02: 1.00 m at the ground, 1.17 m at 0.6 m), narrower leans in. Its default is its
// radius, never written, so a Top left at its default follows the Radius as a Height does.
const FLARE = { top: 1.17, height: 0.6, segments: 36 };  // radius 1 (shapeOf's): Elemental Shield's flame ring
const sha256 = (text) => require("crypto").createHash("sha256").update(text).digest("hex");
// Each triangle's normal against the axis: (b - a) x (c - a) dotted with its centroid's (x, y, 0) - + facing away from
// the axis (out), - towards it (in), whichever way the wall leans.
function radial(m) {
  const out = [];
  for (let f = 0; f < m.faces.length; f += 3) {
    const [a, b, c] = [0, 1, 2].map((k) => vertOf(m, m.faces[f + k]));
    const u = [b[0] - a[0], b[1] - a[1], b[2] - a[2]], v = [c[0] - a[0], c[1] - a[1], c[2] - a[2]];
    const n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]];
    out.push(n[0] * (a[0] + b[0] + c[0]) / 3 + n[1] * (a[1] + b[1] + c[1]) / 3);
  }
  return out;
}
// The signed volume its triangles `from` to `to` (face indices) sweep from the origin: a . (b x c) / 6 each. A wall facing
// out encloses + (with its caps: the ground's adds nothing, the origin on its plane), facing in -.
function signedVolume(m, from, to) {
  let sum = 0;
  for (let f = from; f < to; f++) {
    const [a, b, c] = [0, 1, 2].map((k) => vertOf(m, m.faces[3 * f + k]));
    sum += (a[0] * (b[1] * c[2] - b[2] * c[1]) + a[1] * (b[2] * c[0] - b[0] * c[2]) + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6;
  }
  return sum;
}
// A frustum's wall with n segments, r at 0 and t at h: the solid's h k (r^2 + r t + t^2) / 3 less its top cap's h k t^2 / 3
// (k: a regular n-gon's area for a radius of 1).
const wallVolume = (n, r, t, h) => (h * (n / 2) * Math.sin((2 * Math.PI) / n) * (r * r + r * t)) / 3;
// The outside's rings: [z, its vertices, their least and greatest distance from the axis (to 0.0001)], from the ground up.
const ringsOf = (m) => {
  const half = m.verts.length / 6, at = new Map();
  for (let i = half; i < 2 * half; i++) {
    const [x, y, z] = vertOf(m, i), key = String(z);
    if (!at.has(key)) at.set(key, []);
    at.get(key).push(Math.round(Math.hypot(x, y) * 1e4) / 1e4);
  }
  return [...at.entries()].map(([z, rs]) => [Number(z), rs.length, Math.min(...rs), Math.max(...rs)]).sort((a, b) => a[0] - b[0]);
};

check("TR: the Cylinder's Top radius - its field after Radius on its card, its key after inner; its default the Radius, never written, following the Radius; below 0.01 or past 100 refused, naming it; no other type's", () => {
  assert.deepStrictEqual(F.SHAPE_INFO.top, { label: "Top radius (m)", min: 0.01, max: 100, step: 0.05,
    title: "The radius at its top: the same as Radius is a cylinder; wider flares out (a lampshade upside down), narrower leans in" });
  assert.deepStrictEqual(F.SHAPE_FIELDS.cylinder, ["radius", "top", "height", "segments", "sides"]);
  assert.strictEqual(F.SHAPE_KEYS.indexOf("top"), F.SHAPE_KEYS.indexOf("inner") + 1);
  // Its default the Radius: read in, never written; given, kept when it isn't the Radius - after the height, as SHAPE_KEYS.
  assert.strictEqual(F.shapeDefaults("cylinder", 1.5).top, 1.5);
  assert.strictEqual(F.readShape({ type: "cylinder", radius: 2 }, "s").top, 2);
  assert.deepStrictEqual(F.cleanShape(F.readShape({ type: "cylinder", radius: 1, top: 1 }, "s")), { type: "cylinder", radius: 1 });
  const flared = F.cleanShape(F.readShape({ type: "cylinder", radius: 1, top: 1.17, height: 0.6 }, "s"));
  assert.deepStrictEqual(flared, { type: "cylinder", radius: 1, height: 0.6, top: 1.17 });
  assert.deepStrictEqual(Object.keys(flared), ["type", "radius", "height", "top"]);
  // A Top left at its default follows the Radius (it isn't written), as a Height does; one written stays.
  const written = F.cleanShape(F.readShape({ type: "cylinder", radius: 1 }, "s"));
  assert.deepStrictEqual([F.readShape(Object.assign({}, written, { radius: 3 }), "s").top, F.readShape(Object.assign({}, written, { radius: 3 }), "s").height], [3, 6]);
  assert.strictEqual(F.readShape(Object.assign({}, flared, { radius: 3 }), "s").top, 1.17);
  // In a part: its key and the composition's text carry it; its label is its radius.
  const part = F.readPart({ shape: { type: "cylinder", radius: 1, top: 1.17 }, at: [0, 0, 0] }, "p", BASE);
  assert.strictEqual(F.partKey(part), 'shape:{ "type": "cylinder", "radius": 1, "top": 1.17 }');
  assert.strictEqual(F.partLabel(part), "cylinder 1 m");
  // Its errors name it: below 0.01, past 100, not a number; its bounds read.
  for (const top of [0.005, 0, -1, 101, "1", null]) {
    assert.throws(() => F.readShape({ type: "cylinder", radius: 1, top }, "s"), (e) => e.message === "s.top: a number from 0.01 to 100 (metres)", `top ${top}`);
  }
  for (const top of [0.01, 100]) assert.strictEqual(F.readShape({ type: "cylinder", radius: 1, top }, "s").top, top);
  // A cylinder's own keys, as an error lists them; no other type takes a top.
  assert.throws(() => F.readShape({ type: "cylinder", radius: 1, inner: 0.5 }, "s"),
    (e) => e.message === "s.inner: not a cylinder's - type, radius, height, top, segments, sides, mapping, repeat_around, repeat_up, colour, alpha, glow, texture or blend");
  for (const type of ["dome", "sphere", "cone", "disc", "ring"]) {
    assert.throws(() => F.readShape({ type, radius: 1, top: 1 }, "s"), new RegExp(`^Error: s\\.top: not a ${type}'s - `), type);
    assert.ok(!("top" in F.shapeDefaults(type, 1)), type);
  }
});
// Pinned: these cylinders' models (shapeModel's JSON) as anim c7c650b gave them, before the Top radius.
const BEFORE_TOP = [
  [{ type: "cylinder", radius: 1 }, "53f31cea2527d1f4b846838a12fa34e77ff9b436cd7125792d11c57f62403277", "n5s_8c757fb28f99"],
  [{ type: "cylinder", radius: 4.57 }, "904c6940a58746dce29d1f2aafa6a0563367aa29eb89d38ee1cda56e9d41c84d", "n5s_8c757fb28f99"],
  [{ type: "cylinder", radius: 1, blend: "additive" }, "767e0df8c57cb7543ce714fd004c8a424a6b5c0bb3bfa032867c35cb97e68193", "n5s_617b220c1144"],
  [{ type: "cylinder", radius: 2, height: 0.6, segments: 36, sides: "outside", mapping: "above" }, "4af580ac589fa5dce9c819715e28baf9e2fbd70d1d790146c4b670d2c09ff7b4",
    "n5s_8c757fb28f99"],
  [{ type: "cylinder", radius: 1, repeat_around: 3, repeat_up: 2, texture: "fxpa_flame02", alpha: 1 }, "1c1dae79d176d0d736acd9d549bba62546c352647308f137c25a0d6ea7716bf5",
    "n5s_8c757fb28f99"],
];
check("TR: a cylinder at Top = Radius is the cylinder before it, byte for byte - its model (pinned: anim c7c650b's), its written shape and its soft texture name, Top given or not", () => {
  for (const [s, model, soft] of BEFORE_TOP) {
    for (const given of [s, Object.assign({}, s, { top: s.radius })]) {
      const shape = F.readShape(given, "s");
      assert.strictEqual(sha256(JSON.stringify(F.shapeModel(shape))), model, JSON.stringify(given));
      assert.deepStrictEqual(F.cleanShape(shape), F.cleanShape(F.readShape(s, "s")), JSON.stringify(given));
      assert.ok(!("top" in F.cleanShape(shape)), JSON.stringify(given));
      assert.strictEqual(F.shapeTextureName(shape), soft, JSON.stringify(given));
    }
  }
});
check("TR: a flared cylinder (radius 1, Top 1.17, height 0.6, 36 segments): two rings of 37, at 1 on the ground and 1.17 at 0.6; both sides, the inside first, facing in, the outside out (per face and by signed volume)", () => {
  const model = shapeOf("cylinder", FLARE), m = model.meshes[0];
  assert.strictEqual(m.name, "cylinder");
  assert.deepStrictEqual([m.verts.length / 3, m.tverts.length / 2, m.faces.length / 3], [148, 148, 144]);
  assert.deepStrictEqual(ringsOf(m), [[0, 37, 1, 1], [0.6, 37, 1.17, 1.17]]);
  assert.deepStrictEqual([model.lo, model.hi], [[-1.17, -1.17, 0], [1.17, 1.17, 0.6]]);  // its box the wider ring's
  // Each side its own vertices (the inside's the outside's, UVs too), the inside in group 2.
  assert.deepStrictEqual(m.verts.slice(0, 222), m.verts.slice(222));
  assert.deepStrictEqual(m.tverts.slice(0, 148), m.tverts.slice(148));
  assert.deepStrictEqual(m.groups, new Array(72).fill(2).concat(new Array(72).fill(1)));
  m.faces.forEach((v, k) => assert.ok(k < 3 * 72 ? v >= 0 && v < 74 : v >= 74 && v < 148, `triangle ${Math.floor(k / 3)} uses vertex ${v}`));
  radial(m).forEach((d, t) => assert.ok(t < 72 ? d < 0 : d > 0, `triangle ${t} faces ${d < 0 ? "in" : "out"}`));
  const wall = wallVolume(36, 1, 1.17, 0.6);
  near(signedVolume(m, 72, 144), wall, 1e-4);
  near(signedVolume(m, 0, 72), -wall, 1e-4);
  // Wrapped at one repeat: u round it from +x, v 0 on the ground ring and 1 on the top one.
  assert.deepStrictEqual(uvsAt(m, [1, 0, 0]), [[0, 0], [1, 0]]);
  assert.deepStrictEqual(uvsAt(m, [1.17, 0, 0.6]), [[0, 1], [1, 1]]);
  for (let i = 74; i < 148; i++) assert.strictEqual(uvOf(m, i)[1], vertOf(m, i)[2] === 0 ? 0 : 1);
  rounded(m);
  noNegativeZero(model);
  // Outside only: one side, every triangle out.
  const out = shapeOf("cylinder", Object.assign({ sides: "outside" }, FLARE)).meshes[0];
  assert.deepStrictEqual([out.verts.length / 3, out.faces.length / 3], [74, 72]);
  radial(out).forEach((d, t) => assert.ok(d > 0, `outside only: triangle ${t} faces in`));
  near(signedVolume(out, 0, 72), wall, 1e-4);
  // Leaning in (Top 0.5): its rings at 1 and 0.5, still out outside and in inside; its box the ground ring's.
  const lean = shapeOf("cylinder", { top: 0.5, height: 0.6, segments: 36 }), l = lean.meshes[0];
  assert.deepStrictEqual(ringsOf(l), [[0, 37, 1, 1], [0.6, 37, 0.5, 0.5]]);
  assert.deepStrictEqual([lean.lo, lean.hi], [[-1, -1, 0], [1, 1, 0.6]]);
  radial(l).forEach((d, t) => assert.ok(t < 72 ? d < 0 : d > 0, `leaning in: triangle ${t} faces ${d < 0 ? "in" : "out"}`));
  near(signedVolume(l, 72, 144), wallVolume(36, 1, 0.5, 0.6), 1e-4);
  // The cylinder itself by the same rule: the wall of a cylinder (2 r^2 h k / 3).
  near(signedVolume(shapeOf("cylinder").meshes[0], 64, 128), wallVolume(32, 1, 1, 2), 1e-4);
});
check("TR: a flared or leaning cylinder From above - its UVs from the larger of its two radii, every one within [0, 1]: the wider ring at the picture's edge", () => {
  for (const [top, wide] of [[1.17, 1.17], [0.5, 1]]) {
    const model = shapeOf("cylinder", { top, height: 0.6, segments: 36, mapping: "above" }), m = model.meshes[0];
    rounded(m);
    noNegativeZero(model);
    for (let i = 0; i < m.verts.length / 3; i++) {
      const [x, y] = vertOf(m, i), [u, v] = uvOf(m, i);
      near(u, 0.5 + x / (2 * wide));
      near(v, 0.5 + y / (2 * wide));
    }
    assert.ok(m.tverts.every((t) => t >= 0 && t <= 1), `top ${top}: a UV past [0, 1]`);
    assert.deepStrictEqual(uvsAt(m, top > 1 ? [1.17, 0, 0.6] : [1, 0, 0]), [[1, 0.5], [1, 0.5]]);  // the wider ring at its edge
    const narrow = top > 1 ? [1, 0, 0] : [0.5, 0, 0.6];
    assert.ok(uvsAt(m, narrow).every(([u, v]) => Math.abs(u - (0.5 + narrow[0] / (2 * wide))) <= 1e-5 && v === 0.5), `top ${top}: the narrower ring`);
    assert.deepStrictEqual(shapeOf("cylinder", { top, height: 0.6, segments: 36, mapping: "above", repeat_around: 3, repeat_up: 2 }), model);  // the repeats don't apply
  }
});
check("TR: a flared cylinder Wrapped, Repeat around 3 and up 2: u round it to 3, v up it to 2 - the same vertices and faces as at one repeat", () => {
  const model = shapeOf("cylinder", Object.assign({ repeat_around: 3, repeat_up: 2 }, FLARE)), m = model.meshes[0];
  rounded(m);
  noNegativeZero(model);
  assert.deepStrictEqual(uvsAt(m, [1, 0, 0]), [[0, 0], [3, 0]]);
  assert.deepStrictEqual(uvsAt(m, [1.17, 0, 0.6]), [[0, 2], [3, 2]]);
  for (let i = 0; i < m.tverts.length; i += 2) assert.ok(m.tverts[i] >= 0 && m.tverts[i] <= 3 && m.tverts[i + 1] >= 0 && m.tverts[i + 1] <= 2, `UV ${i / 2}`);
  const one = shapeOf("cylinder", FLARE).meshes[0];
  assert.deepStrictEqual([m.verts, m.faces, m.groups], [one.verts, one.faces, one.groups]);
});
check("TR in a page: the same Top radius, flared and leaning meshes", () => {
  const P = pageFloatDisk();
  assert.deepStrictEqual(P.SHAPE_INFO.top, F.SHAPE_INFO.top);
  assert.deepStrictEqual(P.SHAPE_FIELDS.cylinder, F.SHAPE_FIELDS.cylinder);
  for (const o of [FLARE, { top: 0.5 }, Object.assign({ mapping: "above", sides: "outside" }, FLARE), Object.assign({ repeat_around: 3 }, FLARE)]) {
    const s = Object.assign({ type: "cylinder", radius: 1 }, o);
    assert.deepStrictEqual(P.shapeModel(P.readShape(s, "s")), F.shapeModel(F.readShape(s, "s")), JSON.stringify(o));
    assert.deepStrictEqual(P.cleanShape(P.readShape(s, "s")), F.cleanShape(F.readShape(s, "s")), JSON.stringify(o));
  }
});

// --- a mesh's transparencyhint (T1b; the owner, 2026-11-18: n5_zot's see-through meshes cut away in game - "go") -------
// Every mesh the generator gives carries transparencyhint, the game's (0 to 5 - vfx_model.cpp's rule): a shape's 1 when
// it's see-through (a texture, or Opacity under 1), else 0; the disk's 1; a model's mesh its model's own, kept through
// modelSpaceMeshes, placePart, lowered, uniqueNames and mergeModels. Both writers - meshLines and a draft's plane (the
// Composer's project.js draftText, build.js's drafts) - write meshHint's.
check("a mesh's transparencyhint: a shape's 1 when see-through, the disk's 1, a model's own kept to the end", () => {
  for (const type of F.SHAPE_TYPES) {
    assert.strictEqual(shapeOf(type).meshes[0].transparencyhint, 1, type + ", soft");
    assert.strictEqual(shapeOf(type, { texture: "none", alpha: 1 }).meshes[0].transparencyhint, 0, type + ", solid");
    assert.strictEqual(shapeOf(type, { texture: "none", alpha: 0.5 }).meshes[0].transparencyhint, 1, type + ", Opacity 0.5");
    assert.strictEqual(shapeOf(type, { texture: "fxpa_symbl_001", alpha: 1 }).meshes[0].transparencyhint, 1, type + ", a library texture");
  }
  assert.strictEqual(F.diskMesh(DISK).transparencyhint, 1);
  // A model's (core.readDraft's planes, from the mesh reader's draft): its own, as vfx_model.cpp keeps it; none, 0.
  const plane = (name, extra) => Object.assign({ name, parent: "", position: [0, 0, 0], verts: [0, 0, 0, 1, 0, 0, 0, 1, 0], tverts: [0, 0, 1, 0, 0, 1],
    faces: [0, 1, 2], bitmap: "x" }, extra);
  const read = F.modelSpaceMeshes({ planes: [plane("glass", { transparencyhint: 2 }), plane("pane", { transparencyhint: 1 }),
    plane("wood", { transparencyhint: 0 }), plane("older"), plane("wide", { transparencyhint: 9 }), plane("below", { transparencyhint: -1 })] }, "t");
  const want = [2, 1, 0, 0, 5, 0], h = (m) => m.transparencyhint;
  assert.deepStrictEqual(read.meshes.map(h), want);
  const placed = F.placePart({ at: [0, 0, 0], turn: 30, scale: 0.5, tilt: [0, 0] }, read, 0, 0.9, "j");
  assert.deepStrictEqual(placed.meshes.map(h), want);
  assert.deepStrictEqual(F.lowered(placed.meshes, 0.9).map(h), want);
  assert.deepStrictEqual(F.uniqueNames(placed.meshes, ["j01_glass"]).map(h), want);
  assert.deepStrictEqual(F.bakeBitmaps(placed.meshes, {}, () => true).meshes.map(h), want);  // a dyed part's baked copies too
  assert.deepStrictEqual(F.mergeModels([read, { meshes: [F.diskMesh(DISK)], lo: [0, 0, 0], hi: [0, 0, 0] }]).meshes.map(h), [...want, 1]);
});
check("meshHint: a mesh's own transparencyhint (a whole number, 0 to 5), else 1 when it's see-through, else 0", () => {
  assert.deepStrictEqual([{ transparencyhint: 2 }, { transparencyhint: 0, transparent: true }, { transparent: true }, { transparent: false }, {},
    { transparencyhint: 7 }, { transparencyhint: -1 }, { transparencyhint: 1.7 }, { transparencyhint: NaN, transparent: true }].map(F.meshHint),
  [2, 0, 1, 0, 0, 5, 0, 1, 1]);
  assert.strictEqual(F.HINT_MAX, 5);
  // hintValue: text as the game's parser (and vfx_model.cpp's strtol) reads an integer - its leading digits, base 10.
  assert.deepStrictEqual(["2", " 3 ", "+2", "1.7", "2abc", "1e3", "-1", "9", "x", "", "0x5"].map(F.hintValue), [2, 3, 2, 1, 2, 1, 0, 5, 0, 0, 0]);
  // Before the digits only what C's isspace skips (space, \t, \n, \v, \f, \r): a no-break space (latin1 0xA0) stops it,
  // as it stops vfx_model.cpp's strtol - 0.
  assert.deepStrictEqual(["\t4", "\n\v\f\r 1", " 2", "  2", " 2", "﻿2"].map(F.hintValue), [4, 1, 0, 0, 0, 0]);
  assert.deepStrictEqual([2, 1.7, -3, 99, NaN, Infinity, undefined, null].map(F.hintValue), [2, 1, 0, 5, 0, 0, 0, 0]);
});
check("meshLines writes meshHint's: a model's own 2, a shape's 1 or 0, a mesh without one by its transparent (as before)", () => {
  const mesh = { name: "j01_glass", bitmap: "tm_glass04", verts: [0, 0, 0, 1, 0, 0, 0, 1, 0], tverts: [0, 0, 1, 0, 0, 1], faces: [0, 1, 2],
    diffuse: [1, 1, 1], ambient: [1, 1, 1], selfillum: [0, 0, 0], alpha: 1 };
  const hint = (m) => block(F.pileModelText("plc_t", [m], [0, 0, 0], [1, 1, 1], []), "node trimesh " + m.name).filter((l) => l.startsWith("  transparencyhint"));
  assert.deepStrictEqual(hint(Object.assign({}, mesh, { transparencyhint: 2 })), ["  transparencyhint 2"]);
  assert.deepStrictEqual(hint(Object.assign({}, mesh, { transparencyhint: 0 })), ["  transparencyhint 0"]);
  assert.deepStrictEqual(hint(mesh), ["  transparencyhint 0"]);
  assert.deepStrictEqual(hint(Object.assign({}, mesh, { transparent: true })), ["  transparencyhint 1"]);
  assert.deepStrictEqual(hint(shapeOf("dome").meshes[0]), ["  transparencyhint 1"]);
  assert.deepStrictEqual(hint(shapeOf("dome", { texture: "none", alpha: 1 }).meshes[0]), ["  transparencyhint 0"]);
  assert.deepStrictEqual(hint(F.diskMesh(DISK)), ["  transparencyhint 1"]);
});
check("an ASCII model's mesh keeps its transparencyhint for the mesh reader (a whole number, as the game reads one); none, no line", () => {
  const glass = NEVERBLENDER.replace("  diffuse 0.8 0.8 0.8", "  diffuse 0.8 0.8 0.8\r\n  transparencyhint 2");
  assert.ok(block(F.readableAscii(glass, "x"), "node trimesh Sack").includes("  transparencyhint 2"));
  assert.ok(!F.readableAscii(NEVERBLENDER, "x").includes("transparencyhint"));
  const strap = NEVERBLENDER.replace("node danglymesh Strap\r\n  parent Sack", "node danglymesh Strap\r\n  parent Sack\r\n  transparencyhint 1.7");
  assert.ok(block(F.readableAscii(strap, "x"), "node trimesh Strap").includes("  transparencyhint 1"));
  const junk = NEVERBLENDER.replace("  diffuse 0.8 0.8 0.8", "  diffuse 0.8 0.8 0.8\r\n  transparencyhint x");
  assert.ok(!F.readableAscii(junk, "x").includes("transparencyhint"));
});

// --- tint (M1, #39; the owner, 2026-11-18: "for the existing textures I'd like to add tinting options to them") --------
// A part's "tint": [r, g, b] (0 to 1; white, the default, is none and never written) multiplies every one of its meshes'
// ambient, diffuse and selfillum - the game lights a textured mesh as texture x (selfillum + ambient x the area's +
// diffuse x the lights), so the material tints the texture. placePart applies it: both generators and the page. A shape
// takes none (its Colour is its colour); a shape's library texture is lit by itself as far as its Glow goes (Q3).
const tinted = () => ({ meshes: [
  { name: "box", bitmap: "plc_x", verts: [0, 0, 0, 1, 0, 0, 0, 1, 0.5], tverts: [0, 0, 1, 0, 0, 1], faces: [0, 1, 2], diffuse: [0.8, 0.6, 0.4],
    ambient: [0.2, 0.4, 1], selfillum: [0, 0.5, 1], alpha: 1, transparencyhint: 2 },
  { name: "lid", bitmap: "", verts: [0, 0, 0.5, 1, 0, 0.5, 0, 1, 1], tverts: [0, 0, 1, 0, 0, 1], faces: [0, 1, 2], diffuse: [1, 1, 1],
    ambient: [1, 1, 1], selfillum: [0.123456, 0, 0], alpha: 0.5 }], lo: [0, 0, 0], hi: [1, 1, 1] });
const material = (m) => [m.ambient, m.diffuse, m.selfillum];
const withoutMaterial = (m) => Object.assign({}, m, { ambient: null, diffuse: null, selfillum: null });
check("tint: read, defaulted, refused on a shape and out of range", () => {
  assert.deepStrictEqual(F.readTint(undefined, "p"), [1, 1, 1]);
  const given = [0.5, 1, 0.25], read = F.readTint(given, "p");
  assert.deepStrictEqual(read, [0.5, 1, 0.25]);
  assert.ok(read !== given, "readTint's own copy");
  for (const bad of [[1, 1], [1.2, 0, 0], [-0.1, 0, 0], ["1", 0, 0], [NaN, 0, 0], [0, 0, 0, 1], "red", {}, 0.5]) {
    assert.throws(() => F.readTint(bad, "p"), (e) => e.message === "p.tint: [r, g, b], each 0 to 1", JSON.stringify(bad));
  }
  // A part's: readPart gives it (white without one), each error naming the part.
  const part = (extra) => F.readPart(Object.assign({ model: "plc_a03", at: [0, 0, 0] }, extra), "stages.small[0]", BASE);
  assert.deepStrictEqual(part().tint, [1, 1, 1]);
  assert.deepStrictEqual(part({ tint: [0.5, 1, 0.25] }).tint, [0.5, 1, 0.25]);
  assert.deepStrictEqual(F.readPart({ file: "parts/x.mdl", at: [0, 0, 0], tint: [0, 0.5, 1] }, "p", BASE).tint, [0, 0.5, 1]);
  assert.throws(() => part({ tint: [2, 0, 0] }), (e) => e.message === "stages.small[0].tint: [r, g, b], each 0 to 1");
  assert.deepStrictEqual(F.readComposition(comp([{ model: "plc_a03", at: [0, 0, 0], tint: [0.5, 1, 0.5] }]), BASE).stages.small[0].tint, [0.5, 1, 0.5]);
  // A shape takes none, white or not: its Colour is its colour.
  for (const tint of [[0.5, 1, 0.25], [1, 1, 1], [3, 0, 0]]) {
    assert.throws(() => F.readPart({ shape: { type: "dome", radius: 1 }, at: [0, 0, 0], tint }, "stages.small[0]", BASE),
      (e) => e.message === "stages.small[0].tint: a shape has no tint - its Colour is its colour");
  }
  assert.deepStrictEqual(F.readPart({ shape: { type: "dome", radius: 1 }, at: [0, 0, 0] }, "p", BASE).tint, [1, 1, 1]);
});
check("tintMeshes multiplies ambient, diffuse and selfillum; white changes nothing", () => {
  const meshes = tinted().meshes, out = F.tintMeshes(meshes, [0.5, 1, 0.25]);
  assert.deepStrictEqual(out.map(material), [[[0.1, 0.4, 0.25], [0.4, 0.6, 0.1], [0, 0.5, 0.25]], [[0.5, 1, 0.25], [0.5, 1, 0.25], [0.06173, 0, 0]]]);
  // Through round (0.123456 x 0.5 is 0.061728: 0.06173); the rest of each mesh as it was - its alpha and its hint too;
  // copies, the meshes given untouched.
  assert.deepStrictEqual(out.map(withoutMaterial), meshes.map(withoutMaterial));
  assert.ok(out.every((m, i) => m !== meshes[i]));
  assert.deepStrictEqual(meshes.map(material), tinted().meshes.map(material));
  // White (or none): the same numbers, unrounded - a model's own 0.5882352941 kept as the writers have it today.
  const own = [Object.assign(tinted().meshes[0], { diffuse: [0.5882352941, 0.1, 1] })];
  for (const white of [[1, 1, 1], undefined]) assert.deepStrictEqual(F.tintMeshes(own, white), own);
  // A channel at 1 keeps its numbers; a mesh without a material keeps none.
  assert.deepStrictEqual(F.tintMeshes(own, [1, 0.5, 1])[0].diffuse, [0.5882352941, 0.05, 1]);
  assert.deepStrictEqual(F.tintMeshes([{ name: "bare" }], [0.5, 0.5, 0.5]), [{ name: "bare" }]);
});
check("placePart tints every mesh of a tinted part", () => {
  const place = { at: [0.1, 0, 0], turn: 30, scale: 0.5, tilt: [0, 0] };
  const plain = F.placePart(place, tinted(), 0, 0.9, "j"), tint = F.placePart(Object.assign({ tint: [0.5, 1, 0.25] }, place), tinted(), 0, 0.9, "j");
  assert.strictEqual(tint.meshes.length, 2);
  assert.deepStrictEqual(tint.meshes, F.tintMeshes(plain.meshes, [0.5, 1, 0.25]));
  assert.deepStrictEqual([tint.lo, tint.hi, tint.reach], [plain.lo, plain.hi, plain.reach]);
  assert.deepStrictEqual(tint.meshes.map((m) => m.transparencyhint), [2, undefined]);  // T1b's hint kept
  // readPart's part: white without a tint, its own with one - the part as both generators place it.
  const read = (extra) => F.readPart(Object.assign({ model: "plc_a03", at: [0.1, 0, 0], turn: 30, scale: 0.5 }, extra), "p", BASE);
  assert.deepStrictEqual(F.placePart(read(), tinted(), 0, 0.9, "j"), plain);
  assert.deepStrictEqual(F.placePart(read({ tint: [0.5, 1, 0.25] }), tinted(), 0, 0.9, "j"), tint);
  // Lowered and named unique (the generators' next steps): still tinted.
  assert.deepStrictEqual(F.uniqueNames(F.lowered(tint.meshes, 0.9), []).map(material), tint.meshes.map(material));
});
check("compositionText writes tint after colours, never white", () => {
  const text = F.compositionText({ stages: { small: [
    { tint: [0.5, 1, 0.25], extra: 1, colours: { metal1: 5 }, model: "helm_001", at: [0, 0, 0], tilt: [0, 90] },
    { tint: [1, 1, 1], model: "plc_a03", at: [0, 0, 0] }] } });
  const l = text.split("\n");
  assert.ok(l.includes('      { "model": "helm_001", "at": [0, 0, 0], "tilt": [0, 90], "colours": { "metal1": 5 }, "tint": [0.5, 1, 0.25], "extra": 1 },'), text);
  assert.ok(l.includes('      { "model": "plc_a03", "at": [0, 0, 0] }'), text);
  assert.deepStrictEqual(JSON.parse(text).stages.small.map((p) => p.tint), [[0.5, 1, 0.25], undefined]);
});
check("a library texture: Glow 1 gives selfillum = Colour and no diffuse or ambient; Glow 0.4 splits 0.4 / 0.6", () => {
  const lib = (o, type = "dome") => F.shapeModel(F.readShape(Object.assign({ type, radius: 1, texture: "fxpa_symbl_003", colour: [1, 0.5, 0] }, o), "s"));
  for (const type of F.SHAPE_TYPES) {
    assert.deepStrictEqual(material(lib({}, type).meshes[0]), [[0, 0, 0], [0, 0, 0], [1, 0.5, 0]], type + ", Glow 1");
    assert.deepStrictEqual(material(lib({ glow: 0.4 }, type).meshes[0]), [[0.6, 0.3, 0], [0.6, 0.3, 0], [0.4, 0.2, 0]], type + ", Glow 0.4");
    assert.deepStrictEqual(material(lib({ glow: 0 }, type).meshes[0]), [[1, 0.5, 0], [1, 0.5, 0], [0, 0, 0]], type + ", Glow 0");
  }
  // Each through round, no -0; the rest as before: its name, see-through, its alpha, nothing generated.
  const m = lib({ glow: 0.35, colour: [0.123, 0.5, 1] }).meshes[0];
  assert.deepStrictEqual(material(m), [[0.07995, 0.325, 0.65], [0.07995, 0.325, 0.65], [0.04305, 0.175, 0.35]]);
  noNegativeZero(lib({}));
  const g = lib({ glow: 0.4, alpha: 0.5 });
  assert.deepStrictEqual([g.meshes[0].bitmap, g.meshes[0].alpha, g.meshes[0].transparent, g.meshes[0].transparencyhint, g.textures], ["fxpa_symbl_003", 0.5, true, 1, []]);
  // What the model writer writes: Glow 1's diffuse 0 0 0 and selfillumcolor its Colour.
  const node = block(F.pileModelText("plc_t", lib({}).meshes, [0, 0, 0], [1, 1, 1], []), "node trimesh dome");
  assert.ok(node.includes("  ambient 0 0 0") && node.includes("  diffuse 0 0 0") && node.includes("  selfillumcolor 1 0.5 0"), node.slice(0, 20).join("\n"));
});
check("Soft and None keep their material", () => {
  // Today's numbers: Soft's white lit by its glow (its colour is in its texture); None's its colour, lit by its glow.
  const of = (o) => material(F.shapeModel(F.readShape(Object.assign({ type: "dome", radius: 1, colour: [1, 0.5, 0], glow: 0.4 }, o), "s")).meshes[0]);
  assert.deepStrictEqual(of({}), [[1, 1, 1], [1, 1, 1], [0.4, 0.4, 0.4]]);
  assert.deepStrictEqual(of({ texture: "none" }), [[1, 0.5, 0], [1, 0.5, 0], [0.4, 0.2, 0]]);
  assert.deepStrictEqual(of({ glow: 1 }), [[1, 1, 1], [1, 1, 1], [1, 1, 1]]);
  assert.deepStrictEqual(of({ texture: "none", glow: 1 }), [[1, 0.5, 0], [1, 0.5, 0], [1, 0.5, 0]]);
});
// M1's review: a spell mesh's meshes become a draft's planes, and the game's model writer (nwn5e-ee's WriteVfxModel,
// src/data/vfx_model.cpp; tests/data/vfx_model_test.cpp pins it) writes a plane whose ambient and diffuse are both the
// draft's defaults - 0.2 and 0.8, each as C's %g of a float writes it - as "ambient 1 1 1" / "diffuse 1 1 1": the game
// draws such a plane at full, never at 0.8. drawnMaterial is that rule; on a spell mesh a tint multiplies what the game
// draws, so such a mesh is lifted to full before its tint (placePart's `planes`). Untinted it's left as it is (its draft's
// bytes as before: the game lifts it). A placeable's model is written as it is (meshLines): never lifted.
const PLAIN = { name: "plain", bitmap: "fxpa_fern", verts: [-0.1, 0, 0, 0.1, 0, 0, -0.1, 0, 0.2, 0.1, 0, 0.2], tverts: [0, 0, 1, 0, 0, 1, 1, 1], faces: [0, 1, 2, 2, 1, 3],
  diffuse: [0.8, 0.8, 0.8], ambient: [0.2, 0.2, 0.2], selfillum: [0, 0, 0], alpha: 1, transparencyhint: 1 };
check("drawnMaterial: a plane at the draft's defaults (ambient 0.2, diffuse 0.8, as %g writes them) is drawn full, as WriteVfxModel writes it; any other as it is", () => {
  const full = F.drawnMaterial(PLAIN);
  assert.deepStrictEqual(material(full), [[1, 1, 1], [1, 1, 1], [0, 0, 0]]);
  assert.deepStrictEqual(Object.assign({}, full, { ambient: PLAIN.ambient, diffuse: PLAIN.diffuse }), PLAIN);  // the rest kept, its glow too
  assert.ok(full !== PLAIN && PLAIN.diffuse[0] === 0.8, "a copy; the plane given untouched");
  // As %g of a float: 0.2000001 and 0.79999995 are written 0.2 and 0.8 - the defaults; 0.2001 isn't.
  assert.deepStrictEqual(material(F.drawnMaterial(Object.assign({}, PLAIN, { ambient: [0.2000001, 0.2, 0.2], diffuse: [0.8, 0.79999995, 0.8] }))).slice(0, 2), [[1, 1, 1], [1, 1, 1]]);
  // Either of its own: as it is (the C++'s `own`), a glow of its own changes nothing.
  for (const own of [{ ambient: [0.2001, 0.2, 0.2] }, { diffuse: [0.8, 0.8, 0.7] }, { ambient: [1, 1, 1], diffuse: [1, 1, 1] }, { ambient: [0.1, 0.2, 0.3], diffuse: [0.5, 0.25, 1] }]) {
    const p = Object.assign({}, PLAIN, own);
    assert.deepStrictEqual(F.drawnMaterial(p), p, JSON.stringify(own));
  }
  assert.deepStrictEqual(material(F.drawnMaterial(Object.assign({}, PLAIN, { selfillum: [0, 1, 0.5] }))), [[1, 1, 1], [1, 1, 1], [0, 1, 0.5]]);
  // A mesh without them has the defaults (VfxPlane's): drawn full.
  assert.deepStrictEqual(material(F.drawnMaterial({ name: "bare" })).slice(0, 2), [[1, 1, 1], [1, 1, 1]]);
});
check("placePart for a spell mesh's planes: a default-material mesh lifted to full before its tint; untinted as it is; a placeable's never", () => {
  const model = { meshes: [PLAIN, Object.assign({}, PLAIN, { name: "own", ambient: [0.1, 0.2, 0.3], diffuse: [0.5, 0.25, 1] })], lo: [-0.1, 0, 0], hi: [0.1, 0, 0.2] };
  const place = (tint, planes) => F.placePart(Object.assign({ at: [0, 0, 0], turn: 0, scale: 1, tilt: [0, 0] }, tint ? { tint } : {}), model, 0, 0, "j", planes);
  const TINT = [1, 1, 0.999];
  // A spell mesh tinted: the plain mesh the game's 1 1 1 times the tint (not 0.2 0.2 0.1998, which the game would draw
  // as it is - darker than untinted); one of its own material times the tint, as before.
  assert.deepStrictEqual(place(TINT, true).meshes.map(material), [[[1, 1, 0.999], [1, 1, 0.999], [0, 0, 0]], [[0.1, 0.2, 0.2997], [0.5, 0.25, 0.999], [0, 0, 0]]]);
  // Untinted: as it is, a spell mesh's or a placeable's - the drafts and models written today keep their bytes.
  assert.deepStrictEqual(place(null, true), place(null, false));
  assert.deepStrictEqual(place(null, true).meshes[0].ambient, [0.2, 0.2, 0.2]);
  assert.deepStrictEqual(place([1, 1, 1], true), place(null, false));
  // A placeable's (or no `planes`): its material times the tint, as meshLines writes it and the game draws it.
  assert.deepStrictEqual(place(TINT, false).meshes[0].ambient, [0.2, 0.2, 0.1998]);
  assert.deepStrictEqual(place(TINT).meshes, place(TINT, false).meshes);
  assert.deepStrictEqual(place(TINT, true).meshes.map((m) => m.transparencyhint), [1, 1]);
});

// --- M1b: a shape's Blend (the design's addendum (b); the owner, 2026-11-18: "fxpa_globe works. it's faint, you can see
// the sparkles" - "go with your rec on the blend") ---------------------------------------------------------------------
// Additive: its soft texture gets a TXI saying "blending additive" beside it, so the game draws it on its additive path
// (no 0.2 alpha cut, nothing behind hidden); Normal is today's. A shape saved without it keeps Normal.
check("blend: additive or normal, normal by default, written only when additive; anything else refused, naming the field", () => {
  const { label, choices, title } = F.SHAPE_INFO.blend;
  assert.deepStrictEqual({ label, choices }, { label: "Blend", choices: [["additive", "Additive: glows, adds light, faint and see-through as the game's globes"],
    ["normal", "Normal: blended over what's behind"]] });
  assert.ok(/Soft only: a library texture follows its own TXI/.test(title), title);
  assert.ok(F.LOOK_FIELDS.includes("blend") && F.SHAPE_KEYS[F.SHAPE_KEYS.length - 1] === "blend" && F.SHAPE_KEYS.indexOf("blend") === F.SHAPE_KEYS.indexOf("texture") + 1,
    JSON.stringify(F.SHAPE_KEYS));
  for (const type of F.SHAPE_TYPES) {
    assert.strictEqual(F.shapeDefaults(type, 1).blend, "normal", type);
    assert.strictEqual(F.readShape({ type, radius: 1 }, "s").blend, "normal", type);
    assert.strictEqual(F.readShape({ type, radius: 1, blend: "additive" }, "s").blend, "additive", type);
  }
  // Written only when additive, after its texture (SHAPE_KEYS' order); normal - the default - never.
  const add = F.cleanShape(F.readShape({ type: "dome", radius: 1, blend: "additive", colour: [1, 0.85, 0.4] }, "s"));
  assert.deepStrictEqual(add, { type: "dome", radius: 1, colour: [1, 0.85, 0.4], blend: "additive" });
  assert.deepStrictEqual(Object.keys(F.cleanShape({ type: "dome", radius: 1, texture: "none", blend: "additive" })), ["type", "radius", "texture", "blend"]);
  assert.deepStrictEqual(F.cleanShape(F.readShape({ type: "dome", radius: 1, blend: "normal" }, "s")), { type: "dome", radius: 1 });
  // A library texture or none keeps it (it means nothing there - the texture's own TXI, or no texture - but Soft again
  // brings it back).
  assert.deepStrictEqual(F.cleanShape(F.readShape({ type: "dome", radius: 1, texture: "fxpa_globe", blend: "additive" }, "s")),
    { type: "dome", radius: 1, texture: "fxpa_globe", blend: "additive" });
  for (const blend of ["Additive", "add", "", "lighten", 1, null, true]) {
    assert.throws(() => F.readShape({ type: "dome", radius: 1, blend }, "s"), (e) => e.message === "s.blend: additive or normal", JSON.stringify(blend));
  }
  // In a part's key (the shape as written) and a composition's line.
  const key = (shape) => F.partKey(F.readPart({ shape, at: [0, 0, 0] }, "p", BASE));
  assert.strictEqual(key({ type: "dome", radius: 1, blend: "additive" }), 'shape:{ "type": "dome", "radius": 1, "blend": "additive" }');
  assert.strictEqual(key({ type: "dome", radius: 1, blend: "normal" }), key({ type: "dome", radius: 1 }));
  const text = F.compositionText({ stages: { small: [{ shape: { type: "dome", radius: 1, blend: "additive" }, at: [0, 0, 0] }] } });
  assert.ok(text.split("\n").includes('      { "shape": { "type": "dome", "radius": 1, "blend": "additive" }, "at": [0, 0, 0] }'), text);
});
const ADDITIVE = (colour) => F.shapeTextureName({ colour, blend: "additive" });
check("a soft texture's name hashes its blend: Additive's differs from Normal's, Normal's (and none given) as before", () => {
  for (const colour of [[0.6, 0.8, 1], [1, 0.85, 0.4]]) {
    const normal = F.shapeTextureName({ colour }), additive = ADDITIVE(colour);
    assert.strictEqual(F.shapeTextureName({ colour, blend: "normal" }), normal);
    assert.ok(F.SHAPE_TEXTURE.test(additive), additive);
    assert.notStrictEqual(additive, normal);
    assert.strictEqual(ADDITIVE(colour.map((c) => c - 0.0001)), additive, "to 0.001, as the colour");
  }
  assert.strictEqual(F.shapeTextureName({ colour: [0.6, 0.8, 1] }), "n5s_8c757fb28f99");
  assert.strictEqual(F.shapeTextureName({ colour: [1, 0.85, 0.4], blend: "normal" }), "n5s_a3caa0709dda");
  // Additive's names, the default colour's and gold's (the build checks and the bench's use them).
  assert.deepStrictEqual([ADDITIVE([0.6, 0.8, 1]), ADDITIVE([1, 0.85, 0.4])], ["n5s_617b220c1144", "n5s_438a32a94ebf"]);
  // Its pixels are its colour's: the same picture whichever its blend (the TXI says how the game draws it).
  assert.ok(F.tgaBytes(F.shapeTexture({ colour: [1, 0.85, 0.4], blend: "additive" }), { alpha: true }).equals(F.tgaBytes(F.shapeTexture({ colour: [1, 0.85, 0.4] }), { alpha: true })));
});
check("shapeModel: a soft-additive shape's mesh carries additive and its texture its blend; Normal, None and a library texture never additive", () => {
  const model = (o) => F.shapeModel(F.readShape(Object.assign({ type: "dome", radius: 1, colour: [1, 0.85, 0.4] }, o), "s"));
  const add = model({ blend: "additive" }), m = add.meshes[0];
  assert.strictEqual(m.bitmap, ADDITIVE([1, 0.85, 0.4]));
  assert.strictEqual(m.additive, true);
  assert.deepStrictEqual(add.textures, [{ name: ADDITIVE([1, 0.85, 0.4]), colour: [1, 0.85, 0.4], blend: "additive" }]);
  // Its material, alpha and hint as a normal soft shape's: only its texture's name and the flag differ.
  const plain = model({});
  assert.deepStrictEqual(Object.assign({}, m, { bitmap: plain.meshes[0].bitmap, additive: undefined }), Object.assign({}, plain.meshes[0], { additive: undefined }));
  assert.ok(!("additive" in plain.meshes[0]), "a normal mesh carries no additive");
  assert.deepStrictEqual(plain.textures, [{ name: "n5s_a3caa0709dda", colour: [1, 0.85, 0.4], blend: "normal" }]);
  for (const texture of ["none", "fxpa_globe"]) {
    const x = model({ blend: "additive", texture });
    assert.ok(!("additive" in x.meshes[0]), texture + "'s mesh carries additive");
    assert.deepStrictEqual(x.textures, [], texture);
  }
  // Placed (placePart: a part's meshes, its tint over them), lowered: it stays additive.
  const placed = F.placePart({ at: [0, 0, 0], turn: 30, scale: 1, tilt: [0, 0], tint: [1, 1, 1] }, add, 0, 0.9, "j", true);
  assert.strictEqual(placed.meshes[0].additive, true);
  assert.strictEqual(F.lowered(placed.meshes, 0.9)[0].additive, true);
  noNegativeZero(add);
});
check("the TXI: SOFT_TXI says blending additive; txiAdditive reads a TXI's blending line as the game's keywords (any case, either line end)", () => {
  assert.strictEqual(F.SOFT_TXI, "blending additive\n");
  for (const t of ["blending additive\n", "blending additive\r\n", "Blending Additive", "  blending\tadditive  \r\n", "mipmap 0\r\nblending additive\r\ndecal 1\r\n",
    "proceduretype arturo\nblending additive\n"]) assert.strictEqual(F.txiAdditive(t), true, JSON.stringify(t));
  for (const t of ["", null, undefined, "blending punchthrough\n", "blending additivex", "# blending additive", "decal 1", "blendingadditive", "envmaptexture x\n"]) {
    assert.strictEqual(F.txiAdditive(t), false, JSON.stringify(t));
  }
  assert.strictEqual(F.txiAdditive(Buffer.from("blending additive\r\n", "latin1")), true);
});
check("files.js: an additive soft texture is written with its TXI beside it (both owned by name); a normal one alone", () => {
  const fs = require("fs");
  const os = require("os");
  const W = require("./files.js");
  const part = (shape) => F.readPart({ shape, at: [0, 0, 0] }, "p", BASE);
  const colour = [1, 0.85, 0.4], add = ADDITIVE(colour);
  const placed = [{ part: part({ type: "dome", radius: 1, colour }) }, { part: part({ type: "dome", radius: 2, colour, blend: "additive" }) },
    { part: part({ type: "sphere", radius: 1, colour, blend: "additive" }) }];
  const made = W.shapeTextures(placed);
  assert.deepStrictEqual([...made.keys()], ["n5s_a3caa0709dda", add]);
  assert.deepStrictEqual([made.get("n5s_a3caa0709dda").blend, made.get(add).blend], ["normal", "additive"]);
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "n5_fdtxi-"));
  try {
    const writes = W.shapeWrites(made, dir);
    assert.deepStrictEqual(writes.map((w) => [path.basename(w.file), w.kind]), [["n5s_a3caa0709dda.tga", "shape"], [add + ".tga", "shape"], [add + ".txi", "shape"]]);
    const tga = F.tgaBytes(F.shapeTexture({ colour }), { alpha: true });
    assert.ok(writes[0].bytes.equals(tga) && writes[1].bytes.equals(tga), "its TGA: the colour's soft texture");
    assert.ok(Buffer.isBuffer(writes[2].bytes) && writes[2].bytes.toString("latin1") === "blending additive\n", JSON.stringify(String(writes[2].bytes)));
    // Ours by name, the TXI as its TGA: rewritten when it differs; a TXI not of a soft texture's name isn't.
    const owned = W.ownership({ repo: dir, manifest: new Set(), mark: F.MARK });
    fs.writeFileSync(path.join(dir, add + ".txi"), "blending punchthrough\n");
    for (const name of ["n5s_mine.txi", "n5t_mark01.txi"]) assert.strictEqual(owned({ file: path.join(dir, name), kind: "shape" }, Buffer.from("x")), false, name);
    assert.strictEqual(owned({ file: path.join(dir, add + ".txi"), kind: "shape" }, Buffer.from("x")), true);
    assert.deepStrictEqual(W.writeAll(writes, owned, { repo: dir, manifest: new Set() }).map(([s, f]) => [s, path.basename(f)]),
      [["new", "n5s_a3caa0709dda.tga"], ["new", add + ".tga"], ["rebuilt", add + ".txi"]]);
    assert.strictEqual(fs.readFileSync(path.join(dir, add + ".txi"), "latin1"), "blending additive\n");
    // What the summary and --json say: Normal as before; Additive its blend and its TXI.
    assert.deepStrictEqual(W.shapeJson(made), [{ texture: "n5s_a3caa0709dda", colour }, { texture: add, colour, blend: "additive" }]);
    assert.deepStrictEqual(W.shapeLines(made, "data/files"), [`  texture n5s_a3caa0709dda: a shape's soft texture, colour 1 0.85 0.4 - written into data/files`,
      `  texture ${add}: a shape's soft texture, colour 1 0.85 0.4, additive (${add}.txi beside it: blending additive) - written into data/files`]);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});
check("Blend in a page: the same name, model and TXI", () => {
  const P = pageFloatDisk();
  const s = { type: "dome", radius: 2, colour: [1, 0.85, 0.4], blend: "additive" };
  assert.deepStrictEqual(P.shapeModel(P.readShape(s, "s")), F.shapeModel(F.readShape(s, "s")));
  assert.strictEqual(P.shapeTextureName(s), ADDITIVE([1, 0.85, 0.4]));
  assert.strictEqual(P.SOFT_TXI, F.SOFT_TXI);
  assert.strictEqual(P.txiAdditive("blending additive"), true);
});

// --- M2: textures that move (the design's "Texture motion"; the owner, 2026-11-18: "for the animated textures: 1.
// scrolling, scaling (breathing), rotating" - "the textures on the meshes"; Q1: route A) --------------------------------
// A part's "motion": { scroll: [u, v] (repeats a second), turn (degrees a second), breathe (0-0.9: how much bigger and
// smaller), rate (breaths a second) }. NWN:EE has no UV-motion setting, so a moving texture reaches the game as an EE
// material, n5u_<hash>.mtr - the mesh's bitmap, named for its texture, motion and pivot - driving our vertex shader,
// vsn5uvmove.shd, with the game's lit fragment shader. One maths (uvMotion) for the shader and the stage.
const STILL = { scroll: [0, 0], turn: 0, breathe: 0, rate: 0.5 };
const applyUv = (mx, uv) => [mx[0] * uv[0] + mx[1] * uv[1] + mx[2], mx[3] * uv[0] + mx[4] * uv[1] + mx[5]];
check("motion: read, defaulted, cleaned to its non-zero keys, each error", () => {
  assert.deepStrictEqual(F.MOTION_KEYS, ["scroll", "turn", "breathe", "rate"]);
  assert.deepStrictEqual(Object.keys(F.MOTION_INFO), F.MOTION_KEYS);
  const I = F.MOTION_INFO;
  assert.deepStrictEqual([I.scroll.min, I.scroll.max, I.turn.min, I.turn.max, I.breathe.min, I.breathe.max, I.rate.min, I.rate.max], [-10, 10, -720, 720, 0, 0.9, 0.05, 5]);
  // The card's labels, as the design's sketch has them.
  assert.deepStrictEqual([I.scroll.labels, I.turn.label, I.breathe.label, I.rate.label], [["Scroll round", "Scroll up"], "Turn (°/s)", "Breathe", "Breaths (/s)"]);
  // None, or keys left out: all 0, rate 0.5.
  for (const none of [undefined, null, {}, { turn: undefined }]) assert.deepStrictEqual(F.readMotion(none, "p"), STILL, JSON.stringify(none));
  const given = { scroll: [0, 0.25], breathe: 0.3 }, read = F.readMotion(given, "p");
  assert.deepStrictEqual(read, { scroll: [0, 0.25], turn: 0, breathe: 0.3, rate: 0.5 });
  assert.ok(read.scroll !== given.scroll, "readMotion's own copy");
  assert.deepStrictEqual(F.readMotion({ scroll: [-10, 10], turn: -720, breathe: 0.9, rate: 5 }, "p"), { scroll: [-10, 10], turn: -720, breathe: 0.9, rate: 5 });
  assert.deepStrictEqual(F.readMotion({ turn: 720, rate: 0.05 }, "p"), { scroll: [0, 0], turn: 720, breathe: 0, rate: 0.05 });
  // It moves when it scrolls, turns or breathes; a rate alone moves nothing.
  for (const m of [undefined, null, {}, STILL, { rate: 2 }, { scroll: [0, 0] }, "fast", 3]) assert.strictEqual(F.hasMotion(m), false, JSON.stringify(m));
  for (const m of [{ turn: 1 }, { scroll: [0.01, 0] }, { scroll: [0, -1] }, { breathe: 0.1 }]) assert.strictEqual(F.hasMotion(m), true, JSON.stringify(m));
  // Cleaned: its non-zero keys in MOTION_KEYS' order - the rate only while it breathes (nothing without) and not at its
  // default, 0.5 (a default is never written) - or undefined when it doesn't move.
  for (const m of [undefined, null, {}, STILL, { rate: 2 }]) assert.strictEqual(F.cleanMotion(m), undefined, JSON.stringify(m));
  assert.deepStrictEqual(F.cleanMotion({ scroll: [0, 0.25], turn: 0, breathe: 0, rate: 0.5 }), { scroll: [0, 0.25] });
  assert.deepStrictEqual(F.cleanMotion({ rate: 2, turn: 30, scroll: [0, 0] }), { turn: 30 });
  assert.deepStrictEqual(Object.keys(F.cleanMotion({ rate: 1, breathe: 0.3, turn: -5, scroll: [1, 0] })), ["scroll", "turn", "breathe", "rate"]);
  assert.deepStrictEqual(F.cleanMotion(F.readMotion({ breathe: 0.3 }, "p")), { breathe: 0.3 });
  assert.deepStrictEqual(F.cleanMotion({ breathe: 0.3, rate: 0.5 }), { breathe: 0.3 });
  assert.deepStrictEqual(F.cleanMotion({ breathe: 0.3, rate: 1 }), { breathe: 0.3, rate: 1 });
  // Each error names its field.
  const says = (m, message) => assert.throws(() => F.readMotion(m, "stages.small[0]"), (e) => e.message === message, JSON.stringify(m));
  for (const m of ["fast", 3, [0, 0.25], true]) says(m, 'stages.small[0].motion: { "scroll": [u, v], "turn": <degrees a second>, "breathe": 0-0.9, "rate": <breaths a second> }');
  says({ spin: 1 }, "stages.small[0].motion.spin: not a motion's - scroll, turn, breathe or rate");
  for (const scroll of [[0], [0, 0, 0], [0, 11], [-10.5, 0], ["0", 0], [NaN, 0], 0.25, null]) {
    says({ scroll }, "stages.small[0].motion.scroll: [u, v], each a number from -10 to 10 (repeats a second)");
  }
  for (const turn of [721, -721, "30", NaN, Infinity, null]) says({ turn }, "stages.small[0].motion.turn: a number from -720 to 720 (degrees a second)");
  for (const breathe of [-0.1, 0.91, "0.2", null]) says({ breathe }, "stages.small[0].motion.breathe: a number from 0 to 0.9 (how much bigger and smaller it gets)");
  for (const rate of [0, 0.04, 5.1, "1", null]) says({ rate }, "stages.small[0].motion.rate: a number from 0.05 to 5 (breaths a second)");
});
check("uvMotion: none is identity; scroll [0, 0.25] at t 2 moves v by -0.5; turn 90 at t 1 maps p + (1, 0) to p + (0, -1) (the picture turned counter-clockwise); breathe 0.5 rate 1 at t 0.25 divides uv - p by 1.5", () => {
  const p = [0.5, 0.5];
  for (const none of [undefined, STILL, { rate: 3 }]) for (const t of [0, 1.7, 100]) assert.deepStrictEqual(F.uvMotion(none, p, t), [1, 0, 0, 0, 1, 0]);
  // u' = a u + b v + c, v' = d u + e v + f. Scroll: the picture moves along +u / +v, so the UVs read go back.
  const s = F.uvMotion({ scroll: [0, 0.25] }, p, 2);
  nearAll(s, [1, 0, 0, 0, 1, -0.5], 1e-12);
  nearAll(applyUv(s, [0.3, 0.9]), [0.3, 0.4], 1e-12);
  nearAll(applyUv(F.uvMotion({ scroll: [0.5, 0] }, p, 3), [0, 0]), [-1.5, 0], 1e-12);
  // Turn, round the pivot: + is counter-clockwise (the picture), so the UV read at p + (1, 0) is p + (0, -1).
  const pv = [1.5, 1], tm = F.uvMotion({ turn: 90 }, pv, 1);
  nearAll(applyUv(tm, [2.5, 1]), [1.5, 0], 1e-12);
  nearAll(applyUv(tm, [1.5, 2]), [2.5, 1], 1e-12);
  nearAll(applyUv(tm, pv), pv, 1e-12);
  nearAll(applyUv(F.uvMotion({ turn: -90 }, pv, 1), [2.5, 1]), [1.5, 2], 1e-12);
  nearAll(applyUv(F.uvMotion({ turn: 30 }, pv, 3), [2.5, 1]), [1.5, 0], 1e-12);  // 30 a second for 3 s: 90
  // Breathe, round the pivot: s = 1 + breathe sin(2 pi rate t) - the picture grows by s (the UVs read shrink by it).
  const b = F.uvMotion({ breathe: 0.5, rate: 1 }, p, 0.25);
  nearAll(b, [1 / 1.5, 0, 0.5 - 0.5 / 1.5, 0, 1 / 1.5, 0.5 - 0.5 / 1.5], 1e-12);
  nearAll(applyUv(b, [2, -1]), [0.5 + 1.5 / 1.5, 0.5 - 1.5 / 1.5], 1e-12);
  nearAll(applyUv(F.uvMotion({ breathe: 0.5, rate: 1 }, p, 0.75), [0.6, 0.5]), [0.7, 0.5], 1e-12);  // s 0.5
  // All at once: uv' = R(-turn t) (uv - p) / s + p - scroll t.
  const all = { scroll: [0.1, -0.2], turn: 45, breathe: 0.2, rate: 0.5 }, t = 1.3, uv = [0.9, 0.2];
  const sc = 1 + 0.2 * Math.sin(2 * Math.PI * 0.5 * t), th = (-45 * t * Math.PI) / 180, d = [uv[0] - 0.5, uv[1] - 0.5];
  nearAll(applyUv(F.uvMotion(all, p, t), uv), [(Math.cos(th) * d[0] - Math.sin(th) * d[1]) / sc + 0.5 - 0.1 * t, (Math.sin(th) * d[0] + Math.cos(th) * d[1]) / sc + 0.5 + 0.2 * t], 1e-12);
  // Never -0 (a matrix through JSON is the same matrix).
  for (const m of [{ turn: 30 }, { scroll: [0, 0.25] }, all]) assert.ok(!F.uvMotion(m, p, 0).some((x) => Object.is(x, -0)), JSON.stringify(m));
});
check("uvMotion moves by its material's numbers - each to 0.001, the pivot too - so the stage and the game can't drift", () => {
  // The material carries the motion and the pivot to 0.001 (motionNumbers); the stage's maths takes the same numbers.
  nearAll(F.uvMotion({ scroll: [0, 0.12345] }, [0.5, 0.5], 1), [1, 0, 0, 0, 1, -0.123], 1e-12);
  for (const t of [0.7, 13, 2879.5]) {
    assert.deepStrictEqual(F.uvMotion({ turn: 30.0004, breathe: 0.30049, rate: 0.50001 }, [0.33333, 1.00049], t), F.uvMotion({ turn: 30, breathe: 0.3, rate: 0.5 }, [0.333, 1], t));
    assert.deepStrictEqual(F.uvMotion({ scroll: [0.1234, -2.0004] }, [0.5, 0.5], t), F.uvMotion({ scroll: [0.123, -2] }, [0.5, 0.5], t));
  }
  // What the material says is what the stage moves by.
  const m = { scroll: [0.0123456, 0], turn: -17.77777, breathe: 0.123456, rate: 1.23456 }, pivot = [0.123456, 0.654321];
  const params = Object.fromEntries(F.motionMaterialText({ texture: "x", motion: m, pivot }).split("\n").filter((l) => l.startsWith("parameter float "))
    .map((l) => l.split(" ").slice(2)));
  const said = { scroll: [Number(params.n5ScrollU), Number(params.n5ScrollV)], turn: Number(params.n5Turn), breathe: Number(params.n5Breathe), rate: Number(params.n5Rate) };
  assert.deepStrictEqual(F.uvMotion(m, pivot, 3.3), F.uvMotion(said, [Number(params.n5PivotU), Number(params.n5PivotV)], 3.3));
});
check("uvPivot: a Wrapped dome with repeats 3 / 2 gives [1.5, 1]; From above gives [0.5, 0.5]", () => {
  assert.deepStrictEqual(F.uvPivot(domeModel({ repeat_around: 3, repeat_up: 2 }).meshes[0].tverts), [1.5, 1]);
  assert.deepStrictEqual(F.uvPivot(domeModel({ mapping: "above" }).meshes[0].tverts), [0.5, 0.5]);
  assert.deepStrictEqual(F.uvPivot(domeModel().meshes[0].tverts), [0.5, 0.5]);
  // The middle of the UV box (not their mean), through round; none: the picture's middle; never -0.
  assert.deepStrictEqual(F.uvPivot([0.2, -1, 0.8, 3, 0.3, 0]), [0.5, 1]);
  assert.deepStrictEqual(F.uvPivot([0, 0, 1 / 3, 1 / 3]), [0.16667, 0.16667]);
  assert.deepStrictEqual(F.uvPivot([]), [0.5, 0.5]);
  assert.deepStrictEqual(F.uvPivot(undefined), [0.5, 0.5]);
  assert.ok(!F.uvPivot([-1, -1, 1, 1]).some((x) => Object.is(x, -0)));
});
check("motionName: n5u_ and 12 hex, the same for the same texture, motion and pivot, another for each change", () => {
  const m = { scroll: [0, 0.25] }, p = [0.5, 0.5], name = F.motionName("fxpa_globe", m, p);
  assert.ok(/^n5u_[0-9a-f]{12}$/.test(name) && F.MOTION_TEXTURE.test(name), name);
  assert.ok(!F.MOTION_TEXTURE.test("n5u_" + "0".repeat(11)) && !F.MOTION_TEXTURE.test("n5s_8c757fb28f99") && !F.SHAPE_TEXTURE.test(name) && !F.BAKED.test(name));
  // The same: read or as given, the texture in any case, each number to 0.001 (one rule with its material's text), a rate
  // that means nothing (no breathing).
  for (const [t, mm, pp] of [["fxpa_globe", F.readMotion(m, "p"), p], ["FXPA_Globe", m, p], ["fxpa_globe", { scroll: [0.0001, 0.2502] }, [0.5001, 0.4999]],
    ["fxpa_globe", { scroll: [0, 0.25], rate: 3 }, p]]) assert.strictEqual(F.motionName(t, mm, pp), name, JSON.stringify([t, mm, pp]));
  // Another for each change.
  const others = [["fxpa_globe2", m, p], ["fxpa_globe", { scroll: [0.25, 0] }, p], ["fxpa_globe", { scroll: [0, 0.251] }, p], ["fxpa_globe", { scroll: [0, 0.25], turn: 1 }, p],
    ["fxpa_globe", { scroll: [0, 0.25], breathe: 0.1 }, p], ["fxpa_globe", { scroll: [0, 0.25], breathe: 0.1, rate: 1 }, p], ["fxpa_globe", m, [0.5, 1]],
    ["fxpa_globe", m, [1.5, 0.5]], ["fxpa_globe", { scroll: [0, -0.25] }, p], ["fxpa_globe", { turn: 30 }, p], ["fxpa_globe", { turn: -30 }, p]];
  const names = others.map((a) => F.motionName(...a));
  assert.strictEqual(new Set([name, ...names]).size, others.length + 1, names.join(" "));
  assert.strictEqual(name, "n5u_31e9f07948e1");  // the recipe pinned: cyrb53 of "uv:fxpa_globe:0,0.25,0,0,0.5,0.5,0.5"
});
check("motionMaterialText: its lines, texture0 left out with copy, every MOTION_PARAMS name once", () => {
  assert.deepStrictEqual(F.MOTION_PARAMS, ["n5ScrollU", "n5ScrollV", "n5Turn", "n5Breathe", "n5Rate", "n5PivotU", "n5PivotV"]);
  assert.strictEqual(F.MOTION_SHADER, "vsn5uvmove");
  const m = { texture: "fxpa_globe", motion: { scroll: [0, 0.25], turn: -30, breathe: 0.3, rate: 0.5 }, pivot: [1.5, 1] };
  const text = F.motionMaterialText(m);
  assert.strictEqual(text, ["customshadervs vsn5uvmove", "customshaderfs fslit", "texture0 fxpa_globe", "parameter float n5ScrollU 0",
    "parameter float n5ScrollV 0.25", "parameter float n5Turn -30", "parameter float n5Breathe 0.3", "parameter float n5Rate 0.5",
    "parameter float n5PivotU 1.5", "parameter float n5PivotV 1", ""].join("\n"));
  const copy = F.motionMaterialText(m, { copy: true });
  assert.strictEqual(copy, text.replace("texture0 fxpa_globe\n", ""));
  assert.strictEqual(F.motionMaterialText(m, {}), text);
  for (const t of [text, copy]) {
    assert.ok(!t.includes("\r"), "LF line ends");
    for (const name of F.MOTION_PARAMS) assert.strictEqual(t.split("\n").filter((l) => l.split(" ")[2] === name).length, 1, name);
  }
  // Its numbers as its name has them (to 0.001, the rate its default while it doesn't breathe): the name fixes the bytes.
  const fine = F.motionMaterialText({ texture: "FXPA_Globe", motion: { scroll: [-0.00001, 0.12345], turn: 0, rate: 3 }, pivot: [0.5, 0.33333] });
  for (const l of ["texture0 fxpa_globe", "parameter float n5ScrollU 0", "parameter float n5ScrollV 0.123", "parameter float n5Rate 0.5", "parameter float n5PivotV 0.333"]) {
    assert.ok(fine.split("\n").includes(l), l + "\n" + fine);
  }
  assert.throws(() => F.motionMaterialText({ texture: "x", motion: { turn: 999 }, pivot: [0.5, 0.5] }), /motion\.turn: a number from -720 to 720/);
});
check("motionShaderText: declares every MOTION_PARAMS name, calls ApplyStandardShader before the UVs, reads worldtimerTimeOfDay, has no backtick; its main moves a UV as uvMotion does, to whole repeats", () => {
  const text = F.motionShaderText(), l = text.split("\n");
  assert.ok(/^\/\/ .*nwn5e.*#39/.test(l[0]), "its first line, a comment naming nwn5e and #39: " + l[0]);
  assert.ok(!text.includes("`") && !text.includes("\r"), "a backtick, or not LF");
  const at = (line) => { const i = l.indexOf(line); assert.ok(i >= 0, "no line: " + line); return i; };
  const include = at('#include "inc_standard"');
  for (const d of ["SHADER_TYPE 1", "FOG 1", "KEYHOLING 1", "NORMAL_MAP 0", "ENVIRONMENT_MAP 0", "LIGHTING 1"]) assert.ok(at("#define " + d) < include, d + " after the include");
  for (const name of F.MOTION_PARAMS) {
    assert.strictEqual(l.filter((x) => x === `uniform float ${name};`).length, 1, name + " declared once");
    assert.ok(at(`uniform float ${name};`) > include);
  }
  const main = text.slice(text.indexOf("void main"));
  const standard = main.indexOf("ApplyStandardShader();"), moved = main.indexOf("vVertexTexCoords =");
  assert.ok(standard > 0 && moved > standard, "ApplyStandardShader() before the UVs are moved");
  assert.ok(main.includes("float(worldtimerTimeOfDay) * 0.001"), "t from the world clock");
  for (const name of F.MOTION_PARAMS) assert.ok(main.includes(name), name + " not used");
  assert.strictEqual((text.match(/\{/g) || []).length, (text.match(/\}/g) || []).length);
  // Ours: its first line says so (files.js's ownership), any other text isn't.
  assert.ok(F.isMotionShader(text) && F.isMotionShader(Buffer.from(F.crlf(text), "latin1")) && F.isMotionShader(l[0] + "\n"));
  for (const other of ["", "// vslit.shd\n", "#define SHADER_TYPE 1\n", null]) assert.strictEqual(F.isMotionShader(other), false, JSON.stringify(other));
  // Its main run as JS (GLSL's float, sin, cos, fract, mod and radians; vec2 as { x, y }): each UV where uvMotion puts it,
  // give or take whole repeats (it wraps the scroll, the turn and the breath for a float's precision late in a game day).
  const body = main.slice(standard + "ApplyStandardShader();".length, main.lastIndexOf("}")).replace(/\bfloat (?=\w+ =)/g, "let ");
  const run = new Function("worldtimerTimeOfDay", ...F.MOTION_PARAMS, "uv", "float", "sin", "cos", "fract", "mod", "radians", "vec2",
    "let vVertexTexCoords = uv;" + body + "; return vVertexTexCoords;");
  const glsl = [(v) => v, Math.sin, Math.cos, (v) => v - Math.floor(v), (a, b) => a - b * Math.floor(a / b), (d) => (d * Math.PI) / 180, (x, y) => ({ x, y })];
  for (const motion of [{ scroll: [0, 0.25] }, { turn: 30 }, { breathe: 0.3, rate: 0.5 }, { scroll: [-1.7, 3.2], turn: -125, breathe: 0.9, rate: 4.5 }]) {
    const r = F.readMotion(motion, "m"), pivot = [1.5, 0.5];
    for (const ms of [0, 250, 1300, 61234, 2879999]) {
      for (const uv of [[0, 0], [0.9, 0.2], [3, 1]]) {
        const got = run(ms, r.scroll[0], r.scroll[1], r.turn, r.breathe, r.rate, pivot[0], pivot[1], { x: uv[0], y: uv[1] }, ...glsl);
        const want = applyUv(F.uvMotion(r, pivot, ms * 0.001), uv);
        for (const [g, w] of [[got.x, want[0]], [got.y, want[1]]]) assert.ok(Math.abs(g - w - Math.round(g - w)) < 1e-6, `${JSON.stringify(motion)} at ${ms} ms: ${g} for ${w}`);
      }
    }
  }
});
check("moveTextures: renames textured meshes, leaves untextured ones, one material a name", () => {
  const mesh = (name, bitmap, tverts) => ({ name, bitmap, verts: [0, 0, 0, 1, 0, 0, 0, 1, 0], tverts, faces: [0, 1, 2], diffuse: [1, 1, 1], ambient: [1, 1, 1],
    selfillum: [0, 0, 0], alpha: 0.5, transparencyhint: 1 });
  const meshes = [mesh("a", "fxpa_globe", [0, 0, 1, 0, 0, 1]), mesh("b", "", [0, 0, 1, 0, 0, 1]), mesh("c", "fxpa_globe", [0, 0, 1, 1, 0.5, 0.5]),
    mesh("d", "fxpa_globe", [0, 0, 3, 0, 0, 2]), mesh("e", "plc_a03", [0, 0, 1, 0, 0, 1]), mesh("f", "null", [0, 0, 1, 0, 0, 1]),
    mesh("g", "n5u_0123456789ab", [0, 0, 1, 0, 0, 1])];
  const kept = JSON.parse(JSON.stringify(meshes));
  const motion = F.readMotion({ turn: 30 }, "p"), out = F.moveTextures(meshes, motion);
  const a = F.motionName("fxpa_globe", motion, [0.5, 0.5]), d = F.motionName("fxpa_globe", motion, [1.5, 1]), e = F.motionName("plc_a03", motion, [0.5, 0.5]);
  // Each textured mesh's bitmap is its material's name (its texture, the motion, its UV box's middle); untextured ones,
  // and one naming a moving texture's material already, as they were.
  assert.deepStrictEqual(out.meshes.map((m) => m.bitmap), [a, "", a, d, e, "null", "n5u_0123456789ab"]);
  assert.deepStrictEqual(out.materials, [{ name: a, texture: "fxpa_globe", motion, pivot: [0.5, 0.5] }, { name: d, texture: "fxpa_globe", motion, pivot: [1.5, 1] },
    { name: e, texture: "plc_a03", motion, pivot: [0.5, 0.5] }]);
  // Copies, all else of each mesh kept; the meshes given untouched.
  assert.deepStrictEqual(out.meshes.map((m) => Object.assign({}, m, { bitmap: null })), meshes.map((m) => Object.assign({}, m, { bitmap: null })));
  assert.ok(out.meshes.every((m, i) => m !== meshes[i]));
  assert.deepStrictEqual(meshes, kept);
  // A motion as given is read (the same materials); no motion: the same meshes, no materials.
  assert.deepStrictEqual(F.moveTextures(meshes, { turn: 30 }), out);
  for (const none of [undefined, null, STILL, { rate: 2 }]) {
    const still = F.moveTextures(meshes, none);
    assert.ok(still.meshes === meshes && Array.isArray(still.materials) && still.materials.length === 0, JSON.stringify(none));
  }
  // A placed shape part (placePart: its tverts as the generators write them), then moved: its pivot from those.
  const dome = F.readPart({ shape: { type: "dome", radius: 1, repeat_around: 3, repeat_up: 2 }, at: [0, 0, 0], motion: { scroll: [0, 0.25] } }, "p", BASE);
  const placed = F.placePart(dome, F.shapeModel(dome.source.shape), 0, 0, "j", true);
  const moved = F.moveTextures(placed.meshes, dome.motion);
  assert.deepStrictEqual(moved.materials.map((x) => [x.texture, x.pivot]), [["n5s_8c757fb28f99", [1.5, 1]]]);
  assert.strictEqual(moved.meshes[0].bitmap, F.motionName("n5s_8c757fb28f99", { scroll: [0, 0.25] }, [1.5, 1]));
});
check("compositionText and readPart carry motion", () => {
  // readPart: none is still; a model's, a file's and a shape's read; each error naming its part.
  assert.deepStrictEqual(F.readPart({ model: "plc_a03", at: [0, 0, 0] }, "p", BASE).motion, STILL);
  assert.deepStrictEqual(F.readPart({ model: "plc_a03", at: [0, 0, 0], motion: { turn: 30 } }, "p", BASE).motion, { scroll: [0, 0], turn: 30, breathe: 0, rate: 0.5 });
  assert.deepStrictEqual(F.readPart({ file: "parts/x.mdl", at: [0, 0, 0], motion: { scroll: [0, 0.25] } }, "p", BASE).motion.scroll, [0, 0.25]);
  assert.deepStrictEqual(F.readPart({ shape: { type: "dome", radius: 1 }, at: [0, 0, 0], motion: { breathe: 0.3 } }, "p", BASE).motion, { scroll: [0, 0], turn: 0, breathe: 0.3, rate: 0.5 });
  assert.throws(() => F.readPart({ model: "plc_a03", at: [0, 0, 0], motion: { turn: 800 } }, "stages.small[1]", BASE),
    (e) => e.message === "stages.small[1].motion.turn: a number from -720 to 720 (degrees a second)");
  assert.deepStrictEqual(F.readComposition(comp([{ model: "plc_a03", at: [0, 0, 0], motion: { turn: -45 } }]), BASE).stages.small[0].motion.turn, -45);
  // The same model read once, whatever its motion (partKey is its source's).
  assert.strictEqual(F.partKey(F.readPart({ model: "plc_a03", at: [0, 0, 0], motion: { turn: 30 } }, "p", BASE)), "model:plc_a03");
  // compositionText: after tint, inline, its non-zero keys (cleanMotion's); never a still one.
  const text = F.compositionText({ stages: { small: [
    { motion: { scroll: [0, 0.25], turn: 0, breathe: 0, rate: 0.5 }, extra: 1, tint: [0.5, 1, 0.25], model: "plc_a03", at: [0, 0, 0] },
    { motion: STILL, model: "plc_a03", at: [0, 0, 0] },
    { shape: { type: "dome", radius: 1 }, at: [0, 0, 0], motion: { breathe: 0.3, rate: 0.5, turn: 10 } }] } });
  const l = text.split("\n");
  assert.ok(l.includes('      { "model": "plc_a03", "at": [0, 0, 0], "tint": [0.5, 1, 0.25], "motion": { "scroll": [0, 0.25] }, "extra": 1 },'), text);
  assert.ok(l.includes('      { "model": "plc_a03", "at": [0, 0, 0] },'), text);
  assert.ok(l.includes('      { "shape": { "type": "dome", "radius": 1 }, "at": [0, 0, 0], "motion": { "turn": 10, "breathe": 0.3 } }'), text);
  // Read back: the same motions.
  assert.deepStrictEqual(F.readComposition(comp(JSON.parse(text).stages.small), BASE).stages.small.map((p) => p.motion),
    [{ scroll: [0, 0.25], turn: 0, breathe: 0, rate: 0.5 }, STILL, { scroll: [0, 0], turn: 10, breathe: 0.3, rate: 0.5 }]);
});
check("files.js: a moving part's materials and the shader as writes beside what names them, owned by the n5u_ name and the shader's mark; still parts none", () => {
  const fs = require("fs");
  const os = require("os");
  const W = require("./files.js");
  const part = (o) => F.readPart(Object.assign({ shape: { type: "dome", radius: 1, mapping: "above", texture: "fxpa_globe" }, at: [0, 0, 0] }, o), "p", BASE);
  const placed = [{ part: part({ motion: { turn: 30 } }) }, { part: part({}) }, { part: part({ motion: { turn: 30 } }) }, { part: part({ motion: { scroll: [0, 0.25] } }) }];
  for (const x of placed) x.meshes = F.placePart(x.part, F.shapeModel(x.part.source.shape), 0, 0, "j", true).meshes;
  const materials = W.motionMaterials(placed);
  const turn = F.motionName("fxpa_globe", { turn: 30 }, [0.5, 0.5]), scroll = F.motionName("fxpa_globe", { scroll: [0, 0.25] }, [0.5, 0.5]);
  // Each moving part's meshes renamed in place (x.meshes), the still one's kept; each material once, in the parts' order.
  assert.deepStrictEqual(placed.map((x) => x.meshes[0].bitmap), [turn, "fxpa_globe", turn, scroll]);
  assert.deepStrictEqual([...materials.keys()], [turn, scroll]);
  assert.deepStrictEqual(Object.assign({}, materials.get(turn), { motion: null }), { texture: "fxpa_globe", motion: null, pivot: [0.5, 0.5],
    text: F.motionMaterialText({ texture: "fxpa_globe", motion: { turn: 30 }, pivot: [0.5, 0.5] }), label: "dome 1 m" });
  assert.deepStrictEqual(W.motionJson(materials), [{ material: turn, texture: "fxpa_globe", motion: { turn: 30 }, pivot: [0.5, 0.5] },
    { material: scroll, texture: "fxpa_globe", motion: { scroll: [0, 0.25] }, pivot: [0.5, 0.5] }]);
  assert.deepStrictEqual(W.motionLines(materials, "data/files"), [
    `  material ${turn}: fxpa_globe turns 30 degrees a second, round 0.5 0.5 - written into data/files, with vsn5uvmove.shd`,
    `  material ${scroll}: fxpa_globe scrolls 0 0.25 a second, round 0.5 0.5 - written into data/files, with vsn5uvmove.shd`]);
  assert.strictEqual(W.motionMaterials([{ part: part({}), meshes: [] }]).size, 0);
  assert.deepStrictEqual(W.motionWrites(new Map(), "x"), [], "no material: no shader either");
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "n5_fduv-"));
  try {
    const writes = W.motionWrites(materials, dir);
    assert.deepStrictEqual(writes.map((w) => [path.basename(w.file), w.kind]), [[turn + ".mtr", "material"], [scroll + ".mtr", "material"], ["vsn5uvmove.shd", "shader"]]);
    assert.strictEqual(writes[0].bytes.toString("latin1"), materials.get(turn).text);
    assert.ok(writes[2].bytes.equals(Buffer.from(F.crlf(F.motionShaderText()), "latin1")), "the shader CRLF, as the generators write text");
    // Ours: a material (or a copy-route texture or TXI) by its n5u_ name; the shader when made.txt lists it or it carries
    // the shader's first line (an earlier version's too) - another file of that name isn't.
    const owned = W.ownership({ repo: dir, manifest: new Set(["listed/vsn5uvmove.shd"]), mark: F.MARK });
    for (const ext of [".mtr", ".tga", ".dds", ".txi"]) assert.strictEqual(owned({ file: path.join(dir, turn + ext), kind: "material" }, Buffer.from("x")), true, ext);
    for (const name of ["n5u_mine.mtr", "tti01_iceclear.mtr", turn + ".shd"]) assert.strictEqual(owned({ file: path.join(dir, name), kind: "material" }, Buffer.from("x")), false, name);
    const shader = (file, old) => owned({ file: path.join(dir, file), kind: "shader" }, Buffer.from(old, "latin1"));
    assert.strictEqual(shader("vsn5uvmove.shd", F.MOTION_SHADER_MARK + "\r\n// an earlier one\r\n"), true);
    assert.strictEqual(shader("vsn5uvmove.shd", "// someone's shader\r\n"), false);
    assert.strictEqual(shader("listed/vsn5uvmove.shd", "// someone's shader, but made.txt lists it\r\n"), true);
    fs.writeFileSync(path.join(dir, turn + ".mtr"), "customshadervs vslit\n");
    fs.writeFileSync(path.join(dir, "vsn5uvmove.shd"), F.MOTION_SHADER_MARK + "\r\n// an earlier one\r\n");
    assert.deepStrictEqual(W.writeAll(writes, owned, { repo: dir, manifest: new Set() }).map(([s, f]) => [s, path.basename(f)]),
      [["rebuilt", turn + ".mtr"], ["new", scroll + ".mtr"], ["rebuilt", "vsn5uvmove.shd"]]);
    // The shader as text, as "text" is: the same but for its line ends (an LF checkout's) is unchanged, not rewritten.
    fs.writeFileSync(path.join(dir, "vsn5uvmove.shd"), F.motionShaderText());
    const lf = fs.statSync(path.join(dir, "vsn5uvmove.shd")).mtimeMs;
    assert.deepStrictEqual(W.writeAll(writes, owned, { repo: dir, manifest: new Set() }).map(([s, f]) => [s, path.basename(f)]),
      [["unchanged", turn + ".mtr"], ["unchanged", scroll + ".mtr"], ["unchanged", "vsn5uvmove.shd"]]);
    assert.strictEqual(fs.statSync(path.join(dir, "vsn5uvmove.shd")).mtimeMs, lf, "the LF shader was rewritten");
    fs.writeFileSync(path.join(dir, "vsn5uvmove.shd"), "// someone's shader\r\n");
    assert.throws(() => W.writeAll(writes, owned, { repo: dir, manifest: new Set() }), /^Error: vsn5uvmove\.shd is there and wasn't made by this generator/);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});
check("motion in a page: the same maths, names, material and shader", () => {
  const P = pageFloatDisk();
  for (const k of ["MOTION_KEYS", "MOTION_INFO", "MOTION_PARAMS", "MOTION_SHADER"]) assert.deepStrictEqual(P[k], F[k], k);
  assert.strictEqual(String(P.MOTION_TEXTURE), String(F.MOTION_TEXTURE));
  const m = { scroll: [0.1, -0.2], turn: 45, breathe: 0.2, rate: 0.5 };
  assert.deepStrictEqual(P.readMotion(m, "p"), F.readMotion(m, "p"));
  assert.deepStrictEqual(P.cleanMotion(m), F.cleanMotion(m));
  for (const t of [0, 1.3, 77]) assert.deepStrictEqual(P.uvMotion(m, [1.5, 1], t), F.uvMotion(m, [1.5, 1], t));
  const tverts = domeModel({ repeat_around: 3, repeat_up: 2 }).meshes[0].tverts;
  assert.deepStrictEqual(P.uvPivot(tverts), F.uvPivot(tverts));
  assert.strictEqual(P.motionName("fxpa_globe", m, [1.5, 1]), F.motionName("fxpa_globe", m, [1.5, 1]));
  assert.strictEqual(P.motionMaterialText({ texture: "fxpa_globe", motion: m, pivot: [1.5, 1] }), F.motionMaterialText({ texture: "fxpa_globe", motion: m, pivot: [1.5, 1] }));
  assert.strictEqual(P.motionShaderText(), F.motionShaderText());
  assert.strictEqual(P.isMotionShader(F.motionShaderText()), true);
  const model = domeModel({ mapping: "above" });
  assert.deepStrictEqual(P.moveTextures(model.meshes, m), F.moveTextures(model.meshes, m));
});

// --- W1: a breathing texture's edges clamped (the design's addendum (c); TMO-2, the owner: "when it shrank the texture
// being tiled and wrapping would show a bit at the bottom edge of the dome" - "go with 1 for the wrap") ----------------
// A moving texture that breathes and doesn't scroll, on a mesh whose UVs stay within one repeat, is clamped: its picture
// copied under its n5u_ name with a TXI of the source's lines and the game's clamp line, the material naming the copy
// (TMO-1's copy route) - its edge pixels stretch instead of the next repeat showing when the picture shrinks.
const ABOVE = () => domeModel({ mapping: "above" }).meshes[0].tverts;
check("W1 the rule: clamped when it breathes, doesn't scroll and its UVs stay within one repeat; else it repeats", () => {
  const above = ABOVE(), wrapped = domeModel().meshes[0].tverts;
  assert.ok(above.every((t) => t >= 0 && t <= 1) && wrapped.every((t) => t >= 0 && t <= 1), "the domes' UVs within 0..1");
  // Breathing, no scroll, UVs within one repeat (From above; Wrapped at Repeat 1 / 1): clamped - a turn as well, any rate.
  for (const m of [{ breathe: 0.2 }, { turn: 30, breathe: 0.2 }, { breathe: 0.9, rate: 3 }, F.readMotion({ breathe: 0.1, turn: -5 }, "m")]) {
    assert.strictEqual(F.motionClamps(m, above), true, JSON.stringify(m));
    assert.strictEqual(F.motionClamps(m, wrapped), true, "Wrapped " + JSON.stringify(m));
  }
  // A scroll (either way), no breath, or no motion: it repeats (a scroll needs the next repeat).
  for (const m of [{ scroll: [0, 0.25], breathe: 0.2 }, { scroll: [-0.1, 0], breathe: 0.2 }, { turn: 30 }, { scroll: [0, 0.25] }, {}, undefined, STILL]) {
    assert.strictEqual(F.motionClamps(m, above), false, JSON.stringify(m));
  }
  // The numbers as the material takes them (motionNumbers, to 0.001): a breath that rounds to 0 doesn't breathe, a
  // scroll that rounds to 0 doesn't scroll.
  assert.strictEqual(F.motionClamps({ breathe: 0.0004 }, above), false);
  assert.strictEqual(F.motionClamps({ breathe: 0.2, scroll: [0.0004, -0.0004] }, above), true);
  // UVs past one repeat: Repeat around / up above 1, a tiled game part, UVs below 0 - it repeats; within a small
  // epsilon (0.001) of 0..1 still one repeat.
  for (const o of [{ repeat_around: 2 }, { repeat_up: 2 }]) assert.strictEqual(F.motionClamps({ breathe: 0.2 }, domeModel(o).meshes[0].tverts), false, JSON.stringify(o));
  for (const t of [[0, 0, 3, 0, 0, 1], [-0.5, 0, 1, 1], [0, 0, 1, 1.002], [-0.002, 0.5, 1, 1]]) assert.strictEqual(F.motionClamps({ breathe: 0.2 }, t), false, JSON.stringify(t));
  for (const t of [[0, 0, 1, 1], [-0.0009, 0.5, 1.0009, 1], [0.25, 0.25, 0.75, 0.75]]) assert.strictEqual(F.motionClamps({ breathe: 0.2 }, t), true, JSON.stringify(t));
  // No UVs: nothing known within a repeat - it repeats.
  for (const t of [[], undefined, null]) assert.strictEqual(F.motionClamps({ breathe: 0.2 }, t), false, JSON.stringify(t));
  assert.throws(() => F.motionClamps({ breathe: 2 }, above), /motion\.breathe: a number from 0 to 0\.9/);
});
check("W1 the name: the clamp is hashed in - a clamped and an unclamped motion never share a name; unclamped names as before", () => {
  const m = { breathe: 0.2, turn: 30 }, p = [0.5, 0.5];
  const open = F.motionName("fxpa_symbl_003", m, p), shut = F.motionName("fxpa_symbl_003", m, p, true);
  assert.ok(F.MOTION_TEXTURE.test(shut), shut);
  assert.notStrictEqual(shut, open);
  assert.strictEqual(F.motionName("fxpa_symbl_003", m, p, false), open);
  assert.strictEqual(F.motionName("fxpa_globe", { scroll: [0, 0.25] }, p), "n5u_31e9f07948e1", "an unclamped name changed (M2's pin)");
  // The same clamped name for the same numbers (as the material takes them), another for each change.
  assert.strictEqual(F.motionName("FXPA_Symbl_003", { breathe: 0.20004, turn: 30, rate: 0.5 }, [0.5001, 0.4999], true), shut);
  const others = [["fxpa_symbl_002", m, p], ["fxpa_symbl_003", { breathe: 0.3, turn: 30 }, p], ["fxpa_symbl_003", m, [0.5, 0.6]]].map((a) => F.motionName(...a, true));
  assert.strictEqual(new Set([open, shut, ...others]).size, 5);
  assert.strictEqual(shut, "n5u_bfb2c9b5a9bf", "the clamped recipe pinned: cyrb53 of \"uv:fxpa_symbl_003:0,0,30,0.2,0.5,0.5,0.5:clamp\"");
});
check("W1 the TXI: the source's lines and the game's clamp line (clamp 3: 10 of the game's 15 clamp TXIs); txiClamps reads it", () => {
  assert.strictEqual(F.CLAMP_TXI, "clamp 3");
  // fxpa_symbl_003's own TXI (decal, additive; CRLF, no last line end): its lines, then the clamp line, in its line ends -
  // (DC) its decal line dropped: a decal mesh is drawn unlit, its material skipped (the SHP-8 diagnosis), and our material
  // would read whatever colour the mesh before it left.
  const sym = "decal 1\r\nblending additive\r\ndownsamplemax 0\r\ndownsamplemin 0", symLines = "blending additive\r\ndownsamplemax 0\r\ndownsamplemin 0";
  assert.strictEqual(F.clampedTxi(sym), symLines + "\r\nclamp 3\r\n");
  assert.strictEqual(F.clampedTxi(Buffer.from(sym, "latin1")), symLines + "\r\nclamp 3\r\n");
  // A soft additive texture's (SOFT_TXI, LF); none at all.
  assert.strictEqual(F.clampedTxi(F.SOFT_TXI), "blending additive\nclamp 3\n");
  for (const none of ["", null, undefined, Buffer.alloc(0)]) assert.strictEqual(F.clampedTxi(none), "clamp 3\n", JSON.stringify(none));
  // A flipbook's lines kept in order (its procedure) - but its decal (DC); a clamp line of its own replaced by ours; blank
  // lines at its end dropped, those within kept (fxpa_globe's channelscale numbers run over lines).
  const cycle = "proceduretype cycle\r\nnumx 4\r\nnumy 4\r\nfps 32\r\nblending additive\r\ndecal 1\r\n\r\ndownsamplemax 0\r\ndownsamplemin 0";
  assert.strictEqual(F.clampedTxi(cycle), cycle.replace("decal 1\r\n", "") + "\r\nclamp 3\r\n");
  assert.strictEqual(F.clampedTxi("mipmap 0\r\nclamp 1\r\nfilter 0\r\n\r\n\r\n"), "mipmap 0\r\nfilter 0\r\nclamp 3\r\n");
  assert.strictEqual(F.clampedTxi("channelscale 4\n1\n\t\n1\n\ndownsamplemax 0\n\n"), "channelscale 4\n1\n\t\n1\n\ndownsamplemax 0\nclamp 3\n");
  // Whether a TXI clamps: a "clamp" line with a value other than 0, any case, either line end.
  for (const t of ["clamp 3", "Clamp 1\r\n", "mipmap 0\r\n\tclamp 2", F.clampedTxi(sym), Buffer.from("clamp 3\n")]) assert.strictEqual(F.txiClamps(t), true, JSON.stringify(String(t)));
  for (const t of ["clamp 0", "", null, undefined, "noclamp 3", "# clamp 3", sym, "clampx 3"]) assert.strictEqual(F.txiClamps(t), false, JSON.stringify(t));
});
check("W1 the material: a clamped one names the copy (no texture0) - the copy route TMO-1 proved; an unclamped one texture0 as before", () => {
  const m = { texture: "fxpa_symbl_003", motion: { breathe: 0.2, turn: 30 }, pivot: [0.5, 0.5] };
  const open = F.motionMaterialText(m), shut = F.motionMaterialText(Object.assign({ clamp: true }, m));
  assert.ok(open.split("\n").includes("texture0 fxpa_symbl_003"), open);
  assert.strictEqual(shut, F.motionMaterialText(m, { copy: true }));
  assert.ok(!/texture0/.test(shut), shut);
  assert.strictEqual(F.motionMaterialText(Object.assign({ clamp: false }, m)), open);
});
check("W1 moveTextures: a breathing From-above dome's material is clamped (its own name); a scrolling one, a tiled mesh, or { clamp: false } repeat", () => {
  const mesh = (name, tverts) => ({ name, bitmap: "fxpa_symbl_003", verts: [0, 0, 0, 1, 0, 0, 0, 1, 0], tverts, faces: [0, 1, 2], diffuse: [1, 1, 1],
    ambient: [1, 1, 1], selfillum: [0, 0, 0], alpha: 0.5, transparencyhint: 1 });
  const one = mesh("a", [0, 0, 1, 0, 0, 1]), tiled = mesh("b", [0, 0, 3, 0, 0, 1]), half = mesh("c", [0, 0, 1, 0, 0, 1]);
  const breathe = F.readMotion({ breathe: 0.2, turn: 30 }, "p");
  const out = F.moveTextures([one, tiled, half], breathe);
  const shut = F.motionName("fxpa_symbl_003", breathe, [0.5, 0.5], true), open = F.motionName("fxpa_symbl_003", breathe, [1.5, 0.5]);
  assert.deepStrictEqual(out.meshes.map((m) => m.bitmap), [shut, open, shut]);
  // The clamped material says so (clamp: true); an unclamped one as before - no clamp key.
  assert.deepStrictEqual(out.materials, [{ name: shut, texture: "fxpa_symbl_003", motion: breathe, pivot: [0.5, 0.5], clamp: true },
    { name: open, texture: "fxpa_symbl_003", motion: breathe, pivot: [1.5, 0.5] }]);
  // Scrolling: every material repeats, named as before.
  const scroll = F.readMotion({ scroll: [0, 0.25], breathe: 0.2 }, "p"), s = F.moveTextures([one], scroll);
  assert.deepStrictEqual(s.materials, [{ name: F.motionName("fxpa_symbl_003", scroll, [0.5, 0.5]), texture: "fxpa_symbl_003", motion: scroll, pivot: [0.5, 0.5] }]);
  // { clamp: false }: the rule off - every moving texture repeats (uvprobe: TMO-1's domes as they were written).
  const off = F.moveTextures([one, tiled], breathe, { clamp: false });
  assert.deepStrictEqual(off.materials.map((x) => [x.name, "clamp" in x]), [[F.motionName("fxpa_symbl_003", breathe, [0.5, 0.5]), false], [open, false]]);
  // A placed From-above shape part (TMO-2's dome: fxpa_symbl_003, Turn 30, Breathe 0.2): clamped.
  const dome = F.readPart({ shape: { type: "dome", radius: 1, mapping: "above", texture: "fxpa_symbl_003", colour: [1, 0.502, 0] }, at: [0, 0, 0],
    motion: { turn: 30, breathe: 0.2 } }, "p", BASE);
  const placed = F.placePart(dome, F.shapeModel(dome.source.shape), 0, 0, "j", true), moved = F.moveTextures(placed.meshes, dome.motion);
  assert.deepStrictEqual(moved.materials.map((x) => [x.texture, x.pivot, x.clamp]), [["fxpa_symbl_003", [0.5, 0.5], true]]);
});
check("W1 files.js: a clamped material's copy - its picture under its name and a TXI of the source's lines + clamp 3 - written beside its .mtr; an unclamped one none", () => {
  const fs = require("fs");
  const os = require("os");
  const W = require("./files.js");
  const part = (shape, motion) => F.readPart({ shape: Object.assign({ type: "dome", radius: 1, mapping: "above" }, shape), at: [0, 0, 0], motion }, "p", BASE);
  const placed = [{ part: part({ texture: "fxpa_symbl_003" }, { breathe: 0.2, turn: 30 }) }, { part: part({ texture: "fxpa_symbl_003" }, { scroll: [0, 0.25] }) },
    { part: part({ blend: "additive" }, { breathe: 0.3 }) }, { part: part({ texture: "only_dds" }, { breathe: 0.3 }) }, { part: part({ texture: "n5p_0123456789ab" }, { breathe: 0.3 }) }];
  for (const x of placed) x.meshes = F.placePart(x.part, F.shapeModel(x.part.source.shape), 0, 0, "j", true).meshes;
  const shaped = W.shapeTextures(placed);
  const materials = W.motionMaterials(placed);
  const names = placed.map((x) => x.meshes[0].bitmap), soft = [...shaped.keys()][0];
  // Each clamped one marked (clamp: true), its .mtr with no texture0; the scrolling one as before.
  assert.deepStrictEqual(names.map((n) => materials.get(n).clamp === true), [true, false, true, true, true]);
  assert.ok(!/texture0/.test(materials.get(names[0]).text) && /^texture0 fxpa_symbl_003$/m.test(materials.get(names[1]).text));
  // The pictures: the game's (reader.js image: ovr\, then the archives - a DDS when it has one, as the game draws it), a
  // soft one's as made (its TXI too), a bake's; asked for no other.
  const SYM = { tga: Buffer.from("sym-tga"), dds: null, txi: "decal 1\r\nblending additive\r\ndownsamplemax 0\r\ndownsamplemin 0" };
  const asked = [], rd = { image: (name, dirs) => { asked.push([name, dirs]); return name === "fxpa_symbl_003" ? Object.assign({ where: "game", plt: null }, SYM)
    : name === "only_dds" ? { where: "game", tga: Buffer.from("odd-tga"), dds: Buffer.from("odd-dds"), plt: null, txi: "" } : null; } };
  const baked = new Map([["n5p_0123456789ab", { bytes: Buffer.from("baked-tga") }]]);
  W.motionCopies(materials, { rd, shaped, baked });
  assert.deepStrictEqual(asked.map((a) => a[0]), ["fxpa_symbl_003", "only_dds"]);
  const copy = (k) => materials.get(names[k]).copy;
  assert.strictEqual(copy(1), undefined, "a copy for the scrolling one");
  // (DC: the copy's TXI without the source's decal line.)
  const SYM_COPY = "blending additive\r\ndownsamplemax 0\r\ndownsamplemin 0\r\nclamp 3\r\n";
  assert.deepStrictEqual([copy(0).ext, copy(0).bytes.toString(), copy(0).txi.toString("latin1")], [".tga", "sym-tga", SYM_COPY]);
  assert.deepStrictEqual([copy(2).ext, copy(2).txi.toString("latin1")], [".tga", "blending additive\nclamp 3\n"]);
  assert.ok(copy(2).bytes.equals(shaped.get(soft).bytes), "the soft one's copy isn't its TGA");
  assert.deepStrictEqual([copy(3).ext, copy(3).bytes.toString(), copy(3).txi.toString("latin1")], [".dds", "odd-dds", "clamp 3\n"]);
  assert.deepStrictEqual([copy(4).ext, copy(4).bytes.toString(), copy(4).txi.toString("latin1")], [".tga", "baked-tga", "clamp 3\n"]);
  // Writes: each material's .mtr, then (clamped) its picture and TXI under its name, all "material" (owned by the n5u_
  // name); then the shader.
  const dir = path.join(os.tmpdir(), "n5_fdclamp-x");
  const writes = W.motionWrites(materials, dir);
  assert.deepStrictEqual(writes.map((w) => [path.basename(w.file), w.kind]), [
    [names[0] + ".mtr", "material"], [names[0] + ".tga", "material"], [names[0] + ".txi", "material"], [names[1] + ".mtr", "material"],
    [names[2] + ".mtr", "material"], [names[2] + ".tga", "material"], [names[2] + ".txi", "material"],
    [names[3] + ".mtr", "material"], [names[3] + ".dds", "material"], [names[3] + ".txi", "material"],
    [names[4] + ".mtr", "material"], [names[4] + ".tga", "material"], [names[4] + ".txi", "material"], ["vsn5uvmove.shd", "shader"]]);
  assert.strictEqual(writes[2].bytes.toString("latin1"), SYM_COPY);
  // --json's and the summary's: clamp said.
  assert.deepStrictEqual(W.motionJson(materials).map((j) => j.clamp === true), [true, false, true, true, true]);
  assert.ok(!("clamp" in W.motionJson(materials)[1]));
  const lines = W.motionLines(materials, "data/files");
  assert.match(lines[0], new RegExp(`^  material ${names[0]}: fxpa_symbl_003 turns 30 degrees a second, breathes 0\\.2 at 0\\.5 a second, round 0\\.5 0\\.5, clamped: ` +
    `fxpa_symbl_003 copied as ${names[0]}\\.tga with ${names[0]}\\.txi \\(its TXI and clamp 3\\) - written into data/files, with vsn5uvmove\\.shd$`));
  assert.ok(!/clamped/.test(lines[1]), lines[1]);
  // A clamped texture with no picture anywhere: refused, naming the part and the texture.
  const lost = [{ part: part({ texture: "nowhere" }, { breathe: 0.2 }) }];
  lost[0].meshes = F.placePart(lost[0].part, F.shapeModel(lost[0].part.source.shape), 0, 0, "j", true).meshes;
  assert.throws(() => W.motionCopies(W.motionMaterials(lost), { rd }), /^Error: dome 1 m: texture nowhere - its clamped material needs its picture copied, and it's neither in the game nor beside the part/);
  // A texture beside a part's own file (shipTextures' copies): its picture and TXI from there.
  const beside = fs.mkdtempSync(path.join(os.tmpdir(), "n5_fdclamp-"));
  try {
    fs.writeFileSync(path.join(beside, "my_glyph.tga"), "mine");
    fs.writeFileSync(path.join(beside, "my_glyph.txi"), "blending additive\r\n");
    const own = [{ part: part({ texture: "my_glyph" }, { breathe: 0.2 }) }];
    own[0].meshes = F.placePart(own[0].part, F.shapeModel(own[0].part.source.shape), 0, 0, "j", true).meshes;
    const R = require("./reader.js");
    const realRd = R.create({ game: beside, core: null, gameData: { resources: () => () => null }, reader: null });
    const m = W.motionMaterials(own);
    W.motionCopies(m, { rd: realRd, ship: new Map([["my_glyph", [path.join(beside, "my_glyph.tga"), path.join(beside, "my_glyph.txi")]]]) });
    const c = [...m.values()][0].copy;
    assert.deepStrictEqual([c.ext, c.bytes.toString(), c.txi.toString("latin1")], [".tga", "mine", "blending additive\r\nclamp 3\r\n"]);
  } finally {
    fs.rmSync(beside, { recursive: true, force: true });
  }
});
check("W1 in a page: the same rule, name, TXI and moveTextures", () => {
  const P = pageFloatDisk();
  assert.strictEqual(P.CLAMP_TXI, F.CLAMP_TXI);
  const above = ABOVE(), m = { breathe: 0.2, turn: 30 };
  assert.strictEqual(P.motionClamps(m, above), F.motionClamps(m, above));
  assert.strictEqual(P.motionName("fxpa_symbl_003", m, [0.5, 0.5], true), F.motionName("fxpa_symbl_003", m, [0.5, 0.5], true));
  assert.strictEqual(P.clampedTxi("decal 1\r\nblending additive"), F.clampedTxi("decal 1\r\nblending additive"));
  assert.strictEqual(P.txiClamps("clamp 3"), true);
  const model = domeModel({ mapping: "above" });
  assert.deepStrictEqual(P.moveTextures(model.meshes, m), F.moveTextures(model.meshes, m));
});

// --- DC (#39; SHP-8, the owner, 2026-11-19: "Go"): a library texture marked decal copied under our own name without the
// mark, so a shape's Colour shows in game; flipbook frames and soft textures clamped in v, so no thin line shows at a
// mesh's top rim. The engine (the SHP-8 diagnosis): a decal mesh is drawn unlit - texture x alpha, its material skipped;
// a flipbook's frame is a texture of its own, repeating both ways unless its TXI clamps (bit 1 u, 2 v, 4 r). -------------
const FLAME_TXI = "proceduretype cycle\r\nnumx 4\r\nnumy 4\r\nfps 32\r\nblending additive\r\ndecal 1\r\ndownsamplemax 0\r\ndownsamplemin 0\r\n";
const SYM_TXI = "decal 1\r\nblending additive\r\ndownsamplemax 0\r\ndownsamplemin 0\r\n";
// (DC's fix round: a copy's name hashes its key too - copyKey of its source TXI's text and its picture's bytes.)
const DC_KEY = typeof F.copyKey === "function" ? { fxpa_flame02: F.copyKey(FLAME_TXI, Buffer.from("flame-tga")), fxpa_symbl_003: F.copyKey(SYM_TXI, Buffer.from("sym-tga")),
  my_flip: F.copyKey("proceduretype cycle\nnumx 2\nnumy 2\nfps 8\n", Buffer.from("flip-tga")) } : {};
const DC_INFO = { fxpa_flame02: { decal: true, cycle: { numx: 4, numy: 4, fps: 32 }, key: DC_KEY.fxpa_flame02 }, fxpa_symbl_003: { decal: true, cycle: null, key: DC_KEY.fxpa_symbl_003 },
  fxpa_globe: { decal: false, cycle: null }, my_flip: { decal: false, cycle: { numx: 2, numy: 2, fps: 8 }, key: DC_KEY.my_flip } };
const dcInfo = (t) => DC_INFO[t] || null;
const dcName = (t, clamp) => F.copyName(t, clamp, DC_KEY[t]);
// SHP-8's flame ring (the owner's: Cylinder (open), Top radius, fxpa_flame02, a green Colour) and its kin, placed as a
// spell mesh's planes.
const dcShape = (o) => F.readShape(Object.assign({ type: "cylinder", radius: 1, height: 0.6, top: 1.17 }, o), "s");
const dcPlaced = (shape) => F.placePart({ at: [0, 0, 0], turn: 0, scale: 1, tilt: [0, 0], tint: [1, 1, 1] }, F.shapeModel(shape), 0, 0, "j", true).meshes;
check("DC the TXI's words: decal (a value other than 0, as C's atoi reads it), the flipbook (the bench's txiCycle), the clamp bits (every clamp line's together)", () => {
  for (const t of [SYM_TXI, FLAME_TXI, "decal 1", "Decal\t2\r\n", "  decal 1  ", Buffer.from("decal 1\n"), "mipmap 0\ndecal 1x"]) assert.strictEqual(F.txiDecal(t), true, JSON.stringify(String(t)));
  for (const t of ["decal 0", "decal", "decal x", "", null, undefined, "# decal 1", "decalx 1", "blending additive\n", "nodecal 1"]) assert.strictEqual(F.txiDecal(t), false, JSON.stringify(t));
  assert.deepStrictEqual(F.txiCycle(FLAME_TXI), { numx: 4, numy: 4, fps: 32 });
  assert.deepStrictEqual(F.txiCycle(Buffer.from(FLAME_TXI, "latin1")), { numx: 4, numy: 4, fps: 32 });
  assert.deepStrictEqual(F.txiCycle("proceduretype cycle\nnumx 1\nnumy 8\nfps 12\n"), { numx: 1, numy: 8, fps: 12 });  // the game's waterfalls
  assert.deepStrictEqual(F.txiCycle("ProcedureType Cycle\r\nnumx 2\r\n"), { numx: 2, numy: 1, fps: 0 });  // fps left out: its first frame
  assert.deepStrictEqual(F.txiCycle("proceduretype cycle\nnumx 4\nnumx 1\n"), { numx: 4, numy: 1, fps: 0 }, "the first line naming a word");
  for (const t of [SYM_TXI, "proceduretype arturo\nnumx 4\n", "proceduretype cycle\n", "proceduretype cycle\nnumx 1\nnumy 1\n", "proceduretype cycle\nnumx 2.5\n",
    "proceduretype cycle\nnumx 0\nnumy 4\n", "proceduretype cycle\nnumx 4\nfps -1\n", "", null, undefined]) assert.strictEqual(F.txiCycle(t), null, JSON.stringify(t));
  for (const [t, bits] of [["clamp 3", 3], ["clamp 1\r\nmipmap 0\r\nclamp 2\r\n", 3], ["", 0], [null, 0], ["clamp 0", 0], ["clamp x", 0], ["clamp 2", 2], ["Clamp\t4", 4],
    ["# clamp 3", 0], [SYM_TXI, 0]]) assert.strictEqual(F.txiClampBits(t), bits, JSON.stringify(t));
  // W1's txiClamps is the same reading: clamps when the bits aren't 0.
  for (const t of ["clamp 3", "clamp 1\r\n", "clamp 0", "clamp x", "", SYM_TXI]) assert.strictEqual(F.txiClamps(t), F.txiClampBits(t) !== 0, JSON.stringify(t));
  assert.strictEqual(F.CLAMP_V, 2);
});
check("DC copiedTxi: the source's lines as they are but its decal, in its own line ends, then one clamp line - its own and ours together; W1's clampedTxi is clamp 3 through it", () => {
  const flameLines = "proceduretype cycle\r\nnumx 4\r\nnumy 4\r\nfps 32\r\nblending additive\r\ndownsamplemax 0\r\ndownsamplemin 0";
  assert.strictEqual(F.copiedTxi(FLAME_TXI, 2), flameLines + "\r\nclamp 2\r\n");
  assert.strictEqual(F.copiedTxi(FLAME_TXI, 0), flameLines + "\r\n");
  assert.strictEqual(F.copiedTxi(FLAME_TXI), flameLines + "\r\n");
  assert.strictEqual(F.copiedTxi(Buffer.from(FLAME_TXI, "latin1"), 2), flameLines + "\r\nclamp 2\r\n");
  assert.strictEqual(F.copiedTxi(SYM_TXI, 2), "blending additive\r\ndownsamplemax 0\r\ndownsamplemin 0\r\nclamp 2\r\n");
  // Every decal line goes (any value, any case, a bare one too); a clamp line of its own joins ours - one line, the union,
  // at the end; blank lines at its end dropped.
  assert.strictEqual(F.copiedTxi("Decal 0\nmipmap 0\ndecal\nclamp 1\nfilter 0\n\n\n", 2), "mipmap 0\nfilter 0\nclamp 3\n");
  assert.strictEqual(F.copiedTxi("clamp 2\r\n", 2), "clamp 2\r\n");
  assert.strictEqual(F.copiedTxi("decalx 1\n"), "decalx 1\n", "not a decal line");
  // Nothing left: empty with no clamp to add, else the clamp line alone (LF, ours).
  for (const none of ["", null, undefined, "decal 1", Buffer.alloc(0)]) {
    assert.strictEqual(F.copiedTxi(none), "", JSON.stringify(none));
    assert.strictEqual(F.copiedTxi(none, 2), "clamp 2\n", JSON.stringify(none));
  }
  // W1's clamp 3 is the union with the source's own (the game's clamp TXIs say 1, 2 or 3: clamp 3 always, as before).
  for (const s of [SYM_TXI, FLAME_TXI, "clamp 1\r\n", "mipmap 0\nclamp 2", F.SOFT_TXI, ""]) assert.strictEqual(F.clampedTxi(s), F.copiedTxi(s, 3), JSON.stringify(s));
  assert.strictEqual(F.clampedTxi("mipmap 0\r\nclamp 1\r\nfilter 0\r\n"), "mipmap 0\r\nfilter 0\r\nclamp 3\r\n");
  // What the copy says: no decal, the clamp, the flipbook and the blending kept.
  const copy = F.copiedTxi(FLAME_TXI, 2);
  assert.deepStrictEqual([F.txiDecal(copy), F.txiClampBits(copy), F.txiCycle(copy), F.txiAdditive(copy)], [false, 2, { numx: 4, numy: 4, fps: 32 }, true]);
  assert.strictEqual(F.txiDecal(F.clampedTxi(SYM_TXI)), false);
});
check("DC vClamps: every v within one repeat (0 to 1, 0.001 either side) - u may tile; no UVs, no", () => {
  assert.strictEqual(F.vClamps([0, 0, 3, 1, 1.5, 0.5]), true);  // Elemental Shield's ring: u 0 to 3, v 0 to 1
  assert.strictEqual(F.vClamps([0, 0, 1, 1.0009, -0.5, -0.0009]), true);
  for (const t of [[0, 0, 1, 2], [0, -0.002], [0, 1.002], [], null, undefined, [0, NaN]]) assert.strictEqual(F.vClamps(t), false, JSON.stringify(t));
  // A shape's: Wrapped at Repeat up 1 (any Repeat around) and From above within one repeat; Repeat up 2 not.
  const tv = (o) => F.shapeModel(dcShape(o)).meshes[0].tverts;
  assert.deepStrictEqual([tv({}), tv({ repeat_around: 3 }), tv({ mapping: "above" }), tv({ repeat_up: 2 })].map(F.vClamps), [true, true, true, false]);
  assert.strictEqual(F.vClamps(domeModel().meshes[0].tverts), true);
});
check("DC the names: a copy's n5l_ and 12 hex digits, one per texture, clamp and content - its source TXI's text and its picture's bytes (copyKey) - never per colour; a clamped soft texture its own n5s_ name (the clamp hashed after its colour and blend); every name before DC as it was", () => {
  const c2 = dcName("fxpa_flame02", 2), c0 = dcName("fxpa_flame02", 0), s2 = dcName("fxpa_symbl_003", 2);
  assert.ok([c2, c0, s2].every((n) => F.COPY_TEXTURE.test(n) && F.RESREF.test(n)), [c2, c0, s2].join(" "));
  assert.strictEqual(new Set([c2, c0, s2]).size, 3);
  assert.strictEqual(F.copyName("FXPA_Flame02", 2, DC_KEY.fxpa_flame02), c2);
  assert.ok(![c2, c0, s2].some((n) => F.MOTION_TEXTURE.test(n) || F.SHAPE_TEXTURE.test(n) || F.BAKED.test(n)));
  // The key (copyKey): 14 hex digits of the TXI's text as read, a dot, 14 of the picture's bytes - the same for the same,
  // another for a TXI line or its line ends changed, or a byte of the picture; a Buffer, a Uint8Array or bytes as text alike.
  const key = F.copyKey(FLAME_TXI, Buffer.from("flame-tga"));
  assert.match(key, /^[0-9a-f]{14}\.[0-9a-f]{14}$/);
  assert.strictEqual(F.copyKey(Buffer.from(FLAME_TXI, "latin1"), new Uint8Array(Buffer.from("flame-tga"))), key);
  const others = [F.copyKey(FLAME_TXI.replace("fps 32", "fps 16"), Buffer.from("flame-tga")), F.copyKey(FLAME_TXI.replace(/\r\n/g, "\n"), Buffer.from("flame-tga")),
    F.copyKey(FLAME_TXI, Buffer.from("flame-tgb")), F.copyKey("", Buffer.from("flame-tga")), F.copyKey(FLAME_TXI, Buffer.alloc(0))];
  assert.strictEqual(new Set([key, ...others]).size, 6, JSON.stringify(others));
  assert.strictEqual(new Set(others.map((k) => F.copyName("fxpa_flame02", 2, k)).concat(c2)).size, 6, "a copy's name ignores its key");
  // No key: no name (a copy's bytes not known).
  for (const k of [undefined, null, ""]) assert.throws(() => F.copyName("fxpa_flame02", 2, k), /copyName: fxpa_flame02 - a copy's key/);
  assert.deepStrictEqual([key, c2, c0, s2], ["03bb721f1452fb.1ba8ec87d68e43", "n5l_803909e8bf74", "n5l_457cbc7cb0a0", "n5l_d63cb9ead2b5"],
    "the key and the copies' names pinned: cyrb53 of \"copy:<texture>:<clamp>:<key>\"");
  // The soft texture: unclamped names as before (M1b's pins); clamped, a name of its own for each colour and blend.
  const normal = F.shapeTextureName({ colour: [0.6, 0.8, 1] }), additive = ADDITIVE([0.6, 0.8, 1]);
  assert.deepStrictEqual([normal, additive, F.shapeTextureName({ colour: [0.6, 0.8, 1], clamp: false })], ["n5s_8c757fb28f99", "n5s_617b220c1144", "n5s_8c757fb28f99"]);
  const nc = F.shapeTextureName({ colour: [0.6, 0.8, 1], clamp: true }), ac = F.shapeTextureName({ colour: [0.6, 0.8, 1], blend: "additive", clamp: true });
  assert.ok(F.SHAPE_TEXTURE.test(nc) && F.SHAPE_TEXTURE.test(ac), nc + " " + ac);
  assert.strictEqual(new Set([normal, additive, nc, ac]).size, 4);
  assert.strictEqual(F.shapeTextureName({ colour: [0.6001, 0.8, 1], blend: "additive", clamp: true }), ac, "to 0.001, as the colour");
  assert.deepStrictEqual([nc, ac], ["n5s_a40318b044a1", "n5s_bfc2ebac4509"], "the clamped soft names pinned: \"soft:0.6,0.8,1[:additive]:clamp2\"");
  // Its TXI (softTxi): an Additive one's SOFT_TXI, a Normal one's none - clamped, each with clamp 2.
  assert.strictEqual(F.softTxi({ blend: "additive" }), F.SOFT_TXI);
  for (const s of [{ blend: "normal" }, {}, { blend: "normal", clamp: false }]) assert.strictEqual(F.softTxi(s), null, JSON.stringify(s));
  assert.strictEqual(F.softTxi({ blend: "normal", clamp: true }), "clamp 2\n");
  assert.strictEqual(F.softTxi({ blend: "additive", clamp: true }), "blending additive\nclamp 2\n");
});
check("DC the rule: a still shape's soft texture clamped and a library one copied where the game must have it so (decal; a flipbook within one repeat) - a moving one, a plain texture, a tiled flipbook or None as they were", () => {
  // copied(info, clampV): the copy route for a library texture whose TXI says decal, or a flipbook on a mesh within one repeat.
  assert.deepStrictEqual([[DC_INFO.fxpa_flame02, true], [DC_INFO.fxpa_flame02, false], [DC_INFO.fxpa_symbl_003, false], [DC_INFO.fxpa_globe, true],
    [DC_INFO.my_flip, true], [DC_INFO.my_flip, false], [null, true], [undefined, true]].map(([i, v]) => F.copied(i, v)), [true, true, true, false, true, false, false, false]);
  // copyKind(shape, motion): soft, library or none - only a shape's, only while its part doesn't move.
  assert.deepStrictEqual([[dcShape({}), undefined], [dcShape({ texture: "fxpa_flame02" }), null], [dcShape({ texture: "none" }), undefined],
    [dcShape({}), { breathe: 0.2 }], [dcShape({ texture: "fxpa_flame02" }), { scroll: [0, 0.25] }], [dcShape({}), { rate: 2 }], [undefined, undefined], [null, undefined]]
    .map(([s, m]) => F.copyKind(s, m)), ["soft", "library", null, null, null, "soft", null, null]);
  // SHP-8's ring, green: its mesh names the copy (clamp 2: v 0 to 1 up its side) - everything else of the mesh kept, the
  // mesh given untouched; the same copy in red (one per texture and clamp, not per colour).
  const green = dcShape({ texture: "fxpa_flame02", colour: [0.137, 0.922, 0.314] }), meshes = dcPlaced(green), copy = dcName("fxpa_flame02", 2);
  const out = F.stillTextures(green, undefined, meshes, dcInfo);
  assert.deepStrictEqual(out.meshes.map((m) => m.bitmap), [copy]);
  assert.deepStrictEqual([out.copies, out.softs], [[{ name: copy, texture: "fxpa_flame02", clamp: 2, key: DC_KEY.fxpa_flame02 }], []]);
  assert.deepStrictEqual(Object.assign({}, out.meshes[0], { bitmap: "fxpa_flame02" }), meshes[0]);
  assert.strictEqual(meshes[0].bitmap, "fxpa_flame02", "the mesh given was changed");
  const red = dcShape({ texture: "fxpa_flame02", colour: [1, 0, 0] });
  assert.deepStrictEqual(F.stillTextures(red, undefined, dcPlaced(red), dcInfo).copies, [{ name: copy, texture: "fxpa_flame02", clamp: 2, key: DC_KEY.fxpa_flame02 }]);
  const named = (o, motion) => { const s = dcShape(o); const r = F.stillTextures(s, motion, dcPlaced(s), dcInfo); return [r.meshes[0].bitmap, r.copies, r.softs]; };
  // Repeat up 2 (v tiles): the decal flipbook still copied (no decal), no clamp; TIN-1's glyph From above: clamp 2; a
  // plain texture (fxpa_globe, not decal, not a flipbook): as it is; a flipbook not decal within one repeat: copied for
  // the clamp; tiled: as it is; a texture nothing knows (not in the game): as it is.
  const c0 = dcName("fxpa_flame02", 0), glyph = dcName("fxpa_symbl_003", 2), flip = dcName("my_flip", 2);
  assert.deepStrictEqual(named({ texture: "fxpa_flame02", repeat_up: 2 }), [c0, [{ name: c0, texture: "fxpa_flame02", clamp: 0, key: DC_KEY.fxpa_flame02 }], []]);
  assert.deepStrictEqual(named({ type: "dome", radius: 1, height: undefined, top: undefined, texture: "fxpa_symbl_003", mapping: "above", colour: [1, 0.5, 0] }),
    [glyph, [{ name: glyph, texture: "fxpa_symbl_003", clamp: 2, key: DC_KEY.fxpa_symbl_003 }], []]);
  assert.deepStrictEqual(named({ texture: "fxpa_globe" }), ["fxpa_globe", [], []]);
  assert.deepStrictEqual(named({ texture: "my_flip" }), [flip, [{ name: flip, texture: "my_flip", clamp: 2, key: DC_KEY.my_flip }], []]);
  assert.deepStrictEqual(named({ texture: "my_flip", repeat_up: 2 }), ["my_flip", [], []]);
  assert.deepStrictEqual(named({ texture: "nowhere" }), ["nowhere", [], []]);
  // A decal texture whose key isn't known (its TXI and picture not read: no copy can be named): as it is.
  const keyless = dcShape({ texture: "fxpa_flame02" });
  assert.deepStrictEqual(F.stillTextures(keyless, undefined, dcPlaced(keyless), () => ({ decal: true, cycle: null })).meshes.map((m) => m.bitmap), ["fxpa_flame02"]);
  // Moving: its texture goes through its material (M2; W1's copy drops decal itself) - as it is here.
  assert.deepStrictEqual(named({ texture: "fxpa_flame02" }, { scroll: [0, 0.25] }), ["fxpa_flame02", [], []]);
  // The soft texture: clamped (its own name and blend) within one repeat, Additive or Normal; tiled (Repeat up 2) or
  // moving as before; None nothing.
  const softName = (o) => F.shapeTextureName(Object.assign({ colour: [0.6, 0.8, 1] }, o));
  assert.deepStrictEqual(named({ blend: "additive" }), [softName({ blend: "additive", clamp: true }), [], [{ name: softName({ blend: "additive", clamp: true }),
    colour: [0.6, 0.8, 1], blend: "additive", clamp: true }]]);
  assert.deepStrictEqual(named({ mapping: "above" }), [softName({ clamp: true }), [], [{ name: softName({ clamp: true }), colour: [0.6, 0.8, 1], blend: "normal", clamp: true }]]);
  assert.deepStrictEqual(named({ blend: "additive", repeat_up: 2 }), [softName({ blend: "additive" }), [], []]);
  assert.deepStrictEqual(named({ blend: "additive" }, { scroll: [0, 0.25] }), [softName({ blend: "additive" }), [], []]);
  assert.deepStrictEqual(named({ texture: "none" }), ["", [], []]);
  // Twice is once: a mesh already given its copy or clamped soft name stays (the generators and the page each run it).
  const once = F.stillTextures(green, undefined, meshes, dcInfo);
  assert.deepStrictEqual(F.stillTextures(green, undefined, once.meshes, dcInfo).meshes, once.meshes);
  // No meshes, or none of the shape's own texture: nothing.
  assert.deepStrictEqual(F.stillTextures(green, undefined, [], dcInfo), { meshes: [], softs: [], copies: [] });
});
check("DC in a page: the same TXI words, names, rule and copied TXI", () => {
  const P = pageFloatDisk();
  for (const k of ["CLAMP_V"]) assert.strictEqual(P[k], F[k]);
  assert.strictEqual(String(P.COPY_TEXTURE), String(F.COPY_TEXTURE));
  assert.deepStrictEqual([P.txiDecal(SYM_TXI), P.txiCycle(FLAME_TXI), P.txiClampBits("clamp 1\nclamp 2"), P.copiedTxi(FLAME_TXI, 2), P.softTxi({ blend: "additive", clamp: true })],
    [F.txiDecal(SYM_TXI), F.txiCycle(FLAME_TXI), F.txiClampBits("clamp 1\nclamp 2"), F.copiedTxi(FLAME_TXI, 2), F.softTxi({ blend: "additive", clamp: true })]);
  assert.deepStrictEqual([P.copyName("fxpa_flame02", 2, DC_KEY.fxpa_flame02), P.shapeTextureName({ colour: [1, 0, 0], clamp: true }), P.vClamps([0, 0, 3, 1]), P.copied(DC_INFO.my_flip, true),
    P.copyKey(FLAME_TXI, new Uint8Array(Buffer.from("flame-tga")))], [dcName("fxpa_flame02", 2), F.shapeTextureName({ colour: [1, 0, 0], clamp: true }), true, true, DC_KEY.fxpa_flame02]);
  const s = { type: "cylinder", radius: 1, height: 0.6, top: 1.17, texture: "fxpa_flame02" };
  const ps = P.readShape(s, "s"), fs = F.readShape(s, "s");
  assert.strictEqual(P.copyKind(ps), F.copyKind(fs));
  assert.deepStrictEqual(P.stillTextures(ps, undefined, P.shapeModel(ps).meshes, dcInfo), F.stillTextures(fs, undefined, F.shapeModel(fs).meshes, dcInfo));
});
check("DC files.js: a still Soft shape's clamped texture made under its own name with its TXI; a still library decal or flipbook texture copied - its picture and a TXI of its lines minus decal (+ clamp 2) under its n5l_ name, owned by it; the rest as before", () => {
  const fs = require("fs");
  const os = require("os");
  const W = require("./files.js");
  const part = (shape, motion) => F.readPart({ shape: Object.assign({ type: "cylinder", radius: 1, height: 0.6, top: 1.17 }, shape), at: [0, 0, 0], motion }, "p", BASE);
  // SHP-8's green ring; a red one (the same copy); one tiled up (its copy unclamped); a plain library texture; a moving
  // flipbook (its material's); a Soft Additive cylinder (clamped), a tiled one (as before); None.
  const placed = [part({ texture: "fxpa_flame02", colour: [0.137, 0.922, 0.314] }), part({ texture: "fxpa_flame02", colour: [1, 0, 0] }),
    part({ texture: "fxpa_flame02", repeat_up: 2 }), part({ texture: "fxpa_globe" }), part({ texture: "fxpa_flame02" }, { scroll: [0, 0.25] }),
    part({ blend: "additive" }), part({ blend: "additive", repeat_up: 2 }), part({ texture: "none" })].map((p) => ({ part: p }));
  for (const x of placed) x.meshes = F.placePart(x.part, F.shapeModel(x.part.source.shape), 0, 0, "j", true).meshes;
  const add = ADDITIVE([0.6, 0.8, 1]), addClamped = F.shapeTextureName({ colour: [0.6, 0.8, 1], blend: "additive", clamp: true });
  // The soft textures: the clamped one under its own name (the mesh renamed), its TXI blending additive + clamp 2; the
  // tiled one's as before.
  const shaped = W.shapeTextures(placed);
  assert.deepStrictEqual([...shaped.keys()], [addClamped, add]);
  assert.deepStrictEqual([placed[5].meshes[0].bitmap, placed[6].meshes[0].bitmap], [addClamped, add]);
  assert.deepStrictEqual([shaped.get(addClamped).txi.toString("latin1"), shaped.get(add).txi.toString("latin1")], ["blending additive\nclamp 2\n", F.SOFT_TXI]);
  assert.ok(shaped.get(addClamped).bytes.equals(shaped.get(add).bytes), "the clamped soft texture's picture isn't its colour's");
  assert.deepStrictEqual(W.shapeJson(shaped), [{ texture: addClamped, colour: [0.6, 0.8, 1], blend: "additive", clamp: true }, { texture: add, colour: [0.6, 0.8, 1], blend: "additive" }]);
  assert.deepStrictEqual(W.shapeLines(shaped, "data/files"), [
    `  texture ${addClamped}: a shape's soft texture, colour 0.6 0.8 1, additive, clamped in v (${addClamped}.txi beside it: blending additive, clamp 2) - written into data/files`,
    `  texture ${add}: a shape's soft texture, colour 0.6 0.8 1, additive (${add}.txi beside it: blending additive) - written into data/files`]);
  // The copies: fxpa_flame02's picture as the game draws it (reader.js image), asked for once a texture; the TXI its lines
  // minus decal, + clamp 2 within one repeat; fxpa_globe asked for and left (not decal, not a flipbook).
  const FLAME = { where: "game", tga: Buffer.from("flame-tga"), dds: null, plt: null, txi: FLAME_TXI };
  const GLOBE = { where: "game", tga: Buffer.from("globe-tga"), dds: Buffer.from("globe-dds"), plt: null, txi: "blending additive\r\nproceduretype arturo\r\n" };
  const asked = [], rd = { image: (name) => { asked.push(name); return name === "fxpa_flame02" ? FLAME : name === "fxpa_globe" ? GLOBE : null; } };
  const copies = W.copyTextures(placed, new Map(), { rd, shaped });
  const c2 = dcName("fxpa_flame02", 2), c0 = dcName("fxpa_flame02", 0);  // (its key: FLAME_TXI and "flame-tga", as rd gives them)
  // (MV: the moving flipbook - it scrolls up - names its copy too, unclamped: floatdisk.js gameTextures.)
  assert.deepStrictEqual(placed.map((x) => x.meshes.length ? x.meshes[0].bitmap : null), [c2, c2, c0, "fxpa_globe", c0, addClamped, add, ""]);
  assert.deepStrictEqual([...copies.keys()], [c2, c0]);
  assert.deepStrictEqual([...new Set(asked)], ["fxpa_flame02", "fxpa_globe"]);
  const lines = "proceduretype cycle\r\nnumx 4\r\nnumy 4\r\nfps 32\r\nblending additive\r\ndownsamplemax 0\r\ndownsamplemin 0\r\n";
  assert.deepStrictEqual([c2, c0].map((n) => { const c = copies.get(n); return [c.texture, c.clamp, c.ext, c.bytes.toString(), c.txi.toString("latin1"), c.label]; }),
    [["fxpa_flame02", 2, ".tga", "flame-tga", lines + "clamp 2\r\n", "cylinder 1 m"], ["fxpa_flame02", 0, ".tga", "flame-tga", lines, "cylinder 1 m"]]);
  // The moving one's material: texture0 its copy (MV: before, the game's fxpa_flame02 - M2's route; its decal is never
  // read through a material, the copy is for its flipbook's clamp when it moves sideways: the MV checks).
  const materials = W.motionMaterials(placed);
  assert.deepStrictEqual([...materials.values()].map((m) => [m.texture, m.text.split("\n").includes("texture0 " + c0)]), [[c0, true]]);
  // Writes: each copy's picture and TXI under its name, kind "copy" (owned by the n5l_ name); --json's and the summary's.
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "n5_fddc-"));
  try {
    const writes = W.copyWrites(copies, dir);
    assert.deepStrictEqual(writes.map((w) => [path.basename(w.file), w.kind]), [[c2 + ".tga", "copy"], [c2 + ".txi", "copy"], [c0 + ".tga", "copy"], [c0 + ".txi", "copy"]]);
    assert.deepStrictEqual(W.copyWrites(new Map(), dir), []);
    assert.deepStrictEqual(W.copyJson(copies), [{ copy: c2, texture: "fxpa_flame02", clamp: 2 }, { copy: c0, texture: "fxpa_flame02", clamp: 0 }]);
    assert.deepStrictEqual(W.copyLines(copies, "data/files"), [
      `  texture ${c2}: fxpa_flame02 copied without its decal mark, clamped in v (${c2}.tga, ${c2}.txi: its TXI without decal, and clamp 2) - written into data/files`,
      `  texture ${c0}: fxpa_flame02 copied without its decal mark (${c0}.tga, ${c0}.txi: its TXI without decal) - written into data/files`]);
    const owned = W.ownership({ repo: dir, manifest: new Set(), mark: F.MARK });
    for (const ext of [".tga", ".dds", ".txi"]) assert.strictEqual(owned({ file: path.join(dir, c2 + ext), kind: "copy" }, Buffer.from("x")), true, ext);
    for (const name of ["n5l_mine.tga", "fxpa_flame02.tga", c2 + ".mtr", "n5u_0123456789ab.tga"]) assert.strictEqual(owned({ file: path.join(dir, name), kind: "copy" }, Buffer.from("x")), false, name);
    fs.writeFileSync(path.join(dir, c2 + ".txi"), "decal 1\r\n");
    assert.deepStrictEqual(W.writeAll(writes, owned, { repo: dir, manifest: new Set() }).map(([s, f]) => [s, path.basename(f)]),
      [["new", c2 + ".tga"], ["rebuilt", c2 + ".txi"], ["new", c0 + ".tga"], ["new", c0 + ".txi"]]);
    assert.strictEqual(fs.readFileSync(path.join(dir, c2 + ".txi"), "latin1"), lines + "clamp 2\r\n");
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
  // A DDS when the texture has one (what the game draws), its TXI from the game: a decal DDS-only texture.
  const dds = [{ part: part({ texture: "c_air_skin" }) }];
  dds[0].meshes = F.placePart(dds[0].part, F.shapeModel(dds[0].part.source.shape), 0, 0, "j", true).meshes;
  const air = W.copyTextures(dds, new Map(), { rd: { image: () => ({ where: "game", tga: Buffer.from("t"), dds: Buffer.from("d"), plt: null, txi: "decal 1\r\nproceduretype arturo\r\n" }) } });
  assert.deepStrictEqual([...air.values()].map((c) => [c.ext, c.bytes.toString(), c.txi.toString("latin1")]), [[".dds", "d", "proceduretype arturo\r\nclamp 2\r\n"]]);
  // Without a reader (a page's caller, or none given): nothing copied.
  const bare = [{ part: part({ texture: "fxpa_flame02" }) }];
  bare[0].meshes = F.placePart(bare[0].part, F.shapeModel(bare[0].part.source.shape), 0, 0, "j", true).meshes;
  assert.strictEqual(W.copyTextures(bare, new Map(), {}).size, 0);
  assert.strictEqual(bare[0].meshes[0].bitmap, "fxpa_flame02");
});

// --- MV (#39; the owner, 2026-11-19: "go"): moving textures keep their blend, a flipbook or decal texture moves from DC's
// copy, a sideways-only scroll clamped in v. The engine (the MV diagnosis): a mesh's blend comes only from <its bitmap>.txi
// (Material::Init -> ParseField reads blending and decal there) - an n5u_ name had no TXI, so every moving additive texture
// drew alpha-blended, on black; texture0's own TXI gives its flipbook and its clamp (CAurTextureBasic::Init), never its
// blend or decal. ------------------------------------------------------------------------------------------------------
const RING = { scroll: [0.5, 0] };  // the owner's Scroll round 0.5 (DC-1's flame ring, 2026-11-19)
const mvPart = (shape, motion) => F.readPart({ shape: Object.assign({ type: "cylinder", radius: 1, height: 0.6, top: 1.17 }, shape), at: [0, 0, 0], motion }, "p", BASE);
check("MV the rule: sideways only - scroll up, turn and breath each 0 as the material takes them (to 0.001) - on a mesh whose v stays within one repeat; a still part's is vClamps (DC's); scroll up, a turn or a breath never", () => {
  const ring = dcPlaced(dcShape({ texture: "fxpa_flame02" }))[0].tverts, tiled = dcPlaced(dcShape({ texture: "fxpa_flame02", repeat_up: 2 }))[0].tverts;
  for (const m of [RING, { scroll: [-0.05, 0] }, { scroll: [0.01, 0] }, { scroll: [10, 0.0004] }, { scroll: [0.5, 0], rate: 3 }, F.readMotion({ scroll: [0.31, 0] }, "m")]) {
    assert.strictEqual(F.sidewaysClamps(m, ring), true, JSON.stringify(m));
  }
  // Still: DC's rule, vClamps.
  for (const m of [undefined, null, {}, STILL, { rate: 2 }]) {
    assert.strictEqual(F.sidewaysClamps(m, ring), true, JSON.stringify(m));
    assert.strictEqual(F.sidewaysClamps(m, tiled), false, JSON.stringify(m));
  }
  // Scroll up (either way, with or without a scroll round), a turn, a breath: never (each takes v out of 0..1).
  for (const m of [{ scroll: [0, 0.25] }, { scroll: [0.5, -0.1] }, { scroll: [0.5, 0.001] }, { scroll: [0.5, 0], turn: 5 }, { turn: 30 }, { breathe: 0.2 },
    { scroll: [0.5, 0], breathe: 0.05 }]) assert.strictEqual(F.sidewaysClamps(m, ring), false, JSON.stringify(m));
  // v past one repeat (Repeat up 2, v below 0) or no UVs: never; u past one repeat is fine (round a ring: Repeat around).
  for (const t of [tiled, [0, -0.5, 1, 1], [], null, undefined]) assert.strictEqual(F.sidewaysClamps(RING, t), false, JSON.stringify(t));
  assert.strictEqual(F.sidewaysClamps(RING, [0, 0, 3, 1, 1.5, 0.5]), true);
  // Never with W1's: a breath is W1's rule (motionClamps), never this one.
  for (const m of [{ breathe: 0.2 }, { breathe: 0.2, turn: 30 }]) assert.deepStrictEqual([F.motionClamps(m, ring), F.sidewaysClamps(m, ring)], [true, false], JSON.stringify(m));
  assert.throws(() => F.sidewaysClamps({ scroll: [11, 0] }, ring), /motion\.scroll: /);
});
check("MV motionTxi: a moving texture's material's own TXI - its texture's TXI's blending and map lines as they are (with decal, all the engine reads for a mesh from its bitmap's TXI), LF; none: null", () => {
  for (const t of [FLAME_TXI, SYM_TXI, Buffer.from(FLAME_TXI, "latin1"), F.SOFT_TXI, F.softTxi({ blend: "additive", clamp: true }), F.copiedTxi(FLAME_TXI, 2)]) {
    assert.strictEqual(F.motionTxi(t), "blending additive\n", JSON.stringify(String(t)));
  }
  assert.strictEqual(F.motionTxi("blending punchthrough\r\nmipmap 0\r\n"), "blending punchthrough\n");
  assert.strictEqual(F.motionTxi("  Blending\tAdditive \r\nfilter 0"), "  Blending\tAdditive \n", "the line as it is, but its line end");
  assert.strictEqual(F.motionTxi("blending default\r\nmipmap 0\r\nblending additive"), "blending default\nblending additive\n", "each, in order");
  for (const t of ["", null, undefined, Buffer.alloc(0), "decal 1\r\n", "proceduretype cycle\nnumx 4\nnumy 4\nfps 32\n", "clamp 2\n", "blending\n", "blending  \r\n",
    "# blending additive", "blendingx additive", F.softTxi({ blend: "normal", clamp: true })]) assert.strictEqual(F.motionTxi(t), null, JSON.stringify(t));
  // Never a decal, a clamp or a flipbook: those are texture0's (its own TXI) or nobody's.
  const t = F.motionTxi(FLAME_TXI);
  assert.deepStrictEqual([F.txiDecal(t), F.txiClampBits(t), F.txiCycle(t), F.txiAdditive(t)], [false, 0, null, true]);
  // Its own TXI again: the same (the page is told it as the texture route reads it).
  assert.strictEqual(F.motionTxi(F.motionTxi("blending default\nblending additive\n")), "blending default\nblending additive\n");
  // Fix round 1 (the MV review: Material::ParseField reads these from the bitmap's TXI too): the environment and bump map
  // lines are carried as the source has them, in its order - everything the engine reads for the mesh but decal. The
  // game's c_diamond: envmaptexture and bumpyshinytexture, no blending.
  const DIAMOND = "envmaptexture evmap_irrid\r\nbumpyshinytexture evmap_irrid\r\n";
  assert.strictEqual(F.motionTxi(DIAMOND), "envmaptexture evmap_irrid\nbumpyshinytexture evmap_irrid\n");
  assert.strictEqual(F.motionTxi("decal 1\r\nBumpMapTexture tex_bump\r\nmipmap 0\r\nblending additive\r\nbumpreplacementtexture tex_r\r\nproceduretype cycle\r\n"),
    "BumpMapTexture tex_bump\nblending additive\nbumpreplacementtexture tex_r\n");
  for (const t of ["envmaptexture\n", "envmaptexturex foo\n", "# bumpmaptexture foo"]) assert.strictEqual(F.motionTxi(t), null, JSON.stringify(t));
  assert.strictEqual(F.motionTxi(F.motionTxi(DIAMOND)), F.motionTxi(DIAMOND));
});
check("MV the name: the material's TXI hashed in (M1b's rule: a changed TXI never shares a name) and the sideways clamp its own mark, not W1's; with neither, every name as before", () => {
  const m = F.readMotion(RING, "m"), p = [0.5, 0.5], ADD = "blending additive\n";
  const plain = F.motionName("fxpa_flame02", m, p);
  for (const more of [{}, { side: false, txi: null }, { txi: "" }]) assert.strictEqual(F.motionName("fxpa_flame02", m, p, false, more), plain, JSON.stringify(more));
  assert.strictEqual(F.motionName("fxpa_globe", { scroll: [0, 0.25] }, p, false, {}), "n5u_31e9f07948e1", "M2's pin");
  const add = F.motionName("fxpa_flame02", m, p, false, { txi: ADD }), punch = F.motionName("fxpa_flame02", m, p, false, { txi: "blending punchthrough\n" });
  const side = F.motionName("fxpa_flame02", m, p, false, { side: true }), both = F.motionName("fxpa_flame02", m, p, false, { side: true, txi: ADD });
  const w1 = F.motionName("fxpa_flame02", m, p, true);
  assert.ok([add, punch, side, both].every((n) => F.MOTION_TEXTURE.test(n)), [add, punch, side, both].join(" "));
  assert.strictEqual(new Set([plain, add, punch, side, both, w1]).size, 6);
  assert.deepStrictEqual([add, side, F.motionName("n5l_803909e8bf74", m, p, false, { side: true, txi: ADD })], ["n5u_19a49b6d02f8", "n5u_4ef9b28de294", "n5u_e3017721fff2"],
    "the recipes pinned: cyrb53 of \"uv:<texture0>:<numbers>[:clampv][:txi:<its TXI>]\"");
});
check("MV gameTextures: a shape's textures as the game is given them, moving too - DC's rule with its 'v within one repeat' read as 'sideways only, v within one repeat' (sidewaysClamps): the flame ring scrolling round names the still ring's copy (clamp 2), scrolling up or turning its copy unclamped; a soft texture clamped sideways only; a breath W1's own; stillTextures still DC's", () => {
  const c2 = dcName("fxpa_flame02", 2), c0 = dcName("fxpa_flame02", 0), flip2 = dcName("my_flip", 2), glyph0 = dcName("fxpa_symbl_003", 0);
  const copy = (name, texture, clamp) => [{ name, texture, clamp, key: DC_KEY[texture] }];
  const named = (o, motion) => { const s = dcShape(o); const r = F.gameTextures(s, motion, dcPlaced(s), dcInfo); return [r.meshes[0] ? r.meshes[0].bitmap : null, r.copies, r.softs]; };
  // A still shape: DC's stillTextures, every case.
  for (const o of [{ texture: "fxpa_flame02" }, { texture: "fxpa_flame02", repeat_up: 2 }, { texture: "fxpa_globe" }, { texture: "my_flip" }, { blend: "additive" }, { texture: "none" },
    { type: "dome", radius: 1, height: undefined, top: undefined, texture: "fxpa_symbl_003", mapping: "above" }]) {
    const s = dcShape(o);
    assert.deepStrictEqual(F.gameTextures(s, undefined, dcPlaced(s), dcInfo), F.stillTextures(s, undefined, dcPlaced(s), dcInfo), JSON.stringify(o));
    assert.deepStrictEqual(F.gameTextures(s, STILL, dcPlaced(s), dcInfo), F.stillTextures(s, STILL, dcPlaced(s), dcInfo), JSON.stringify(o));
  }
  // Scroll round (MV-1's 0.5, MV-3's 0.01, either way): the still ring's own copy, clamp 2 - one copy a texture, not a speed.
  for (const m of [RING, { scroll: [0.01, 0] }, { scroll: [-0.31, 0] }]) assert.deepStrictEqual(named({ texture: "fxpa_flame02" }, m), [c2, copy(c2, "fxpa_flame02", 2), []], JSON.stringify(m));
  // Scroll up, a turn, both ways at once: its copy (decal dropped) unclamped - v must repeat.
  for (const m of [{ scroll: [0, 0.25] }, { scroll: [0.5, 0.25] }, { turn: 30 }, { scroll: [0.5, 0], turn: 5 }]) {
    assert.deepStrictEqual(named({ texture: "fxpa_flame02" }, m), [c0, copy(c0, "fxpa_flame02", 0), []], JSON.stringify(m));
  }
  // A breath W1 clamps (every UV within one repeat): W1's own copy route - as it is here; one W1 doesn't (u round a ring
  // three times): its copy, unclamped.
  assert.deepStrictEqual(named({ texture: "fxpa_flame02" }, { breathe: 0.2 }), ["fxpa_flame02", [], []]);
  assert.deepStrictEqual(named({ texture: "fxpa_flame02", repeat_around: 3 }, { breathe: 0.2 }), [c0, copy(c0, "fxpa_flame02", 0), []]);
  // A flipbook that isn't decal: copied for the clamp sideways, as it is scrolling up; TIN-1's glyph From above turning:
  // copied (decal) unclamped; a plain texture (fxpa_globe) as it is either way; one nothing knows, as it is.
  assert.deepStrictEqual(named({ texture: "my_flip" }, RING), [flip2, copy(flip2, "my_flip", 2), []]);
  assert.deepStrictEqual(named({ texture: "my_flip" }, { scroll: [0, 0.25] }), ["my_flip", [], []]);
  assert.deepStrictEqual(named({ type: "dome", radius: 1, height: undefined, top: undefined, texture: "fxpa_symbl_003", mapping: "above" }, { turn: 30 }),
    [glyph0, copy(glyph0, "fxpa_symbl_003", 0), []]);
  for (const m of [RING, { scroll: [0, 0.25] }]) assert.deepStrictEqual(named({ texture: "fxpa_globe" }, m), ["fxpa_globe", [], []], JSON.stringify(m));
  assert.deepStrictEqual(named({ texture: "nowhere" }, RING), ["nowhere", [], []]);
  // A soft texture: clamped sideways within one repeat (its own name, as a still one's), as it is scrolling up or tiled.
  const softName = (o) => F.shapeTextureName(Object.assign({ colour: [0.6, 0.8, 1] }, o));
  assert.deepStrictEqual(named({ blend: "additive" }, RING), [softName({ blend: "additive", clamp: true }), [],
    [{ name: softName({ blend: "additive", clamp: true }), colour: [0.6, 0.8, 1], blend: "additive", clamp: true }]]);
  assert.deepStrictEqual(named({ blend: "additive" }, { scroll: [0, 0.25] }), [softName({ blend: "additive" }), [], []]);
  assert.deepStrictEqual(named({ blend: "additive", repeat_up: 2 }, RING), [softName({ blend: "additive" }), [], []]);
  assert.deepStrictEqual(named({ blend: "additive" }, { breathe: 0.2 }), [softName({ blend: "additive" }), [], []], "W1's");
  // stillTextures stays DC's: a moving part's meshes as they are (a bench from before MV gets what it got).
  const green = dcShape({ texture: "fxpa_flame02", colour: [0.137, 0.922, 0.314] }), meshes = dcPlaced(green);
  assert.deepStrictEqual(F.stillTextures(green, RING, meshes, dcInfo), { meshes, softs: [], copies: [] });
  // Twice is once; the meshes given untouched.
  const once = F.gameTextures(green, RING, meshes, dcInfo);
  assert.deepStrictEqual(F.gameTextures(green, RING, once.meshes, dcInfo).meshes, once.meshes);
  assert.strictEqual(meshes[0].bitmap, "fxpa_flame02");
});
check("MV moveTextures: each moving mesh's material names the texture it has (texture0: the shape rule's copy or clamped soft texture), its own TXI that texture's blending lines (txiOf), the sideways clamp marked; with neither, named as before; W1's as before", () => {
  const green = dcShape({ texture: "fxpa_flame02", colour: [0.137, 0.922, 0.314] }), c2 = dcName("fxpa_flame02", 2), c0 = dcName("fxpa_flame02", 0);
  const TXIS = { [c2]: F.copiedTxi(FLAME_TXI, 2), [c0]: F.copiedTxi(FLAME_TXI, 0), fxpa_flame02: FLAME_TXI, fxpa_globe: "blending additive\r\nproceduretype arturo\r\n",
    tex_rock: "mipmap 0\r\n" };
  const txiOf = (t) => (t in TXIS ? TXIS[t] : null), ADD = "blending additive\n";
  const moved = (shape, motion, opts = {}) => {
    const g = F.gameTextures(shape, motion, dcPlaced(shape), dcInfo);
    return F.moveTextures(g.meshes, motion, Object.assign({ shape, txiOf }, opts));
  };
  const m = F.readMotion(RING, "m"), p = F.uvPivot(dcPlaced(green)[0].tverts);
  // MV-1's ring: texture0 the copy (clamp 2), its TXI blending additive, the sideways mark.
  const ring = moved(green, RING), name = F.motionName(c2, m, p, false, { side: true, txi: ADD });
  assert.deepStrictEqual(ring.meshes.map((x) => x.bitmap), [name]);
  assert.deepStrictEqual(ring.materials, [{ name, texture: c2, motion: m, pivot: p, side: true, txi: ADD }]);
  assert.ok(F.motionMaterialText(ring.materials[0]).split("\n").includes("texture0 " + c2), F.motionMaterialText(ring.materials[0]));
  // Scrolling up: its copy unclamped, no mark; its TXI still.
  const up = F.readMotion({ scroll: [0, 0.25] }, "m"), u = moved(green, up);
  assert.deepStrictEqual(u.materials, [{ name: F.motionName(c0, up, p, false, { txi: ADD }), texture: c0, motion: up, pivot: p, txi: ADD }]);
  // A plain texture whose TXI has a blending line (fxpa_globe), sideways: not clamped, so no mark - its TXI; one with none
  // (tex_rock), or no txiOf (uvprobe; a caller with no pictures): named as before, no TXI.
  const globe = moved(dcShape({ texture: "fxpa_globe" }), RING).materials[0];
  assert.deepStrictEqual([globe.name, globe.texture, "side" in globe, globe.txi], [F.motionName("fxpa_globe", m, p, false, { txi: ADD }), "fxpa_globe", false, ADD]);
  const rock = moved(dcShape({ texture: "tex_rock" }), RING).materials[0];
  assert.deepStrictEqual(rock, { name: F.motionName("tex_rock", m, p), texture: "tex_rock", motion: m, pivot: p });
  const bare = F.moveTextures(dcPlaced(green), RING);
  assert.deepStrictEqual(bare.materials, [{ name: F.motionName("fxpa_flame02", m, p), texture: "fxpa_flame02", motion: m, pivot: p }]);
  // A soft Additive cylinder sideways: its clamped soft texture, marked; its TXI blending additive (its own TXI's line).
  const soft = dcShape({ blend: "additive" }), sc = F.shapeTextureName({ colour: soft.colour, blend: "additive", clamp: true });
  const softTxi = (t) => (t === sc ? F.softTxi({ blend: "additive", clamp: true }) : null);
  assert.deepStrictEqual(moved(soft, RING, { txiOf: softTxi }).materials, [{ name: F.motionName(sc, m, p, false, { side: true, txi: ADD }), texture: sc, motion: m, pivot: p, side: true, txi: ADD }]);
  // The mark only where the rule clamped it: the soft texture not yet renamed (the stage's meshes, before Play's rule) -
  // no mark; nor a still part's (no motion: no material at all).
  const su = F.shapeTextureName({ colour: soft.colour, blend: "additive" });
  assert.deepStrictEqual(F.moveTextures(dcPlaced(soft), RING, { shape: soft }).materials, [{ name: F.motionName(su, m, p), texture: su, motion: m, pivot: p }]);
  assert.deepStrictEqual(F.moveTextures(dcPlaced(soft), undefined, { shape: soft }).materials, []);
  // No shape (a game part's or your own file's mesh): never marked (DC copies only a shape's) - its TXI still; a mesh of
  // no shape naming a copy (a model's own n5l_ texture) isn't marked either.
  const part = F.moveTextures(dcPlaced(green), RING, { txiOf });
  assert.deepStrictEqual(part.materials, [{ name: F.motionName("fxpa_flame02", m, p, false, { txi: ADD }), texture: "fxpa_flame02", motion: m, pivot: p, txi: ADD }]);
  const named = F.moveTextures(F.gameTextures(green, RING, dcPlaced(green), dcInfo).meshes, RING, { txiOf });
  assert.deepStrictEqual(named.materials, [{ name: F.motionName(c2, m, p, false, { txi: ADD }), texture: c2, motion: m, pivot: p, txi: ADD }]);
  // W1's clamped material as before: its own name, no TXI of this kind (its copy's TXI carries the source's lines).
  const breath = F.readMotion({ breathe: 0.2 }, "m"), w1 = moved(green, breath);
  assert.deepStrictEqual(w1.materials, [{ name: F.motionName("fxpa_flame02", breath, p, true), texture: "fxpa_flame02", motion: breath, pivot: p, clamp: true }]);
});
check("MV files.js: a moving flame ring scrolling round names DC's copy (clamp 2) through its material, its n5u_ TXI (blending additive) beside its .mtr; scrolling up its copy unclamped; an Additive soft cylinder's TXI; a plain texture with no blending line, and no pictures, as before", () => {
  const fs = require("fs");
  const os = require("os");
  const W = require("./files.js");
  const up = { scroll: [0, 0.25] };
  const placed = [mvPart({ texture: "fxpa_flame02" }, RING), mvPart({ texture: "fxpa_flame02" }, up), mvPart({ blend: "additive" }, up), mvPart({ blend: "additive" }, RING),
    mvPart({ texture: "tex_rock" }, RING), mvPart({ texture: "fxpa_globe" }, RING)].map((p) => ({ part: p }));
  for (const x of placed) x.meshes = F.placePart(x.part, F.shapeModel(x.part.source.shape), 0, 0, "j", true).meshes;
  const FLAME = { where: "game", tga: Buffer.from("flame-tga"), dds: null, plt: null, txi: FLAME_TXI };
  const GLOBE = { where: "game", tga: Buffer.from("globe-tga"), dds: null, plt: null, txi: "blending additive\r\nproceduretype arturo\r\n" };
  const ROCK = { where: "game", tga: Buffer.from("rock-tga"), dds: null, plt: null, txi: "" };
  const rd = { image: (name) => ({ fxpa_flame02: FLAME, fxpa_globe: GLOBE, tex_rock: ROCK })[name] || null };
  const shaped = W.shapeTextures(placed);
  const add = ADDITIVE([0.6, 0.8, 1]), addClamped = F.shapeTextureName({ colour: [0.6, 0.8, 1], blend: "additive", clamp: true });
  // The soft textures: the sideways one clamped (its own name and TXI), the one scrolling up as it was - each made once.
  assert.deepStrictEqual([...shaped.keys()], [add, addClamped]);
  const copies = W.copyTextures(placed, new Map(), { rd, shaped });
  const c2 = dcName("fxpa_flame02", 2), c0 = dcName("fxpa_flame02", 0);
  assert.deepStrictEqual([...copies.keys()], [c2, c0]);
  assert.deepStrictEqual(placed.map((x) => x.meshes[0].bitmap), [c2, c0, add, addClamped, "tex_rock", "fxpa_globe"]);
  const materials = W.motionMaterials(placed, new Map(), { sources: { rd, shaped }, copies });
  const ADD = "blending additive\n", p = [0.5, 0.5], ring = F.readMotion(RING, "m"), scroll = F.readMotion(up, "m");
  const want = [[c2, ring, true, ADD], [c0, scroll, false, ADD], [add, scroll, false, ADD], [addClamped, ring, true, ADD], ["tex_rock", ring, false, null], ["fxpa_globe", ring, false, ADD]]
    .map(([t, m, side, txi]) => F.motionName(t, m, p, false, { side, txi }));
  assert.deepStrictEqual(placed.map((x) => x.meshes[0].bitmap), want);
  assert.strictEqual(want[4], F.motionName("tex_rock", ring, p), "a plain texture with no blending line isn't named as before");
  const mat = (k) => materials.get(want[k]);
  assert.deepStrictEqual([0, 1, 2, 3, 4, 5].map((k) => [mat(k).texture, mat(k).side === true, mat(k).txi || null]),
    [[c2, true, ADD], [c0, false, ADD], [add, false, ADD], [addClamped, true, ADD], ["tex_rock", false, null], ["fxpa_globe", false, ADD]]);
  assert.ok(/^texture0 /m.test(mat(0).text) && mat(0).text.split("\n").includes("texture0 " + c2), mat(0).text);
  // Writes: a material with a TXI its .mtr then <name>.txi, kind "material" (owned by its n5u_ name); then the shader.
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "n5_fdmv-"));
  try {
    const writes = W.motionWrites(materials, dir);
    assert.deepStrictEqual(writes.map((w) => [path.basename(w.file), w.kind]), [
      [want[0] + ".mtr", "material"], [want[0] + ".txi", "material"], [want[1] + ".mtr", "material"], [want[1] + ".txi", "material"],
      [want[2] + ".mtr", "material"], [want[2] + ".txi", "material"], [want[3] + ".mtr", "material"], [want[3] + ".txi", "material"],
      [want[4] + ".mtr", "material"], [want[5] + ".mtr", "material"], [want[5] + ".txi", "material"], ["vsn5uvmove.shd", "shader"]]);
    assert.strictEqual(writes[1].bytes.toString("latin1"), ADD);
    const owned = W.ownership({ repo: dir, manifest: new Set(), mark: F.MARK });
    assert.strictEqual(owned(writes[1], Buffer.from("x")), true);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
  // --json's: side and the TXI said; the summary's line names the copy, the clamp and the TXI.
  assert.deepStrictEqual(W.motionJson(materials).map((j) => [j.texture, j.side === true, j.txi || null]),
    [[c2, true, ADD], [c0, false, ADD], [add, false, ADD], [addClamped, true, ADD], ["tex_rock", false, null], ["fxpa_globe", false, ADD]]);
  assert.ok(!("side" in W.motionJson(materials)[4]) && !("txi" in W.motionJson(materials)[4]));
  const lines = W.motionLines(materials, "data/files");
  assert.strictEqual(lines[0], `  material ${want[0]}: ${c2} scrolls 0.5 0 a second, round 0.5 0.5, clamped in v (scrolling sideways only), ${want[0]}.txi beside it: ` +
    "blending additive - written into data/files, with vsn5uvmove.shd");
  assert.strictEqual(lines[4], `  material ${want[4]}: tex_rock scrolls 0.5 0 a second, round 0.5 0.5 - written into data/files, with vsn5uvmove.shd`);
  // No pictures given (uvprobe: TMO-1's domes as they were): no TXI, named as before.
  const old = [mvPart({ texture: "fxpa_globe" }, RING)].map((p) => ({ part: p }));
  for (const x of old) x.meshes = F.placePart(x.part, F.shapeModel(x.part.source.shape), 0, 0, "j", true).meshes;
  assert.deepStrictEqual([...W.motionMaterials(old).values()].map((x) => [x.texture, x.txi || null]), [["fxpa_globe", null]]);
  assert.strictEqual(old[0].meshes[0].bitmap, F.motionName("fxpa_globe", ring, p));
});
check("MV in a page: the same rule, TXI, names, shape rule and moveTextures", () => {
  const P = pageFloatDisk();
  const ring = dcPlaced(dcShape({ texture: "fxpa_flame02" }))[0].tverts;
  assert.deepStrictEqual([P.sidewaysClamps(RING, ring), P.sidewaysClamps({ turn: 5 }, ring), P.motionTxi(FLAME_TXI)], [true, false, "blending additive\n"]);
  assert.strictEqual(P.motionName("fxpa_flame02", RING, [0.5, 0.5], false, { side: true, txi: "blending additive\n" }),
    F.motionName("fxpa_flame02", RING, [0.5, 0.5], false, { side: true, txi: "blending additive\n" }));
  const s = { type: "cylinder", radius: 1, height: 0.6, top: 1.17, texture: "fxpa_flame02" };
  const ps = P.readShape(s, "s"), fs = F.readShape(s, "s");
  const pg = P.gameTextures(ps, RING, P.shapeModel(ps).meshes, dcInfo), fg = F.gameTextures(fs, RING, F.shapeModel(fs).meshes, dcInfo);
  assert.deepStrictEqual(pg, fg);
  const txiOf = () => FLAME_TXI;
  assert.deepStrictEqual(P.moveTextures(pg.meshes, RING, { shape: ps, txiOf }), F.moveTextures(fg.meshes, RING, { shape: fs, txiOf }));
});

// --- AB (#39; the owner, 2026-11-23: "for these effects can I pick an existing weapon mesh instead of using a custom one?",
// then "set it up and make 1 handed swords the first weapon, you can pick one at random"): a part's Blend, Additive - a game
// weapon glowing as the generated blade does. Its textured meshes lit by themselves at its tint and named for copies of
// their textures whose TXI says blending additive (the game takes a mesh's blend only from its bitmap's TXI); an
// untextured one only self-lit. -------------------------------------------------------------------------------------------
const AB_AT = { at: [0, 0, 0], turn: 0, scale: 1, tilt: [0, 0] };
check("AB a part's blend: Normal without one, Additive; anything else refused, a shape's on its shape; isAdditivePart; compositionText writes it after the tint, never Normal", () => {
  const part = (extra) => F.readPart(Object.assign({ model: "plc_a03", at: [0, 0, 0] }, extra), "stages.small[0]", BASE);
  assert.deepStrictEqual([part(), part({ blend: null }), part({ blend: "normal" }), part({ blend: "additive" })].map((p) => p.blend), ["normal", "normal", "normal", "additive"]);
  assert.strictEqual(F.readPart({ model: ["wswss_b_012", "wswss_m_012", "wswss_t_012"], at: [0, 0, 0], blend: "additive" }, "p", BASE).blend, "additive");
  assert.strictEqual(F.readPart({ file: "parts/x.mdl", at: [0, 0, 0], blend: "additive" }, "p", BASE).blend, "additive");
  for (const bad of ["Additive", "add", "", 1, true, [], {}]) {
    assert.throws(() => part({ blend: bad }), (e) => e.message === "stages.small[0].blend: additive or normal", JSON.stringify(bad));
  }
  for (const blend of ["additive", "normal"]) {
    assert.throws(() => F.readPart({ shape: { type: "dome", radius: 1 }, at: [0, 0, 0], blend }, "stages.small[0]", BASE),
      (e) => e.message === "stages.small[0].blend: a shape's blend is on its shape (its own Blend: shape.blend)", blend);
  }
  assert.strictEqual(F.readPart({ shape: { type: "dome", radius: 1, blend: "additive" }, at: [0, 0, 0] }, "p", BASE).blend, "normal");
  assert.deepStrictEqual([part({ blend: "additive" }), part(), { model: "x", blend: "additive" }, { model: "x" }, { file: "a.mdl", blend: "additive" },
    { shape: { type: "dome", radius: 1 }, blend: "additive" }, { source: { kind: "shape" }, blend: "additive" }, { model: "x", blend: "Additive" }, null].map(F.isAdditivePart),
  [true, false, true, false, true, false, false, false, false]);
  assert.deepStrictEqual(F.PART_BLENDS, ["normal", "additive"]);
  assert.deepStrictEqual(F.PART_BLEND_INFO.choices.map((c) => c[0]), F.PART_BLENDS);
  const text = F.compositionText({ disk: { diameter: 0.9 }, stages: { small: [{ blend: "additive", motion: { turn: 30 }, model: "plc_a03", at: [0, 0, 0], tint: [0.5, 1, 1] },
    { model: "plc_b04", at: [0, 0, 0], blend: "normal" }, { model: "plc_b06", at: [0, 0, 0], blend: null }], medium: [], large: [] } });
  assert.ok(text.includes('      { "model": "plc_a03", "at": [0, 0, 0], "tint": [0.5, 1, 1], "blend": "additive", "motion": { "turn": 30 } },'), text);
  assert.ok(text.includes('      { "model": "plc_b04", "at": [0, 0, 0] },') && text.includes('      { "model": "plc_b06", "at": [0, 0, 0] }'), text);
});
check("AB placePart: an Additive part's textured meshes lit by themselves - selfillum its tint (white without one), ambient and diffuse 0 - and marked additive; an untextured one self-lit by its own colour, unmarked; a spell mesh's lifted first; Normal and a shape as before", () => {
  const place = (extra, planes, model = tinted()) => F.placePart(Object.assign({}, AB_AT, extra), model, 0, 0, "j", planes).meshes;
  const add = place({ blend: "additive", tint: [0.5, 1, 0.25] });
  // The box (textured) at its tint; the lid (untextured: diffuse 1 1 1, selfillum 0.123456 0 0) its tinted diffuse plus its
  // tinted glow, at most 1 a channel: 0.5 + 0.06173, 1, 0.25.
  assert.deepStrictEqual(add.map(material), [[[0, 0, 0], [0, 0, 0], [0.5, 1, 0.25]], [[0, 0, 0], [0, 0, 0], [0.56173, 1, 0.25]]]);
  assert.deepStrictEqual(add.map((m) => m.additive), [true, undefined]);
  assert.ok(!("additive" in add[1]), "the untextured mesh carries no mark");
  // Everything else as a Normal part's placed so (its alpha, its hint, its place).
  const strip = (m) => { const c = withoutMaterial(m); delete c.additive; return c; };
  assert.deepStrictEqual(add.map(strip), place({ tint: [0.5, 1, 0.25] }).map(strip));
  // No tint: white - the box at 1 1 1; the lid's 1 + 0.123456 held at 1.
  assert.deepStrictEqual(place({ blend: "additive" }).map((m) => m.selfillum), [[1, 1, 1], [1, 1, 1]]);
  // A spell mesh's untextured mesh at the draft's defaults is lifted first (the game draws such a plane at full): its tint.
  const plain = { meshes: [{ name: "gem", bitmap: "null", verts: [0, 0, 0, 1, 0, 0, 0, 1, 0], tverts: [0, 0, 1, 0, 0, 1], faces: [0, 1, 2], diffuse: [0.8, 0.8, 0.8],
    ambient: [0.2, 0.2, 0.2], selfillum: [0, 0, 0], alpha: 1 }], lo: [0, 0, 0], hi: [1, 1, 0] };
  assert.deepStrictEqual(place({ blend: "additive", tint: [0.5, 1, 0.25] }, true, plain).map(material), [[[0, 0, 0], [0, 0, 0], [0.5, 1, 0.25]]]);
  assert.deepStrictEqual(place({ blend: "additive", tint: [0.5, 1, 0.25] }, false, plain).map(material), [[[0, 0, 0], [0, 0, 0], [0.4, 0.8, 0.2]]]);
  // Untinted too: lifted for its blend alone - full (1 1 1) on a spell mesh, its 0.8 on a placeable's model.
  assert.deepStrictEqual(place({ blend: "additive" }, true, plain).map(material), [[[0, 0, 0], [0, 0, 0], [1, 1, 1]]]);
  assert.deepStrictEqual(place({ blend: "additive" }, false, plain).map(material), [[[0, 0, 0], [0, 0, 0], [0.8, 0.8, 0.8]]]);
  // Normal (none, or said) as before: no mark, the tinted material; a shape's part-level blend (the page's) changes nothing.
  for (const extra of [{}, { blend: "normal" }]) {
    const n = place(Object.assign({ tint: [0.5, 1, 0.25] }, extra), true);
    assert.deepStrictEqual(n, place({ tint: [0.5, 1, 0.25] }, true));
    assert.ok(n.every((m) => !("additive" in m)));
  }
  const s = F.readShape({ type: "dome", radius: 1, texture: "fxpa_globe" }, "s");
  assert.deepStrictEqual(F.placePart(Object.assign({ shape: s, blend: "additive" }, AB_AT), F.shapeModel(s), 0, 0, "j", true).meshes,
    F.placePart(Object.assign({ shape: s }, AB_AT), F.shapeModel(s), 0, 0, "j", true).meshes);
  // addedLight itself: copies, never -0; a mesh without a material self-lit from nothing.
  const given = [{ name: "a", bitmap: "x", selfillum: [0, 0, 0] }, { name: "b", bitmap: "" }];
  const lit = F.addedLight(given, [-0, 0.5, 1]);
  assert.ok(lit.every((m, i) => m !== given[i]) && !("additive" in given[0]));
  assert.ok(!lit[0].selfillum.some((v) => Object.is(v, -0)), JSON.stringify(lit[0].selfillum));
  assert.deepStrictEqual(lit.map(material), [[[0, 0, 0], [0, 0, 0], [0, 0.5, 1]], [[0, 0, 0], [0, 0, 0], [0, 0, 0]]]);
});
check("AB additiveTxi: the source's lines but its decal and blending, in its own line ends, then blending additive, then its clamp; never empty; copiedTxi without it as before", () => {
  assert.strictEqual(F.additiveTxi(FLAME_TXI), "proceduretype cycle\r\nnumx 4\r\nnumy 4\r\nfps 32\r\ndownsamplemax 0\r\ndownsamplemin 0\r\nblending additive\r\n");
  assert.strictEqual(F.additiveTxi(SYM_TXI), "downsamplemax 0\r\ndownsamplemin 0\r\nblending additive\r\n");
  for (const none of ["", null, undefined, Buffer.alloc(0)]) assert.strictEqual(F.additiveTxi(none), "blending additive\n", JSON.stringify(none));
  assert.strictEqual(F.additiveTxi("envmaptexture CM_Baremetal\r\nblending punchthrough\r\nBlending\r\n\tBLENDING   normal\r\nclamp 1\r\n\r\n"),
    "envmaptexture CM_Baremetal\r\nblending additive\r\nclamp 1\r\n");
  assert.strictEqual(F.additiveTxi(Buffer.from("blending normal\nmipmap 0\n", "latin1")), "mipmap 0\nblending additive\n");
  assert.strictEqual(F.additiveTxi("blendingx 1\nnoblending 1\n"), "blendingx 1\nnoblending 1\nblending additive\n", "not blending lines");
  for (const s of [FLAME_TXI, SYM_TXI, "", "envmaptexture cm_metal\r\nblending normal\r\n", "decal 1\n"]) {
    const t = F.additiveTxi(s);
    assert.deepStrictEqual([F.txiAdditive(t), F.txiDecal(t), t.split(/\r?\n/).filter((l) => /^[ \t]*blending/i.test(l)).length], [true, false, 1], JSON.stringify(s));
    // The page names a moving copy's material by its texture's X-Motion-Txi (motionTxi of its TXI): the same TXI comes of it.
    assert.strictEqual(F.motionTxi(F.additiveTxi(F.motionTxi(s) || "")), F.motionTxi(t), JSON.stringify(s));
    for (const clamp of [0, 2, 3]) assert.strictEqual(F.copiedTxi(s, clamp, { additive: false }), F.copiedTxi(s, clamp), JSON.stringify(s));
  }
  assert.strictEqual(F.copiedTxi(FLAME_TXI, 2, { additive: true }), "proceduretype cycle\r\nnumx 4\r\nnumy 4\r\nfps 32\r\ndownsamplemax 0\r\ndownsamplemin 0\r\nblending additive\r\nclamp 2\r\n");
});
check("AB copyName: an Additive copy's n5l_ name hashes the additive mark - never DC's for the same texture, clamp and key; without it every name as before", () => {
  const key = DC_KEY.fxpa_flame02, add = F.copyName("fxpa_flame02", 0, key, true);
  assert.strictEqual(add, "n5l_5d105670235b");  // pinned: the rule's text is the name
  assert.strictEqual(F.copyName("fxpa_flame02", 0, key), "n5l_457cbc7cb0a0");  // DC's, as before AB
  assert.ok(F.COPY_TEXTURE.test(add));
  assert.strictEqual(F.copyName("FXPA_Flame02", 0, key, true), add);
  assert.strictEqual(F.copyName("fxpa_flame02", 0, key, false), F.copyName("fxpa_flame02", 0, key));
  const names = [add, F.copyName("fxpa_flame02", 0, key), F.copyName("fxpa_flame02", 2, key), F.copyName("fxpa_flame02", 2, key, true),
    F.copyName("fxpa_flame02", 0, DC_KEY.fxpa_symbl_003, true), F.copyName("fxpa_symbl_003", 0, key, true)];
  assert.strictEqual(new Set(names).size, names.length, names.join(" "));
  for (const k of [undefined, null, ""]) assert.throws(() => F.copyName("fxpa_flame02", 0, k, true), /copyName: fxpa_flame02 - a copy's key/);
});
check("AB addedTextures: an Additive part's textured meshes name their copies (its key known), each once; untextured, a copy or a material, an unknown texture as they are; twice is once; a dyed part refused; Normal and a shape as they are", () => {
  // (A copy's name and a material's known too - the page's loader reads them from where they're built: never copied again.)
  const KEYS = { plc_x: { key: "k1" }, plc_y: { key: "k2" }, plc_nokey: { key: "" }, n5l_0123456789ab: { key: "k3" }, n5u_0123456789ab: { key: "k4" } };
  const info = (t) => KEYS[t] || null;
  const mesh = (name, bitmap) => ({ name, bitmap, verts: [0, 0, 0], tverts: [0, 0], faces: [0, 0, 0] });
  const meshes = [mesh("a", "PLC_X"), mesh("b", ""), mesh("c", "null"), mesh("d", "plc_x"), mesh("e", "plc_y"), mesh("f", "plc_z"), mesh("g", "plc_nokey"),
    mesh("h", "n5l_0123456789ab"), mesh("i", "n5u_0123456789ab")];
  const x = F.copyName("plc_x", 0, "k1", true), y = F.copyName("plc_y", 0, "k2", true);
  assert.strictEqual(x, "n5l_890a937fc0f7");
  for (const part of [{ model: "plc_a03", blend: "additive" }, F.readPart({ model: "plc_a03", at: [0, 0, 0], blend: "additive" }, "p", BASE)]) {
    const r = F.addedTextures(part, meshes, info);
    assert.deepStrictEqual(r.meshes.map((m) => m.bitmap), [x, "", "null", x, y, "plc_z", "plc_nokey", "n5l_0123456789ab", "n5u_0123456789ab"]);
    assert.deepStrictEqual(r.copies, [{ name: x, texture: "plc_x", clamp: 0, key: "k1", additive: true }, { name: y, texture: "plc_y", clamp: 0, key: "k2", additive: true }]);
    assert.strictEqual(meshes[0].bitmap, "PLC_X", "the given meshes untouched");
    assert.deepStrictEqual(F.addedTextures(part, r.meshes, info), { meshes: r.meshes, copies: [], whites: [] }, "twice is once");
  }
  // Not known yet (the page's: undefined) or no info at all: as it is.
  assert.deepStrictEqual(F.addedTextures({ model: "m", blend: "additive" }, meshes, () => undefined).meshes.map((m) => m.bitmap), meshes.map((m) => m.bitmap));
  assert.deepStrictEqual(F.addedTextures({ model: "m", blend: "additive" }, meshes).copies, []);
  // A dyed part's bake: refused, saying so.
  const bake = F.bakedName("helm_001", {});
  assert.throws(() => F.addedTextures({ model: "helm_001", blend: "additive" }, [mesh("a", bake)], info),
    (e) => e.message === `${bake}: a dyed texture (the game's PLT baked in the part's colours) - a dyed part can't be Additive: set its Blend to Normal`);
  // Normal, a shape: the meshes given, nothing copied (a dyed Normal part as it is).
  for (const part of [{ model: "m" }, { model: "m", blend: "normal" }, { shape: { type: "dome", radius: 1 }, blend: "additive" }, null]) {
    const r = F.addedTextures(part, meshes.concat(mesh("z", bake)), info);
    assert.deepStrictEqual(r.copies, [], JSON.stringify(part));
    assert.deepStrictEqual(r.meshes.map((m) => m.bitmap), meshes.concat(mesh("z", bake)).map((m) => m.bitmap));
  }
});
check("AB files.js: an Additive game part's textures copied for added light - its picture (the DDS when it has one) and its additiveTxi under its own n5l_ name, kind copy; a moving one's material names the copy, its TXI blending additive; a breathing one's W1 copy of its copy; a dyed one refused naming it; Normal and no reader as before", () => {
  const fs = require("fs");
  const os = require("os");
  const W = require("./files.js");
  const tri = (name, bitmap, z) => ({ name, bitmap, verts: [0, 0, z, 1, 0, z, 0, 1, z], tverts: [0, 0, 1, 0, 0, 1], faces: [0, 1, 2], diffuse: [1, 1, 1],
    ambient: [1, 1, 1], selfillum: [0, 0, 0], alpha: 1, transparencyhint: 0 });
  const model = () => ({ meshes: [tri("blade", "w_blade", 0), tri("hilt", "w_hilt", 0.1), tri("gem", "", 0.2)], lo: [0, 0, 0], hi: [1, 1, 0.2] });
  const BLADE_TXI = "envmaptexture cm_metal\r\nblending normal\r\n";
  const IMAGES = { w_blade: { where: "game", tga: Buffer.from("blade-tga"), dds: Buffer.from("blade-dds"), plt: null, txi: BLADE_TXI },
    w_hilt: { where: "game", tga: Buffer.from("hilt-tga"), dds: null, plt: null, txi: "" } };
  const rd = { image: (name) => IMAGES[name] || null };
  const read = (extra) => F.readPart(Object.assign({ model: ["wswss_b_012", "wswss_m_012", "wswss_t_012"], at: [0, 0, 0] }, extra), "p", BASE);
  const placedOf = (parts) => parts.map((p) => ({ part: p, meshes: F.placePart(p, model(), 0, 0, "j", true).meshes }));
  const placed = placedOf([read({ blend: "additive", tint: [0.72, 0.86, 1] }), read({})]);
  const copies = W.copyTextures(placed, new Map(), { rd });
  const bn = F.copyName("w_blade", 0, F.copyKey(BLADE_TXI, Buffer.from("blade-dds")), true), hn = F.copyName("w_hilt", 0, F.copyKey("", Buffer.from("hilt-tga")), true);
  assert.deepStrictEqual(placed.map((x) => x.meshes.map((m) => m.bitmap)), [[bn, hn, ""], ["w_blade", "w_hilt", ""]]);
  assert.deepStrictEqual(placed[0].meshes.map((m) => m.selfillum), [[0.72, 0.86, 1], [0.72, 0.86, 1], [0.72, 0.86, 1]]);
  assert.deepStrictEqual([...copies.keys()], [bn, hn]);
  assert.deepStrictEqual([...copies.values()].map((c) => [c.texture, c.clamp, c.additive, c.decal, c.ext, c.bytes.toString(), c.txi.toString("latin1"), c.label]), [
    ["w_blade", 0, true, false, ".dds", "blade-dds", "envmaptexture cm_metal\r\nblending additive\r\n", "wswss_b_012 + wswss_m_012 + wswss_t_012"],
    ["w_hilt", 0, true, false, ".tga", "hilt-tga", "blending additive\n", "wswss_b_012 + wswss_m_012 + wswss_t_012"]]);
  assert.deepStrictEqual(W.copyJson(copies), [{ copy: bn, texture: "w_blade", clamp: 0, additive: true }, { copy: hn, texture: "w_hilt", clamp: 0, additive: true }]);
  assert.deepStrictEqual(W.copyLines(copies, "saved"), [
    `  texture ${bn}: w_blade copied for added light, an Additive part's (${bn}.dds, ${bn}.txi: its TXI without blending, and blending additive) - written into saved`,
    `  texture ${hn}: w_hilt copied for added light, an Additive part's (${hn}.tga, ${hn}.txi: its TXI without blending, and blending additive) - written into saved`]);
  // A decal texture's says so.
  const decal = placedOf([read({ blend: "additive" })]);
  const dc = W.copyTextures(decal, new Map(), { rd: { image: (n) => Object.assign({}, IMAGES[n] || IMAGES.w_hilt, { txi: "decal 1\r\n" }) } });
  assert.ok(W.copyLines(dc, "x")[0].includes(".txi: its TXI without decal or blending, and blending additive)"), W.copyLines(dc, "x")[0]);
  assert.deepStrictEqual([...dc.values()].map((c) => [c.decal, c.txi.toString("latin1")]), [[true, "blending additive\r\n"], [true, "blending additive\r\n"]]);
  // The writes: each copy's picture and TXI under its name, kind copy, owned by the n5l_ name.
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "n5_fdab-"));
  try {
    const writes = W.copyWrites(copies, dir);
    assert.deepStrictEqual(writes.map((w) => [path.basename(w.file), w.kind]), [[bn + ".dds", "copy"], [bn + ".txi", "copy"], [hn + ".tga", "copy"], [hn + ".txi", "copy"]]);
    const owned = W.ownership({ repo: dir, manifest: new Set(), mark: F.MARK });
    assert.ok(writes.every((w) => owned(w, Buffer.from("x"))));
    W.writeAll(writes, owned, { repo: dir, manifest: new Set() });
    assert.strictEqual(fs.readFileSync(path.join(dir, bn + ".txi"), "latin1"), "envmaptexture cm_metal\r\nblending additive\r\n");
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
  // Moving (a turn): its material names the copy as texture0, its own TXI the copy's map and blending lines.
  const moving = placedOf([read({ blend: "additive", motion: { turn: 30 } })]);
  const mc = W.copyTextures(moving, new Map(), { rd });
  const mats = W.motionMaterials(moving, new Map(), { sources: { rd }, copies: mc });
  assert.deepStrictEqual([...mats.values()].map((m) => [m.texture, m.txi || null, m.text.split("\n").includes("texture0 " + m.texture)]),
    [[bn, "envmaptexture cm_metal\nblending additive\n", true], [hn, "blending additive\n", true]]);
  // Breathing within one repeat (W1): its clamped material's copy is of its own copy - its TXI's blending additive kept.
  const breathing = placedOf([read({ blend: "additive", motion: { breathe: 0.3 } })]);
  const bc = W.copyTextures(breathing, new Map(), { rd });
  const bm = W.motionCopies(W.motionMaterials(breathing, new Map(), { sources: { rd }, copies: bc }), { rd, copies: bc });
  assert.deepStrictEqual([...bm.values()].map((m) => [m.texture, m.clamp, m.copy.ext, m.copy.bytes.toString(), m.copy.txi.toString("latin1")]), [
    [bn, true, ".dds", "blade-dds", "envmaptexture cm_metal\r\nblending additive\r\nclamp 3\r\n"], [hn, true, ".tga", "hilt-tga", "blending additive\nclamp 3\n"]]);
  // A dyed part's bake: the build stops, naming the part.
  const dyed = [{ part: read({ blend: "additive" }), meshes: [tri("helm", F.bakedName("helm_001", {}), 0)] }];
  assert.throws(() => W.copyTextures(dyed, new Map(), { rd, baked: new Map() }), /^Error: wswss_b_012 \+ wswss_m_012 \+ wswss_t_012: n5p_[0-9a-f]{12}: a dyed texture .* a dyed part can't be Additive/);
  // No reader: nothing copied, the meshes as placed (still lit by themselves).
  const bare = placedOf([read({ blend: "additive" })]);
  assert.strictEqual(W.copyTextures(bare, new Map(), {}).size, 0);
  assert.deepStrictEqual(bare[0].meshes.map((m) => m.bitmap), ["w_blade", "w_hilt", ""]);
});
check("AB in a page: the same blend, light, TXI, names and rule", () => {
  const P = pageFloatDisk();
  assert.deepStrictEqual([P.PART_BLENDS, P.PART_BLEND_INFO], [F.PART_BLENDS, F.PART_BLEND_INFO]);
  const raw = { model: "plc_a03", blend: "additive", tint: [0.5, 1, 0.25] };
  assert.deepStrictEqual([P.isAdditivePart(raw), P.isAdditivePart({ model: "x" })], [true, false]);
  assert.deepStrictEqual(P.placePart(Object.assign({}, AB_AT, raw), tinted(), 0, 0, "j", true).meshes, F.placePart(Object.assign({}, AB_AT, raw), tinted(), 0, 0, "j", true).meshes);
  assert.deepStrictEqual([P.additiveTxi(FLAME_TXI), P.copyName("fxpa_flame02", 0, DC_KEY.fxpa_flame02, true)], [F.additiveTxi(FLAME_TXI), "n5l_5d105670235b"]);
  const info = (t) => (t === "plc_x" ? { key: "k1" } : null);
  assert.deepStrictEqual(P.addedTextures(raw, tinted().meshes, info), F.addedTextures(raw, tinted().meshes, info));
});

// --- TX (#39; the owner, 2026-11-23, of the shortsword's brass texture times the blue tint reading blue-green: "cas we just
// use the mesh, change the texture to something white and tint that?"): a part's Texture - Own, Greyscale (a grey copy of
// each texture: its luminance, Rec. 601, its alpha kept) or White (the plain white of our own, or a white copy keeping an
// alpha that cuts the mesh out) - so its Tint is its colour; with Blend Additive the copy's TXI says blending additive. ----
// A 2 x 2 picture (rows top-down): red, green, blue (alpha 128), a dark grey-blue (alpha 0).
const TX_PIC = () => ({ width: 2, height: 2, data: Uint8ClampedArray.from([255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 128, 10, 20, 30, 0]) });
const TX_OPAQUE = () => ({ width: 2, height: 1, data: Uint8ClampedArray.from([255, 255, 255, 255, 0, 0, 0, 255]) });
check("TX a part's texture: Own without one, Greyscale, White; anything else refused, a shape's on its shape; partLook; compositionText writes it after the tint and before the blend, never Own", () => {
  const part = (extra) => F.readPart(Object.assign({ model: "plc_a03", at: [0, 0, 0] }, extra), "stages.small[0]", BASE);
  assert.deepStrictEqual([part(), part({ texture: null }), part({ texture: "own" }), part({ texture: "greyscale" }), part({ texture: "white" })].map((p) => p.texture),
    ["own", "own", "own", "greyscale", "white"]);
  assert.strictEqual(F.readPart({ file: "parts/x.mdl", at: [0, 0, 0], texture: "greyscale" }, "p", BASE).texture, "greyscale");
  for (const bad of ["Greyscale", "grey", "", 1, true, [], {}]) {
    assert.throws(() => part({ texture: bad }), (e) => e.message === "stages.small[0].texture: own, greyscale or white", JSON.stringify(bad));
  }
  for (const texture of ["greyscale", "white", "own"]) {
    assert.throws(() => F.readPart({ shape: { type: "dome", radius: 1 }, at: [0, 0, 0], texture }, "stages.small[0]", BASE),
      (e) => e.message === "stages.small[0].texture: a shape's texture is on its shape (its own Texture: shape.texture)", texture);
  }
  assert.strictEqual(F.readPart({ shape: { type: "dome", radius: 1, texture: "none" }, at: [0, 0, 0] }, "p", BASE).texture, "own");
  assert.deepStrictEqual([part({ texture: "greyscale" }), part({ texture: "white" }), part(), { model: "x", texture: "white" }, { file: "a.mdl", texture: "greyscale" },
    { model: "x", texture: "own" }, { shape: { type: "dome", radius: 1 }, texture: "white" }, { source: { kind: "shape" }, texture: "greyscale" }, { model: "x", texture: "Grey" }, null]
    .map(F.partLook), ["greyscale", "white", "own", "white", "greyscale", "own", "own", "own", "own", "own"]);
  assert.deepStrictEqual([F.PART_TEXTURES, F.PART_TEXTURE_INFO.choices.map((c) => c[0])], [["own", "greyscale", "white"], ["own", "greyscale", "white"]]);
  const text = F.compositionText({ disk: { diameter: 0.9 }, stages: { small: [{ blend: "additive", texture: "greyscale", model: "plc_a03", at: [0, 0, 0], tint: [0.5, 1, 1] },
    { model: "plc_b04", at: [0, 0, 0], texture: "own" }, { model: "plc_b06", at: [0, 0, 0], texture: null }, { model: "plc_b05", at: [0, 0, 0], texture: "white" }], medium: [], large: [] } });
  assert.ok(text.includes('      { "model": "plc_a03", "at": [0, 0, 0], "tint": [0.5, 1, 1], "texture": "greyscale", "blend": "additive" },'), text);
  assert.ok(text.includes('      { "model": "plc_b04", "at": [0, 0, 0] },') && text.includes('      { "model": "plc_b06", "at": [0, 0, 0] },') &&
    text.includes('      { "model": "plc_b05", "at": [0, 0, 0], "texture": "white" }'), text);
});
check("TX the pictures: greyPicture each pixel its luminance (Rec. 601: 0.299 R + 0.587 G + 0.114 B, rounded), its alpha kept, white white and black black; whitePicture white, its alpha kept; hasAlpha; lookTga 32-bit with alpha, 24-bit without, rows bottom-up; whiteKeepsAlpha", () => {
  assert.deepStrictEqual(F.LUMA, [0.299, 0.587, 0.114]);
  const src = TX_PIC(), grey = F.greyPicture(src);
  assert.deepStrictEqual(Array.from(grey.data), [76, 76, 76, 255, 150, 150, 150, 255, 29, 29, 29, 128, 18, 18, 18, 0]);
  assert.deepStrictEqual([grey.width, grey.height], [2, 2]);
  assert.deepStrictEqual(Array.from(src.data.slice(0, 4)), [255, 0, 0, 255], "the source untouched");
  assert.deepStrictEqual(Array.from(F.greyPicture(TX_OPAQUE()).data), [255, 255, 255, 255, 0, 0, 0, 255]);
  assert.deepStrictEqual(Array.from(F.whitePicture(src).data), [255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 128, 255, 255, 255, 0]);
  assert.deepStrictEqual([F.hasAlpha(src), F.hasAlpha(TX_OPAQUE()), F.hasAlpha(F.whiteTexture())], [true, false, false]);
  // The TGA: 32-bit (8 alpha bits) for a source whose alpha is used - its bottom row first, BGRA; 24-bit, BGR, without.
  const g = F.lookTga("greyscale", src), w = F.lookTga("white", TX_OPAQUE());
  assert.deepStrictEqual([g.length, g[2], g[16], g[17]], [18 + 16, 2, 32, 8]);
  assert.deepStrictEqual(Array.from(g.subarray(18)), [29, 29, 29, 128, 18, 18, 18, 0, 76, 76, 76, 255, 150, 150, 150, 255]);
  assert.deepStrictEqual([w.length, w[16], w[17], ...w.subarray(18)], [18 + 6, 24, 0, 255, 255, 255, 255, 255, 255]);
  assert.ok(F.lookTga("white", src).subarray(18).every((v, i) => (i % 4 === 3 ? true : v === 255)));
  // The plain white: 8 x 8, opaque; its TGA 24-bit, every byte 255.
  const plain = F.tgaBytes(F.whiteTexture());
  assert.deepStrictEqual([F.WHITE_SIZE, plain.length, plain[16], plain.subarray(18).every((v) => v === 255)], [8, 18 + 8 * 8 * 3, 24, true]);
  // White keeps the alpha only where it's used and no environment map makes it shine (w_metal_tex's: envmaptexture).
  const METAL_TXI = "# Specify an environment map for static geometry.\r\n\r\nenvmaptexture TTR01__ref01\r\nalphamean 0.364\r\n";
  assert.deepStrictEqual([F.whiteKeepsAlpha("", src), F.whiteKeepsAlpha(METAL_TXI, src), F.whiteKeepsAlpha(Buffer.from(METAL_TXI), src), F.whiteKeepsAlpha("", TX_OPAQUE()),
    F.whiteKeepsAlpha("blending punchthrough\n", src), F.whiteKeepsAlpha("envmaptexture CM_Baremetal\n", src), F.whiteKeepsAlpha("# envmaptexture x\n", src)],
  [true, false, false, false, true, false, true]);
});
check("TX the names: a look's copy hashes its look after the additive mark - Greyscale, White, Additive + Greyscale all their own, never DC's or AB's; Own as before; the plain white one name a blend (n5s_), its TXI blending additive when Additive", () => {
  const key = DC_KEY.fxpa_flame02;
  assert.deepStrictEqual([F.copyName("fxpa_flame02", 0, key, { look: "greyscale" }), F.copyName("fxpa_flame02", 0, key, { look: "white" }),
    F.copyName("fxpa_flame02", 0, key, { additive: true, look: "greyscale" })], ["n5l_3f7b378fbe34", "n5l_28e0e5add308", "n5l_2995293c1a6a"]);  // pinned
  assert.deepStrictEqual([F.copyName("fxpa_flame02", 0, key, { additive: true }), F.copyName("fxpa_flame02", 0, key, { additive: true, look: "own" }),
    F.copyName("fxpa_flame02", 0, key, { look: "own" }), F.copyName("fxpa_flame02", 0, key, {})], ["n5l_5d105670235b", "n5l_5d105670235b", "n5l_457cbc7cb0a0", "n5l_457cbc7cb0a0"]);
  const names = ["greyscale", "white"].flatMap((look) => [false, true].map((additive) => F.copyName("fxpa_flame02", 0, key, { additive, look })))
    .concat([F.copyName("fxpa_flame02", 0, key), F.copyName("fxpa_flame02", 0, key, true), F.copyName("fxpa_flame02", 2, key)]);
  assert.strictEqual(new Set(names).size, names.length, names.join(" "));
  assert.deepStrictEqual([F.whiteName(), F.whiteName({ additive: true }), F.whiteName({ additive: false })], ["n5s_0cf5986cd86c", "n5s_372d77d112b9", "n5s_0cf5986cd86c"]);
  assert.ok(F.SHAPE_TEXTURE.test(F.whiteName()) && F.SHAPE_TEXTURE.test(F.whiteName({ additive: true })));
  assert.deepStrictEqual([F.whiteTxi(), F.whiteTxi({ additive: true })], [null, "blending additive\n"]);
  // A copy's TXI by its blend: Normal its lines minus decal (its blending kept); Additive additiveTxi's.
  assert.deepStrictEqual([F.partCopyTxi(SYM_TXI), F.partCopyTxi(SYM_TXI, { additive: true }), F.partCopyTxi("blending punchthrough\r\n")],
    ["blending additive\r\ndownsamplemax 0\r\ndownsamplemin 0\r\n", "downsamplemax 0\r\ndownsamplemin 0\r\nblending additive\r\n", "blending punchthrough\r\n"]);
});
check("TX partTextures: Greyscale names grey copies, White the plain white (its alpha not used, or its shine) or a white copy keeping its alpha, each once; Additive + Greyscale one name with both marks; not known as it is; twice is once; a dyed part White the plain white, Greyscale refused; Own and Normal, a shape as they are", () => {
  const INFO = { plc_x: { key: "k1", alpha: false }, plc_y: { key: "k2", alpha: true }, plc_z: { key: "k3" }, plc_nokey: { key: "", alpha: true } };
  const info = (t) => INFO[t] || null;
  const mesh = (name, bitmap) => ({ name, bitmap, verts: [0, 0, 0], tverts: [0, 0], faces: [0, 0, 0] });
  const meshes = [mesh("a", "PLC_X"), mesh("b", ""), mesh("c", "plc_y"), mesh("d", "plc_x"), mesh("e", "plc_z"), mesh("f", "plc_q"), mesh("g", "plc_nokey")];
  const bits = (r) => r.meshes.map((m) => m.bitmap);
  const grey = F.partTextures({ model: "m", texture: "greyscale" }, meshes, info), gx = F.copyName("plc_x", 0, "k1", { look: "greyscale" });
  const gy = F.copyName("plc_y", 0, "k2", { look: "greyscale" }), gz = F.copyName("plc_z", 0, "k3", { look: "greyscale" });
  assert.deepStrictEqual(bits(grey), [gx, "", gy, gx, gz, "plc_q", "plc_nokey"]);
  assert.deepStrictEqual(grey.copies, [{ name: gx, texture: "plc_x", clamp: 0, key: "k1", look: "greyscale" }, { name: gy, texture: "plc_y", clamp: 0, key: "k2", look: "greyscale" },
    { name: gz, texture: "plc_z", clamp: 0, key: "k3", look: "greyscale" }]);
  assert.deepStrictEqual(grey.whites, []);
  // White: plc_x (alpha not used) the plain white, plc_y (used) its white copy, plc_z (not known) as it is.
  const white = F.partTextures(F.readPart({ model: "m", at: [0, 0, 0], texture: "white" }, "p", BASE), meshes, info), W = F.whiteName(), wy = F.copyName("plc_y", 0, "k2", { look: "white" });
  assert.deepStrictEqual(bits(white), [W, "", wy, W, "plc_z", "plc_q", "plc_nokey"]);
  assert.deepStrictEqual([white.copies, white.whites], [[{ name: wy, texture: "plc_y", clamp: 0, key: "k2", look: "white" }], [{ name: W }]]);
  // Additive + Greyscale: one copy a texture, both marks; Additive + White: the Additive plain white.
  const ag = F.partTextures({ model: "m", texture: "greyscale", blend: "additive" }, meshes, info), agx = F.copyName("plc_x", 0, "k1", { additive: true, look: "greyscale" });
  assert.deepStrictEqual([bits(ag)[0], ag.copies[0]], [agx, { name: agx, texture: "plc_x", clamp: 0, key: "k1", additive: true, look: "greyscale" }]);
  const aw = F.partTextures({ model: "m", texture: "white", blend: "additive" }, meshes, info);
  assert.deepStrictEqual([bits(aw)[0], aw.whites], [F.whiteName({ additive: true }), [{ name: F.whiteName({ additive: true }), additive: true }]]);
  // Twice is once: a copy's name or the plain white's as it is.
  for (const r of [grey, white, ag, aw]) {
    const named = r.meshes.filter((m) => /^n5[ls]_/.test(m.bitmap));
    const again = F.partTextures({ model: "m", texture: "white", blend: "additive" }, named, () => ({ key: "kk", alpha: false }));
    assert.deepStrictEqual([again.meshes.map((m) => m.bitmap), again.copies, again.whites], [named.map((m) => m.bitmap), [], []]);
  }
  // A dyed part: White the plain white (a bake has no alpha: nothing of it needed), Greyscale refused, saying so.
  const bake = F.bakedName("helm_001", {});
  assert.deepStrictEqual(bits(F.partTextures({ model: "helm_001", texture: "white" }, [mesh("h", bake)], () => null)), [W]);
  assert.deepStrictEqual(bits(F.partTextures({ model: "helm_001", texture: "white", blend: "additive" }, [mesh("h", bake)], () => null)), [F.whiteName({ additive: true })]);
  for (const extra of [{}, { blend: "additive" }]) {
    assert.throws(() => F.partTextures(Object.assign({ model: "helm_001", texture: "greyscale" }, extra), [mesh("h", bake)], info),
      (e) => e.message === `${bake}: a dyed texture (the game's PLT baked in the part's colours) - a dyed part can't be Greyscale: set its Texture to White or Own`);
  }
  // Own and Normal, a shape's part-level texture, nothing: the meshes given.
  for (const part of [{ model: "m" }, { model: "m", texture: "own" }, { shape: { type: "dome", radius: 1 }, texture: "greyscale" }, null]) {
    const r = F.partTextures(part, meshes, info);
    assert.ok(r.meshes === meshes && !r.copies.length && !r.whites.length, JSON.stringify(part));
  }
  assert.strictEqual(F.addedTextures, F.partTextures);
});
check("TX files.js: a Greyscale part's grey copies (a DDS decoded, written as a TGA; its alpha kept), its TXI minus decal (Additive: blending additive); White the plain white (kind shape, a TXI only Additive) or a white copy keeping its alpha; once a texture; a breathing White part's W1 copy of the white; no decoder, a dyed Greyscale part refused", () => {
  const fs = require("fs");
  const os = require("os");
  const W = require("./files.js");
  const tri = (name, bitmap, z) => ({ name, bitmap, verts: [0, 0, z, 1, 0, z, 0, 1, z], tverts: [0, 0, 1, 0, 0, 1], faces: [0, 1, 2], diffuse: [1, 1, 1],
    ambient: [1, 1, 1], selfillum: [0, 0, 0], alpha: 1, transparencyhint: 0 });
  const model = () => ({ meshes: [tri("blade", "w_blade", 0), tri("leaf", "w_leaf", 0.1), tri("gem", "", 0.2)], lo: [0, 0, 0], hi: [1, 1, 0.2] });
  const BLADE_TXI = "envmaptexture cm_metal\r\ndecal 1\r\nblending normal\r\n", LEAF_TXI = "blending punchthrough\r\n";
  // w_blade: a DDS (its alpha its shine: an environment map); w_leaf: a TGA whose alpha cuts it out. Their pixels, decoded.
  const IMAGES = { w_blade: { where: "game", tga: Buffer.from("blade-tga"), dds: Buffer.from("blade-dds"), plt: null, txi: BLADE_TXI },
    w_leaf: { where: "game", tga: Buffer.from("leaf-tga"), dds: null, plt: null, txi: LEAF_TXI } };
  const PIXELS = { "blade-dds": TX_PIC(), "leaf-tga": TX_PIC() }, decodes = [];
  const rd = { image: (name) => IMAGES[name] || null }, decode = (pic) => { decodes.push(pic.bytes.toString()); return PIXELS[pic.bytes.toString()]; };
  const read = (extra) => F.readPart(Object.assign({ model: ["wswss_b_012", "wswss_m_012", "wswss_t_012"], at: [0, 0, 0] }, extra), "p", BASE);
  const placedOf = (parts) => parts.map((p) => ({ part: p, meshes: F.placePart(p, model(), 0, 0, "j", true).meshes }));
  const bk = F.copyKey(BLADE_TXI, Buffer.from("blade-dds")), lk = F.copyKey(LEAF_TXI, Buffer.from("leaf-tga"));
  const LABEL = "wswss_b_012 + wswss_m_012 + wswss_t_012";
  // Greyscale, Normal and Additive.
  const placed = placedOf([read({ texture: "greyscale" }), read({ texture: "greyscale", blend: "additive", tint: [0.72, 0.86, 1] }), read({})]);
  const copies = W.copyTextures(placed, new Map(), { rd, decode });
  const g = (t, k, additive) => F.copyName(t, 0, k, { additive, look: "greyscale" });
  assert.deepStrictEqual(placed.map((x) => x.meshes.map((m) => m.bitmap)), [[g("w_blade", bk, false), g("w_leaf", lk, false), ""], [g("w_blade", bk, true), g("w_leaf", lk, true), ""],
    ["w_blade", "w_leaf", ""]]);
  assert.deepStrictEqual([...decodes].sort(), ["blade-dds", "leaf-tga"], "each picture decoded once");
  const greyTga = F.lookTga("greyscale", TX_PIC());
  assert.deepStrictEqual([...copies].map(([n, c]) => [n, c.texture, c.look, c.additive === true, c.decal, c.ext, c.bytes.equals(greyTga), c.txi.toString("latin1"), c.label]), [
    [g("w_blade", bk, false), "w_blade", "greyscale", false, true, ".tga", true, "envmaptexture cm_metal\r\nblending normal\r\n", LABEL],
    [g("w_leaf", lk, false), "w_leaf", "greyscale", false, false, ".tga", true, LEAF_TXI, LABEL],
    [g("w_blade", bk, true), "w_blade", "greyscale", true, true, ".tga", true, "envmaptexture cm_metal\r\nblending additive\r\n", LABEL],
    [g("w_leaf", lk, true), "w_leaf", "greyscale", true, false, ".tga", true, "blending additive\r\n", LABEL]]);
  assert.deepStrictEqual(W.copyJson(copies).slice(0, 1).concat(W.copyJson(copies).slice(2, 3)), [{ copy: g("w_blade", bk, false), texture: "w_blade", clamp: 0, look: "greyscale" },
    { copy: g("w_blade", bk, true), texture: "w_blade", clamp: 0, additive: true, look: "greyscale" }]);
  assert.deepStrictEqual(W.copyLines(copies, "saved").slice(0, 2).concat(W.copyLines(copies, "saved").slice(3)), [
    `  texture ${g("w_blade", bk, false)}: w_blade in grey (its luminance), a Greyscale part's (${g("w_blade", bk, false)}.tga, ${g("w_blade", bk, false)}.txi: its TXI without decal) - written into saved`,
    `  texture ${g("w_leaf", lk, false)}: w_leaf in grey (its luminance), a Greyscale part's (${g("w_leaf", lk, false)}.tga, ${g("w_leaf", lk, false)}.txi: its TXI) - written into saved`,
    `  texture ${g("w_leaf", lk, true)}: w_leaf in grey (its luminance), a Greyscale part's, for added light (${g("w_leaf", lk, true)}.tga, ${g("w_leaf", lk, true)}.txi: its TXI ` +
      "without blending, and blending additive) - written into saved"]);
  // White: w_blade's alpha its shine (the plain white), w_leaf's cuts it out (its white copy); Additive: the Additive white.
  const whites = placedOf([read({ texture: "white" }), read({ texture: "white", blend: "additive" })]);
  const wc = W.copyTextures(whites, new Map(), { rd, decode });
  const WN = F.whiteName(), WA = F.whiteName({ additive: true }), wl = F.copyName("w_leaf", 0, lk, { look: "white" }), wla = F.copyName("w_leaf", 0, lk, { additive: true, look: "white" });
  assert.deepStrictEqual(whites.map((x) => x.meshes.map((m) => m.bitmap)), [[WN, wl, ""], [WA, wla, ""]]);
  const plain = F.tgaBytes(F.whiteTexture());
  assert.deepStrictEqual([...wc].map(([n, c]) => [n, c.white === true, c.look || null, c.bytes.equals(c.white ? plain : F.lookTga("white", TX_PIC())), c.txi ? c.txi.toString("latin1") : null]), [
    [wl, false, "white", true, LEAF_TXI], [WN, true, null, true, null], [wla, false, "white", true, "blending additive\r\n"], [WA, true, null, true, "blending additive\n"]]);
  assert.deepStrictEqual(W.copyJson(wc).filter((j) => j.white), [{ copy: WN, white: true }, { copy: WA, white: true, additive: true }]);
  assert.deepStrictEqual(W.copyLines(wc, "data/files").filter((l) => l.includes("plain white")), [`  texture ${WN}: the plain white (8 x 8, ours), a White part's - written into data/files`,
    `  texture ${WA}: the plain white (8 x 8, ours), a White part's, additive (${WA}.txi: blending additive) - written into data/files`]);
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "n5_fdtx-"));
  try {
    const writes = W.copyWrites(wc, dir);
    assert.deepStrictEqual(writes.map((w) => [path.basename(w.file), w.kind]), [[wl + ".tga", "copy"], [wl + ".txi", "copy"], [WN + ".tga", "shape"], [wla + ".tga", "copy"],
      [wla + ".txi", "copy"], [WA + ".tga", "shape"], [WA + ".txi", "shape"]]);
    const owned = W.ownership({ repo: dir, manifest: new Set(), mark: F.MARK });
    assert.ok(writes.every((w) => owned(w, Buffer.from("x"))));
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
  // Breathing within one repeat (W1): the White part's clamped materials' copies are of the plain white, clamp 3.
  const breathing = placedOf([read({ texture: "white", motion: { breathe: 0.3 } })]);
  const bc = W.copyTextures(breathing, new Map(), { rd, decode });
  const bm = W.motionCopies(W.motionMaterials(breathing, new Map(), { sources: { rd }, copies: bc }), { rd, copies: bc });
  assert.deepStrictEqual([...bm.values()].map((m) => [m.texture, m.clamp, m.copy.bytes.equals(m.texture === WN ? plain : F.lookTga("white", TX_PIC())), m.copy.txi.toString("latin1")]),
    [[WN, true, true, "clamp 3\n"], [wl, true, true, "blending punchthrough\r\nclamp 3\r\n"]]);
  // No decoder: a Texture part stops, naming it; a dyed Greyscale part too.
  assert.throws(() => W.copyTextures(placedOf([read({ texture: "greyscale" })]), new Map(), { rd }), /^Error: wswss_b_012 \+ wswss_m_012 \+ wswss_t_012: texture w_blade: a part's Texture needs its picture decoded/);
  const dyed = [{ part: read({ texture: "greyscale" }), meshes: [tri("helm", F.bakedName("helm_001", {}), 0)] }];
  assert.throws(() => W.copyTextures(dyed, new Map(), { rd, decode }), /^Error: wswss_b_012 \+ wswss_m_012 \+ wswss_t_012: n5p_[0-9a-f]{12}: .* can't be Greyscale/);
  // No reader: nothing copied, the meshes as placed.
  const bare = placedOf([read({ texture: "greyscale" })]);
  assert.strictEqual(W.copyTextures(bare, new Map(), { decode }).size, 0);
  assert.deepStrictEqual(bare[0].meshes.map((m) => m.bitmap), ["w_blade", "w_leaf", ""]);
  assert.strictEqual(typeof W.decoder({ decodeDds: () => "dds", decodeTga: () => "tga" }), "function");
  assert.deepStrictEqual([W.decoder({ decodeDds: () => "dds", decodeTga: () => "tga" })({ ext: ".dds", bytes: null }), W.decoder({ decodeDds: () => "dds", decodeTga: () => "tga" })({ ext: ".tga" })], ["dds", "tga"]);
});
check("TX in a page: the same texture reading, pictures, names and rule", () => {
  const P = pageFloatDisk();
  assert.deepStrictEqual([P.PART_TEXTURES, P.PART_TEXTURE_INFO, P.LUMA], [F.PART_TEXTURES, F.PART_TEXTURE_INFO, F.LUMA]);
  assert.deepStrictEqual(Array.from(P.greyPicture(TX_PIC()).data), Array.from(F.greyPicture(TX_PIC()).data));
  assert.deepStrictEqual(Array.from(P.whitePicture(TX_PIC()).data), Array.from(F.whitePicture(TX_PIC()).data));
  assert.deepStrictEqual(Array.from(P.lookTga("greyscale", TX_PIC())), Array.from(F.lookTga("greyscale", TX_PIC())));
  assert.deepStrictEqual([P.whiteName(), P.whiteKeepsAlpha("", TX_PIC()), P.copyName("x", 0, "k", { look: "white" }), P.partLook({ model: "m", texture: "white" })],
    [F.whiteName(), true, F.copyName("x", 0, "k", { look: "white" }), "white"]);
  const info = (t) => (t === "plc_x" ? { key: "k1", alpha: true } : null);
  for (const texture of ["greyscale", "white"]) {
    assert.deepStrictEqual(P.partTextures({ model: "m", texture }, tinted().meshes, info), F.partTextures({ model: "m", texture }, tinted().meshes, info));
  }
});

console.log(process.exitCode ? `floatdisk check: FAILED (${count} passed)` : `floatdisk check: ${count} checks pass`);
