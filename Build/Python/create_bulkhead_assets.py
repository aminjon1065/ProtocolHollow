"""Creates the S3-01 breachable bulkhead content (idempotent, ADR-0010, ADR-0014).

  /Game/Hollow/World/Kit/Bulkheads/GC_Bulkhead_Plate   geometry collection: one panel filling the opening, Voronoi pieces
  /Game/Hollow/World/Kit/Bulkheads/GC_Bulkhead_Half    geometry collection: half a panel (used twice by the Twin variant)
  /Game/Hollow/World/Kit/Bulkheads/PM_HollowDebris     physical material of live debris: sleep thresholds, friction
  /Game/Hollow/World/Kit/Bulkheads/DA_Bulkhead_Plate   UHollowBreachableDefinition: one zone
  /Game/Hollow/World/Kit/Bulkheads/DA_Bulkhead_Twin    UHollowBreachableDefinition: two zones, the passage needs both
  /Game/Hollow/World/Kit/Bulkheads/BP_Bulkhead_Plate   AHollowBreachableBulkhead with DA_Bulkhead_Plate
  /Game/Hollow/World/Kit/Bulkheads/BP_Bulkhead_Twin    AHollowBreachableBulkhead with DA_Bulkhead_Twin
  /HollowSector_Docks/Data/DA_Biome_Docks              connection rule Bulkhead -> both variants (replaces the marker)

Sizes come from UHollowLevelMetrics (door opening, wall thickness). Creates or fills only what is missing, then validates.
Run after create_door_assets.py:  Build/Scripts/run-editor-python.sh Build/Python/create_bulkhead_assets.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

KIT_DIR = "/Game/Hollow/World/Kit/Bulkheads"
GC_PLATE = KIT_DIR + "/GC_Bulkhead_Plate"
GC_HALF = KIT_DIR + "/GC_Bulkhead_Half"
DEBRIS_MATERIAL = KIT_DIR + "/PM_HollowDebris"
DA_PLATE = KIT_DIR + "/DA_Bulkhead_Plate"
DA_TWIN = KIT_DIR + "/DA_Bulkhead_Twin"
BP_PLATE = KIT_DIR + "/BP_Bulkhead_Plate"
BP_TWIN = KIT_DIR + "/BP_Bulkhead_Twin"
BIOME_PATH = "/HollowSector_Docks/Data/DA_Biome_Docks"
MARKER_BP_PATH = "/Game/Hollow/World/Sector/BP_SectorMarker"

CUBE_MESH = "/Engine/BasicShapes/Cube.Cube"
BASIC_MATERIAL = "/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"
COLOR_PARAMETER = "Color"

# Greybox geometry (data). The bulkhead fills the connection's door opening: a lintel on top, the zones below it.
FRAME_HEIGHT = 30.0
PANEL_THICKNESS_WALLS = 1.0          # panel thickness in wall thicknesses
PLATE_PIECES = 24                    # Voronoi cells of the full panel
HALF_PIECES = 14
PLATE_SEED = 3101                    # fixed: the pieces never change between runs
HALF_SEED = 3102
SLEEP_SECONDS = unreal.Vector2D(1.0, 2.0)     # resting debris starts to shrink after this long (min, max)
REMOVAL_SECONDS = unreal.Vector2D(1.0, 1.5)   # and is gone this long after (min, max)

# Tuning (data). Structural damage numbers meet the Iron Pilgrim in S3-04.
PLATE_HEALTH = 400.0
HALF_HEALTH = 200.0
FRESH_WINDOW = 1.5                   # Docs/Architecture/03 sec. 3.4: a break younger than this may play live
DEBRIS_SPEED = 300.0                 # cm/s along the blow
RUBBLE_HEIGHT = 25.0
RUBBLE_DEPTH = 60.0
BLOCKER_REMOVAL_DELAY = 0.5          # the nav link carries agents at once; the tile rebuild follows
DEBRIS_SLEEP = {"sleep_linear_velocity_threshold": 5.0, "sleep_angular_velocity_threshold": 0.1, "sleep_counter_threshold": 4}
DEBRIS_FRICTION = 0.8
DEBRIS_RESTITUTION = 0.1

# Nav (ADR-0004): the blocker spans the double wall twice over, the link ends beyond it on both navmeshes (Heavy r80).
BLOCKER_DEPTH_WALLS = 2.0
LINK_MARGIN = 100.0

# Greybox look (data): panels tint from intact to damaged; frame and rubble keep the basic shape material.
INTACT_COLOR = unreal.LinearColor(0.22, 0.24, 0.27, 1.0)
DAMAGED_COLOR = unreal.LinearColor(0.55, 0.18, 0.08, 1.0)

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_bulkhead_assets: " + message)


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


def metrics():
    cdo = unreal.get_default_object(unreal.HollowLevelMetrics)
    return {name: float(cdo.get_editor_property(name)) for name in ("door_width", "door_height", "wall_thickness")}


def layout():
    """Zone boxes in actor space (X across the opening, Y along the wall, Z up from the floor)."""
    m = metrics()
    thickness = m["wall_thickness"] * PANEL_THICKNESS_WALLS
    zone_height = m["door_height"] - FRAME_HEIGHT
    return {
        "thickness": thickness,
        "zone_height": zone_height,
        "width": m["door_width"],
        "height": m["door_height"],
        "wall": m["wall_thickness"],
        "plate": [((0.0, 0.0), m["door_width"])],
        "twin": [((0.0, -m["door_width"] / 4.0), m["door_width"] / 2.0), ((0.0, m["door_width"] / 4.0), m["door_width"] / 2.0)],
    }


def ensure_collection(path, size, pieces, seed):
    if assets.does_asset_exist(path):
        return
    collection = unreal.HollowGeometryCollectionAuthoring.create_fractured_box(path, load(CUBE_MESH), load(BASIC_MATERIAL), size, pieces, seed,
                                                                               SLEEP_SECONDS, REMOVAL_SECONDS)
    if collection is None:
        raise RuntimeError("Could not create " + path)
    save(collection)
    log("created %s (%d pieces)" % (path.rsplit("/", 1)[1], unreal.HollowGeometryCollectionAuthoring.get_piece_count(collection)))


def ensure_debris_material():
    if assets.does_asset_exist(DEBRIS_MATERIAL):
        return
    factory = unreal.PhysicalMaterialFactoryNew()
    material = create(DEBRIS_MATERIAL, unreal.PhysicalMaterial, factory)
    for key, value in DEBRIS_SLEEP.items():
        material.set_editor_property(key, value)
    material.set_editor_property("friction", DEBRIS_FRICTION)
    material.set_editor_property("restitution", DEBRIS_RESTITUTION)
    save(material)
    log("created PM_HollowDebris")


def make_zone(centre_y, width, health, collection, geometry):
    zone = unreal.HollowBreachZone()
    zone.set_editor_property("center", unreal.Vector(0.0, centre_y, geometry["zone_height"] / 2.0))
    zone.set_editor_property("half_size", unreal.Vector(geometry["thickness"] / 2.0, width / 2.0, geometry["zone_height"] / 2.0))
    zone.set_editor_property("max_health", health)
    zone.set_editor_property("required_for_passage", True)
    zone.set_editor_property("fracture", load(collection))
    return zone


def ensure_definition(path, zones):
    if assets.does_asset_exist(path):
        return
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.HollowBreachableDefinition)
    definition = create(path, unreal.HollowBreachableDefinition, factory)
    definition.set_editor_property("zones", zones)
    definition.set_editor_property("fresh_window_seconds", FRESH_WINDOW)
    definition.set_editor_property("debris_speed", DEBRIS_SPEED)
    definition.set_editor_property("debris_material", load(DEBRIS_MATERIAL))
    definition.set_editor_property("rubble_height", RUBBLE_HEIGHT)
    definition.set_editor_property("rubble_depth", RUBBLE_DEPTH)
    definition.set_editor_property("blocker_removal_delay", BLOCKER_REMOVAL_DELAY)
    save(definition)
    log("created " + path.rsplit("/", 1)[1])


def ensure_blueprint(path, definition_path, geometry):
    if assets.does_asset_exist(path):
        return
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.HollowBreachableBulkhead)
    blueprint = create(path, unreal.Blueprint, factory)
    bulkhead = unreal.get_default_object(blueprint.generated_class())
    cube = load(CUBE_MESH)
    material = load(BASIC_MATERIAL)
    cube_size = cube.get_bounds().box_extent.x * 2.0

    # The actor stands on the floor at the centre of the opening, X across it (HollowSectorManifest::OpeningTransform).
    frame = bulkhead.get_editor_property("frame")
    frame.set_editor_property("static_mesh", cube)
    frame.set_material(0, material)
    frame.set_editor_property("relative_scale3d", unreal.Vector(geometry["thickness"] / cube_size, geometry["width"] / cube_size, FRAME_HEIGHT / cube_size))
    frame.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, geometry["zone_height"] + FRAME_HEIGHT / 2.0))

    blocker = bulkhead.get_editor_property("nav_blocker")
    blocker_half_depth = BLOCKER_DEPTH_WALLS * geometry["wall"]
    blocker.set_editor_property("box_extent", unreal.Vector(blocker_half_depth, geometry["width"] / 2.0, geometry["height"] / 2.0 + geometry["wall"]))
    blocker.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, geometry["height"] / 2.0))
    bulkhead.set_editor_property("nav_link_half_length", blocker_half_depth + LINK_MARGIN)

    bulkhead.get_editor_property("integrity").set_editor_property("definition", load(definition_path))
    bulkhead.set_editor_property("panel_mesh", cube)
    bulkhead.set_editor_property("panel_material", material)
    bulkhead.set_editor_property("rubble_mesh", cube)
    bulkhead.set_editor_property("rubble_material", material)
    bulkhead.set_editor_property("damage_color_parameter", COLOR_PARAMETER)
    bulkhead.set_editor_property("intact_color", INTACT_COLOR)
    bulkhead.set_editor_property("damaged_color", DAMAGED_COLOR)
    save(blueprint)
    log("created " + path.rsplit("/", 1)[1])


def ensure_biome_rule():
    biome = load(BIOME_PATH)
    variants = [load(BP_PLATE).generated_class(), load(BP_TWIN).generated_class()]
    rules = list(biome.get_editor_property("connection_actors"))
    bulkhead_rules = [r for r in rules if r.get_editor_property("kind") == unreal.HollowConnectorKind.BULKHEAD]
    if bulkhead_rules and list(bulkhead_rules[0].get_editor_property("variants")) == variants:
        return
    marker_class = load(MARKER_BP_PATH).generated_class()
    if bulkhead_rules and bulkhead_rules[0].get_editor_property("actor_class") not in (None, marker_class, variants[0]):
        log("Bulkhead rule points to a custom class; kept as is")
        return
    rule = bulkhead_rules[0] if bulkhead_rules else unreal.HollowConnectionActorRule()
    rule.set_editor_property("kind", unreal.HollowConnectorKind.BULKHEAD)
    rule.set_editor_property("actor_class", variants[0])
    rule.set_editor_property("variants", variants)
    if not bulkhead_rules:
        rules.append(rule)
    biome.set_editor_property("connection_actors", rules)
    save(biome)
    log("DA_Biome_Docks: Bulkhead connections now spawn BP_Bulkhead_Plate / BP_Bulkhead_Twin")


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate(geometry):
    helper = unreal.HollowGeometryCollectionAuthoring
    for path, width, pieces in ((GC_PLATE, geometry["width"], PLATE_PIECES), (GC_HALF, geometry["width"] / 2.0, HALF_PIECES)):
        collection = load(path)
        count = helper.get_piece_count(collection)
        check(count >= pieces, "%s has %d pieces from %d cells" % (path, count, pieces))   # island splitting may add some
        size = helper.get_bounds_size(collection)
        expected = unreal.Vector(geometry["thickness"], width, geometry["zone_height"])
        check(abs(size.x - expected.x) < 1.0 and abs(size.y - expected.y) < 1.0 and abs(size.z - expected.z) < 1.0,
              "%s bounds %s match the zone %s" % (path, size, expected))
    for path, zone_count in ((DA_PLATE, 1), (DA_TWIN, 2)):
        zones = load(path).get_editor_property("zones")
        check(len(zones) == zone_count, path + " zone count")
    for path, definition in ((BP_PLATE, DA_PLATE), (BP_TWIN, DA_TWIN)):
        bulkhead = unreal.get_default_object(load(path).generated_class())
        check(bulkhead.get_editor_property("integrity").get_editor_property("definition") == load(definition), path + " uses " + definition)
        check(bulkhead.get_editor_property("nav_link_half_length") > 0.0, path + " nav link")
        check(bulkhead.get_editor_property("panel_mesh") is not None, path + " panel mesh")
    rules = load(BIOME_PATH).get_editor_property("connection_actors")
    check(any(r.get_editor_property("kind") == unreal.HollowConnectorKind.BULKHEAD and len(r.get_editor_property("variants")) == 2 for r in rules),
          "biome bulkhead rule with both variants")
    log("validation passed")


def main():
    geometry = layout()
    ensure_collection(GC_PLATE, unreal.Vector(geometry["thickness"], geometry["width"], geometry["zone_height"]), PLATE_PIECES, PLATE_SEED)
    ensure_collection(GC_HALF, unreal.Vector(geometry["thickness"], geometry["width"] / 2.0, geometry["zone_height"]), HALF_PIECES, HALF_SEED)
    ensure_debris_material()
    ensure_definition(DA_PLATE, [make_zone(y, width, PLATE_HEALTH, GC_PLATE, geometry) for (_, y), width in geometry["plate"]])
    ensure_definition(DA_TWIN, [make_zone(y, width, HALF_HEALTH, GC_HALF, geometry) for (_, y), width in geometry["twin"]])
    ensure_blueprint(BP_PLATE, DA_PLATE, geometry)
    ensure_blueprint(BP_TWIN, DA_TWIN, geometry)
    ensure_biome_rule()
    validate(geometry)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_bulkhead_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
