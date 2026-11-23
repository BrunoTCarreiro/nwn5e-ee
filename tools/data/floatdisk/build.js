#!/usr/bin/env node
// Floating Disk's four looks (#39; the owner, 2026-11-16: "I'm imagining an actual disk and there would be 4 stages to
// it: empty - just the disk; 0-33%, 33%-66%, 66%-99% of the weight they would have 3 different meshes of increasing size
// of random junk on top" - then "go with all your recommendations, let them compose it. If I don't like the
// compositions, is there a way to compose them myself?"). This is that way: composition.json beside this file says what
// each pile is made of - the game's own placeable models, or your own model files (README.md), each placed, turned,
// tilted and scaled on the disk's top - and this generator rebuilds the game's files from it:
//
// - data\files: the four creature models c_n5fd_empty / _small / _med / _large (supermodel c_invsguy - the invisible
//   man's walk and idles - with his nodes, the junk and the disk floating at its height), the three piles without the
//   disk plc_n5fd_small / _med / _large (their base on the ground) with their .pwk use points, and the disk's texture
//   n5t_forcedisk.tga; with your own files' textures the game doesn't have, copied from beside them; and a dyed part's
//   textures (a helmet's: the game has them only as a PLT), baked from the game's palettes in the part's colours into
//   n5p_<12 hex digits>.tga, which the models name instead (files.js's bakeParts); and a shape part's soft texture,
//   generated from its colour into n5s_<12 hex digits>.tga (original: files.js's shapeTextures) - an Additive shape's
//   (M1b) with n5s_<...>.txi beside it, "blending additive", in data\files and beside the drafts alike (shapeWrites);
//   and a moving part's (M2: a `motion`) materials, n5u_<12 hex digits>.mtr, which its textured meshes name instead - each
//   our vertex shader (vsn5uvmove.shd, committed in data\files and listed in made.txt) with the real texture as texture0
//   (files.js's motionWrites), beside the models and the drafts alike - a clamped one (W1: it breathes, doesn't scroll,
//   its mesh's UVs within one repeat) naming its copy instead, the texture's picture copied under the material's name with
//   a TXI of its lines and the game's clamp line (files.js's motionCopies), git-ignored as the material; and (DC) a still
//   shape's library texture whose TXI says decal, or a flipbook on a mesh whose v stays within one repeat, copied under
//   its own name, n5l_<12 hex digits>, with a TXI of its lines minus decal (+ clamp 2 within one repeat), which the models
//   name instead (files.js's copyTextures) - git-ignored, the game's art - and a still Soft shape within one repeat
//   naming its clamped soft texture (its own n5s_ name, a TXI saying clamp 2). MV: a moving shape's texture as a still
//   one's (floatdisk.js gameTextures) - its copy, its clamped soft texture, clamped in v only while it moves sideways only -
//   its material naming that as texture0; and a moving texture's material with a TXI of its own, n5u_<...>.txi, its
//   texture's blending and map lines - the engine takes a mesh's blend only from its bitmap's TXI (files.js motionMaterials).
//   AB: an Additive part ("blend": "additive" - not a shape's: its own Blend) lit by itself at its tint, its textures
//   copied for added light, n5l_<12 hex digits> with a TXI of their lines minus decal and blending + blending additive
//   (floatdisk.js addedLight, addedTextures; files.js copyTextures) - git-ignored as DC's copies.
// - data\rows: appearance.floatdisk.txt (rows 14000-14003, each appearance 298's) and placeables.floatdisk.txt (14001-
//   14003, each 189 TreasureSml's), floatdisk.js's ROWS.
//
// Nothing of the game's is kept in the repo: the parts, c_invsguy and the rows copied are read from the player's own
// game at build time (its KEY / BIF archives and ovr\, through nwn5e-tools' spell_bench\game_data.js) and the meshes
// through nwn5e-ee's own model reader (src\data\vfx_model.cpp) compiled into mdlmesh (mdlmesh.cpp, build_mdlmesh.bat;
// built into build\floatdisk\ when missing or older than its sources). The models, the baked textures and the shapes'
// soft textures (original, but build output: a pure function of their colour) it writes into data\files are
// git-ignored: build.bat makes them for every build (tools\data\build_derived.js).
//
//   node tools\data\floatdisk\build.js [options]
//     --composition <file>  default: composition.json beside this file
//     --game <folder>       the game (default: NWN_GAME_DIR, nwn5e.ini's [loader] game_dir, the Steam folder)
//     --tools <folder>      nwn5e-tools (default: NWN5E_TOOLS, else beside this repo)
//     --reader <exe>        the mesh reader (default: NWN5E_MDLMESH, else build\floatdisk\mdlmesh.exe, built if needed)
//     --files <folder>      where the models and textures go (default: data\files)
//     --rows <folder>       where the rows files go (default: data\rows)
//     --json                the summary as one line of JSON on stdout (what the VFX Bench's Composer reads); an error as
//                           { "error": message }
//     --check               read and place everything, print the summary, write nothing
//     --drafts              also the stages as VFX Bench drafts (phase 1's previews): n5_floatdisk_<stage>.vfx into
//                           <build>\vfxedit\saved and the texture into its library (--build <nwn5e build folder>,
//                           default NWN5E_BUILD or ..\nwn5e-ee\build beside nwn5e-tools; --out, --textures)
//
// It never overwrites a file it didn't make: a model, .pwk or rows file must carry the MARK line; a texture must be in
// made.txt (beside this file: the files it wrote that can't carry a mark), a baked one have a bake's name (n5p_ and 12
// hex digits: only the bake makes them), a shape's soft texture its name (n5s_ and 12 hex digits). Every file is
// checked before any is written.
"use strict";
const fs = require("fs");
const path = require("path");
const F = require("./floatdisk.js");
const R = require("./reader.js");
const W = require("./files.js");

const REPO = path.resolve(__dirname, "..", "..", "..");
const MANIFEST = path.join(__dirname, "made.txt");

// --- options ------------------------------------------------------------------------------------------------------
const argv = process.argv.slice(2);
const option = (name, fallback) => W.option(argv, name, fallback);
const CHECK = argv.includes("--check");
const DRAFTS = argv.includes("--drafts");
const TOOLS = path.resolve(option("tools", process.env.NWN5E_TOOLS || path.join(REPO, "..", "nwn5e-tools")));
const BUILD = path.resolve(option("build", process.env.NWN5E_BUILD || path.join(TOOLS, "..", "nwn5e-ee", "build")));
const FILES = path.resolve(option("files", path.join(REPO, "data", "files")));
const ROWS_DIR = path.resolve(option("rows", path.join(REPO, "data", "rows")));
const DRAFT_OUT = path.resolve(option("out", path.join(BUILD, "vfxedit", "saved")));
const DRAFT_TEXTURES = path.resolve(option("textures", path.join(BUILD, "vfxedit", "library", "textures")));
const COMPOSITION = path.resolve(option("composition", path.join(__dirname, "composition.json")));
const READER = option("reader", process.env.NWN5E_MDLMESH || "");
const JSON_OUT = argv.includes("--json");

// What the nwn5e-tools folder gives, loaded when main starts (a bad --tools is an error like any other, JSON included).
let core, gameData, GAME;
function setup() {
  ({ core, gameData, game: GAME } = W.loadTools({ tools: TOOLS, game: option("game", ""), build: BUILD }));
}

// --- writing ----------------------------------------------------------------------------------------------------------
// A file in this repo by its path from the repo's root ("data/files/x.tga"); null outside it.
const repoPath = (file) => W.repoPath(REPO, file);
// Each write { file, bytes, kind }, through files.js's one ownership rule: "text" (a model, .pwk, rows or draft: ours
// when it carries MARK), "binary" (a texture shipped in this repo: ours when made.txt lists it; outside the repo only
// an identical one is), "baked" (a dyed part's texture: ours by its name, n5p_ and 12 hex digits), "shape" (a shape's
// soft texture: ours by its name, n5s_ and 12 hex digits), "library" (the bench library's disk texture: ours when its
// TGA image ID is TEXTURE_MARK).
// -> [[ "new" | "rebuilt" | "unchanged", file ]], `manifest` gaining the binary files written in the repo.
function writeAll(writes, manifest) {
  const owned = W.ownership({ repo: REPO, manifest, mark: F.MARK, kinds: {
    library: (w, old) => old.length > 18 && old.toString("latin1", 18, 18 + old[0]) === F.TEXTURE_MARK } });
  return W.writeAll(writes, owned, { repo: REPO, manifest });
}

// --- main -----------------------------------------------------------------------------------------------------------
function main() {
  setup();
  const comp = F.readComposition(JSON.parse(fs.readFileSync(COMPOSITION, "utf8")), __dirname);
  if (!fs.existsSync(path.join(GAME, "data", "nwn_base.key"))) throw new Error("no game at " + GAME + " (--game <folder>)");
  const reader = R.ensureReader({ repo: REPO, reader: READER, log: JSON_OUT ? console.error : console.log });
  const rd = R.create({ game: GAME, core, gameData, reader });
  const res = rd.res;
  const disk = comp.disk;

  // The parts, read once each (a game model, a weapon's pieces or a file of yours: reader.js).
  const models = new Map();
  try {
    for (const stage of F.PILES) for (const p of comp.stages[stage]) if (!models.has(F.partKey(p))) models.set(F.partKey(p), rd.part(p));
  } finally {
    rd.close();
  }

  // c_invsguy's nodes (the creature models copy them where they are).
  const invs = res(F.SUPERMODEL, 2002);
  if (!invs) throw new Error(`${F.SUPERMODEL}: not in the game`);
  const nodes = F.modelNodes(invs);
  const taken = nodes.map((n) => n.name);

  // Each stage: its parts placed on the disk's top, a dyed part's textures renamed and baked (`baked`); the creature's
  // meshes (the junk, then the disk: see-through, drawn over it) and the pile's (the junk lowered by the disk's height,
  // its base on the ground).
  const diskMesh = F.diskMesh(disk);
  const stages = {};
  const baked = new Map();  // a baked texture's name -> { plt, colours, bytes, label } (files.js's bakeParts)
  const shaped = new Map();  // a shape's soft texture's name -> { colour, bytes, label } (files.js's shapeTextures)
  const placed = {};
  for (const stage of F.STAGES) {
    const parts = comp.stages[stage].map((p, i) => Object.assign(F.placePart(p, models.get(F.partKey(p)), i, disk.height, "j"), { part: p }));
    W.shapeTextures(parts, shaped);
    W.bakeParts(parts, rd, baked);
    placed[stage] = parts;
  }

  // Textures: a part from the game has the game's (a dyed one's baked, above); one of your own files' is looked up (the
  // game, then beside the file).
  const ship = W.shipTextures([].concat(...F.PILES.map((stage) => placed[stage])), rd, baked);  // texture name -> its copies beside a file

  // A moving part's textured meshes (M2) renamed for their materials (files.js's motionMaterials: after the bakes and the
  // textures shipped, so texture0 is the real texture); then the creature's meshes and the pile's.
  const moved = new Map();  // a material's name -> { texture, motion, pivot, clamp?, text, label, copy? }
  // DC (SHP-8): a still shape's library texture the game must have copied (its TXI says decal, or a flipbook within one
  // repeat) renamed for its copy, n5l_<12 hex digits>, its picture and TXI beside it (files.js copyTextures). MV: a moving
  // shape's too (floatdisk.js gameTextures) - its material names the copy as texture0 - and each moving texture's material
  // a TXI of its own with its texture's blending and map lines (files.js motionMaterials, given where the TXIs are read).
  const copied = new Map();  // a copy's name -> { texture, clamp, decal, ext, bytes, txi, label }
  for (const stage of F.STAGES) {
    const parts = placed[stage];
    W.copyTextures(parts, copied, { rd, shaped, baked, ship });
    W.motionMaterials(parts, moved, { sources: { rd, shaped, baked, ship }, copies: copied });
    const junk = [].concat(...parts.map((x) => x.meshes));
    const creature = F.uniqueNames([...junk, diskMesh], taken);
    const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
    for (const x of parts) for (let k = 0; k < 3; k++) { lo[k] = Math.min(lo[k], x.lo[k]); hi[k] = Math.max(hi[k], x.hi[k]); }
    const pile = parts.length ? F.uniqueNames(F.lowered(junk, disk.height), []) : [];
    stages[stage] = { parts, creature, pile, lo, hi };
  }
  // W1: a clamped material's copy (a breathing texture that doesn't scroll, on a mesh within one repeat): its picture and a
  // TXI of its lines and the game's clamp line, under the material's name beside it (files.js motionCopies).
  W.motionCopies(moved, { rd, shaped, baked, ship, copies: copied });  // (AB: a breathing Additive part's own copy first)

  // The summary: per stage its parts, triangles, how high the pile goes and how far it reaches.
  const r = disk.diameter / 2;
  const round3 = (v) => Math.round(v * 1000) / 1000;
  const say = JSON_OUT ? () => {} : console.log;
  const summary = {};
  for (const stage of F.STAGES) {
    const { parts, creature, hi } = stages[stage];
    const tris = creature.reduce((n, m) => n + m.faces.length / 3, 0);
    const top = parts.length ? hi[2] : disk.height;
    say(`${F.creatureModel(stage)}: ${parts.length} part${parts.length === 1 ? "" : "s"}, ${creature.length} meshes, ${tris} triangles; ` +
      `the pile ${(top - disk.height).toFixed(2)} m tall, its top ${top.toFixed(2)} m above the ground` +
      (parts.length ? `; ${F.pileModel(stage)} ${(hi[2] - disk.height).toFixed(2)} m tall on the ground` : ""));
    summary[stage] = { model: F.creatureModel(stage), parts: parts.length, meshes: creature.length, triangles: tris, top: round3(top), tall: round3(top - disk.height),
      pile: parts.length ? { model: F.pileModel(stage), tall: round3(hi[2] - disk.height) } : null, list: [] };
    for (const x of parts) {
      const over = x.reach - r;
      say(`  ${F.partLabel(x.part).padEnd(16)} ${(x.hi[0] - x.lo[0]).toFixed(2)} x ${(x.hi[1] - x.lo[1]).toFixed(2)} x ${(x.hi[2] - x.lo[2]).toFixed(2)} m, ` +
        `${(x.lo[2] - disk.height).toFixed(2)}..${(x.hi[2] - disk.height).toFixed(2)} above the disk` +
        (over > 0.005 ? `, reaches ${(over * 100).toFixed(0)} cm past the rim` : ""));
      summary[stage].list.push({ label: F.partLabel(x.part), size: [0, 1, 2].map((k) => round3(x.hi[k] - x.lo[k])),
        from: round3(x.lo[2] - disk.height), to: round3(x.hi[2] - disk.height), past: round3(Math.max(0, over)) });
    }
  }
  for (const [name, copies] of ship) say(`  texture ${name}: not in the game - shipped from ${copies.join(", ")}`);
  W.bakedLines(baked, W.shown(REPO, FILES)).forEach((l) => say(l));
  W.shapeLines(shaped, W.shown(REPO, FILES)).forEach((l) => say(l));
  W.copyLines(copied, W.shown(REPO, FILES)).forEach((l) => say(l));
  W.motionLines(moved, W.shown(REPO, FILES)).forEach((l) => say(l));

  // The files.
  const writes = [];
  const text = (file, body) => writes.push({ file, bytes: Buffer.from(F.crlf(body), "latin1"), kind: "text" });
  for (const stage of F.STAGES) {
    const s = stages[stage], name = F.creatureModel(stage);
    text(path.join(FILES, name + ".mdl"), F.creatureModelText(name, nodes, s.creature, [
      `Floating Disk (#39), the ${stage} stage${stage === "empty" ? "" : " (" + { small: "0-33%", medium: "33-66%", large: "66-99%" }[stage] + " of its load)"}:`,
      `the disk of force, its top ${disk.height} m up, ${s.parts.length ? "and the junk on it, " : ""}straight under the root - it floats level while`,
      `rootdummy walks. ${F.SUPERMODEL}'s nodes as dummies where his are. The game's textures by name. Appearance row ${F.ROWS.appearance[stage]}.`]));
    if (stage === "empty") continue;
    const pile = F.pileModel(stage);
    const lo = [s.lo[0], s.lo[1], s.lo[2] - disk.height], hi = [s.hi[0], s.hi[1], s.hi[2] - disk.height];
    text(path.join(FILES, pile + ".mdl"), F.pileModelText(pile, s.pile, lo, hi, [
      `Floating Disk (#39): the ${stage} stage's junk without the disk, its base on the ground - what falls where the disk`,
      `floated, a container. Laid out as the game's junk placeables (PLC_B04, PLC_C07). Placeables row ${F.ROWS.placeables[stage]}.`]));
    text(path.join(FILES, pile + ".pwk"), F.pileWalkmeshText(pile, lo, hi));
  }
  writes.push({ file: path.join(FILES, disk.texture + ".tga"), bytes: F.diskTexture(disk), kind: "binary" });
  for (const [name, copies] of ship) {
    for (const c of copies) writes.push({ file: path.join(FILES, name + path.extname(c).toLowerCase()), bytes: fs.readFileSync(c), kind: "binary" });
  }
  writes.push(...W.bakedWrites(baked, FILES));  // beside the models that name them, git-ignored; never in made.txt
  writes.push(...W.shapeWrites(shaped, FILES));  // likewise: build output, git-ignored; never in made.txt
  writes.push(...W.copyWrites(copied, FILES));  // DC: a library texture's copies (the game's art: git-ignored); never in made.txt
  writes.push(...W.motionWrites(moved, FILES));  // a moving texture's materials (git-ignored) and the shader (committed, in made.txt)
  const appearance = gameData.gameTable(GAME, "appearance"), placeables = gameData.gameTable(GAME, "placeables");
  text(path.join(ROWS_DIR, "appearance.floatdisk.txt"), F.rowsText(appearance, F.SOURCE_ROWS.appearance,
    F.STAGES.map((st) => ({ row: F.ROWS.appearance[st], label: F.LABELS.appearance[st], model: F.creatureModel(st) })),
    { labelColumn: "LABEL", modelColumn: "RACE", overrides: F.APPEARANCE_OVERRIDES, about: [
      "appearance.2da: Floating Disk's four stages (#39) - the disk a henchman, its stage swapped by SetCreatureAppearanceType.",
      `Each row is the game's row ${F.SOURCE_ROWS.appearance} (Invisible_Human_Male) with its LABEL and model (RACE) changed, TARGETABLE 1`,
      `(${F.SOURCE_ROWS.appearance}'s 0 would make the disk unclickable, and its inventory opens on a click) and FOOTSTEPTYPE -1`,
      "(it floats: no footsteps, as the game's floating creatures)."] }));
  text(path.join(ROWS_DIR, "placeables.floatdisk.txt"), F.rowsText(placeables, F.SOURCE_ROWS.placeables,
    F.PILES.map((st) => ({ row: F.ROWS.placeables[st], label: F.LABELS.placeables[st], model: F.pileModel(st) })),
    { labelColumn: "Label", modelColumn: "ModelName", about: [
      "placeables.2da: Floating Disk's piles (#39) - its load, fallen where it floated when it ends, a container.",
      `Each row is the game's row ${F.SOURCE_ROWS.placeables} (TreasureSml) with its Label and ModelName changed; a pile's row is its stage's appearance row.`] }));
  if (DRAFTS) {
    for (const stage of F.STAGES) {
      const planes = stages[stage].creature.map((m) => core.plane({ name: m.name, bitmap: m.bitmap || "null", verts: m.verts, tverts: m.tverts,
        faces: m.faces, diffuse: m.diffuse, ambient: m.ambient, selfillum: m.selfillum, alpha: m.alpha,
        transparencyhint: F.meshHint(m) }));  // T1b: as the models' meshLines writes it
      const name = "n5_floatdisk_" + stage;
      const draft = F.MARK + "\n" + core.writeDraft(core.effect({ name, duration: true, seconds: 2, attach: "root", planes }));
      writes.push({ file: path.join(DRAFT_OUT, name + ".vfx"), bytes: Buffer.from(F.crlf(draft), "latin1"), kind: "text" });
    }
    writes.push({ file: path.join(DRAFT_TEXTURES, disk.texture + ".tga"), bytes: F.diskTexture(disk, F.TEXTURE_MARK), kind: "library" });
    writes.push(...W.bakedWrites(baked, DRAFT_OUT));  // the drafts name them: beside them, where the game reads the bench's
    writes.push(...W.shapeWrites(shaped, DRAFT_OUT));
    writes.push(...W.copyWrites(copied, DRAFT_OUT));
    writes.push(...W.motionWrites(moved, DRAFT_OUT));
  }
  const result = { check: CHECK, stages: summary, shipped: [...ship].map(([texture, from]) => ({ texture, from })), baked: W.bakedJson(baked),
    shapes: W.shapeJson(shaped), copies: W.copyJson(copied), materials: W.motionJson(moved), files: [], wouldWrite: writes.length, unused: [] };
  const finish = () => { if (JSON_OUT) console.log(JSON.stringify(result)); };
  if (CHECK) {
    say(`--check: nothing written (${writes.length} files would be)`);
    finish();
    return;
  }
  const manifest = W.readManifest(MANIFEST);
  const results = writeAll(writes, manifest);
  for (const [state, file] of results) {
    say(state.padEnd(9) + " " + W.shown(REPO, file));
    result.files.push({ state, file: W.shown(REPO, file) });
  }
  W.writeManifest(MANIFEST, manifest, ["# made by nwn5e-ee tools/data/floatdisk/build.js: the files it wrote in this repo that can't carry its mark (textures) -",
    "# it may rewrite these; it refuses any other file in its way"]);
  for (const f of manifest) {
    // The moving textures' shader (M2) is committed whole and listed here, but written only beside a material: never unused.
    if (path.posix.basename(f).toLowerCase() === F.MOTION_SHADER + ".shd") continue;
    if (!writes.some((w) => repoPath(w.file) === f)) {
      say(`no longer used: ${f} (made by this generator) - delete it and its made.txt line if nothing needs it`);
      result.unused.push(f);
    }
  }
  finish();
}

try {
  main();
} catch (e) {
  if (JSON_OUT) console.log(JSON.stringify({ error: e.message }));
  console.error("floatdisk: " + e.message);
  process.exit(1);
}
