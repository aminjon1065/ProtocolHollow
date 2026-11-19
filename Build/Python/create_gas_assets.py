"""Creates the S1-05 GAS content (idempotent, ADR-0010).

  /Game/Hollow/GAS/Effects/GE_ExoFrame_InitAttributes   instant, Override modifiers for every attribute
  /Game/Hollow/Characters/ExoFrame/DA_AbilitySet_ExoFrame  grants GE_ExoFrame_InitAttributes
  BP_ExoFrame.AbilitySets                                  set to [DA_AbilitySet_ExoFrame] only if empty

Creates only missing assets (never overwrites tuning done in the editor), then validates what is on disk.
Run: Build/Scripts/run-editor-python.sh Build/Python/create_gas_assets.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

INIT_EFFECT_PATH = "/Game/Hollow/GAS/Effects/GE_ExoFrame_InitAttributes"
ABILITY_SET_PATH = "/Game/Hollow/Characters/ExoFrame/DA_AbilitySet_ExoFrame"
EXOFRAME_BP_PATH = "/Game/Hollow/Characters/ExoFrame/BP_ExoFrame"
ATTRIBUTE_MODULE = "/Script/HollowGAS"

# Initial ExoFrame attributes (data; tune in the editor afterwards).
# Order matters: every max comes before its current value, otherwise the current value is clamped to the old max (0).
INIT_ATTRIBUTES = [
    ("HollowHealthSet", "MaxHealth", 100.0), ("HollowHealthSet", "Health", 100.0), ("HollowHealthSet", "Armor", 0.0),
    ("HollowSurvivalSet", "MaxStamina", 100.0), ("HollowSurvivalSet", "Stamina", 100.0),
    ("HollowSurvivalSet", "MaxOxygen", 100.0), ("HollowSurvivalSet", "Oxygen", 100.0),
    ("HollowSurvivalSet", "MaxThermal", 100.0), ("HollowSurvivalSet", "Thermal", 100.0),
    ("HollowResonanceSet", "MaxResonance", 100.0), ("HollowResonanceSet", "Resonance", 0.0),
    ("HollowResonanceSet", "ResonanceGainMultiplier", 1.0),
    ("HollowToolSet", "MaxToolFuel", 100.0), ("HollowToolSet", "ToolFuel", 100.0),
    ("HollowToolSet", "MaxToolHeat", 100.0), ("HollowToolSet", "ToolHeat", 0.0),
]
EFFECT_LEVEL = 1.0   # gameplay ability system levels start at 1

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_gas_assets: " + message)


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


def attribute_path(set_name, attribute):
    return "%s.%s:%s" % (ATTRIBUTE_MODULE, set_name, attribute)


def make_modifier(set_name, attribute, value):
    # Modifier fields are EditDefaultsOnly, so the struct is built from text (verified against UE 5.8).
    text = ('(Attribute=(AttributeName="{attr}",Attribute={path},AttributeOwner="/Script/CoreUObject.Class\'{module}.{set}\'"),'
            'ModifierOp=Override,ModifierMagnitude=(MagnitudeCalculationType=ScalableFloat,ScalableFloatMagnitude=(Value={value:.6f})))'
            ).format(attr=attribute, path=attribute_path(set_name, attribute), module=ATTRIBUTE_MODULE, set=set_name, value=value)
    modifier = unreal.GameplayModifierInfo()
    if not modifier.import_text(text) or attribute_path(set_name, attribute) not in modifier.export_text():
        raise RuntimeError("Could not build modifier for %s.%s" % (set_name, attribute))
    return modifier


def ensure(path, build):
    if assets.does_asset_exist(path):
        log("%s exists, kept as is" % path)
        return
    save(build(path))
    log("created " + path)


def build_init_effect(path):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.GameplayEffect)
    blueprint = create(path, unreal.Blueprint, factory)
    effect = unreal.get_default_object(blueprint.generated_class())
    effect.set_editor_property("duration_policy", unreal.GameplayEffectDurationType.INSTANT)
    effect.set_editor_property("modifiers", [make_modifier(*entry) for entry in INIT_ATTRIBUTES])
    return blueprint


def build_ability_set(path):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.HollowAbilitySet)
    ability_set = create(path, unreal.HollowAbilitySet, factory)
    entry = unreal.HollowAbilitySetEffect()
    entry.set_editor_property("gameplay_effect", load(INIT_EFFECT_PATH).generated_class())
    entry.set_editor_property("effect_level", EFFECT_LEVEL)
    ability_set.set_editor_property("granted_effects", [entry])
    return ability_set


def link_exoframe():
    blueprint = load(EXOFRAME_BP_PATH)
    character = unreal.get_default_object(blueprint.generated_class())
    if len(character.get_editor_property("ability_sets")) > 0:
        log("BP_ExoFrame already has ability sets, kept as is")
        return
    character.set_editor_property("ability_sets", [load(ABILITY_SET_PATH)])
    save(blueprint)
    log("linked DA_AbilitySet_ExoFrame into BP_ExoFrame")


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate():
    effect = unreal.get_default_object(load(INIT_EFFECT_PATH).generated_class())
    check(effect.get_editor_property("duration_policy") == unreal.GameplayEffectDurationType.INSTANT, "init effect is instant")
    modifiers = [m.export_text() for m in effect.get_editor_property("modifiers")]
    check(len(modifiers) == len(INIT_ATTRIBUTES), "init effect modifier count")
    for set_name, attribute, _ in INIT_ATTRIBUTES:
        check(any(attribute_path(set_name, attribute) + "," in m and "ModifierOp=Override" in m for m in modifiers),
              "init effect overrides %s.%s" % (set_name, attribute))

    effects = load(ABILITY_SET_PATH).get_editor_property("granted_effects")
    # Later scripts grant more effects (e.g. tool cooling, S2-03): check presence, not an exact count.
    check(any(e.get_editor_property("gameplay_effect") == load(INIT_EFFECT_PATH).generated_class() for e in effects),
          "ability set grants the init effect")

    character = unreal.get_default_object(load(EXOFRAME_BP_PATH).generated_class())
    check(load(ABILITY_SET_PATH) in list(character.get_editor_property("ability_sets")), "BP_ExoFrame grants the ability set")
    log("validation passed")


def main():
    ensure(INIT_EFFECT_PATH, build_init_effect)
    ensure(ABILITY_SET_PATH, build_ability_set)
    link_exoframe()
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_gas_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
