# The test module reads the game's tilesets and blueprints at build time (#156) - from the owner's install, never
# committed. The game checks skip when no install is found.
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gamedata

fails = []


def check(what, ok, detail=""):
    print(("ok   " if ok else "FAIL ") + what + (("  " + str(detail)) if detail != "" else ""))
    if not ok:
        fails.append(what)


SET = """[GENERAL]
Name=TEST
[TERRAIN TYPES]
Count=2
[TERRAIN0]
Name=Wall
[TERRAIN1]
Name=Floor
[TILES]
Count=2
[TILE0]
Model=t_a
TopLeft=Wall
TopLeftHeight=0
TopRight=Wall
TopRightHeight=0
BottomLeft=Floor
BottomLeftHeight=0
BottomRight=Wall
BottomRightHeight=1
Top=
Right=
Bottom=
Left=
[TILE1]
Model=t_b
TopLeft=Floor
TopRight=Floor
BottomLeft=Floor
BottomRight=Floor
Top=road
"""
ts = gamedata.read_set(SET)
check("a .set's terrains", ts["terrains"] == ["Wall", "Floor"], ts["terrains"])
t0 = ts["tiles"][0]
check("its corners in (tl, tr, br, bl) order", t0["id"] == 0 and t0["corners"] == ("Wall", "Wall", "Wall", "Floor"), t0["corners"])
check("their heights", t0["heights"] == (0, 0, 1, 0), t0["heights"])
check("a crosser", ts["tiles"][1]["crossers"] == ("road", "", "", ""), ts["tiles"][1]["crossers"])

args = sys.argv[1:]
game_dir = gamedata.find_game(args[args.index("--game") + 1] if "--game" in args else None)
if not game_dir:
    print("skip: no game install found (--game DIR or NWN_GAME) - the game's tilesets unchecked")
else:
    game = gamedata.Game(game_dir)
    rural = gamedata.read_set(game.resource("ttr01", gamedata.SET).decode("latin1"))
    check("ttr01: 283 tiles, Grass, Water, Trees", len(rural["tiles"]) == 283 and rural["terrains"] == ["Grass", "Water", "Trees"],
          (len(rural["tiles"]), rural["terrains"]))
    check("tic01 has Stone", "Stone" in gamedata.read_set(game.resource("tic01", gamedata.SET).decode("latin1"))["terrains"])
    dungeon = gamedata.read_set(game.resource("tde01", gamedata.SET).decode("latin1"))["terrains"]
    check("tde01 (the dark wing) has Floor and Wall", "Floor" in dungeon and "Wall" in dungeon, dungeon)
    pillar = game.resource("plc_pillar1", gamedata.UTP)
    check("a base blueprint reads (plc_pillar1.utp)", pillar is not None and pillar[:4] == b"UTP ", pillar[:8] if pillar else None)
    check("a missing one is None", game.resource("no_such_thing", gamedata.UTP) is None)

print(f"FAILED: {len(fails)}" if fails else "all gamedata checks pass")
sys.exit(1 if fails else 0)
