"""The stealth rings: how far a hostile creature sees and hears.

Owner, 2026-10-25: "next up we'll add the range indicators". While a player
character hides, the nearest hostiles get two rings on the ground
(stealth_watch.cpp): a solid amber one at the distance they see and a dashed
blue one at the distance they hear - each creature's own distances (the engine's
+0x7b0 sight / +0x7b8 hearing, in combat +0x7b4 / +0x7bc; ranges.2da: 10 / 20 /
35 m sight, 10 - 20 m hearing), shown by the ring nearest to it.

  python tools/data/stealth_rings.py
      models and textures in data/files, rows in data/rows/visualeffects.rings.txt.

The models: a flat ring (a trimesh band lying 5-6 cm over the ground, self-lit,
its texture's alpha softening the edges; the hearing one dashed), one model per
radius (the band keeps its width at every size), on the creature's root, tilted
to the ground under it (OrientWithGround). nwn5e's own art; nothing of the game's.
Rows: visualeffects 1573-1582 sight, 1583-1592 hearing, in RADII's order.

Run from the repo root.
"""

import math
import os
import struct

FILES = os.path.join("data", "files")
ROWS = os.path.join("data", "rows", "visualeffects.rings.txt")
RADII = [5, 10, 15, 20, 25, 28, 30, 35, 40, 50]  # metres; stealth_watch.cpp has the same list
SIGHT_ROW, HEAR_ROW = 1573, 1583
KINDS = [
    # (prefix, first row, label, texture, band width m, height m, color, dashed)
    ("n5_rs", SIGHT_ROW, "NWN5E_RING_SIGHT", "n5_ring_s", 0.22, 0.06, (255, 186, 60), False),
    ("n5_rh", HEAR_ROW, "NWN5E_RING_HEAR", "n5_ring_h", 0.16, 0.05, (110, 185, 255), True),
]
TEX_U, TEX_V = 64, 16
DASH_M = 1.6  # one dash + gap along the hearing ring


def write_tga(path, width, height, pixels):
    """Uncompressed 32-bit TGA, bottom-left origin; pixels[y][x] = (r, g, b, a)."""
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, width, height, 32, 8)
    body = bytearray()
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[y][x]
            body += bytes((b, g, r, a))
    with open(path, "wb") as f:
        f.write(header + bytes(body))


def texture(color, dashed):
    pixels = []
    for y in range(TEX_V):
        v = (y + 0.5) / TEX_V
        across = 1.0 - abs(v - 0.5) * 2.0          # 1 in the middle of the band, 0 at its edges
        edge = min(1.0, across * 2.2)               # soft edges
        row = []
        for x in range(TEX_U):
            u = (x + 0.5) / TEX_U
            along = 1.0
            if dashed:
                d = abs(((u * 2.0) % 1.0) - 0.5) * 2.0  # two dashes per repeat
                along = max(0.0, min(1.0, (0.62 - d) * 6.0))
            a = int(round(235 * edge * along))
            row.append(color + (a,))
        pixels.append(row)
    return pixels


def write_model(name, texture_name, radius, width, height):
    segments = max(48, min(160, int(radius * 2 * math.pi / 1.0)))
    inner, outer = radius - width / 2, radius + width / 2
    verts, tverts, faces = [], [], []
    circumference = 2 * math.pi * radius
    repeats = max(1, round(circumference / DASH_M))
    for i in range(segments + 1):
        angle = 2 * math.pi * i / segments
        c, s = math.cos(angle), math.sin(angle)
        verts.append((inner * c, inner * s, 0.0))
        verts.append((outer * c, outer * s, 0.0))
        u = repeats * i / segments
        tverts.append((u, 0.0))
        tverts.append((u, 1.0))
    for i in range(segments):
        a, b, c, d = 2 * i, 2 * i + 1, 2 * i + 2, 2 * i + 3  # inner i, outer i, inner i+1, outer i+1
        faces.append((a, b, d))  # counter-clockwise seen from above: facing up
        faces.append((a, d, c))
    lines = [
        "#MAXMODEL ASCII",
        "# nwn5e (tools/data/stealth_rings.py): a %g m ring on the ground - how far a" % radius,
        "# hostile creature %s (stealth_watch.cpp). nwn5e's own art." % ("hears" if "rh" in name else "sees"),
        "newmodel " + name,
        "setsupermodel %s NULL" % name,
        "classification effect",
        "setanimationscale 1",
        "beginmodelgeom " + name,
        "node dummy " + name,
        "  parent NULL",
        "endnode",
        "node trimesh " + name + "_band",
        "  parent " + name,
        "  ambient 1 1 1", "  diffuse 1 1 1", "  specular 0 0 0", "  shininess 1",
        "  bitmap " + texture_name,
        "  render 1", "  shadow 0", "  beaming 0", "  inheritcolor 0", "  alpha 1",
        "  transparencyhint 1", "  selfillumcolor 1 1 1", "  tilefade 0", "  rotatetexture 0",
        "  position 0 0 %g" % height,
        "  orientation 0 0 0 0",
        "  verts %d" % len(verts),
    ]
    lines += ["    %.4f %.4f %.4f" % v for v in verts]
    lines.append("  faces %d" % len(faces))
    lines += ["    %d %d %d 1 %d %d %d 0" % (f[0], f[1], f[2], f[0], f[1], f[2]) for f in faces]
    lines.append("  tverts %d" % len(tverts))
    lines += ["    %.4f %.4f 0" % t for t in tverts]
    lines += ["endnode", "endmodelgeom " + name]
    for anim, length in (("impact", 0.1), ("duration", 1.0), ("cessation", 0.1)):
        lines += ["", "newanim %s %s" % (anim, name), "  length %g" % length, "  transtime 0.25",
                  "  animroot " + name, "node dummy " + name, "  parent NULL", "endnode",
                  "doneanim %s %s" % (anim, name)]
    lines += ["", "donemodel " + name, ""]
    with open(os.path.join(FILES, name + ".mdl"), "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(lines))


def main():
    rows = [
        "# visualeffects.2da - the stealth rings (stealth_watch.cpp), generated by",
        "# tools/data/stealth_rings.py: a ring on the ground at how far a hostile creature",
        "# sees (1573-1582: amber) and hears (1583-1592: blue, dashed), radii %s m;" % "/".join(map(str, RADII)),
        "# nwn5e's own models and textures (data\\files\\n5_rs*, n5_rh*, n5_ring_*), on the root.",
        "Label Type_FD OrientWithGround Imp_Root_S_Node Imp_Root_M_Node Imp_Root_L_Node Imp_Root_H_Node OrientWithObject",
    ]
    for prefix, first, label, tex, width, height, color, dashed in KINDS:
        write_tga(os.path.join(FILES, tex + ".tga"), TEX_U, TEX_V, texture(color, dashed))
        for i, radius in enumerate(RADII):
            name = "%s%d" % (prefix, radius)
            write_model(name, tex, radius, width, height)
            rows.append("%d %s_%d D 1 %s %s %s %s 0" % (first + i, label, radius, name, name, name, name))
    with open(ROWS, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(rows) + "\n")
    print("wrote %d ring models, 2 textures to %s, and %s" % (len(RADII) * len(KINDS), FILES, ROWS))


if __name__ == "__main__":
    main()
