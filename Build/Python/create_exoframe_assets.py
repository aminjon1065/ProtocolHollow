"""Creates the S1-04 ExoFrame content (idempotent, ADR-0010, ADR-0011).

Creates only missing assets (never overwrites tuning done in the editor), then validates every asset.
Requires the placeholder characters: Build/Scripts/import-placeholders.sh

  /Game/Hollow/Core/Input/Actions/IA_Move|IA_Look|IA_Jump|IA_Crouch|IA_Sprint
  /Game/Hollow/Core/Input/IMC_Raid                         keyboard/mouse + gamepad
  /Game/Hollow/Core/Input/DA_InputConfig_Raid              Hollow.Input.* -> input action
  /Game/Hollow/Characters/ExoFrame/DA_MovementTuning_ExoFrame
  /Game/Hollow/Characters/ExoFrame/BP_ExoFrame             meshes, anims, camera, tuning, input
  /Game/Hollow/Core/Framework/BP_GameMode_Hub              default pawn = BP_ExoFrame

Run: Build/Scripts/run-editor-python.sh Build/Python/create_exoframe_assets.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

INPUT_DIR = "/Game/Hollow/Core/Input"
ACTIONS_DIR = INPUT_DIR + "/Actions"
EXOFRAME_DIR = "/Game/Hollow/Characters/ExoFrame"
FRAMEWORK_DIR = "/Game/Hollow/Core/Framework"

IMC_PATH = INPUT_DIR + "/IMC_Raid"
INPUT_CONFIG_PATH = INPUT_DIR + "/DA_InputConfig_Raid"
TUNING_PATH = EXOFRAME_DIR + "/DA_MovementTuning_ExoFrame"
EXOFRAME_BP_PATH = EXOFRAME_DIR + "/BP_ExoFrame"
GAME_MODE_BP_PATH = FRAMEWORK_DIR + "/BP_GameMode_Hub"

# Placeholders (ADR-0011): imported from the engine templates, not committed.
PLACEHOLDER_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
PLACEHOLDER_BODY_ANIM = "/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"
PLACEHOLDER_ARMS_ANIM = "/Game/FirstPerson/Anims/ABP_FP_Copy"

# name -> (value type, input tag)
ACTIONS = {
    "IA_Move": (unreal.InputActionValueType.AXIS2D, "Hollow.Input.Move"),
    "IA_Look": (unreal.InputActionValueType.AXIS2D, "Hollow.Input.Look"),
    "IA_Jump": (unreal.InputActionValueType.BOOLEAN, "Hollow.Input.Jump"),
    "IA_Crouch": (unreal.InputActionValueType.BOOLEAN, "Hollow.Input.Crouch"),
    "IA_Sprint": (unreal.InputActionValueType.BOOLEAN, "Hollow.Input.Sprint"),
}

# Initial data values (tune in the editor afterwards; this script never overwrites them).
GAMEPAD_LOOK_RATE = unreal.Vector(100.0, 100.0, 1.0)   # deg/s at full stick deflection
TUNING = {
    "walk_speed": 400.0, "crouch_speed": 180.0, "sprint_speed_multiplier": 1.6, "sprint_max_angle": 50.0,
    "max_acceleration": 1800.0, "braking_deceleration": 2000.0, "ground_friction": 8.0,
    "jump_z_velocity": 420.0, "air_control": 0.3, "braking_deceleration_falling": 1500.0,
}
# Character layout of the UE5 mannequin, as in the engine first-person template.
CAPSULE_RADIUS = 34.0
CAPSULE_HALF_HEIGHT = 96.0
BODY_LOCATION = unreal.Vector(0.0, 0.0, -CAPSULE_HALF_HEIGHT)
BODY_ROTATION = unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0)
CAMERA_LOCATION = unreal.Vector(-2.8, 5.89, 0.0)
CAMERA_ROTATION = unreal.Rotator(roll=-90.0, pitch=0.0, yaw=90.0)
FIRST_PERSON_FOV = 70.0
FIRST_PERSON_SCALE = 0.6

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_exoframe_assets: " + message)


def split(path):
    return path.rsplit("/", 1)


def create(path, asset_class, factory):
    package_path, name = split(path)
    asset = tools.create_asset(name, package_path, asset_class, factory)
    if asset is None:
        raise RuntimeError("Could not create " + path)
    return asset


def save(asset):
    if not assets.save_loaded_asset(asset, False):
        raise RuntimeError("Could not save " + asset.get_path_name())


def ensure(path, build):
    if assets.does_asset_exist(path):
        log("%s exists, kept as is" % path)
        return
    save(build(path))
    log("created " + path)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset " + path)
    return asset


def make_key(name):
    key = unreal.Key()
    if not key.import_text(name):
        raise RuntimeError("Unknown key " + name)
    return key


def make_tag(name):
    tag = unreal.GameplayTag()
    if not tag.import_text('(TagName="%s")' % name) or tag.export_text() != '(TagName="%s")' % name:
        raise RuntimeError("Unknown gameplay tag " + name)
    return tag


# --- builders ------------------------------------------------------------------------------------------------------

def build_action(value_type):
    def build(path):
        action = create(path, unreal.InputAction, unreal.InputAction_Factory())
        action.set_editor_property("value_type", value_type)
        return action
    return build


def build_mapping_context(path):
    context = create(path, unreal.InputMappingContext, unreal.InputMappingContext_Factory())

    def modifier(modifier_class, **properties):
        instance = unreal.new_object(modifier_class, outer=context)
        for name, value in properties.items():
            instance.set_editor_property(name, value)
        return instance

    def mapping(action_name, key_name, modifiers=()):
        entry = unreal.EnhancedActionKeyMapping()
        entry.set_editor_property("action", load(ACTIONS_DIR + "/" + action_name))
        entry.set_editor_property("key", make_key(key_name))
        entry.set_editor_property("modifiers", list(modifiers))
        return entry

    to_forward = lambda: modifier(unreal.InputModifierSwizzleAxis, order=unreal.InputAxisSwizzle.YXZ)
    negate_all = lambda: modifier(unreal.InputModifierNegate)
    negate_y = lambda: modifier(unreal.InputModifierNegate, x=False, y=True, z=False)
    dead_zone = lambda: modifier(unreal.InputModifierDeadZone)

    mappings = [
        mapping("IA_Move", "W", [to_forward()]),
        mapping("IA_Move", "S", [to_forward(), negate_all()]),
        mapping("IA_Move", "A", [negate_all()]),
        mapping("IA_Move", "D"),
        mapping("IA_Move", "Gamepad_Left2D", [dead_zone()]),
        mapping("IA_Look", "Mouse2D", [negate_y()]),
        mapping("IA_Look", "Gamepad_Right2D", [dead_zone(), negate_y(), modifier(unreal.InputModifierScaleByDeltaTime),
                                               modifier(unreal.InputModifierScalar, scalar=GAMEPAD_LOOK_RATE)]),
        mapping("IA_Jump", "SpaceBar"),
        mapping("IA_Jump", "Gamepad_FaceButton_Bottom"),
        mapping("IA_Crouch", "LeftControl"),
        mapping("IA_Crouch", "Gamepad_FaceButton_Right"),
        mapping("IA_Sprint", "LeftShift"),
        mapping("IA_Sprint", "Gamepad_LeftThumbstick"),
    ]
    data = unreal.InputMappingContextMappingData()
    data.set_editor_property("mappings", mappings)
    context.set_editor_property("default_key_mappings", data)
    return context


def build_input_config(path):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.HollowInputConfig)
    config = create(path, unreal.HollowInputConfig, factory)
    entries = []
    for action_name, (_, tag_name) in ACTIONS.items():
        entry = unreal.HollowInputAction()
        entry.set_editor_property("input_action", load(ACTIONS_DIR + "/" + action_name))
        entry.set_editor_property("input_tag", make_tag(tag_name))
        entries.append(entry)
    config.set_editor_property("native_actions", entries)
    return config


def build_tuning(path):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.HollowMovementTuning)
    tuning = create(path, unreal.HollowMovementTuning, factory)
    ground = unreal.HollowGroundSpeedTuning()
    for name in ("walk_speed", "crouch_speed", "sprint_speed_multiplier", "sprint_max_angle"):
        ground.set_editor_property(name, TUNING[name])
    tuning.set_editor_property("ground", ground)
    for name in ("max_acceleration", "braking_deceleration", "ground_friction", "jump_z_velocity", "air_control",
                 "braking_deceleration_falling"):
        tuning.set_editor_property(name, TUNING[name])
    return tuning


def build_blueprint(parent_class):
    def build(path):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        return create(path, unreal.Blueprint, factory)
    return build


def configure_exoframe(blueprint):
    mesh = load(PLACEHOLDER_MESH)
    character = unreal.get_default_object(blueprint.generated_class())
    character.set_editor_property("input_config", load(INPUT_CONFIG_PATH))
    character.set_editor_property("default_mapping_context", load(IMC_PATH))
    character.get_editor_property("character_movement").set_editor_property("tuning", load(TUNING_PATH))

    capsule = character.get_editor_property("capsule_component")
    capsule.set_editor_property("capsule_radius", CAPSULE_RADIUS)
    capsule.set_editor_property("capsule_half_height", CAPSULE_HALF_HEIGHT)

    body = character.get_editor_property("mesh")
    body.set_editor_property("skeletal_mesh_asset", mesh)
    body.set_editor_property("anim_class", load(PLACEHOLDER_BODY_ANIM).generated_class())
    body.set_editor_property("relative_location", BODY_LOCATION)
    body.set_editor_property("relative_rotation", BODY_ROTATION)

    arms = character.get_editor_property("first_person_mesh")
    arms.set_editor_property("skeletal_mesh_asset", mesh)
    arms.set_editor_property("anim_class", load(PLACEHOLDER_ARMS_ANIM).generated_class())

    camera = character.get_editor_property("first_person_camera")
    camera.set_editor_property("relative_location", CAMERA_LOCATION)
    camera.set_editor_property("relative_rotation", CAMERA_ROTATION)
    camera.set_editor_property("first_person_field_of_view", FIRST_PERSON_FOV)
    camera.set_editor_property("first_person_scale", FIRST_PERSON_SCALE)
    return blueprint


def build_exoframe(path):
    return configure_exoframe(build_blueprint(unreal.HollowExoFrameCharacter)(path))


def build_game_mode(path):
    blueprint = build_blueprint(unreal.HollowGameModeBase)(path)
    game_mode = unreal.get_default_object(blueprint.generated_class())
    game_mode.set_editor_property("default_pawn_class", load(EXOFRAME_BP_PATH).generated_class())
    return blueprint


# --- validation (reads what is on disk: run the script twice to check persistence) ---------------------------------

def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate():
    for action_name, (value_type, _) in ACTIONS.items():
        check(load(ACTIONS_DIR + "/" + action_name).get_editor_property("value_type") == value_type, action_name + " value type")

    mappings = load(IMC_PATH).get_editor_property("default_key_mappings").get_editor_property("mappings")
    mapped_actions = {entry.get_editor_property("action").get_name() for entry in mappings}
    check(mapped_actions == set(ACTIONS), "IMC_Raid maps every action")

    native = load(INPUT_CONFIG_PATH).get_editor_property("native_actions")
    check(len(native) == len(ACTIONS) and all(e.get_editor_property("input_action") for e in native), "input config entries")

    character = unreal.get_default_object(load(EXOFRAME_BP_PATH).generated_class())
    check(isinstance(character, unreal.HollowExoFrameCharacter), "BP_ExoFrame parent class")
    check(character.get_editor_property("input_config") is not None, "BP_ExoFrame input config")
    check(character.get_editor_property("default_mapping_context") is not None, "BP_ExoFrame mapping context")
    check(character.get_editor_property("character_movement").get_editor_property("tuning") is not None, "BP_ExoFrame tuning")
    for component in ("mesh", "first_person_mesh"):
        skeletal = character.get_editor_property(component)
        check(skeletal.get_editor_property("skeletal_mesh_asset") is not None, "BP_ExoFrame %s mesh" % component)
        check(skeletal.get_editor_property("anim_class") is not None, "BP_ExoFrame %s anim class" % component)

    game_mode = unreal.get_default_object(load(GAME_MODE_BP_PATH).generated_class())
    check(game_mode.get_editor_property("default_pawn_class") == load(EXOFRAME_BP_PATH).generated_class(), "hub default pawn")
    log("validation passed")


def main():
    for path in (PLACEHOLDER_MESH, PLACEHOLDER_BODY_ANIM, PLACEHOLDER_ARMS_ANIM):
        if not assets.does_asset_exist(path):
            raise RuntimeError("Placeholder %s missing: run Build/Scripts/import-placeholders.sh" % path)

    for action_name, (value_type, _) in ACTIONS.items():
        ensure(ACTIONS_DIR + "/" + action_name, build_action(value_type))
    ensure(IMC_PATH, build_mapping_context)
    ensure(INPUT_CONFIG_PATH, build_input_config)
    ensure(TUNING_PATH, build_tuning)
    ensure(EXOFRAME_BP_PATH, build_exoframe)
    ensure(GAME_MODE_BP_PATH, build_game_mode)
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_exoframe_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
