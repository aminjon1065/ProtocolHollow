"""Creates the S2-05 door navigation content (idempotent, ADR-0010, ADR-0004).

  /Game/Hollow/AI/Nav/NAV_Filter_Standard   doors: open/closed passable, locked/welded/barricaded excluded
  /Game/Hollow/AI/Nav/NAV_Filter_Heavy      every door passable, stronger doors cost more (the Iron Pilgrim breaks them)
                                            S3-01: shut bulkheads - excluded for Standard, dearest for Heavy (added if missing)
  DA_Door_Standard                          WeldedHighThreshold, if unset
  BP_Door                                   NavCut box and NavLink reach from UHollowLevelMetrics, if unset

Creates or fills only what is missing, then validates.
Run after create_door_assets.py:  Build/Scripts/run-editor-python.sh Build/Python/create_nav_assets.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

NAV_DIR = "/Game/Hollow/AI/Nav"
DOOR_DEFINITION = "/Game/Hollow/World/Kit/Doors/DA_Door_Standard"
DOOR_BP = "/Game/Hollow/World/Kit/Doors/BP_Door"

WELDED_HIGH_THRESHOLD = 0.5
# The link ends this far beyond the cut, so they sit on the navmesh of the widest agent (Heavy, r80) in either room.
LINK_MARGIN = 100.0

# Door areas -> (travel cost, excluded) per filter (Docs/Architecture/03 sec. 3.7). Cost multiplies the link length.
FILTERS = {
    "NAV_Filter_Standard": {
        "HollowNavArea_Doorway": (1.0, False),
        "HollowNavArea_DoorClosed": (1.5, False),
        "HollowNavArea_DoorLocked": (1.0, True),
        "HollowNavArea_DoorWeldedLow": (1.0, True),
        "HollowNavArea_DoorWeldedHigh": (1.0, True),
        "HollowNavArea_DoorBarricaded": (1.0, True),
        "HollowNavArea_BulkheadIntact": (1.0, True),
    },
    "NAV_Filter_Heavy": {
        "HollowNavArea_Doorway": (1.0, False),
        "HollowNavArea_DoorClosed": (1.5, False),
        "HollowNavArea_DoorLocked": (4.0, False),
        "HollowNavArea_DoorWeldedLow": (6.0, False),
        "HollowNavArea_DoorWeldedHigh": (10.0, False),
        "HollowNavArea_DoorBarricaded": (8.0, False),
        "HollowNavArea_BulkheadIntact": (12.0, False),
    },
}

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_nav_assets: " + message)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset " + path)
    return asset


def save(asset):
    if not assets.save_loaded_asset(asset, False):
        raise RuntimeError("Could not save " + asset.get_path_name())


def filter_entry(area_name, cost, excluded):
    entry = unreal.NavigationFilterArea()
    entry.set_editor_property("area_class", getattr(unreal, area_name).static_class())
    entry.set_editor_property("override_travel_cost", True)
    entry.set_editor_property("travel_cost_override", cost)
    entry.set_editor_property("is_excluded", excluded)
    return entry


def ensure_filter(name, areas):
    """Creates the filter, or adds the areas it does not cover yet (existing entries keep their tuning)."""
    path = NAV_DIR + "/" + name
    if assets.does_asset_exist(path):
        filter_bp = load(path)
        cdo = unreal.get_default_object(filter_bp.generated_class())
        entries = list(cdo.get_editor_property("areas"))
        covered = [e.get_editor_property("area_class") for e in entries]
        missing = [a for a in areas if getattr(unreal, a).static_class() not in covered]
        if not missing:
            return
        for area_name in missing:
            entries.append(filter_entry(area_name, *areas[area_name]))
        cdo.set_editor_property("areas", entries)
        save(filter_bp)
        log("%s: added %s" % (name, ", ".join(missing)))
        return
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.NavigationQueryFilter)
    package_path, asset_name = path.rsplit("/", 1)
    filter_bp = tools.create_asset(asset_name, package_path, unreal.Blueprint, factory)
    if filter_bp is None:
        raise RuntimeError("Could not create " + path)
    entries = [filter_entry(area_name, cost, excluded) for area_name, (cost, excluded) in areas.items()]
    unreal.get_default_object(filter_bp.generated_class()).set_editor_property("areas", entries)
    save(filter_bp)
    log("created " + name)


def ensure_door_nav():
    definition = load(DOOR_DEFINITION)
    if definition.get_editor_property("welded_high_threshold") <= 0.0:
        definition.set_editor_property("welded_high_threshold", WELDED_HIGH_THRESHOLD)
        save(definition)
        log("DA_Door_Standard: welded high threshold set")

    door_bp = load(DOOR_BP)
    door = unreal.get_default_object(door_bp.generated_class())
    if door.get_editor_property("nav_link_half_length") > 0.0:
        return
    metrics = unreal.get_default_object(unreal.HollowLevelMetrics)
    wall = float(metrics.get_editor_property("wall_thickness"))
    width = float(metrics.get_editor_property("door_width"))
    height = float(metrics.get_editor_property("door_height"))
    # Cut twice the double wall deep (both rooms' walls plus as much again), from below the floor to the lintel.
    cut_half_depth = 2.0 * wall
    cut = door.get_editor_property("nav_cut")
    cut.set_editor_property("box_extent", unreal.Vector(cut_half_depth, width / 2.0, height / 2.0 + wall))
    cut.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, height / 2.0))
    door.set_editor_property("nav_link_half_length", cut_half_depth + LINK_MARGIN)
    save(door_bp)
    log("BP_Door: nav cut and link set")


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate():
    for name, areas in FILTERS.items():
        entries = unreal.get_default_object(load(NAV_DIR + "/" + name).generated_class()).get_editor_property("areas")
        check(len(entries) == len(areas), name + " covers every door and bulkhead area")
    door = unreal.get_default_object(load(DOOR_BP).generated_class())
    check(door.get_editor_property("nav_link_half_length") > 0.0, "BP_Door nav link")
    check(load(DOOR_DEFINITION).get_editor_property("welded_high_threshold") > 0.0, "welded high threshold")
    log("validation passed")


def main():
    for name, areas in FILTERS.items():
        ensure_filter(name, areas)
    ensure_door_nav()
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_nav_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
