"""Builds the greybox kit of sector II "Reactor Cathedral" inside its Game Feature plugin (idempotent, ADR-0010, ADR-0012, ADR-0038).

  /HollowSector_Cathedral/HollowSector_Cathedral        UGameFeatureData: adds /HollowSector_Cathedral/Data to HollowRoom and HollowBiome
  /HollowSector_Cathedral/Data/DA_Biome_Cathedral       UHollowBiomeDefinition: generator rules (S7-01); its assembly rules
                                                        mirror DA_Biome_Docks (doors, bulkheads, loot, core, enemies,
                                                        extraction, sealed plug), the Cathedral's own ones follow them
For every room of ROOMS:
  /HollowSector_Cathedral/Rooms/LV_Cathedral_<Name>     static shell: floor, ceiling, walls with an opening at every
                                                        connector, as the Docks kit; gothic decor in two instanced meshes
                                                        (Decor actor): piers, plinths, pilasters and door shafts block,
                                                        ribs, capitals, string course, arches and lancets do not
  /HollowSector_Cathedral/Data/DA_Room_Cathedral_<Name> UHollowRoomDefinition: footprint (Z = storeys), connectors, sockets, type

Two-storey halls (Footprint.Z = 2) are one tall volume: clear height 2 x CellHeight, every opening 300 x 350 on the floor,
the upper tier (string course, lancets) out of reach. Level and data asset are generated from the same spec; all sizes
come from UHollowLevelMetrics (DefaultGame.ini), conventions in Docs/QA/LevelMetrics.md. The spec is checked before
anything is built (sockets on clear floor, piers off the openings). Creates only what is missing (never rewrites a level),
keeps the sockets and the mirrored assembly rules in step, then validates. No lights and no acoustics yet (S7-03).
Run after the scripts that fill DA_Biome_Docks (last in the order):
  Build/Scripts/run-editor-python.sh Build/Python/create_cathedral_greybox.py
"""
import collections
import math
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

SECTOR_ROOT = "/HollowSector_Cathedral"                 # mount point of Plugins/GameFeatures/HollowSector_Cathedral
FEATURE_DATA_PATH = SECTOR_ROOT + "/HollowSector_Cathedral"   # the engine requires <root>/<PluginName>
SCANNED_TYPES = [("HollowRoom", "/Script/HollowPCG.HollowRoomDefinition"),
                 ("HollowBiome", "/Script/HollowPCG.HollowBiomeDefinition")]
ASSET_TYPE_INFO = '(PrimaryAssetType="%s",AssetBaseClass="%s",bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=((Path="%s")))'
LEVEL_DIR = SECTOR_ROOT + "/Rooms"
DATA_DIR = SECTOR_ROOT + "/Data"
LEVEL_PREFIX = "LV_Cathedral_"
DATA_PREFIX = "DA_Room_Cathedral_"
BIOME_PATH = SECTOR_ROOT + "/Data/DA_Biome_Cathedral"
DOCKS_BIOME_PATH = "/HollowSector_Docks/Data/DA_Biome_Docks"   # the assembly rules mirrored (create_raid_assets.py and later)

# Generator rules of the Cathedral (data; tune in the editor afterwards). Counts are rooms, routes are links.
# Measured by Hollow.PCG.Generator.CathedralPool: 1000/1000 seeds generate.
BIOME_RULES = {
    "spawn_room_type": "Hollow.Room.Type.Spawn",
    "extraction_room_type": "Hollow.Room.Type.Extraction",
    "core_vault_room_type": "Hollow.Room.Type.CoreVault",
    "path_room_types": ["Hollow.Room.Type.Corridor", "Hollow.Room.Type.Junction", "Hollow.Room.Type.Hangar"],
    "branch_room_types": ["Hollow.Room.Type.Storage"],
    "critical_path_min": 4,
    "critical_path_max": 6,
    "core_vault_count": 1,
    "optional_branches_min": 0,
    "optional_branches_max": 2,
    "max_branch_depth": 2,
    "min_loops": 2,
    "max_bridge_rooms": 3,
    "min_bulkhead_connections": 2,
    "max_rooms": 14,
    "max_extent_cells": 24,          # the NavMeshBoundsVolume of L_Raid_Dev covers the Docks' extent: never more
    "max_attempts": 32,
    "max_placements_per_attempt": 3000,
    # S7-02 (ADR-0039): every Cathedral sector has an altar and an arc (a layout without either is a failed attempt).
    "required_socket_types": ["Hollow.Socket.Altar", "Hollow.Socket.ArcHazard"],
}

CUBE_MESH = "/Engine/BasicShapes/Cube.Cube"
CUBE_SIZE = 100.0                                   # BasicShapes cube edge, uu
GREYBOX_MATERIAL = "/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"

# Gothic kit (uu; the spec's art numbers, checked against the metrics below).
PIER = 80.0                 # square pier at an inner grid point of a hall
PLINTH = 100.0              # its base: plinths leave MinCorridorWidth between them
PLINTH_HEIGHT = 30.0        # below MaxStepHeight (45): a foot, not a wall
CAPITAL = 120.0
CAPITAL_HEIGHT = 30.0
PILASTER_WIDTH = 60.0       # wall shaft at every inner cell boundary
PILASTER_DEPTH = 20.0
SHAFT = 20.0                # door shafts beside an opening of a tall hall (the arch springs from them)
ARCH_RISE = 150.0           # pointed arch over an opening of a tall hall, from the door head
ARCH_DEPTH = 20.0
ARCH_THICKNESS = 25.0
RIB_WIDTH = 20.0
RIB_HEIGHT = 20.0
BOSS = 40.0
VAULT_RISE = {1: 80.0, 2: 200.0}   # ribs spring this far below the ceiling (per storeys): one storey keeps 300+ uu clear
CRYPT_VAULT_RISE = 60.0            # the reactor crypt is lower still
BAND_DEPTH = 30.0           # string course of a two-storey hall, on top of the first storey
BAND_HEIGHT = 20.0
LANCET_WIDTH = 80.0         # blind lancets of the upper tier (the hall's "second light"), one per wall cell
LANCET_DEPTH = 10.0
LANCET_SILL = 120.0         # above the string course
LANCET_HEAD = 100.0         # below the ceiling, the point rises into it
LANCET_POINT = 60.0

# Spec checks (uu).
SOCKET_WALL_CLEARANCE = 100.0       # LevelMetrics.md sec. 4
VENT_WALL_CLEARANCE = 40.0          # vent nodes sit against a wall (the Echo-Stalker's hatch), as in the Docks
SOCKET_PIER_CLEARANCE = 100.0
PAWN_RADIUS = 34.0                  # BP_ExoFrame capsule (create_exoframe_assets.py)
OPENING_APPROACH = 300.0            # keep this deep a strip in front of every opening free of sockets and piers
EXTRACTION_ZONE_HALF = 300.0        # BP_ExtractionZone volume half size (create_raid_flow_assets.py ZONE_HALF)
TALL_HALLS = 4

Connector = collections.namedtuple("Connector", "x y side kinds")
Socket = collections.namedtuple("Socket", "type x y yaw")    # x, y in cells (fractions allowed), yaw in degrees
Room = collections.namedtuple("Room", "name type footprint storeys connectors sockets piers vault_rise")

DOOR = unreal.HollowConnectorKind.DOOR.value
BULKHEAD = unreal.HollowConnectorKind.BULKHEAD.value
OPEN = unreal.HollowConnectorKind.OPEN.value
ANY = DOOR | BULKHEAD | OPEN
N, E, S, W = "NORTH", "EAST", "SOUTH", "WEST"   # North = +X, East = +Y

ROOMS = [
    # Narthex: the porch the squad lands in, one door on.
    Room("Narthex", "Spawn", (2, 2), 1, [Connector(1, 0, N, DOOR)],
         [Socket("PlayerSpawn", 0.5, 0.4, 0.0), Socket("PlayerSpawn", 0.5, 0.8, 0.0),
          Socket("PlayerSpawn", 0.5, 1.2, 0.0), Socket("PlayerSpawn", 0.5, 1.6, 0.0)], [], None),
    # Belfry: the bell tower the shuttle takes the squad from; a tall empty shaft over the extraction zone.
    Room("Extraction_Belfry", "Extraction", (3, 2), 2, [Connector(0, 0, S, DOOR | OPEN)],
         [Socket("Extraction", 2.0, 1.0, 0.0), Socket("VentNode", 2.8, 1.8, 0.0)], [], None),
    # Reactor crypt: low ribbed vault over the Aether core.
    Room("ReactorCrypt", "CoreVault", (2, 2), 1, [Connector(0, 0, S, DOOR | BULKHEAD), Connector(1, 1, N, DOOR | BULKHEAD)],
         [Socket("CoreSpawn", 1.0, 1.0, 0.0), Socket("Loot", 0.3, 1.7, 0.0), Socket("Loot", 1.7, 0.3, 0.0),
          Socket("VentNode", 0.5, 1.85, 0.0)], [], CRYPT_VAULT_RISE),
    # Nave: two-storey hall, three aisles between two rows of piers; the north end of the middle aisle stays free (the
    # apse: S7-02's altar).
    Room("Nave", "Hangar", (4, 3), 2,
         [Connector(0, 1, S, ANY), Connector(1, 0, W, ANY), Connector(1, 2, E, ANY), Connector(3, 0, W, ANY)],
         [Socket("Loot", 0.3, 0.3, 0.0), Socket("Loot", 0.3, 2.7, 0.0), Socket("Loot", 3.7, 2.7, 0.0),
          Socket("EnemySpawn", 2.5, 0.5, 180.0), Socket("EnemySpawn", 2.5, 2.5, 0.0), Socket("VentNode", 3.8, 2.5, 0.0),
          Socket("Altar", 3.5, 1.5, 180.0)],   # S7-02: the apse, the altar facing down the nave
         [(1, 1), (2, 1), (3, 1), (1, 2), (2, 2), (3, 2)], None),
    # Crossing: two-storey square under the tower, four piers round the middle bay, an exit on every side.
    Room("Crossing", "Junction", (3, 3), 2,
         [Connector(2, 1, N, ANY), Connector(1, 2, E, ANY), Connector(0, 1, S, ANY), Connector(1, 0, W, ANY)],
         [Socket("Loot", 0.3, 0.3, 0.0), Socket("Loot", 2.7, 2.7, 0.0), Socket("EnemySpawn", 2.5, 0.5, 180.0),
          Socket("VentNode", 0.5, 2.8, 0.0)],
         [(1, 1), (2, 1), (1, 2), (2, 2)], None),
    # Cloister walks: one-storey arcades.
    Room("Cloister_Straight", "Corridor", (1, 2), 1, [Connector(0, 0, W, ANY), Connector(0, 1, E, ANY)],
         [Socket("VentNode", 0.5, 1.0, 0.0),   # between the openings' approaches and the arcade shafts
          Socket("ArcHazard", 0.5, 1.0, 90.0)], [], None),   # S7-02: across the walk on the shafts' line, looking along it
    Room("Cloister_Corner", "Corridor", (1, 1), 1, [Connector(0, 0, S, ANY), Connector(0, 0, E, ANY)], [], [], None),
    # Side chapel: two-storey, its middle stays free (an altar's place, S7-02).
    Room("Chapel", "Storage", (2, 2), 2, [Connector(0, 0, W, DOOR | BULKHEAD), Connector(1, 1, E, DOOR | BULKHEAD)],
         [Socket("Loot", 0.3, 1.7, 0.0), Socket("Loot", 1.7, 0.3, 0.0), Socket("Loot", 0.3, 1.1, 0.0),
          Socket("EnemySpawn", 1.0, 0.4, 90.0), Socket("VentNode", 1.8, 0.5, 0.0),
          Socket("Altar", 1.0, 1.0, 0.0)], [], None),   # S7-02: the middle of the chapel
]

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(message):
    unreal.log("create_cathedral_greybox: " + message)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset " + path)
    return asset


def save(asset):
    if not assets.save_loaded_asset(asset, False):
        raise RuntimeError("Could not save " + asset.get_path_name())


def make_tag(name):
    tag = unreal.GameplayTag()
    if not tag.import_text('(TagName="%s")' % name):
        raise RuntimeError("Unknown gameplay tag " + name)
    return tag


def make_tags(*names):
    tags = unreal.GameplayTagContainer()
    if not tags.import_text("(GameplayTags=(%s))" % ",".join('(TagName="%s")' % n for n in names)):
        raise RuntimeError("Bad gameplay tags %s" % (names,))
    return tags


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


class Metrics(object):
    """UHollowLevelMetrics as plain floats (uu)."""

    def __init__(self):
        cdo = unreal.get_default_object(unreal.HollowLevelMetrics)
        for name in ("cell_size", "cell_height", "wall_thickness", "slab_thickness", "door_width", "door_height",
                     "min_corridor_width", "largest_agent_radius", "largest_agent_height"):
            value = float(cdo.get_editor_property(name))
            if value <= 0.0:
                raise RuntimeError("HollowLevelMetrics.%s is not set (DefaultGame.ini)" % name)
            setattr(self, name, value)
        self.max_room_storeys = int(cdo.get_editor_property("max_room_storeys"))
        if self.max_room_storeys < 2:
            raise RuntimeError("HollowLevelMetrics.MaxRoomStoreys must allow two-storey halls (DefaultGame.ini)")


def room_height(metrics, room):
    return room.storeys * metrics.cell_height


def vault_rise(room):
    return room.vault_rise if room.vault_rise is not None else VAULT_RISE[room.storeys]


# ---------------------------------------------------------------- spec checks (before building)

def opening_span(metrics, connector):
    """Interval of the opening along its wall (Y for North/South, X for East/West)."""
    cell = connector.y if connector.side in (N, S) else connector.x
    start = cell * metrics.cell_size + (metrics.cell_size - metrics.door_width) / 2.0
    return start, start + metrics.door_width


def approach_box(metrics, room, connector):
    """Floor strip in front of an opening (x0, y0, x1, y1): kept clear of sockets and piers."""
    t = metrics.wall_thickness
    size_x = room.footprint[0] * metrics.cell_size
    size_y = room.footprint[1] * metrics.cell_size
    a, b = opening_span(metrics, connector)
    if connector.side == S:
        return (t, a, t + OPENING_APPROACH, b)
    if connector.side == N:
        return (size_x - t - OPENING_APPROACH, a, size_x - t, b)
    if connector.side == W:
        return (a, t, b, t + OPENING_APPROACH)
    return (a, size_y - t - OPENING_APPROACH, b, size_y - t)


def pilaster_boxes(metrics, room):
    """Footprints (x0, y0, x1, y1) of the wall shafts at every inner cell boundary."""
    t = metrics.wall_thickness
    c = metrics.cell_size
    size_x = room.footprint[0] * c
    size_y = room.footprint[1] * c
    half = PILASTER_WIDTH / 2.0
    boxes = []
    for k in range(1, room.footprint[1]):            # along the North and South walls
        y = k * c
        boxes.append((t, y - half, t + PILASTER_DEPTH, y + half))
        boxes.append((size_x - t - PILASTER_DEPTH, y - half, size_x - t, y + half))
    for k in range(1, room.footprint[0]):            # along the East and West walls
        x = k * c
        boxes.append((x - half, t, x + half, t + PILASTER_DEPTH))
        boxes.append((x - half, size_y - t - PILASTER_DEPTH, x + half, size_y - t))
    return boxes


def plinth_box(metrics, pier):
    x, y = pier[0] * metrics.cell_size, pier[1] * metrics.cell_size
    half = PLINTH / 2.0
    return (x - half, y - half, x + half, y + half)


def overlaps(a, b):
    return a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]


def box_distance(x, y, box):
    dx = max(box[0] - x, 0.0, x - box[2])
    dy = max(box[1] - y, 0.0, y - box[3])
    return math.hypot(dx, dy)


def check_spec(metrics):
    """The kit itself: piers leave corridors and openings free, every socket stands on clear floor."""
    check(len(ROOMS) == 8, "eight rooms")
    check(sum(1 for r in ROOMS if r.storeys == 2) == TALL_HALLS, "%d two-storey halls" % TALL_HALLS)
    c = metrics.cell_size
    t = metrics.wall_thickness
    check(c - PLINTH >= metrics.min_corridor_width and c - PLINTH / 2.0 - t >= metrics.min_corridor_width,
          "plinths leave MinCorridorWidth between them and to the walls")
    check(2.0 * metrics.largest_agent_radius <= c - PLINTH, "the largest agent passes between piers")
    check(PLINTH_HEIGHT < 45.0, "plinths stay below MaxStepHeight")
    check(VAULT_RISE[1] <= metrics.cell_height - metrics.largest_agent_height - 20.0, "one-storey ribs above the largest agent")
    for room in ROOMS:
        check(1 <= room.storeys <= metrics.max_room_storeys, "storeys of " + room.name)
        size_x = room.footprint[0] * c
        size_y = room.footprint[1] * c
        inner = (t, t, size_x - t, size_y - t)
        approaches = [approach_box(metrics, room, conn) for conn in room.connectors]
        plinths = [plinth_box(metrics, p) for p in room.piers]
        pilasters = pilaster_boxes(metrics, room)
        for pier in room.piers:
            check(0 < pier[0] < room.footprint[0] and 0 < pier[1] < room.footprint[1], "%s: pier %s on an inner grid point" % (room.name, pier))
        for index, approach in enumerate(approaches):
            for plinth in plinths:
                check(not overlaps(approach, plinth), "%s: a pier stands in front of opening %d" % (room.name, index))
        for s in room.sockets:
            x, y = s.x * c, s.y * c
            wall = VENT_WALL_CLEARANCE if s.type == "VentNode" else SOCKET_WALL_CLEARANCE
            check(min(x - inner[0], y - inner[1], inner[2] - x, inner[3] - y) >= wall,
                  "%s: %s at (%.0f, %.0f) is %.0f uu from the walls" % (room.name, s.type, x, y, wall))
            for plinth in plinths:
                check(box_distance(x, y, plinth) >= SOCKET_PIER_CLEARANCE, "%s: %s at (%.0f, %.0f) near a pier" % (room.name, s.type, x, y))
            for pilaster in pilasters:
                check(box_distance(x, y, pilaster) > PAWN_RADIUS, "%s: %s at (%.0f, %.0f) inside a pilaster" % (room.name, s.type, x, y))
            for index, approach in enumerate(approaches):
                check(not (approach[0] <= x <= approach[2] and approach[1] <= y <= approach[3]),
                      "%s: %s at (%.0f, %.0f) in front of opening %d" % (room.name, s.type, x, y, index))
            if s.type == "Extraction":
                zone = (x - EXTRACTION_ZONE_HALF, y - EXTRACTION_ZONE_HALF, x + EXTRACTION_ZONE_HALF, y + EXTRACTION_ZONE_HALF)
                check(zone[0] >= inner[0] and zone[1] >= inner[1] and zone[2] <= inner[2] and zone[3] <= inner[3]
                      and not any(overlaps(zone, p) for p in plinths), "%s: the extraction zone fits" % room.name)


# ---------------------------------------------------------------- game feature data and biome

def ensure_feature_data():
    """Creates the GameFeatureData if missing and adds every type of SCANNED_TYPES it does not scan yet."""
    created = False
    if assets.does_asset_exist(FEATURE_DATA_PATH):
        feature = load(FEATURE_DATA_PATH)
    else:
        package_path, name = FEATURE_DATA_PATH.rsplit("/", 1)
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.GameFeatureData)
        feature = tools.create_asset(name, package_path, unreal.GameFeatureData, factory)
        if feature is None:
            raise RuntimeError("Could not create " + FEATURE_DATA_PATH)
        created = True

    scanned = list(feature.get_editor_property("primary_asset_types_to_scan"))
    known = [t.export_text() for t in scanned]
    added = 0
    for type_name, base_class in SCANNED_TYPES:
        if any('"%s"' % type_name in text for text in known):
            continue
        info = unreal.PrimaryAssetTypeInfo()
        if not info.import_text(ASSET_TYPE_INFO % (type_name, base_class, DATA_DIR)):
            raise RuntimeError("Bad primary asset type info for " + type_name)
        scanned.append(info)
        added += 1
    if created or added:
        feature.set_editor_property("primary_asset_types_to_scan", scanned)
        save(feature)
        log("GameFeatureData: %s, %d type(s) added" % ("created" if created else "updated", added))


def ensure_biome():
    if assets.does_asset_exist(BIOME_PATH):
        return False
    package_path, name = BIOME_PATH.rsplit("/", 1)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.HollowBiomeDefinition)
    biome = tools.create_asset(name, package_path, unreal.HollowBiomeDefinition, factory)
    if biome is None:
        raise RuntimeError("Could not create " + BIOME_PATH)
    rules = unreal.HollowSectorGenRules()
    for key, value in BIOME_RULES.items():
        if isinstance(value, list):
            rules.set_editor_property(key, make_tags(*value))
        elif isinstance(value, str):
            rules.set_editor_property(key, make_tag(value))
        else:
            rules.set_editor_property(key, value)
    biome.set_editor_property("gen_rules", rules)
    save(biome)
    return True


def ensure_required_sockets():
    """The required socket types follow BIOME_RULES while the biome has none (S7-02 added them; edits stay)."""
    biome = load(BIOME_PATH)
    rules = biome.get_editor_property("gen_rules")
    if rules.get_editor_property("required_socket_types").export_text() not in ("", "()", "(GameplayTags=)"):
        return False
    rules.set_editor_property("required_socket_types", make_tags(*BIOME_RULES["required_socket_types"]))
    biome.set_editor_property("gen_rules", rules)
    save(biome)
    log("DA_Biome_Cathedral requires " + ", ".join(BIOME_RULES["required_socket_types"]))
    return True


def class_key(value):
    if value is None:
        return ""
    return value.get_path_name() if hasattr(value, "get_path_name") else str(value)


def socket_rule_key(rule):
    return (rule.get_editor_property("socket_type").export_text(), class_key(rule.get_editor_property("actor_class")))


def connection_rule_key(rule):
    return str(rule.get_editor_property("kind"))


def mirrored(docks_rules, own_rules, key):
    """The Docks' rules in their order (the Cathedral's copy where it has one, so its edits stay), then its own."""
    own = {key(r): r for r in own_rules}
    docks_keys = set(key(r) for r in docks_rules)
    return [own.get(key(r), r) for r in docks_rules] + [r for r in own_rules if key(r) not in docks_keys]


def ensure_assembly():
    """Doors, bulkheads, loot, core, enemies, extraction and the sealed plug as in the Docks (S7-02 adds the Cathedral's own)."""
    docks = load(DOCKS_BIOME_PATH)
    biome = load(BIOME_PATH)
    changed = []
    plug = biome.get_editor_property("sealed_plug")
    if plug.get_editor_property("mesh") is None:
        biome.set_editor_property("sealed_plug", docks.get_editor_property("sealed_plug"))
        changed.append("sealed plug")
    for name, key in (("socket_actors", socket_rule_key), ("connection_actors", connection_rule_key)):
        own = list(biome.get_editor_property(name))
        wanted = mirrored(list(docks.get_editor_property(name)), own, key)
        if [key(r) for r in wanted] != [key(r) for r in own]:
            biome.set_editor_property(name, wanted)
            changed.append(name)
    if changed:
        save(biome)
        log("assembly rules of DA_Biome_Cathedral now mirror DA_Biome_Docks: " + ", ".join(changed))


# ---------------------------------------------------------------- level geometry

def beam(a, b, width, height):
    """Transform of an engine cube stretched from point a to point b (room-local uu), width x height across."""
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    length = math.sqrt(dx * dx + dy * dy + dz * dz)
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
    return unreal.Transform(location=unreal.Vector((a[0] + b[0]) / 2.0, (a[1] + b[1]) / 2.0, (a[2] + b[2]) / 2.0),
                            rotation=unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw),
                            scale=unreal.Vector(length / CUBE_SIZE, width / CUBE_SIZE, height / CUBE_SIZE))


def block(lo, hi):
    """Transform of an engine cube filling an axis-aligned box between two corners (room-local uu)."""
    return unreal.Transform(location=unreal.Vector((lo[0] + hi[0]) / 2.0, (lo[1] + hi[1]) / 2.0, (lo[2] + hi[2]) / 2.0),
                            rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0),
                            scale=unreal.Vector((hi[0] - lo[0]) / CUBE_SIZE, (hi[1] - lo[1]) / CUBE_SIZE, (hi[2] - lo[2]) / CUBE_SIZE))


class Builder(object):
    def __init__(self, metrics):
        self.m = metrics
        self.cube = load(CUBE_MESH)
        self.material = load(GREYBOX_MATERIAL)

    # -------- structure (as the Docks kit, walls as tall as the room)

    def box(self, label, lo, hi):
        """Static box between two corners (room-local uu)."""
        center = unreal.Vector((lo[0] + hi[0]) / 2.0, (lo[1] + hi[1]) / 2.0, (lo[2] + hi[2]) / 2.0)
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, center, unreal.Rotator(0.0, 0.0, 0.0))
        if actor is None:
            raise RuntimeError("Could not spawn " + label)
        actor.set_actor_label(label)
        actor.set_folder_path("Geometry")
        component = actor.static_mesh_component
        component.set_static_mesh(self.cube)
        component.set_material(0, self.material)
        actor.set_actor_scale3d(unreal.Vector((hi[0] - lo[0]) / CUBE_SIZE, (hi[1] - lo[1]) / CUBE_SIZE, (hi[2] - lo[2]) / CUBE_SIZE))

    def wall(self, room, side, height):
        m = self.m
        size_x = room.footprint[0] * m.cell_size
        size_y = room.footprint[1] * m.cell_size
        t = m.wall_thickness
        # North/South walls run the full edge; East/West walls fit between them.
        if side in (N, S):
            span = (0.0, size_y)
            across = (size_x - t, size_x) if side == N else (0.0, t)
        else:
            span = (t, size_x - t)
            across = (size_y - t, size_y) if side == E else (0.0, t)

        def piece(label, a, b, z0, z1):
            if side in (N, S):
                self.box(label, (across[0], a, z0), (across[1], b, z1))
            else:
                self.box(label, (a, across[0], z0), (b, across[1], z1))

        cursor, index = span[0], 0
        for start, end in sorted(opening_span(m, c) for c in room.connectors if c.side == side):
            if start > cursor:
                piece("Wall_%s_%d" % (side, index), cursor, start, 0.0, height)
                index += 1
            piece("Lintel_%s_%d" % (side, index), start, end, m.door_height, height)
            index += 1
            cursor = end
        if span[1] > cursor:
            piece("Wall_%s_%d" % (side, index), cursor, span[1], 0.0, height)

    # -------- gothic decor

    def wall_frame(self, room, side):
        """(axis along the wall: 'x' or 'y', inner face coordinate, direction into the room, wall length)."""
        m = self.m
        size_x = room.footprint[0] * m.cell_size
        size_y = room.footprint[1] * m.cell_size
        t = m.wall_thickness
        if side == S:
            return "y", t, 1.0, size_y
        if side == N:
            return "y", size_x - t, -1.0, size_y
        if side == W:
            return "x", t, 1.0, size_x
        return "x", size_y - t, -1.0, size_x

    def on_wall(self, room, side, along0, along1, depth, z0, z1):
        """Box against the inner face of a wall: along the wall [along0, along1], depth into the room, height [z0, z1]."""
        axis, face, inward, _ = self.wall_frame(room, side)
        d0, d1 = sorted((face, face + inward * depth))
        if axis == "y":
            return block((d0, along0, z0), (d1, along1, z1))
        return block((along0, d0, z0), (along1, d1, z1))

    def wall_point(self, room, side, along, depth, z):
        axis, face, inward, _ = self.wall_frame(room, side)
        d = face + inward * depth
        return (d, along, z) if axis == "y" else (along, d, z)

    def decor(self, room):
        """Two lists of transforms: solid (blocks pawns: piers, plinths, pilasters, door shafts) and vault (out of reach)."""
        m = self.m
        c = m.cell_size
        height = room_height(m, room)
        rise = vault_rise(room)
        tall = room.storeys > 1
        solid, vault = [], []

        for gx, gy in room.piers:
            x, y = gx * c, gy * c
            solid.append(block((x - PIER / 2.0, y - PIER / 2.0, 0.0), (x + PIER / 2.0, y + PIER / 2.0, height)))
            solid.append(block((x - PLINTH / 2.0, y - PLINTH / 2.0, 0.0), (x + PLINTH / 2.0, y + PLINTH / 2.0, PLINTH_HEIGHT)))
            spring = height - rise
            vault.append(block((x - CAPITAL / 2.0, y - CAPITAL / 2.0, spring - CAPITAL_HEIGHT), (x + CAPITAL / 2.0, y + CAPITAL / 2.0, spring)))

        for side in (N, E, S, W):
            axis, _, _, length = self.wall_frame(room, side)
            cells = room.footprint[1] if axis == "y" else room.footprint[0]
            for k in range(1, cells):
                solid.append(self.on_wall(room, side, k * c - PILASTER_WIDTH / 2.0, k * c + PILASTER_WIDTH / 2.0, PILASTER_DEPTH, 0.0, height))
            if not tall:
                continue
            # The second light: a string course on the first storey, a blind lancet over every wall cell.
            vault.append(self.on_wall(room, side, 0.0, length, BAND_DEPTH, m.cell_height, m.cell_height + BAND_HEIGHT))
            sill = m.cell_height + BAND_HEIGHT + LANCET_SILL
            head = height - LANCET_HEAD
            for k in range(cells):
                mid = (k + 0.5) * c
                vault.append(self.on_wall(room, side, mid - LANCET_WIDTH / 2.0, mid + LANCET_WIDTH / 2.0, LANCET_DEPTH, sill, head))
                apex = self.wall_point(room, side, mid, LANCET_DEPTH / 2.0, head + LANCET_POINT)
                for edge in (mid - LANCET_WIDTH / 2.0, mid + LANCET_WIDTH / 2.0):
                    vault.append(beam(self.wall_point(room, side, edge, LANCET_DEPTH / 2.0, head), apex, LANCET_DEPTH, RIB_HEIGHT))
            # A portal round each opening: shafts at its sides, a pointed arch over its head.
            for connector in room.connectors:
                if connector.side != side:
                    continue
                start, end = opening_span(m, connector)
                solid.append(self.on_wall(room, side, start - SHAFT, start, SHAFT, 0.0, m.door_height))
                solid.append(self.on_wall(room, side, end, end + SHAFT, SHAFT, 0.0, m.door_height))
                apex = self.wall_point(room, side, (start + end) / 2.0, ARCH_DEPTH / 2.0, m.door_height + ARCH_RISE)
                for edge in (start - SHAFT / 2.0, end + SHAFT / 2.0):
                    vault.append(beam(self.wall_point(room, side, edge, ARCH_DEPTH / 2.0, m.door_height), apex, ARCH_DEPTH, ARCH_THICKNESS))

        # Rib vault: diagonal ribs of every bay meet at its boss; transverse ribs over every inner grid line.
        top = height - RIB_HEIGHT / 2.0
        spring = height - rise
        for bx in range(room.footprint[0]):
            for by in range(room.footprint[1]):
                cx, cy = (bx + 0.5) * c, (by + 0.5) * c
                for corner in ((bx * c, by * c), ((bx + 1) * c, by * c), (bx * c, (by + 1) * c), ((bx + 1) * c, (by + 1) * c)):
                    vault.append(beam((corner[0], corner[1], spring), (cx, cy, top), RIB_WIDTH, RIB_HEIGHT))
                vault.append(block((cx - BOSS / 2.0, cy - BOSS / 2.0, height - BOSS / 2.0), (cx + BOSS / 2.0, cy + BOSS / 2.0, height)))
        for gx in range(1, room.footprint[0]):
            for by in range(room.footprint[1]):
                x, y0, y1 = gx * c, by * c, (by + 1) * c
                mid = (x, (y0 + y1) / 2.0, top)
                vault.append(beam((x, y0, spring), mid, RIB_WIDTH, RIB_HEIGHT))
                vault.append(beam((x, y1, spring), mid, RIB_WIDTH, RIB_HEIGHT))
        for gy in range(1, room.footprint[1]):
            for bx in range(room.footprint[0]):
                y, x0, x1 = gy * c, bx * c, (bx + 1) * c
                mid = ((x0 + x1) / 2.0, y, top)
                vault.append(beam((x0, y, spring), mid, RIB_WIDTH, RIB_HEIGHT))
                vault.append(beam((x1, y, spring), mid, RIB_WIDTH, RIB_HEIGHT))
        return solid, vault

    def instanced(self, owner, name, transforms, solid):
        component = unreal.HollowAssetAuthoring.add_instance_component(owner, unreal.InstancedStaticMeshComponent, name)
        if component is None:
            raise RuntimeError("Could not add " + name)
        component.set_mobility(unreal.ComponentMobility.STATIC)
        component.set_static_mesh(self.cube)
        component.set_material(0, self.material)
        if solid:
            component.set_collision_profile_name("BlockAll")
        else:
            component.set_collision_profile_name("NoCollision")
            component.set_editor_property("can_ever_affect_navigation", False)
        for transform in transforms:
            component.add_instance(transform, False)

    def build(self, room):
        m = self.m
        size_x = room.footprint[0] * m.cell_size
        size_y = room.footprint[1] * m.cell_size
        height = room_height(m, room)
        self.box("Floor", (0.0, 0.0, -m.slab_thickness), (size_x, size_y, 0.0))
        self.box("Ceiling", (0.0, 0.0, height), (size_x, size_y, height + m.slab_thickness))
        for side in (N, E, S, W):
            self.wall(room, side, height)
        solid, vault = self.decor(room)
        owner = actors.spawn_actor_from_class(unreal.Actor, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
        if owner is None:
            raise RuntimeError("Could not spawn the decor of " + room.name)
        owner.set_actor_label("Decor")
        owner.set_folder_path("Decor")
        self.instanced(owner, "DecorSolid", solid, True)     # the first scene component becomes the actor's root
        self.instanced(owner, "DecorVault", vault, False)
        owner.set_actor_location(unreal.Vector(0.0, 0.0, 0.0), False, False)
        return len(solid), len(vault)
        # Lights and acoustics are S7-03's (DA_LightBudget, RE_Cathedral).


def level_path(room):
    return "%s/%s%s" % (LEVEL_DIR, LEVEL_PREFIX, room.name)


def data_path(room):
    return "%s/%s%s" % (DATA_DIR, DATA_PREFIX, room.name)


def ensure_level(builder, room):
    path = level_path(room)
    if assets.does_asset_exist(path):
        return False
    if not levels.new_level(path):
        raise RuntimeError("new_level failed for " + path)
    solid, vault = builder.build(room)
    if not levels.save_current_level():
        raise RuntimeError("save_current_level failed for " + path)
    log("built %s: %d solid and %d vault decor pieces" % (room.name, solid, vault))
    return True


# ---------------------------------------------------------------- room definition

def ensure_definition(metrics, room):
    path = data_path(room)
    if assets.does_asset_exist(path):
        return False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.HollowRoomDefinition)
    package_path, name = path.rsplit("/", 1)
    definition = tools.create_asset(name, package_path, unreal.HollowRoomDefinition, factory)
    if definition is None:
        raise RuntimeError("Could not create " + path)

    definition.set_editor_property("room_level", load(level_path(room)))
    definition.set_editor_property("footprint", unreal.IntVector(room.footprint[0], room.footprint[1], room.storeys))

    connectors = []
    for c in room.connectors:
        entry = unreal.HollowRoomConnector()
        entry.set_editor_property("cell", unreal.IntVector(c.x, c.y, 0))
        entry.set_editor_property("side", getattr(unreal.HollowRoomSide, c.side))
        entry.set_editor_property("allowed_kinds", c.kinds)
        connectors.append(entry)
    definition.set_editor_property("connectors", connectors)

    definition.set_editor_property("sockets", socket_entries(metrics, room))
    definition.set_editor_property("room_tags", make_tags("Hollow.Room.Type." + room.type))
    save(definition)
    return True


def socket_entries(metrics, room):
    sockets = []
    for s in room.sockets:
        entry = unreal.HollowRoomSocket()
        entry.set_editor_property("socket_type", make_tag("Hollow.Socket." + s.type))
        entry.set_editor_property("transform", unreal.Transform(
            location=unreal.Vector(s.x * metrics.cell_size, s.y * metrics.cell_size, 0.0),
            rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=s.yaw),
            scale=unreal.Vector(1.0, 1.0, 1.0)))
        sockets.append(entry)
    return sockets


def socket_key(entry):
    transform = entry.get_editor_property("transform")
    location = transform.translation
    return (entry.get_editor_property("socket_type").export_text(), round(location.x, 2), round(location.y, 2), round(location.z, 2),
            round(transform.rotation.rotator().yaw, 2))


def ensure_sockets(metrics, room):
    """Sockets of an existing definition follow ROOMS (not part of the layout hash)."""
    definition = load(data_path(room))
    wanted = socket_entries(metrics, room)
    if [socket_key(s) for s in definition.get_editor_property("sockets")] == [socket_key(s) for s in wanted]:
        return False
    definition.set_editor_property("sockets", wanted)
    save(definition)
    log("updated the sockets of " + room.name)
    return True


# ---------------------------------------------------------------- validation

def validate(metrics):
    feature = load(FEATURE_DATA_PATH)
    scanned = [t.export_text() for t in feature.get_editor_property("primary_asset_types_to_scan")]
    for type_name, _ in SCANNED_TYPES:
        check(any('"%s"' % type_name in t and DATA_DIR in t for t in scanned), "GameFeatureData scans %s in %s" % (type_name, DATA_DIR))
    biome = load(BIOME_PATH)
    docks = load(DOCKS_BIOME_PATH)
    rules = biome.get_editor_property("gen_rules")
    check(rules.get_editor_property("min_loops") > 0 and rules.get_editor_property("max_attempts") > 0, "DA_Biome_Cathedral rules are set")
    check(rules.get_editor_property("max_extent_cells") <= docks.get_editor_property("gen_rules").get_editor_property("max_extent_cells"),
          "the Cathedral fits the raid map's nav bounds (the Docks' extent)")
    check(biome.get_editor_property("sealed_plug").get_editor_property("mesh") is not None, "DA_Biome_Cathedral sealed plug")
    required = rules.get_editor_property("required_socket_types").export_text()
    check(all('"%s"' % tag in required for tag in BIOME_RULES["required_socket_types"]), "DA_Biome_Cathedral requires its mechanics' sockets")
    for tag in BIOME_RULES["required_socket_types"]:
        check(any(("Hollow.Socket." + s.type) == tag for room in ROOMS for s in room.sockets), "a room carries " + tag)
    for name, key in (("socket_actors", socket_rule_key), ("connection_actors", connection_rule_key)):
        own = [key(r) for r in biome.get_editor_property(name)]
        check([key(r) for r in docks.get_editor_property(name)] == own[:len(docks.get_editor_property(name))],
              "DA_Biome_Cathedral %s mirror the Docks" % name)
    for room in ROOMS:
        check(assets.does_asset_exist(level_path(room)), "level of " + room.name)
        definition = load(data_path(room))
        check(isinstance(definition, unreal.HollowRoomDefinition), "class of " + room.name)
        footprint = definition.get_editor_property("footprint")
        check((footprint.x, footprint.y, footprint.z) == (room.footprint[0], room.footprint[1], room.storeys), "footprint of " + room.name)
        check(len(definition.get_editor_property("connectors")) == len(room.connectors), "connectors of " + room.name)
        check(all(c.get_editor_property("cell").z == 0 for c in definition.get_editor_property("connectors")), "openings of %s on the floor" % room.name)
        check([socket_key(s) for s in definition.get_editor_property("sockets")] == [socket_key(s) for s in socket_entries(metrics, room)],
              "sockets of " + room.name)
        level = definition.get_editor_property("room_level")
        check(level is not None and level.get_path_name().startswith(level_path(room)), "room_level of " + room.name)
    log("validation passed (%d rooms, %d two-storey; full rules: Hollow.PCG.CathedralRooms, Hollow.PCG.CathedralOpenings)"
        % (len(ROOMS), sum(1 for r in ROOMS if r.storeys == 2)))


def main():
    metrics = Metrics()
    check_spec(metrics)
    ensure_feature_data()
    if ensure_biome():
        log("created " + BIOME_PATH)
    ensure_required_sockets()
    ensure_assembly()
    builder = Builder(metrics)
    built_levels = sum(1 for room in ROOMS if ensure_level(builder, room))
    built_data = sum(1 for room in ROOMS if ensure_definition(metrics, room))
    updated = sum(1 for room in ROOMS if ensure_sockets(metrics, room))
    log("created %d level(s) and %d room definition(s), updated %d socket list(s) of %d rooms" % (built_levels, built_data, updated, len(ROOMS)))
    validate(metrics)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_cathedral_greybox failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
