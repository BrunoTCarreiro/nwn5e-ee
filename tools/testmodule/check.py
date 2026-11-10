# Runs every check_*.py beside it (the test module's checks, #156), each in its own process; prints each one's last
# line and exits 1 when any fails.
#   python -I tools/testmodule/check.py [--game DIR]
import os
import subprocess
import sys

here = os.path.dirname(os.path.abspath(__file__))
failed = 0
names = sorted(f for f in os.listdir(here) if f.startswith("check_") and f.endswith(".py"))
for name in names:
    r = subprocess.run([sys.executable, "-I", os.path.join(here, name)] + sys.argv[1:], capture_output=True, text=True)
    lines = [l for l in (r.stdout + r.stderr).splitlines() if l.strip()]
    ok = r.returncode == 0
    failed += 0 if ok else 1
    print(("ok   " if ok else "FAIL ") + name + ": " + (lines[-1] if lines else "(no output)"))
    if not ok:
        for l in [l for l in lines if l.startswith("FAIL")][:8]:
            print("       " + l)
print("all testmodule checks pass" if not failed else f"{failed} of {len(names)} checks failed")
sys.exit(1 if failed else 0)
