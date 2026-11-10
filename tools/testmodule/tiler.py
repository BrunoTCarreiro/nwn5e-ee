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
