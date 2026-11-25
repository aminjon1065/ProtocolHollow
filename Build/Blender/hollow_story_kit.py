"""Procedural story kit of Protocol: Hollow (NAR-01b, ADR-0050), built by Blender in the background.

  Blender -b --factory-startup --python Build/Blender/hollow_story_kit.py -- <spec.json> <out_dir>

The crew datapad in the art kit's way (Build/Blender/hollow_kit.py, ADR-0043): one FBX per mesh, its material slots
named for the project's materials. It reuses the art kit's primitives, read from hollow_kit.py up to its tools section,
so the other kits' meshes keep their hashes (create_art_kit.py digests each mesh's own script). Numbers only: the same
spec always gives the same mesh. Blender's +X stays the engine's +X, Blender's -Y is the engine's +Y; the origin is on
the floor under the pad.
"""
import json
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

PRIMITIVES_END = "# ---------------------------------------------------------------- tools (muzzle along +X, grip down)"
with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "hollow_kit.py")) as kit:
    _source = kit.read()
exec(compile(_source[:_source.index(PRIMITIVES_END)], "hollow_kit.py", "exec"))   # Part, box, cylinder, sphere, torus, wedge, assemble, export


def build_datapad(spec):
    """A crew datapad lying on the floor: a dark slab, its glowing screen up, an amber end cap with an Aether status
    strip (looks only)."""
    parts = [box((0.18, 0.12, 0.014), "Body", 0.004).place((0.0, 0.0, 0.007)),
             box((0.142, 0.094, 0.002), "Screen", 0.0).place((0.012, 0.0, 0.0145)),
             box((0.024, 0.124, 0.018), "Trim", 0.003).place((-0.084, 0.0, 0.009)),
             box((0.008, 0.08, 0.003), "Glow", 0.001).place((-0.084, 0.0, 0.0185))]
    return parts, ["Body", "Screen", "Trim", "Glow"], {}


BUILDERS = {name[len("build_"):]: fn for name, fn in globals().items() if name.startswith("build_") and callable(fn)}


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    spec_path, out_dir = args[0], args[1]
    with open(spec_path) as f:
        spec = json.load(f)
    os.makedirs(out_dir, exist_ok=True)
    report = {}
    for name, entry in sorted(spec["meshes"].items()):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        parts, slots, sockets = BUILDERS[entry["kind"]](entry)
        obj = assemble(name, parts, slots, sockets)
        export(obj, os.path.join(out_dir, name + ".fbx"))
        report[name] = {"slots": slots, "sockets": sorted(sockets), "triangles": sum(len(p.vertices) - 2 for p in obj.data.polygons)}
        print("hollow_story_kit: %s %d tris %s" % (name, report[name]["triangles"], slots))
    with open(os.path.join(out_dir, "kit_report.json"), "w") as f:
        json.dump(report, f, indent=1, sort_keys=True)
    print("HOLLOW_KIT_OK %d" % len(report))


main()
