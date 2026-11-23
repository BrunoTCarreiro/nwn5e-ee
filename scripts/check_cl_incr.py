"""scripts/cl_incr.py on a small project of its own (docs/TOOLS-PLAN.md, "Faster checks"), in a temporary folder - no
game, VS 2019 Build Tools as build.bat:

  python -I scripts/check_cl_incr.py

Three sources, a header two of them include and one included by that one, a define one source names: each change
compiles exactly the sources it reaches, the program shows it, and nothing stale is linked - a source's bytes (the same
bytes with a new time: nothing), a header (and one it includes), the flags, the volatile define's value (only the source
naming it, or including a header that does), a new header that shadows an included one, a removed source (its object
deleted, not linked), an object changed behind its back, a compile error (nothing linked; the next run compiles only
what failed), the output deleted (linked again, nothing compiled), --clean (everything), and the objects linked in the
folder's order.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VCVARS = r"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"


def vc_env():
    r = subprocess.run(f'cmd /d /c "call "{VCVARS}" >nul && set"', capture_output=True, text=True, shell=False)
    env = {}
    for line in r.stdout.splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            env[k] = v
    if "INCLUDE" not in env:
        raise SystemExit("check_cl_incr: vcvars64.bat gave no INCLUDE - VS 2019 Build Tools missing?")
    return env


def main():
    if not os.path.exists(VCVARS):
        print("skip: no VS 2019 Build Tools")
        return 0
    env = vc_env()
    tmp = tempfile.mkdtemp(prefix="n5_clincr-")
    src, inc1, inc2, obj = (os.path.join(tmp, d) for d in ("src", "inc1", "inc2", "obj"))
    for d in (src, inc1, inc2):
        os.makedirs(d)
    out = os.path.join(tmp, "prog.exe")

    def put(path, text):
        with open(path, "w", newline="\n") as f:
            f.write(text)

    put(os.path.join(src, "a.cpp"), '#include "common.h"\nint A() { return kCommon + 1; }\n')
    put(os.path.join(src, "b.cpp"), '#include "common.h"\nint B() { return kCommon * 10; }\n')
    put(os.path.join(src, "main.cpp"), '#include <cstdio>\nint A(); int B(); int C();\n'
        'int main() { std::printf("%d %d %d %s\\n", A(), B(), C(), STAMP); return 0; }\n'
        '#ifndef STAMP\n#define STAMP "none"\n#endif\n')
    put(os.path.join(src, "c.cpp"), '#include "other.h"\nint C() { return kOther; }\n')
    put(os.path.join(inc2, "common.h"), '#pragma once\n#include "deep.h"\nconstexpr int kCommon = kDeep + 1;\n')
    put(os.path.join(inc2, "deep.h"), '#pragma once\nconstexpr int kDeep = 1;\n')
    put(os.path.join(inc2, "other.h"), '#pragma once\nconstexpr int kOther = 7;\n')
    flags = ["/nologo", "/O2", "/MT", "/EHsc", "/std:c++17", "/I" + inc1, "/I" + inc2]

    def build(*extra, opts=(), stamp="s1", ok=True, cwd=None):
        args = [sys.executable, "-I", os.path.join(ROOT, "scripts", "cl_incr.py"), *opts, "--volatile", "STAMP", "--",
                *flags, *extra, '/DSTAMP="' + stamp + '"', "/Fo" + obj + os.sep, os.path.join(src, "*.cpp"), "/Fe:" + out]
        r = subprocess.run(args, env=env, capture_output=True, text=True, cwd=cwd or tmp)
        if ok and r.returncode != 0:
            raise AssertionError("build failed: " + (r.stdout + r.stderr)[-1500:])
        if not ok and r.returncode == 0:
            raise AssertionError("build passed: " + r.stdout[-500:])
        m = re.search(r"compiled (\d+) of (\d+) sources .*?; (linked|link up to date)", r.stdout)
        return (int(m.group(1)), m.group(3) == "linked") if m else (None, None)

    def runs():
        return subprocess.run([out], capture_output=True, text=True).stdout.strip()

    def touch(path):
        t = time.time() + 2
        os.utime(path, (t, t))

    failed = 0

    def check(name, fn):
        nonlocal failed
        try:
            fn()
            print("ok   " + name)
        except Exception as e:  # noqa: BLE001 - a check's failure, said
            failed += 1
            print(f"FAIL {name}: {e}")

    def eq(got, want, what=""):
        if got != want:
            raise AssertionError(f"{what} {got!r}, not {want!r}")

    try:
        check("the first build compiles all 4 and links", lambda: (eq(build(), (4, True)), eq(runs(), "3 20 7 s1")))
        check("nothing changed: nothing compiled, the link up to date", lambda: eq(build(), (0, False)))

        def source():
            put(os.path.join(src, "a.cpp"), '#include "common.h"\nint A() { return kCommon + 2; }\n')
            eq(build(), (1, True))
            eq(runs(), "4 20 7 s1")
            touch(os.path.join(src, "a.cpp"))
            eq(build(), (0, False), "the same bytes, a new time:")
        check("a source's bytes: that one; the same bytes with a new time: none", source)

        def header():
            put(os.path.join(inc2, "common.h"), '#pragma once\n#include "deep.h"\nconstexpr int kCommon = kDeep + 2;\n')
            eq(build(), (2, True))
            eq(runs(), "5 30 7 s1")
            put(os.path.join(inc2, "deep.h"), '#pragma once\nconstexpr int kDeep = 2;\n')
            eq(build(), (2, True), "a header it includes:")
            eq(runs(), "6 40 7 s1")
        check("a header: the two that include it; a header that one includes: the same two", header)

        check("a flag: everything; the same flags from another folder (a relative /I would find other headers): everything",
              lambda: (eq(build("/DEXTRA=1"), (4, True)), eq(build(), (4, True), "and back:"),
                       eq(build(cwd=src), (4, True), "from src\\:"), eq(build(), (4, True), "and back:")))

        def volatile():
            eq(build(stamp="s2"), (1, True))
            eq(runs(), "6 40 7 s2")
            put(os.path.join(inc2, "other.h"), '#pragma once\nconstexpr int kOther = 7;\n#ifdef STAMP\n#endif\n')
            eq(build(stamp="s2"), (1, True), "a header now naming it (its own change):")
            eq(build(stamp="s3"), (2, True), "the value again, c.cpp's header naming it too:")
            eq(runs(), "6 40 7 s3")
        check("the volatile define's value: only the sources naming it, or including a header that does", volatile)

        def shadow():
            put(os.path.join(inc1, "other.h"), '#pragma once\nconstexpr int kOther = 8;\n')
            t = time.time() + 120  # (a time ahead of the clock - a copy from elsewhere - isn't "changed while cl ran")
            os.utime(os.path.join(inc1, "other.h"), (t, t))
            eq(build(stamp="s3"), (1, True))
            eq(runs(), "6 40 8 s3")
            eq(build(stamp="s3"), (0, False), "again, its time ahead of the clock:")
            os.remove(os.path.join(inc1, "other.h"))
            eq(build(stamp="s3"), (1, True), "the shadow gone:")
            eq(runs(), "6 40 7 s3")
        check("a new header shadowing an included one (earlier on the path): its includers (kept, though its time is ahead "
              "of the clock); gone again: the same", shadow)

        def removed():
            put(os.path.join(src, "d.cpp"), 'int D() { return 4; }\n')
            eq(build(stamp="s3"), (1, True))
            if not os.path.exists(os.path.join(obj, "d.obj")):
                raise AssertionError("d.obj not made")
            os.remove(os.path.join(src, "d.cpp"))
            eq(build(stamp="s3"), (0, True), "d.cpp removed:")
            if os.path.exists(os.path.join(obj, "d.obj")):
                raise AssertionError("d.obj left behind")
        check("a removed source: its object deleted, not linked", removed)

        def tampered():
            with open(os.path.join(obj, "b.obj"), "r+b") as f:
                f.seek(100)
                f.write(b"\0\0\0\0")
            eq(build(stamp="s3"), (1, True))
            eq(runs(), "6 40 7 s3")
        check("an object changed behind its back: compiled again", tampered)

        def error():
            put(os.path.join(src, "b.cpp"), '#include "common.h"\nint B() { return kCommon * 10 }\n')
            before = os.path.getmtime(out)
            build(stamp="s3", ok=False)
            if os.path.exists(os.path.join(obj, "b.obj")):
                raise AssertionError("b.obj after a failed compile")
            if os.path.getmtime(out) != before:
                raise AssertionError("linked after a failed compile")
            put(os.path.join(src, "b.cpp"), '#include "common.h"\nint B() { return kCommon * 100; }\n')
            eq(build(stamp="s3"), (1, True), "fixed:")
            eq(runs(), "6 400 7 s3")
        check("a compile error: nothing linked, its object gone; fixed: only it compiled", error)

        def output():
            os.remove(out)
            eq(build(stamp="s3"), (0, True))
            eq(runs(), "6 400 7 s3")
        check("the output deleted: linked again, nothing compiled", output)

        check("--clean: everything", lambda: eq(build(stamp="s3", opts=("--clean",)), (4, True)))

        def order():
            # Names whose order differs between a plain sort and the folder's (NTFS: upper case, "_" after the letters).
            for name in ("ab_x.cpp", "abc.cpp", "Ab.cpp"):
                put(os.path.join(tmp, "order", name) if os.path.isdir(os.path.join(tmp, "order")) else (os.makedirs(os.path.join(tmp, "order")) or os.path.join(tmp, "order", name)), "")
            r = subprocess.run(["cmd", "/d", "/c", "dir", "/b", os.path.join(tmp, "order", "*.cpp")], capture_output=True, text=True)
            sys.path.insert(0, os.path.join(ROOT, "scripts"))
            import cl_incr
            eq([os.path.basename(p) for p in cl_incr.expand(os.path.join(tmp, "order", "*.cpp"))], r.stdout.split(), "expanded:")
        check("a wildcard expanded in the folder's order, as cl expands it (the link's order)", order)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f"cl_incr check: {failed} failed" if failed else "cl_incr check: all pass")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
