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

  // The recipe's clip from its base (and mix) - DeriveAnimClip.
  function deriveClip(recipe, base, mixClip) {
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
    return out;
  }

  function num(v) {
    let s = Number(v).toPrecision(6);
    if (s.indexOf("e") < 0 && s.indexOf(".") >= 0) s = s.replace(/0+$/, "").replace(/\.$/, "");
    return s;
  }

  function writeRecipe(r) {
    return "# nwn5e animation recipe (!animedit, Animation Recipe Bench) - key = value\n" +
      "name = " + r.name + "\nbase = " + r.base + "\nmix = " + (r.mix || "") + "\namount = " + num(r.amount) +
      "\nspeed = " + num(r.speed) + "\nstart = " + num(r.start) + "\nend = " + num(r.end) + "\n";
  }

  function readRecipe(text) {
    const r = { name: "", base: "pause1", mix: "", amount: 0.5, speed: 1, start: 0, end: 0 };
    for (const raw of String(text).split(/\r?\n/)) {
      const line = raw.replace(/#.*/, "").trim();
      if (!line) continue;
      const eq = line.indexOf("=");
      if (eq < 0) continue;
      const key = lower(line.slice(0, eq).trim()), value = line.slice(eq + 1).trim();
      if (key === "name") r.name = value;
      else if (key === "base") r.base = value;
      else if (key === "mix") r.mix = value;
      else if (["amount", "speed", "start", "end"].includes(key)) r[key] = parseFloat(value) || 0;
    }
    if (!r.name || !r.base) throw new Error("a recipe needs a name and a base");
    if (r.speed <= 0) r.speed = 1;
    return r;
  }

  const api = { readLibrary, readClip, deriveClip, sample, writeRecipe, readRecipe };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.AnimRecipes = api;
})(typeof window !== "undefined" ? window : globalThis);
