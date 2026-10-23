"""The death-save marker (Phase 5b): a sprite over a down player character,
always facing the camera, showing its death saves.

Layout (owner, 2026-10-24: "three thin vertical segments on each side, green
or blue on one side and red on the other ... icon in the middle"): an icon in
the middle, three thin bars on its left for successes (green; all blue when
Stable) and three on its right for failures (red), filled from the icon out.
The owner's icon is the Pale Master's Bone Skin (ife_x2bonesk1: a face on the
left half, a skull on the right - so successes by the face, failures by the
skull). It is the game's art, so it goes in the private layer only:

  python tools/data/death_save_marker.py
      the public build: models, rows and textures with a plain emblem in the
      middle (nwn5e's own art), in data/files and data/rows;
  python tools/data/death_save_marker.py --center <icon.tga> --out private/data/files
      textures only, with that icon in the middle (its near-black background
      faded out); served after data/files, they replace the public ones.

The models: ASCII effect models with one emitter (update Single, render
Normal: one camera-facing particle that lives as long as the effect, like the
game's vdr_seen), 1.4 m over the creature's root, growing in, holding,
shrinking out. Rows: visualeffects 1560-1568 (1560 + 3 x successes +
failures) and 1569 (Stable), duration effects on the root node.

Run from the repo root.
"""

import argparse
import math
import os
import struct

FILES = os.path.join("data", "files")
ROWS = os.path.join("data", "rows", "visualeffects.txt")
FIRST_ROW = 1560
STABLE_ROW = 1569
SIZE = 128           # texture pixels
ICON = 64            # the icon's size in the texture (a 32 px icon at 2x)
SPRITE_M = 0.8       # sprite width in metres
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


def read_tga(path):
    """An uncompressed or RLE true-colour TGA as rows of RGBA, top row first."""
    b = open(path, "rb").read()
    idlen, cmap, itype = b[0], b[1], b[2]
    w, h, bpp, desc = struct.unpack_from("<HHBB", b, 12)
    assert not cmap and itype in (2, 10) and bpp in (24, 32), "a true-colour TGA"
    px, data, raw = bpp // 8, b[18 + idlen:], bytearray()
    if itype == 2:
        raw = data[:w * h * px]
    else:
        i = 0
        while len(raw) < w * h * px:
            c, i = data[i], i + 1
            n = (c & 0x7F) + 1
            if c & 0x80:
                raw, i = raw + data[i:i + px] * n, i + px
            else:
                raw, i = raw + data[i:i + px * n], i + px * n
    rows = [[(raw[(y * w + x) * px + 2], raw[(y * w + x) * px + 1], raw[(y * w + x) * px],
              raw[(y * w + x) * px + 3] if px == 4 else 255) for x in range(w)] for y in range(h)]
    if not desc & 0x20:
        rows.reverse()
    return w, h, rows


def icon_pixels(path):
    """The icon at ICON x ICON (nearest), its near-black background faded out."""
    w, h, rows = read_tga(path)
    out = []
    for y in range(ICON):
        row = []
        for x in range(ICON):
            r, g, b, a = rows[y * h // ICON][x * w // ICON]
            lum = max(r, g, b)
            fade = min(1.0, max(0.0, (lum - 12) / 40.0))  # black -> clear, soft over 12-52
            row.append((r, g, b, int(a * fade)))
        out.append(row)
    return out


def emblem_pixels():
    """The public build's middle: a pale disc, nwn5e's own."""
    out = []
    c, r = ICON / 2.0, ICON * 0.36
    for y in range(ICON):
        row = []
        for x in range(ICON):
            d = math.hypot(x + 0.5 - c, y + 0.5 - c)
            a = max(0.0, min(1.0, r - d + 0.5))
            edge = max(0.0, min(1.0, d - (r - 3) + 0.5))
            shade = int(200 - 90 * edge)
            row.append((shade, shade, shade + 10, int(230 * a)))
        out.append(row)
    return out


def bar(px, x0, rgb):
    y0 = (SIZE - BAR_H) // 2
    for y in range(y0 - 1, y0 + BAR_H + 1):
        for x in range(x0 - 1, x0 + BAR_W + 1):
            px[y][x] = over(px[y][x], OUTLINE[:3], OUTLINE[3] / 255.0)
    for y in range(y0, y0 + BAR_H):
        for x in range(x0, x0 + BAR_W):
            px[y][x] = (rgb[0], rgb[1], rgb[2], 255)


def texture(middle, successes, failures, stable):
    px = [[(0, 0, 0, 0) for _ in range(SIZE)] for _ in range(SIZE)]
    o = (SIZE - ICON) // 2
    for y in range(ICON):
        for x in range(ICON):
            r, g, b, a = middle[y][x]
            px[o + y][o + x] = over(px[o + y][o + x], (r, g, b), a / 255.0)
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
    parser = argparse.ArgumentParser()
    parser.add_argument("--center", help="a TGA icon for the middle (textures only; the private layer)")
    parser.add_argument("--out", default=FILES, help="where the textures go")
    args = parser.parse_args()
    middle = icon_pixels(args.center) if args.center else emblem_pixels()
    rows = [
        "# visualeffects.2da - Phase 5b: the death-save marker over a down player character",
        "# (rules_dying.cpp), generated by tools/data/death_save_marker.py: rows 1560-1568 =",
        "# 1560 + 3 x successes + failures, 1569 = Stable; nwn5e's own models and art",
        "# (data\\files\\n5_ds_*; the private layer's textures replace them), on the root node.",
        "Label Type_FD OrientWithGround Imp_Root_S_Node Imp_Root_M_Node Imp_Root_L_Node Imp_Root_H_Node OrientWithObject",
    ]
    states = [(s, f) for s in range(3) for f in range(3)] + [None]
    os.makedirs(args.out, exist_ok=True)
    for state in states:
        if state is None:
            name, row, label, s, f, stable = "n5_ds_st", STABLE_ROW, "NWN5E_DEATH_SAVES_STABLE", 3, 0, True
        else:
            s, f = state
            name, row, stable = "n5_ds_%d%d" % (s, f), FIRST_ROW + 3 * s + f, False
            label = "NWN5E_DEATH_SAVES_%d_%d" % (s, f)
        write_tga(os.path.join(args.out, name + ".tga"), texture(middle, s, f, stable))
        if not args.center:
            write_model(name)
            rows.append("%d %s D 0 %s %s %s %s 0" % (row, label, name, name, name, name))
    if not args.center:
        with open(ROWS, "w", encoding="ascii", newline="\n") as f:
            f.write("\n".join(rows) + "\n")
    print("wrote %d marker textures to %s%s" % (len(states), args.out,
                                                "" if args.center else ", their models and " + ROWS))


if __name__ == "__main__":
    main()
