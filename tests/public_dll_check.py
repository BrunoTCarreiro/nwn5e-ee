"""The public DLL check (docs/BENCH-REDESIGN.md, #87).

A DLL built with "build.bat public" - any DLL that leaves this machine - carries none of the
dev layer's commands (nwn5e-tools\\dev) or the retired library export, and does carry the
recipe core's stamp written beside it (core_stamp.txt), so the tools can tell which core it
assembles with.

Usage: python -I tests/public_dll_check.py <folder>\\nwn5e.dll
Exit 0: clean. Exit 1: a dev string found, or the stamp missing. Exit 2: bad usage.
"""
import os
import sys

# The dev layer's chat commands (as the code compares them, NUL-ended) and the old export's folder.
FORBIDDEN = ["!animtrace\0", "!animedit\0", "!vfxedit\0", "!vfxexport\0", "!anim\0", "animedit\\libraries"]


def encodings(text):
    return [("ascii", text.encode("ascii")), ("utf-16", text.encode("utf-16-le"))]


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    dll = argv[1]
    try:
        with open(dll, "rb") as f:
            data = f.read()
    except OSError as e:
        print("can't read %s: %s" % (dll, e))
        return 1
    failed = False
    for text in FORBIDDEN:
        for form, needle in encodings(text):
            if needle in data:
                print("FOUND %r (%s)" % (text.rstrip("\0"), form))
                failed = True
    stamp_path = os.path.join(os.path.dirname(os.path.abspath(dll)), "core_stamp.txt")
    try:
        with open(stamp_path, encoding="ascii") as f:
            stamp = f.read().strip()
    except OSError:
        stamp = ""
    if not stamp:
        print("MISSING core_stamp.txt beside the DLL (%s)" % stamp_path)
        failed = True
    elif stamp.encode("ascii") not in data:
        print("MISSING the core stamp %s in the DLL" % stamp)
        failed = True
    else:
        print("core stamp %s present" % stamp)
    print("FAIL: %s is not a public DLL" % dll if failed else "PASS: %s has no dev strings" % dll)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
