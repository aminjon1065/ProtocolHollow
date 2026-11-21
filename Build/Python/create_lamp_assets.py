"""Creates the S4-08 helmet lamp switch (idempotent, ADR-0010, ADR-0026).

  /Game/Hollow/GAS/Effects/GE_Cooldown_HelmetLamp    UHollowHelmetLampCooldownEffect: Hollow.Cooldown.HelmetLamp
  /Game/Hollow/Characters/ExoFrame/GA_HelmetLamp      UHollowGA_HelmetLamp with that cooldown (predicted switch)
  IA_HelmetLamp (F, gamepad D-pad up) in IMC_Raid and DA_InputConfig_Raid (Hollow.Input.HelmetLamp)
  DA_AbilitySet_ExoFrame                              GA_HelmetLamp on Hollow.Input.HelmetLamp

The lamp's cone, brightness and reach stay in create_gadget_assets.py; its click is in create_audio_assets.py.
Creates or fills only what is missing or differs, then validates.
Run after create_gadget_assets.py:  Build/Scripts/run-editor-python.sh Build/Python/create_lamp_assets.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

EFFECTS_DIR = "/Game/Hollow/GAS/Effects"
COOLDOWN_EFFECT = EFFECTS_DIR + "/GE_Cooldown_HelmetLamp"
LAMP_ABILITY = "/Game/Hollow/Characters/ExoFrame/GA_HelmetLamp"
ACTIONS_DIR = "/Game/Hollow/Core/Input/Actions"
IMC_PATH = "/Game/Hollow/Core/Input/IMC_Raid"
INPUT_CONFIG_PATH = "/Game/Hollow/Core/Input/DA_InputConfig_Raid"
ABILITY_SET_PATH = "/Game/Hollow/Characters/ExoFrame/DA_AbilitySet_ExoFrame"

# S4-08 clarification 5: F and D-pad up (the D-pad's other directions stay free for gadgets and pings).
LAMP_INPUT = ("IA_HelmetLamp", "Hollow.Input.HelmetLamp", ["F", "Gamepad_DPad_Up"])
# S4-08 clarification 3: the switch rests this long, so the lamp cannot strobe (a flash, off and on, takes 0.6 s: well
# under the 3 flashes a second of the photosensitivity limit).
COOLDOWN_SECONDS = 0.3

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_lamp_assets: " + message)


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


def make_key(name):
    return from_text(unreal.Key(), name)


def same(a, b):
    return a.export_text() == b.export_text() if hasattr(a, "export_text") else a == b


def scalable(value):
    return "(MagnitudeCalculationType=ScalableFloat,ScalableFloatMagnitude=(Value=%.6f))" % value


# ---------------------------------------------------------------- effect and ability

def ensure_cooldown():
    effect_bp, created = blueprint(COOLDOWN_EFFECT, unreal.HollowHelmetLampCooldownEffect)
    effect = unreal.get_default_object(effect_bp.generated_class())
    wanted = from_text(unreal.GameplayEffectModifierMagnitude(), scalable(COOLDOWN_SECONDS))
    if created or not same(effect.get_editor_property("duration_magnitude"), wanted):
        effect.set_editor_property("duration_magnitude", wanted)
        save(effect_bp)
        log("GE_Cooldown_HelmetLamp: %.2f s" % COOLDOWN_SECONDS)
    return effect_bp.generated_class()


def ensure_ability(cooldown):
    ability_bp, created = blueprint(LAMP_ABILITY, unreal.HollowGA_HelmetLamp)
    ability = unreal.get_default_object(ability_bp.generated_class())
    if created or ability.get_editor_property("cooldown_gameplay_effect_class") != cooldown:
        ability.set_editor_property("cooldown_gameplay_effect_class", cooldown)
        save(ability_bp)
        log("GA_HelmetLamp ready")
    return ability_bp.generated_class()


# ---------------------------------------------------------------- input and ability set

def mapped_keys(context, action):
    mappings = context.get_editor_property("default_key_mappings").get_editor_property("mappings")
    return [m.get_editor_property("key").export_text() for m in mappings if m.get_editor_property("action") == action]


def ensure_input():
    action_name, tag_name, keys = LAMP_INPUT
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
            entry.set_editor_property("key", make_key(key_name))
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
    ability_set = load(ABILITY_SET_PATH)
    granted = list(ability_set.get_editor_property("granted_abilities"))
    if any(e.get_editor_property("ability") == ability_class for e in granted):
        return
    entry = unreal.HollowAbilitySetAbility()
    entry.set_editor_property("ability", ability_class)
    entry.set_editor_property("input_tag", make_tag(LAMP_INPUT[1]))
    granted.append(entry)
    ability_set.set_editor_property("granted_abilities", granted)
    save(ability_set)
    log("granted GA_HelmetLamp in DA_AbilitySet_ExoFrame")


# ---------------------------------------------------------------- validation

def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate(ability_class, action):
    ability = unreal.get_default_object(ability_class)
    check(ability.get_editor_property("cooldown_gameplay_effect_class") == load(COOLDOWN_EFFECT).generated_class(), "GA_HelmetLamp cooldown")
    granted = load(ABILITY_SET_PATH).get_editor_property("granted_abilities")
    check(any(e.get_editor_property("ability") == ability_class and same(e.get_editor_property("input_tag"), make_tag(LAMP_INPUT[1]))
              for e in granted), "ExoFrame ability set grants GA_HelmetLamp on the lamp input")
    context = load(IMC_PATH)
    check(sorted(mapped_keys(context, action)) == sorted(make_key(k).export_text() for k in LAMP_INPUT[2]), "IA_HelmetLamp keys")
    for key_name in LAMP_INPUT[2]:
        key = make_key(key_name).export_text()
        users = [m.get_editor_property("action").get_name() for m in context.get_editor_property("default_key_mappings").get_editor_property("mappings")
                 if m.get_editor_property("key").export_text() == key]
        check(users == [LAMP_INPUT[0]], "%s belongs to the lamp only in IMC_Raid (also: %s)" % (key_name, ", ".join(users)))
    config = load(INPUT_CONFIG_PATH)
    check(any(e.get_editor_property("input_action") == action and same(e.get_editor_property("input_tag"), make_tag(LAMP_INPUT[1]))
              for e in config.get_editor_property("ability_actions")), "DA_InputConfig_Raid routes the lamp input")
    log("validation passed")


def main():
    cooldown = ensure_cooldown()
    ability_class = ensure_ability(cooldown)
    action = ensure_input()
    ensure_ability_set(ability_class)
    validate(ability_class, action)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_lamp_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
