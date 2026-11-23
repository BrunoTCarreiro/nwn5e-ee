#!/usr/bin/env node
// #39's Spiritual Weapon look's check (the owner, 2026-11-22: "A, make the draft look"):
//   node tools\data\spiritweapon\check.js [--tools <nwn5e-tools>] [--reader <mdlmesh.exe>] [--from-drafts]
//
// Builds the effects twice in memory (build.js's build(), through the VFX Bench's core.js) and checks, writing nothing:
// - the three effects: n5_swp_idle (visualeffects 4180, lasting), n5_swp_fly (spells.2da 1279's projectile - no
//   visualeffects row; lasting: its emitters on in its geometry, the blade 1.2 m over its root) and n5_swp_hit (4182, a
//   one-shot), names under 17 letters starting n5_, on the root; the same drafts both builds;
//   data\rows\visualeffects.spiritweapon.txt names 4180 and 4182's in the four root columns, flat;
// - the blade: about 0.9 m from its pommel to its point (the point on its +y axis - the way a projectile flies, the
//   game's own ammunition's), 11 cm wide at the guard, a crossguard 28 cm across; every face on its vertices, every edge
//   shared by two faces (closed: seen from every side), its UVs within the texture;
// - the idle: one loop of 16 s, its last key its first (seamless), a bob of 5 cm either way with 6 rises a loop, a sway of
//   at most 5 degrees, one whole turn about the blade a loop (half at 8 s); the blade hanging point down round 1.2 m; it
//   fades in over its start (0.5 s) and out over its end (0.25 s);
// - the strike: from its pivot 1.3 m up in front of the model (+y), 72 degrees in 0.45 s, past the middle of its arc at
//   0.15 s (when the game session's flash lands), its tip 0.915 m out: an arc of about 1.15 m, ending in front (+y) and
//   lower than it began; its impact starting on its keys (its blade's fade at 0), the blade gone by 0.8 s;
// - the textures: the generator's bytes (the blade's TXI "blending additive"), 32-bit TGAs;
// - tools\data\vfx holds the drafts the recipe makes (--from-drafts: the owner's, as they are), and the bench reads each
//   back byte for byte; data\files holds the models the game's writer makes from them (mdlmesh --model);
// - on the bench's stage (its own sim.js): each one plays its particles - the idle and the flight going on, the strike
//   over by 1.2 s - every one added light (Lighten), few at once (the idle under 40, the flight under 40, the strike
//   under 150), the whole effect within the game's typical budget (sim.js particleBudget).
// Exit 0: all pass; 1: a failure (each printed).
"use strict";
const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "..", "..", "..");
const FILES = path.join(ROOT, "data", "files");
const DRAFTS = path.join(ROOT, "tools", "data", "vfx");
const ROWS = path.join(ROOT, "data", "rows", "visualeffects.spiritweapon.txt");
let failures = 0;
const fail = (what) => { ++failures; console.log("FAIL " + what); };
const ok = (what) => console.log("ok   " + what);
const check = (what, pass, detail) => (pass ? ok(what) : fail(what + (detail ? "  " + detail : "")));
const lf = (file) => fs.readFileSync(file, "utf8").replace(/\r/g, "");
const near = (a, b, tol = 1e-4) => Math.abs(a - b) <= tol;

let B, T;
try {
  B = require("./build.js");
  T = require("../debris/build.js").tools(process.argv.slice(2));
} catch (e) {
  fail("build.js and nwn5e-tools' core.js load (--tools): " + e.message);
  console.log("\n" + failures + " failed");
  process.exit(1);
}
const core = T.core;
const fromDrafts = process.argv.includes("--from-drafts");
const S = require(path.join(T.dir, "vfx_bench", "public", "sim.js"));

// --- the effects as built ----------------------------------------------------------------------------------------------
const one = B.build(core), two = B.build(core);
check("three effects - rows 4180 and 4182, the flight spells.2da 1279's projectile - names n5_ under 17 letters, on the root",
  one.map((e) => `${e.row} ${e.name}`).join(",") === "4180 n5_swp_idle,null n5_swp_fly,4182 n5_swp_hit" && B.FLY_PROJECTILE_ROW === 1279 &&
  one.every((e) => /^n5_[a-z0-9_]{1,13}$/.test(e.name) && e.fx.attach === "root"));
check("the same drafts on both builds", one.every((e, i) => e.draft === two[i].draft));
const byName = Object.fromEntries(one.map((e) => [e.name, e]));
check("the idle and the flight lasting (their emitters on in the geometry), the strike a one-shot",
  byName.n5_swp_idle.fx.duration && byName.n5_swp_fly.fx.duration && !byName.n5_swp_hit.fx.duration);

// The rows file.
const rows = lf(ROWS).split("\n").map((l) => l.trim().split(/\s+/)).filter((w) => w[0] && w[0][0] !== "#");
const header = rows[0] || [];
const body = rows.slice(1);
const shown = [B.IDLE, B.HIT];
check("the rows file: 4180 (D) and 4182 (F) naming their models in the four root columns, flat; no 4181",
  header.join(" ") === "Label Type_FD OrientWithGround Imp_Root_S_Node Imp_Root_M_Node Imp_Root_L_Node Imp_Root_H_Node" &&
  body.length === 2 && body.every((w, i) => w[0] === String(B.ROWS[shown[i]]) && w[1] === B.LABELS[shown[i]] && w[2] === ["D", "F"][i] &&
    w[3] === "0" && w.slice(4).length === 4 && w.slice(4).every((m) => m === shown[i])), JSON.stringify(body));

// --- the blade -------------------------------------------------------------------------------------------------------
const m = B.bladeMesh(), n = m.verts.length / 3;
const v = (i) => [m.verts[3 * i], m.verts[3 * i + 1], m.verts[3 * i + 2]];
const span = (k) => { let lo = Infinity, hi = -Infinity; for (let i = 0; i < n; i++) { lo = Math.min(lo, v(i)[k]); hi = Math.max(hi, v(i)[k]); } return [lo, hi]; };
const [ylo, yhi] = span(1), [xlo, xhi] = span(0);
let tip = 0;
for (let i = 1; i < n; i++) if (v(i)[1] > v(tip)[1]) tip = i;
const guard = [...Array(n).keys()].filter((i) => near(v(i)[1] + B.BLADE_MID, 0, 1e-6)).map((i) => Math.abs(v(i)[0]));
check("the blade: 0.915 m pommel to point, the point on +y, 11 cm wide at the guard, a crossguard 28 cm across",
  near(yhi - ylo, 0.915, 1e-4) && near(v(tip)[0], 0) && near(v(tip)[2], 0) && near(Math.max(...guard), 0.055, 1e-6) &&
  near(xhi - xlo, 0.28, 1e-6), JSON.stringify([yhi - ylo, v(tip), Math.max(...guard), xhi - xlo]));
const edges = new Map();
let onVerts = m.faces.length % 3 === 0;
for (let f = 0; f + 2 < m.faces.length; f += 3) {
  const t = [m.faces[f], m.faces[f + 1], m.faces[f + 2]];
  onVerts = onVerts && t.every((i) => Number.isInteger(i) && i >= 0 && i < n) && new Set(t).size === 3;
  for (let k = 0; k < 3; k++) {
    const a = t[k], b = t[(k + 1) % 3], key = Math.min(a, b) + ":" + Math.max(a, b);
    edges.set(key, (edges.get(key) || 0) + 1);
  }
}
check("the blade: every face on its vertices, every edge shared by two faces (closed), its UVs within the texture",
  onVerts && [...edges.values()].every((c) => c === 2) && m.tverts.length === 2 * n && m.tverts.every((x) => x >= 0 && x <= 1),
  JSON.stringify([onVerts, [...edges.values()].filter((c) => c !== 2).length]));

// --- the idle ----------------------------------------------------------------------------------------------------------
const quatAngle = (q) => 2 * Math.acos(Math.min(1, Math.abs(q[3])));
const idle = byName.n5_swp_idle.fx, hover = idle.nodes[0], loop = hover.keys.find((k) => k.anim === "duration");
const P = loop.position, O = loop.orientation, last = P.length - 4, lastO = O.length - 5;
check("the idle: one loop of 16 s keyed every 0.25 s, its last key its first",
  idle.lengths[1] === 16 && P.length === 65 * 4 && O.length === 65 * 5 && P.slice(1, 4).join() === P.slice(last + 1).join() &&
  O.slice(1, 5).join() === O.slice(lastO + 1).join() && near(P[last], 16));
const zs = [];
for (let i = 0; i < P.length; i += 4) zs.push(P[i + 3]);
let rises = 0;
for (let i = 1; i < zs.length; i++) if (zs[i - 1] < B.HOVER_Z && zs[i] >= B.HOVER_Z) rises++;
check("the idle: a bob of 5 cm either way, 6 rises a loop", near(Math.max(...zs) - B.HOVER_Z, 0.05, 0.002) && near(B.HOVER_Z - Math.min(...zs), 0.05, 0.002) &&
  rises === 6, JSON.stringify([Math.max(...zs), Math.min(...zs), rises]));
// Its sway (the tilt of its long axis - z - from upright) at most 5 degrees; its turn about the blade half a turn at 8 s.
let mostTilt = 0;
for (let t = 0; t <= 16; t += 0.05) {
  const q = B.hoverAt(t).q, z = S.qrot(q, [0, 0, 1]);
  mostTilt = Math.max(mostTilt, Math.acos(Math.min(1, z[2])) * 180 / Math.PI);
}
check("the idle: a sway of at most 5 degrees, half a turn about the blade at 8 s (a whole one a loop)",
  mostTilt > 2 && mostTilt <= 5 && near(quatAngle(B.hoverAt(8).q), Math.PI, 0.02), JSON.stringify([mostTilt, quatAngle(B.hoverAt(8).q)]));
const blade = idle.planes[0], down = S.qrot(S.quat(blade.orientation), [0, 1, 0]);
const fade = (anim) => (blade.keys.find((k) => k.anim === anim) || {}).alpha || [];
check("the idle: the blade hangs point down round 1.2 m, fading in over its start (0.5 s) and out over its end (0.25 s)",
  near(down[2], -1, 1e-5) && blade.parent === "hover" && hover.position[2] === 1.2 && fade("impact").join() === "0,0,0.5,1" &&
  fade("cessation").join() === "0,1,0.25,0" && idle.lengths[0] === 0.5 && idle.lengths[2] === 0.25);
// The flight: the blade 1.2 m over the model's root (cast from a marker at ground level), its emitters with it - all
// hanging from its node.
const fly = byName.n5_swp_fly.fx;
check("the flight: the blade point first along +y, 1.2 m over its root on its node, its emitters with it",
  fly.nodes.length === 1 && fly.nodes[0].name === "flight" && fly.nodes[0].position.join() === "0,0,1.2" && fly.nodes[0].orientation.join() === "0,0,0,0" &&
  fly.planes[0].parent === "flight" && fly.planes[0].position.join() === "0,0,0" && fly.planes[0].orientation.join() === "0,0,0,0" &&
  fly.emitters.every((e) => e.parent === "flight" && e.position.join() === "0,0,0"));
// Every blade within the owner's reach in the VFX Bench (the owner, 2026-11-23: "I can't seem to affect the mesh in the
// last one, only the glow and streak"): the bench gives a draft's plane (no `mesh`) no row of its own, so each one hangs
// under a node of its draft - its row moves, turns and keys it - and every emitter has its own row (a beat).
check("in the bench: every blade under a node of its draft (its row moves it), every emitter a beat of its own",
  one.every((e) => e.fx.planes.length > 0 && e.fx.planes.every((p) => !p.mesh && e.fx.nodes.some((n) => n.name === p.parent)) &&
    e.fx.emitters.every((x) => x.name && e.fx.emitters.filter((y) => y.name === x.name).length === 1)),
  JSON.stringify(one.map((e) => e.fx.planes.map((p) => [e.name, p.parent]))));

// --- the strike --------------------------------------------------------------------------------------------------------
const hit = byName.n5_swp_hit.fx, swingKeys = hit.nodes[0].keys[0].orientation;
const from = S.qrot(B.swingAt(0), [0, 1, 0]), to = S.qrot(B.swingAt(B.SWING_TIME), [0, 1, 0]);
const sweep = Math.acos(Math.max(-1, Math.min(1, from[0] * to[0] + from[1] * to[1] + from[2] * to[2])));
const reach = B.BLADE_LENGTH - B.BLADE_LO;
check("the strike: 72 degrees in 0.45 s, its tip 0.915 m out - an arc of about 1.15 m - in front, ending lower",
  near(sweep * 180 / Math.PI, 72, 1.5) && near(reach, 0.915, 1e-4) && near(sweep * reach, 1.15, 0.03) && from[1] > 0.5 && to[1] > 0.5 &&
  to[2] < from[2] && swingKeys.length === 55 && near(swingKeys[50], 0.45) && B.PIVOT[2] === 1.3,
  JSON.stringify([sweep * 180 / Math.PI, sweep * reach, from, to]));
// Past the middle of its arc at 0.15 s (the game session's flash or sparks land then).
const mid = S.qrot(B.swingAt(B.SLASH_AT), [0, 1, 0]), fromMid = Math.acos(Math.max(-1, Math.min(1, from[0] * mid[0] + from[1] * mid[1] + from[2] * mid[2])));
check("the strike: the middle of its arc at 0.15 s", B.SLASH_AT === 0.15 && near(fromMid, sweep / 2, 0.01), JSON.stringify([fromMid, sweep / 2]));
// The edge leads: where the tip goes first is across the blade (its x), not along its flat (z).
const lead = S.qrot(B.swingAt(0), [1, 0, 0]), dTip = [to[0] - from[0], to[1] - from[1], to[2] - from[2]];
check("the strike: its edge first", lead[0] * dTip[0] + lead[1] * dTip[1] + lead[2] * dTip[2] > 0.5 * Math.hypot(...dTip));
const hitFade = (hit.planes[0].keys.find((k) => k.anim === "impact") || {}).alpha || [];
check("the strike: its blade from its pommel at the pivot, fading from 0 (the impact on its keys) to 0 by 0.8 s",
  hit.planes[0].parent === "swing" && near(hit.planes[0].position[1], B.BLADE_MID - B.BLADE_LO) && hitFade[1] === 0 && hitFade[hitFade.length - 2] === 0.8 &&
  hitFade[hitFade.length - 1] === 0);

// --- the textures --------------------------------------------------------------------------------------------------------
for (const t of B.textures()) {
  const file = path.join(FILES, t.name + ".tga"), txi = path.join(FILES, t.name + ".txi");
  check(`${t.name}: the generator's 32-bit TGA` + (t.txi ? `, its TXI "${t.txi.trim().split("\n")[0]}"` : ", no TXI"),
    fs.existsSync(file) && fs.readFileSync(file).equals(t.bytes) && t.bytes[16] === 32 &&
    (t.txi ? fs.existsSync(txi) && lf(txi) === t.txi : !fs.existsSync(txi)));
}

// --- the committed drafts and models -------------------------------------------------------------------------------------
for (const e of one) {
  const file = path.join(DRAFTS, e.name + ".vfx");
  const text = fs.existsSync(file) ? lf(file) : "";
  check(`${e.name}.vfx: ${fromDrafts ? "the owner's, read back byte for byte" : "the recipe's"}`,
    text && core.writeDraft(core.readDraft(text)) === text && (fromDrafts || text === e.draft));
}
let reader = null;
try {
  const R = require("../floatdisk/reader.js");
  reader = R.ensureReader({ repo: ROOT, reader: T.option("reader", process.env.NWN5E_MDLMESH || ""), log: () => {} });
} catch (e) {
  fail("the mesh reader (mdlmesh) for the models: " + e.message);
}
if (reader) {
  for (const e of one) {
    const draft = path.join(DRAFTS, e.name + ".vfx"), model = path.join(FILES, e.name + ".mdl");
    check(`${e.name}.mdl: the game's writer's from its draft`, fs.existsSync(model) && lf(model) === require("../debris/build.js").modelOf(reader, draft, e.name));
  }
}
const extra = fs.readdirSync(FILES).filter((f) => /^n5_swp_/i.test(f) && !/^n5_swp_(idle|fly|hit)\.mdl$|^n5_swp_(blade\.(tga|txi)|glow\.tga)$/i.test(f));
check("data\\files: no other n5_swp_ file", extra.length === 0, extra.join(", "));

// --- on the bench's stage ----------------------------------------------------------------------------------------------
const played = (fx, seconds) => {
  const sim = new S.Sim();
  sim.sections = !!fx.duration;
  sim.setEffect(core.readDraft(core.writeDraft(fx)));
  let most = 0, lighten = true;
  for (let t = 0; t < seconds - 1e-9; t += 1 / 60) {
    sim.step(1 / 60);
    const shot = sim.snapshot();
    most = Math.max(most, shot.reduce((k, s) => k + s.particles.length, 0));
    lighten = lighten && shot.every((s) => !s.particles.length || s.emitter.blend === "Lighten");
  }
  return { sim, most, lighten, alive: sim.snapshot().reduce((k, s) => k + s.particles.length, 0) };
};
const idlePlay = played(byName.n5_swp_idle.fx, 6), flyPlay = played(byName.n5_swp_fly.fx, 2), hitPlay = played(byName.n5_swp_hit.fx, 1.2);
check("on the stage: the idle going on, under 40 at once, all added light", idlePlay.alive > 10 && idlePlay.most < 40 && idlePlay.lighten,
  JSON.stringify([idlePlay.alive, idlePlay.most]));
check("on the stage: the flight going on, under 40 at once, all added light", flyPlay.alive > 10 && flyPlay.most < 40 && flyPlay.lighten,
  JSON.stringify([flyPlay.alive, flyPlay.most]));
check("on the stage: the strike's arc under 150 at once, all added light, over by 1.2 s", hitPlay.most > 40 && hitPlay.most < 150 && hitPlay.lighten &&
  hitPlay.alive === 0, JSON.stringify([hitPlay.most, hitPlay.alive]));
check("within the game's typical budget", one.every((e) => S.particleBudget(e.fx).total <= S.BUDGET.typical),
  JSON.stringify(one.map((e) => S.particleBudget(e.fx))));

// --- the looks (the owner, 2026-11-23: "for these effects can I pick an existing weapon mesh instead of using a custom one?",
// then "set it up and make 1 handed swords the first weapon, you can pick one at random") ----------------------------------
// A weapon's spell mesh stands it upright, point up, its lowest point on the guide (the Composer): a test one here, made of
// nothing of the game's - a stick 0.9 m tall from the ground, its point a single vertex at the top, two meshes (a grip and a
// blade, one textured n5l_0123456789ab).
const stickDraft = core.writeDraft(core.effect({ name: "n5m_test", planes: [
  core.plane({ name: "grip", bitmap: "null", verts: [-0.02, -0.02, 0, 0.02, -0.02, 0, 0, 0.02, 0, 0, 0, 0.2], faces: [0, 1, 3, 1, 2, 3, 2, 0, 3, 0, 2, 1], tverts: [0, 0, 1, 0, 0.5, 1, 0.5, 0.5] }),
  core.plane({ name: "blade", bitmap: "n5l_0123456789ab", selfillum: [0.72, 0.86, 1], verts: [-0.04, -0.01, 0.2, 0.04, -0.01, 0.2, 0, 0.01, 0.2, 0, 0, 0.9],
    faces: [0, 1, 3, 1, 2, 3, 2, 0, 3, 0, 2, 1], tverts: [0, 0, 1, 0, 0.5, 1, 0.5, 0.5] })] }));
const weapon = B.weaponMeshes(core, stickDraft, "n5m_test");
const along = (ms) => { let lo = Infinity, hi = -Infinity; for (const m of ms) for (let i = 1; i < m.verts.length; i += 3) { lo = Math.min(lo, m.verts[i]); hi = Math.max(hi, m.verts[i]); } return [lo, hi]; };
const [wlo, whi] = along(weapon), tipAt = weapon[1].verts.slice(9, 12);
check("looks: a spell mesh's weapon placed as the blade is - upright (z) turned to +y, its point at +y, its middle at 0",
  weapon.length === 2 && near(wlo, -0.45) && near(whi, 0.45) && near(tipAt[0], 0) && near(tipAt[1], 0.45) && near(tipAt[2], 0) &&
  weapon[1].bitmap === "n5l_0123456789ab" && [0.72, 0.86, 1].every((c, k) => near(weapon[1].selfillum[k], c, 1e-6)), JSON.stringify([wlo, whi, tipAt, weapon[1].bitmap, weapon[1].selfillum]));
// Each effect with it swapped in: the blade's planes replaced, under the blade's parent at its place and turn with its keys
// (its fades); every node, emitter and key as it was; the slash's middle still at 0.15 s; the flight's weapon along +y,
// 1.2 m up.
for (const e of one) {
  const fx = core.readDraft(e.draft), swapped = B.withWeapon(core, fx, weapon), blade = fx.planes.find((p) => p.name === "blade");
  const strip = (x) => JSON.stringify(Object.assign({}, x, { planes: [] }));
  const ws = swapped.planes;
  check(`looks: ${e.name} with the weapon swapped in for its blade - the rest as it was`,
    ws.length === 2 && ws.map((p) => p.name).join() === "weapon,weapon_2" && ws.every((p) => p.parent === blade.parent && p.position.join() === blade.position.join() &&
      p.orientation.join() === blade.orientation.join() && JSON.stringify(p.keys) === JSON.stringify(blade.keys)) && strip(swapped) === strip(fx) &&
    ws[1].bitmap === "n5l_0123456789ab" && ws[0].bitmap === "null");
}
{
  const flyFx = B.withWeapon(core, core.readDraft(byName.n5_swp_fly.draft), weapon), node = flyFx.nodes.find((n) => n.name === flyFx.planes[0].parent);
  const tipWorld = [node.position[0] + tipAt[0], node.position[1] + tipAt[1], node.position[2] + tipAt[2]];
  check("looks: the flight's weapon point first along +y, 1.2 m up (spells.2da 1279's projectile)", near(node.position[2], 1.2, 1e-6) && near(tipWorld[1], 0.45) && near(tipWorld[2], 1.2));
  const hitFx = B.withWeapon(core, core.readDraft(byName.n5_swp_hit.draft), weapon);
  check("looks: the strike's swing as it was - its middle at 0.15 s", JSON.stringify(hitFx.nodes) === JSON.stringify(core.readDraft(byName.n5_swp_hit.draft).nodes) &&
    B.SLASH_AT === 0.15);
}
check("looks: the sword the default look, its own names n5_sws_*, the blade the committed fallback",
  B.DEFAULT_LOOK === "sword" && B.LOOKS.sword.project === "swpsword" && B.LOOKS.blade.project === null && B.lookName("n5_swp_idle", "sword") === "n5_sws_idle" &&
  B.lookName("n5_swp_hit", "blade") === "n5_swb_hit");
{
  const tmp = fs.mkdtempSync(path.join(require("os").tmpdir(), "n5_swp_check-"));
  try {
    for (const f of ["n5l_0123456789ab.tga", "n5l_0123456789ab.txi", "n5l_ffffffffffff.tga", "w_metal_tex.tga", "n5m_test.vfx"]) fs.writeFileSync(path.join(tmp, f), "x");
    const named = B.namedTextures(weapon, tmp).map((t) => t.name).sort().join();
    check("looks: the textures carried are those its meshes name (an Additive part's copy and its TXI) - nothing else", named === "n5l_0123456789ab.tga,n5l_0123456789ab.txi", named);
  } finally {
    fs.rmSync(tmp, { recursive: true, force: true });
  }
}
// The project: the game's parts named, nothing of the game's kept - the shortsword's three pieces, upright, pale blue,
// Additive.
{
  const proj = JSON.parse(lf(path.join(ROOT, "tools", "data", "composer", "swpsword.json")));
  const part = proj.variants[0].parts[0];
  check("looks: the sword's project - shortsword 012's three pieces named, upright, about 0.9 m, its texture greyscale, tinted pale blue, Additive",
    proj.kind === "spellmesh" && proj.name === "swpsword" && proj.variants.length === 1 && part.model.join() === "wswss_b_012,wswss_m_012,wswss_t_012" &&
    part.tilt.join() === "90,0" && part.scale === 1.2 && part.blend === "additive" && part.texture === "greyscale" && part.tint[2] === 1 && part.tint[0] < part.tint[1] && part.tint[1] < 1);
}
// build_derived.js builds it: the Spiritual Weapon's default look a generator of its own.
{
  const text = lf(path.join(ROOT, "tools", "data", "build_derived.js"));
  check("looks: build_derived.js builds the default look over the committed fallback (n5_swp_* owned by the writer's line)",
    /args: \["--look", look\]/.test(text) && /SWP_MODEL = \/\^n5_swp_\(idle\|fly\|hit\)\\\.mdl\$\/i/.test(text) && /swpmodel: \(w, old\)/.test(text));
}
// With the game (skipped without one): the default look built as build_derived.js runs it - the three models with the
// sword's meshes in (each the model writer's, naming the game's own texture or an Additive copy), into a scratch folder.
{
  const gameDir = T.option("game", process.env.NWN_GAME_DIR || "X:\\SteamLibrary\\steamapps\\common\\Neverwinter Nights");
  if (!fs.existsSync(path.join(gameDir, "data", "nwn_base.key"))) {
    console.log("skip: the sword's look built from the game - no game at " + gameDir + " (--game)");
  } else {
    const tmp = fs.mkdtempSync(path.join(require("os").tmpdir(), "n5_swp_look-"));
    try {
      const { spawnSync } = require("child_process");
      const r = spawnSync(process.execPath, [path.join(__dirname, "build.js"), "--look", "sword", "--files", tmp, "--json", "--game", gameDir,
        "--tools", T.dir, ...(T.option("reader", "") ? ["--reader", T.option("reader", "")] : [])], { encoding: "utf8", windowsHide: true, maxBuffer: 64 << 20 });
      let j = null;
      try { j = JSON.parse((r.stdout || "").trim().split(/\r?\n/).pop()); } catch (e) { /* below */ }
      const models = ["n5_swp_idle", "n5_swp_fly", "n5_swp_hit"].map((n) => path.join(tmp, n + ".mdl"));
      const ok = r.status === 0 && j && j.mesh === "n5m_swpsword" && models.every((m) => fs.existsSync(m) && lf(m).split("\n")[1] === "# nwn5e VFX editor (!vfxedit): " + path.basename(m, ".mdl") &&
        /\nnode trimesh n5_swp_\w+_p0_weapon\n/.test(lf(m)) && !/_blade\n/.test(lf(m)));
      check("looks, with the game: the sword's three models built (the weapon's meshes in, the blade's out), their textures with them",
        ok, JSON.stringify(j ? { mesh: j.mesh, files: (j.files || []).map((f) => path.basename(f.file)) } : (r.stderr || r.stdout || "").slice(0, 400)));
    } finally {
      fs.rmSync(tmp, { recursive: true, force: true });
    }
  }
}

console.log(failures ? "\n" + failures + " failed" : "\nall passed");
process.exit(failures ? 1 : 0);
