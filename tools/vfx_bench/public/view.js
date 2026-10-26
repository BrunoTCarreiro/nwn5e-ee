// VFX Bench - the stage: WebGL, an orbiting camera, the ground, the game's 2 m human with
// its three effect nodes, and the particles as the game draws them (quads per render mode,
// blends, culling - PartEmitter::RenderEmitter, read 0.42.2). No libraries.
(function (root) {
  "use strict";
  const { ATTACH_HEIGHT } = root.VfxSim;

  // --- matrices (column-major, as WebGL takes them) ---------------------------------
  function perspective(fovy, aspect, near, far) {
    const f = 1 / Math.tan(fovy / 2), nf = 1 / (near - far);
    return [f / aspect, 0, 0, 0, 0, f, 0, 0, 0, 0, (far + near) * nf, -1, 0, 0, 2 * far * near * nf, 0];
  }
  function sub(a, b) { return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]; }
  function cross(a, b) { return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]; }
  function dot(a, b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
  function len(a) { return Math.hypot(a[0], a[1], a[2]); }
  function norm(a) { const l = len(a) || 1; return [a[0] / l, a[1] / l, a[2] / l]; }
  function scale(a, s) { return [a[0] * s, a[1] * s, a[2] * s]; }
  // v turned by `angle` about the unit vector k (Rodrigues).
  function rotateAbout(v, k, angle) {
    const c = Math.cos(angle), s = Math.sin(angle), kv = cross(k, v), d = dot(k, v) * (1 - c);
    return [v[0] * c + kv[0] * s + k[0] * d, v[1] * c + kv[1] * s + k[1] * d, v[2] * c + kv[2] * s + k[2] * d];
  }
  // The game's render modes (PartEmitter::Initialize: exact names; anything else draws as 0).
  const RENDER_MODES = { Normal: 0, Billboard_to_Local_Z: 1, Billboard_to_World_Z: 2, Linked: 3, Aligned_to_World_Z: 4,
    Aligned_to_Particle_Dir: 5, Motion_Blur: 6 };
  function lookAt(eye, target, up) {
    const z = norm(sub(eye, target)), x = norm(cross(up, z)), y = cross(z, x);
    return [x[0], y[0], z[0], 0, x[1], y[1], z[1], 0, x[2], y[2], z[2], 0, -dot(x, eye), -dot(y, eye), -dot(z, eye), 1];
  }
  function multiply(a, b) {
    const out = new Array(16);
    for (let c = 0; c < 4; c++) for (let r = 0; r < 4; r++) {
      let s = 0;
      for (let k = 0; k < 4; k++) s += a[k * 4 + r] * b[c * 4 + k];
      out[c * 4 + r] = s;
    }
    return out;
  }

  const PARTICLE_VS = `
attribute vec3 a_pos; attribute vec2 a_uv; attribute vec4 a_color;
uniform mat4 u_mvp; varying vec2 v_uv; varying vec4 v_color;
void main() { v_uv = a_uv; v_color = a_color; gl_Position = u_mvp * vec4(a_pos, 1.0); }`;
  const PARTICLE_FS = `
precision mediump float;
uniform sampler2D u_tex; uniform float u_cut; varying vec2 v_uv; varying vec4 v_color;
void main() { vec4 c = texture2D(u_tex, v_uv) * v_color; if (c.a <= u_cut) discard; gl_FragColor = c; }`;
  const LINE_VS = `
attribute vec3 a_pos; attribute vec4 a_color; uniform mat4 u_mvp; varying vec4 v_color;
void main() { v_color = a_color; gl_Position = u_mvp * vec4(a_pos, 1.0); }`;
  const LINE_FS = `
precision mediump float; varying vec4 v_color; void main() { gl_FragColor = v_color; }`;

  class View {
    constructor(canvas, loadTexture) {
      this.canvas = canvas;
      const gl = canvas.getContext("webgl", { antialias: true, premultipliedAlpha: false, alpha: false });
      if (!gl) throw new Error("This browser has no WebGL.");
      this.gl = gl;
      this.loadTexture = loadTexture;  // name -> Promise<{width, height, data}>
      this.textures = new Map();       // name -> { texture, ready, missing }
      this.particleProgram = this.program(PARTICLE_VS, PARTICLE_FS);
      this.lineProgram = this.program(LINE_VS, LINE_FS);
      this.buffer = gl.createBuffer();
      this.lineBuffer = gl.createBuffer();
      this.dot = this.makeDot();
      this.camera = { yaw: -0.9, pitch: 0.3, distance: 5, target: [0, 0, 1] };
      this.backdrop = [0.047, 0.067, 0.082];
      this.showFigure = true;
      this.attach = "root";
      this.bindPointer();
    }

    program(vs, fs) {
      const gl = this.gl;
      const shader = (type, source) => {
        const s = gl.createShader(type);
        gl.shaderSource(s, source);
        gl.compileShader(s);
        if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(s));
        return s;
      };
      const p = gl.createProgram();
      gl.attachShader(p, shader(gl.VERTEX_SHADER, vs));
      gl.attachShader(p, shader(gl.FRAGMENT_SHADER, fs));
      gl.linkProgram(p);
      if (!gl.getProgramParameter(p, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(p));
      const info = { program: p, attrib: {}, uniform: {} };
      for (const name of ["a_pos", "a_uv", "a_color"]) info.attrib[name] = gl.getAttribLocation(p, name);
      for (const name of ["u_mvp", "u_tex", "u_cut"]) info.uniform[name] = gl.getUniformLocation(p, name);
      return info;
    }

    upload(image) {
      const gl = this.gl;
      const t = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, t);
      gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, image.width, image.height, 0, gl.RGBA, gl.UNSIGNED_BYTE,
        new Uint8Array(image.data.buffer, image.data.byteOffset, image.data.byteLength));
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      return t;
    }

    // A soft white dot: a texture the library doesn't have.
    makeDot() {
      const size = 32, data = new Uint8ClampedArray(size * size * 4);
      for (let y = 0; y < size; y++) for (let x = 0; x < size; x++) {
        const d = Math.hypot(x + 0.5 - size / 2, y + 0.5 - size / 2) / (size / 2);
        const a = Math.max(0, 1 - d) ** 1.5;
        const o = (y * size + x) * 4;
        data[o] = data[o + 1] = data[o + 2] = 255;
        data[o + 3] = Math.round(a * 255);
      }
      return this.upload({ width: size, height: size, data });
    }

    texture(name) {
      const key = String(name || "").toLowerCase();
      let entry = this.textures.get(key);
      if (!entry) {
        entry = { texture: null, ready: false, missing: false };
        this.textures.set(key, entry);
        this.loadTexture(key).then((image) => {
          entry.texture = this.upload(image);
          entry.ready = true;
        }).catch(() => { entry.missing = true; });
      }
      return entry.ready ? entry.texture : this.dot;
    }

    textureMissing(name) {
      const entry = this.textures.get(String(name || "").toLowerCase());
      return !!(entry && entry.missing);
    }

    bindPointer() {
      const c = this.canvas;
      let drag = null;
      c.addEventListener("pointerdown", (e) => { drag = { x: e.clientX, y: e.clientY, pan: e.button !== 0 || e.shiftKey }; c.setPointerCapture(e.pointerId); });
      c.addEventListener("pointermove", (e) => {
        if (!drag) return;
        const dx = e.clientX - drag.x, dy = e.clientY - drag.y;
        drag.x = e.clientX; drag.y = e.clientY;
        const cam = this.camera;
        if (drag.pan) {
          const right = [Math.cos(cam.yaw), Math.sin(cam.yaw), 0];
          const k = cam.distance * 0.0016;
          cam.target[0] -= right[0] * dx * k;
          cam.target[1] -= right[1] * dx * k;
          cam.target[2] = Math.max(-1, Math.min(6, cam.target[2] + dy * k));
        } else {
          cam.yaw -= dx * 0.008;
          cam.pitch = Math.max(-0.2, Math.min(1.45, cam.pitch + dy * 0.006));
        }
      });
      c.addEventListener("pointerup", () => { drag = null; });
      c.addEventListener("contextmenu", (e) => e.preventDefault());
      c.addEventListener("wheel", (e) => {
        e.preventDefault();
        this.camera.distance = Math.max(1, Math.min(40, this.camera.distance * Math.exp(e.deltaY * 0.0012)));
      }, { passive: false });
      c.addEventListener("dblclick", () => this.resetCamera());
    }

    resetCamera() { this.camera = { yaw: -0.9, pitch: 0.3, distance: 5, target: [0, 0, 1] }; }

    frame() {
      const gl = this.gl, c = this.canvas;
      const dpr = Math.min(2, window.devicePixelRatio || 1);
      const w = Math.max(1, Math.round(c.clientWidth * dpr)), h = Math.max(1, Math.round(c.clientHeight * dpr));
      if (c.width !== w || c.height !== h) { c.width = w; c.height = h; }
      gl.viewport(0, 0, w, h);
      const cam = this.camera;
      const eye = [
        cam.target[0] + cam.distance * Math.cos(cam.pitch) * Math.cos(cam.yaw - Math.PI / 2),
        cam.target[1] + cam.distance * Math.cos(cam.pitch) * Math.sin(cam.yaw - Math.PI / 2),
        cam.target[2] + cam.distance * Math.sin(cam.pitch),
      ];
      const view = lookAt(eye, cam.target, [0, 0, 1]);
      const mvp = multiply(perspective(Math.PI / 4, w / h, 0.05, 300), view);
      // The camera's right, up and back (towards the viewer) - the game's camera X, Y, Z.
      return { mvp, eye, right: [view[0], view[4], view[8]], up: [view[1], view[5], view[9]], back: [view[2], view[6], view[10]] };
    }

    lines(vertices, mvp) {
      const gl = this.gl, p = this.lineProgram;
      gl.useProgram(p.program);
      gl.uniformMatrix4fv(p.uniform.u_mvp, false, mvp);
      gl.bindBuffer(gl.ARRAY_BUFFER, this.lineBuffer);
      gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(vertices), gl.STREAM_DRAW);
      gl.enableVertexAttribArray(p.attrib.a_pos);
      gl.vertexAttribPointer(p.attrib.a_pos, 3, gl.FLOAT, false, 28, 0);
      gl.enableVertexAttribArray(p.attrib.a_color);
      gl.vertexAttribPointer(p.attrib.a_color, 4, gl.FLOAT, false, 28, 12);
      gl.drawArrays(gl.LINES, 0, vertices.length / 7);
      gl.disableVertexAttribArray(p.attrib.a_color);
    }

    groundAndFigure(figure, mvp, nodes) {
      const v = [];
      const line = (a, b, c) => v.push(a[0], a[1], a[2], c[0], c[1], c[2], c[3], b[0], b[1], b[2], c[0], c[1], c[2], c[3]);
      const grid = [0.5, 0.6, 0.68, 0.16], axis = [0.5, 0.6, 0.68, 0.35];
      for (let i = -6; i <= 6; i++) {
        line([i, -6, 0], [i, 6, 0], i === 0 ? axis : grid);
        line([-6, i, 0], [6, i, 0], i === 0 ? axis : grid);
      }
      if (this.showFigure) {
        // The game's human male (pmh0) in its rest pose, about 2 m: its joints' heights.
        const body = [0.75, 0.8, 0.86, 0.75];
        const c = Math.cos(figure.heading), s = Math.sin(figure.heading);
        const at = (x, y, z) => [figure.x + x * c - y * s, figure.y + x * s + y * c, z];
        for (const side of [-1, 1]) {
          line(at(side * 0.108, 0, 1.025), at(side * 0.077, 0, 0.564), body);  // thigh
          line(at(side * 0.077, 0, 0.564), at(side * 0.057, 0, 0.137), body);  // shin
          line(at(side * 0.057, 0, 0.137), at(side * 0.06, 0, 0.0), body);     // foot
          line(at(side * 0.201, 0, 1.557), at(side * 0.24, 0, 1.255), body);   // upper arm
          line(at(side * 0.24, 0, 1.255), at(side * 0.273, 0, 0.964), body);   // forearm
          line(at(side * 0.273, 0, 0.964), at(side * 0.284, 0, 0.868), body);  // hand
        }
        line(at(-0.108, 0, 1.025), at(0.108, 0, 1.025), body);  // hips
        line(at(0, 0, 1.025), at(0, 0, 1.676), body);           // spine
        line(at(-0.201, 0, 1.557), at(0.201, 0, 1.557), body);  // shoulders
        line(at(0, 0, 1.676), at(0, 0, 1.753), body);           // neck
        for (let k = 0; k < 16; k++) {                           // head, its top at 2 m
          const a0 = (k / 16) * 2 * Math.PI, a1 = ((k + 1) / 16) * 2 * Math.PI;
          line(at(0.115 * Math.cos(a0), 0, 1.875 + 0.125 * Math.sin(a0)), at(0.115 * Math.cos(a1), 0, 1.875 + 0.125 * Math.sin(a1)), body);
        }
        for (const name of Object.keys(ATTACH_HEIGHT)) {  // the three nodes; the effect's own lit
          const z = ATTACH_HEIGHT[name];
          const color = name === this.attach ? [0.95, 0.63, 0.24, 1] : [0.5, 0.6, 0.68, 0.45];
          const r = name === this.attach ? 0.09 : 0.05;
          line(at(-r, 0, z), at(r, 0, z), color);
          line(at(0, -r, z), at(0, r, z), color);
          line(at(0, 0, z - r), at(0, 0, z + r), color);
        }
      }
      // The effect's nodes where they are now (a small cross each, its x axis longer).
      if (this.showFigure) {
        const qrot = window.VfxSim.qrot, mark = [0.45, 0.75, 0.95, 0.85];
        for (const n of nodes) {
          const x = qrot(n.q, [0.12, 0, 0]), y = qrot(n.q, [0, 0.06, 0]), z = qrot(n.q, [0, 0, 0.06]);
          const at = (d, s) => [n.pos[0] + d[0] * s, n.pos[1] + d[1] * s, n.pos[2] + d[2] * s];
          line(at(x, -0.5), at(x, 1), mark);
          line(at(y, -1), at(y, 1), mark);
          line(at(z, -1), at(z, 1), mark);
        }
      }
      this.lines(v, mvp);
    }

    // One emitter's particles as quads (x y z u v r g b a, six vertices each), built as the
    // game's PartEmitter::RenderEmitter builds them (0.42.2): corners C +- A.W +- B.H (tail P2
    // for Motion_Blur / Linked), +A to the cell's right, +B to its top; the front is A x B and
    // the back isn't drawn (twosidedtex is never on in the drafts). Modes by their exact
    // names: Normal 0 faces the screen; Billboard_to_Local_Z 1 lies flat in the emitter's
    // turn; Billboard_to_World_Z 2 lies flat facing up; Linked 3 joins each particle to the
    // next; Aligned_to_World_Z 4 stands upright; Aligned_to_Particle_Dir 5 lies along the
    // particle's own frame; Motion_Blur 6 streaks from its trail point; anything else: 0.
    quads(entry, cam) {
      const e = entry.emitter;
      const mode = Object.prototype.hasOwnProperty.call(RENDER_MODES, e.render) ? RENDER_MODES[e.render] : 0;
      const grid = entry.grid;
      const out = [];
      const vertex = (v, u, w, c) => out.push(v[0], v[1], v[2], u, w, c[0], c[1], c[2], c[3]);
      // The cell of a sheet frame: u left / right, v top / bottom (the TGA's top row is v 0 here).
      const uvOf = (frame) => {
        if (frame < 0) return [0, 0, 1, 1];
        const row = Math.floor(frame / grid), col = frame - row * grid;
        return [col / grid, row / grid, (col + 1) / grid, (row + 1) / grid];
      };
      const quad = (C, P2, A, B, W, H, uv, head, tail) => {
        if (dot(cross(A, B), sub(cam.eye, C)) <= 0) return;  // its back: culled
        const Aw = scale(A, W), Bh = scale(B, H);
        const v0 = [C[0] + Aw[0] + Bh[0], C[1] + Aw[1] + Bh[1], C[2] + Aw[2] + Bh[2]];
        const v1 = [C[0] - Aw[0] + Bh[0], C[1] - Aw[1] + Bh[1], C[2] - Aw[2] + Bh[2]];
        const v2 = [P2[0] + Aw[0] - Bh[0], P2[1] + Aw[1] - Bh[1], P2[2] + Aw[2] - Bh[2]];
        const v5 = [P2[0] - Aw[0] - Bh[0], P2[1] - Aw[1] - Bh[1], P2[2] - Aw[2] - Bh[2]];
        vertex(v0, uv[2], uv[1], head); vertex(v1, uv[0], uv[1], head); vertex(v2, uv[2], uv[3], tail);
        vertex(v2, uv[2], uv[3], tail); vertex(v1, uv[0], uv[1], head); vertex(v5, uv[0], uv[3], tail);
      };
      // A streak's axes in the view plane: B along it, A across (Motion_Blur and Linked).
      const streak = (d) => {
        const u = dot(cam.right, d), v = dot(cam.up, d), l = Math.hypot(u, v);
        if (!l) return null;
        return { B: [cam.right[0] * u / l + cam.up[0] * v / l, cam.right[1] * u / l + cam.up[1] * v / l, cam.right[2] * u / l + cam.up[2] * v / l],
          A: [cam.right[0] * v / l - cam.up[0] * u / l, cam.right[1] * v / l - cam.up[1] * u / l, cam.right[2] * v / l - cam.up[2] * u / l], l };
      };
      const ps = entry.particles;  // oldest first, not sorted (the game doesn't)
      if (mode === 3) {
        for (let i = 0; i + 1 < ps.length; i++) {
          const a = ps[i], b = ps[i + 1], s2 = streak(sub(b.pos, a.pos));
          if (s2) quad(b.pos, a.pos, s2.A, s2.B, 2 * a.w, a.h / 2, uvOf(a.frame), a.color, a.color);
        }
        return out;
      }
      // The emitter's turn now (its keys and its parents' - 0.42.3).
      const qrot = window.VfxSim.qrot, q = entry.place ? entry.place.q : [0, 0, 0, 1];
      const eX = qrot(q, [1, 0, 0]), eY = qrot(q, [0, 1, 0]), eZ = qrot(q, [0, 0, 1]);
      for (const p of ps) {
        if (p.color[3] <= 0 || p.w <= 0) continue;
        const uv = uvOf(p.frame);
        if (mode === 6) {
          const s2 = streak(sub(p.pos, p.trail));
          if (!s2) continue;  // no streak yet: nothing to see
          const W = 2 * p.w;
          const tail = [p.color[0], p.color[1], p.color[2], Math.trunc(p.alpha255 * Math.min(1, W / s2.l)) / 255];
          quad(p.pos, p.trail, s2.A, s2.B, W, p.h / 2, uv, p.color, tail);
          continue;
        }
        if (mode === 5) {
          quad(p.pos, p.pos, p.xaxis, p.dir, p.w, p.h, uv, p.color, p.color);
          continue;
        }
        // 0, 1, 2, 4: spun by the particle's spin about the frame's normal.
        const c = Math.cos(p.spin), s = Math.sin(p.spin);
        let A;
        if (p.stopped) A = rotateAbout(p.xaxis, p.dir, p.spin);  // resting: in its own (ground) frame
        else if (mode === 1) A = [eX[0] * c + eY[0] * s, eX[1] * c + eY[1] * s, eX[2] * c + eY[2] * s];
        else if (mode === 2) A = [c, s, 0];
        else A = [cam.right[0] * c + cam.up[0] * s, cam.right[1] * c + cam.up[1] * s, cam.right[2] * c + cam.up[2] * s];
        let B;
        if (mode === 4) B = [0, 0, cam.up[2] < 0 ? -1 : 1];
        else if (e.splat) B = cross(p.dir, A);
        else B = cross(mode === 1 ? eZ : mode === 2 ? [0, 0, 1] : cam.back, A);
        quad(p.pos, p.pos, A, B, p.w, p.h, uv, p.color, p.color);
      }
      return out;
    }

    draw(snapshot, figure, nodes) {
      const gl = this.gl;
      const cam = this.frame();
      gl.clearColor(this.backdrop[0], this.backdrop[1], this.backdrop[2], 1);
      gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
      gl.enable(gl.DEPTH_TEST);
      gl.depthMask(true);
      gl.enable(gl.BLEND);
      gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
      this.groundAndFigure(figure, cam.mvp, nodes || []);
      // The emitters in their order (all on one character: the game leaves their order to its
      // sort), each one's particles oldest first.
      const entries = snapshot.filter((s) => s.particles.length && String(s.emitter.update).toLowerCase() !== "lightning");
      const p = this.particleProgram;
      gl.useProgram(p.program);
      gl.uniformMatrix4fv(p.uniform.u_mvp, false, cam.mvp);
      gl.uniform1i(p.uniform.u_tex, 0);
      gl.activeTexture(gl.TEXTURE0);
      gl.bindBuffer(gl.ARRAY_BUFFER, this.buffer);
      for (const entry of entries) {
        const vertices = this.quads(entry, cam);
        if (!vertices.length) continue;
        // Blend by the exact name: "Normal" covers, "Punch-Through" cuts out (no blending, depth
        // written, gone at alpha 0.2 or less), anything else ("Lighten") adds light.
        const blend = entry.emitter.blend;
        if (blend === "Punch-Through") {
          gl.disable(gl.BLEND); gl.depthMask(true); gl.uniform1f(p.uniform.u_cut, 0.2);
        } else if (blend === "Normal") {
          gl.enable(gl.BLEND); gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA); gl.depthMask(false); gl.uniform1f(p.uniform.u_cut, -1);
        } else {
          gl.enable(gl.BLEND); gl.blendFunc(gl.SRC_ALPHA, gl.ONE); gl.depthMask(false); gl.uniform1f(p.uniform.u_cut, -1);
        }
        gl.bindTexture(gl.TEXTURE_2D, this.texture(entry.emitter.texture));
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(vertices), gl.STREAM_DRAW);
        gl.enableVertexAttribArray(p.attrib.a_pos);
        gl.vertexAttribPointer(p.attrib.a_pos, 3, gl.FLOAT, false, 36, 0);
        gl.enableVertexAttribArray(p.attrib.a_uv);
        gl.vertexAttribPointer(p.attrib.a_uv, 2, gl.FLOAT, false, 36, 12);
        gl.enableVertexAttribArray(p.attrib.a_color);
        gl.vertexAttribPointer(p.attrib.a_color, 4, gl.FLOAT, false, 36, 20);
        gl.drawArrays(gl.TRIANGLES, 0, vertices.length / 9);
        gl.disableVertexAttribArray(p.attrib.a_uv);
        gl.disableVertexAttribArray(p.attrib.a_color);
      }
      gl.depthMask(true);
      gl.enable(gl.BLEND);
      gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
    }
  }

  root.VfxView = { View };
})(typeof self !== "undefined" ? self : this);
