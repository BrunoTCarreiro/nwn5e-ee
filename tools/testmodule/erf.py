# ERF V1.0 (a module's .mod archive) for the test module (#156): written for the build, read back for the checks.
#   header (160 bytes): type, "V1.0", language count 0, localized size 0, entry count, the offsets of the localized
#   strings, the key list and the resource list, build year / day 0, description strref 0xFFFFFFFF, 116 reserved;
#   key list: resref (16, NUL-padded), res id, type, 2 unused; resource list: offset, size; then the data.
import struct

HEADER = 160


def write_erf(file_type, entries):
    """file_type: 4 chars ("MOD "); entries: [(resref, type, bytes)], written sorted by (resref, type)."""
    items = sorted(((r.lower(), t, b) for r, t, b in entries), key=lambda e: (e[0], e[1]))
    count = len(items)
    off_keys = HEADER
    off_res = off_keys + 24 * count
    off_data = off_res + 8 * count
    head = file_type.encode("latin1")[:4].ljust(4) + b"V1.0"
    head += struct.pack("<8I", 0, 0, count, off_keys, off_keys, off_res, 0, 0)
    head += struct.pack("<I", 0xFFFFFFFF) + b"\0" * 116
    keys, res, blob = b"", b"", b""
    for i, (resref, rtype, body) in enumerate(items):
        name = resref.encode("latin1")
        if len(name) > 16:
            raise ValueError("a resref is 16 characters at most: " + resref)
        keys += name.ljust(16, b"\0") + struct.pack("<IHH", i, rtype, 0)
        res += struct.pack("<II", off_data + len(blob), len(body))
        blob += body
    return head + keys + res + blob


def read_erf(data):
    """[(resref, type, bytes)] in the key list's order."""
    count, off_keys, off_res = struct.unpack_from("<I", data, 16)[0], *struct.unpack_from("<2I", data, 24)
    out = []
    for i in range(count):
        resref = data[off_keys + 24 * i: off_keys + 24 * i + 16].rstrip(b"\0").decode("latin1")
        rid, rtype = struct.unpack_from("<IH", data, off_keys + 24 * i + 16)
        offset, size = struct.unpack_from("<II", data, off_res + 8 * rid)
        out.append((resref, rtype, data[offset: offset + size]))
    return out
