"""Rebuild only what changed (owner, 2026-11-23: "is there anyway to make the checks faster?" - then "yes, do 1 to 3
after the install"; docs/TOOLS-PLAN.md, "Faster checks"). test.bat and build.bat hand it the cl command line they ran
before; it keeps each source's object file and compiles again only the sources whose inputs changed, all of them at
once on the machine's cores (cl /MP), then links every object in the order cl would have:

  python -I scripts/cl_incr.py [--clean] [--jobs <n>] [--volatile <NAME>] [--log <file>] -- <cl's arguments>

- **What a source's object depends on** (its key, kept in <obj folder>/<output>.incr.json): the source's bytes, every
  file it included (cl /sourceDependencies: the project's headers and the compiler's and SDK's, each by its bytes), the
  compiler options (every one but /Fo, /Fe and /MP, in order: a flag or a define changed rebuilds everything) and the
  folder it runs in (a relative /I), the toolchain (cl.exe, its compiler DLLs and link.exe by size and time, INCLUDE,
  LIB) and this script's version.
- **--volatile NAME:** a define whose value changes often but only a few sources use (build.bat's NWN5E_CORE_STAMP,
  which only anim_recipe.cpp reads): its value is part of a source's key only when the source or a file it includes
  names it, so a new stamp recompiles only those. A macro reaches only code that names it, so nothing else can change.
- **A new header** that shadows an included one (the same file name, found earlier on the include path) can't be seen
  from the old includes: when a header file appears under an include folder, a source's folder or an included file's
  folder that wasn't there last time, every source that included a file of that name is compiled again.
- **A stale object is never linked:** before compiling, each out-of-date source's object (and its record) is deleted;
  cl writes an object only when it compiles; every object is checked against its record (size, time) - or, compiled
  by this run, against what cl left - right before the link, and the link takes exactly the current sources' objects:
  an object left by a removed source is deleted. A source whose files changed while cl ran (a size or time other than
  before it ran, or a new include's time inside cl's run) is linked as cl compiled it but not recorded: compiled again
  next time.
- **The link** runs cl as before, with the objects in place of the sources (the same order: the folder's order, as cl
  expands a wildcard), and is skipped when every object and the output are as the last link left them.
- **--clean** forgets the record: every source compiled (test.bat clean, build.bat clean).
- **Output:** cl's lines (into --log when given), then one line: how many sources it compiled and how many were
  up to date, and whether it linked. Exit status: cl's when it fails, else 0.
"""
import fnmatch
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

VERSION = 1  # bump when what a key covers changes: every object is rebuilt once
SOURCE_EXT = (".c", ".cpp", ".cxx", ".cc")
HEADER_EXT = (".h", ".hpp", ".hh", ".hxx", ".inl", ".inc", ".ipp")
# Options that take their value as the next argument when written alone (cl accepts "/I dir" as "/Idir").
SPLIT = {"/i", "-i", "/d", "-d", "/u", "-u", "/fi", "-fi"}


def fail(message, code=1):
    print("cl_incr: " + message, file=sys.stderr)
    sys.exit(code)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def norm(p):
    return os.path.normcase(os.path.abspath(p))


# --- cl's command line ----------------------------------------------------------------------------------------------
def expand(token):
    """A source argument as cl expands it: a wildcard by the folder's own order (FindFirstFile's, as cl's), files only."""
    if "*" not in token and "?" not in token:
        return [token]
    folder, pattern = os.path.split(token)
    try:
        entries = [e for e in os.scandir(folder or ".") if e.is_file() and fnmatch.fnmatch(e.name.lower(), pattern.lower())]
    except FileNotFoundError:
        entries = []
    if not entries:
        fail(f"no file matches {token}")
    return [os.path.join(folder, e.name) if folder else e.name for e in entries]


def parse(args):
    """cl's arguments -> {options, inputs: [(kind, path)], fo, fe, link}. Sources and other inputs keep their order."""
    options, inputs, link = [], [], []
    fo = fe = None
    i = 0
    while i < len(args):
        a = args[i]
        low = a.lower()
        if low in ("/link", "-link"):
            link = args[i + 1:]
            break
        if a[:1] in "/-" and len(a) > 1:
            if low[1:3] == "fo":
                fo = a[3:]
            elif low[1:3] == "fe":
                fe = a[3:]
                if fe.startswith(":"):
                    fe = fe[1:]
                if not fe and i + 1 < len(args):
                    i += 1
                    fe = args[i]
            elif low.startswith(("/mp", "-mp")):
                pass  # this script picks the parallelism
            else:
                options.append(a)
                if low in SPLIT and i + 1 < len(args):
                    i += 1
                    options.append(args[i])
        else:
            for p in expand(a):
                inputs.append(("source" if p.lower().endswith(SOURCE_EXT) else "other", p))
        i += 1
    if not fo or not fo.endswith(("\\", "/")):
        fail("needs /Fo<folder>\\ (the object folder)")
    if not fe:
        fail("needs /Fe:<output>")
    return {"options": options, "inputs": inputs, "fo": fo, "fe": fe, "link": link}


# --- the toolchain --------------------------------------------------------------------------------------------------
def toolchain():
    cl = shutil.which("cl")
    if not cl:
        fail("cl isn't on PATH (vcvars64.bat not called?)")
    bits = [cl.lower(), os.environ.get("INCLUDE", ""), os.environ.get("LIB", ""), str(VERSION)]
    for name in ("cl.exe", "c1.dll", "c1xx.dll", "c2.dll", "link.exe"):
        p = os.path.join(os.path.dirname(cl), name)
        if os.path.exists(p):
            st = os.stat(p)
            bits.append(f"{name}:{st.st_size}:{st.st_mtime_ns}")
    return sha("\n".join(bits).encode())


# --- the files' bytes, by stat ----------------------------------------------------------------------------------------
class Files:
    """Each file's sha-256 and the volatile names it mentions, from the last run's table when its size and time agree."""

    def __init__(self, table, volatile):
        self.old, self.new, self.volatile = table, {}, volatile

    def info(self, path):
        key = path if path in self.new or path in self.old else norm(path)  # (a dependency's path is kept normalized)
        if key in self.new:
            return self.new[key]
        try:
            st = os.stat(key)
        except OSError:
            self.new[key] = None
            return None
        old = self.old.get(key)
        if old and old[0] == st.st_size and old[1] == st.st_mtime_ns:
            got = old
        else:
            with open(key, "rb") as f:
                data = f.read()
            got = [st.st_size, st.st_mtime_ns, sha(data), [v for v in self.volatile if v.encode() in data]]
        self.new[key] = got
        return got


def header_files(folders):
    """The header files (normalized paths) under these folders, each walked whole."""
    found = set()
    for top in folders:
        for root, dirs, files in os.walk(top):
            dirs[:] = [d for d in dirs if d.lower() not in ("obj", ".git", "node_modules", "__pycache__")]
            found.update(norm(os.path.join(root, f)) for f in files if f.lower().endswith(HEADER_EXT))
    return found


def main():
    argv = sys.argv[1:]
    if "--" not in argv:
        fail("usage: cl_incr.py [--clean] [--jobs <n>] [--volatile <NAME>] [--log <file>] -- <cl's arguments>", 2)
    mine, args = argv[:argv.index("--")], argv[argv.index("--") + 1:]
    clean, jobs, volatile, log = False, os.cpu_count() or 4, [], None
    i = 0
    while i < len(mine):
        if mine[i] == "--clean":
            clean = True
        elif mine[i] == "--jobs":
            i += 1
            jobs = max(1, int(mine[i]))
        elif mine[i] == "--volatile":
            i += 1
            volatile.append(mine[i])
        elif mine[i] == "--log":
            i += 1
            log = mine[i]
        else:
            fail("unknown option " + mine[i], 2)
        i += 1
    if os.environ.get("NWN5E_JOBS", "").isdigit():
        jobs = max(1, int(os.environ["NWN5E_JOBS"]))
    cmd = parse(args)
    objdir, out = cmd["fo"], cmd["fe"]
    os.makedirs(objdir, exist_ok=True)
    depdir = os.path.join(objdir, "deps", os.path.basename(out))  # (each output its own: build.bat's DLL and exe share obj\)
    os.makedirs(depdir, exist_ok=True)
    state_file = os.path.join(objdir, os.path.basename(out) + ".incr.json")
    logf = open(log, "w", encoding="utf-8", errors="replace") if log else None

    def say_cl(text):
        if not text:
            return
        if logf:
            logf.write(text)
            logf.flush()
        else:
            sys.stdout.write(text)
            sys.stdout.flush()

    state = {}
    if os.path.exists(state_file):
        try:
            with open(state_file, encoding="utf-8") as f:
                state = json.load(f)
        except (OSError, ValueError):
            state = {}
    if clean or state.get("version") != VERSION:
        # Forgotten: every object it made (and its includes' record) deleted, every source compiled.
        for name in (state.get("objects") or {}):
            if os.path.exists(os.path.join(objdir, name)):
                os.remove(os.path.join(objdir, name))
        for f in os.listdir(depdir):
            os.remove(os.path.join(depdir, f))
        state = {}
    old_objects = state.get("objects", {})

    # The options as the key sees them: a volatile define's value left out (it goes into the keys of the sources that
    # name it).
    def volatile_of(opt):
        low = opt.lower()
        for v in volatile:
            for p in ("/d", "-d"):
                if low.startswith(p + v.lower() + "=") or low == p + v.lower():
                    return v
        return None

    keyed_options, volatile_values = [], {}
    for o in cmd["options"]:
        v = volatile_of(o)
        if v:
            volatile_values[v] = o
            keyed_options.append("/D" + v + "=<volatile>")
        else:
            keyed_options.append(o)
    tool = toolchain()
    # (the folder it runs in too: a relative /I finds other headers from elsewhere)
    flags = sha(("\n".join(keyed_options) + "\n" + norm(os.getcwd()) + "\n" + tool).encode())
    # (each file's record says which volatile names it holds: a different --volatile reads every file again)
    files = Files(state.get("files", {}) if state.get("volatile") == volatile else {}, volatile)

    # The sources, their objects (one folder: a name twice is refused, as build.bat does).
    sources, by_obj = [], {}
    for kind, p in cmd["inputs"]:
        if kind != "source":
            continue
        obj = os.path.splitext(os.path.basename(p))[0] + ".obj"
        if obj.lower() in by_obj:
            fail(f"two sources make {obj}: {by_obj[obj.lower()]} and {p}")
        by_obj[obj.lower()] = p
        sources.append((p, obj))

    # The header files now under the include folders, the sources' folders and the folders of what they included; one
    # that wasn't there last time (under a folder looked through then too) marks the sources that included a file of
    # its name - it may shadow it.
    roots = set()
    opts = cmd["options"]
    for k, o in enumerate(opts):
        low = o.lower()
        if low.startswith(("/i", "-i")) and not low.startswith(("/ifc", "-ifc")):
            d = o[2:] or (opts[k + 1] if k + 1 < len(opts) else "")
            if d and os.path.isdir(d):
                roots.add(norm(d))
    for p, _ in sources:
        roots.add(norm(os.path.dirname(p) or "."))
    for rec in old_objects.values():
        for d in rec.get("deps", []):
            if not any(d.startswith(s) for s in system_dirs()):
                roots.add(os.path.dirname(d))
    headers_now = header_files(sorted(roots))
    headers_before = set(state.get("header_files", []))
    roots_before = state.get("header_roots", [])
    appeared = {os.path.basename(h) for h in headers_now - headers_before
                if any(h.startswith(r + os.sep) for r in roots_before)}

    def key_of(p, deps):
        src = files.info(p)
        if src is None:
            return None
        parts = [flags, norm(p), src[2]]
        mentioned = set(src[3])
        for d in deps:
            got = files.info(d)
            if got is None:
                return None
            parts.append(d + ":" + got[2])
            mentioned.update(got[3])
        for v in sorted(mentioned):
            parts.append(v + "=" + volatile_values.get(v, "<undefined>"))
        return sha("\n".join(parts).encode())

    def obj_stat(path):
        try:
            st = os.stat(path)
            return [st.st_size, st.st_mtime_ns]
        except OSError:
            return None

    # Which are out of date.
    stale, records = [], {}
    for p, obj in sources:
        objpath = os.path.join(objdir, obj)
        rec = old_objects.get(obj.lower())
        ok = (rec is not None and rec.get("src") == norm(p) and obj_stat(objpath) == rec.get("obj")
              and not (appeared and any(os.path.basename(d) in appeared for d in rec.get("deps", [])))
              and key_of(p, rec.get("deps", [])) == rec.get("key"))
        if ok:
            records[obj.lower()] = rec
        else:
            stale.append((p, obj))
    # An object no current source makes: deleted (never linked).
    for name, rec in old_objects.items():
        if name not in by_obj:
            for f in (os.path.join(objdir, name), os.path.join(depdir, os.path.basename(rec.get("src", "")) + ".json")):
                if os.path.exists(f):
                    os.remove(f)

    def save():
        new = {"version": VERSION, "objects": records, "header_files": sorted(headers_now), "header_roots": sorted(roots),
               "link": state.get("link"), "volatile": volatile,
               "files": {k: v for k, v in files.new.items() if v is not None}}
        tmp = state_file + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(new, f)
        os.replace(tmp, state_file)

    compiled = 0
    fresh = {}  # objects this run's cl made: linked, recorded only when nothing they read changed while cl ran
    unrecorded = []
    if stale:
        # What each out-of-date source read last time (and the source), by size and time before cl runs.
        before = {}
        for p, obj in stale:
            rec = old_objects.get(obj.lower()) or {}
            for x in [norm(p)] + rec.get("deps", []):
                try:
                    st = os.stat(x)
                    before[x] = (st.st_size, st.st_mtime_ns)
                except OSError:
                    before[x] = None
        for p, obj in stale:
            for f in (os.path.join(objdir, obj), os.path.join(depdir, os.path.basename(p) + ".json")):
                if os.path.exists(f):
                    os.remove(f)
        state["link"] = None
        started = time.time_ns()
        compile_options = [o for o in cmd["options"]] + ["/c", "/MP" + str(jobs), "/Fo" + objdir, "/sourceDependencies", depdir + os.sep]
        r = subprocess.run(["cl"] + compile_options + [p for p, _ in stale], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        finished = time.time_ns()
        say_cl(r.stdout.decode("utf-8", "replace").replace("\r\n", "\n"))

        def changed_while(x):
            """Did x change while cl ran? Known before: its size or time differ; new: its time falls in cl's run."""
            try:
                st = os.stat(x)
            except OSError:
                return True
            if x in before:
                return before[x] != (st.st_size, st.st_mtime_ns)
            return started <= st.st_mtime_ns <= finished

        for p, obj in stale:
            objpath = os.path.join(objdir, obj)
            st = obj_stat(objpath)
            if not st:
                continue  # not compiled (an error): never linked, compiled again next time
            fresh[obj.lower()] = st
            deps = None
            try:
                with open(os.path.join(depdir, os.path.basename(p) + ".json"), encoding="utf-8") as f:
                    deps = [norm(d) for d in json.load(f)["Data"]["Includes"]]
            except (OSError, ValueError, KeyError):
                pass
            files.new.pop(norm(p), None)
            for d in deps or []:
                files.new.pop(d, None)
            key = key_of(p, deps) if deps is not None else None
            if key and not any(changed_while(x) for x in [norm(p)] + deps):
                records[obj.lower()] = {"src": norm(p), "key": key, "deps": deps, "obj": st}
                compiled += 1
            else:
                unrecorded.append(p)  # linked as this run made it; compiled again next time
        if r.returncode != 0:
            save()
            if logf:
                logf.close()
            print(f"cl_incr: {os.path.basename(out)}: compiling failed ({len(stale)} of {len(sources)} sources out of date)")
            sys.exit(r.returncode)
        missing = [p for p, obj in stale if obj.lower() not in fresh]
        if missing:
            save()
            fail(f"cl made no object for {len(missing)} source(s): {', '.join(missing[:5])} - not linked")

    # The link: every object, in cl's order, each checked against its record (or as this run's cl made it) first.
    link_inputs = []
    for kind, p in cmd["inputs"]:
        if kind == "source":
            obj = os.path.splitext(os.path.basename(p))[0] + ".obj"
            objpath = os.path.join(objdir, obj)
            rec = records.get(obj.lower())
            made = rec["obj"] if rec else fresh.get(obj.lower())
            if not made or obj_stat(objpath) != made:
                save()
                fail(f"{objpath} isn't the object its record says - not linked; run it again")
            link_inputs.append(objpath)
        else:
            link_inputs.append(p)
    link_key = sha("\n".join([flags, tool] + cmd["options"] + ["/Fe" + norm(out)] + cmd["link"] +
                             [x + ":" + json.dumps(obj_stat(x)) for x in link_inputs]).encode())
    last = state.get("link") or {}
    linked = False
    if last.get("key") == link_key and obj_stat(out) == last.get("out"):
        pass  # every object and the output as the last link left them
    else:
        if os.path.exists(out):
            os.remove(out)
        r = subprocess.run(["cl"] + cmd["options"] + link_inputs + ["/Fe:" + out] + (["/link"] + cmd["link"] if cmd["link"] else []),
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        say_cl(r.stdout.decode("utf-8", "replace").replace("\r\n", "\n"))
        if r.returncode != 0:
            state["link"] = None
            save()
            if logf:
                logf.close()
            print(f"cl_incr: {os.path.basename(out)}: linking failed")
            sys.exit(r.returncode)
        state["link"] = {"key": link_key, "out": obj_stat(out)}
        linked = True
    save()
    if logf:
        logf.close()
    print(f"cl_incr: {os.path.basename(out)}: compiled {len(fresh)} of {len(sources)} sources ({len(sources) - len(stale)} up to date)"
          f"{', cl /MP' + str(jobs) if stale else ''}; {'linked' if linked else 'link up to date'}{' (--clean)' if clean else ''}"
          + (f"; {len(unrecorded)} changed while cl ran - compiled again next time" if unrecorded else ""))


_SYSTEM = None


def system_dirs():
    """The compiler's and SDK's include folders (INCLUDE), lower case: their headers change with the toolchain."""
    global _SYSTEM
    if _SYSTEM is None:
        _SYSTEM = tuple(norm(d) for d in os.environ.get("INCLUDE", "").split(";") if d)
    return _SYSTEM


if __name__ == "__main__":
    main()
