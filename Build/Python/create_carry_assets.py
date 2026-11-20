"""Creates the S3-02 heavy-carry content (idempotent, ADR-0010, ADR-0015).

  /Game/Hollow/GAS/Effects/GE_Carry_Solo                 solo carry: CarryingHeavy.Solo tag, no tools, stamina drain, Resonance
  /Game/Hollow/GAS/Effects/GE_Stamina_Recovery           passive stamina recovery, paused while carrying
  /Game/Hollow/Items/AetherCore/PM_AetherCore            physical material of the core: friction, sleep thresholds
  /Game/Hollow/Items/AetherCore/MI_AetherCore            greybox glow colour
  /Game/Hollow/Items/AetherCore/DA_Carryable_AetherCore  UHollowCarryableDefinition: grip, FP/TP attachment, physics, timing
  /Game/Hollow/Items/AetherCore/BP_AetherCore            AHollowAetherCore: box body, cylinder visual
  DA_MovementTuning_ExoFrame                             carry slowdown per mode (Solo, Coop, Dragged)
  DA_AbilitySet_ExoFrame                                 grants GE_Stamina_Recovery
  /HollowSector_Docks/Data/DA_Biome_Docks                socket rule CoreSpawn -> BP_AetherCore (replaces the marker)

The third-person attachment is computed from BP_ExoFrame's body mesh, so the visual sits where the server body rides.
Creates or fills only what is missing, then validates.
Run after create_exoframe_assets.py, create_gas_assets.py and create_raid_assets.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_carry_assets.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

EFFECTS_DIR = "/Game/Hollow/GAS/Effects"
CARRY_EFFECT = EFFECTS_DIR + "/GE_Carry_Solo"
RECOVERY_EFFECT = EFFECTS_DIR + "/GE_Stamina_Recovery"
CORE_DIR = "/Game/Hollow/Items/AetherCore"
CORE_MATERIAL = CORE_DIR + "/PM_AetherCore"
CORE_LOOK = CORE_DIR + "/MI_AetherCore"
CORE_DEFINITION = CORE_DIR + "/DA_Carryable_AetherCore"
CORE_BP = CORE_DIR + "/BP_AetherCore"
EXOFRAME_BP = "/Game/Hollow/Characters/ExoFrame/BP_ExoFrame"
TUNING_PATH = "/Game/Hollow/Characters/ExoFrame/DA_MovementTuning_ExoFrame"
ABILITY_SET_PATH = "/Game/Hollow/Characters/ExoFrame/DA_AbilitySet_ExoFrame"
BIOME_PATH = "/HollowSector_Docks/Data/DA_Biome_Docks"
MARKER_BP_PATH = "/Game/Hollow/World/Sector/BP_SectorMarker"
CORE_SOCKET = "Hollow.Socket.CoreSpawn"

ATTRIBUTE_MODULE = "/Script/HollowGAS"
CYLINDER_MESH = "/Engine/BasicShapes/Cylinder.Cylinder"
BASIC_MATERIAL = "/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"
COLOR_PARAMETER = "Color"

# Core (data). A canister 50 x 50 x 70 cm; the cylinder mesh is 100 x 100 x 100 around its centre.
CORE_HALF = unreal.Vector(25.0, 25.0, 35.0)
CYLINDER_SIZE = 100.0
CORE_COLOR = unreal.LinearColor(0.1, 0.75, 1.0, 1.0)
CORE_MASS = 120.0                   # kg
CORE_FRICTION = 0.9
CORE_RESTITUTION = 0.05
CORE_SLEEP = {"sleep_linear_velocity_threshold": 5.0, "sleep_angular_velocity_threshold": 0.1, "sleep_counter_threshold": 4}
LINEAR_DAMPING = 0.1
ANGULAR_DAMPING = 1.0
INHERIT_VELOCITY = 0.5              # share of the carrier's velocity kept on a drop
AWAKE_NET_RATE = 20.0               # Hz while a dropped core is awake (Docs/Architecture/03 sec. 3.0: 10-20 Hz)
DORMANCY_DELAY = 0.5                # s asleep before it settles (clients converge on the rest pose meanwhile)
MAX_AWAKE = 6.0                     # s: a dropped core sleeps by then at the latest
PROMPT = "Lift the Aether core"

# Grip: in front of the capsule at hand height (capsule space); the own view holds it low in front of the camera.
GRIP_GAP = 10.0                     # cm between the capsule and the core
GRIP_HEIGHT = 5.0                   # cm above the capsule centre
FIRST_PERSON_OFFSET = unreal.Vector(70.0, 0.0, -45.0)   # camera space: forward, right, up

# Carry movement per mode (data; Docs/Architecture/03 sec. 3.5: solo 0.45, coop 0.75).
CARRY_MODES = {
    "solo": {"speed_scale": 0.45, "acceleration_scale": 0.6, "allow_sprint": False, "allow_jump": False},
    "coop": {"speed_scale": 0.75, "acceleration_scale": 0.8, "allow_sprint": False, "allow_jump": False},
    "dragged": {"speed_scale": 0.25, "acceleration_scale": 0.5, "allow_sprint": False, "allow_jump": False},
}

# Effects (data). MaxStamina and MaxResonance are 100 (GE_ExoFrame_InitAttributes).
CARRY_PERIOD = 1.0                  # s
STAMINA_DRAIN_PER_SECOND = 2.0
RESONANCE_PER_SECOND = 0.5          # flat for now; the gain multiplier execution arrives in S4-02
RECOVERY_PERIOD = 0.5               # s
STAMINA_RECOVERY_PER_SECOND = 5.0

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_carry_assets: " + message)


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


def scalable(value):
    return "(MagnitudeCalculationType=ScalableFloat,ScalableFloatMagnitude=(Value=%.6f))" % value


def modifier(attribute_set, attribute, magnitude_text):
    path = "%s.%s:%s" % (ATTRIBUTE_MODULE, attribute_set, attribute)
    text = ('(Attribute=(AttributeName="{attr}",Attribute={path},AttributeOwner="/Script/CoreUObject.Class\'{module}.{set}\'"),'
            'ModifierOp=Additive,ModifierMagnitude={magnitude})').format(attr=attribute, path=path, module=ATTRIBUTE_MODULE, set=attribute_set,
                                                                     magnitude=magnitude_text)
    result = from_text(unreal.GameplayModifierInfo(), text)
    if path not in result.export_text():
        raise RuntimeError("Modifier on %s did not import" % attribute)
    return result


# ---------------------------------------------------------------- effects

def ensure_effect(path, parent, period, modifiers):
    effect_bp, created = blueprint(path, parent)
    if created:
        effect = unreal.get_default_object(effect_bp.generated_class())
        effect.set_editor_property("period", from_text(unreal.ScalableFloat(), "(Value=%.6f)" % period))
        effect.set_editor_property("modifiers", modifiers)
        save(effect_bp)
        log("created " + path.rsplit("/", 1)[1])
    return effect_bp.generated_class()


def ensure_effects():
    carry = ensure_effect(CARRY_EFFECT, unreal.HollowCarrySoloEffect, CARRY_PERIOD, [
        modifier("HollowSurvivalSet", "Stamina", scalable(-STAMINA_DRAIN_PER_SECOND * CARRY_PERIOD)),
        modifier("HollowResonanceSet", "Resonance", scalable(RESONANCE_PER_SECOND * CARRY_PERIOD)),
    ])
    recovery = ensure_effect(RECOVERY_EFFECT, unreal.HollowStaminaRecoveryEffect, RECOVERY_PERIOD, [
        modifier("HollowSurvivalSet", "Stamina", scalable(STAMINA_RECOVERY_PER_SECOND * RECOVERY_PERIOD)),
    ])
    return carry, recovery


def ensure_ability_set(recovery):
    ability_set = load(ABILITY_SET_PATH)
    effects = list(ability_set.get_editor_property("granted_effects"))
    if any(e.get_editor_property("gameplay_effect") == recovery for e in effects):
        return
    entry = unreal.HollowAbilitySetEffect()
    entry.set_editor_property("gameplay_effect", recovery)
    effects.append(entry)
    ability_set.set_editor_property("granted_effects", effects)
    save(ability_set)
    log("granted GE_Stamina_Recovery in DA_AbilitySet_ExoFrame")


# ---------------------------------------------------------------- movement

def carry_tuning():
    tuning = unreal.HollowCarryMovementTuning()
    for mode, values in CARRY_MODES.items():
        entry = unreal.HollowCarryModeTuning()
        for key, value in values.items():
            entry.set_editor_property(key, value)
        tuning.set_editor_property(mode, entry)
    return tuning


def ensure_movement_tuning():
    tuning = load(TUNING_PATH)
    wanted = carry_tuning()
    if tuning.get_editor_property("carry").export_text() == wanted.export_text():
        return
    tuning.set_editor_property("carry", wanted)
    save(tuning)
    log("DA_MovementTuning_ExoFrame: carry slowdown per mode")


# ---------------------------------------------------------------- core

def ensure_physical_material():
    if assets.does_asset_exist(CORE_MATERIAL):
        return
    material = create(CORE_MATERIAL, unreal.PhysicalMaterial, unreal.PhysicalMaterialFactoryNew())
    for key, value in CORE_SLEEP.items():
        material.set_editor_property(key, value)
    material.set_editor_property("friction", CORE_FRICTION)
    material.set_editor_property("restitution", CORE_RESTITUTION)
    save(material)
    log("created PM_AetherCore")


def ensure_look():
    if assets.does_asset_exist(CORE_LOOK):
        return
    instance = create(CORE_LOOK, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, load(BASIC_MATERIAL))
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance, COLOR_PARAMETER, CORE_COLOR)
    save(instance)
    log("created MI_AetherCore")


def grip_offset():
    """Core centre in the carrier's capsule space: in front of the capsule, at hand height."""
    exoframe = unreal.get_default_object(load(EXOFRAME_BP).generated_class())
    radius = exoframe.get_editor_property("capsule_component").get_editor_property("capsule_radius")
    return unreal.Transform(location=unreal.Vector(radius + CORE_HALF.x + GRIP_GAP, 0.0, GRIP_HEIGHT))


def third_person_offset(grip):
    """The same frame in the body mesh's space, so the smoothed body carries the visual exactly where the server body rides."""
    exoframe = unreal.get_default_object(load(EXOFRAME_BP).generated_class())
    body = exoframe.get_editor_property("mesh")
    mesh = unreal.Transform(location=body.get_editor_property("relative_location"), rotation=body.get_editor_property("relative_rotation"),
                            scale=body.get_editor_property("relative_scale3d"))
    return unreal.MathLibrary.make_relative_transform(grip, mesh)


def make_attach(offset):
    attach = unreal.HollowCarryAttach()
    attach.set_editor_property("socket", "None")
    attach.set_editor_property("offset", offset)
    return attach


def ensure_definition(carry_effect):
    if assets.does_asset_exist(CORE_DEFINITION):
        return
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.HollowCarryableDefinition)
    definition = create(CORE_DEFINITION, unreal.HollowCarryableDefinition, factory)
    grip = grip_offset()
    definition.set_editor_property("pickup_prompt", PROMPT)
    definition.set_editor_property("grip_offset", grip)
    definition.set_editor_property("first_person", make_attach(unreal.Transform(location=FIRST_PERSON_OFFSET)))
    definition.set_editor_property("third_person", make_attach(third_person_offset(grip)))
    definition.set_editor_property("solo_effect", carry_effect)
    definition.set_editor_property("mass", CORE_MASS)
    definition.set_editor_property("physical_material", load(CORE_MATERIAL))
    definition.set_editor_property("linear_damping", LINEAR_DAMPING)
    definition.set_editor_property("angular_damping", ANGULAR_DAMPING)
    definition.set_editor_property("inherit_velocity_fraction", INHERIT_VELOCITY)
    definition.set_editor_property("awake_net_update_frequency", AWAKE_NET_RATE)
    definition.set_editor_property("dormancy_delay_seconds", DORMANCY_DELAY)
    definition.set_editor_property("max_awake_seconds", MAX_AWAKE)
    save(definition)
    log("created DA_Carryable_AetherCore")


def ensure_blueprint():
    core_bp, created = blueprint(CORE_BP, unreal.HollowAetherCore)
    if not created:
        return
    core = unreal.get_default_object(core_bp.generated_class())
    core.get_editor_property("body").set_editor_property("box_extent", CORE_HALF)
    visual = core.get_editor_property("visual")
    visual.set_editor_property("static_mesh", load(CYLINDER_MESH))
    visual.set_material(0, load(CORE_LOOK))
    visual.set_editor_property("relative_scale3d", unreal.Vector(CORE_HALF.x * 2.0 / CYLINDER_SIZE, CORE_HALF.y * 2.0 / CYLINDER_SIZE,
                                                                   CORE_HALF.z * 2.0 / CYLINDER_SIZE))
    core.get_editor_property("carryable").set_editor_property("definition", load(CORE_DEFINITION))
    save(core_bp)
    log("created BP_AetherCore")


def ensure_biome_rule():
    biome = load(BIOME_PATH)
    core_class = load(CORE_BP).generated_class()
    offset = unreal.Vector(0.0, 0.0, CORE_HALF.z)   # the body rests on the floor socket
    tag = make_tag(CORE_SOCKET)
    rules = list(biome.get_editor_property("socket_actors"))
    matches = [r for r in rules if r.get_editor_property("socket_type").export_text() == tag.export_text()]
    rule = matches[0] if matches else unreal.HollowSocketActorRule()
    current = rule.get_editor_property("actor_class")
    if current == core_class and rule.get_editor_property("spawn_offset").export_text() == offset.export_text():
        return
    if matches and current not in (None, load(MARKER_BP_PATH).generated_class(), core_class):
        log("CoreSpawn rule points to a custom class; kept as is")
        return
    rule.set_editor_property("socket_type", tag)
    rule.set_editor_property("actor_class", core_class)
    rule.set_editor_property("spawn_offset", offset)
    if not matches:
        rules.append(rule)
    biome.set_editor_property("socket_actors", rules)
    save(biome)
    log("DA_Biome_Docks: CoreSpawn sockets now spawn BP_AetherCore")


# ---------------------------------------------------------------- validation

def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate(carry_effect, recovery):
    core = unreal.get_default_object(load(CORE_BP).generated_class())
    definition = core.get_editor_property("carryable").get_editor_property("definition")
    check(definition == load(CORE_DEFINITION), "BP_AetherCore uses DA_Carryable_AetherCore")
    check(core.get_editor_property("visual").get_editor_property("static_mesh") is not None, "BP_AetherCore visual mesh")
    check(core.get_editor_property("body").get_editor_property("box_extent").export_text() == CORE_HALF.export_text(), "BP_AetherCore body size")
    check(definition.get_editor_property("solo_effect") == carry_effect, "the core applies GE_Carry_Solo")
    check(definition.get_editor_property("physical_material") is not None, "the core's physical material")
    grip = definition.get_editor_property("grip_offset")
    check(grip.translation.x > CORE_HALF.x, "the grip holds the core clear of the capsule")
    body = unreal.get_default_object(load(EXOFRAME_BP).generated_class()).get_editor_property("mesh")
    mesh = unreal.Transform(location=body.get_editor_property("relative_location"), rotation=body.get_editor_property("relative_rotation"),
                            scale=body.get_editor_property("relative_scale3d"))
    shown = unreal.MathLibrary.compose_transforms(definition.get_editor_property("third_person").get_editor_property("offset"), mesh)
    check((shown.translation - grip.translation).length() < 0.1, "the third-person visual sits where the server body rides")
    check(len(unreal.get_default_object(carry_effect).get_editor_property("modifiers")) == 2, "GE_Carry_Solo drains stamina and adds Resonance")
    tuning = load(TUNING_PATH).get_editor_property("carry")
    check(abs(tuning.get_editor_property("solo").get_editor_property("speed_scale") - CARRY_MODES["solo"]["speed_scale"]) < 1e-4, "solo speed scale")
    effects = load(ABILITY_SET_PATH).get_editor_property("granted_effects")
    check(any(e.get_editor_property("gameplay_effect") == recovery for e in effects), "the ExoFrame recovers stamina")
    tag = make_tag(CORE_SOCKET).export_text()
    rules = load(BIOME_PATH).get_editor_property("socket_actors")
    check(any(r.get_editor_property("socket_type").export_text() == tag and r.get_editor_property("actor_class") == load(CORE_BP).generated_class()
              for r in rules), "biome CoreSpawn rule spawns BP_AetherCore")
    log("validation passed")


def main():
    carry_effect, recovery = ensure_effects()
    ensure_ability_set(recovery)
    ensure_movement_tuning()
    ensure_physical_material()
    ensure_look()
    ensure_definition(carry_effect)
    ensure_blueprint()
    ensure_biome_rule()
    validate(carry_effect, recovery)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_carry_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
