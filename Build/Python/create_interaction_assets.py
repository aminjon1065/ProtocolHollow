"""Creates the S1-06 interaction content (idempotent, ADR-0010).

  /Game/Hollow/Core/Input/Actions/IA_Interact             + IMC_Raid mappings and DA_InputConfig_Raid entry, if missing
  /Game/Hollow/World/Interactables/BP_Switch              generic switch: cube mesh, green lamp, prompts
  /Game/Hollow/Maps/L_Hub_Dev                             one BP_Switch in front of every PlayerStart, if missing

Creates or adds only what is missing (never overwrites tuning done in the editor), then validates.
Run: Build/Scripts/run-editor-python.sh Build/Python/create_interaction_assets.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

ACTION_PATH = "/Game/Hollow/Core/Input/Actions/IA_Interact"
IMC_PATH = "/Game/Hollow/Core/Input/IMC_Raid"
INPUT_CONFIG_PATH = "/Game/Hollow/Core/Input/DA_InputConfig_Raid"
INTERACT_TAG = "Hollow.Input.Interact"
INTERACT_KEYS = ["E", "Gamepad_FaceButton_Left"]
# GP-01 (ADR-0044): the gamepad's X is shared, a tap reloads (create_inventory_assets.py) and holding it interacts; E on
# press. Bound to ETriggerEvent::Triggered, so every mapping has its trigger: key -> (trigger class, its settings).
INTERACT_TRIGGERS = {"E": ("InputTriggerPressed", {}), "Gamepad_FaceButton_Left": ("InputTriggerHold", {"hold_time_threshold": 0.3, "is_one_shot": True})}

SWITCH_BP_PATH = "/Game/Hollow/World/Interactables/BP_Switch"
HUB_MAP = "/Game/Hollow/Maps/L_Hub_Dev"
CUBE_MESH = "/Engine/BasicShapes/Cube.Cube"

# Switch look (data; tune in the editor afterwards).
SWITCH_SCALE = unreal.Vector(0.25, 0.25, 0.25)            # 25 cm cube
LAMP_LOCAL_LOCATION = unreal.Vector(0.0, 0.0, 70.0)       # just above the cube (cube local extent is 50)
LAMP_COLOR = unreal.Color(r=40, g=255, b=90, a=255)
LAMP_INTENSITY = 800.0
LAMP_RADIUS = 250.0
PROMPT_ON = "Turn on"
PROMPT_OFF = "Turn off"

# Placement in L_Hub_Dev: in reach and at eye level for whoever spawns at each PlayerStart.
SWITCH_FORWARD = 150.0          # uu in front of the start (interaction range is 250)
SWITCH_HEIGHT_OVER_START = 60.0 # PlayerStart sits at capsule height; eyes are ~60 uu higher
SWITCH_LABEL = "Switch_%d"

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(message):
    unreal.log("create_interaction_assets: " + message)


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


def make_key(name):
    key = unreal.Key()
    if not key.import_text(name):
        raise RuntimeError("Unknown key " + name)
    return key


def make_tag(name):
    tag = unreal.GameplayTag()
    if not tag.import_text('(TagName="%s")' % name):
        raise RuntimeError("Unknown gameplay tag " + name)
    return tag


def same_tag(a, b):
    # GameplayTag structs do not compare by value in Python; their text form does.
    return a.export_text() == b.export_text()


def ensure_action():
    if assets.does_asset_exist(ACTION_PATH):
        log("IA_Interact exists, kept as is")
        return
    action = create(ACTION_PATH, unreal.InputAction, unreal.InputAction_Factory())
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    save(action)
    log("created IA_Interact")


def trigger_matches(entry, spec):
    """The mapping's triggers are exactly the spec's one, with its settings."""
    class_name, settings = spec
    triggers = list(entry.get_editor_property("triggers"))
    if len(triggers) != 1 or triggers[0] is None or triggers[0].get_class().get_name() != class_name:
        return False
    return all(abs(float(triggers[0].get_editor_property(k)) - float(v)) < 1e-4 if isinstance(v, float) else triggers[0].get_editor_property(k) == v
               for k, v in settings.items())


def make_trigger(context, spec):
    class_name, settings = spec
    trigger = unreal.new_object(getattr(unreal, class_name), outer=context)
    for key, value in settings.items():
        trigger.set_editor_property(key, value)
    return trigger


def ensure_mappings():
    context = load(IMC_PATH)
    action = load(ACTION_PATH)
    data = context.get_editor_property("default_key_mappings")
    mappings = list(data.get_editor_property("mappings"))
    own = [m for m in mappings if m.get_editor_property("action") == action]
    if sorted(m.get_editor_property("key").export_text() for m in own) == sorted(make_key(k).export_text() for k in INTERACT_KEYS) \
            and all(trigger_matches(m, INTERACT_TRIGGERS[str(m.get_editor_property("key").get_editor_property("key_name"))]) for m in own):
        return
    mappings = [m for m in mappings if m.get_editor_property("action") != action]
    for key_name in INTERACT_KEYS:
        entry = unreal.EnhancedActionKeyMapping()
        entry.set_editor_property("action", action)
        entry.set_editor_property("key", make_key(key_name))
        entry.set_editor_property("triggers", [make_trigger(context, INTERACT_TRIGGERS[key_name])])
        mappings.append(entry)
    data.set_editor_property("mappings", mappings)
    context.set_editor_property("default_key_mappings", data)
    save(context)
    log("mapped IA_Interact in IMC_Raid (E pressed, gamepad X held)")


def ensure_config_entry():
    config = load(INPUT_CONFIG_PATH)
    entries = list(config.get_editor_property("native_actions"))
    tag = make_tag(INTERACT_TAG)
    if any(same_tag(entry.get_editor_property("input_tag"), tag) for entry in entries):
        log("DA_InputConfig_Raid already has Interact, kept as is")
        return
    entry = unreal.HollowInputAction()
    entry.set_editor_property("input_action", load(ACTION_PATH))
    entry.set_editor_property("input_tag", tag)
    entries.append(entry)
    config.set_editor_property("native_actions", entries)
    save(config)
    log("added Interact to DA_InputConfig_Raid")


def ensure_switch_blueprint():
    if assets.does_asset_exist(SWITCH_BP_PATH):
        log("BP_Switch exists, kept as is")
        return
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.HollowSwitch)
    blueprint = create(SWITCH_BP_PATH, unreal.Blueprint, factory)
    switch = unreal.get_default_object(blueprint.generated_class())
    switch.set_editor_property("prompt_turn_on", PROMPT_ON)
    switch.set_editor_property("prompt_turn_off", PROMPT_OFF)

    mesh = switch.get_editor_property("mesh")
    mesh.set_editor_property("static_mesh", load(CUBE_MESH))
    mesh.set_editor_property("relative_scale3d", SWITCH_SCALE)

    lamp = switch.get_editor_property("lamp")
    lamp.set_editor_property("relative_location", LAMP_LOCAL_LOCATION)
    lamp.set_editor_property("light_color", LAMP_COLOR)
    lamp.set_editor_property("intensity", LAMP_INTENSITY)
    lamp.set_editor_property("attenuation_radius", LAMP_RADIUS)
    lamp.set_editor_property("cast_shadows", False)
    save(blueprint)
    log("created BP_Switch")


def ensure_hub_switches():
    if not levels.load_level(HUB_MAP):
        raise RuntimeError("Could not load " + HUB_MAP)
    level_actors = actors.get_all_level_actors()
    labels = {actor.get_actor_label() for actor in level_actors}
    starts = sorted((a for a in level_actors if isinstance(a, unreal.PlayerStart)), key=lambda a: a.get_actor_label())
    switch_class = load(SWITCH_BP_PATH).generated_class()

    placed = 0
    for index, start in enumerate(starts):
        label = SWITCH_LABEL % index
        if label in labels:
            continue
        rotation = start.get_actor_rotation()
        location = start.get_actor_location() + rotation.get_forward_vector() * SWITCH_FORWARD
        location.z += SWITCH_HEIGHT_OVER_START
        facing_start = unreal.Rotator(roll=0.0, pitch=0.0, yaw=rotation.yaw + 180.0)
        switch = actors.spawn_actor_from_class(switch_class, location, facing_start)
        if switch is None:
            raise RuntimeError("Could not place " + label)
        switch.set_actor_label(label)
        placed += 1

    if placed and not levels.save_current_level():
        raise RuntimeError("Could not save " + HUB_MAP)
    log("placed %d switch(es) in L_Hub_Dev" % placed)
    return len(starts)


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate(start_count):
    action = load(ACTION_PATH)
    mappings = load(IMC_PATH).get_editor_property("default_key_mappings").get_editor_property("mappings")
    check(sum(1 for e in mappings if e.get_editor_property("action") == action) >= len(INTERACT_KEYS), "IMC_Raid maps IA_Interact")
    for e in mappings:
        if e.get_editor_property("action") == action:
            key_name = str(e.get_editor_property("key").get_editor_property("key_name"))
            check(key_name in INTERACT_TRIGGERS and trigger_matches(e, INTERACT_TRIGGERS[key_name]), "IA_Interact on %s has its trigger" % key_name)
    entries = load(INPUT_CONFIG_PATH).get_editor_property("native_actions")
    tag = make_tag(INTERACT_TAG)
    check(sum(1 for e in entries if same_tag(e.get_editor_property("input_tag"), tag)) == 1, "input config has exactly one Interact entry")
    check(any(e.get_editor_property("input_action") == action for e in entries), "input config maps IA_Interact")
    check(isinstance(unreal.get_default_object(load(SWITCH_BP_PATH).generated_class()), unreal.HollowSwitch), "BP_Switch parent class")
    switches = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.HollowSwitch)]
    check(start_count > 0 and len(switches) >= start_count, "one switch per PlayerStart in L_Hub_Dev")
    log("validation passed (%d switches)" % len(switches))


def main():
    ensure_action()
    ensure_mappings()
    ensure_config_entry()
    ensure_switch_blueprint()
    validate(ensure_hub_switches())
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_interaction_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
