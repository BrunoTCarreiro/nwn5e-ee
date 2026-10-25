// Animation Recipe Bench - the core: a compiled NWN animation library's reader and the
// recipe maths, ported line for line from nwn5e's src/data/anim_recipe.cpp so the
// preview is what the game builds. Runs in the page and under node (for checking).
(function (root) {
  const BASE = 12;

  function lower(s) { return String(s).toLowerCase(); }

  function reader(buffer) {
    const view = new DataView(buffer);
    const size = buffer.byteLength;
    return {
      size,
      u32: (at) => (at + 4 <= size ? view.getUint32(at, true) : 0),
      u16: (at) => (at + 2 <= size ? view.getUint16(at, true) : 0),
      u8: (at) => (at < size ? view.getUint8(at) : 0),
      f32: (at) => (at + 4 <= size ? view.getFloat32(at, true) : 0),
      str(at, max) {
        let out = "";
        for (let i = 0; i < max && at + i < size; i++) {
          const c = view.getUint8(at + i);
          if (c === 0) break;
          out += String.fromCharCode(c);
        }
        return out;
      },
    };
  }

  // A node's position (8) and orientation (20) keys; compressed orientations skipped.
  function readTracks(m, node, out) {
    const keys = m.u32(node + 0x54), keyCount = m.u32(node + 0x58);
    const data = m.u32(node + 0x60), dataCount = m.u32(node + 0x64);
    if (keyCount > 96 || dataCount > 65536) return;
    for (let k = 0; k < keyCount; k++) {
      const at = BASE + keys + 12 * k;
      const type = m.u32(at), rows = m.u16(at + 4), timeAt = m.u16(at + 6), valueAt = m.u16(at + 8);
      const raw = m.u8(at + 10), columns = raw & 0x0f;
      if ((type !== 8 && type !== 20) || (raw & 0x10) || rows <= 0) continue;
      if ((type === 8 && columns !== 3) || (type === 20 && columns !== 4)) continue;
      if (timeAt + rows > dataCount || valueAt + rows * columns > dataCount) continue;
      const track = { times: [], values: [] };
      for (let r = 0; r < rows; r++) {
        track.times.push(m.f32(BASE + data + 4 * (timeAt + r)));
        const v = [0, 0, 0, 1];
        for (let c = 0; c < columns; c++) v[c] = m.f32(BASE + data + 4 * (valueAt + r * columns + c));
        track.values.push(v);
      }
      if (type === 8) out.position = track; else out.orientation = track;
    }
  }

  function walkNodes(m, node, parent, depth, out) {
    if (depth > 64 || out.length > 1024 || node + 0x70 > m.size) return false;
    const n = { name: m.str(node + 0x20, 32), parent, position: null, orientation: null };
    readTracks(m, node, n);
    const index = out.length;
    out.push(n);
    const children = m.u32(node + 0x48), count = m.u32(node + 0x4c);
    if (count > 256 || BASE + children + 4 * count > m.size) return count === 0;
    for (let c = 0; c < count; c++) {
      if (!walkNodes(m, BASE + m.u32(BASE + children + 4 * c), index, depth + 1, out)) return false;
    }
    return true;
  }

  // A compiled library: its bone tree and its animations (by lower-case name).
  function readLibrary(buffer) {
    const m = reader(buffer);
    if (m.size < 0x200 || m.u32(0) !== 0) throw new Error("not a compiled model (an animation library is needed)");
    const lib = { name: lower(m.str(BASE + 8, 64)), supermodel: lower(m.str(BASE + 0xa8, 64)), bones: [], clips: new Map(), m };
    if (lib.supermodel === "null") lib.supermodel = "";
    if (!walkNodes(m, BASE + m.u32(BASE + 0x48), -1, 0, lib.bones) || !lib.bones.length) {
      throw new Error("its bone tree couldn't be read");
    }
    const clips = m.u32(BASE + 0x78), count = m.u32(BASE + 0x7c);
    if (count > 4096 || BASE + clips + 4 * count > m.size) throw new Error("its animation list couldn't be read");
    for (let i = 0; i < count; i++) {
      const anim = BASE + m.u32(BASE + clips + 4 * i);
      lib.clips.set(lower(m.str(anim + 8, 64)), anim);
    }
    return lib;
  }

  function readClip(lib, name) {
    const anim = lib.clips.get(lower(name));
    if (anim === undefined) return null;
    const m = lib.m;
    const clip = { name: m.str(anim + 8, 64), length: m.f32(anim + 0x70), transtime: m.f32(anim + 0x74),
                   animroot: m.str(anim + 0x78, 64), nodes: [] };
    if (!walkNodes(m, BASE + m.u32(anim + 0x48), -1, 0, clip.nodes) || !clip.nodes.length) return null;
    return clip;
  }

  function mix(a, b, f, quaternion) {
    if (quaternion) {
      let dot = 0;
      for (let i = 0; i < 4; i++) dot += a[i] * b[i];
      if (dot < 0) b = b.map((v) => -v);
    }
    const out = [0, 0, 0, 0];
    for (let i = 0; i < 4; i++) out[i] = a[i] + (b[i] - a[i]) * f;
    if (quaternion) {
      const n = Math.hypot(out[0], out[1], out[2], out[3]);
      if (n > 1e-6) for (let i = 0; i < 4; i++) out[i] /= n;
      else return a.slice();
    }
    return out;
  }

  function sample(track, t, quaternion) {
    if (!track || !track.times.length) return [0, 0, 0, 1];
    if (t <= track.times[0]) return track.values[0];
    for (let i = 1; i < track.times.length; i++) {
      if (t <= track.times[i]) {
        const span = Math.max(track.times[i] - track.times[i - 1], 1e-6);
        return mix(track.values[i - 1], track.values[i], (t - track.times[i - 1]) / span, quaternion);
      }
    }
    return track.values[track.values.length - 1];
  }

  function findNode(clip, name) {
    const want = lower(name);
    return clip.nodes.find((n) => lower(n.name) === want) || null;
  }

  // The recipe's clip from its base (and mix) - DeriveAnimClip; the crouch step needs the library's bones.
  function deriveClip(recipe, base, mixClip, bones) {
    const length = Math.max(base.length, 0.001);
    const start = Math.max(0, Math.min(recipe.start, length));
    let end = recipe.end > start ? Math.min(recipe.end, length) : length;
    if (end <= start) end = length;
    const speed = Math.max(0.05, recipe.speed);
    const mixing = !!mixClip && !!recipe.mix && recipe.amount !== 0;
    const mixScale = mixing ? Math.max(mixClip.length, 0.001) / length : 1;
    const out = { name: recipe.name, length: (end - start) / speed, transtime: base.transtime, animroot: base.animroot, nodes: [] };
    for (const node of base.nodes) {
      const n = { name: node.name, parent: node.parent, position: null, orientation: null };
      const other = mixing ? findNode(mixClip, node.name) : null;
      for (const key of ["position", "orientation"]) {
        const quaternion = key === "orientation";
        const track = node[key];
        if (!track || !track.times.length) continue;
        const toward = other && other[key] ? other[key] : null;
        const times = [start];
        for (const t of track.times) if (t > start + 1e-4 && t < end - 1e-4) times.push(t);
        times.push(end);
        const result = { times: [], values: [] };
        for (const t of times) {
          let v = sample(track, t, quaternion);
          if (toward) v = mix(v, sample(toward, t * mixScale, quaternion), recipe.amount, quaternion);
          result.times.push((t - start) / speed);
          result.values.push(v);
        }
        n[key] = result;
      }
      out.nodes.push(n);
    }
    if (bones) applyCrouch(out, bones, recipe.crouch || 0, recipe.lean || 0);
    return out;
  }

  // --- The crouch step (ApplyCrouch in anim_recipe.cpp): the hips drop, each leg
  // solved again so the ankle keeps the animation's footfall, the upper body leans.
  const IDENTITY = [0, 0, 0, 1];
  const add = (a, b) => [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
  const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
  const scale = (a, k) => [a[0] * k, a[1] * k, a[2] * k];
  const dot3 = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  const len3 = (a) => Math.sqrt(dot3(a, a));
  const norm3 = (a) => { const l = len3(a); return l > 1e-9 ? scale(a, 1 / l) : [0, 0, 0]; };
  const qmul = (a, b) => [
    a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
    a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]];
  const qconj = (q) => [-q[0], -q[1], -q[2], q[3]];
  const qnorm = (q) => { const n = Math.hypot(q[0], q[1], q[2], q[3]); return n > 1e-9 ? q.map((v) => v / n) : IDENTITY.slice(); };
  function rotate(q, v) {
    const u = [q[0], q[1], q[2]];
    const t = scale(cross(u, v), 2);
    return add(add(v, scale(t, q[3])), cross(u, t));
  }
  const axisAngle = (axis, angle) => { const s = Math.sin(angle / 2); return [axis[0] * s, axis[1] * s, axis[2] * s, Math.cos(angle / 2)]; };
  function fromTo(from, to) {
    const a = norm3(from), b = norm3(to);
    const d = dot3(a, b);
    if (d < -0.999999) {
      let axis = cross([1, 0, 0], a);
      if (len3(axis) < 1e-6) axis = cross([0, 1, 0], a);
      return axisAngle(norm3(axis), Math.PI);
    }
    const c = cross(a, b);
    return qnorm([c[0], c[1], c[2], 1 + d]);
  }

  function clipIndex(clip, bones) {
    return bones.map((b) => { const name = lower(b.name); return clip.nodes.findIndex((n) => lower(n.name) === name); });
  }
  function forward(bones, pose) {
    for (let i = 0; i < bones.length; i++) {
      const up = bones[i].parent;
      if (up < 0) { pose.wp[i] = pose.lp[i]; pose.wq[i] = pose.lq[i]; }
      else { pose.wq[i] = qnorm(qmul(pose.wq[up], pose.lq[i])); pose.wp[i] = add(pose.wp[up], rotate(pose.wq[up], pose.lp[i])); }
    }
  }
  function poseAt(clip, inClip, bones, t) {
    const pose = { lp: [], lq: [], wp: [], wq: [] };
    bones.forEach((b, i) => {
      const node = inClip[i] >= 0 ? clip.nodes[inClip[i]] : null;
      let p = b.position && b.position.values.length ? b.position.values[0] : [0, 0, 0];
      let q = b.orientation && b.orientation.values.length ? b.orientation.values[0] : IDENTITY;
      if (node && node.position && node.position.times.length) p = sample(node.position, t, false);
      if (node && node.orientation && node.orientation.times.length) q = sample(node.orientation, t, true);
      pose.lp[i] = [p[0], p[1], p[2]];
      pose.lq[i] = qnorm(q);
    });
    forward(bones, pose);
    return pose;
  }
  const copyPose = (p) => ({ lp: p.lp.slice(), lq: p.lq.slice(), wp: p.wp.slice(), wq: p.wq.slice() });

  function solveLeg(bones, before, now, leg) {
    const hip = now.wp[leg.thigh], target = before.wp[leg.foot];
    const l1 = len3(now.lp[leg.shin]), l2 = len3(now.lp[leg.foot]);
    const to = sub(target, hip);
    const reach = len3(to);
    if (reach < 1e-5 || l1 < 1e-5 || l2 < 1e-5) return;
    const u = scale(to, 1 / reach);
    const d = Math.max(Math.abs(l1 - l2) + 1e-4, Math.min(l1 + l2 - 1e-4, reach));
    // The knee bends the way it did, off the old hip-ankle line; a nearly straight leg the way its thigh faces.
    const oldLine = norm3(sub(before.wp[leg.foot], before.wp[leg.thigh]));
    let bend = sub(before.wp[leg.shin], before.wp[leg.thigh]);
    bend = sub(bend, scale(oldLine, dot3(bend, oldLine)));
    const facing = norm3(rotate(before.wq[leg.thigh], [0, 1, 0]));
    let pole = add(bend, scale(facing, 0.05));
    pole = sub(pole, scale(u, dot3(pole, u)));
    if (len3(pole) < 1e-6) return;
    pole = norm3(pole);
    const a = (l1 * l1 - l2 * l2 + d * d) / (2 * d);
    const h = Math.sqrt(Math.max(0, l1 * l1 - a * a));
    const knee = add(hip, add(scale(u, a), scale(pole, h)));
    const up = now.wq[bones[leg.thigh].parent];
    const thigh = qnorm(qmul(fromTo(rotate(now.wq[leg.thigh], now.lp[leg.shin]), sub(knee, hip)), now.wq[leg.thigh]));
    let shin = qnorm(qmul(thigh, now.lq[leg.shin]));
    shin = qnorm(qmul(fromTo(rotate(shin, now.lp[leg.foot]), sub(target, knee)), shin));
    now.lq[leg.thigh] = qnorm(qmul(qconj(up), thigh));
    now.lq[leg.shin] = qnorm(qmul(qconj(thigh), shin));
    now.lq[leg.foot] = qnorm(qmul(qconj(shin), before.wq[leg.foot]));
  }

  function applyCrouch(clip, bones, crouch, leanDegrees) {
    const inClip = clipIndex(clip, bones);
    const find = (name) => bones.findIndex((b, i) => lower(b.name) === name && inClip[i] >= 0);
    const under = (bone, ancestor) => { for (let at = bone; at >= 0; at = bones[at].parent) if (at === ancestor) return true; return false; };
    const root = find("rootdummy"), torso = find("torso_g");
    let head = find("head_g");
    if (head >= 0 && (torso < 0 || !under(head, torso))) head = -1;
    const crouching = crouch !== 0 && root >= 0;
    const leaning = leanDegrees !== 0 && torso >= 0;
    if (!crouching && !leaning) return false;
    const legs = [];
    for (const side of ["l", "r"]) {
      const leg = { thigh: find(side + "thigh_g"), shin: find(side + "shin_g"), foot: find(side + "foot_g") };
      if (leg.thigh > 0 && leg.shin > 0 && leg.foot > 0 && bones[leg.shin].parent === leg.thigh &&
          bones[leg.foot].parent === leg.shin && bones[leg.thigh].parent >= 0) legs.push(leg);
    }
    const times = [];
    const step = 1 / 30;
    for (let k = 0; k * step < clip.length - 1e-4; k++) times.push(k * step);
    times.push(Math.max(0, clip.length));
    const lean = (leanDegrees * Math.PI) / 180;
    const leanTurn = axisAngle([1, 0, 0], -lean);  // +Z toward +Y: forward
    const headTurn = axisAngle([1, 0, 0], -lean * 0.3);
    const moved = [];
    if (crouching) moved.push(root);
    if (leaning) { moved.push(torso); if (head >= 0) moved.push(head); }
    if (crouching) for (const leg of legs) moved.push(leg.thigh, leg.shin, leg.foot);
    const keys = new Map(moved.map((b) => [b, { times: [], values: [] }]));
    for (const t of times) {
      const before = poseAt(clip, inClip, bones, t);
      const now = copyPose(before);
      if (leaning) {
        const up = bones[torso].parent;
        now.lq[torso] = qnorm(qmul(qconj(up >= 0 ? before.wq[up] : IDENTITY), qmul(leanTurn, before.wq[torso])));
        if (head >= 0) {
          forward(bones, now);
          now.lq[head] = qnorm(qmul(qconj(now.wq[bones[head].parent]), qmul(headTurn, before.wq[head])));
        }
      }
      if (crouching) {
        const up = bones[root].parent;
        now.lp[root] = add(now.lp[root], rotate(qconj(up >= 0 ? before.wq[up] : IDENTITY), [0, 0, -crouch]));
        forward(bones, now);
        for (const leg of legs) solveLeg(bones, before, now, leg);
      }
      for (const b of moved) {
        const k = keys.get(b);
        k.times.push(t);
        if (b === root && crouching) k.values.push([now.lp[b][0], now.lp[b][1], now.lp[b][2], 1]);
        else {
          let q = now.lq[b];
          const last = k.values[k.values.length - 1];
          if (last && last[0] * q[0] + last[1] * q[1] + last[2] * q[2] + last[3] * q[3] < 0) q = q.map((v) => -v);
          k.values.push(q);
        }
      }
    }
    for (const b of moved) {
      const node = clip.nodes[inClip[b]];
      if (b === root && crouching) node.position = keys.get(b); else node.orientation = keys.get(b);
    }
    return true;
  }

  // Where each bone is at t in the clip, in the model's space (AnimWorldPositions).
  function worldPositions(clip, bones, t) {
    return poseAt(clip, clipIndex(clip, bones), bones, t).wp;
  }

  function num(v) {
    let s = Number(v).toPrecision(6);
    if (s.indexOf("e") < 0 && s.indexOf(".") >= 0) s = s.replace(/0+$/, "").replace(/\.$/, "");
    return s;
  }

  function writeRecipe(r) {
    return "# nwn5e animation recipe (!animedit, Animation Recipe Bench) - key = value\n" +
      "name = " + r.name + "\nbase = " + r.base + "\nmix = " + (r.mix || "") + "\namount = " + num(r.amount) +
      "\nspeed = " + num(r.speed) + "\nstart = " + num(r.start) + "\nend = " + num(r.end) +
      "\ncrouch = " + num(r.crouch || 0) + "\nlean = " + num(r.lean || 0) + "\n";
  }

  function readRecipe(text) {
    const r = { name: "", base: "pause1", mix: "", amount: 0.5, speed: 1, start: 0, end: 0, crouch: 0, lean: 0 };
    for (const raw of String(text).split(/\r?\n/)) {
      const line = raw.replace(/#.*/, "").trim();
      if (!line) continue;
      const eq = line.indexOf("=");
      if (eq < 0) continue;
      const key = lower(line.slice(0, eq).trim()), value = line.slice(eq + 1).trim();
      if (key === "name") r.name = value;
      else if (key === "base") r.base = value;
      else if (key === "mix") r.mix = value;
      else if (["amount", "speed", "start", "end", "crouch", "lean"].includes(key)) r[key] = parseFloat(value) || 0;
    }
    if (!r.name || !r.base) throw new Error("a recipe needs a name and a base");
    if (r.speed <= 0) r.speed = 1;
    r.crouch = Math.max(0, Math.min(0.6, r.crouch));  // as anim_recipe.cpp keeps them
    r.lean = Math.max(-45, Math.min(60, r.lean));
    return r;
  }

  const api = { readLibrary, readClip, deriveClip, applyCrouch, worldPositions, sample, writeRecipe, readRecipe };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.AnimRecipes = api;
})(typeof window !== "undefined" ? window : globalThis);
