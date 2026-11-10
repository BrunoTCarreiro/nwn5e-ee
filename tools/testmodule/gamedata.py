# The game's resources for the test module's build (#156): read from the owner's install at build time, never committed.
#   KEY V1 (data\nwn_base.key, then data\nwn_retail.key over it): BIF file table, then keys (resref 16, type 2, id 4:
#   BIF index = id >> 20, entry = id & 0xFFFFF); BIF V1: the variable table at +16, 16 bytes an entry (id, offset, size,
#   type). A loose ovr\<resref>.<ext> wins, as in the game.
import os
import struct

SET, UTP = 2013, 2044
EXT = {SET: "set", UTP: "utp", 2012: "are", 2023: "git", 2014: "ifo"}
CANDIDATES = [r"X:\SteamLibrary\steamapps\common\Neverwinter Nights",
              r"C:\Program Files (x86)\Steam\steamapps\common\Neverwinter Nights"]


def find_game(arg=None):
    """The install: the argument, else NWN_GAME, else the usual Steam folders; "" when none has data\\nwn_base.key."""
    for path in [arg, os.environ.get("NWN_GAME")] + CANDIDATES:
        if path and os.path.isfile(os.path.join(path, "data", "nwn_base.key")):
            return path
    return ""


class Game:
    def __init__(self, path):
        self.path = path
        self.index = {}
        for key in ("nwn_base.key", "nwn_retail.key"):
            self._read_key(os.path.join(path, "data", key))

    def _read_key(self, at):
        if not os.path.isfile(at):
            return
        data = open(at, "rb").read()
        bif_count, key_count, file_table, key_table = struct.unpack_from("<4I", data, 8)
        bifs = []
        for i in range(bif_count):
            offset, length = struct.unpack_from("<IH", data, file_table + i * 12 + 4)
            bifs.append(data[offset: offset + length].split(b"\0")[0].decode("latin1").replace("\\", os.sep))
        for k in range(key_count):
            at_key = key_table + k * 22
            name = data[at_key: at_key + 16].split(b"\0")[0].decode("latin1").lower()
            rtype, rid = struct.unpack_from("<HI", data, at_key + 16)
            self.index[(name, rtype)] = (bifs[rid >> 20], rid)

    def resource(self, resref, rtype):
        """The resource's bytes, or None."""
        name = resref.lower()
        loose = os.path.join(self.path, "ovr", name + "." + EXT.get(rtype, str(rtype)))
        if os.path.isfile(loose):
            return open(loose, "rb").read()
        found = self.index.get((name, rtype))
        if not found:
            return None
        bif, rid = found
        with open(os.path.join(self.path, bif), "rb") as f:
            head = f.read(20)
            table = struct.unpack_from("<I", head, 16)[0]
            f.seek(table + (rid & 0xFFFFF) * 16)
            _, offset, size, _ = struct.unpack("<4I", f.read(16))
            f.seek(offset)
            return f.read(size)


def read_set(text):
    """A tileset's .set: {"terrains": [...], "tiles": [{"id", "model", "corners": (tl, tr, br, bl), "heights": (...),
    "crossers": (top, right, bottom, left)}]} in tile order."""
    sections, current = {}, None
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith("[") and line.endswith("]"):
            current = line[1:-1].strip().upper()
            sections[current] = {}
        elif current is not None and "=" in line:
            key, value = line.split("=", 1)
            sections[current][key.strip().lower()] = value.strip()
    terrains, k = [], 0
    while ("TERRAIN%d" % k) in sections:
        terrains.append(sections["TERRAIN%d" % k].get("name", ""))
        k += 1
    tiles = []
    count = int(sections.get("TILES", {}).get("count", "0") or 0)
    for i in range(count):
        s = sections.get("TILE%d" % i, {})
        height = lambda key: int(s.get(key, "0") or 0)
        tiles.append({
            "id": i,
            "model": s.get("model", ""),
            "corners": (s.get("topleft", ""), s.get("topright", ""), s.get("bottomright", ""), s.get("bottomleft", "")),
            "heights": (height("topleftheight"), height("toprightheight"), height("bottomrightheight"), height("bottomleftheight")),
            "crossers": (s.get("top", ""), s.get("right", ""), s.get("bottom", ""), s.get("left", "")),
        })
    return {"terrains": terrains, "tiles": tiles}
