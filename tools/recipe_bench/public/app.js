// Animation Recipe Bench - the page: the game's libraries from the local server,
// the recipe controls, and three figures (base, recipe, mix) drawn on a canvas.
// The recipe maths is core.js (a port of nwn5e's anim_recipe.cpp).
(function () {
  "use strict";
  const A = window.AnimRecipes;
  const $ = (id) => document.getElementById(id);

  const libs = new Map();  // name -> library
  const state = {
    folders: { libraries: "", recipes: "" },
    saved: [],              // [{name, file, source}] from the server
    body: "",
    recipe: { name: "n5_myidle", base: "pausepsn", mix: "pause1", amount: 0.5, speed: 1, start: 0, end: 0, crouch: 0, lean: 0 },
    clips: { base: null, mix: null, result: null },
    playing: true,
    time: 0,
  };

  // --- The server ---------------------------------------------------------------
  async function getJson(url, options) {
    const res = await fetch(url, options);
    const body = await res.json().catch(() => ({}));
    if (!res.ok) throw new Error(body.error || res.status + " " + res.statusText);
    return body;
  }

  async function loadEverything() {
    setLibStatus("Reading…");
    let s;
    try {
      s = await getJson("/api/state");
    } catch (e) {
      setLibStatus("The bench's server isn't answering (" + e.message + "). Start recipe_bench.bat again.", true);
      return;
    }
    applyServerState(s);
    libs.clear();
    const failed = [];
    await Promise.all(s.libraries.map(async (name) => {
      try {
        const res = await fetch("/api/library/" + encodeURIComponent(name));
        if (!res.ok) throw new Error(res.status + "");
        const lib = A.readLibrary(await res.arrayBuffer());
        libs.set(lib.name, lib);
      } catch (e) {
        failed.push(name + " (" + e.message + ")");
      }
    }));
    if (!libs.has(state.body)) state.body = libs.has("a_ba") ? "a_ba" : "";
    $("lib-folder").textContent = s.librariesFolder + "\\";
    if (!libs.size) setLibStatus("No animation libraries in " + s.librariesFolder + " yet.", true);
    else setLibStatus(libs.size + " animation libraries from " + s.librariesFolder + (failed.length ? " - couldn't read " + failed.join(", ") : "") + ".", !!failed.length);
    refreshLibraryUi();
  }

  function applyServerState(s) {
    state.folders = { libraries: s.librariesFolder, recipes: s.recipesFolder };
    state.saved = s.recipes || [];
    const open = $("open");
    const keep = open.value;
    open.textContent = "";
    open.appendChild(new Option(state.saved.length ? "(pick one)" : "(none saved yet)", ""));
    for (const r of state.saved) {
      open.appendChild(new Option((r.source === "yours" ? "yours   " : "nwn5e   ") + r.name, r.source + "|" + r.file));
    }
    open.value = [...open.options].some((o) => o.value === keep) ? keep : "";
    nameNote();
  }

  // --- Libraries, bodies, animation lists ------------------------------------------
  // An animation as a body plays it: its own library's, else its supermodel chain's.
  function findClip(body, name) {
    let current = body;
    for (let depth = 0; depth < 8 && current; depth++) {
      const lib = libs.get(current);
      if (!lib) return null;
      if (lib.clips.has(name.toLowerCase())) return A.readClip(lib, name);
      current = lib.supermodel;
    }
    return null;
  }
  function chainNames(body) {
    const names = new Set();
    let current = body;
    for (let depth = 0; depth < 8 && current; depth++) {
      const lib = libs.get(current);
      if (!lib) break;
      for (const n of lib.clips.keys()) names.add(n);
      current = lib.supermodel;
    }
    return [...names].sort();
  }
  const BODY_ORDER = ["a_ba", "a_fa", "a_da", "a_dfa", "a_ba2", "a_fa2", "a_da2", "a_dfa2"];
  const BODY_NAMES = {
    a_ba: "man (human, elf, halfling)", a_fa: "woman (human, elf, halfling, gnome)",
    a_da: "stout man (dwarf, gnome, half-orc)", a_dfa: "stout woman (dwarf, half-orc)",
    a_ba2: "man, large build", a_fa2: "woman, large build", a_da2: "stout man, large build", a_dfa2: "stout woman, large build",
  };
  function bodies() {
    const supers = new Set([...libs.values()].map((l) => l.supermodel));
    return [...libs.keys()]
      .filter((n) => !supers.has(n) || BODY_ORDER.includes(n))
      .sort((a, b) => (BODY_ORDER.indexOf(a) + 1 || 99) - (BODY_ORDER.indexOf(b) + 1 || 99) || a.localeCompare(b));
  }

  function refreshLibraryUi() {
    const chips = $("chips");
    chips.textContent = "";
    for (const lib of [...libs.values()].sort((a, b) => a.name.localeCompare(b.name))) {
      const c = document.createElement("span");
      c.className = "chip" + (lib.name === state.body ? " on" : "");
      c.textContent = lib.name + " · " + lib.clips.size;
      chips.appendChild(c);
    }
    const select = $("body");
    select.textContent = "";
    for (const name of bodies()) select.appendChild(new Option(name + (BODY_NAMES[name] ? " - " + BODY_NAMES[name] : ""), name));
    select.value = state.body;
    $("empty").hidden = libs.size > 0;
    buildRig();
    refreshAnimationLists();
  }

  function refreshAnimationLists() {
    const names = chainNames(state.body);
    const base = $("base"), mix = $("mix");
    base.textContent = "";
    mix.textContent = "";
    mix.appendChild(new Option("(none)", ""));
    for (const n of names) {
      base.appendChild(new Option(n, n));
      mix.appendChild(new Option(n, n));
    }
    const r = state.recipe;
    r.base = r.base.toLowerCase();
    r.mix = (r.mix || "").toLowerCase();
    if (names.length && !names.includes(r.base)) r.base = names.includes("pause1") ? "pause1" : names[0];
    if (r.mix && names.length && !names.includes(r.mix)) r.mix = "";
    base.value = r.base;
    mix.value = r.mix;
    recompute();
  }

  // --- The recipe -----------------------------------------------------------------------
  const sanitizeName = (text) => String(text).toLowerCase().replace(/[^a-z0-9_]/g, "").slice(0, 32);

  function nodeMap(clip) {
    if (!clip) return null;
    const map = new Map();
    for (const n of clip.nodes) map.set(n.name.toLowerCase(), n);
    clip.byName = map;
    return clip;
  }

  function recompute() {
    const r = state.recipe;
    const base = state.body ? findClip(state.body, r.base) : null;
    const mix = state.body && r.mix ? findClip(state.body, r.mix) : null;
    state.clips.base = nodeMap(base);
    state.clips.mix = nodeMap(mix);
    const lib = libs.get(state.body);  // the crouch solves the legs on this body's bones, as the game's pack does
    state.clips.result = nodeMap(base ? A.deriveClip(r, base, mix, lib ? lib.bones : null) : null);
    const length = base ? base.length : 4;
    $("start").max = $("end").max = length.toFixed(2);
    $("end").value = r.end > 0 && r.end < length ? r.end : length;
    $("start").value = Math.min(r.start, length);
    $("amount").value = r.amount;
    $("speed").value = r.speed;
    $("amount-v").textContent = r.amount.toFixed(2);
    $("speed-v").textContent = r.speed.toFixed(2) + "×";
    $("crouch").value = r.crouch || 0;
    $("lean").value = r.lean || 0;
    $("crouch-v").textContent = (r.crouch || 0).toFixed(2) + " m";
    $("lean-v").textContent = Math.round(r.lean || 0) + "°";
    $("start-v").textContent = r.start.toFixed(2) + " s";
    $("end-v").textContent = r.end > 0 && r.end < length ? r.end.toFixed(2) + " s" : "end";
    $("lab-base").textContent = base ? r.base + " · " + base.length.toFixed(2) + " s" : "–";
    $("lab-mix").textContent = mix ? r.mix + " · " + mix.length.toFixed(2) + " s" : "(none)";
    const result = state.clips.result;
    $("lab-result").textContent = result ? (r.name || "?") + " · " + result.length.toFixed(2) + " s" : "–";
    $("out").textContent = A.writeRecipe(r);
  }

  function nameNote() {
    const name = state.recipe.name;
    const mine = state.saved.find((s) => s.source === "yours" && s.name === name);
    const theirs = state.saved.find((s) => s.source === "nwn5e" && s.name === name);
    $("name-note").textContent = !name ? "Give it a name." :
      mine ? "Save replaces your saved " + name + "." :
      theirs ? "Save makes your own " + name + ", used instead of nwn5e's." : "";
  }

  function setStatus(text, kind) {
    $("status").textContent = text;
    $("status").className = "status" + (kind ? " " + kind : "");
  }
  function setLibStatus(text, bad) {
    $("lib-status").textContent = text;
    $("lib-status").style.color = bad ? "var(--danger)" : "";
  }

  async function save() {
    const r = state.recipe;
    if (!r.name) { setStatus("Give the recipe a name first.", "bad"); return; }
    try {
      const done = await getJson("/api/recipe", { method: "POST", headers: { "Content-Type": "text/plain; charset=utf-8" }, body: A.writeRecipe(r) });
      applyServerState(done.state);
      $("open").value = "yours|" + done.file;
      setStatus((done.replaced ? "Saved over " : "Saved ") + done.path + ". Start the game to build it.", "good");
    } catch (e) {
      setStatus("Not saved: " + e.message, "bad");
    }
  }

  async function openSaved() {
    const value = $("open").value;
    if (!value) return;
    const [source, file] = value.split("|");
    try {
      const res = await fetch("/api/recipe?source=" + encodeURIComponent(source) + "&file=" + encodeURIComponent(file));
      if (!res.ok) throw new Error((await res.json().catch(() => ({}))).error || res.status + "");
      const r = A.readRecipe(await res.text());
      state.recipe = Object.assign(state.recipe, r, { name: sanitizeName(r.name) });
      $("name").value = state.recipe.name;
      nameNote();
      refreshAnimationLists();
      setStatus("Opened " + state.recipe.name + (source === "nwn5e" ? " (nwn5e's own)." : "."));
    } catch (e) {
      setStatus("Couldn't open it: " + e.message, "bad");
    }
  }

  function bindInputs() {
    const r = () => state.recipe;
    $("name").addEventListener("input", () => { r().name = sanitizeName($("name").value); nameNote(); recompute(); });
    $("name").addEventListener("change", () => { $("name").value = r().name; });
    $("base").addEventListener("change", () => { r().base = $("base").value; r().start = 0; r().end = 0; recompute(); });
    $("mix").addEventListener("change", () => { r().mix = $("mix").value; recompute(); });
    $("amount").addEventListener("input", () => { r().amount = parseFloat($("amount").value); recompute(); });
    $("speed").addEventListener("input", () => { r().speed = parseFloat($("speed").value); recompute(); });
    $("crouch").addEventListener("input", () => { r().crouch = parseFloat($("crouch").value); recompute(); });
    $("lean").addEventListener("input", () => { r().lean = parseFloat($("lean").value); recompute(); });
    $("start").addEventListener("input", () => {
      const end = r().end > 0 ? r().end : parseFloat($("end").max);
      r().start = Math.max(0, Math.min(parseFloat($("start").value), end - 0.05));
      recompute();
    });
    $("end").addEventListener("input", () => {
      const max = parseFloat($("end").max);
      const v = Math.max(parseFloat($("end").value), r().start + 0.05);
      r().end = v >= max - 0.005 ? 0 : v;
      recompute();
    });
    $("body").addEventListener("change", () => { state.body = $("body").value; refreshLibraryUi(); });
    $("reload").addEventListener("click", loadEverything);
    $("open").addEventListener("change", openSaved);
    $("save").addEventListener("click", save);
    $("copy").addEventListener("click", () => {
      const text = A.writeRecipe(r());
      navigator.clipboard.writeText(text).then(() => setStatus("Copied."), () => setStatus("Couldn't copy - open 'The recipe file' below and copy it from there.", "bad"));
    });
    $("play").addEventListener("click", () => setPlaying(!state.playing));
    $("scrub").addEventListener("input", () => {
      const result = state.clips.result;
      if (!result) return;
      setPlaying(false);
      state.time = parseFloat($("scrub").value) * result.length;
    });
    $("view").addEventListener("click", () => Object.assign(view, { yaw: 0, pitch: 0.12, distance: view.home }));
    document.addEventListener("keydown", (e) => {
      if (e.key === " " && e.target === document.body) { e.preventDefault(); setPlaying(!state.playing); }
    });
  }
  function setPlaying(on) {
    state.playing = on;
    $("play").textContent = on ? "Pause" : "Play";
    $("play").setAttribute("aria-pressed", String(on));
  }

  // --- The figures ------------------------------------------------------------------
  // Game space is Z up, the figures face +Y. The camera orbits a point at chest height.
  const canvas = $("canvas");
  const ctx = canvas.getContext("2d");
  const view = { yaw: 0.35, pitch: 0.12, distance: 7.2, home: 7.2, target: [0, 0, 0.95], fov: (32 * Math.PI) / 180 };
  const SPOTS = [1.5, 0, -1.5];  // base, recipe, mix along X (seen from the front, +X is on the left)
  // Bones drawn, and how thick (metres).
  const LIMBS = {
    neck_g: 0.06, head_g: 0.045, lforearm_g: 0.035, lhand_g: 0.03, lhand: 0.025, rforearm_g: 0.035, rhand_g: 0.03, rhand: 0.025,
    lthigh_g: 0.05, lshin_g: 0.042, lfoot_g: 0.035, rthigh_g: 0.05, rshin_g: 0.042, rfoot_g: 0.035, lbicep_g: 0.045, rbicep_g: 0.045,
  };
  const JOINTS = ["neck_g", "lbicep_g", "lforearm_g", "lhand_g", "rbicep_g", "rforearm_g", "rhand_g", "pelvis_g", "lthigh_g", "lshin_g", "lfoot_g", "rthigh_g", "rshin_g", "rfoot_g"];
  let rig = null;  // the body's bone list with what to draw between them

  function buildRig() {
    const lib = libs.get(state.body);
    if (!lib) { rig = null; return; }
    const index = new Map(lib.bones.map((b, i) => [b.name.toLowerCase(), i]));
    const segments = [];
    lib.bones.forEach((b, i) => {
      const name = b.name.toLowerCase();
      if (b.parent < 0 || !(name in LIMBS)) return;
      const parent = lib.bones[b.parent].name.toLowerCase();
      if (parent === "rootdummy") return;
      segments.push({ a: b.parent, b: i, r: LIMBS[name] });
    });
    // The torso bone sits at the waist: a spine up to the neck, clavicles out to the shoulders.
    const link = (a, b, r) => { if (index.has(a) && index.has(b)) segments.push({ a: index.get(a), b: index.get(b), r }); };
    link("torso_g", "neck_g", 0.07);
    link("neck_g", "lbicep_g", 0.04);
    link("neck_g", "rbicep_g", 0.04);
    link("pelvis_g", "lthigh_g", 0.055);
    link("pelvis_g", "rthigh_g", 0.055);
    rig = {
      lib,
      segments: segments.filter((s, i, all) => all.findIndex((o) => o.a === s.a && o.b === s.b) === i),
      joints: JOINTS.filter((n) => index.has(n)).map((n) => index.get(n)),
      head: index.has("head_g") && index.has("head") ? [index.get("head_g"), index.get("head")] : null,
    };
  }

  const qmul = (a, b) => [
    a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
    a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
    a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
    a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2],
  ];
  function rotate(q, v) {
    const p = qmul(qmul(q, [v[0], v[1], v[2], 0]), [-q[0], -q[1], -q[2], q[3]]);
    return [p[0], p[1], p[2]];
  }

  // World positions of every bone: the rest pose, the clip's keys over it.
  function pose(clip, t, x) {
    const bones = rig.lib.bones;
    const world = new Array(bones.length);
    for (let i = 0; i < bones.length; i++) {
      const b = bones[i];
      let p = b.position ? b.position.values[0] : [0, 0, 0];
      let q = b.orientation ? b.orientation.values[0] : [0, 0, 0, 1];
      const node = clip && clip.byName.get(b.name.toLowerCase());
      if (node && node.orientation) q = A.sample(node.orientation, t, true);
      if (node && node.position) p = A.sample(node.position, t, false);
      if (b.parent < 0) world[i] = { p: [p[0] + x, p[1], p[2]], q };
      else {
        const up = world[b.parent];
        const r = rotate(up.q, p);
        world[i] = { p: [up.p[0] + r[0], up.p[1] + r[1], up.p[2] + r[2]], q: qmul(up.q, q) };
      }
    }
    return world.map((w) => w.p);
  }

  let colors = {};
  function readColors() {
    const css = getComputedStyle(document.documentElement);
    const get = (n) => css.getPropertyValue(n).trim();
    colors = { side: hex(get("--figure-side")), accent: hex(get("--accent")), stage: hex(get("--stage")), line: get("--line"), shadow: get("--shadow") };
  }
  function hex(text) {
    const m = /^#([0-9a-f]{6})$/i.exec(text);
    const n = m ? parseInt(m[1], 16) : 0x888888;
    return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
  }
  const shade = (c, f, toward) => "rgb(" + c.map((v, i) => Math.round(v + (toward[i] - v) * f)).join(",") + ")";

  let W = 0, H = 0, F = 1, camera = null;
  function resize() {
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    W = canvas.clientWidth;
    H = canvas.clientHeight;
    canvas.width = Math.round(W * dpr);
    canvas.height = Math.round(H * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    const home = W < 560 ? 9.5 : 7.2;
    if (home !== view.home) { view.home = home; view.distance = home; }
  }

  function setCamera() {
    const { yaw, pitch, distance: d, target: t } = view;
    const eye = [t[0] + d * Math.sin(yaw) * Math.cos(pitch), t[1] + d * Math.cos(yaw) * Math.cos(pitch), t[2] + d * Math.sin(pitch)];
    const f = norm([t[0] - eye[0], t[1] - eye[1], t[2] - eye[2]]);
    const right = norm(cross(f, [0, 0, 1]));
    camera = { eye, f, right, up: cross(right, f) };
    F = H / 2 / Math.tan(view.fov / 2);
  }
  const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  const norm = (a) => { const l = Math.hypot(a[0], a[1], a[2]) || 1; return [a[0] / l, a[1] / l, a[2] / l]; };
  const NEAR = 0.2;
  function toCamera(p) {
    const v = [p[0] - camera.eye[0], p[1] - camera.eye[1], p[2] - camera.eye[2]];
    return [dot(v, camera.right), dot(v, camera.up), dot(v, camera.f)];
  }
  const toScreen = (c) => [W / 2 + (F * c[0]) / c[2], H / 2 - (F * c[1]) / c[2]];

  // A floor line, cut where it passes behind the camera.
  function floorLine(a, b) {
    let ca = toCamera(a), cb = toCamera(b);
    if (ca[2] < NEAR && cb[2] < NEAR) return;
    if (ca[2] < NEAR || cb[2] < NEAR) {
      const k = (NEAR - ca[2]) / (cb[2] - ca[2]);
      const cut = [ca[0] + (cb[0] - ca[0]) * k, ca[1] + (cb[1] - ca[1]) * k, NEAR];
      if (ca[2] < NEAR) ca = cut; else cb = cut;
    }
    const sa = toScreen(ca), sb = toScreen(cb);
    ctx.moveTo(sa[0], sa[1]);
    ctx.lineTo(sb[0], sb[1]);
  }

  function draw() {
    ctx.fillStyle = "rgb(" + colors.stage.join(",") + ")";
    ctx.fillRect(0, 0, W, H);
    setCamera();
    // The floor.
    ctx.beginPath();
    for (let v = -4; v <= 4.001; v += 0.5) {
      floorLine([v, -3, 0], [v, 3, 0]);
      if (Math.abs(v) <= 3.001) floorLine([-4, v, 0], [4, v, 0]);
    }
    ctx.strokeStyle = colors.line;
    ctx.lineWidth = 1;
    ctx.stroke();
    if (!rig) return;

    const { base, mix, result } = state.clips;
    const at = (clip) => (clip && clip.length > 0 ? state.time % clip.length : 0);
    const figures = [
      { clip: base, color: colors.side, x: SPOTS[0], show: true },
      { clip: result, color: colors.accent, x: SPOTS[1], show: true },
      { clip: mix, color: colors.side, x: SPOTS[2], show: !!mix },
    ];
    const items = [];
    for (const fig of figures) {
      if (!fig.show) continue;
      const points = pose(fig.clip, at(fig.clip), fig.x);
      const cam = points.map(toCamera);
      if (cam.some((c) => c[2] < NEAR)) continue;
      const screen = cam.map(toScreen);
      // Its shadow: the limbs pressed flat on the floor.
      ctx.beginPath();
      const flat = points.map((p) => toScreen(toCamera([p[0], p[1], 0])));
      for (const s of rig.segments) { ctx.moveTo(flat[s.a][0], flat[s.a][1]); ctx.lineTo(flat[s.b][0], flat[s.b][1]); }
      ctx.strokeStyle = colors.shadow;
      ctx.lineCap = "round";
      ctx.lineWidth = (F * 0.07) / view.distance;
      ctx.stroke();
      const depthShade = (z) => Math.max(0, Math.min(0.35, (z - view.distance + 0.6) * 0.18));
      for (const s of rig.segments) {
        const z = (cam[s.a][2] + cam[s.b][2]) / 2;
        items.push({ z, kind: 0, a: screen[s.a], b: screen[s.b], w: (2 * F * s.r) / z, color: shade(fig.color, depthShade(z), colors.stage) });
      }
      for (const j of rig.joints) {
        const z = cam[j][2];
        items.push({ z: z - 0.01, kind: 1, a: screen[j], w: (F * 0.05) / z, color: shade(fig.color, depthShade(z) + 0.12, [0, 0, 0]) });
      }
      if (rig.head) {
        const [g, h] = rig.head;
        const p = [0, 1, 2].map((k) => points[g][k] + (points[h][k] - points[g][k]) * 0.55);
        const c = toCamera(p);
        items.push({ z: c[2], kind: 2, a: toScreen(c), w: (F * 0.115) / c[2], color: shade(fig.color, depthShade(c[2]), colors.stage), edge: shade(fig.color, 0.3, [0, 0, 0]) });
      }
    }
    items.sort((p, q) => q.z - p.z);
    ctx.lineCap = "round";
    for (const it of items) {
      if (it.kind === 0) {
        ctx.beginPath();
        ctx.moveTo(it.a[0], it.a[1]);
        ctx.lineTo(it.b[0], it.b[1]);
        ctx.strokeStyle = it.color;
        ctx.lineWidth = it.w;
        ctx.stroke();
      } else {
        ctx.beginPath();
        ctx.arc(it.a[0], it.a[1], it.w, 0, Math.PI * 2);
        ctx.fillStyle = it.color;
        ctx.fill();
        if (it.edge) { ctx.strokeStyle = it.edge; ctx.lineWidth = 1.5; ctx.stroke(); }
      }
    }
  }

  // Drag to turn, wheel to zoom.
  let dragging = null;
  canvas.addEventListener("pointerdown", (e) => { dragging = { x: e.clientX, y: e.clientY }; canvas.setPointerCapture(e.pointerId); });
  canvas.addEventListener("pointermove", (e) => {
    if (!dragging) return;
    view.yaw += (e.clientX - dragging.x) * 0.008;
    view.pitch = Math.max(-0.15, Math.min(1.3, view.pitch + (e.clientY - dragging.y) * 0.006));
    dragging = { x: e.clientX, y: e.clientY };
  });
  canvas.addEventListener("pointerup", () => { dragging = null; });
  canvas.addEventListener("wheel", (e) => {
    e.preventDefault();
    view.distance = Math.max(2.5, Math.min(16, view.distance * (1 + Math.sign(e.deltaY) * 0.08)));
  }, { passive: false });

  let last = performance.now();
  function frame(now) {
    const dt = Math.min(0.1, (now - last) / 1000);
    last = now;
    if (state.playing) state.time += dt;
    draw();
    const result = state.clips.result;
    if (result && result.length > 0) {
      const t = state.time % result.length;
      $("scrub").value = (t / result.length).toFixed(3);
      $("clock").textContent = t.toFixed(2) + " / " + result.length.toFixed(2) + " s";
    }
    requestAnimationFrame(frame);
  }

  // --- Start ------------------------------------------------------------------------
  readColors();
  if (window.matchMedia) window.matchMedia("(prefers-color-scheme: dark)").addEventListener("change", readColors);
  new ResizeObserver(resize).observe(canvas);
  resize();
  bindInputs();
  recompute();
  requestAnimationFrame(frame);
  loadEverything();
})();
