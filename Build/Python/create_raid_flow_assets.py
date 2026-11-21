"""Creates the S3-03 raid flow content (idempotent, ADR-0010, ADR-0016).

  /Game/Hollow/Items/Tools/DA_Item_Welder|DA_Item_PlasmaCutter   UHollowItemDefinition: insurable tools
  /Game/Hollow/Items/Consumables/DA_Item_FuelCell|DA_Item_PulseMine  consumables, not insurable (what they do in a raid:
                                                                 create_inventory_assets.py)
  /Game/Hollow/Items/Materials/DA_Item_Scrap|DA_Item_AetherShard  workbench materials (S5-02), never taken into a raid
  /Game/Hollow/Items/AetherCore/DA_Item_AetherCore               the raid's loot, sold on extraction
  /Game/Hollow/GAS/Effects/GE_Damage_Generic                     instant IncomingDamage (SetByCaller Hollow.Data.Damage)
  /Game/Hollow/GAS/Effects/GE_State_Dead                         UHollowDeadEffect (Hollow.State.Dead)
  /Game/Hollow/World/Raid/MI_ExtractionPad, BP_ExtractionZone    AHollowExtractionZone: volume + greybox pad
  DA_Carryable_AetherCore LootItem, BP_ExoFrame DeathEffect, DA_Biome_Docks socket rule Extraction -> BP_ExtractionZone

Creates or fills only what is missing, then validates.
Run after create_carry_assets.py:  Build/Scripts/run-editor-python.sh Build/Python/create_raid_flow_assets.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

ITEMS_DIR = "/Game/Hollow/Items"
EFFECTS_DIR = "/Game/Hollow/GAS/Effects"
DAMAGE_EFFECT = EFFECTS_DIR + "/GE_Damage_Generic"
DEAD_EFFECT = EFFECTS_DIR + "/GE_State_Dead"
RAID_DIR = "/Game/Hollow/World/Raid"
PAD_LOOK = RAID_DIR + "/MI_ExtractionPad"
ZONE_BP = RAID_DIR + "/BP_ExtractionZone"
CORE_DEFINITION = "/Game/Hollow/Items/AetherCore/DA_Carryable_AetherCore"
EXOFRAME_BP = "/Game/Hollow/Characters/ExoFrame/BP_ExoFrame"
BIOME_PATH = "/HollowSector_Docks/Data/DA_Biome_Docks"
MARKER_BP_PATH = "/Game/Hollow/World/Sector/BP_SectorMarker"
EXTRACTION_SOCKET = "Hollow.Socket.Extraction"

ATTRIBUTE_MODULE = "/Script/HollowGAS"
CUBE_MESH = "/Engine/BasicShapes/Cube.Cube"
CUBE_SIZE = 100.0
BASIC_MATERIAL = "/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"
COLOR_PARAMETER = "Color"

# Items (data): path, display name, type, value in credits, insurable, max stack.
ITEMS = [
    ("Tools/DA_Item_Welder", "Welder", "Hollow.Item.Type.Tool", 300, True, 1),
    ("Tools/DA_Item_PlasmaCutter", "Plasma cutter", "Hollow.Item.Type.Tool", 400, True, 1),
    ("Consumables/DA_Item_FuelCell", "Fuel cell", "Hollow.Item.Type.Consumable", 25, False, 10),
    ("Consumables/DA_Item_PulseMine", "Pulse mine", "Hollow.Item.Type.Consumable", 40, False, 3),
    ("Materials/DA_Item_Scrap", "Scrap", "Hollow.Item.Type.Material", 8, False, 50),
    ("Materials/DA_Item_AetherShard", "Aether shard", "Hollow.Item.Type.Material", 20, False, 20),
    ("AetherCore/DA_Item_AetherCore", "Aether core", "Hollow.Item.Type.AetherCore", 500, False, 1),
]
CORE_ITEM = "AetherCore/DA_Item_AetherCore"

# Extraction zone (data): a 6 x 6 m pad in the extraction room, 3.5 m high, its pivot in the middle of the volume.
ZONE_HALF = unreal.Vector(300.0, 300.0, 175.0)
PAD_THICKNESS = 2.0
PAD_COLOR = unreal.LinearColor(0.15, 0.8, 0.35, 1.0)

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_raid_flow_assets: " + message)


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


def from_text(struct, text):
    if not struct.import_text(text):
        raise RuntimeError("Could not import " + text)
    return struct


def make_tag(name):
    return from_text(unreal.GameplayTag(), '(TagName="%s")' % name)


def item_id(relative_path):
    return from_text(unreal.PrimaryAssetId(), "HollowItem:" + relative_path.rsplit("/", 1)[1])


# ---------------------------------------------------------------- items and effects

def ensure_items():
    for relative, name, item_type, value, insurable, max_stack in ITEMS:
        path = ITEMS_DIR + "/" + relative
        if assets.does_asset_exist(path):
            continue
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.HollowItemDefinition)
        item = create(path, unreal.HollowItemDefinition, factory)
        item.set_editor_property("display_name", unreal.Text(name))
        item.set_editor_property("item_type", make_tag(item_type))
        item.set_editor_property("base_value", value)
        item.set_editor_property("insurable", insurable)
        item.set_editor_property("max_stack", max_stack)
        save(item)
        log("created " + relative.rsplit("/", 1)[1])


def ensure_damage_effect():
    effect_bp, created = blueprint(DAMAGE_EFFECT, unreal.GameplayEffect)
    if not created:
        return
    effect = unreal.get_default_object(effect_bp.generated_class())
    effect.set_editor_property("duration_policy", unreal.GameplayEffectDurationType.INSTANT)
    path = "%s.HollowHealthSet:IncomingDamage" % ATTRIBUTE_MODULE
    text = ('(Attribute=(AttributeName="IncomingDamage",Attribute={path},AttributeOwner="/Script/CoreUObject.Class\'{module}.HollowHealthSet\'"),'
            'ModifierOp=Additive,ModifierMagnitude=(MagnitudeCalculationType=SetByCaller,SetByCallerMagnitude=(DataTag=(TagName="Hollow.Data.Damage"))))'
            ).format(path=path, module=ATTRIBUTE_MODULE)
    modifier = from_text(unreal.GameplayModifierInfo(), text)
    if path not in modifier.export_text():
        raise RuntimeError("IncomingDamage modifier did not import")
    effect.set_editor_property("modifiers", [modifier])
    save(effect_bp)
    log("created GE_Damage_Generic")


def ensure_dead_effect():
    effect_bp, created = blueprint(DEAD_EFFECT, unreal.HollowDeadEffect)
    if created:
        save(effect_bp)
        log("created GE_State_Dead")
    return effect_bp.generated_class()


def ensure_death_on_exoframe(dead_effect):
    exoframe_bp = load(EXOFRAME_BP)
    character = unreal.get_default_object(exoframe_bp.generated_class())
    if character.get_editor_property("death_effect") == dead_effect:
        return
    character.set_editor_property("death_effect", dead_effect)
    save(exoframe_bp)
    log("BP_ExoFrame: death effect set")


def ensure_core_loot():
    definition = load(CORE_DEFINITION)
    wanted = item_id(CORE_ITEM)
    if definition.get_editor_property("loot_item").export_text() == wanted.export_text():
        return
    definition.set_editor_property("loot_item", wanted)
    save(definition)
    log("DA_Carryable_AetherCore: counts as DA_Item_AetherCore when extracted")


# ---------------------------------------------------------------- extraction zone

def ensure_pad_look():
    if assets.does_asset_exist(PAD_LOOK):
        return
    instance = create(PAD_LOOK, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, load(BASIC_MATERIAL))
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance, COLOR_PARAMETER, PAD_COLOR)
    save(instance)
    log("created MI_ExtractionPad")


def ensure_zone():
    zone_bp, created = blueprint(ZONE_BP, unreal.HollowExtractionZone)
    if not created:
        return
    zone = unreal.get_default_object(zone_bp.generated_class())
    zone.get_editor_property("volume").set_editor_property("box_extent", ZONE_HALF)
    pad = zone.get_editor_property("pad")
    pad.set_editor_property("static_mesh", load(CUBE_MESH))
    pad.set_material(0, load(PAD_LOOK))
    pad.set_editor_property("relative_scale3d", unreal.Vector(ZONE_HALF.x * 2.0 / CUBE_SIZE, ZONE_HALF.y * 2.0 / CUBE_SIZE, PAD_THICKNESS / CUBE_SIZE))
    pad.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, -ZONE_HALF.z + PAD_THICKNESS / 2.0))
    save(zone_bp)
    log("created BP_ExtractionZone")


def ensure_biome_rule():
    biome = load(BIOME_PATH)
    zone_class = load(ZONE_BP).generated_class()
    offset = unreal.Vector(0.0, 0.0, ZONE_HALF.z)   # the volume stands on the floor socket
    tag = make_tag(EXTRACTION_SOCKET)
    rules = list(biome.get_editor_property("socket_actors"))
    matches = [r for r in rules if r.get_editor_property("socket_type").export_text() == tag.export_text()]
    rule = matches[0] if matches else unreal.HollowSocketActorRule()
    current = rule.get_editor_property("actor_class")
    if current == zone_class and rule.get_editor_property("spawn_offset").export_text() == offset.export_text():
        return
    if matches and current not in (None, load(MARKER_BP_PATH).generated_class(), zone_class):
        log("Extraction rule points to a custom class; kept as is")
        return
    rule.set_editor_property("socket_type", tag)
    rule.set_editor_property("actor_class", zone_class)
    rule.set_editor_property("spawn_offset", offset)
    if not matches:
        rules.append(rule)
    biome.set_editor_property("socket_actors", rules)
    save(biome)
    log("DA_Biome_Docks: Extraction sockets now spawn BP_ExtractionZone")


# ---------------------------------------------------------------- validation

def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate(dead_effect):
    for relative, name, item_type, value, insurable, _ in ITEMS:
        item = load(ITEMS_DIR + "/" + relative)
        check(unreal.SystemLibrary.get_primary_asset_id_from_object(item).export_text() == item_id(relative).export_text(), relative + " primary asset id")
        check(item.get_editor_property("base_value") == value and item.get_editor_property("insurable") == insurable, relative + " economy")
    damage = unreal.get_default_object(load(DAMAGE_EFFECT).generated_class())
    check(len(damage.get_editor_property("modifiers")) == 1, "GE_Damage_Generic damages")
    check(unreal.get_default_object(load(EXOFRAME_BP).generated_class()).get_editor_property("death_effect") == dead_effect, "BP_ExoFrame death effect")
    check(load(CORE_DEFINITION).get_editor_property("loot_item").export_text() == item_id(CORE_ITEM).export_text(), "core loot item")
    zone = unreal.get_default_object(load(ZONE_BP).generated_class())
    check(zone.get_editor_property("volume").get_editor_property("box_extent").export_text() == ZONE_HALF.export_text(), "zone size")
    tag = make_tag(EXTRACTION_SOCKET).export_text()
    rules = load(BIOME_PATH).get_editor_property("socket_actors")
    check(any(r.get_editor_property("socket_type").export_text() == tag and r.get_editor_property("actor_class") == load(ZONE_BP).generated_class()
              for r in rules), "biome Extraction rule spawns BP_ExtractionZone")
    log("validation passed")


def main():
    ensure_items()
    ensure_damage_effect()
    dead_effect = ensure_dead_effect()
    ensure_death_on_exoframe(dead_effect)
    ensure_core_loot()
    ensure_pad_look()
    ensure_zone()
    ensure_biome_rule()
    validate(dead_effect)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_raid_flow_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
