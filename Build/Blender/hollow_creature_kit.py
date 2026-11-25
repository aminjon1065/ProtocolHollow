"""Procedural creature kit of Protocol: Hollow (GP-02, ADR-0047), built by Blender in the background.

  Blender -b --factory-startup --python Build/Blender/hollow_creature_kit.py -- <spec.json> <out_dir>

The Whisperers and their nests in the art kit's way (Build/Blender/hollow_kit.py, ADR-0043): one FBX per mesh, its
material slots named for the project's materials. It reuses the art kit's primitives, read from hollow_kit.py up to its
tools section, so the art kit's and the combat kit's meshes keep their hashes (create_art_kit.py digests each mesh's own
script). Numbers only: the same spec always gives the same meshes. Blender's +X stays the engine's +X (the creature's
front), Blender's -Y is the engine's +Y; the origin is on the floor under the body. The legs' scuttle is not here: the
Legs slot's material lifts the vertices near the floor (M_Kit_Creature, world position offset), so a leg is one piece.
"""
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

PRIMITIVES_END = "# ---------------------------------------------------------------- tools (muzzle along +X, grip down)"
with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "hollow_kit.py")) as kit:
    _source = kit.read()
exec(compile(_source[:_source.index(PRIMITIVES_END)], "hollow_kit.py", "exec"))   # Part, box, cylinder, sphere, torus, wedge, assemble, export


def segment(a, b, radius, slot, segments=6, taper=0.6):
    """A tapering rod from a to b (metres), its thick end at a."""
    a, b = Vector(a), Vector(b)
    d = b - a
    length = d.length
    pitch = math.degrees(math.acos(max(-1.0, min(1.0, d.z / length))))
    yaw = math.degrees(math.atan2(d.y, d.x))
    # A cone along +Z (radius at -Z, the tapered end at +Z) turned onto a -> b: Ry(pitch) then Rz(yaw).
    return cylinder(radius, length, slot, segments, radius_top=radius * taper).place(tuple((a + b) * 0.5), (0.0, pitch, yaw))


def blob(radii, slot, location, rotation=(0.0, 0.0, 0.0), segments=14, rings=9):
    """An ellipsoid of radii (metres)."""
    return sphere(1.0, slot, segments, rings).place(location, rotation, radii)


# ---------------------------------------------------------------- the Whisperer (front along +X, floor at Z = 0)

LEGS = [   # (x of the hip, x of the tip): three a side, the front pair reaching forward, the back pair back
    (0.09, 0.2), (-0.02, -0.02), (-0.13, -0.24),
]


def build_whisperer(spec):
    """A crawler a forearm long: thorax, abdomen and a low head under ribbed plates, spines, mandibles, a cluster of eyes."""
    s = spec.get("scale", 1.0)
    parts = []
    parts.append(blob((0.15 * s, 0.1 * s, 0.07 * s), "Shell", (0.0, 0.0, 0.15 * s)))                         # the thorax
    parts.append(blob((0.17 * s, 0.11 * s, 0.075 * s), "Shell", (-0.21 * s, 0.0, 0.14 * s), (0.0, -8.0, 0.0)))  # the abdomen
    parts.append(blob((0.08 * s, 0.07 * s, 0.05 * s), "Shell", (0.17 * s, 0.0, 0.13 * s), (0.0, 12.0, 0.0)))   # the head
    for i, x in enumerate((0.06, -0.06, -0.18, -0.3)):                                                        # ribbed plates
        parts.append(wedge((0.1 * s, 0.17 * s - 0.012 * i * s, 0.03 * s), "Shell", 0.7, 0.006 * s)
                     .place((x * s, 0.0, (0.205 - 0.008 * i) * s), (0.0, -6.0, 0.0)))
    for i, x in enumerate((0.02, -0.1, -0.22)):                                                               # a row of spines
        parts.append(cylinder(0.012 * s, 0.09 * s, "Shell", 6, radius_top=0.001).place((x * s, 0.0, (0.26 - 0.01 * i) * s), (0.0, -25.0, 0.0)))
    for side in (-1.0, 1.0):                                                                                  # the mandibles
        parts.append(segment((0.2 * s, side * 0.035 * s, 0.115 * s), (0.29 * s, side * 0.012 * s, 0.085 * s), 0.012 * s, "Shell", 6, 0.3))
    for (x, y, z, r) in ((0.235, 0.0, 0.15, 0.016), (0.225, 0.03, 0.145, 0.012), (0.225, -0.03, 0.145, 0.012),
                         (0.215, 0.018, 0.168, 0.009), (0.215, -0.018, 0.168, 0.009)):                       # the eyes
        parts.append(sphere(r * s, "Eyes", 8, 6).place((x * s, y * s, z * s)))
    for hip_x, tip_x in LEGS:                                                                                 # six legs
        for side in (-1.0, 1.0):
            hip = (hip_x * s, side * 0.08 * s, 0.15 * s)
            knee = ((hip_x + tip_x) * 0.5 * s, side * 0.21 * s, 0.24 * s)
            tip = (tip_x * s, side * 0.3 * s, 0.0)
            parts.append(segment(hip, knee, 0.016 * s, "Legs", 6, 0.75))
            parts.append(segment(knee, tip, 0.012 * s, "Legs", 6, 0.25))
    return parts, ["Shell", "Legs", "Eyes"], {}


def build_whisperer_nest(spec):
    """A growth on the floor: fused lumps of shell, ribs over them, and glowing pods in the folds."""
    s = spec.get("scale", 1.0)
    parts = []
    lumps = [((0.0, 0.0), (0.42, 0.36, 0.22)), ((0.32, 0.18), (0.24, 0.2, 0.16)), ((-0.3, 0.2), (0.22, 0.18, 0.14)),
             ((-0.18, -0.3), (0.26, 0.2, 0.15)), ((0.24, -0.26), (0.2, 0.16, 0.12))]
    for (x, y), radii in lumps:
        parts.append(blob(tuple(r * s for r in radii), "Shell", (x * s, y * s, radii[2] * 0.55 * s), segments=16, rings=10))
    for i in range(7):                                                                                        # ribs arching over
        a = 2.0 * math.pi * i / 7
        foot = (math.cos(a) * 0.5 * s, math.sin(a) * 0.42 * s, 0.0)
        crown = (math.cos(a) * 0.12 * s, math.sin(a) * 0.1 * s, 0.36 * s)
        parts.append(segment(foot, crown, 0.03 * s, "Shell", 6, 0.35))
    for (x, y, z, r) in ((0.12, 0.08, 0.2, 0.07), (-0.1, 0.12, 0.17, 0.06), (0.02, -0.14, 0.18, 0.065),
                         (0.3, -0.05, 0.12, 0.05), (-0.28, -0.08, 0.1, 0.05), (0.18, 0.3, 0.1, 0.045)):    # the pods
        parts.append(blob((r * s, r * s, r * 1.3 * s), "Pods", (x * s, y * s, z * s), segments=12, rings=8))
    return parts, ["Shell", "Pods"], {}


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
        builder = BUILDERS[entry["kind"]]
        parts, slots, sockets = builder(entry)
        obj = assemble(name, parts, slots, sockets)
        path = os.path.join(out_dir, name + ".fbx")
        export(obj, path)
        report[name] = {"slots": slots, "sockets": sorted(sockets), "triangles": sum(len(p.vertices) - 2 for p in obj.data.polygons)}
        print("hollow_creature_kit: %s %d tris %s" % (name, report[name]["triangles"], slots))
    with open(os.path.join(out_dir, "kit_report.json"), "w") as f:
        json.dump(report, f, indent=1, sort_keys=True)
    print("HOLLOW_KIT_OK %d" % len(report))


main()
