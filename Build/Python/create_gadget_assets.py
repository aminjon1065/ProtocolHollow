"""Creates the S4-01 pulse mine and helmet lamp content (idempotent, ADR-0010, ADR-0019).

  /Game/Hollow/GAS/Effects/GE_State_Stunned          UHollowStunnedEffect: Hollow.State.Stunned for STUN_SECONDS
  /Game/Hollow/GAS/Effects/GE_Cooldown_PulseMine     UHollowPulseMineCooldownEffect: Hollow.Cooldown.Gadget.PulseMine
  /Game/Hollow/Gadgets/PulseMine/
      DA_PulseMine                                   UHollowPulseMineDefinition (arming, radii, stun, noise)
      BP_PulseMine                                   AHollowPulseMine: greybox disc and its flash
      GA_PulseMine                                   UHollowGA_PulseMine: mine class and cooldown; costs one mine item per use
      DA_AbilitySet_PulseMine                        GA_PulseMine on Hollow.Input.Gadget, granted by the pulse mine item while
                                                     it is in the raid inventory (ADR-0030; create_inventory_assets.py links it)
  IA_Gadget (G, gamepad right shoulder) in IMC_Raid and DA_InputConfig_Raid (Hollow.Input.Gadget)
  DA_AbilitySet_ExoFrame                             no longer grants GA_PulseMine (S5-01)
  BP_ExoFrame                                        helmet lamp cone, brightness and reach (the stalker avoids its cone)

Creates or fills only what is missing or differs, then validates.
Run after create_stalker_assets.py:  Build/Scripts/run-editor-python.sh Build/Python/create_gadget_assets.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

EFFECTS_DIR = "/Game/Hollow/GAS/Effects"
STUN_EFFECT = EFFECTS_DIR + "/GE_State_Stunned"
COOLDOWN_EFFECT = EFFECTS_DIR + "/GE_Cooldown_PulseMine"
MINE_DIR = "/Game/Hollow/Gadgets/PulseMine"
MINE_DEFINITION = MINE_DIR + "/DA_PulseMine"
MINE_BP = MINE_DIR + "/BP_PulseMine"
MINE_ABILITY = MINE_DIR + "/GA_PulseMine"
MINE_ABILITY_SET = MINE_DIR + "/DA_AbilitySet_PulseMine"
MINES_PER_USE = 1   # UHollowGA_PulseMine's item cost (its C++ default)
ACTIONS_DIR = "/Game/Hollow/Core/Input/Actions"
IMC_PATH = "/Game/Hollow/Core/Input/IMC_Raid"
INPUT_CONFIG_PATH = "/Game/Hollow/Core/Input/DA_InputConfig_Raid"
ABILITY_SET_PATH = "/Game/Hollow/Characters/ExoFrame/DA_AbilitySet_ExoFrame"
EXOFRAME_BP = "/Game/Hollow/Characters/ExoFrame/BP_ExoFrame"

CYLINDER_MESH = "/Engine/BasicShapes/Cylinder.Cylinder"
GREYBOX_MATERIAL = "/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"

GADGET_INPUT = ("IA_Gadget", "Hollow.Input.Gadget", ["G", "Gamepad_RightShoulder"])

# Pulse mine (data, S4-01 clarification 4).
STUN_SECONDS = 5.0
COOLDOWN_SECONDS = 20.0
MINE = {"arm_seconds": 1.0, "check_seconds": 0.1, "trigger_radius": 250.0, "blast_radius": 400.0, "sight_height": 30.0,
        "flash_seconds": 0.6, "place_trace_distance": 100.0}
BLAST_NOISE = "Hollow.Noise.Impact.Heavy"
MINE_SCALE = unreal.Vector(0.3, 0.3, 0.06)    # a 30 cm disc, 6 cm tall (the Cylinder is 100 x 100, centred)
MINE_LIFT = unreal.Vector(0.0, 0.0, 3.0)      # its centre half its height above the floor point
FLASH = {"intensity": 20000.0, "attenuation_radius": 600.0}
FLASH_COLOR = unreal.Color(r=140, g=210, b=255, a=255)
FLASH_OFFSET = unreal.Vector(0.0, 0.0, 600.0)   # relative to the scaled disc: about 36 cm above the floor

# Helmet lamp (data): the cone the Echo-Stalker keeps out of (UHollowEnvQueryTest_OutOfLightCone).
LAMP = {"intensity": 1500.0, "inner_cone_angle": 16.0, "outer_cone_angle": 26.0, "attenuation_radius": 1800.0, "cast_shadows": True}
LAMP_COLOR = unreal.Color(r=255, g=244, b=225, a=255)
LAMP_OFFSET = unreal.Vector(15.0, 0.0, 0.0)   # just in front of the eyes

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_gadget_assets: " + message)


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


def same(a, b):
    return a.export_text() == b.export_text() if hasattr(a, "export_text") else a == b


def apply_values(obj, values):
    """Sets the properties that differ; True if any did."""
    changed = False
    for name, value in values.items():
        current = obj.get_editor_property(name)
        if isinstance(value, float) and isinstance(current, float):
            differs = abs(current - value) > 1e-4
        else:
            differs = not same(current, value)
        if differs:
            obj.set_editor_property(name, value)
            changed = True
    return changed


def scalable(value):
    return "(MagnitudeCalculationType=ScalableFloat,ScalableFloatMagnitude=(Value=%.6f))" % value


# ---------------------------------------------------------------- effects

def ensure_timed_effect(path, parent, seconds):
    effect_bp, created = blueprint(path, parent)
    effect = unreal.get_default_object(effect_bp.generated_class())
    wanted = from_text(unreal.GameplayEffectModifierMagnitude(), scalable(seconds))
    if created or not same(effect.get_editor_property("duration_magnitude"), wanted):
        effect.set_editor_property("duration_magnitude", wanted)
        save(effect_bp)
        log("%s: %.1f s" % (path.rsplit("/", 1)[1], seconds))
    return effect_bp.generated_class()


# ---------------------------------------------------------------- mine

def ensure_mine_definition(stun_effect):
    definition, created = data_asset(MINE_DEFINITION, unreal.HollowPulseMineDefinition)
    changed = apply_values(definition, MINE)
    changed = apply_values(definition, {"blast_noise": make_tag(BLAST_NOISE)}) or changed
    if definition.get_editor_property("stun_effect") != stun_effect:
        definition.set_editor_property("stun_effect", stun_effect)
        changed = True
    if created or changed:
        save(definition)
        log("DA_PulseMine ready")
    return definition


def ensure_mine_bp(definition):
    mine_bp, created = blueprint(MINE_BP, unreal.HollowPulseMine)
    mine = unreal.get_default_object(mine_bp.generated_class())
    changed = created
    if mine.get_editor_property("definition") != definition:
        mine.set_editor_property("definition", definition)
        changed = True
    body = mine.get_editor_property("body")
    if body.get_editor_property("static_mesh") != load(CYLINDER_MESH):
        body.set_editor_property("static_mesh", load(CYLINDER_MESH))
        body.set_material(0, load(GREYBOX_MATERIAL))
        changed = True
    changed = apply_values(body, {"relative_scale3d": MINE_SCALE, "relative_location": MINE_LIFT}) or changed
    flash = mine.get_editor_property("flash")
    changed = apply_values(flash, dict(FLASH, relative_location=FLASH_OFFSET)) or changed
    if not same(flash.get_editor_property("light_color"), FLASH_COLOR):
        flash.set_editor_property("light_color", FLASH_COLOR)
        changed = True
    if changed:
        save(mine_bp)
        log("BP_PulseMine ready")
    return mine_bp.generated_class()


def ensure_mine_ability(mine_class, cooldown):
    ability_bp, created = blueprint(MINE_ABILITY, unreal.HollowGA_PulseMine)
    ability = unreal.get_default_object(ability_bp.generated_class())
    changed = created
    for name, value in (("mine_class", mine_class), ("cooldown_gameplay_effect_class", cooldown)):
        if ability.get_editor_property(name) != value:
            ability.set_editor_property(name, value)
            changed = True
    if changed:
        save(ability_bp)
        log("GA_PulseMine ready")
    return ability_bp.generated_class()


# ---------------------------------------------------------------- input, ability set, lamp

def ensure_input():
    action_name, tag_name, keys = GADGET_INPUT
    path = ACTIONS_DIR + "/" + action_name
    if not assets.does_asset_exist(path):
        action = create(path, unreal.InputAction, unreal.InputAction_Factory())
        action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        save(action)
        log("created " + action_name)
    action = load(path)
    context = load(IMC_PATH)
    data = context.get_editor_property("default_key_mappings")
    mappings = list(data.get_editor_property("mappings"))
    if not any(m.get_editor_property("action") == action for m in mappings):
        for key_name in keys:
            entry = unreal.EnhancedActionKeyMapping()
            entry.set_editor_property("action", action)
            entry.set_editor_property("key", from_text(unreal.Key(), key_name))
            mappings.append(entry)
        data.set_editor_property("mappings", mappings)
        context.set_editor_property("default_key_mappings", data)
        save(context)
        log("mapped %s to %s in IMC_Raid" % (action_name, " and ".join(keys)))
    config = load(INPUT_CONFIG_PATH)
    entries = list(config.get_editor_property("ability_actions"))
    tag = make_tag(tag_name)
    if not any(same(e.get_editor_property("input_tag"), tag) for e in entries):
        entry = unreal.HollowInputAction()
        entry.set_editor_property("input_action", action)
        entry.set_editor_property("input_tag", tag)
        entries.append(entry)
        config.set_editor_property("ability_actions", entries)
        save(config)
        log("added %s to DA_InputConfig_Raid" % tag_name)
    return action


def ensure_ability_set(ability_class):
    """The mine comes from its item (ADR-0030): its own set grants it, the suit's no longer does."""
    suit = load(ABILITY_SET_PATH)
    granted = list(suit.get_editor_property("granted_abilities"))
    kept = [e for e in granted if e.get_editor_property("ability") != ability_class]
    if len(kept) != len(granted):
        suit.set_editor_property("granted_abilities", kept)
        save(suit)
        log("DA_AbilitySet_ExoFrame: no GA_PulseMine")
    mine_set, _ = data_asset(MINE_ABILITY_SET, unreal.HollowAbilitySet)
    entries = mine_set.get_editor_property("granted_abilities")
    if len(entries) == 1 and entries[0].get_editor_property("ability") == ability_class \
            and same(entries[0].get_editor_property("input_tag"), make_tag(GADGET_INPUT[1])):
        return
    entry = unreal.HollowAbilitySetAbility()
    entry.set_editor_property("ability", ability_class)
    entry.set_editor_property("input_tag", make_tag(GADGET_INPUT[1]))
    mine_set.set_editor_property("granted_abilities", [entry])
    save(mine_set)
    log("DA_AbilitySet_PulseMine: GA_PulseMine on " + GADGET_INPUT[1])


def ensure_lamp():
    exo_bp = load(EXOFRAME_BP)
    lamp = unreal.get_default_object(exo_bp.generated_class()).get_editor_property("helmet_light")
    changed = apply_values(lamp, dict(LAMP, relative_location=LAMP_OFFSET))
    if not same(lamp.get_editor_property("light_color"), LAMP_COLOR):
        lamp.set_editor_property("light_color", LAMP_COLOR)
        changed = True
    if changed:
        save(exo_bp)
        log("BP_ExoFrame: helmet lamp set")


# ---------------------------------------------------------------- validation

def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate(mine_class, ability_class):
    definition = load(MINE_DEFINITION)
    check(definition.get_editor_property("blast_radius") >= definition.get_editor_property("trigger_radius"), "the blast reaches the trigger")
    check(definition.get_editor_property("stun_effect") == load(STUN_EFFECT).generated_class(), "stun effect")
    check(unreal.get_default_object(mine_class).get_editor_property("definition") == definition, "BP_PulseMine definition")
    ability = unreal.get_default_object(ability_class)
    check(ability.get_editor_property("mine_class") == mine_class, "GA_PulseMine mine")
    check(ability.get_editor_property("cooldown_gameplay_effect_class") == load(COOLDOWN_EFFECT).generated_class(), "GA_PulseMine cooldown")
    check(ability.get_editor_property("item_cost_per_use") == MINES_PER_USE, "GA_PulseMine costs one mine")
    check(not any(e.get_editor_property("ability") == ability_class for e in load(ABILITY_SET_PATH).get_editor_property("granted_abilities")),
          "the suit does not grant GA_PulseMine")
    granted = load(MINE_ABILITY_SET).get_editor_property("granted_abilities")
    check(len(granted) == 1 and granted[0].get_editor_property("ability") == ability_class
          and same(granted[0].get_editor_property("input_tag"), make_tag(GADGET_INPUT[1])), "DA_AbilitySet_PulseMine grants GA_PulseMine on the gadget input")
    action = load(ACTIONS_DIR + "/" + GADGET_INPUT[0])
    mapped = [m for m in load(IMC_PATH).get_editor_property("default_key_mappings").get_editor_property("mappings")
              if m.get_editor_property("action") == action]
    check(len(mapped) == len(GADGET_INPUT[2]), "IA_Gadget keys")
    lamp = unreal.get_default_object(load(EXOFRAME_BP).generated_class()).get_editor_property("helmet_light")
    check(lamp.get_editor_property("outer_cone_angle") > lamp.get_editor_property("inner_cone_angle") > 0.0, "helmet lamp cone")
    check(lamp.get_editor_property("attenuation_radius") > 0.0 and lamp.get_editor_property("intensity") > 0.0, "helmet lamp reach")
    log("validation passed")


def main():
    stun = ensure_timed_effect(STUN_EFFECT, unreal.HollowStunnedEffect, STUN_SECONDS)
    cooldown = ensure_timed_effect(COOLDOWN_EFFECT, unreal.HollowPulseMineCooldownEffect, COOLDOWN_SECONDS)
    definition = ensure_mine_definition(stun)
    mine_class = ensure_mine_bp(definition)
    ability_class = ensure_mine_ability(mine_class, cooldown)
    ensure_input()
    ensure_ability_set(ability_class)
    ensure_lamp()
    validate(mine_class, ability_class)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_gadget_assets failed:\n" + traceback.format_exc())
