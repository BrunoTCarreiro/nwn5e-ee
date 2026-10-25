"""The stealth rings and cones: how far a hostile creature sees and hears.

Owner, 2026-10-25: "next up we'll add the range indicators", then vision cones
("Go with option 2"). While a player character hides, the nearest hostiles show
(stealth_watch.cpp) a dashed blue ring at the distance they hear and, at the
distance they see, an amber vision cone when they are calm (filled, translucent,
centred where they face, turning with them - owner: "Vision cones should be 90
degrees, and they should be filled, not outlined") or a full amber ring when
they search (they see all round) - each creature's own distances (the
engine's +0x7b0 sight / +0x7b8 hearing, in combat +0x7b4 / +0x7bc; ranges.2da:
10 / 20 / 35 m sight, 10 - 20 m hearing), shown by the nearest model.

  python tools/data/stealth_rings.py
      models and textures in data/files, rows in data/rows/visualeffects.rings.txt.

The models: flat bands (trimeshes 5-6 cm over the ground, self-lit, the
texture's alpha softening the edges; the hearing ring dashed), one model per
radius (the band keeps its width at every size) and, for cones, per width;
on the creature's root, tilted to the ground under it (OrientWithGround), the
cones turned with its facing (OrientWithObject; the creature faces +Y). nwn5e's
own art; nothing of the game's. Rows: 1573-1582 sight rings, 1583-1592 hearing
rings (RADII's order); 1610 + 10 x CONE_ANGLES index + RADII index: cones.

Light zones (owner: "Light sources should have a circle indicator so the player
can see where they take effect"): around a lit light source, a pale ring where
its bright light ends (solid) and one where its dim light ends (dashed) - the
distances stealth_watch.cpp's light rule uses (a lit placeable: 4 / 9 m; a light
effect of radius r: r/2 / r). Rows 1820 + 8 x kind (0 bright, 1 dim) + LIGHT_RADII
index.

Run from the repo root.
"""

import math
import os
import struct

FILES = os.path.join("data", "files")
ROWS = os.path.join("data", "rows", "visualeffects.rings.txt")
RADII = [5, 10, 15, 20, 25, 28, 30, 35, 40, 50]  # metres; stealth_watch.cpp has the same lists
CONE_ANGLES = [60, 90, 120, 150]                 # degrees (vision_cone: 90)
SIGHT_ROW, HEAR_ROW, CONE_ROW = 1573, 1583, 1610
KINDS = [
    # (prefix, first row, label, texture, band width m, height m, color, dashed)
    ("n5_rs", SIGHT_ROW, "NWN5E_RING_SIGHT", "n5_ring_s", 0.22, 0.06, (255, 186, 60), False),
    ("n5_rh", HEAR_ROW, "NWN5E_RING_HEAR", "n5_ring_h", 0.16, 0.05, (110, 185, 255), True),
]
LIGHT_RADII = [2.5, 4, 5, 7.5, 9, 10, 15, 20]   # stealth_watch.cpp has the same list
LIGHT_ROW = 1820
LIGHT_KINDS = [
    # (prefix, texture, band width m, height m, color, dashed)
    ("n5_lb", "n5_lring_b", 0.14, 0.04, (255, 236, 170), False),
    ("n5_ld", "n5_lring_d", 0.12, 0.035, (255, 236, 170), True),
]
TEX_U, TEX_V = 64, 16
DASH_M = 1.6        # one dash + gap along the hearing ring
CONE_INNER_M = 0.5  # the cone starts this far out (clear of the creature's feet)
# The cone's colours (owner: "from 50% alpha red for very likely to 20% alpha yellow
# for very unlikely"): 0 yellow ... 4 red, as stealth_watch.cpp's ColourOf.
CONE_COLOURS = [((255, 222, 60), 0.20), ((255, 176, 48), 0.275), ((250, 128, 40), 0.35),
                ((238, 78, 32), 0.425), ((225, 36, 28), 0.50)]


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


def cone_texture(color, alpha):
    """The filled cone: u across (side to side), v out (from the creature to the end).
    `alpha` over most of it, a little lighter near the creature, soft at the sides
    and the far end."""
    pixels = []
    for y in range(TEX_U):
        v = (y + 0.5) / TEX_U
        out = 255 * alpha * (0.75 + 0.25 * min(1.0, v / 0.3))
        if v > 0.92:
            out *= max(0.0, (1.0 - v) / 0.08)
        row = []
        for x in range(TEX_U):
            u = (x + 0.5) / TEX_U
            side = min(1.0, min(u, 1.0 - u) / 0.03)
            row.append(color + (int(round(out * side)),))
        pixels.append(row)
    return pixels


class Mesh:
    """Bands as triangles, every face turned to look up (+Z)."""

    def __init__(self):
        self.verts, self.tverts, self.faces = [], [], []

    def vert(self, x, y, u, v):
        self.verts.append((x, y, 0.0))
        self.tverts.append((u, v))
        return len(self.verts) - 1

    def tri(self, a, b, c):
        (ax, ay, _), (bx, by, _), (cx, cy, _) = self.verts[a], self.verts[b], self.verts[c]
        if (bx - ax) * (cy - ay) - (by - ay) * (cx - ax) < 0:
            b, c = c, b
        self.faces.append((a, b, c))

    def arc(self, radius, width, start, end, repeats_per_m=None):
        """A band along a circle from angle `start` to `end` (radians, from +X)."""
        inner, outer = radius - width / 2, radius + width / 2
        length = radius * (end - start)
        segments = max(8, min(160, int(outer * (end - start) / 1.0)))  # ~1 m along the outer edge
        repeats = max(1, round(length / DASH_M)) if repeats_per_m else 1
        prev = None
        for i in range(segments + 1):
            a = start + (end - start) * i / segments
            c, s = math.cos(a), math.sin(a)
            u = repeats * i / segments
            pair = (self.vert(inner * c, inner * s, u, 0.0), self.vert(outer * c, outer * s, u, 1.0))
            if prev:
                self.tri(prev[0], prev[1], pair[1])
                self.tri(prev[0], pair[1], pair[0])
            prev = pair

    def ray(self, angle, start, end, width):
        """A straight band out from the centre along `angle` (radians, from +X)."""
        dx, dy = math.cos(angle), math.sin(angle)
        nx, ny = -dy * width / 2, dx * width / 2
        p = [self.vert(start * dx - nx, start * dy - ny, 0, 0.0), self.vert(start * dx + nx, start * dy + ny, 0, 1.0),
             self.vert(end * dx - nx, end * dy - ny, 1, 0.0), self.vert(end * dx + nx, end * dy + ny, 1, 1.0)]
        self.tri(p[0], p[1], p[3])
        self.tri(p[0], p[3], p[2])


def write_model(name, texture_name, height, mesh, about):
    lines = [
        "#MAXMODEL ASCII",
        "# nwn5e (tools/data/stealth_rings.py): %s (stealth_watch.cpp). nwn5e's own art." % about,
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
        "  verts %d" % len(mesh.verts),
    ]
    lines += ["    %.4f %.4f %.4f" % v for v in mesh.verts]
    lines.append("  faces %d" % len(mesh.faces))
    lines += ["    %d %d %d 1 %d %d %d 0" % (f[0], f[1], f[2], f[0], f[1], f[2]) for f in mesh.faces]
    lines.append("  tverts %d" % len(mesh.tverts))
    lines += ["    %.4f %.4f 0" % t for t in mesh.tverts]
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
        "# visualeffects.2da - the stealth rings and cones (stealth_watch.cpp), generated by",
        "# tools/data/stealth_rings.py: how far a hostile creature sees (1573-1582: an amber",
        "# ring - searching, it sees all round) and hears (1583-1592: blue, dashed), radii",
        "# %s m; 1610-%d: its filled vision cone when calm, 5 colours (yellow: very unlikely" % (
            "/".join(map(str, RADII)), CONE_ROW + len(CONE_COLOURS) * 40 - 1),
        "# to see the hider - red: very likely) x %s degrees x the radii," % "/".join(map(str, CONE_ANGLES)),
        "# turned with its facing. nwn5e's own models and textures (data\\files\\n5_r*), on the root.",
        "Label Type_FD OrientWithGround Imp_Root_S_Node Imp_Root_M_Node Imp_Root_L_Node Imp_Root_H_Node OrientWithObject",
    ]
    count = 0
    for prefix, first, label, tex, width, height, color, dashed in KINDS:
        write_tga(os.path.join(FILES, tex + ".tga"), TEX_U, TEX_V, texture(color, dashed))
        for i, radius in enumerate(RADII):
            name = "%s%d" % (prefix, radius)
            mesh = Mesh()
            mesh.arc(radius, width, 0.0, 2 * math.pi, repeats_per_m=dashed)
            write_model(name, tex, height, mesh, "a %g m ring on the ground - how far a hostile creature %s" % (
                radius, "hears" if dashed else "sees"))
            rows.append("%d %s_%d D 1 %s %s %s %s 0" % (first + i, label, radius, name, name, name, name))
            count += 1
    height = KINDS[0][5]
    for c, (color, alpha) in enumerate(CONE_COLOURS):
        texture_name = "n5_cone_%d" % c
        write_tga(os.path.join(FILES, texture_name + ".tga"), TEX_U, TEX_U, cone_texture(color, alpha))
        for a, angle in enumerate(CONE_ANGLES):
            half = math.radians(angle) / 2
            centre = math.pi / 2  # +Y: where the creature faces
            for i, radius in enumerate(RADII):
                name = "n5_rc%d_%d_%d" % (c, angle, radius)
                mesh = Mesh()
                # Filled: one band from just off the creature out to the radius.
                mesh.arc((radius + CONE_INNER_M) / 2, radius - CONE_INNER_M, centre - half, centre + half)
                write_model(name, texture_name, height, mesh, "a filled %d-degree vision cone %g m deep, colour %d"
                            " (0 yellow: very unlikely to see the hider - 4 red: very likely)" % (angle, radius, c))
                row = CONE_ROW + 40 * c + 10 * a + i
                rows.append("%d NWN5E_CONE_%d_%d_%d D 1 %s %s %s %s 1" % (row, c, angle, radius, name, name, name,
                                                                          name))
                count += 1
    for k, (prefix, tex, width, height, color, dashed) in enumerate(LIGHT_KINDS):
        write_tga(os.path.join(FILES, tex + ".tga"), TEX_U, TEX_V, texture(color, dashed))
        for i, radius in enumerate(LIGHT_RADII):
            name = "%s%s" % (prefix, ("%g" % radius).replace(".", "_"))
            mesh = Mesh()
            mesh.arc(radius, width, 0.0, 2 * math.pi, repeats_per_m=dashed)
            write_model(name, tex, height, mesh, "a %g m ring round a light source - where its %s light ends" % (
                radius, "dim" if dashed else "bright"))
            label = "NWN5E_LIGHT_%s_%s" % ("DIM" if dashed else "BRIGHT", ("%g" % radius).replace(".", "_"))
            rows.append("%d %s D 1 %s %s %s %s 0" % (LIGHT_ROW + 8 * k + i, label, name, name, name, name))
            count += 1
    header = next(i for i, r in enumerate(rows) if r.startswith("Label "))
    rows.insert(header, "# 1820-1835: light zones round a lit light source (pale; bright edge solid, dim edge"
                        " dashed), radii %s m." % "/".join("%g" % r for r in LIGHT_RADII))
    with open(ROWS, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(rows) + "\n")
    print("wrote %d models, %d textures to %s, and %s" % (count, 4 + len(CONE_COLOURS), FILES, ROWS))


if __name__ == "__main__":
    main()
