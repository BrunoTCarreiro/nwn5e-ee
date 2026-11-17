// The Floating Disk's part and texture reading (#39), moved out of build.js so the VFX Bench's Composer reads the same
// way: a part's meshes in its model's space (a game model, a weapon's pieces, or a model file of yours), and a texture
// found in the game or beside a part's file. Node only: it runs the mesh reader (mdlmesh.exe, README.md).
//
//   const R = require("./reader.js");
//   const exe = R.ensureReader({ repo, reader });
//   const rd = R.create({ game, core, gameData, reader: exe });   // core: the bench's core.js; gameData: spell_bench\game_data.js
//   rd.part(part)  rd.findTexture(name, besideDirs)  rd.image(name, besideDirs)  rd.close()
"use strict";
const fs = require("fs");
const os = require("os");
const path = require("path");
const { execFileSync } = require("child_process");
const F = require("./floatdisk.js");

// The mesh reader: `reader` when given (it must exist), else <repo>\build\floatdisk\mdlmesh.exe, built by
// build_mdlmesh.bat (VS 2019 Build Tools, as build.bat) when it's missing or older than its sources. `log` says so.
function ensureReader({ repo, reader, log = console.log }) {
  const own = path.join(repo, "build", "floatdisk", "mdlmesh.exe");
  const need = (file, what) => {
    if (!fs.existsSync(file)) throw new Error(`${what} isn't there: ${file}`);
    return file;
  };
  if (reader && path.resolve(reader) !== path.resolve(own)) return need(path.resolve(reader), "the mesh reader (--reader / NWN5E_MDLMESH)");
  const sources = [path.join(__dirname, "mdlmesh.cpp"), path.join(repo, "src", "data", "vfx_model.cpp"), path.join(repo, "src", "data", "vfx_model.h")];
  const built = fs.existsSync(own) ? fs.statSync(own).mtimeMs : 0;
  if (built && sources.every((s) => fs.statSync(s).mtimeMs <= built)) return own;
  log(built ? "mdlmesh is older than its sources: rebuilding" : "building mdlmesh (build_mdlmesh.bat)");
  try {
    execFileSync("cmd.exe", ["/d", "/c", path.join(__dirname, "build_mdlmesh.bat")], { stdio: ["ignore", "pipe", "pipe"], windowsHide: true });
  } catch (e) {
    throw new Error("build_mdlmesh.bat failed:\n" + String((e.stdout || "") + (e.stderr || "")).trim());
  }
  return need(own, "mdlmesh after build_mdlmesh.bat");
}

function create({ game, core, gameData, reader }) {
  const res = gameData.resources(game);
  let tmp = null;
  const tmpDir = () => tmp || (tmp = fs.mkdtempSync(path.join(os.tmpdir(), "n5_floatdisk-")));

  // One model's bytes through the reader: its meshes in the model's space.
  const read = (bytes, label) => {
    const dir = tmpDir();
    const file = path.join(dir, "part" + fs.readdirSync(dir).length + ".mdl");
    fs.writeFileSync(file, bytes);
    let text;
    try {
      text = execFileSync(reader, [file], { encoding: "utf8", maxBuffer: 64 << 20, windowsHide: true });
    } catch (e) {
      throw new Error(`${label}: the reader couldn't read it - ${String(e.stderr || e.message).trim()}`);
    }
    return F.modelSpaceMeshes(core.readDraft(text.replace(/\r/g, "")), label);
  };
  const gameModel = (name, label) => {
    const bytes = res(name, 2002);
    if (!bytes) throw new Error(`${label}: no such model in the game (${game})`);
    return read(bytes, name === label ? label : `${label} (${name})`);
  };

  // A part's meshes and box: a game model, a weapon's pieces read one by one and merged, or a model file of yours (ASCII -
  // made readable for the reader first - or compiled). -> { meshes, lo, hi }
  function part(p) {
    const label = F.partLabel(p);
    if (p.source.kind === "model") return gameModel(p.source.model, label);
    if (p.source.kind === "models") return F.mergeModels(p.source.models.map((m) => gameModel(m, label)));
    if (!fs.existsSync(p.source.file)) throw new Error(`${label}: no such file (${p.source.file})`);
    let bytes = fs.readFileSync(p.source.file);
    if (bytes.length >= 4 && bytes.readUInt32LE(0) !== 0) bytes = Buffer.from(F.readableAscii(bytes.toString("latin1"), label), "latin1");
    return read(bytes, label);
  }

  // A texture by name: the game's (its archives or ovr\: TGA, DDS or PLT), else beside a part's own file - then shipped
  // beside the models. -> { where: "game" } | { where: "beside", copies: [paths] } | null
  function findTexture(name, besideDirs) {
    if (res(name, 3) || res(name, 2033) || res(name, 6)) return { where: "game" };
    for (const ext of [".tga", ".dds", ".plt"]) if (fs.existsSync(path.join(game, "ovr", name + ext))) return { where: "game" };
    for (const dir of besideDirs) {
      const found = F.texturesBeside(name, fs.existsSync(dir) ? fs.readdirSync(dir) : []);
      if (found.image) return { where: "beside", copies: [found.image, found.txi].filter(Boolean).map((f) => path.join(dir, f)) };
    }
    return null;
  }

  // A texture's pictures, for the bench: the game's (ovr\ first, then its archives: TGA 3, DDS 2033, PLT 6, TXI 2022), else
  // beside a part's file. -> { where, tga, dds, plt, txi } | null
  function image(name, besideDirs) {
    const ovr = (ext) => {
      const file = path.join(game, "ovr", name + ext);
      return fs.existsSync(file) ? fs.readFileSync(file) : null;
    };
    const tga = ovr(".tga") || res(name, 3) || null, dds = ovr(".dds") || res(name, 2033) || null;
    const plt = !!(ovr(".plt") || res(name, 6)), txi = ovr(".txi") || res(name, 2022) || null;
    if (tga || dds || plt) return { where: "game", tga, dds, plt, txi: txi ? txi.toString("latin1") : "" };
    for (const dir of besideDirs || []) {
      const found = F.texturesBeside(name, fs.existsSync(dir) ? fs.readdirSync(dir) : []);
      if (!found.image) continue;
      const bytes = fs.readFileSync(path.join(dir, found.image));
      const isDds = found.image.toLowerCase().endsWith(".dds");
      return { where: "beside", tga: isDds ? null : bytes, dds: isDds ? bytes : null, plt: false,
        txi: found.txi ? fs.readFileSync(path.join(dir, found.txi)).toString("latin1") : "" };
    }
    return null;
  }

  function close() {
    if (tmp) fs.rmSync(tmp, { recursive: true, force: true });
    tmp = null;
  }

  return { res, part, findTexture, image, close };
}

module.exports = { ensureReader, create };
