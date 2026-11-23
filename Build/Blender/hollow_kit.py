"""Procedural art kit of Protocol: Hollow (Art-01, ADR-0043), built by Blender in the background.

  Blender -b --factory-startup --python Build/Blender/hollow_kit.py -- <spec.json> <out_dir>

Build/Python/create_art_kit.py writes the spec (which meshes, their sizes from the project's data) and imports the FBX
files this writes: one per mesh, named after it, with its material slots named for the project's materials and its
sockets as SOCKET_<Name> empties (the FBX importer turns them into static mesh sockets). Everything is made of
primitives with bevels, from numbers only: the same spec always gives the same meshes. No textures: the project's
materials are procedural or CC0 and world aligned. Blender's +X stays the engine's +X, Blender's -Y is the engine's +Y.
"""
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

UV_METRES = 0.5   # box-projected UVs repeat every half metre (the materials are world aligned anyway)


# ---------------------------------------------------------------- primitives (metres)

class Part(object):
    """Geometry in a bmesh, one material slot, placed by a matrix."""

    def __init__(self, bm, slot, smooth=False):
        self.bm = bm
        self.slot = slot
        self.smooth = smooth

    def place(self, location=(0.0, 0.0, 0.0), rotation=(0.0, 0.0, 0.0), scale=(1.0, 1.0, 1.0)):
        euler = Matrix.Rotation(math.radians(rotation[2]), 4, "Z") @ Matrix.Rotation(math.radians(rotation[1]), 4, "Y") \
            @ Matrix.Rotation(math.radians(rotation[0]), 4, "X")
        matrix = Matrix.Translation(Vector(location)) @ euler @ Matrix.Diagonal(Vector(scale + (1.0,)) if len(scale) == 3 else Vector(scale))
        bmesh.ops.transform(self.bm, matrix=matrix, verts=self.bm.verts)
        return self


def bevel(bm, width, segments=2):
    if width <= 0.0:
        return
    bmesh.ops.bevel(bm, geom=list(bm.edges) + list(bm.verts), offset=width, offset_type="OFFSET", segments=segments,
                    profile=0.5, affect="EDGES", clamp_overlap=True)


def box(size, slot, bevel_width=0.0, segments=2):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector(size), verts=bm.verts)
    bevel(bm, bevel_width, segments)
    return Part(bm, slot)


def cylinder(radius, depth, slot, segments=20, radius_top=None, bevel_width=0.0, axis="Z"):
    """Along axis, centred."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segments, radius1=radius,
                          radius2=radius if radius_top is None else radius_top, depth=depth)
    bevel(bm, bevel_width, 1)
    part = Part(bm, slot, smooth=True)
    if axis == "X":
        part.place(rotation=(0.0, 90.0, 0.0))
    elif axis == "Y":
        part.place(rotation=(90.0, 0.0, 0.0))
    return part


def sphere(radius, slot, segments=16, rings=10):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segments, v_segments=rings, radius=radius)
    return Part(bm, slot, smooth=True)


def torus(major, minor, slot, segments=32, sides=8, axis="Z"):
    bm = bmesh.new()
    rows = []
    for i in range(segments):
        a = 2.0 * math.pi * i / segments
        row = []
        for j in range(sides):
            b = 2.0 * math.pi * j / sides
            r = major + minor * math.cos(b)
            row.append(bm.verts.new((r * math.cos(a), r * math.sin(a), minor * math.sin(b))))
        rows.append(row)
    for i in range(segments):
        for j in range(sides):
            bm.faces.new((rows[i][j], rows[(i + 1) % segments][j], rows[(i + 1) % segments][(j + 1) % sides], rows[i][(j + 1) % sides]))
    part = Part(bm, slot, smooth=True)
    if axis == "X":
        part.place(rotation=(0.0, 90.0, 0.0))
    elif axis == "Y":
        part.place(rotation=(90.0, 0.0, 0.0))
    return part


def wedge(size, slot, taper=0.5, bevel_width=0.0):
    """A box whose top face (+Z) shrinks by taper in X: armour plates, consoles."""
    part = box(size, slot, 0.0)
    for v in part.bm.verts:
        if v.co.z > 0.0:
            v.co.x *= taper
    bevel(part.bm, bevel_width, 2)
    return part


# ---------------------------------------------------------------- assembly

def assemble(name, parts, slots, sockets):
    """One object of all parts, a material slot per name in slots, box UVs, SOCKET_ empties."""
    mesh = bpy.data.meshes.new(name)
    master = bmesh.new()
    for part in parts:
        index = slots.index(part.slot)
        for face in part.bm.faces:
            face.material_index = index
            face.smooth = part.smooth
        temp = bpy.data.meshes.new("_part")
        part.bm.to_mesh(temp)
        part.bm.free()
        master.from_mesh(temp)
        bpy.data.meshes.remove(temp)
    bmesh.ops.remove_doubles(master, verts=master.verts, dist=1e-5)
    uv = master.loops.layers.uv.new("UVMap")
    for face in master.faces:
        n = face.normal
        axis = max(range(3), key=lambda k: abs(n[k]))
        u_axis, v_axis = [(1, 2), (0, 2), (0, 1)][axis]
        for loop in face.loops:
            loop[uv].uv = (loop.vert.co[u_axis] / UV_METRES, loop.vert.co[v_axis] / UV_METRES)
    master.to_mesh(mesh)
    master.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    for slot in slots:
        material = bpy.data.materials.get(slot) or bpy.data.materials.new(slot)
        obj.data.materials.append(material)
    for socket_name, location in sockets.items():
        empty = bpy.data.objects.new("SOCKET_" + socket_name, None)
        empty.location = Vector(location)
        empty.parent = obj
        bpy.context.scene.collection.objects.link(empty)
    return obj


def export(obj, path):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    for child in obj.children:
        child.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH", "EMPTY"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_UNITS", mesh_smooth_type="FACE", add_leaf_bones=False,
                             bake_anim=False, use_mesh_modifiers=True, axis_forward="-Z", axis_up="Y")


# ---------------------------------------------------------------- tools (muzzle along +X, grip down)

def tool_body(parts, length):
    parts.append(box((length, 0.07, 0.085), "Body", 0.012).place((0.02, 0.0, 0.0)))
    parts.append(box((0.045, 0.042, 0.13), "Body", 0.01).place((-0.06, 0.0, -0.085), (0.0, -14.0, 0.0)))
    parts.append(box((0.05, 0.012, 0.022), "Metal", 0.004).place((-0.015, 0.0, -0.058)))
    parts.append(box((0.09, 0.074, 0.012), "Trim", 0.004).place((0.0, 0.0, 0.046)))     # the amber stripe on top


def build_cutter(spec):
    parts = []
    tool_body(parts, 0.22)
    parts.append(cylinder(0.032, 0.17, "Trim", 18, bevel_width=0.006, axis="X").place((-0.01, 0.0, 0.085)))   # the fuel canister
    parts.append(cylinder(0.034, 0.012, "Metal", 18, axis="X").place((0.075, 0.0, 0.085)))
    parts.append(cylinder(0.034, 0.012, "Metal", 18, axis="X").place((-0.095, 0.0, 0.085)))
    parts.append(cylinder(0.012, 0.09, "Body", 10, axis="Z").place((0.07, 0.0, 0.045), (0.0, 30.0, 0.0)))   # its feed
    parts.append(cylinder(0.022, 0.15, "Metal", 18, axis="X").place((0.2, 0.0, 0.0)))                        # the barrel
    for i in range(5):
        parts.append(cylinder(0.034, 0.008, "Metal", 18, axis="X").place((0.15 + 0.022 * i, 0.0, 0.0)))       # cooling fins
    parts.append(cylinder(0.024, 0.05, "Metal", 18, radius_top=0.012, axis="X").place((0.3, 0.0, 0.0)))     # the nozzle
    parts.append(cylinder(0.015, 0.008, "Glow", 18, axis="X").place((0.328, 0.0, 0.0)))                      # its hot lip
    return parts, ["Body", "Trim", "Metal", "Glow"], {"Muzzle": (0.335, 0.0, 0.0)}


def build_welder(spec):
    parts = []
    tool_body(parts, 0.2)
    parts.append(cylinder(0.034, 0.16, "Metal", 18, bevel_width=0.006, axis="X").place((0.0, 0.0, -0.035)))   # the gas bottle under it
    parts.append(cylinder(0.02, 0.045, "Trim", 18, axis="X").place((0.105, 0.0, -0.035)))
    parts.append(cylinder(0.026, 0.06, "Metal", 16, axis="X").place((0.14, 0.0, 0.0)))
    for side in (-1.0, 1.0):                                                                                # the two electrodes
        parts.append(box((0.1, 0.01, 0.012), "Metal", 0.002).place((0.215, side * 0.016, 0.0), (0.0, 0.0, -side * 6.0)))
        parts.append(box((0.018, 0.012, 0.016), "Body", 0.002).place((0.268, side * 0.012, 0.0)))
    parts.append(sphere(0.008, "Glow", 10, 6).place((0.282, 0.0, 0.0)))                                    # the arc gap
    return parts, ["Body", "Trim", "Metal", "Glow"], {"Muzzle": (0.285, 0.0, 0.0)}


def build_lantern(spec):
    parts = []
    parts.append(box((0.05, 0.045, 0.12), "Body", 0.012).place((-0.1, 0.0, -0.05), (0.0, -10.0, 0.0)))   # the grip
    parts.append(box((0.12, 0.04, 0.03), "Body", 0.008).place((-0.05, 0.0, 0.0)))
    parts.append(cylinder(0.072, 0.02, "Gilt", 24, bevel_width=0.004).place((0.04, 0.0, 0.085)))        # the cage's caps
    parts.append(cylinder(0.072, 0.02, "Gilt", 24, bevel_width=0.004).place((0.04, 0.0, -0.085)))
    for i in range(6):                                                                                     # its ribs
        a = 2.0 * math.pi * i / 6
        parts.append(cylinder(0.006, 0.16, "Metal", 8).place((0.04 + 0.064 * math.cos(a), 0.064 * math.sin(a), 0.0)))
    parts.append(cylinder(0.056, 0.15, "Glass", 24).place((0.04, 0.0, 0.0)))
    parts.append(sphere(0.03, "Glow", 16, 10).place((0.04, 0.0, 0.0)))                                     # the aether flame
    parts.append(torus(0.062, 0.006, "Gilt", 32, 6).place((0.04, 0.0, 0.03)))
    parts.append(torus(0.062, 0.006, "Gilt", 32, 6).place((0.04, 0.0, -0.03)))
    parts.append(cylinder(0.05, 0.05, "Gilt", 24, radius_top=0.066, axis="X").place((0.14, 0.0, 0.0)))   # the hood
    parts.append(cylinder(0.05, 0.006, "Glow", 24, axis="X").place((0.168, 0.0, 0.0)))                     # its lens
    return parts, ["Body", "Metal", "Gilt", "Glass", "Glow"], {"Muzzle": (0.175, 0.0, 0.0)}


# ---------------------------------------------------------------- the Aether core's container

def rune_strokes(parts, radius, z, count, height):
    """Raised rune strokes round a band: a fixed pattern of uprights, slants and bars (no randomness)."""
    pattern = [(0.0, 1.0), (25.0, 0.7), (-25.0, 0.7), (90.0, 0.5), (0.0, 0.6), (40.0, 1.0)]
    for i in range(count):
        a = 360.0 * i / count
        tilt, length = pattern[i % len(pattern)]
        x, y = radius * math.cos(math.radians(a)), radius * math.sin(math.radians(a))
        stroke = box((0.006, 0.012, height * length), "Runes", 0.0)
        stroke.place((0.0, 0.0, 0.0), (tilt, 0.0, 0.0))
        stroke.place((x, y, z), (0.0, 0.0, a))
        parts.append(stroke)


def build_core(spec):
    hx, hy, hz = spec["half"]
    r = min(hx, hy)
    parts = []
    parts.append(box((2 * hx, 2 * hy, 0.1 * hz), "Body", 0.015).place((0.0, 0.0, -0.95 * hz)))   # base
    parts.append(box((2 * hx * 0.92, 2 * hy * 0.92, 0.08 * hz), "Body", 0.012).place((0.0, 0.0, 0.94 * hz)))   # cap
    parts.append(box((1.4 * hx, 1.4 * hy, 0.05 * hz), "Metal", 0.008).place((0.0, 0.0, 1.0 * hz)))
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):                                                                               # the frame's posts
            parts.append(box((0.16 * hx, 0.16 * hy, 1.85 * hz), "Body", 0.01).place((sx * 0.86 * hx, sy * 0.86 * hy, 0.0)))
            parts.append(box((0.2 * hx, 0.2 * hy, 0.06 * hz), "Metal", 0.006).place((sx * 0.86 * hx, sy * 0.86 * hy, 0.3 * hz)))
            parts.append(box((0.2 * hx, 0.2 * hy, 0.06 * hz), "Metal", 0.006).place((sx * 0.86 * hx, sy * 0.86 * hy, -0.3 * hz)))
    for side in (-1.0, 1.0):                                                                                 # carry handles
        parts.append(box((0.5 * hx, 0.06 * hy, 0.08 * hz), "Metal", 0.006).place((0.0, side * 1.02 * hy, 0.55 * hz)))
    parts.append(cylinder(0.55 * r, 0.06 * hz, "Metal", 24).place((0.0, 0.0, -0.82 * hz)))                    # the crystal's seat
    crystal = sphere(0.42 * r, "Glow", 6, 4)                                                                  # a faceted crystal
    crystal.smooth = False
    parts.append(crystal.place((0.0, 0.0, 0.0), (0.0, 0.0, 30.0), (1.0, 1.0, 1.9 * hz / r * 0.5)))
    for z in (0.42 * hz, -0.42 * hz):                                                                         # the rune bands
        parts.append(torus(0.62 * r, 0.035 * r, "Metal", 40, 8).place((0.0, 0.0, z)))
        rune_strokes(parts, 0.66 * r, z, 18, 0.16 * hz)
    return parts, ["Glow", "Body", "Metal", "Runes"], {}   # the crystal first: the core shows its item's look on slot 0


# ---------------------------------------------------------------- doors and bulkheads (sizes from the project's metrics)

def build_door_leaf(spec):
    t, w, h = spec["size"]     # thickness (X), width (Y), height (Z); origin at its centre, like the scaled cube it replaces
    parts = [box((t, w, h), "Body", 0.01)]
    ribs = 4
    for i in range(ribs):                                                                                     # horizontal ribs
        z = -h * 0.35 + i * h * 0.7 / (ribs - 1)
        for side in (-1.0, 1.0):
            parts.append(box((0.03, w * 0.86, 0.06), "Metal", 0.008).place((side * (t * 0.5 + 0.01), 0.0, z)))
    stripe = h * 0.06
    for side in (-1.0, 1.0):                                                                                 # hazard stripes at the foot
        parts.append(box((0.012, w * 0.94, stripe), "Trim", 0.002).place((side * (t * 0.5 + 0.002), 0.0, -h * 0.5 + stripe)))
        parts.append(box((0.02, 0.12, 0.3), "Metal", 0.006).place((side * (t * 0.5 + 0.008), w * 0.35, 0.0)))   # handles
    seam = 0.035                                                                                              # the weld seam round its edge
    parts.append(box((t * 1.04, w + 0.004, seam), "Seam", 0.0).place((0.0, 0.0, -h * 0.5 + seam * 0.5)))
    for side in (-1.0, 1.0):
        parts.append(box((t * 1.04, seam, h), "Seam", 0.0).place((0.0, side * (w * 0.5 - seam * 0.5 + 0.002), 0.0)))
    return parts, ["Body", "Metal", "Trim", "Seam"], {}


def build_bulkhead_panel(spec):
    t, w, h = spec["size"]
    parts = [box((t, w, h), "Body", 0.008)]
    for side in (-1.0, 1.0):
        for k in range(3):                                                                                    # vertical ribs
            y = -w * 0.3 + k * w * 0.3
            parts.append(box((0.025, 0.05, h * 0.9), "Metal", 0.006).place((side * (t * 0.5 + 0.008), y, 0.0)))
        for z in (-h * 0.42, h * 0.42):                                                                       # rivet rows
            for k in range(7):
                parts.append(cylinder(0.012, 0.01, "Metal", 8, axis="X").place((side * (t * 0.5 + 0.004), -w * 0.42 + k * w * 0.14, z)))
    return parts, ["Body", "Metal"], {}


# ---------------------------------------------------------------- hub dressing

def build_console(spec):
    parts = []
    parts.append(wedge((0.6, 0.9, 1.0), "Body", 0.75, 0.02).place((0.0, 0.0, 0.5)))
    parts.append(box((0.04, 0.78, 0.46), "Metal", 0.01).place((-0.13, 0.0, 1.12), (0.0, -18.0, 0.0)))   # the screen's frame
    parts.append(box((0.02, 0.7, 0.38), "Screen", 0.0).place((-0.155, 0.0, 1.12), (0.0, -18.0, 0.0)))   # the screen
    parts.append(box((0.32, 0.86, 0.05), "Metal", 0.01).place((-0.22, 0.0, 0.86), (0.0, 12.0, 0.0)))    # the desk
    for k in range(5):
        parts.append(box((0.04, 0.06, 0.02), "Glow", 0.004).place((-0.26, -0.3 + k * 0.15, 0.89), (0.0, 12.0, 0.0)))   # keys
    parts.append(box((0.62, 0.92, 0.06), "Trim", 0.01).place((0.0, 0.0, 0.03)))
    return parts, ["Body", "Metal", "Trim", "Screen", "Glow"], {}


def build_pilot_seat(spec):
    parts = []
    parts.append(cylinder(0.08, 0.4, "Metal", 16).place((0.0, 0.0, 0.2)))
    parts.append(box((0.55, 0.55, 0.12), "Body", 0.04).place((0.0, 0.0, 0.46)))
    parts.append(box((0.12, 0.55, 0.75), "Body", 0.04).place((0.26, 0.0, 0.86), (0.0, -10.0, 0.0)))
    parts.append(box((0.1, 0.3, 0.2), "Body", 0.04).place((0.3, 0.0, 1.32)))
    for side in (-1.0, 1.0):
        parts.append(box((0.4, 0.07, 0.06), "Metal", 0.015).place((0.0, side * 0.31, 0.66)))
    parts.append(box((0.02, 0.06, 0.6), "Trim", 0.0).place((0.19, 0.12, 0.86), (0.0, -10.0, 0.0)))     # the harness
    parts.append(box((0.02, 0.06, 0.6), "Trim", 0.0).place((0.19, -0.12, 0.86), (0.0, -10.0, 0.0)))
    return parts, ["Body", "Metal", "Trim"], {}


def build_crate(spec):
    s = 0.6
    parts = [box((s, s * 1.4, s), "Body", 0.02).place((0.0, 0.0, s * 0.5))]
    for side in (-1.0, 1.0):
        parts.append(box((s * 1.04, 0.06, s * 1.04), "Metal", 0.01).place((0.0, side * s * 0.6, s * 0.5)))
        parts.append(box((0.01, s * 0.9, 0.08), "Trim", 0.0).place((side * s * 0.51, 0.0, s * 0.75)))
    return parts, ["Body", "Metal", "Trim"], {}


def build_pipe_run(spec):
    length = 3.0
    parts = []
    for k, (y, z, r) in enumerate(((0.0, 0.0, 0.07), (0.18, 0.04, 0.05), (-0.16, -0.02, 0.04))):
        parts.append(cylinder(r, length, "Metal" if k else "Trim", 16, axis="X").place((0.0, y, z)))
    for k in range(4):
        x = -length * 0.375 + k * length * 0.25
        parts.append(box((0.06, 0.5, 0.06), "Body", 0.01).place((x, 0.0, 0.1)))
    return parts, ["Body", "Metal", "Trim"], {}


# ---------------------------------------------------------------- armour (component space of the UE5 mannequin, -Y = front)

def build_exo_helmet(spec):
    parts = []
    parts.append(sphere(0.145, "Body", 20, 12).place((0.0, 0.0, 0.0), (0.0, 0.0, 0.0), (1.0, 1.12, 1.08)))
    parts.append(box((0.22, 0.03, 0.09), "Visor", 0.012).place((0.0, -0.152, 0.01)))                       # the visor's band
    parts.append(box((0.25, 0.05, 0.13), "Metal", 0.015).place((0.0, -0.128, 0.01)))                       # its frame
    parts.append(box((0.06, 0.08, 0.05), "Metal", 0.01).place((0.12, -0.08, 0.09)))                          # the lamp's housing
    parts.append(cylinder(0.018, 0.02, "Glow", 12, axis="Y").place((0.12, -0.125, 0.09)))
    parts.append(box((0.3, 0.12, 0.05), "Body", 0.015).place((0.0, 0.03, -0.12)))                           # the neck seal
    parts.append(box((0.035, 0.08, 0.16), "Trim", 0.008).place((0.0, 0.07, 0.11)))                          # the crest rib
    return parts, ["Body", "Metal", "Visor", "Trim", "Glow"], {}


def build_exo_chest(spec):
    parts = []
    parts.append(wedge((0.42, 0.12, 0.34), "Body", 0.82, 0.03).place((0.0, -0.05, 0.0), (0.0, 0.0, 0.0)))
    parts.append(box((0.3, 0.04, 0.08), "Metal", 0.01).place((0.0, -0.115, 0.06)))
    parts.append(box((0.06, 0.03, 0.06), "Glow", 0.008).place((0.1, -0.125, -0.05)))                         # the status light
    parts.append(box((0.38, 0.1, 0.06), "Trim", 0.01).place((0.0, -0.06, 0.16)))
    return parts, ["Body", "Metal", "Trim", "Glow"], {}


def build_exo_pack(spec):
    parts = []
    parts.append(box((0.34, 0.14, 0.42), "Body", 0.03).place((0.0, 0.12, 0.0)))
    for side in (-1.0, 1.0):
        parts.append(cylinder(0.055, 0.4, "Trim", 18, bevel_width=0.01).place((side * 0.1, 0.22, 0.02)))
        parts.append(cylinder(0.03, 0.03, "Metal", 12).place((side * 0.1, 0.22, 0.24)))
    parts.append(torus(0.08, 0.012, "Metal", 16, 6, axis="X").place((0.0, 0.19, 0.25)))                      # the hose
    return parts, ["Body", "Metal", "Trim"], {}


def build_exo_pauldron(spec):
    parts = [sphere(0.09, "Body", 16, 10).place((0.0, 0.0, 0.0), (0.0, 0.0, 0.0), (1.25, 1.1, 0.7))]
    parts.append(box((0.2, 0.18, 0.025), "Trim", 0.008).place((0.02, 0.0, 0.05)))
    return parts, ["Body", "Trim"], {}


def build_exo_bracer(spec):
    parts = [cylinder(0.048, 0.2, "Body", 16, radius_top=0.042, bevel_width=0.008, axis="X")]
    parts.append(box((0.12, 0.03, 0.03), "Metal", 0.006).place((0.0, -0.045, 0.0)))
    parts.append(box((0.04, 0.035, 0.02), "Glow", 0.004).place((0.03, -0.055, 0.0)))
    return parts, ["Body", "Metal", "Glow"], {}


def build_exo_shin(spec):
    parts = [wedge((0.11, 0.06, 0.26), "Body", 0.8, 0.012).place((0.0, -0.06, 0.0))]
    parts.append(sphere(0.06, "Metal", 12, 8).place((0.0, -0.06, 0.15), (0.0, 0.0, 0.0), (1.0, 0.7, 0.8)))
    return parts, ["Body", "Metal"], {}


def build_pilgrim_pauldron(spec):
    parts = [sphere(0.16, "Body", 16, 10).place((0.0, 0.0, 0.0), (0.0, 0.0, 0.0), (1.3, 1.2, 0.8))]
    for k in range(3):
        parts.append(box((0.36 - k * 0.06, 0.3 - k * 0.04, 0.03), "Metal", 0.01).place((0.02, 0.0, 0.08 + k * 0.03)))
    parts.append(box((0.04, 0.04, 0.12), "Metal", 0.01).place((0.12, 0.0, 0.16), (0.0, 25.0, 0.0)))        # a spike
    return parts, ["Body", "Metal"], {}


def build_pilgrim_chest(spec):
    parts = [wedge((0.6, 0.18, 0.5), "Body", 0.78, 0.03).place((0.0, -0.08, 0.0))]
    parts.append(torus(0.09, 0.025, "Metal", 24, 8, axis="Y").place((0.0, -0.17, 0.02)))
    parts.append(sphere(0.075, "Glow", 16, 10).place((0.0, -0.16, 0.02)))                                   # the furnace core
    for side in (-1.0, 1.0):
        parts.append(box((0.08, 0.06, 0.42), "Metal", 0.012).place((side * 0.22, -0.14, 0.0)))
    return parts, ["Body", "Metal", "Glow"], {}


def build_pilgrim_helm(spec):
    parts = [box((0.26, 0.28, 0.3), "Body", 0.04)]
    parts.append(box((0.18, 0.02, 0.025), "Glow", 0.004).place((0.0, -0.142, 0.03)))                         # the eye slit
    parts.append(box((0.3, 0.06, 0.06), "Metal", 0.012).place((0.0, -0.13, 0.1)))
    parts.append(wedge((0.06, 0.24, 0.14), "Metal", 0.3, 0.01).place((0.0, 0.0, 0.2)))                       # the ridge
    return parts, ["Body", "Metal", "Glow"], {}


def build_pilgrim_gauntlet(spec):
    parts = [box((0.26, 0.16, 0.16), "Body", 0.03)]
    for k in range(3):
        parts.append(box((0.04, 0.18, 0.18), "Metal", 0.01).place((-0.08 + k * 0.08, 0.0, 0.0)))
    return parts, ["Body", "Metal"], {}


def build_stalker_crest(spec):
    parts = []
    for k in range(5):                                                                                        # a fan of blades
        tilt = -40.0 + k * 20.0
        blade = wedge((0.03, 0.02, 0.22 - abs(k - 2) * 0.04), "Body", 0.1, 0.0)
        parts.append(blade.place((0.0, 0.03, 0.08), (tilt, 0.0, 0.0)))
    return parts, ["Body"], {}


def build_stalker_spines(spec):
    parts = []
    for k in range(6):
        spine = wedge((0.03, 0.03, 0.16 - k * 0.015), "Body", 0.08, 0.0)
        parts.append(spine.place((0.0, 0.05, 0.25 - k * 0.1), (40.0, 0.0, 0.0)))
    return parts, ["Body"], {}


def build_stalker_eyes(spec):
    parts = []
    for side in (-1.0, 1.0):
        parts.append(sphere(0.012, "Glow", 10, 6).place((side * 0.035, -0.11, 0.03), (0.0, 0.0, 0.0), (1.6, 0.6, 0.8)))
    return parts, ["Glow"], {}


def build_stalker_blade(spec):
    blade = wedge((0.04, 0.02, 0.38), "Body", 0.05, 0.0).place((0.0, 0.0, 0.0), (0.0, 90.0, 0.0))
    return [blade], ["Body"], {}


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
        print("hollow_kit: %s %d tris %s" % (name, report[name]["triangles"], slots))
    with open(os.path.join(out_dir, "kit_report.json"), "w") as f:
        json.dump(report, f, indent=1, sort_keys=True)
    print("HOLLOW_KIT_OK %d" % len(report))


main()
