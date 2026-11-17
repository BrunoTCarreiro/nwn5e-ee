#!/usr/bin/env node
// The Composer's project generator end to end (build.js, with floatdisk.js and reader.js), on a temporary copy - needs
// the game, prints "skip" without it:
//   node tools\data\composer\check_build.js [--game <folder>] [--tools <nwn5e-tools>]
// The copy is <tmp>\n5_cpbuild-*\tools\data\floatdisk and \composer (the folders) and <tmp>\...\data\files, \rows,
// \looks (empty; build.js's repo root is three folders up from itself), and a saved\ for the spell meshes - so the real
// worktree's data\ and tools\data\composer\ are never written.
"use strict";
const assert = require("assert");
const crypto = require("crypto");
const fs = require("fs");
const os = require("os");
const path = require("path");
const { spawnSync } = require("child_process");
const P = require("./project.js");
const F = require("../floatdisk/floatdisk.js");
const R = require("../floatdisk/reader.js");

const REPO = path.resolve(__dirname, "..", "..", "..");
const argv = process.argv.slice(2);
const option = (name, fallback) => {
  const i = argv.indexOf("--" + name);
  return i >= 0 && i + 1 < argv.length ? argv[i + 1] : fallback;
};
const TOOLS = path.resolve(option("tools", process.env.NWN5E_TOOLS || path.join(REPO, "..", "nwn5e-tools")));
const READER = path.join(REPO, "build", "floatdisk", "mdlmesh.exe");
const GAME = require(path.join(TOOLS, "anim_helper", "helper.js")).findGameDir({ game: option("game", ""), buildDir: path.join(TOOLS, "..", "nwn5e-ee", "build") });
if (!fs.existsSync(path.join(GAME, "data", "nwn_base.key"))) {
  console.log("skip: no game at " + GAME);
  process.exit(0);
}
global.window = global.window || {};  // core.js runs in the page and under node
const core = require(path.join(TOOLS, "vfx_bench", "public", "core.js"));
const gameData = require(path.join(TOOLS, "spell_bench", "game_data.js"));

// --- the copy -----------------------------------------------------------------------------------------------------
const root = fs.mkdtempSync(path.join(os.tmpdir(), "n5_cpbuild-"));
const folder = path.join(root, "tools", "data", "composer");
const FILES = path.join(root, "data", "files"), ROWS = path.join(root, "data", "rows"), LOOKS = path.join(root, "data", "looks");
const SAVED = path.join(root, "saved");
fs.cpSync(path.join(REPO, "tools", "data", "floatdisk"), path.join(root, "tools", "data", "floatdisk"), { recursive: true });
fs.cpSync(__dirname, folder, { recursive: true });
for (const dir of [FILES, ROWS, LOOKS, SAVED]) fs.mkdirSync(dir, { recursive: true });

// A project in the copy's tools\data\composer; build.js on it, the copy's data\ and saved\ its targets.
const project = (name, json) => {
  const file = path.join(folder, name + ".json");
  fs.writeFileSync(file, P.projectText(Object.assign({ _about: ["a check's project"], name }, json)));
  return file;
};
function build(file, args = []) {
  const r = spawnSync(process.execPath, [path.join(folder, "build.js"), file, "--game", GAME, "--tools", TOOLS, "--reader", READER,
    "--files", FILES, "--rows", ROWS, ...args], { encoding: "utf8", maxBuffer: 64 << 20, windowsHide: true });
  return { code: r.status, out: r.stdout || "", err: r.stderr || "" };
}
const json = (r) => {
  try {
    return JSON.parse(r.out.trim());
  } catch (e) {
    throw new Error("build.js printed no JSON" + (r.err ? " (" + r.err.trim() + ")" : ""));
  }
};
const ok = (r) => {
  assert.strictEqual(r.code, 0, "build.js exited " + r.code + ": " + r.err.trim());
  return json(r);
};
const refused = (r, pattern) => {
  assert.strictEqual(r.code, 1, "build.js exited " + r.code + " (refused expected)");
  const j = json(r);
  assert.match(j.error, pattern);
  assert.ok(r.err.includes("composer: " + j.error), "stderr says composer: <error>");
  return j;
};
const snapshot = () => {
  const s = {};
  for (const dir of [FILES, ROWS, LOOKS, SAVED]) {
    for (const f of fs.readdirSync(dir)) {
      const file = path.join(dir, f), st = fs.statSync(file);
      s[file] = st.mtimeMs + ":" + crypto.createHash("sha1").update(fs.readFileSync(file)).digest("hex");
    }
  }
  return s;
};
const marked = (file) => F.carriesMark(fs.readFileSync(file), P.MARK);
const tok = (s) => [...s.matchAll(/"([^"]*)"|(\S+)/g)].map((m) => (m[1] !== undefined ? m[1] : m[2]));
// A model's or a draft's vertices' lowest z.
const lowestZ = (text) => {
  let low = Infinity;
  const l = text.split(/\r?\n/);
  for (let i = 0; i < l.length; i++) {
    const m = l[i].match(/^  verts (\d+)$/);
    if (m) for (let k = 1; k <= +m[1]; k++) low = Math.min(low, Number(l[i + k].trim().split(/\s+/)[2]));
  }
  return low;
};
const CIRCLE = { shape: "circle", diameter: 1, height: 0.3 };
const THROUGH = { copy: null, use: null, walk: "through" };
const PILE = project("chkpile", { kind: "placeable", guide: CIRCLE, placeable: THROUGH, variants: [
  { name: "1", row: 14100, parts: [{ model: "plc_b04", at: [0, 0, 0] }] },
  { name: "2", row: 14101, parts: [{ model: "plc_a03", at: [0, 0, 0], scale: 0.5 }, { model: "plc_b06", at: [0.2, 0.1, 0] }] },
] });

// --- the checks ---------------------------------------------------------------------------------------------------
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
  check("a placeable builds its model, pwk and rows", () => {
    const j = ok(build(PILE, ["--json"]));
    assert.strictEqual(j.kind, "placeable");
    assert.strictEqual(j.check, false);
    // The variants as the stage shows them (A2's stage fields, F7): the parts on the guide's height.
    assert.deepStrictEqual(j.variants.map((v) => [v.name, v.model, v.row, v.parts, v.list.length]),
      [["1", "plc_n5chkpile_1", 14100, 1, 1], ["2", "plc_n5chkpile_2", 14101, 2, 2]]);
    for (const v of j.variants) {
      assert.ok(v.meshes > 0 && v.triangles > 0 && v.tall > 0, JSON.stringify(v));
      assert.ok(Math.abs(v.top - v.tall - CIRCLE.height) <= 0.0015, `top ${v.top}, tall ${v.tall}`);
      for (const x of v.list) assert.ok(x.from === 0 && x.to > 0 && x.size.length === 3 && x.past >= 0, JSON.stringify(x));
    }
    assert.strictEqual(j.variants[0].list[0].label, "plc_b04");
    // Four model files and the rows file, new, each with the Composer's mark.
    const names = ["plc_n5chkpile_1.mdl", "plc_n5chkpile_1.pwk", "plc_n5chkpile_2.mdl", "plc_n5chkpile_2.pwk"];
    assert.deepStrictEqual(fs.readdirSync(FILES).sort(), names);
    assert.deepStrictEqual(fs.readdirSync(ROWS), ["placeables.chkpile.txt"]);
    assert.deepStrictEqual(j.files.map((f) => [f.state, path.basename(f.file)]), [...names, "placeables.chkpile.txt"].map((f) => ["new", f]));
    for (const f of names) assert.ok(marked(path.join(FILES, f)), f + " has no mark");
    assert.ok(marked(path.join(ROWS, "placeables.chkpile.txt")), "the rows have no mark");
    // The model's base on the ground: the parts lowered by the guide's height. Its use point the default, no ground mesh.
    const mdl = fs.readFileSync(path.join(FILES, "plc_n5chkpile_2.mdl"), "latin1");
    assert.ok(Math.abs(lowestZ(mdl)) <= 1e-4, "the lowest point at z " + lowestZ(mdl));
    assert.ok(mdl.includes("\r\nnewmodel plc_n5chkpile_2\r\n") && mdl.includes("\r\nnode dummy n5chkpile_2_hand\r\n"));
    const pwk = fs.readFileSync(path.join(FILES, "plc_n5chkpile_1.pwk"), "latin1");
    assert.ok(pwk.includes("node dummy n5chkpile_1_pwk_use01\r\n  parent plc_n5chkpile_1_pwk\r\n  position 0 -0.6 0\r\n"), pwk);
    assert.ok(!pwk.includes("node trimesh") && !pwk.includes("_use02"));
    // The rows: LootBag1's (row 13, plc_b04's, the first placeable part's) with Label and ModelName changed.
    const table = gameData.gameTable(GAME, "placeables"), source = table.rows.get(13);
    assert.strictEqual(source.Label, "LootBag1");
    const l = fs.readFileSync(path.join(ROWS, "placeables.chkpile.txt"), "latin1").split(/\r?\n/).filter((x) => x.trim() && !x.startsWith("#"));
    assert.deepStrictEqual(tok(l[0]), table.columns);
    assert.strictEqual(l.length, 3);
    [[14100, "N5_Chkpile_1", "plc_n5chkpile_1"], [14101, "N5_Chkpile_2", "plc_n5chkpile_2"]].forEach(([row, label, model], k) => {
      const t = tok(l[k + 1]);
      assert.strictEqual(t[0], String(row));
      table.columns.forEach((c, i) => {
        const want = c === "Label" ? label : c === "ModelName" ? model : source[c];
        assert.strictEqual(t[i + 1], want, `row ${row} ${c}`);
      });
    });
  });

  check("a second build changes nothing", () => {
    const before = snapshot();
    const j = ok(build(PILE, ["--json"]));
    assert.strictEqual(j.files.length, 5);
    assert.ok(j.files.every((f) => f.state === "unchanged"), JSON.stringify(j.files));
    assert.deepStrictEqual(j.unused, []);
    assert.deepStrictEqual(snapshot(), before);
  });

  check("a row another rows file sets is refused, nothing written", () => {
    const other = path.join(ROWS, "placeables.other.txt");
    fs.writeFileSync(other, "# someone's rows\r\n       Label     ModelName\r\n14101  Theirs    plc_theirs\r\n");
    try {
      const before = snapshot();
      refused(build(PILE, ["--json"]), /variants\[1\]\.row: 14101 .*placeables\.other\.txt/);
      // A new project's row that another project's rows file sets: refused, and none of its files appear.
      const row = project("chkrow", { kind: "placeable", guide: CIRCLE, placeable: THROUGH, variants: [{ name: "1", row: 14100, parts: [{ model: "plc_b04", at: [0, 0, 0] }] }] });
      refused(build(row, ["--json"]), /variants\[0\]\.row: 14100 .*placeables\.chkpile\.txt/);
      assert.deepStrictEqual(snapshot(), before);
    } finally {
      fs.rmSync(other, { force: true });
    }
  });

  check("Blocks with no guide is refused", () => {
    const before = snapshot();
    const settings = { copy: null, use: [0, -0.8], walk: "blocks" };
    const parts = [{ model: "plc_b04", at: [0, 0, 0] }];
    const none = project("chkblock", { kind: "placeable", guide: { shape: "none", height: 0 }, placeable: settings, variants: [{ name: "1", row: 14150, parts }] });
    refused(build(none, ["--json"]), /Blocks/);
    assert.deepStrictEqual(snapshot(), before);
    // With a guide it builds: its outline is the ground mesh, then the use point.
    const rect = project("chkblock", { kind: "placeable", guide: { shape: "rect", width: 1, length: 0.8, height: 0 }, placeable: settings, variants: [{ name: "1", row: 14150, parts }] });
    ok(build(rect, ["--json"]));
    const pwk = fs.readFileSync(path.join(FILES, "plc_n5chkblock.pwk"), "latin1").split("\r\n");
    const at = pwk.indexOf("node trimesh PLC_N5CHKBLOCK_wg");
    assert.ok(at > 0, "no ground mesh");
    assert.deepStrictEqual(pwk.slice(at + 5, at + 12), ["  verts 4", "    0.5 0.4 0", "    -0.5 0.4 0", "    -0.5 -0.4 0", "    0.5 -0.4 0", "  faces 2", "    0 1 2  1  0 0 0  7"]);
    assert.ok(pwk.indexOf("node dummy n5chkblock_pwk_use01") > at && pwk.includes("  position 0 -0.8 0"));
  });

  check("a spell mesh writes its draft into --saved", () => {
    const mesh = project("chkmesh", { kind: "spellmesh", guide: { shape: "circle", diameter: 0.5, height: -0.2 }, variants: [{ name: "1", parts: [{ model: "plc_b06", at: [0, 0, 0] }] }] });
    const before = snapshot();
    refused(build(mesh, ["--json"]), /--saved/);
    assert.deepStrictEqual(snapshot(), before);
    const j = ok(build(mesh, ["--json", "--saved", SAVED]));
    assert.strictEqual(j.kind, "spellmesh");
    assert.deepStrictEqual(j.variants.map((v) => [v.name, v.model, v.row, v.parts]), [["1", "n5m_chkmesh", null, 1]]);
    assert.deepStrictEqual(j.files.map((f) => [f.state, path.basename(f.file)]), [["new", "n5m_chkmesh.vfx"]]);
    const text = fs.readFileSync(path.join(SAVED, "n5m_chkmesh.vfx"), "latin1");
    assert.ok(text.startsWith(P.MARK + "\r\n") && !/[^\r]\n/.test(text), "the mark first, CRLF");
    const fx = core.readDraft(text.replace(/\r/g, ""));
    assert.strictEqual(fx.name, "n5m_chkmesh");
    assert.strictEqual(fx.planes.length, j.variants[0].meshes);
    assert.strictEqual(fx.planes.reduce((n, p) => n + p.faces.length / 3, 0), j.variants[0].triangles);
    // Its origin the guide's centre at its height: the part's base at z 0, though the guide sits below the ground.
    let low = Infinity;
    for (const p of fx.planes) for (let i = 2; i < p.verts.length; i += 3) low = Math.min(low, p.verts[i]);
    assert.ok(Math.abs(low) <= 1e-4, "the lowest point at z " + low);
    assert.ok(Math.abs(j.variants[0].top - j.variants[0].tall - -0.2) <= 0.0015);
    // Nothing in data\ for a spell mesh.
    const after = snapshot();
    for (const f of Object.keys(after)) if (!f.startsWith(SAVED)) assert.strictEqual(after[f], before[f], f);
  });

  check("a file in the way it didn't make is refused", () => {
    const theirs = path.join(FILES, "plc_n5chkway.mdl");
    fs.writeFileSync(theirs, "#MAXMODEL ASCII\r\n# someone's model\r\nnewmodel plc_n5chkway\r\n");
    const before = snapshot();
    const way = project("chkway", { kind: "placeable", guide: CIRCLE, placeable: THROUGH, variants: [{ name: "1", row: 14160, parts: [{ model: "plc_b04", at: [0, 0, 0] }] }] });
    refused(build(way, ["--json"]), /^data\/files\/plc_n5chkway\.mdl is there and wasn't made by this generator/);
    assert.deepStrictEqual(snapshot(), before);
    assert.ok(!fs.existsSync(path.join(FILES, "plc_n5chkway.pwk")) && !fs.existsSync(path.join(ROWS, "placeables.chkway.txt")));
  });

  check("a second variant renames the model: the old files are listed unused, not deleted", () => {
    const parts = [{ model: "plc_b04", at: [0, 0, 0] }];
    const one = project("chkren", { kind: "placeable", guide: CIRCLE, placeable: THROUGH, variants: [{ name: "1", row: 14170, parts }] });
    ok(build(one, ["--json"]));
    const two = project("chkren", { kind: "placeable", guide: CIRCLE, placeable: THROUGH, variants: [{ name: "1", row: 14170, parts }, { name: "2", row: 14171, parts }] });
    const j = ok(build(two, ["--json"]));
    assert.deepStrictEqual(j.variants.map((v) => v.model), ["plc_n5chkren_1", "plc_n5chkren_2"]);
    assert.deepStrictEqual(j.unused.map((f) => path.basename(f)).sort(), ["plc_n5chkren.mdl", "plc_n5chkren.pwk"]);
    assert.ok(fs.existsSync(path.join(FILES, "plc_n5chkren.mdl")));
    assert.match(build(two, []).out, /no longer used: .*plc_n5chkren\.mdl/);
  });

  check("an own file's part: read from this folder wherever the project is, its texture shipped", () => {
    // A 12-triangle ASCII cube, its texture a 2 x 2 TGA beside it, neither in the game; the project in another folder, as
    // the VFX Bench's temporary copy is (F2).
    const parts = path.join(folder, "parts");
    fs.mkdirSync(parts, { recursive: true });
    const v = [[-0.2, -0.2, 0], [0.2, -0.2, 0], [0.2, 0.2, 0], [-0.2, 0.2, 0], [-0.2, -0.2, 0.4], [0.2, -0.2, 0.4], [0.2, 0.2, 0.4], [-0.2, 0.2, 0.4]];
    const f = [[0, 2, 1], [0, 3, 2], [4, 5, 6], [4, 6, 7], [0, 1, 5], [0, 5, 4], [1, 2, 6], [1, 6, 5], [2, 3, 7], [2, 7, 6], [3, 0, 4], [3, 4, 7]];
    fs.writeFileSync(path.join(parts, "n5_chkcrate.mdl"), ["newmodel n5_chkcrate", "setsupermodel n5_chkcrate null", "classification placeable",
      "beginmodelgeom n5_chkcrate", "node dummy n5_chkcrate", "  parent NULL", "endnode", "node trimesh box", "  parent n5_chkcrate",
      "  position 0 0 0", "  orientation 0 0 0 0", "  bitmap n5_chkcrate", "  verts 8", ...v.map((p) => "    " + p.join(" ")),
      "  tverts 8", ...v.map((p) => `    ${p[0] + 0.5} ${p[1] + 0.5} 0`), "  faces 12", ...f.map((t) => `    ${t.join(" ")} 1 ${t.join(" ")} 1`),
      "endnode", "endmodelgeom n5_chkcrate", "donemodel n5_chkcrate", ""].join("\r\n"));
    const tga = (shade) => Buffer.concat([Buffer.from([0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 32, 8]), Buffer.alloc(16, shade)]);
    fs.writeFileSync(path.join(parts, "n5_chkcrate.tga"), tga(0x80));
    const elsewhere = path.join(root, "elsewhere");
    fs.mkdirSync(elsewhere, { recursive: true });
    const own = (file, json) => {
      fs.writeFileSync(path.join(elsewhere, file), P.projectText(Object.assign({ _about: ["a check's project"] }, json)));
      return path.join(elsewhere, file);
    };
    const part = [{ file: "parts/n5_chkcrate.mdl", at: [0, 0, 0] }];
    const pile = own("n5_composer-1.json", { kind: "placeable", name: "chkown", guide: CIRCLE, placeable: THROUGH, variants: [{ name: "1", row: 14190, parts: part }] });
    const j = ok(build(pile, ["--json"]));
    assert.deepStrictEqual([j.variants[0].triangles, j.variants[0].list[0].label], [12, "parts/n5_chkcrate.mdl"]);
    assert.deepStrictEqual(j.shipped.map((s) => s.texture), ["n5_chkcrate"]);
    assert.ok(j.files.some((x) => x.state === "new" && x.file === "data/files/n5_chkcrate.tga"), JSON.stringify(j.files));
    assert.ok(fs.readFileSync(path.join(FILES, "n5_chkcrate.tga")).equals(tga(0x80)));
    assert.ok(fs.readFileSync(path.join(folder, "made.txt"), "utf8").split("\n").includes("data/files/n5_chkcrate.tga"), "made.txt lists it");
    assert.ok(fs.readFileSync(path.join(FILES, "plc_n5chkown.mdl"), "latin1").includes("\r\n  bitmap n5_chkcrate\r\n"));
    // A spell mesh's copy goes beside its draft; changed beside the part, it's rewritten there (the marked draft names it).
    const mesh = own("n5_composer-2.json", { kind: "spellmesh", name: "chkownm", guide: CIRCLE, variants: [{ name: "1", parts: part }] });
    const states = (r) => Object.fromEntries(json(r).files.map((x) => [path.basename(x.file), x.state]));
    assert.deepStrictEqual(states(build(mesh, ["--json", "--saved", SAVED])), { "n5m_chkownm.vfx": "new", "n5_chkcrate.tga": "new" });
    fs.writeFileSync(path.join(parts, "n5_chkcrate.tga"), tga(0x40));
    assert.deepStrictEqual(states(build(mesh, ["--json", "--saved", SAVED])), { "n5m_chkownm.vfx": "unchanged", "n5_chkcrate.tga": "rebuilt" });
    assert.deepStrictEqual(states(build(pile, ["--json"]))["n5_chkcrate.tga"], "rebuilt");  // made.txt's
    // Without a marked draft naming it, a different copy in saved\ is someone's.
    fs.rmSync(path.join(SAVED, "n5m_chkownm.vfx"));
    fs.writeFileSync(path.join(parts, "n5_chkcrate.tga"), tga(0x20));
    const before = snapshot();
    refused(build(mesh, ["--json", "--saved", SAVED]), /n5_chkcrate\.tga is there and wasn't made by this generator/);
    assert.deepStrictEqual(snapshot(), before);
  });

  check("--check writes nothing and needs no row yet", () => {
    const before = snapshot();
    const fresh = project("chknew", { kind: "placeable", guide: CIRCLE, placeable: THROUGH, variants: [{ name: "1", parts: [{ model: "plc_b04", at: [0, 0, 0] }] }] });
    const j = ok(build(fresh, ["--check", "--json"]));
    assert.strictEqual(j.check, true);
    assert.deepStrictEqual(j.variants.map((v) => [v.model, v.row, v.parts]), [["plc_n5chknew", null, 1]]);
    assert.deepStrictEqual(j.files, []);
    refused(build(fresh, ["--json"]), /variants\[0\]\.row: none yet/);
    const empty = project("chknew", { kind: "placeable", guide: CIRCLE, placeable: THROUGH, variants: [{ name: "1", row: 14180, parts: [] }] });
    refused(build(empty, ["--check", "--json"]), /variants\[0\]\.parts: at least one part/);
    assert.deepStrictEqual(snapshot(), before);
  });

  // A dyed part (its texture the game has only as a PLT): baked from the game's palettes into a texture of its own.
  const DEF = "n5p_2cd3d725dedb", LEATHER = F.bakedName("helm_011", { leather1: 7 });  // helm_001 at the defaults; helm_011 leather 1 7
  const rd = R.create({ game: GAME, core, gameData, reader: null });
  const byName = (j) => Object.fromEntries(j.files.map((x) => [path.basename(x.file), x.state]));
  check("a placeable's dyed part is baked into data\\files beside its model; the model names it, made.txt doesn't list it", () => {
    const made = path.join(folder, "made.txt"), madeText = () => (fs.existsSync(made) ? fs.readFileSync(made, "utf8") : "");
    const kept = madeText();
    // Row 14151: 14150 is chkblock's now (Blocks, above), and a row another rows file sets is refused.
    const helm = project("chkhelm", { kind: "placeable", guide: CIRCLE, placeable: THROUGH, variants: [{ name: "1", row: 14151, parts: [
      { model: "helm_001", at: [0, 0, 0] }, { model: "helm_011", at: [0.3, 0, 0], colours: { leather1: 7 } }] }] });
    const said = build(helm, ["--check"]).out;
    for (const line of [`  texture ${DEF}: helm_001 dyed with metal 1 0, metal 2 3, leather 1 3, leather 2 2, cloth 1 22, cloth 2 23 - baked into data/files`,
      `  texture ${LEATHER}: helm_011 dyed with metal 1 0, metal 2 3, leather 1 7, leather 2 2, cloth 1 22, cloth 2 23 - baked into data/files`]) {
      assert.ok(said.split(/\r?\n/).includes(line), "no line: " + line + "\n" + said);
    }
    const j = ok(build(helm, ["--json"]));
    assert.deepStrictEqual(j.baked, [{ texture: DEF, plt: "helm_001", colours: {} }, { texture: LEATHER, plt: "helm_011", colours: { leather1: 7 } }]);
    const mdl = fs.readFileSync(path.join(FILES, "plc_n5chkhelm.mdl"), "latin1");
    for (const name of [DEF, LEATHER]) assert.ok(mdl.includes(`\r\n  bitmap ${name}\r\n`), "plc_n5chkhelm.mdl doesn't name " + name);
    assert.ok(!/\r\n  bitmap helm_0(01|11)\r\n/i.test(mdl), "plc_n5chkhelm.mdl still names a PLT");
    // Both in data\files, new, reader.js's bakes; neither in saved\ nor made.txt.
    const states = byName(j);
    for (const [name, plt, colours] of [[DEF, "helm_001", F.readColours()], [LEATHER, "helm_011", F.readColours({ leather1: 7 })]]) {
      assert.strictEqual(states[name + ".tga"], "new", name);
      assert.ok(j.files.some((x) => x.file === `data/files/${name}.tga`), name + " isn't named from the repo's root");
      assert.ok(fs.readFileSync(path.join(FILES, name + ".tga")).equals(rd.bake(plt, colours).bytes), name + " isn't reader.js's bake");
      assert.ok(!fs.existsSync(path.join(SAVED, name + ".tga")), name + " is in saved\\");
    }
    assert.strictEqual(madeText(), kept, "made.txt changed");
    // Built again: every state unchanged.
    const again = ok(build(helm, ["--json"]));
    assert.ok(again.files.length && again.files.every((x) => x.state === "unchanged"), JSON.stringify(again.files));
  });

  check("a spell mesh's dyed part: its draft names the baked texture, baked beside it into --saved", () => {
    const mesh = project("chkhelmm", { kind: "spellmesh", guide: { shape: "circle", diameter: 0.5, height: 0 }, variants: [{ name: "1", parts: [{ model: "helm_001", at: [0, 0, 0] }] }] });
    assert.ok(build(mesh, ["--check", "--saved", SAVED]).out.split(/\r?\n/).includes(
      `  texture ${DEF}: helm_001 dyed with metal 1 0, metal 2 3, leather 1 3, leather 2 2, cloth 1 22, cloth 2 23 - baked into ${SAVED}`));
    const files = snapshot();
    const j = ok(build(mesh, ["--json", "--saved", SAVED]));
    assert.deepStrictEqual(j.baked, [{ texture: DEF, plt: "helm_001", colours: {} }]);
    const planes = core.readDraft(fs.readFileSync(path.join(SAVED, "n5m_chkhelmm.vfx"), "latin1").replace(/\r/g, "")).planes;
    assert.ok(planes.some((p) => p.bitmap === DEF) && !planes.some((p) => /^helm_001$/i.test(p.bitmap)), "the draft's bitmaps: " + planes.map((p) => p.bitmap));
    assert.deepStrictEqual(byName(j), { "n5m_chkhelmm.vfx": "new", [DEF + ".tga"]: "new" });
    assert.ok(fs.readFileSync(path.join(SAVED, DEF + ".tga")).equals(rd.bake("helm_001", F.readColours()).bytes));
    // Nothing in data\ for a spell mesh.
    const after = snapshot();
    for (const f of Object.keys(after)) if (!f.startsWith(SAVED)) assert.strictEqual(after[f], files[f], f);
    // Built again: unchanged. A file of the baked name there is the bake's (the name fixes its bytes): rewritten.
    assert.deepStrictEqual(byName(ok(build(mesh, ["--json", "--saved", SAVED]))), { "n5m_chkhelmm.vfx": "unchanged", [DEF + ".tga"]: "unchanged" });
    fs.writeFileSync(path.join(SAVED, DEF + ".tga"), "not a bake");
    assert.deepStrictEqual(byName(ok(build(mesh, ["--json", "--saved", SAVED]))), { "n5m_chkhelmm.vfx": "unchanged", [DEF + ".tga"]: "rebuilt" });
  });

  // A shape part (floatdisk.js's shapeModel): its mesh made from numbers - no mesh reader, no game model - and its soft
  // texture (n5s_ and 12 hex digits of its colour: original) generated and written beside what names it.
  const SOFT = "n5s_8c757fb28f99", GOLD = "n5s_a3caa0709dda";  // the default colour's, and [1, 0.85, 0.4]'s
  const softBytes = (colour) => F.tgaBytes(F.shapeTexture({ colour }), { alpha: true });
  const madeTxt = () => (fs.existsSync(path.join(folder, "made.txt")) ? fs.readFileSync(path.join(folder, "made.txt"), "utf8") : "");
  const shapeFiles = () => [FILES, SAVED].flatMap((dir) => fs.readdirSync(dir).filter((f) => /^n5s_/i.test(f)).map((f) => path.join(dir, f)));
  const lines = (file, list) => {
    const text = fs.readFileSync(file, "latin1");
    for (const l of list) assert.ok(text.includes(`\r\n${l}\r\n`), `${path.basename(file)} has no line "${l}"`);
  };
  // chkdome: a spell mesh, Zone of Truth's 9.14 m circle and its dome. chkdomep: a placeable, a 1 m dome in gold.
  const zone = (shape) => project("chkdome", { kind: "spellmesh", guide: { shape: "circle", diameter: 9.14, height: 0 }, variants: [{ name: "1", parts: [
    { note: "zone", shape: Object.assign({ type: "dome", radius: 4.57 }, shape), at: [0, 0, 0] }] }] });
  const gold = (shape) => project("chkdomep", { kind: "placeable", guide: { shape: "circle", diameter: 2, height: 0 }, placeable: THROUGH, variants: [
    { name: "1", row: 14152, parts: [{ shape: Object.assign({ type: "dome", radius: 1, colour: [1, 0.85, 0.4] }, shape), at: [0, 0, 0] }] }] });
  const draftPlanes = (file) => {
    const text = fs.readFileSync(file, "latin1");
    assert.ok(text.startsWith(P.MARK + "\r\n"), path.basename(file) + ": no mark line first");
    return core.readDraft(text.slice(P.MARK.length + 2).replace(/\r/g, "")).planes;
  };

  check("a spell mesh's dome: its draft and its soft texture into --saved", () => {
    const file = zone({});
    const files = snapshot(), kept = madeTxt();
    assert.ok(build(file, ["--check", "--saved", SAVED]).out.split(/\r?\n/).includes(
      `  texture ${SOFT}: a shape's soft texture, colour 0.6 0.8 1 - written into ${SAVED}`), "no summary line for " + SOFT);
    const j = ok(build(file, ["--json", "--saved", SAVED]));
    assert.deepStrictEqual([j.variants[0].meshes, j.variants[0].triangles, j.variants[0].tall], [1, 960, 4.57]);
    assert.deepStrictEqual(j.shapes, [{ texture: SOFT, colour: [0.6, 0.8, 1] }]);
    assert.deepStrictEqual(byName(j), { "n5m_chkdome.vfx": "new", [SOFT + ".tga"]: "new" });
    // The draft: one plane, both sides of the dome, naming its texture; its material what the model writer writes
    // (alpha read back as the game's reader reads it, a float: 0.35's).
    const planes = draftPlanes(path.join(SAVED, "n5m_chkdome.vfx"));
    assert.strictEqual(planes.length, 1);
    const p = planes[0];
    assert.deepStrictEqual([p.bitmap, p.alpha, p.selfillum, p.verts.length / 3, p.faces.length / 3], [SOFT, Math.fround(0.35), [1, 1, 1], 592, 960]);
    // Its texture beside it: 128 x 128 with 8 alpha bits (18 + 128 x 128 x 4 bytes), floatdisk.js's own.
    const tga = fs.readFileSync(path.join(SAVED, SOFT + ".tga"));
    assert.deepStrictEqual([tga.length, tga[16], tga[17]], [65554, 32, 8]);
    assert.ok(tga.equals(softBytes([0.6, 0.8, 1])), SOFT + ".tga isn't floatdisk.js's soft texture");
    // Built again: unchanged. A file of its name there is the generator's (the name fixes its bytes): rewritten.
    const again = ok(build(file, ["--json", "--saved", SAVED]));
    assert.ok(again.files.length && again.files.every((x) => x.state === "unchanged"), JSON.stringify(again.files));
    fs.writeFileSync(path.join(SAVED, SOFT + ".tga"), "not a texture");
    assert.deepStrictEqual(byName(ok(build(file, ["--json", "--saved", SAVED]))), { "n5m_chkdome.vfx": "unchanged", [SOFT + ".tga"]: "rebuilt" });
    // Nothing in data\ for a spell mesh, nothing in made.txt.
    const after = snapshot();
    for (const f of Object.keys(after)) if (!f.startsWith(SAVED)) assert.strictEqual(after[f], files[f], f);
    assert.strictEqual(madeTxt(), kept, "made.txt changed");
  });

  check("a placeable's dome: its model is see-through (transparencyhint 1) and names its texture, written beside it", () => {
    const kept = madeTxt();
    const file = gold({});
    assert.ok(build(file, ["--check"]).out.split(/\r?\n/).includes(`  texture ${GOLD}: a shape's soft texture, colour 1 0.85 0.4 - written into data/files`),
      "no summary line for " + GOLD);
    const j = ok(build(file, ["--json"]));
    assert.deepStrictEqual(j.shapes, [{ texture: GOLD, colour: [1, 0.85, 0.4] }]);
    lines(path.join(FILES, "plc_n5chkdomep.mdl"), ["  bitmap " + GOLD, "  alpha 0.35", "  transparencyhint 1", "  selfillumcolor 1 1 1"]);
    // Its two sides' smoothing groups reach the model: the inside's 480 faces in group 2, and first.
    const mdl = fs.readFileSync(path.join(FILES, "plc_n5chkdomep.mdl"), "latin1").split("\r\n");
    const at = mdl.indexOf("  faces 960");
    assert.ok(at > 0, "plc_n5chkdomep.mdl has no \"faces 960\"");
    const groups = mdl.slice(at + 1, at + 961).map((l) => l.trim().split(/\s+/)[3]);
    assert.deepStrictEqual([groups.filter((g) => g === "2").length, groups.filter((g) => g === "1").length, groups[0]], [480, 480, "2"]);
    // Its texture in data\files, new, named from the repo's root; not in saved\, not in made.txt.
    assert.ok(j.files.some((x) => x.state === "new" && x.file === `data/files/${GOLD}.tga`), JSON.stringify(j.files));
    assert.ok(fs.readFileSync(path.join(FILES, GOLD + ".tga")).equals(softBytes([1, 0.85, 0.4])), GOLD + ".tga isn't floatdisk.js's soft texture");
    assert.ok(!fs.existsSync(path.join(SAVED, GOLD + ".tga")), GOLD + " is in saved\\");
    assert.strictEqual(madeTxt(), kept, "made.txt changed");
    const again = ok(build(file, ["--json"]));
    assert.ok(again.files.length && again.files.every((x) => x.state === "unchanged"), JSON.stringify(again.files));
  });

  check("a dome without a texture: bitmap NULL, its colour as its material, no texture written", () => {
    for (const f of shapeFiles()) fs.rmSync(f);  // the checks' above (this copy's): none may come back
    const j = ok(build(gold({ texture: "none" }), ["--json"]));
    lines(path.join(FILES, "plc_n5chkdomep.mdl"), ["  bitmap NULL", "  diffuse 1 0.85 0.4"]);
    assert.deepStrictEqual(j.shapes, []);
    assert.ok(!j.files.some((x) => /n5s_/i.test(x.file)), JSON.stringify(j.files));
    assert.deepStrictEqual(shapeFiles(), []);
  });

  check("a dome with a library texture: its model and its draft name it, no texture written", () => {
    for (const f of shapeFiles()) fs.rmSync(f);
    const LIB = "fxpa_symbl_001";  // the game's own: named, nothing generated or copied
    const j = ok(build(gold({ texture: LIB }), ["--json"]));
    lines(path.join(FILES, "plc_n5chkdomep.mdl"), ["  bitmap " + LIB, "  diffuse 1 0.85 0.4", "  transparencyhint 1"]);
    assert.deepStrictEqual(j.shapes, []);
    const m = ok(build(zone({ texture: LIB }), ["--json", "--saved", SAVED]));
    assert.deepStrictEqual(m.shapes, []);
    assert.deepStrictEqual(draftPlanes(path.join(SAVED, "n5m_chkdome.vfx")).map((p) => p.bitmap), [LIB]);
    assert.ok(![...j.files, ...m.files].some((x) => /n5s_/i.test(x.file)), JSON.stringify([...j.files, ...m.files]));
    assert.deepStrictEqual(shapeFiles(), []);
  });

  check("a dome with a texture the game has only as a PLT is refused (a shape takes no dyes), nothing written", () => {
    const before = snapshot(), kept = madeTxt();
    for (const r of [build(gold({ texture: "helm_001" }), ["--json"]), build(zone({ texture: "helm_001" }), ["--json", "--saved", SAVED])]) {
      refused(r, /^dome [\d.]+ m: texture helm_001 is one the game has only as a PLT \(a dyed texture\); a shape takes no dyes - pick a TGA or DDS$/);
    }
    assert.deepStrictEqual(snapshot(), before);
    assert.strictEqual(madeTxt(), kept, "made.txt changed");
  });
} finally {
  fs.rmSync(root, { recursive: true, force: true });
}

console.log(failed ? `composer build check: ${failed} failed` : "composer build check: all pass");
process.exitCode = failed ? 1 : 0;
