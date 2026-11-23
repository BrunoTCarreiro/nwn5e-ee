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
  for (const f of ["bool N5Slab(", "bool N5Shadowed(", "void N5ParticleMask(", "#include \"inc_scriptable\"", "#if POSITION_VIEW != 1"]) {
    const at = lines.findIndex((l) => l.startsWith(f));
    assert.ok(at >= 0 && at < main, label + ": " + f + " not before main");
  }
  // Off is the default and exact: the mask returns at once while scriptableVec1.w is under 0.5.
  assert.ok(/if \(scriptableVec1\.w < 0\.5\)\r?\n\s+return;/.test(made.text), label + ": no early return when off");
  // Seven boxes: scriptableVec2 to scriptableVec15, in pairs.
  for (let k = 0; k < 7; k++) {
    assert.ok(made.text.includes(`scriptableVec${2 + 2 * k}, scriptableVec${3 + 2 * k})`), label + ": box " + (k + 1) + " not read");
  }
  assert.ok(!made.text.includes("scriptableVec16"), label + ": scriptableVec16 used");
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
}

console.log(failed ? `particle mask check: ${failed} failed` : "particle mask check: all pass");
process.exitCode = failed ? 1 : 0;
