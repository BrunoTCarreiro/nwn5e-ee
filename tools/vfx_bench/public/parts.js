// VFX Bench - parts to build effects from (0.43.0; owner, 2026-10-26: "build a system that
// allows to mix and match different presets of the various parts of a vfx to create a new
// one"). An emitter is three sides, and each has presets here:
//   particle - how each particle looks: texture, how it's drawn, size, colour, fade;
//   emission - how they come out and move by themselves: how many, how fast, which way,
//              falling, bouncing, to a target, a lightning bolt;
//   motion   - how the emitter goes round the character: still, orbit, spiral, rise...
//              (nodes and keys in the effect's loop).
// A mix is one of each with its settings: compose() makes it a layer (emitters and nodes) to
// add to an effect; applyParticle / applyEmission / applyMotion put one side on an emitter
// that's there already. Your own particles and emissions (kept from an emitter) are checked
// by cleanPart() - the page and the bench's server both use it. The values start from the
// game's own effects (how they use each texture: sheet, render, blend, size, life). A look that
// covers (Normal blend) needs a texture with transparency - fxpa_smoke01a, fx_bubble and the
// flame sheets have none (squares): those are Lighten here, as the game uses them.
(function (root) {
  "use strict";
  const core = typeof module === "object" && module.exports ? require("./core.js") : root.VfxCore;

  // Which emitter fields each side sets (the rest of the emitter is left as it is).
  const PARTICLE_KEYS = ["texture", "render", "blend", "size_start", "size_end", "color_start", "color_end",
    "alpha_start", "alpha_end", "grid", "frame_end", "fps", "random", "particle_rot", "twosidedtex", "blur_length", "chunk"];
  const EMISSION_KEYS = ["update", "birthrate", "emit_seconds", "life", "velocity", "randvel", "spread", "mass",
    "xsize", "ysize", "inherit", "inherit_local", "inherit_part", "bounce", "bounce_co", "splat", "deadspace",
    "p2p", "p2p_bezier2", "p2p_bezier3", "combinetime", "grav", "drag", "threshold",
    "lightning_delay", "lightning_radius", "lightning_scale"];
  const TAU = Math.PI * 2;

  // --- particles ---------------------------------------------------------------------------
  // Every particle has the same settings: its colour, the colour it fades to, its size and
  // how opaque it starts (the end size and opacity keep the preset's proportions).
  const PARTICLE_PARAMS = [
    { key: "colour", kind: "colour", label: "Colour" },
    { key: "fade", kind: "colour", label: "Fades to" },
    { key: "size", label: "Size (m)", min: 0.01, max: 6, step: 0.01 },
    { key: "opacity", label: "Opacity", min: 0, max: 1, step: 0.05 },
  ];
  const look = (id, name, note, fields) => ({ id, name, note, fields });
  const PARTICLES = [
    look("spark", "Spark", "A tiny hot point that fades: embers, fire, magic.",
      { texture: "fxpa_dot01", blend: "Lighten", size_start: 0.08, size_end: 0.02, color_start: [1, 0.85, 0.45], color_end: [1, 0.35, 0.1], alpha_start: 1, alpha_end: 0 }),
    look("glow", "Soft glow", "A big soft light: orbs, auras, a hand's glow.",
      { texture: "fxpa_dot01", blend: "Lighten", size_start: 0.6, size_end: 0.5, color_start: [0.55, 0.75, 1], color_end: [0.3, 0.45, 1], alpha_start: 0.8, alpha_end: 0 }),
    look("flare", "Star flare", "A four-point flare that shrinks: hits, sparkle.",
      { texture: "fxpa_flare", blend: "Lighten", size_start: 0.35, size_end: 0.05, color_start: [0.85, 0.9, 1], color_end: [0.5, 0.6, 1], alpha_start: 1, alpha_end: 0.2 }),
    look("twinkle", "Twinkling star", "Little stars that flicker as they go.",
      { texture: "fxpa_starbnw", blend: "Lighten", grid: 2, frame_end: 3, fps: 15, random: true, size_start: 0.3, size_end: 0.1, color_start: [1, 0.9, 0.55], color_end: [1, 0.75, 0.3], alpha_start: 1, alpha_end: 0 }),
    look("sparkle", "Magic sparkle", "Crackling sparkles (an animated sheet).",
      { texture: "fxpa_sparks", blend: "Lighten", grid: 4, frame_end: 15, fps: 16, random: true, size_start: 0.4, size_end: 0.1, color_start: [0.75, 0.55, 1], color_end: [0.4, 0.3, 1], alpha_start: 1, alpha_end: 0 }),
    look("flame", "Flame", "Licking flames (an animated sheet).",
      { texture: "fxpa_flame00", blend: "Lighten", grid: 4, frame_end: 15, fps: 16, random: true, size_start: 0.45, size_end: 0.2, color_start: [1, 0.75, 0.4], color_end: [1, 0.3, 0.05], alpha_start: 1, alpha_end: 0 }),
    look("fire", "Fire puff", "A big rolling puff of fire: fireballs, burning.",
      { texture: "fxpa_firebb", blend: "Lighten", grid: 4, frame_end: 15, fps: 16, size_start: 0.8, size_end: 1.4, color_start: [1, 0.85, 0.6], color_end: [0.9, 0.35, 0.1], alpha_start: 1, alpha_end: 0 }),
    look("smoke", "Smoke", "Grey smoke that grows and thins (it covers what's behind it).",
      { texture: "fxpa_smoke01", blend: "Normal", grid: 4, frame_end: 15, fps: 12, random: true, size_start: 0.5, size_end: 2, color_start: [0.45, 0.45, 0.45], color_end: [0.3, 0.3, 0.3], alpha_start: 0.6, alpha_end: 0 }),
    look("mist", "Mist", "Soft low fog.",
      { texture: "fxpa_mist", blend: "Normal", grid: 4, frame_end: 15, random: true, size_start: 1, size_end: 3, color_start: [0.75, 0.85, 0.95], color_end: [0.6, 0.7, 0.85], alpha_start: 0.35, alpha_end: 0 }),
    look("shadow", "Shadow wisp", "Dark smoke: necromancy, fear, curses.",
      { texture: "fxpa_smoke01", blend: "Normal", grid: 4, frame_end: 15, fps: 12, random: true, size_start: 0.4, size_end: 1.2, color_start: [0.18, 0.05, 0.25], color_end: [0.02, 0, 0.05], alpha_start: 0.85, alpha_end: 0 }),
    look("streak", "Streak", "A line along its path (motion blur): fast sparks, energy.",
      { texture: "fxpa_dot01", render: "Motion_Blur", blend: "Lighten", blur_length: 0.08, size_start: 0.08, size_end: 0.03, color_start: [0.7, 0.85, 1], color_end: [0.4, 0.55, 1], alpha_start: 1, alpha_end: 0 }),
    look("holy", "Holy light", "A warm lens flare.",
      { texture: "fxpa_lensflare", blend: "Lighten", size_start: 0.6, size_end: 0.15, color_start: [1, 0.92, 0.6], color_end: [1, 0.8, 0.35], alpha_start: 1, alpha_end: 0 }),
    look("snow", "Snowflake", "Spinning snowflakes.",
      { texture: "fxpa_flake", blend: "Lighten", particle_rot: 1.5, size_start: 0.1, size_end: 0.07, color_start: [1, 1, 1], color_end: [0.7, 0.85, 1], alpha_start: 1, alpha_end: 0.3 }),
    look("leaf", "Leaves", "Tumbling leaves (they cover what's behind them).",
      { texture: "fxpa_leaf", blend: "Normal", grid: 4, frame_end: 15, random: true, particle_rot: 2, size_start: 0.25, size_end: 0.25, color_start: [1, 1, 1], color_end: [0.9, 0.85, 0.7], alpha_start: 1, alpha_end: 0 }),
    look("bubble", "Bubbles", "Glassy bubbles that swell and fade.",
      { texture: "fx_bubble", blend: "Lighten", grid: 2, frame_end: 3, random: true, size_start: 0.12, size_end: 0.25, color_start: [1, 1, 1], color_end: [1, 1, 1], alpha_start: 0.9, alpha_end: 0 }),
    look("ring", "Ground ring", "A flat ring on the ground that grows: with One flash, a shockwave.",
      { texture: "fxpa_ringwave", render: "Billboard_to_World_Z", blend: "Lighten", grid: 2, frame_end: 3, fps: 12, size_start: 0.5, size_end: 6, color_start: [0.7, 0.85, 1], color_end: [0.4, 0.6, 1], alpha_start: 1, alpha_end: 0 }),
    look("circle", "Magic circle", "A glowing circle on the ground: with Hover it stays.",
      { texture: "fxpa_symbl_004", render: "Billboard_to_World_Z", blend: "Lighten", size_start: 3, size_end: 3, color_start: [0.6, 0.8, 1], color_end: [0.6, 0.8, 1], alpha_start: 0.9, alpha_end: 0.9 }),
    look("bolt", "Lightning", "A jagged electric line, drawn from one particle to the next: for the Lightning bolt, beams, chains.",
      { texture: "fxpa_lbolt2l", render: "Linked", blend: "Lighten", size_start: 0.12, size_end: 0.08, color_start: [0.75, 0.85, 1], color_end: [0.4, 0.6, 1], alpha_start: 1, alpha_end: 1 }),
    look("rain", "Raindrop", "A falling streak: with Falling from an area.",
      { texture: "fxpa_rain01", render: "Aligned_to_World_Z", blend: "Lighten", size_start: 0.18, size_end: 0.15, color_start: [0.7, 0.8, 0.95], color_end: [0.7, 0.8, 0.95], alpha_start: 0.8, alpha_end: 0.5 }),
    look("rock", "Rock chunks", "Each particle a small rock model of the game's, no sprite (grey squares in the bench).",
      { chunk: "fx_rockchunksml", texture: "fxpa_dot01", size_start: 0.1, size_end: 0.1, color_start: [1, 1, 1], color_end: [1, 1, 1], alpha_start: 1, alpha_end: 1 }),
  ];

  // --- emissions ---------------------------------------------------------------------------
  const rate = (value) => ({ key: "rate", label: "Particles a second", min: 0, max: 400, step: 1, value, field: "birthrate" });
  const count = (value) => ({ key: "rate", label: "How many", min: 1, max: 200, step: 1, value, field: "birthrate" });
  const speed = (value) => ({ key: "speed", label: "Speed (m/s)", min: 0, max: 20, step: 0.1, value, field: "velocity" });
  const life = (value, label) => ({ key: "life", label: label || "Life (seconds)", min: 0.05, max: 10, step: 0.05, value, field: "life" });
  const spread = (value) => ({ key: "spread", label: "Spread (radians; 6.28 all round)", min: 0, max: 6.2832, step: 0.05, value, field: "spread" });
  const fall = (value) => ({ key: "gravity", label: "Gravity (below 0 rises)", min: -3, max: 5, step: 0.1, value, field: "mass" });
  const area = (value) => ({ key: "area", label: "Area (m across)", min: 0, max: 10, step: 0.1, value, set: (e, v) => { e.xsize = e.ysize = v * 100; } });
  const distance = (value) => ({ key: "distance", label: "Target: m along its x", min: 0.1, max: 20, step: 0.1, value });
  const emit = (id, name, note, fields, params, more) => Object.assign({ id, name, note, fields, params }, more || {});
  const EMISSIONS = [
    emit("stream", "Stream", "A steady jet up the emitter's z.",
      { update: "Fountain", birthrate: 30, life: 1, velocity: 1.2, randvel: 0.3, spread: 0.25 }, [rate(30), speed(1.2), life(1), spread(0.25)]),
    emit("spray", "Fountain spray", "Shoots up and falls back.",
      { update: "Fountain", birthrate: 40, life: 1.4, velocity: 3, randvel: 0.8, spread: 0.7, mass: 1 }, [rate(40), speed(3), life(1.4), fall(1)]),
    emit("burst", "Burst", "All at once when it starts, every way.",
      { update: "Explosion", birthrate: 40, life: 0.9, velocity: 3, randvel: 1, spread: TAU }, [count(40), speed(3), life(0.9)]),
    emit("flash", "One flash", "One particle where it is, once: with Ground ring, a shockwave.",
      { update: "Explosion", birthrate: 1, life: 0.6, velocity: 0, randvel: 0, spread: 0 }, [life(0.6)]),
    emit("hover", "Hover", "One particle that rides with the emitter and stays.",
      { update: "Single", birthrate: 1, life: -1, velocity: 0, randvel: 0, spread: 0, inherit: true }, []),
    emit("aura", "Aura", "A slow shell all round.",
      { update: "Fountain", birthrate: 40, life: 0.8, velocity: 0.3, randvel: 0.2, spread: TAU }, [rate(40), speed(0.3), life(0.8)]),
    emit("trail", "Trail", "Left behind where the emitter passes: give it a Motion.",
      { update: "Fountain", birthrate: 60, life: 0.6, velocity: 0, randvel: 0, spread: 0 }, [rate(60), life(0.6)]),
    emit("drift", "Drifting motes", "A few slow motes floating every way.",
      { update: "Fountain", birthrate: 12, life: 3, velocity: 0.2, randvel: 0.2, spread: TAU }, [rate(12), speed(0.2), life(3)]),
    emit("rise", "Rising from an area", "Up from a patch round the emitter: steam, souls, embers.",
      { update: "Fountain", birthrate: 25, life: 1.8, velocity: 0.6, randvel: 0.2, spread: 0.15, xsize: 120, ysize: 120 }, [rate(25), area(1.2), speed(0.6), life(1.8)]),
    emit("rain", "Falling from an area", "Dropped from a patch: give it a height (Still, 3 m).",
      { update: "Fountain", birthrate: 60, life: 1.2, velocity: 0, randvel: 0, spread: 0, mass: 2, xsize: 300, ysize: 300 }, [rate(60), area(3), life(1.2), fall(2)]),
    emit("sparks", "Bouncing sparks", "Thrown out, they fall and bounce on the ground.",
      { update: "Fountain", birthrate: 30, life: 1.6, velocity: 2.5, randvel: 1, spread: 1.2, mass: 2, bounce: true, bounce_co: 0.4 }, [rate(30), speed(2.5), life(1.6)]),
    emit("gather", "Gather in", "Drawn in from all round to a point above the emitter.",
      { update: "Fountain", birthrate: 40, life: 2, velocity: 0, randvel: 0, spread: 0, xsize: 250, ysize: 250, p2p: "gravity", grav: 4, drag: 0.6, threshold: 0.15 },
      [rate(40), area(2.5), { key: "pull", label: "Pull (m/s²)", min: 0, max: 30, step: 0.5, value: 4, field: "grav" },
        { key: "up", label: "Point: m above it", min: 0, max: 5, step: 0.1, value: 1 }],
      { target: (p) => [0, 0, p.up] }),
    emit("beam", "Beam to a target", "Streams to a target node (move it in Nodes).",
      { update: "Fountain", birthrate: 60, life: 0.5, velocity: 0, randvel: 0, spread: 0, p2p: "bezier" },
      [rate(60), life(0.5, "Seconds to get there"), distance(1.5)], { target: (p) => [p.distance, 0, 0] }),
    emit("arc", "Arc to a target", "Curves up and over to a target node.",
      { update: "Fountain", birthrate: 40, life: 0.8, velocity: 0, randvel: 0, spread: 0, p2p: "bezier", p2p_bezier2: 1.5, p2p_bezier3: 1.5 },
      [rate(40), life(0.8, "Seconds to get there"), distance(2),
        { key: "height", label: "How high it arcs (m)", min: 0, max: 10, step: 0.1, value: 1.5, set: (e, v) => { e.p2p_bezier2 = e.p2p_bezier3 = v; } }],
      { target: (p) => [p.distance, 0, 0] }),
    emit("lightning", "Lightning bolt", "A crackling bolt to a target node (drawn Linked: the Lightning particle fits).",
      { update: "Lightning", birthrate: 17, lightning_delay: 0.08, lightning_radius: 0.15, lightning_scale: 0.2 },
      [{ key: "points", label: "Points (3, 5, 9, 17 or 33)", min: 3, max: 33, step: 1, value: 17, field: "birthrate" },
        { key: "every", label: "A new bolt every (s)", min: 0, max: 2, step: 0.01, value: 0.08, field: "lightning_delay" },
        { key: "jag", label: "Jag across (m)", min: 0, max: 1, step: 0.01, value: 0.15, field: "lightning_radius" },
        distance(2)],
      { target: (p) => [p.distance, 0, 0] }),
  ];

  // --- motions -----------------------------------------------------------------------------
  const height = (value, label) => ({ key: "height", label: label || "Height (m)", min: -1, max: 6, step: 0.05, value });
  const radius = (value) => ({ key: "radius", label: "Radius (m)", min: 0, max: 8, step: 0.05, value });
  const turns = (value) => ({ key: "turns", label: "Turns a second (below 0: the other way)", min: -5, max: 5, step: 0.05, value });
  const copies = (value) => ({ key: "count", label: "How many, spaced round", min: 1, max: 12, step: 1, value });
  const from = (value) => ({ key: "from", label: "From height (m)", min: -1, max: 6, step: 0.05, value });
  const to = (value) => ({ key: "to", label: "To height (m)", min: -1, max: 6, step: 0.05, value });

  // Keys that turn round z through `n` whole turns (sign: the way) over `length` seconds, in
  // quarter turns so the game's short-way slerp goes the right way round (rows t ax ay az angle).
  function turnKeys(length, n) {
    const rows = [], steps = Math.max(4, Math.round(Math.abs(n)) * 4);
    for (let i = 0; i <= steps; i++) {
      const angle = (i / steps) * n * TAU;
      rows.push(Math.fround((i / steps) * length), 0, 0, angle < 0 ? -1 : 1, Math.abs(angle));
    }
    return rows;
  }
  // Position keys sampled from f(u), u 0..1 over the length (rows t x y z).
  function moveKeys(length, steps, f) {
    const rows = [];
    for (let i = 0; i <= steps; i++) rows.push(Math.fround((i / steps) * length), ...f(i / steps));
    return rows;
  }
  // Whole turns in the loop (so it loops without a jump), at least one, the rate's way.
  function wholeTurns(rateValue, length) {
    const n = Math.max(1, Math.round(Math.abs(rateValue) * length));
    return rateValue < 0 ? -n : n;
  }
  const around = (n, r, k, z) => [r * Math.cos((TAU * k) / n), r * Math.sin((TAU * k) / n), z || 0];
  // Quaternions (x y z w): a turn round an axis, one turn after another (b first), and back to
  // axis-angle as drafts keep it.
  const qaxis = (axis, angle) => [axis[0] * Math.sin(angle / 2), axis[1] * Math.sin(angle / 2), axis[2] * Math.sin(angle / 2), Math.cos(angle / 2)];
  const qmul = (a, b) => [a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
    a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]];
  function axisAngle(q) {
    const w = Math.max(-1, Math.min(1, q[3])), s = Math.sqrt(Math.max(0, 1 - w * w));
    return s > 1e-6 ? [q[0] / s, q[1] / s, q[2] / s, 2 * Math.acos(w)] : [0, 0, 0, 0];
  }

  // A motion's emitters: `n` copies of the base, named name, name2, ...
  function spaced(ctx, base, n, place) {
    const out = [];
    for (let k = 0; k < n; k++) {
      const e = core.clone(base);
      e.name = k ? ctx.unique(base.name + (k + 1)) : base.name;
      place(e, k);
      out.push(e);
    }
    return out;
  }
  // A node turning round z in the loop, at [0 0 z] under the character's point.
  function spinner(ctx, suffix, z, n) {
    return core.node({ name: ctx.unique(ctx.base + "_" + suffix), position: [0, 0, z],
      keys: [{ anim: ctx.anim, position: [], orientation: turnKeys(ctx.length, n) }] });
  }
  const motion = (id, name, note, params, build) => ({ id, name, note, params, build });
  const MOTIONS = [
    motion("still", "Still", "Stays where it is (several: a ring of them).", [height(1), radius(0), copies(1)], (ctx, base, p) => ({
      nodes: [],
      emitters: spaced(ctx, base, p.count, (e, k) => { e.parent = ""; e.position = around(p.count, p.radius, k, p.height); }),
    })),
    motion("orbit", "Orbit", "Circles the character.", [height(1), radius(1), turns(0.5), copies(1)], (ctx, base, p) => {
      const node = spinner(ctx, "orbit", p.height, wholeTurns(p.turns, ctx.length));
      return { nodes: [node], emitters: spaced(ctx, base, p.count, (e, k) => { e.parent = node.name; e.position = around(p.count, p.radius, k); }) };
    }),
    motion("spiral_up", "Spiral up", "Circles while rising, then starts again at the bottom.", [from(0.1), to(2.2), radius(0.8), turns(1), copies(1)], (ctx, base, p) => {
      const node = spinner(ctx, "spiral", p.from, wholeTurns(p.turns, ctx.length));
      node.keys[0].position = moveKeys(ctx.length, 1, (u) => [0, 0, p.from + (p.to - p.from) * u]);
      return { nodes: [node], emitters: spaced(ctx, base, p.count, (e, k) => { e.parent = node.name; e.position = around(p.count, p.radius, k); }) };
    }),
    motion("spiral_down", "Spiral down", "Circles while sinking, then starts again at the top.", [from(2.2), to(0.1), radius(0.8), turns(1), copies(1)], (ctx, base, p) => {
      const node = spinner(ctx, "spiral", p.from, wholeTurns(p.turns, ctx.length));
      node.keys[0].position = moveKeys(ctx.length, 1, (u) => [0, 0, p.from + (p.to - p.from) * u]);
      return { nodes: [node], emitters: spaced(ctx, base, p.count, (e, k) => { e.parent = node.name; e.position = around(p.count, p.radius, k); }) };
    }),
    motion("spiral_in", "Spiral in", "Circles inward to the middle.", [height(1), radius(1.5), turns(0.8), copies(1)], (ctx, base, p) => {
      const node = spinner(ctx, "spiral", p.height, wholeTurns(p.turns, ctx.length));
      return { nodes: [node], emitters: spaced(ctx, base, p.count, (e, k) => {
        const out = around(p.count, p.radius, k);
        e.parent = node.name;
        e.position = out;
        e.keys = [{ anim: ctx.anim, position: moveKeys(ctx.length, 1, (u) => [out[0] * (1 - u), out[1] * (1 - u), 0]), orientation: [] }];
      }) };
    }),
    motion("spiral_out", "Spiral out", "Circles outward from the middle.", [height(1), radius(1.5), turns(0.8), copies(1)], (ctx, base, p) => {
      const node = spinner(ctx, "spiral", p.height, wholeTurns(p.turns, ctx.length));
      return { nodes: [node], emitters: spaced(ctx, base, p.count, (e, k) => {
        const out = around(p.count, p.radius, k);
        e.parent = node.name;
        e.position = [0, 0, 0];
        e.keys = [{ anim: ctx.anim, position: moveKeys(ctx.length, 1, (u) => [out[0] * u, out[1] * u, 0]), orientation: [] }];
      }) };
    }),
    motion("rise", "Rise", "Goes straight up, then starts again.", [from(0), to(2.5), radius(0), copies(1)], (ctx, base, p) => ({
      nodes: [],
      emitters: spaced(ctx, base, p.count, (e, k) => {
        const at = around(p.count, p.radius, k);
        e.parent = "";
        e.position = [at[0], at[1], p.from];
        e.keys = [{ anim: ctx.anim, position: moveKeys(ctx.length, 1, (u) => [at[0], at[1], p.from + (p.to - p.from) * u]), orientation: [] }];
      }),
    })),
    motion("fall", "Fall", "Comes straight down, then starts again.", [from(3), to(0), radius(0), copies(1)], (ctx, base, p) => ({
      nodes: [],
      emitters: spaced(ctx, base, p.count, (e, k) => {
        const at = around(p.count, p.radius, k);
        e.parent = "";
        e.position = [at[0], at[1], p.from];
        e.keys = [{ anim: ctx.anim, position: moveKeys(ctx.length, 1, (u) => [at[0], at[1], p.from + (p.to - p.from) * u]), orientation: [] }];
      }),
    })),
    motion("bob", "Bob", "Floats up and down.", [height(1.2), { key: "amplitude", label: "Up and down by (m)", min: 0, max: 3, step: 0.05, value: 0.25 },
      { key: "rate", label: "Bobs a second", min: 0.1, max: 5, step: 0.05, value: 0.5 }, radius(0), copies(1)], (ctx, base, p) => {
      const n = Math.max(1, Math.round(p.rate * ctx.length));
      return { nodes: [], emitters: spaced(ctx, base, p.count, (e, k) => {
        const at = around(p.count, p.radius, k);
        e.parent = "";
        e.position = [at[0], at[1], p.height];
        e.keys = [{ anim: ctx.anim, position: moveKeys(ctx.length, n * 12, (u) => [at[0], at[1], p.height + p.amplitude * Math.sin(TAU * n * u + (TAU * k) / p.count)]), orientation: [] }];
      }) };
    }),
    motion("sweep", "Sweep", "Swings side to side in front of the character.", [height(1.2), radius(1),
      { key: "angle", label: "Swing (degrees each way)", min: 5, max: 170, step: 5, value: 60 },
      { key: "rate", label: "Swings a second", min: 0.1, max: 5, step: 0.05, value: 0.5 }], (ctx, base, p) => {
      const n = Math.max(1, Math.round(p.rate * ctx.length)), a = (p.angle * Math.PI) / 180;
      const rows = [], steps = n * 16;
      for (let i = 0; i <= steps; i++) {
        const angle = a * Math.sin((TAU * n * i) / steps);
        rows.push(Math.fround((i / steps) * ctx.length), 0, 0, angle < 0 ? -1 : 1, Math.abs(angle));
      }
      const node = core.node({ name: ctx.unique(ctx.base + "_sweep"), position: [0, 0, p.height], keys: [{ anim: ctx.anim, position: [], orientation: rows }] });
      return { nodes: [node], emitters: spaced(ctx, base, 1, (e) => { e.parent = node.name; e.position = [p.radius, 0, 0]; }) };
    }),
    motion("figure8", "Figure eight", "Loops a figure of eight round the character.", [height(1), radius(1.2),
      { key: "rate", label: "Loops a second", min: 0.05, max: 3, step: 0.05, value: 0.4 }], (ctx, base, p) => {
      const n = Math.max(1, Math.round(p.rate * ctx.length));
      return { nodes: [], emitters: spaced(ctx, base, 1, (e) => {
        e.parent = "";
        e.position = [0, 0, p.height];
        e.keys = [{ anim: ctx.anim, orientation: [], position: moveKeys(ctx.length, n * 24, (u) => {
          const t = TAU * n * u;
          return [p.radius * Math.sin(t), p.radius * Math.sin(t) * Math.cos(t), p.height];
        }) }];
      }) };
    }),
    motion("sprinkler", "Sprinkler", "Turned to point sideways and spun round: its jet sweeps round the character.", [height(1), turns(0.75), copies(1)], (ctx, base, p) => {
      const node = spinner(ctx, "spin", p.height, wholeTurns(p.turns, ctx.length));
      return { nodes: [node], emitters: spaced(ctx, base, p.count, (e, k) => {
        e.parent = node.name;
        e.position = [0, 0, 0];
        // Its z (where particles go) laid down along x, then turned round to its place in the ring.
        e.orientation = axisAngle(qmul(qaxis([0, 0, 1], (TAU * k) / p.count), qaxis([0, 1, 0], Math.PI / 2)));
      }) };
    }),
    motion("wander", "Wander", "Drifts round a loose path near the character.", [height(1.2), radius(1.2),
      { key: "seed", label: "Path (another number, another path)", min: 1, max: 99, step: 1, value: 7 }], (ctx, base, p) => {
      // Eight points from the seed (the same seed, the same path), joined smoothly and closed.
      let s = Math.max(1, Math.round(p.seed)) * 2654435761 % 4294967296;
      const next = () => { s = (s * 1664525 + 1013904223) % 4294967296; return s / 4294967296; };
      const pts = [];
      for (let i = 0; i < 8; i++) {
        const a = (TAU * i) / 8 + (next() - 0.5) * 0.6, r = p.radius * (0.4 + 0.6 * next());
        pts.push([r * Math.cos(a), r * Math.sin(a), p.height + (next() - 0.5) * 0.6]);
      }
      const at = (u) => {  // Catmull-Rom through the closed loop
        const f = u * 8, i = Math.floor(f) % 8, t = f - Math.floor(f);
        const P = (k) => pts[(i + k + 8) % 8];
        return [0, 1, 2].map((c) => 0.5 * ((2 * P(0)[c]) + (-P(-1)[c] + P(1)[c]) * t + (2 * P(-1)[c] - 5 * P(0)[c] + 4 * P(1)[c] - P(2)[c]) * t * t
          + (-P(-1)[c] + 3 * P(0)[c] - 3 * P(1)[c] + P(2)[c]) * t * t * t));
      };
      return { nodes: [], emitters: spaced(ctx, base, 1, (e) => {
        e.parent = "";
        e.position = at(0);
        e.keys = [{ anim: ctx.anim, orientation: [], position: moveKeys(ctx.length, 48, at) }];
      }) };
    }),
  ];

  // --- composing ---------------------------------------------------------------------------
  function loopAnim(fx) { return fx.duration ? "duration" : "impact"; }
  function loopLength(fx) {
    const l = fx.lengths || [0, 2, 1];
    return fx.duration ? (l[1] > 0 ? l[1] : 2) : Math.max(0.2, l[0] > 0 ? l[0] : fx.seconds || 1);
  }

  // A preset's settings with the given values over its own (numbers clamped to their range).
  function settings(defs, given) {
    const out = {};
    for (const d of defs) {
      let v = given && Object.prototype.hasOwnProperty.call(given, d.key) ? given[d.key] : d.value;
      if (d.kind === "colour") v = Array.isArray(v) && v.length === 3 && v.every(Number.isFinite) ? v.map((c) => Math.max(0, Math.min(1, c))) : d.value;
      else {
        v = Number(v);
        if (!Number.isFinite(v)) v = d.value;
        if (d.min !== undefined) v = Math.max(d.min, Math.min(d.max, v));
      }
      out[d.key] = v;
    }
    return out;
  }
  // A particle's settings start from its own colours, size and opacity.
  function particleParams(part) {
    const f = part.fields;
    return PARTICLE_PARAMS.map((d) => Object.assign({}, d, { value: {
      colour: (f.color_start || [1, 1, 1]).slice(), fade: (f.color_end || f.color_start || [1, 1, 1]).slice(),
      size: f.size_start !== undefined ? f.size_start : 0.1, opacity: f.alpha_start !== undefined ? f.alpha_start : 1,
    }[d.key] }));
  }
  // Your own emissions: how many, how fast, how long, from what they were kept with.
  function emissionParams(part) {
    if (part.params) return part.params;
    const f = part.fields;
    return [rate(f.birthrate !== undefined ? f.birthrate : 10), speed(f.velocity !== undefined ? f.velocity : 1), life(f.life !== undefined ? f.life : 1)];
  }
  function paramsOf(kind, part) {
    return kind === "particle" ? particleParams(part) : kind === "emission" ? emissionParams(part) : part.params;
  }

  function defaultsOf(keys) {
    const d = core.emitter();
    const out = {};
    for (const k of keys) out[k] = core.clone(d[k]);
    return out;
  }

  // One side on an emitter: its fields back to the defaults first, then the part's (nothing of
  // a look or an emission before it stays behind).
  function applyParticle(e, part, given) {
    const p = settings(particleParams(part), given), f = part.fields;
    Object.assign(e, defaultsOf(PARTICLE_KEYS), core.clone(f));
    e.color_start = p.colour.slice();
    e.color_end = p.fade.slice();
    const k = f.size_start > 0 ? p.size / f.size_start : 1;
    e.size_start = p.size;
    e.size_end = (f.size_end !== undefined ? f.size_end : f.size_start) * k;
    const o = f.alpha_start > 0 ? p.opacity / f.alpha_start : 1;
    e.alpha_start = p.opacity;
    e.alpha_end = Math.max(0, Math.min(1, (f.alpha_end !== undefined ? f.alpha_end : 0) * o));
    if (String(e.update).toLowerCase() === "lightning") e.render = "Linked";
    return e;
  }
  // Returns where its target goes (under the emitter), or null.
  function applyEmission(e, part, given) {
    const defs = emissionParams(part), p = settings(defs, given);
    Object.assign(e, defaultsOf(EMISSION_KEYS), core.clone(part.fields));
    for (const d of defs) {
      if (d.field) e[d.field] = p[d.key];
      else if (d.set) d.set(e, p[d.key]);
    }
    if (String(e.update).toLowerCase() === "lightning") e.render = "Linked";  // the game's bolts are all Linked
    if (part.target) return typeof part.target === "function" ? part.target(p) : part.target.slice();
    return null;
  }

  function find(kind, id, own) {
    const list = kind === "particle" ? PARTICLES : kind === "emission" ? EMISSIONS : MOTIONS;
    if (String(id).startsWith("yours:")) {
      const name = String(id).slice(6);
      return (own || []).find((x) => x.kind === kind && x.name === name) || null;
    }
    return list.find((x) => x.id === id) || null;
  }

  // Names unique in the effect (nodes and emitters share them).
  function namer(fx) {
    const taken = new Set([...(fx.nodes || []).map((n) => n.name), ...fx.emitters.map((e) => e.name)]);
    return (wanted) => {
      let name = core.resref(wanted, 24) || "part";
      for (let k = 2; taken.has(name); k++) name = core.resref(wanted, 21) + k;
      taken.add(name);
      return name;
    };
  }

  // The mix as a layer for `fx`: { emitters, nodes } (fx itself isn't changed).
  // mix: { particle: { id, params }, emission: { id, params }, motion: { id, params } }.
  function compose(fx, mix, own) {
    const particle = find("particle", mix.particle.id, own), emission = find("emission", mix.emission.id, own), move = find("motion", mix.motion.id);
    if (!particle || !emission || !move) throw new Error("a part of the mix isn't there any more");
    const unique = namer(fx);
    const baseName = (particle.id || particle.name) + "_" + (emission.id || emission.name);
    const base = core.emitter({ name: unique(baseName) });
    applyParticle(base, particle, mix.particle.params);
    const target = applyEmission(base, emission, mix.emission.params);  // its fields and the particle's don't overlap
    const ctx = { fx, anim: loopAnim(fx), length: loopLength(fx), unique, base: base.name };
    const layer = move.build(ctx, base, settings(move.params, mix.motion.params));
    if (target) {
      for (const e of layer.emitters) layer.nodes.push(core.node({ name: unique(e.name + "_target"), parent: e.name, position: target.slice() }));
    }
    return layer;
  }

  // A motion on an emitter that's there: it's placed (and keyed) as the motion's single copy
  // would be; returns the motion's new nodes to add (what hangs under it stays under it).
  function applyMotion(fx, e, part, given) {
    const p = settings(part.params, given);
    if (p.count !== undefined) p.count = 1;
    const ctx = { fx, anim: loopAnim(fx), length: loopLength(fx), unique: namer(fx), base: e.name };
    const layer = part.build(ctx, Object.assign(core.clone(e), { keys: [], orientation: [0, 0, 0, 0] }), p);
    const built = layer.emitters[0];
    e.parent = built.parent;
    e.position = built.position;
    e.orientation = built.orientation;
    e.keys = built.keys;
    return layer.nodes;
  }

  // Your own part kept from an emitter: one side's fields, and an emission's target.
  function partFrom(kind, e, name, fx) {
    const keys = kind === "particle" ? PARTICLE_KEYS : EMISSION_KEYS;
    const fields = {};
    for (const k of keys) fields[k] = core.clone(e[k]);
    const out = { kind, name: core.resref(name, 24), fields };
    if (kind === "emission" && fx) {
      const t = (fx.nodes || []).find((n) => n.parent === e.name);
      if (t) out.target = t.position.slice();
    }
    return out;
  }

  // Your own part as read from a file or a request: only its side's fields, each of the type
  // the emitter has (strings: letters, digits, _ - the names of textures, renders...).
  function cleanPart(raw) {
    if (!raw || typeof raw !== "object") throw new Error("not a part");
    const kind = raw.kind;
    if (kind !== "particle" && kind !== "emission") throw new Error("a part is a particle or an emission");
    const name = core.resref(raw.name || "", 24);
    if (!name) throw new Error("a part needs a name (letters, digits, _)");
    const keys = kind === "particle" ? PARTICLE_KEYS : EMISSION_KEYS, d = core.emitter(), fields = {};
    for (const k of keys) {
      if (!raw.fields || !Object.prototype.hasOwnProperty.call(raw.fields, k)) continue;
      const v = raw.fields[k], want = d[k];
      if (typeof want === "number" && typeof v === "number" && Number.isFinite(v)) fields[k] = v;
      else if (typeof want === "boolean" && typeof v === "boolean") fields[k] = v;
      else if (typeof want === "string" && typeof v === "string" && /^[A-Za-z0-9_-]{0,64}$/.test(v)) fields[k] = v;
      else if (Array.isArray(want) && Array.isArray(v) && v.length === want.length && v.every((x) => typeof x === "number" && Number.isFinite(x))) fields[k] = v.slice();
    }
    const out = { kind, name, fields };
    if (kind === "emission" && Array.isArray(raw.target) && raw.target.length === 3 && raw.target.every((x) => typeof x === "number" && Number.isFinite(x))) {
      out.target = raw.target.slice();
    }
    return out;
  }

  const api = { PARTICLES, EMISSIONS, MOTIONS, PARTICLE_KEYS, EMISSION_KEYS, turnKeys, moveKeys, loopAnim, loopLength,
    settings, paramsOf, find, namer, compose, applyParticle, applyEmission, applyMotion, partFrom, cleanPart };
  if (typeof module === "object" && module.exports) module.exports = api;
  else root.VfxParts = api;
})(typeof self !== "undefined" ? self : this);
