"""Low hit points: a bleeding character (owner, 2026-10-24: "there's a bleeding
animation but it's too severe, can you make a couple more variations to use
when the character is in low hp?" - "the bleeding animation I meant vfx 491" -
"doesn't need to be from the head, just some mild dripping, then dripping with
a small puddle, then more bleeding" - then "just do 1 bleeding effect, mild,
with a small puddle when the PC is under 10%").

One duration effect model of nwn5e's own (ASCII), on the creature's root node,
using the game's particle textures by name (nothing of the game's copied):
  n5_bleed - mild dripping from waist height and a small puddle: blood splats on
             the ground that spread slowly and fade (row 1570).
The puddle is made the way the game's own blood effects make theirs (0.22.5,
owner: "there isn't a puddle on the floor like in 491"): sticky drops with
the game's `blood` splat texture (a 2x2 sheet, a random frame each) and the
emitter's splat flag - each falls, flattens where it lands and spreads as it
fades. They stay where they fall (world space), so a character who walks
leaves a short trail instead of dragging a puddle. low_hp_look.cpp puts it on
the player character under bleed_percent of its hit points (nwn5e.ini).

  python tools/data/bleed_vfx.py    (from the repo root)
writes data/files/n5_bleed_*.mdl and data/rows/visualeffects.bleed.txt.
"""

import os

FILES = os.path.join("data", "files")
ROWS = os.path.join("data", "rows", "visualeffects.bleed.txt")

DARK_RED = (0.42, 0.02, 0.02)
DEEP_RED = (0.25, 0.0, 0.0)
SPLAT_RED = (0.33, 0.0, 0.0)  # the game's blood splats (vwp_l_blood_red)
WAIST_M = 0.95


def drops(rate, size, area_cm):
    # The falling drips move with the character (inherit, 0.22.7 - owner: "can
    # the dripping blood follow the character model instead of staying where it
    # was spawned?"); the puddle's splats stay where they land.
    return dict(name="drops", texture="fxpa_sphere", render="Motion_Blur", birthrate=rate, life=0.55,
                velocity=0.0, randvel=0.05, spread=0.3, mass=1.8, size=(size, size * 0.8),
                color=(DARK_RED, DEEP_RED), alpha=(1.0, 0.9), area=area_cm, blur=7, height=WAIST_M,
                flags=("inherit",))


def puddle(rate, size_to, life, area_cm, velocity=0.6, name="puddle"):
    # Sticky drops: they fall from the waist, splat flat where they land and
    # spread to size_to as they fade (the size grows over the whole life). A
    # little outward speed lands them around the feet, not hidden under them.
    return dict(name=name, texture="blood", render="Normal", birthrate=rate, life=life,
                velocity=velocity, randvel=velocity, spread=1.2, mass=2.4, size=(0.02, size_to),
                color=(SPLAT_RED, SPLAT_RED), alpha=(1.0, 0.0), area=area_cm, blur=0.1, height=WAIST_M,
                flags=("bounce", "random", "splat"), grid=2, frames=3)


EFFECTS = [
    ("n5_bleed", 1570, "NWN5E_BLEED", [drops(1.5, 0.03, 18), puddle(2.0, 0.8, 6.0, 12)]),
]

# (0.22.6 had a check row, 1571: 491's own sticky settings in an ASCII model of
# ours. Both left puddles - an ASCII model's splat works - so it went in 0.22.7.)


def emitter_lines(model, e):
    (r1, g1, b1), (r2, g2, b2) = e["color"]
    flags = e.get("flags", ())
    flag = lambda name: "  %s %d" % (name, 1 if name in flags else 0)
    grid = e.get("grid", 1)
    return [
        "node emitter %s_%s" % (model, e["name"]),
        "  parent " + model,
        "  p2p 0", "  p2p_sel 1", "  affectedByWind 0", "  m_isTinted 0", flag("bounce"), flag("random"),
        flag("inherit"), "  inheritvel 0", "  inherit_local 0", flag("splat"), "  inherit_part 0",
        "  renderorder 0", "  spawntype 0",
        "  update Fountain",
        "  render " + e["render"],
        "  blend Normal",
        "  texture " + e["texture"],
        "  xgrid %d" % grid, "  ygrid %d" % grid, "  loop 0", "  deadspace 0", "  twosidedtex 0",
        "  blastRadius 0", "  blastLength 0",
        "  position 0 0 %g" % e["height"],
        "  orientation 0 0 0 0",
        "  colorStart %g %g %g" % (r1, g1, b1), "  colorEnd %g %g %g" % (r2, g2, b2),
        "  alphaStart %g" % e["alpha"][0], "  alphaEnd %g" % e["alpha"][1],
        "  sizeStart %g" % e["size"][0], "  sizeEnd %g" % e["size"][1],
        "  sizeStart_y 0", "  sizeEnd_y 0",
        "  frameStart 0", "  frameEnd %d" % e.get("frames", 0),
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
    for name in os.listdir(FILES):  # earlier versions' models (a head spray; three tiers)
        if name in ("n5_bleed_s.mdl", "n5_bleed_m.mdl", "n5_bleed_1.mdl", "n5_bleed_2.mdl", "n5_bleed_3.mdl",
                    "n5_splat_t.mdl"):
            os.remove(os.path.join(FILES, name))
    rows = [
        "# visualeffects.2da - low hit points (low_hp_look.cpp): a bleeding character, generated by",
        "# tools/data/bleed_vfx.py - mild dripping and a small puddle; a duration effect on the",
        "# root node, nwn5e's own model (data\\files\\n5_bleed.mdl).",
        "Label Type_FD OrientWithGround Imp_Root_S_Node Imp_Root_M_Node Imp_Root_L_Node Imp_Root_H_Node",
    ]
    for model, row, label, emitters in EFFECTS:
        write_model(model, emitters)
        rows.append("%d %s D 0 %s %s %s %s" % (row, label, model, model, model, model))
    with open(ROWS, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(rows) + "\n")
    print("wrote %d bleeding model(s) to %s and %s" % (len(EFFECTS), FILES, ROWS))


if __name__ == "__main__":
    main()
