"""Creates the DRONE-01 solo drone content (idempotent, ADR-0010, ADR-0054).

  /Game/Hollow/Drone/DA_SoloDrone     UHollowDroneDefinition: where it hovers beside its player, how softly it follows, where
                                      the core hangs under it, its two prompts (ST_World: World.Drone.HandOver / TakeBack)
  /Game/Hollow/Drone/BP_SoloDrone     AHollowSoloDrone: the Fab drone mesh (SM_LegacyDrone, FAB-01 pipeline) x2.5, no
                                      collision, no shadow; a soft unshadowed spotlight ahead

The mesh comes from Content/Fab/AdvancedCompanionDroneSystem (fab_manifest.json, prepare_fab_assets.py): only the static mesh
of the Legacy drone with its two materials and four textures (1024). DefaultGame.ini names BP_SoloDrone as the raid's
SoloDroneClass. Its hum (MS_Drone_Hum, Hollow.Sound.Drone.Hum) is create_audio_assets.py's; the drone carry effect and pace
(GE_Carry_Drone, DA_MovementTuning_ExoFrame) are create_carry_assets.py's.
Creates or fixes only what differs, then validates.
Run after create_carry_assets.py and create_audio_assets.py:  Build/Scripts/run-editor-python.sh Build/Python/create_drone_assets.py
"""
import os
import sys
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hollow_loc as loc   # LOC-01 (ADR-0048): player texts are string table entries

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

DRONE_DIR = "/Game/Hollow/Drone"
DEFINITION = DRONE_DIR + "/DA_SoloDrone"
DRONE_BP = DRONE_DIR + "/BP_SoloDrone"
MESH = "/Game/Fab/AdvancedCompanionDroneSystem/Mesh/Drone/Legacy/SM_LegacyDrone"
CORE_BP = "/Game/Hollow/Items/AetherCore/BP_AetherCore"   # create_carry_assets.py

# Look (data). The Fab mesh is a 38 cm drone: x2.5 reads at arm's length beside the view (~95 cm).
MESH_SCALE = 2.5
LIGHT = {"intensity": 500.0, "inner_cone_angle": 12.0, "outer_cone_angle": 38.0, "attenuation_radius": 900.0, "cast_shadows": False,
         "source_radius": 8.0}   # soft: a wide penumbra round a small hot spot; never a shadow (the light budget)
LIGHT_COLOR = unreal.Color(r=205, g=232, b=255, a=255)   # a cool white beside the warm helmet lamp
LIGHT_PITCH = -12.0   # degrees: ahead and a little down, where the player walks

# Follow (data, UHollowDroneDefinition): right-front of the view at about eye level, out of the line of fire.
FOLLOW_OFFSET = unreal.Vector(120.0, 90.0, 55.0)   # cm from the capsule centre: forward, right, up
FOLLOW_LAG_SPEED = 5.0                             # 1/s: catches up in a few tenths of a second
TURN_LAG_SPEED = 6.0                               # 1/s
MAX_LAG_DISTANCE = 150.0                           # cm
HEADING_DEAD_ZONE = 50.0                           # degrees: wider than the hover's 37 degrees, so the player can face it
HEADING_RATE_HZ = 10.0                             # Hz: how often it looks at the player's view
PROBE_RADIUS = 14.0                                # cm: keeps the hull out of walls
FOCUS_RADIUS = 30.0                                # cm: what the player looks at to hand over or take back
CORE_CLEARANCE = 12.0                              # cm between the drone's belly and the top of the core it carries

PROMPTS = (("hand_over_prompt", "World.Drone.HandOver", "Hand the core to the drone"),
           ("take_back_prompt", "World.Drone.TakeBack", "Take the core back"))   # loc.WORLD

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_drone_assets: " + message)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset " + path)
    return asset


def save(asset):
    if not assets.save_loaded_asset(asset, False):
        raise RuntimeError("Could not save " + asset.get_path_name())


def create(path, asset_class, factory):
    package_path, name = path.rsplit("/", 1)
    asset = tools.create_asset(name, package_path, asset_class, factory)
    if asset is None:
        raise RuntimeError("Could not create " + path)
    return asset


def same(a, b):
    return a.export_text() == b.export_text() if hasattr(a, "export_text") else a == b


def apply_values(obj, values):
    """Sets every property that differs; True if any did."""
    changed = False
    for name, value in values.items():
        current = obj.get_editor_property(name)
        if isinstance(value, float) and isinstance(current, float):
            if abs(current - value) < 1e-4:
                continue
        elif same(current, value):
            continue
        obj.set_editor_property(name, value)
        changed = True
    return changed


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


# ---------------------------------------------------------------- definition

def carry_point():
    """Under the drone: its belly, a clearance, then the core's half height (the core's body hangs from its centre)."""
    core = unreal.get_default_object(load(CORE_BP).generated_class())
    core_half = core.get_editor_property("body").get_editor_property("box_extent")
    box = load(MESH).get_bounding_box()
    belly = -box.min.z * MESH_SCALE
    return unreal.Vector(0.0, 0.0, -(belly + CORE_CLEARANCE + core_half.z))


def ensure_definition():
    if assets.does_asset_exist(DEFINITION):
        definition = load(DEFINITION)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.HollowDroneDefinition)
        definition = create(DEFINITION, unreal.HollowDroneDefinition, factory)
        log("created DA_SoloDrone")
    changed = apply_values(definition, {
        "follow_offset": FOLLOW_OFFSET, "follow_lag_speed": FOLLOW_LAG_SPEED, "turn_lag_speed": TURN_LAG_SPEED,
        "max_lag_distance": MAX_LAG_DISTANCE, "heading_dead_zone": HEADING_DEAD_ZONE, "heading_rate_hz": HEADING_RATE_HZ,
        "probe_radius": PROBE_RADIUS, "focus_radius": FOCUS_RADIUS,
        "carry_point_offset": carry_point(),
    })
    for prop, key, source in PROMPTS:
        changed |= loc.set_text(definition, prop, loc.WORLD, key, source)
    if changed:
        save(definition)
        log("DA_SoloDrone: follow, carry point and prompts set")
    return definition


# ---------------------------------------------------------------- blueprint

def ensure_blueprint(definition):
    if assets.does_asset_exist(DRONE_BP):
        drone_bp = load(DRONE_BP)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.HollowSoloDrone)
        drone_bp = create(DRONE_BP, unreal.Blueprint, factory)
        log("created BP_SoloDrone")
    drone = unreal.get_default_object(drone_bp.generated_class())
    changed = apply_values(drone, {"definition": definition})
    mesh = drone.get_editor_property("mesh")
    changed |= apply_values(mesh, {"static_mesh": load(MESH), "relative_scale3d": unreal.Vector(MESH_SCALE, MESH_SCALE, MESH_SCALE),
                                   "cast_shadow": False})
    light = drone.get_editor_property("light")
    changed |= apply_values(light, dict(LIGHT, relative_rotation=unreal.Rotator(roll=0.0, pitch=LIGHT_PITCH, yaw=0.0)))
    if not same(light.get_editor_property("light_color"), LIGHT_COLOR):
        light.set_editor_property("light_color", LIGHT_COLOR)
        changed = True
    if changed:
        save(drone_bp)
        log("BP_SoloDrone: mesh, light and definition set")
    return drone_bp


# ---------------------------------------------------------------- validation

def validate(definition, drone_bp):
    drone = unreal.get_default_object(drone_bp.generated_class())
    check(drone.get_editor_property("definition") == definition, "BP_SoloDrone uses DA_SoloDrone")
    mesh = drone.get_editor_property("mesh")
    check(mesh.get_editor_property("static_mesh") == load(MESH), "BP_SoloDrone shows the Fab drone")
    check(not mesh.get_editor_property("cast_shadow"), "the drone casts no shadow")
    light = drone.get_editor_property("light")
    check(not light.get_editor_property("cast_shadows"), "the drone's light casts no shadow")
    check(light.get_editor_property("outer_cone_angle") > light.get_editor_property("inner_cone_angle") > 0.0, "a soft cone")
    offset = definition.get_editor_property("follow_offset")
    check(offset.x > 0.0 and offset.y > 0.0, "the drone hovers right-front of the view")
    check(definition.get_editor_property("heading_dead_zone") > unreal.MathLibrary.radians_to_degrees(unreal.MathLibrary.atan2(offset.y, offset.x)),
          "the player can turn to face the drone without chasing it away")
    check(definition.get_editor_property("carry_point_offset").z < 0.0, "the core hangs under the drone")
    for prop, key, _ in PROMPTS:
        check(loc.is_text(definition.get_editor_property(prop), loc.WORLD, key), "%s is the string table's %s" % (prop, key))
    log("validation passed")


def main():
    definition = ensure_definition()
    drone_bp = ensure_blueprint(definition)
    loc.save()
    validate(definition, drone_bp)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_drone_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
