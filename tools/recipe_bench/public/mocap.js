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
//
// #74 (owner, 2026-10-29: a FreeMoCap test take - "I want to see what can be done
// with it, we also probably need to trim it and clean it up"): FreeMoCap's newer
// RTMPose output read too - output_data/rtmpose_body_3d_xyz.csv, a row a point a
// frame ("frame,keypoint,x,y,z", COCO-WholeBody's names: the big toes are
// MediaPipe's foot_index points), its hands in their own files (the index and
// pinky knuckles give the hand's turn), its frame times from
// synchronized_videos/timestamps/<take>_timestamps.csv (webcams run unevenly:
// 10-17 a second) - resampled to an even rate. Clean-up gains a glitch filter (a
// point that jumps away from its neighbours for a frame or two is dropped and
// filled), and the take a motion profile and a suggested trim and standing window.
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
  // RTMPose (COCO-WholeBody) names that differ from MediaPipe's; its hand files' points
  // that stand for MediaPipe's hand points; points a tracker may not give, and what
  // stands in for them (they're only drawn).
  const ALIASES = { left_big_toe: "left_foot_index", right_big_toe: "right_foot_index" };
  const HAND_POINTS = { forefinger1: "index", pinky_finger1: "pinky", thumb2: "thumb" };
  const STAND_INS = {
    left_eye_inner: "left_eye", left_eye_outer: "left_eye", right_eye_inner: "right_eye", right_eye_outer: "right_eye",
    left_eye: "nose", right_eye: "nose", mouth_left: "nose", mouth_right: "nose",
    left_thumb: "left_index", right_thumb: "right_index", left_heel: "left_ankle", right_heel: "right_ankle",
    left_foot_index: "left_ankle", right_foot_index: "right_ankle",
  };

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
    if (isLong(header)) return parseLong(lines, header);
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
    const timeColumn = header.indexOf("time_s");  // the points round trip's files (#77): each frame's time
    const times = timeColumn >= 0 ? [] : null;
    // And the frames marked standing / kept (pointsCsv's marks), first to last, and the
    // feet planted (#79), as runs.
    const markColumns = timeColumn >= 0 ? { standing: header.indexOf("standing"), keep: header.indexOf("keep"),
      left_planted: header.indexOf("left_planted"), right_planted: header.indexOf("right_planted") } : {};
    const marked = { standing: [], keep: [], left_planted: [], right_planted: [] };
    for (let i = 1; i < lines.length; i++) {
      const cells = lines[i].split(",");
      const frame = new Float64Array(COUNT * 3);
      for (let s = 0; s < COUNT * 3; s++) {
        const c = columns[s];
        const v = c < 0 ? NaN : parseFloat(cells[c]);
        frame[s] = Number.isFinite(v) ? v : NaN;
      }
      for (const m of Object.keys(markColumns)) if (markColumns[m] >= 0 && parseFloat(cells[markColumns[m]]) > 0) marked[m].push(frames.length);
      frames.push(frame);
      if (times) times.push(parseFloat(cells[timeColumn]));
    }
    const capture = { frames, columns: header.length, format: timeColumn >= 0 ? "points" : "mediapipe" };
    if (times && times.every(Number.isFinite) && times.every((t, k) => k === 0 || t > times[k - 1])) capture.times = times;
    const marks = {};
    for (const m of ["standing", "keep"]) if (marked[m].length) marks[m] = [marked[m][0], marked[m][marked[m].length - 1]];
    if (markColumns.left_planted >= 0 || markColumns.right_planted >= 0) {
      const runs = (rows) => {
        const out = [];
        for (const k of rows) { const last = out[out.length - 1]; if (last && k === last[1] + 1) last[1] = k; else out.push([k, k]); }
        return out;
      };
      marks.plants = { left: runs(marked.left_planted), right: runs(marked.right_planted) };  // none marked: none (not suggested)
    }
    if (Object.keys(marks).length) capture.marks = marks;
    return capture;
  }

  // A row a point a frame: "frame,keypoint,x,y,z" (FreeMoCap's RTMPose files).
  const isLong = (header) => ["frame", "keypoint", "x", "y", "z"].every((h) => header.includes(h));
  const num = (s) => { const v = parseFloat(s); return Number.isFinite(v) ? v : NaN; };
  function readLong(lines, header, each) {
    const fi = header.indexOf("frame"), ki = header.indexOf("keypoint");
    const xi = header.indexOf("x"), yi = header.indexOf("y"), zi = header.indexOf("z");
    let last = -1;
    for (let i = 1; i < lines.length; i++) {
      const c = lines[i].split(",");
      const f = parseInt(c[fi], 10);
      if (!(f >= 0) || c.length <= Math.max(fi, ki, xi, yi, zi)) continue;
      each(f, c[ki].trim().toLowerCase(), [num(c[xi]), num(c[yi]), num(c[zi])]);
      if (f > last) last = f;
    }
    return last + 1;
  }
  function parseLong(lines, header) {
    const byFrame = new Map();
    const seen = new Set();
    const count = readLong(lines, header, (f, name, v) => {
      const n = ALIASES[name] || name;
      if (!(n in P)) return;
      let frame = byFrame.get(f);
      if (!frame) { frame = new Float64Array(COUNT * 3).fill(NaN); byFrame.set(f, frame); }
      frame.set(v, P[n] * 3);
      if (valid(v)) seen.add(n);
    });
    const missing = NEEDED.filter((n) => !seen.has(n));
    if (missing.length) throw new Error("these body points aren't in it: " + missing.join(", ") + " (FreeMoCap's output_data/rtmpose_body_3d_xyz.csv has them)");
    const frames = [];
    for (let f = 0; f < count; f++) frames.push(byFrame.get(f) || new Float64Array(COUNT * 3).fill(NaN));
    return { frames, columns: header.length, format: "rtmpose" };
  }

  // A hand file (rtmpose_left_hand_3d_xyz.csv): its index and pinky knuckles and its
  // thumb into the capture's hand points. side: "left" / "right". The frames filled.
  function addHand(capture, text, side) {
    const lines = String(text).split(/\r?\n/).filter((l) => l.trim().length);
    const header = (lines[0] || "").split(",").map((h) => h.trim().toLowerCase());
    if (!isLong(header)) throw new Error("not a FreeMoCap hand file (frame,keypoint,x,y,z)");
    let filled = 0;
    const done = new Set();
    readLong(lines, header, (f, name, v) => {
      const short = name.replace(/^(left|right)_hand_/, "");
      const what = HAND_POINTS[short];
      if (!what || f >= capture.frames.length || !valid(v)) return;
      capture.frames[f].set(v, P[side + "_" + what] * 3);
      if (!done.has(f)) { done.add(f); filled++; }
    });
    capture.hands = Object.assign({}, capture.hands, { [side]: filled });
    return filled;
  }

  // synchronized_videos/timestamps/<take>_timestamps.csv: each frame's time from the
  // recording's start (seconds), or null.
  function timesFrom(text) {
    const lines = String(text).split(/\r?\n/).filter((l) => l.trim().length);
    if (lines.length < 3) return null;
    const header = lines[0].split(",").map((h) => h.trim().toLowerCase());
    const c = header.findIndex((h) => h.startsWith("timestamp.from_recording_start"));
    if (c < 0) return null;
    const times = [];
    for (let i = 1; i < lines.length; i++) times.push(num(lines[i].split(",")[c]));
    return times.every(Number.isFinite) ? times : null;
  }

  // Even frames from uneven ones (times in seconds, rising): straight lines between.
  function resample(frames, times, fps) {
    const t0 = times[0], t1 = times[times.length - 1];
    const n = Math.max(2, Math.floor((t1 - t0) * fps + 1e-6) + 1);
    const out = [];
    let j = 0;
    for (let k = 0; k < n; k++) {
      const t = t0 + k / fps;
      while (j < times.length - 2 && times[j + 1] < t) j++;
      const u = Math.max(0, Math.min(1, (t - times[j]) / Math.max(times[j + 1] - times[j], 1e-9)));
      const a = frames[j], b = frames[j + 1];
      const f = new Float64Array(a.length);
      for (let s = 0; s < a.length; s++) f[s] = a[s] + (b[s] - a[s]) * u;
      out.push(f);
    }
    return out;
  }

  // Glitches, dropped (NaN) and then filled: a point whose bone from its parent joint is
  // suddenly stretched or squashed - by more than 30% (and 6 cm) of its length in the
  // frames around it (two each side) - for a frame: a fast real move keeps its bones, a
  // tracking slip doesn't (the owner's test take: an ankle a metre off for a frame, its
  // shin at 1.6x); the frames around, not the whole take, because two webcams' bones
  // drift slowly with the pose (that take's shin 35-49 cm). And, for any point, one
  // further than `limit` (a metre) from the median of its neighbours - only that far:
  // at 13 frames a second a stab's peak sits 40 cm and more off its neighbours, and a
  // nearer limit cut it off. Lengths and limits in the capture's units (`metre`: a
  // metre in them).
  const BONES = [["left_shoulder", "left_elbow"], ["left_elbow", "left_wrist"], ["right_shoulder", "right_elbow"],
    ["right_elbow", "right_wrist"], ["left_hip", "left_knee"], ["left_knee", "left_ankle"], ["right_hip", "right_knee"],
    ["right_knee", "right_ankle"], ["left_ankle", "left_heel"], ["left_ankle", "left_foot_index"], ["right_ankle", "right_heel"],
    ["right_ankle", "right_foot_index"], ["left_wrist", "left_index"], ["left_wrist", "left_pinky"], ["right_wrist", "right_index"],
    ["right_wrist", "right_pinky"], ["left_shoulder", "left_hip"], ["right_shoulder", "right_hip"], ["left_ear", "nose"]]
    .map(([a, b]) => [P[a], P[b]]);
  function despike(frames, limit, metre) {
    const n = frames.length;
    if (!(limit > 0) || n < 5) return 0;
    const m = metre > 0 ? metre : 1;
    const drop = new Set();
    const med = (v) => v.slice().sort((x, y) => x - y)[2];
    const around = (f) => Math.max(0, Math.min(n - 5, f - 2));
    for (const [a, b] of BONES) {
      const lengths = frames.map((f) => { const p = point(f, a), q = point(f, b); return valid(p) && valid(q) ? len(sub(q, p)) : NaN; });
      for (let f = 0; f < n; f++) {
        if (!Number.isFinite(lengths[f])) continue;
        const w = lengths.slice(around(f), around(f) + 5);
        if (!w.every(Number.isFinite)) continue;
        const usual = med(w);
        if (Math.abs(lengths[f] - usual) > Math.max(usual * 0.3, 0.06 * m)) drop.add(f * COUNT + b);
      }
    }
    for (let i = 0; i < COUNT; i++) {
      for (let f = 0; f < n; f++) {
        const p = point(frames[f], i);
        if (!valid(p)) continue;
        const w = [];
        for (let g = around(f); g < around(f) + 5; g++) w.push(point(frames[g], i));
        if (!w.every(valid)) continue;
        const c = [0, 1, 2].map((k) => med(w.map((q) => q[k])));
        if (Math.hypot(p[0] - c[0], p[1] - c[1], p[2] - c[2]) > limit) drop.add(f * COUNT + i);
      }
    }
    for (const key of drop) {
      const f = Math.floor(key / COUNT), i = key % COUNT;
      frames[f][i * 3] = frames[f][i * 3 + 1] = frames[f][i * 3 + 2] = NaN;
    }
    return drop.size;
  }

  // A hand file's points that don't sit on the wrist - its index knuckle further from it,
  // in most frames, than the forearm is long (the owner's test take: 74 cm off - the
  // hands too small in the webcams' frames to place) - are dropped: that hand then
  // follows its forearm.
  function dropStrayHands(frames, notes, unit) {
    for (const side of ["left", "right"]) {
      const reach = [], forearm = [];
      for (const f of frames) {
        const w = point(f, P[side + "_wrist"]), e = point(f, P[side + "_elbow"]), k = point(f, P[side + "_index"]);
        if (valid(w) && valid(k)) reach.push(len(sub(k, w)));
        if (valid(w) && valid(e)) forearm.push(len(sub(w, e)));
      }
      if (!reach.length || !forearm.length) continue;
      const off = percentile(reach, 0.5), arm = percentile(forearm, 0.5);
      if (off <= arm) continue;
      for (const f of frames) for (const what of ["index", "pinky", "thumb"]) f.fill(NaN, P[side + "_" + what] * 3, P[side + "_" + what] * 3 + 3);
      notes.push("the " + side + " hand file's points don't sit on the wrist (" + Math.round(off * unit * 100) + " cm off): left out");
    }
  }

  // Points the capture never gives: the hands from the forearms (the knuckles a third of a
  // forearm past the wrist - no turn about the arm), the rest from their stand-ins.
  function standIns(frames, notes) {
    const never = (n) => frames.every((f) => !Number.isFinite(f[P[n] * 3]));
    for (const side of ["left", "right"]) {
      if (!never(side + "_index") || !never(side + "_pinky")) continue;
      for (const f of frames) {
        const w = point(f, P[side + "_wrist"]), e = point(f, P[side + "_elbow"]);
        if (!valid(w) || !valid(e)) continue;
        const k = add(w, scale(sub(w, e), 0.35));
        for (const what of ["index", "pinky", "thumb"]) f.set(k, P[side + "_" + what] * 3);
      }
      notes.push("no usable " + side + " hand points: that hand follows its forearm (no turn about it)");
    }
    for (let pass = 0; pass < 2; pass++) {
      for (const [n, from] of Object.entries(STAND_INS)) {
        if (!never(n) || never(from)) continue;
        for (const f of frames) f.set(point(f, P[from]), P[n] * 3);
      }
    }
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
  // Limb points the cameras lost for a while: a limb bone 60% longer, or half as long, as in the
  // standing part [r0, r1), and 15 cm off it (a heel bone is 7-10 cm: its own jitter crosses 60%)
  // - its end and everything past it on the limb dropped (to be filled). `metre`: a metre in the
  // frames' units.
  // despike's window is five frames: a second of lost feet passes it. The owner's first take with
  // three cameras threw the feet 1.4 m below the floor for 0.8 s and again for its last 4.5 s, the
  // floor went down with them and the hips read 2.27 m (0.92 on the two-camera takes).
  const LIMB = [["left_hip", "left_knee"], ["left_knee", "left_ankle"], ["left_ankle", "left_heel"], ["left_ankle", "left_foot_index"],
    ["right_hip", "right_knee"], ["right_knee", "right_ankle"], ["right_ankle", "right_heel"], ["right_ankle", "right_foot_index"],
    ["left_shoulder", "left_elbow"], ["left_elbow", "left_wrist"], ["right_shoulder", "right_elbow"], ["right_elbow", "right_wrist"]];
  const PAST = {
    left_knee: ["left_ankle", "left_heel", "left_foot_index"], left_ankle: ["left_heel", "left_foot_index"],
    right_knee: ["right_ankle", "right_heel", "right_foot_index"], right_ankle: ["right_heel", "right_foot_index"],
    left_elbow: ["left_wrist", "left_index", "left_pinky", "left_thumb"], left_wrist: ["left_index", "left_pinky", "left_thumb"],
    right_elbow: ["right_wrist", "right_index", "right_pinky", "right_thumb"], right_wrist: ["right_index", "right_pinky", "right_thumb"],
  };
  function dropImpossible(frames, r0, r1, metre) {
    const far = 0.15 * (metre > 0 ? metre : 1);
    let dropped = 0;
    // Two frames either side go too: resampling blends a lost frame into its neighbours (13 a second
    // to 30: 2.3 frames apart), and a fill anchored on a half-lost frame bends toward it.
    const lost = new Set();
    const lose = (f, name) => { for (let g = Math.max(0, f - 2); g <= Math.min(frames.length - 1, f + 2); g++) lost.add(g * COUNT + P[name]); };
    for (const [a, b] of LIMB) {
      const ia = P[a], ib = P[b];
      if (ia === undefined || ib === undefined) continue;
      const length = (f) => { const p = point(frames[f], ia), q = point(frames[f], ib); return valid(p) && valid(q) ? len(sub(q, p)) : NaN; };
      const standing = [];
      for (let f = r0; f < r1; f++) standing.push(length(f));
      const usual = percentile(standing, 0.5);
      if (!(usual > 0)) continue;
      for (let f = 0; f < frames.length; f++) {
        const l = length(f);
        if (Number.isFinite(l) && (l > usual * 1.6 || l < usual * 0.5) && Math.abs(l - usual) > far) { lose(f, b); for (const n of PAST[b] || []) lose(f, n); }
      }
    }
    for (const key of lost) {
      const f = Math.floor(key / COUNT), i = key % COUNT;
      if (!valid(point(frames[f], i))) continue;
      frames[f][i * 3] = frames[f][i * 3 + 1] = frames[f][i * 3 + 2] = NaN;
      dropped++;
    }
    return dropped;
  }

  // A lost heel or toe filled following its ankle: its offset from the ankle, straight between the
  // frames either side of the gap (a straight line through space would leave it behind for the
  // seconds a long gap lasts while the foot moves on).
  function fillFeetFromAnkles(frames) {
    let filled = 0;
    for (const side of ["left", "right"]) {
      const ia = P[side + "_ankle"];
      for (const n of ["heel", "foot_index"]) {
        const ip = P[side + "_" + n];
        if (ia === undefined || ip === undefined) continue;
        const offset = frames.map((f) => { const a = point(f, ia), q = point(f, ip); return valid(a) && valid(q) ? sub(q, a) : null; });
        let last = -1;
        for (let f = 0; f <= frames.length; f++) {
          if (f < frames.length && !offset[f]) continue;
          for (let g = last + 1; g < f; g++) {
            const a = point(frames[g], ia);
            if (!valid(a) || valid(point(frames[g], ip))) continue;
            const o = last < 0 ? offset[f] : f >= frames.length ? offset[last] : add(offset[last], scale(sub(offset[f], offset[last]), (g - last) / (f - last)));
            if (!o) continue;
            frames[g].set(add(a, o), ip * 3);
            filled++;
          }
          last = f;
        }
      }
    }
    return filled;
  }

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
    const opt = Object.assign({ fps: 30, smoothing: 0, reference: [0, 0.33], despike: 1 }, options || {});
    let raw = capture.frames.map((f) => Float64Array.from(f));
    if (raw.length < 2) throw new Error("a capture needs two frames or more");
    const notes = [];
    // Size: millimetres when the body is metres long in thousands.
    const heights = raw.map((f) => len(sub(mid(point(f, P.left_shoulder), point(f, P.right_shoulder)), mid(point(f, P.left_ankle), point(f, P.right_ankle)))));
    const body = percentile(heights, 0.5);
    const unit = body > 50 ? 0.001 : body > 5 ? 0.01 : 1;
    dropStrayHands(raw, notes, unit);
    const glitches = opt.despike > 0 ? despike(raw, opt.despike / unit, 1 / unit) : 0;
    if (glitches) notes.push(glitches + " glitches removed (a bone suddenly stretched or squashed by 30% or more for a frame)");
    const gaps = fillGaps(raw);
    if (gaps) notes.push(gaps + " missing values filled");
    standIns(raw, notes);
    let source = null;
    const target = opt.fps > 1 ? opt.fps : 30;
    const even = (times) => times.every((t, i) => i === 0 || Math.abs(t - times[i - 1] - 1 / target) < 0.01 / target);
    if (capture.times && capture.times.length === raw.length && !even(capture.times)) {  // already even (a points file): kept
      const times = capture.times;
      const span = times[times.length - 1] - times[0];
      const steps = [];
      for (let i = 1; i < times.length; i++) steps.push(1 / Math.max(times[i] - times[i - 1], 1e-6));
      source = { frames: raw.length, rate: (raw.length - 1) / Math.max(span, 1e-6), slowest: percentile(steps, 0.02), fastest: percentile(steps, 0.98) };
      raw = resample(raw, times, opt.fps > 1 ? opt.fps : 30);
      notes.push("resampled from " + source.frames + " frames at " + source.rate.toFixed(1) + " a second (" + source.slowest.toFixed(0) + "-" +
                 source.fastest.toFixed(0) + ") to " + (opt.fps > 1 ? opt.fps : 30) + " - from the recording's timestamps");
    }
    // Axes, from the standing reference (the T-pose at the take's start): up is the
    // floor's - the plane through both heels and both toes - when those points look
    // right (within 25 degrees of the body's ankles-to-shoulders line), else the
    // body's; right is the shoulder and hip lines. From the body alone, up leans with
    // it (a few degrees: centimetres at the hands).
    const fps0 = opt.fps > 1 ? opt.fps : 30;
    const [r0, r1] = referenceSpan({ frames: raw, fps: fps0 }, opt.reference);
    if (opt.despike > 0) {
      const wild = dropImpossible(raw, r0, r1, 1 / unit);
      if (wild) {
        fillFeetFromAnkles(raw);
        fillGaps(raw);
        notes.push(wild + " points the cameras lost dropped and filled (a limb 60% longer, or half as long, as in the standing part)");
      }
    }
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
      for (let k = notes.length - 1; k >= 0; k--) if (/^up from/.test(notes[k])) notes.splice(k, 1);
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
    // The floor, to 0: the feet's low points in the standing part (median) - the feet are flat there;
    // over the whole take (5th percentile) when there's no standing part. A take's lost feet (far
    // under it) don't take the floor down with them.
    const lows = frames.map((f) => Math.min(...["left_heel", "right_heel", "left_foot_index", "right_foot_index", "left_ankle", "right_ankle"]
      .filter((n) => P[n] !== undefined).map((n) => f[P[n] * 3 + 2])));
    const floor = r1 > r0 ? percentile(lows.slice(r0, r1), 0.5) : percentile(lows, 0.05);
    for (const f of frames) for (let i = 0; i < COUNT; i++) f[i * 3 + 2] -= floor;
    const fps = opt.fps > 1 ? opt.fps : 30;
    const take = { fps, frames, up, right, forward, unit, mirrored, floor: floor, duration: (frames.length - 1) / fps, notes, source };
    setReference(take, opt.reference);
    return take;
  }

  // The frames a standing window [start, end) s covers: [a, b) - prepare's, setReference's
  // and the points export's.
  function referenceSpan(take, reference) {
    const n = take.frames.length;
    const a = Math.max(0, Math.min(n - 1, Math.round((reference ? reference[0] : 0) * take.fps)));
    const b = Math.max(a + 1, Math.min(n, Math.round((reference ? reference[1] : 0.33) * take.fps)));
    return [a, b];
  }

  // The frames the kept part [start, end] s covers (end 0: to the end): [a, b], both kept -
  // the retarget's and the points export's.
  function keepSpan(take, start, end) {
    const n = take.frames.length;
    const a = Math.max(0, Math.min(n - 2, Math.round((start || 0) * take.fps)));
    const b = end > 0 ? Math.max(a + 1, Math.min(n - 1, Math.round(end * take.fps))) : n - 1;
    return [a, b];
  }

  // The standing reference: frames [start, end) s, averaged.
  function setReference(take, reference) {
    const [a, b] = referenceSpan(take, reference);
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
  // The way a knee bends forward (#79): the hips' forward, turned halfway toward the foot's
  // when the foot is flat enough to say and faces within 100 degrees of the hips (a turned-out
  // foot's knee follows it; a pointed foot's heel-to-toe can face backwards - nwstab's back leg,
  // its toe 20 cm under its heel, the knee 25 cm ahead along the hips).
  function kneeWay(lhip, rhip, foot) {
    const hips = norm(cross([0, 0, 1], sub(rhip, lhip)));
    const flat = [foot[0], foot[1], 0];
    if (len(flat) > 0.02 && Math.abs(foot[2]) < 0.6 * len(flat)) {
      const f = norm(flat);
      if (dot(f, hips) > -0.17) return norm(add(hips, f));
    }
    return hips;
  }

  // Knees don't bend backwards (#79): a knee point behind the hip-ankle line - along kneeWay -
  // goes onto the line: the leg straight, never bent the wrong way. Webcams put a straight leg's
  // knee a few centimetres either side of the line (the owner's take: behind by over 1 cm in
  // about 40% of frames, up to 10 cm), and the doll's knee bent backwards with it.
  function straightenKnees(L) {
    for (const s of ["l", "r"]) {
      const H = L[s + "hip"], K = L[s + "kn"], F = L[s + "an"];
      if (![H, K, F, L.lhip, L.rhip, L[s + "toe"], L[s + "heel"]].every(valid)) continue;
      const u = norm(sub(F, H));
      const way = kneeWay(L.lhip, L.rhip, sub(L[s + "toe"], L[s + "heel"]));
      const f = norm(sub(way, scale(u, dot(way, u))));
      if (len(f) < 0.5) continue;
      const ahead = dot(sub(K, H), f);  // the knee's offset from the line along f (f is square to it)
      if (ahead < 0) L[s + "kn"] = sub(K, scale(f, ahead));
    }
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

  // The legs' neutral (#80): a turn per thigh, shin and foot taking its direction in the standing
  // part onto the stance's (pause1, or the stance picked) - applied in the pelvis's frame, so it
  // turns with the body - as the hips, torso and head already are: the performer's standing legs
  // become the stance's legs (their lean, their knees' bend), what they do from there is the
  // performer's. Not the arms: the standing part may be a T-pose.
  const LEG_NEUTRAL = ["lthigh_g", "lshin_g", "lfoot_g", "rthigh_g", "rshin_g", "rfoot_g"];
  function legNeutral(R, refL) {
    const out = new Map();
    for (const d of DRIVEN) {
      const i = R.index.get(d.bone);
      if (i === undefined || !LEG_NEUTRAL.includes(d.bone)) continue;
      let have = null, want = null;
      if (d.kind === "aim") {
        const dir = childDirection(R, d.bone, d.child);
        if (dir) { have = sub(refL[d.to], refL[d.from]); want = rotate(R.neutral[i], dir); }
      } else if (d.kind === "foot") {
        have = sub(refL[d.toe], refL[d.heel]);
        want = rotate(R.neutral[i], rotate(qconj(R.world[i]), [0, 1, 0]));
      }
      if (have && want && valid(have) && len(have) > 1e-6 && len(want) > 1e-6) out.set(i, fromTo(have, want));
    }
    return out;
  }

  // One frame's local rotations (by bone index) and the root's position.
  function solveFrame(R, f, ref) {
    const L = landmarks(f);
    straightenKnees(L);
    const frames = bodyFrames(L, ref.waist, ref.frames.pelvis);
    // A leg direction onto the stance's: its neutral turn, in the pelvis's frame since the standing part.
    const since = ref.legs && ref.legs.size ? qmul(frames.pelvis, qconj(ref.frames.pelvis)) : null;
    const toNeutral = (i, v) => {
      const turn = since && ref.legs.get(i);
      return turn ? rotate(since, rotate(turn, rotate(qconj(since), v))) : v;
    };
    // The thighs' and shins' directions, turned - and a knee the turn takes past straight set straight
    // (#80): a standing part crouched (a combat stance) under an upright stance (pause1) turns a later
    // straight leg backwards (the owner's first combat-stance take: 24 cm). Checked when the thigh is
    // solved - at this body's thigh and shin lengths, along kneeWay of the doll's own hips (its
    // pelvis solved by then: in a kick the capture's hip points face 20-30 degrees off it).
    const legDir = new Map();
    const pelvisIndex = R.index.get("pelvis_g");
    const legTarget = (i) => {
      if (legDir.has(i)) return legDir.get(i);
      const s = i === R.index.get("lthigh_g") ? "l" : i === R.index.get("rthigh_g") ? "r" : null;
      if (!s) return null;
      const lt = R.index.get("lthigh_g"), rt = R.index.get("rthigh_g"), sh = R.index.get(s + "shin_g"), ft = R.index.get(s + "foot_g");
      const H = L[s + "hip"], K = L[s + "kn"], F = L[s + "an"];
      if (sh === undefined || ft === undefined || pelvisIndex === undefined || ![H, K, F].every(valid)) return null;
      const d1 = norm(toNeutral(i, sub(K, H))), d2 = norm(toNeutral(sh, sub(F, K)));
      const l1 = len(R.restPos[sh]), l2 = len(R.restPos[ft]);
      const u = norm(add(scale(d1, l1), scale(d2, l2)));
      const across = rotate(world[pelvisIndex], sub(R.restPos[rt], R.restPos[lt]));
      const foot = valid(L[s + "toe"]) && valid(L[s + "heel"]) ? sub(L[s + "toe"], L[s + "heel"]) : [0, 0, 0];
      const way = kneeWay([0, 0, 0], across, foot);
      const f = norm(sub(way, scale(u, dot(way, u))));
      const straight = len(f) > 0.5 && dot(d1, f) < 0;
      legDir.set(i, straight ? u : d1);
      legDir.set(sh, straight ? u : d2);
      return legDir.get(i);
    };
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
        const target = d.kind === "hand" ? sub(mid(L[d.index], L[d.pinky]), L[d.wrist]) : legTarget(i) || toNeutral(i, sub(L[d.to], L[d.from]));
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
        const target = toNeutral(i, sub(L[d.toe], L[d.heel]));
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
  // of the standing hip height) and swayed with the hips over the ground under them -
  // `ground`, the body's spot (placeGround), else the feet.
  function rootPosition(R, L, take, refL, sway, ground, home) {
    const i = R.index.get("rootdummy");
    if (i === undefined) return null;
    const rest = home || R.restPos[i];  // the stance's root (#80): its hips' height, a crouch's low
    const s = take.hipHeight > 0.1 ? R.hipHeight / take.hipHeight : 1;
    const z = rest[2] + (L.hips[2] - take.hipHeight) * s;
    if (!sway) return [rest[0], rest[1], z];
    const now = sub(L.hips, ground ? [ground[0], ground[1], 0] : L.feet), then = sub(refL.hips, refL.feet);
    return [rest[0] + (now[0] - then[0]) * s, rest[1] + (now[1] - then[1]) * s, z];
  }

  // --- Where the body stands, and planted feet (#79) ------------------------------------------
  // The body's spot on the ground, in the performer's space (x, y), per kept frame [a, b]:
  // "return" - where the feet stood at the first frame kept: the body moves with the steps
  // from there, and the blend-out brings it back (the game never moves the creature for an
  // animation); "inplace" - the part's net travel taken out, at an even speed: the hips'
  // move from its first fifth of a second to its last (a walk cycle's ends are the same
  // step: no cycle's sway mistaken for travel; a loop ends where it starts), through the
  // feet's midpoint on average - a walk on a treadmill (the game moves the creature), a
  // stance on its spot.
  function placeGround(take, a, b, place) {
    const feet = [], hips = [];
    for (let f = a; f <= b; f++) {
      const fr = take.frames[f];
      feet.push(mid(point(fr, P.left_ankle), point(fr, P.right_ankle)));
      hips.push(mid(point(fr, P.left_hip), point(fr, P.right_hip)));
    }
    if (place === "return") return feet.map(() => [feet[0][0], feet[0][1]]);
    const n = feet.length, tm = (n - 1) / 2;
    const mx = feet.reduce((s, p) => s + p[0], 0) / n, my = feet.reduce((s, p) => s + p[1], 0) / n;
    const w = Math.max(1, Math.min(Math.round(0.2 * take.fps), Math.floor(n / 4)));
    const avg = (from) => [0, 1].map((c) => hips.slice(from, from + w).reduce((s, p) => s + p[c], 0) / w);
    const h0 = avg(0), h1 = avg(n - w), dt = n - w;
    const vx = dt > 0 ? (h1[0] - h0[0]) / dt : 0, vy = dt > 0 ? (h1[1] - h0[1]) / dt : 0;
    return feet.map((_, k) => [mx + vx * (k - tm), my + vy * (k - tm)]);
  }

  // Where each foot stands still on the floor: its heel slower than 0.3 m/s (over a sixth
  // of a second) and within 6 cm of its low (the take's 5th percentile), gaps under 0.15 s
  // bridged - and split where the heel has wandered 5 cm from where it landed (a shuffle
  // is two plants: one lock can't hold a foot that really moves), 0.2 s apart (the foot moves
  // between the two spots over the locks' ramps, not in a frame); 0.4 s or more each.
  // { left: [[from, to] s, ...], right: [...] }.
  function suggestPlants(take) {
    const fps = take.fps, n = take.frames.length;
    const out = {};
    for (const side of ["left", "right"]) {
      const heel = take.frames.map((f) => point(f, P[side + "_heel"]));
      const low = percentile(heel.map((p) => p[2]), 0.05);
      // The heel over 5 frames (webcam jitter is a few millimetres a frame).
      const at = heel.map((_, k) => {
        let x = 0, y = 0, m = 0;
        for (let j = Math.max(0, k - 2); j <= Math.min(n - 1, k + 2); j++, m++) { x += heel[j][0]; y += heel[j][1]; }
        return [x / m, y / m];
      });
      const still = heel.map((p, k) => {
        const i = Math.max(0, k - 2), j = Math.min(n - 1, k + 2);
        const speed = j > i ? (Math.hypot(heel[j][0] - heel[i][0], heel[j][1] - heel[i][1]) * fps) / (j - i) : 0;
        return speed < 0.3 && p[2] < low + 0.06;
      });
      const runs = [];
      let run = -1;
      for (let k = 0; k <= n; k++) {
        const on = k < n && still[k];
        if (on && run < 0) run = k;
        if (!on && run >= 0) {
          const last = runs[runs.length - 1];
          if (last && run - last[1] - 1 < Math.round(0.15 * fps)) last[1] = k - 1; else runs.push([run, k - 1]);
          run = -1;
        }
      }
      const plants = [];
      const gap = Math.max(1, Math.round(0.1 * fps));
      for (const [r0, r1] of runs) {
        let from = r0;
        for (let k = r0; k <= r1 + 1; k++) {
          const wandered = k <= r1 && Math.hypot(at[k][0] - at[from][0], at[k][1] - at[from][1]) > 0.05;
          if (k > r1) plants.push([from, r1]);
          else if (wandered) { plants.push([from, k - gap]); from = k + gap; k = from; }
        }
      }
      out[side] = plants.filter((r) => r[1] - r[0] + 1 >= Math.round(0.4 * fps)).map((r) => [r[0] / fps, r[1] / fps]);
    }
    return out;
  }

  // Every bone's world turn (W) and place (P) for a solved frame's local turns and root.
  function poseWorld(R, local, root) {
    const rootIndex = R.index.get("rootdummy");
    const W = new Array(R.bones.length), X = new Array(R.bones.length);
    for (let i = 0; i < R.bones.length; i++) {
      const p = R.bones[i].parent;
      const q = local.has(i) ? local.get(i) : R.rest[i];
      const offset = i === rootIndex && root ? root : R.restPos[i];
      W[i] = p < 0 ? q : qmul(W[p], q);
      X[i] = p < 0 ? offset : add(X[p], rotate(W[p], offset));
    }
    return { W, P: X };
  }

  // A leg bent so its ankle (the foot bone's place) moves `weight` of the way to `target`:
  // the thigh and the shin turned in the plane the knee already bends in (the pelvis's
  // forward when straight), the foot keeping its turn in the world. Returns how far the
  // target is beyond the leg's reach (0 when it reaches).
  function legIK(R, local, root, side, target, weight) {
    const s = side === "left" ? "l" : "r";
    const th = R.index.get(s + "thigh_g"), sh = R.index.get(s + "shin_g"), ft = R.index.get(s + "foot_g");
    if (th === undefined || sh === undefined || ft === undefined || R.bones[sh].parent !== th || R.bones[ft].parent !== sh) return 0;
    const { W, P: X } = poseWorld(R, local, root);
    const H = X[th], K = X[sh], A = X[ft];
    const T = add(A, scale(sub(target, A), weight));
    const l1 = len(sub(K, H)), l2 = len(sub(A, K));
    const want = len(sub(T, H));
    const short = Math.max(0, want - (l1 + l2));
    const d = Math.max(Math.abs(l1 - l2) + 1e-4, Math.min(l1 + l2 - 1e-4, want));
    const u = norm(sub(T, H));
    let v = sub(sub(K, H), scale(u, dot(sub(K, H), u)));
    // The knee bends forward (kneeWay: the hips' forward, toward the toes) - never behind the
    // hip-ankle line (#79): a leg straight in the capture, or leaning back a little, would fold
    // the wrong way.
    const lt = R.index.get("lthigh_g"), rt = R.index.get("rthigh_g");
    const toes = rotate(W[ft], rotate(qconj(R.world[ft]), [0, 1, 0]));
    const way = lt !== undefined && rt !== undefined ? kneeWay(X[lt], X[rt], toes) : toes;
    const ahead = sub(way, scale(u, dot(way, u)));
    if ((len(v) < 1e-4 || dot(v, ahead) < 0) && len(ahead) > 1e-4) v = ahead;
    v = norm(v);
    const along = (l1 * l1 - l2 * l2 + d * d) / (2 * d);
    const K2 = add(add(H, scale(u, along)), scale(v, Math.sqrt(Math.max(0, l1 * l1 - along * along))));
    const A2 = add(H, scale(u, d));
    const q1 = fromTo(sub(K, H), sub(K2, H));
    const Wth = qmul(q1, W[th]);
    const q2 = fromTo(rotate(q1, sub(A, K)), sub(A2, K2));
    const Wsh = qmul(q2, qmul(q1, W[sh]));
    local.set(th, qnorm(qmul(qconj(W[R.bones[th].parent]), Wth)));
    local.set(sh, qnorm(qmul(qconj(Wth), Wsh)));
    local.set(ft, qnorm(qmul(qconj(Wsh), W[ft])));
    return short;
  }

  // --- The retarget -----------------------------------------------------------------------------
  // options: name, start / end (s; end 0 = the end), sway (true), stance (a clip:
  // pause1, for the blends), place ("return" | "inplace": placeGround - "inplace" for walk
  // cycles and loops, whose net travel is travel; sway is always kept for "return"), plants ({ left / right: [[from, to] s in the take] }: feet held
  // on the floor - the result's locks), blendIn / blendOut (s), loop (false), loopBlend (s),
  // fitLength (s; 0 keeps it), contact (s in the take) and contactAt (s in the
  // result), transtime (0.25), reduce (true), tolerance (degrees; 0.75 - and 4 mm
  // for the root, scaled alike). The clip's takeTime(t) is its time in the take.
  function retarget(take, bones, options) {
    const opt = Object.assign({ name: "n5_mocap", start: 0, end: 0, sway: true, stance: null, place: "return", plants: null,
      blendIn: 0, blendOut: 0, loop: false, loopBlend: 0.25, fitLength: 0, contact: null, contactAt: null, transtime: 0.25,
      reduce: true, tolerance: 0.75 }, options || {});
    const tolerance = Math.max(0.01, opt.tolerance);
    const R = rig(bones, opt.stance);
    const refL = landmarks(take.reference.pose);
    straightenKnees(refL);
    const waist = scale(R.waistOffset, take.hipHeight > 0.1 ? take.hipHeight / R.hipHeight : 1);
    const ref = { waist, frames: bodyFrames(refL, waist, null) };
    ref.legs = legNeutral(R, refL);
    const [a, b] = keepSpan(take, opt.start, opt.end);
    // Solve every frame kept.
    const driven = [];
    const rootIndex = R.index.get("rootdummy");
    const solved = [];
    for (let f = a; f <= b; f++) {
      const { local, L } = solveFrame(R, take.frames[f], ref);
      solved.push({ local, L });
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
    // The root over the body's spot - from the part kept in the end (a loop's, once cut).
    const ground = placeGround(take, a, a + count - 1, opt.place);
    let home = rootIndex !== undefined ? R.restPos[rootIndex] : null;  // the stance's root: the standing part's
    if (opt.stance && rootIndex !== undefined) {
      for (const node of opt.stance.nodes) {
        if (R.index.get(lower(node.name)) === rootIndex && node.position && node.position.times.length) home = node.position.values[0].slice(0, 3);
      }
    }
    solved.forEach((sv, k) => { sv.root = rootPosition(R, sv.L, take, refL, opt.place === "return" || opt.sway, ground[k], home); });
    // Planted feet held (#79): each lock keeps its ankle on the spot on the ground where it
    // landed, by bending the leg (legIK) - on this body, whatever its legs' proportions. In
    // from 0.1 s before it lands, out over 0.2 s as it lifts (the correction rides the step).
    // The ground moves under an "inplace" body (its travel taken out): the spot moves with it.
    const locks = [];
    if (opt.plants && rootIndex !== undefined) {
      const fps = take.fps;
      const s = take.hipHeight > 0.1 ? R.hipHeight / take.hipHeight : 1;
      const shift = (k) => [(ground[k][0] - ground[0][0]) * s, (ground[k][1] - ground[0][1]) * s, 0];
      const foot = { left: R.index.get("lfoot_g"), right: R.index.get("rfoot_g") };
      const ankles = solved.map((sv) => poseWorld(R, sv.local, sv.root).P);
      const rIn = Math.max(1, Math.round(0.1 * fps)), rOut = Math.max(1, Math.round(0.2 * fps));
      const held = { left: new Array(count).fill(null), right: new Array(count).fill(null) };
      for (const side of ["left", "right"]) {
        if (foot[side] === undefined) continue;
        for (const [t0, t1] of opt.plants[side] || []) {
          const p0 = Math.max(0, Math.round(t0 * fps) - a), p1 = Math.min(count - 1, Math.round(t1 * fps) - a);
          if (p1 - p0 < 2) continue;
          let spot = [0, 0, 0], m = 0;
          for (let k = p0; k <= Math.min(p1, p0 + rIn); k++, m++) spot = add(spot, add(ankles[k][foot[side]], shift(k)));
          spot = scale(spot, 1 / m);
          const lock = { side, from: p0, to: p1, short: 0 };
          locks.push(lock);
          for (let k = Math.max(0, p0 - rIn); k <= Math.min(count - 1, p1 + rOut); k++) {
            const w = k < p0 ? smoothstep(1 - (p0 - k) / (rIn + 1)) : k > p1 ? smoothstep(1 - (k - p1) / (rOut + 1)) : 1;
            if (!held[side][k] || w > held[side][k].w) held[side][k] = { w, target: sub(spot, shift(k)), lock };
          }
        }
      }
      for (let k = 0; k < count; k++) {
        for (const side of ["left", "right"]) {
          const h = held[side][k];
          if (!h || h.w <= 0) continue;
          const short = legIK(R, solved[k].local, solved[k].root, side, h.target, h.w);
          if (h.w >= 0.999) h.lock.short = Math.max(h.lock.short, short);
        }
      }
    }
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
    // A locked foot's leg and the root kept closer (a third of the tolerance): their keys'
    // slack adds up at the foot - a centimetre at 0.75 degrees (#79).
    const tight = new Set();
    for (const l of locks) for (const n of ["thigh_g", "shin_g", "foot_g"]) { const i = R.index.get((l.side === "left" ? "l" : "r") + n); if (i !== undefined) tight.add(i); }
    const nodes = [];
    for (const [j, track] of tracks) {
      const node = { name: R.bones[j].name, parent: R.bones[j].parent, orientation: { times: track.times.map(warp), values: track.values } };
      if (opt.reduce) node.orientation = reduceTrack(node.orientation, true, ((tight.has(j) ? tolerance / 3 : tolerance) * Math.PI) / 180);
      nodes.push(node);
    }
    if (rootIndex !== undefined && rootTrack.times.length) {
      let position = { times: rootTrack.times.map(warp), values: rootTrack.values };
      if (opt.reduce) position = reduceTrack(position, false, 0.004 * (tolerance / 0.75) * (locks.length ? 1 / 3 : 1));
      nodes.unshift({ name: R.bones[rootIndex].name, parent: R.bones[rootIndex].parent, position });
    }
    const start = a / take.fps;
    return { name: opt.name, length: outLength, transtime: opt.transtime, animroot: "rootdummy", nodes,
             source: { start, end: (a + count - 1) / take.fps }, takeTime: (t) => start + unwarp(Math.max(0, Math.min(outLength, t))),
             clipTime: (t) => warp(Math.max(0, Math.min(length, t - start))),  // a time in the take, in the result (the part kept)
             locks: locks.map((l) => ({ side: l.side, start: warp(timeOf(l.from)), end: warp(timeOf(l.to)), takeStart: (a + l.from) / take.fps,
                                        takeEnd: (a + l.to) / take.fps, short: l.short })) };
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

  // --- The points round trip (#77) -----------------------------------------------------------
  // The prepared take - cleaned, resampled, upright, facing forward (game space: +X the
  // performer's right, +Y forward, +Z up, metres, the floor at 0) - as a CSV: a row a frame,
  // its time (0 at the first written), its marks, then every point's x / y / z (MediaPipe's
  // names). Blender's nwn5e mocap points add-on (tools/mocap_blender) reads and writes it;
  // parseCapture reads it back, the marks as capture.marks (trimFromMarks: the sliders).
  // marks: the bench's sliders - { reference: [start, end) s, start, end (s; end 0: to the
  // end), plants ({ left / right: [[from, to] s]}: columns left_planted / right_planted) }. With them, from the standing window's first frame to the kept part's last (the
  // owner, 2026-10-29: "go for b"): the retarget measures every turn and the hip height from
  // the standing pose, so it travels with the part kept; and columns standing / keep, 1 on
  // the frames of each. Without them, the whole take, unmarked.
  function pointsSpan(take, marks) {
    const n = take.frames.length;
    if (!marks) return { from: 0, to: n - 1, standing: null, keep: null };
    const standing = referenceSpan(take, marks.reference);
    const keep = keepSpan(take, marks.start, marks.end);
    return { from: Math.min(standing[0], keep[0]), to: Math.max(standing[1] - 1, keep[1]), standing, keep };
  }

  function pointsCsv(take, marks) {
    const span = pointsSpan(take, marks);
    const plants = marks && marks.plants
      ? ["left", "right"].map((side) => (marks.plants[side] || []).map(([s0, s1]) => [Math.round(s0 * take.fps), Math.round(s1 * take.fps)]))
      : null;
    const header = ["time_s"].concat(marks ? ["standing", "keep"] : [], plants ? ["left_planted", "right_planted"] : []);
    for (const n of LANDMARKS) for (const a of ["x", "y", "z"]) header.push("body_" + n + "_" + a);
    const rows = [header.join(",")];
    for (let k = span.from; k <= span.to; k++) {
      const f = take.frames[k];
      const row = [((k - span.from) / take.fps).toFixed(6)];
      if (marks) row.push(k >= span.standing[0] && k < span.standing[1] ? 1 : 0, k >= span.keep[0] && k <= span.keep[1] ? 1 : 0);
      if (plants) for (const runs of plants) row.push(runs.some(([p0, p1]) => k >= p0 && k <= p1) ? 1 : 0);
      for (let s = 0; s < COUNT * 3; s++) row.push(Number.isFinite(f[s]) ? f[s].toFixed(5) : "");
      rows.push(row.join(","));
    }
    return rows.join("\n") + "\n";
  }

  // A points file's marks (capture.marks) as the bench's sliders, in seconds of the prepared
  // take: { reference: [start, end), start, end (0: to the end) } - each only when marked.
  function trimFromMarks(capture, fps) {
    const out = {};
    const marks = capture.marks;
    if (!marks) return out;
    const t = capture.times;
    const at = (k) => (t ? t[k] - t[0] : k / fps);
    const last = capture.frames.length - 1;
    if (marks.standing) out.reference = [at(marks.standing[0]), at(marks.standing[1]) + 1 / fps];
    if (marks.keep) { out.start = at(marks.keep[0]); out.end = marks.keep[1] >= last ? 0 : at(marks.keep[1]); }
    if (marks.plants) out.plants = { left: marks.plants.left.map(([x, y]) => [at(x), at(y)]), right: marks.plants.right.map(([x, y]) => [at(x), at(y)]) };
    return out;
  }

  // --- The take's motion, and where to trim ----------------------------------------------------
  // Per frame: the fastest hand, the fastest foot and the hips (m/s), smoothed over a fifth of a second.
  function motionProfile(take) {
    const n = take.frames.length;
    const speed = (f, ids) => {
      if (f === 0) return 0;
      let s = 0;
      for (const i of ids) s = Math.max(s, len(sub(point(take.frames[f], i), point(take.frames[f - 1], i))) * take.fps);
      return s;
    };
    const raw = { hands: [], feet: [], hips: [] };
    for (let f = 0; f < n; f++) {
      raw.hands.push(speed(f, [P.left_wrist, P.right_wrist]));
      raw.feet.push(speed(f, [P.left_ankle, P.right_ankle]));
      const h = (g) => mid(point(take.frames[g], P.left_hip), point(take.frames[g], P.right_hip));
      raw.hips.push(f ? len(sub(h(f), h(f - 1))) * take.fps : 0);
    }
    const r = Math.max(1, Math.round(take.fps * 0.1));
    const soften = (v) => v.map((_, f) => { let s = 0, c = 0; for (let g = Math.max(0, f - r); g <= Math.min(n - 1, f + r); g++) { s += v[g]; c++; } return s / c; });
    const out = { hands: soften(raw.hands), feet: soften(raw.feet), hips: soften(raw.hips) };
    out.overall = out.hands.map((v, f) => Math.max(v, out.feet[f], out.hips[f]));
    return out;
  }

  // A suggested standing window (the longest still stretch in the first 40%), and the part
  // to keep: from just before the first movement after it to just after the last.
  function suggestTrim(take) {
    const m = motionProfile(take).overall;
    const n = m.length;
    const quiet = percentile(m, 0.15);
    const moving = Math.max(0.6, quiet * 2.5);
    let best = [0, 0], run = -1;
    const limit = Math.max(2, Math.floor(n * 0.4));
    for (let f = 0; f <= limit; f++) {
      const still = f < limit && m[f] < moving;
      if (still && run < 0) run = f;
      if (!still && run >= 0) { if (f - run > best[1] - best[0]) best = [run, f]; run = -1; }
    }
    const pad = Math.round(take.fps * 0.3);
    let refA = best[0], refB = best[1];
    if (refB - refA > Math.round(take.fps * 0.4)) { const trim = Math.round((refB - refA) * 0.1); refA += trim; refB -= trim; }
    refB = Math.min(refB, refA + Math.round(take.fps * 1.5));
    let first = -1, last = -1;
    for (let f = best[1]; f < n; f++) if (m[f] >= moving) { if (first < 0) first = f; last = f; }
    const start = first < 0 ? 0 : Math.max(best[1], first - pad);
    const end = last < 0 ? 0 : Math.min(n - 1, last + pad);
    return { reference: [refA / take.fps, Math.max(refA + 1, refB) / take.fps], start: start / take.fps,
             end: end >= n - 1 ? 0 : end / take.fps, moving, still: best[1] > best[0] };
  }

  const api = { LANDMARKS, CONNECTIONS, parseCapture, addHand, timesFrom, resample, despike, pointsSpan, pointsCsv, trimFromMarks, frameRateFrom,
                prepare, setReference, referenceSpan, keepSpan, placeGround, suggestPlants, poseWorld, legIK,
                pointsAt, retarget, suggestContact, motionProfile, suggestTrim, reduceTrack, rig, solveFrame, landmarks, bodyFrames };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.Mocap = api;
})(typeof window !== "undefined" ? window : this);
