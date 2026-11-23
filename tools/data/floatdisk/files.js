// The generators' file plumbing (#39), shared by the Floating Disk's build.js and the Composer's
// (tools\data\composer\build.js): their options, loading nwn5e-tools, a repo file's path, a dyed part's baked textures,
// a shape's soft texture, a moving texture's material and shader (M2), the textures a part's own file brings, and
// writing only files that are the generator's own - one ownership rule for both. Node only.
//
//   const W = require("./files.js");
//   W.option(argv, "game", "")   W.loadTools({ tools, game, build })   W.bakeParts(placed, rd, baked)   W.shipTextures(placed, rd, baked)
//   W.bakedWrites(baked, dir)   W.bakedLines(baked, where)   W.bakedJson(baked)
//   W.shapeTextures(placed, made)   W.shapeWrites(made, dir)   W.shapeLines(made, where)   W.shapeJson(made)
//   W.motionMaterials(placed, materials, options)   W.motionCopies(materials, { rd, shaped, baked, ship })   W.motionPicture(texture, sources)
//   W.motionWrites(materials, dir)   W.motionLines(materials, where)   W.motionJson(materials)
//   W.writeAll(writes, W.ownership({ repo, manifest, mark, kinds }), { repo, manifest })   W.writeManifest(file, manifest, header)
"use strict";
const fs = require("fs");
const path = require("path");
const F = require("./floatdisk.js");

// --- options and nwn5e-tools ------------------------------------------------------------------------------------------
// `--<name> <value>` in argv, else `fallback`.
function option(argv, name, fallback) {
  const i = argv.indexOf("--" + name);
  return i >= 0 && i + 1 < argv.length ? argv[i + 1] : fallback;
}
function need(file, what) {
  if (!fs.existsSync(file)) throw new Error(`${what} isn't there: ${file}`);
  return file;
}
// What the nwn5e-tools folder gives: the VFX Bench's core.js, spell_bench's game data reader, and the game's folder
// (anim_helper's findGameDir: `game`, else NWN_GAME_DIR, nwn5e.ini's [loader] game_dir, the Steam folder). Called when a
// generator's main starts, so a bad --tools is an error like any other, JSON included.
function loadTools({ tools, game, build }) {
  global.window = global.window || {};  // core.js runs in the page and under node
  const core = require(need(path.join(tools, "vfx_bench", "public", "core.js"), "nwn5e-tools' VFX Bench core (--tools)"));
  const gameData = require(need(path.join(tools, "spell_bench", "game_data.js"), "nwn5e-tools' game data reader (--tools)"));
  const helper = require(need(path.join(tools, "anim_helper", "helper.js"), "nwn5e-tools' anim helper (--tools)"));
  return { core, gameData, game: helper.findGameDir({ game, buildDir: build }) };
}

// --- paths ------------------------------------------------------------------------------------------------------------
// A file in the repo by its path from the repo's root ("data/files/x.tga"); null outside it.
function repoPath(repo, file) {
  const rel = path.relative(repo, file);
  return rel.startsWith("..") || path.isAbsolute(rel) ? null : rel.split(path.sep).join("/");
}
// A file as the generators name it in what they print: from the repo's root, else in full.
const shown = (repo, file) => repoPath(repo, file) || file;

// --- dyed parts -------------------------------------------------------------------------------------------------------
// A dyed part's textures - the game has them only as a PLT - renamed for its colours and baked from the game's palettes
// (floatdisk.js's bakeBitmaps, reader.js's bake): each placed part's meshes replaced by their renamed copies (x.meshes),
// and each bake `baked` lacks added. `placed`: [{ part, meshes }] (placePart's, with its part). -> `baked`: Map(name ->
// { plt, colours (only what isn't a default), bytes (its TGA), label (the part's) }), in the parts' order.
function bakeParts(placed, rd, baked = new Map()) {
  for (const x of placed) {
    const b = F.bakeBitmaps(x.meshes, x.part.colours, rd.pltOnly);
    x.meshes = b.meshes;
    for (const k of b.bakes) {
      if (!baked.has(k.name)) baked.set(k.name, { plt: k.plt, colours: k.colours, bytes: rd.bake(k.plt, x.part.colours).bytes, label: F.partLabel(x.part) });
    }
  }
  return baked;
}
// The bakes as writes into `dir` (the models' folder, or a spell mesh's drafts'): <dir>\<name>.tga, kind "baked" - not
// noted in made.txt: whose it is, its name says (ownership).
const bakedWrites = (baked, dir) => [...baked].map(([name, b]) => ({ file: path.join(dir, name + ".tga"), bytes: b.bytes, kind: "baked" }));
// The summary's line a bake: its PLT and the part's six colours, in the card's order, and where it goes.
const bakedLines = (baked, where) => [...baked].map(([name, b]) => `  texture ${name}: ${b.plt} dyed with ` +
  F.PLT_FIELDS.map((k) => `${F.PLT_LABELS[k].toLowerCase()} ${b.colours[k] !== undefined ? b.colours[k] : F.PLT_DEFAULTS[k]}`).join(", ") + ` - baked into ${where}`);
// --json's: [{ texture, plt, colours (only what isn't a default) }].
const bakedJson = (baked) => [...baked].map(([texture, b]) => ({ texture, plt: b.plt, colours: b.colours }));

// --- a shape's soft texture -------------------------------------------------------------------------------------------
// The soft textures the placed shape parts name (floatdisk.js's shapeModel: `textures`, none for `none` or a library
// texture), each generated from its colour: original, nothing of the game's. Each `made` lacks is added. `placed`:
// [{ part, meshes }] (placePart's, with its part). DC: a still shape's soft mesh whose v stays within one repeat
// (floatdisk.js stillTextures: its part doesn't move) is renamed in place (x.meshes) for its clamped soft texture - its
// own name, its TXI saying clamp 2 - and only the soft textures its meshes name are made (a part given without meshes:
// its shape's, as before). -> `made`: Map(name -> { colour, blend (M1b: additive | normal), clamp: true (DC: a clamped
// one's only), bytes (its 32-bit TGA), txi (its TXI, floatdisk.js's softTxi: an Additive one's "blending additive", a
// clamped one's "clamp 2"; null for a Normal unclamped one), label (the part's) }), in the parts' order.
function shapeTextures(placed, made = new Map()) {
  for (const x of placed) {
    if (x.part.source.kind !== "shape") continue;
    const shape = x.part.source.shape;
    // (MV: the shape rule for a moving part too - floatdisk.js gameTextures: its soft texture clamped while it moves
    // sideways only, within one repeat.)
    const still = x.meshes ? F.gameTextures(shape, x.part.motion, x.meshes, () => null) : null;
    if (still) x.meshes = still.meshes;
    const named = still ? new Set(x.meshes.map((m) => String(m.bitmap || "").toLowerCase())) : null;
    const textures = [...F.shapeModel(shape).textures.filter((t) => !named || named.has(t.name)), ...(still ? still.softs : [])];
    for (const { name, colour, blend, clamp } of textures) {
      if (made.has(name)) continue;
      const txi = F.softTxi({ blend, clamp });
      made.set(name, Object.assign({ colour, blend }, clamp ? { clamp: true } : {}, { bytes: F.tgaBytes(F.shapeTexture({ colour }), { alpha: true }),
        txi: txi ? Buffer.from(txi, "latin1") : null, label: F.partLabel(x.part) }));
    }
  }
  return made;
}
// The soft textures as writes into `dir` (beside what names them: the models' folder, or a spell mesh's drafts'):
// <dir>\<name>.tga and, one with a TXI (M1b: an additive one's "blending additive", so the game draws it on its additive
// path; DC: a clamped one's "clamp 2"), <dir>\<name>.txi beside it - each kind "shape", not noted in made.txt: whose it
// is, its name says (ownership).
const shapeWrites = (made, dir) => [].concat(...[...made].map(([name, s]) => [{ file: path.join(dir, name + ".tga"), bytes: s.bytes, kind: "shape" },
  ...(s.txi ? [{ file: path.join(dir, name + ".txi"), bytes: s.txi, kind: "shape" }] : [])]));
// The summary's line a soft texture: its colour (an additive one's blend, a clamped one's clamp - DC - and its TXI's
// lines), and where it goes.
const shapeLines = (made, where) => [...made].map(([name, s]) => `  texture ${name}: a shape's soft texture, colour ${s.colour.join(" ")}` +
  (s.blend === "additive" ? ", additive" : "") + (s.clamp ? ", clamped in v" : "") +
  (s.txi ? ` (${name}.txi beside it: ${Buffer.from(s.txi).toString("latin1").trim().split(/\r?\n/).join(", ")})` : "") + ` - written into ${where}`);
// --json's: [{ texture, colour, blend (only "additive": a Normal one's as before M1b), clamp (DC: only true, a clamped one's) }].
const shapeJson = (made) => [...made].map(([texture, s]) => Object.assign({ texture, colour: s.colour }, s.blend === "additive" ? { blend: "additive" } : {},
  s.clamp ? { clamp: true } : {}));

// --- a moving texture's material (M2) ---------------------------------------------------------------------------------
// The placed parts' moving textures: each part with a motion has its textured meshes renamed for it (floatdisk.js's
// moveTextures: the bitmap becomes the material's name, n5u_ and 12 hex digits) - each placed part's meshes replaced by
// their renamed copies (x.meshes) - and each material `materials` lacks added. Called after bakeParts and shipTextures,
// so a material's texture (texture0) is the real one and a shipped texture is found by its own name. `placed`: [{ part,
// meshes }] (placePart's, with its part); `options`: { clamp } (moveTextures': { clamp: false } is W1's rule off -
// uvprobe's) and (MV) `sources` (motionPicture's: { rd, shaped, baked, ship } - where each texture's TXI is read) and
// `copies` (copyTextures' map: a copy's TXI, n5l_...). Called after shapeTextures and copyTextures, so a moving shape's
// texture is the one the shape rule gave it (floatdisk.js gameTextures: its copy, its clamped soft texture).
// -> `materials`: Map(name -> { texture, motion (readMotion's), pivot, clamp: true (W1: a clamped one's only - its copy
// comes from motionCopies), side: true (MV: its texture0 clamped in v, sideways only), txi (MV: its own TXI's text -
// floatdisk.js motionTxi, its texture's blending and map lines - when it has one), text (its .mtr: motionMaterialText, LF - a
// clamped one's naming its copy, no texture0), label (the part's) }), in the parts' order. Without `sources` no TXI is
// read: every material as before MV (uvprobe: TMO-1's domes as they were).
function motionMaterials(placed, materials = new Map(), options = {}) {
  const { sources = null, copies = new Map() } = options;
  const txis = new Map();
  const txiOf = (t) => {
    if (!txis.has(t)) {
      const pic = copies.has(t) ? { txi: copies.get(t).txi } : motionPicture(t, sources);
      txis.set(t, pic ? txiText(pic.txi) : null);
    }
    return txis.get(t);
  };
  for (const x of placed) {
    const shape = x.part.source.kind === "shape" ? x.part.source.shape : null;
    const moved = F.moveTextures(x.meshes, x.part.motion, Object.assign({ shape }, "clamp" in options ? { clamp: options.clamp } : {}, sources ? { txiOf } : {}));
    x.meshes = moved.meshes;
    for (const m of moved.materials) {
      if (!materials.has(m.name)) {
        materials.set(m.name, Object.assign({ texture: m.texture, motion: m.motion, pivot: m.pivot }, m.clamp ? { clamp: true } : {}, m.side ? { side: true } : {},
          m.txi ? { txi: m.txi } : {}, { text: F.motionMaterialText(m), label: F.partLabel(x.part) }));
      }
    }
  }
  return materials;
}
// A moving texture's picture, to copy under a material's name (TMO-1's copy route; W1's clamped materials, uvprobe's copy
// domes): a shape's soft one as made (`shaped`, shapeTextures': its TGA and an Additive one's TXI), a bake (`baked`,
// bakeParts': its TGA, no TXI), else reader.js's image (`rd`: the game's - ovr\ first, then its archives - else beside a
// part's own file, `ship`'s folder for a texture shipped from there, or `dirs`): its DDS when it has one (what the game
// draws), else its TGA, with its TXI. -> { ext: ".tga" | ".dds", bytes, txi (its text, "" for none) } | null.
// AB: a copy made in this build (`copies`, copyTextures': an Additive part's n5l_ copy - its picture and its additiveTxi -
// or DC's) as made, first: a breathing Additive part's W1 copy is of its own copy, its TXI's "blending additive" kept.
const txiText = (txi) => (!txi ? "" : typeof txi === "string" ? txi : Buffer.from(txi).toString("latin1"));
function motionPicture(texture, { rd, shaped = new Map(), baked = new Map(), ship = new Map(), dirs = [], copies = new Map() }) {
  const copy = copies.get(texture);
  if (copy && copy.bytes) return { ext: copy.ext, bytes: copy.bytes, txi: txiText(copy.txi) };
  const soft = shaped.get(texture);
  if (soft) return { ext: ".tga", bytes: soft.bytes, txi: txiText(soft.txi) };
  const bake = baked.get(texture);
  if (bake) return { ext: ".tga", bytes: bake.bytes, txi: "" };
  const beside = ship.has(texture) ? [path.dirname(ship.get(texture)[0])] : [];
  const img = rd ? rd.image(texture, [...new Set([...beside, ...dirs])]) : null;
  if (!img || !(img.dds || img.tga)) return null;
  return { ext: img.dds ? ".dds" : ".tga", bytes: img.dds || img.tga, txi: txiText(img.txi) };
}
// W1: each clamped material's copy (`materials`, motionMaterials'), its `copy` set: { ext, bytes (its picture,
// motionPicture's from `sources`), txi (bytes: floatdisk.js's clampedTxi - the source's TXI lines and the game's clamp
// line) } - motionWrites writes it under the material's name. A clamped texture with no picture anywhere stops the build.
// -> `materials`.
function motionCopies(materials, sources) {
  for (const m of materials.values()) {
    if (!m.clamp || m.copy) continue;
    const pic = motionPicture(m.texture, sources);
    if (!pic) throw new Error(`${m.label}: texture ${m.texture} - its clamped material needs its picture copied, and it's neither in the game nor beside the part`);
    m.copy = { ext: pic.ext, bytes: pic.bytes, txi: Buffer.from(F.clampedTxi(pic.txi), "latin1") };
  }
  return materials;
}
// The materials as writes into `dir` (beside what names them: the models' folder, or a spell mesh's drafts'):
// <dir>\<name>.mtr - and a material with a `copy` (W1's clamped one, motionCopies'; uvprobe's copy domes) its picture
// <dir>\<name>.tga or .dds and its TXI <dir>\<name>.txi when it has them - MV: a material with a TXI of its own (`txi`,
// its texture's blending and map lines) its <dir>\<name>.txi, LF - each kind "material" (ours by its name: only the generator
// makes n5u_ names, and the name fixes the bytes), and - when there's any - the shader they name,
// <dir>\vsn5uvmove.shd, kind "shader" (ours when made.txt lists it, as data\files' committed one, or when it's the
// generators' shader: floatdisk.js's isMotionShader), CRLF as the generators write text. None noted in made.txt.
function motionWrites(materials, dir) {
  if (!materials.size) return [];
  const writes = [];
  for (const [name, m] of materials) {
    writes.push({ file: path.join(dir, name + ".mtr"), bytes: Buffer.from(m.text, "latin1"), kind: "material" });
    if (m.copy && m.copy.bytes) writes.push({ file: path.join(dir, name + m.copy.ext), bytes: m.copy.bytes, kind: "material" });
    if (m.copy && m.copy.txi) writes.push({ file: path.join(dir, name + ".txi"), bytes: m.copy.txi, kind: "material" });
    else if (m.txi) writes.push({ file: path.join(dir, name + ".txi"), bytes: Buffer.from(m.txi, "latin1"), kind: "material" });
  }
  writes.push({ file: path.join(dir, F.MOTION_SHADER + ".shd"), bytes: Buffer.from(F.crlf(F.motionShaderText()), "latin1"), kind: "shader" });
  return writes;
}
// A motion in words: "scrolls 0 0.25 a second, turns 30 degrees a second, breathes 0.3 at 0.5 a second" (what it does).
const motionWords = (m) => [m.scroll.some((v) => v !== 0) ? `scrolls ${m.scroll.join(" ")} a second` : null, m.turn ? `turns ${m.turn} degrees a second` : null,
  m.breathe ? `breathes ${m.breathe} at ${m.rate} a second` : null].filter(Boolean).join(", ");
// The summary's line a material: its texture, its motion and pivot, a clamped one's copy (W1), MV's clamp in v and its
// TXI's lines, where it goes (and the shader beside it).
const motionLines = (materials, where) => [...materials].map(([name, m]) => `  material ${name}: ${m.texture} ${motionWords(m.motion)}, round ` +
  `${m.pivot.join(" ")}${m.clamp ? `, clamped: ${m.texture} copied as ${name}${m.copy ? m.copy.ext : ".tga"} with ${name}.txi (its TXI and ${F.CLAMP_TXI})` : ""}` +
  `${m.side ? ", clamped in v (scrolling sideways only)" : ""}${m.txi ? `, ${name}.txi beside it: ${m.txi.trim().split(/\r?\n/).join(", ")}` : ""}` +
  ` - written into ${where}, with ${F.MOTION_SHADER}.shd`);
// --json's: [{ material, texture, motion (its non-zero keys: cleanMotion's), pivot, clamp: true (W1: a clamped one's only),
// side: true (MV: clamped in v, sideways only), txi (MV: its own TXI's text, when it has one) }].
const motionJson = (materials) => [...materials].map(([material, m]) => Object.assign({ material, texture: m.texture, motion: F.cleanMotion(m.motion), pivot: m.pivot },
  m.clamp ? { clamp: true } : {}, m.side ? { side: true } : {}, m.txi ? { txi: m.txi } : {}));

// --- DC: a library texture copied without its decal mark, a flipbook clamped in v ---------------------------------------
// The placed still shape parts' library textures the game must have copied (floatdisk.js stillTextures: a TXI that says
// decal, or a flipbook on a mesh whose v stays within one repeat; a part that moves goes through its material): each such
// mesh renamed in place (x.meshes) for its copy, n5l_<12 hex digits> (its texture, clamp and key: the source's TXI text and
// picture bytes, floatdisk.js copyKey), and each copy `copies` lacks added - its picture as
// the copy path finds it (motionPicture, `sources`: the game's - its DDS when it has one - or one beside a part's own file)
// and its TXI (floatdisk.js copiedTxi: its lines minus decal, + clamp 2 within one repeat). A texture found nowhere (nor its
// TXI) is left as it is - the game hasn't it either; without a reader nothing is copied. Called after bakeParts and
// shipTextures, before motionMaterials. `placed`: [{ part, meshes }]. -> `copies`: Map(name -> { texture, clamp (0 or 2),
// decal (its TXI said so), additive: true (AB: an Additive part's copy only), ext, bytes, txi (bytes), label (the part's)
// }), in the parts' order.
// AB (the owner, 2026-11-23: a game weapon glowing as the generated blade does): an Additive part's - a game model, a
// weapon's pieces or your own file (floatdisk.js isAdditivePart; a shape's blend is its own) - textured meshes, still or
// moving, each renamed in place for its copy made for added light (floatdisk.js addedTextures: n5l_<12 hex digits>, the
// additive mark hashed in), its picture as the copy path finds it (the game's - its DDS when it has one - or beside your
// file: `ship`) and its TXI floatdisk.js additiveTxi's: its lines minus decal and blending, + "blending additive". A dyed
// part's bake (n5p_) stops the build, naming the part (addedTextures refuses it). A moving one's material then names the
// copy as texture0 (motionMaterials: its own TXI the copy's blending line).
function copyTextures(placed, copies = new Map(), sources = {}) {
  const pictures = new Map();
  const picture = (t) => {
    if (!pictures.has(t)) pictures.set(t, sources.rd ? motionPicture(t, sources) : null);
    return pictures.get(t);
  };
  for (const x of placed) {
    if (!x.meshes) continue;
    if (x.part.source.kind !== "shape") {
      if (!F.isAdditivePart(x.part)) continue;
      let added;
      try {
        added = F.addedTextures(x.part, x.meshes, (t) => {
          const pic = picture(t);  // (its key - its TXI's text and its picture's bytes - names the copy, as DC's: floatdisk.js copyKey)
          return pic ? { key: F.copyKey(pic.txi, pic.bytes) } : null;
        });
      } catch (e) {
        throw new Error(`${F.partLabel(x.part)}: ${e.message}`);
      }
      x.meshes = added.meshes;
      for (const c of added.copies) {
        if (copies.has(c.name)) continue;
        const pic = picture(c.texture);
        copies.set(c.name, { texture: c.texture, clamp: 0, decal: F.txiDecal(pic.txi), additive: true, ext: pic.ext, bytes: pic.bytes,
          txi: Buffer.from(F.additiveTxi(pic.txi), "latin1"), label: F.partLabel(x.part) });
      }
      continue;
    }
    // (MV: a moving part's too - floatdisk.js gameTextures: its decal texture copied, a flipbook moving sideways only too,
    // clamped in v while it does; its material names the copy as texture0.)
    const still = F.gameTextures(x.part.source.shape, x.part.motion, x.meshes, (t) => {
      const pic = picture(t);  // (its key - its TXI's text and its picture's bytes - names the copy: floatdisk.js copyKey)
      return pic ? { decal: F.txiDecal(pic.txi), cycle: F.txiCycle(pic.txi), key: F.copyKey(pic.txi, pic.bytes) } : null;
    });
    x.meshes = still.meshes;
    for (const c of still.copies) {
      if (copies.has(c.name)) continue;
      const pic = picture(c.texture);
      copies.set(c.name, { texture: c.texture, clamp: c.clamp, decal: F.txiDecal(pic.txi), ext: pic.ext, bytes: pic.bytes,
        txi: Buffer.from(F.copiedTxi(pic.txi, c.clamp), "latin1"), label: F.partLabel(x.part) });
    }
  }
  return copies;
}
// The copies as writes into `dir` (beside what names them: the models' folder, or a spell mesh's drafts'): <dir>\<name>
// .tga or .dds and <dir>\<name>.txi, each kind "copy" (ours by its n5l_ name: only the generator makes them, and the name
// fixes the bytes) - git-ignored in data\files (/data/files/n5l_*: a copy is the game's art), never noted in made.txt.
const copyWrites = (copies, dir) => [].concat(...[...copies].map(([name, c]) => [{ file: path.join(dir, name + c.ext), bytes: c.bytes, kind: "copy" },
  { file: path.join(dir, name + ".txi"), bytes: c.txi, kind: "copy" }]));
// The summary's line a copy: its texture, what's changed in it, and where it goes (AB: an Additive part's, made for added
// light - its TXI without decal or blending, then blending additive).
const copyLines = (copies, where) => [...copies].map(([name, c]) => (c.additive
  ? `  texture ${name}: ${c.texture} copied for added light, an Additive part's (${name}${c.ext}, ${name}.txi: its TXI without ${c.decal ? "decal or " : ""}` +
    `blending, and blending additive) - written into ${where}`
  : `  texture ${name}: ${c.texture} copied${c.decal ? " without its decal mark" : ""}` +
    `${c.clamp ? ", clamped in v" : ""} (${name}${c.ext}, ${name}.txi: its TXI${c.decal ? " without decal" : ""}${c.clamp ? `, and clamp ${c.clamp}` : ""}) - written into ${where}`));
// --json's: [{ copy, texture, clamp (0 or 2), additive (AB: only true, an Additive part's copy) }].
const copyJson = (copies) => [...copies].map(([copy, c]) => Object.assign({ copy, texture: c.texture, clamp: c.clamp }, c.additive ? { additive: true } : {}));

// --- textures ---------------------------------------------------------------------------------------------------------
// The textures that the placed parts' own files bring and the game doesn't have: each mesh's, looked up in the game,
// then beside its file (reader.js's findTexture); a baked one (`baked`, bakeParts' - renamed, so the game hasn't it) is
// passed over. `placed`: [{ part, meshes }] (placePart's, with the part). -> Map(name -> its copies beside a file: the
// image, and its .txi), in the parts' order. A texture found nowhere stops the build.
function shipTextures(placed, rd, baked = new Map()) {
  const ship = new Map();
  for (const x of placed) {
    if (x.part.source.kind !== "file") continue;
    for (const m of x.meshes) {
      if (!m.bitmap || ship.has(m.bitmap) || baked.has(m.bitmap)) continue;
      const found = rd.findTexture(m.bitmap, [path.dirname(x.part.source.file)]);
      if (!found) throw new Error(`${F.partLabel(x.part)}: texture ${m.bitmap} is neither in the game nor beside the file (${m.bitmap}.tga / .dds)`);
      if (found.where === "beside") ship.set(m.bitmap, found.copies);
    }
  }
  return ship;
}

// --- whose a file is, and writing -------------------------------------------------------------------------------------
// made.txt: the files a generator wrote in the repo that can't carry its mark (textures), by their repo paths.
function readManifest(file) {
  if (!fs.existsSync(file)) return new Set();
  return new Set(fs.readFileSync(file, "utf8").split(/\r?\n/).map((l) => l.trim()).filter((l) => l && !l.startsWith("#")));
}
// It rewritten when its list changed: the `header` lines, then the paths sorted. Made only once it has one.
function writeManifest(file, manifest, header) {
  const body = [...header, ...[...manifest].sort()].join("\n") + "\n";
  const had = fs.existsSync(file);
  if ((had || manifest.size) && (!had || fs.readFileSync(file, "utf8").replace(/\r\n/g, "\n") !== body)) fs.writeFileSync(file, body);
}
// The rule for a file already where a write { file, bytes, kind } goes: "text" (a model, .pwk, rows file or draft) is
// ours when it carries `mark`; "baked" (a dyed part's texture) when its name without .tga is a bake's (F.BAKED: only the
// bake makes such names, and the name fixes the bytes); "shape" (a shape's soft texture, or an additive one's TXI: M1b)
// likewise when it's a soft texture's (F.SHAPE_TEXTURE: only the generator makes them); "material" (M2: a moving
// texture's material - or, on the copy route, its texture and TXI under the material's name) likewise by its n5u_ name
// (F.MOTION_TEXTURE); "copy" (DC: a library texture's copy, its picture or its TXI) likewise by its n5l_ name
// (F.COPY_TEXTURE); "shader" (vsn5uvmove.shd) when made.txt lists it (data\files' committed one) or it's the
// generators' shader, this version's or an earlier one's (F.isMotionShader: its first line); a kind in `kinds` by its own
// test (w, old); any other (a texture in the repo) when made.txt (`manifest`) lists it. -> (w, old) => true when it's ours.
function ownership({ repo, manifest, mark, kinds = {} }) {
  const listed = (file) => repoPath(repo, file) !== null && manifest.has(repoPath(repo, file));
  return (w, old) => {
    if (w.kind === "text") return F.carriesMark(old, mark);
    if (w.kind === "baked") return F.BAKED.test(path.basename(w.file).replace(/\.tga$/i, ""));
    if (w.kind === "shape") return F.SHAPE_TEXTURE.test(path.basename(w.file).replace(/\.(tga|txi)$/i, ""));
    if (w.kind === "material") return /\.(mtr|tga|dds|txi)$/i.test(w.file) && F.MOTION_TEXTURE.test(path.basename(w.file).replace(/\.(mtr|tga|dds|txi)$/i, ""));
    if (w.kind === "copy") return /\.(tga|dds|txi)$/i.test(w.file) && F.COPY_TEXTURE.test(path.basename(w.file).replace(/\.(tga|dds|txi)$/i, ""));
    if (w.kind === "shader") return listed(w.file) || F.isMotionShader(old);
    if (kinds[w.kind]) return kinds[w.kind](w, old);
    return listed(w.file);
  };
}
// Every write checked before any is written: a file there that isn't ours (`owned`, as ownership gives it) and isn't
// what would be written stops it all; then each written unless unchanged (a text - or the shader, M2: text too - the same
// but for its line ends is).
// -> [[ "new" | "rebuilt" | "unchanged", file ]], `manifest` gaining the binary files written in the repo. `dry`: the
// same checks and states, nothing written and the manifest left as it is (tools\data\build_derived.js --check).
const sameAs = (w, old) => old.equals(w.bytes) || ((w.kind === "text" || w.kind === "shader") && F.sameText(old, w.bytes));
function writeAll(writes, owned, { repo, manifest, dry = false }) {
  for (const w of writes) {
    if (!fs.existsSync(w.file)) continue;
    const old = fs.readFileSync(w.file);
    if (!owned(w, old) && !sameAs(w, old)) {
      throw new Error(`${shown(repo, w.file)} is there and wasn't made by this generator: nothing written - move it away or rename it`);
    }
  }
  const results = [];
  for (const w of writes) {
    let state = "new";
    if (fs.existsSync(w.file)) {
      const old = fs.readFileSync(w.file);
      state = sameAs(w, old) ? "unchanged" : "rebuilt";
    }
    if (state !== "unchanged" && !dry) {
      fs.mkdirSync(path.dirname(w.file), { recursive: true });
      fs.writeFileSync(w.file, w.bytes);
    }
    if (!dry && w.kind === "binary" && repoPath(repo, w.file) !== null) manifest.add(repoPath(repo, w.file));
    results.push([state, w.file]);
  }
  return results;
}

module.exports = { option, loadTools, repoPath, shown, bakeParts, bakedWrites, bakedLines, bakedJson, shapeTextures, shapeWrites, shapeLines,
  shapeJson, motionMaterials, motionPicture, motionCopies, motionWrites, motionLines, motionJson, copyTextures, copyWrites, copyLines, copyJson,
  shipTextures, readManifest, writeManifest, ownership, writeAll };
