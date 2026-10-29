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
}
console.log(failed ? failed + " FAILED" : "all ok");
process.exitCode = failed ? 1 : 0;
