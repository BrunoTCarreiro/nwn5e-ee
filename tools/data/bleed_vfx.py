"""Low hit points: a bleeding character (owner, 2026-10-24: "there's a bleeding
animation but it's too severe, can you make a couple more variations to use
when the character is in low hp?" - "the bleeding animation I meant vfx 491" -
"doesn't need to be from the head, just some mild dripping, then dripping with
a small puddle, then more bleeding").

Three duration effect models of nwn5e's own (ASCII), on the creature's root
node, using the game's particle textures by name (nothing of the game's copied):
  n5_bleed_1 - mild dripping: a drop now and then falls from waist height (row 1570);
  n5_bleed_2 - dripping and a small puddle: more drops, and blood splats on the
               ground that spread slowly and fade (row 1571);
  n5_bleed_3 - more bleeding: a steady drip and a bigger puddle (row 1572).
The splats stay where they fall (world space), so a character who walks leaves
a short trail instead of dragging a puddle. low_hp_look.cpp puts one on the
player character by its hit points (nwn5e.ini: low_hp_bleed, bleed_*_percent).

  python tools/data/bleed_vfx.py    (from the repo root)
writes data/files/n5_bleed_*.mdl and data/rows/visualeffects.bleed.txt.
"""

import os

FILES = os.path.join("data", "files")
ROWS = os.path.join("data", "rows", "visualeffects.bleed.txt")

DARK_RED = (0.42, 0.02, 0.02)
DEEP_RED = (0.25, 0.0, 0.0)
WAIST_M = 0.95


def drops(rate, size, area_cm):
    return dict(name="drops", texture="fxpa_sphere", render="Motion_Blur", birthrate=rate, life=0.55,
                velocity=0.0, randvel=0.05, spread=0.3, mass=1.8, size=(size, size * 0.8),
                color=(DARK_RED, DEEP_RED), alpha=(1.0, 0.9), area=area_cm, blur=7, height=WAIST_M)


def puddle(rate, size_from, size_to, life, area_cm):
    # Flat on the ground, spreading and fading where it fell.
    return dict(name="puddle", texture="blood", render="Aligned_to_World_z", birthrate=rate, life=life,
                velocity=0.0, randvel=0.0, spread=0.0, mass=0.0, size=(size_from, size_to),
                color=(DEEP_RED, DEEP_RED), alpha=(0.85, 0.0), area=area_cm, blur=0, height=0.02)


EFFECTS = [
    ("n5_bleed_1", 1570, "NWN5E_BLEED_MILD", [drops(1.2, 0.03, 18)]),
    ("n5_bleed_2", 1571, "NWN5E_BLEED_PUDDLE", [drops(2.5, 0.035, 20), puddle(0.5, 0.12, 0.3, 6.0, 15)]),
    ("n5_bleed_3", 1572, "NWN5E_BLEED_HEAVY", [drops(7.0, 0.04, 24), puddle(1.0, 0.2, 0.45, 7.0, 20)]),
]


def emitter_lines(model, e):
    (r1, g1, b1), (r2, g2, b2) = e["color"]
    return [
        "node emitter %s_%s" % (model, e["name"]),
        "  parent " + model,
        "  p2p 0", "  p2p_sel 1", "  affectedByWind 0", "  m_isTinted 0", "  bounce 0", "  random 0",
        "  inherit 0", "  inheritvel 0", "  inherit_local 0", "  splat 0", "  inherit_part 0",
        "  renderorder 0", "  spawntype 0",
        "  update Fountain",
        "  render " + e["render"],
        "  blend Normal",
        "  texture " + e["texture"],
        "  xgrid 1", "  ygrid 1", "  loop 0", "  deadspace 0", "  twosidedtex 0",
        "  blastRadius 0", "  blastLength 0",
        "  position 0 0 %g" % e["height"],
        "  orientation 0 0 0 0",
        "  colorStart %g %g %g" % (r1, g1, b1), "  colorEnd %g %g %g" % (r2, g2, b2),
        "  alphaStart %g" % e["alpha"][0], "  alphaEnd %g" % e["alpha"][1],
        "  sizeStart %g" % e["size"][0], "  sizeEnd %g" % e["size"][1],
        "  sizeStart_y 0", "  sizeEnd_y 0",
        "  frameStart 0", "  frameEnd 0",
        "  birthrate %g" % e["birthrate"],
        "  lifeExp %g" % e["life"],
        "  mass %g" % e["mass"], "  spread %g" % e["spread"], "  particleRot 0",
        "  velocity %g" % e["velocity"], "  randvel %g" % e["randvel"], "  fps 0",
        "  xsize %g" % e["area"], "  ysize %g" % e["area"], "  bounce_co 0", "  blurlength %g" % e["blur"],
        "  lightningDelay 0", "  lightningRadius 0", "  lightningScale 0",
        "  p2p_bezier2 0", "  p2p_bezier3 0", "  combinetime 0", "  drag 0", "  grav 0", "  threshold 0",
        "endnode",
    ]


def anim(model, name, length, emitters, birthrate_keys):
    lines = [
        "",
        "newanim %s %s" % (name, model),
        "  length %g" % length,
        "  transtime 0.25",
        "  animroot " + model,
        "node dummy " + model,
        "  parent NULL",
        "endnode",
    ]
    for e in emitters:
        lines += ["node emitter %s_%s" % (model, e["name"]), "  parent " + model]
        keys = birthrate_keys(e)
        if keys:
            lines.append("  birthratekey %d" % len(keys))
            lines += ["    %g %g" % k for k in keys]
        lines.append("endnode")
    lines.append("doneanim %s %s" % (name, model))
    return lines


def write_model(model, emitters):
    lines = [
        "#MAXMODEL ASCII",
        "# nwn5e (tools/data/bleed_vfx.py): low hit points - a character bleeding;",
        "# the game's textures by name.",
        "newmodel " + model,
        "setsupermodel %s NULL" % model,
        "classification effect",
        "setanimationscale 1",
        "beginmodelgeom " + model,
        "node dummy " + model,
        "  parent NULL",
        "endnode",
    ]
    for e in emitters:
        lines += emitter_lines(model, e)
    lines.append("endmodelgeom " + model)
    lines += anim(model, "impact", 0.2, emitters, lambda e: None)
    lines += anim(model, "duration", 2.0, emitters, lambda e: None)
    lines += anim(model, "cessation", 1.0, emitters, lambda e: [(0, 0)])  # stops; what fell fades out
    lines += ["", "donemodel " + model, ""]
    with open(os.path.join(FILES, model + ".mdl"), "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(lines))


def main():
    for name in os.listdir(FILES):  # the first version's head-spray models
        if name in ("n5_bleed_s.mdl", "n5_bleed_m.mdl"):
            os.remove(os.path.join(FILES, name))
    rows = [
        "# visualeffects.2da - low hit points (low_hp_look.cpp): a bleeding character, generated by",
        "# tools/data/bleed_vfx.py - mild dripping, dripping and a small puddle, more bleeding;",
        "# duration effects on the root node, nwn5e's own models (data\\files\\n5_bleed_*).",
        "Label Type_FD OrientWithGround Imp_Root_S_Node Imp_Root_M_Node Imp_Root_L_Node Imp_Root_H_Node",
    ]
    for model, row, label, emitters in EFFECTS:
        write_model(model, emitters)
        rows.append("%d %s D 0 %s %s %s %s" % (row, label, model, model, model, model))
    with open(ROWS, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(rows) + "\n")
    print("wrote %d bleeding models to %s and %s" % (len(EFFECTS), FILES, ROWS))


if __name__ == "__main__":
    main()
