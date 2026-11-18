#!/usr/bin/env node
// TMO-1's probe (nwn5e-ee #39, M2; the design's "Texture motion", docs/superpowers/specs/2026-11-18-mesh-texture-
// animation-and-tint-design.md): one spell mesh, n5m_uvprobe, that shows in game whether a moving texture works - and by
// which route - before the VFX Bench's side (M3) is built. NWN:EE has no UV-motion setting, so a moving texture is an EE
// material (n5u_<12 hex digits>.mtr, the mesh's bitmap) naming our vertex shader, vsn5uvmove.shd. Untested in game:
// whether our shader compiles and runs on an effect's mesh, whether the material's `texture0` binds the real texture,
// and whether that texture's TXI (additive: faint, see-through, no 0.2 alpha cut) still applies - and if not, where a TXI
// does. So seven faint domes (radius 0.4: 0.8 m wide; alpha 0.3), 1 m apart along x and clear of the character:
//
//   texture0 route (0.4 m high, x -3, -2, -1): the material names the texture (motionMaterialText)
//     1  Soft, Additive, Wrapped            scrolls up            scroll [0, 0.25]
//     2  fxpa_symbl_003 (a glyph, additive by its TXI), From above   turns counter-clockwise   turn 30
//     3  Soft, Additive, From above         breathes, a breath every 2 s   breathe 0.3, rate 0.5
//   copy route (0.7 m high, x 3, 2, 1): no texture0 - the texture copied under the material's own name, its TXI with it
//   (the soft one made again; fxpa_symbl_003's from the game, as the dyed parts' bakes are made from the game's PLTs)
//     4  as 1, scrolls down    scroll [0, -0.25]
//     5  as 2, turns clockwise turn -30
//     6  as 3, a breath a second   breathe 0.3, rate 1
//   texture0 route with the TXI beside the material (0.4 m high, x -4): the material names the texture, and the texture's
//   TXI is copied under the material's name - no texture there
//     7  as 2, turns counter-clockwise, a turn in 4 s   turn 90
//
// Each side reads scroll, turn, breathe from the middle out to 3 m, the opposite way to the other side (so each material
// is its own, and a side that moves says which it is); the heights tell the routes apart even when nothing moves; the
// seventh is the low side's outermost. Written into <saved> (a bench build's vfxedit\saved, where the game reads the
// bench's spell meshes): n5m_uvprobe.vfx (a spell mesh draft carrying project.js's MARK), the Additive soft texture
// (n5s_617b220c1144.tga / .txi), the seven materials, the shader, the copy route's textures and TXIs and the seventh's TXI
// under their materials' names; and n5m_uvprobe is added to <build>\vfxedit\meshes.txt, the bench's list of spell meshes,
// so + mesh offers it.
//
//   node tools\data\composer\uvprobe.js --saved <nwn5e-ee>\build\vfxedit\saved [--game <folder>] [--tools <nwn5e-tools>]
//
// --game and --tools default as build.js's (the game: NWN_GAME_DIR, the build's nwn5e.ini [loader] game_dir, the Steam
// folder). It refuses a folder that isn't a bench build's vfxedit\saved, and overwrites only what carries its mark (the
// draft: the Composer's; the shader: its own first line) or an n5u_ / n5s_ name (files.js's ownership): a file in the
// way that isn't refuses it all, every file checked before any is written. Then restart vfx_bench.bat (TOOLS-PLAN's TMO-1).
"use strict";
const fs = require("fs");
const path = require("path");
const F = require("../floatdisk/floatdisk.js");
const R = require("../floatdisk/reader.js");
const W = require("../floatdisk/files.js");
const P = require("./project.js");

const REPO = path.resolve(__dirname, "..", "..", "..");
const argv = process.argv.slice(2);
const option = (name, fallback) => W.option(argv, name, fallback);
const MODEL = "n5m_uvprobe";
const SOFT = { type: "dome", radius: 0.4, alpha: 0.3, blend: "additive" };  // the default colour, Additive: n5s_617b220c1144
// A glyph that turns plainly (no procedure of its own, unlike fxpa_globe's arturo ripple), additive by its TXI; its own colours.
const GLYPH = { type: "dome", radius: 0.4, alpha: 0.3, texture: "fxpa_symbl_003", colour: [1, 1, 1], mapping: "above" };
const TALL = { height: 0.7 };  // the copy route's
const DOMES = [
  { what: "scrolls up", route: "texture0", x: -3, shape: SOFT, motion: { scroll: [0, 0.25] } },
  { what: "turns counter-clockwise, seen from above, a turn in 12 s", route: "texture0", x: -2, shape: GLYPH, motion: { turn: 30 } },
  { what: "breathes, a breath every 2 s", route: "texture0", x: -1, shape: Object.assign({}, SOFT, { mapping: "above" }), motion: { breathe: 0.3, rate: 0.5 } },
  { what: "scrolls down", route: "copy", x: 3, shape: Object.assign({}, SOFT, TALL), motion: { scroll: [0, -0.25] } },
  { what: "turns clockwise, seen from above, a turn in 12 s", route: "copy", x: 2, shape: Object.assign({}, GLYPH, TALL), motion: { turn: -30 } },
  { what: "breathes, a breath a second", route: "copy", x: 1, shape: Object.assign({}, SOFT, { mapping: "above" }, TALL), motion: { breathe: 0.3, rate: 1 } },
  { what: "turns counter-clockwise, seen from above, a turn in 4 s", route: "texture0 + TXI", x: -4, shape: GLYPH, motion: { turn: 90 } },
];
// What a route writes beside its material: texture0 nothing; copy the texture and its TXI; "texture0 + TXI" the TXI alone.
const COPIES = { texture0: { image: false, txi: false }, copy: { image: true, txi: true }, "texture0 + TXI": { image: false, txi: true } };

function main() {
  const given = option("saved", "");
  const saved = given ? path.resolve(given) : null;
  const isSaved = saved && fs.existsSync(saved) && fs.statSync(saved).isDirectory() && path.basename(saved).toLowerCase() === "saved" &&
    path.basename(path.dirname(saved)).toLowerCase() === "vfxedit";
  if (!isSaved) throw new Error(`--saved <folder>: a bench build's vfxedit\\saved (<nwn5e-ee>\\build\\vfxedit\\saved)${given ? ", not " + given : ""}`);
  const vfxedit = path.dirname(saved);
  const tools = path.resolve(option("tools", process.env.NWN5E_TOOLS || path.join(REPO, "..", "nwn5e-tools")));
  const { core, gameData, game } = W.loadTools({ tools, game: option("game", ""), build: path.dirname(vfxedit) });
  if (!fs.existsSync(path.join(game, "data", "nwn_base.key"))) throw new Error("no game at " + game + " (--game <folder>)");
  const rd = R.create({ game, core, gameData, reader: null });  // shapes need no mesh reader
  try {
    // The domes as a spell mesh's parts: read, placed as its planes, their soft texture, their materials (files.js's, as
    // build.js makes them); then the copy route's materials without texture0.
    const placed = DOMES.map((d, i) => {
      const part = F.readPart({ shape: d.shape, at: [d.x, 0, 0], motion: d.motion }, `dome ${i + 1}`, __dirname);
      return Object.assign(F.placePart(part, rd.part(part), i, 0, "j", true), { part });
    });
    const shaped = W.shapeTextures(placed);
    const moved = W.motionMaterials(placed);
    const names = placed.map((x) => x.meshes[0].bitmap);
    names.forEach((name, i) => { if (COPIES[DOMES[i].route].image) moved.get(name).text = F.motionMaterialText(moved.get(name), { copy: true }); });
    const meshes = F.uniqueNames([].concat(...placed.map((x) => x.meshes)), []);

    const writes = [{ file: path.join(saved, MODEL + ".vfx"), bytes: Buffer.from(F.crlf(P.draftText(core, MODEL, meshes)), "latin1"), kind: "text" }];
    writes.push(...W.shapeWrites(shaped, saved), ...W.motionWrites(moved, saved));
    // Under a material's name, as its route says: the copy route's texture and its TXI (a soft one made again, a library
    // one copied from the game - reader.js's image: ovr\ first, then the archives), the seventh's TXI alone.
    names.forEach((name, i) => {
      const put = COPIES[DOMES[i].route];
      if (!put.image && !put.txi) return;
      const m = moved.get(name), soft = shaped.get(m.texture);
      const img = soft ? { tga: soft.bytes, dds: null, txi: soft.txi ? soft.txi.toString("latin1") : "" } : rd.image(m.texture, []);
      if (!img || !(img.tga || img.dds)) throw new Error(`${m.texture}: not in the game (${game})`);
      if (put.image) writes.push({ file: path.join(saved, name + (img.tga ? ".tga" : ".dds")), bytes: img.tga || img.dds, kind: "material" });
      if (put.txi && img.txi) writes.push({ file: path.join(saved, name + ".txi"), bytes: Buffer.from(img.txi, "latin1"), kind: "material" });
    });

    const shown = (file) => path.relative(vfxedit, file).split(path.sep).join("/");
    console.log(`uvprobe: ${MODEL}, ${DOMES.length} domes 0.8 m wide, 1 m apart along x - TMO-1 (#39):`);
    DOMES.forEach((d, i) => console.log(`  ${i + 1}  x ${d.x}, ${d.shape.height || d.shape.radius} m high, ${d.shape.texture === "soft" || !d.shape.texture ? "Soft, Additive" : d.shape.texture}, ` +
      `${d.shape.mapping === "above" ? "From above" : "Wrapped"}: ${d.what} - ${names[i]} (${d.route} route)`));
    // Every file checked before any is written (files.js's ownership: the draft carrying the Composer's mark, a soft
    // texture and a material or copy by its name, the shader by its first line); then the bench's list.
    const owned = W.ownership({ repo: vfxedit, manifest: new Set(), mark: P.MARK });
    const results = W.writeAll(writes, owned, { repo: vfxedit, manifest: new Set() });
    const list = path.join(vfxedit, "meshes.txt"), had = fs.existsSync(list) ? fs.readFileSync(list, "utf8") : null;
    const listed = new Set(String(had || "").split(/\r?\n/).map((l) => l.trim().toLowerCase()).filter((l) => l && !l.startsWith("#") && F.RESREF.test(l)));
    if (listed.has(MODEL)) {
      results.push(["unchanged", list]);
    } else {
      fs.writeFileSync(list, [...listed, MODEL].sort().join("\n") + "\n");  // as the bench writes it at a spell mesh's Save
      results.push([had === null ? "new" : "rebuilt", list]);
    }
    for (const [state, file] of results) console.log(state.padEnd(9) + " " + shown(file));
    console.log("Then restart the VFX Bench (close its console window, double-click vfx_bench.bat): + mesh offers n5m_uvprobe.");
  } finally {
    rd.close();
  }
}

try {
  main();
} catch (e) {
  console.error("uvprobe: " + e.message);
  process.exit(1);
}
