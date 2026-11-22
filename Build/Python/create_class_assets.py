"""Creates the S6-01 player classes (idempotent, ADR-0010, ADR-0034).

  /Game/Hollow/Classes/Effects/GE_Class_Engineer|GE_Class_Occultist          infinite, the class's attribute modifiers
  /Game/Hollow/Classes/AbilitySets/DA_AbilitySet_Class_Engineer|_Occultist   the class's own abilities (S6-02/S6-03 fill)
  /Game/Hollow/Classes/DA_Class_Engineer, DA_Class_Occultist                 primary assets HollowClass

A class's effect, ability set and tag are granted by the server while a player plays it (UHollowClassComponent); its
starting loadout is a profile's the first time it picks the class; its visor colour tints the visor's telemetry.
The default class and the HollowClass scan are config (DefaultGame.ini), checked here. Effects and ability sets are
created when missing; the classes are filled or refreshed when they differ from the spec. Run after
create_inventory_assets.py, create_lantern_assets.py, create_ward_assets.py and create_economy_assets.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_class_assets.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

CLASSES_DIR = "/Game/Hollow/Classes"
EFFECTS_DIR = CLASSES_DIR + "/Effects"
SETS_DIR = CLASSES_DIR + "/AbilitySets"
ITEMS_DIR = "/Game/Hollow/Items"
ATTRIBUTE_MODULE = "/Script/HollowGAS"
INI = "Config/DefaultGame.ini"
INI_LINES = ["DefaultClass=HollowClass:DA_Class_Engineer"]
INI_SCAN = 'PrimaryAssetType="HollowClass"'
RAID_ITEM_TYPES = ["Hollow.Item.Type.Tool", "Hollow.Item.Type.Consumable", "Hollow.Item.Type.Relic"]   # UHollowEconomySettings::LoadoutItemTypes
ITEM_DIRS = {"DA_Item_Welder": "Tools", "DA_Item_PlasmaCutter": "Tools", "DA_Item_AetherLantern": "Tools", "DA_Item_FuelCell": "Consumables",
             "DA_Item_PulseMine": "Consumables", "DA_Item_OccultWard": "Consumables"}
EFFECT_LEVEL = 1.0   # gameplay ability system levels start at 1

# The classes (S6-01 clarifications 2, 4, 6). Modifiers: (attribute set, attribute, op, magnitude); only attributes
# that need no refill after a spawn (the init effect sets the base values, these modify on top).
CLASSES = {
    "DA_Class_Engineer": {
        "display": "Engineer",
        "description": "Cuts, welds and mines the squad's way through the wreck. Tools run hotter before they overheat; heavier plating.",
        "tag": "Hollow.Class.Engineer",
        "effect": ("GE_Class_Engineer", [("HollowToolSet", "MaxToolHeat", "AddBase", 25.0), ("HollowHealthSet", "Armor", "AddBase", 10.0)]),
        "ability_set": "DA_AbilitySet_Class_Engineer",
        # The old new-profile loadout (DefaultGame.ini before S6-01).
        "kit": [("DA_Item_Welder", 1, True), ("DA_Item_PlasmaCutter", 1, False), ("DA_Item_FuelCell", 3, False), ("DA_Item_PulseMine", 2, False)],
        "visor": (1.0, 0.72, 0.38, 0.9),
    },
    "DA_Class_Occultist": {
        "display": "Techno-Occultist",
        "description": "Reads the Aether and keeps it quiet: Resonance builds slower. The Aether lantern exposes what hunts the squad and dampens its Resonance; the ritual ward holds a doorway and calms whoever stands in it.",
        "tag": "Hollow.Class.Occultist",
        "effect": ("GE_Class_Occultist", [("HollowResonanceSet", "ResonanceGainMultiplier", "MultiplyAdditive", 0.8)]),
        "ability_set": "DA_AbilitySet_Class_Occultist",
        "kit": [("DA_Item_AetherLantern", 1, True), ("DA_Item_FuelCell", 3, False), ("DA_Item_OccultWard", 2, False)],   # S6-02 lantern, S6-03 wards
        "visor": (0.78, 0.62, 1.0, 0.9),
    },
}

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_class_assets: " + message)


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


def from_text(struct, text):
    if not struct.import_text(text):
        raise RuntimeError("Could not import " + text)
    return struct


def make_tag(name):
    return from_text(unreal.GameplayTag(), '(TagName="%s")' % name)


def item_path(name):
    return "%s/%s/%s" % (ITEMS_DIR, ITEM_DIRS[name], name)


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


# ---------------------------------------------------------------- effects and ability sets


def attribute_path(set_name, attribute):
    return "%s.%s:%s" % (ATTRIBUTE_MODULE, set_name, attribute)


def make_modifier(set_name, attribute, op, value):
    # Modifier fields are EditDefaultsOnly, so the struct is built from text (as create_gas_assets.py does).
    text = ('(Attribute=(AttributeName="{attr}",Attribute={path},AttributeOwner="/Script/CoreUObject.Class\'{module}.{set}\'"),'
            'ModifierOp={op},ModifierMagnitude=(MagnitudeCalculationType=ScalableFloat,ScalableFloatMagnitude=(Value={value:.6f})))'
            ).format(attr=attribute, path=attribute_path(set_name, attribute), module=ATTRIBUTE_MODULE, set=set_name, op=op, value=value)
    modifier = unreal.GameplayModifierInfo()
    if not modifier.import_text(text) or attribute_path(set_name, attribute) not in modifier.export_text() or ("ModifierOp=" + op) not in modifier.export_text():
        raise RuntimeError("Could not build modifier %s %s.%s" % (op, set_name, attribute))
    return modifier


def ensure_effect(name, modifiers):
    path = EFFECTS_DIR + "/" + name
    if assets.does_asset_exist(path):
        return load(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.GameplayEffect)
    blueprint = create(path, unreal.Blueprint, factory)
    effect = unreal.get_default_object(blueprint.generated_class())
    effect.set_editor_property("duration_policy", unreal.GameplayEffectDurationType.INFINITE)
    effect.set_editor_property("modifiers", [make_modifier(*m) for m in modifiers])
    save(blueprint)
    log("created " + name)
    return blueprint


def ensure_ability_set(name):
    path = SETS_DIR + "/" + name
    if assets.does_asset_exist(path):
        return load(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.HollowAbilitySet)
    ability_set = create(path, unreal.HollowAbilitySet, factory)
    save(ability_set)
    log("created " + name)
    return ability_set


# ---------------------------------------------------------------- classes


def kit(spec):
    slots = []
    for name, count, insured in spec:
        slots.append(from_text(unreal.HollowLoadoutSlot(), "(Item=HollowItem:%s,Count=%d,bInsured=%s)" % (name, count, "True" if insured else "False")))
    return slots


def ensure_class(name, spec, effect, ability_set):
    path = CLASSES_DIR + "/" + name
    created = not assets.does_asset_exist(path)
    if created:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.HollowClassDefinition)
        definition = create(path, unreal.HollowClassDefinition, factory)
    else:
        definition = load(path)
    wanted = {
        "display_name": unreal.Text(spec["display"]),
        "description": unreal.Text(spec["description"]),
        "class_tag": make_tag(spec["tag"]),
        "ability_set": ability_set,
        "attribute_effect": effect.generated_class(),
        "starting_loadout": kit(spec["kit"]),
        "visor_color": unreal.LinearColor(*spec["visor"]),
    }

    def text_of(value):
        if isinstance(value, (list, unreal.Array)):
            return [v.export_text() for v in value]
        if hasattr(value, "export_text"):
            return value.export_text()
        if isinstance(value, unreal.Text):
            return str(value)
        return value.get_path_name() if hasattr(value, "get_path_name") else value

    changed = created or any(text_of(definition.get_editor_property(k)) != text_of(v) for k, v in wanted.items())
    if changed:
        for key, value in wanted.items():
            definition.set_editor_property(key, value)
        save(definition)
        log("%s: %s" % (name, "created" if created else "refreshed"))
    return definition


# ---------------------------------------------------------------- validation


def validate():
    with open(unreal.Paths.project_dir() + INI) as ini:
        text = ini.read()
    lines = set(line.strip() for line in text.splitlines())
    for line in INI_LINES:
        check(line in lines, "DefaultGame.ini: " + line)
    check(INI_SCAN in text, "DefaultGame.ini scans the classes")
    raid_types = [make_tag(t).export_text() for t in RAID_ITEM_TYPES]
    colours = set()
    for name, spec in CLASSES.items():
        definition = load(CLASSES_DIR + "/" + name)
        check(unreal.SystemLibrary.get_primary_asset_id_from_object(definition).export_text().endswith(name), name + " primary asset id")
        check(definition.get_editor_property("class_tag").export_text() == make_tag(spec["tag"]).export_text(), name + " tag")
        check(definition.get_editor_property("ability_set") is not None, name + " ability set")
        effect = unreal.get_default_object(definition.get_editor_property("attribute_effect"))
        check(effect.get_editor_property("duration_policy") == unreal.GameplayEffectDurationType.INFINITE, name + " effect lasts while played")
        check(len(effect.get_editor_property("modifiers")) == len(spec["effect"][1]), name + " effect modifiers")
        for slot in definition.get_editor_property("starting_loadout"):
            item_name = str(slot.get_editor_property("item").get_editor_property("primary_asset_name"))
            item = load(item_path(item_name))
            check(item.get_editor_property("item_type").export_text() in raid_types, "%s: %s is a raid item" % (name, item_name))
            check(1 <= slot.get_editor_property("count") <= item.get_editor_property("max_stack"), "%s: %s within its stack" % (name, item_name))
        colours.add(definition.get_editor_property("visor_color").export_text())
    check(len(colours) == len(CLASSES), "every class has its own visor colour")
    log("validation passed")


def main():
    for name, spec in CLASSES.items():
        effect_name, modifiers = spec["effect"]
        ensure_class(name, spec, ensure_effect(effect_name, modifiers), ensure_ability_set(spec["ability_set"]))
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_class_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
