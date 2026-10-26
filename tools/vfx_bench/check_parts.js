#!/usr/bin/env node
// Checks the parts to build effects from (public/parts.js, 0.43.0): every particle x emission x
// motion mix makes a sound layer (names, parents, keys) that the draft keeps byte for byte, on
// a lasting and a one-shot effect; a sample runs in the preview without breaking; the motions
// go where they say; the settings, a part put on an emitter and your own parts work; the
// textures are in the game's library, and a look that covers has transparency.
//   node tools/vfx_bench/check_parts.js [build\vfxedit]
"use strict";
const fs = require("fs"), path = require("path");
const pub = path.join(__dirname, "public");
const texDir = path.join(path.resolve(process.argv[2] || path.join(__dirname, "..", "..", "build", "vfxedit")), "library", "textures");
const core = require(path.join(pub, "core.js"));
const parts = require(path.join(pub, "parts.js"));
const { Sim } = require(path.join(pub, "sim.js"));
const fails = [];
const fail = (what) => { if (fails.length < 25) console.log("FAIL", what); fails.push(what); };
const textures = new Set(fs.readdirSync(texDir).map((n) => n.replace(/\.tga$/i, "").toLowerCase()));

// 1. The particles' textures are in the game's library.
for (const p of parts.PARTICLES) if (!textures.has(p.fields.texture)) fail("texture " + p.fields.texture + " of " + p.id);
// A look that covers (Normal or Punch-Through blend) needs a texture with transparency, or it
// draws squares (fxpa_smoke01a, fx_bubble have none).
for (const p of parts.PARTICLES) {
  const blend = p.fields.blend || "Normal";
  if (p.fields.chunk || (blend !== "Normal" && blend !== "Punch-Through")) continue;
  const t = core.decodeTga(new Uint8Array(fs.readFileSync(path.join(texDir, p.fields.texture + ".tga"))));
  let clear = 0;
  for (let i = 3; i < t.data.length; i += 4) if (t.data[i] < 16) clear++;
  if (clear < t.data.length / 40) fail(`${p.id}: ${blend} blend on ${p.fields.texture}, which has no transparency`);
}
// Ids unique per kind.
for (const list of [parts.PARTICLES, parts.EMISSIONS, parts.MOTIONS]) {
  const ids = list.map((x) => x.id);
  if (new Set(ids).size !== ids.length) fail("duplicate ids");
}

// 2. Every mix composes into a sound layer, on a lasting and a one-shot effect.
let mixes = 0;
const mixOf = (pa, em, mo) => ({ particle: { id: pa.id, params: {} }, emission: { id: em.id, params: {} }, motion: { id: mo.id, params: {} } });
for (const duration of [true, false]) {
  for (const pa of parts.PARTICLES) for (const em of parts.EMISSIONS) for (const mo of parts.MOTIONS) {
    const fx = core.newEffect();
    fx.duration = duration;
    const layer = parts.compose(fx, mixOf(pa, em, mo));
    mixes++;
    const all = { ...fx, nodes: [...fx.nodes, ...layer.nodes], emitters: [...fx.emitters, ...layer.emitters] };
    const names = [...all.nodes.map((n) => n.name), ...all.emitters.map((e) => e.name)];
    const tag = `${pa.id}+${em.id}+${mo.id}${duration ? "" : " (F)"}`;
    if (!layer.emitters.length) fail("no emitters " + tag);
    if (new Set(names).size !== names.length) fail("names " + tag);
    for (const x of [...layer.nodes, ...layer.emitters]) if (x.parent && !names.includes(x.parent)) fail("parent " + x.parent + " " + tag);
    for (const x of [...layer.nodes, ...layer.emitters]) for (const k of x.keys || []) {
      if (k.position.length % 4 || k.orientation.length % 5) fail("key widths " + tag);
      if (k.anim !== (duration ? "duration" : "impact")) fail("key anim " + tag);
      if ([...k.position, ...k.orientation].some((v) => !Number.isFinite(v))) fail("key value " + tag);
    }
    for (const e of layer.emitters) {
      if (e.texture !== pa.fields.texture) fail("particle not applied " + tag);
      if (em.fields.update && e.update !== em.fields.update) fail("emission not applied " + tag);
      if (e.update === "Lightning" && e.render !== "Linked") fail("lightning render " + tag);
      if (em.target && !layer.nodes.some((n) => n.parent === e.name)) fail("no target " + tag);
      if ([...e.position, ...e.orientation].some((v) => !Number.isFinite(v))) fail("placement " + tag);
    }
    // The draft keeps it: written, read and written again the same.
    const text = core.writeDraft(all);
    if (core.writeDraft(core.readDraft(text)) !== text) fail("draft round trip " + tag);
  }
}
console.log("mixes composed:", mixes);

// 3. They run: a sample of mixes (each particle, emission and motion at least once) for 2 s.
let ran = 0;
for (let i = 0; i < Math.max(parts.PARTICLES.length, parts.EMISSIONS.length, parts.MOTIONS.length); i++) {
  const pa = parts.PARTICLES[i % parts.PARTICLES.length], em = parts.EMISSIONS[i % parts.EMISSIONS.length], mo = parts.MOTIONS[i % parts.MOTIONS.length];
  const fx = core.effect({ emitters: [] });
  const layer = parts.compose(fx, mixOf(pa, em, mo));
  fx.nodes = layer.nodes; fx.emitters = layer.emitters;
  const sim = new Sim();
  sim.setEffect(fx);
  sim.eye = [4, -3, 2];
  for (let t = 0; t < 2; t += 1 / 30) sim.step(1 / 30);
  const snap = sim.snapshot();
  const bad = snap.some((s) => s.particles.some((p) => p.pos.some((v) => !Number.isFinite(v))));
  if (bad) fail(`not finite: ${pa.id}+${em.id}+${mo.id}`);
  ran++;
}
console.log("mixes run:", ran);

// 4. The motions go where they say. Emitter places at time t of the loop (after the impact).
function placeAt(fx, name, t) {
  const sim = new Sim();
  sim.setEffect(fx);
  const impact = sim.anims().impact;
  return sim.placesAt(impact + t).get(name);
}
function layerFor(motionId, params, emissionId) {
  const fx = core.effect({ emitters: [], lengths: [0, 2, 1] });
  const layer = parts.compose(fx, { particle: { id: "spark", params: {} }, emission: { id: emissionId || "stream", params: {} }, motion: { id: motionId, params: params || {} } });
  fx.nodes = layer.nodes; fx.emitters = layer.emitters;
  return fx;
}
const near = (a, b, tol) => Math.abs(a - b) <= (tol || 1e-3);
{
  const fx = layerFor("orbit", { height: 1, radius: 1.5, turns: 0.5, count: 3 });
  const names = fx.emitters.map((e) => e.name);
  const ps = names.map((n) => placeAt(fx, n, 0.5));
  const ok = ps.every((p) => near(Math.hypot(p.pos[0], p.pos[1]), 1.5) && near(p.pos[2], 1));
  const p0 = placeAt(fx, names[0], 0), p1 = placeAt(fx, names[0], 0.5);
  const turned = Math.atan2(p1.pos[1], p1.pos[0]) - Math.atan2(p0.pos[1], p0.pos[0]);
  console.log("orbit: 3 copies on the circle at 1 m:", ok, "turned in 0.5 s (rad):", turned.toFixed(3));
  if (!ok || !near(Math.abs(turned), Math.PI / 2, 0.02)) fail("orbit");
}
{
  const fx = layerFor("spiral_up", { from: 0.2, to: 2.2, radius: 0.8, turns: 1 });
  const n = fx.emitters[0].name, a = placeAt(fx, n, 0), b = placeAt(fx, n, 1), c = placeAt(fx, n, 1.98);
  console.log("spiral up: z", a.pos[2].toFixed(2), b.pos[2].toFixed(2), c.pos[2].toFixed(2), "r", Math.hypot(b.pos[0], b.pos[1]).toFixed(2));
  if (!near(a.pos[2], 0.2, 0.02) || !near(b.pos[2], 1.2, 0.02) || !(c.pos[2] > 2.1) || !near(Math.hypot(b.pos[0], b.pos[1]), 0.8, 0.01)) fail("spiral up");
}
{
  const fx = layerFor("spiral_in", { height: 1, radius: 1.5, turns: 0.5 });
  const n = fx.emitters[0].name, a = placeAt(fx, n, 0), b = placeAt(fx, n, 1);
  console.log("spiral in: r", Math.hypot(a.pos[0], a.pos[1]).toFixed(2), "->", Math.hypot(b.pos[0], b.pos[1]).toFixed(2));
  if (!near(Math.hypot(a.pos[0], a.pos[1]), 1.5, 0.01) || !near(Math.hypot(b.pos[0], b.pos[1]), 0.75, 0.02)) fail("spiral in");
}
{
  const fx = layerFor("sprinkler", { height: 1, turns: 0.5, count: 2 });
  const zs = fx.emitters.map((e) => { const p = placeAt(fx, e.name, 0); const q = p.q; return [2 * (q[0] * q[2] + q[3] * q[1]), 2 * (q[1] * q[2] - q[3] * q[0]), 1 - 2 * (q[0] * q[0] + q[1] * q[1])]; });
  console.log("sprinkler: the jets' z:", zs.map((z) => z.map((v) => v.toFixed(2)).join(",")).join(" | "));
  if (!zs.every((z) => near(z[2], 0, 1e-3)) || !near(zs[0][0] + zs[1][0], 0, 1e-3)) fail("sprinkler");
}
{
  const fx = layerFor("bob", { height: 1.2, amplitude: 0.3, rate: 0.5 });
  const n = fx.emitters[0].name;
  const zs = [0, 0.5, 1, 1.5].map((t) => placeAt(fx, n, t).pos[2]);
  console.log("bob: z", zs.map((z) => z.toFixed(2)).join(" "));
  if (!near(zs[0], 1.2, 0.01) || !near(zs[1], 1.5, 0.02) || !near(zs[3], 0.9, 0.02)) fail("bob");
}
{
  const fx = layerFor("still", { height: 0.5, radius: 2, count: 4 });
  const ps = fx.emitters.map((e) => placeAt(fx, e.name, 0).pos);
  console.log("still ring:", ps.map((p) => p.map((v) => v.toFixed(1)).join(",")).join(" | "));
  if (ps.length !== 4 || !ps.every((p) => near(Math.hypot(p[0], p[1]), 2) && near(p[2], 0.5))) fail("still ring");
}
{
  const fx = layerFor("still", { height: 1 }, "lightning");
  const e = fx.emitters[0], t = fx.nodes.find((n) => n.parent === e.name);
  console.log("lightning: render", e.render, "points", e.birthrate, "target", t && t.position.join(","));
  if (e.render !== "Linked" || e.birthrate !== 17 || !t || !near(t.position[0], 2)) fail("lightning");
}
// A one-shot effect's keys go in its impact, over its seconds.
{
  const fx = core.effect({ emitters: [], duration: false, seconds: 3 });
  const layer = parts.compose(fx, { particle: { id: "spark", params: {} }, emission: { id: "stream", params: {} }, motion: { id: "orbit", params: { turns: 1 } } });
  const k = layer.nodes[0].keys[0];
  console.log("F effect: anim", k.anim, "last key at", k.orientation[k.orientation.length - 5], "turns", (k.orientation[k.orientation.length - 1] / (2 * Math.PI)).toFixed(2));
  if (k.anim !== "impact" || !near(k.orientation[k.orientation.length - 5], 3) || !near(k.orientation[k.orientation.length - 1], 6 * Math.PI, 1e-3)) fail("F keys");
}

// 5. Settings: a particle's colour / size / opacity, an emission's rate, clamped to range.
{
  const e = core.emitter();
  parts.applyParticle(e, parts.find("particle", "flame"), { colour: [0, 1, 0], size: 0.9, opacity: 0.5 });
  const f = parts.find("particle", "flame").fields;
  console.log("particle settings: colour", e.color_start.join(","), "size", e.size_start, "->", e.size_end.toFixed(3), "opacity", e.alpha_start);
  if (e.color_start[1] !== 1 || e.size_start !== 0.9 || !near(e.size_end, f.size_end * 2) || e.alpha_start !== 0.5 || e.grid !== 4) fail("particle settings");
  parts.applyParticle(e, parts.find("particle", "spark"), {});
  if (e.grid !== 1 || e.fps !== 0 || e.random !== false) fail("a look left behind (grid / fps / random)");
  const target = parts.applyEmission(e, parts.find("emission", "beam"), { rate: 9999, distance: 4 });
  console.log("emission settings: rate clamped", e.birthrate, "p2p", e.p2p, "target", target.join(","));
  if (e.birthrate !== 400 || e.p2p !== "bezier" || target[0] !== 4) fail("emission settings");
  parts.applyEmission(e, parts.find("emission", "stream"), {});
  if (e.p2p !== "" || e.xsize !== 0 || e.mass !== 0) fail("an emission left behind (p2p / area / mass)");
}

// 6. A motion on an emitter that's there; your own parts.
{
  const fx = core.newEffect();
  const e = fx.emitters[0];
  const nodes = parts.applyMotion(fx, e, parts.find("motion", "orbit"), { radius: 2, count: 5 });
  console.log("motion on an emitter: nodes", nodes.map((n) => n.name).join(","), "parent", e.parent, "position", e.position.map((v) => v.toFixed(2)).join(","));
  if (nodes.length !== 1 || e.parent !== nodes[0].name || !near(e.position[0], 2)) fail("applyMotion");
  const mine = parts.cleanPart(JSON.parse(JSON.stringify(parts.partFrom("particle", e, "My Sparks!", fx))));
  if (mine.name !== "mysparks" || mine.fields.texture !== e.texture || "birthrate" in mine.fields) fail("partFrom / cleanPart particle");
  const own = [mine, parts.cleanPart({ kind: "emission", name: "jet", fields: { birthrate: 5, velocity: 9, update: "Fountain", bogus: 1, life: "x" }, target: [1, 2, 3] })];
  if ("bogus" in own[1].fields || "life" in own[1].fields || own[1].target[2] !== 3) fail("cleanPart emission");
  for (const bad of [null, { kind: "motion", name: "x" }, { kind: "particle", name: "!!!" }, { kind: "particle", name: "a", fields: { texture: "../x" } }]) {
    try { const c = parts.cleanPart(bad); if (bad && bad.fields && c.fields.texture) fail("cleanPart let a bad texture in"); } catch (err) { /* refused */ }
  }
  const layer = parts.compose(fx, { particle: { id: "yours:mysparks", params: {} }, emission: { id: "yours:jet", params: { speed: 2 } }, motion: { id: "still", params: {} } }, own);
  const le = layer.emitters[0];
  console.log("your parts in a mix:", le.name, le.texture, "rate", le.birthrate, "speed", le.velocity, "target", layer.nodes.length);
  if (le.velocity !== 2 || le.birthrate !== 5 || layer.nodes.length !== 1) fail("your parts in a mix");
}
console.log(fails.length ? `FAILED: ${fails.length}` : "all parts checks pass");
process.exitCode = fails.length ? 1 : 0;
