// VFX Bench - the core: nwn5e's VFX draft (key = value text), ported line for line
// from src/data/vfx_model.cpp's WriteVfxDraft / ReadVfxDraft / VfxResRef (keep the
// two in step: check.js reads and writes back every draft the game exported), the
// effect's defaults (vfx_editor.cpp's), and a TGA reader for the particle textures.
// Runs in the page and under node (server.js, check.js).
(function (root) {
  "use strict";

  // The values the game's own models use (a model's own spelling is kept as it is).
  const UPDATES = ["Fountain", "Single", "Explosion", "Lightning"];
  // data::kVfxDeadspaceMax: above about 0.39 the game's deadspace loop never ends for some
  // particles (it would hang), so a draft holds it there (0.42.4).
  const DEADSPACE_MAX = 0.39;
  const RENDERS = ["Normal", "Linked", "Billboard_to_Local_Z", "Billboard_to_World_Z", "Aligned_to_World_Z",
    "Aligned_to_Particle_Dir", "Motion_Blur"];
  const BLENDS = ["Normal", "Lighten", "Punch-Through"];
  const ATTACHES = ["root", "head", "impact"];

  // data::VfxEmitter's defaults.
  function emitter(overrides) {
    return Object.assign({
      name: "emitter", update: "Fountain", render: "Normal", blend: "Normal", texture: "fxpa_sphere",
      position: [0, 0, 0], orientation: [0, 0, 0, 0], birthrate: 10, emit_seconds: 0, life: 1, velocity: 1,
      randvel: 0.5, spread: 0.8,
      mass: 0, size_start: 0.1, size_end: 0.1, color_start: [1, 1, 1], color_end: [1, 1, 1], alpha_start: 1,
      alpha_end: 0, xsize: 0, ysize: 0, grid: 1, frame_end: 0, blur_length: 0, bounce_co: 0, fps: 0, particle_rot: 0,
      inherit: false, splat: false, bounce: false, random: false, inherit_local: false, inherit_part: false,
      twosidedtex: false, deadspace: 0, parent: "", keys: [],
      lightning_delay: 0, lightning_radius: 0, lightning_scale: 0,
      chunk: "", p2p: "", p2p_bezier2: 0, p2p_bezier3: 0, combinetime: 0, grav: 0, drag: 0, threshold: 0,
    }, overrides || {});
  }

  // data::VfxNode's: a node an emitter hangs under (0.42.3), with its keys.
  function node(overrides) {
    return Object.assign({ name: "node", parent: "", position: [0, 0, 0], orientation: [0, 0, 0, 0], keys: [], ref_model: "" },
      overrides || {});
  }

  function effect(overrides) {
    return Object.assign({ name: "n5_new", duration: true, seconds: 8, attach: "root", lengths: [0, 2, 1], nodes: [], emitters: [] },
      overrides || {});
  }

  // The animations, in the order a draft lists them; a node's / emitter's keys of one.
  const ANIMS = ["impact", "duration", "cessation"];
  function findKeys(list, anim, create) {
    let k = list.find((x) => x.anim === anim);
    if (!k && create) { k = { anim, position: [], orientation: [] }; list.push(k); }
    return k || null;
  }
  function draftKeys(keys) {
    let out = "";
    for (const anim of ANIMS) {
      for (const k of keys || []) {
        if (k.anim !== anim) continue;
        if (k.position.length) out += anim + "_position = " + k.position.map(num).join(" ") + "\n";
        if (k.orientation.length) out += anim + "_orientation = " + k.orientation.map(num).join(" ") + "\n";
      }
    }
    return out;
  }
  function draftKeyLine(key, words, keys) {
    const m = /^(impact|duration|cessation)_(position|orientation)$/.exec(key);
    if (!m) return false;
    findKeys(keys, m[1], true)[m[2]] = words.map(toFloat);
    return true;
  }

  // vfx_editor.cpp's DefaultEmitter / DefaultEffect: orange sparks.
  function sparks() {
    return emitter({
      name: "sparks", texture: "fxpa_sphere", render: "Normal", blend: "Lighten", position: [0, 0, 0], birthrate: 20,
      life: 1, velocity: 1.5, randvel: 0.5, spread: 0.8, mass: 0.5, size_start: 0.08, size_end: 0.02,
      color_start: [1, 0.8, 0.3], color_end: [1, 0.3, 0.1],
    });
  }

  function newEffect() { return effect({ emitters: [sparks()] }); }

  function clone(value) { return JSON.parse(JSON.stringify(value)); }

  // C's "%g" of a float (what the game writes): 6 significant digits, no trailing zeros.
  function num(value) {
    const x = Math.fround(Number(value));
    if (Number.isNaN(x)) return "nan";
    if (!Number.isFinite(x)) return x > 0 ? "inf" : "-inf";
    if (x === 0) return Object.is(x, -0) ? "-0" : "0";
    const [mantissa, e] = x.toExponential(5).split("e");
    const exponent = Number(e);
    if (exponent < -4 || exponent >= 6) {
      const m = mantissa.includes(".") ? mantissa.replace(/\.?0+$/, "") : mantissa;
      return m + "e" + (exponent < 0 ? "-" : "+") + String(Math.abs(exponent)).padStart(2, "0");
    }
    const fixed = x.toFixed(5 - exponent);
    return fixed.includes(".") ? fixed.replace(/\.?0+$/, "") : fixed;
  }

  // atof / atoi as the game reads them: the leading number, else 0.
  function toFloat(text) {
    const v = parseFloat(String(text).trim());
    return Number.isNaN(v) ? 0 : Math.fround(v);
  }
  function toInt(text) {
    const v = parseInt(String(text).trim(), 10);
    return Number.isNaN(v) ? 0 : v;
  }

  // data::VfxResRef: letters, digits and _, lower case, at most `max`.
  function resref(text, max) {
    max = max || 16;
    let out = "";
    for (const c of String(text)) {
      if (out.length >= max) break;
      if (/[A-Za-z0-9_]/.test(c)) out += c.toLowerCase();
    }
    return out;
  }

  function writeDraft(fx) {
    let out = "# nwn5e VFX draft (!vfxedit) - key = value; one [emitter <name>] section per emitter\n";
    out += "name = " + fx.name + "\n";
    out += "type = " + (fx.duration ? "D" : "F") + "\n";
    out += "seconds = " + num(fx.seconds) + "\n";
    out += "attach = " + fx.attach + "\n";
    const l = fx.lengths || [0, 2, 1];
    out += "lengths = " + num(l[0]) + " " + num(l[1]) + " " + num(l[2]) + "\n";
    for (const n of fx.nodes || []) {
      out += "\n[node " + n.name + "]\n";
      if (n.parent) out += "parent = " + n.parent + "\n";
      out += "position = " + num(n.position[0]) + " " + num(n.position[1]) + " " + num(n.position[2]) + "\n";
      out += "orientation = " + num(n.orientation[0]) + " " + num(n.orientation[1]) + " " + num(n.orientation[2]) + " " +
        num(n.orientation[3]) + "\n";
      if (n.ref_model) out += "ref_model = " + n.ref_model + "\n";
      out += draftKeys(n.keys);
    }
    for (const e of fx.emitters) {
      out += "\n[emitter " + e.name + "]\n";
      out += "update = " + e.update + "\nrender = " + e.render + "\nblend = " + e.blend + "\ntexture = " + e.texture + "\n";
      if (e.chunk) out += "chunk = " + e.chunk + "\n";
      out += "position = " + num(e.position[0]) + " " + num(e.position[1]) + " " + num(e.position[2]) + "\n";
      const o = e.orientation || [0, 0, 0, 0];
      out += "orientation = " + num(o[0]) + " " + num(o[1]) + " " + num(o[2]) + " " + num(o[3]) + "\n";
      if (e.parent) out += "parent = " + e.parent + "\n";
      out += "birthrate = " + num(e.birthrate) + "\nemit_seconds = " + num(e.emit_seconds) + "\nlife = " + num(e.life) + "\n";
      out += "velocity = " + num(e.velocity) + "\nrandvel = " + num(e.randvel) + "\nspread = " + num(e.spread) + "\n";
      out += "mass = " + num(e.mass) + "\nsize = " + num(e.size_start) + " " + num(e.size_end) + "\n";
      out += "color_start = " + num(e.color_start[0]) + " " + num(e.color_start[1]) + " " + num(e.color_start[2]) + "\n";
      out += "color_end = " + num(e.color_end[0]) + " " + num(e.color_end[1]) + " " + num(e.color_end[2]) + "\n";
      out += "alpha = " + num(e.alpha_start) + " " + num(e.alpha_end) + "\n";
      out += "area = " + num(e.xsize) + " " + num(e.ysize) + "\n";
      out += "grid = " + e.grid + "\nframe_end = " + e.frame_end + "\n";
      out += "fps = " + num(e.fps || 0) + "\nparticle_rot = " + num(e.particle_rot || 0) + "\n";
      out += "blur_length = " + num(e.blur_length) + "\nbounce_co = " + num(e.bounce_co) + "\n";
      out += "deadspace = " + num(e.deadspace || 0) + "\n";
      if (e.p2p) {
        out += "p2p = " + e.p2p + "\np2p_bezier = " + num(e.p2p_bezier2 || 0) + " " + num(e.p2p_bezier3 || 0) + "\n";
        out += "combinetime = " + num(e.combinetime || 0) + "\ngrav = " + num(e.grav || 0) + "\ndrag = " + num(e.drag || 0) + "\n";
        out += "threshold = " + num(e.threshold || 0) + "\n";
      }
      if (String(e.update).toLowerCase() === "lightning") {
        out += "lightning = " + num(e.lightning_delay || 0) + " " + num(e.lightning_radius || 0) + " " + num(e.lightning_scale || 0) + "\n";
      }
      let flags = "";
      if (e.inherit) flags += " inherit";
      if (e.splat) flags += " splat";
      if (e.bounce) flags += " bounce";
      if (e.random) flags += " random";
      if (e.inherit_local) flags += " inherit_local";
      if (e.inherit_part) flags += " inherit_part";
      if (e.twosidedtex) flags += " twosidedtex";
      out += "flags =" + flags + "\n";
      out += draftKeys(e.keys);
    }
    return out;
  }

  function readFloats(words, out, count) {
    for (let i = 0; i < count && i < words.length; i++) out[i] = toFloat(words[i]);
  }

  // The draft back; throws (with the line) where the game would refuse it.
  function readDraft(text) {
    const fx = effect();
    let e = null, nd = null;  // the section being read
    const lines = String(text).split("\n");
    for (let n = 0; n < lines.length; n++) {
      const raw = lines[n];
      const hash = raw.indexOf("#");
      const line = (hash >= 0 ? raw.slice(0, hash) : raw).trim();
      if (!line) continue;
      if (line[0] === "[") {
        const close = line.indexOf("]");
        const inner = (close < 0 ? line.slice(1) : line.slice(1, close)).trim();
        if (inner.toLowerCase().startsWith("emitter")) {
          e = emitter();
          nd = null;
          e.name = inner.slice(7).trim() || "emitter";
          fx.emitters.push(e);
        } else if (inner.toLowerCase().startsWith("node")) {
          nd = node();
          e = null;
          nd.name = inner.slice(4).trim() || "node";
          fx.nodes.push(nd);
        } else {
          throw new Error("line " + (n + 1) + ": unknown section [" + inner + "]");
        }
        continue;
      }
      const eq = line.indexOf("=");
      if (eq < 0) throw new Error("line " + (n + 1) + ": no '='");
      const key = line.slice(0, eq).trim().toLowerCase();
      const value = line.slice(eq + 1).trim();
      const words = value.split(/\s+/).filter(Boolean);
      if (nd) {
        if (key === "parent") nd.parent = value;
        else if (key === "position") readFloats(words, nd.position, 3);
        else if (key === "orientation") readFloats(words, nd.orientation, 4);
        else if (key === "ref_model") nd.ref_model = resref(value, 16);
        else draftKeyLine(key, words, nd.keys);
        continue;
      }
      if (!e) {
        if (key === "name") fx.name = value;
        else if (key === "type") fx.duration = value.toLowerCase() !== "f";
        else if (key === "seconds") fx.seconds = toFloat(value);
        else if (key === "attach") fx.attach = value.toLowerCase();
        else if (key === "lengths") readFloats(words, fx.lengths, 3);
        continue;
      }
      if (key === "update") e.update = value;
      else if (key === "render") e.render = value;
      else if (key === "blend") e.blend = value;
      else if (key === "texture") e.texture = value;
      else if (key === "chunk") e.chunk = resref(value, 16);
      else if (key === "parent") e.parent = value;
      else if (key === "position") readFloats(words, e.position, 3);
      else if (key === "orientation") readFloats(words, e.orientation, 4);
      else if (key === "birthrate") e.birthrate = toFloat(value);
      else if (key === "emit_seconds") e.emit_seconds = toFloat(value);
      else if (key === "life") e.life = toFloat(value);
      else if (key === "velocity") e.velocity = toFloat(value);
      else if (key === "randvel") e.randvel = toFloat(value);
      else if (key === "spread") e.spread = toFloat(value);
      else if (key === "mass") e.mass = toFloat(value);
      else if (key === "size") {
        const size = [e.size_start, e.size_end];
        readFloats(words, size, 2);
        [e.size_start, e.size_end] = size;
      } else if (key === "color_start") readFloats(words, e.color_start, 3);
      else if (key === "color_end") readFloats(words, e.color_end, 3);
      else if (key === "alpha") {
        const alpha = [e.alpha_start, e.alpha_end];
        readFloats(words, alpha, 2);
        [e.alpha_start, e.alpha_end] = alpha;
      } else if (key === "area") {
        const area = [e.xsize, e.ysize];
        readFloats(words, area, 2);
        [e.xsize, e.ysize] = area;
      } else if (key === "grid") e.grid = Math.max(1, toInt(value));
      else if (key === "frame_end") e.frame_end = Math.max(0, toInt(value));
      else if (key === "fps") e.fps = toFloat(value);
      else if (key === "particle_rot") e.particle_rot = toFloat(value);
      else if (key === "blur_length") e.blur_length = toFloat(value);
      else if (key === "bounce_co") e.bounce_co = toFloat(value);
      else if (key === "deadspace") e.deadspace = Math.min(Math.max(toFloat(value), 0), DEADSPACE_MAX);
      else if (key === "p2p") e.p2p = ["bezier", "gravity"].includes(value.toLowerCase()) ? value.toLowerCase() : "";
      else if (key === "p2p_bezier") {
        const bezier = [e.p2p_bezier2, e.p2p_bezier3];
        readFloats(words, bezier, 2);
        [e.p2p_bezier2, e.p2p_bezier3] = bezier;
      } else if (key === "combinetime") e.combinetime = toFloat(value);
      else if (key === "grav") e.grav = toFloat(value);
      else if (key === "drag") e.drag = toFloat(value);
      else if (key === "threshold") e.threshold = toFloat(value);
      else if (key === "lightning") {
        const bolt = [e.lightning_delay, e.lightning_radius, e.lightning_scale];
        readFloats(words, bolt, 3);
        [e.lightning_delay, e.lightning_radius, e.lightning_scale] = bolt;
      }
      else if (key === "flags") {
        for (const word of words) {
          const f = word.toLowerCase();
          e.inherit = e.inherit || f === "inherit";
          e.splat = e.splat || f === "splat";
          e.bounce = e.bounce || f === "bounce";
          e.random = e.random || f === "random";
          e.inherit_local = e.inherit_local || f === "inherit_local";
          e.inherit_part = e.inherit_part || f === "inherit_part";
          e.twosidedtex = e.twosidedtex || f === "twosidedtex";
        }
      } else draftKeyLine(key, words, e.keys);
    }
    return fx;
  }

  // A TGA (uncompressed or RLE; true colour or grey) as top-down RGBA.
  function decodeTga(bytes) {
    const b = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    if (b.length < 18) throw new Error("not a TGA");
    const idLength = b[0], mapType = b[1], type = b[2];
    const mapLength = b[5] | (b[6] << 8), mapDepth = b[7];
    const width = b[12] | (b[13] << 8), height = b[14] | (b[15] << 8);
    const depth = b[16], descriptor = b[17];
    const grey = type === 3 || type === 11;
    const rle = type === 10 || type === 11;
    if (![2, 3, 10, 11].includes(type) || !width || !height) throw new Error("TGA type " + type + " isn't read");
    const bpp = depth >> 3;
    if (!(grey ? bpp === 1 || bpp === 2 : bpp === 3 || bpp === 4)) throw new Error("TGA depth " + depth + " isn't read");
    let at = 18 + idLength + (mapType ? mapLength * ((mapDepth + 7) >> 3) : 0);
    const count = width * height;
    const out = new Uint8ClampedArray(count * 4);
    const put = (i, p) => {
      const o = i * 4;
      if (grey) {
        out[o] = out[o + 1] = out[o + 2] = b[p];
        out[o + 3] = bpp === 2 ? b[p + 1] : 255;
      } else {
        out[o] = b[p + 2];
        out[o + 1] = b[p + 1];
        out[o + 2] = b[p];
        out[o + 3] = bpp === 4 ? b[p + 3] : 255;
      }
    };
    let i = 0;
    if (!rle) {
      if (at + count * bpp > b.length) throw new Error("TGA cut short");
      for (; i < count; i++, at += bpp) put(i, at);
    } else {
      while (i < count && at < b.length) {
        const header = b[at++];
        const run = (header & 0x7f) + 1;
        if (header & 0x80) {
          for (let k = 0; k < run && i < count; k++) put(i++, at);
          at += bpp;
        } else {
          for (let k = 0; k < run && i < count; k++, at += bpp) put(i++, at);
        }
      }
    }
    // Rows bottom-up unless the descriptor says top-down (bit 5); right-to-left (bit 4) mirrored.
    const topDown = (descriptor & 0x20) !== 0, rightToLeft = (descriptor & 0x10) !== 0;
    if (topDown && !rightToLeft) return { width, height, data: out };
    const flipped = new Uint8ClampedArray(count * 4);
    for (let y = 0; y < height; y++) {
      const sy = topDown ? y : height - 1 - y;
      for (let x = 0; x < width; x++) {
        const sx = rightToLeft ? width - 1 - x : x;
        const s = (sy * width + sx) * 4, d = (y * width + x) * 4;
        flipped[d] = out[s]; flipped[d + 1] = out[s + 1]; flipped[d + 2] = out[s + 2]; flipped[d + 3] = out[s + 3];
      }
    }
    return { width, height, data: flipped };
  }

  const api = { UPDATES, RENDERS, BLENDS, ATTACHES, ANIMS, DEADSPACE_MAX, emitter, node, effect, findKeys, sparks, newEffect, clone, num, resref,
    writeDraft, readDraft, decodeTga };
  if (typeof module === "object" && module.exports) module.exports = api;
  else root.VfxCore = api;
})(typeof self !== "undefined" ? self : this);
