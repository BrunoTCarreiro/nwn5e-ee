# The test module's auto-tiler (#156): each tile picked by its corner terrains, and the rotation convention proven on a
# shipped module's area (Review Focus 1) - a wrong one builds walls that don't join, which only shows in game.
import os
import sys

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
sys.path.insert(0, os.path.join(here, "..", "gff"))
import erf
import gamedata
import gffjson
import tiler

fails = []


def check(what, ok, detail=""):
    print(("ok   " if ok else "FAIL ") + what + (("  " + str(detail)) if detail != "" else ""))
    if not ok:
        fails.append(what)


def tile(i, tl, tr, br, bl, crossers=("", "", "", ""), heights=(0, 0, 0, 0)):
    return {"id": i, "model": "t%d" % i, "corners": (tl, tr, br, bl), "heights": heights, "crossers": crossers}


# A corner tile (floor only at its bottom-left) and a plain floor; a room of floor inside walls, 2 by 2 tiles.
W, F = "Wall", "Floor"
synthetic = {"terrains": [W, F], "tiles": [tile(0, W, W, W, F), tile(1, F, F, F, F), tile(2, W, W, F, F),
                                           tile(3, F, F, F, F, crossers=("road", "", "", ""))]}
check("rotate 0 is the corners as they are", tiler.rotate((W, W, W, F), 0) == (W, W, W, F))
grid = [[W, W, W], [W, F, W], [W, W, W]]  # 3 x 3 corners: the middle one floor, row 0 south
picks = tiler.tile_grid(synthetic, grid, "room")
check("four tiles, each a corner tile", len(picks) == 4 and all(p[0] == 0 for p in picks), picks)
check("each turned so its floor corner is the room's middle",
      all(tiler.rotate(synthetic["tiles"][0]["corners"], o) == want for (_, o), want in
          zip(picks, [(W, F, W, W), (F, W, W, W), (W, W, F, W), (W, W, W, F)])), picks)
check("a crossed tile is never picked", 3 not in [p[0] for p in tiler.tile_grid(synthetic, [[F, F], [F, F]], "floor")])
try:
    tiler.tile_grid({"terrains": [W, F], "tiles": [tile(1, F, F, F, F)]}, [[W, F], [F, F]], "hall")
    check("an untileable grid names the tile", False, "no error")
except ValueError as e:
    check("an untileable grid names the tile", "hall" in str(e) and "0,0" in str(e), e)

# The convention on a shipped area: every pair of neighbours agrees on its shared corners.
args = sys.argv[1:]
game_dir = gamedata.find_game(args[args.index("--game") + 1] if "--game" in args else None)
module = os.path.join(game_dir, "data", "mod", "Contest Of Champions 0492.mod") if game_dir else ""
if not game_dir or not os.path.isfile(module):
    print("skip: no game install (or its Contest Of Champions module) - the rotation convention unproven here")
else:
    game = gamedata.Game(game_dir)
    entries = erf.read_erf(open(module, "rb").read())
    areas = [gffjson.read_gff(b)["root"] for _, t, b in entries if t == 2012]
    tested = 0
    for area in areas:
        tileset = gamedata.read_set(game.resource(area["Tileset"][1], gamedata.SET).decode("latin1"))
        w, h = area["Width"][1], area["Height"][1]
        tiles = area["Tile_List"][1]
        def mismatches(rot):
            corners = [rot(tileset["tiles"][t["Tile_ID"][1]]["corners"], t["Tile_Orientation"][1]) for t in tiles]
            bad = 0
            for y in range(h):
                for x in range(w):
                    tl, tr, br, bl = corners[y * w + x]
                    if x + 1 < w:
                        rtl, rtr, rbr, rbl = corners[y * w + x + 1]
                        bad += (tr != rtl) + (br != rbl)
                    if y + 1 < h:
                        utl, utr, ubr, ubl = corners[(y + 1) * w + x]
                        bad += (tl != ubl) + (tr != ubr)
            return bad
        ours, other = mismatches(tiler.rotate), mismatches(lambda c, k: tiler.rotate(c, (4 - k) % 4))
        if ours == other:
            continue  # this area can't tell the two apart
        tested += 1
        check("the convention matches %s (%s, %dx%d)" % (area["ResRef"][1], area["Tileset"][1], w, h), ours == 0,
              "ours %d, the other way %d" % (ours, other))
    check("an area told the two conventions apart", tested > 0, tested)

print(f"FAILED: {len(fails)}" if fails else "all tiler checks pass")
sys.exit(1 if fails else 0)
