# The test module's ERF writer (#156): a module archive that reads back, deterministically.
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import erf

fails = []


def check(what, ok, detail=""):
    print(("ok   " if ok else "FAIL ") + what + (("  " + str(detail)) if detail != "" else ""))
    if not ok:
        fails.append(what)


entries = [("Module", 2014, b"ifo"), ("n5test_field", 2012, b"are")]
data = erf.write_erf("MOD ", entries)
check("the header says MOD V1.0", data[0:8] == b"MOD V1.0", data[0:8])
check("two entries", struct.unpack_from("<I", data, 16)[0] == 2)
back = erf.read_erf(data)
check("it reads back, resrefs lower-cased", sorted(back) == [("module", 2014, b"ifo"), ("n5test_field", 2012, b"are")], back)
check("the same entries give the same bytes", erf.write_erf("MOD ", list(reversed(entries))) == data)

print(f"FAILED: {len(fails)}" if fails else "all erf checks pass")
sys.exit(1 if fails else 0)
