// Mocap mode (docs/PLAN.md "Mocap mode"; owner, 2026-10-28: "go ahead with mocap
// mode"): a FreeMoCap recording - MediaPipe's 33 body points per frame, in
// output_data/mediapipe_body_3d_xyz.csv - cleaned, put in game space and
// retargeted to each body library's bones, into a keyed animation saved as a
// keys recipe (core.js writeRecipe; nwn5e's anim_recipe.cpp reads it).
//
// - Game space: +X the performer's right, +Y forward, +Z up, metres, the floor
//   at 0. The up axis is the ankles-to-shoulders direction, right the shoulder
//   and hip lines, forward up x right - checked against the nose (in front of
//   the ears), so a mirrored (left-handed) recording still comes out right;
//   millimetres are recognised by the body's size.
// - Clean-up: gaps filled (straight lines), then smoothing (a Gaussian, sigma in
//   frames). FreeMoCap has already filtered the points (7 Hz) and filled short
//   gaps; this is for what's left. Bone lengths aren't held: the retarget uses
//   only directions.
// - Retarget, per library: the limbs aimed at where the performer's point
//   (absolute: an arm forward is an NWN arm forward) - shoulder > elbow > wrist >
//   knuckles (the hand's turn from the index and pinky), hip > knee > ankle,
//   heel > toes; the pelvis, the torso, the neck and the head turned by how much
//   the performer's turn from the standing reference (the T-pose at the take's
//   start): an NWN torso's rest leans (its neck sits behind the waist) where a
//   person's stands up, so their absolute directions don't match. The pelvis
//   tips 35% of the torso's lean (as the crouch step's hip hinge). The root
//   follows the hips' height (as a share of the standing height) and their sway
//   over the feet - travel across the floor is stripped.
// - Then: trim, blend in / out to the stance (pause1), a loop seam (cut where the
//   pose comes closest to the first, then crossfaded), fit to a length with the
//   contact moment ("hit": suggested where the hands move fastest) put at a
//   chosen time, and key reduction (keys dropped where the engine's
//   interpolation stays within 0.3 degrees / 3 mm).
(function (root) {
  "use strict";

  const LANDMARKS = [
    "nose", "left_eye_inner", "left_eye", "left_eye_outer", "right_eye_inner", "right_eye", "right_eye_outer",
    "left_ear", "right_ear", "mouth_left", "mouth_right", "left_shoulder", "right_shoulder", "left_elbow",
    "right_elbow", "left_wrist", "right_wrist", "left_pinky", "right_pinky", "left_index", "right_index",
    "left_thumb", "right_thumb", "left_hip", "right_hip", "left_knee", "right_knee", "left_ankle", "right_ankle",
    "left_heel", "right_heel", "left_foot_index", "right_foot_index",
  ];
  const P = {};
  LANDMARKS.forEach((n, i) => { P[n] = i; });
  const COUNT = LANDMARKS.length;
  // The capture figure: lines between points.
  const CONNECTIONS = [
    ["left_shoulder", "right_shoulder"], ["left_hip", "right_hip"], ["left_shoulder", "left_hip"], ["right_shoulder", "right_hip"],
    ["left_shoulder", "left_elbow"], ["left_elbow", "left_wrist"], ["left_wrist", "left_index"], ["left_wrist", "left_pinky"], ["left_index", "left_pinky"],
    ["right_shoulder", "right_elbow"], ["right_elbow", "right_wrist"], ["right_wrist", "right_index"], ["right_wrist", "right_pinky"], ["right_index", "right_pinky"],
    ["left_hip", "left_knee"], ["left_knee", "left_ankle"], ["left_ankle", "left_heel"], ["left_heel", "left_foot_index"], ["left_ankle", "left_foot_index"],
    ["right_hip", "right_knee"], ["right_knee", "right_ankle"], ["right_ankle", "right_heel"], ["right_heel", "right_foot_index"], ["right_ankle", "right_foot_index"],
    ["left_ear", "nose"], ["right_ear", "nose"], ["left_ear", "right_ear"],
  ].map(([a, b]) => [P[a], P[b]]);
  const NEEDED = ["left_shoulder", "right_shoulder", "left_elbow", "right_elbow", "left_wrist", "right_wrist", "left_hip",
    "right_hip", "left_knee", "right_knee", "left_ankle", "right_ankle", "left_ear", "right_ear", "nose"];

  // --- Vectors and quaternions (x y z w, as the libraries key them) ----------------------
  const add = (a, b) => [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
  const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
  const scale = (a, s) => [a[0] * s, a[1] * s, a[2] * s];
  const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  const len = (a) => Math.hypot(a[0], a[1], a[2]);
  const norm = (a) => { const l = len(a); return l > 1e-9 ? [a[0] / l, a[1] / l, a[2] / l] : [0, 0, 0]; };
  const mid = (a, b) => [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2];
  const IDENTITY = [0, 0, 0, 1];
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
    const a = norm(from), b = norm(to);
    const d = dot(a, b);
    if (d < -0.999999) {
      let axis = cross([1, 0, 0], a);
      if (len(axis) < 1e-6) axis = cross([0, 1, 0], a);
      return axisAngle(norm(axis), Math.PI);
    }
    const c = cross(a, b);
    return qnorm([c[0], c[1], c[2], 1 + d]);
  }
  // The rotation whose columns are the basis x, y, z (orthonormal, right-handed).
  function fromBasis(x, y, z) {
    const m00 = x[0], m10 = x[1], m20 = x[2], m01 = y[0], m11 = y[1], m21 = y[2], m02 = z[0], m12 = z[1], m22 = z[2];
    const trace = m00 + m11 + m22;
    let q;
    if (trace > 0) {
      const s = Math.sqrt(trace + 1) * 2;
      q = [(m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, s / 4];
    } else if (m00 > m11 && m00 > m22) {
      const s = Math.sqrt(1 + m00 - m11 - m22) * 2;
      q = [s / 4, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s];
    } else if (m11 > m22) {
      const s = Math.sqrt(1 + m11 - m00 - m22) * 2;
      q = [(m01 + m10) / s, s / 4, (m12 + m21) / s, (m02 - m20) / s];
    } else {
      const s = Math.sqrt(1 + m22 - m00 - m11) * 2;
      q = [(m02 + m20) / s, (m12 + m21) / s, s / 4, (m10 - m01) / s];
    }
    return qnorm(q);
  }
  // nlerp, as core.js samples keys (the shorter way round).
  function qmix(a, b, f) {
    const s = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3] < 0 ? -1 : 1;
    return qnorm([a[0] + (s * b[0] - a[0]) * f, a[1] + (s * b[1] - a[1]) * f, a[2] + (s * b[2] - a[2]) * f, a[3] + (s * b[3] - a[3]) * f]);
  }
  const qangle = (a, b) => 2 * Math.acos(Math.min(1, Math.abs(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3])));
  const smoothstep = (u) => { const x = Math.max(0, Math.min(1, u)); return x * x * (3 - 2 * x); };

  // --- Reading a recording ------------------------------------------------------------------
  // FreeMoCap's mediapipe_body_3d_xyz.csv (body_<point>_x ...), its saved_data
  // body_trajectories.csv ("# nose_x, nose_y ...") or a by-trajectory file with
  // hands and face too: the 33 body points are picked out by name.
  function parseCapture(text) {
    const lines = String(text).split(/\r?\n/).filter((l) => l.trim().length);
    if (lines.length < 3) throw new Error("not a capture: fewer than two frames");
    const header = lines[0].replace(/^\s*#\s*/, "").split(",").map((h) => h.trim().toLowerCase());
    const columns = new Array(COUNT * 3).fill(-1);
    header.forEach((h, c) => {
      const m = /^(?:body_)?([a-z_]+)_([xyz])$/.exec(h);
      if (!m || !(m[1] in P)) return;
      const slot = P[m[1]] * 3 + "xyz".indexOf(m[2]);
      if (columns[slot] < 0) columns[slot] = c;
    });
    const missing = NEEDED.filter((n) => columns[P[n] * 3] < 0 || columns[P[n] * 3 + 1] < 0 || columns[P[n] * 3 + 2] < 0);
    if (missing.length) throw new Error("these body points aren't in it: " + missing.join(", ") + " (FreeMoCap's output_data/mediapipe_body_3d_xyz.csv has them)");
    const frames = [];
    for (let i = 1; i < lines.length; i++) {
      const cells = lines[i].split(",");
      const frame = new Float64Array(COUNT * 3);
      for (let s = 0; s < COUNT * 3; s++) {
        const c = columns[s];
        const v = c < 0 ? NaN : parseFloat(cells[c]);
        frame[s] = Number.isFinite(v) ? v : NaN;
      }
      frames.push(frame);
    }
    return { frames, columns: header.length };
  }

  // FreeMoCap's output_data/recording_parameters.json: the frame rate, or null.
  function frameRateFrom(text) {
    try {
      const j = JSON.parse(text);
      const r = j && j.post_processing_parameters_model && j.post_processing_parameters_model.framerate;
      return typeof r === "number" && r > 1 && r < 1000 ? r : null;
    } catch (e) {
      return null;
    }
  }

  const point = (frame, i) => [frame[i * 3], frame[i * 3 + 1], frame[i * 3 + 2]];
  const valid = (v) => Number.isFinite(v[0]) && Number.isFinite(v[1]) && Number.isFinite(v[2]);

  // Gaps filled with straight lines (the ends held), per coordinate.
  function fillGaps(frames) {
    const n = frames.length;
    let filled = 0;
    for (let s = 0; s < COUNT * 3; s++) {
      let last = -1;
      for (let f = 0; f < n; f++) {
        if (!Number.isFinite(frames[f][s])) continue;
        if (last < 0 && f > 0) for (let g = 0; g < f; g++) { frames[g][s] = frames[f][s]; filled++; }
        else if (last >= 0 && f - last > 1) {
          for (let g = last + 1; g < f; g++) {
            frames[g][s] = frames[last][s] + ((frames[f][s] - frames[last][s]) * (g - last)) / (f - last);
            filled++;
          }
        }
        last = f;
      }
      if (last >= 0) for (let g = last + 1; g < n; g++) { frames[g][s] = frames[last][s]; filled++; }
    }
    return filled;
  }

  function smooth(frames, sigma) {
    if (!(sigma > 0.05)) return frames;
    const radius = Math.ceil(sigma * 3);
    const weights = [];
    for (let k = -radius; k <= radius; k++) weights.push(Math.exp(-(k * k) / (2 * sigma * sigma)));
    const n = frames.length;
    const out = frames.map((f) => new Float64Array(f.length));
    for (let f = 0; f < n; f++) {
      for (let s = 0; s < COUNT * 3; s++) {
        let sum = 0, w = 0;
        for (let k = -radius; k <= radius; k++) {
          const g = f + k;
          if (g < 0 || g >= n) continue;
          sum += frames[g][s] * weights[k + radius];
          w += weights[k + radius];
        }
        out[f][s] = sum / w;
      }
    }
    return out;
  }

  function percentile(values, p) {
    const sorted = values.filter(Number.isFinite).sort((a, b) => a - b);
    if (!sorted.length) return 0;
    return sorted[Math.min(sorted.length - 1, Math.max(0, Math.round((sorted.length - 1) * p)))];
  }

  // The recording in game space: { fps, frames (Float64Array per frame, metres),
  // up / right / forward (the recording's axes), unit, mirrored, floor, reference
  // (frames [a, b) averaged: the standing pose), hipHeight (standing), notes }.
  // options: fps (30), smoothing (sigma, frames; 0), reference: [start, end] s.
  function prepare(capture, options) {
    const opt = Object.assign({ fps: 30, smoothing: 0, reference: [0, 0.33] }, options || {});
    const raw = capture.frames.map((f) => Float64Array.from(f));
    if (raw.length < 2) throw new Error("a capture needs two frames or more");
    const notes = [];
    const gaps = fillGaps(raw);
    if (gaps) notes.push(gaps + " missing values filled");
    // Size: millimetres when the body is metres long in thousands.
    const heights = raw.map((f) => len(sub(mid(point(f, P.left_shoulder), point(f, P.right_shoulder)), mid(point(f, P.left_ankle), point(f, P.right_ankle)))));
    const body = percentile(heights, 0.5);
    const unit = body > 50 ? 0.001 : body > 5 ? 0.01 : 1;
    // Axes, from the standing reference (the T-pose at the take's start): up is the
    // floor's - the plane through both heels and both toes - when those points look
    // right (within 25 degrees of the body's ankles-to-shoulders line), else the
    // body's; right is the shoulder and hip lines. From the body alone, up leans with
    // it (a few degrees: centimetres at the hands).
    const fps0 = opt.fps > 1 ? opt.fps : 30;
    const r0 = Math.max(0, Math.min(raw.length - 1, Math.round((opt.reference ? opt.reference[0] : 0) * fps0)));
    const r1 = Math.max(r0 + 1, Math.min(raw.length, Math.round((opt.reference ? opt.reference[1] : 0.33) * fps0)));
    const avg = (i) => { let v = [0, 0, 0]; for (let f = r0; f < r1; f++) v = add(v, point(raw[f], i)); return scale(v, 1 / (r1 - r0)); };
    let bodyUp = norm(sub(mid(avg(P.left_shoulder), avg(P.right_shoulder)), mid(avg(P.left_ankle), avg(P.right_ankle))));
    let up = bodyUp;
    const heelL = avg(P.left_heel), heelR = avg(P.right_heel), toeL = avg(P.left_foot_index), toeR = avg(P.right_foot_index);
    if ([heelL, heelR, toeL, toeR].every(valid)) {
      const across = sub(mid(heelR, toeR), mid(heelL, toeL));
      const along = sub(mid(toeL, toeR), mid(heelL, heelR));
      let floorUp = norm(cross(across, along));
      if (dot(floorUp, bodyUp) < 0) floorUp = scale(floorUp, -1);
      const off = (Math.acos(Math.max(-1, Math.min(1, dot(floorUp, bodyUp)))) * 180) / Math.PI;
      if (len(across) * unit > 0.05 && len(along) * unit > 0.05 && off < 25) {
        up = floorUp;
        notes.push("up from the floor under the feet (" + off.toFixed(1) + " degrees from the body's line)");
      } else {
        notes.push("up from the body's line (the feet's points don't make a floor)");
      }
    }
    let right = add(norm(sub(avg(P.right_shoulder), avg(P.left_shoulder))), norm(sub(avg(P.right_hip), avg(P.left_hip))));
    if (opt.axes) {  // given (a check that knows them; a manual fix)
      up = norm(opt.axes.up);
      right = opt.axes.right;
      notes.length = 0;
      notes.push("axes given");
    }
    right = norm(sub(right, scale(up, dot(right, up))));
    let forward = cross(up, right);
    let nose = dot(sub(avg(P.nose), mid(avg(P.left_ear), avg(P.right_ear))), forward);
    const mirrored = nose < 0;
    if (mirrored) { forward = scale(forward, -1); notes.push("a mirrored (left-handed) recording: turned the right way"); }
    // Into game space, the hips of the first frame over the origin.
    const first = mid(point(raw[0], P.left_hip), point(raw[0], P.right_hip));
    let frames = raw.map((f) => {
      const g = new Float64Array(COUNT * 3);
      for (let i = 0; i < COUNT; i++) {
        const d = sub(point(f, i), first);
        g[i * 3] = dot(d, right) * unit;
        g[i * 3 + 1] = dot(d, forward) * unit;
        g[i * 3 + 2] = dot(d, up) * unit;
      }
      return g;
    });
    frames = smooth(frames, opt.smoothing);
    // The floor: the feet's low points (5th percentile), to 0.
    const lows = frames.map((f) => Math.min(...["left_heel", "right_heel", "left_foot_index", "right_foot_index", "left_ankle", "right_ankle"]
      .filter((n) => P[n] !== undefined).map((n) => f[P[n] * 3 + 2])));
    const floor = percentile(lows, 0.05);
    for (const f of frames) for (let i = 0; i < COUNT; i++) f[i * 3 + 2] -= floor;
    const fps = opt.fps > 1 ? opt.fps : 30;
    const take = { fps, frames, up, right, forward, unit, mirrored, floor: floor, duration: (frames.length - 1) / fps, notes };
    setReference(take, opt.reference);
    return take;
  }

  // The standing reference: frames [start, end) s, averaged.
  function setReference(take, reference) {
    const n = take.frames.length;
    let a = Math.max(0, Math.min(n - 1, Math.round((reference ? reference[0] : 0) * take.fps)));
    let b = Math.max(a + 1, Math.min(n, Math.round((reference ? reference[1] : 0.33) * take.fps)));
    const avg = new Float64Array(COUNT * 3);
    for (let f = a; f < b; f++) for (let s = 0; s < COUNT * 3; s++) avg[s] += take.frames[f][s] / (b - a);
    take.reference = { start: a / take.fps, end: b / take.fps, pose: avg };
    take.hipHeight = mid(point(avg, P.left_hip), point(avg, P.right_hip))[2];
  }

  // The points at time t (interpolated), for drawing.
  function pointsAt(take, t) {
    const x = Math.max(0, Math.min(take.frames.length - 1, t * take.fps));
    const a = Math.floor(x), b = Math.min(take.frames.length - 1, a + 1), u = x - a;
    const out = [];
    for (let i = 0; i < COUNT; i++) {
      const pa = point(take.frames[a], i), pb = point(take.frames[b], i);
      out.push([pa[0] + (pb[0] - pa[0]) * u, pa[1] + (pb[1] - pa[1]) * u, pa[2] + (pb[2] - pa[2]) * u]);
    }
    return out;
  }

  // --- The capture's body frames ------------------------------------------------------------
  const WORLD_UP = [0, 0, 1];
  function landmarks(f) {
    const p = (n) => point(f, P[n]);
    const L = {
      lsh: p("left_shoulder"), rsh: p("right_shoulder"), lel: p("left_elbow"), rel: p("right_elbow"),
      lwr: p("left_wrist"), rwr: p("right_wrist"), lhip: p("left_hip"), rhip: p("right_hip"),
      lkn: p("left_knee"), rkn: p("right_knee"), lan: p("left_ankle"), ran: p("right_ankle"),
      lear: p("left_ear"), rear: p("right_ear"), nose: p("nose"),
      lidx: p("left_index"), ridx: p("right_index"), lpin: p("left_pinky"), rpin: p("right_pinky"),
      lheel: p("left_heel"), rheel: p("right_heel"), ltoe: p("left_foot_index"), rtoe: p("right_foot_index"),
    };
    L.hips = mid(L.lhip, L.rhip);
    L.shoulders = mid(L.lsh, L.rsh);
    L.ears = mid(L.lear, L.rear);
    L.feet = mid(L.lan, L.ran);
    return L;
  }
  // Right / forward / up frames (a rotation) of the pelvis, the torso and the head.
  function frameOf(x, upHint) {
    const xr = norm(x);
    const y = norm(cross(upHint, xr));
    const z = cross(xr, y);
    return fromBasis(xr, y, z);
  }
  // The torso turns about the waist, where NWN's torso and pelvis pivot - above the
  // hip joints: the hips' midpoint plus the rig's rest offset (at the performer's
  // size), turned with the pelvis since the reference. Aimed from the hips instead,
  // the pelvis's own turn leaked into the torso (the shoulders 3-4 cm off).
  function bodyFrames(L, waistOffset, refPelvis) {
    const spine = norm(sub(L.shoulders, L.hips));
    const pelvisUp = norm(add(scale(WORLD_UP, 0.65), scale(spine, 0.35)));
    const pelvis = frameOf(sub(L.rhip, L.lhip), pelvisUp);
    const turn = refPelvis ? qmul(pelvis, qconj(refPelvis)) : IDENTITY;
    const waist = waistOffset ? add(L.hips, rotate(turn, waistOffset)) : L.hips;
    const headRight = norm(sub(L.rear, L.lear));
    let face = sub(L.nose, L.ears);
    face = norm(sub(face, scale(headRight, dot(face, headRight))));
    return {
      pelvis,
      torso: frameOf(sub(L.rsh, L.lsh), sub(L.shoulders, waist)),
      head: fromBasis(headRight, face, cross(headRight, face)),
      neck: norm(sub(L.ears, L.shoulders)),
    };
  }

  // --- The library's bones ---------------------------------------------------------------------
  const lower = (s) => String(s).toLowerCase();
  // The bones; `stance` (pause1) gives the neutral: what a performer standing in the
  // reference maps to - the pelvis, torso, neck and head turn from there. The bind
  // pose won't do: a_dfa's binds the pelvis turned 22 degrees, the torso -13.
  function rig(bones, stance) {
    const index = new Map(bones.map((b, i) => [lower(b.name), i]));
    const rest = bones.map((b) => (b.orientation && b.orientation.values && b.orientation.values.length ? qnorm(b.orientation.values[0]) : IDENTITY));
    const restPos = bones.map((b) => (b.position && b.position.values && b.position.values.length ? b.position.values[0].slice(0, 3) : [0, 0, 0]));
    const world = new Array(bones.length), worldPos = new Array(bones.length);
    for (let i = 0; i < bones.length; i++) {
      const p = bones[i].parent;
      world[i] = p < 0 ? rest[i] : qmul(world[p], rest[i]);
      worldPos[i] = p < 0 ? restPos[i] : add(worldPos[p], rotate(world[p], restPos[i]));
    }
    // The hip joints' height at rest (the thighs' tops): what a performer's hips match.
    const thighs = ["lthigh_g", "rthigh_g"].map((n) => index.get(n)).filter((i) => i !== undefined);
    const hipHeight = thighs.length ? thighs.reduce((s, i) => s + worldPos[i][2], 0) / thighs.length : 1;
    // The waist (the pelvis's pivot) from the hip joints' midpoint, at rest.
    const pelvis = index.get("pelvis_g");
    const hips = thighs.length === 2 ? mid(worldPos[thighs[0]], worldPos[thighs[1]]) : null;
    const waistOffset = pelvis !== undefined && hips ? sub(worldPos[pelvis], hips) : [0, 0, 0];
    const stanceLocal = rest.slice();
    if (stance) {
      for (const node of stance.nodes) {
        const k = index.get(lower(node.name));
        if (k !== undefined && node.orientation && node.orientation.times.length) stanceLocal[k] = qnorm(node.orientation.values[0]);
      }
    }
    const neutral = new Array(bones.length);
    for (let i = 0; i < bones.length; i++) neutral[i] = bones[i].parent < 0 ? stanceLocal[i] : qmul(neutral[bones[i].parent], stanceLocal[i]);
    return { bones, index, rest, restPos, world, worldPos, hipHeight, waistOffset, neutral, stanceLocal };
  }
  // The bones driven, and how.
  const DRIVEN = [
    { bone: "pelvis_g", kind: "frame", frame: "pelvis" },
    { bone: "lthigh_g", kind: "aim", from: "lhip", to: "lkn", child: "lshin_g" },
    { bone: "lshin_g", kind: "aim", from: "lkn", to: "lan", child: "lfoot_g" },
    { bone: "lfoot_g", kind: "foot", heel: "lheel", toe: "ltoe" },
    { bone: "rthigh_g", kind: "aim", from: "rhip", to: "rkn", child: "rshin_g" },
    { bone: "rshin_g", kind: "aim", from: "rkn", to: "ran", child: "rfoot_g" },
    { bone: "rfoot_g", kind: "foot", heel: "rheel", toe: "rtoe" },
    { bone: "torso_g", kind: "frame", frame: "torso" },
    { bone: "neck_g", kind: "neck", child: "head_g" },
    { bone: "head_g", kind: "frame", frame: "head" },
    { bone: "lbicep_g", kind: "aim", from: "lsh", to: "lel", child: "lforearm_g" },
    { bone: "lforearm_g", kind: "aim", from: "lel", to: "lwr", child: "lhand_g" },
    { bone: "lhand_g", kind: "hand", wrist: "lwr", index: "lidx", pinky: "lpin", child: "lhand" },
    { bone: "rbicep_g", kind: "aim", from: "rsh", to: "rel", child: "rforearm_g" },
    { bone: "rforearm_g", kind: "aim", from: "rel", to: "rwr", child: "rhand_g" },
    { bone: "rhand_g", kind: "hand", wrist: "rwr", index: "ridx", pinky: "rpin", child: "rhand" },
  ];

  // The bone's rest direction toward its child, in its own frame.
  function childDirection(R, bone, child) {
    const c = R.index.get(child);
    if (c === undefined || R.bones[c].parent !== R.index.get(bone)) return null;
    const d = R.restPos[c];
    return len(d) > 1e-6 ? norm(d) : null;
  }

  // One frame's local rotations (by bone index) and the root's position.
  function solveFrame(R, f, ref) {
    const L = landmarks(f);
    const frames = bodyFrames(L, ref.waist, ref.frames.pelvis);
    const local = new Map();
    const world = R.world.slice();
    const worldOf = (i) => world[i];
    const order = DRIVEN.map((d) => Object.assign({ i: R.index.get(d.bone) }, d)).filter((d) => d.i !== undefined).sort((a, b) => a.i - b.i);
    for (const d of order) {
      const i = d.i, parent = R.bones[i].parent;
      const pw = parent < 0 ? IDENTITY : worldOf(parent);
      const unturned = qmul(pw, R.rest[i]);  // where it is with its rest local turn under the parent's solve
      let w = unturned;
      if (d.kind === "frame") {
        // Turned by the performer's turn since the standing reference: C(t) C(ref)^-1 W_neutral.
        w = qmul(qmul(frames[d.frame], qconj(ref.frames[d.frame])), R.neutral[i]);
      } else if (d.kind === "neck") {
        const dir = childDirection(R, d.bone, d.child);
        if (dir) {
          const restWorldDir = rotate(R.neutral[i], dir);
          const target = rotate(fromTo(ref.frames.neck, frames.neck), restWorldDir);
          w = qmul(fromTo(rotate(unturned, dir), target), unturned);
        }
      } else if (d.kind === "aim" || d.kind === "hand") {
        const dir = childDirection(R, d.bone, d.child);
        const target = d.kind === "hand" ? sub(mid(L[d.index], L[d.pinky]), L[d.wrist]) : sub(L[d.to], L[d.from]);
        if (dir && len(target) > 1e-6) {
          w = qmul(fromTo(rotate(unturned, dir), target), unturned);
          if (d.kind === "hand") {
            // The hand's turn about its length: the index side forward at rest.
            const axis = norm(target);
            const across = rotate(w, rotate(qconj(R.world[i]), [0, 1, 0]));
            const want = sub(L[d.index], L[d.pinky]);
            const a = norm(sub(across, scale(axis, dot(across, axis))));
            const b = norm(sub(want, scale(axis, dot(want, axis))));
            if (len(a) > 0.5 && len(b) > 0.5) {
              const angle = Math.atan2(dot(cross(a, b), axis), dot(a, b));
              w = qmul(axisAngle(axis, angle), w);
            }
          }
        }
      } else if (d.kind === "foot") {
        const target = sub(L[d.toe], L[d.heel]);
        if (len(target) > 1e-6) {
          const forward = rotate(qconj(R.world[i]), [0, 1, 0]);
          w = qmul(fromTo(rotate(unturned, forward), target), unturned);
        }
      }
      world[i] = w;
      local.set(i, qnorm(qmul(qconj(pw), w)));
      // Children not driven keep their rest turn under this one.
      for (let c = i + 1; c < R.bones.length; c++) {
        const p = R.bones[c].parent;
        if (p >= 0 && !order.some((o) => o.i === c)) world[c] = qmul(world[p], R.rest[c]);
      }
    }
    return { local, L };
  }

  // The root's position: its rest place, lowered / raised with the hips (as a share
  // of the standing hip height) and swayed with the hips over the feet.
  function rootPosition(R, L, take, refL, sway) {
    const i = R.index.get("rootdummy");
    if (i === undefined) return null;
    const rest = R.restPos[i];
    const s = take.hipHeight > 0.1 ? R.hipHeight / take.hipHeight : 1;
    const z = rest[2] + (L.hips[2] - take.hipHeight) * s;
    if (!sway) return [rest[0], rest[1], z];
    const now = sub(L.hips, L.feet), then = sub(refL.hips, refL.feet);
    return [rest[0] + (now[0] - then[0]) * s, rest[1] + (now[1] - then[1]) * s, z];
  }

  // --- The retarget -----------------------------------------------------------------------------
  // options: name, start / end (s; end 0 = the end), sway (true), stance (a clip:
  // pause1, for the blends), blendIn / blendOut (s), loop (false), loopBlend (s),
  // fitLength (s; 0 keeps it), contact (s in the take) and contactAt (s in the
  // result), transtime (0.25), reduce (true), tolerance (degrees; 0.75 - and 4 mm
  // for the root, scaled alike). The clip's takeTime(t) is its time in the take.
  function retarget(take, bones, options) {
    const opt = Object.assign({ name: "n5_mocap", start: 0, end: 0, sway: true, stance: null, blendIn: 0, blendOut: 0,
      loop: false, loopBlend: 0.25, fitLength: 0, contact: null, contactAt: null, transtime: 0.25, reduce: true,
      tolerance: 0.75 }, options || {});
    const tolerance = Math.max(0.01, opt.tolerance);
    const R = rig(bones, opt.stance);
    const refL = landmarks(take.reference.pose);
    const waist = scale(R.waistOffset, take.hipHeight > 0.1 ? take.hipHeight / R.hipHeight : 1);
    const ref = { waist, frames: bodyFrames(refL, waist, null) };
    const n = take.frames.length;
    let a = Math.max(0, Math.min(n - 2, Math.round(opt.start * take.fps)));
    let b = opt.end > 0 ? Math.max(a + 1, Math.min(n - 1, Math.round(opt.end * take.fps))) : n - 1;
    // Solve every frame kept.
    const driven = [];
    const rootIndex = R.index.get("rootdummy");
    const solved = [];
    for (let f = a; f <= b; f++) {
      const { local, L } = solveFrame(R, take.frames[f], ref);
      solved.push({ local, root: rootPosition(R, L, take, refL, opt.sway) });
      if (!driven.length) for (const k of local.keys()) driven.push(k);
    }
    // The loop: cut where the pose comes closest to the first (in the last 40%), then crossfade.
    if (opt.loop && solved.length > 6) {
      let best = solved.length - 1, bestCost = Infinity;
      for (let f = Math.floor(solved.length * 0.6); f < solved.length; f++) {
        let cost = 0;
        for (const k of driven) cost += qangle(solved[f].local.get(k), solved[0].local.get(k));
        if (cost < bestCost) { bestCost = cost; best = f; }
      }
      solved.length = best + 1;
    }
    const count = solved.length;
    const length = Math.max((count - 1) / take.fps, 1 / take.fps);
    const timeOf = (k) => k / take.fps;
    // The stance (pause1 at 0): the blends' ends.
    const stanceLocal = new Map(), stanceRoot = rootIndex !== undefined ? R.restPos[rootIndex] : null;
    let stanceRootPos = stanceRoot;
    for (const k of driven) stanceLocal.set(k, R.rest[k]);
    if (opt.stance) {
      for (const node of opt.stance.nodes) {
        const k = R.index.get(lower(node.name));
        if (k === undefined) continue;
        if (stanceLocal.has(k) && node.orientation && node.orientation.times.length) stanceLocal.set(k, qnorm(node.orientation.values[0]));
        if (k === rootIndex && node.position && node.position.times.length) stanceRootPos = node.position.values[0].slice(0, 3);
      }
    }
    const tracks = new Map(driven.map((k) => [k, { times: [], values: [] }]));
    const rootTrack = { times: [], values: [] };
    for (let k = 0; k < count; k++) {
      const t = timeOf(k);
      let w = 1;  // 1: the capture; 0: the stance
      if (opt.blendIn > 0) w = Math.min(w, smoothstep(t / opt.blendIn));
      if (opt.blendOut > 0) w = Math.min(w, smoothstep((length - t) / opt.blendOut));
      const loopW = opt.loop && opt.loopBlend > 0 ? smoothstep((t - (length - opt.loopBlend)) / opt.loopBlend) : 0;
      for (const j of driven) {
        let q = solved[k].local.get(j);
        if (loopW > 0) q = qmix(q, solved[0].local.get(j), loopW);
        if (w < 1) q = qmix(stanceLocal.get(j), q, w);
        tracks.get(j).times.push(t);
        tracks.get(j).values.push(q);
      }
      if (solved[k].root) {
        let p = solved[k].root;
        if (loopW > 0 && solved[0].root) p = p.map((v, c) => v + (solved[0].root[c] - v) * loopW);
        if (w < 1 && stanceRootPos) p = p.map((v, c) => stanceRootPos[c] + (v - stanceRootPos[c]) * w);
        rootTrack.times.push(t);
        rootTrack.values.push(p);
      }
    }
    // Fit to a length, the contact moment put at contactAt (two straight time segments).
    let outLength = length;
    let warp = (t) => t, unwarp = (t) => t;
    if (opt.fitLength > 0) {
      outLength = opt.fitLength;
      const c = opt.contact !== null && opt.contact !== undefined ? opt.contact - a / take.fps : null;
      const at = opt.contactAt !== null && opt.contactAt !== undefined ? opt.contactAt : null;
      if (c !== null && at !== null && c > 0 && c < length && at > 0 && at < outLength) {
        warp = (t) => (t <= c ? (t / c) * at : at + ((t - c) / (length - c)) * (outLength - at));
        unwarp = (u) => (u <= at ? (u / at) * c : c + ((u - at) / (outLength - at)) * (length - c));
      } else {
        warp = (t) => (t / length) * outLength;
        unwarp = (u) => (u / outLength) * length;
      }
    }
    const nodes = [];
    for (const [j, track] of tracks) {
      const node = { name: R.bones[j].name, parent: R.bones[j].parent, orientation: { times: track.times.map(warp), values: track.values } };
      if (opt.reduce) node.orientation = reduceTrack(node.orientation, true, (tolerance * Math.PI) / 180);
      nodes.push(node);
    }
    if (rootIndex !== undefined && rootTrack.times.length) {
      let position = { times: rootTrack.times.map(warp), values: rootTrack.values };
      if (opt.reduce) position = reduceTrack(position, false, 0.004 * (tolerance / 0.75));
      nodes.unshift({ name: R.bones[rootIndex].name, parent: R.bones[rootIndex].parent, position });
    }
    const start = a / take.fps;
    return { name: opt.name, length: outLength, transtime: opt.transtime, animroot: "rootdummy", nodes,
             source: { start, end: (a + count - 1) / take.fps }, takeTime: (t) => start + unwarp(Math.max(0, Math.min(outLength, t))) };
  }

  // --- Contact and key reduction ---------------------------------------------------------------
  // The moment the hands move fastest in [start, end] s (the "hit" of a shove or a swing).
  function suggestContact(take, start, end) {
    const n = take.frames.length;
    const a = Math.max(1, Math.round((start || 0) * take.fps));
    const b = end > 0 ? Math.min(n - 1, Math.round(end * take.fps)) : n - 1;
    let best = a, bestSpeed = -1;
    for (let f = a; f <= b; f++) {
      let speed = 0;
      for (const w of [P.left_wrist, P.right_wrist]) speed = Math.max(speed, len(sub(point(take.frames[f], w), point(take.frames[f - 1], w))));
      if (speed > bestSpeed) { bestSpeed = speed; best = f; }
    }
    return best / take.fps;
  }

  // Keys dropped where interpolating their neighbours stays within `tolerance`
  // (radians for orientations, metres for positions): Douglas-Peucker.
  function reduceTrack(track, quaternion, tolerance) {
    const n = track.times.length;
    if (n <= 2) return track;
    const keep = new Uint8Array(n);
    keep[0] = keep[n - 1] = 1;
    const error = (i, a, b) => {
      const u = (track.times[i] - track.times[a]) / Math.max(track.times[b] - track.times[a], 1e-9);
      if (quaternion) return qangle(qmix(track.values[a], track.values[b], u), track.values[i]);
      const p = track.values[a].map((v, c) => v + (track.values[b][c] - v) * u);
      return Math.hypot(p[0] - track.values[i][0], p[1] - track.values[i][1], p[2] - track.values[i][2]);
    };
    const stack = [[0, n - 1]];
    while (stack.length) {
      const [a, b] = stack.pop();
      let worst = -1, at = -1;
      for (let i = a + 1; i < b; i++) { const e = error(i, a, b); if (e > worst) { worst = e; at = i; } }
      if (worst > tolerance && at > 0) { keep[at] = 1; stack.push([a, at], [at, b]); }
    }
    const out = { times: [], values: [] };
    for (let i = 0; i < n; i++) if (keep[i]) { out.times.push(track.times[i]); out.values.push(track.values[i]); }
    return out;
  }

  const api = { LANDMARKS, CONNECTIONS, parseCapture, frameRateFrom, prepare, setReference, pointsAt, retarget,
                suggestContact, reduceTrack, rig, solveFrame, landmarks, bodyFrames };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.Mocap = api;
})(typeof window !== "undefined" ? window : this);
