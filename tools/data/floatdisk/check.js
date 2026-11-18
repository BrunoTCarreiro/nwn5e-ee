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

check("the other shapes: six types, their labels and fields; a sphere's, a cylinder's and a cone's height twice the radius, a flat one's none, a ring's inner 0.85 of it", () => {
  assert.deepStrictEqual(F.SHAPE_TYPES, ["dome", "sphere", "cylinder", "cone", "disc", "ring"]);
  assert.deepStrictEqual(F.SHAPE_LABELS, { dome: "Dome (half sphere)", sphere: "Sphere", cylinder: "Cylinder (open)", cone: "Cone (point up)",
    disc: "Disc (flat, on the ground)", ring: "Ring (flat band)" });
  assert.deepStrictEqual(F.SHAPE_KEYS, ["type", "radius", "height", "inner", "segments", "sides", "mapping", "repeat_around", "repeat_up", "colour", "alpha", "glow", "texture",
    "blend"]);
  const solid = ["radius", "height", "segments", "sides"];
  assert.deepStrictEqual(F.SHAPE_FIELDS, { dome: solid, sphere: solid, cylinder: solid, cone: solid, disc: ["radius", "segments", "sides"],
    ring: ["radius", "inner", "segments", "sides"] });
  assert.deepStrictEqual(F.SHAPE_FLAT, ["disc", "ring"]);
  assert.deepStrictEqual(F.SHAPE_INFO.inner, { label: "Inner radius (m)", min: 0.01, max: 100, step: 0.05 });
  // Smoothness's tooltip true of every type (the card shows one for each).
  assert.strictEqual(F.SHAPE_INFO.segments.title, "Segments round it; a dome's rings up it are a quarter of that, a sphere's half");
  // Opacity's true with a texture too (the final review): a Soft or library texture's own alpha still shows through at 1.
  assert.strictEqual(F.SHAPE_INFO.alpha.title,
    "Its alpha: at 1 it's as solid as its texture (Soft and library textures still show through where their own alpha does); the game's globes go to about 0.3");
  // The defaults, in SHAPE_KEYS' order; each read as its defaults and written as its type and radius alone.
  const want = {
    sphere: { type: "sphere", radius: 4.57, height: 9.14 }, cylinder: { type: "cylinder", radius: 4.57, height: 9.14 }, cone: { type: "cone", radius: 4.57, height: 9.14 },
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
  // Cleaned: its non-zero keys in MOTION_KEYS' order - the rate only while it breathes (needed then; nothing without) -
  // or undefined when it doesn't move.
  for (const m of [undefined, null, {}, STILL, { rate: 2 }]) assert.strictEqual(F.cleanMotion(m), undefined, JSON.stringify(m));
  assert.deepStrictEqual(F.cleanMotion({ scroll: [0, 0.25], turn: 0, breathe: 0, rate: 0.5 }), { scroll: [0, 0.25] });
  assert.deepStrictEqual(F.cleanMotion({ rate: 2, turn: 30, scroll: [0, 0] }), { turn: 30 });
  assert.deepStrictEqual(Object.keys(F.cleanMotion({ rate: 1, breathe: 0.3, turn: -5, scroll: [1, 0] })), ["scroll", "turn", "breathe", "rate"]);
  assert.deepStrictEqual(F.cleanMotion(F.readMotion({ breathe: 0.3 }, "p")), { breathe: 0.3, rate: 0.5 });
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
  assert.ok(l.includes('      { "shape": { "type": "dome", "radius": 1 }, "at": [0, 0, 0], "motion": { "turn": 10, "breathe": 0.3, "rate": 0.5 } }'), text);
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

console.log(process.exitCode ? `floatdisk check: FAILED (${count} passed)` : `floatdisk check: ${count} checks pass`);
