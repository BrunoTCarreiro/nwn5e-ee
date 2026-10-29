# Checks the nwn5e mocap points add-on in Blender, without a window:
#   blender --background --factory-startup --python tools/mocap_blender/check.py -- <points.csv> <out folder>
# Imports a take (the bench's Export points for Blender: marked standing / keep), checks the
# empties, the keys, the stick figure and the marks (the scene's range on the kept part, the
# standing markers), edits one key (the left ankle 10 cm up at one frame), exports, and checks
# every value came back - the edited one changed. Then narrows the scene's range and exports
# again: the kept part follows the range, the standing part and every frame still written.
import csv
import importlib.util
import os
import sys

import bpy

args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
points_csv, out_dir = args[0], args[1]
here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("nwn5e_mocap_points", os.path.join(here, "nwn5e_mocap_points.py"))
addon = importlib.util.module_from_spec(spec)
spec.loader.exec_module(addon)
addon.register()
failed = []


def check(ok, what):
    print(("ok   " if ok else "FAIL ") + what)
    if not ok:
        failed.append(what)


def table(path):
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.reader(f))
    return rows[0], [[float(v) if v else float("nan") for v in r] for r in rows[1:]]


def marked(rows, column):
    return [k for k in range(len(rows)) if rows[k][column] > 0]


def runs(rows):
    out = []
    for k in rows:
        if out and k == out[-1][1] + 1:
            out[-1][1] = k
        else:
            out.append([k, k])
    return ",".join("%d:%d" % tuple(r) for r in out)


h0, a = table(points_csv)
si, ki = h0.index("standing"), h0.index("keep")
li, ri = h0.index("left_planted"), h0.index("right_planted")
st, kp = marked(a, si), marked(a, ki)
check(bool(st) and bool(kp), "the file marks a standing part and a kept part (%d and %d frames)" % (len(st), len(kp)))
check(bool(marked(a, li)) and bool(marked(a, ri)), "the file marks planted feet (%d left, %d right stretches)" %
      (len(runs(marked(a, li)).split(",")), len(runs(marked(a, ri)).split(","))))

ctx = bpy.context
for o in list(bpy.data.objects):
    bpy.data.objects.remove(o)
coll, count, fps = addon.import_points(ctx, points_csv)
names = coll[addon.KEY_POINTS].split(",")
check(len(names) == 33, "33 points imported (%d)" % len(names))
check(fps == 30 and ctx.scene.render.fps == 30, "the scene at 30 a second (%d)" % ctx.scene.render.fps)
check(count == len(a), "every row a frame (%d of %d)" % (count, len(a)))
check(list(coll[addon.KEY_STANDING]) == [st[0], st[-1]], "the standing part stored: frames %d-%d" % (st[0], st[-1]))
check(coll[addon.KEY_PLANTS + "left"] == runs(marked(a, li)) and coll[addon.KEY_PLANTS + "right"] == runs(marked(a, ri)),
      "the planted feet stored as they came (left %s)" % coll[addon.KEY_PLANTS + "left"][:40])
check((ctx.scene.frame_start, ctx.scene.frame_end) == (kp[0], kp[-1]),
      "the scene plays the kept part: frames %d-%d (the file's %d-%d)" % (ctx.scene.frame_start, ctx.scene.frame_end, kp[0], kp[-1]))
markers = sorted((m.name, m.frame) for m in ctx.scene.timeline_markers)
check(markers == sorted([("standing", st[0]), ("standing end", st[-1])]), "the standing part's markers: %s" % markers)
ankle = coll.objects[addon.PREFIX + "left_ankle"]
fcs = list(addon_fc for addon_fc in ankle.animation_data.action.layers[0].strips[0].channelbags[0].fcurves) \
    if hasattr(ankle.animation_data.action, "layers") else []
check(len(fcs) == 3 and all(len(fc.keyframe_points) == count for fc in fcs), "the left ankle keyed every frame on x, y, z")
figure = coll.objects[addon.FIGURE]
check(len(figure.data.vertices) == 33 and len(figure.data.edges) >= 25 and len(figure.modifiers) == 33,
      "the stick figure: 33 hooked points, %d lines" % len(figure.data.edges))
# The figure follows the empties (frame 200).
ctx.scene.frame_set(min(200, count - 1))
deps = ctx.evaluated_depsgraph_get()
ev = figure.evaluated_get(deps)
wrist = names.index("right_wrist")
dist = (ev.matrix_world @ ev.data.vertices[wrist].co - coll.objects[addon.PREFIX + "right_wrist"].matrix_world.translation).length
check(dist < 1e-4, "the stick figure's wrist on its empty at frame %d (%.6f m off)" % (ctx.scene.frame_current, dist))
# Facing Blender's front: the nose in front of the ears is -Y.
ctx.scene.frame_set(10)
nose = coll.objects[addon.PREFIX + "nose"].matrix_world.translation
ears = (coll.objects[addon.PREFIX + "left_ear"].matrix_world.translation + coll.objects[addon.PREFIX + "right_ear"].matrix_world.translation) / 2
check(nose.y < ears.y, "the performer faces -Y, Blender's front (nose %.3f, ears %.3f)" % (nose.y, ears.y))
# An edit: the left ankle 10 cm up at frame 100.
z = fcs[2]
z.keyframe_points[100].co.y += 0.1
z.update()
full = os.path.join(out_dir, "check_points_clean.csv")
trim = os.path.join(out_dir, "check_points_trim.csv")
n_full, keep_full, standing_full = addon.export_points(ctx, full, coll)
check(n_full == count and keep_full == (kp[0], kp[-1]) and standing_full == (st[0], st[-1]),
      "exported all %d frames, kept part %s, standing part %s" % (n_full, keep_full, standing_full))
h1, b = table(full)
check(h0 == h1, "the same columns (time, the marks, the planted feet, 33 points)")
col = h1.index("body_left_ankle_z")
worst, edited = 0.0, None
for k in range(len(a)):
    for c in range(len(h0)):
        if a[k][c] != a[k][c]:  # NaN
            continue
        d = abs(a[k][c] - b[k][c])
        if (k, c) == (100, col):
            edited = d
        else:
            worst = max(worst, d)
check(worst < 2e-5, "every other value back unchanged, the marks and planted feet too (worst %.6f)" % worst)
check(edited is not None and abs(edited - 0.1) < 1e-4, "the edited ankle 10 cm up (%.4f)" % (edited or 0))
# Trimmed tighter here: the scene's range narrowed by 10 frames each side.
ctx.scene.frame_start, ctx.scene.frame_end = kp[0] + 10, kp[-1] - 10
n_trim, keep_trim, _ = addon.export_points(ctx, trim, coll)
_, p = table(trim)
kt, stt = marked(p, ki), marked(p, si)
check(marked(p, li) == marked(a, li) and marked(p, ri) == marked(a, ri), "the range narrowed: the planted feet as they came")
check(n_trim == count and kt == list(range(kp[0] + 10, kp[-1] - 9)) and stt == st and abs(p[0][0]) < 1e-9,
      "the range narrowed: kept frames %d-%d, the standing part and all %d frames still written" % (kt[0], kt[-1], n_trim))
print("FAILED %d" % len(failed) if failed else "all ok")
sys.exit(1 if failed else 0)
