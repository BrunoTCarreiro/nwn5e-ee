// The spells' masked looks (#184; the owner, 2026-11-23: "maskproof seems good!", then "wire it, lets test it out"):
// copies of the area spells' effect models whose emitters draw through nwn5e's particle mask, built from the player's
// own game at build time into the build's data\files (build_derived.js calls derive). Never committed: they are the
// game's models.
//
// - A copy (n5k_<the model's name past its first four letters>: as long as the game's, which the copy's name fields
//   need - mdlmesh --mask, src\data\vfx_model.cpp MaskedModelCopy) is the game's model with every emitter's texture
//   (the animations' copies of the emitter too) named as a masked material instead, and the model's own name changed;
//   every other byte as it was. A chunk emitter (NULL: it throws a model) keeps its chunks, which nothing masks.
// - A material (n5x_<the texture's name, past fxpa_, lower case>.mtr): the game's vsparticle and nwn5e's fsn5pmask
//   (particle_mask.js) - the game's own particle shader with the mask - and the game's texture by name (texture0). It
//   draws as the game's emitter does while no mask is set (scriptableVec1.w 0), so a copy looks exactly as the game's.
// - Which models: MODELS - the effects the spells' looks name (src/rules/spell_masks.h lists the spells): the cones
//   (spells.2da CastHandVisual), the bursts (visualeffects.2da: an FNF or impact effect at the spell's point), the
//   lasting areas (vfx_persistent.2da MODEL01-03, MODELMIN01-03; Web's by its DurationVFX effect) and each one's
//   low-quality model. data\rows' *.if_spellmask.txt rows name the copies: served with spell_mask=1 in faithful
//   (core/spell_mask.cpp checks each copy they name is in data\files first).
// - A model the game hasn't, or one that can't be copied cleanly (ASCII, or its name not where a model keeps it): skipped
//   and said - the rows naming it then keep the game's look (spell_mask.cpp serves none of the rows when a copy is
//   missing, and says which).
//
//   const S = require("./spell_mask.js");
//   S.derive({ res, reader })  -> { files: [{ name, bytes }], copies: [{ model, name, emitters, masked }], skipped: [{ model, why }] }
"use strict";
const fs = require("fs");
const os = require("os");
const path = require("path");
const { execFileSync } = require("child_process");

const MODELS = [
  // the cones: Burning Hands, Color Spray, Cone of Cold, Prismatic Spray, Dragon's Breath's acid and poison exhales
  "var_conefire", "var_conespar", "var_conecold", "var_conepris", "vca_coneacid01", "var_conepois",
  // the bursts: Fireball, Thunderwave and Shatter's sound burst, Ice Knife's frost, Ice Storm, Fire Storm, Meteor
  // Swarm, Sunburst, Sunbeam, Flame Strike
  "vff_explfire", "vff_explfire_L", "vff_explsonic", "vff_explsonic_L", "vim_exp2frost", "vff_rainice", "vff_rainice_L",
  "vff_rainfire", "vff_rainfire_L", "vff_rainmeteor", "vff_rainmeteor_L", "vff_expdholy30", "vff_explsun",
  "vff_explsun_L", "vim_strkfire",
  // the lasting areas: Fog Cloud (NWN's Mind Fog cloud), Entangle, Grease, Cloudkill, Stinking Cloud, Incendiary Cloud,
  // Web
  "vps_fogmind", "vps_fogmind_L", "vps_entangle", "vps_entangle_L", "vps_grease", "vps_grease_L", "vps_fogkill",
  "vps_fogkill_L", "vps_fogstink", "vps_fogstink_L", "vps_fogfire", "vps_fogfire_L", "vdr_webmass",
];
const MDL = 2002;
const SHADER = "fsn5pmask";  // particle_mask.js's NAME
const lower = (s) => String(s).toLowerCase();

// The copy's name: n5k_ and the model's past its first four letters (vff_explfire -> n5k_explfire) - as long as the
// game's. "" when the model's name has no three-letter prefix and underscore.
const copyName = (model) => (/^[a-z0-9]{3}_./i.test(model) ? "n5k_" + lower(model).slice(4) : "");
// A texture's material: n5x_ and its name past fxpa_ (fxpa_Smoke01a -> n5x_smoke01a). At most 16 characters.
const materialName = (texture) => "n5x_" + lower(texture).replace(/^fxpa_/, "");
const noTexture = (t) => !t || t === "-" || lower(t) === "null";
// The material's text: the game's particle vertex shader, nwn5e's masked fragment shader, the game's texture.
const materialText = (texture) => `customshadervs vsparticle\r\ncustomshaderfs ${SHADER}\r\ntexture0 ${lower(texture)}\r\n`;

// mdlmesh --emitters over files -> one list a file: [{ node, update, texture }], or { error }.
function emitters(reader, files) {
  const text = execFileSync(reader, ["--emitters", ...files], { encoding: "latin1", maxBuffer: 64 << 20, windowsHide: true });
  const byFile = new Map(files.map((f) => [f, { list: [], form: "" }]));
  let cur = null;
  for (const line of text.split(/\r?\n/)) {
    let m;
    if ((m = line.match(/^# model (.+) (compiled|ascii)$/))) {
      cur = byFile.get(m[1]);
      if (cur) cur.form = m[2];
    } else if ((m = line.match(/^# model (.+) unreadable (.*)$/))) {
      cur = byFile.get(m[1]);
      if (cur) cur.error = m[2] || "unreadable";
      cur = null;
    } else if (cur && (m = line.match(/^texture (\S+) (\S+) (\S+)$/))) {
      cur.list.push({ node: m[1], update: m[2], texture: m[3] });
    }
  }
  return files.map((f) => byFile.get(f));
}

// The copies of `models` (MODELS) from the game's resources (`res`: spell_bench\game_data.js resources(game)) through
// mdlmesh (`reader`), each read back and checked: its emitters the game's, in order, those with a texture naming that
// texture's material, the rest as they were. A model the game hasn't, or one that can't be copied cleanly, is skipped
// and said; a copy that reads back wrong stops it (throws).
function derive({ models = MODELS, res, reader }) {
  const files = [], copies = [], skipped = [];
  const materials = new Map();  // material name -> texture (lower case)
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "n5_spellmask-"));
  try {
    const games = [];
    for (const model of models) {
      const name = copyName(model);
      if (!name) {
        skipped.push({ model, why: "its name has no three-letter prefix to take the copy's in place of" });
        continue;
      }
      const bytes = res(model, MDL);
      if (!bytes) {
        skipped.push({ model, why: "not in the game" });
        continue;
      }
      const file = path.join(dir, model + ".game.mdl");
      fs.writeFileSync(file, bytes);
      games.push({ model, name, bytes, file });
    }
    const read = games.length ? emitters(reader, games.map((g) => g.file)) : [];
    games.forEach((g, i) => {
      const r = read[i];
      if (!r || r.error) return skipped.push({ model: g.model, why: "mdlmesh can't read it (" + (r ? r.error : "nothing said") + ")" });
      if (r.form !== "compiled") return skipped.push({ model: g.model, why: "an ASCII model (only compiled ones are copied)" });
      const pairs = new Map();  // texture -> material
      for (const e of r.list) {
        if (noTexture(e.texture)) continue;
        const texture = lower(e.texture), material = materialName(texture);
        if (material.length > 16) return skipped.push({ model: g.model, why: `its texture ${e.texture}'s material ${material} is past 16 characters` });
        const had = materials.get(material);
        if (had && had !== texture) return skipped.push({ model: g.model, why: `${material} would name both ${had} and ${texture}` });
        pairs.set(texture, material);
      }
      // (No emitter with a texture - Ice Storm's low-quality model throws only chunks: copied under its name all the same,
      // nothing in it masked, so the rows naming the copies stay whole.)
      const out = path.join(dir, g.name + ".mdl");
      let said;
      try {
        said = execFileSync(reader, ["--mask", g.file, out, g.name, ...[...pairs].map(([t, m]) => `${t}=${m}`)],
          { encoding: "latin1", windowsHide: true, stdio: ["ignore", "pipe", "pipe"] });
      } catch (e) {
        return skipped.push({ model: g.model, why: "not copied cleanly - " + String(e.stderr || e.message).trim().replace(/^.*?not copied - /, "") });
      }
      const masked = Number((said.match(/^masked (\d+)/m) || [])[1] || 0);
      for (const [t, m] of pairs) materials.set(m, t);
      copies.push({ model: g.model, name: g.name, file: out, emitters: r.list, pairs, masked });
    });
    // Each copy read back beside the game's: the same emitters in order, each texture with a material now naming it.
    const back = copies.length ? emitters(reader, copies.map((c) => c.file)) : [];
    copies.forEach((c, i) => {
      const r = back[i];
      const fail = (what) => {
        throw new Error(`spell mask: ${c.model}: ${what} - nothing written`);
      };
      if (!r || r.error || r.form !== "compiled") fail("the copy can't be read back");
      if (r.list.length !== c.emitters.length) fail(`the copy has ${r.list.length} emitter headers, the game's ${c.emitters.length}`);
      let named = 0;
      r.list.forEach((e, k) => {
        const g = c.emitters[k];
        if (e.node !== g.node || e.update !== g.update) fail(`the copy's emitter ${k} isn't the game's ${g.node}`);
        const want = noTexture(g.texture) ? g.texture : c.pairs.get(lower(g.texture));
        if (e.texture !== want) fail(`the copy's ${g.node} names ${e.texture}, not ${want}`);
        if (!noTexture(g.texture)) named++;
      });
      if (named !== c.masked) fail(`mdlmesh changed ${c.masked} emitter headers, ${named} name a texture`);
      files.push({ name: c.name + ".mdl", bytes: fs.readFileSync(c.file) });
    });
    for (const [material, texture] of [...materials].sort()) {
      files.push({ name: material + ".mtr", bytes: Buffer.from(materialText(texture), "latin1") });
    }
    return { files, copies: copies.map((c) => ({ model: c.model, name: c.name, emitters: c.emitters.length, masked: c.masked })), skipped };
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
}

// Whether a built file is one of this generator's (build_derived.js's ownership in --out): a copy of MODELS' or a
// material by its name.
function owns(name) {
  const n = lower(name);
  if (/^n5x_[a-z0-9_]+\.mtr$/.test(n)) return true;
  return n.endsWith(".mdl") && MODELS.some((m) => copyName(m) + ".mdl" === n);
}

// --- The rows that name the copies (data\rows\*.mask.if_spellmask.txt; served with spell_mask=1 in faithful) ----------
// - spells.2da: each cone's CastHandVisual named as its copy (nwn5e's Burning Hands, Color Spray and Dragon's Breath
//   exhales; NWN's Cone of Cold and Prismatic Spray).
// - visualeffects.2da: nwn5e's rows 4200-4209, each a copy of the game's row (EFFECT_ROWS) with its models named as
//   their copies - core/spell_mask.cpp hands one to the spell's script in place of the game's.
// - vfx_persistent.2da: nwn5e's own areas (105-115: Entangle, Grease, Fog Cloud) with their model columns alone named as
//   the copies (the rest stays vfx_persistent.if_spells2024.txt's); rows 120-123 (their own file), copies of NWN's clouds and web (AREA_ROWS) - the same label (the area's tag, which scripts look for),
//   its models (or its DurationVFX effect, Web's) the copies.
const CAST_ROWS = [10, 24, 25, 135, 1269, 1270, 1271, 1273];
const EFFECT_ROWS = [[4200, 22], [4201, 183], [4202, 62], [4203, 231], [4204, 23], [4205, 28], [4206, 190], [4207, 37],
  [4208, 54], [4209, 230]];
const AREA_ROWS = [[120, 1], [121, 2], [122, 3], [123, 8]];
const OWN_AREAS = [105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115];
const EFFECT_MODELS = (c) => /_Node$/.test(c) || c === "LowQuality";
const AREA_MODELS = (c) => /^MODEL(MIN)?0[1-3]$/.test(c);

const FILES = {
  spells: "spells.mask.if_spellmask.txt",
  visualeffects: "visualeffects.mask.if_spellmask.txt",
  vfx_persistent: "vfx_persistent.mask.if_spellmask.txt",
  vfx_persistent_copies: "vfx_persistent.maskcopies.if_spellmask.txt",
};
const HEADERS = {
  spells: "# spells.2da - #184, the spells' masked looks (tools/data/spell_mask.js writes these rows: change it, not them):\n" +
    "# each cone's cast effect named as its masked copy (n5k_*, built from your game at build time). Served with\n" +
    "# spell_mask=1 in faithful (core/spell_mask.cpp), after the spells' own rows (this file sorts after theirs).\n",
  visualeffects: "# visualeffects.2da - #184, the spells' masked looks (tools/data/spell_mask.js writes these rows: change it, not\n" +
    "# them): each a copy of the game's row named in its comment, its models named as their masked copies (n5k_*).\n" +
    "# core/spell_mask.cpp hands a spell's script the copy in place of the game's row. Served with spell_mask=1.\n",
  vfx_persistent: "# vfx_persistent.2da - #184, the spells' masked looks (tools/data/spell_mask.js writes these rows: change it, not\n" +
    "# them): nwn5e's own areas' models (105-115) named as their masked copies (n5k_*), nothing else of theirs. Served\n" +
    "# with spell_mask=1 (after vfx_persistent.if_spells2024.txt: this file sorts after it).\n",
  vfx_persistent_copies: "# vfx_persistent.2da - #184, the spells' masked looks (tools/data/spell_mask.js writes these rows: change it,\n" +
    "# not them): copies of the game's rows named in their comments - the label kept (it is the area's tag, which scripts\n" +
    "# look for), their models (Web's: its DurationVFX effect) the masked copies. Served with spell_mask=1.\n",
};

const cell = (v) => (v === undefined || v === "" ? "****" : /\s/.test(v) ? `"${v}"` : String(v));
const masked = (v) => {
  const n = lower(v);
  return MODELS.some((m) => lower(m) === n) ? copyName(n) : v;
};

// The three files' text from the served tables (`table(name)` -> { columns, rows: Map(row -> values) }: the game's
// with nwn5e's rows, not these). -> FILES' keys -> its text, CRLF.
function rowsText(table) {
  const out = {};
  const spells = table("spells");
  out.spells = HEADERS.spells + "CastHandVisual\n" + CAST_ROWS.map((r) => {
    const v = (spells.rows.get(r) || {}).CastHandVisual;
    return `${r} ${cell(masked(v))}`;
  }).join("\n") + "\n";
  const vfx = table("visualeffects");
  const effectCols = vfx.columns;
  const effectOf = new Map();
  out.visualeffects = HEADERS.visualeffects + EFFECT_ROWS.map(([row, from]) => `# ${row} from ${from}`).join("\n") + "\n" +
    effectCols.join(" ") + "\n" + EFFECT_ROWS.map(([row, from]) => {
      effectOf.set(from, row);
      const g = vfx.rows.get(from) || {};
      return row + " " + effectCols.map((c) => c === "Label" ? "NWN5E_MASK_" + String(g.Label || from).replace(/^VFX_/, "")
        : EFFECT_MODELS(c) ? cell(masked(g[c])) : cell(g[c])).join(" ");
    }).join("\n") + "\n";
  const per = table("vfx_persistent");
  const perCols = per.columns;
  const modelCols = perCols.filter(AREA_MODELS);
  out.vfx_persistent = HEADERS.vfx_persistent + modelCols.join(" ") + "\n" +
    OWN_AREAS.map((r) => r + " " + modelCols.map((c) => cell(masked((per.rows.get(r) || {})[c]))).join(" ")).join("\n") + "\n";
  const copyLine = (row, g) => row + " " + perCols.map((c) => AREA_MODELS(c) ? cell(masked(g[c]))
    : c === "DurationVFX" && effectOf.has(Number(g[c])) ? String(effectOf.get(Number(g[c]))) : cell(g[c])).join(" ");
  out.vfx_persistent_copies = HEADERS.vfx_persistent_copies + AREA_ROWS.map(([row, from]) => `# ${row} from ${from}`).join("\n") +
    "\n" + perCols.join(" ") + "\n" + AREA_ROWS.map(([row, from]) => copyLine(row, per.rows.get(from) || {})).join("\n") + "\n";
  for (const k of Object.keys(out)) out[k] = out[k].split("\n").join("\r\n");
  return out;
}

// Every copy a rows text names (n5k_...).
const namedCopies = (text) => [...new Set([...String(text).matchAll(/\bn5k_[a-z0-9_]+/gi)].map((m) => lower(m[0])))].sort();

module.exports = { MODELS, SHADER, copyName, materialName, materialText, emitters, derive, owns, CAST_ROWS, EFFECT_ROWS,
  AREA_ROWS, OWN_AREAS, FILES, rowsText, namedCopies };

// node tools\data\spell_mask.js --rows [--game <folder>] [--tools <nwn5e-tools>]: writes the three rows files into
// data\rows from the game's tables and nwn5e's other rows (servedTable without the looks, the mask's own files left out).
if (require.main === module) {
  const argv = process.argv.slice(2);
  const option = (name, fallback) => {
    const i = argv.indexOf("--" + name);
    return i >= 0 && i + 1 < argv.length ? argv[i + 1] : fallback;
  };
  if (!argv.includes("--rows")) {
    console.error("spell_mask: --rows [--game <folder>] [--tools <nwn5e-tools>] - writes data\\rows' *.mask.if_spellmask.txt");
    process.exit(2);
  }
  const repo = path.resolve(__dirname, "..", "..");
  const tools = path.resolve(option("tools", process.env.NWN5E_TOOLS || path.join(repo, "..", "nwn5e-tools")));
  const game = require(path.join(tools, "anim_helper", "helper.js")).findGameDir({ game: option("game", ""), buildDir: path.join(repo, "build") });
  const G = require(path.join(tools, "spell_bench", "game_data.js"));
  const rowsDir = path.join(repo, "data", "rows");
  const table = (name) => {
    const t = G.gameTable(game, name);
    for (const f of fs.readdirSync(rowsDir).sort()) {
      if (f.split(".")[0].toLowerCase() !== name || /\.if_spellmask\./i.test(f) || !f.endsWith(".txt")) continue;
      G.applyRows(t, fs.readFileSync(path.join(rowsDir, f), "latin1"));
    }
    return t;
  };
  const text = rowsText(table);
  for (const [k, file] of Object.entries(FILES)) {
    fs.writeFileSync(path.join(rowsDir, file), text[k], "latin1");
    console.log(`spell_mask: written data/rows/${file} (${namedCopies(text[k]).length} copies named)`);
  }
}
