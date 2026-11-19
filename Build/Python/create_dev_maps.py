"""Creates the S1-03 development maps (idempotent, ADR-0010).

L_Boot     empty front-end map: startup map and destination after leaving a session.
L_Hub_Dev  temporary greybox hub: floor, sun, sky and four co-op player starts.
           Replaced by the real hub built from the greybox kit (S1-07).

Run: Build/Scripts/run-editor-python.sh Build/Python/create_dev_maps.py
"""
import traceback

import unreal

MAPS_DIR = "/Game/Hollow/Maps"
BOOT_MAP = MAPS_DIR + "/L_Boot"
HUB_MAP = MAPS_DIR + "/L_Hub_Dev"

FLOOR_MESH = "/Engine/BasicShapes/Plane.Plane"   # 100 x 100 uu
FLOOR_SCALE = unreal.Vector(40.0, 40.0, 1.0)      # 40 m x 40 m
SUN_LOCATION = unreal.Vector(0.0, 0.0, 500.0)
SUN_ROTATION = unreal.Rotator(roll=0.0, pitch=-50.0, yaw=30.0)
PLAYER_START_COUNT = 4                            # brief: 1-4 players
PLAYER_START_SPACING = 300.0                      # uu between spawn points
PLAYER_START_HEIGHT = 100.0                       # uu above the floor
SUCCESS_MARKER = "HOLLOW_PYTHON_OK"               # checked by run-editor-python.sh

assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def spawn(actor_class, label, location=unreal.Vector(0.0, 0.0, 0.0), rotation=unreal.Rotator(0.0, 0.0, 0.0)):
    actor = actors.spawn_actor_from_class(actor_class, location, rotation)
    if actor is None:
        raise RuntimeError("Could not spawn " + label)
    actor.set_actor_label(label)
    return actor


def populate_boot():
    # Intentionally empty: the front-end UI arrives with CommonUI (S2-06).
    pass


def populate_hub():
    floor = spawn(unreal.StaticMeshActor, "Floor")
    floor.static_mesh_component.set_static_mesh(unreal.load_asset(FLOOR_MESH))
    floor.set_actor_scale3d(FLOOR_SCALE)

    spawn(unreal.DirectionalLight, "Sun", SUN_LOCATION, SUN_ROTATION)
    sky = spawn(unreal.SkyLight, "SkyLight", SUN_LOCATION)
    sky.light_component.set_editor_property("real_time_capture", True)
    spawn(unreal.SkyAtmosphere, "SkyAtmosphere")

    first_offset = -(PLAYER_START_COUNT - 1) * PLAYER_START_SPACING / 2.0
    for index in range(PLAYER_START_COUNT):
        location = unreal.Vector(0.0, first_offset + index * PLAYER_START_SPACING, PLAYER_START_HEIGHT)
        spawn(unreal.PlayerStart, "PlayerStart_%d" % index, location)


def create_map(path, populate):
    if assets.does_asset_exist(path):
        unreal.log("create_dev_maps: %s already exists, skipped" % path)
        return
    if not levels.new_level(path):
        raise RuntimeError("new_level failed for " + path)
    populate()
    if not levels.save_current_level():
        raise RuntimeError("save_current_level failed for " + path)
    unreal.log("create_dev_maps: created " + path)


def main():
    create_map(BOOT_MAP, populate_boot)
    create_map(HUB_MAP, populate_hub)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_dev_maps failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
