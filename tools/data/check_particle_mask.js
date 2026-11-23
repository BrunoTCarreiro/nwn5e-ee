#!/usr/bin/env node
// particle_mask.js without the game (#184's spike) - a stand-in shader written here, nothing of the game's:
//   node tools\data\check_particle_mask.js [--game <folder> --tools <nwn5e-tools>]
// With --game and --tools it also patches the player's own fsparticle.shd in memory (nothing written) and checks it the
// same way.
"use strict";
const assert = require("assert");
const path = require("path");
const M = require("./particle_mask.js");

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

// A stand-in with the anchors a particle fragment shader has: defines and includes, a uniform block, a main that sets
// gl_FragColor last.
const STAND_IN = [
  "// a stand-in", "#define SHADER_TYPE 2", "#define FOG 1", "#include \"inc_standard\"", "", "#if SHADER_QUALITY_MODE > 0",
  "uniform int blendMode;", "#endif", "void main ()", "{", "    FragmentColor = VertexColor;", "    gl_FragColor = FragmentColor;", "}", "",
].join("\r\n");

function checkPatched(source, label) {
  const made = M.patch(source);
  assert.ok(!made.error, label + ": " + made.error);
  const lines = made.text.split(/\r?\n/);
  assert.strictEqual(lines[0], M.MARK, label + ": the mark isn't the first line");
  // Exactly one call, right before the one gl_FragColor line, inside main.
  const calls = lines.map((l, i) => (l === M.CALL ? i : -1)).filter((i) => i >= 0);
  assert.strictEqual(calls.length, 1, label + ": " + calls.length + " calls");
  assert.ok(/^\s*gl_FragColor\s*=\s*FragmentColor\s*;\s*$/.test(lines[calls[0] + 1]), label + ": the call isn't just before gl_FragColor");
  const main = lines.findIndex((l) => /^\s*void\s+main\s*\(/.test(l));
  assert.ok(main >= 0 && main < calls[0], label + ": the call isn't in main");
  // The mask's functions before main, the scriptable uniforms included, the varying only when not already declared.
  for (const f of ["bool N5Slab(", "bool N5Box(", "float N5Near(", "void N5ParticleMask(", "#include \"inc_scriptable\"",
    "#if POSITION_VIEW != 1"]) {
    const at = lines.findIndex((l) => l.startsWith(f));
    assert.ok(at >= 0 && at < main, label + ": " + f + " not before main");
  }
  // Off is the default and exact: the mask returns at once while scriptableVec1.w (the reach) is 0; past the reach the
  // fragment is drawn.
  assert.ok(/if \(scriptableVec1\.w <= 0\.0\)\r?\n\s+return;/.test(made.text), label + ": no early return when off");
  assert.ok(/if \(r > scriptableVec1\.w\)\r?\n\s+return;/.test(made.text), label + ": no return past the reach");
  // The ring (src/rules/particle_mask.h): its 32 directions in scriptableVec2..9 - N5Near takes them in that order, the
  // components x y z w; the angle's step 360 / 32 degrees; the wrap past the last direction.
  const near = made.text.slice(made.text.indexOf("float N5Near("), made.text.indexOf("// nwn5e: off ("));
  const slots = [...near.matchAll(/scriptableVec(\d+)/g)].map((m) => Number(m[1]));
  assert.deepStrictEqual(slots, [2, 3, 4, 5, 6, 7, 8, 9], label + ": the ring's slots " + slots.join(","));
  assert.ok(/return v\.x;[\s\S]*return v\.y;[\s\S]*return v\.z;[\s\S]*return v\.w;/.test(near), label + ": the ring's components");
  assert.ok(made.text.includes("atan(d.y, d.x) / 0.19634954;"), label + ": the ring's step isn't 2 pi / 32");
  assert.ok(/if \(k1 > 31\.5\)\r?\n\s+k1 = 0\.0;/.test(made.text), label + ": the ring doesn't wrap");
  // Four boxes, 7 numbers each back to back: read in their order, every component of scriptableVec10..16 once.
  const boxes = [...made.text.matchAll(/N5Box\(o, p, (.*)\);?\r?$/gm)].map((m) => m[1]);
  assert.strictEqual(boxes.length, 4, label + ": " + boxes.length + " boxes read");
  const read = boxes.map((c) => [...c.matchAll(/scriptableVec(\d+)\.([xyzw])/g)].map((m) => m[1] + m[2])).flat();
  const expected = [];
  for (let s = 10; s <= 16; s++) for (const c of "xyzw") expected.push(s + c);
  assert.deepStrictEqual(read, expected, label + ": the boxes' numbers aren't read back to back from scriptableVec10.x");
  // Nothing of the game's text changed: the added lines out, it is the source again, byte for byte.
  assert.strictEqual(M.unpatch(made.text), source, label + ": the game's text changed");
  // Its line ends kept.
  assert.strictEqual(made.text.includes("\r\n"), source.includes("\r\n"), label + ": line ends changed");
}

check("a stand-in particle shader patched: one call before gl_FragColor, the mask before main, the source unchanged", () => {
  checkPatched(STAND_IN, "stand-in");
  checkPatched(STAND_IN.split("\r\n").join("\n"), "stand-in (LF)");
});

check("refused: no main, two mains, no gl_FragColor line, two, before main, a mask already there", () => {
  const no = (text, word) => {
    const made = M.patch(text);
    assert.ok(made.error && made.error.includes(word), "not refused: " + word + " (" + made.error + ")");
  };
  no(STAND_IN.replace("void main ()", "void notmain ()"), "void main");
  no(STAND_IN + "\r\nvoid main ()\r\n{\r\n}", "void main");
  no(STAND_IN.replace("    gl_FragColor = FragmentColor;", "    gl_FragColor = vec4(1.0);"), "gl_FragColor");
  no(STAND_IN.replace("}", "    gl_FragColor = FragmentColor;\r\n}"), "gl_FragColor");
  no(["gl_FragColor = FragmentColor;", "void main ()", "{", "}"].join("\n"), "before its main");
  no(M.patch(STAND_IN).text, "already");
});

check("derive: the shader's name and bytes, or skipped with the reason", () => {
  const made = M.derive({ res: (name, type) => (name === M.SOURCE && type === M.TYPE_SHD ? Buffer.from(STAND_IN, "latin1") : null) });
  assert.strictEqual(made.name, "fsn5pmask.shd");
  assert.strictEqual(made.bytes.toString("latin1").split(/\r?\n/)[0], M.MARK);
  assert.ok(M.derive({ res: () => null }).skipped.includes("no fsparticle"));
  assert.ok(M.derive({ res: () => Buffer.from("void main ()\n{\n}\n") }).skipped.includes("gl_FragColor"));
});

// #184: the spells' masked looks (spell_mask.js). The rows name only copies it makes, and every copy it makes is named.
const S = require("./spell_mask.js");
const fs = require("fs");
const ROWS = path.join(__dirname, "..", "..", "data", "rows");
check("the spells' masked copies: their names as long as the game's, the rows naming each and nothing else", () => {
  for (const m of S.MODELS) {
    const c = S.copyName(m);
    assert.ok(/^n5k_/.test(c) && c.length === m.length, m + " -> " + c);
  }
  assert.strictEqual(S.copyName("plain"), "");
  assert.strictEqual(S.materialName("fxpa_Smoke01a"), "n5x_smoke01a");
  assert.strictEqual(S.materialName("Blood"), "n5x_blood");
  const named = new Set();
  for (const file of Object.values(S.FILES)) {
    const text = fs.readFileSync(path.join(ROWS, file), "latin1");
    for (const n of S.namedCopies(text)) named.add(n);
    assert.ok(/\.if_spellmask\.txt$/.test(file), file + " isn't served with the spellmask flag alone");
  }
  const made = new Set(S.MODELS.map(S.copyName));
  for (const n of named) assert.ok(made.has(n), "a row names " + n + ", which spell_mask.js doesn't make");
  for (const n of made) assert.ok(named.has(n), n + " is made but no row names it");
  // The rows files sort after the spells' own (if_spells2024) rows: theirs first, ours over them.
  assert.ok("spells.mask.if_spellmask.txt" > "spells.if_spells2024.level1.txt" &&
    "vfx_persistent.mask.if_spellmask.txt" > "vfx_persistent.if_spells2024.txt", "a mask rows file sorts first");
  assert.ok(S.owns("n5k_conefire.mdl") && S.owns("N5X_SMOKE01A.mtr") && !S.owns("n5k_other.mdl") && !S.owns("var_conefire.mdl"));
});

// The player's own shader, when asked (in memory; nothing written).
const argv = process.argv.slice(2);
const option = (name) => {
  const i = argv.indexOf("--" + name);
  return i >= 0 && i + 1 < argv.length ? argv[i + 1] : "";
};
if (option("game") && option("tools")) {
  check("the player's own fsparticle.shd patched the same way", () => {
    const res = require(path.join(path.resolve(option("tools")), "spell_bench", "game_data.js")).resources(path.resolve(option("game")));
    const source = res(M.SOURCE, M.TYPE_SHD);
    assert.ok(source, "the game has no fsparticle.shd");
    checkPatched(source.toString("latin1"), "the game's");
  });
  check("the spells' masked rows are what spell_mask.js --rows makes from the game's tables now", () => {
    const G = require(path.join(path.resolve(option("tools")), "spell_bench", "game_data.js"));
    const game = path.resolve(option("game"));
    const table = (name) => {
      const t = G.gameTable(game, name);
      for (const f of fs.readdirSync(ROWS).sort()) {
        if (f.split(".")[0].toLowerCase() !== name || /\.if_spellmask\./i.test(f) || !f.endsWith(".txt")) continue;
        G.applyRows(t, fs.readFileSync(path.join(ROWS, f), "latin1"));
      }
      return t;
    };
    const want = S.rowsText(table);
    for (const [k, file] of Object.entries(S.FILES)) {
      const have = fs.readFileSync(path.join(ROWS, file), "latin1").split(/\r?\n/).join("\n");
      assert.strictEqual(have, want[k].split("\r\n").join("\n"), file + " differs - run node tools\\data\\spell_mask.js --rows");
    }
  });
}

console.log(failed ? `particle mask check: ${failed} failed` : "particle mask check: all pass");
process.exitCode = failed ? 1 : 0;
