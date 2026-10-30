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
    if (typeof refreshMixLists === "function") refreshMixLists();
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
    refreshStances(names);
    mixClips.clear();
    refreshMixLists();
    recompute();
  }

  // The neutral stance picker (#80): the stances first (NWN's names: nw unarmed, 1h one-handed,
  // 2h two-handed, 2w two weapons, pl polearm; r / l the side forward), then every animation.
  const STANCES = [
    ["pause1", "standing"], ["pause2", "standing, second idle"], ["pausetrd", "standing, tired"],
    ["nwreadyr", "unarmed ready (r)"], ["nwreadyl", "unarmed ready (l)"],
    ["1hreadyr", "one-handed ready (r)"], ["1hreadyl", "one-handed ready (l)"],
    ["2hreadyr", "two-handed ready (r)"], ["2hreadyl", "two-handed ready (l)"],
    ["2wreadyr", "two weapons ready (r)"], ["2wreadyl", "two weapons ready (l)"],
    ["plreadyr", "polearm ready (r)"], ["plreadyl", "polearm ready (l)"],
    ["bowrdy", "bow ready"], ["xbowrdy", "crossbow ready"],
    ["getlowlp", "crouched low"], ["getmidlp", "crouched"], ["kneel", "kneeling"], ["meditate", "meditating"], ["sitcross", "sitting cross-legged"],
  ];
  function refreshStances(names) {
    const select = $("mc-stance");
    select.textContent = "";
    const known = new Set();
    const group = (label) => { const g = document.createElement("optgroup"); g.label = label; select.appendChild(g); return g; };
    const stances = group("Stances");
    for (const [name, what] of STANCES) if (names.includes(name)) { stances.appendChild(new Option(name + " - " + what, name)); known.add(name); }
    const all = group("Every animation (its first frame)");
    for (const n of names) if (!known.has(n)) all.appendChild(new Option(n, n));
    if (names.length && !names.includes(mc.stance)) mc.stance = names.includes("pause1") ? "pause1" : names[0];
    select.value = mc.stance;
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
    $("step-back").addEventListener("click", () => stepFrame(-1));
    $("step-on").addEventListener("click", () => stepFrame(1));
    document.addEventListener("keydown", (e) => {
      // Not while typing or on a slider (its arrows are its own).
      const t = e.target, typing = t && (t.tagName === "TEXTAREA" || t.tagName === "SELECT" || (t.tagName === "INPUT" && t.type !== "checkbox" && t.type !== "button"));
      if (e.key === " " && e.target === document.body) { e.preventDefault(); setPlaying(!state.playing); }
      else if (!typing && !e.altKey && !e.ctrlKey && !e.metaKey && (e.key === "ArrowLeft" || e.key === "ArrowRight")) {
        e.preventDefault();
        stepFrame(e.key === "ArrowLeft" ? -1 : 1);
      } else if (!typing && state.mode === "mocap" && (mc.poseSel || mc.selected) && (e.key === "Delete" || e.key === "Backspace")) {
        e.preventDefault();
        if (mc.poseSel) deletePoseKey(); else removeSelectedPlant();
      }
    });
    bindMocap();
  }
  function setPlaying(on) {
    state.playing = on;
    $("play").textContent = on ? "Pause" : "Play";
    $("play").setAttribute("aria-pressed", String(on));
  }
  // A frame back or on (a thirtieth of a second: in Mocap, a frame of the take), paused there.
  function stepFrame(dir) {
    const result = state.clips.result;
    if (!result || !(result.length > 0)) return;
    setPlaying(false);
    const last = Math.max(0, Math.ceil(result.length * 30 - 1e-6) - 1);
    state.time = Math.max(0, Math.min(last, Math.round((state.time % result.length) * 30) + dir)) / 30;
  }


  // --- Mocap mode (mocap.js; docs/PLAN.md "Mocap mode") ------------------------------------
  // A FreeMoCap capture retargeted to the body shown, played with its points over it; Save
  // retargets it to every body and writes a keys recipe (anim_recipe.cpp builds it per library).
  const MC = window.Mocap;
  const mc = {
    capture: null, take: null, fileName: "", fps: 30, fpsFromFile: false, fpsSource: "", despike: true, profile: null, result: null,
    slot: "n5_rage", name: "n5_mocap", smoothing: 0, ref: [0, 0.33], start: 0, end: 0,
    plants: { left: [], right: [] }, locks: true, place: "return",  // planted feet (s in the take) and where the body stands (#79)
    selected: null,  // the plant selected on the timeline: { side, bar }
    stance: "pause1",  // the neutral stance (#80): the standing part becomes it; the blends start and end in it
    stanceView: false,  // the stage showing the stance from the front, 3/4 and the side (#80)
    poses: [], posePart: "rfoot_g", poseSel: null,  // pose keys (#83): parts, the part posed, a key selected on the timeline
    solveCache: {},  // the retarget's solve kept per body (MC.retarget reuses it while the take, stance and span are the same)
    // Legs and torso from different sources (#84): "take", "anim:<name>" or "recipe:<source>|<file>".
    halves: { legs: { src: "take", offset: 0, speed: 1, loop: true }, torso: { src: "take", offset: 0, speed: 1, loop: true }, seam: "direction", balance: true },
    recipes: {},  // saved recipes read for a half, by "<source>|<file>"
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
    $("mc-timeline").hidden = mode !== "mocap";
    document.querySelector(".stage-wrap").classList.toggle("mocap", mode === "mocap");
    stageLabels();
    recompute();
  }
  // The stage's labels: Base / Recipe / Mix, Mocap - or the stance's three views (#80).
  function stageLabels() {
    const mocap = state.mode === "mocap", stance = mocap && mc.stanceView;
    const spans = $("labels").querySelectorAll("span");
    spans[0].hidden = spans[2].hidden = mocap && !stance;
    spans[0].firstChild.textContent = stance ? "Front" : "Base";
    spans[1].firstChild.textContent = stance ? "3/4" : mocap ? "Mocap" : "Recipe";
    spans[2].firstChild.textContent = stance ? "Side" : "Mix";
    $("labels").classList.toggle("one", mocap && !stance);
    if (stance) for (const id of ["lab-base", "lab-result", "lab-mix"]) $(id).textContent = mc.stance + " on " + state.body;
    $("mc-stance-show").setAttribute("aria-pressed", String(stance));
    $("mc-stance-show").textContent = stance ? "Back to the take" : "Show the stance (front, 3/4, side)";
  }

  function applySlot(slot) {
    mc.slot = slot;
    $("mc-name-l").hidden = !!slot;
    const preset = SLOTS[slot];
    if (preset) {
      mc.blendIn = preset.blendIn;
      mc.blendOut = preset.blendOut;
      mc.loop = preset.loop;
      mc.place = preset.loop ? "inplace" : "return";  // a loop stays on its spot; a one-off comes back to it
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
    $("mc-sway").disabled = mc.place === "return";  // returning, the body follows the hips
    $("mc-place").value = mc.place;
    $("mc-locks").checked = mc.locks;
    $("mc-stance").value = mc.stance;
    $("mc-fit").value = mc.fitLength;
    $("mc-at").value = mc.contactAt;
    $("mc-contact").value = mc.contact || 0;
    $("mc-tol").value = mc.tolerance;
    syncMixUi();
  }

  function mocapOptions(body) {
    const fit = mc.fitLength > 0;
    return { name: mcName(), start: mc.start, end: mc.end, sway: mc.sway, stance: findClip(body, mc.stance) || findClip(body, "pause1"),
      place: mc.place, plants: mc.locks ? mc.plants : null, poses: mc.poses, halves: mixOptions(body),
      cache: (mc.solveCache[body] = mc.solveCache[body] || {}),
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

  // The take's points, for Blender (#77): downloaded as <take>_points.csv - from the standing
  // part to the kept part's end, both marked (MC.pointsCsv).
  function exportPoints() {
    if (!mc.take) { setMcStatus("Drop a capture first.", "bad"); return; }
    const base = (mc.fileName.split(":")[0] || "take").replace(/\.csv.*$/i, "").replace(/[^A-Za-z0-9_.-]+/g, "_").replace(/_points$/, "");
    const marks = { reference: mc.ref, start: mc.start, end: mc.end, plants: mc.plants };
    const span = MC.pointsSpan(mc.take, marks);
    const blob = new Blob([MC.pointsCsv(mc.take, marks)], { type: "text/csv" });
    const link = document.createElement("a");
    link.href = URL.createObjectURL(blob);
    link.download = base + "_points.csv";
    document.body.appendChild(link);
    link.click();
    link.remove();
    setTimeout(() => URL.revokeObjectURL(link.href), 5000);
    setMcStatus("Exported " + link.download + " to your downloads: " + (span.from / mc.take.fps).toFixed(2) + "-" + (span.to / mc.take.fps).toFixed(2) +
      " s of the take, from the standing part to the end of the kept part (" + (span.to - span.from + 1) + " frames, " + mc.take.fps +
      " a second). In Blender, File > Import > nwn5e mocap points: it plays the kept part.", "good");
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
    mc.solveCache = {};
    let suggested = "";
    if (mc.fresh) {  // a new capture: the standing window and the trim - a points file's marks, else suggested
      mc.fresh = false;
      const marked = MC.trimFromMarks(mc.capture, mc.fps);
      if (!marked.reference || marked.start === undefined) applySuggestedTrim();
      if (marked.reference) mc.ref = marked.reference;
      if (marked.start !== undefined) { mc.start = marked.start; mc.end = marked.end; mc.contact = null; }
      mc.take = MC.prepare(mc.capture, { fps: mc.fps, smoothing: mc.smoothing, reference: mc.ref, despike: mc.despike ? 1 : 0 });
      mc.plants = marked.plants || MC.suggestPlants(mc.take);
      mc.selected = null;
      const from = marked.reference && marked.start !== undefined ? "From the points file's marks" : marked.reference || marked.start !== undefined
        ? "From the points file's marks and suggested" : "Suggested";
      suggested = from + ": standing " + mc.ref[0].toFixed(2) + "-" + mc.ref[1].toFixed(2) + " s, keep " + mc.start.toFixed(2) + "-" +
        (mc.end > 0 ? mc.end.toFixed(2) + " s" : "the end") + " (the graph below; change them with the sliders); planted feet " +
        (marked.plants ? "from its marks" : "suggested") + ": " + mc.plants.left.length + " left, " + mc.plants.right.length + " right (the rows under the graph).";
      // This take's edits from last time (#83), kept in this browser.
      mc.poses = [];
      mc.poseSel = null;
      const kept = loadSession();
      if (kept) {
        for (const k of ["ref", "start", "end", "plants", "stance", "place", "locks", "poses", "halves"]) if (kept[k] !== undefined) mc[k] = kept[k];
        for (const h of [mc.halves.legs, mc.halves.torso]) {
          if (h.src.startsWith("recipe:")) loadHalfRecipe(h.src.slice(7)).then(() => { mixClips.clear(); recompute(); }).catch(() => { h.src = "take"; syncMixUi(); recompute(); });
        }
        mc.contact = null;
        mc.take = MC.prepare(mc.capture, { fps: mc.fps, smoothing: mc.smoothing, reference: mc.ref, despike: mc.despike ? 1 : 0 });
        suggested = "This take's edits restored (saved " + new Date(kept.saved).toLocaleString() + "): standing " + mc.ref[0].toFixed(2) + "-" +
          mc.ref[1].toFixed(2) + " s, keep " + mc.start.toFixed(2) + "-" + (mc.end > 0 ? mc.end.toFixed(2) + " s" : "the end") + ", " +
          (mc.plants.left.length + mc.plants.right.length) + " planted feet, " + mc.poses.reduce((n, p) => n + p.keys.length, 0) + " pose keys, from " + mc.stance +
          (mixDescription() ? "; " + mixDescription() : "") + ".";
      }
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
    let feet = mc.take ? "Planted feet: " + mc.plants.left.length + " left, " + mc.plants.right.length + " right - " +
      (mc.locks ? "locked on every body." : "not locked (tick Lock planted feet).") : "";
    if (mc.take && mc.halves.legs.src !== "take") {
      feet = "The legs are " + mixLabel(mc.halves.legs.src) + "'s: " + (mc.locks && result && result.legPlants
        ? "its planted feet (where a foot stands still) - " + result.legPlants.left.length + " left, " + result.legPlants.right.length + " right - locked; the rows show them (not editable)."
        : "its feet as it moves them (tick Lock planted feet to hold where they stand still).");
    }
    if (result && result.locks && mc.locks) {
      const out = result.locks.filter((l) => l.short > 0.03).map((l) => l.side + " " + l.takeStart.toFixed(2) + " s (" + Math.round(l.short * 100) + " cm)");
      if (out.length) feet += " Out of the leg's reach on " + state.body + " (the foot slides that much): " + out.join(", ") + ".";
    }
    $("mc-feet").textContent = feet;
    if (mc.stanceView) stageLabels();
    saveSession();
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
        (mc.place === "inplace" ? ", in place" : ", returns to its spot") + ", from " + mc.stance +
        (mc.locks ? ", " + (mc.plants.left.length + mc.plants.right.length) + " planted feet locked" : "") +
        (mc.poses.length ? ", " + mc.poses.reduce((n, p) => n + p.keys.length, 0) + " pose keys" : "") +
        (mixDescription() ? ", " + mixDescription() : "") +
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
    $("mc-place").addEventListener("change", () => { mc.place = $("mc-place").value; syncMocapInputs(); recompute(); });
    $("mc-stance").addEventListener("change", () => { mc.stance = $("mc-stance").value; stageLabels(); recompute(); });
    $("mc-stance-show").addEventListener("click", () => {
      mc.stanceView = !mc.stanceView;
      if (mc.stanceView) Object.assign(view, { yaw: 0, pitch: 0.12, distance: view.home });  // from the front: the three side by side
      stageLabels();
      recompute();
    });
    $("mc-locks").addEventListener("change", () => { mc.locks = $("mc-locks").checked; recompute(); });
    $("mc-feet-suggest").addEventListener("click", () => { if (mc.take) { mc.plants = MC.suggestPlants(mc.take); mc.selected = null; recompute(); } });
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
    // The feet: NWN's foot bone ends at the ankle (no toe bone), so nothing drew which way a foot
    // points - a heel and a toe drawn from it, turned with it since rest (the rest pose's forward +Y,
    // the sole on the floor).
    const rest = new Array(lib.bones.length);
    lib.bones.forEach((b, i) => {
      const p = b.position ? b.position.values[0] : [0, 0, 0], q = b.orientation ? b.orientation.values[0] : [0, 0, 0, 1];
      if (b.parent < 0) rest[i] = { p: p.slice(0, 3), q };
      else { const up = rest[b.parent], r = rotate(up.q, p); rest[i] = { p: [up.p[0] + r[0], up.p[1] + r[1], up.p[2] + r[2]], q: qmul(up.q, q) }; }
    });
    const feet = [];
    let more = lib.bones.length;
    for (const n of ["lfoot_g", "rfoot_g"]) {
      if (!index.has(n)) continue;
      const i = index.get(n), foot = { i, restQ: rest[i].q, sole: rest[i].p[2], heel: more++, toe: more++ };
      feet.push(foot);
      segments.push({ a: i, b: foot.heel, r: 0.03 }, { a: foot.heel, b: foot.toe, r: 0.03 }, { a: i, b: foot.toe, r: 0.03 });
    }
    rig = {
      lib,
      feet,
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
    const points = world.map((w) => w.p);
    for (const f of rig.feet || []) {  // the heel and the toe, turned with the foot
      const w = world[f.i], r = f.restQ, since = qmul(w.q, [-r[0], -r[1], -r[2], r[3]]);
      const at = (v) => { const o = rotate(since, v); return [w.p[0] + o[0], w.p[1] + o[1], w.p[2] + o[2]]; };
      points[f.heel] = at([0, -0.06, -f.sole]);
      points[f.toe] = at([0, 0.14, -f.sole]);
    }
    return points;
  }

  let stanceCache = null;
  function stanceClip() {
    const key = state.body + "|" + mc.stance;
    if (!stanceCache || stanceCache.key !== key) stanceCache = { key, clip: nodeMap(findClip(state.body, mc.stance)) };
    return stanceCache.clip;
  }

  let colors = {};
  function readColors() {
    const css = getComputedStyle(document.documentElement);
    const get = (n) => css.getPropertyValue(n).trim();
    colors = { side: hex(get("--figure-side")), accent: hex(get("--accent")), stage: hex(get("--stage")), line: get("--line"), shadow: get("--shadow"),
               capture: get("--capture") || "#2563eb", ok: get("--ok") || "#1f7a3a" };
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
    // The neutral stance from the front, 3/4 and the side (#80): its first frame, turned on its spot.
    const stance = mocap && mc.stanceView ? stanceClip() : null;
    const figures = stance
      ? [0, Math.PI / 4, Math.PI / 2].map((turn, k) => ({ clip: stance, layer: null, color: k === 1 ? colors.accent : colors.side, x: SPOTS[k], turn, show: true }))
      : [
        { clip: base, layer: state.layers.base, color: colors.side, x: SPOTS[0], show: !mocap },
        { clip: result, layer: state.layers.result, color: colors.accent, x: SPOTS[1], show: !mocap || !!result, capture: mocap },
        { clip: mix, layer: state.layers.mix, color: colors.side, x: SPOTS[2], show: !mocap && !!mix },
      ];
    const items = [];
    for (const fig of figures) {
      if (!fig.show) continue;
      const points = fig.turn === undefined ? pose(fig.clip, at(fig.clip), fig.x, fig.layer)
        : pose(fig.clip, 0, 0, null).map((p) => [p[0] * Math.cos(fig.turn) - p[1] * Math.sin(fig.turn) + fig.x, p[0] * Math.sin(fig.turn) + p[1] * Math.cos(fig.turn), p[2]]);
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
      // A ring on the floor under a locked foot (#79).
      if (fig.capture && mc.locks && mc.result && mc.result.locks) {
        const now = at(fig.clip);
        for (const l of mc.result.locks) {
          if (now < l.start || now > l.end) continue;
          const i = rig.lib.bones.findIndex((b) => b.name.toLowerCase() === (l.side === "left" ? "lfoot_g" : "rfoot_g"));
          if (i < 0) continue;
          const ring = [];
          for (let s = 0; s <= 24; s++) {
            const a = (s / 24) * Math.PI * 2;
            ring.push(toCamera([points[i][0] + 0.12 * Math.cos(a), points[i][1] + 0.12 * Math.sin(a), 0]));
          }
          if (ring.some((c) => c[2] < NEAR)) continue;
          ctx.beginPath();
          ring.map(toScreen).forEach((p, s) => (s ? ctx.lineTo(p[0], p[1]) : ctx.moveTo(p[0], p[1])));
          ctx.strokeStyle = colors.ok;
          ctx.lineWidth = 2;
          ctx.stroke();
        }
      }
      const depthShade = (z) => Math.max(0, Math.min(0.35, (z - view.distance + 0.6) * 0.18));
      const posed = fig.capture && mc.take ? rig.lib.bones.findIndex((b) => b.name.toLowerCase() === (mc.posePart === "rootdummy" ? "pelvis_g" : mc.posePart)) : -1;
      for (const s of rig.segments) {
        const z = (cam[s.a][2] + cam[s.b][2]) / 2;
        const color = s.a === posed ? colors.capture : shade(fig.color, depthShade(z), colors.stage);
        items.push({ z, kind: 0, a: screen[s.a], b: screen[s.b], w: (2 * F * s.r) / z, color });
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

  // The timeline under the preview (#79): the hands' and feet's speed over the take, the
  // standing window and the part kept shaded, the contact and the marker (the playing time);
  // under it a row a foot, its planted stretches - red where the lock is out of the leg's
  // reach on this body; the selected one outlined, a handle at each end.
  const ROW_H = 15, ROW_GAP = 2;
  const rowsHeight = () => (2 + poseRows().length) * (ROW_H + ROW_GAP);  // CSS px under the curves: the feet's rows, a posed part's each
  const rowTop = (r) => ROW_GAP + r * (ROW_H + ROW_GAP);  // a row's top, from the curves' bottom
  let footDrag = null;  // a press on the timeline: { mode: seek | new | edge | select, side, bar, end, t0, t1, snapped }
  const markerTime = () => (mc.result && mc.result.takeTime && mc.result.length > 0 ? mc.result.takeTime(state.time % mc.result.length) : null);
  function drawMotion() {
    const c = $("mc-motion");
    if (!c || !mc.take || !mc.profile || state.mode !== "mocap") return;
    const tall = 106 + rowsHeight() + "px";
    if (c.style.height !== tall) {
      c.style.height = tall;
      const wrap = c.closest(".stage-wrap");  // the pinned stage gives up the pose rows' height (#83)
      if (wrap) wrap.style.setProperty("--pose-rows", poseRows().length * (ROW_H + ROW_GAP) + "px");
    }
    syncPoseUi(false);
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
    const ch = h - rowsHeight() * dpr;  // the curves' height
    g.globalAlpha = 0.25;
    g.fillStyle = v("--accent");
    g.fillRect(x(mc.start), 0, x(end) - x(mc.start), ch);
    g.globalAlpha = 0.35;
    g.fillStyle = v("--ok");
    g.fillRect(x(mc.ref[0]), 0, Math.max(1, x(mc.ref[1]) - x(mc.ref[0])), ch);
    g.globalAlpha = 1;
    const top = Math.max(2, ...mc.profile.hands, ...mc.profile.feet);
    const scale = Math.min(top, 6);  // m/s: a glitch-sized spike doesn't flatten the rest
    const line = (values, color, width) => {
      g.strokeStyle = color;
      g.lineWidth = width * dpr;
      g.beginPath();
      values.forEach((s, f) => { const px = x(f / mc.take.fps), py = ch - 3 * dpr - (Math.min(s, scale) / scale) * (ch - 6 * dpr); if (f) g.lineTo(px, py); else g.moveTo(px, py); });
      g.stroke();
    };
    ["left", "right"].forEach((side, r) => {
      const y = ch + rowTop(r) * dpr, rh = ROW_H * dpr;
      g.globalAlpha = 0.5;
      g.fillStyle = v("--line");
      g.fillRect(0, y, w, rh);
      const fromAnim = mc.halves.legs.src !== "take";  // the legs' animation's own plants (#84)
      const bars = fromAnim ? (mc.result && mc.result.legPlants ? mc.result.legPlants[side].slice() : []) : mc.plants[side].slice();
      if (!fromAnim && footDrag && footDrag.mode === "new" && footDrag.side === side) bars.push([Math.min(footDrag.t0, footDrag.t1), Math.max(footDrag.t0, footDrag.t1)]);
      for (const [a, b] of bars) {
        const out = mc.locks && mc.result && mc.result.locks &&
          mc.result.locks.some((l) => l.side === side && l.short > 0.03 && l.takeStart < b + 0.02 && l.takeEnd > a - 0.02);
        g.globalAlpha = mc.locks ? (fromAnim ? 0.45 : 0.8) : 0.3;
        g.fillStyle = out ? v("--danger") : v("--ink");
        g.fillRect(x(a), y, Math.max(2 * dpr, x(b) - x(a)), rh);
      }
      g.globalAlpha = 1;
      if (mc.selected && mc.selected.side === side) {
        const [a, b] = mc.selected.bar;
        g.strokeStyle = v("--accent");
        g.lineWidth = 2 * dpr;
        g.strokeRect(x(a), y + dpr, Math.max(2 * dpr, x(b) - x(a)), rh - 2 * dpr);
        g.fillStyle = v("--accent");
        g.fillRect(x(a) - 3 * dpr, y, 6 * dpr, rh);
        g.fillRect(x(b) - 3 * dpr, y, 6 * dpr, rh);
      }
      g.fillStyle = v("--muted");
      g.font = 10 * dpr + "px " + v("--mono");
      g.textBaseline = "middle";
      g.fillText(side === "left" ? "L" : "R", 3 * dpr, y + rh / 2);
    });
    // The posed parts' rows (#83): their keys (diamonds - set ones dark, adjust ones blue, the selected
    // one in the accent), the stretch they act over (with the fades) shaded.
    poseRows().forEach((part, n) => {
      const y = ch + rowTop(2 + n) * dpr, rh = ROW_H * dpr, keys = part.keys, fade = part.fade === undefined ? 0.15 : part.fade;
      g.globalAlpha = 0.5;
      g.fillStyle = v("--line");
      g.fillRect(0, y, w, rh);
      g.globalAlpha = 0.35;
      g.fillStyle = v("--capture");
      g.fillRect(x(keys[0].t - fade), y + rh * 0.35, Math.max(dpr, x(keys[keys.length - 1].t + fade) - x(keys[0].t - fade)), rh * 0.3);
      g.globalAlpha = 1;
      for (const k of keys) {
        const cx = x(k.t), cy = y + rh / 2, r = rh * 0.42;
        g.beginPath();
        g.moveTo(cx, cy - r); g.lineTo(cx + r, cy); g.lineTo(cx, cy + r); g.lineTo(cx - r, cy); g.closePath();
        g.fillStyle = mc.poseSel && mc.poseSel.key === k ? v("--accent") : part.mode === "set" ? v("--ink") : v("--capture");
        g.fill();
      }
      g.fillStyle = part.bone === mc.posePart ? v("--accent") : v("--muted");
      g.font = 10 * dpr + "px " + v("--mono");
      g.textBaseline = "middle";
      g.fillText(partName(part.bone, true), 3 * dpr, y + rh / 2);
    });
    line(mc.profile.feet, v("--muted"), 1);
    line(mc.profile.hands, v("--capture"), 1.5);
    // The torso's source's hands (#84), dashed, where they fall in the part kept: line a strike up.
    if (mc.result && mc.result.torsoHands && torsoMixed()) {
      const th = mc.result.torsoHands;
      g.setLineDash([4 * dpr, 3 * dpr]);
      g.strokeStyle = v("--accent");
      g.lineWidth = 1.5 * dpr;
      g.beginPath();
      th.values.forEach((s, k) => { const px = x(th.start + k / th.fps), py = ch - 3 * dpr - (Math.min(s, scale) / scale) * (ch - 6 * dpr); if (k) g.lineTo(px, py); else g.moveTo(px, py); });
      g.stroke();
      g.setLineDash([]);
    }
    const nowMix = mixNowText();
    if ($("mix-now").textContent !== nowMix) $("mix-now").textContent = nowMix;
    if (mc.contact !== null && mc.fitLength > 0) {
      g.fillStyle = v("--accent");
      g.fillRect(x(mc.contact) - dpr, 0, 2 * dpr, h);
    }
    const now = markerTime();
    if (now !== null) {
      const snapped = footDrag && footDrag.snapped;  // an edge on the marker: the marker shows it
      const mw = snapped ? 3 * dpr : Math.max(1, dpr);
      g.fillStyle = snapped ? v("--accent") : v("--ink");
      g.fillRect(x(now) - mw / 2, 0, mw, h);
    }
    // The readouts: the marker in the take, the selected plant.
    const nowText = now !== null ? "marker " + now.toFixed(2) + " s · frame " + Math.round(now * mc.take.fps) : "";
    if ($("mc-now").textContent !== nowText) $("mc-now").textContent = nowText;
    const selText = mc.selected ? mc.selected.side + " foot " + mc.selected.bar[0].toFixed(2) + "-" + mc.selected.bar[1].toFixed(2) + " s" : "no plant selected";
    if ($("mc-plant-sel").textContent !== selText) $("mc-plant-sel").textContent = selText;
    if ($("mc-plant-remove").disabled !== !mc.selected) $("mc-plant-remove").disabled = !mc.selected;
  }

  // --- Legs and torso from different sources (#84) -----------------------------------------------
  // Each half from this take, an animation of the body or a saved recipe (a take saved before):
  // MC.retarget's halves. The take's legs are the clock; the rest has an offset and a speed.
  const mixClips = new Map();  // "<body>|<src>" -> the clip
  function halfClip(body, src) {
    const key = body + "|" + src;
    if (mixClips.has(key)) return mixClips.get(key);
    const lib = libs.get(body);
    let clip = null;
    if (src.startsWith("anim:")) clip = findClip(body, src.slice(5));
    else if (src.startsWith("recipe:")) {
      const r = mc.recipes[src.slice(7)];
      if (!r) return null;  // still being read: not kept
      if (lib && r.keyed) clip = A.keyedClip(r, body, lib.bones);
      else if (lib) { const b = findClip(body, r.base); clip = b ? A.deriveClip(r, b, r.mix ? findClip(body, r.mix) : null, lib.bones) : null; }
    }
    mixClips.set(key, clip);
    return clip;
  }
  async function loadHalfRecipe(key) {
    if (mc.recipes[key]) return;
    const [source, file] = key.split("|");
    const res = await fetch("/api/recipe?source=" + encodeURIComponent(source) + "&file=" + encodeURIComponent(file));
    if (!res.ok) throw new Error((await res.json().catch(() => ({}))).error || res.status + "");
    mc.recipes[key] = A.readRecipe(await res.text());
  }
  function mixOptions(body) {
    const h = mc.halves;
    const half = (x, legs) => {
      if (x.src === "take") return legs ? { kind: "take" } : { kind: "take", offset: x.offset, speed: x.speed };
      const clip = halfClip(body, x.src);
      return clip ? { kind: "clip", clip, offset: x.offset, speed: x.speed, loop: x.loop } : { kind: "take" };
    };
    return { legs: half(h.legs, true), torso: half(h.torso, false), seam: h.seam, balance: h.balance };
  }
  const torsoMixed = () => mc.halves.torso.src !== "take" || mc.halves.torso.offset !== 0 || mc.halves.torso.speed !== 1;
  function mixLabel(src) {
    if (src === "take") return "this take";
    if (src.startsWith("anim:")) return src.slice(5);
    const [source, file] = src.slice(7).split("|");
    const r = state.saved.find((s) => s.source === source && s.file === file);
    return (r ? r.name : file) + (source === "nwn5e" ? " (nwn5e's)" : " (saved)");
  }
  function mixDescription() {
    const h = mc.halves;
    if (h.legs.src === "take" && !torsoMixed()) return "";
    const timing = (x) => (x.offset ? (x.offset > 0 ? " +" : " ") + x.offset.toFixed(2) + " s" : "") + (x.speed !== 1 ? " x" + x.speed.toFixed(2) : "") + (x.src !== "take" && !x.loop ? " (once)" : "");
    return "legs from " + mixLabel(h.legs.src) + (h.legs.src !== "take" ? timing(h.legs) : "") + ", torso from " + mixLabel(h.torso.src) + timing(h.torso) +
      " (the chest " + (h.seam === "hips" ? "turning with the hips" : "keeping its direction") + (h.balance ? ", the hips following the weight" : "") + ")";
  }
  // Where each half is in its source at the marker.
  function mixNowText() {
    if (!mc.result || !mc.result.source || (mc.halves.legs.src === "take" && !torsoMixed())) return "";
    const T = markerTime();
    if (T === null) return "";
    const t = T - mc.result.source.start;
    const at = (x) => {
      if (x.src === "take") return "this take " + MC.halfTime({ kind: "take", offset: x.offset, speed: x.speed }, t, mc.result.source.start).toFixed(2) + " s";
      const clip = halfClip(state.body, x.src);
      return clip ? mixLabel(x.src) + " " + MC.halfTime({ kind: "clip", clip, offset: x.offset, speed: x.speed, loop: x.loop }, t, 0).toFixed(2) + " s" : mixLabel(x.src) + " (reading it)";
    };
    return "At the marker - legs: " + (mc.halves.legs.src === "take" ? "this take " + T.toFixed(2) + " s" : at(mc.halves.legs)) + " · torso: " + at(mc.halves.torso);
  }
  function refreshMixLists() {
    for (const id of ["mix-legs", "mix-torso"]) {
      const select = $(id), half = mc.halves[id === "mix-legs" ? "legs" : "torso"];
      select.textContent = "";
      select.appendChild(new Option("this take", "take"));
      const group = (label) => { const g = document.createElement("optgroup"); g.label = label; select.appendChild(g); return g; };
      if (state.saved && state.saved.length) {
        const saved = group("Saved recipes (a take saved before, or a recipe)");
        for (const r of state.saved) saved.appendChild(new Option((r.source === "nwn5e" ? "nwn5e's " : "") + r.name, "recipe:" + r.source + "|" + r.file));
      }
      const anims = group("Animations of the body");
      for (const n of chainNames(state.body)) anims.appendChild(new Option(n, "anim:" + n));
      if (![...select.options].some((o) => o.value === half.src)) half.src = "take";
      select.value = half.src;
    }
    syncMixUi();
  }
  function syncMixUi() {
    const h = mc.halves;
    $("mix-legs").value = h.legs.src;
    $("mix-torso").value = h.torso.src;
    for (const [name, x] of [["legs", h.legs], ["torso", h.torso]]) {
      const fixed = name === "legs" && x.src === "take";  // the take's legs are the clock
      for (const k of ["offset", "speed"]) {
        const input = $("mix-" + name + "-" + k);
        if (document.activeElement !== input) input.value = x[k];
        input.disabled = fixed;
      }
      $("mix-" + name + "-loop").checked = x.loop;
      $("mix-" + name + "-loop").disabled = x.src === "take";
    }
    $("mix-seam").value = h.seam;
    $("mix-balance").checked = h.balance;
    $("mix-balance").disabled = !mc.locks;
    $("mix-info").textContent = mixDescription() ? "Mixed: " + mixDescription() + "." : "Not mixed: legs and torso from this take.";
  }
  {
    for (const name of ["legs", "torso"]) {
      $("mix-" + name).addEventListener("change", () => {
        const x = mc.halves[name], src = $("mix-" + name).value;
        x.src = src;
        syncMixUi();
        if (src.startsWith("recipe:")) {
          const key = src.slice(7);
          loadHalfRecipe(key).then(() => { mixClips.clear(); syncMixUi(); recompute(); })
            .catch((e) => { x.src = "take"; syncMixUi(); recompute(); setMcStatus("Couldn't read " + mixLabel(src) + ": " + e.message, "bad"); });
        }
        recompute();
      });
      $("mix-" + name + "-offset").addEventListener("change", () => { mc.halves[name].offset = parseFloat($("mix-" + name + "-offset").value) || 0; syncMixUi(); recompute(); });
      $("mix-" + name + "-speed").addEventListener("change", () => {
        mc.halves[name].speed = Math.max(0.1, Math.min(4, parseFloat($("mix-" + name + "-speed").value) || 1));
        syncMixUi();
        recompute();
      });
      $("mix-" + name + "-loop").addEventListener("change", () => { mc.halves[name].loop = $("mix-" + name + "-loop").checked; recompute(); });
    }
    $("mix-seam").addEventListener("change", () => { mc.halves.seam = $("mix-seam").value; recompute(); });
    $("mix-balance").addEventListener("change", () => { mc.halves.balance = $("mix-balance").checked; recompute(); });
  }

  // --- Pose keys (#83) -------------------------------------------------------------------------
  // A part picked, turned at the marker with three sliders (moving one keys it there), in "adjust"
  // (on top of the capture) or "set" (the capture ignored, 0 the stance's pose) - MC.applyPoses.
  const POSE_PARTS = [["rootdummy", "Body position", "Body"], ["pelvis_g", "Hips", "Hips"], ["torso_g", "Torso", "Torso"],
    ["neck_g", "Neck", "Neck"], ["head_g", "Head", "Head"], ["lthigh_g", "Left thigh", "L thigh"], ["lshin_g", "Left shin", "L shin"],
    ["lfoot_g", "Left foot", "L foot"], ["rthigh_g", "Right thigh", "R thigh"], ["rshin_g", "Right shin", "R shin"], ["rfoot_g", "Right foot", "R foot"],
    ["lbicep_g", "Left upper arm", "L arm"], ["lforearm_g", "Left forearm", "L forearm"], ["lhand_g", "Left hand", "L hand"],
    ["rbicep_g", "Right upper arm", "R arm"], ["rforearm_g", "Right forearm", "R forearm"], ["rhand_g", "Right hand", "R hand"]];
  const partName = (bone, short) => { const p = POSE_PARTS.find(([b]) => b === bone); return p ? p[short ? 2 : 1] : bone; };
  const findPart = (bone) => mc.poses.find((p) => p.bone === bone);
  const poseRows = () => POSE_PARTS.map(([b]) => findPart(b)).filter((p) => p && p.keys.length);
  const keyAt = (part, t) => (part && mc.take ? part.keys.find((k) => Math.abs(k.t - t) < 0.5 / mc.take.fps) : null);
  const poseFps = () => (mc.take ? mc.take.fps : 30);
  const markerFrame = () => { const t = markerTime(); return t === null ? null : Math.round(t * poseFps()) / poseFps(); };
  let recomputeQueued = false;
  const recomputeSoon = () => { if (!recomputeQueued) { recomputeQueued = true; requestAnimationFrame(() => { recomputeQueued = false; recompute(); }); } };
  // The part's value at the marker: its key there, else between its keys, else its nearest key's, else 0.
  function poseShown() {
    const part = findPart(mc.posePart), t = markerTime();
    if (!part || !part.keys.length || t === null) return [0, 0, 0];
    const at = keyAt(part, t);
    if (at) return at.v.slice();
    const keys = part.keys;
    if (t <= keys[0].t) return keys[0].v.slice();
    if (t >= keys[keys.length - 1].t) return keys[keys.length - 1].v.slice();
    return MC.poseValue(part, t).v.slice();
  }
  let poseUiStamp = "";
  function syncPoseUi(force) {
    const part = findPart(mc.posePart), t = markerTime();
    const stamp = [mc.posePart, t === null ? "-" : t.toFixed(4), part ? part.mode + part.fade + part.keys.map((k) => k.t.toFixed(4) + ":" + k.v.join(",")).join(";") : "",
      mc.poseSel ? mc.poseSel.key.t : ""].join("|");
    if (!force && stamp === poseUiStamp) return;
    poseUiStamp = stamp;
    $("pose-part").value = mc.posePart;
    if (part) { $("pose-mode").value = part.mode; $("pose-fade").value = part.fade; }
    const body = mc.posePart === "rootdummy";
    $("pose-mode").disabled = body;
    const names = body ? ["Forward / back (cm)", "Side, + right (cm)", "Up / down (cm)"] : ["Forward / back (°)", "Side, + right (°)", "Twist (°)"];
    const v = poseShown();
    for (let c = 0; c < 3; c++) {
      const s = $("pose-" + c);
      s.min = body ? -60 : -180;
      s.max = body ? 60 : 180;
      if (document.activeElement !== s) s.value = v[c];
      $("pose-n" + c).textContent = names[c];
      $("pose-v" + c).textContent = Math.round(v[c]) + (body ? " cm" : "°");
    }
    const at = part && t !== null ? keyAt(part, t) : null;
    $("pose-del").disabled = !(at || mc.poseSel);
    for (const id of ["pose-prev", "pose-next", "pose-hold", "pose-clear"]) $(id).disabled = !part || !part.keys.length;
    $("pose-info").textContent = !mc.take ? "Drop a take to pose it." : (partName(mc.posePart) + ": " + (part && part.keys.length
      ? part.keys.length + " key" + (part.keys.length > 1 ? "s" : "") + " (" + (body ? "moved" : part.mode === "set" ? "set" : "adjusted") + ")" + (at ? "; the marker is on one" : "")
      : "no keys yet - move a slider to key it at the marker") + ".");
  }
  function poseSlider(c) {
    if (!mc.take || markerFrame() === null) return;
    setPlaying(false);
    const t = markerFrame();
    let part = findPart(mc.posePart);
    if (!part) { part = { bone: mc.posePart, mode: $("pose-mode").value, fade: Math.max(0, parseFloat($("pose-fade").value) || 0), keys: [] }; mc.poses.push(part); }
    let key = keyAt(part, t);
    if (!key) {
      key = { t, v: [0, 1, 2].map((i) => parseFloat($("pose-" + i).value) || 0) };
      part.keys.push(key);
      part.keys.sort((p, q) => p.t - q.t);
    }
    key.v[c] = parseFloat($("pose-" + c).value) || 0;
    mc.poseSel = { bone: part.bone, key };
    mc.selected = null;
    $("pose-v" + c).textContent = Math.round(key.v[c]) + (part.bone === "rootdummy" ? " cm" : "°");
    recomputeSoon();
  }
  function deletePoseKey() {
    const part = mc.poseSel ? findPart(mc.poseSel.bone) : findPart(mc.posePart);
    const key = mc.poseSel ? mc.poseSel.key : keyAt(part, markerTime());
    if (!part || !key) return;
    part.keys = part.keys.filter((k) => k !== key);
    if (!part.keys.length) mc.poses = mc.poses.filter((p) => p !== part);
    mc.poseSel = null;
    syncPoseUi(true);
    recompute();
  }
  function poseJump(dir) {
    const part = findPart(mc.posePart), t = markerTime();
    if (!part || t === null) return;
    const half = 0.5 / poseFps();
    const keys = dir < 0 ? part.keys.filter((k) => k.t < t - half) : part.keys.filter((k) => k.t > t + half);
    const key = dir < 0 ? keys[keys.length - 1] : keys[0];
    if (!key) return;
    setPlaying(false);
    seek(key.t);
    mc.poseSel = { bone: part.bone, key };
  }
  // Hold: the part's last pose before the marker kept to the marker (a key alike there).
  function poseHold() {
    const part = findPart(mc.posePart), t = markerFrame();
    if (!part || t === null) return;
    const before = part.keys.filter((k) => k.t < t - 0.5 / poseFps());
    if (!before.length) return;
    let key = keyAt(part, t);
    if (!key) { key = { t, v: before[before.length - 1].v.slice() }; part.keys.push(key); part.keys.sort((p, q) => p.t - q.t); }
    else key.v = before[before.length - 1].v.slice();
    mc.poseSel = { bone: part.bone, key };
    syncPoseUi(true);
    recompute();
  }
  {
    const select = $("pose-part");
    for (const [bone, name] of POSE_PARTS) select.appendChild(new Option(name, bone));
    select.value = mc.posePart;
    select.addEventListener("change", () => { mc.posePart = select.value; mc.poseSel = null; syncPoseUi(true); });
    $("pose-mode").addEventListener("change", () => { const p = findPart(mc.posePart); if (p) { p.mode = $("pose-mode").value; recompute(); } syncPoseUi(true); });
    $("pose-fade").addEventListener("change", () => { const p = findPart(mc.posePart); if (p) { p.fade = Math.max(0, parseFloat($("pose-fade").value) || 0); recompute(); } syncPoseUi(true); });
    for (let c = 0; c < 3; c++) {
      $("pose-" + c).addEventListener("input", () => poseSlider(c));
      $("pose-" + c).addEventListener("change", () => syncPoseUi(true));
    }
    $("pose-prev").addEventListener("click", () => poseJump(-1));
    $("pose-next").addEventListener("click", () => poseJump(1));
    $("pose-hold").addEventListener("click", poseHold);
    $("pose-del").addEventListener("click", deletePoseKey);
    $("pose-clear").addEventListener("click", () => { mc.poses = mc.poses.filter((p) => p.bone !== mc.posePart); mc.poseSel = null; syncPoseUi(true); recompute(); });
  }
  // This take's edits, kept in this browser (#83): its file and frame count name it.
  const sessionKey = () => (mc.capture ? "nwn5e-mocap|" + mc.fileName + "|" + mc.capture.frames.length : null);
  function saveSession() {
    const key = sessionKey();
    if (!key || !mc.take) return;
    try {
      localStorage.setItem(key, JSON.stringify({ saved: Date.now(), ref: mc.ref, start: mc.start, end: mc.end, plants: mc.plants, stance: mc.stance,
        place: mc.place, locks: mc.locks, poses: mc.poses, halves: mc.halves }));
    } catch (e) { /* no storage: the edits last until the page closes */ }
  }
  function loadSession() {
    const key = sessionKey();
    if (!key) return null;
    try { const s = JSON.parse(localStorage.getItem(key) || "null"); return s && Array.isArray(s.poses) ? s : null; } catch (e) { return null; }
  }

  // Pressing the timeline (#79): above the rows, the marker goes there (drag to scrub); on a
  // row, a drag along it adds a plant, a click selects one, and the selected one's ends drag
  // to lengthen or shorten it - snapping to the marker (within 8 px) and else to whole frames.
  function footHit(e) {
    const r = $("mc-motion").getBoundingClientRect();
    const d = Math.max(mc.take ? mc.take.duration : 1, 1e-6);
    const x = e.clientX - r.left, y = e.clientY - r.top;
    const top = r.height - rowsHeight();
    let side = null;
    [["left", 0], ["right", 1]].forEach(([s, i]) => { if (y >= top + rowTop(i) - 1 && y < top + rowTop(i) + ROW_H + 1) side = s; });
    const animRows = mc.halves.legs.src !== "take" && y >= top && y < top + rowTop(2);  // the legs' animation's plants: not editable (#84)
    if (animRows) side = null;
    const t = Math.max(0, Math.min(d, (x / Math.max(r.width, 1)) * d));
    const pxPerS = r.width / d;
    const parts = poseRows();
    for (let n = 0; n < parts.length; n++) {
      if (y < top + rowTop(2 + n) - 1 || y >= top + rowTop(2 + n) + ROW_H + 1) continue;
      let key = null;
      for (const k of parts[n].keys) if (Math.abs(x - k.t * pxPerS) <= 6 && (!key || Math.abs(x - k.t * pxPerS) < Math.abs(x - key.t * pxPerS))) key = k;
      return { area: "pose", part: parts[n], key, t, x, pxPerS };
    }
    const area = side ? "row" : (y >= 0 && y < top) || animRows ? "curves" : null;
    let edge = null, inside = null;
    if (side) {
      const sel = mc.selected && mc.selected.side === side ? mc.selected.bar : null;
      const near = (bar, grab) => (Math.abs(x - bar[0] * pxPerS) <= grab ? 0 : Math.abs(x - bar[1] * pxPerS) <= grab ? 1 : -1);
      if (sel && near(sel, 7) >= 0) edge = { bar: sel, end: near(sel, 7) };  // the selected one's handles first, and wider
      for (const bar of mc.plants[side]) {
        if (edge) break;
        const n = near(bar, 4);
        if (n >= 0) edge = { bar, end: n };
        else if (t >= bar[0] && t <= bar[1]) inside = bar;
      }
    }
    return { area, side, t, x, pxPerS, edge, inside };
  }
  // An edge's time: on the marker when within 8 px of it, else on a whole frame of the take.
  function snapTime(t, pxPerS) {
    const now = markerTime();
    if (now !== null && Math.abs(t - now) * pxPerS <= 8) return { t: now, snapped: true };
    return { t: Math.round(t * mc.take.fps) / mc.take.fps, snapped: false };
  }
  const seek = (t) => { if (mc.result && mc.result.clipTime) state.time = mc.result.clipTime(t); };
  // Sorted, the overlapping ones joined (ones that only come close stay two: a shuffle's);
  // the selection follows its plant.
  function tidyPlants(side) {
    const sel = mc.selected && mc.selected.side === side ? mc.selected.bar : null;
    const bars = mc.plants[side].filter(([a, b]) => b - a >= 0.1).sort((p, q) => p[0] - q[0]);
    const out = [];
    for (const bar of bars) {
      const last = out[out.length - 1];
      if (last && bar[0] < last[1]) { last[1] = Math.max(last[1], bar[1]); if (bar === sel) mc.selected.bar = last; } else out.push(bar);
    }
    mc.plants[side] = out;
    if (mc.selected && mc.selected.side === side && !out.includes(mc.selected.bar)) mc.selected = null;
  }
  function removeSelectedPlant() {
    if (!mc.selected) return;
    const { side, bar } = mc.selected;
    mc.plants[side] = mc.plants[side].filter((p) => p !== bar);
    mc.selected = null;
    recompute();
  }
  {
    const graph = $("mc-motion");
    graph.addEventListener("pointerdown", (e) => {
      if (!mc.take) return;
      const h = footHit(e);
      if (!h.area) return;
      if (h.area === "curves") { footDrag = { mode: "seek" }; seek(h.t); }
      else if (h.area === "pose") {
        mc.posePart = h.part.bone;
        mc.selected = null;
        setPlaying(false);
        if (h.key) { mc.poseSel = { bone: h.part.bone, key: h.key }; seek(h.key.t); footDrag = { mode: "posekey", part: h.part, key: h.key, moved: false }; }
        else { mc.poseSel = null; seek(h.t); footDrag = { mode: "seek" }; }
      } else if (h.edge) { mc.poseSel = null; mc.selected = { side: h.side, bar: h.edge.bar }; footDrag = { mode: "edge", side: h.side, bar: h.edge.bar, end: h.edge.end, moved: false }; }
      else if (h.inside) { mc.poseSel = null; mc.selected = { side: h.side, bar: h.inside }; footDrag = { mode: "select" }; }
      else { const s = snapTime(h.t, h.pxPerS); footDrag = { mode: "new", side: h.side, t0: s.t, t1: s.t, snapped: s.snapped }; }
      graph.setPointerCapture(e.pointerId);
      e.preventDefault();
    });
    graph.addEventListener("pointermove", (e) => {
      if (!mc.take) return;
      const h = footHit(e);
      if (!footDrag) {
        graph.style.cursor = h.area === "curves" ? "pointer" : h.area === "pose" ? (h.key ? "ew-resize" : "pointer") : !h.area ? "" : h.edge ? "ew-resize" : h.inside ? "pointer" : "crosshair";
        return;
      }
      if (footDrag.mode === "seek") seek(h.t);
      else if (footDrag.mode === "posekey") {
        const s = snapTime(h.t, h.pxPerS);
        footDrag.key.t = s.t;
        footDrag.snapped = s.snapped;
        footDrag.moved = true;
      } else if (footDrag.mode === "new") { const s = snapTime(h.t, h.pxPerS); footDrag.t1 = s.t; footDrag.snapped = s.snapped; }
      else if (footDrag.mode === "edge") {
        const s = snapTime(h.t, h.pxPerS), bar = footDrag.bar;
        if (footDrag.end === 0) bar[0] = Math.min(s.t, bar[1] - 0.1); else bar[1] = Math.max(s.t, bar[0] + 0.1);
        footDrag.snapped = s.snapped;
        footDrag.moved = true;
      }
    });
    const done = (cancel) => {
      const f = footDrag;
      footDrag = null;
      if (!f || cancel || f.mode === "seek" || f.mode === "select") return;
      if (f.mode === "posekey") {
        if (f.moved) { f.part.keys.sort((p, q) => p.t - q.t); seek(f.key.t); recompute(); }
        return;
      }
      if (f.mode === "new") {
        if (Math.abs(f.t1 - f.t0) < 0.1) { mc.selected = null; return; }  // a click on an empty stretch: nothing selected
        const bar = [Math.min(f.t0, f.t1), Math.max(f.t0, f.t1)];
        mc.plants[f.side].push(bar);
        mc.selected = { side: f.side, bar };
      }
      if (f.mode === "edge" && !f.moved) return;
      tidyPlants(f.side);
      recompute();
    };
    graph.addEventListener("pointerup", () => done(false));
    graph.addEventListener("pointercancel", () => done(true));
    $("mc-plant-remove").addEventListener("click", removeSelectedPlant);
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
