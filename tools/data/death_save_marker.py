"""The death-save marker (Phase 5b, 0.20.0): a sprite over a down player
character, always facing the camera, showing its death saves - a row of three
success pips over a row of three failure pips; Stable shows three blue pips.

Writes, from nothing of the game's (nwn5e's own art and model text):
- data/files/n5_ds_<s><f>.mdl and .tga (s successes, f failures, 0-2 each) and
  n5_ds_st.mdl / .tga (Stable): ASCII effect models with one emitter
  (update Single, render Normal: one camera-facing particle that lives as long
  as the effect, like the game's vdr_seen), 1.4 m over the creature's root,
  growing in, holding, shrinking out;
- data/rows/visualeffects.txt: rows 1560-1568 (1560 + 3 x successes +
  failures) and 1569 (Stable), duration effects on the root node.

Run from the repo root: python tools/data/death_save_marker.py
"""

import math
import os
import struct

FILES = os.path.join("data", "files")
ROWS = os.path.join("data", "rows", "visualeffects.txt")
FIRST_ROW = 1560
STABLE_ROW = 1569
SIZE = 64            # texture pixels
SPRITE_M = 0.6       # sprite width in metres
HEIGHT_M = 1.4       # over the creature's root

BACK = (16, 16, 20, 150)
SUCCESS = (70, 220, 100)
FAILURE = (235, 60, 45)
STABLE = (90, 160, 255)
EMPTY = (150, 150, 150)


def coverage(d, r):
    """Anti-aliased coverage of a pixel at distance d from a circle's edge r."""
    return max(0.0, min(1.0, r - d + 0.5))


def over(dst, rgb, a):
    """Composites rgb at alpha a (0-1) over an RGBA pixel."""
    if a <= 0:
        return dst
    da = dst[3] / 255.0
    oa = a + da * (1 - a)
    if oa <= 0:
        return (0, 0, 0, 0)
    out = tuple(int(round((c * a + dc * da * (1 - a)) / oa)) for c, dc in zip(rgb, dst[:3]))
    return out + (int(round(oa * 255)),)


def texture(top, bottom):
    """top / bottom: three (rgb, filled) pips each."""
    px = [[(0, 0, 0, 0) for _ in range(SIZE)] for _ in range(SIZE)]
    # A dark rounded backing so the pips read on any ground.
    for y in range(SIZE):
        for x in range(SIZE):
            cx = min(max(x + 0.5, 10), SIZE - 10)
            cy = min(max(y + 0.5, 10), SIZE - 10)
            a = coverage(math.hypot(x + 0.5 - cx, y + 0.5 - cy), 9.5) * BACK[3] / 255.0
            px[y][x] = over(px[y][x], BACK[:3], a)
    for row_y, pips in ((21, top), (43, bottom)):
        for i, (rgb, filled) in enumerate(pips):
            cx, cy = 14 + 18 * i, row_y
            for y in range(SIZE):
                for x in range(SIZE):
                    d = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
                    if filled:
                        a = coverage(d, 7.0)
                    else:  # a ring
                        a = min(coverage(d, 7.0), 1 - coverage(d, 5.0))
                    px[y][x] = over(px[y][x], rgb, a)
    return px


def write_tga(path, px):
    # 32-bit uncompressed, bottom-up like the game's own (descriptor 0x08: 8 alpha bits).
    head = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, SIZE, SIZE, 32, 0x08)
    body = bytes(c for row in reversed(px) for (r, g, b, a) in row for c in (b, g, r, a))
    with open(path, "wb") as f:
        f.write(head + body)


def size_keys(points):
    lines = []
    for key in ("sizeStartkey", "sizeEndkey"):
        lines.append("  %s %d" % (key, len(points)))
        lines += ["    %g %g" % p for p in points]
    return lines


def write_model(name):
    s = SPRITE_M
    lines = [
        "#MAXMODEL ASCII",
        "# nwn5e (tools/data/death_save_marker.py): the death-save marker - one",
        "# camera-facing sprite over a down player character.",
        "newmodel " + name,
        "setsupermodel %s NULL" % name,
        "classification effect",
        "setanimationscale 1",
        "beginmodelgeom " + name,
        "node dummy " + name,
        "  parent NULL",
        "endnode",
        "node emitter %s_pips" % name,
        "  parent " + name,
        "  p2p 0", "  p2p_sel 1", "  affectedByWind 0", "  m_isTinted 0", "  bounce 0", "  random 0",
        "  inherit 1", "  inheritvel 0", "  inherit_local 0", "  splat 0", "  inherit_part 0",
        "  renderorder 0", "  spawntype 0",
        "  update Single",
        "  render Normal",
        "  blend Normal",
        "  texture " + name,
        "  xgrid 1", "  ygrid 1", "  loop 0", "  deadspace 0", "  twosidedtex 0",
        "  blastRadius 0", "  blastLength 0",
        "  position 0 0 %g" % HEIGHT_M,
        "  orientation 0 0 0 0",
        "  colorStart 1 1 1", "  colorEnd 1 1 1", "  alphaStart 1", "  alphaEnd 1",
        "  sizeStart 0", "  sizeEnd 0", "  sizeStart_y 0", "  sizeEnd_y 0",
        "  frameStart 0", "  frameEnd 0",
        "  birthrate 1",
        "  lifeExp -1",
        "  mass 0", "  spread 0", "  particleRot 0", "  velocity 0", "  randvel 0", "  fps 0",
        "  xsize 0", "  ysize 0", "  bounce_co 0", "  blurlength 10",
        "  lightningDelay 0", "  lightningRadius 0", "  lightningScale 0",
        "  p2p_bezier2 0", "  p2p_bezier3 0", "  combinetime 0", "  drag 0", "  grav 0", "  threshold 0",
        "endnode",
        "endmodelgeom " + name,
    ]
    for anim, length, points in (("impact", 0.3, [(0, 0), (0.2, s), (0.3, s)]),
                                 ("duration", 1.0, [(0, s), (1.0, s)]),
                                 ("cessation", 0.2, [(0, s), (0.2, 0)])):
        lines += [
            "",
            "newanim %s %s" % (anim, name),
            "  length %g" % length,
            "  transtime 0.25",
            "  animroot " + name,
            "node dummy " + name,
            "  parent NULL",
            "endnode",
            "node emitter %s_pips" % name,
            "  parent " + name,
        ] + size_keys(points) + [
            "endnode",
            "doneanim %s %s" % (anim, name),
        ]
    lines += ["", "donemodel " + name, ""]
    with open(os.path.join(FILES, name + ".mdl"), "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(lines))


def main():
    rows = [
        "# visualeffects.2da - Phase 5b (0.20.0): the death-save marker over a down player",
        "# character (rules_dying.cpp), generated by tools/data/death_save_marker.py: rows",
        "# 1560-1568 = 1560 + 3 x successes + failures, 1569 = Stable; nwn5e's own models",
        "# and art (data\\files\\n5_ds_*), on the creature's root node, all sizes.",
        "Label Type_FD OrientWithGround Imp_Root_S_Node Imp_Root_M_Node Imp_Root_L_Node Imp_Root_H_Node OrientWithObject",
    ]
    states = [(s, f) for s in range(3) for f in range(3)] + [None]
    for state in states:
        if state is None:
            name, row, label = "n5_ds_st", STABLE_ROW, "NWN5E_DEATH_SAVES_STABLE"
            top = [(STABLE, True)] * 3
            bottom = [(EMPTY, False)] * 3
        else:
            s, f = state
            name, row = "n5_ds_%d%d" % (s, f), FIRST_ROW + 3 * s + f
            label = "NWN5E_DEATH_SAVES_%d_%d" % (s, f)
            top = [(SUCCESS, True)] * s + [(EMPTY, False)] * (3 - s)
            bottom = [(FAILURE, True)] * f + [(EMPTY, False)] * (3 - f)
        write_tga(os.path.join(FILES, name + ".tga"), texture(top, bottom))
        write_model(name)
        rows.append("%d %s D 0 %s %s %s %s 0" % (row, label, name, name, name, name))
    with open(ROWS, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(rows) + "\n")
    print("wrote %d marker models and textures, %s" % (len(states), ROWS))


if __name__ == "__main__":
    main()
