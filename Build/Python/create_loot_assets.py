"""Creates the GP-01b loot crates (idempotent, ADR-0010, ADR-0045).

  /Game/Hollow/World/Loot/DA_LootTable_Docks, _Cathedral   UHollowLootTableDefinition: what a sector's crates hold
  /Game/Hollow/World/Loot/GC_LootCrate                     geometry collection: the crate in Voronoi pieces (cosmetic debris)
  /Game/Hollow/World/Loot/BP_LootPickup                    AHollowLootPickup: loot on the floor (the item's mesh, else this)
  /Game/Hollow/World/Loot/BP_LootCrate_Docks, _Cathedral   AHollowLootCrate: the hub's kit crate, its health, its table
  DA_Biome_Docks / DA_Biome_Cathedral                      their Loot sockets spawn the sector's crate (first rule of the type)

The pickups' meshes are create_art_kit.py's (UHollowItemDefinition::PickupMesh), the break's noise rule
create_noise_assets.py's, its sounds create_audio_assets.py's. Every asset is created when missing and refreshed only
when it differs from the spec below, then validated.
Run after create_weapon_assets.py and create_cathedral_greybox.py, before create_art_kit.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_loot_assets.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

LOOT_DIR = "/Game/Hollow/World/Loot"
GC_CRATE = LOOT_DIR + "/GC_LootCrate"
PICKUP_BP = LOOT_DIR + "/BP_LootPickup"
CRATE_MESH = "/Game/Hollow/Art/Kit/SM_Hub_Crate"                # the art kit's crate (0.6 x 0.84 x 0.6 m, pivot on its floor)
CRATE_MATERIAL = "/Game/Hollow/Art/Materials/MI_Kit_DarkAlloy"
CUBE_MESH = "/Engine/BasicShapes/Cube.Cube"
BIOMES = {"Docks": "/HollowSector_Docks/Data/DA_Biome_Docks", "Cathedral": "/HollowSector_Cathedral/Data/DA_Biome_Cathedral"}
LOOT_SOCKET = "Hollow.Socket.Loot"

# The crate (data; balance later): health against the nailgun (20 a round: 3), a blow (25: 3), the shotgun (one blast),
# the plasma cutter (10 work/s x 4: 1.5 s).
CRATE_SIZE = (60.0, 84.0, 60.0)    # cm, the kit mesh's
CRATE = {"max_health": 60.0, "cut_damage_per_work": 4.0, "spill_speed": 350.0, "debris_speed": 250.0, "fresh_window_seconds": 1.5}
CUTTER_TAG = "Hollow.Ability.Tool.PlasmaCutter"
BREAK_NOISE = "Hollow.Noise.Impact.Crate"
PICKUP_EXTENT = (8.0, 6.0, 4.0)   # cm: a hand's size, the pickup meshes' (Build/Blender/hollow_combat_kit.py)
GC = {"pieces": 12, "seed": 4711, "sleep": (2.0, 3.0), "removal": (4.0, 6.0)}
# What a sector's crates hold: item -> (weight, min, max); drops per crate.
TABLES = {
    "Docks": {"drops": (1, 2), "rows": [("DA_Item_Scrap", 40, 2, 5), ("DA_Item_Ammo_Nails", 25, 8, 16), ("DA_Item_AetherShard", 13, 1, 2),
                                         ("DA_Item_FuelCell", 12, 1, 1), ("DA_Item_Ammo_Shells", 10, 2, 4)]},
    "Cathedral": {"drops": (1, 2), "rows": [("DA_Item_AetherShard", 30, 1, 3), ("DA_Item_Scrap", 25, 2, 4), ("DA_Item_Ammo_Nails", 20, 8, 14),
                                             ("DA_Item_Ammo_Shells", 15, 2, 4), ("DA_Item_FuelCell", 10, 1, 1)]},
}
ITEM_DIRS = {"DA_Item_Scrap": "Materials", "DA_Item_AetherShard": "Materials", "DA_Item_FuelCell": "Consumables", "DA_Item_Ammo_Nails": "Ammo",
             "DA_Item_Ammo_Shells": "Ammo"}

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_loot_assets: " + message)


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


def blueprint(path, parent_class):
    if assets.does_asset_exist(path):
        return load(path), False
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return create(path, unreal.Blueprint, factory), True


def data_asset(path, asset_class):
    if assets.does_asset_exist(path):
        return load(path), False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    return create(path, asset_class, factory), True


def from_text(struct, text):
    if not struct.import_text(text):
        raise RuntimeError("Could not import " + text)
    return struct


def make_tag(name):
    return from_text(unreal.GameplayTag(), '(TagName="%s")' % name)


def item_id(name):
    return from_text(unreal.PrimaryAssetId(), "HollowItem:" + name)


def text_of(value):
    """A comparable form of a property value (structs and arrays by their text, objects by their path)."""
    if isinstance(value, (list, tuple, unreal.Array)):
        return [text_of(v) for v in value]
    if hasattr(value, "export_text"):
        return value.export_text()
    if hasattr(value, "get_path_name"):
        return value.get_path_name()
    if isinstance(value, float):
        return round(value, 4)
    return value


def refresh(obj, wanted):
    """Sets every wanted property that differs; True if any did."""
    changed = False
    for key, value in wanted.items():
        if text_of(obj.get_editor_property(key)) != text_of(value):
            obj.set_editor_property(key, value)
            changed = True
    return changed


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def vector(values):
    return unreal.Vector(*values)


# ---------------------------------------------------------------- assets

def ensure_table(sector, spec):
    table, created = data_asset("%s/DA_LootTable_%s" % (LOOT_DIR, sector), unreal.HollowLootTableDefinition)
    rows = []
    for name, weight, low, high in spec["rows"]:
        row = unreal.HollowLootRow()
        row.set_editor_property("item", item_id(name))
        row.set_editor_property("weight", weight)
        row.set_editor_property("min_count", low)
        row.set_editor_property("max_count", high)
        rows.append(row)
    wanted = {"rows": rows, "min_drops": spec["drops"][0], "max_drops": spec["drops"][1]}
    if refresh(table, wanted) or created:
        save(table)
        log("DA_LootTable_%s: %d rows, %d..%d drops a crate" % (sector, len(rows), spec["drops"][0], spec["drops"][1]))
    return table


def ensure_fracture():
    if assets.does_asset_exist(GC_CRATE):
        return load(GC_CRATE)
    collection = unreal.HollowGeometryCollectionAuthoring.create_fractured_box(GC_CRATE, load(CUBE_MESH), load(CRATE_MATERIAL), vector(CRATE_SIZE),
                                                                               GC["pieces"], GC["seed"], unreal.Vector2D(*GC["sleep"]),
                                                                               unreal.Vector2D(*GC["removal"]))
    if collection is None:
        raise RuntimeError("Could not create " + GC_CRATE)
    save(collection)
    log("created GC_LootCrate (%d pieces)" % unreal.HollowGeometryCollectionAuthoring.get_piece_count(collection))
    return collection


def ensure_pickup():
    pickup_bp, created = blueprint(PICKUP_BP, unreal.HollowLootPickup)
    pickup = unreal.get_default_object(pickup_bp.generated_class())
    mesh = pickup.get_editor_property("mesh")
    body = pickup.get_editor_property("body")
    changed = created
    extent = unreal.Vector(*PICKUP_EXTENT)
    if (body.get_editor_property("box_extent") - extent).length() > 1e-3:
        body.set_editor_property("box_extent", extent)   # the body; the kit's pickup meshes are looks only
        changed = True
    lift = unreal.Vector(0.0, 0.0, -PICKUP_EXTENT[2])   # the pickup meshes' pivot is on their floor: the box's bottom
    if mesh.get_editor_property("static_mesh") != load(CUBE_MESH) or (mesh.get_editor_property("relative_scale3d") - unreal.Vector(0.1, 0.1, 0.06)).length() > 1e-4 \
            or (mesh.get_editor_property("relative_location") - lift).length() > 1e-3:
        mesh.set_editor_property("static_mesh", load(CUBE_MESH))   # an item without its own pickup mesh: a small box
        mesh.set_editor_property("relative_scale3d", unreal.Vector(0.1, 0.1, 0.06))
        mesh.set_editor_property("relative_location", lift)
        changed = True
    if changed:
        save(pickup_bp)
        log("BP_LootPickup ready")
    return pickup_bp.generated_class()


def ensure_crate(sector, table, fracture, pickup_class):
    crate_bp, created = blueprint("%s/BP_LootCrate_%s" % (LOOT_DIR, sector), unreal.HollowLootCrate)
    crate = unreal.get_default_object(crate_bp.generated_class())
    wanted = dict(CRATE, cutter_tag=make_tag(CUTTER_TAG), break_noise_tag=make_tag(BREAK_NOISE), loot_table=table, pickup_class=pickup_class,
                  fracture=fracture)
    changed = refresh(crate, wanted) or created
    half = unreal.Vector(CRATE_SIZE[0] / 2.0, CRATE_SIZE[1] / 2.0, CRATE_SIZE[2] / 2.0)
    bounds = crate.get_editor_property("bounds")
    if (bounds.get_editor_property("box_extent") - half).length() > 1e-3:
        bounds.set_editor_property("box_extent", half)
        changed = True
    body = crate.get_editor_property("body")
    mesh = load(CRATE_MESH) if assets.does_asset_exist(CRATE_MESH) else load(CUBE_MESH)
    lift = unreal.Vector(0.0, 0.0, -half.z)   # the kit crate's pivot is on its floor: the box's bottom
    if body.get_editor_property("static_mesh") != mesh or (body.get_editor_property("relative_location") - lift).length() > 1e-3:
        body.set_editor_property("static_mesh", mesh)
        body.set_editor_property("relative_location", lift)
        changed = True
    if changed:
        save(crate_bp)
        log("BP_LootCrate_%s: %.0f health, DA_LootTable_%s" % (sector, CRATE["max_health"], sector))
    return crate_bp.generated_class()


def ensure_biome_rule(sector, crate_class):
    biome = load(BIOMES[sector])
    rules = list(biome.get_editor_property("socket_actors"))
    loot = make_tag(LOOT_SOCKET).export_text()
    offset = unreal.Vector(0.0, 0.0, CRATE_SIZE[2] / 2.0)   # the box's middle half its height above the floor socket
    for rule in rules:
        if rule.get_editor_property("socket_type").export_text() == loot:
            if rule.get_editor_property("actor_class") == crate_class:
                if (rule.get_editor_property("spawn_offset") - offset).length() < 1e-3 and rule.get_editor_property("max_per_sector") == 0:
                    return
                rule.set_editor_property("spawn_offset", offset)
                rule.set_editor_property("max_per_sector", 0)
                biome.set_editor_property("socket_actors", rules)
                save(biome)
                log("DA_Biome_%s: the crates' offset" % sector)
                return
            break   # the first Loot rule is another's: ours goes in front of it
    rule = unreal.HollowSocketActorRule()
    rule.set_editor_property("socket_type", make_tag(LOOT_SOCKET))
    rule.set_editor_property("actor_class", crate_class)
    rule.set_editor_property("spawn_offset", offset)
    rule.set_editor_property("max_per_sector", 0)   # every Loot socket: the markers behind it get none
    index = next((i for i, r in enumerate(rules) if r.get_editor_property("socket_type").export_text() == loot), len(rules))
    rules.insert(index, rule)
    biome.set_editor_property("socket_actors", rules)
    save(biome)
    log("DA_Biome_%s: Loot sockets spawn BP_LootCrate_%s" % (sector, sector))


# ---------------------------------------------------------------- validation

def validate():
    for sector, spec in TABLES.items():
        table = load("%s/DA_LootTable_%s" % (LOOT_DIR, sector))
        check(len(table.get_editor_property("rows")) == len(spec["rows"]), sector + " table rows")
        for name, _, _, _ in spec["rows"]:
            check(assets.does_asset_exist("/Game/Hollow/Items/%s/%s" % (ITEM_DIRS[name], name)), "%s rolls a real item: %s" % (sector, name))
        crate_class = load("%s/BP_LootCrate_%s" % (LOOT_DIR, sector)).generated_class()
        crate = unreal.get_default_object(crate_class)
        check(crate.get_editor_property("loot_table") == table, sector + " crate holds its table")
        check(crate.get_editor_property("pickup_class") == load(PICKUP_BP).generated_class(), sector + " crate spills BP_LootPickup")
        check(crate.get_editor_property("fracture") is not None, sector + " crate breaks into GC_LootCrate")
        first = [r for r in load(BIOMES[sector]).get_editor_property("socket_actors")
                 if r.get_editor_property("socket_type").export_text() == make_tag(LOOT_SOCKET).export_text()]
        check(first and first[0].get_editor_property("actor_class") == crate_class, "DA_Biome_%s: the first Loot rule spawns the crate" % sector)
    log("validation passed")


def main():
    fracture = ensure_fracture()
    pickup_class = ensure_pickup()
    for sector, spec in TABLES.items():
        table = ensure_table(sector, spec)
        crate_class = ensure_crate(sector, table, fracture, pickup_class)
        ensure_biome_rule(sector, crate_class)
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_loot_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
