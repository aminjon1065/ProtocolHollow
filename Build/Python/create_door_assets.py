"""Creates the S2-04 door content (idempotent, ADR-0010).

  /Game/Hollow/World/Kit/Doors/DA_Door_Standard   UHollowDoorDefinition: weld/cut rates, threshold, leaf timing (data)
  /Game/Hollow/World/Kit/Doors/BP_Door            AHollowDoor: greybox leaf and doorway sized from UHollowLevelMetrics
  /HollowSector_Docks/Data/DA_Biome_Docks         connection rule Door -> BP_Door (replaces the S2-02 placeholder marker)

Creates or updates only what is missing, then validates.
Run after create_raid_assets.py and create_tool_assets.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_door_assets.py
"""
import os
import sys
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hollow_loc as loc   # LOC-01 (ADR-0048): player texts are string table entries

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

DOORS_DIR = "/Game/Hollow/World/Kit/Doors"
DEFINITION_PATH = DOORS_DIR + "/DA_Door_Standard"
DOOR_BP_PATH = DOORS_DIR + "/BP_Door"
BIOME_PATH = "/HollowSector_Docks/Data/DA_Biome_Docks"
MARKER_BP_PATH = "/Game/Hollow/World/Sector/BP_SectorMarker"

CUBE_MESH = "/Engine/BasicShapes/Cube.Cube"
CUBE_SIZE = 100.0                                     # BasicShapes cube edge, uu
LEAF_MATERIAL = "/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"
COLOR_PARAMETER = "Color"

# Door tuning (data). Tool work is 10 units/s (DA_Tool_*): welding seals in ~0.4 s and fills in 2.5 s; cutting a full
# weld takes 5 s, so a welded door buys real time.
DEFINITION = {
    "weld_per_work_unit": 0.04,
    "cut_per_work_unit": 0.02,
    "seal_threshold": 0.15,
    "weld_send_interval": 0.2,
    "transition_replay_window": 1.0,
    "open_duration": 1.2,
    "open_ease_exponent": 2.0,
    "passable_fraction": 0.3,
}
WELD_TOOL = "Hollow.Ability.Tool.Welder"
CUT_TOOL = "Hollow.Ability.Tool.PlasmaCutter"
PROMPT_OPEN = "Open"
PROMPT_CLOSE = "Close"
PROMPT_KEYS = (("prompt_open", "World.Door.Open", PROMPT_OPEN), ("prompt_close", "World.Door.Close", PROMPT_CLOSE))   # loc.WORLD (LOC-01)

# Greybox look (data).
LEAF_THICKNESS = 10.0
LEAF_UPDATE_HZ = 30.0
CLOSED_COLOR = unreal.LinearColor(0.35, 0.37, 0.4, 1.0)
WELDED_COLOR = unreal.LinearColor(1.0, 0.45, 0.1, 1.0)

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_door_assets: " + message)


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


def make_tag(name):
    tag = unreal.GameplayTag()
    if not tag.import_text('(TagName="%s")' % name):
        raise RuntimeError("Unknown gameplay tag " + name)
    return tag


def metrics():
    cdo = unreal.get_default_object(unreal.HollowLevelMetrics)
    return {name: float(cdo.get_editor_property(name)) for name in ("door_width", "door_height", "wall_thickness")}


def ensure_definition():
    if assets.does_asset_exist(DEFINITION_PATH):
        return
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.HollowDoorDefinition)
    definition = create(DEFINITION_PATH, unreal.HollowDoorDefinition, factory)
    for key, value in DEFINITION.items():
        definition.set_editor_property(key, value)
    definition.set_editor_property("weld_tool_tag", make_tag(WELD_TOOL))
    definition.set_editor_property("cut_tool_tag", make_tag(CUT_TOOL))
    definition.set_editor_property("initial_state", unreal.HollowDoorState.CLOSED)
    for prop, key, source in PROMPT_KEYS:
        definition.set_editor_property(prop, loc.text(loc.WORLD, key, source))
    save(definition)
    log("created DA_Door_Standard")


def ensure_door_blueprint():
    if assets.does_asset_exist(DOOR_BP_PATH):
        return
    m = metrics()
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.HollowDoor)
    door_bp = create(DOOR_BP_PATH, unreal.Blueprint, factory)
    door = unreal.get_default_object(door_bp.generated_class())

    # The actor stands on the floor at the centre of the opening, X across it (HollowSectorManifest::OpeningTransform).
    leaf = door.get_editor_property("leaf")
    leaf.set_editor_property("static_mesh", load(CUBE_MESH))
    leaf.set_material(0, load(LEAF_MATERIAL))
    leaf.set_editor_property("relative_scale3d", unreal.Vector(LEAF_THICKNESS / CUBE_SIZE, m["door_width"] / CUBE_SIZE, m["door_height"] / CUBE_SIZE))
    leaf.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, m["door_height"] / 2.0))

    doorway = door.get_editor_property("doorway")
    doorway.set_editor_property("box_extent", unreal.Vector(m["wall_thickness"], m["door_width"] / 2.0, m["door_height"] / 2.0))
    doorway.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, m["door_height"] / 2.0))

    door.get_editor_property("door_state").set_editor_property("definition", load(DEFINITION_PATH))
    door.set_editor_property("leaf_open_offset", unreal.Vector(0.0, 0.0, m["door_height"]))   # slides up into the lintel
    door.set_editor_property("leaf_update_rate_hz", LEAF_UPDATE_HZ)
    door.set_editor_property("weld_color_parameter", COLOR_PARAMETER)
    door.set_editor_property("closed_color", CLOSED_COLOR)
    door.set_editor_property("welded_color", WELDED_COLOR)
    save(door_bp)
    log("created BP_Door")


def ensure_biome_rule():
    biome = load(BIOME_PATH)
    door_class = load(DOOR_BP_PATH).generated_class()
    rules = list(biome.get_editor_property("connection_actors"))
    door_rules = [r for r in rules if r.get_editor_property("kind") == unreal.HollowConnectorKind.DOOR]
    if door_rules and door_rules[0].get_editor_property("actor_class") == door_class:
        return
    marker_class = load(MARKER_BP_PATH).generated_class()
    if door_rules and door_rules[0].get_editor_property("actor_class") not in (None, marker_class):
        log("Door rule points to a custom class; kept as is")
        return
    if door_rules:
        door_rules[0].set_editor_property("actor_class", door_class)
    else:
        rule = unreal.HollowConnectionActorRule()
        rule.set_editor_property("kind", unreal.HollowConnectorKind.DOOR)
        rule.set_editor_property("actor_class", door_class)
        rules.append(rule)
    biome.set_editor_property("connection_actors", rules)
    save(biome)
    log("DA_Biome_Docks: Door connections now spawn BP_Door")


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate():
    door = unreal.get_default_object(load(DOOR_BP_PATH).generated_class())
    check(door.get_editor_property("door_state").get_editor_property("definition") == load(DEFINITION_PATH), "BP_Door uses DA_Door_Standard")
    check(door.get_editor_property("leaf").get_editor_property("static_mesh") is not None, "BP_Door leaf mesh")
    rules = load(BIOME_PATH).get_editor_property("connection_actors")
    check(any(r.get_editor_property("kind") == unreal.HollowConnectorKind.DOOR for r in rules), "biome door rule")
    log("validation passed")


def localize_prompts():
    """LOC-01 (ADR-0048): the door's prompts from before the string tables point at their entries now."""
    definition = load(DEFINITION_PATH)
    changed = [prop for prop, key, source in PROMPT_KEYS if loc.set_text(definition, prop, loc.WORLD, key, source)]
    if changed:
        save(definition)
        log("DA_Door_Standard: %s into %s" % (", ".join(changed), loc.WORLD))


def main():
    ensure_definition()
    localize_prompts()
    loc.save()
    ensure_door_blueprint()
    ensure_biome_rule()
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_door_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
