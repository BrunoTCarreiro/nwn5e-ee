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
    hold: "",                                       // a weapon's walk layer to preview: swordr, swordl, shieldl, bowl
    layers: { base: null, mix: null, result: null },
    playing: true,
    time: 0,
    mode: "recipe",  // or "mocap"
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
    if (s.stale) setStatus("The bench was updated since its window started: close that window and start recipe_bench.bat again, then reload this page.", "bad");
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
    if (state.mode === "mocap") { recomputeMocap(); return; }
    const r = state.recipe;
    const base = state.body ? findClip(state.body, r.base) : null;
    const mix = state.body && r.mix ? findClip(state.body, r.mix) : null;
    state.clips.base = nodeMap(base);
    state.clips.mix = nodeMap(mix);
    const lib = libs.get(state.body);  // the crouch solves the legs on this body's bones, as the game's pack does
    state.clips.result = nodeMap(base ? A.deriveClip(r, base, mix, lib ? lib.bones : null) : null);
    // Holding a weapon: the game plays <walk>_<hold> over the walk, the torso up; nwn5e builds
    // the recipe's own layer the same way (anim_packs.cpp) - previewed here as the game layers it.
    const hold = state.hold;
    const layerOf = (name) => (hold && name && state.body ? findClip(state.body, name + "_" + hold) : null);
    state.layers.base = nodeMap(base ? layerOf(r.base) : null);
    state.layers.mix = nodeMap(mix ? layerOf(r.mix) : null);
    state.layers.result = state.layers.base && lib
      ? nodeMap(A.deriveClip(Object.assign({}, r, { name: r.name + "_" + hold, mix: state.layers.mix ? r.mix + "_" + hold : "" }),
          layerOf(r.base), state.layers.mix ? layerOf(r.mix) : null, lib.bones))
      : null;
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
    const held = (layer) => (layer ? " + " + layer.name : "");
    $("lab-base").textContent = base ? r.base + held(state.layers.base) + " · " + base.length.toFixed(2) + " s" : "–";
    $("lab-mix").textContent = mix ? r.mix + " · " + mix.length.toFixed(2) + " s" : "(none)";
    const result = state.clips.result;
    $("lab-result").textContent = result ? (r.name || "?") + (state.layers.result ? " + " + state.layers.result.name : "") + " · " + result.length.toFixed(2) + " s" : "–";
    $("hold-note").textContent = hold && base && !state.layers.base ? r.base + " has no " + hold + " layer: the game plays it as it is." : "";
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
      // Read it back: every setting must have arrived.
      const back = A.readRecipe(await (await fetch("/api/recipe?source=yours&file=" + encodeURIComponent(done.file))).text());
      const lost = ["base", "mix", "amount", "speed", "start", "end", "crouch", "lean"].filter((k) =>
        typeof r[k] === "number" ? Math.abs((back[k] || 0) - r[k]) > 1e-4 : String(back[k] || "").toLowerCase() !== String(r[k] || "").toLowerCase());
      if (lost.length) {
        setStatus("Saved " + done.path + ", but " + lost.join(", ") + " didn't arrive: the bench's server is out of date. Close its window, start recipe_bench.bat again, then Save again.", "bad");
        return;
      }
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
      if (r.keyed) {  // Mocap mode's: its keys, shown in that mode
        openKeysRecipe(r);
        setStatus("Opened " + r.name + " - a capture's keys: in the Mocap tab.");
        return;
      }
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
    $("hold").addEventListener("change", () => { state.hold = $("hold").value; recompute(); });
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
    bindMocap();
  }
  function setPlaying(on) {
    state.playing = on;
    $("play").textContent = on ? "Pause" : "Play";
    $("play").setAttribute("aria-pressed", String(on));
  }


  // --- Mocap mode (mocap.js; docs/PLAN.md "Mocap mode") ------------------------------------
  // A FreeMoCap capture retargeted to the body shown, played with its points over it; Save
  // retargets it to every body and writes a keys recipe (anim_recipe.cpp builds it per library).
  const MC = window.Mocap;
  const mc = {
    capture: null, take: null, fileName: "", fps: 30, fpsFromFile: false, fpsSource: "", despike: true, profile: null, result: null,
    slot: "n5_rage", name: "n5_mocap", smoothing: 0, ref: [0, 0.33], start: 0, end: 0,
    blendIn: 0.2, blendOut: 0.3, loop: false, sway: true, fitLength: 0, contactAt: 0, contact: null, tolerance: 0.75,
    saved: null,  // a keys recipe opened without its capture: its own keys, shown as they are
  };
  const SLOTS = {
    n5_rage: { blendIn: 0.2, blendOut: 0.3, loop: false, fit: false, note: "plays when a Rage starts, in place of the victory flourish." },
    n5_cridle: { blendIn: 0, blendOut: 0, loop: true, fit: false, note: "plays as the crouched idle while hiding (a loop)." },
    n5_shove: { blendIn: 0.1, blendOut: 0.15, loop: false, fit: true, note: "plays as a player's Shove swing, in place of nwstab - fitted to its length." },
  };
  const mcName = () => (mc.slot || sanitizeName(mc.name));

  function setMcStatus(text, kind) {
    $("mc-status").textContent = text;
    $("mc-status").className = "status" + (kind ? " " + kind : "");
  }

  function setMode(mode) {
    state.mode = mode;
    $("tab-recipe").setAttribute("aria-selected", String(mode === "recipe"));
    $("tab-mocap").setAttribute("aria-selected", String(mode === "mocap"));
    $("recipe-card").hidden = mode !== "recipe";
    $("mocap-card").hidden = mode !== "mocap";
    const spans = $("labels").querySelectorAll("span");
    spans[0].hidden = spans[2].hidden = mode === "mocap";
    spans[1].firstChild.textContent = mode === "mocap" ? "Mocap" : "Recipe";
    $("labels").classList.toggle("one", mode === "mocap");
    recompute();
  }

  function applySlot(slot) {
    mc.slot = slot;
    $("mc-name-l").hidden = !!slot;
    const preset = SLOTS[slot];
    if (preset) {
      mc.blendIn = preset.blendIn;
      mc.blendOut = preset.blendOut;
      mc.loop = preset.loop;
      if (preset.fit) {
        const swing = state.body ? findClip(state.body, "nwstab") : null;
        mc.fitLength = swing ? Math.round(swing.length * 100) / 100 : 1;
        mc.contactAt = Math.round(mc.fitLength * 0.5 * 100) / 100;
      } else {
        mc.fitLength = 0;
      }
    }
    syncMocapInputs();
    recompute();
  }

  function syncMocapInputs() {
    $("mc-slot").value = mc.slot;
    $("mc-name").value = mc.name;
    $("mc-fps").value = mc.fps;
    $("mc-smooth").value = mc.smoothing;
    $("mc-glitch").checked = mc.despike;
    $("mc-ref0").value = mc.ref[0];
    $("mc-ref1").value = mc.ref[1];
    $("mc-start").value = mc.start;
    $("mc-end").value = mc.end > 0 ? mc.end : $("mc-end").max;
    $("mc-in").value = mc.blendIn;
    $("mc-out").value = mc.blendOut;
    $("mc-loop").checked = mc.loop;
    $("mc-sway").checked = mc.sway;
    $("mc-fit").value = mc.fitLength;
    $("mc-at").value = mc.contactAt;
    $("mc-contact").value = mc.contact || 0;
    $("mc-tol").value = mc.tolerance;
  }

  function mocapOptions(body) {
    const fit = mc.fitLength > 0;
    return { name: mcName(), start: mc.start, end: mc.end, sway: mc.sway, stance: findClip(body, "pause1"),
      blendIn: mc.blendIn, blendOut: mc.blendOut, loop: mc.loop, loopBlend: 0.25, fitLength: mc.fitLength,
      contact: fit && mc.contact !== null ? mc.contact : null, contactAt: fit && mc.contactAt > 0 ? mc.contactAt : null,
      tolerance: mc.tolerance };
  }

  // A recording's files, by name (a whole folder may come: the rest are passed over):
  // the body (RTMPose's or MediaPipe's, the rigid one if that's all), its hands, the
  // frame times (synchronized_videos/timestamps/<take>_timestamps.csv - not the per-camera
  // ones), MediaPipe's recording_parameters.json; any other CSV as the body when nothing
  // else is.
  function sortRecordingFiles(files) {
    const pick = { body: null, rigid: null, left: null, right: null, stamps: null, json: null, other: null };
    for (const f of files) {
      const n = f.name.toLowerCase();
      if (/recording_parameters\.json$/.test(n)) pick.json = f;
      else if (/_points.*\.csv$/.test(n)) pick.body = pick.body || f;  // the Blender round trip's file
      else if (/left_hand_3d_xyz\.csv$/.test(n)) pick.left = f;
      else if (/right_hand_3d_xyz\.csv$/.test(n)) pick.right = f;
      else if (/_body_rigid_3d_xyz\.csv$/.test(n)) pick.rigid = f;
      else if (/(rtmpose|mediapipe)_body_3d_xyz\.csv$/.test(n)) pick.body = f;
      else if (/_timestamps\.csv$/.test(n) && !/\.camera/.test(n)) pick.stamps = f;
      else if (/body_trajectories\.csv$/.test(n) || (/\.(csv|txt)$/.test(n) && !pick.other && !/(face|hand|center_of_mass|by_frame|timestamps)/.test(n))) pick.other = f;
    }
    pick.body = pick.body || pick.rigid || pick.other;
    return pick;
  }

  // Dropped folders, walked (CSV and JSON files only).
  async function filesFromDrop(transfer) {
    const entries = [...(transfer.items || [])].map((i) => (i.webkitGetAsEntry ? i.webkitGetAsEntry() : null)).filter(Boolean);
    if (!entries.some((e) => e.isDirectory)) return [...transfer.files];
    const out = [];
    async function walk(entry, depth) {
      if (entry.isFile) {
        if (/\.(csv|json)$/i.test(entry.name)) out.push(await new Promise((ok, no) => entry.file(ok, no)));
        return;
      }
      if (!entry.isDirectory || depth > 4) return;
      const reader = entry.createReader();
      for (;;) {
        const batch = await new Promise((ok, no) => reader.readEntries(ok, no));
        if (!batch.length) break;
        for (const e of batch) await walk(e, depth + 1);
      }
    }
    for (const e of entries) await walk(e, 0);
    return out;
  }

  async function loadMocapFiles(files, folderName) {
    const pick = sortRecordingFiles(files);
    try {
      if (pick.json) {
        const rate = MC.frameRateFrom(await pick.json.text());
        if (rate) { mc.fps = rate; mc.fpsFromFile = true; mc.fpsSource = "recording_parameters.json"; }
      }
      if (pick.body) {
        const capture = MC.parseCapture(await pick.body.text());
        const hands = [];
        for (const side of ["left", "right"]) {
          if (pick[side]) { MC.addHand(capture, await pick[side].text(), side); hands.push(side); }
        }
        let timing = "";
        if (capture.times) {  // a points file (the Blender round trip): its own times
          mc.fps = 30;
          mc.fpsFromFile = true;
          mc.fpsSource = "the points file's times";
          // Cleaned already: the glitch filter would "fix" a point moved by hand (a heel left
          // behind by a lifted ankle reads as a glitch) - off until ticked.
          mc.despike = false;
          timing = " A points file, cleaned already: the glitch filter is off (tick Remove glitches to run it again).";
        } else if (pick.stamps) {
          const times = MC.timesFrom(await pick.stamps.text());
          if (times && times.length === capture.frames.length) {
            capture.times = times;
            mc.fps = 30;
            mc.fpsFromFile = true;
            mc.fpsSource = "resampled from the recording's frame times";
          } else {
            timing = times ? " Its timestamps list " + times.length + " frames, the capture " + capture.frames.length + ": not used." : "";
          }
        } else if (capture.format === "rtmpose") {
          timing = " No frame times (synchronized_videos/timestamps/…_timestamps.csv): set the frame rate by hand.";
          mc.fpsFromFile = false;
        }
        mc.capture = capture;
        mc.fileName = (folderName ? folderName + ": " : "") + pick.body.name + (hands.length ? " + " + hands.join(" and ") + " hand" + (hands.length > 1 ? "s" : "") : "");
        mc.saved = null;
        mc.start = 0;
        mc.end = 0;
        mc.contact = null;
        mc.fresh = true;
        mc.timingNote = timing;
      }
      if (!mc.capture) {
        setMcStatus(pick.json ? "Frame rate " + mc.fps + " read: now drop the capture's CSV." : "No capture in it: drop a FreeMoCap recording folder or its body CSV.", pick.json ? "" : "bad");
        syncMocapInputs();
        return;
      }
      prepareTake();
      setMcStatus("");
    } catch (e) {
      setMcStatus("Couldn't read it: " + e.message, "bad");
    }
  }

  // The take's points, for Blender (#77): downloaded as <take>_points.csv.
  function exportPoints() {
    if (!mc.take) { setMcStatus("Drop a capture first.", "bad"); return; }
    const base = (mc.fileName.split(":")[0] || "take").replace(/\.csv.*$/i, "").replace(/[^A-Za-z0-9_.-]+/g, "_").replace(/_points$/, "");
    const blob = new Blob([MC.pointsCsv(mc.take)], { type: "text/csv" });
    const link = document.createElement("a");
    link.href = URL.createObjectURL(blob);
    link.download = base + "_points.csv";
    document.body.appendChild(link);
    link.click();
    link.remove();
    setTimeout(() => URL.revokeObjectURL(link.href), 5000);
    setMcStatus("Exported " + link.download + " (" + mc.take.frames.length + " frames, " + mc.take.fps + " a second) to your downloads: " +
      "in Blender, File > Import > nwn5e mocap points.", "good");
  }

  // The suggested standing window and part to keep (MC.suggestTrim) - the take prepared
  // again: the floor and the facing are found in the standing window.
  function applySuggestedTrim() {
    const s = MC.suggestTrim(mc.take);
    mc.ref = s.reference.slice();
    mc.start = s.start;
    mc.end = s.end;
    mc.contact = null;
  }

  function prepareTake() {
    if (!mc.capture) return;
    mc.take = MC.prepare(mc.capture, { fps: mc.fps, smoothing: mc.smoothing, reference: mc.ref, despike: mc.despike ? 1 : 0 });
    let suggested = "";
    if (mc.fresh) {  // a new capture: the standing window and the trim suggested
      mc.fresh = false;
      applySuggestedTrim();
      mc.take = MC.prepare(mc.capture, { fps: mc.fps, smoothing: mc.smoothing, reference: mc.ref, despike: mc.despike ? 1 : 0 });
      suggested = "Suggested: standing " + mc.ref[0].toFixed(2) + "-" + mc.ref[1].toFixed(2) + " s, keep " + mc.start.toFixed(2) + "-" +
        (mc.end > 0 ? mc.end.toFixed(2) + " s" : "the end") + " (the graph below; change them with the sliders).";
    }
    mc.profile = MC.motionProfile(mc.take);
    const d = mc.take.duration;
    for (const id of ["mc-ref0", "mc-ref1", "mc-start", "mc-end", "mc-contact"]) $(id).max = d.toFixed(2);
    if (mc.contact === null) mc.contact = MC.suggestContact(mc.take, mc.start, mc.end);
    const unit = mc.take.unit === 0.001 ? "millimetres" : mc.take.unit === 0.01 ? "centimetres" : "metres";
    // What was read, then what was done to it - a line each.
    const info = $("mc-info");
    info.textContent = mc.fileName + ": " + mc.capture.frames.length + " frames" + (mc.capture.format === "rtmpose" ? " (RTMPose)" : "") +
      ", " + d.toFixed(2) + " s at " + mc.fps + " a second" + (mc.fpsFromFile && mc.fpsSource ? " (" + mc.fpsSource + ")" : "") + ", in " + unit +
      "; standing hips " + mc.take.hipHeight.toFixed(2) + " m.";
    const lines = mc.take.notes.concat(mc.timingNote ? [mc.timingNote.trim()] : [], suggested ? [suggested] : []);
    if (lines.length) {
      const list = document.createElement("ul");
      for (const text of lines) {
        const item = document.createElement("li");
        item.textContent = text.charAt(0).toUpperCase() + text.slice(1);
        list.appendChild(item);
      }
      info.appendChild(list);
    }
    syncMocapInputs();
    recompute();
  }

  function recomputeMocap() {
    const lib = libs.get(state.body);
    let result = null;
    if (lib && mc.take) result = MC.retarget(mc.take, lib.bones, mocapOptions(state.body));
    else if (lib && mc.saved) result = A.keyedClip(mc.saved, state.body, lib.bones);
    mc.result = mc.take ? result : null;
    state.clips.base = state.clips.mix = null;
    state.layers = { base: null, mix: null, result: null };
    state.clips.result = nodeMap(result);
    const end = mc.take ? mc.take.duration : 5;
    $("mc-smooth-v").textContent = mc.smoothing.toFixed(2);
    $("mc-ref0-v").textContent = mc.ref[0].toFixed(2) + " s";
    $("mc-ref1-v").textContent = mc.ref[1].toFixed(2) + " s";
    $("mc-start-v").textContent = mc.start.toFixed(2) + " s";
    $("mc-end-v").textContent = mc.end > 0 && mc.end < end ? mc.end.toFixed(2) + " s" : "end";
    $("mc-in-v").textContent = mc.blendIn.toFixed(2) + " s";
    $("mc-out-v").textContent = mc.blendOut.toFixed(2) + " s";
    $("mc-contact-v").textContent = mc.contact !== null ? mc.contact.toFixed(2) + " s" : "-";
    $("mc-tol-v").textContent = mc.tolerance.toFixed(2) + "°";
    let keys = 0;
    if (result) for (const n of result.nodes) keys += (n.orientation ? n.orientation.times.length : 0) + (n.position ? n.position.times.length : 0);
    $("lab-result").textContent = result
      ? mcName() + " on " + state.body + " · " + result.length.toFixed(2) + " s · " + keys + " keys" + (mc.saved ? " (saved keys)" : "")
      : mc.saved ? mcName() + ": no keys for " + state.body : "drop a capture";
  }

  async function saveMocap() {
    const name = mcName();
    if (!name) { setMcStatus("Give it a name first.", "bad"); return; }
    if (!mc.take) { setMcStatus(mc.saved ? "Drop the capture again to change and save it." : "Drop a capture first.", "bad"); return; }
    const keys = {};
    let length = 0, count = 0;
    const missing = [];
    for (const body of BODY_ORDER) {
      const lib = libs.get(body);
      if (!lib) { missing.push(body); continue; }
      const clip = MC.retarget(mc.take, lib.bones, mocapOptions(body));
      length = clip.length;
      keys[body] = [];
      for (const n of clip.nodes) {
        if (n.position) keys[body].push({ bone: n.name, position: true, times: n.position.times, values: n.position.values });
        if (n.orientation) keys[body].push({ bone: n.name, position: false, times: n.orientation.times, values: n.orientation.values });
        count += (n.position ? n.position.times.length : 0) + (n.orientation ? n.orientation.times.length : 0);
      }
    }
    if (!Object.keys(keys).length) { setMcStatus("No body libraries to retarget to: reload the game files.", "bad"); return; }
    const recipe = { name, keyed: true, length, transtime: 0.25, keys,
      source: mc.fileName + " " + mc.start.toFixed(2) + "-" + (mc.end > 0 ? mc.end.toFixed(2) : mc.take.duration.toFixed(2)) + " s of " +
        mc.take.duration.toFixed(2) + ", " + mc.fps + " fps, smoothing " + mc.smoothing + (mc.loop ? ", looped" : "") +
        (mc.fitLength > 0 ? ", fitted to " + mc.fitLength + " s" : "") };
    const text = A.writeRecipe(recipe);
    try {
      const done = await getJson("/api/recipe", { method: "POST", headers: { "Content-Type": "text/plain; charset=utf-8" }, body: text });
      applyServerState(done.state);
      const back = A.readRecipe(await (await fetch("/api/recipe?source=yours&file=" + encodeURIComponent(done.file))).text());
      const bodies = Object.keys(back.keys || {});
      if (!back.keyed || bodies.length !== Object.keys(keys).length) {
        setMcStatus("Saved " + done.path + ", but it doesn't read back whole: the bench's server is out of date. Close its window, start recipe_bench.bat again, then Save again.", "bad");
        return;
      }
      setMcStatus((done.replaced ? "Saved over " : "Saved ") + done.path + " - " + bodies.length + " bodies, " + count + " keys, " +
        Math.round(text.length / 1024) + " KB. Start the game to build it: " + name + " " + (SLOTS[name] ? SLOTS[name].note : "plays with !anim " + name + ".") +
        (missing.length ? " No keys for " + missing.join(", ") + " (not in the game files read)." : ""), "good");
    } catch (e) {
      setMcStatus("Not saved: " + e.message, "bad");
    }
  }

  function openKeysRecipe(r) {
    mc.saved = r;
    mc.capture = mc.take = null;
    mc.slot = SLOTS[r.name] ? r.name : "";
    mc.name = r.name;
    $("mc-info").textContent = r.name + ": keys for " + Object.keys(r.keys).join(", ") + (r.source ? " - made from " + r.source : "") + ". Drop its capture again to change it.";
    syncMocapInputs();
    $("mc-name-l").hidden = !!mc.slot;
    setMode("mocap");
  }

  function bindMocap() {
    $("tab-recipe").addEventListener("click", () => setMode("recipe"));
    $("tab-mocap").addEventListener("click", () => setMode("mocap"));
    $("mc-files").addEventListener("change", () => loadMocapFiles([...$("mc-files").files]));
    $("mc-folder-b").addEventListener("click", () => $("mc-folder").click());
    $("mc-folder").addEventListener("change", () => {
      const files = [...$("mc-folder").files].filter((f) => /\.(csv|json)$/i.test(f.name));
      const first = $("mc-folder").files[0];
      loadMocapFiles(files, first && first.webkitRelativePath ? first.webkitRelativePath.split("/")[0] : "");
    });
    const drop = $("drop");
    for (const type of ["dragenter", "dragover"]) drop.addEventListener(type, (e) => { e.preventDefault(); drop.classList.add("over"); });
    for (const type of ["dragleave", "drop"]) drop.addEventListener(type, () => drop.classList.remove("over"));
    drop.addEventListener("drop", (e) => {
      e.preventDefault();
      const items = [...(e.dataTransfer.items || [])].map((i) => (i.webkitGetAsEntry ? i.webkitGetAsEntry() : null)).filter(Boolean);
      const folder = items.find((i) => i.isDirectory);
      filesFromDrop(e.dataTransfer).then((files) => loadMocapFiles(files, folder ? folder.name : ""));
    });
    $("mc-glitch").addEventListener("change", () => { mc.despike = $("mc-glitch").checked; prepareTake(); });
    $("mc-trim").addEventListener("click", () => { if (mc.take) { applySuggestedTrim(); prepareTake(); } });
    $("mc-export").addEventListener("click", exportPoints);
    // A file dropped beside the zone mustn't open in the tab.
    for (const type of ["dragover", "drop"]) window.addEventListener(type, (e) => { if (!drop.contains(e.target)) e.preventDefault(); });
    $("mc-slot").addEventListener("change", () => applySlot($("mc-slot").value));
    $("mc-name").addEventListener("input", () => { mc.name = sanitizeName($("mc-name").value); recompute(); });
    $("mc-name").addEventListener("change", () => { $("mc-name").value = mc.name; });
    $("mc-fps").addEventListener("change", () => { const v = parseFloat($("mc-fps").value); if (v > 0) { mc.fps = v; mc.fpsFromFile = false; prepareTake(); } });
    $("mc-smooth").addEventListener("change", () => { mc.smoothing = parseFloat($("mc-smooth").value); prepareTake(); });
    $("mc-smooth").addEventListener("input", () => { $("mc-smooth-v").textContent = parseFloat($("mc-smooth").value).toFixed(2); });
    $("mc-ref0").addEventListener("change", () => { mc.ref[0] = Math.min(parseFloat($("mc-ref0").value), mc.ref[1] - 0.05); prepareTake(); });
    $("mc-ref1").addEventListener("change", () => { mc.ref[1] = Math.max(parseFloat($("mc-ref1").value), mc.ref[0] + 0.05); prepareTake(); });
    $("mc-start").addEventListener("input", () => {
      const end = mc.end > 0 ? mc.end : parseFloat($("mc-end").max);
      mc.start = Math.max(0, Math.min(parseFloat($("mc-start").value), end - 0.1));
      recompute();
    });
    $("mc-end").addEventListener("input", () => {
      const max = parseFloat($("mc-end").max);
      const v = Math.max(parseFloat($("mc-end").value), mc.start + 0.1);
      mc.end = v >= max - 0.005 ? 0 : v;
      recompute();
    });
    $("mc-in").addEventListener("input", () => { mc.blendIn = parseFloat($("mc-in").value); recompute(); });
    $("mc-out").addEventListener("input", () => { mc.blendOut = parseFloat($("mc-out").value); recompute(); });
    $("mc-loop").addEventListener("change", () => { mc.loop = $("mc-loop").checked; recompute(); });
    $("mc-sway").addEventListener("change", () => { mc.sway = $("mc-sway").checked; recompute(); });
    $("mc-fit").addEventListener("change", () => { mc.fitLength = Math.max(0, parseFloat($("mc-fit").value) || 0); recompute(); });
    $("mc-at").addEventListener("change", () => { mc.contactAt = Math.max(0, parseFloat($("mc-at").value) || 0); recompute(); });
    $("mc-contact").addEventListener("input", () => { mc.contact = parseFloat($("mc-contact").value); recompute(); });
    $("mc-suggest").addEventListener("click", () => {
      if (!mc.take) return;
      mc.contact = MC.suggestContact(mc.take, mc.start, mc.end);
      syncMocapInputs();
      recompute();
    });
    $("mc-tol").addEventListener("change", () => { mc.tolerance = parseFloat($("mc-tol").value); recompute(); });
    $("mc-tol").addEventListener("input", () => { $("mc-tol-v").textContent = parseFloat($("mc-tol").value).toFixed(2) + "°"; });
    $("mc-save").addEventListener("click", saveMocap);
    syncMocapInputs();
  }

  // The capture's points over the figure at time t: scaled to the body, the hips put
  // over the figure's root (the take's travel left out).
  function capturePoints(result, t, points) {
    if (!mc.take || !rig || !result.takeTime) return null;
    const lib = rig.lib;
    if (!rig.mocapHip) rig.mocapHip = MC.rig(lib.bones).hipHeight;
    const s = mc.take.hipHeight > 0.1 ? rig.mocapHip / mc.take.hipHeight : 1;
    const pts = MC.pointsAt(mc.take, result.takeTime(t));
    const L = MC.LANDMARKS;
    const lh = pts[L.indexOf("left_hip")], rh = pts[L.indexOf("right_hip")];
    const hips = [(lh[0] + rh[0]) / 2, (lh[1] + rh[1]) / 2];
    const rootIndex = lib.bones.findIndex((b) => b.name.toLowerCase() === "rootdummy");
    const root = rootIndex >= 0 ? points[rootIndex] : [0, 0, 0];
    return pts.map((p) => [root[0] + (p[0] - hips[0]) * s, root[1] + (p[1] - hips[1]) * s, p[2] * s]);
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

  // Which bones a layer drives: those hanging from its animroot.
  function layerMask(layer) {
    if (layer.mask && layer.mask.rig === rig) return layer.mask.inside;
    const bones = rig.lib.bones;
    const top = bones.findIndex((b) => b.name.toLowerCase() === String(layer.animroot || "").toLowerCase());
    const inside = bones.map((b, i) => { for (let at = i; at >= 0; at = bones[at].parent) if (at === top) return true; return false; });
    layer.mask = { rig, inside };
    return inside;
  }

  // World positions of every bone: the rest pose, the clip's keys over it, a layer's over those.
  function pose(clip, t, x, layer) {
    const bones = rig.lib.bones;
    const world = new Array(bones.length);
    const inLayer = layer ? layerMask(layer) : null;
    for (let i = 0; i < bones.length; i++) {
      const b = bones[i];
      let p = b.position ? b.position.values[0] : [0, 0, 0];
      let q = b.orientation ? b.orientation.values[0] : [0, 0, 0, 1];
      const node = clip && clip.byName.get(b.name.toLowerCase());
      if (node && node.orientation) q = A.sample(node.orientation, t, true);
      if (node && node.position) p = A.sample(node.position, t, false);
      const over = inLayer && inLayer[i] ? layer.byName.get(b.name.toLowerCase()) : null;
      if (over && over.orientation && over.orientation.times.length) q = A.sample(over.orientation, t, true);
      if (over && over.position && over.position.times.length) p = A.sample(over.position, t, false);
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
    colors = { side: hex(get("--figure-side")), accent: hex(get("--accent")), stage: hex(get("--stage")), line: get("--line"), shadow: get("--shadow"),
               capture: get("--capture") || "#2563eb" };
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
    const mocap = state.mode === "mocap";
    const figures = [
      { clip: base, layer: state.layers.base, color: colors.side, x: SPOTS[0], show: !mocap },
      { clip: result, layer: state.layers.result, color: colors.accent, x: SPOTS[1], show: !mocap || !!result, capture: mocap },
      { clip: mix, layer: state.layers.mix, color: colors.side, x: SPOTS[2], show: !mocap && !!mix },
    ];
    const items = [];
    for (const fig of figures) {
      if (!fig.show) continue;
      const points = pose(fig.clip, at(fig.clip), fig.x, fig.layer);
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
      const cap = fig.capture && fig.clip ? capturePoints(fig.clip, at(fig.clip), points) : null;
      if (cap) {  // the capture's points and lines, a little in front of the bones they match
        const capCam = cap.map(toCamera);
        if (!capCam.some((c) => c[2] < NEAR)) {
          const capScreen = capCam.map(toScreen);
          for (const [a, b] of MC.CONNECTIONS) {
            items.push({ z: (capCam[a][2] + capCam[b][2]) / 2 - 0.05, kind: 0, a: capScreen[a], b: capScreen[b], w: 1.5, color: colors.capture });
          }
          for (let k = 0; k < capScreen.length; k++) {
            items.push({ z: capCam[k][2] - 0.06, kind: 1, a: capScreen[k], w: (F * 0.012) / capCam[k][2], color: colors.capture });
          }
        }
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

  // The motion graph: the hands' and feet's speed over the take, the standing window and
  // the part kept shaded, the contact and the playing time marked.
  function drawMotion() {
    const c = $("mc-motion");
    if (!c || !mc.take || !mc.profile || state.mode !== "mocap") return;
    const dpr = window.devicePixelRatio || 1;
    const w = Math.max(10, Math.round(c.clientWidth * dpr)), h = Math.max(10, Math.round(c.clientHeight * dpr));
    if (c.width !== w || c.height !== h) { c.width = w; c.height = h; }
    const g = c.getContext("2d");
    g.clearRect(0, 0, w, h);
    const d = Math.max(mc.take.duration, 1e-6);
    const x = (t) => (t / d) * w;
    const css = getComputedStyle(document.documentElement);
    const v = (n) => css.getPropertyValue(n).trim();
    const end = mc.end > 0 ? mc.end : d;
    g.globalAlpha = 0.25;
    g.fillStyle = v("--accent");
    g.fillRect(x(mc.start), 0, x(end) - x(mc.start), h);
    g.globalAlpha = 0.35;
    g.fillStyle = v("--ok");
    g.fillRect(x(mc.ref[0]), 0, Math.max(1, x(mc.ref[1]) - x(mc.ref[0])), h);
    g.globalAlpha = 1;
    const top = Math.max(2, ...mc.profile.hands, ...mc.profile.feet);
    const scale = Math.min(top, 6);  // m/s: a glitch-sized spike doesn't flatten the rest
    const line = (values, color, width) => {
      g.strokeStyle = color;
      g.lineWidth = width * dpr;
      g.beginPath();
      values.forEach((s, f) => { const px = x(f / mc.take.fps), py = h - 3 * dpr - (Math.min(s, scale) / scale) * (h - 6 * dpr); if (f) g.lineTo(px, py); else g.moveTo(px, py); });
      g.stroke();
    };
    line(mc.profile.feet, v("--muted"), 1);
    line(mc.profile.hands, v("--capture"), 1.5);
    if (mc.contact !== null && mc.fitLength > 0) {
      g.fillStyle = v("--accent");
      g.fillRect(x(mc.contact) - dpr, 0, 2 * dpr, h);
    }
    if (mc.result && mc.result.takeTime && mc.result.length > 0) {
      g.fillStyle = v("--ink");
      g.fillRect(x(mc.result.takeTime(state.time % mc.result.length)) - dpr / 2, 0, Math.max(1, dpr), h);
    }
  }

  let last = performance.now();
  function frame(now) {
    const dt = Math.min(0.1, (now - last) / 1000);
    last = now;
    if (state.playing) state.time += dt;
    draw();
    drawMotion();
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
