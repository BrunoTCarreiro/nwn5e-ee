// VFX Bench - the stage: WebGL, an orbiting camera, the ground, a 1.8 m figure with its
// three effect nodes, and the particles as textured quads in the emitter's render mode
// and blend. No libraries.
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
void main() { vec4 c = texture2D(u_tex, v_uv) * v_color; if (c.a < u_cut) discard; gl_FragColor = c; }`;
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
      return { mvp, eye, right: [view[0], view[4], view[8]], up: [view[1], view[5], view[9]] };
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

    groundAndFigure(figure, mvp) {
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
      this.lines(v, mvp);
    }

    // One emitter's particles as quads: x y z u v r g b a, six vertices each.
    quads(entry, cam) {
      const e = entry.emitter;
      const render = String(e.render).toLowerCase();
      const grid = Math.max(1, e.grid | 0);
      const out = [];
      const toCamera = (p) => norm(sub(cam.eye, p));
      const push = (center, right, up, uv, color) => {
        const corners = [[-1, -1], [1, -1], [1, 1], [-1, -1], [1, 1], [-1, 1]];
        for (const [sx, sy] of corners) {
          out.push(center[0] + right[0] * sx + up[0] * sy, center[1] + right[1] * sx + up[1] * sy,
            center[2] + right[2] * sx + up[2] * sy,
            sx < 0 ? uv[0] : uv[2], sy < 0 ? uv[3] : uv[1], color[0], color[1], color[2], color[3]);
        }
      };
      const uvOf = (frame) => {
        const f = Math.max(0, Math.min(grid * grid - 1, frame | 0));
        const col = f % grid, row = Math.floor(f / grid);
        return [col / grid, row / grid, (col + 1) / grid, (row + 1) / grid];  // left, top, right, bottom
      };
      let particles = entry.particles;
      if (render === "linked") {
        particles = particles.slice().sort((a, b) => a.id - b.id);
        for (let i = 0; i + 1 < particles.length; i++) {
          const a = particles[i], b = particles[i + 1];
          const along = sub(b.pos, a.pos);
          if (len(along) < 1e-5) continue;
          const mid = scale([a.pos[0] + b.pos[0], a.pos[1] + b.pos[1], a.pos[2] + b.pos[2]], 0.5);
          const side = norm(cross(along, toCamera(mid)));
          push(mid, scale(side, Math.max(a.size, b.size) / 2), scale(along, 0.5), uvOf(a.frame), a.color);
        }
        return out;
      }
      if (String(e.blend).toLowerCase() === "normal") {  // back to front
        particles = particles.slice().sort((a, b) => dot(sub(b.pos, cam.eye), sub(b.pos, cam.eye)) - dot(sub(a.pos, cam.eye), sub(a.pos, cam.eye)));
      }
      for (const p of particles) {
        if (p.size <= 0 || p.color[3] <= 0) continue;
        const half = p.size / 2;
        let right, up;
        const speed = len(p.vel);
        if (p.flat || render === "aligned_to_world_z") {
          right = [half, 0, 0]; up = [0, half, 0];
        } else if (render === "billboard_to_world_z" || render === "billboard_to_local_z") {
          const toCam = toCamera(p.pos);
          right = scale(norm([-toCam[1], toCam[0], 0]), half); up = [0, 0, half];
        } else if ((render === "aligned_to_particle_dir" || render === "motion_blur") && speed > 1e-4) {
          const dir = scale(p.vel, 1 / speed);
          const stretch = render === "motion_blur" ? half + speed * Math.max(0, e.blur_length) / 2 : half;
          right = scale(norm(cross(dir, toCamera(p.pos))), half); up = scale(dir, stretch);
        } else {
          right = scale(cam.right, half); up = scale(cam.up, half);
        }
        push(p.pos, right, up, uvOf(p.frame), p.color);
      }
      return out;
    }

    draw(snapshot, figure) {
      const gl = this.gl;
      const cam = this.frame();
      gl.clearColor(this.backdrop[0], this.backdrop[1], this.backdrop[2], 1);
      gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
      gl.enable(gl.DEPTH_TEST);
      gl.depthMask(true);
      gl.enable(gl.BLEND);
      gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
      this.groundAndFigure(figure, cam.mvp);
      // Punch-through first (it writes depth), then the see-through ones, then the added light.
      const order = (blend) => (blend === "punch-through" ? 0 : blend === "lighten" ? 2 : 1);
      const entries = snapshot.filter((s) => s.particles.length && String(s.emitter.update).toLowerCase() !== "lightning")
        .sort((a, b) => order(String(a.emitter.blend).toLowerCase()) - order(String(b.emitter.blend).toLowerCase()));
      const p = this.particleProgram;
      gl.useProgram(p.program);
      gl.uniformMatrix4fv(p.uniform.u_mvp, false, cam.mvp);
      gl.uniform1i(p.uniform.u_tex, 0);
      gl.activeTexture(gl.TEXTURE0);
      gl.bindBuffer(gl.ARRAY_BUFFER, this.buffer);
      for (const entry of entries) {
        const vertices = this.quads(entry, cam);
        if (!vertices.length) continue;
        const blend = String(entry.emitter.blend).toLowerCase();
        if (blend === "punch-through") {
          gl.disable(gl.BLEND); gl.depthMask(true); gl.uniform1f(p.uniform.u_cut, 0.5);
        } else if (blend === "lighten") {
          gl.enable(gl.BLEND); gl.blendFunc(gl.SRC_ALPHA, gl.ONE); gl.depthMask(false); gl.uniform1f(p.uniform.u_cut, 0.002);
        } else {
          gl.enable(gl.BLEND); gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA); gl.depthMask(false); gl.uniform1f(p.uniform.u_cut, 0.002);
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
      gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
    }
  }

  root.VfxView = { View };
})(typeof self !== "undefined" ? self : this);
