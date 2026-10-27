#!/usr/bin/env node
// Checks the effect-wide parts of the bench (0.44.0): the camera shake as the game moves its
// camera (a bump: 25 cm up and back in 0.1 s; a shake: 10 cm, 0.07 s up and 0.07 s down, for its
// duration, both after the delay), the draft's shake line (the C++ writer's port), and the count
// of particles alive at once against the game's own effects.
//   node tools/vfx_bench/check_effect.js
"use strict";
const path = require("path");
const core = require(path.join(__dirname, "public", "core.js"));
const { shakeAt, particleBudget, BUDGET } = require(path.join(__dirname, "public", "sim.js"));
const fails = [];
const near = (a, b, tol) => Math.abs(a - b) <= (tol || 1e-9);
const check = (what, ok, detail) => { console.log((ok ? "ok   " : "FAIL ") + what + (detail ? "  " + detail : "")); if (!ok) fails.push(what); };

// The bump: from its delay, up 0.25 m in 0.05 s, back down in 0.05 s, then still.
{
  const fx = core.effect({ shake_type: 1, shake_delay: 0.5, shake_duration: 9 });
  const at = (t) => shakeAt(fx, t);
  check("bump: still before its delay", at(0.49) === 0);
  check("bump: up 0.25 m 0.05 s in", near(at(0.55), 0.25, 1e-6), at(0.55).toFixed(3));
  check("bump: halfway up and down", near(at(0.525), 0.125, 1e-6) && near(at(0.575), 0.125, 1e-6));
  check("bump: down by 0.1 s, its duration ignored", at(0.6) < 1e-6 && at(0.61) === 0 && at(1.5) === 0);
}
// The shake: from its delay for its duration, 0 -> 0.1 -> 0 every 0.14 s.
{
  const fx = core.effect({ shake_type: 2, shake_delay: 0.2, shake_duration: 1 });
  const at = (t) => shakeAt(fx, t);
  check("shake: still before its delay", at(0.19) === 0);
  check("shake: up 0.1 m 0.07 s in", near(at(0.2 + 0.0699999), 0.1, 1e-5));
  check("shake: back down 0.14 s in", at(0.2 + 0.1399999) < 0.001);
  let top = 0;
  for (let t = 0.2; t < 1.2; t += 0.001) top = Math.max(top, at(t));
  check("shake: never more than 0.1 m", top <= 0.1 + 1e-9 && top > 0.099, top.toFixed(4));
  check("shake: over after its duration", at(1.21) === 0 && at(3) === 0);
  check("shake: none without a duration", shakeAt(core.effect({ shake_type: 2, shake_duration: 0 }), 0.05) === 0);
  check("no shake: never", shakeAt(core.effect(), 0.05) === 0);
}
// The draft keeps it (and an effect without one has no line).
{
  const fx = core.effect({ shake_type: 2, shake_delay: 0.5, shake_duration: 1.25 });
  const text = core.writeDraft(fx), back = core.readDraft(text);
  check("draft: shake line", text.includes("\nshake = 2 0.5 1.25\n"));
  check("draft: read back", back.shake_type === 2 && back.shake_delay === 0.5 && back.shake_duration === 1.25);
  check("draft: no shake, no line", !core.writeDraft(core.effect()).includes("shake"));
  check("draft: an odd type reads as none", core.readDraft("name = x\nshake = 7 1 1\n").shake_type === 0);
}
// Particles alive at once.
{
  const fx = core.effect({ duration: true, seconds: 6, emitters: [
    core.emitter({ birthrate: 100, life: 2 }),                          // 200
    core.emitter({ birthrate: 100, life: 2, emit_seconds: 0.5 }),       // switched off after 0.5 s: 50
    core.emitter({ birthrate: 10, life: -1 }),                          // never die: 10 x 6 s = 60
    core.emitter({ update: "Explosion", birthrate: 40.7, life: 1 }),    // 40
    core.emitter({ update: "Single", birthrate: 1, life: -1 }),         // 1
    core.emitter({ update: "Lightning", birthrate: 17 }),               // 17
    core.emitter({ birthrate: 20, life: 1, chunk: "fx_rockchunksml" }), // 20, chunk models
  ] });
  const b = particleBudget(fx);
  check("count: every kind of emitter", b.total === 388 && b.chunks === 20, JSON.stringify(b));
  check("count: the game's guide", BUDGET.typical === 250 && BUDGET.heaviest === 1700);
}
console.log(fails.length ? `FAILED: ${fails.length}` : "all effect checks pass");
process.exitCode = fails.length ? 1 : 0;
