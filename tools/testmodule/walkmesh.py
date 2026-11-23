# A tile's walkmesh (#166 tile blockers, steps 1-2; owner, 2026-11-23: "plan the tile blockers" - "do steps 1 and 2"):
# the game's <tile model>.wok (ASCII: an aabb node's verts and faces, each face's last number its surfacemat.2da
# material) read in tile space, and its features - the shapes that stand up out of the floor: faces of a material that
# blocks sight and isn't walkable (surfacemat.2da LineOfSight 1, Walk 0), joined by shared vertices, at least 1 m
# taller than the floor around them, inside the tile (not touching its edges: those are its walls). Read from the
# owner's install at build time, never committed.
import math
from collections import defaultdict

WOK = 2016
MIN_HEIGHT = 1.0  # over the tile's floor
INSIDE = 4.7      # metres from the tile's centre: a shape reaching past it is a wall
MAX_SIZE = 6.5


def parse_wok(text):
    """[(material, ((x, y, z), (x, y, z), (x, y, z)))] in tile space (the node's position and turn applied)."""
    lines = text.splitlines()
    pos, rot = (0.0, 0.0, 0.0), (0.0, 0.0, 1.0, 0.0)
    verts, faces = [], []
    i = 0
    while i < len(lines):
        t = lines[i].split()
        if t and t[0] == "position" and len(t) >= 4:
            pos = tuple(float(v) for v in t[1:4])
        elif t and t[0] == "orientation" and len(t) >= 5:
            rot = tuple(float(v) for v in t[1:5])
        elif t and t[0] == "verts":
            n = int(t[1])
            verts = [tuple(float(v) for v in lines[i + 1 + k].split()[:3]) for k in range(n)]
            i += n
        elif t and t[0] == "faces":
            n = int(t[1])
            for k in range(n):
                f = lines[i + 1 + k].split()
                faces.append((int(f[7]), (verts[int(f[0])], verts[int(f[1])], verts[int(f[2])])))
            i += n
        i += 1
    ax, ay, az, angle = rot
    length = math.sqrt(ax * ax + ay * ay + az * az) or 1.0
    ax, ay, az = ax / length, ay / length, az / length
    c, s = math.cos(angle), math.sin(angle)

    def turn(p):
        x, y, z = p
        dot = ax * x + ay * y + az * z
        cx, cy, cz = ay * z - az * y, az * x - ax * z, ax * y - ay * x
        return (x * c + cx * s + ax * dot * (1 - c) + pos[0], y * c + cy * s + ay * dot * (1 - c) + pos[1],
                z * c + cz * s + az * dot * (1 - c) + pos[2])

    return [(m, tuple(turn(p) for p in tri)) for m, tri in faces]


def surfacemat(table_text):
    """surfacemat.2da -> {material: (line_of_sight, walk)}."""
    rows = [l.split() for l in table_text.splitlines() if l.strip()]
    cols = rows[1]
    los, walk = cols.index("LineOfSight"), cols.index("Walk")
    out = {}
    for r in rows[2:]:
        if r[0].isdigit() and len(r) > max(los, walk) + 1:
            out[int(r[0])] = (int(r[los + 1]) if r[los + 1].isdigit() else 0,
                              int(r[walk + 1]) if r[walk + 1].isdigit() else 0)
    return out


def _groups(faces):
    """Faces joined by shared vertices."""
    parent = list(range(len(faces)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    seen = {}
    for i, (_, tri) in enumerate(faces):
        for p in tri:
            key = (round(p[0], 2), round(p[1], 2), round(p[2], 2))
            if key in seen:
                parent[find(i)] = find(seen[key])
            else:
                seen[key] = i
    groups = defaultdict(list)
    for i in range(len(faces)):
        groups[find(i)].append(faces[i])
    return list(groups.values())


def _floor_z(faces, materials):
    floor = sorted(p[2] for m, tri in faces if materials.get(m, (1, 1))[1] == 1 for p in tri)
    return floor[len(floor) // 2] if floor else 0.0


def features(faces, materials):
    """The tile's features - tall: [{"box": (x0, x1, y0, y1, z0, z1), "faces": [...], "materials": [...]}]."""
    floor_z = _floor_z(faces, materials)
    out = []
    for group in _groups([(m, tri) for m, tri in faces if materials.get(m, (1, 1)) == (1, 0)]):
        xs = [p[0] for _, t in group for p in t]
        ys = [p[1] for _, t in group for p in t]
        zs = [p[2] for _, t in group for p in t]
        box = (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs))
        if box[5] - floor_z < MIN_HEIGHT:
            continue  # low: a kerb, a basin's rim
        if max(abs(box[0]), abs(box[1]), abs(box[2]), abs(box[3])) > INSIDE:
            continue  # a wall
        if box[1] - box[0] > MAX_SIZE or box[3] - box[2] > MAX_SIZE:
            continue
        out.append({"box": box, "faces": [t for _, t in group], "materials": sorted({m for m, _ in group})})
    return out


def footprints(faces, materials):
    """The tile's flat obstacles: non-walkable islands in the floor (under 0.3 m of rise) - a column's or a statue's
    base drawn as a hole: creatures walk round it, a sight line over the floor never meets it. As features()."""
    out = []
    for group in _groups([(m, tri) for m, tri in faces if materials.get(m, (1, 1))[1] == 0]):
        xs = [p[0] for _, t in group for p in t]
        ys = [p[1] for _, t in group for p in t]
        zs = [p[2] for _, t in group for p in t]
        box = (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs))
        size = max(box[1] - box[0], box[3] - box[2])
        if box[5] - box[4] >= 0.3 or size < 0.3 or size > MAX_SIZE:
            continue
        if max(abs(box[0]), abs(box[1]), abs(box[2]), abs(box[3])) > INSIDE + 0.31:
            continue  # reaching past the tile's edge: not this tile's own
        out.append({"box": box, "faces": [t for _, t in group], "materials": sorted({m for m, _ in group})})
    return out


def crosses_2d(a, b, faces, samples=200):
    """Whether the segment a -> b, seen from above, passes over any of the triangles."""
    for i in range(samples + 1):
        px, py = a[0] + (b[0] - a[0]) * i / samples, a[1] + (b[1] - a[1]) * i / samples
        for (x0, y0, _), (x1, y1, _), (x2, y2, _) in faces:
            d = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
            if abs(d) < 1e-9:
                continue
            u = ((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) / d
            v = ((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) / d
            if u >= 0 and v >= 0 and u + v <= 1:
                return True
    return False


def placed(point, tile_x, tile_y, orientation):
    """A tile-space point in the area: the tile at grid (tile_x, tile_y), turned `orientation` quarter turns
    anticlockwise (an .are's Tile_Orientation)."""
    x, y, z = point
    for _ in range(orientation % 4):
        x, y = -y, x
    return (x + tile_x * 10.0 + 5.0, y + tile_y * 10.0 + 5.0, z)


def segment_hits(a, b, faces):
    """Whether the segment a -> b crosses any of the triangles (Moller-Trumbore)."""
    d = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
    for p0, p1, p2 in faces:
        e1 = (p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2])
        e2 = (p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2])
        h = (d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0])
        det = e1[0] * h[0] + e1[1] * h[1] + e1[2] * h[2]
        if abs(det) < 1e-9:
            continue
        f = 1.0 / det
        s = (a[0] - p0[0], a[1] - p0[1], a[2] - p0[2])
        u = f * (s[0] * h[0] + s[1] * h[1] + s[2] * h[2])
        if u < 0.0 or u > 1.0:
            continue
        q = (s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0])
        v = f * (d[0] * q[0] + d[1] * q[1] + d[2] * q[2])
        if v < 0.0 or u + v > 1.0:
            continue
        t = f * (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2])
        if 0.0 <= t <= 1.0:
            return True
    return False
