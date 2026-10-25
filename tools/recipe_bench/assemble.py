"""The Animation Recipe Bench as one HTML file: page.html with core.js inlined.

usage: python tools/recipe_bench/assemble.py [out.html]   (default: build/recipe_bench.html)

The result is what is published as the web page; opened straight from disk it
works too, without the Save button (Copy instead). check.js runs core.js and the
page's library lookups on a folder of libraries (build/animedit/libraries):
  node tools/recipe_bench/check.js build/animedit/libraries
"""
import os
import sys

here = os.path.dirname(os.path.abspath(__file__))
page = open(os.path.join(here, "page.html"), encoding="utf-8").read()
core = open(os.path.join(here, "core.js"), encoding="utf-8").read()
assert page.count("/*CORE*/") == 1, "page.html needs exactly one /*CORE*/ marker"
out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "build", "recipe_bench.html")
open(out, "w", encoding="utf-8", newline="\n").write(page.replace("/*CORE*/", core))
print("wrote", os.path.normpath(out))
