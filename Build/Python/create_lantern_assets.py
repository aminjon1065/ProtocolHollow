"""Creates the S6-02 Aether lantern of the Techno-Occultist (idempotent, ADR-0010, ADR-0035).

  /Game/Hollow/GAS/Effects/GE_Occult_Expose                     UHollowOccultExposeEffect: an enemy in the light, slowed, weaker armour
  /Game/Hollow/GAS/Effects/GE_AntiResonance                     UHollowAntiResonanceEffect: a player in the aura, slower Resonance
  /Game/Hollow/Tools/AetherLantern/DA_Tool_AetherLantern        UHollowLanternDefinition: cone, aura, fuel, heat, hum
  /Game/Hollow/Tools/AetherLantern/GA_Tool_AetherLantern        UHollowGA_AetherLantern with that definition
  /Game/Hollow/Tools/AetherLantern/DA_AbilitySet_AetherLantern  the lantern on Hollow.Input.PrimaryUse
  /Game/Hollow/Items/Tools/DA_Item_AetherLantern                the item (tool, insurable) granting that set in a raid
  /Game/Hollow/GAS/Cues/GCN_Tool_Lantern                        AHollowLanternCue: the light cone on every machine
  /Game/Hollow/GAS/Cues/GCN_Status_Exposed                      AHollowExposedCue: the glow on an exposed enemy
  GE_Stalker_Traits                                             checked: it inherits the S6-02 light requirement (no immunity in the light)

Checked here, made elsewhere: the lantern's noise rule (create_noise_assets.py) and hum (create_audio_assets.py). The shop
offer and the Occultist's starting loadout are create_economy_assets.py's and create_class_assets.py's.
Every asset is created when missing and refreshed only when it differs from the spec below.
Run after create_inventory_assets.py, before create_economy_assets.py and create_class_assets.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_lantern_assets.py
"""
import os
import sys
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hollow_loc as loc   # LOC-01 (ADR-0048): player texts are string table entries

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

EFFECTS_DIR = "/Game/Hollow/GAS/Effects"
CUES_DIR = "/Game/Hollow/GAS/Cues"
LANTERN_DIR = "/Game/Hollow/Tools/AetherLantern"
ITEM_PATH = "/Game/Hollow/Items/Tools/DA_Item_AetherLantern"
DEFINITION_PATH = LANTERN_DIR + "/DA_Tool_AetherLantern"
ABILITY_PATH = LANTERN_DIR + "/GA_Tool_AetherLantern"
SET_PATH = LANTERN_DIR + "/DA_AbilitySet_AetherLantern"
EXPOSE_PATH = EFFECTS_DIR + "/GE_Occult_Expose"
AURA_PATH = EFFECTS_DIR + "/GE_AntiResonance"
LIGHT_CUE_PATH = CUES_DIR + "/GCN_Tool_Lantern"
GLOW_CUE_PATH = CUES_DIR + "/GCN_Status_Exposed"
STALKER_TRAITS = EFFECTS_DIR + "/GE_Stalker_Traits"
NOISE_CONFIG = "/Game/Hollow/Core/DA_NoiseConfig"
SOUND_BANK = "/Game/Hollow/Audio/DA_SoundBank"
ATTRIBUTE_MODULE = "/Script/HollowGAS"
TOOL_EFFECTS = {"fuel_drain_effect": "GE_Tool_FuelDrain", "heat_effect": "GE_Tool_Heat", "overheat_effect": "GE_Tool_Overheat"}

TOOL_TAG = "Hollow.Ability.Tool.AetherLantern"
NOISE_TAG = "Hollow.Noise.Tool.Lantern"
INPUT_TAG = "Hollow.Input.PrimaryUse"     # S6-02 clarification 1: the class's main tool, like the Engineer's cutter
REQUIREMENTS_CLASS = "/Script/GameplayAbilities.TargetTagRequirementsGameplayEffectComponent"
REQUIREMENTS_NAME = "NotInTheLight"   # UHollowStalkerTraitsEffect's subobject

# S6-02 clarification 2 (data, balance later). Effects: (attribute set, attribute, op, magnitude); the lantern's effects
# multiply on top of everything else (MultiplyCompound: with the Occultist's x0.8 the aura makes x0.4).
EXPOSE_SECONDS = 2.0                      # lingers that long once the light has gone
EXPOSE_MODIFIERS = [("HollowMovementSet", "MoveSpeedMultiplier", "MultiplyCompound", 0.6),
                    ("HollowHealthSet", "Armor", "MultiplyCompound", 0.5)]
AURA_SECONDS = 1.0
AURA_MODIFIERS = [("HollowResonanceSet", "ResonanceGainMultiplier", "MultiplyCompound", 0.5)]
# The tool: a 25 degree half cone 9 m long, sampled 10 times a second; 3 fuel a second (a full tank of 100 lasts ~33 s),
# 1.5 heat a sample against the suit's cooling of 10 a second (MaxToolHeat 100: overheats after ~20 s on).
LANTERN = {
    "action_label": "Exposing",
    "sample_rate_hz": 10.0, "range": 900.0, "hit_tolerance_radius": 0.0, "sticky_angle_degrees": 0.0, "sticky_grace_seconds": 0.0,
    "work_rate": 0.0, "fuel_per_second": 3.0, "min_fuel_to_start": 1.0, "heat_per_sample": 1.5,
    "beam_color": (0.55, 0.45, 1.0, 1.0), "beam_width": 0.0,
    "cone_half_angle_degrees": 25.0, "aura_radius": 600.0,
}
# The item (S6-02 clarification 7): a tool, insurable; the shop sells it for more than its value (create_economy_assets.py).
ITEM = {"display_name": "Aether lantern", "item_type": "Hollow.Item.Type.Tool", "base_value": 300, "insurable": True, "max_stack": 1,
        "equip_slot": "Hollow.Item.Slot.Primary"}   # S6-04: the cutter's input, one of them per loadout
# Cosmetics (greybox data): the light cone (no shadows: light budget) and the glow on an exposed enemy.
LIGHT_CUE = {"update_rate_hz": 30.0, "muzzle_offset": (30.0, 15.0, -15.0), "inner_cone_fraction": 0.6}
# AK-8: in lumens like the rooms' lights (3500 lm practicals): unitless 2000 was far dimmer than a practical, and
# the cone did not show at all; it flares x2 as the lantern lights (GCN_Tool_Lantern, create_art_kit.py).
LIGHT = {"intensity_units": unreal.LightUnits.LUMENS, "intensity": 2500.0}
GLOW_CUE = {"glow_offset": (0.0, 0.0, 40.0)}
GLOW = {"intensity": 800.0, "attenuation_radius": 220.0}
GLOW_COLOR = (180, 110, 255)

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_lantern_assets: " + message)


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


def make_tags(*names):
    return from_text(unreal.GameplayTagContainer(), "(GameplayTags=(%s))" % ",".join('(TagName="%s")' % n for n in names))


def text_of(value):
    """A comparable form of a property value (structs and arrays by their text, objects by their path)."""
    if isinstance(value, (list, tuple, unreal.Array)):
        return [text_of(v) for v in value]
    if isinstance(value, unreal.Text):
        return [loc.entry(value), str(value)]   # LOC-01: a plain text is not the table's entry
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


# ---------------------------------------------------------------- effects

def make_modifier(set_name, attribute, op, value):
    # Modifier fields are EditDefaultsOnly, so the struct is built from text (as create_class_assets.py does).
    path = "%s.%s:%s" % (ATTRIBUTE_MODULE, set_name, attribute)
    text = ('(Attribute=(AttributeName="{attr}",Attribute={path},AttributeOwner="/Script/CoreUObject.Class\'{module}.{set}\'"),'
            'ModifierOp={op},ModifierMagnitude=(MagnitudeCalculationType=ScalableFloat,ScalableFloatMagnitude=(Value={value:.6f})))'
            ).format(attr=attribute, path=path, module=ATTRIBUTE_MODULE, set=set_name, op=op, value=value)
    modifier = unreal.GameplayModifierInfo()
    if not modifier.import_text(text) or path not in modifier.export_text() or ("ModifierOp=" + op) not in modifier.export_text():
        raise RuntimeError("Could not build modifier %s %s.%s" % (op, set_name, attribute))
    return modifier


def duration(seconds):
    return from_text(unreal.GameplayEffectModifierMagnitude(), "(MagnitudeCalculationType=ScalableFloat,ScalableFloatMagnitude=(Value=%.6f))" % seconds)


def ensure_effect(path, parent, seconds, modifiers):
    effect_bp, created = blueprint(path, parent)
    effect = unreal.get_default_object(effect_bp.generated_class())
    if refresh(effect, {"duration_magnitude": duration(seconds), "modifiers": [make_modifier(*m) for m in modifiers]}) or created:
        save(effect_bp)
        log("%s: %.1f s, %s" % (path.rsplit("/", 1)[1], seconds, ", ".join("%s %s %.2f" % (m[1], m[2], m[3]) for m in modifiers)))
    return effect_bp.generated_class()


def stalker_traits_ready():
    """
    GE_Stalker_Traits (create_stalker_assets.py, S4-01) inherits its C++ structure's light requirement (S6-02): the
    native component, by its class and name (Python sees neither its type nor its fields; Hollow.GAS.AetherLantern.Effects
    checks what it does).
    """
    effect = unreal.get_default_object(load(STALKER_TRAITS).generated_class())
    return any(c.get_class().get_path_name() == REQUIREMENTS_CLASS and c.get_name() == REQUIREMENTS_NAME
               for c in effect.get_editor_property("ge_components"))


# ---------------------------------------------------------------- tool, ability, set, item

def ensure_definition(expose, aura):
    definition, created = data_asset(DEFINITION_PATH, unreal.HollowLanternDefinition)
    wanted = {key: value for key, value in LANTERN.items() if key not in ("action_label", "beam_color")}
    wanted["action_label"] = loc.text(loc.ITEMS, "Tool.AetherLantern.Action", LANTERN["action_label"])
    wanted["beam_color"] = unreal.LinearColor(*LANTERN["beam_color"])
    wanted["tool_tag"] = make_tag(TOOL_TAG)
    wanted["noise_tag"] = make_tag(NOISE_TAG)
    wanted["expose_effect"] = expose
    wanted["aura_effect"] = aura
    for key, name in TOOL_EFFECTS.items():
        wanted[key] = load(EFFECTS_DIR + "/" + name).generated_class()
    if refresh(definition, wanted) or created:
        save(definition)
        log("DA_Tool_AetherLantern: cone %.0f deg x %.0f cm, aura %.0f cm" % (LANTERN["cone_half_angle_degrees"], LANTERN["range"], LANTERN["aura_radius"]))
    return definition


def ensure_ability(definition):
    ability_bp, created = blueprint(ABILITY_PATH, unreal.HollowGA_AetherLantern)
    ability = unreal.get_default_object(ability_bp.generated_class())
    if refresh(ability, {"tool_definition": definition, "ability_tags": make_tags(TOOL_TAG)}) or created:
        save(ability_bp)
        log("GA_Tool_AetherLantern ready")
    return ability_bp.generated_class()


def ensure_ability_set(ability_class):
    ability_set, created = data_asset(SET_PATH, unreal.HollowAbilitySet)
    entry = unreal.HollowAbilitySetAbility()
    entry.set_editor_property("ability", ability_class)
    entry.set_editor_property("input_tag", make_tag(INPUT_TAG))
    if refresh(ability_set, {"granted_abilities": [entry]}) or created:
        save(ability_set)
        log("DA_AbilitySet_AetherLantern: the lantern on " + INPUT_TAG)
    return ability_set


def ensure_item(ability_set):
    item, created = data_asset(ITEM_PATH, unreal.HollowItemDefinition)
    wanted = dict(ITEM, display_name=loc.text(loc.ITEMS, "Item.AetherLantern.Name", ITEM["display_name"]), item_type=make_tag(ITEM["item_type"]), equip_slot=make_tag(ITEM["equip_slot"]))
    changed = refresh(item, wanted)
    if text_of(item.get_editor_property("ability_set")) != text_of(ability_set):
        item.set_editor_property("ability_set", ability_set)
        changed = True
    if changed or created:
        save(item)
        log("DA_Item_AetherLantern: %d cr, insurable, grants the lantern in a raid" % ITEM["base_value"])


# ---------------------------------------------------------------- cues

def vector(values):
    return unreal.Vector(*values)


def ensure_cues():
    light_bp, created = blueprint(LIGHT_CUE_PATH, unreal.HollowLanternCue)
    cue = unreal.get_default_object(light_bp.generated_class())
    changed = refresh(cue, dict(LIGHT_CUE, gameplay_cue_tag=make_tag("GameplayCue.Hollow.Tool.Lantern"), muzzle_offset=vector(LIGHT_CUE["muzzle_offset"])))
    changed = refresh(cue.get_editor_property("light"), LIGHT) or changed
    if changed or created:
        save(light_bp)
        log("GCN_Tool_Lantern ready")

    glow_bp, created = blueprint(GLOW_CUE_PATH, unreal.HollowExposedCue)
    cue = unreal.get_default_object(glow_bp.generated_class())
    changed = refresh(cue, {"gameplay_cue_tag": make_tag("GameplayCue.Hollow.Status.Exposed"), "glow_offset": vector(GLOW_CUE["glow_offset"])})
    glow = cue.get_editor_property("glow")
    changed = refresh(glow, dict(GLOW, light_color=unreal.Color(r=GLOW_COLOR[0], g=GLOW_COLOR[1], b=GLOW_COLOR[2], a=255))) or changed
    if changed or created:
        save(glow_bp)
        log("GCN_Status_Exposed ready")


# ---------------------------------------------------------------- validation

def validate():
    validator = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
    definition = load(DEFINITION_PATH)
    result = validator.is_object_valid(definition, unreal.DataValidationUsecase.SCRIPT)
    check(result[0] != unreal.DataValidationResult.INVALID, "DA_Tool_AetherLantern is valid: %s" % [str(e) for e in result[1]])
    for path in (EXPOSE_PATH, AURA_PATH):
        effect = unreal.get_default_object(load(path).generated_class())
        check(effect.get_editor_property("duration_policy") == unreal.GameplayEffectDurationType.HAS_DURATION, path + " lasts a while")
    ability = unreal.get_default_object(load(ABILITY_PATH).generated_class())
    check(ability.get_editor_property("tool_definition") == definition, "GA_Tool_AetherLantern uses DA_Tool_AetherLantern")
    granted = load(SET_PATH).get_editor_property("granted_abilities")
    check(len(granted) == 1 and granted[0].get_editor_property("ability") == load(ABILITY_PATH).generated_class(), "the set grants the lantern")
    item = load(ITEM_PATH)
    check(text_of(item.get_editor_property("ability_set")) == text_of(load(SET_PATH)), "DA_Item_AetherLantern grants the set")
    check(unreal.SystemLibrary.get_primary_asset_id_from_object(item).export_text().endswith("DA_Item_AetherLantern"), "item primary asset id")
    noise_tag = make_tag(NOISE_TAG).export_text()
    check(any(r.get_editor_property("noise_tag").export_text() == noise_tag for r in load(NOISE_CONFIG).get_editor_property("rules")),
          "DA_NoiseConfig has the lantern's rule (create_noise_assets.py)")
    check(any(e.get_editor_property("sound_tag").export_text() == noise_tag for e in load(SOUND_BANK).get_editor_property("entries")),
          "DA_SoundBank has the lantern's hum (create_audio_assets.py)")
    check(stalker_traits_ready(), "GE_Stalker_Traits: no kinetic immunity in the Aether light")
    log("validation passed")


def main():
    expose = ensure_effect(EXPOSE_PATH, unreal.HollowOccultExposeEffect, EXPOSE_SECONDS, EXPOSE_MODIFIERS)
    aura = ensure_effect(AURA_PATH, unreal.HollowAntiResonanceEffect, AURA_SECONDS, AURA_MODIFIERS)
    definition = ensure_definition(expose, aura)
    ensure_item(ensure_ability_set(ensure_ability(definition)))
    ensure_cues()
    loc.save()
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_lantern_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
