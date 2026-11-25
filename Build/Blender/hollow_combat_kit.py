"""Procedural combat kit of Protocol: Hollow (GP-01, ADR-0044), built by Blender in the background.

  Blender -b --factory-startup --python Build/Blender/hollow_combat_kit.py -- <spec.json> <out_dir>

The weapons (and later the loot crates and pickups) in the art kit's way (Build/Blender/hollow_kit.py, ADR-0043): one FBX
per mesh, its material slots named for the project's materials, its sockets as SOCKET_<Name> empties. It reuses the
art kit's primitives, read from hollow_kit.py up to its tools section, so the art kit's meshes keep their hashes
(create_art_kit.py digests each mesh's own script). Numbers only: the same spec always gives the same meshes.
Blender's +X stays the engine's +X, Blender's -Y is the engine's +Y; muzzles along +X, the grip where the tools' is.
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


# ---------------------------------------------------------------- weapons (muzzle along +X, the tools' grip)

def grip(parts):
    """The tools' grip (hollow_kit.py tool_body), so the hand holds a weapon where it holds a tool (create_art_kit.py GRIP)."""
    parts.append(box((0.045, 0.042, 0.13), "Body", 0.01).place((-0.06, 0.0, -0.085), (0.0, -14.0, 0.0)))
    parts.append(box((0.05, 0.012, 0.022), "Metal", 0.004).place((-0.015, 0.0, -0.058)))      # the trigger guard


def build_nailgun(spec):
    """A pneumatic nailgun: a squat receiver, a short barrel, the nail strip slanting under it, a pressure bottle on top."""
    parts = []
    grip(parts)
    parts.append(box((0.21, 0.06, 0.075), "Body", 0.012).place((0.0, 0.0, 0.0)))
    parts.append(box((0.08, 0.064, 0.012), "Trim", 0.004).place((-0.02, 0.0, 0.042)))         # the amber stripe on top
    parts.append(cylinder(0.024, 0.07, "Metal", 18, axis="X").place((0.135, 0.0, 0.005)))      # the barrel's shroud
    parts.append(cylinder(0.012, 0.09, "Metal", 14, axis="X").place((0.2, 0.0, 0.005)))        # the barrel
    parts.append(cylinder(0.017, 0.012, "Glow", 14, axis="X").place((0.246, 0.0, 0.005)))      # its charged tip
    parts.append(box((0.03, 0.022, 0.12), "Metal", 0.004).place((0.06, 0.0, -0.075), (0.0, 18.0, 0.0)))   # the nail strip
    parts.append(cylinder(0.022, 0.12, "Trim", 16, bevel_width=0.005, axis="X").place((-0.01, 0.0, 0.07)))  # the pressure bottle
    for x in (-0.06, 0.04):
        parts.append(cylinder(0.024, 0.008, "Metal", 16, axis="X").place((x, 0.0, 0.07)))
    return parts, ["Body", "Trim", "Metal", "Glow"], {"Muzzle": (0.254, 0.0, 0.005)}


def build_shotgun(spec):
    """A heavy pump shotgun: a long receiver, the barrel over its magazine tube, the pump grip, a short stock."""
    parts = []
    grip(parts)
    parts.append(box((0.26, 0.065, 0.085), "Body", 0.012).place((0.0, 0.0, 0.0)))
    parts.append(box((0.12, 0.069, 0.012), "Trim", 0.004).place((0.0, 0.0, 0.047)))           # the amber stripe on top
    parts.append(cylinder(0.021, 0.42, "Metal", 18, axis="X").place((0.33, 0.0, 0.018)))       # the barrel
    parts.append(cylinder(0.017, 0.34, "Metal", 16, axis="X").place((0.29, 0.0, -0.024)))      # the magazine tube under it
    parts.append(box((0.12, 0.06, 0.05), "Body", 0.01).place((0.3, 0.0, -0.026)))              # the pump
    for i in range(4):
        parts.append(box((0.008, 0.062, 0.052), "Metal", 0.0).place((0.255 + 0.03 * i, 0.0, -0.026)))   # its ribs
    parts.append(cylinder(0.026, 0.02, "Metal", 18, axis="X").place((0.53, 0.0, 0.018)))       # the muzzle brake
    parts.append(cylinder(0.015, 0.006, "Glow", 14, axis="X").place((0.543, 0.0, 0.018)))      # its hot lip
    parts.append(box((0.16, 0.05, 0.07), "Body", 0.012).place((-0.2, 0.0, -0.02), (0.0, 8.0, 0.0)))   # the stock
    parts.append(box((0.02, 0.055, 0.09), "Metal", 0.004).place((-0.285, 0.0, -0.03), (0.0, 8.0, 0.0)))  # its butt plate
    return parts, ["Body", "Trim", "Metal", "Glow"], {"Muzzle": (0.556, 0.0, 0.018)}


# ---------------------------------------------------------------- pickups (GP-01b: loot on the floor, a hand's size)

def build_pickup_scrap(spec):
    """A few bent plates and a bolt."""
    parts = [box((0.16, 0.1, 0.012), "Metal", 0.003).place((0.0, 0.0, 0.006), (0.0, 0.0, 12.0)),
             box((0.12, 0.08, 0.01), "Body", 0.003).place((0.02, 0.01, 0.022), (8.0, -6.0, -20.0)),
             box((0.09, 0.05, 0.01), "Trim", 0.002).place((-0.03, -0.02, 0.034), (-10.0, 4.0, 35.0)),
             cylinder(0.008, 0.06, "Metal", 10, axis="X").place((0.04, -0.04, 0.012))]
    return parts, ["Body", "Trim", "Metal"], {}


def build_pickup_shard(spec):
    """An Aether crystal on a fused stub."""
    parts = [cylinder(0.03, 0.02, "Metal", 12).place((0.0, 0.0, 0.01)),
             cylinder(0.022, 0.12, "Glow", 6, radius_top=0.002).place((0.0, 0.0, 0.08), (6.0, 4.0, 0.0)),
             cylinder(0.012, 0.07, "Glow", 6, radius_top=0.001).place((0.02, 0.012, 0.05), (28.0, -18.0, 0.0))]
    return parts, ["Metal", "Glow"], {}


def build_pickup_fuelcell(spec):
    """A fuel cell canister lying on its side."""
    parts = [cylinder(0.035, 0.16, "Trim", 16, bevel_width=0.006, axis="X").place((0.0, 0.0, 0.035)),
             cylinder(0.037, 0.012, "Metal", 16, axis="X").place((0.06, 0.0, 0.035)),
             cylinder(0.037, 0.012, "Metal", 16, axis="X").place((-0.06, 0.0, 0.035)),
             cylinder(0.012, 0.02, "Glow", 10, axis="X").place((0.088, 0.0, 0.035))]
    return parts, ["Trim", "Metal", "Glow"], {}


def build_pickup_ammo(spec):
    """A box of rounds: nails (flat) or shells (taller), the amber band its kind."""
    tall = spec.get("tall", False)
    h = 0.07 if tall else 0.045
    parts = [box((0.14, 0.09, h), "Body", 0.006).place((0.0, 0.0, h / 2.0)),
             box((0.142, 0.092, 0.014), "Trim", 0.002).place((0.0, 0.0, h * 0.6)),
             box((0.05, 0.03, 0.004), "Metal", 0.001).place((0.0, 0.0, h + 0.002))]
    return parts, ["Body", "Trim", "Metal"], {}


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
        print("hollow_combat_kit: %s %d tris %s" % (name, report[name]["triangles"], slots))
    with open(os.path.join(out_dir, "kit_report.json"), "w") as f:
        json.dump(report, f, indent=1, sort_keys=True)
    print("HOLLOW_KIT_OK %d" % len(report))


main()
