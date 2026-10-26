// VFX Bench - the core: nwn5e's VFX draft (key = value text), ported line for line
// from src/data/vfx_model.cpp's WriteVfxDraft / ReadVfxDraft / VfxResRef (keep the
// two in step: check.js reads and writes back every draft the game exported), the
// effect's defaults (vfx_editor.cpp's), and a TGA reader for the particle textures.
// Runs in the page and under node (server.js, check.js).
(function (root) {
  "use strict";

  // The values the game's own models use (a model's own spelling is kept as it is).
  const UPDATES = ["Fountain", "Single", "Explosion", "Lightning"];
  const RENDERS = ["Normal", "Linked", "Billboard_to_Local_Z", "Billboard_to_World_Z", "Aligned_to_World_Z",
    "Aligned_to_Particle_Dir", "Motion_Blur"];
  const BLENDS = ["Normal", "Lighten", "Punch-Through"];
  const ATTACHES = ["root", "head", "impact"];

  // data::VfxEmitter's defaults.
  function emitter(overrides) {
    return Object.assign({
      name: "emitter", update: "Fountain", render: "Normal", blend: "Normal", texture: "fxpa_sphere",
      position: [0, 0, 1], birthrate: 10, emit_seconds: 0, life: 1, velocity: 1, randvel: 0.5, spread: 0.8,
      mass: 0, size_start: 0.1, size_end: 0.1, color_start: [1, 1, 1], color_end: [1, 1, 1], alpha_start: 1,
      alpha_end: 0, xsize: 0, ysize: 0, grid: 1, frame_end: 0, blur_length: 0, bounce_co: 0,
      inherit: false, splat: false, bounce: false, random: false,
    }, overrides || {});
  }

  function effect(overrides) {
    return Object.assign({ name: "n5_new", duration: true, seconds: 8, attach: "root", emitters: [] }, overrides || {});
  }

  // vfx_editor.cpp's DefaultEmitter / DefaultEffect: orange sparks.
  function sparks() {
    return emitter({
      name: "sparks", texture: "fxpa_sphere", render: "Normal", blend: "Lighten", position: [0, 0, 1], birthrate: 20,
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
    for (const e of fx.emitters) {
      out += "\n[emitter " + e.name + "]\n";
      out += "update = " + e.update + "\nrender = " + e.render + "\nblend = " + e.blend + "\ntexture = " + e.texture + "\n";
      out += "position = " + num(e.position[0]) + " " + num(e.position[1]) + " " + num(e.position[2]) + "\n";
      out += "birthrate = " + num(e.birthrate) + "\nemit_seconds = " + num(e.emit_seconds) + "\nlife = " + num(e.life) + "\n";
      out += "velocity = " + num(e.velocity) + "\nrandvel = " + num(e.randvel) + "\nspread = " + num(e.spread) + "\n";
      out += "mass = " + num(e.mass) + "\nsize = " + num(e.size_start) + " " + num(e.size_end) + "\n";
      out += "color_start = " + num(e.color_start[0]) + " " + num(e.color_start[1]) + " " + num(e.color_start[2]) + "\n";
      out += "color_end = " + num(e.color_end[0]) + " " + num(e.color_end[1]) + " " + num(e.color_end[2]) + "\n";
      out += "alpha = " + num(e.alpha_start) + " " + num(e.alpha_end) + "\n";
      out += "area = " + num(e.xsize) + " " + num(e.ysize) + "\n";
      out += "grid = " + e.grid + "\nframe_end = " + e.frame_end + "\n";
      out += "blur_length = " + num(e.blur_length) + "\nbounce_co = " + num(e.bounce_co) + "\n";
      let flags = "";
      if (e.inherit) flags += " inherit";
      if (e.splat) flags += " splat";
      if (e.bounce) flags += " bounce";
      if (e.random) flags += " random";
      out += "flags =" + flags + "\n";
    }
    return out;
  }

  function readFloats(words, out, count) {
    for (let i = 0; i < count && i < words.length; i++) out[i] = toFloat(words[i]);
  }

  // The draft back; throws (with the line) where the game would refuse it.
  function readDraft(text) {
    const fx = effect();
    let e = null;
    const lines = String(text).split("\n");
    for (let n = 0; n < lines.length; n++) {
      const raw = lines[n];
      const hash = raw.indexOf("#");
      const line = (hash >= 0 ? raw.slice(0, hash) : raw).trim();
      if (!line) continue;
      if (line[0] === "[") {
        const close = line.indexOf("]");
        const inner = (close < 0 ? line.slice(1) : line.slice(1, close)).trim();
        if (!inner.toLowerCase().startsWith("emitter")) throw new Error("line " + (n + 1) + ": unknown section [" + inner + "]");
        e = emitter({ position: [0, 0, 1], color_start: [1, 1, 1], color_end: [1, 1, 1] });
        e.name = inner.slice(7).trim() || "emitter";
        fx.emitters.push(e);
        continue;
      }
      const eq = line.indexOf("=");
      if (eq < 0) throw new Error("line " + (n + 1) + ": no '='");
      const key = line.slice(0, eq).trim().toLowerCase();
      const value = line.slice(eq + 1).trim();
      const words = value.split(/\s+/).filter(Boolean);
      if (!e) {
        if (key === "name") fx.name = value;
        else if (key === "type") fx.duration = value.toLowerCase() !== "f";
        else if (key === "seconds") fx.seconds = toFloat(value);
        else if (key === "attach") fx.attach = value.toLowerCase();
        continue;
      }
      if (key === "update") e.update = value;
      else if (key === "render") e.render = value;
      else if (key === "blend") e.blend = value;
      else if (key === "texture") e.texture = value;
      else if (key === "position") readFloats(words, e.position, 3);
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
      else if (key === "blur_length") e.blur_length = toFloat(value);
      else if (key === "bounce_co") e.bounce_co = toFloat(value);
      else if (key === "flags") {
        for (const word of words) {
          const f = word.toLowerCase();
          e.inherit = e.inherit || f === "inherit";
          e.splat = e.splat || f === "splat";
          e.bounce = e.bounce || f === "bounce";
          e.random = e.random || f === "random";
        }
      }
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

  const api = { UPDATES, RENDERS, BLENDS, ATTACHES, emitter, effect, sparks, newEffect, clone, num, resref,
    writeDraft, readDraft, decodeTga };
  if (typeof module === "object" && module.exports) module.exports = api;
  else root.VfxCore = api;
})(typeof self !== "undefined" ? self : this);
