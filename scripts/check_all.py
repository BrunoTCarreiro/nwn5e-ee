"""Every check at once (owner, 2026-11-23: "is there anyway to make the checks faster?" - then "yes, do 1 to 3 after the
install"; docs/TOOLS-PLAN.md, "Faster checks"). The independent checks run side by side on the machine's cores; the
bench's checks that start a server (and save through the Composer's server, whose temporary files one of them counts)
run one after another, in a lane of their own. Then one summary: each check's result, its time and its last line.

  python -I scripts/check_all.py [--only a,b] [--skip a,b] [--list] [--clean] [--build <the real build>]
                                 [--tools <nwn5e-tools>] [--out <scratch folder>] [--serial]

- **The checks:** test.bat; build.bat into a scratch folder (NWN5E_OUT=--out, default build\\checks\\dll); tools\\data\\
  check_derived.js; the faster checks' own (scripts\\check_cl_incr.py, tools\\data\\check_derived_cache.js);
  nwn5e-tools' VFX Bench checks from its README: check.js and check_parts.js (on --build's
  vfxedit\\), check_effect.js, check_beats.js, check_chunks.js, check_textures.js, check_composer.js, and in the server
  lane check_composer_server.js, browser_check.mjs (--ee this checkout, --build) and composer_check.mjs. Each runs as
  its own command does (each still runs alone, as before); --ee is this checkout. The server lane starts once
  test.bat and build.bat are done: their compiles load every core, and under that load check_composer_server.js lost
  requests (read ECONNRESET) - with nothing to compile they're done in seconds.
- **Before them:** the mesh reader (build\\floatdisk\\mdlmesh.exe) is built when it's missing or older than its sources,
  once - the derived build, check_derived.js and the Composer's checks all run it, and two rebuilding it at once
  would collide.
- **--clean:** test.bat clean and build.bat clean (every source compiled, every game-derived model built again).
- **--build:** the real build whose vfxedit\\ library the bench's checks read (NWN5E_BUILD, else this checkout's
  build\\); from a worktree, the main checkout's. **--tools:** NWN5E_TOOLS, else ..\\nwn5e-tools.
- **Logs:** each check's whole output in build\\checks\\<name>.log (overwritten each run).
- **Exit status:** 0 when every check passed, else 1.
"""
import os
import subprocess
import sys
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEAVY = ("test.bat", "build.bat")  # cl /MP on every core: the server lane starts after them


def arg(name, default=None):
    if name in sys.argv:
        i = sys.argv.index(name)
        if i + 1 < len(sys.argv):
            return sys.argv[i + 1]
    return default


def checks(tools, build, out, clean):
    """[(name, command, cwd, extra env, lane)] - lane None runs beside the others; a named lane, one at a time."""
    node = "node"
    bench = os.path.join(tools, "vfx_bench")
    vfxedit = os.path.join(build, "vfxedit")
    c = ["clean"] if clean else []
    return [
        ("test.bat", ["cmd", "/d", "/c", os.path.join(ROOT, "test.bat")] + c, ROOT, {}, None),
        ("build.bat", ["cmd", "/d", "/c", os.path.join(ROOT, "build.bat")] + c, ROOT, {"NWN5E_OUT": out, "NWN5E_TOOLS": tools}, None),
        ("check_derived", [node, os.path.join(ROOT, "tools", "data", "check_derived.js"), "--tools", tools], ROOT, {}, None),
        # The faster checks' own: the incremental compile and the derived cache on projects of their own.
        ("check_cl_incr", [sys.executable, "-I", os.path.join(ROOT, "scripts", "check_cl_incr.py")], ROOT, {}, None),
        ("check_derived_cache", [node, os.path.join(ROOT, "tools", "data", "check_derived_cache.js")], ROOT, {}, None),
        ("bench check", [node, os.path.join(bench, "check.js"), vfxedit, "--ee", ROOT], tools, {}, None),
        ("bench check_parts", [node, os.path.join(bench, "check_parts.js"), vfxedit], tools, {}, None),
        ("bench check_effect", [node, os.path.join(bench, "check_effect.js")], tools, {}, None),
        ("bench check_beats", [node, os.path.join(bench, "check_beats.js")], tools, {}, None),
        ("bench check_chunks", [node, os.path.join(bench, "check_chunks.js")], tools, {}, None),
        ("bench check_textures", [node, os.path.join(bench, "check_textures.js"), "--ee", ROOT], tools, {}, None),
        ("bench check_composer", [node, os.path.join(bench, "check_composer.js"), "--ee", ROOT], tools, {}, None),
        # The server lane: each starts the bench's server (ports 18776, 18766, 18778) and saves through the Composer's
        # routes, whose temporary files (n5_composer-*.json in %TEMP%) check_composer_server.js counts.
        ("bench check_composer_server", [node, os.path.join(bench, "check_composer_server.js"), "--ee", ROOT], tools, {}, "server"),
        ("bench browser_check", [node, os.path.join(bench, "browser_check.mjs"), "--ee", ROOT, "--build", build], tools, {}, "server"),
        ("bench composer_check", [node, os.path.join(bench, "composer_check.mjs"), "--ee", ROOT], tools, {}, "server"),
    ]


def last_line(text):
    for line in reversed(text.splitlines()):
        if line.strip():
            return line.strip()
    return ""


def main():
    tools = os.path.abspath(arg("--tools", os.environ.get("NWN5E_TOOLS") or os.path.join(ROOT, "..", "nwn5e-tools")))
    build = os.path.abspath(arg("--build", os.environ.get("NWN5E_BUILD") or os.path.join(ROOT, "build")))
    out = os.path.abspath(arg("--out", os.path.join(ROOT, "build", "checks", "dll")))
    clean = "--clean" in sys.argv
    every = checks(tools, build, out, clean)
    if "--list" in sys.argv:
        for name, cmd, cwd, env, lane in every:
            print(f"{name:28} {'(' + lane + ' lane) ' if lane else ''}{subprocess.list2cmdline(cmd)}")
        return 0
    only, skip = arg("--only"), arg("--skip")
    if only:
        want = [x.strip() for x in only.split(",")]
        every = [c for c in every if any(w in c[0] for w in want)]
    if skip:
        drop = [x.strip() for x in skip.split(",")]
        every = [c for c in every if not any(d in c[0] for d in drop)]
    if not os.path.isdir(os.path.join(tools, "vfx_bench")):
        print(f"check_all: nwn5e-tools isn't at {tools} - pass --tools <folder> or set NWN5E_TOOLS")
        return 1
    logs = os.path.join(ROOT, "build", "checks")
    os.makedirs(logs, exist_ok=True)

    # The mesh reader, once, before anything runs it.
    t0 = time.time()
    r = subprocess.run(["node", "-e", "require(process.argv[1]).ensureReader({ repo: process.argv[2] })",
                        os.path.join(ROOT, "tools", "data", "floatdisk", "reader.js"), ROOT], cwd=ROOT,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if r.returncode != 0:
        print("check_all: the mesh reader doesn't build:\n" + r.stdout)
        return 1
    if r.stdout.strip():
        print("check_all: " + last_line(r.stdout))

    results = {}
    lock = threading.Lock()

    def run_one(name, cmd, cwd, env):
        log = os.path.join(logs, name.replace(" ", "_") + ".log")
        e = dict(os.environ)
        e.update(env)
        start = time.time()
        with open(log, "w", encoding="utf-8", errors="replace") as f:
            try:
                code = subprocess.run(cmd, cwd=cwd, env=e, stdout=f, stderr=subprocess.STDOUT).returncode
            except OSError as x:
                f.write(f"check_all: couldn't start it - {x}\n")
                code = -1
        took = time.time() - start
        with open(log, encoding="utf-8", errors="replace") as f:
            text = f.read()
        with lock:
            results[name] = (code, took, last_line(text), log)
            print(f"{'ok  ' if code == 0 else 'FAIL'} {name} ({took:.0f} s)", flush=True)

    # The server lane waits for test.bat and build.bat: their cl /MP runs load every core, and under that load the
    # Composer server's check lost requests (read ECONNRESET, 2 of its steps, in the first cold run side by side).
    compiling = [c for c in every if c[0] in HEAVY]
    compiled = threading.Event()
    left = [len(compiling)]
    if not compiling:
        compiled.set()

    def heavy_done():
        with lock:
            left[0] -= 1
            if left[0] <= 0:
                compiled.set()

    def heavy_runner(name, cmd, cwd, env):
        try:
            run_one(name, cmd, cwd, env)
        finally:
            heavy_done()

    def lane_runner(items, wait=False):
        if wait:
            compiled.wait()
        for name, cmd, cwd, env, _ in items:
            run_one(name, cmd, cwd, env)

    serial = "--serial" in sys.argv
    threads = []
    lanes = {}
    for c in every:
        if serial:
            lanes.setdefault("all", []).append(c)
        elif c[4]:
            lanes.setdefault(c[4], []).append(c)
        elif c[0] in HEAVY:
            threads.append(threading.Thread(target=heavy_runner, args=c[:4]))
        else:
            threads.append(threading.Thread(target=run_one, args=c[:4]))
    for lane, items in lanes.items():
        threads.insert(0, threading.Thread(target=lane_runner, args=(items, lane == "server")))
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    wall = time.time() - t0
    print()
    print(f"{'':4} {'check':28} {'time':>6}  last line")
    failed = 0
    for name, *_ in every:
        code, took, line, log = results.get(name, (-1, 0, "(didn't run)", ""))
        failed += code != 0
        if len(line) > 110:
            line = line[:107] + "..."
        print(f"{'ok  ' if code == 0 else 'FAIL'} {name:28} {took:5.0f}s  {line}")
    print(f"\n{len(every) - failed} of {len(every)} passed in {wall:.0f} s"
          f"{' (one at a time)' if serial else ''}; logs in {os.path.relpath(logs, ROOT)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
