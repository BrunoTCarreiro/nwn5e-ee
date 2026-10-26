// VFX Bench - the preview's particles, run the way the game's own code runs them (0.42.2;
// owner, 2026-10-26: "reading the game's code would be the most straightforward way" - read
// in nwmain EE 89.8193.37: FountainEmitter / ExplosionEmitter / SingleEmitter::Update,
// PartEmitter::chkParticleLife / chkRandom / moveParticleStandard, Particle::initialize /
// randomDirection / randomPosition; RVAs in README.md). Metres, z up, the character at the
// origin. What the draft doesn't hold (an emitter's tilt, its node's motion, particle spin,
// mid colours) isn't here either - and isn't in what Play in game makes.
(function (root) {
  "use strict";
  const GRAVITY = 9.81;          // moveParticleStandard: vel.z -= mass * 9.81 * dt
  const MAX_PARTICLES = 6000;    // an emitter's, in the preview
  // The effect nodes on the game's human male (pmh0, measured 0.42.1): the root at the feet,
  // impact on the chest, headconjure (the visualeffects head column) some 0.4 m over the head.
  const ATTACH_HEIGHT = { root: 0, impact: 1.47, head: 2.4 };
  const rand = () => Math.floor(Math.random() * 32768);  // the C runtime's rand(), 0..32767

  // An emitter's turn (axis x y z, angle) applied to a vector - its particles leave along its
  // z, its area lies in its x / y (Particle::initialize: the emitter's orientation).
  function turn(o, v) {
    const len = o ? Math.hypot(o[0], o[1], o[2]) : 0;
    if (!len || !o[3]) return v;
    const s = Math.sin(o[3] / 2) / len;
    const q = [o[0] * s, o[1] * s, o[2] * s, Math.cos(o[3] / 2)];
    // v + 2w (q x v) + 2 q x (q x v)
    const c1 = [q[1] * v[2] - q[2] * v[1], q[2] * v[0] - q[0] * v[2], q[0] * v[1] - q[1] * v[0]];
    const c2 = [q[1] * c1[2] - q[2] * c1[1], q[2] * c1[0] - q[0] * c1[2], q[0] * c1[1] - q[1] * c1[0]];
    return [v[0] + 2 * (q[3] * c1[0] + c2[0]), v[1] + 2 * (q[3] * c1[1] + c2[1]), v[2] + 2 * (q[3] * c1[2] + c2[2])];
  }

  class Sim {
    constructor() {
      this.fx = null;
      this.time = 0;
      this.states = [];  // per emitter: { particles, acc, burst }
      this.walk = false;
      this.hidden = new Set();
      this.serial = 0;
    }

    setEffect(fx) {
      this.fx = fx;
      while (this.states.length < fx.emitters.length) this.states.push({ particles: [], acc: 0, burst: false });
      this.states.length = fx.emitters.length;
    }

    restart() {
      this.time = 0;
      for (const s of this.states) { s.particles = []; s.acc = 0; s.burst = false; }
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

    // How long an emitter is on - as nwn5e writes the model: its emit_seconds (the impact keys
    // its birthrate off after it), else the effect's seconds (D: while applied; F: keyed too).
    onFor(e) { return e.emit_seconds > 0 ? e.emit_seconds : Math.max(0.1, this.fx.seconds); }

    // The emitter's birthrate at time t (its keyed value: 0 once it's off).
    birthrate(e, t) { return t <= this.onFor(e) ? e.birthrate : 0; }

    // One play: until every emitter is off and its particles gone (or a cap).
    length() {
      if (!this.fx) return 1;
      let end = 0.5;
      for (const e of this.fx.emitters) end = Math.max(end, this.onFor(e) + (e.life > 0 ? e.life : 0));
      return Math.min(end + 0.6, 60);
    }

    // Particle::initialize (+ randomPosition, randomDirection, PartEmitter::chkRandom).
    spawn(e, state, node) {
      if (state.particles.length >= MAX_PARTICLES) return;
      // Speed: velocity plus up to randvel either way, in 0.01 steps.
      let delta = 0;
      if (e.randvel > 0.01) {
        const span = Math.trunc(e.randvel * 100);
        delta = span > 0 ? rand() % span : 0;
        if (rand() % 2) delta = -delta;
      }
      // Where: the emitter, or a point of its xsize x ysize (cm) area.
      const local = [e.position[0], e.position[1], e.position[2]];
      const xs = Math.trunc(e.xsize * 100), ys = Math.trunc(e.ysize * 100);
      if (xs || ys) {
        let a = xs > 3 ? rand() % Math.trunc(xs * 0.5) : 0;
        let b = ys > 3 ? rand() % Math.trunc(ys * 0.5) : 0;
        if (rand() % 2) a = -a;
        if (rand() % 2) b = -b;
        const offset = turn(e.orientation, [a * 0.0001, b * 0.0001, 0]);
        for (let i = 0; i < 3; i++) local[i] += offset[i];
      }
      // Which way: the emitter's z, tilted by up to half the spread (an even ANGLE, 0.01 steps)
      // and turned round it (0 - 6.27).
      let dir = [0, 0, 1], xaxis = [1, 0, 0];
      if (e.spread !== 0) {
        const tilt = (rand() % (1 + Math.trunc(e.spread * 0.5 * 100))) * 0.01;
        const round = (rand() % 628) * 0.01;
        dir = [Math.sin(tilt) * Math.sin(round), -Math.sin(tilt) * Math.cos(round), Math.cos(tilt)];
        xaxis = [Math.cos(round), Math.sin(round), 0];
      }
      // Its own frame (Particle +0x34): z = the way it left, x = its spawn turn's x - what
      // Aligned_to_Particle_Dir draws on.
      dir = turn(e.orientation, dir);
      xaxis = turn(e.orientation, xaxis);
      const speed = e.velocity + delta * 0.01;
      let frame = 0;
      if (e.random) {
        const span = Math.abs(Math.trunc(e.frame_end) + 1) || 1;
        frame = rand() % span;
      }
      const pos = e.inherit ? local : [node[0] + local[0], node[1] + local[1], node[2] + local[2]];
      state.particles.push({
        pos, trail: pos.slice(), vel: [dir[0] * speed, dir[1] * speed, dir[2] * speed],
        age: 0, frame, lastFrameAge: 0, spin: 0, dir, xaxis, stopped: false, id: ++this.serial,
      });
    }

    // PartEmitter::chkParticleLife: gone once its age reaches lifeExp (checked before it ages);
    // lifeExp below 0: for ever.
    ageParticles(e, state, dt) {
      const kept = [];
      for (const p of state.particles) {
        if (e.life >= 0 && p.age >= e.life) continue;
        p.age = Math.max(0, p.age + dt);
        kept.push(p);
      }
      state.particles = kept;
    }

    // Motion_Blur's tail: each frame it closes min(dt, blurlength) / blurlength of the way
    // to the particle (so it lags ~blurlength seconds behind).
    trail(e, state, dt) {
      if (e.render !== "Motion_Blur") return;  // the game matches the name exactly
      const k = e.blur_length > 0 ? Math.min(dt, e.blur_length) / e.blur_length : 1;
      for (const p of state.particles) {
        for (let i = 0; i < 3; i++) p.trail[i] = p.trail[i] * (1 - k) + p.pos[i] * k;
      }
    }

    // A particle's spin: particleRot radians a second, kept within a turn either way.
    spin(e, state, dt) {
      const rate = e.particle_rot || 0;
      if (!rate) return;
      for (const p of state.particles) {
        p.spin += dt * rate;
        if (p.spin > 2 * Math.PI) p.spin -= 2 * Math.PI;
        else if (p.spin < -2 * Math.PI) p.spin += 2 * Math.PI;
      }
    }

    // PartEmitter::moveParticleStandard: dt x velocity, gravity mass x 9.81; with bounce, the
    // ground (z 0) stops it (bounce_co 0) or throws it back (x 0.75, its rise x bounce_co) and
    // it rests once slow (|v|^2 < 0.5), 2 cm over the ground (splat: lying flat on it).
    move(e, state, dt, node) {
      const ground = e.inherit ? -node[2] : 0;
      for (const p of state.particles) {
        if (p.stopped) continue;
        const before = p.pos[2];
        p.pos[0] += p.vel[0] * dt;
        p.pos[1] += p.vel[1] * dt;
        p.pos[2] += p.vel[2] * dt;
        p.vel[2] -= e.mass * GRAVITY * dt;
        if (!e.bounce || !(p.pos[2] < ground && before >= ground)) continue;
        const k = (before - ground) / (before - p.pos[2] || 1);
        for (let i = 0; i < 2; i++) p.pos[i] -= p.vel[i] * dt * (1 - k);
        p.pos[2] = ground;
        if (e.bounce_co === 0) {
          p.vel = [0, 0, 0];
        } else {
          p.vel = [p.vel[0] * 0.75, p.vel[1] * 0.75, -p.vel[2] * 0.75];
          p.vel[2] *= e.bounce_co * Math.max(0.5, Math.min(1, 0.033 / dt));
        }
        if (p.vel[0] ** 2 + p.vel[1] ** 2 + p.vel[2] ** 2 < 0.5) {
          p.stopped = true;
          p.pos[2] = ground + 0.02;
          p.dir = [0, 0, 1];   // turned to the ground's normal (RotationArc(z, n))
          p.xaxis = [1, 0, 0];
        }
      }
    }

    // One frame (the game runs its emitters once a frame; dt at most 0.5 s).
    step(dt) {
      if (!this.fx) return;
      dt = Math.min(dt, 0.5);
      this.time += dt;
      const t = this.time;
      const node = this.node(t);
      this.fx.emitters.forEach((e, i) => {
        const state = this.states[i];
        if (!state) return;
        const update = String(e.update).toLowerCase();
        const birth = this.birthrate(e, t);
        if (update === "single") {
          if (Math.trunc(birth) === 0) { state.particles = []; return; }
          if (!state.particles.length) { this.spawn(e, state, node); return; }
          state.particles[0].age += dt;  // never dies by age; loop isn't written (0)
          this.spin(e, state, dt);
          this.trail(e, state, dt);
          this.move(e, state, dt, node);
          return;
        }
        if (update !== "fountain" && update !== "explosion") return;  // Lightning: not drawn here
        this.ageParticles(e, state, dt);
        this.trail(e, state, dt);
        this.spin(e, state, dt);
        this.move(e, state, dt, node);
        const n = Math.trunc(birth);
        if (update === "explosion") {
          // The impact's detonate event at the start: int(birthrate) at once.
          if (!state.burst) {
            state.burst = true;
            for (let k = 0; k < n; k++) this.spawn(e, state, node);
          }
          return;
        }
        // Fountain: time adds up; past 1 / birthrate, int(time x birthrate) are born and the
        // rest of the time is dropped (at 60 fps a rate of 100 makes 60 a second).
        if (n <= 0) return;
        state.acc += dt;
        if (state.acc >= 1 / birth) {
          const count = Math.trunc(state.acc * birth) % (n + 1);
          for (let k = 0; k < count; k++) this.spawn(e, state, node);
          state.acc = 0;
        }
      });
    }

    // What PartEmitter::RenderEmitter draws, per emitter: each particle's half size, colour
    // and alpha (8-bit, as the game packs them) and sheet frame. t = min(1, age / lifeExp) -
    // a straight start -> end (the mid values and percents do nothing); lifeExp 0: nothing
    // drawn, below 0: the start values. Splat: sizeStart in flight, sizeEnd once it rests.
    // The frame steps by whole frames at fps while drawn (the rest of the time dropped) and
    // wraps to the first; fps 0 (or a one-frame range): it stays.
    snapshot() {
      if (!this.fx) return [];
      const node = this.node(this.time);
      const clamp01 = (v) => Math.max(0, Math.min(1, v));
      return this.fx.emitters.map((e, i) => {
        const state = this.states[i];
        const grid = Math.max(1, e.grid | 0);
        if (!state || this.hidden.has(i) || e.life === 0) return { emitter: e, particles: [], grid };
        const life = e.life;
        const hs0 = e.size_start * 0.5, hs1 = e.size_end * 0.5;
        const sizeVaries = life > 0 && e.size_start !== e.size_end && !e.splat;
        const cS = e.color_start.map((v) => clamp01(v) * 255.99), cE = e.color_end.map((v) => clamp01(v) * 255.99);
        const colorVaries = life > 0 && e.color_start.some((v, k) => v !== e.color_end[k]);
        const aS = clamp01(e.alpha_start) * 255.99, aE = clamp01(e.alpha_end) * 255.99;
        const alphaVaries = life > 0 && e.alpha_start !== e.alpha_end;
        const spf = e.fps > 0 ? 1 / e.fps : 0;
        const frameEnd = Math.trunc(e.frame_end);
        const world = (v) => (e.inherit ? [node[0] + v[0], node[1] + v[1], node[2] + v[2]] : v);
        const particles = state.particles.map((p) => {
          const t = life > 0 ? Math.min(1, p.age / life) : 0;
          let w = sizeVaries ? hs0 + (hs1 - hs0) * t : hs0;
          if (e.splat && p.stopped) w = hs1;
          if (grid > 1 && spf !== 0 && frameEnd !== 0) {
            const since = p.age - p.lastFrameAge;
            if (since >= spf) {
              p.frame += Math.trunc(since / spf);
              if (p.frame > frameEnd) p.frame = 0;
              p.lastFrameAge = p.age;
            }
          }
          const rgb = [0, 1, 2].map((k) => Math.trunc(colorVaries ? t * cE[k] + (1 - t) * cS[k] : cS[k]) / 255);
          const alpha = alphaVaries ? t * aE + (1 - t) * aS : aS;
          return {
            pos: world(p.pos), trail: world(p.trail), w, h: w, color: [...rgb, Math.trunc(alpha) / 255], alpha255: alpha,
            frame: grid > 1 ? p.frame : -1, spin: p.spin, dir: p.dir, xaxis: p.xaxis, stopped: p.stopped,
          };
        });
        return { emitter: e, particles, grid };
      });
    }

    count() { return this.states.reduce((n, s, i) => n + (this.hidden.has(i) ? 0 : s.particles.length), 0); }
  }

  const api = { Sim, ATTACH_HEIGHT, turn };
  if (typeof module === "object" && module.exports) module.exports = api;
  else root.VfxSim = api;
})(typeof self !== "undefined" ? self : this);
