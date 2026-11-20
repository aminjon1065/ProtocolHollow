"""Creates the S2-02 raid content (idempotent, ADR-0010).

  /Game/Hollow/Maps/L_Transition                       empty seamless-travel map (hub <-> raid)
  /Game/Hollow/Maps/L_Raid_Dev                         persistent raid map: NavMeshBoundsVolume over the largest sector,
                                                       game mode override BP_GameMode_Raid; rooms stream in at runtime
  /Game/Hollow/Core/Framework/BP_GameMode_Raid         AHollowRaidGameMode, default pawn BP_ExoFrame
  /Game/Hollow/World/Sector/BP_SectorMarker            placeholder gameplay actor for sockets and connections
  /HollowSector_Docks/Data/DA_Biome_Docks              assembly fields filled if empty: sealed plug, socket and
                                                       connection actor rules (created by create_docks_greybox.py)

Sizes come from UHollowLevelMetrics and the biome rules. Creates or fills only what is missing, then validates.
Run after create_dev_maps.py, create_exoframe_assets.py and create_docks_greybox.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_raid_assets.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

MAPS_DIR = "/Game/Hollow/Maps"
TRANSITION_MAP = MAPS_DIR + "/L_Transition"
RAID_MAP = MAPS_DIR + "/L_Raid_Dev"
RAID_GAME_MODE = "/Game/Hollow/Core/Framework/BP_GameMode_Raid"
EXOFRAME_BP = "/Game/Hollow/Characters/ExoFrame/BP_ExoFrame"
MARKER_BP = "/Game/Hollow/World/Sector/BP_SectorMarker"
BIOME = "/HollowSector_Docks/Data/DA_Biome_Docks"

CUBE_MESH = "/Engine/BasicShapes/Cube.Cube"
SPHERE_MESH = "/Engine/BasicShapes/Sphere.Sphere"
GREYBOX_MATERIAL = "/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"
MARKER_SCALE = unreal.Vector(0.3, 0.3, 0.3)      # 30 cm sphere: visible, never in the way (no collision)
NAV_BOUNDS_LABEL = "NavMeshBounds_Sector"

SOCKETS_TO_MARKER = ["Loot", "CoreSpawn", "EnemySpawn", "VentNode", "Extraction"]
PLAYER_SPAWN_SOCKET = "PlayerSpawn"
CONNECTION_KINDS = ["DOOR", "BULKHEAD", "OPEN"]

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(message):
    unreal.log("create_raid_assets: " + message)


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


def blueprint(path, parent_class):
    """Returns (blueprint, created)."""
    if assets.does_asset_exist(path):
        return load(path), False
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    package_path, name = path.rsplit("/", 1)
    asset = tools.create_asset(name, package_path, unreal.Blueprint, factory)
    if asset is None:
        raise RuntimeError("Could not create " + path)
    return asset, True


def ensure_marker():
    marker_bp, created = blueprint(MARKER_BP, unreal.HollowSectorMarker)
    if created:
        mesh = unreal.get_default_object(marker_bp.generated_class()).get_editor_property("mesh")
        mesh.set_editor_property("static_mesh", load(SPHERE_MESH))
        mesh.set_editor_property("relative_scale3d", MARKER_SCALE)
        save(marker_bp)
        log("created BP_SectorMarker")
    return marker_bp


def ensure_game_mode():
    game_mode_bp, created = blueprint(RAID_GAME_MODE, unreal.HollowRaidGameMode)
    if created:
        game_mode = unreal.get_default_object(game_mode_bp.generated_class())
        game_mode.set_editor_property("default_pawn_class", load(EXOFRAME_BP).generated_class())
        save(game_mode_bp)
        log("created BP_GameMode_Raid")
    return game_mode_bp


def ensure_transition_map():
    if assets.does_asset_exist(TRANSITION_MAP):
        return
    if not levels.new_level(TRANSITION_MAP) or not levels.save_current_level():
        raise RuntimeError("Could not create " + TRANSITION_MAP)
    log("created L_Transition")


def nav_bounds():
    """Centre and half size covering the largest sector of the Docks biome, one cell of margin around it."""
    metrics = unreal.get_default_object(unreal.HollowLevelMetrics)
    cell = float(metrics.get_editor_property("cell_size"))
    height = float(metrics.get_editor_property("cell_height"))
    slab = float(metrics.get_editor_property("slab_thickness"))
    extent_cells = load(BIOME).get_editor_property("gen_rules").get_editor_property("max_extent_cells")
    low = unreal.Vector(-cell, -cell, -slab - cell)
    high = unreal.Vector(extent_cells * cell + cell, extent_cells * cell + cell, height + slab + cell)
    centre = unreal.Vector((low.x + high.x) / 2.0, (low.y + high.y) / 2.0, (low.z + high.z) / 2.0)
    half = unreal.Vector((high.x - low.x) / 2.0, (high.y - low.y) / 2.0, (high.z - low.z) / 2.0)
    return centre, half


def ensure_raid_map(game_mode_bp):
    if assets.does_asset_exist(RAID_MAP):
        return
    if not levels.new_level(RAID_MAP):
        raise RuntimeError("Could not create " + RAID_MAP)
    centre, half = nav_bounds()
    volume = actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume, centre, unreal.Rotator(0.0, 0.0, 0.0))
    if volume is None:
        raise RuntimeError("Could not spawn the NavMeshBoundsVolume")
    volume.set_actor_label(NAV_BOUNDS_LABEL)
    _, brush_half = volume.get_actor_bounds(False)
    if min(brush_half.x, brush_half.y, brush_half.z) <= 0.0:
        raise RuntimeError("NavMeshBoundsVolume has no brush to scale")
    volume.set_actor_scale3d(unreal.Vector(half.x / brush_half.x, half.y / brush_half.y, half.z / brush_half.z))

    world_settings = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_world_settings()
    world_settings.set_editor_property("default_game_mode", game_mode_bp.generated_class())
    if not levels.save_current_level():
        raise RuntimeError("Could not save " + RAID_MAP)
    log("created L_Raid_Dev (nav bounds %.0f x %.0f x %.0f)" % (2 * half.x, 2 * half.y, 2 * half.z))


def fill_biome(marker_bp):
    biome = load(BIOME)
    changed = False

    plug = biome.get_editor_property("sealed_plug")
    if plug.get_editor_property("mesh") is None:
        plug.set_editor_property("mesh", load(CUBE_MESH))
        plug.set_editor_property("material", load(GREYBOX_MATERIAL))
        biome.set_editor_property("sealed_plug", plug)
        changed = True

    if len(biome.get_editor_property("socket_actors")) == 0:
        rules = []
        for socket, actor_class in [(PLAYER_SPAWN_SOCKET, unreal.PlayerStart.static_class())] + \
                [(name, marker_bp.generated_class()) for name in SOCKETS_TO_MARKER]:
            rule = unreal.HollowSocketActorRule()
            rule.set_editor_property("socket_type", make_tag("Hollow.Socket." + socket))
            rule.set_editor_property("actor_class", actor_class)
            rules.append(rule)
        biome.set_editor_property("socket_actors", rules)
        changed = True

    if len(biome.get_editor_property("connection_actors")) == 0:
        rules = []
        for kind in CONNECTION_KINDS:
            rule = unreal.HollowConnectionActorRule()
            rule.set_editor_property("kind", getattr(unreal.HollowConnectorKind, kind))
            rule.set_editor_property("actor_class", marker_bp.generated_class())
            rules.append(rule)
        biome.set_editor_property("connection_actors", rules)
        changed = True

    if changed:
        save(biome)
        log("filled assembly fields of DA_Biome_Docks")


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate():
    check(assets.does_asset_exist(TRANSITION_MAP) and assets.does_asset_exist(RAID_MAP), "maps exist")
    check(isinstance(unreal.get_default_object(load(RAID_GAME_MODE).generated_class()), unreal.HollowRaidGameMode), "raid game mode")
    check(isinstance(unreal.get_default_object(load(MARKER_BP).generated_class()), unreal.HollowSectorMarker), "marker")
    biome = load(BIOME)
    check(biome.get_editor_property("sealed_plug").get_editor_property("mesh") is not None, "biome sealed plug")
    # Later scripts put real actors in front of the markers (extraction zone, the Iron Pilgrim): count types, not rules.
    socket_types = set(r.get_editor_property("socket_type").export_text() for r in biome.get_editor_property("socket_actors"))
    check(len(socket_types) == 1 + len(SOCKETS_TO_MARKER), "biome socket rules")
    check(len(biome.get_editor_property("connection_actors")) == len(CONNECTION_KINDS), "biome connection rules")
    levels.load_level(RAID_MAP)
    volumes = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.NavMeshBoundsVolume)]
    check(len(volumes) == 1, "one NavMeshBoundsVolume in L_Raid_Dev")
    world_settings = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_world_settings()
    check(world_settings.get_editor_property("default_game_mode") == load(RAID_GAME_MODE).generated_class(), "raid map game mode")
    log("validation passed")


def main():
    marker_bp = ensure_marker()
    game_mode_bp = ensure_game_mode()
    fill_biome(marker_bp)
    ensure_transition_map()
    ensure_raid_map(game_mode_bp)
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_raid_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
