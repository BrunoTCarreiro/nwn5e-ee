# The test module's build (#156): nwn5e Test Grounds from layout.json - its files, its marks, the dark wing unlit, the
# factions, the same bytes every time, and where it goes by default. The build needs the game: skipped without it.
import os
import sys
import tempfile

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
sys.path.insert(0, os.path.join(here, "..", "gff"))
import build
import erf
import gamedata
import gffjson

fails = []


def check(what, ok, detail=""):
    print(("ok   " if ok else "FAIL ") + what + (("  " + str(detail)) if detail != "" else ""))
    if not ok:
        fails.append(what)


check("--out wins, the default is named", build.default_out().replace("/", "\\").lower().endswith(
    "neverwinter nights\\modules\\n5test.mod"), build.default_out())

args = sys.argv[1:]
game_dir = gamedata.find_game(args[args.index("--game") + 1] if "--game" in args else None)
if not game_dir:
    print("skip: no game install found - the build unchecked")
else:
    out = os.path.join(tempfile.mkdtemp(), "n5test.mod")
    data = build.build(game_dir, os.path.join(here, "layout.json"))
    open(out, "wb").write(data)
    entries = {(r, t): b for r, t, b in erf.read_erf(data)}
    areas = ["n5test_field", "n5test_hall", "n5test_dark"]
    needed = [("module", 2014), ("repute", 2038)] + [(a, t) for a in areas for t in (2012, 2023, 2046)]
    check("every file is in it", all(n in entries for n in needed), sorted(n for n in needed if n not in entries))
    ifo = gffjson.read_gff(entries[("module", 2014)])["root"]
    check("the entry is the field", ifo["Mod_Entry_Area"][1] == "n5test_field", ifo["Mod_Entry_Area"][1])
    check("the module lists the three areas", [a["Area_Name"][1] for a in ifo["Mod_Area_list"][1]] == areas)
    marks = {"n5test_field": "n5test_cast_field", "n5test_hall": "n5test_cast_hall", "n5test_dark": "n5test_cast_dark"}
    for area in areas:
        are = gffjson.read_gff(entries[(area, 2012)])["root"]
        w, h = are["Width"][1], are["Height"][1]
        check("%s: one tile a cell (%dx%d)" % (area, w, h), len(are["Tile_List"][1]) == w * h)
        git = gffjson.read_gff(entries[(area, 2023)])["root"]
        tags = [wp["Tag"][1] for wp in git["WaypointList"][1]]
        check("%s: its cast mark" % area, marks[area] in tags, tags)
        gic = gffjson.read_gff(entries[(area, 2046)])["root"]
        check("%s: a comment for each placeable" % area, len(gic["Placeable List"][1]) == len(git["Placeable List"][1]))
    dark = gffjson.read_gff(entries[("n5test_dark", 2012)])["root"]
    lights = [t[k][1] for t in dark["Tile_List"][1] for k in ("Tile_MainLight1", "Tile_MainLight2", "Tile_SrcLight1", "Tile_SrcLight2")]
    check("the dark wing: every tile light off", set(lights) == {0}, sorted(set(lights)))
    check("the dark wing: no ambient, no diffuse", all(dark[k][1] == 0 for k in ("SunAmbientColor", "SunDiffuseColor",
                                                                                "MoonAmbientColor", "MoonDiffuseColor")))
    hall = gffjson.read_gff(entries[("n5test_hall", 2023)])["root"]
    templates = sorted({p["TemplateResRef"][1] for p in hall["Placeable List"][1]})
    check("the hall: pillars, crates and braziers", templates == ["plc_brazier", "plc_pillar1", "x2_plc_bcrate1"], templates)
    fac = gffjson.read_gff(entries[("repute", 2038)])["root"]
    names = [f["FactionName"][1] for f in fac["FactionList"][1]]
    check("repute.fac has PC, Hostile, Commoner, Merchant, Defender", names == ["PC", "Hostile", "Commoner", "Merchant", "Defender"], names)
    check("the same build, the same bytes", build.build(game_dir, os.path.join(here, "layout.json")) == data)

print(f"FAILED: {len(fails)}" if fails else "all build checks pass")
sys.exit(1 if fails else 0)
