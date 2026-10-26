#!/usr/bin/env node
// Checks core.js against the game's own writer: every draft nwn5e wrote (the library's
// game\ and nwn5e\ folders, saved\) is read and written back by core.js and must come
// out byte for byte as the game wrote it; every texture must decode.
//   node tools/vfx_bench/check.js [build\vfxedit]
"use strict";
const fs = require("fs");
const path = require("path");
const core = require("./public/core.js");

const editor = path.resolve(process.argv[2] || path.join(__dirname, "..", "..", "build", "vfxedit"));
let drafts = 0, emitters = 0, textures = 0;
const failures = [];
for (const folder of [path.join(editor, "library", "game"), path.join(editor, "library", "nwn5e"), path.join(editor, "saved")]) {
  let files = [];
  try { files = fs.readdirSync(folder).filter((f) => f.endsWith(".vfx")); } catch (e) { continue; }
  for (const file of files) {
    const text = fs.readFileSync(path.join(folder, file), "utf8");
    try {
      const fx = core.readDraft(text);
      const again = core.writeDraft(fx);
      if (again !== text) {
        const a = text.split("\n"), b = again.split("\n");
        const line = a.findIndex((l, i) => l !== b[i]);
        failures.push(`${file}: line ${line + 1}: game "${a[line]}" - core.js "${b[line]}"`);
      }
      drafts++;
      emitters += fx.emitters.length;
    } catch (e) {
      failures.push(`${file}: ${e.message}`);
    }
  }
}
const textureFolder = path.join(editor, "library", "textures");
let textureFiles = [];
try { textureFiles = fs.readdirSync(textureFolder).filter((f) => f.endsWith(".tga")); } catch (e) { /* none */ }
for (const file of textureFiles) {
  try {
    const image = core.decodeTga(fs.readFileSync(path.join(textureFolder, file)));
    if (image.data.length !== image.width * image.height * 4) throw new Error("wrong size");
    textures++;
  } catch (e) {
    failures.push(`${file}: ${e.message}`);
  }
}
for (const f of failures.slice(0, 20)) console.log("FAIL " + f);
console.log(`${drafts} drafts (${emitters} emitters) read and written back, ${textures} textures decoded, ${failures.length} failed`);
process.exitCode = failures.length || !drafts ? 1 : 0;
