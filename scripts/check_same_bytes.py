"""The faster builds give the same bytes (docs/TOOLS-PLAN.md, "Faster checks"; the owner, 2026-11-23: "yes, do 1 to 3
after the install"): build.bat and test.bat compiling only what changed (scripts/cl_incr.py) and build_derived.js
reusing its cache (tools/data/derived_cache.js) must make exactly what a full build makes.

  python -I scripts/check_same_bytes.py [--scratch <folder>] [--tools <nwn5e-tools>]

On one copy of this checkout's sources (src, tests, third_party, data, tools, scripts and the root's .bat and .ini -
never private\\, which builds from where it is: NWN5E_PRIVATE) in --scratch (default build\\checks\\same_bytes)\\tree -
one copy, as a source's path goes into its object (MSVC names an anonymous namespace from it):
- **B, built a step at a time:** build.bat (into out_b) and test.bat as they run now (only what changed, its objects
  kept from this check's last run; the derived cache this checkout's build\\derived_cache), then three edits - a source
  (the first of src\\rules), the project header the most sources include (each target's own), and a file of the recipe
  core's stamp (core_stamp.bat's first .cpp but anim_recipe's: the stamp changes, so anim_recipe.cpp, which names it,
  must compile again though it includes no edited file) - each adding a static initializer (or, the stamp's, a
  comment), built again (the DLL and the stamp must change: the edits reached them); then every edit undone and built
  again.
- **A, the full build, as before:** build.bat (into out_a, emptied first) and test.bat with NWN5E_INCREMENTAL=off (cl
  compiles every source in one run, as the .bat files always did) and NWN5E_DERIVED_CACHE=off (every generator runs).
- **The check:** every file of B's build (the DLL, the exe, core_stamp.txt, nwn5e.ini, all of data\\ with the
  game-derived files and placeable_bounds.txt) and B's rules_test.exe must be A's, byte for byte - a .dll or .exe with
  its link time stamps (the PE header's, the debug directory's, the export and resource tables', the load config's) and
  its checksum set to 0 first, as two full builds differ there and nowhere else. Every object B's last build linked is
  up to date, or the DLL would differ.
- Prints what it compared and exits 0, or names each file that differs and exits 1.
"""
import os
import shutil
import struct
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROBE = ("\n#ifndef CHECK_SAME_BYTES_PROBE_{tag}\n#define CHECK_SAME_BYTES_PROBE_{tag}\n"
         "namespace {{ struct CheckSameBytesProbe{tag} {{ CheckSameBytesProbe{tag}() {{ static volatile int n = 0; n = n + 1; }} }}"
         " check_same_bytes_probe_{tag}; }}\n#endif\n")


def arg(name, default=None):
    if name in sys.argv:
        i = sys.argv.index(name)
        if i + 1 < len(sys.argv):
            return sys.argv[i + 1]
    return default


# --- a PE file with its time stamps and checksum zeroed ---------------------------------------------------------------
def pe_normalized(data):
    b = bytearray(data)
    if len(b) < 0x40 or b[:2] != b"MZ":
        return bytes(b)
    pe = struct.unpack_from("<I", b, 0x3C)[0]
    if b[pe:pe + 4] != b"PE\0\0":
        return bytes(b)
    coff = pe + 4
    nsections = struct.unpack_from("<H", b, coff + 2)[0]
    opt_size = struct.unpack_from("<H", b, coff + 16)[0]
    struct.pack_into("<I", b, coff + 4, 0)  # TimeDateStamp
    opt = coff + 20
    magic = struct.unpack_from("<H", b, opt)[0]
    struct.pack_into("<I", b, opt + 64, 0)  # CheckSum
    dirs = opt + (112 if magic == 0x20B else 96)
    ndirs = struct.unpack_from("<I", b, dirs - 4)[0]
    sections = []
    sec = opt + opt_size
    for i in range(nsections):
        vsize, va, rsize, raw = struct.unpack_from("<IIII", b, sec + i * 40 + 8)
        sections.append((va, max(vsize, rsize), raw))

    def off(rva):
        for va, size, raw in sections:
            if va <= rva < va + size:
                return raw + rva - va
        return None

    def directory(i):
        if i >= ndirs:
            return None, 0
        rva, size = struct.unpack_from("<II", b, dirs + i * 8)
        return (off(rva) if rva else None), size

    o, size = directory(0)  # exports
    if o is not None:
        struct.pack_into("<I", b, o + 4, 0)
    o, size = directory(2)  # resources: every directory table's stamp
    if o is not None:
        base = o

        def walk(t, depth):
            struct.pack_into("<I", b, t + 4, 0)
            named, ids = struct.unpack_from("<HH", b, t + 12)
            for k in range(named + ids):
                _, child = struct.unpack_from("<II", b, t + 16 + k * 8)
                if child & 0x80000000 and depth < 4:
                    walk(base + (child & 0x7FFFFFFF), depth + 1)
        walk(base, 0)
    o, size = directory(6)  # debug directory
    if o is not None:
        for k in range(size // 28):
            struct.pack_into("<I", b, o + k * 28 + 4, 0)
    o, size = directory(10)  # load config
    if o is not None and size >= 8:
        struct.pack_into("<I", b, o + 4, 0)
    return bytes(b)


def same_file(a, b):
    with open(a, "rb") as f:
        x = f.read()
    with open(b, "rb") as f:
        y = f.read()
    if a.lower().endswith((".dll", ".exe")):
        x, y = pe_normalized(x), pe_normalized(y)
    return x == y


def files_under(top):
    out = set()
    for root, dirs, files in os.walk(top):
        rel = os.path.relpath(root, top)
        if rel == "obj" or rel.startswith("obj" + os.sep):
            continue
        for f in files:
            out.add(os.path.normpath(os.path.join(rel, f)))
    return out


# --- running ----------------------------------------------------------------------------------------------------------
def run(cmd, cwd, env, log):
    e = dict(os.environ)
    e.update(env)
    start = time.time()
    r = subprocess.run(cmd, cwd=cwd, env=e, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    with open(log, "a", encoding="utf-8") as f:
        f.write(f"\n=== {subprocess.list2cmdline(cmd)} ({time.time() - start:.0f} s, exit {r.returncode})\n{r.stdout}")
    if r.returncode != 0:
        print(r.stdout[-3000:])
        raise SystemExit(f"check_same_bytes: {subprocess.list2cmdline(cmd)} failed (exit {r.returncode}) - {log}")
    return r.stdout


def copy_tree(dst):
    for d in ("src", "tests", "third_party", "data", "tools", "scripts"):
        r = subprocess.run(["robocopy", os.path.join(ROOT, d), os.path.join(dst, d), "/MIR", "/R:1", "/W:1", "/NFL", "/NDL", "/NJH",
                            "/NJS", "/NP", "/XD", "__pycache__", "node_modules"], stdout=subprocess.DEVNULL)
        if r.returncode >= 8:
            raise SystemExit(f"check_same_bytes: copying {d} failed (robocopy {r.returncode})")
    for f in ("build.bat", "test.bat", "core_stamp.bat", "nwn5e.ini"):
        shutil.copy2(os.path.join(ROOT, f), os.path.join(dst, f))


def main():
    scratch = os.path.abspath(arg("--scratch", os.path.join(ROOT, "build", "checks", "same_bytes")))
    tools = os.path.abspath(arg("--tools", os.environ.get("NWN5E_TOOLS") or os.path.join(ROOT, "..", "nwn5e-tools")))
    os.makedirs(scratch, exist_ok=True)
    log = os.path.join(scratch, "log.txt")
    open(log, "w").close()
    reader = os.path.join(ROOT, "build", "floatdisk", "mdlmesh.exe")
    run(["node", "-e", "require(process.argv[1]).ensureReader({ repo: process.argv[2] })",
         os.path.join(ROOT, "tools", "data", "floatdisk", "reader.js"), ROOT], ROOT, {}, log)
    env = {"NWN5E_TOOLS": tools, "NWN5E_MDLMESH": reader}
    private = os.environ.get("NWN5E_PRIVATE") or os.path.join(ROOT, "private")
    if os.path.isdir(private):
        env["NWN5E_PRIVATE"] = private
    print(f"check_same_bytes: in {scratch} ({'with' if 'NWN5E_PRIVATE' in env else 'without'} the private layer); log {log}")

    # One copy of the sources for both builds: where a source is goes into its object (MSVC names an anonymous
    # namespace from its file's path), so two copies would differ for that alone.
    tree = tree_b = os.path.join(scratch, "tree")
    out_a, out_b = os.path.join(scratch, "out_a"), os.path.join(scratch, "out_b")
    copy_tree(tree)
    test_exe = os.path.join(tree, "build", "rules_test.exe")

    # B: as they build now, a step at a time (its objects and records kept from the last run of this check).
    t = time.time()
    inc = dict(env, NWN5E_OUT=out_b, NWN5E_DERIVED_CACHE_DIR=os.path.join(ROOT, "build", "derived_cache"))
    def build_b(what):
        said = run(["cmd", "/d", "/c", os.path.join(tree_b, "build.bat")], tree_b, inc, log)
        said += run(["cmd", "/d", "/c", os.path.join(tree_b, "test.bat")], tree_b, env, log)
        lines = [l for l in said.splitlines() if l.startswith(("cl_incr:", "build_derived: cache"))]
        print(f"  B, {what}: " + "; ".join(lines))
    build_b("as it stands")
    with open(os.path.join(out_b, "nwn5e.dll"), "rb") as f:
        dll_before = f.read()
    with open(os.path.join(out_b, "core_stamp.txt")) as f:
        stamp_before = f.read()

    # The edits: a source, each target's most included project header, a file of the core stamp.
    import json
    def most_included(state):
        with open(state, encoding="utf-8") as f:
            objects = json.load(f)["objects"]
        count = {}
        src = os.path.normcase(os.path.join(tree_b, "src")) + os.sep
        for rec in objects.values():
            for d in rec["deps"]:
                if d.startswith(src) and d.endswith(".h"):
                    count[d] = count.get(d, 0) + 1
        return max(count, key=count.get), max(count.values())
    edits = {}
    rules = sorted(f for f in os.listdir(os.path.join(tree_b, "src", "rules")) if f.endswith(".cpp"))
    edits[os.path.join(tree_b, "src", "rules", rules[0])] = PROBE.format(tag="src")
    h_dll, n_dll = most_included(os.path.join(out_b, "obj", "nwn5e.dll.incr.json"))
    h_test, n_test = most_included(os.path.join(tree_b, "build", "obj", "rules", "rules_test.exe.incr.json"))
    edits[h_dll] = PROBE.format(tag="hdll")
    edits.setdefault(h_test, "")
    edits[h_test] += PROBE.format(tag="htest")
    stamp = None
    with open(os.path.join(tree_b, "core_stamp.bat"), encoding="utf-8") as f:
        for word in f.read().replace("(", " ").replace(")", " ").split():
            if word.startswith("src\\data\\") and word.endswith(".cpp") and "anim_recipe" not in word:
                stamp = os.path.join(tree_b, word)
                break
    edits[stamp] = edits.get(stamp, "") + "\n// check_same_bytes: the core stamp changes\n"
    kept = {}
    for p, add in edits.items():
        with open(p, "rb") as f:
            kept[p] = f.read()
        with open(p, "ab") as f:
            f.write(add.encode())
    print(f"  edits: {os.path.relpath(rules[0] and os.path.join('src', 'rules', rules[0]))}; {os.path.relpath(h_dll, tree_b)} "
          f"({n_dll} of the DLL's sources include it); {os.path.relpath(h_test, tree_b)} ({n_test} of the tests'); "
          f"{os.path.relpath(stamp, tree_b)} (the core stamp)")
    try:
        build_b("edited")
        with open(os.path.join(out_b, "nwn5e.dll"), "rb") as f:
            if pe_normalized(f.read()) == pe_normalized(dll_before):
                raise SystemExit("check_same_bytes: B's DLL with the edits is as before them - the edits didn't reach it")
        with open(os.path.join(out_b, "core_stamp.txt")) as f:
            if f.read() == stamp_before:
                raise SystemExit("check_same_bytes: the core stamp didn't change with its file")
    finally:
        for p, data in kept.items():
            with open(p, "wb") as f:
                f.write(data)
    build_b("the edits undone")
    test_b = os.path.join(scratch, "rules_test_b.exe")
    shutil.copy2(test_exe, test_b)
    print(f"  B, the steps: {time.time() - t:.0f} s")

    # A: every source compiled by one cl run (as the .bat files always did), every generator run - from the same copy.
    t = time.time()
    if os.path.exists(out_a):
        shutil.rmtree(out_a)
    full = dict(env, NWN5E_INCREMENTAL="off", NWN5E_DERIVED_CACHE="off")
    run(["cmd", "/d", "/c", os.path.join(tree, "build.bat")], tree, dict(full, NWN5E_OUT=out_a), log)
    run(["cmd", "/d", "/c", os.path.join(tree, "test.bat")], tree, full, log)
    print(f"  A, the full build: {time.time() - t:.0f} s")

    # The same bytes?
    fa, fb = files_under(out_a), files_under(out_b)
    bad = sorted(fa ^ fb)
    compared = 0
    for rel in sorted(fa & fb):
        compared += 1
        if not same_file(os.path.join(out_a, rel), os.path.join(out_b, rel)):
            bad.append(rel)
    compared += 1
    if not same_file(test_exe, test_b):
        bad.append("rules_test.exe")
    if bad:
        print(f"check_same_bytes: FAIL - {len(bad)} of {compared} files differ between the full build and the step-by-step one:")
        for rel in bad[:40]:
            print("  " + rel)
        return 1
    print(f"check_same_bytes: the same bytes - {compared} files (nwn5e.dll and nwn5e.exe but for their link time stamps, "
          f"rules_test.exe likewise, data\\ with the game-derived files from the cache, placeable_bounds.txt)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
