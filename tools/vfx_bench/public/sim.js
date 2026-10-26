// VFX Bench - the preview's particles: NWN's emitters as the game's models describe
// them, approximated (the game itself is one click away: Play in game). Metres, z up,
// the character standing at the origin.
//
// Per emitter: Fountain emits `birthrate` a second, Single keeps one particle at the
// emitter, Explosion bursts `birthrate` at once when the effect starts (the impact's
// detonate), Lightning isn't drawn. An emitter is on while the effect plays
// (`seconds`) or, with `emit_seconds`, that long - as nwn5e writes the model. Particles
// leave in a cone round the emitter's z (`spread`, its full angle) at `velocity` plus up
// to `randvel`, fall with `mass` (x 9.8 m/s2), start in an `xsize` x `ysize` cm area,
// and go from the start size / colour / alpha to the end ones over `life` (-1: for
// ever). `inherit`: they move with the character; `bounce` / `splat` on the ground.
(function (root) {
  "use strict";
  const GRAVITY = 9.8;
  const MAX_PARTICLES = 6000;  // an emitter's, in the preview
  // The effect nodes on the game's human male (pmh0, measured 0.42.1): the root at
  // the feet, impact on the chest, headconjure (the visualeffects head column) some
  // 0.4 m over the head - where the game's own head effects sit (their emitters at 0).
  const ATTACH_HEIGHT = { root: 0, impact: 1.47, head: 2.4 };

  function lerp(a, b, t) { return a + (b - a) * t; }

  class Sim {
    constructor() {
      this.fx = null;
      this.time = 0;
      this.states = [];  // per emitter: { particles, carry, burst, next }
      this.walk = false;
      this.hidden = new Set();
      this.serial = 0;
    }

    setEffect(fx) {
      this.fx = fx;
      while (this.states.length < fx.emitters.length) this.states.push({ particles: [], carry: 0, burst: false });
      this.states.length = fx.emitters.length;
    }

    restart() {
      this.time = 0;
      for (const s of this.states) { s.particles = []; s.carry = 0; s.burst = false; }
    }

    // The character's position (it walks a slow figure of eight with `walk`).
    figure(t) {
      if (!this.walk) return { x: 0, y: 0, heading: 0 };
      const w = 0.45;
      return { x: 2.2 * Math.sin(t * w), y: 1.2 * Math.sin(2 * t * w), heading: Math.atan2(2.4 * w * Math.cos(2 * t * w), 2.2 * w * Math.cos(t * w)) };
    }

    node(t) {
      const f = this.figure(t);
      return [f.x, f.y, ATTACH_HEIGHT[this.fx.attach] || 0];
    }

    // How long an emitter is on: its emit_seconds, else the effect's seconds.
    onFor(e) { return e.emit_seconds > 0 ? e.emit_seconds : Math.max(0.1, this.fx.seconds); }

    // One play: until every emitter is off and its particles gone (or a cap).
    length() {
      if (!this.fx) return 1;
      let end = 0.5;
      for (const e of this.fx.emitters) end = Math.max(end, this.onFor(e) + (e.life > 0 ? e.life : 0));
      return Math.min(end + 0.6, 60);
    }

    spawn(e, state, node) {
      if (state.particles.length >= MAX_PARTICLES) return;
      const local = [
        e.position[0] + (Math.random() - 0.5) * e.xsize / 100,
        e.position[1] + (Math.random() - 0.5) * e.ysize / 100,
        e.position[2],
      ];
      let vel = [0, 0, 0];
      if (e.update.toLowerCase() !== "single") {
        const half = Math.min(Math.PI, Math.max(0, e.spread) / 2);
        const cos = 1 - Math.random() * (1 - Math.cos(half));
        const sin = Math.sqrt(Math.max(0, 1 - cos * cos));
        const phi = Math.random() * 2 * Math.PI;
        const speed = e.velocity + Math.random() * e.randvel;
        vel = [sin * Math.cos(phi) * speed, sin * Math.sin(phi) * speed, cos * speed];
      }
      const frames = Math.max(0, e.frame_end) + 1;
      state.particles.push({
        pos: e.inherit ? local : [node[0] + local[0], node[1] + local[1], node[2] + local[2]],
        vel, age: 0, life: e.life, flat: false,
        frame: e.random ? Math.floor(Math.random() * frames) : -1,
        id: ++this.serial,
      });
    }

    step(dt) {
      if (!this.fx) return;
      const t0 = this.time, t1 = this.time + dt;
      this.time = t1;
      const node = this.node(t1);
      this.fx.emitters.forEach((e, i) => {
        const state = this.states[i];
        if (!state) return;
        const update = String(e.update).toLowerCase();
        const on = t1 <= this.onFor(e);
        if (update === "fountain" && on) {
          state.carry += Math.max(0, e.birthrate) * dt;
          while (state.carry >= 1) { state.carry -= 1; this.spawn(e, state, node); }
        } else if (update === "explosion" && !state.burst) {
          state.burst = true;
          const count = Math.round(Math.max(0, e.birthrate));
          for (let k = 0; k < count; k++) this.spawn(e, state, node);
        } else if (update === "single") {
          if (!on) state.particles = [];
          else if (!state.particles.length) this.spawn(e, state, node);
        }
        const g = e.mass * GRAVITY;
        const ground = e.inherit ? -node[2] : 0;
        const kept = [];
        for (const p of state.particles) {
          p.age += dt;
          if (p.life > 0 && p.age >= p.life) {
            if (update === "single" && on) { p.age = 0; kept.push(p); }
            continue;
          }
          if (!p.flat) {
            p.vel[2] -= g * dt;
            p.pos[0] += p.vel[0] * dt;
            p.pos[1] += p.vel[1] * dt;
            p.pos[2] += p.vel[2] * dt;
            if (p.pos[2] < ground && (e.splat || e.bounce)) {
              if (e.splat) {
                p.pos[2] = ground + 0.005;
                p.vel = [0, 0, 0];
                p.flat = true;
              } else {
                p.pos[2] = ground + (ground - p.pos[2]);
                p.vel[2] = -p.vel[2] * Math.max(0, e.bounce_co);
              }
            }
          }
          kept.push(p);
        }
        state.particles = kept;
      });
      void t0;
    }

    // What to draw: per emitter, its particles' world position, velocity, size, colour,
    // alpha and sheet frame.
    snapshot() {
      if (!this.fx) return [];
      const node = this.node(this.time);
      return this.fx.emitters.map((e, i) => {
        const state = this.states[i];
        if (!state || this.hidden.has(i)) return { emitter: e, particles: [] };
        const frames = Math.max(0, e.frame_end) + 1;
        const particles = state.particles.map((p) => {
          const k = p.life > 0 ? Math.min(1, p.age / p.life) : 0;
          const pos = e.inherit ? [node[0] + p.pos[0], node[1] + p.pos[1], node[2] + p.pos[2]] : p.pos;
          return {
            pos, vel: p.vel, flat: p.flat, id: p.id,
            size: Math.max(0, lerp(e.size_start, e.size_end, k)),
            color: [lerp(e.color_start[0], e.color_end[0], k), lerp(e.color_start[1], e.color_end[1], k),
              lerp(e.color_start[2], e.color_end[2], k), Math.max(0, Math.min(1, lerp(e.alpha_start, e.alpha_end, k)))],
            frame: p.frame >= 0 ? p.frame : Math.min(frames - 1, Math.floor(k * frames)),
          };
        });
        return { emitter: e, particles };
      });
    }

    count() { return this.states.reduce((n, s, i) => n + (this.hidden.has(i) ? 0 : s.particles.length), 0); }
  }

  const api = { Sim, ATTACH_HEIGHT };
  if (typeof module === "object" && module.exports) module.exports = api;
  else root.VfxSim = api;
})(typeof self !== "undefined" ? self : this);
