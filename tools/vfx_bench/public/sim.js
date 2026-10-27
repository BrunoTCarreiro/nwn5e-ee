// VFX Bench - the preview's particles, run the way the game's own code runs them (0.42.2;
// owner, 2026-10-26: "reading the game's code would be the most straightforward way" - read
// in nwmain EE 89.8193.37: FountainEmitter / ExplosionEmitter / SingleEmitter::Update,
// PartEmitter::AnimateParticles / chkParticleLife / chkRandom / moveParticleStandard,
// Particle::initialize / randomDirection / randomPosition; PARTICLES.md). Metres, z up, the
// character at the origin. 0.42.3 ("Go for A"): the effect's nodes and emitters move and turn
// by their keys - the impact once, the duration looping while applied, the cessation as it's
// removed (MdlNode::InternalControl: linear positions, short-way slerped turns).
(function (root) {
  "use strict";
  const GRAVITY = 9.81;          // moveParticleStandard: vel.z -= mass * 9.81 * dt
  const MAX_PARTICLES = 6000;    // an emitter's, in the preview
  // The effect nodes on the game's human male (pmh0, measured 0.42.1): the root at the feet,
  // impact on the chest, headconjure (the visualeffects head column) some 0.4 m over the head.
  const ATTACH_HEIGHT = { root: 0, impact: 1.47, head: 2.4 };
  const rand = () => Math.floor(Math.random() * 32768);  // the C runtime's rand(), 0..32767

  // --- quaternions (x y z w) ------------------------------------------------------------
  function qmul(a, b) {
    return [a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
      a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]];
  }
  function qrot(q, v) {
    const c1 = [q[1] * v[2] - q[2] * v[1], q[2] * v[0] - q[0] * v[2], q[0] * v[1] - q[1] * v[0]];
    const c2 = [q[1] * c1[2] - q[2] * c1[1], q[2] * c1[0] - q[0] * c1[2], q[0] * c1[1] - q[1] * c1[0]];
    return [v[0] + 2 * (q[3] * c1[0] + c2[0]), v[1] + 2 * (q[3] * c1[1] + c2[1]), v[2] + 2 * (q[3] * c1[2] + c2[2])];
  }
  const qconj = (q) => [-q[0], -q[1], -q[2], q[3]];
  // Axis-angle (as the drafts and ASCII models keep it) to a quaternion; a zero axis is a turn
  // of 0 or a whole turn round (w -1: the same turn, its sign kept).
  function quat(o) {
    if (!o) return [0, 0, 0, 1];
    const len = Math.hypot(o[0], o[1], o[2]), half = (o[3] || 0) / 2;
    if (!len) return [0, 0, 0, Math.cos(half) < 0 ? -1 : 1];
    const s = Math.sin(half) / len;
    return [o[0] * s, o[1] * s, o[2] * s, Math.cos(half)];
  }
  // The game's slerp: the first quaternion turned round when they're more than a half apart.
  function slerp(a, b, t) {
    let d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (d < 0) { a = [-a[0], -a[1], -a[2], -a[3]]; d = -d; }
    if (d > 0.9995) {
      const r = [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t, a[3] + (b[3] - a[3]) * t];
      const l = Math.hypot(...r) || 1;
      return r.map((x) => x / l);
    }
    const th = Math.acos(d), s = Math.sin(th), wa = Math.sin((1 - t) * th) / s, wb = Math.sin(t * th) / s;
    return [a[0] * wa + b[0] * wb, a[1] * wa + b[1] * wb, a[2] * wa + b[2] * wb, a[3] * wa + b[3] * wb];
  }
  // Particle::randomDirection's deadspace (0x68a25-0x68d0f, read 0.42.4): a particle whose own y
  // (the normal of an Aligned_to_Particle_Dir sheet) is within D of square to the line from the
  // camera is turned round the axis square to both: by cos - pi/2 + D the first time (cos + pi/2
  // + D when the cos is below 0), and while it's still within, again from where it started by
  // |cos| - pi/2 + D. Its z (the way it leaves) turns with it. The game loops until that ends -
  // above about 0.39 it never does for some (drafts hold it there); here at most 64 passes.
  function deadspace(D, dir, xaxis, pos, eye) {
    const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
    const unit = (v) => { const l = Math.hypot(v[0], v[1], v[2]); return l ? [v[0] / l, v[1] / l, v[2] / l] : null; };
    const V = eye && unit([pos[0] - eye[0], pos[1] - eye[1], pos[2] - eye[2]]);
    if (!V) return [dir, xaxis];
    const Y = cross(dir, xaxis);
    const limit = Math.cos(Math.PI / 2 - D);
    let d = dot(V, Y);
    if (!(limit > Math.abs(d))) return [dir, xaxis];
    const n = unit(cross(V, Y));
    if (!n) return [dir, xaxis];
    let out = [dir, xaxis];
    for (let pass = 0; pass < 64; pass++) {
      const a = d < 0 ? D + Math.PI / 2 - d : d - Math.PI / 2 + D, s = Math.sin(a / 2);
      const r = [n[0] * s, n[1] * s, n[2] * s, Math.cos(a / 2)];
      out = [qrot(r, dir), qrot(r, xaxis)];
      d = Math.abs(dot(V, qrot(r, Y)));
      if (!(limit > d)) break;
    }
    return out;
  }

  // RotationArc (0x22e1c0): the shortest turn taking direction u to direction v (both made unit;
  // opposite: a half turn round x).
  function arc(u, v) {
    const lu = Math.hypot(u[0], u[1], u[2]), lv = Math.hypot(v[0], v[1], v[2]);
    const a = [u[0] / lu, u[1] / lu, u[2] / lu], b = [v[0] / lv, v[1] / lv, v[2] / lv];
    const d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    if (d <= -1) return [1, 0, 0, 0];
    const s = Math.sqrt((d + 1) * 2);
    return [(a[1] * b[2] - a[2] * b[1]) / s, (a[2] * b[0] - a[0] * b[2]) / s, (a[0] * b[1] - a[1] * b[0]) / s, s / 2];
  }

  // An emitter's own turn applied to a vector (kept for callers of 0.42.2).
  function turn(o, v) { return qrot(quat(o), v); }

  // A key list's value at time t: rows of `width` (t + values); held before the first key and
  // after the last; positions linear, turns slerped.
  function sample(rows, width, t, turns) {
    const n = Math.floor(rows.length / width);
    if (!n) return null;
    const at = (i) => rows.slice(i * width + 1, i * width + width);
    if (n === 1 || t <= rows[0]) return turns ? quat(at(0)) : at(0);
    if (t >= rows[(n - 1) * width]) return turns ? quat(at(n - 1)) : at(n - 1);
    let i = 0;
    while (i + 1 < n && rows[(i + 1) * width] < t) i++;
    const t0 = rows[i * width], t1 = rows[(i + 1) * width], k = t1 > t0 ? (t - t0) / (t1 - t0) : 1;
    if (turns) return slerp(quat(at(i)), quat(at(i + 1)), k);
    const a = at(i), b = at(i + 1);
    return [a[0] + (b[0] - a[0]) * k, a[1] + (b[1] - a[1]) * k, a[2] + (b[2] - a[2]) * k];
  }

  class Sim {
    constructor() {
      this.fx = null;
      this.time = 0;
      this.states = [];  // per emitter: { particles, acc, burst }
      this.walk = false;
      this.hidden = new Set();
      this.serial = 0;
      this.places = null;      // this frame's world place of every node and emitter, by name
      this.prevPlaces = null;  // the frame before's
    }

    setEffect(fx) {
      this.fx = fx;
      while (this.states.length < fx.emitters.length) this.states.push({ particles: [], acc: 0, burst: false });
      this.states.length = fx.emitters.length;
      this.places = this.prevPlaces = null;
    }

    restart() {
      this.time = 0;
      for (const s of this.states) { s.particles = []; s.acc = 0; s.burst = false; }
      this.places = this.prevPlaces = null;
    }

    // The character's position (it walks a slow figure of eight with `walk`).
    figure(t) {
      if (!this.walk) return { x: 0, y: 0, heading: 0 };
      const w = 0.45;
      return { x: 2.2 * Math.sin(t * w), y: 1.2 * Math.sin(2 * t * w), heading: Math.atan2(2.4 * w * Math.cos(2 * t * w), 2.2 * w * Math.cos(t * w)) };
    }

    // How long an emitter is on - as nwn5e writes the model: its emit_seconds (the impact keys
    // its birthrate off after it), else the effect's seconds (D: while applied; F: keyed too).
    onFor(e) { return e.emit_seconds > 0 ? e.emit_seconds : Math.max(0.1, this.fx.seconds); }

    // The emitter's birthrate at time t (its keyed value: 0 once it's off).
    birthrate(e, t) { return t <= this.onFor(e) ? e.birthrate : 0; }

    lastKey(anim) {
      let last = 0;
      const scan = (keys) => {
        for (const k of keys || []) {
          if (k.anim !== anim) continue;
          for (let i = 0; i + 3 < k.position.length; i += 4) last = Math.max(last, k.position[i]);
          for (let i = 0; i + 4 < k.orientation.length; i += 5) last = Math.max(last, k.orientation[i]);
        }
      };
      for (const n of this.fx.nodes || []) scan(n.keys);
      for (const e of this.fx.emitters) scan(e.keys);
      return last;
    }

    // The animations' lengths as nwn5e writes the model (WriteVfxModel).
    anims() {
      const fx = this.fx, l = fx.lengths || [0, 2, 1];
      let impact = 0.2;
      for (const e of fx.emitters) {
        const on = e.emit_seconds > 0 ? e.emit_seconds : (fx.duration ? 0 : fx.seconds);
        impact = Math.max(impact, on + 0.01 + (fx.duration ? 0 : e.life));
      }
      impact = Math.max(impact, l[0], this.lastKey("impact"));
      return { impact, duration: Math.max(l[1] > 0 ? l[1] : 2, this.lastKey("duration")),
        cessation: Math.max(l[2] > 0 ? l[2] : 1, this.lastKey("cessation")) };
    }

    // Which animation plays at time t, and how far into it: the impact, then (D) the duration
    // looping until it's removed (`seconds`), then the cessation.
    animAt(t) {
      const a = this.anims();
      if (!this.fx.duration) return { anim: "impact", time: Math.min(t, a.impact) };
      if (t < Math.min(a.impact, this.fx.seconds)) return { anim: "impact", time: t };
      if (t < this.fx.seconds) return { anim: "duration", time: (t - a.impact) % a.duration };
      return { anim: "cessation", time: Math.min(t - this.fx.seconds, a.cessation) };
    }

    // One play: until it's removed and every emitter's particles are gone (or a cap).
    length() {
      if (!this.fx) return 1;
      const a = this.anims();
      let end = this.fx.duration ? this.fx.seconds + a.cessation : a.impact;
      for (const e of this.fx.emitters) end = Math.max(end, this.onFor(e) + (e.life > 0 ? e.life : 0));
      return Math.min(end + 0.6, 60);
    }

    // Where every node and emitter is at time t (world position and turn): its keys in the
    // animation playing, else the place the model gives it, under its parent's, under the
    // character's node (and facing).
    placesAt(t) {
      const fx = this.fx, f = this.figure(t), { anim, time } = this.animAt(t);
      const base = { pos: [f.x, f.y, ATTACH_HEIGHT[fx.attach] || 0], q: [0, 0, Math.sin(f.heading / 2), Math.cos(f.heading / 2)] };
      const items = new Map();
      for (const n of fx.nodes || []) if (!items.has(n.name)) items.set(n.name, n);
      for (const e of fx.emitters) if (!items.has(e.name)) items.set(e.name, e);
      const places = new Map();
      const place = (item, depth) => {
        if (places.has(item.name)) return places.get(item.name);
        const keys = (item.keys || []).find((k) => k.anim === anim);
        const local = (keys && sample(keys.position, 4, time, false)) || item.position;
        const turnNow = (keys && sample(keys.orientation, 5, time, true)) || quat(item.orientation);
        const parent = item.parent && depth < 32 && items.has(item.parent) && item.parent !== item.name
          ? place(items.get(item.parent), depth + 1) : base;
        const moved = qrot(parent.q, local);
        const p = { pos: [parent.pos[0] + moved[0], parent.pos[1] + moved[1], parent.pos[2] + moved[2]], q: qmul(parent.q, turnNow) };
        places.set(item.name, p);
        return p;
      };
      for (const item of items.values()) place(item, 0);
      places.set("", base);
      return places;
    }

    emitterPlace(e) { return (this.places && this.places.get(e.name)) || { pos: [0, 0, 0], q: [0, 0, 0, 1] }; }

    // A point-to-point emitter's target (0.42.5): the first node under it, where it is now
    // (the game: the emitter's first reference child). None: null (the game would crash).
    targetPlace(e) {
      const n = (this.fx.nodes || []).find((x) => x.parent === e.name);
      return n && this.places ? this.places.get(n.name) || null : null;
    }

    // Particle::randomPosition: a point of the emitter's xsize x ysize (cm) area, in its x / y.
    areaOffset(e) {
      const offset = [0, 0, 0];
      const xs = Math.trunc(e.xsize * 100), ys = Math.trunc(e.ysize * 100);
      if (xs || ys) {
        let a = xs > 3 ? rand() % Math.trunc(xs * 0.5) : 0;
        let b = ys > 3 ? rand() % Math.trunc(ys * 0.5) : 0;
        if (rand() % 2) a = -a;
        if (rand() % 2) b = -b;
        offset[0] = a * 0.0001;
        offset[1] = b * 0.0001;
      }
      return offset;
    }

    // LightningEmitter::Update (0x65290, read 0.42.7): birthrate (whole) points from the emitter
    // to its target - none without one. A new bolt when more than lightning_delay seconds have
    // passed: the first point at the emitter (or a point of its area), the last at the target,
    // the middle by RecursiveFractal, then each inner point pushed along the bolt by up to
    // lightning_scale x half a step. In between, the bolt turns with the emitter -> target line
    // (RotationArc) about the emitter. The points age by dt (life 1 s - the game sets it) and
    // are kept on a spare list when not needed (ages and all); Motion_Blur trails as usual.
    lightning(e, state, dt, t) {
      const n = Math.trunc(this.birthrate(e, t));
      state.timer = (state.timer || 0) + dt;
      state.spare = state.spare || [];
      const target = this.targetPlace(e);
      if (!n || !target) {
        while (state.particles.length) state.spare.push(state.particles.pop());
        return;
      }
      while (state.particles.length < n) {
        if (state.spare.length) state.particles.push(state.spare.pop());
        else this.spawn(e, state);
        if (state.particles.length >= MAX_PARTICLES) break;
      }
      while (state.particles.length > n) state.spare.push(state.particles.pop());
      for (const p of state.particles) {
        if (p.age === p.lastFrameAge) { p.lastFrameAge = 0; p.age = 0; }
        p.age += dt;
      }
      this.trail(e, state, dt);
      const P = state.particles, E = this.emitterPlace(e), count = P.length;
      const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
      if (state.timer > e.lightning_delay) {
        const start = e.xsize === 0 && e.ysize === 0 ? E.pos.slice()
          : (() => { const o = qrot(E.q, this.areaOffset(e)); return [E.pos[0] + o[0], E.pos[1] + o[1], E.pos[2] + o[2]]; })();
        P[0].pos = start;
        P[count - 1].pos = target.pos.slice();
        const D = sub(P[0].pos, P[count - 1].pos), len = Math.hypot(D[0], D[1], D[2]);
        if (!len) return;
        state.timer = 0;
        const d = [D[0] / len, D[1] / len, D[2] / len];
        const v = [-d[1], d[2], -d[0]];
        const w = [d[1] * v[2] - d[2] * v[1], d[2] * v[0] - d[0] * v[2], d[0] * v[1] - d[1] * v[0]];
        const mid = Math.trunc(count * 0.5);
        this.fractal(e, P, mid, mid, v, w, 0);
        const step = (len / count) * 0.5;
        for (let i = 1; i < count - 1; i++) {
          if (step < 0.01) break;
          const s = rand() & 1 ? 1 : -1;
          const k = (rand() % Math.max(1, Math.trunc(step * 100))) * 0.01;
          for (let c = 0; c < 3; c++) P[i].pos[c] += s * d[c] * k * (e.lightning_scale || 0);
        }
        return;
      }
      const D2 = sub(target.pos, E.pos);
      if (state.prevD && Math.hypot(...state.prevD) && Math.hypot(...D2)) {
        const q = arc(state.prevD, D2);
        for (const p of P) {
          const r = qrot(q, sub(p.pos, state.prevE));
          p.pos = [E.pos[0] + r[0], E.pos[1] + r[1], E.pos[2] + r[2]];
        }
      }
      state.prevD = D2;
      state.prevE = E.pos.slice();
    }

    // PartEmitter::RecursiveFractal (0x62460): point c becomes the middle of points c - h and
    // c + h, pushed up to lightning_radius along +-a1 and +-a2 (0.01 steps; under 0.01: not
    // pushed), then the same for the halves (h / 2) - down to h 0, which pushes c again. The
    // game reads past the last point when there are an even number; here the last stands in.
    fractal(e, P, c, h, a1, a2, depth) {
      if (depth > 40) return;
      const n = P.length, at = (i) => P[Math.max(0, Math.min(n - 1, i))];
      const r = e.lightning_radius || 0, reach = Math.max(1, Math.trunc(Math.fround(r * 100)));
      const s1 = rand() & 1 ? 1 : -1;
      const A = at(c + h).pos, B = at(c - h).pos;
      const m = [(A[0] + B[0]) * 0.5, (A[1] + B[1]) * 0.5, (A[2] + B[2]) * 0.5];
      if (r >= 0.01) {
        const k = (rand() % reach) * 0.01;
        for (let i = 0; i < 3; i++) m[i] += s1 * a1[i] * k;
      }
      const s2 = rand() & 1 ? 1 : -1;
      if (r >= 0.01) {
        const k = (rand() % reach) * 0.01;
        for (let i = 0; i < 3; i++) m[i] += s2 * a2[i] * k;
      }
      at(c).pos = m;
      if (h === 0) return;
      const h2 = Math.trunc(h * 0.5);
      this.fractal(e, P, c - h2, h2, a1, a2, depth + 1);
      this.fractal(e, P, c + h2, h2, a1, a2, depth + 1);
    }

    // Particle::initialize (+ randomPosition, randomDirection, PartEmitter::chkRandom), from the
    // emitter where it is now: its z the way out, its x / y the area.
    spawn(e, state) {
      if (state.particles.length >= MAX_PARTICLES) return;
      const at = this.emitterPlace(e);
      // Speed: velocity plus up to randvel either way, in 0.01 steps.
      let delta = 0;
      if (e.randvel > 0.01) {
        const span = Math.trunc(e.randvel * 100);
        delta = span > 0 ? rand() % span : 0;
        if (rand() % 2) delta = -delta;
      }
      // Where: the emitter, or a point of its xsize x ysize (cm) area.
      const moved = qrot(at.q, this.areaOffset(e));
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
      dir = qrot(at.q, dir);
      xaxis = qrot(at.q, xaxis);
      const pos = [at.pos[0] + moved[0], at.pos[1] + moved[1], at.pos[2] + moved[2]];
      if (e.spread !== 0 && e.deadspace) [dir, xaxis] = deadspace(e.deadspace, dir, xaxis, pos, this.eye);
      const speed = e.velocity + delta * 0.01;
      let frame = 0;
      if (e.random) {
        const span = Math.abs(Math.trunc(e.frame_end) + 1) || 1;
        frame = rand() % span;
      }
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

    // PartEmitter::moveParticleStandard. First what the particle follows: inherit - the
    // emitter, carried and turned with it; inherit_local - the character's move; inherit_part
    // - the emitter's move. Then dt x velocity, gravity mass x 9.81; with bounce, the ground
    // (z 0) stops it (bounce_co 0) or throws it back (x 0.75, its rise x bounce_co) and it
    // rests once slow (|v|^2 < 0.5), 2 cm over the ground (splat: lying flat on it).
    move(e, state, dt) {
      const target = e.p2p === "bezier" || e.p2p === "gravity" ? this.targetPlace(e) : null;
      if (target) return e.p2p === "bezier" ? this.moveBezier(e, state, dt, target) : this.moveGravity(e, state, dt, target);
      const now = this.emitterPlace(e);
      const before = (this.prevPlaces && this.prevPlaces.get(e.name)) || now;
      const turnBy = qmul(now.q, qconj(before.q));
      const fNow = this.places ? this.places.get("") : null, fBefore = this.prevPlaces ? this.prevPlaces.get("") : fNow;
      const shift = (p, d) => { for (let i = 0; i < 3; i++) { p.pos[i] += d[i]; p.trail[i] += d[i]; } };
      for (const p of state.particles) {
        if (p.stopped) continue;
        if (e.inherit) {
          const rel = qrot(turnBy, [p.pos[0] - before.pos[0], p.pos[1] - before.pos[1], p.pos[2] - before.pos[2]]);
          p.pos = [now.pos[0] + rel[0], now.pos[1] + rel[1], now.pos[2] + rel[2]];
          p.vel = qrot(turnBy, p.vel);
        } else if (e.inherit_local && fNow && fBefore) {
          shift(p, [fNow.pos[0] - fBefore.pos[0], fNow.pos[1] - fBefore.pos[1], fNow.pos[2] - fBefore.pos[2]]);
        }
        if (e.inherit_part) shift(p, [now.pos[0] - before.pos[0], now.pos[1] - before.pos[1], now.pos[2] - before.pos[2]]);
        const above = p.pos[2];
        p.pos[0] += p.vel[0] * dt;
        p.pos[1] += p.vel[1] * dt;
        p.pos[2] += p.vel[2] * dt;
        p.vel[2] -= e.mass * GRAVITY * dt;
        this.land(e, p, above, dt);
      }
    }

    // A bouncing particle that reaches the ground this frame (the game tests the scene; here
    // the ground at z 0): stopped (bounce_co 0) or thrown back, and at rest when slow.
    land(e, p, above, dt) {
      if (!e.bounce || !(p.pos[2] < 0 && above >= 0)) return;
      const k = above / (above - p.pos[2] || 1);
      for (let i = 0; i < 2; i++) p.pos[i] -= p.vel[i] * dt * (1 - k);
      p.pos[2] = 0;
      if (e.bounce_co === 0) {
        p.vel = [0, 0, 0];
      } else {
        p.vel = [p.vel[0] * 0.75, p.vel[1] * 0.75, -p.vel[2] * 0.75];
        p.vel[2] *= e.bounce_co * Math.max(0.5, Math.min(1, 0.033 / dt));
      }
      if (p.vel[0] ** 2 + p.vel[1] ** 2 + p.vel[2] ** 2 < 0.5) {
        p.stopped = true;
        p.restDir = p.dir;    // a chunk keeps the turn it had (RenderNothing skips a stopped one)
        p.restSpin = p.spin;
        p.pos[2] = 0.02;
        p.dir = [0, 0, 1];   // turned to the ground's normal (RotationArc(z, n))
        p.xaxis = [1, 0, 0];
      }
      if (e.chunk) p.pos[2] += e.size_start;  // a chunk: pushed its sizeStart off the ground (0.42.6)
    }

    // The emitter's turn since the frame before applied to a particle (inherit): carried and
    // turned with it.
    carry(e, p) {
      const now = this.emitterPlace(e);
      const before = (this.prevPlaces && this.prevPlaces.get(e.name)) || now;
      const turnBy = qmul(now.q, qconj(before.q));
      const rel = qrot(turnBy, [p.pos[0] - before.pos[0], p.pos[1] - before.pos[1], p.pos[2] - before.pos[2]]);
      p.pos = [now.pos[0] + rel[0], now.pos[1] + rel[1], now.pos[2] + rel[2]];
      p.vel = qrot(turnBy, p.vel);
    }

    // PartEmitter::moveParticleP2PBezier (0x66940, read 0.42.5): each particle flies on as usual,
    // then is drawn onto the curve from the emitter to the target - B(t), t = age / life (at most
    // 1), its middle points the emitter + p2p_bezier2 along its z and the target + p2p_bezier3
    // along the target's z - by s = (t^3)^combinetime (0: on it all the way). Only inherit of
    // the follow flags; gravity (mass) and bouncing as usual; stopped particles are moved too.
    moveBezier(e, state, dt, T) {
      const E = this.emitterPlace(e);
      const ez = qrot(E.q, [0, 0, 1]), tz = qrot(T.q, [0, 0, 1]);
      const P1 = [0, 1, 2].map((i) => E.pos[i] + (e.p2p_bezier2 || 0) * ez[i]);
      const P2 = [0, 1, 2].map((i) => T.pos[i] + (e.p2p_bezier3 || 0) * tz[i]);
      for (const p of state.particles) {
        if (e.inherit) this.carry(e, p);
        const above = p.pos[2];
        const t = Math.min(1, p.age / e.life), u = 1 - t;
        const b0 = u * u * u, b1 = 3 * t * u * u, b2 = 3 * t * t * u, b3 = t * t * t;
        const s = Math.pow(t * t * t, e.combinetime || 0);
        for (let i = 0; i < 3; i++) {
          const free = p.pos[i] + dt * p.vel[i];
          const curve = b0 * E.pos[i] + b1 * P1[i] + b2 * P2[i] + b3 * T.pos[i];
          p.pos[i] = free * (1 - s) + curve * s;
        }
        p.vel[2] -= e.mass * GRAVITY * dt;
        this.land(e, p, above, dt);
      }
    }

    // PartEmitter::moveParticleP2PGrav (0x67340, read 0.42.5): pulled toward the target at grav
    // m/s^2, the speed kept drag^dt; gone (aged out) within threshold of it - checked from where
    // it starts the frame (the game then leaves its loop: the rest don't move this frame) and
    // along the frame's step. inherit_local / inherit_part also turn the particles with the
    // character here (the standard move only shifts them).
    moveGravity(e, state, dt, T) {
      const now = this.emitterPlace(e);
      const before = (this.prevPlaces && this.prevPlaces.get(e.name)) || now;
      const fNow = this.places ? this.places.get("") : null, fBefore = (this.prevPlaces && this.prevPlaces.get("")) || fNow;
      const fTurn = fNow && fBefore ? qmul(fNow.q, qconj(fBefore.q)) : [0, 0, 0, 1];
      const follow = (p, from, to) => {
        const rel = qrot(fTurn, [p.pos[0] - from.pos[0], p.pos[1] - from.pos[1], p.pos[2] - from.pos[2]]);
        for (let i = 0; i < 3; i++) { p.trail[i] += to.pos[i] - from.pos[i]; p.pos[i] = to.pos[i] + rel[i]; }
        p.vel = qrot(fTurn, p.vel);
      };
      const near = e.threshold * e.threshold;
      for (const p of state.particles) {
        if (e.inherit) this.carry(e, p);
        else if (e.inherit_local && fNow && fBefore) follow(p, fBefore, fNow);
        if (e.inherit_part) follow(p, before, now);
        const old = p.pos.slice();
        const next = [0, 1, 2].map((i) => old[i] + dt * p.vel[i]);
        const d = [0, 1, 2].map((i) => T.pos[i] - old[i]);
        const d2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
        if (near >= d2) { p.age = e.life; return; }
        const keep = Math.pow(e.drag, dt), pull = (e.grav * dt) / Math.sqrt(d2);
        for (let i = 0; i < 3; i++) {
          p.vel[i] *= keep;
          next[i] += pull * d[i] * dt;
          p.vel[i] += pull * d[i];
        }
        // The point of this frame's step nearest the target (segmentproject).
        const step = [0, 1, 2].map((i) => next[i] - old[i]), len2 = step[0] ** 2 + step[1] ** 2 + step[2] ** 2;
        const k = len2 ? Math.max(0, Math.min(1, (d[0] * step[0] + d[1] * step[1] + d[2] * step[2]) / len2)) : 0;
        const miss = [0, 1, 2].map((i) => T.pos[i] - (old[i] + k * step[i]));
        if (near >= miss[0] ** 2 + miss[1] ** 2 + miss[2] ** 2) p.age = e.life;
        p.pos = next;
        p.vel[2] -= e.mass * GRAVITY * dt;
        this.land(e, p, old[2], dt);
      }
    }

    // One frame (the game runs its emitters once a frame; dt at most 0.5 s): the nodes moved
    // to now (PartEmitter::AnimateParticles), then each emitter's update.
    step(dt) {
      if (!this.fx) return;
      dt = Math.min(dt, 0.5);
      this.time += dt;
      const t = this.time;
      this.prevPlaces = this.places || this.placesAt(Math.max(0, t - dt));
      this.places = this.placesAt(t);
      this.fx.emitters.forEach((e, i) => {
        const state = this.states[i];
        if (!state) return;
        const update = String(e.update).toLowerCase();
        const birth = this.birthrate(e, t);
        if (update === "single") {
          if (Math.trunc(birth) === 0) { state.particles = []; return; }
          if (!state.particles.length) { this.spawn(e, state); return; }
          state.particles[0].age += dt;  // never dies by age; loop isn't written (0)
          this.spin(e, state, dt);
          this.trail(e, state, dt);
          this.move(e, state, dt);
          return;
        }
        if (update === "lightning") { this.lightning(e, state, dt, t); return; }
        if (update !== "fountain" && update !== "explosion") return;
        this.ageParticles(e, state, dt);
        this.trail(e, state, dt);
        this.spin(e, state, dt);
        this.move(e, state, dt);
        const n = Math.trunc(birth);
        if (update === "explosion") {
          // The impact's detonate event at the start: int(birthrate) at once.
          if (!state.burst) {
            state.burst = true;
            for (let k = 0; k < n; k++) this.spawn(e, state);
          }
          return;
        }
        // Fountain: time adds up; past 1 / birthrate, int(time x birthrate) are born and the
        // rest of the time is dropped (at 60 fps a rate of 100 makes 60 a second).
        if (n <= 0) return;
        state.acc += dt;
        if (state.acc >= 1 / birth) {
          const count = Math.trunc(state.acc * birth) % (n + 1);
          for (let k = 0; k < count; k++) this.spawn(e, state);
          state.acc = 0;
        }
      });
    }

    // What PartEmitter::RenderEmitter draws, per emitter: each particle's half size, colour
    // and alpha (8-bit, as the game packs them) and sheet frame. t = min(1, age / lifeExp) -
    // a straight start -> end (the mid values and percents do nothing); lifeExp 0: nothing
    // drawn, below 0: the start values. Splat: sizeStart in flight, sizeEnd once it rests.
    // The frame steps by whole frames at fps while drawn (the rest of the time dropped) and
    // wraps to the first; fps 0 (or a one-frame range): it stays. Also where the emitter is
    // and how it's turned (Billboard_to_Local_Z lies in its x / y).
    snapshot() {
      if (!this.fx) return [];
      if (!this.places) this.places = this.placesAt(this.time);
      const clamp01 = (v) => Math.max(0, Math.min(1, v));
      return this.fx.emitters.map((e, i) => {
        const state = this.states[i];
        const grid = Math.max(1, e.grid | 0);
        const at = this.emitterPlace(e);
        const lightning = String(e.update).toLowerCase() === "lightning";
        if (!state || this.hidden.has(i) || (e.life === 0 && !lightning)) return { emitter: e, particles: [], grid, place: at };
        const life = lightning ? 1 : e.life;  // LightningEmitter::Initialize sets lifeExp 1
        const hs0 = e.size_start * 0.5, hs1 = e.size_end * 0.5;
        const sizeVaries = life > 0 && e.size_start !== e.size_end && !e.splat;
        const cS = e.color_start.map((v) => clamp01(v) * 255.99), cE = e.color_end.map((v) => clamp01(v) * 255.99);
        const colorVaries = life > 0 && e.color_start.some((v, k) => v !== e.color_end[k]);
        const aS = clamp01(e.alpha_start) * 255.99, aE = clamp01(e.alpha_end) * 255.99;
        const alphaVaries = life > 0 && e.alpha_start !== e.alpha_end;
        const spf = e.fps > 0 ? 1 / e.fps : 0;
        const frameEnd = Math.trunc(e.frame_end);
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
            pos: p.pos, trail: p.trail, w, h: w, color: [...rgb, Math.trunc(alpha) / 255], alpha255: alpha,
            frame: grid > 1 ? p.frame : -1, spin: p.spin, dir: p.dir, xaxis: p.xaxis, stopped: p.stopped,
            restDir: p.restDir, restSpin: p.restSpin,
          };
        });
        return { emitter: e, particles, grid, place: at };
      });
    }

    // Where the nodes are now (the stage marks them).
    nodePlaces() {
      if (!this.fx) return [];
      if (!this.places) this.places = this.placesAt(this.time);
      return (this.fx.nodes || []).map((n) => ({ name: n.name, ...(this.places.get(n.name) || { pos: [0, 0, 0], q: [0, 0, 0, 1] }) }));
    }

    count() { return this.states.reduce((n, s, i) => n + (this.hidden.has(i) ? 0 : s.particles.length), 0); }
  }

  // The camera shake as the game moves its camera (0.44.0, read in nwmain: the client's camera
  // update 0x7d0470, set by 0x7cbfa0 / 0x7cfd00 from the effect's row): how far up, t seconds after
  // the effect is applied. A bump (1): from the delay, up 0.25 m over 0.05 s and back over 0.05 s.
  // A shake (2): from the delay for its duration, 0 -> 0.1 m -> 0 every 0.14 s (0.07 s up, 0.07 s
  // down; the sideways part is 0 in this build); one of no duration doesn't happen.
  function shakeAt(fx, t) {
    const type = fx.shake_type | 0, u = t - (fx.shake_delay || 0);
    if (!type || u < 0) return 0;
    if (type === 1) return u < 0.05 ? (0.25 * u) / 0.05 : u < 0.1 ? 0.25 * (1 - (u - 0.05) / 0.05) : 0;
    if (type === 2 && u < (fx.shake_duration || 0)) {
      const k = (u % 0.07) / 0.07;
      return Math.floor(u / 0.07) % 2 ? 0.1 * (1 - k) : 0.1 * k;
    }
    return 0;
  }

  // About how many particles an effect has alive at once at most (0.44.0): a stream's rate x its
  // life (shorter if it switches off sooner; never-dying ones: what its time lets build up), a
  // burst's count, a Single's one, a bolt's points; and how many of them are chunk models. The
  // game's own effects, counted so: half under 25, 9 in 10 under 250, the heaviest (Hellball)
  // about 1,700.
  const BUDGET = { typical: 250, heaviest: 1700 };
  function particleBudget(fx) {
    let total = 0, chunks = 0;
    const length = fx.duration ? Math.max(fx.seconds || 0, 1) : Math.max(fx.seconds || 0, 0.2);
    for (const e of fx.emitters) {
      const u = String(e.update).toLowerCase(), r = Math.max(0, e.birthrate || 0);
      let n;
      if (u === "single") n = Math.trunc(r) ? 1 : 0;
      else if (u === "lightning" || u === "explosion") n = Math.trunc(r);
      else {
        const on = e.emit_seconds > 0 ? Math.min(e.emit_seconds, length) : length;
        n = r * (e.life < 0 ? on : Math.min(e.life, on));
      }
      total += n;
      if (e.chunk) chunks += n;
    }
    return { total: Math.round(total), chunks: Math.round(chunks) };
  }

  const api = { Sim, ATTACH_HEIGHT, turn, quat, qrot, qmul, slerp, sample, shakeAt, particleBudget, BUDGET };
  if (typeof module === "object" && module.exports) module.exports = api;
  else root.VfxSim = api;
})(typeof self !== "undefined" ? self : this);
