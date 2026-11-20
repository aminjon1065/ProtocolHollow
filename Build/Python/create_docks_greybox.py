"""Builds the greybox kit of sector I "Orbital Docks" inside its Game Feature plugin (idempotent, ADR-0010, ADR-0012).

  /HollowSector_Docks/HollowSector_Docks            UGameFeatureData: adds /HollowSector_Docks/Data to HollowRoom and HollowBiome
  /HollowSector_Docks/Data/DA_Biome_Docks           UHollowBiomeDefinition: sector generator rules (S2-01)
For every room of ROOMS:
  /HollowSector_Docks/Rooms/LV_Docks_<Name>         static shell: floor, ceiling, walls with an opening at every
                                                    connector, one light (no gameplay actors, ADR-0003)
  /HollowSector_Docks/Data/DA_Room_Docks_<Name>     UHollowRoomDefinition: footprint, connectors, sockets, type

Level and data asset are generated from the same spec, so openings and connectors match by construction.
All sizes come from UHollowLevelMetrics (DefaultGame.ini); conventions in Docs/QA/LevelMetrics.md.
Creates only what is missing (never overwrites work done in the editor), keeps the sockets of existing definitions in step
with ROOMS (S4-01: VentNode openings of the Echo-Stalker's vents, ADR-0019), then validates.
Run: Build/Scripts/run-editor-python.sh Build/Python/create_docks_greybox.py
"""
import collections
import math
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

SECTOR_ROOT = "/HollowSector_Docks"                 # mount point of Plugins/GameFeatures/HollowSector_Docks
FEATURE_DATA_PATH = SECTOR_ROOT + "/HollowSector_Docks"   # the engine requires <root>/<PluginName>
# Primary asset types the plugin adds its Data directory to (cook rules come from DefaultGame.ini, ADR-0012).
SCANNED_TYPES = [("HollowRoom", "/Script/HollowPCG.HollowRoomDefinition"),
                 ("HollowBiome", "/Script/HollowPCG.HollowBiomeDefinition")]
ASSET_TYPE_INFO = '(PrimaryAssetType="%s",AssetBaseClass="%s",bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=((Path="%s")))'
LEVEL_DIR = SECTOR_ROOT + "/Rooms"
DATA_DIR = SECTOR_ROOT + "/Data"
LEVEL_PREFIX = "LV_Docks_"
DATA_PREFIX = "DA_Room_Docks_"
BIOME_PATH = SECTOR_ROOT + "/Data/DA_Biome_Docks"

# Generator rules of the Docks (data; tune in the editor afterwards). Counts are rooms, routes are links.
# Measured on 1000 seeds (S2-01): every seed generates, ~3.2 attempts and ~2 ms per sector.
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
    "max_bridge_rooms": 3,          # 2 leaves ~1.4% of seeds without a layout in 32 attempts
    "min_bulkhead_connections": 2,
    "max_rooms": 14,
    "max_extent_cells": 24,
    "max_attempts": 32,
    "max_placements_per_attempt": 3000,
}

CUBE_MESH = "/Engine/BasicShapes/Cube.Cube"
CUBE_SIZE = 100.0                                   # BasicShapes cube edge, uu
GREYBOX_MATERIAL = "/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"

# Greybox lighting (data; replaced on the art pass).
LIGHT_DROP = 50.0          # uu below the ceiling
LIGHT_INTENSITY = 5000.0   # lumens
LIGHT_COLOR = unreal.Color(r=255, g=236, b=210, a=255)

Connector = collections.namedtuple("Connector", "x y side kinds")
Socket = collections.namedtuple("Socket", "type x y yaw")    # x, y in cells (fractions allowed), yaw in degrees
Room = collections.namedtuple("Room", "name type footprint connectors sockets")

DOOR = unreal.HollowConnectorKind.DOOR.value
BULKHEAD = unreal.HollowConnectorKind.BULKHEAD.value
OPEN = unreal.HollowConnectorKind.OPEN.value
ANY = DOOR | BULKHEAD | OPEN
N, E, S, W = "NORTH", "EAST", "SOUTH", "WEST"   # North = +X, East = +Y

ROOMS = [
    Room("Spawn_Airlock", "Spawn", (2, 2), [Connector(1, 0, N, DOOR)],
         [Socket("PlayerSpawn", 0.5, 0.4, 0.0), Socket("PlayerSpawn", 0.5, 0.8, 0.0),
          Socket("PlayerSpawn", 0.5, 1.2, 0.0), Socket("PlayerSpawn", 0.5, 1.6, 0.0)]),
    Room("Extraction_ShuttlePad", "Extraction", (3, 2), [Connector(0, 0, S, DOOR | OPEN)],
         [Socket("Extraction", 2.0, 1.0, 0.0), Socket("VentNode", 2.8, 1.8, 0.0)]),
    Room("CoreVault", "CoreVault", (2, 2), [Connector(0, 0, S, DOOR | BULKHEAD), Connector(1, 1, N, DOOR | BULKHEAD)],
         [Socket("CoreSpawn", 1.0, 1.0, 0.0), Socket("Loot", 0.3, 1.7, 0.0), Socket("Loot", 1.7, 0.3, 0.0), Socket("VentNode", 1.0, 1.85, 0.0)]),
    Room("Hangar", "Hangar", (3, 3),
         [Connector(2, 1, N, ANY), Connector(1, 2, E, ANY), Connector(0, 1, S, ANY), Connector(1, 0, W, ANY)],
         [Socket("Loot", 0.3, 0.3, 0.0), Socket("Loot", 2.7, 2.7, 0.0), Socket("Loot", 0.3, 2.7, 0.0),
          Socket("EnemySpawn", 2.5, 0.5, 180.0), Socket("EnemySpawn", 0.5, 2.0, 0.0), Socket("VentNode", 2.8, 2.0, 0.0)]),
    Room("Corridor_Straight", "Corridor", (1, 2), [Connector(0, 0, W, ANY), Connector(0, 1, E, ANY)], [Socket("VentNode", 0.85, 1.0, 0.0)]),
    Room("Corridor_Corner", "Corridor", (1, 1), [Connector(0, 0, S, ANY), Connector(0, 0, E, ANY)], []),
    Room("Junction_T", "Junction", (1, 1), [Connector(0, 0, S, ANY), Connector(0, 0, E, ANY), Connector(0, 0, W, ANY)], [Socket("VentNode", 0.85, 0.5, 0.0)]),
    Room("Storage", "Storage", (2, 2), [Connector(0, 0, W, DOOR | BULKHEAD), Connector(1, 1, E, DOOR | BULKHEAD)],
         [Socket("Loot", 0.3, 1.7, 0.0), Socket("Loot", 1.7, 0.3, 0.0), Socket("Loot", 1.7, 1.0, 0.0),
          Socket("EnemySpawn", 0.5, 1.0, 0.0), Socket("VentNode", 1.0, 1.8, 0.0)]),
]

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(message):
    unreal.log("create_docks_greybox: " + message)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset " + path)
    return asset


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


class Metrics(object):
    """UHollowLevelMetrics as plain floats (uu)."""

    def __init__(self):
        cdo = unreal.get_default_object(unreal.HollowLevelMetrics)
        for name in ("cell_size", "cell_height", "wall_thickness", "slab_thickness", "door_width", "door_height"):
            value = float(cdo.get_editor_property(name))
            if value <= 0.0:
                raise RuntimeError("HollowLevelMetrics.%s is not set (DefaultGame.ini)" % name)
            setattr(self, name, value)


# ---------------------------------------------------------------- game feature data

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
        if not assets.save_loaded_asset(feature, False):
            raise RuntimeError("Could not save " + FEATURE_DATA_PATH)
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
    if not assets.save_loaded_asset(biome, False):
        raise RuntimeError("Could not save " + BIOME_PATH)
    return True


# ---------------------------------------------------------------- level geometry

class Builder(object):
    def __init__(self, metrics):
        self.m = metrics
        self.cube = load(CUBE_MESH)
        self.material = load(GREYBOX_MATERIAL)

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

    def openings(self, room, side):
        """Opening intervals along the wall axis of one side (Y for North/South, X for East/West)."""
        m = self.m
        result = []
        for c in room.connectors:
            if c.side != side:
                continue
            cell = c.y if side in (N, S) else c.x
            start = cell * m.cell_size + (m.cell_size - m.door_width) / 2.0
            result.append((start, start + m.door_width))
        return sorted(result)

    def wall(self, room, side):
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
        for start, end in self.openings(room, side):
            if start > cursor:
                piece("Wall_%s_%d" % (side, index), cursor, start, 0.0, m.cell_height)
                index += 1
            piece("Lintel_%s_%d" % (side, index), start, end, m.door_height, m.cell_height)
            index += 1
            cursor = end
        if span[1] > cursor:
            piece("Wall_%s_%d" % (side, index), cursor, span[1], 0.0, m.cell_height)

    def build(self, room):
        m = self.m
        size_x = room.footprint[0] * m.cell_size
        size_y = room.footprint[1] * m.cell_size
        self.box("Floor", (0.0, 0.0, -m.slab_thickness), (size_x, size_y, 0.0))
        self.box("Ceiling", (0.0, 0.0, m.cell_height), (size_x, size_y, m.cell_height + m.slab_thickness))
        for side in (N, E, S, W):
            self.wall(room, side)

        light = actors.spawn_actor_from_class(unreal.PointLight,
                                              unreal.Vector(size_x / 2.0, size_y / 2.0, m.cell_height - LIGHT_DROP),
                                              unreal.Rotator(0.0, 0.0, 0.0))
        light.set_actor_label("Light")
        light.set_folder_path("Lights")
        component = light.point_light_component
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
        component.set_editor_property("intensity", LIGHT_INTENSITY)
        component.set_editor_property("light_color", LIGHT_COLOR)
        component.set_editor_property("attenuation_radius", math.hypot(size_x, size_y))
        component.set_editor_property("cast_shadows", False)


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
    builder.build(room)
    if not levels.save_current_level():
        raise RuntimeError("save_current_level failed for " + path)
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
    definition.set_editor_property("footprint", unreal.IntVector(room.footprint[0], room.footprint[1], 1))

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

    if not assets.save_loaded_asset(definition, False):
        raise RuntimeError("Could not save " + path)
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
    """Sockets of an existing definition follow ROOMS (S4-01 added VentNode openings; not part of the layout hash)."""
    definition = load(data_path(room))
    wanted = socket_entries(metrics, room)
    if [socket_key(s) for s in definition.get_editor_property("sockets")] == [socket_key(s) for s in wanted]:
        return False
    definition.set_editor_property("sockets", wanted)
    if not assets.save_loaded_asset(definition, False):
        raise RuntimeError("Could not save " + data_path(room))
    log("updated the sockets of " + room.name)
    return True


# ---------------------------------------------------------------- validation

def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate(metrics):
    feature = load(FEATURE_DATA_PATH)
    scanned = [t.export_text() for t in feature.get_editor_property("primary_asset_types_to_scan")]
    for type_name, _ in SCANNED_TYPES:
        check(any('"%s"' % type_name in t and DATA_DIR in t for t in scanned), "GameFeatureData scans %s in %s" % (type_name, DATA_DIR))
    rules = load(BIOME_PATH).get_editor_property("gen_rules")
    check(rules.get_editor_property("min_loops") > 0 and rules.get_editor_property("max_attempts") > 0, "DA_Biome_Docks rules are set")
    for room in ROOMS:
        check(assets.does_asset_exist(level_path(room)), "level of " + room.name)
        definition = load(data_path(room))
        check(isinstance(definition, unreal.HollowRoomDefinition), "class of " + room.name)
        check(len(definition.get_editor_property("connectors")) == len(room.connectors), "connectors of " + room.name)
        check([socket_key(s) for s in definition.get_editor_property("sockets")] == [socket_key(s) for s in socket_entries(metrics, room)],
              "sockets of " + room.name)
        level = definition.get_editor_property("room_level")
        check(level is not None and level.get_path_name().startswith(level_path(room)), "room_level of " + room.name)
    log("validation passed (%d rooms; full rules: Hollow.PCG.DocksRooms)" % len(ROOMS))


def main():
    ensure_feature_data()
    if ensure_biome():
        log("created " + BIOME_PATH)
    metrics = Metrics()
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
    unreal.log_error("create_docks_greybox failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
