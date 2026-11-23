# The test module's auto-tiler (#156): each tile of an area picked from its tileset by the terrains at its four corners.
# A tile placed with Tile_Orientation k shows its .set corners turned k quarter turns counter-clockwise - the top-right
# corner comes to the top-left - as check_tiler.py proves on a shipped module's area (every pair of neighbours agrees on
# its shared corners this way, and not the other).


def rotate(corners, k):
    """corners (tl, tr, br, bl) as a tile placed with orientation k shows them."""
    k %= 4
    return tuple(corners[(i + k) % 4] for i in range(4))


def tile_grid(tileset, corners, area=""):
    """corners: (height+1) rows of (width+1) terrain names, row 0 the south. One (Tile_ID, Tile_Orientation) a tile,
    row by row from the south-west (as an .are's Tile_List). A tile is flat (every corner height 0) and has no crossers;
    among those that fit, the lowest id, then the lowest orientation, so a build is the same every time."""
    plain = [t for t in tileset["tiles"] if all(h == 0 for h in t["heights"]) and not any(t["crossers"])]
    out = []
    for y in range(len(corners) - 1):
        for x in range(len(corners[0]) - 1):
            want = (corners[y + 1][x], corners[y + 1][x + 1], corners[y][x + 1], corners[y][x])  # tl, tr, br, bl
            pick = next(((t["id"], k) for t in plain for k in range(4) if rotate(t["corners"], k) == want), None)
            if pick is None:
                raise ValueError("%s: no tile for %d,%d with corners %s (tl, tr, br, bl)" % (area, x, y, ", ".join(want)))
            out.append(pick)
    return out


def place_features(tileset, corners, tiles, features, area=""):
    """#166 tile blockers, step 1: a feature tile - one with a built-in obstacle (a column, a block) - at its grid spot
    instead of the plain one tile_grid picked: `features` [{"tile": [x, y], "model": <its .set model>, "orientation":
    k}]. The tile must fit the spot's corners turned k quarter turns, as tile_grid's. `tiles` changed in place."""
    width = len(corners[0]) - 1
    for f in features:
        x, y = f["tile"]
        k = f.get("orientation", 0)
        want = (corners[y + 1][x], corners[y + 1][x + 1], corners[y][x + 1], corners[y][x])
        match = [t for t in tileset["tiles"] if t["model"].lower() == f["model"].lower()]
        if not match:
            raise ValueError("%s: no tile %s in the tileset" % (area, f["model"]))
        if rotate(match[0]["corners"], k) != want:
            raise ValueError("%s: %s turned %d doesn't fit %d,%d (corners %s, the spot %s)" % (
                area, f["model"], k, x, y, ", ".join(rotate(match[0]["corners"], k)), ", ".join(want)))
        tiles[y * width + x] = (match[0]["id"], k)
    return tiles
