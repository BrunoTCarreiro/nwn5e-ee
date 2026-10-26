// VFX Bench - the page: the effect being edited, the effects to open, the emitter's
// fields, the stage, Save, and Play / Stop in game through the bench's server.
(function () {
  "use strict";
  const core = window.VfxCore;
  const { Sim } = window.VfxSim;
  const $ = (id) => document.getElementById(id);
  const STORE = "vfxbench.v1";
  const BACKDROPS = { dark: [0.047, 0.067, 0.082], grey: [0.3, 0.33, 0.36], light: [0.8, 0.83, 0.86] };

  const state = {
    server: null,     // /api/state
    tab: "game",
    fx: core.newEffect(),
    selected: 0,
    node: -1,         // the node being edited (-1: none)
    loadedText: "",   // the draft as last opened or saved: anything else is unsaved
    current: null,    // "<source>:<name>" of the opened effect, for the list's highlight
    paused: false,
    speed: 1,
    loop: true,
    link: { alive: false, status: null },
  };
  const sim = new Sim();
  let view = null;

  // --- small helpers -------------------------------------------------------------------
  function el(tag, props, children) {
    const node = document.createElement(tag);
    if (props) for (const [k, v] of Object.entries(props)) {
      if (k === "class") node.className = v;
      else if (k === "text") node.textContent = v;
      else if (k.startsWith("on")) node.addEventListener(k.slice(2), v);
      else if (v !== undefined && v !== null && v !== false) node.setAttribute(k, v === true ? "" : v);
    }
    for (const child of [].concat(children || [])) if (child) node.append(child);
    return node;
  }
  function setStatus(text, kind) {
    const s = $("status");
    s.textContent = text || "";
    s.className = "status" + (kind ? " " + kind : "");
  }
  async function getJson(url) {
    const res = await fetch(url, { cache: "no-store" });
    if (!res.ok) throw new Error((await res.json().catch(() => ({}))).error || res.statusText);
    return res.json();
  }
  async function getText(url) {
    const res = await fetch(url, { cache: "no-store" });
    if (!res.ok) throw new Error((await res.json().catch(() => ({}))).error || res.statusText);
    return res.text();
  }
  async function postText(url, text) {
    const res = await fetch(url, { method: "POST", headers: { "Content-Type": "text/plain; charset=utf-8" }, body: text });
    const body = await res.json().catch(() => ({}));
    if (!res.ok) throw new Error(body.error || res.statusText);
    return body;
  }
  const offline = "The bench's server isn't answering - is the VFX Bench window still open?";
  const draftText = () => core.writeDraft(state.fx);
  const dirty = () => draftText() !== state.loadedText;
  const emitterNow = () => state.fx.emitters[state.selected] || null;

  // --- textures: decoded once, for the stage and the thumbnails --------------------------
  const images = new Map();
  function image(name) {
    const key = String(name || "").toLowerCase();
    if (!images.has(key)) {
      images.set(key, fetch("/api/texture/" + encodeURIComponent(key), { cache: "force-cache" })
        .then((res) => { if (!res.ok) throw new Error("no texture"); return res.arrayBuffer(); })
        .then((buffer) => core.decodeTga(new Uint8Array(buffer))));
    }
    return images.get(key);
  }
  const thumbs = new Map();
  function thumb(name) {
    const key = String(name || "").toLowerCase();
    if (!thumbs.has(key)) {
      thumbs.set(key, image(key).then((img) => {
        const source = el("canvas", { width: img.width, height: img.height });
        source.getContext("2d").putImageData(new ImageData(img.data, img.width, img.height), 0, 0);
        const size = 96, out = el("canvas", { width: size, height: size });
        const g = out.getContext("2d");
        g.fillStyle = "#0c1115";
        g.fillRect(0, 0, size, size);
        g.drawImage(source, 0, 0, size, size);
        return out.toDataURL();
      }));
    }
    return thumbs.get(key);
  }
  function showThumb(node, name) {
    node.style.backgroundImage = "";
    thumb(name).then((url) => { node.style.backgroundImage = `url("${url}")`; }).catch(() => {});
  }
  const inLibrary = (name) => !!state.server && state.server.textures.includes(String(name).toLowerCase());

  // --- the library -------------------------------------------------------------------------
  function entries() {
    const s = state.server;
    if (!s) return [];
    if (state.tab === "saved") return s.saved.map((d) => ({ source: "saved", name: d.name, title: d.name,
      sub: d.model === "written" ? "model written" : d.model === "older" ? "model older than the draft: the game rewrites it" : "the game writes its model when it runs" }));
    if (state.tab === "nwn5e") return s.nwn5e.map((name) => ({ source: "nwn5e", name, title: name, sub: "nwn5e's own (opens as a copy)" }));
    return s.game.map((g) => ({ source: "game", name: g.model, meta: g, title: g.row + "  " + g.label,
      sub: `${g.model} · ${g.attach} · ${g.type === "F" ? "plays once" : "stays"}` }));
  }

  function renderLibrary() {
    const s = state.server;
    for (const tab of ["saved", "nwn5e", "game"]) {
      const button = $("tab-" + tab);
      button.setAttribute("aria-selected", String(state.tab === tab));
      const count = s ? (tab === "saved" ? s.saved.length : tab === "nwn5e" ? s.nwn5e.length : s.game.length) : 0;
      button.replaceChildren(document.createTextNode(button.dataset.label + " "), el("small", { text: String(count) }));
    }
    const query = $("search").value.trim().toLowerCase();
    const shown = entries().filter((e) => !query || (e.title + " " + e.sub).toLowerCase().includes(query));
    const list = $("list");
    list.replaceChildren(...shown.map((e) => el("li", null, el("button", {
      type: "button", class: state.current === e.source + ":" + e.name ? "current" : "",
      onclick: () => open(e),
    }, [document.createTextNode(e.title), el("span", { class: "sub", text: e.sub })]))));
    if (!shown.length) list.append(el("li", { class: "empty-row", text: s && !s.exported && state.tab !== "saved"
      ? "Nothing yet: start the game with nwn5e 0.42.0 or later (vfx_bench=1) - it copies its effects out 3 s in, and this list fills in by itself."
      : state.tab === "saved" && !query ? "Nothing saved yet." : "Nothing matches." }));
    $("lib-note").textContent = state.tab === "game" ? "The game's effects with emitters (its own files, copied out by nwn5e). They open as fx<row>: Save keeps your copy." : "";
    renderOverlay();
  }

  async function refresh() {
    try {
      state.server = await getJson("/api/state");
    } catch (e) {
      setStatus(offline, "bad");
      return;
    }
    $("stale").hidden = !state.server.stale;
    state.link = state.server.link;
    renderLink();
    renderLibrary();
  }

  async function open(entry, quiet) {
    if (!quiet && dirty() && !confirm(`Drop the unsaved changes to ${state.fx.name}?`)) return;
    let fx;
    try {
      fx = core.readDraft(await getText(`/api/draft?source=${entry.source}&name=${encodeURIComponent(entry.name)}`));
    } catch (e) {
      setStatus("Couldn't open it: " + e.message, "bad");
      return;
    }
    if (entry.source === "game") {
      fx.name = "fx" + entry.meta.row;
      fx.attach = entry.meta.attach;
      fx.duration = entry.meta.type !== "F";
    } else if (entry.source === "nwn5e") {
      fx.name = core.resref(fx.name + "_copy", 16);
    }
    state.current = entry.source + ":" + entry.name;
    setEffect(fx, true);
    setStatus(`Opened ${entry.title.trim()} (${fx.emitters.length} emitter${fx.emitters.length === 1 ? "" : "s"}) as ${fx.name}.`);
    renderLibrary();
  }

  // --- the effect ----------------------------------------------------------------------------
  function setEffect(fx, baseline) {
    state.fx = fx;
    state.selected = 0;
    state.node = fx.nodes && fx.nodes.length ? 0 : -1;
    if (baseline) state.loadedText = draftText();
    sim.hidden.clear();
    sim.setEffect(fx);
    sim.restart();
    renderEffect();
    renderEmitters();
    renderFields();
    renderNodes();
    changed(true);
  }

  // After any change: the preview reads the effect as it goes; the rest follows.
  let storeTimer = 0;
  function changed(structure) {
    if (structure) sim.setEffect(state.fx);
    if (view) view.attach = state.fx.attach;
    if (!$("draft").matches(":focus")) $("draft").value = draftText();
    renderOverlay();
    clearTimeout(storeTimer);
    storeTimer = setTimeout(() => {
      try {
        localStorage.setItem(STORE, JSON.stringify({ text: draftText(), loaded: state.loadedText, selected: state.selected, tab: state.tab, current: state.current }));
      } catch (e) { /* storage off: nothing kept */ }
    }, 300);
    document.title = (dirty() ? "• " : "") + state.fx.name + " - VFX Bench";
  }

  function renderEffect() {
    const fx = state.fx;
    $("fx-name").value = fx.name;
    $("fx-type").value = fx.duration ? "D" : "F";
    $("fx-seconds").value = core.num(fx.seconds);
    $("fx-attach").value = core.ATTACHES.includes(fx.attach) ? fx.attach : "root";
    const l = fx.lengths || (fx.lengths = [0, 2, 1]);
    [0, 1, 2].forEach((k) => { $("fx-len" + k).value = core.num(l[k]); });
    $("fx-note").textContent = (fx.duration
      ? "Stays on the character while applied; the preview (and Play in game) runs it for Seconds."
      : "Plays once; an emitter without its own On for stays on for Seconds.") +
      " An emitter's position is from that point: on a human the feet, the chest (1.5 m) or over the head (2.4 m, where the game's spell glows sit).";
  }

  function emitterLine(e) { return `${e.update} · ${e.render} · ${e.texture}`; }

  function renderEmitters() {
    const list = $("emitters");
    list.replaceChildren(...state.fx.emitters.map((e, i) => el("li", { class: i === state.selected ? "current" : "" }, [
      el("label", { class: "check", title: "Shown in the preview (not saved)" }, [
        el("input", { type: "checkbox", checked: !sim.hidden.has(i), "aria-label": "Show " + e.name, onchange: (ev) => {
          if (ev.target.checked) sim.hidden.delete(i); else sim.hidden.add(i);
        } }),
      ]),
      el("button", { type: "button", class: "pick", onclick: () => { state.selected = i; renderEmitters(); renderFields(); } },
        [document.createTextNode(e.name), el("span", { class: "sub", text: emitterLine(e) })]),
    ])));
    if (!state.fx.emitters.length) list.append(el("li", null, el("span", { class: "note", text: "No emitters: Add one." })));
    $("em-copy").disabled = $("em-del").disabled = !emitterNow();
    $("em-up").disabled = state.selected <= 0;
    $("em-down").disabled = state.selected >= state.fx.emitters.length - 1;
  }

  // --- the emitter's fields ----------------------------------------------------------------
  const GROUPS = [
    { title: "Kind", fields: [
      { key: "name", type: "text", label: "Name" },
      { key: "parent", type: "parent", label: "Hangs under (its position and turn are from there)" },
      { key: "update", type: "select", options: core.UPDATES, label: "Update",
        help: "Fountain: a stream. Single: one particle that stays. Explosion: all at once when it starts. Lightning: a bolt (only in game)." },
      { key: "render", type: "select", options: core.RENDERS, label: "Render",
        help: "Normal faces the screen. Billboard_to_World_Z lies flat facing up (seen from above only); Billboard_to_Local_Z lies flat in the emitter's turn (seen from one side). Aligned_to_World_Z stands upright. Aligned_to_Particle_Dir lies along the way it left (seen from one side). Twosidedtex (under Look) shows both sides. Motion_Blur streaks along its path; Linked joins each particle to the next." },
      { key: "blend", type: "select", options: core.BLENDS, label: "Blend",
        help: "Lighten adds light (fire, magic); Normal covers (smoke, dust); Punch-Through cuts out what's 20% opaque or less, no blending." },
    ] },
    { title: "Texture", fields: [
      { key: "texture", type: "texture", label: "Texture" },
      { key: "grid", type: "int", min: 1, max: 8, label: "Sheet: frames across and down" },
      { key: "frame_end", type: "int", min: 0, max: 63, label: "Last frame used (0 = the first only)" },
      { key: "fps", type: "num", min: 0, max: 60, step: 1, label: "Frames a second (0 = the frame stays; they loop)" },
      { key: "random", type: "bool", label: "A random first frame for each particle" },
    ] },
    { title: "Emission", fields: [
      { key: "position", type: "vec3", step: 0.05, label: "Position x y z (metres from the node)" },
      { key: "orientation", type: "vec4", step: 0.05, label: "Turn: axis x y z, then angle in radians (0 = straight up, 3.14 = straight down)" },
      { key: "birthrate", type: "num", min: 0, max: 400, step: 1, label: "Rate (a second; Explosion: how many)" },
      { key: "emit_seconds", type: "num", min: 0, max: 10, step: 0.05, label: "On for (seconds; 0 = while the effect plays)" },
      { key: "xsize", type: "num", min: 0, max: 600, step: 1, label: "Area across (cm)" },
      { key: "ysize", type: "num", min: 0, max: 600, step: 1, label: "Area deep (cm)" },
    ] },
    { title: "Motion", fields: [
      { key: "life", type: "num", min: -1, max: 10, step: 0.05, label: "Life (seconds; -1 = for ever)" },
      { key: "velocity", type: "num", min: 0, max: 20, step: 0.05, label: "Speed (m/s)" },
      { key: "randvel", type: "num", min: 0, max: 20, step: 0.05, label: "Extra random speed (up to)" },
      { key: "spread", type: "num", min: 0, max: 6.2832, step: 0.01, label: "Spread (radians: 0 straight up, 3.14 half a sphere, 6.28 all round)" },
      { key: "deadspace", type: "num", min: 0, max: core.DEADSPACE_MAX, step: 0.01, limit: true, label: "Dead space (radians, up to 0.39)",
        help: "With a spread: a particle that would leave with its sheet almost edge-on to the camera is turned toward it as it's born (the game's own effects use 0.17 - 0.21). Above 0.39 the game can hang, so it stops there." },
      { key: "mass", type: "num", min: -3, max: 3, step: 0.05, label: "Gravity (mass; below 0 rises)" },
      { key: "inherit", type: "bool", label: "Carried and turned with the emitter (inherit)" },
      { key: "inherit_local", type: "bool", label: "Follows the character's move (inherit_local; try Walk)" },
      { key: "inherit_part", type: "bool", label: "Follows the emitter's move (inherit_part)" },
      { key: "bounce", type: "bool", label: "Bounces on the ground" },
      { key: "bounce_co", type: "num", min: 0, max: 1, step: 0.05, label: "Bounce: speed kept" },
      { key: "splat", type: "bool", label: "Lies flat where it lands" },
      { key: "blur_length", type: "num", min: 0, max: 10, step: 0.05, label: "Blur length (Motion_Blur: how many seconds its tail lags)" },
      { key: "particle_rot", type: "num", min: -10, max: 10, step: 0.1, label: "Spin (radians a second)" },
    ] },
    { title: "Its own motion", fields: [
      { key: "keys", type: "motion", label: "Keys" },
    ] },
    { title: "Look", fields: [
      { key: "size_start", type: "num", min: 0, max: 5, step: 0.01, label: "Size at birth (m)" },
      { key: "size_end", type: "num", min: 0, max: 5, step: 0.01, label: "Size at the end (m)" },
      { key: "color_start", type: "colour", label: "Colour at birth" },
      { key: "color_end", type: "colour", label: "Colour at the end" },
      { key: "alpha_start", type: "num", min: 0, max: 1, step: 0.01, label: "Opacity at birth" },
      { key: "alpha_end", type: "num", min: 0, max: 1, step: 0.01, label: "Opacity at the end" },
      { key: "twosidedtex", type: "bool", label: "Seen from both sides (twosidedtex: the flat and particle-direction kinds otherwise show one side)" },
    ] },
  ];

  function hex(rgb) {
    return "#" + rgb.map((v) => Math.round(Math.max(0, Math.min(1, v)) * 255).toString(16).padStart(2, "0")).join("");
  }

  function numberInput(value, step, onValue, label) {
    return el("input", { type: "number", step: String(step), value: core.num(value), "aria-label": label,
      oninput: (ev) => { const v = parseFloat(ev.target.value); if (!Number.isNaN(v)) onValue(Math.fround(v)); } });
  }

  function field(e, f) {
    const set = (value) => {
      e[f.key] = value;
      if (f.key === "name" || f.key === "update" || f.key === "render" || f.key === "texture") renderEmitters();
      changed(false);
    };
    const wrap = (children, cls) => el("div", { class: "field" + (cls ? " " + cls : ""), title: f.help || null }, children);
    if (f.type === "text") {
      return wrap([el("span", { text: f.label }), el("input", { type: "text", value: e[f.key], spellcheck: "false", "aria-label": f.label,
        oninput: (ev) => set(ev.target.value || "emitter") })]);
    }
    if (f.type === "select") {
      const value = String(e[f.key]);
      const match = f.options.find((o) => o.toLowerCase() === value.toLowerCase());
      const options = match ? f.options : [...f.options, value];
      const exact = f.options.includes(value);
      return wrap([el("span", { text: f.label }), el("select", { "aria-label": f.label, onchange: (ev) => set(ev.target.value) },
        options.map((o) => el("option", { value: o, selected: o === (exact ? value : match || value),
          text: o === value && !exact ? o + " (the model's spelling - the game reads only " + (match || "the listed names") + ")" : o }))),
      f.help ? el("span", { class: "note", text: f.help }) : null]);
    }
    if (f.type === "bool") {
      return el("label", { class: "field flag" }, [el("input", { type: "checkbox", checked: !!e[f.key], onchange: (ev) => set(ev.target.checked) }),
        document.createTextNode(f.label)]);
    }
    if (f.type === "num" || f.type === "int") {
      const integer = f.type === "int";
      const range = el("input", { type: "range", min: f.min, max: f.max, step: integer ? 1 : f.step, value: e[f.key], "aria-label": f.label });
      const number = el("input", { type: "number", step: integer ? 1 : f.step, value: integer ? e[f.key] : core.num(e[f.key]), "aria-label": f.label });
      range.addEventListener("input", () => {
        const v = integer ? parseInt(range.value, 10) : Math.fround(parseFloat(range.value));
        number.value = integer ? v : core.num(v);
        set(v);
      });
      number.addEventListener("input", () => {
        const raw = integer ? parseInt(number.value, 10) : parseFloat(number.value);
        if (Number.isNaN(raw)) return;
        let v = integer ? Math.max(f.key === "grid" ? 1 : 0, raw) : Math.fround(raw);
        if (f.limit && (v > f.max || v < f.min)) {  // a hard limit (deadspace): held there
          v = Math.fround(Math.min(f.max, Math.max(f.min, v)));
          number.value = core.num(v);
        }
        range.value = v;
        set(v);
      });
      return wrap([el("span", { text: f.label }), el("div", { class: "pair" }, [range, number])]);
    }
    if (f.type === "vec3" || f.type === "vec4") {
      const n = f.type === "vec4" ? 4 : 3;
      if (!e[f.key]) e[f.key] = new Array(n).fill(0);
      return wrap([el("span", { text: f.label }), el("div", { class: n === 4 ? "quad" : "triple" }, [...Array(n).keys()].map((k) =>
        numberInput(e[f.key][k], f.step, (v) => { e[f.key][k] = v; changed(false); }, f.label + " " + "xyzw"[k])))]);
    }
    if (f.type === "colour") {
      const values = e[f.key];
      const numbers = [0, 1, 2].map((k) => numberInput(values[k], 0.01, (v) => { values[k] = v; picker.value = hex(values); changed(false); }, f.label + " " + "rgb"[k]));
      const picker = el("input", { type: "color", value: hex(values), "aria-label": f.label, oninput: (ev) => {
        const h = ev.target.value;
        for (let k = 0; k < 3; k++) {
          values[k] = Math.fround(parseInt(h.slice(1 + 2 * k, 3 + 2 * k), 16) / 255);
          numbers[k].value = core.num(values[k]);
        }
        changed(false);
      } });
      return wrap([el("span", { text: f.label + " (red green blue, 0-1)" }), el("div", { class: "colour" }, [picker, ...numbers])]);
    }
    if (f.type === "parent") return parentField(e, f.label, () => { renderEmitters(); changed(true); });
    if (f.type === "motion") return motionField(e, true);
    if (f.type === "texture") {
      const box = el("span", { class: "thumb" });
      showThumb(box, e.texture);
      return wrap([el("span", { text: f.label }), el("button", { type: "button", class: "texture-pick", onclick: () => openPicker() },
        [box, document.createTextNode(e.texture + (inLibrary(e.texture) ? "" : "  (not in the library)"))])]);
    }
    return null;
  }

  function renderFields() {
    const e = emitterNow();
    const box = $("fields");
    if (!e) {
      box.replaceChildren(el("p", { class: "note", text: "Add an emitter to start." }));
      return;
    }
    $("emf-h").textContent = "Emitter: " + e.name;
    box.replaceChildren(...GROUPS.map((g) => el("div", { class: "group" }, [el("h3", { text: g.title }), ...g.fields.map((f) => field(e, f))])));
  }

  // --- nodes and motion (0.42.3) -------------------------------------------------------------
  // What a node or an emitter can hang under: the character's point, the nodes, the other
  // emitters - not itself or anything under it.
  function under(name) {
    const fx = state.fx, items = [...(fx.nodes || []), ...fx.emitters];
    const parentOf = (n) => (items.find((x) => x.name === n) || {}).parent || "";
    const below = (n) => { for (let up = n, k = 0; up && k < 64; up = parentOf(up), k++) if (up === name) return true; return false; };
    return items.map((x) => x.name).filter((n, i, all) => n && all.indexOf(n) === i && !below(n));
  }

  function parentField(item, label, onChange) {
    const options = ["", ...under(item.name)];
    if (item.parent && !options.includes(item.parent)) options.push(item.parent);
    return el("div", { class: "field" }, [el("span", { text: label }), el("select", { "aria-label": label, onchange: (ev) => {
      item.parent = ev.target.value;
      onChange();
    } }, options.map((o) => el("option", { value: o, selected: o === (item.parent || ""), text: o || "the character's point" })))]);
  }

  function keysLine(keys) {
    const names = { impact: "start", duration: "loop", cessation: "end" };
    const parts = (keys || []).filter((k) => k.position.length || k.orientation.length).map((k) =>
      names[k.anim] + ": " + [k.position.length ? Math.floor(k.position.length / 4) + " move keys" : "", k.orientation.length ? Math.floor(k.orientation.length / 5) + " turn keys" : ""].filter(Boolean).join(", "));
    return parts.length ? parts.join(" · ") : "none - it stays where it is";
  }

  // Keys that turn round z through `turns` whole turns over `length` seconds (quarter-turn
  // steps, so the short-way slerp goes the right way).
  function turnKeys(length, turns) {
    const rows = [], steps = Math.max(4, Math.round(Math.abs(turns)) * 4);
    for (let i = 0; i <= steps; i++) {
      const angle = (i / steps) * turns * 2 * Math.PI;
      rows.push(Math.fround((i / steps) * length), 0, 0, angle < 0 ? -1 : 1, Math.abs(angle));
    }
    return rows;
  }

  function loopAnim() { return state.fx.duration ? "duration" : "impact"; }
  function loopLength() {
    const fx = state.fx;
    return fx.duration ? (fx.lengths[1] > 0 ? fx.lengths[1] : 2) : Math.max(0.2, sim.anims().impact);
  }

  // The motion of a node or an emitter: its keys, and quick ways to make some.
  function motionField(item, isEmitter) {
    const radius = el("input", { type: "number", step: "0.1", value: "1", "aria-label": "Orbit radius (m)" });
    const rate = el("input", { type: "number", step: "0.1", value: "0.5", "aria-label": "Turns a second" });
    const height = el("input", { type: "number", step: "0.1", value: isEmitter ? core.num(item.position[2]) : "0", "aria-label": "Height (m)" });
    const apply = () => {
      const r = parseFloat(rate.value) || 0.5, length = loopLength(), turns = Math.max(1, Math.round(r * length)) * Math.sign(r || 1);
      if (state.fx.duration && Math.abs(r * length - Math.round(r * length)) > 0.01) setStatus(`The loop is ${core.num(length)} s: it makes ${Math.abs(turns)} whole turn${Math.abs(turns) === 1 ? "" : "s"} in it.`);
      if (isEmitter) {
        // A node under the emitter's parent turns; the emitter hangs under it, out at the radius.
        const fx = state.fx;
        let name = core.resref(item.name + "_orbit", 20) || "orbit";
        for (let k = 2; [...fx.nodes, ...fx.emitters].some((x) => x.name === name); k++) name = core.resref(item.name + "_orbit" + k, 20);
        fx.nodes.push(core.node({ name, parent: item.parent || "", position: [0, 0, parseFloat(height.value) || 0],
          keys: [{ anim: loopAnim(), position: [], orientation: turnKeys(length, turns) }] }));
        item.parent = name;
        item.position = [parseFloat(radius.value) || 0, 0, 0];
        state.node = fx.nodes.length - 1;
      } else {
        const keys = core.findKeys(item.keys, loopAnim(), true);
        keys.orientation = turnKeys(length, turns);
      }
      renderEmitters(); renderFields(); renderNodes(); changed(true); sim.restart();
    };
    const clear = () => { item.keys = []; renderFields(); renderNodes(); changed(true); };
    return el("div", { class: "field" }, [
      el("span", { class: "note", text: "Keys: " + keysLine(item.keys) }),
      el("div", { class: "row tight" }, isEmitter
        ? [el("label", null, ["Orbit radius (m)", radius]), el("label", null, ["Turns a second", rate]), el("label", null, ["Height (m)", height])]
        : [el("label", null, ["Turns a second (round its z)", rate])]),
      el("div", { class: "row tight" }, [
        el("button", { type: "button", onclick: apply, title: isEmitter ? "Hangs it under a new node that turns round z while the effect loops" : "Turns it round its z while the effect loops" }, isEmitter ? "Make it orbit" : "Make it spin"),
        el("button", { type: "button", onclick: clear }, isEmitter ? "Clear its motion" : "Clear its keys"),
      ]),
    ]);
  }

  function renderNodes() {
    const fx = state.fx;
    fx.nodes = fx.nodes || [];
    if (state.node >= fx.nodes.length) state.node = fx.nodes.length - 1;
    $("nodes").replaceChildren(...fx.nodes.map((n, i) => el("li", { class: i === state.node ? "current" : "" }, [
      el("button", { type: "button", class: "pick", onclick: () => { state.node = i; renderNodes(); } },
        [document.createTextNode(n.name), el("span", { class: "sub", text: (n.parent ? "under " + n.parent : "on the character") + " · " + keysLine(n.keys) })]),
    ])));
    if (!fx.nodes.length) $("nodes").append(el("li", null, el("span", { class: "note", text: "None: the emitters hang on the character's point. Add one, or Make it orbit on an emitter." })));
    $("nd-del").disabled = state.node < 0;
    const n = fx.nodes[state.node];
    if (!n) { $("node-fields").replaceChildren(); return; }
    const rename = (value) => {
      const clean = value.trim() || "node";
      for (const x of [...fx.nodes, ...fx.emitters]) if (x.parent === n.name) x.parent = clean;
      n.name = clean;
      renderEmitters(); renderFields(); changed(true);
    };
    $("node-fields").replaceChildren(el("div", { class: "group" }, [
      el("div", { class: "field" }, [el("span", { text: "Name" }), el("input", { type: "text", value: n.name, spellcheck: "false", "aria-label": "Node name",
        onchange: (ev) => { rename(ev.target.value); renderNodes(); } })]),
      parentField(n, "Hangs under", () => { renderNodes(); changed(true); }),
      field(n, { key: "position", type: "vec3", step: 0.05, label: "Position x y z (metres, under its parent)" }),
      field(n, { key: "orientation", type: "vec4", step: 0.05, label: "Turn: axis x y z, then angle in radians" }),
      motionField(n, false),
    ]));
  }

  // --- the texture picker --------------------------------------------------------------------
  function openPicker() {
    const e = emitterNow();
    if (!e) return;
    $("picker").hidden = false;
    $("picker-name").value = e.texture;
    $("picker-search").value = "";
    renderPicker();
    $("picker-search").focus();
  }
  function closePicker() { $("picker").hidden = true; }
  function useTexture(name) {
    const e = emitterNow();
    const clean = core.resref(name, 16);
    if (!e || !clean) return;
    e.texture = clean;
    closePicker();
    renderEmitters();
    renderFields();
    changed(false);
  }
  function renderPicker() {
    const e = emitterNow();
    const query = $("picker-search").value.trim().toLowerCase();
    const names = (state.server ? state.server.textures : []).filter((n) => !query || n.includes(query));
    $("picker-grid").replaceChildren(...names.map((name) => {
      const box = el("span", { class: "thumb" });
      showThumb(box, name);
      return el("button", { type: "button", class: e && e.texture.toLowerCase() === name ? "current" : "", onclick: () => useTexture(name) },
        [box, document.createTextNode(name)]);
    }));
    if (!names.length) $("picker-grid").append(el("p", { class: "note", text: state.server && state.server.textures.length ? "Nothing matches." : "No textures yet: start the game once with nwn5e 0.42.0 or later." }));
  }

  // --- the stage --------------------------------------------------------------------------------
  function renderOverlay() {
    const notes = [];
    const s = state.server;
    if (s && !s.exported) notes.push("No game library yet: start the game once with nwn5e 0.42.0 or later (vfx_bench=1), then Reload.");
    const lightning = state.fx.emitters.filter((e) => String(e.update).toLowerCase() === "lightning");
    if (lightning.length) notes.push("Lightning isn't drawn here (" + lightning.map((e) => e.name).join(", ") + ") - Play in game shows it.");
    const missing = [...new Set(state.fx.emitters.map((e) => e.texture.toLowerCase()))].filter((t) => s && s.exported && !inLibrary(t));
    if (missing.length) notes.push(missing.join(", ") + (missing.length > 1 ? " aren't" : " isn't") + " in the library: a soft dot stands in.");
    $("overlay").replaceChildren(...notes.map((text) => el("li", { text })));
  }

  let last = 0;
  function tick(now) {
    const dt = Math.min(0.1, last ? (now - last) / 1000 : 0) * state.speed;
    last = now;
    if (!state.paused && state.fx && dt > 0) {
      // Once a frame with the frame's time, as the game runs its emitters (its birth count
      // depends on the frame's length).
      const length = sim.length();
      if (view) sim.eye = view.eye();
      if (sim.time < length) sim.step(dt);
      if (sim.time >= length && state.loop) sim.restart();
    }
    if (view) view.draw(sim.snapshot(), sim.figure(sim.time), sim.nodePlaces());
    $("clock").textContent = `${sim.time.toFixed(1)} / ${sim.length().toFixed(1)} s · ${sim.count()} particles`;
    requestAnimationFrame(tick);
  }

  // --- the game -----------------------------------------------------------------------------------
  function renderLink() {
    const on = !!state.link.alive;
    $("link").classList.toggle("on", on);
    $("link-text").textContent = on ? "Game: running" : "Game: not running";
  }

  async function pollLink() {
    try {
      state.link = await getJson("/api/game");
    } catch (e) {
      state.link = { alive: false, status: null };
    }
    renderLink();
    // The game copied its effects out (again) since the lists were read: read them again.
    if (state.server && state.link.library && state.link.library !== state.server.link.library) {
      const first = !state.server.exported;
      await refresh();
      if (first) setStatus(`The game's effects are here: ${state.server.game.length} in the Game list on the left.`, "good");
    }
  }

  async function request(kind) {
    let body;
    try {
      body = await postText("/api/" + kind, kind === "play" ? draftText() : "");
    } catch (e) {
      setStatus(e.message === "Failed to fetch" ? offline : "Not sent: " + e.message, "bad");
      return;
    }
    state.link = body.link;
    renderLink();
    if (!body.link.alive) {
      setStatus("The game isn't running with a character in play: start it (nwn5e 0.42.0 or later, vfx_bench=1), load a game, then try again.", "bad");
      return;
    }
    setStatus(kind === "play" ? "Sent to the game…" : "Stopping…");
    const until = Date.now() + 8000;
    while (Date.now() < until) {
      await new Promise((r) => setTimeout(r, 300));
      try {
        const link = await getJson("/api/game");
        if (link.status && link.status.serial === body.serial) {
          const good = /^(Playing|Stopped)/.test(link.status.text);
          setStatus("Game: " + link.status.text, good ? "good" : "bad");
          return;
        }
      } catch (e) { /* try again */ }
    }
    setStatus("The game didn't answer in 8 s - is it paused, loading, or at the menu?", "bad");
  }

  async function save() {
    const name = core.resref(state.fx.name, 16);
    if (!name) { setStatus("Give the effect a name first.", "bad"); return; }
    if (state.server && state.server.game.some((g) => g.model === name) &&
        !confirm(`${name} is one of the game's own models: added to the mod under that name, it would replace the game's effect everywhere it's used. Save under this name anyway?`)) return;
    state.fx.name = name;
    $("fx-name").value = name;
    const text = draftText();
    try {
      const body = await postText("/api/save", text);
      state.loadedText = text;
      state.server = body.state;
      state.current = "saved:" + name;
      renderLibrary();
      changed(false);
      setStatus(`Saved ${name} (vfxedit\\saved\\${name}.vfx${body.replaced ? ", replacing the older one" : ""}). ` +
        (state.link.alive ? "The game writes its model in a moment." : "The game writes its model when it next runs.") +
        " To make it a permanent effect, ask Claude to add it to the mod.", "good");
    } catch (e) {
      setStatus(e.message === "Failed to fetch" ? offline : "Not saved: " + e.message, "bad");
    }
  }

  // --- wiring --------------------------------------------------------------------------------------
  function wire() {
    for (const tab of ["saved", "nwn5e", "game"]) {
      const button = $("tab-" + tab);
      button.dataset.label = button.textContent;
      button.addEventListener("click", () => { state.tab = tab; renderLibrary(); changed(false); });
    }
    $("search").addEventListener("input", renderLibrary);
    $("reload").addEventListener("click", () => refresh().then(() => setStatus("Reloaded.")));
    $("new").addEventListener("click", () => {
      if (dirty() && !confirm(`Drop the unsaved changes to ${state.fx.name}?`)) return;
      state.current = null;
      setEffect(core.newEffect(), true);
      state.loadedText = "";
      changed(false);
      renderLibrary();
      setStatus("A new effect with one emitter.");
    });

    $("fx-name").addEventListener("input", (ev) => { state.fx.name = ev.target.value; changed(false); });
    $("fx-name").addEventListener("change", (ev) => {
      const clean = core.resref(ev.target.value, 16) || "n5_new";
      if (clean !== ev.target.value) setStatus(`The name is a file name: ${clean}.`);
      state.fx.name = ev.target.value = clean;
      changed(false);
    });
    $("fx-type").addEventListener("change", (ev) => { state.fx.duration = ev.target.value === "D"; renderEffect(); changed(false); sim.restart(); });
    $("fx-seconds").addEventListener("input", (ev) => {
      const v = parseFloat(ev.target.value);
      if (!Number.isNaN(v) && v > 0) { state.fx.seconds = Math.fround(v); changed(false); }
    });
    $("fx-attach").addEventListener("change", (ev) => { state.fx.attach = ev.target.value; changed(false); sim.restart(); });
    [0, 1, 2].forEach((k) => $("fx-len" + k).addEventListener("input", (ev) => {
      const v = parseFloat(ev.target.value);
      if (Number.isNaN(v) || v < 0) return;
      state.fx.lengths[k] = Math.fround(v);
      changed(false);
    }));
    $("nd-add").addEventListener("click", () => {
      const fx = state.fx;
      let name = "node";
      for (let k = 2; [...fx.nodes, ...fx.emitters].some((x) => x.name === name); k++) name = "node" + k;
      fx.nodes.push(core.node({ name }));
      state.node = fx.nodes.length - 1;
      renderNodes(); renderFields(); changed(true);
    });
    $("nd-del").addEventListener("click", () => {
      const fx = state.fx, n = fx.nodes[state.node];
      if (!n) return;
      // What hung under it hangs under its parent now.
      for (const x of [...fx.nodes, ...fx.emitters]) if (x.parent === n.name) x.parent = n.parent || "";
      fx.nodes.splice(state.node, 1);
      state.node = Math.min(state.node, fx.nodes.length - 1);
      renderNodes(); renderEmitters(); renderFields(); changed(true);
    });

    $("em-add").addEventListener("click", () => {
      state.fx.emitters.push(core.sparks());
      state.selected = state.fx.emitters.length - 1;
      sim.hidden.clear();
      renderEmitters(); renderFields(); changed(true);
    });
    $("em-copy").addEventListener("click", () => {
      const e = emitterNow();
      if (!e) return;
      const copy = core.clone(e);
      copy.name = e.name + "2";
      state.fx.emitters.splice(state.selected + 1, 0, copy);
      state.selected += 1;
      sim.hidden.clear();
      renderEmitters(); renderFields(); changed(true);
    });
    $("em-del").addEventListener("click", () => {
      if (!emitterNow()) return;
      state.fx.emitters.splice(state.selected, 1);
      state.selected = Math.max(0, Math.min(state.selected, state.fx.emitters.length - 1));
      sim.hidden.clear();
      sim.setEffect(state.fx);
      sim.restart();
      renderEmitters(); renderFields(); changed(true);
    });
    const move = (by) => {
      const i = state.selected, j = i + by;
      if (j < 0 || j >= state.fx.emitters.length) return;
      const list = state.fx.emitters;
      [list[i], list[j]] = [list[j], list[i]];
      state.selected = j;
      sim.hidden.clear();
      sim.restart();
      renderEmitters(); renderFields(); changed(true);
    };
    $("em-up").addEventListener("click", () => move(-1));
    $("em-down").addEventListener("click", () => move(1));

    $("restart").addEventListener("click", () => sim.restart());
    $("pause").addEventListener("click", () => {
      state.paused = !state.paused;
      $("pause").textContent = state.paused ? "Play" : "Pause";
    });
    $("speed").addEventListener("change", (ev) => { state.speed = parseFloat(ev.target.value) || 1; });
    $("loop").addEventListener("change", (ev) => { state.loop = ev.target.checked; });
    $("walk").addEventListener("change", (ev) => { sim.walk = ev.target.checked; });
    $("figure").addEventListener("change", (ev) => { if (view) view.showFigure = ev.target.checked; });
    $("backdrop").addEventListener("change", (ev) => { if (view) view.backdrop = BACKDROPS[ev.target.value] || BACKDROPS.dark; });

    $("play").addEventListener("click", () => request("play"));
    $("stop").addEventListener("click", () => request("stop"));
    $("save").addEventListener("click", save);

    $("draft-box").addEventListener("toggle", () => { $("draft").value = draftText(); $("draft-note").textContent = ""; });
    $("apply").addEventListener("click", () => {
      try {
        const fx = core.readDraft($("draft").value.replace(/\r?\n/g, "\n"));
        const keep = state.loadedText;
        setEffect(fx, false);
        state.loadedText = keep;
        changed(false);
        $("draft-note").textContent = `Applied: ${fx.emitters.length} emitter${fx.emitters.length === 1 ? "" : "s"}.`;
      } catch (e) {
        $("draft-note").textContent = "It doesn't read: " + e.message;
      }
    });

    $("picker-close").addEventListener("click", closePicker);
    $("picker").addEventListener("click", (ev) => { if (ev.target === $("picker")) closePicker(); });
    $("picker-search").addEventListener("input", renderPicker);
    $("picker-use").addEventListener("click", () => useTexture($("picker-name").value));
    document.addEventListener("keydown", (ev) => {
      if (ev.key === "Escape" && !$("picker").hidden) { closePicker(); return; }
      const typing = /^(INPUT|TEXTAREA|SELECT)$/.test(document.activeElement && document.activeElement.tagName);
      if (typing || ev.ctrlKey || ev.metaKey || ev.altKey) return;
      if (ev.key === " ") { ev.preventDefault(); $("pause").click(); }
      else if (ev.key === "r" || ev.key === "R") sim.restart();
    });
    window.addEventListener("beforeunload", (ev) => { if (dirty()) { ev.preventDefault(); ev.returnValue = ""; } });
  }

  function restore() {
    try {
      const kept = JSON.parse(localStorage.getItem(STORE) || "null");
      if (!kept || !kept.text) return false;
      const fx = core.readDraft(kept.text);
      setEffect(fx, false);
      state.loadedText = kept.loaded || "";
      state.selected = Math.min(kept.selected || 0, Math.max(0, fx.emitters.length - 1));
      state.tab = ["saved", "nwn5e", "game"].includes(kept.tab) ? kept.tab : state.tab;
      state.current = kept.current || null;
      renderEmitters();
      renderFields();
      changed(false);
      return true;
    } catch (e) {
      return false;
    }
  }

  async function start() {
    try {
      view = new window.VfxView.View($("canvas"), image);
    } catch (e) {
      $("no-gl").hidden = false;
    }
    wire();
    const restored = restore();
    if (!restored) setEffect(core.newEffect(), true);
    await refresh();
    if (state.server && !restored && state.server.saved.length) state.tab = "saved";
    renderLibrary();
    if (view) view.attach = state.fx.attach;
    // ?open=<saved|nwn5e|game>:<name>&warm=<seconds>: opens an effect and runs it that far
    // (for a look without waiting; the bench's own checks use it).
    const params = new URLSearchParams(location.search);
    const wanted = /^(saved|nwn5e|game):([a-z0-9_]{1,32})$/.exec(params.get("open") || "");
    if (wanted && state.server) {
      const meta = state.server.game.find((g) => g.model === wanted[2]);
      state.tab = wanted[1];
      await open({ source: wanted[1], name: wanted[2], meta, title: wanted[2] }, true);
      const warm = Math.min(30, Math.max(0, parseFloat(params.get("warm")) || 0));
      if (view) sim.eye = view.eye();
      for (let t = 0; t < warm; t += 1 / 60) sim.step(1 / 60);
      if (params.has("pause")) { state.paused = true; $("pause").textContent = "Play"; }
    }
    setInterval(pollLink, 2000);
    requestAnimationFrame(tick);
  }

  start();
})();
