// Checks Mocap mode (public/mocap.js) on the real libraries:
//  1. Round trips: an NWN animation becomes a made-up FreeMoCap recording (its
//     joints as MediaPipe's body points, the rest pose first as the standing
//     reference, turned, moved, in millimetres; also Y-up and mirrored), goes
//     through parse > prepare > retarget on the same body, and the result's joints
//     must land where the animation's did.
//  2. A real recording (a FreeMoCap mediapipe_body_3d_xyz.csv), retargeted to every
//     body: finite keys, every driven bone keyed, the feet near the floor.
// node tools/recipe_bench/mocap_check.js build/animedit/libraries [recording.csv]
const fs = require("fs");
const path = require("path");
const A = require("./public/core.js");
const M = require("./public/mocap.js");

const dir = process.argv[2];
const recording = process.argv[3] || "";
const libs = new Map();
for (const f of fs.readdirSync(dir)) {
  if (!f.startsWith("a_") || !f.endsWith(".mdl")) continue;
  const b = fs.readFileSync(path.join(dir, f));
  const lib = A.readLibrary(b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength));
  libs.set(lib.name, lib);
}
function findClip(body, name) {
  let current = body;
  for (let d = 0; d < 8 && current; d++) {
    const lib = libs.get(current);
    if (!lib) return null;
    if (lib.clips.has(name)) return A.readClip(lib, name);
    current = lib.supermodel;
  }
  return null;
}

// --- Forward kinematics with rotations ---------------------------------------------------------
const qmul = (a, b) => [
  a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
  a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]];
function rotate(q, v) {
  const p = qmul(qmul(q, [v[0], v[1], v[2], 0]), [-q[0], -q[1], -q[2], q[3]]);
  return [p[0], p[1], p[2]];
}
const add = (a, b) => [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
function posed(bones, clip, t) {
  const byName = new Map();
  if (clip) for (const n of clip.nodes) byName.set(n.name.toLowerCase(), n);
  const out = [];
  for (let i = 0; i < bones.length; i++) {
    const b = bones[i];
    let p = b.position ? b.position.values[0] : [0, 0, 0];
    let q = b.orientation ? b.orientation.values[0] : [0, 0, 0, 1];
    const node = byName.get(b.name.toLowerCase());
    if (node && node.orientation && node.orientation.times.length) q = A.sample(node.orientation, t, true);
    if (node && node.position && node.position.times.length) p = A.sample(node.position, t, false);
    if (b.parent < 0) out.push({ p: p.slice(0, 3), q });
    else {
      const up = out[b.parent];
      out.push({ p: add(up.p, rotate(up.q, p)), q: qmul(up.q, q) });
    }
  }
  return out;
}

// --- A made-up recording from an animation ------------------------------------------------------
// Offsets are given in the rest pose's world axes (forward +Y, up +Z) and turned with
// the bone since rest - a_dfa's feet and hands rest tilted in their own axes.
function landmarksFrom(bones, pose, restPose) {
  const at = (name) => { const i = bones.findIndex((b) => b.name.toLowerCase() === name); return pose[i]; };
  const restAt = (name) => { const i = bones.findIndex((b) => b.name.toLowerCase() === name); return restPose[i]; };
  const since = (name) => { const r = restAt(name).q; return qmul(at(name).q, [-r[0], -r[1], -r[2], r[3]]); };
  const off = (name, v) => add(at(name).p, rotate(since(name), v));
  const pt = {};
  pt.left_shoulder = at("lbicep_g").p; pt.right_shoulder = at("rbicep_g").p;
  pt.left_elbow = at("lforearm_g").p; pt.right_elbow = at("rforearm_g").p;
  pt.left_wrist = at("lhand_g").p; pt.right_wrist = at("rhand_g").p;
  const lk = at("lhand").p, rk = at("rhand").p, lq = since("lhand_g"), rq = since("rhand_g");
  pt.left_index = add(lk, rotate(lq, [0, 0.03, 0])); pt.left_pinky = add(lk, rotate(lq, [0, -0.03, 0]));
  pt.right_index = add(rk, rotate(rq, [0, 0.03, 0])); pt.right_pinky = add(rk, rotate(rq, [0, -0.03, 0]));
  pt.left_thumb = pt.left_index; pt.right_thumb = pt.right_index;
  pt.left_hip = at("lthigh_g").p; pt.right_hip = at("rthigh_g").p;
  pt.left_knee = at("lshin_g").p; pt.right_knee = at("rshin_g").p;
  pt.left_ankle = at("lfoot_g").p; pt.right_ankle = at("rfoot_g").p;
  const sole = at("lfoot_g").p[2];  // heel and toes on the floor at rest (the caller passes the rest pose's)
  const soleL = restAt("lfoot_g").p[2], soleR = restAt("rfoot_g").p[2];  // each sole on the floor at rest
  pt.left_heel = off("lfoot_g", [0, -0.06, -soleL]); pt.left_foot_index = off("lfoot_g", [0, 0.14, -soleL]);
  pt.right_heel = off("rfoot_g", [0, -0.06, -soleR]); pt.right_foot_index = off("rfoot_g", [0, 0.14, -soleR]);
  pt.left_ear = off("head_g", [-0.08, 0, 0.1]); pt.right_ear = off("head_g", [0.08, 0, 0.1]);
  pt.nose = off("head_g", [0, 0.1, 0.08]);
  for (const n of ["left_eye_inner", "left_eye", "left_eye_outer"]) pt[n] = off("head_g", [-0.03, 0.09, 0.11]);
  for (const n of ["right_eye_inner", "right_eye", "right_eye_outer"]) pt[n] = off("head_g", [0.03, 0.09, 0.11]);
  pt.mouth_left = off("head_g", [-0.025, 0.09, 0.04]); pt.mouth_right = off("head_g", [0.025, 0.09, 0.04]);
  void sole;
  return pt;
}

// To a recording's space: turned `yaw` about up, moved, in mm; Y-up and / or mirrored on request.
function toRecording(p, opt) {
  const c = Math.cos(opt.yaw), s = Math.sin(opt.yaw);
  let x = p[0] * c - p[1] * s, y = p[0] * s + p[1] * c, z = p[2];
  x += 0.7; y -= 1.3; z += 0.05;
  if (opt.mirror) x = -x;
  let out = opt.yUp ? [x, z, -y] : [x, y, z];
  return out.map((v) => v * 1000);
}

// The standing reference is the body's idle stance (pause1 at 0): what the retarget maps
// a performer standing to.
function makeCsv(bones, clip, opt, stance) {
  const fps = 30;
  const restPose = posed(bones, null, 0);
  const restLeft = restPose[bones.findIndex((b) => b.name.toLowerCase() === "lfoot_g")].p[2];
  landmarksFrom.sole = restLeft;  // the soles at the floor at rest
  const header = M.LANDMARKS.map((n) => ["x", "y", "z"].map((a) => "body_" + n + "_" + a).join(",")).join(",");
  const rows = [];
  const truth = [];  // the joints the result must match, per frame
  const reference = Math.round(fps * 0.5);
  const frames = reference + Math.round(clip.length * fps) + 1;
  for (let f = 0; f < frames; f++) {
    const t = f < reference ? null : (f - reference) / fps;
    const pose = posed(bones, t === null ? stance : clip, t === null ? 0 : t);
    const pt = landmarksFrom(bones, pose, restPose);
    rows.push(M.LANDMARKS.map((n) => toRecording(pt[n], opt).map((v) => v.toFixed(3)).join(",")).join(","));
    truth.push(pose);
  }
  return { csv: header + "\n" + rows.join("\n") + "\n", truth, reference: reference / fps };
}

const JOINTS = ["lforearm_g", "lhand_g", "lhand", "rforearm_g", "rhand_g", "rhand", "lshin_g", "lfoot_g", "rshin_g", "rfoot_g", "head", "neck_g"];
let failed = 0;
function roundTrip(body, name, opt, limits) {
  const lib = libs.get(body);
  const clip = findClip(body, name);
  if (!lib || !clip) { console.log("SKIP", body, name); return; }
  const stance = findClip(body, "pause1");
  const made = makeCsv(lib.bones, clip, opt, stance);
  // The axes the made-up recording used (the retarget is checked here; axisCheck checks finding them).
  const o = toRecording([0, 0, 0], opt);
  const axis = (v) => toRecording(v, opt).map((x, c) => x - o[c]);
  const take = M.prepare(M.parseCapture(made.csv), { fps: 30, reference: [0, made.reference - 0.05], axes: { up: axis([0, 0, 1]), right: axis([1, 0, 0]) } });
  const result = M.retarget(take, lib.bones, { name: "rt", start: made.reference, stance, reduce: !process.env.MOCAP_NOREDUCE });
  // The result starts at the clip's first frame; compare in place (no travel: the root's x / y are the sway).
  let worst = 0, sum = 0, n = 0, worstAt = "";
  const perJoint = [];
  let frames = 0;
  const index = JOINTS.map((j) => lib.bones.findIndex((b) => b.name.toLowerCase() === j));
  const rootIndex = lib.bones.findIndex((b) => b.name.toLowerCase() === "rootdummy");
  for (let f = 0; f * (1 / 30) <= clip.length + 1e-6; f++) {
    const t = f / 30;
    const want = made.truth[Math.round(made.reference * 30) + f];
    const got = posed(lib.bones, result, Math.min(t, result.length));
    frames++;
    // Measured from the root (the original's root may travel; the result's stays in place).
    for (let k = 0; k < index.length; k++) {
      const i = index[k];
      if (i < 0) continue;
      const a = want[i].p.map((v, c) => v - want[rootIndex].p[c]);
      const b = got[i].p.map((v, c) => v - got[rootIndex].p[c]);
      const e = Math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
      sum += e; n++;
      perJoint[k] = (perJoint[k] || 0) + e;
      if (e > worst) { worst = e; worstAt = JOINTS[k] + " at " + t.toFixed(2) + " s"; }
    }
  }
  const mean = sum / Math.max(n, 1);
  const keys = result.nodes.reduce((s, node) => s + (node.orientation ? node.orientation.times.length : 0) + (node.position ? node.position.times.length : 0), 0);
  const ok = mean <= limits.mean && worst <= limits.worst;
  if (!ok || process.env.MOCAP_DETAIL) console.log("     by joint (cm): " + JOINTS.map((j, k) => j + " " + ((perJoint[k] || 0) / frames * 100).toFixed(1)).join(", "));
  if (!ok) failed++;
  console.log((ok ? "ok  " : "FAIL") + " round trip " + body + " " + name + " (" + Object.entries(opt).map(([k, v]) => k + "=" + (typeof v === "number" ? v.toFixed(2) : v)).join(" ") +
              "): joints off by mean " + (mean * 100).toFixed(1) + " cm, worst " + (worst * 100).toFixed(1) + " cm (" + worstAt + "); " +
              keys + " keys; " + take.notes.join("; "));
}

// Finding the axes: a performer standing square (the bind pose: shoulders and hips
// level and square, feet flat), then the animation; the found axes against the true.
function axisCheck(body, name, opt) {
  const lib = libs.get(body);
  const clip = findClip(body, name);
  if (!lib || !clip) { console.log("SKIP", body, name); return; }
  const made = makeCsv(lib.bones, clip, opt, null);
  const take = M.prepare(M.parseCapture(made.csv), { fps: 30, reference: [0, made.reference - 0.05] });
  const o = toRecording([0, 0, 0], opt);
  const axis = (v) => { const a = toRecording(v, opt).map((x, c) => x - o[c]); const l = Math.hypot(...a); return a.map((x) => x / l); };
  const deg = (a, b) => (Math.acos(Math.max(-1, Math.min(1, a[0] * b[0] + a[1] * b[1] + a[2] * b[2]))) * 180) / Math.PI;
  const upOff = deg(take.up, axis([0, 0, 1])), rightOff = deg(take.right, axis([1, 0, 0])), forwardOff = deg(take.forward, axis([0, 1, 0]));
  const ok = upOff < 1 && rightOff < 1 && forwardOff < 1;
  if (!ok) failed++;
  console.log((ok ? "ok  " : "FAIL") + " axes " + body + " (" + Object.entries(opt).map(([k, v]) => k + "=" + (typeof v === "number" ? v.toFixed(2) : v)).join(" ") +
              "): up off " + upOff.toFixed(2) + ", right " + rightOff.toFixed(2) + ", forward " + forwardOff.toFixed(2) + " degrees; " + take.notes.join("; "));
}
axisCheck("a_ba", "victorymg", { yaw: 0.65, yUp: false, mirror: false });
axisCheck("a_ba", "victorymg", { yaw: -1.9, yUp: true, mirror: false });
axisCheck("a_ba", "victorymg", { yaw: 2.4, yUp: true, mirror: true });
axisCheck("a_fa", "taunt", { yaw: 1.2, yUp: true, mirror: false });

const limits = { mean: 0.04, worst: 0.10 };  // the pelvis tilt the 33 points can't give: 1-3 cm
roundTrip("a_ba", "victorymg", { yaw: 0.65, yUp: false, mirror: false }, limits);
roundTrip("a_ba", "victorymg", { yaw: -1.9, yUp: true, mirror: false }, limits);
roundTrip("a_ba", "victorymg", { yaw: 2.4, yUp: true, mirror: true }, limits);
roundTrip("a_ba", "taunt", { yaw: 0.3, yUp: false, mirror: false }, limits);
roundTrip("a_ba", "nwstab", { yaw: 0.3, yUp: false, mirror: false }, limits);
roundTrip("a_fa", "victorymg", { yaw: 0.3, yUp: false, mirror: false }, limits);
roundTrip("a_dfa", "victorymg", { yaw: 0.3, yUp: false, mirror: false }, limits);
roundTrip("a_da2", "taunt", { yaw: 0.3, yUp: false, mirror: false }, limits);

// --- FreeMoCap's RTMPose files (#74) ------------------------------------------------------------
// The same made-up recording as RTMPose writes it: a row a point a frame (its names: the big
// toes for the foot_index points, no inner / outer eyes, mouth or hand points in the body
// file), the hands in files of their own, and uneven frame times (webcams: 10-17 a second)
// that the reader resamples to 30. Options: rate (frames a second, 13 by default),
// glitches (a frame's ankle a metre off, a wrist 60 cm off - the filter must drop them),
// strayHands (the hand files 80 cm off the wrists - they must be left out), info (print
// only: what a rate loses - at 13 a second a stab's peak falls between frames).
const RTM_RENAME = { left_foot_index: "left_big_toe", right_foot_index: "right_big_toe" };
const RTM_SKIP = new Set(["left_eye_inner", "left_eye_outer", "right_eye_inner", "right_eye_outer", "mouth_left", "mouth_right",
  "left_index", "left_pinky", "left_thumb", "right_index", "right_pinky", "right_thumb"]);
// Legs of other proportions: each thigh `ratio` times as long and the shin (2 - ratio) times
// - the hip and the ankle where they were, the knee moved in its bend plane: the feet stay
// where the animation put them, at knee angles NWN's legs don't share (#79).
function reproportion(pt, ratio) {
  const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]], dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  const scale = (a, s) => [a[0] * s, a[1] * s, a[2] * s], len = (a) => Math.hypot(a[0], a[1], a[2]);
  const norm = (a) => scale(a, 1 / Math.max(len(a), 1e-9));
  for (const side of ["left", "right"]) {
    const H = pt[side + "_hip"], K = pt[side + "_knee"], F = pt[side + "_ankle"];
    const l1 = len(sub(K, H)) * ratio, l2 = len(sub(F, K)) * (2 - ratio);
    const u = norm(sub(F, H));
    const d = Math.max(Math.abs(l1 - l2) + 1e-4, Math.min(l1 + l2 - 1e-4, len(sub(F, H))));
    let v = sub(sub(K, H), scale(u, dot(sub(K, H), u)));
    if (len(v) < 1e-4) v = sub([0, 1, 0], scale(u, u[1]));
    v = norm(v);
    const along = (l1 * l1 - l2 * l2 + d * d) / (2 * d);
    pt[side + "_knee"] = add(add(H, scale(u, along)), scale(v, Math.sqrt(Math.max(0, l1 * l1 - along * along))));
  }
}
function makeRtm(bones, clip, opt, stance, extra) {
  const restPose = posed(bones, null, 0);
  const lead = 0.5;  // the standing reference, seconds
  const times = [];
  const rate = extra.rate || 13;
  for (let t = 0, k = 0; t <= lead + clip.length + 1e-9; k++) {
    times.push(t);
    t += (1 / rate) * (1 + (((k * 7919) % 11) - 5) * 0.08);  // unevenly: 13 a second is about 10-17
  }
  // A frame where the stance gives way to the clip (a person moves on from standing; the
  // made-up one jumps, and resampling across the jump isn't what's checked).
  if (!times.some((t) => Math.abs(t - lead) < 1e-9)) { times.push(lead); times.sort((a, b) => a - b); }
  const body = ["frame,keypoint,x,y,z"], hands = { left: ["frame,keypoint,x,y,z"], right: ["frame,keypoint,x,y,z"] };
  const fmt = (v) => v.map((x) => x.toFixed(3)).join(",");
  times.forEach((t, f) => {
    const pose = t < lead ? posed(bones, stance, 0) : posed(bones, clip, Math.min(t - lead, clip.length));
    const pt = landmarksFrom(bones, pose, restPose);
    if (extra.legRatio) reproportion(pt, extra.legRatio);
    if (extra.kneeBack) for (const side of ["left", "right"]) pt[side + "_knee"] = add(pt[side + "_knee"], [0, -extra.kneeBack, 0]);
    if (extra.footBack) { const heel = pt.right_heel; pt.right_heel = pt.right_foot_index; pt.right_foot_index = heel; }
    // A camera rig losing the feet (the owner's three-camera take): ankles, heels and toes 1.4 m
    // under the floor between extra.lostFeet[0] and [1] s; a heel 30 cm off its ankle in
    // extra.lostHeel; the left heel 15 cm further back in extra.heelDrift (a heel point slow to
    // follow: 70% longer, 10 cm more); the whole body extra.sink[2] m down in [0]-[1] s.
    if (extra.lostFeet && t >= extra.lostFeet[0] && t <= extra.lostFeet[1]) {
      for (const n of ["left_ankle", "left_heel", "left_foot_index", "right_ankle", "right_heel", "right_foot_index"]) pt[n] = [pt[n][0], pt[n][1], pt[n][2] - 1.4];
    }
    if (extra.lostHeel && t >= extra.lostHeel[0] && t <= extra.lostHeel[1]) pt.left_heel = [pt.left_heel[0] + 0.3, pt.left_heel[1] - 0.2, pt.left_heel[2]];
    if (extra.heelDrift && t >= extra.heelDrift[0] && t <= extra.heelDrift[1]) pt.left_heel = [pt.left_heel[0], pt.left_heel[1] - 0.15, pt.left_heel[2]];
    if (extra.sink && t >= extra.sink[0] && t <= extra.sink[1]) for (const n in pt) pt[n] = [pt[n][0], pt[n][1], pt[n][2] - extra.sink[2]];
    if (extra.lean) {  // the whole performer tilted forward about the floor's origin, all take: a posture's lean
      // (these takes give "up" (axes), so it isn't levelled away by the floor)
      const co = Math.cos(extra.lean), si = Math.sin(extra.lean);
      for (const n in pt) pt[n] = [pt[n][0], pt[n][1] * co + pt[n][2] * si, -pt[n][1] * si + pt[n][2] * co];
    }
    if (extra.turn && t >= lead) {  // turned about up after the standing part
      const c = Math.cos(extra.turn), s = Math.sin(extra.turn);
      for (const n in pt) pt[n] = [pt[n][0] * c - pt[n][1] * s, pt[n][0] * s + pt[n][1] * c, pt[n][2]];
    }
    if (extra.travel && t >= lead) { const d = extra.travel(t - lead); for (const n in pt) pt[n] = [pt[n][0] + d[0], pt[n][1] + d[1], pt[n][2]]; }
    for (const n of M.LANDMARKS) {
      if (RTM_SKIP.has(n)) continue;
      let p = toRecording(pt[n], opt);
      if (extra.glitches && f === 9 && n === "right_ankle") p = p.map((v, c) => v + (c === 0 ? 1000 : 0));
      if (extra.glitches && f === 14 && n === "left_wrist") p = p.map((v, c) => v + (c === 2 ? 600 : 0));
      body.push(f + "," + (RTM_RENAME[n] || n) + "," + fmt(p));
    }
    for (const side of ["left", "right"]) {
      const off = extra.strayHands ? [800, 0, 0] : [0, 0, 0];
      const at = (n) => toRecording(pt[side + "_" + n], opt).map((v, c) => v + off[c]);
      hands[side].push(f + ",hand_root," + fmt(toRecording(pt[side + "_wrist"], opt).map((v, c) => v + off[c])));
      hands[side].push(f + ",forefinger1," + fmt(at("index")));
      hands[side].push(f + ",pinky_finger1," + fmt(at("pinky")));
      hands[side].push(f + ",thumb2," + fmt(at("thumb")));
    }
  });
  const stamps = ["multiframe_number,timestamp.from_recording_start.sec,other"].concat(times.map((t, f) => f + "," + t.toFixed(6) + ",0"));
  return { body: body.join("\n") + "\n", left: hands.left.join("\n") + "\n", right: hands.right.join("\n") + "\n",
           stamps: stamps.join("\n") + "\n", lead };
}
function rtmRoundTrip(body, name, opt, extra, limits) {
  const lib = libs.get(body);
  const clip = findClip(body, name);
  if (!lib || !clip) { console.log("SKIP", body, name); return; }
  const stance = findClip(body, "pause1");
  const made = makeRtm(lib.bones, clip, opt, stance, extra);
  const capture = M.parseCapture(made.body);
  M.addHand(capture, made.left, "left");
  M.addHand(capture, made.right, "right");
  capture.times = M.timesFrom(made.stamps);
  const o = toRecording([0, 0, 0], opt);
  const axis = (v) => toRecording(v, opt).map((x, c) => x - o[c]);
  const take = M.prepare(capture, { fps: 30, reference: [0, made.lead - 0.1], axes: { up: axis([0, 0, 1]), right: axis([1, 0, 0]) } });
  const result = M.retarget(take, lib.bones, { name: "rt", start: made.lead, stance });
  const index = JOINTS.map((j) => lib.bones.findIndex((b) => b.name.toLowerCase() === j));
  const rootIndex = lib.bones.findIndex((b) => b.name.toLowerCase() === "rootdummy");
  const restWorld = posed(lib.bones, null, 0).map((w) => w.q);
  let worst = 0, sum = 0, n = 0, worstAt = "";
  for (let t = 0; t <= Math.min(clip.length, result.length) + 1e-6; t += 1 / 30) {
    const want = posed(lib.bones, clip, t), got = posed(lib.bones, result, t);
    for (let k = 0; k < index.length; k++) {
      const i = index[k];
      if (i < 0 || (extra.strayHands && /hand$/.test(JOINTS[k]))) continue;  // a hand left out follows its forearm: not its knuckles
      const a = want[i].p.map((v, c) => v - want[rootIndex].p[c]), b = got[i].p.map((v, c) => v - got[rootIndex].p[c]);
      const e = Math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
      sum += e; n++;
      if (e > worst) { worst = e; worstAt = JOINTS[k] + " at " + t.toFixed(2) + " s"; }
    }
    // The hands' turn about their length (the hand files' index and pinky knuckles): a point
    // 10 cm to the index side of each hand - a joint's place can't show it.
    for (const name of extra.strayHands ? [] : ["lhand_g", "rhand_g"]) {
      const i = lib.bones.findIndex((b) => b.name.toLowerCase() === name);
      if (i < 0) continue;
      const rest = restWorld[i];
      const side = (pose) => { const since = qmul(pose[i].q, [-rest[0], -rest[1], -rest[2], rest[3]]);
        return add(pose[i].p, rotate(since, [0, 0.1, 0])).map((v, c) => v - pose[rootIndex].p[c]); };
      const a = side(want), b = side(got);
      const e = Math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
      sum += e; n++;
      if (e > worst) { worst = e; worstAt = name + "'s side at " + t.toFixed(2) + " s"; }
    }
  }
  const mean = sum / Math.max(n, 1);
  const notes = take.notes.join("; ");
  const wantNotes = [/resampled from \d+ frames at \d+\.\d a second/];
  if (extra.glitches) wantNotes.push(/glitches removed/);
  if (extra.strayHands) wantNotes.push(/left hand file's points don't sit on the wrist/, /right hand file's points don't sit on the wrist/);
  else wantNotes.push({ test: (s) => !/hand file/.test(s) });
  const notesOk = wantNotes.every((r) => r.test(notes));
  const ok = mean <= limits.mean && worst <= limits.worst && notesOk && capture.format === "rtmpose";
  if (!ok && !extra.info) failed++;
  console.log((extra.info ? "info" : ok ? "ok  " : "FAIL") + " rtmpose " + body + " " + name + " (" +
              Object.keys(extra).filter((k) => extra[k] && k !== "rate" && k !== "info").concat(["uneven " + (extra.rate || 13) + "/s"]).join(", ") +
              "): joints off by mean " + (mean * 100).toFixed(1) + " cm, worst " + (worst * 100).toFixed(1) + " cm (" + worstAt + "); " +
              (notesOk ? "" : "MISSING NOTES; ") + notes);
}
// 13 frames a second through straight lines: a little softer than the 30 a second round trips.
const rtmLimits = { mean: 0.05, worst: 0.14 };
rtmRoundTrip("a_ba", "victorymg", { yaw: 0.65, yUp: true, mirror: false }, {}, rtmLimits);
rtmRoundTrip("a_ba", "nwstab", { yaw: -1.2, yUp: false, mirror: false }, { rate: 60 }, rtmLimits);
// What a rate loses on a fast move (a stab's peak between frames) - printed, not checked.
rtmRoundTrip("a_ba", "nwstab", { yaw: -1.2, yUp: false, mirror: false }, { rate: 25, info: true }, rtmLimits);
rtmRoundTrip("a_ba", "nwstab", { yaw: -1.2, yUp: false, mirror: false }, { info: true }, rtmLimits);
rtmRoundTrip("a_ba", "victorymg", { yaw: 0.65, yUp: true, mirror: false }, { glitches: true }, rtmLimits);
rtmRoundTrip("a_fa", "taunt", { yaw: 0.3, yUp: true, mirror: false }, { strayHands: true }, rtmLimits);
{
  // A frame time column missing or not numbers: no times (the frame rate then comes from the box).
  const ok = M.timesFrom("a,b\n1,2\n3,4\n") === null && M.timesFrom("x,timestamp.from_recording_start.sec\n0,0\n1,nan\n") === null;
  if (!ok) failed++;
  console.log((ok ? "ok  " : "FAIL") + " timestamps: no times from a file without a usable time column");
  // Resampling uneven frames: straight lines between, the even grid from the first time.
  const r = M.resample([Float64Array.of(0), Float64Array.of(10), Float64Array.of(30)], [0, 0.1, 0.3], 10);
  const okR = r.length === 4 && Math.abs(r[1][0] - 10) < 1e-9 && Math.abs(r[2][0] - 20) < 1e-9 && Math.abs(r[3][0] - 30) < 1e-9;
  if (!okR) failed++;
  console.log((okR ? "ok  " : "FAIL") + " resample: " + Array.from(r, (f) => f[0].toFixed(1)).join(" "));
}

// The points round trip (#77): a prepared take written as points (what Blender's add-on
// reads and writes) and read back. With the bench's sliders as marks, only the standing
// part to the end of the kept part is written (both ends cut here); read back, it is the
// same frames (moved as a whole: the first written frame's hips over the origin, the floor
// from the part written), the marks give the same sliders - the same standing and kept
// frames - and it retargets the same. Without marks: the whole take, unmarked.
{
  const lib = libs.get("a_ba");
  const clip = findClip("a_ba", "victorymg");
  const stance = findClip("a_ba", "pause1");
  if (lib && clip) {
    const made = makeRtm(lib.bones, clip, { yaw: 0.65, yUp: true, mirror: false }, stance, {});
    const capture = M.parseCapture(made.body);
    M.addHand(capture, made.left, "left");
    M.addHand(capture, made.right, "right");
    capture.times = M.timesFrom(made.stamps);
    const reference = [0.1, 0.33];
    const take = M.prepare(capture, { fps: 30, reference });
    const marks = { reference, start: made.lead, end: take.duration - 0.3, plants: { left: [[0.6, 1.1], [1.5, 1.9]], right: [] } };
    const span = M.pointsSpan(take, marks);
    const back = M.parseCapture(M.pointsCsv(take, marks));
    const sliders = M.trimFromMarks(back, 30);
    const again = M.prepare(back, { fps: 30, reference: sliders.reference, despike: 0 });
    const shift = [0, 1, 2].map((c) => again.frames[0][c] - take.frames[span.from][c]);
    let worst = 0;
    again.frames.forEach((f, k) => {
      for (let s = 0; s < f.length; s++) worst = Math.max(worst, Math.abs(f[s] - take.frames[k + span.from][s] - shift[s % 3]));
    });
    const rs = M.referenceSpan(again, sliders.reference), ks = M.keepSpan(again, sliders.start, sliders.end);
    const same = rs[0] === span.standing[0] - span.from && rs[1] === span.standing[1] - span.from &&
      ks[0] === span.keep[0] - span.from && ks[1] === span.keep[1] - span.from;
    const a = M.retarget(take, lib.bones, { start: marks.start, end: marks.end, stance });
    const b = M.retarget(again, lib.bones, { start: sliders.start, end: sliders.end, stance });
    let off = 0;
    for (let t = 0; t <= Math.min(a.length, b.length); t += 1 / 30) {
      const pa = posed(lib.bones, a, t), pb = posed(lib.bones, b, t);
      pa.forEach((w, i) => { off = Math.max(off, Math.hypot(...w.p.map((v, c) => v - pb[i].p[c]))); });
    }
    const plantsBack = sliders.plants && sliders.plants.right.length === 0 && sliders.plants.left.length === 2 &&
      sliders.plants.left.every(([x, y], i) => Math.abs(x - (marks.plants.left[i][0] - span.from / 30)) < 1e-3 && Math.abs(y - (marks.plants.left[i][1] - span.from / 30)) < 1e-3);
    const whole = M.parseCapture(M.pointsCsv(take));
    const ok = back.format === "points" && back.times && span.from > 0 && span.to < take.frames.length - 1 &&
      again.frames.length === span.to - span.from + 1 && again.fps === 30 && same && worst < 1e-4 &&
      Math.abs(again.hipHeight - shift[2] - take.hipHeight) < 1e-4 && off < 0.002 && whole.frames.length === take.frames.length && !whole.marks && plantsBack;
    if (!ok) failed++;
    console.log((ok ? "ok  " : "FAIL") + " points round trip: frames " + span.from + "-" + span.to + " of " + take.frames.length + " written (standing " +
                span.standing.join("-") + ", keep " + span.keep.join("-") + "), " + again.frames.length + " back, the same sliders " + (same ? "yes" : "NO") +
                ", the planted feet " + (plantsBack ? "yes" : "NO") + ", points off by " + (worst * 1000).toFixed(3) + " mm, retargeted joints by " +
                (off * 1000).toFixed(2) + " mm; unmarked " +
                whole.frames.length + " frames" + (whole.marks ? " WITH marks" : ", no marks"));
  }
}

// --- Foot locks and where the body stands (#79) ----------------------------------------------------
// Made-up takes from NWN animations, retargeted to the same body, tweaked: legs of other
// proportions (getlow - a crouch, both feet still - at knee angles NWN's legs don't share),
// travel added (walk - NWN's is in place), and taunt as it is (its root moves 37 cm forward).
function madeTake(body, name, extra) {
  const lib = libs.get(body), clip = findClip(body, name), stance = findClip(body, "pause1");
  if (!lib || !clip) return null;
  const opt = { yaw: 0.4, yUp: false, mirror: false };
  const made = makeRtm(lib.bones, clip, opt, stance, Object.assign({ rate: 30 }, extra));
  const capture = M.parseCapture(made.body);
  M.addHand(capture, made.left, "left");
  M.addHand(capture, made.right, "right");
  capture.times = M.timesFrom(made.stamps);
  const o = toRecording([0, 0, 0], opt);
  const axis = (v) => toRecording(v, opt).map((x, c) => x - o[c]);
  const take = M.prepare(capture, { fps: 30, reference: [0, made.lead - 0.1], axes: { up: axis([0, 0, 1]), right: axis([1, 0, 0]) } });
  const bone = (n) => lib.bones.findIndex((b) => b.name.toLowerCase() === n);
  return { lib, clip, stance, made, take, bone };
}
const track = (bones, clip, index, step) => {
  const out = [];
  for (let t = 0; t <= clip.length + 1e-6; t += step || 1 / 30) out.push(posed(bones, clip, t)[index].p);
  return out;
};
const sub3 = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const wander = (ps) => Math.max(...ps.map((p) => Math.hypot(p[0] - ps[0][0], p[1] - ps[0][1])));
// How far a doll's knee sits behind its hip-ankle line at worst (m): behind along the hips' forward
// AND along its foot's (when the foot is flat) - a turned-out leg's knee follows its foot, sideways
// of the hips: not backwards. Between `from` and `to` s (the blends left out: a mix toward NWN's
// stance can lean a knee back a centimetre for a frame).
function kneesBehind(bones, clip, from, to) {
  const bi = (n) => bones.findIndex((b) => b.name.toLowerCase() === n);
  const rest = posed(bones, null, 0);
  const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]], dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  const scale = (a, s) => [a[0] * s, a[1] * s, a[2] * s], norm = (a) => scale(a, 1 / Math.max(Math.hypot(...a), 1e-9));
  let worst = -Infinity;
  for (let t = from || 0; t <= (to === undefined ? clip.length : to) + 1e-6; t += 1 / 30) {
    const p = posed(bones, clip, t);
    const across = sub(p[bi("rthigh_g")].p, p[bi("lthigh_g")].p), hips = [-across[1], across[0], 0];
    for (const s of ["l", "r"]) {
      const [th, sh, ft] = [bi(s + "thigh_g"), bi(s + "shin_g"), bi(s + "foot_g")];
      const u = norm(sub(p[ft].p, p[th].p));
      const behind = (way) => { const f = norm(sub(way, scale(u, dot(way, u)))); return -dot(sub(p[sh].p, p[th].p), f); };
      const r = rest[ft].q, toes = rotate(qmul(p[ft].q, [-r[0], -r[1], -r[2], r[3]]), [0, 1, 0]);
      const flat = Math.abs(toes[2]) < 0.6 * Math.hypot(toes[0], toes[1]);
      worst = Math.max(worst, flat ? Math.min(behind(hips), behind([toes[0], toes[1], 0])) : behind(hips));
    }
  }
  return worst;
}
{
  // Locks: getlow with thighs 1.25x, shins 0.75x - the feet slide on NWN's legs unless locked.
  const m = madeTake("a_ba", "getlow", { legRatio: 1.25 });
  if (m) {
    const plants = M.suggestPlants(m.take);
    const covered = (side) => plants[side].reduce((s, [a, b]) => s + Math.max(0, Math.min(b, m.take.duration) - Math.max(a, m.made.lead)), 0) /
      Math.max(1e-6, m.take.duration - m.made.lead);
    const base = { start: m.made.lead, stance: m.stance, place: "return" };
    const free = M.retarget(m.take, m.lib.bones, base);
    const exact = M.retarget(m.take, m.lib.bones, Object.assign({ plants, reduce: false }, base));
    const held = M.retarget(m.take, m.lib.bones, Object.assign({ plants }, base));
    const slide = (clip) => Math.max(...["lfoot_g", "rfoot_g"].map((n) => wander(track(m.lib.bones, clip, m.bone(n)))));
    const s0 = slide(free), s1 = slide(exact), s2 = slide(held);
    const reach = Math.max(0, ...held.locks.map((l) => l.short));
    const ok = covered("left") > 0.9 && covered("right") > 0.9 && held.locks.length >= 2 && s0 > 0.02 && s1 < 0.0005 && s2 < 0.01 && reach < 0.005;
    if (!ok) failed++;
    console.log((ok ? "ok  " : "FAIL") + " foot locks: getlow, legs 1.25x / 0.75x on a_ba: feet slide " + (s0 * 100).toFixed(1) + " cm free, " +
                (s1 * 1000).toFixed(2) + " mm locked (" + (s2 * 100).toFixed(2) + " cm with the keys reduced as saved; " + held.locks.length +
                " locks, out of reach " + (reach * 100).toFixed(1) + " cm); plants cover " +
                (covered("left") * 100).toFixed(0) + "% / " + (covered("right") * 100).toFixed(0) + "% of the clip");
  }
}
{
  // Knees: a standing take (pause1 - its knees about 6 cm ahead of the hip-ankle line) with them
  // moved 10 cm back, about 4 cm behind (a webcam's straight leg, overdone): the doll's never bend
  // backwards - free or locked (a lock bends the knee over the toes).
  const m = madeTake("a_ba", "pause1", { kneeBack: 0.1 });
  if (m) {
    const opt = { start: m.made.lead, stance: m.stance, place: "return" };
    const free = kneesBehind(m.lib.bones, M.retarget(m.take, m.lib.bones, opt));
    const locked = kneesBehind(m.lib.bones, M.retarget(m.take, m.lib.bones, Object.assign({ plants: M.suggestPlants(m.take) }, opt)));
    const ok = free <= 0.005 && locked <= 0.005;
    if (!ok) failed++;
    console.log((ok ? "ok  " : "FAIL") + " knees: pause1 with the knees moved 10 cm back - the doll's behind the hip-ankle line by " + (free * 100).toFixed(1) +
                " cm at worst, " + (locked * 100).toFixed(1) + " cm locked");
  }
}
{
  // Knees: a flat foot facing backwards (the right one turned round, pause1) doesn't say which way
  // the knee bends - the hips do: its knee keeps pause1's bend, not straightened against the foot.
  const m = madeTake("a_ba", "pause1", { footBack: true });
  if (m) {
    const got = M.retarget(m.take, m.lib.bones, { start: m.made.lead, stance: m.stance, place: "return" });
    const knee = m.bone("rshin_g"), root = m.bone("rootdummy");
    let worst = 0;
    for (let t = 0; t <= Math.min(got.length, m.clip.length) + 1e-6; t += 1 / 30) {
      const a = posed(m.lib.bones, m.clip, t), b = posed(m.lib.bones, got, t);
      const da = [0, 1, 2].map((c) => a[knee].p[c] - a[root].p[c]), db = [0, 1, 2].map((c) => b[knee].p[c] - b[root].p[c]);
      worst = Math.max(worst, Math.hypot(da[0] - db[0], da[1] - db[1], da[2] - db[2]));
    }
    const ok = worst < 0.03;
    if (!ok) failed++;
    console.log((ok ? "ok  " : "FAIL") + " knees: pause1 with the right foot turned round - the knee where pause1's is, off by " + (worst * 100).toFixed(1) + " cm at worst");
  }
}
// The doll's joints off the animation's (root-relative; `turn`: the result turned back about up first) at
// worst, over `joints`.
function jointsOff(m, got, joints, turn) {
  const idx = joints.map((n) => m.bone(n)), root = m.bone("rootdummy");
  const c = Math.cos(-(turn || 0)), s = Math.sin(-(turn || 0));
  let worst = 0, at = "";
  for (let t = 0; t <= Math.min(got.length, m.clip.length) + 1e-6; t += 1 / 30) {
    const a = posed(m.lib.bones, m.clip, t), b = posed(m.lib.bones, got, t);
    idx.forEach((i, k) => {
      const da = [0, 1, 2].map((q) => a[i].p[q] - a[root].p[q]), r = [0, 1, 2].map((q) => b[i].p[q] - b[root].p[q]);
      const db = [r[0] * c - r[1] * s, r[0] * s + r[1] * c, r[2]];
      const e = Math.hypot(da[0] - db[0], da[1] - db[1], da[2] - db[2]);
      if (e > worst) { worst = e; at = joints[k] + " at " + t.toFixed(2) + " s"; }
    });
  }
  return { worst, at };
}
const LEG_JOINTS = ["lshin_g", "lfoot_g", "rshin_g", "rfoot_g"];
{
  // The legs' neutral (#80): a performer leaning 5 degrees forward the whole take (tilted as a whole)
  // - the standing part's lean is posture: the legs come back as
  // the animation's; and so with the body turned a quarter after the standing part (the neutral
  // turns with the pelvis).
  // Against the same take upright: what the round trip itself costs the legs (its frames, its knees).
  for (const turn of [0, Math.PI / 2]) {
    const off = [0, 5].map((d) => {
      const m = madeTake("a_ba", "victorymg", { lean: (d * Math.PI) / 180, turn });
      return m ? jointsOff(m, M.retarget(m.take, m.lib.bones, { start: m.made.lead, stance: m.stance, place: "return" }), LEG_JOINTS, turn) : null;
    });
    if (!off[0]) continue;
    const ok = off[1].worst < off[0].worst + 0.01;
    if (!ok) failed++;
    console.log((ok ? "ok  " : "FAIL") + " legs' neutral: victorymg, leaning 5 degrees forward all take" + (turn ? ", turned a quarter" : "") +
                " - the legs off the animation's by " + (off[1].worst * 100).toFixed(1) + " cm at worst (" + off[1].at + "), upright " +
                (off[0].worst * 100).toFixed(1) + " cm (" + off[0].at + ")");
  }
}
{
  // The neutral stance picked (#80): the standing part becomes it - nwreadyr, the unarmed ready
  // stance (knees bent, hips low): the doll's legs, hips, torso and head and its root where the
  // stance has them (not the arms: the standing part may be a T-pose).
  const m = madeTake("a_ba", "victorymg", {});
  const ready = m && findClip("a_ba", "nwreadyr");
  if (m && ready) {
    const got = M.retarget(m.take, m.lib.bones, { start: 0.05, end: m.made.lead - 0.15, stance: ready, place: "return" });
    const names = ["rootdummy", "pelvis_g", "torso_g", "head_g", ...LEG_JOINTS];
    const idx = names.map((n) => m.bone(n));
    const want = posed(m.lib.bones, ready, 0);
    let worst = 0, at = "";
    for (let t = 0; t <= got.length + 1e-6; t += 1 / 30) {
      const b = posed(m.lib.bones, got, t);
      idx.forEach((i, k) => { const e = Math.hypot(...[0, 1, 2].map((q) => b[i].p[q] - want[i].p[q])); if (e > worst) { worst = e; at = names[k]; } });
    }
    const low = want[m.bone("rootdummy")].p[2] - posed(m.lib.bones, m.stance, 0)[m.bone("rootdummy")].p[2];
    const ok = worst < 0.03;
    if (!ok) failed++;
    console.log((ok ? "ok  " : "FAIL") + " neutral stance nwreadyr: the standing part played as it - off by " + (worst * 100).toFixed(1) +
                " cm at worst (" + at + "; the stance's root " + (low * 100).toFixed(0) + " cm from pause1's)");
  }
}
{
  // Pose keys (#83) on victorymg (a_ba; keys not reduced): the right foot set 50 degrees toes-up from
  // 0.3 to 0.6 s - against its shin turned 50 from the stance's, the shin untouched, nothing changed
  // before the keys' fade; the left forearm adjusted 0 -> 40 degrees forward over 0.3-0.5 s - 40 there,
  // 20 halfway (eased); the body's position 10 cm forward and 5 up at 0.4 s - the root moved so; a thigh
  // turned 15 degrees through a plant (getlow, other legs, locked) - the foot still pinned; the solve
  // cache - the same result as without it, again with other keys, and after the stance or the part kept
  // changes (the same cache object: it re-solves).
  const m = madeTake("a_ba", "victorymg", {});
  if (m) {
    const lead = m.made.lead, at = (t) => lead + t;
    const base = { start: lead, stance: m.stance, place: "return", reduce: false };
    const plain = M.retarget(m.take, m.lib.bones, base);
    const poses = [
      { bone: "rfoot_g", mode: "set", fade: 0.1, keys: [{ t: at(0.3), v: [50, 0, 0] }, { t: at(0.6), v: [50, 0, 0] }] },
      { bone: "lforearm_g", mode: "add", fade: 0.1, keys: [{ t: at(0.3), v: [0, 0, 0] }, { t: at(0.5), v: [40, 0, 0] }] },
      { bone: "rootdummy", mode: "add", fade: 0.1, keys: [{ t: at(0.4), v: [10, 0, 5] }] },
    ];
    const edited = M.retarget(m.take, m.lib.bones, Object.assign({ poses }, base));
    const conj = (q) => [-q[0], -q[1], -q[2], q[3]];
    const ang = (a, b) => (2 * Math.acos(Math.min(1, Math.abs(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]))) * 180) / Math.PI;
    const bone = (clip, t, n) => posed(m.lib.bones, clip, t)[m.bone(n)];
    const rel = (pose, child, parent) => qmul(conj(pose[m.bone(parent)].q), pose[m.bone(child)].q);
    const stance = posed(m.lib.bones, m.stance, 0);
    const footTurn = ang(rel(posed(m.lib.bones, edited, 0.45), "rfoot_g", "rshin_g"), rel(stance, "rfoot_g", "rshin_g"));
    const shinMoved = ang(bone(edited, 0.45, "rshin_g").q, bone(plain, 0.45, "rshin_g").q);
    const before = ang(bone(edited, 0.1, "rfoot_g").q, bone(plain, 0.1, "rfoot_g").q);
    const forearm = ang(bone(edited, 0.5, "lforearm_g").q, bone(plain, 0.5, "lforearm_g").q);
    const halfway = ang(bone(edited, 0.4, "lforearm_g").q, bone(plain, 0.4, "lforearm_g").q);
    const r0 = bone(plain, 0.4, "rootdummy").p, r1 = bone(edited, 0.4, "rootdummy").p;
    const moved = Math.hypot(r1[0] - r0[0], r1[1] - r0[1] - 0.10, r1[2] - r0[2] - 0.05);
    // halfway through its fade-in (0.1 s before its key at 0.4): half of it
    const h0 = bone(plain, 0.35, "rootdummy").p, h1 = bone(edited, 0.35, "rootdummy").p;
    const fadeHalf = Math.hypot(h1[0] - h0[0], h1[1] - h0[1] - 0.05, h1[2] - h0[2] - 0.025);
    // locks under a turned thigh
    const g = madeTake("a_ba", "getlow", { legRatio: 1.25 });
    let slide = 1;
    if (g) {
      const held = M.retarget(g.take, g.lib.bones, { start: g.made.lead, stance: g.stance, place: "return", reduce: false, plants: M.suggestPlants(g.take),
        poses: [{ bone: "rthigh_g", mode: "add", fade: 0.05, keys: [{ t: g.made.lead + 0.1, v: [15, 0, 0] }, { t: g.made.lead + 0.4, v: [15, 0, 0] }] }] });
      slide = wander(track(g.lib.bones, held, g.bone("rfoot_g")));
    }
    // the cache
    const cache = {};
    const c1 = M.retarget(m.take, m.lib.bones, Object.assign({ poses, cache }, base));
    const c2 = M.retarget(m.take, m.lib.bones, Object.assign({ poses: poses.slice(0, 1), cache }, base));
    const u2 = M.retarget(m.take, m.lib.bones, Object.assign({ poses: poses.slice(0, 1) }, base));
    const other = Object.assign({}, base, { stance: findClip("a_ba", "nwreadyr") });
    const c3 = M.retarget(m.take, m.lib.bones, Object.assign({ poses, cache }, other));
    const u3 = M.retarget(m.take, m.lib.bones, Object.assign({ poses }, other));
    const later = Object.assign({}, other, { start: lead + 0.1 });  // the stance as the last call's: only the span changed
    const c4 = M.retarget(m.take, m.lib.bones, Object.assign({ poses, cache }, later));
    const u4 = M.retarget(m.take, m.lib.bones, Object.assign({ poses }, later));
    const same = (x, y) => { if (Math.abs(x.length - y.length) > 1e-9) return Infinity; let d = 0; for (let t = 0; t <= x.length; t += 0.1) { const a = posed(m.lib.bones, x, t), b = posed(m.lib.bones, y, t); a.forEach((w, i) => { d = Math.max(d, Math.hypot(...w.p.map((v, c) => v - b[i].p[c]))); }); } return d; };
    const cached = Math.max(same(c1, edited), same(c2, u2), same(c3, u3), same(c4, u4));
    const ok = Math.abs(footTurn - 50) < 0.5 && shinMoved < 0.1 && before < 0.1 && Math.abs(forearm - 40) < 0.5 && Math.abs(halfway - 20) < 0.5 &&
      moved < 0.001 && fadeHalf < 0.002 && slide < 0.0005 && cached < 1e-9;
    if (!ok) failed++;
    console.log((ok ? "ok  " : "FAIL") + " pose keys: the foot set " + footTurn.toFixed(1) + " degrees from the stance's (shin moved " + shinMoved.toFixed(2) +
                ", before the keys " + before.toFixed(2) + "); the forearm " + forearm.toFixed(1) + " at its key, " + halfway.toFixed(1) + " halfway; the body moved off by " +
                (moved * 1000).toFixed(2) + " mm, halfway through its fade-in by " + (fadeHalf * 1000).toFixed(2) + " mm; a locked foot under a turned thigh slides " + (slide * 1000).toFixed(2) + " mm; cached " + (cached < 1e-9 ? "the same" : "DIFFERENT (" + cached + ")"));
  }
}
{
  // Legs and torso from different sources (#84): both halves from the take change nothing; a torso
  // from an animation (offset, looped) and from the take elsewhere (offset, speed) follow their
  // sources, the legs untouched; legs from an animation follow it (its root too), the torso
  // untouched; "chest keeps its direction" keeps the torso's turn, "turns with the hips" adds the
  // hips' turn; an animation's planted feet (a still stance: both, all along; an in-place walk:
  // none); the hips follow the weight - a bowing torso over standing legs keeps the body's weight
  // over the feet, the feet held (the hips sinking where a leg can't reach); a torso leaned by a
  // pose key the same; the cache, mixed.
  const m = madeTake("a_ba", "victorymg", {});
  const taunt = findClip("a_ba", "taunt"), ready = findClip("a_ba", "nwreadyr"), pause = findClip("a_ba", "pause1");
  const bow = findClip("a_ba", "bow"), walk = findClip("a_ba", "walk");
  if (m && taunt && ready && pause && bow && walk) {
    const bones = m.lib.bones, lead = m.made.lead;
    const conj = (q) => [-q[0], -q[1], -q[2], q[3]];
    const ang = (a, b) => (2 * Math.acos(Math.min(1, Math.abs(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]))) * 180) / Math.PI;
    const nodeOf = (clip, n) => clip.nodes.find((x) => x.name.toLowerCase() === n);
    const localAt = (clip, n, t) => { const node = nodeOf(clip, n); return node && node.orientation ? A.sample(node.orientation, t, true) : bones[m.bone(n)].orientation.values[0]; };
    const rootAt = (clip, t) => A.sample(nodeOf(clip, "rootdummy").position, t, false);
    const LEGS = ["pelvis_g", "lthigh_g", "lshin_g", "lfoot_g", "rthigh_g", "rshin_g", "rfoot_g"], TORSO = ["torso_g", "neck_g", "head_g", "lbicep_g", "rforearm_g", "rhand_g"];
    const base = { start: lead, stance: m.stance, place: "return", reduce: false };
    const plain = M.retarget(m.take, bones, base);
    const frames = (clip) => { const out = []; for (let k = 0; k / 30 <= clip.length + 1e-9; k++) out.push(k / 30); return out; };
    const worst = (clip, f) => Math.max(0, ...frames(clip).map(f));
    const same = (x, y) => { if (Math.abs(x.length - y.length) > 1e-9) return Infinity; let d = 0; for (const t of frames(x)) { const p = posed(bones, x, t), q = posed(bones, y, t); p.forEach((w, i) => { d = Math.max(d, Math.hypot(...w.p.map((v, c) => v - q[i].p[c]))); }); } return d; };
    // both from the take; and with a lock and the balance on the getlow take
    const id = same(M.retarget(m.take, bones, Object.assign({ halves: { legs: { kind: "take" }, torso: { kind: "take", offset: 0, speed: 1 }, seam: "direction", balance: true } }, base)), plain);
    const g = madeTake("a_ba", "getlow", { legRatio: 1.25 });
    const gBase = { start: g.made.lead, stance: g.stance, place: "return", reduce: false, plants: M.suggestPlants(g.take) };
    const idLocked = same(M.retarget(g.take, g.lib.bones, Object.assign({ halves: { legs: { kind: "take" }, torso: { kind: "take" }, seam: "hips", balance: true } }, gBase)),
                          M.retarget(g.take, g.lib.bones, gBase));
    // the torso from taunt, 0.5 s in, looped
    const tSrc = { kind: "clip", clip: taunt, offset: 0.5, speed: 1, loop: true };
    const tm = M.retarget(m.take, bones, Object.assign({ halves: { torso: tSrc, seam: "direction" } }, base));
    const tauntOff = worst(tm, (t) => Math.max(...TORSO.map((n) => ang(localAt(tm, n, t), localAt(taunt, n, M.halfTime(tSrc, t, 0))))));
    const tauntLegs = worst(tm, (t) => Math.max(...LEGS.map((n) => ang(localAt(tm, n, t), localAt(plain, n, t)))));
    // the torso from the take 0.3 s on, 1.5 times as fast: against the whole take's solve
    const whole = M.retarget(m.take, bones, { start: 0, end: 0, stance: m.stance, place: "return", reduce: false });
    const kSrc = { kind: "take", offset: 0.3, speed: 1.5 };
    const km = M.retarget(m.take, bones, Object.assign({ halves: { torso: kSrc, seam: "direction" } }, base));
    const kStart = km.source.start;
    const takeOff = worst(km, (t) => { const T = M.halfTime(kSrc, t, kStart); return T > whole.length ? 0 : Math.max(...TORSO.map((n) => ang(localAt(km, n, t), localAt(whole, n, T)))); });
    // the legs from nwreadyr (looped), no locks: its legs and its root; the torso the take's
    const lSrc = { kind: "clip", clip: ready, offset: 0, speed: 1, loop: true };
    const lm = M.retarget(m.take, bones, Object.assign({ halves: { legs: lSrc, seam: "direction" } }, base));
    const readyLegs = worst(lm, (t) => Math.max(...LEGS.map((n) => ang(localAt(lm, n, t), localAt(ready, n, M.halfTime(lSrc, t, 0))))));
    const readyRoot = worst(lm, (t) => { const a = rootAt(lm, t), b = rootAt(ready, M.halfTime(lSrc, t, 0)); return Math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]); });
    const keptTorso = worst(lm, (t) => Math.max(...TORSO.map((n) => ang(localAt(lm, n, t), localAt(plain, n, t)))));
    // the seam: the torso's turn against the hips' (about up), taken from the stance's
    const stancePose = posed(bones, m.stance, 0);
    const yaw = (pose, n) => { const i = m.bone(n), f = rotate(qmul(pose[i].q, conj(stancePose[i].q)), [0, 1, 0]); return Math.atan2(f[1], f[0]); };
    const wrap = (x) => Math.atan2(Math.sin(x), Math.cos(x));
    const hm = M.retarget(m.take, bones, Object.assign({ halves: { legs: lSrc, seam: "hips" } }, base));
    let hipsGap = 0, dirGap = 0, hipTurn = 0;
    for (const t of frames(hm)) {
      const ph = posed(bones, hm, t), pd = posed(bones, lm, t), pp = posed(bones, plain, t);
      hipsGap = Math.max(hipsGap, Math.abs(wrap((yaw(ph, "torso_g") - yaw(ph, "pelvis_g")) - (yaw(pp, "torso_g") - yaw(pp, "pelvis_g")))) * 180 / Math.PI);
      dirGap = Math.max(dirGap, ang(pd[m.bone("torso_g")].q, pp[m.bone("torso_g")].q));
      hipTurn = Math.max(hipTurn, Math.abs(wrap(yaw(pd, "pelvis_g") - yaw(pp, "pelvis_g"))) * 180 / Math.PI);
    }
    // an animation's planted feet: pause1 (still) - both, all along; walk (in place) - none
    const span = (r) => r.source.end - r.source.start;
    const pm = M.retarget(m.take, bones, Object.assign({ halves: { legs: { kind: "clip", clip: pause, loop: true } }, plants: { left: [], right: [] } }, base));
    const covered = (list) => list.reduce((n, [t0, t1]) => n + t1 - t0, 0);
    const pausePlanted = Math.min(covered(pm.legPlants.left), covered(pm.legPlants.right)) / span(pm);
    const wm = M.retarget(m.take, bones, Object.assign({ halves: { legs: { kind: "clip", clip: walk, loop: true } }, plants: { left: [], right: [] } }, base));
    const walkPlanted = wm.legPlants.left.length + wm.legPlants.right.length;
    // the hips follow the weight: bow's torso over pause1's legs, the feet held
    const MASS = [["torso_g", "neck_g", 0.355], ["head_g", null, 0.081], ["lbicep_g", "lforearm_g", 0.028], ["rbicep_g", "rforearm_g", 0.028], ["lforearm_g", "lhand_g", 0.016],
      ["rforearm_g", "rhand_g", 0.016], ["lhand_g", null, 0.006], ["rhand_g", null, 0.006], ["pelvis_g", null, 0.142], ["lthigh_g", "lshin_g", 0.1], ["rthigh_g", "rshin_g", 0.1],
      ["lshin_g", "lfoot_g", 0.0465], ["rshin_g", "rfoot_g", 0.0465], ["lfoot_g", null, 0.0145], ["rfoot_g", null, 0.0145]];
    const com = (pose) => { let x = 0, y = 0, w = 0; for (const [n, to, k] of MASS) { const p = to ? pose[m.bone(n)].p.map((v, c) => (v + pose[m.bone(to)].p[c]) / 2) : pose[m.bone(n)].p; x += k * p[0]; y += k * p[1]; w += k; } return [x / w, y / w]; };
    const bSrc = { kind: "clip", clip: bow, offset: 0, speed: 1, loop: false };
    const bowOpt = (balance) => Object.assign({ halves: { legs: { kind: "clip", clip: pause, loop: true }, torso: bSrc, seam: "direction", balance } }, base, { plants: { left: [], right: [] } });
    const on = M.retarget(m.take, bones, bowOpt(true)), off = M.retarget(m.take, bones, bowOpt(false));
    let offBy = 0, onBy = 0;
    for (const t of frames(on)) {
      const ref = com(posed(bones, pause, t % pause.length));
      const d = (r) => { const c = com(posed(bones, r, t)); return Math.hypot(c[0] - ref[0], c[1] - ref[1]); };
      offBy = Math.max(offBy, d(off)); onBy = Math.max(onBy, d(on));
    }
    const bowFeet = Math.max(wander(track(bones, on, m.bone("lfoot_g"))), wander(track(bones, on, m.bone("rfoot_g"))));
    // a torso leaned 30 degrees by a pose key over a standing take's held feet
    const st = madeTake("a_ba", "pause1", {});
    const sBase = { start: st.made.lead, stance: st.stance, place: "return", reduce: false, plants: M.suggestPlants(st.take) };
    const lean = [{ bone: "torso_g", mode: "add", fade: 0.2, keys: [{ t: st.made.lead + 0.5, v: [-30, 0, 0] }, { t: st.made.lead + 1.5, v: [-30, 0, 0] }] }];
    const gPlain = M.retarget(st.take, st.lib.bones, sBase);
    const leanOn = M.retarget(st.take, st.lib.bones, Object.assign({ poses: lean, halves: { balance: true } }, sBase));
    const leanOff = M.retarget(st.take, st.lib.bones, Object.assign({ poses: lean }, sBase));
    const gCom = (r, t) => { const pose = posed(st.lib.bones, r, t); let x = 0, y = 0, w = 0; for (const [n, to, k] of MASS) { const i = st.bone(n), j = to ? st.bone(to) : -1;
      const p = j >= 0 ? pose[i].p.map((v, c) => (v + pose[j].p[c]) / 2) : pose[i].p; x += k * p[0]; y += k * p[1]; w += k; } return [x / w, y / w]; };
    const leanBy = (r) => { const a = gCom(r, 1.0), b = gCom(gPlain, 1.0); return Math.hypot(a[0] - b[0], a[1] - b[1]); };
    const leanFeet = Math.max(wander(track(st.lib.bones, leanOn, st.bone("lfoot_g"))), wander(track(st.lib.bones, leanOn, st.bone("rfoot_g"))));
    // the cache, mixed: the torso 2 s on in the take, with the frames another span solved
    const cache = {};
    M.retarget(m.take, bones, Object.assign({ cache }, base, { start: lead + 0.5 }));
    const mixedCached = same(M.retarget(m.take, bones, Object.assign({ cache, halves: { torso: { kind: "take", offset: 0.4, speed: 1.2 } } }, base)),
                             M.retarget(m.take, bones, Object.assign({ halves: { torso: { kind: "take", offset: 0.4, speed: 1.2 } } }, base)));
    const times = [M.halfTime({ kind: "clip", clip: { length: 1 }, offset: 0.2, speed: 2, loop: true }, 0.5, 0), M.halfTime({ kind: "clip", clip: { length: 1 }, offset: 0.2, speed: 2, loop: false }, 0.5, 0),
      M.halfTime({ kind: "take", offset: 0.3, speed: 1.5 }, 1, 4)];
    const timesOk = Math.abs(times[0] - 0.2) < 1e-9 && Math.abs(times[1] - 1) < 1e-9 && Math.abs(times[2] - 5.8) < 1e-9;
    const ok = id < 1e-9 && idLocked < 1e-9 && tauntOff < 0.05 && tauntLegs < 1e-3 && takeOff < 0.5 && readyLegs < 0.05 && readyRoot < 1e-4 && keptTorso < 1e-3 &&
      hipTurn > 10 && hipsGap < 0.5 && dirGap < 1e-3 && pausePlanted > 0.95 && walkPlanted === 0 && offBy > 0.04 && onBy < 0.25 * offBy && bowFeet < 0.005 &&
      leanBy(leanOff) > 0.02 && leanBy(leanOn) < 0.3 * leanBy(leanOff) && leanFeet < 0.005 && mixedCached < 1e-9 && timesOk;
    if (!ok) failed++;
    console.log((ok ? "ok  " : "FAIL") + " legs and torso mixed: both from the take " + (id < 1e-9 && idLocked < 1e-9 ? "the same" : "DIFFERENT (" + id + ", " + idLocked + ")") +
      "; taunt's torso " + tauntOff.toFixed(3) + " deg off it (legs " + tauntLegs.toFixed(4) + "); the take's 0.3 s on x1.5 " + takeOff.toFixed(3) + " deg off; nwreadyr's legs " +
      readyLegs.toFixed(3) + " deg, root " + (readyRoot * 1000).toFixed(2) + " mm off (torso " + keptTorso.toFixed(4) + "); its hips turned up to " + hipTurn.toFixed(0) +
      " deg: chest with the hips " + hipsGap.toFixed(2) + " deg off, keeping its direction " + dirGap.toFixed(4) + "; pause1's feet planted " + (pausePlanted * 100).toFixed(0) +
      "% of it, walk's " + walkPlanted + " plants; bow's torso over pause1's legs: the weight " + (offBy * 100).toFixed(1) + " cm off the feet's, the hips following " +
      (onBy * 100).toFixed(1) + " cm, feet held to " + (bowFeet * 1000).toFixed(1) + " mm; a 30-degree lean by a pose key " + (leanBy(leanOff) * 100).toFixed(1) + " -> " +
      (leanBy(leanOn) * 100).toFixed(1) + " cm (feet " + (leanFeet * 1000).toFixed(1) + " mm); cached mixed " + (mixedCached < 1e-9 ? "the same" : "DIFFERENT") + "; times " + (timesOk ? "ok" : "WRONG " + times.join(", ")));
  } else console.log("skip legs and torso mixed: a clip missing");
}
{
  // A rough capture (#81): feet lost under the floor for 0.35 s - the lost points dropped and
  // filled: the same standing hips as the clean take, the feet on the floor; the whole body 30 cm
  // down for 0.35 s (no limb stretched) - the floor from the standing part, not the take's lowest;
  // a heel point lagging 15 cm (70% longer, 10 cm more) is kept; a heel lost on a walk is filled
  // following its ankle.
  const clean = madeTake("a_ba", "victorymg", {}), lost = madeTake("a_ba", "victorymg", { lostFeet: [0.9, 1.25] });
  if (clean && lost) {
    const P = (f, n) => { const i = M.LANDMARKS.indexOf(n) * 3; return [f[i], f[i + 1], f[i + 2]]; };
    let lowest = Infinity;
    for (let k = Math.round(0.9 * 30); k <= Math.round(1.25 * 30); k++) for (const n of ["left_ankle", "right_ankle", "left_heel", "right_heel"]) lowest = Math.min(lowest, P(lost.take.frames[k], n)[2]);
    const hips = Math.abs(lost.take.hipHeight - clean.take.hipHeight);
    const sunk = madeTake("a_ba", "victorymg", { sink: [0.9, 1.25, 0.3] });
    const floorOff = sunk ? Math.abs(sunk.take.hipHeight - clean.take.hipHeight) : 1;
    const drift = madeTake("a_ba", "victorymg", { heelDrift: [0.8, 1.6] });
    const kept = drift && !drift.take.notes.some((n) => /cameras lost/.test(n));
    const walk = madeTake("a_ba", "walk", { lostHeel: [0.8, 1.2] }), walkClean = madeTake("a_ba", "walk", {});
    // The filled heel stays on its ankle - at its own distance (a straight line through space leaves
    // it behind as the foot moves on); it can't know how the foot rolls meanwhile.
    let heelOff = 0;
    if (walk && walkClean) {
      const bone = (take, k) => Math.hypot(...sub3(P(take.frames[k], "left_heel"), P(take.frames[k], "left_ankle")));
      for (let k = Math.round(0.8 * 30); k <= Math.round(1.2 * 30); k++) heelOff = Math.max(heelOff, Math.abs(bone(walk.take, k) - bone(walkClean.take, k)));
    }
    const ok = hips < 0.02 && lowest > -0.1 && lost.take.notes.some((n) => /cameras lost/.test(n)) && floorOff < 0.01 && kept && heelOff < 0.03;
    if (!ok) failed++;
    console.log((ok ? "ok  " : "FAIL") + " lost points: feet 1.4 m under for 0.35 s - standing hips " + lost.take.hipHeight.toFixed(2) + " m (clean " +
                clean.take.hipHeight.toFixed(2) + "), the feet's lowest there " + (lowest * 100).toFixed(0) + " cm; the body sunk 30 cm: hips off by " +
                (floorOff * 100).toFixed(1) + " cm; a lagging heel " + (kept ? "kept" : "DROPPED") +
                "; a lost heel on a walk at its ankle's distance to " + (heelOff * 100).toFixed(1) + " cm");
  }
}
{
  // In place: a walk with 1.2 m/s of travel added comes back as NWN's own (in place) walk - the
  // root swaying as the walk's does (each about its own middle: the bench centres the hips over
  // the feet's average, NWN's walk sits a little behind); returning, it travels.
  const m = madeTake("a_ba", "walk", { travel: (t) => [0, 1.2 * t] });
  if (m) {
    const opt = { start: m.made.lead, stance: m.stance };
    const inplace = M.retarget(m.take, m.lib.bones, Object.assign({ place: "inplace" }, opt));
    const travel = M.retarget(m.take, m.lib.bones, Object.assign({ place: "return" }, opt));
    const root = m.bone("rootdummy");
    const want = track(m.lib.bones, m.clip, root), got = track(m.lib.bones, inplace, root), went = track(m.lib.bones, travel, root);
    const n = Math.min(want.length, got.length);
    const middle = (ps) => [0, 1].map((c) => ps.slice(0, n).reduce((s, p) => s + p[c], 0) / n);
    const mw = middle(want), mg = middle(got);
    let worst = 0;
    for (let k = 0; k < n; k++) worst = Math.max(worst, Math.hypot(got[k][0] - mg[0] - (want[k][0] - mw[0]), got[k][1] - mg[1] - (want[k][1] - mw[1])));
    const moved = went[Math.min(went.length, n) - 1][1] - went[0][1];
    const ok = worst < 0.03 && moved > 0.9;
    if (!ok) failed++;
    console.log((ok ? "ok  " : "FAIL") + " in place: walk + 1.2 m/s travel on a_ba: the root sways as the walk's does (off by " + (worst * 100).toFixed(1) +
                " cm at worst; centred " + (Math.hypot(mg[0] - mw[0], mg[1] - mw[1]) * 100).toFixed(0) + " cm from it); returning instead, it travels " +
                (moved * 100).toFixed(0) + " cm");
  }
}
{
  // Returning: taunt (its root moves forward) travels as the animation did, then the blend-out
  // brings it back to the stance's spot.
  const m = madeTake("a_ba", "taunt", {});
  if (m) {
    const opt = { start: m.made.lead, stance: m.stance, blendIn: 0, blendOut: 0.3 };
    const back = M.retarget(m.take, m.lib.bones, Object.assign({ place: "return" }, opt));
    const root = m.bone("rootdummy");
    const want = track(m.lib.bones, m.clip, root), got = track(m.lib.bones, back, root);
    const before = Math.round((back.length - 0.3) * 30) - 1;
    let worst = 0;
    for (let k = 0; k <= before; k++) worst = Math.max(worst, Math.abs((got[k][1] - got[0][1]) - (want[k][1] - want[0][1])));
    const stanceRoot = posed(m.lib.bones, m.stance, 0)[root].p;
    const end = posed(m.lib.bones, back, back.length)[root].p;
    const home = Math.hypot(end[0] - stanceRoot[0], end[1] - stanceRoot[1]);
    const net = (ps, k) => Math.hypot(ps[k][0] - ps[0][0], ps[k][1] - ps[0][1]);
    const ok = worst < 0.03 && home < 0.001 && net(want, before) > 0.2;
    if (!ok) failed++;
    console.log((ok ? "ok  " : "FAIL") + " returning: taunt on a_ba travels as the animation did (" + (net(want, before) * 100).toFixed(0) +
                " cm forward, off by " + (worst * 100).toFixed(1) + " cm at worst), ends " + (home * 1000).toFixed(1) + " mm from the stance's spot");
  }
}

// A recording: FreeMoCap's mediapipe_body_3d_xyz.csv, or a recording folder (its RTMPose
// files: body, hands, timestamps).
function readRecording(where) {
  if (!fs.statSync(where).isDirectory()) return M.parseCapture(fs.readFileSync(where, "utf8"));
  const out = path.join(where, "output_data");
  const capture = M.parseCapture(fs.readFileSync(path.join(out, "rtmpose_body_3d_xyz.csv"), "utf8"));
  for (const side of ["left", "right"]) {
    const f = path.join(out, "rtmpose_" + side + "_hand_3d_xyz.csv");
    if (fs.existsSync(f)) M.addHand(capture, fs.readFileSync(f, "utf8"), side);
  }
  const stampsDir = path.join(where, "synchronized_videos", "timestamps");
  const stamps = fs.existsSync(stampsDir) ? fs.readdirSync(stampsDir).find((f) => /_timestamps\.csv$/i.test(f)) : null;
  if (stamps) capture.times = M.timesFrom(fs.readFileSync(path.join(stampsDir, stamps), "utf8"));
  return capture;
}

if (recording) {
  const capture = readRecording(recording);
  const take = M.prepare(capture, { fps: 30, smoothing: 1, reference: [0, 0.33] });
  const trim = M.suggestTrim(take);
  console.log("suggested: standing " + trim.reference.map((v) => v.toFixed(2)).join("-") + " s, keep " + trim.start.toFixed(2) + "-" +
              (trim.end > 0 ? trim.end.toFixed(2) : "end") + " s");
  console.log("recording: " + capture.frames.length + " frames, " + take.duration.toFixed(2) + " s; unit " + take.unit + ", up " +
              take.up.map((v) => v.toFixed(2)).join(" ") + ", forward " + take.forward.map((v) => v.toFixed(2)).join(" ") +
              "; standing hips " + take.hipHeight.toFixed(2) + " m; " + take.notes.join("; "));
  for (const body of ["a_ba", "a_fa", "a_da", "a_dfa", "a_ba2", "a_fa2", "a_da2", "a_dfa2"]) {
    const lib = libs.get(body);
    if (!lib) continue;
    const result = M.retarget(take, lib.bones, { name: "rec", stance: findClip(body, "pause1"), blendIn: 0.3, blendOut: 0.3 });
    let bad = 0, keys = 0, lowest = Infinity, highest = -Infinity;
    for (const node of result.nodes) {
      for (const track of [node.orientation, node.position]) {
        if (!track) continue;
        keys += track.times.length;
        for (const v of track.values) if (v.some((x) => !Number.isFinite(x))) bad++;
      }
    }
    const feet = ["lfoot_g", "rfoot_g"].map((n) => lib.bones.findIndex((b) => b.name.toLowerCase() === n));
    for (let t = 0; t <= result.length; t += 1 / 15) {
      const pose = posed(lib.bones, result, t);
      for (const i of feet) { lowest = Math.min(lowest, pose[i].p[2]); highest = Math.max(highest, pose[i].p[2]); }
    }
    const rest = posed(lib.bones, null, 0);
    const restFoot = rest[feet[0]].p[2];
    const ok = bad === 0 && result.nodes.length >= 16 && lowest > restFoot - 0.15;
    if (!ok) failed++;
    console.log((ok ? "ok  " : "FAIL") + " recording -> " + body + ": " + result.nodes.length + " bones keyed, " + keys + " keys, " + bad +
                " not finite; ankles " + lowest.toFixed(2) + "-" + highest.toFixed(2) + " m (rest " + restFoot.toFixed(2) + ")");
  }
  // Foot locks on the recording (#79): trimmed and planted as the bench suggests, both places,
  // every body - each lock's foot between the blends, start to end on the ground (in place, the
  // ground's own even slide taken out), within 1 cm where the leg reaches the spot.
  {
    let held = M.prepare(capture, { fps: 30, reference: trim.reference });
    const plants = M.suggestPlants(held);
    {
      // A foot's plants apart - 0.15 s or more (a shuffle split leaves room for the foot to move).
      let closest = Infinity;
      for (const side of ["left", "right"]) for (let i = 1; i < plants[side].length; i++) closest = Math.min(closest, plants[side][i][0] - plants[side][i - 1][1]);
      const ok = plants.left.length > 0 && plants.right.length > 0 && closest >= 0.15;
      if (!ok) failed++;
      console.log((ok ? "ok  " : "FAIL") + " recording planted feet suggested: " + plants.left.length + " left, " + plants.right.length + " right, " +
                  (closest * 1000).toFixed(0) + " ms apart at the closest");
    }
    const [k0, k1] = M.keepSpan(held, trim.start, trim.end);
    const blendIn = 0.2, blendOut = 0.3, fps = held.fps;
    {
      // Knees (#79): the capture's often sit behind the hip-ankle line (webcams); no body's do.
      let worst = -Infinity, at = "";
      for (const body of ["a_ba", "a_fa", "a_da", "a_dfa", "a_ba2", "a_fa2", "a_da2", "a_dfa2"]) {
        const lib = libs.get(body);
        if (!lib) continue;
        for (const locked of [false, true]) {
          const clip = M.retarget(held, lib.bones, { start: trim.start, end: trim.end, stance: findClip(body, "pause1"), blendIn: 0.2, blendOut: 0.3,
            plants: locked ? plants : null });
          const b = kneesBehind(lib.bones, clip, 0.2, clip.length - 0.3);  // between the blends
          if (b > worst) { worst = b; at = body + (locked ? " locked" : ""); }
        }
      }
      // Within 2 cm of the line is a straight leg to the eye (about 2.5 degrees): the doll's hips face a
      // degree or two off the capture's, enough to put a straight knee a centimetre either side.
      const ok = worst <= 0.02;
      if (!ok) failed++;
      console.log((ok ? "ok  " : "FAIL") + " recording knees: behind the hip-ankle line by " + (worst * 100).toFixed(1) +
                  " cm at worst on the 8 bodies, free and locked, between the blends (" + at + ")");
    }
    for (const place of ["return", "inplace"]) {
      let worst = 0, worstAt = "", reached = 0, total = 0, free = 0;
      const ground = M.placeGround(held, k0, k1, place);
      for (const body of ["a_ba", "a_fa", "a_da", "a_dfa", "a_ba2", "a_fa2", "a_da2", "a_dfa2"]) {
        const lib = libs.get(body);
        if (!lib) continue;
        const opt = { start: trim.start, end: trim.end, stance: findClip(body, "pause1"), blendIn, blendOut, place, plants };
        const locked = M.retarget(held, lib.bones, opt), loose = M.retarget(held, lib.bones, Object.assign({}, opt, { plants: null }));
        const foot = { left: lib.bones.findIndex((b) => b.name.toLowerCase() === "lfoot_g"), right: lib.bones.findIndex((b) => b.name.toLowerCase() === "rfoot_g") };
        const rest = posed(lib.bones, null, 0), thighs = ["lthigh_g", "rthigh_g"].map((n) => lib.bones.findIndex((b) => b.name.toLowerCase() === n));
        const scale = held.hipHeight > 0.1 ? (rest[thighs[0]].p[2] + rest[thighs[1]].p[2]) / 2 / held.hipHeight : 1;  // the retarget's (rig's hipHeight)
        const drift = (clip, l, a, b) => {
          const at = (k) => { const p = posed(lib.bones, clip, (k - k0) / fps)[foot[l.side]].p, g = ground[k - k0], g0 = ground[a - k0];
            return [p[0] + (g[0] - g0[0]) * scale, p[1] + (g[1] - g0[1]) * scale]; };
          const s0 = [0, 1, 2].map((d) => at(a + d)), s1 = [0, 1, 2].map((d) => at(b - d));
          const m0 = [0, 1].map((c) => s0.reduce((s, p) => s + p[c], 0) / 3), m1 = [0, 1].map((c) => s1.reduce((s, p) => s + p[c], 0) / 3);
          return Math.hypot(m1[0] - m0[0], m1[1] - m0[1]);
        };
        for (const l of locked.locks) {
          const a = Math.max(Math.round(l.takeStart * fps), k0 + Math.round(blendIn * fps)), b = Math.min(Math.round(l.takeEnd * fps), k1 - Math.round(blendOut * fps));
          if (b - a < 5) continue;
          total++;
          if (l.short > 0.03) continue;
          reached++;
          const d = drift(locked, l, a, b);
          free = Math.max(free, drift(loose, l, a, b));
          if (d > worst) { worst = d; worstAt = body + " " + l.side + " " + l.takeStart.toFixed(2) + " s"; }
        }
      }
      const ok = reached > 0 && worst <= 0.01;
      if (!ok) failed++;
      console.log((ok ? "ok  " : "FAIL") + " recording foot locks (" + place + "): " + reached + "/" + total + " locks within 3 cm of reach on the 8 bodies hold to " +
                  (worst * 100).toFixed(2) + " cm at worst (" + worstAt + "); unlocked, those slide up to " + (free * 100).toFixed(1) + " cm");
    }
  }
}
console.log(failed ? failed + " FAILED" : "all ok");
process.exitCode = failed ? 1 : 0;
