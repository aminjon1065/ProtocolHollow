"""FAB-01 (ADR-0051): where the Fab props of OvergrownFactoryENV stand in the hub and the Docks rooms (not a content script).

Pure Python (no unreal import), used by create_fab_dressing.py, so the layout and its checks also run outside the
editor. A placement names a mesh of /Game/Fab/OvergrownFactoryENV and how it stands, from its local bounding box:

  ("floor", x, y, yaw)                 the box's footprint centred on (x, y), its bottom on the floor
  ("ceiling", x, y, yaw)               the box's footprint centred on (x, y), its top just under the ceiling
  ("wall", side, along, z)             the box's back flush on that wall's inner face, centred at `along` on it, its
                                       bottom at z; the mesh's front (FRONT) faces into the room
  ("free", x, y, z, pitch, yaw, roll)  any rotation: the rotated box's footprint centred on (x, y), its bottom at z

Sides are the rooms' (HollowRoomTypes.h): South = -X, North = +X, West = -Y, East = +Y. In the hub the ramp is South,
the cockpit North, port West, starboard East. The checks (check_layout) refuse a prop that leaves the room, enters the
clearance of a connector opening (300 x 350, ArtPassGuide.md) or stands on a socket (a spawn, loot, core, vent); the hub
keeps its terminals, their use spots, the player starts and the ramp free.
"""
import math

FAB_MESHES = "/Game/Fab/OvergrownFactoryENV/Meshes"
MESH_DIRS = {   # the mesh's folder under FAB_MESHES
    "SM_SteelBeam_01": "Architecture", "SM_Ivy_01": "Foliage", "SM_IvyDecal_01": "Foliage",
}
# The local axis the mesh's face looks along (wall props); checked on the screenshots of FAB-01.
FRONT = {
    "SM_ElectricalPanel_01": "+Y", "SM_ElectricalPanel_01_Box": "+Y", "SM_ElectricBox_02": "+X", "SM_ACVent_01": "+Y",
    "SM_AC_Unit_A_01": "+Y", "SM_Air_Vents_02": "+X", "SM_Wearhouse_Shelf_01": "+X", "SM_Ivy_01": "+X",
    "SM_IvyDecal_01": "+X", "SM_ElectricWire_01": "+X", "SM_ElectricWire_02": "+X",
}
# Big props cast shadows (they sit on the floor); thin wall and ceiling ones do not (cheaper, and a lamp housing
# under a ceiling light must not shade it).
SHADOWED = ("SM_Industrial_PumpMotorSkid_01", "SM_WaterTank", "SM_Barrel_02", "SM_WoodenCableDrum_01",
            "SM_Wearhouse_Shelf_01", "SM_AC_Unit_A_01", "SM_Air_Vents_02")
FIXTURES = ("SM_FluorescentLight_01",)   # lamp housings (no light of their own): the Fixtures folder
DARK_MATERIALS = {   # FAB-01: the overgrowth darkened to the ark's biomechanics (create_fab_dressing.py makes them)
    "MI_IVY_Leafs_01": "MI_Fab_Ivy_Dark", "MI_PlantStem_01": "MI_Fab_PlantStem_Dark", "MI_SpanishMoss_01": "MI_Fab_Moss_Dark",
}

CEILING = 400.0          # LevelMetrics: CellHeight (rooms and hub)
GAP = 0.5                # off the wall, floor and ceiling (no z-fighting)
OPENING_HALF_WIDTH = 150.0
OPENING_HEIGHT = 350.0
OPENING_MARGIN = 30.0    # around the opening, beside and above it
OPENING_DEPTH = 150.0    # into the room from the wall's inner face
SOCKET_CLEARANCE = 90.0  # from a socket, for what stands below SOCKET_HEIGHT
SOCKET_HEIGHT = 200.0

INWARD = {"South": 0.0, "North": 180.0, "West": 90.0, "East": 270.0}   # yaw of the direction into the room
AXIS_YAW = {"+X": 0.0, "+Y": 90.0, "-X": 180.0, "-Y": 270.0}


def stack(label, mesh, x, y, count, height, z=0.0):
    """A vertical run of `count` copies (a pipe riser), each `height` above the last."""
    return [("%s_%d" % (label, i), mesh, ("free", x, y, z + i * height, 0.0, 0.0, 0.0)) for i in range(count)]


def run_y(label, mesh, x, ys, z):
    """A horizontal run along Y (a pipe laid on its side) at x, one copy per centre in ys."""
    return [("%s_%d" % (label, i), mesh, ("free", x, y, z, 90.0, 90.0, 0.0)) for i, y in enumerate(ys)]


ROOMS = {   # level -> placements (label, mesh, how)
    "/Game/Hollow/Maps/L_Hub_Skiff": [
        ("Fab_Panel_Port", "SM_ElectricalPanel_01", ("wall", "West", 500.0, 100.0)),
        ("Fab_Box_Port", "SM_ElectricBox_02", ("wall", "West", 600.0, 140.0)),
        ("Fab_Cable_Port", "SM_ElectricWire_02", ("wall", "West", 600.0, 190.0)),
        ("Fab_Vent_Port", "SM_ACVent_01", ("wall", "West", 1050.0, 270.0)),
        ("Fab_Panel_Starboard", "SM_ElectricalPanel_01_Box", ("wall", "East", 500.0, 100.0)),
        ("Fab_Wire_Starboard_0", "SM_ElectricWire_01", ("wall", "East", 460.0, 190.0)),
        ("Fab_Wire_Starboard_1", "SM_ElectricWire_01", ("wall", "East", 540.0, 190.0)),
        ("Fab_Lamp_Aft", "SM_FluorescentLight_01", ("ceiling", 550.0, 400.0, 0.0)),
        ("Fab_Lamp_Fore", "SM_FluorescentLight_01", ("ceiling", 1050.0, 400.0, 0.0)),
        ("Fab_Barrel_Ramp", "SM_Barrel_02", ("floor", 300.0, 735.0, 25.0)),
    ] + stack("Fab_Riser_Port", "SM_SteelPipe_01", 1578.0, 22.0, 4, 94.0)
      + stack("Fab_Riser_Starboard", "SM_SteelPipe_01", 1578.0, 778.0, 4, 94.0),
    "/HollowSector_Docks/Rooms/LV_Docks_CoreVault": [   # the hero room: the pump station by the west wall
        ("Fab_PumpStation", "SM_Industrial_PumpMotorSkid_01", ("floor", 335.0, 104.0, 0.0)),
        ("Fab_Panel_South", "SM_ElectricalPanel_01", ("wall", "South", 550.0, 100.0)),
        ("Fab_Box_North", "SM_ElectricBox_02", ("wall", "North", 250.0, 140.0)),
        ("Fab_Cable_North", "SM_ElectricWire_02", ("wall", "North", 250.0, 192.0)),
        ("Fab_Overgrowth_West", "SM_Ivy_01", ("wall", "West", 600.0, 246.0)),
        ("Fab_Moss_Beam_0", "SM_IvyDecal_01", ("free", 200.0, 150.0, 311.0, 0.0, 0.0, 0.0)),
        ("Fab_Moss_Beam_1", "SM_IvyDecal_01", ("free", 600.0, 640.0, 311.0, 0.0, 0.0, 0.0)),
        ("Fab_Moss_Beam_2", "SM_IvyDecal_01", ("free", 200.0, 560.0, 311.0, 0.0, 0.0, 0.0)),
        ("Fab_Wire_Console_A", "SM_ElectricWire_01", ("wall", "East", 230.0, 160.0)),
        ("Fab_Wire_Console_B", "SM_ElectricWire_01", ("wall", "East", 570.0, 160.0)),
    ] + stack("Fab_Riser", "SM_SteelPipe_01", 40.0, 760.0, 4, 94.0),
    "/HollowSector_Docks/Rooms/LV_Docks_Spawn_Airlock": [
        ("Fab_Shelf", "SM_Wearhouse_Shelf_01", ("wall", "East", 560.0, 0.0)),
        ("Fab_Barrel_0", "SM_Barrel_02", ("floor", 480.0, 92.0, 0.0)),
        ("Fab_Barrel_1", "SM_Barrel_02", ("floor", 555.0, 82.0, 140.0)),
        ("Fab_Barrel_2", "SM_Barrel_02", ("floor", 515.0, 160.0, 40.0)),
        ("Fab_ACUnit", "SM_AC_Unit_A_01", ("wall", "South", 400.0, 0.0)),
        ("Fab_Vent", "SM_ACVent_01", ("wall", "South", 400.0, 270.0)),
        ("Fab_Panel", "SM_ElectricalPanel_01_Box", ("wall", "West", 330.0, 100.0)),
        ("Fab_Wire_0", "SM_ElectricWire_01", ("wall", "West", 300.0, 190.0)),
        ("Fab_Wire_1", "SM_ElectricWire_01", ("wall", "West", 360.0, 190.0)),
        ("Fab_Overgrowth", "SM_Ivy_01", ("wall", "East", 640.0, 246.0)),
        ("Fab_Moss", "SM_IvyDecal_01", ("free", 560.0, 560.0, 346.0, 0.0, 0.0, 0.0)),
    ],
    "/HollowSector_Docks/Rooms/LV_Docks_Hangar": [
        ("Fab_CableDrum", "SM_WoodenCableDrum_01", ("floor", 330.0, 300.0, 0.0)),
        ("Fab_WaterTank", "SM_WaterTank", ("floor", 290.0, 480.0, 0.0)),
        ("Fab_Barrel_0", "SM_Barrel_02", ("floor", 445.0, 335.0, 0.0)),
        ("Fab_Barrel_1", "SM_Barrel_02", ("floor", 470.0, 255.0, 70.0)),
        ("Fab_ACUnit", "SM_AC_Unit_A_01", ("wall", "East", 880.0, 0.0)),
        ("Fab_AirVents", "SM_Air_Vents_02", ("wall", "South", 250.0, 0.0)),
        ("Fab_Panel", "SM_ElectricalPanel_01", ("wall", "North", 960.0, 100.0)),
        ("Fab_Beam_0", "SM_SteelBeam_01", ("ceiling", 600.0, 300.0, 0.0)),
        ("Fab_Beam_1", "SM_SteelBeam_01", ("ceiling", 600.0, 900.0, 0.0)),
        ("Fab_Beam_2", "SM_SteelBeam_01", ("ceiling", 600.0, 600.0, 90.0)),
        ("Fab_Lamp_0", "SM_FluorescentLight_01", ("free", 600.0, 300.0, 349.0, 0.0, 0.0, 0.0)),
        ("Fab_Lamp_1", "SM_FluorescentLight_01", ("free", 600.0, 900.0, 349.0, 0.0, 0.0, 0.0)),
        ("Fab_Moss_0", "SM_IvyDecal_01", ("free", 600.0, 520.0, 327.0, 0.0, 90.0, 0.0)),
        ("Fab_Moss_1", "SM_IvyDecal_01", ("free", 600.0, 680.0, 327.0, 0.0, 90.0, 0.0)),
        ("Fab_Overgrowth", "SM_Ivy_01", ("wall", "West", 280.0, 246.0)),
    ],
    "/HollowSector_Docks/Rooms/LV_Docks_Corridor_Straight": [
        ("Fab_Panel", "SM_ElectricalPanel_01", ("wall", "South", 300.0, 100.0)),
        ("Fab_Box", "SM_ElectricBox_02", ("wall", "South", 520.0, 140.0)),
        ("Fab_Cable", "SM_ElectricWire_02", ("wall", "South", 520.0, 192.0)),
        ("Fab_AirVents", "SM_Air_Vents_02", ("wall", "North", 230.0, 0.0)),
        ("Fab_Vent", "SM_ACVent_01", ("wall", "North", 560.0, 260.0)),
        ("Fab_Lamp_0", "SM_FluorescentLight_01", ("ceiling", 200.0, 270.0, 90.0)),
        ("Fab_Lamp_1", "SM_FluorescentLight_01", ("ceiling", 200.0, 530.0, 90.0)),
        ("Fab_Moss", "SM_IvyDecal_01", ("free", 300.0, 450.0, 346.0, 0.0, 0.0, 0.0)),
    ] + run_y("Fab_PipeRun", "SM_SteelPipe_01", 31.0, (270.0, 364.0, 458.0, 552.0), 310.0),
    # The most frequent Docks room: only its north wall is closed (a vent socket at its middle), every other side an
    # opening, so a pipe run under the ceiling (above the openings' clearance) and a junction box over the vent.
    "/HollowSector_Docks/Rooms/LV_Docks_Junction_T": [
        ("Fab_Box", "SM_ElectricBox_02", ("wall", "North", 200.0, 225.0)),
        ("Fab_Cable", "SM_ElectricWire_02", ("wall", "North", 200.0, 276.0)),
    ] + run_y("Fab_PipeRun", "SM_SteelPipe_01", 369.0, (70.0, 164.0, 258.0), 380.0),
    "/HollowSector_Docks/Rooms/LV_Docks_Corridor_Corner": [
        ("Fab_AirVents", "SM_Air_Vents_02", ("wall", "North", 90.0, 0.0)),
        ("Fab_Panel", "SM_ElectricalPanel_01_Box", ("wall", "North", 180.0, 100.0)),
        ("Fab_Elbow", "SM_SteelPipe_02", ("free", 358.0, 40.0, 282.0, 0.0, 0.0, 0.0)),
        ("Fab_Wire", "SM_ElectricWire_01", ("wall", "West", 260.0, 150.0)),
        ("Fab_Box", "SM_ElectricBox_02", ("wall", "West", 300.0, 140.0)),
        ("Fab_Moss", "SM_IvyDecal_01", ("free", 300.0, 120.0, 346.0, 0.0, 0.0, 0.0)),
    ] + stack("Fab_Riser", "SM_SteelPipe_01", 360.0, 40.0, 3, 94.0),
}

HUB = "/Game/Hollow/Maps/L_Hub_Skiff"
HUB_INNER = (0.0, 0.0, 1600.0, 800.0)   # the cabin (create_hub_assets.py): walls outside it
HUB_KEEP_FREE = [   # (what, x, y, radius): the terminals and where their users stand, the starts, the ramp
    ("Contracts", 1487.0, 400.0, 110.0), ("Contracts use", 1377.0, 400.0, 110.0),
    ("Workbench", 800.0, 762.0, 110.0), ("Workbench use", 800.0, 652.0, 110.0),
    ("Locker", 800.0, 38.0, 110.0), ("Locker use", 800.0, 148.0, 110.0),
    ("Codex", 1200.0, 762.0, 110.0), ("Codex use", 1200.0, 652.0, 110.0),
    ("Start 0", 550.0, 300.0, 100.0), ("Start 1", 550.0, 500.0, 100.0), ("Start 2", 750.0, 300.0, 100.0), ("Start 3", 750.0, 500.0, 100.0),
    ("Ramp", 75.0, 400.0, 200.0),
]


def mesh_path(mesh):
    return "%s/%s/%s.%s" % (FAB_MESHES, MESH_DIRS.get(mesh, "Props"), mesh, mesh)


def used_meshes():
    return sorted({mesh for placements in ROOMS.values() for _, mesh, _ in placements})


def rotation(pitch, yaw, roll):
    """Rows: where the local X, Y and Z axes go (FRotationMatrix)."""
    sp, cp = math.sin(math.radians(pitch)), math.cos(math.radians(pitch))
    sy, cy = math.sin(math.radians(yaw)), math.cos(math.radians(yaw))
    sr, cr = math.sin(math.radians(roll)), math.cos(math.radians(roll))
    return ((cp * cy, cp * sy, sp),
            (sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp),
            (-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp))


def rotate(v, m):
    return tuple(v[0] * m[0][i] + v[1] * m[1][i] + v[2] * m[2][i] for i in range(3))


def rotated_box(box, m):
    lo, hi = box
    corners = [rotate((x, y, z), m) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
    return tuple(min(c[i] for c in corners) for i in range(3)), tuple(max(c[i] for c in corners) for i in range(3))


def place(how, mesh, box, inner):
    """(location, (pitch, yaw, roll), world box) of a placement; box is the mesh's local (min, max)."""
    kind = how[0]
    if kind == "wall":
        _, side, along, z = how
        rot = (0.0, (INWARD[side] - AXIS_YAW[FRONT[mesh]]) % 360.0, 0.0)
    elif kind in ("floor", "ceiling"):
        rot = (0.0, how[3], 0.0)
    elif kind == "free":
        rot = (how[4], how[5], how[6])
    else:
        raise ValueError("unknown placement " + kind)
    lo, hi = rotated_box(box, rotation(*rot))
    centre = ((lo[0] + hi[0]) * 0.5, (lo[1] + hi[1]) * 0.5)
    if kind == "wall":
        x0, y0, x1, y1 = inner
        if side == "South":
            x, y = x0 + GAP - lo[0], along - centre[1]
        elif side == "North":
            x, y = x1 - GAP - hi[0], along - centre[1]
        elif side == "West":
            x, y = along - centre[0], y0 + GAP - lo[1]
        else:
            x, y = along - centre[0], y1 - GAP - hi[1]
        location = (x, y, z + GAP - lo[2])
    elif kind == "ceiling":
        location = (how[1] - centre[0], how[2] - centre[1], CEILING - GAP - hi[2])
    else:
        location = (how[1] - centre[0], how[2] - centre[1], (how[3] if kind == "free" else 0.0) + GAP - lo[2])
    world = (tuple(location[i] + lo[i] for i in range(3)), tuple(location[i] + hi[i] for i in range(3)))
    return location, rot, world


def overlaps(a, b):
    return all(a[0][i] < b[1][i] and b[0][i] < a[1][i] for i in range(3))


def distance_2d(box, x, y):
    dx = max(box[0][0] - x, 0.0, x - box[1][0])
    dy = max(box[0][1] - y, 0.0, y - box[1][1])
    return math.hypot(dx, dy)


def opening_clearance(cell, side, size, cell_size):
    """The room-side box an opening keeps free (its width and height plus a margin, OPENING_DEPTH deep)."""
    w = OPENING_HALF_WIDTH + OPENING_MARGIN
    top = OPENING_HEIGHT + OPENING_MARGIN
    cx, cy = (cell[0] + 0.5) * cell_size, (cell[1] + 0.5) * cell_size
    reach = OPENING_DEPTH + 20.0   # the wall's thickness, then the depth
    if side == "South":
        return ((-1.0, cy - w, -1.0), (reach, cy + w, top))
    if side == "North":
        return ((size[0] - reach, cy - w, -1.0), (size[0] + 1.0, cy + w, top))
    if side == "West":
        return ((cx - w, -1.0, -1.0), (cx + w, reach, top))
    return ((cx - w, size[1] - reach, -1.0), (cx + w, size[1] + 1.0, top))


def check_layout(level, placements, boxes, inner, connectors, keep_free):
    """Problems of a level's placements (empty: all fine). connectors: (cell, side, room size, cell size);
    keep_free: (what, x, y, radius) points that nothing low may stand on."""
    problems, placed = [], []
    for label, mesh, how in placements:
        location, rot, world = place(how, mesh, boxes[mesh], inner)
        placed.append((label, mesh, location, rot, world))
        x0, y0, x1, y1 = inner
        if world[0][0] < x0 - 1.0 or world[0][1] < y0 - 1.0 or world[1][0] > x1 + 1.0 or world[1][1] > y1 + 1.0 \
                or world[0][2] < -1.0 or world[1][2] > CEILING + 1.0:
            problems.append("%s %s: outside the room (%s)" % (level, label, fmt(world)))
        for cell, side, size, cell_size in connectors:
            if overlaps(world, opening_clearance(cell, side, size, cell_size)):
                problems.append("%s %s: in the clearance of the opening %s %s (%s)" % (level, label, cell, side, fmt(world)))
        if world[0][2] < SOCKET_HEIGHT:
            for what, px, py, radius in keep_free:
                if distance_2d(world, px, py) < radius:
                    problems.append("%s %s: %.0f cm from %s (keep %.0f) (%s)" % (level, label, distance_2d(world, px, py), what, radius, fmt(world)))
    return problems, placed


def fmt(box):
    return "x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f" % (box[0][0], box[1][0], box[0][1], box[1][1], box[0][2], box[1][2])
