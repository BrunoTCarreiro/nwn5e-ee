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
// The motions of 0.43.1, measured over the loop (2 s; sampled every 0.05 s).
const over = (fx, name, f) => { const out = []; for (let t = 0; t <= 1.96; t += 0.05) out.push(f(placeAt(fx, name, t).pos, t)); return out; };
const range = (xs) => [Math.min(...xs), Math.max(...xs)];
{
  const fx = layerFor("tilted", { height: 1.2, radius: 1, tilt: 60, turns: 0.5, count: 3 });
  const names = fx.emitters.map((e) => e.name);
  const dist = names.flatMap((n) => over(fx, n, (p) => Math.hypot(p[0], p[1], p[2] - 1.2)));
  const top = range(over(fx, names[0], (p) => p[2]));
  const starts = names.map((n) => placeAt(fx, n, 0.5).pos.map((v) => v.toFixed(2)).join(","));
  console.log("tilted orbit: 3 rings, distance from the middle", range(dist).map((v) => v.toFixed(3)).join("-"), "heights", top.map((v) => v.toFixed(2)).join("-"), "places", starts.join(" | "));
  if (!near(range(dist)[0], 1, 0.002) || !near(range(dist)[1], 1, 0.002) || !near(top[1], 1.2 + Math.sin(Math.PI / 3), 0.03) || new Set(starts).size !== 3) fail("tilted orbit");
}
{
  const fx = layerFor("tornado", { from: 0, to: 2.5, bottom: 0.2, top: 1.4, turns: 1 });
  const n = fx.emitters[0].name, a = placeAt(fx, n, 0).pos, b = placeAt(fx, n, 1).pos, c = placeAt(fx, n, 1.95).pos;
  const r = (p) => Math.hypot(p[0], p[1]);
  console.log("tornado: radius", r(a).toFixed(2), r(b).toFixed(2), r(c).toFixed(2), "height", a[2].toFixed(2), b[2].toFixed(2), c[2].toFixed(2));
  if (!near(r(a), 0.2, 0.01) || !near(r(b), 0.8, 0.01) || !near(b[2], 1.25, 0.01) || !(r(c) > 1.35)) fail("tornado");
}
{
  const fx = layerFor("pendulum", { pivot: 2.2, length: 1.2, angle: 45, rate: 0.5, turned: 90 });
  const n = fx.emitters[0].name;
  const len = range(over(fx, n, (p) => Math.hypot(p[0], p[1], p[2] - 2.2))), q = placeAt(fx, n, 0.5).pos;
  console.log("pendulum: length", len.map((v) => v.toFixed(3)).join("-"), "at the swing's end", q.map((v) => v.toFixed(2)).join(","));
  if (!near(len[0], 1.2, 0.002) || !near(len[1], 1.2, 0.002) || !near(Math.abs(q[1]), 1.2 * Math.sin(Math.PI / 4), 0.01) || !near(q[0], 0, 0.01)) fail("pendulum");
}
{
  const fx = layerFor("wheel", { height: 1.2, radius: 0.8, out: 1, turns: 0.5 });
  const n = fx.emitters[0].name;
  const r = range(over(fx, n, (p) => Math.hypot(p[1], p[2] - 1.2))), x = range(over(fx, n, (p) => p[0]));
  console.log("wheel: radius", r.map((v) => v.toFixed(3)).join("-"), "x", x.map((v) => v.toFixed(3)).join("-"));
  if (!near(r[0], 0.8, 0.002) || !near(r[1], 0.8, 0.002) || !near(x[0], 1, 0.002) || !near(x[1], 1, 0.002)) fail("wheel");
}
{
  const fx = layerFor("corkscrew", { height: 1.2, length: 3, radius: 0.3, turns: 2 });
  const n = fx.emitters[0].name;
  const r = range(over(fx, n, (p) => Math.hypot(p[1], p[2] - 1.2))), x1 = placeAt(fx, n, 1).pos[0];
  console.log("corkscrew: radius", r.map((v) => v.toFixed(3)).join("-"), "x at 1 s", x1.toFixed(2));
  if (!near(r[0], 0.3, 0.002) || !near(r[1], 0.3, 0.002) || !near(x1, 1.5, 0.01)) fail("corkscrew");
}
{
  const fx = layerFor("boomerang", { height: 1.2, distance: 2.5, curve: 0.8, rate: 0.5, turned: 90 });
  const n = fx.emitters[0].name, a = placeAt(fx, n, 0).pos, b = placeAt(fx, n, 1).pos, c = placeAt(fx, n, 0.5).pos;
  console.log("boomerang (turned 90): start", a.map((v) => v.toFixed(2)).join(","), "far end", b.map((v) => v.toFixed(2)).join(","), "on the way", c.map((v) => v.toFixed(2)).join(","));
  if (!near(Math.hypot(a[0], a[1]), 0, 0.01) || !near(b[1], 2.5, 0.02) || !near(b[0], 0, 0.02) || !near(Math.abs(c[0]), 0.8, 0.02)) fail("boomerang");
}
{
  const fx = layerFor("breathe", { height: 1, inner: 0.3, outer: 1.5, rate: 0.5, count: 6 });
  const r = fx.emitters.flatMap((e) => over(fx, e.name, (p) => Math.hypot(p[0], p[1])));
  const mid = Math.hypot(...placeAt(fx, fx.emitters[0].name, 1).pos.slice(0, 2));
  console.log("breathe: 6 on a ring from", range(r).map((v) => v.toFixed(2)).join(" to "), "widest at 1 s", mid.toFixed(2));
  if (fx.emitters.length !== 6 || !near(range(r)[0], 0.3, 0.02) || !near(mid, 1.5, 0.01)) fail("breathe");
}
{
  const fx = layerFor("bounce", { base: 0, hop: 1, rate: 1 });
  const z = over(fx, fx.emitters[0].name, (p) => p[2]);
  console.log("bounce: heights", range(z).map((v) => v.toFixed(2)).join(" to "), "at 0 / 0.5 / 1 s", [0, 0.5, 1].map((t) => placeAt(fx, fx.emitters[0].name, t).pos[2].toFixed(2)).join(" "));
  if (!near(range(z)[0], 0, 0.01) || !near(range(z)[1], 1, 0.01) || !near(placeAt(fx, fx.emitters[0].name, 0.5).pos[2], 1, 0.01)) fail("bounce");
}
{
  const fx = layerFor("jitter", { height: 1.2, amount: 0.12, rate: 12 });
  const off = over(fx, fx.emitters[0].name, (p) => Math.max(Math.abs(p[0]), Math.abs(p[1]), Math.abs(p[2] - 1.2)));
  console.log("jitter: furthest", Math.max(...off).toFixed(3), "keys", fx.emitters[0].keys[0].position.length / 4);
  if (!(Math.max(...off) <= 0.12 + 1e-6) || !(Math.max(...off) > 0.03)) fail("jitter");
}
{
  const fx = layerFor("flower", { height: 1, radius: 1.2, petals: 5, depth: 0.6, turns: 0.5 });
  const r = range(over(fx, fx.emitters[0].name, (p) => Math.hypot(p[0], p[1])));
  console.log("flower: radius", r.map((v) => v.toFixed(2)).join(" to "));
  if (!near(r[1], 1.2, 0.01) || !(r[0] < 0.6) || !(r[0] >= 1.2 * 0.4 - 0.01)) fail("flower");
}
{
  const fx = layerFor("blink", { height: 1.2, radius: 1.5, rate: 2 });
  const n = fx.emitters[0].name, at = (t) => placeAt(fx, n, t).pos.map((v) => v.toFixed(3)).join(",");
  console.log("blink: held", at(0.1) === at(0.4), "jumped", at(0.4) !== at(0.6), "spots", [0.2, 0.7, 1.2, 1.7].map(at).join(" | "));
  if (at(0.1) !== at(0.4) || at(0.4) === at(0.6)) fail("blink");
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
