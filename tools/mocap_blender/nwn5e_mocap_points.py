# nwn5e mocap points - a Blender add-on for cleaning a motion capture take between
# FreeMoCap and nwn5e's Animation Recipe Bench (issue #77; tools/mocap_blender/README.md).
#
# The bench's "Export points for Blender" writes a take as a CSV: a row a frame, its
# time (time_s), its marks (standing, keep: 1 on the frames of the bench's standing part
# and of the part it keeps), then each body point's x / y / z (body_<point>_x ...) in
# metres, in the bench's game space (+X the performer's right, +Y forward, +Z up, the
# floor at 0). It runs from the standing part to the end of the kept part: the bench
# measures every turn from the standing pose, so that part travels with the take.
#
# - File > Import > nwn5e mocap points (.csv): an empty per point ("mc.<point>"), keyed
#   every frame, and a stick figure joining them (a mesh hooked to the empties), in a
#   collection of their own; the scene set to the take's frame rate, its range to the
#   kept part (what plays), and markers "standing" / "standing end" on the standing part.
#   The take faces Blender's front (-Y): the front view (numpad 1) looks at the performer.
# - Clean the empties' keys with Blender's own tools (Graph Editor > Key > Smooth / Blend
#   / Density), or move an empty and key it.
# - File > Export > nwn5e mocap points (.csv): every keyed frame back to the same CSV, the
#   standing part marked as it came and the kept part as the scene's range now (narrow the
#   range to trim tighter). Drop it on the bench's Mocap tab: its sliders follow the marks.
#
# Install: Edit > Preferences > Add-ons > (the drop-down at the top right) Install from
# Disk... > this file, then tick it. Or open it in the Scripting tab and press Run Script
# (the menu entries last until Blender closes).

bl_info = {
    "name": "nwn5e mocap points",
    "author": "nwn5e",
    "version": (1, 0, 0),
    "blender": (4, 2, 0),
    "location": "File > Import / Export > nwn5e mocap points (.csv)",
    "description": "Clean a mocap take's points between FreeMoCap and nwn5e's Animation Recipe Bench",
    "category": "Import-Export",
}

import csv
import os

import bpy
from bpy.props import StringProperty
from bpy_extras.io_utils import ExportHelper, ImportHelper

PREFIX = "mc."
FIGURE = "mc.figure"
KEY_POINTS = "nwn5e_points"  # the collection's custom properties
KEY_FPS = "nwn5e_fps"
KEY_SOURCE = "nwn5e_source"
KEY_STANDING = "nwn5e_standing"  # the standing part's first and last frames, as imported
KEY_KEEP = "nwn5e_keep"  # the kept part's, as imported (the scene's range carries it after)
MARKERS = ("standing", "standing end")

# The stick figure's lines (the bench's CONNECTIONS).
CONNECTIONS = [
    ("left_shoulder", "right_shoulder"), ("left_hip", "right_hip"), ("left_shoulder", "left_hip"),
    ("right_shoulder", "right_hip"), ("left_shoulder", "left_elbow"), ("left_elbow", "left_wrist"),
    ("left_wrist", "left_index"), ("left_wrist", "left_pinky"), ("left_index", "left_pinky"),
    ("right_shoulder", "right_elbow"), ("right_elbow", "right_wrist"), ("right_wrist", "right_index"),
    ("right_wrist", "right_pinky"), ("right_index", "right_pinky"), ("left_hip", "left_knee"),
    ("left_knee", "left_ankle"), ("left_ankle", "left_heel"), ("left_heel", "left_foot_index"),
    ("left_ankle", "left_foot_index"), ("right_hip", "right_knee"), ("right_knee", "right_ankle"),
    ("right_ankle", "right_heel"), ("right_heel", "right_foot_index"), ("right_ankle", "right_foot_index"),
    ("left_ear", "nose"), ("right_ear", "nose"), ("left_ear", "right_ear"),
]
BIG = {"left_wrist", "right_wrist", "left_ankle", "right_ankle", "left_knee", "right_knee", "left_elbow",
       "right_elbow", "left_hip", "right_hip", "left_shoulder", "right_shoulder", "nose"}


# Game space <-> Blender: turned half a turn about Z, so the performer faces -Y (Blender's front).
def to_blender(x, y, z):
    return (-x, -y, z)


def to_game(x, y, z):
    return (-x, -y, z)


def read_points(path):
    """The CSV: (times, names, frames, marks) - frames[k][name] = (x, y, z) in game space;
    marks["standing"] / marks["keep"] = (first, last) row marked 1 (absent: not marked)."""
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.reader(f))
    if len(rows) < 3:
        raise ValueError("fewer than two frames")
    header = [h.strip().lower() for h in rows[0]]
    if "time_s" not in header:
        raise ValueError("no time_s column: export the take from the bench's Mocap tab (Export points for Blender)")
    t_col = header.index("time_s")
    names, cols = [], {}
    for c, h in enumerate(header):
        if h.startswith("body_") and h[-2:] in ("_x", "_y", "_z"):
            name = h[5:-2]
            if name not in cols:
                cols[name] = [-1, -1, -1]
                names.append(name)
            cols[name]["xyz".index(h[-1])] = c
    names = [n for n in names if min(cols[n]) >= 0]
    mark_cols = {m: header.index(m) for m in ("standing", "keep") if m in header}
    marked = {m: [] for m in mark_cols}
    times, frames = [], []
    for row in rows[1:]:
        if not row or not row[t_col].strip():
            continue
        for m, c in mark_cols.items():
            try:
                if float(row[c]) > 0:
                    marked[m].append(len(times))
            except (ValueError, IndexError):
                pass
        times.append(float(row[t_col]))
        frame = {}
        for n in names:
            try:
                frame[n] = tuple(float(row[c]) for c in cols[n])
            except (ValueError, IndexError):
                frame[n] = None
        frames.append(frame)
    marks = {m: (v[0], v[-1]) for m, v in marked.items() if v}
    return times, names, frames, marks


def import_points(context, path):
    times, names, frames, marks = read_points(path)
    steps = sorted(b - a for a, b in zip(times, times[1:]))
    step = steps[len(steps) // 2] if steps else 1 / 30
    fps = max(1, round(1 / step)) if step > 0 else 30
    scene = context.scene
    stem = os.path.splitext(os.path.basename(path))[0]
    coll = bpy.data.collections.new("mocap " + stem)
    scene.collection.children.link(coll)
    coll[KEY_POINTS] = ",".join(names)
    coll[KEY_FPS] = fps
    coll[KEY_SOURCE] = os.path.basename(path)
    first_frame = 0
    keyed = [first_frame + t * fps - times[0] * fps for t in times]
    at = {m: (int(round(keyed[a])), int(round(keyed[b]))) for m, (a, b) in marks.items()}
    if "standing" in at:
        coll[KEY_STANDING] = list(at["standing"])
    if "keep" in at:
        coll[KEY_KEEP] = list(at["keep"])
    empties = {}
    for n in names:
        e = bpy.data.objects.new(PREFIX + n, None)
        e.empty_display_type = "SPHERE"
        e.empty_display_size = 0.03 if n in BIG else 0.015
        coll.objects.link(e)
        e.animation_data_create()
        action = bpy.data.actions.new(PREFIX + n)
        e.animation_data.action = action
        for axis in range(3):
            fc = action.fcurve_ensure_for_datablock(e, "location", index=axis)
            co = []
            for k, frame in enumerate(frames):
                p = frame.get(n)
                if p is None:
                    continue
                co += [keyed[k], to_blender(*p)[axis]]
            fc.keyframe_points.add(len(co) // 2)
            fc.keyframe_points.foreach_set("co", co)
            for kp in fc.keyframe_points:
                kp.interpolation = "BEZIER"
                kp.handle_left_type = kp.handle_right_type = "AUTO_CLAMPED"
            fc.update()
        empties[n] = e
    scene.render.fps = fps
    scene.render.fps_base = 1
    # What plays: the kept part (the whole take when unmarked); the standing part marked.
    scene.frame_start, scene.frame_end = at.get("keep", (first_frame, int(round(keyed[-1]))))
    for m in list(scene.timeline_markers):
        if m.name in MARKERS:
            scene.timeline_markers.remove(m)
    if "standing" in at:
        for name, frame in zip(MARKERS, at["standing"]):
            scene.timeline_markers.new(name, frame=frame)
    scene.frame_set(first_frame)
    context.view_layer.update()
    # The stick figure: a vertex per point, hooked to its empty.
    order = [n for n in names]
    mesh = bpy.data.meshes.new(FIGURE)
    mesh.from_pydata([tuple(empties[n].matrix_world.translation) for n in order],
                     [(order.index(a), order.index(b)) for a, b in CONNECTIONS if a in empties and b in empties], [])
    figure = bpy.data.objects.new(FIGURE, mesh)
    figure.show_in_front = True
    coll.objects.link(figure)
    for i, n in enumerate(order):
        hook = figure.modifiers.new("hook " + n, "HOOK")
        hook.object = empties[n]
        hook.vertex_indices_set([i])
        hook.matrix_inverse = empties[n].matrix_world.inverted()
    return coll, len(frames), fps


def points_collection(context):
    """The collection to export: the active object's, else the newest imported one."""
    obj = context.active_object
    if obj:
        for c in obj.users_collection:
            if KEY_POINTS in c:
                return c
    found = [c for c in bpy.data.collections if KEY_POINTS in c]
    return found[-1] if found else None


def export_points(context, path, coll):
    """Every keyed frame; the standing part marked as imported, the kept part as the scene's
    range. (frames written, kept part's (first, last) frame or None, standing's or None)."""
    names = coll[KEY_POINTS].split(",")
    fps = int(coll.get(KEY_FPS, 30))
    empties = {n: coll.objects.get(PREFIX + n) for n in names}
    missing = [n for n, e in empties.items() if e is None]
    if missing:
        raise ValueError("these points are missing from " + coll.name + ": " + ", ".join(missing))
    scene = context.scene
    a, b = None, None
    for e in empties.values():
        ad = e.animation_data
        if ad and ad.action:
            lo, hi = ad.action.frame_range
            a = lo if a is None else min(a, lo)
            b = hi if b is None else max(b, hi)
    a, b = int(round(a or 0)), int(round(b or 0))
    # The marks: the standing part as imported, the kept part as the scene's range - when the
    # file came marked, or the range was narrowed (an unmarked take, trimmed here).
    standing = None
    if KEY_STANDING in coll:
        s0, s1 = max(a, int(coll[KEY_STANDING][0])), min(b, int(coll[KEY_STANDING][1]))
        standing = (s0, s1) if s0 <= s1 else None
    keep = (max(a, scene.frame_start), min(b, scene.frame_end))
    marked = standing is not None or KEY_KEEP in coll or keep != (a, b)
    if keep[0] > keep[1]:
        keep = None
    current = scene.frame_current
    header = ["time_s"] + (["standing", "keep"] if marked else []) + ["body_%s_%s" % (n, axis) for n in names for axis in "xyz"]
    rows = [header]
    for f in range(a, b + 1):
        scene.frame_set(f)
        row = ["%.6f" % ((f - a) / fps)]
        if marked:
            row += [1 if standing and standing[0] <= f <= standing[1] else 0, 1 if keep and keep[0] <= f <= keep[1] else 0]
        for n in names:
            p = to_game(*empties[n].matrix_world.translation)
            row += ["%.5f" % v for v in p]
        rows.append(row)
    scene.frame_set(current)
    with open(path, "w", newline="", encoding="utf-8") as f:
        csv.writer(f).writerows(rows)
    return b - a + 1, (keep if marked else None), standing


class IMPORT_OT_nwn5e_points(bpy.types.Operator, ImportHelper):
    """Import a take's points exported by nwn5e's Animation Recipe Bench"""
    bl_idname = "import_anim.nwn5e_mocap_points"
    bl_label = "Import nwn5e mocap points"
    bl_options = {"REGISTER", "UNDO"}
    filename_ext = ".csv"
    filter_glob: StringProperty(default="*.csv", options={"HIDDEN"})

    def execute(self, context):
        try:
            coll, count, fps = import_points(context, self.filepath)
        except Exception as e:  # noqa: BLE001 - shown to the user
            self.report({"ERROR"}, "nwn5e mocap points: " + str(e))
            return {"CANCELLED"}
        self.report({"INFO"}, "%s: %d frames at %d a second" % (coll.name, count, fps))
        return {"FINISHED"}


class EXPORT_OT_nwn5e_points(bpy.types.Operator, ExportHelper):
    """Export the take's points for nwn5e's Animation Recipe Bench"""
    bl_idname = "export_anim.nwn5e_mocap_points"
    bl_label = "Export nwn5e mocap points"
    filename_ext = ".csv"
    filter_glob: StringProperty(default="*.csv", options={"HIDDEN"})

    def invoke(self, context, event):
        coll = points_collection(context)
        if coll and not self.filepath:
            stem = os.path.splitext(coll.get(KEY_SOURCE, "take_points.csv"))[0]
            self.filepath = stem + "_clean.csv"
        return super().invoke(context, event)

    def execute(self, context):
        coll = points_collection(context)
        if coll is None:
            self.report({"ERROR"}, "nwn5e mocap points: no imported take in this file (File > Import > nwn5e mocap points)")
            return {"CANCELLED"}
        try:
            count, keep, _ = export_points(context, self.filepath, coll)
        except Exception as e:  # noqa: BLE001 - shown to the user
            self.report({"ERROR"}, "nwn5e mocap points: " + str(e))
            return {"CANCELLED"}
        kept = "; kept part frames %d-%d (the scene's range)" % keep if keep else ""
        self.report({"INFO"}, "%d frames written to %s%s" % (count, self.filepath, kept))
        return {"FINISHED"}


def menu_import(self, context):
    self.layout.operator(IMPORT_OT_nwn5e_points.bl_idname, text="nwn5e mocap points (.csv)")


def menu_export(self, context):
    self.layout.operator(EXPORT_OT_nwn5e_points.bl_idname, text="nwn5e mocap points (.csv)")


CLASSES = (IMPORT_OT_nwn5e_points, EXPORT_OT_nwn5e_points)


def register():
    for c in CLASSES:
        bpy.utils.register_class(c)
    bpy.types.TOPBAR_MT_file_import.append(menu_import)
    bpy.types.TOPBAR_MT_file_export.append(menu_export)


def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(menu_export)
    bpy.types.TOPBAR_MT_file_import.remove(menu_import)
    for c in reversed(CLASSES):
        bpy.utils.unregister_class(c)


if __name__ == "__main__":
    register()
