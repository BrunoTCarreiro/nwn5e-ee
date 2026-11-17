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
    refused(build(way, ["--json"]), /plc_n5chkway\.mdl is there and wasn't made by this generator/);
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
} finally {
  fs.rmSync(root, { recursive: true, force: true });
}

console.log(failed ? `composer build check: ${failed} failed` : "composer build check: all pass");
process.exitCode = failed ? 1 : 0;
