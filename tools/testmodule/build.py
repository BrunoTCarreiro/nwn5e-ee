# Builds nwn5e Test Grounds (n5test.mod; #156, docs/superpowers/specs/2026-11-10-test-module-design.md) from layout.json:
# each area's .are (its tiles from the auto-tiler, its light), .git (placeables from the game's own blueprints, the cast
# marks) and .gic, module.ifo and the standard factions' repute.fac, packed as an ERF. The game's tilesets and blueprints
# are read from the install at build time; nothing of the game's is in the repo.
#   python -I tools/testmodule/build.py [--game DIR] [--layout FILE] [--out FILE]
import argparse
import hashlib
import json
import math
import os
import sys

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
sys.path.insert(0, os.path.join(here, "..", "gff"))
import erf
import gamedata
import gffjson
import tiler

ARE, GIT, IFO, GIC, FAC = 2012, 2023, 2014, 2046, 2038
ROOT = 0xFFFFFFFF
# The five standard factions and how each regards the others (FactionID1 toward FactionID2), as every module carries.
FACTIONS = ["PC", "Hostile", "Commoner", "Merchant", "Defender"]
REPUTATIONS = [(0, 1, 0), (0, 2, 50), (0, 3, 50), (0, 4, 50), (1, 1, 100), (1, 2, 0), (1, 3, 0), (1, 4, 0), (2, 0, 100),
               (2, 1, 0), (2, 2, 100), (2, 3, 50), (2, 4, 100), (3, 1, 0), (3, 2, 50), (3, 3, 100), (3, 4, 100),
               (4, 1, 0), (4, 2, 50), (4, 3, 100), (4, 4, 100)]
BLUEPRINT_ONLY = ("PaletteID", "Comment")  # a .utp's fields an instance doesn't carry


def loc(text):
    return {"strref": -1, "strings": {"0": text} if text else {}}


def gff(file_type, root):
    return gffjson.write_gff({"file_type": file_type, "file_version": "V3.2", "root": dict(root, __struct_id=ROOT)})


def default_out():
    """<Documents>\\Neverwinter Nights\\modules\\n5test.mod: the Documents that has a Neverwinter Nights folder."""
    home = os.environ.get("USERPROFILE", os.path.expanduser("~"))
    for docs in (os.path.join(home, "Documents"), os.path.join(home, "OneDrive", "Documents")):
        if os.path.isdir(os.path.join(docs, "Neverwinter Nights")):
            return os.path.join(docs, "Neverwinter Nights", "modules", "n5test.mod")
    return os.path.join(home, "Documents", "Neverwinter Nights", "modules", "n5test.mod")


def are_file(area, tiles):
    light = area["light"]
    w, h = len(area["corners"][0]) - 1, len(area["corners"]) - 1
    tile_list = [{"__struct_id": 1, "Tile_ID": [5, tid], "Tile_Orientation": [5, orient], "Tile_Height": [5, 0],
                  "Tile_MainLight1": [0, light["tile_main"][0]], "Tile_MainLight2": [0, light["tile_main"][1]],
                  "Tile_SrcLight1": [0, light["tile_src"][0]], "Tile_SrcLight2": [0, light["tile_src"][1]],
                  "Tile_AnimLoop1": [0, 1], "Tile_AnimLoop2": [0, 1], "Tile_AnimLoop3": [0, 1]} for tid, orient in tiles]
    return gff("ARE ", {
        "ID": [5, -1], "Creator_ID": [5, -1], "Version": [4, 1], "Tag": [10, area["resref"].upper()],
        "Name": [12, loc(area["name"])], "ResRef": [11, area["resref"]], "Comments": [10, ""], "Expansion_List": [15, []],
        "Flags": [4, area["flags"]], "ModSpotCheck": [5, 0], "ModListenCheck": [5, 0],
        "MoonAmbientColor": [4, light["moon_ambient"]], "MoonDiffuseColor": [4, light["moon_diffuse"]],
        "MoonFogAmount": [0, light["moon_fog"]], "MoonFogColor": [4, light["moon_fog_color"]], "MoonShadows": [0, 0],
        "SunAmbientColor": [4, light["sun_ambient"]], "SunDiffuseColor": [4, light["sun_diffuse"]],
        "SunFogAmount": [0, light["sun_fog"]], "SunFogColor": [4, light["sun_fog_color"]],
        "SunShadows": [0, light["sun_shadows"]], "IsNight": [0, light["is_night"]],
        "LightingScheme": [0, light["lighting_scheme"]], "ShadowOpacity": [0, light["shadow_opacity"]],
        "DayNightCycle": [0, light["day_night"]], "ChanceRain": [5, 0], "ChanceSnow": [5, 0], "ChanceLightning": [5, 0],
        "WindPower": [5, 0], "LoadScreenID": [2, 0], "PlayerVsPlayer": [0, 0], "NoRest": [0, 0],
        "Width": [5, w], "Height": [5, h], "OnEnter": [11, ""], "OnExit": [11, ""], "OnHeartbeat": [11, ""],
        "OnUserDefined": [11, ""], "Tileset": [11, area["tileset"]], "Tile_List": [15, tile_list]})


def placeable(game, p):
    raw = game.resource(p["template"], gamedata.UTP)
    if raw is None:
        raise ValueError("no blueprint " + p["template"] + " in the game")
    root = gffjson.read_gff(raw)["root"]
    out = {"__struct_id": 9}
    for key, value in root.items():
        if key != "__struct_id" and key not in BLUEPRINT_ONLY:
            out[key] = value
    out["TemplateResRef"] = [11, p["template"]]
    out.update({"X": [8, float(p["x"])], "Y": [8, float(p["y"])], "Z": [8, 0.0],
                "Bearing": [8, math.radians(p.get("bearing", 0))]})
    if "plot" in p:  # the layout's say over the blueprint's (the hall's crates: cover that doesn't break)
        out["Plot"] = [0, int(p["plot"])]
    return out


def waypoint(w):
    a = math.radians(w["facing"])
    return {"__struct_id": 5, "Appearance": [0, 1], "LinkedTo": [10, ""], "TemplateResRef": [11, "nw_waypoint001"],
            "Tag": [10, w["tag"]], "LocalizedName": [12, loc(w["name"])], "Description": [12, loc("")],
            "HasMapNote": [0, 0], "MapNote": [12, loc("")], "MapNoteEnabled": [0, 0],
            "XPosition": [8, float(w["x"])], "YPosition": [8, float(w["y"])], "ZPosition": [8, 0.0],
            "XOrientation": [8, round(math.cos(a), 6)], "YOrientation": [8, round(math.sin(a), 6)]}


def lists(placeables, waypoints):
    return {"Creature List": [15, []], "Door List": [15, []], "Encounter List": [15, []], "List": [15, []],
            "SoundList": [15, []], "StoreList": [15, []], "TriggerList": [15, []], "WaypointList": [15, waypoints],
            "Placeable List": [15, placeables]}


def git_file(game, area):
    props = {"__struct_id": 100, "AmbientSndDay": [5, 0], "AmbientSndNight": [5, 0], "AmbientSndDayVol": [5, 0],
             "AmbientSndNitVol": [5, 0], "EnvAudio": [5, 0], "MusicBattle": [5, 0], "MusicDay": [5, 0],
             "MusicNight": [5, 0], "MusicDelay": [5, 0]}
    root = {"AreaProperties": [14, props]}
    root.update(lists([placeable(game, p) for p in area["placeables"]], [waypoint(w) for w in area["waypoints"]]))
    return gff("GIT ", root)


def gic_file(area):
    return gff("GIC ", lists([{"__struct_id": 9, "Comment": [10, ""]} for _ in area["placeables"]],
                             [{"__struct_id": 5, "Comment": [10, ""]} for _ in area["waypoints"]]))


def ifo_file(layout):
    m = layout["module"]
    entry = next(a for a in layout["areas"] if a["resref"] == m["entry"])
    mark = entry["waypoints"][0]
    a = math.radians(mark["facing"])
    scripts = ["Mod_OnHeartbeat", "Mod_OnModLoad", "Mod_OnModStart", "Mod_OnClientEntr", "Mod_OnClientLeav",
               "Mod_OnActvtItem", "Mod_OnAcquirItem", "Mod_OnUsrDefined", "Mod_OnUnAqreItem", "Mod_OnPlrDeath",
               "Mod_OnPlrDying", "Mod_OnPlrLvlUp", "Mod_OnSpawnBtnDn", "Mod_OnPlrRest"]
    root = {"Mod_ID": [13, hashlib.md5(("nwn5e " + m["tag"]).encode()).hexdigest()], "Mod_MinGameVer": [10, "1.69"],
            "Mod_Creator_ID": [5, 2], "Mod_Version": [4, 3], "Expansion_Pack": [2, 0], "Mod_Name": [12, loc(m["name"])],
            "Mod_Tag": [10, m["tag"]], "Mod_Hak": [10, ""], "Mod_Description": [12, loc(m["description"])],
            "Mod_IsSaveGame": [0, 0], "Mod_Entry_Area": [11, m["entry"]], "Mod_Entry_X": [8, float(mark["x"])],
            "Mod_Entry_Y": [8, float(mark["y"])], "Mod_Entry_Z": [8, 0.0], "Mod_Entry_Dir_X": [8, round(math.cos(a), 6)],
            "Mod_Entry_Dir_Y": [8, round(math.sin(a), 6)], "Mod_Expan_List": [15, []], "Mod_DawnHour": [0, 6],
            "Mod_DuskHour": [0, 18], "Mod_MinPerHour": [0, 2], "Mod_StartMonth": [0, 6], "Mod_StartDay": [0, 1],
            "Mod_StartHour": [0, 13], "Mod_StartYear": [4, 1372], "Mod_XPScale": [0, 10]}
    for s in scripts:
        root[s] = [11, ""]
    root.update({"Mod_StartMovie": [11, ""], "Mod_CutSceneList": [15, []], "Mod_GVar_List": [15, []],
                 "Mod_Area_list": [15, [{"__struct_id": 6, "Area_Name": [11, a["resref"]]} for a in layout["areas"]]]})
    return gff("IFO ", root)


def fac_file():
    factions = [{"__struct_id": i, "FactionParentID": [4, 0xFFFFFFFF], "FactionName": [10, n], "FactionGlobal": [2, 0]}
                for i, n in enumerate(FACTIONS)]
    reps = [{"__struct_id": i, "FactionID1": [4, a], "FactionID2": [4, b], "FactionRep": [4, r]}
            for i, (a, b, r) in enumerate(REPUTATIONS)]
    return gff("FAC ", {"FactionList": [15, factions], "RepList": [15, reps]})


def build(game_dir, layout_path):
    """The module's bytes."""
    game = gamedata.Game(game_dir)
    layout = json.load(open(layout_path, encoding="utf-8"))
    entries = [("module", IFO, ifo_file(layout)), ("repute", FAC, fac_file())]
    for area in layout["areas"]:
        tileset = gamedata.read_set(game.resource(area["tileset"], gamedata.SET).decode("latin1"))
        corners = [[area["terrain"][c] for c in row] for row in area["corners"]]
        tiles = tiler.tile_grid(tileset, corners, area["resref"])
        entries += [(area["resref"], ARE, are_file(area, tiles)), (area["resref"], GIT, git_file(game, area)),
                    (area["resref"], GIC, gic_file(area))]
    return erf.write_erf("MOD ", entries)


def main():
    parser = argparse.ArgumentParser(description="Build nwn5e Test Grounds (n5test.mod).")
    parser.add_argument("--game", help="the game's folder (default: NWN_GAME or the usual Steam folders)")
    parser.add_argument("--layout", default=os.path.join(here, "layout.json"))
    parser.add_argument("--out", help="the .mod to write (default: your Documents\\Neverwinter Nights\\modules)")
    args = parser.parse_args()
    game_dir = gamedata.find_game(args.game)
    if not game_dir:
        sys.exit("build: no game install found - give --game <the Neverwinter Nights folder>")
    out = args.out or default_out()
    if not args.out and not os.path.isdir(os.path.dirname(os.path.dirname(out))):
        sys.exit("build: no Neverwinter Nights folder in your Documents - give --out <file.mod>")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    data = build(game_dir, args.layout)
    with open(out, "wb") as f:
        f.write(data)
    layout = json.load(open(args.layout, encoding="utf-8"))
    sizes = ", ".join("%s %dx%d" % (a["name"], len(a["corners"][0]) - 1, len(a["corners"]) - 1) for a in layout["areas"])
    print("build: wrote %s (%d bytes; %s)" % (out, len(data), sizes))


if __name__ == "__main__":
    main()
