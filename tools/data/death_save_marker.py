"""The death-save marker (Phase 5b): a sprite over a down player character,
always facing the camera, showing its death saves.

Layout (owner, 2026-10-24: "three thin vertical segments on each side, green
or blue on one side and red on the other ... icon in the middle"): an icon in
the middle, three thin bars on its left for successes (green; all blue when
Stable) and three on its right for failures (red), filled from the icon out.
The owner's icon is the Pale Master's Bone Skin (ife_x2bonesk1: a face on the
left half, a skull on the right - so successes by the face, failures by the
skull).

Two sprites in the same spot, so nothing of the game's goes in the repo (owner,
2026-10-24): the bars are nwn5e's own texture (n5_ds_*.tga, the middle left
clear), and the icon is a second, smaller sprite whose texture is the game's
own ife_x2bonesk1 by name - the engine loads it from the player's install, as
it is (its own dark square, not faded out).

  python tools/data/death_save_marker.py
      models, bar textures and rows, in data/files and data/rows.

The models: ASCII effect models with two emitters (update Single, render
Normal: one camera-facing particle each that lives as long as the effect, like
the game's vdr_seen), 1.4 m over the creature's root, growing in, holding,
shrinking out. Rows: visualeffects 1560-1568 (1560 + 3 x successes +
failures) and 1569 (Stable), duration effects on the root node.

Run from the repo root.
"""

import os
import struct

FILES = os.path.join("data", "files")
ROWS = os.path.join("data", "rows", "visualeffects.txt")
FIRST_ROW = 1560
STABLE_ROW = 1569
SIZE = 128           # bar texture pixels
ICON = 64            # the icon's slot in the bar texture (a 32 px icon at 2x)
ICON_TEXTURE = "ife_x2bonesk1"  # the game's own, loaded from the player's install
SPRITE_M = 0.8       # bar sprite width in metres
ICON_M = SPRITE_M * ICON / SIZE  # icon sprite width: exactly its slot
HEIGHT_M = 1.4       # over the creature's root

SUCCESS = (70, 220, 100)
STABLE = (90, 160, 255)
FAILURE = (235, 60, 45)
EMPTY = (55, 55, 62)
OUTLINE = (0, 0, 0, 210)
BAR_W, BAR_H, BAR_GAP = 6, 40, 4


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


def bar(px, x0, rgb):
    y0 = (SIZE - BAR_H) // 2
    for y in range(y0 - 1, y0 + BAR_H + 1):
        for x in range(x0 - 1, x0 + BAR_W + 1):
            px[y][x] = over(px[y][x], OUTLINE[:3], OUTLINE[3] / 255.0)
    for y in range(y0, y0 + BAR_H):
        for x in range(x0, x0 + BAR_W):
            px[y][x] = (rgb[0], rgb[1], rgb[2], 255)


def texture(successes, failures, stable):
    """The bars only; the middle stays clear for the icon sprite."""
    px = [[(0, 0, 0, 0) for _ in range(SIZE)] for _ in range(SIZE)]
    o = (SIZE - ICON) // 2
    for i in range(3):  # from the icon out
        left = o - (BAR_GAP + BAR_W) * (i + 1)
        right = o + ICON + BAR_GAP + (BAR_GAP + BAR_W) * i
        bar(px, left, STABLE if stable else (SUCCESS if i < successes else EMPTY))
        bar(px, right, FAILURE if i < failures else EMPTY)
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


def emitter(node, parent, texture_name, renderorder):
    return [
        "node emitter " + node,
        "  parent " + parent,
        "  p2p 0", "  p2p_sel 1", "  affectedByWind 0", "  m_isTinted 0", "  bounce 0", "  random 0",
        "  inherit 1", "  inheritvel 0", "  inherit_local 0", "  splat 0", "  inherit_part 0",
        "  renderorder %d" % renderorder, "  spawntype 0",
        "  update Single",
        "  render Normal",
        "  blend Normal",
        "  texture " + texture_name,
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
    ]


def write_model(name):
    # (node, texture, width, renderorder): the icon draws after the bars.
    sprites = [(name + "_pips", name, SPRITE_M, 0), (name + "_icon", ICON_TEXTURE, ICON_M, 1)]
    lines = [
        "#MAXMODEL ASCII",
        "# nwn5e (tools/data/death_save_marker.py): the death-save marker - two",
        "# camera-facing sprites over a down player character: nwn5e's bars and the",
        "# game's own %s icon (by name, from the player's install)." % ICON_TEXTURE,
        "newmodel " + name,
        "setsupermodel %s NULL" % name,
        "classification effect",
        "setanimationscale 1",
        "beginmodelgeom " + name,
        "node dummy " + name,
        "  parent NULL",
        "endnode",
    ]
    for node, tex, _, order in sprites:
        lines += emitter(node, name, tex, order)
    lines.append("endmodelgeom " + name)
    for anim, length, keys in (("impact", 0.3, [(0, 0), (0.2, 1), (0.3, 1)]),
                               ("duration", 1.0, [(0, 1), (1.0, 1)]),
                               ("cessation", 0.2, [(0, 1), (0.2, 0)])):
        lines += [
            "",
            "newanim %s %s" % (anim, name),
            "  length %g" % length,
            "  transtime 0.25",
            "  animroot " + name,
            "node dummy " + name,
            "  parent NULL",
            "endnode",
        ]
        for node, _, width, _ in sprites:
            lines += ["node emitter " + node, "  parent " + name]
            lines += size_keys([(t, width * k) for t, k in keys])
            lines.append("endnode")
        lines.append("doneanim %s %s" % (anim, name))
    lines += ["", "donemodel " + name, ""]
    with open(os.path.join(FILES, name + ".mdl"), "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(lines))


def main():
    rows = [
        "# visualeffects.2da - Phase 5b: the death-save marker over a down player character",
        "# (rules_dying.cpp), generated by tools/data/death_save_marker.py: rows 1560-1568 =",
        "# 1560 + 3 x successes + failures, 1569 = Stable; nwn5e's own models and bar art",
        "# (data\\files\\n5_ds_*) plus the game's ife_x2bonesk1 icon by name, on the root node.",
        "Label Type_FD OrientWithGround Imp_Root_S_Node Imp_Root_M_Node Imp_Root_L_Node Imp_Root_H_Node OrientWithObject",
    ]
    states = [(s, f) for s in range(3) for f in range(3)] + [None]
    for state in states:
        if state is None:
            name, row, label, s, f, stable = "n5_ds_st", STABLE_ROW, "NWN5E_DEATH_SAVES_STABLE", 3, 0, True
        else:
            s, f = state
            name, row, stable = "n5_ds_%d%d" % (s, f), FIRST_ROW + 3 * s + f, False
            label = "NWN5E_DEATH_SAVES_%d_%d" % (s, f)
        write_tga(os.path.join(FILES, name + ".tga"), texture(s, f, stable))
        write_model(name)
        rows.append("%d %s D 0 %s %s %s %s 0" % (row, label, name, name, name, name))
    with open(ROWS, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(rows) + "\n")
    print("wrote %d marker models and bar textures to %s, and %s" % (len(states), FILES, ROWS))


if __name__ == "__main__":
    main()
