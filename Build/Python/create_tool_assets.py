"""Creates the S2-03 continuous tool content (idempotent, ADR-0010).

  /Game/Hollow/Core/Input/Actions/IA_PrimaryUse|IA_SecondaryUse   + IMC_Raid mappings, DA_InputConfig_Raid ability actions
  /Game/Hollow/GAS/Effects/GE_Tool_FuelDrain|Heat|Overheat|Cooling  fuel and heat effects (numbers are data here)
  /Game/Hollow/Tools/<Tool>/DA_Tool_<Tool>, GA_Tool_<Tool>          tool definitions and abilities (Welder, PlasmaCutter)
  /Game/Hollow/GAS/Cues/GCN_Tool_Beam                               greybox beam cue (GameplayCue.Hollow.Tool.Beam)
  /Game/Hollow/World/Interactables/BP_ToolTestTarget                 dev target, placed between the hub's player starts
  /Game/Hollow/Tools/<Tool>/DA_AbilitySet_<Tool>                    the tool on its input (Cutter: PrimaryUse, Welder: SecondaryUse),
                                                                    granted by the tool's item while it is in the raid inventory
                                                                    (ADR-0030; create_inventory_assets.py links the items)
  DA_AbilitySet_ExoFrame                                            cooling only (the tools left it in S5-01)
  BP_ExoFrame                                                       the cutter in hand at spawn (UHollowToolMountComponent, ADR-0027;
                                                                    held meshes and muzzle sockets are DA_Tool_* data of the art pass)

Creates or adds only what is missing, then validates.
Run after create_exoframe_assets.py and create_gas_assets.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_tool_assets.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

ACTIONS_DIR = "/Game/Hollow/Core/Input/Actions"
IMC_PATH = "/Game/Hollow/Core/Input/IMC_Raid"
INPUT_CONFIG_PATH = "/Game/Hollow/Core/Input/DA_InputConfig_Raid"
EFFECTS_DIR = "/Game/Hollow/GAS/Effects"
CUE_PATH = "/Game/Hollow/GAS/Cues/GCN_Tool_Beam"
ABILITY_SET_PATH = "/Game/Hollow/Characters/ExoFrame/DA_AbilitySet_ExoFrame"
EXOFRAME_BP = "/Game/Hollow/Characters/ExoFrame/BP_ExoFrame"
DEFAULT_HELD_TOOL = "/Game/Hollow/Tools/PlasmaCutter/DA_Tool_PlasmaCutter"
TARGET_BP_PATH = "/Game/Hollow/World/Interactables/BP_ToolTestTarget"
HUB_MAP = "/Game/Hollow/Maps/L_Hub_Dev"

ATTRIBUTE_MODULE = "/Script/HollowGAS"
CUBE_MESH = "/Engine/BasicShapes/Cube.Cube"
CYLINDER_MESH = "/Engine/BasicShapes/Cylinder.Cylinder"
BEAM_MATERIAL = "/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"

# Ability inputs: (action, input tag, keys)
ABILITY_INPUTS = [
    ("IA_PrimaryUse", "Hollow.Input.PrimaryUse", ["LeftMouseButton", "Gamepad_RightTrigger"]),
    ("IA_SecondaryUse", "Hollow.Input.SecondaryUse", ["RightMouseButton", "Gamepad_LeftTrigger"]),
]

# Fuel and heat (data). MaxToolFuel / MaxToolHeat are 100 (GE_ExoFrame_InitAttributes).
EFFECT_PERIOD = 0.25          # s, periodic fuel drain and cooling
COOLING_PER_SECOND = 10.0     # always on; tools out-heat it while running
OVERHEAT_SECONDS = 4.0        # tools are blocked this long after reaching MaxToolHeat

TOOLS = {
    "Welder": {
        "dir": "/Game/Hollow/Tools/Welder", "asset": "Welder", "tag": "Hollow.Ability.Tool.Welder", "input": "Hollow.Input.SecondaryUse",
        "work_rate": 10.0, "fuel_per_second": 4.0, "heat_per_sample": 1.5,
        "noise_tag": "Hollow.Noise.Tool.Welder",
        "beam_color": unreal.LinearColor(1.0, 0.55, 0.15, 1.0), "beam_width": 2.0,
    },
    "PlasmaCutter": {
        "dir": "/Game/Hollow/Tools/PlasmaCutter", "asset": "PlasmaCutter", "tag": "Hollow.Ability.Tool.PlasmaCutter", "input": "Hollow.Input.PrimaryUse",
        "work_rate": 10.0, "fuel_per_second": 5.0, "heat_per_sample": 2.0,
        "noise_tag": "Hollow.Noise.Tool.Cutter",
        "beam_color": unreal.LinearColor(0.25, 0.6, 1.0, 1.0), "beam_width": 3.0,
    },
}
COMMON_TOOL = {
    "sample_rate_hz": 10.0, "range": 250.0, "hit_tolerance_radius": 10.0,
    "sticky_angle_degrees": 15.0, "sticky_grace_seconds": 0.3, "min_fuel_to_start": 1.0,
}

# Beam cue (greybox)
BEAM_UPDATE_HZ = 30.0
BEAM_COLOR_PARAMETER = "Color"
BEAM_MUZZLE_OFFSET = unreal.Vector(30.0, 15.0, -15.0)   # forward, right, down from the eyes
GLOW_INTENSITY = 20.0
GLOW_RADIUS = 150.0

# Dev targets between the hub's player starts, at eye height.
TARGET_SCALE = unreal.Vector(0.5, 0.5, 0.5)
TARGET_HEIGHT_OVER_START = 60.0
TARGET_LABEL = "ToolTarget_%d"

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(message):
    unreal.log("create_tool_assets: " + message)


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


def make_tags(*names):
    return from_text(unreal.GameplayTagContainer(), "(GameplayTags=(%s))" % ",".join('(TagName="%s")' % n for n in names))


def same_tag(a, b):
    return a.export_text() == b.export_text()


def modifier(attribute, magnitude_text):
    path = "%s.HollowToolSet:%s" % (ATTRIBUTE_MODULE, attribute)
    text = ('(Attribute=(AttributeName="{attr}",Attribute={path},AttributeOwner="/Script/CoreUObject.Class\'{module}.HollowToolSet\'"),'
            'ModifierOp=Additive,ModifierMagnitude={magnitude})').format(attr=attribute, path=path, module=ATTRIBUTE_MODULE, magnitude=magnitude_text)
    result = from_text(unreal.GameplayModifierInfo(), text)
    if path not in result.export_text():
        raise RuntimeError("Modifier on %s did not import" % attribute)
    return result


def set_by_caller(tag):
    return '(MagnitudeCalculationType=SetByCaller,SetByCallerMagnitude=(DataTag=(TagName="%s")))' % tag


def scalable(value):
    return "(MagnitudeCalculationType=ScalableFloat,ScalableFloatMagnitude=(Value=%.6f))" % value


# ---------------------------------------------------------------- input

def ensure_inputs():
    context = load(IMC_PATH)
    data = context.get_editor_property("default_key_mappings")
    mappings = list(data.get_editor_property("mappings"))
    config = load(INPUT_CONFIG_PATH)
    entries = list(config.get_editor_property("ability_actions"))
    changed_imc = changed_config = False
    for action_name, tag_name, keys in ABILITY_INPUTS:
        path = ACTIONS_DIR + "/" + action_name
        if not assets.does_asset_exist(path):
            action = create(path, unreal.InputAction, unreal.InputAction_Factory())
            action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
            save(action)
            log("created " + action_name)
        action = load(path)
        if not any(m.get_editor_property("action") == action for m in mappings):
            for key_name in keys:
                entry = unreal.EnhancedActionKeyMapping()
                entry.set_editor_property("action", action)
                entry.set_editor_property("key", from_text(unreal.Key(), key_name))
                mappings.append(entry)
            changed_imc = True
        tag = make_tag(tag_name)
        if not any(same_tag(e.get_editor_property("input_tag"), tag) for e in entries):
            entry = unreal.HollowInputAction()
            entry.set_editor_property("input_action", action)
            entry.set_editor_property("input_tag", tag)
            entries.append(entry)
            changed_config = True
    if changed_imc:
        data.set_editor_property("mappings", mappings)
        context.set_editor_property("default_key_mappings", data)
        save(context)
        log("mapped tool inputs in IMC_Raid")
    if changed_config:
        config.set_editor_property("ability_actions", entries)
        save(config)
        log("added ability actions to DA_InputConfig_Raid")


# ---------------------------------------------------------------- effects

def ensure_effect(name, parent, configure):
    path = EFFECTS_DIR + "/" + name
    effect_bp, created = blueprint(path, parent)
    if created:
        configure(unreal.get_default_object(effect_bp.generated_class()))
        save(effect_bp)
        log("created " + name)
    return effect_bp.generated_class()


def configure_fuel_drain(effect):
    effect.set_editor_property("duration_policy", unreal.GameplayEffectDurationType.INFINITE)
    effect.set_editor_property("period", from_text(unreal.ScalableFloat(), "(Value=%.6f)" % EFFECT_PERIOD))
    effect.set_editor_property("modifiers", [modifier("ToolFuel", set_by_caller("Hollow.Data.FuelCost"))])


def configure_heat(effect):
    effect.set_editor_property("duration_policy", unreal.GameplayEffectDurationType.INSTANT)
    effect.set_editor_property("modifiers", [modifier("ToolHeat", set_by_caller("Hollow.Data.HeatGain"))])


def configure_overheat(effect):
    effect.set_editor_property("duration_magnitude", from_text(unreal.GameplayEffectModifierMagnitude(), scalable(OVERHEAT_SECONDS)))


def configure_cooling(effect):
    effect.set_editor_property("duration_policy", unreal.GameplayEffectDurationType.INFINITE)
    effect.set_editor_property("period", from_text(unreal.ScalableFloat(), "(Value=%.6f)" % EFFECT_PERIOD))
    effect.set_editor_property("modifiers", [modifier("ToolHeat", scalable(-COOLING_PER_SECOND * EFFECT_PERIOD))])


# ---------------------------------------------------------------- tools

def ensure_tool(spec, effects):
    definition_path = "%s/DA_Tool_%s" % (spec["dir"], spec["asset"])
    if not assets.does_asset_exist(definition_path):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.HollowToolDefinition)
        definition = create(definition_path, unreal.HollowToolDefinition, factory)
        definition.set_editor_property("tool_tag", make_tag(spec["tag"]))
        for key, value in COMMON_TOOL.items():
            definition.set_editor_property(key, value)
        for key in ("work_rate", "fuel_per_second", "heat_per_sample", "beam_color", "beam_width"):
            definition.set_editor_property(key, spec[key])
        definition.set_editor_property("noise_tag", make_tag(spec["noise_tag"]))
        definition.set_editor_property("fuel_drain_effect", effects["fuel"])
        definition.set_editor_property("heat_effect", effects["heat"])
        definition.set_editor_property("overheat_effect", effects["overheat"])
        save(definition)
        log("created DA_Tool_" + spec["asset"])

    ability_bp, created = blueprint("%s/GA_Tool_%s" % (spec["dir"], spec["asset"]), unreal.HollowGA_ContinuousTool)
    if created:
        ability = unreal.get_default_object(ability_bp.generated_class())
        ability.set_editor_property("tool_definition", load(definition_path))
        ability.set_editor_property("ability_tags", make_tags(spec["tag"]))
        save(ability_bp)
        log("created GA_Tool_" + spec["asset"])
    return ability_bp.generated_class()


def tool_set_path(spec):
    return "%s/DA_AbilitySet_%s" % (spec["dir"], spec["asset"])


def ensure_ability_set(abilities, cooling):
    """Cooling stays with the suit; the tools come from their items (ADR-0030), so the suit's set no longer grants them."""
    ability_set = load(ABILITY_SET_PATH)
    tool_classes = [ability_class for ability_class, _ in abilities]
    granted = list(ability_set.get_editor_property("granted_abilities"))
    kept = [e for e in granted if e.get_editor_property("ability") not in tool_classes]
    effects = list(ability_set.get_editor_property("granted_effects"))
    changed = len(kept) != len(granted)
    if not any(e.get_editor_property("gameplay_effect") == cooling for e in effects):
        entry = unreal.HollowAbilitySetEffect()
        entry.set_editor_property("gameplay_effect", cooling)
        effects.append(entry)
        changed = True
    if changed:
        ability_set.set_editor_property("granted_abilities", kept)
        ability_set.set_editor_property("granted_effects", effects)
        save(ability_set)
        log("DA_AbilitySet_ExoFrame: cooling, no tools")


def ensure_tool_set(spec, ability_class):
    """The tool's own set: its ability on its input, nothing else."""
    path = tool_set_path(spec)
    if assets.does_asset_exist(path):
        tool_set = load(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.HollowAbilitySet)
        tool_set = create(path, unreal.HollowAbilitySet, factory)
    granted = tool_set.get_editor_property("granted_abilities")
    if len(granted) == 1 and granted[0].get_editor_property("ability") == ability_class \
            and same_tag(granted[0].get_editor_property("input_tag"), make_tag(spec["input"])):
        return
    entry = unreal.HollowAbilitySetAbility()
    entry.set_editor_property("ability", ability_class)
    entry.set_editor_property("input_tag", make_tag(spec["input"]))
    tool_set.set_editor_property("granted_abilities", [entry])
    save(tool_set)
    log("DA_AbilitySet_%s: GA_Tool_%s on %s" % (spec["asset"], spec["asset"], spec["input"]))


# ---------------------------------------------------------------- cue and dev target

def ensure_cue():
    cue_bp, created = blueprint(CUE_PATH, unreal.HollowToolBeamCue)
    if not created:
        return
    cue = unreal.get_default_object(cue_bp.generated_class())
    cue.set_editor_property("gameplay_cue_tag", make_tag("GameplayCue.Hollow.Tool.Beam"))
    cue.set_editor_property("update_rate_hz", BEAM_UPDATE_HZ)
    cue.set_editor_property("muzzle_offset", BEAM_MUZZLE_OFFSET)
    material = load(BEAM_MATERIAL)
    names = unreal.MaterialEditingLibrary.get_vector_parameter_names(material)
    if BEAM_COLOR_PARAMETER in [str(n) for n in names]:
        cue.set_editor_property("color_parameter", BEAM_COLOR_PARAMETER)
    else:
        log("beam material has no '%s' parameter; the beam keeps its colour" % BEAM_COLOR_PARAMETER)
    beam = cue.get_editor_property("beam")
    beam.set_editor_property("static_mesh", load(CYLINDER_MESH))
    beam.set_material(0, material)
    glow = cue.get_editor_property("glow")
    glow.set_editor_property("intensity", GLOW_INTENSITY)
    glow.set_editor_property("attenuation_radius", GLOW_RADIUS)
    save(cue_bp)
    log("created GCN_Tool_Beam")


def ensure_target():
    target_bp, created = blueprint(TARGET_BP_PATH, unreal.HollowToolTestTarget)
    if created:
        mesh = unreal.get_default_object(target_bp.generated_class()).get_editor_property("mesh")
        mesh.set_editor_property("static_mesh", load(CUBE_MESH))
        mesh.set_editor_property("relative_scale3d", TARGET_SCALE)
        save(target_bp)
        log("created BP_ToolTestTarget")

    if not levels.load_level(HUB_MAP):
        raise RuntimeError("Could not load " + HUB_MAP)
    level_actors = actors.get_all_level_actors()
    labels = {a.get_actor_label() for a in level_actors}
    starts = sorted((a.get_actor_location() for a in level_actors if isinstance(a, unreal.PlayerStart)), key=lambda v: (v.x, v.y))
    placed = 0
    for index in range(len(starts) - 1):
        label = TARGET_LABEL % index
        if label in labels:
            continue
        a, b = starts[index], starts[index + 1]
        location = unreal.Vector((a.x + b.x) / 2.0, (a.y + b.y) / 2.0, (a.z + b.z) / 2.0 + TARGET_HEIGHT_OVER_START)
        target = actors.spawn_actor_from_class(target_bp.generated_class(), location, unreal.Rotator(0.0, 0.0, 0.0))
        target.set_actor_label(label)
        placed += 1
    if placed and not levels.save_current_level():
        raise RuntimeError("Could not save " + HUB_MAP)
    log("placed %d tool target(s) in L_Hub_Dev" % placed)
    return len(starts) - 1


# ---------------------------------------------------------------- validation

def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def ensure_tool_mount():
    """ADR-0027: the tool the ExoFrame holds before it uses one (its mesh, if any, is the tool's data)."""
    exo_bp = load(EXOFRAME_BP)
    mount = unreal.get_default_object(exo_bp.generated_class()).get_editor_property("tool_mount")
    tool = load(DEFAULT_HELD_TOOL)
    if mount.get_editor_property("default_tool") != tool:
        mount.set_editor_property("default_tool", tool)
        save(exo_bp)
        log("BP_ExoFrame: holds DA_Tool_PlasmaCutter at spawn")


def validate(abilities, expected_targets):
    config = load(INPUT_CONFIG_PATH).get_editor_property("ability_actions")
    for _, tag_name, _ in ABILITY_INPUTS:
        check(sum(1 for e in config if same_tag(e.get_editor_property("input_tag"), make_tag(tag_name))) == 1, "one ability action for " + tag_name)
    granted = load(ABILITY_SET_PATH).get_editor_property("granted_abilities")
    for (ability_class, input_tag), spec in zip(abilities, TOOLS.values()):
        check(not any(e.get_editor_property("ability") == ability_class for e in granted), "the suit does not grant " + ability_class.get_name())
        tool_set = load(tool_set_path(spec)).get_editor_property("granted_abilities")
        check(len(tool_set) == 1 and tool_set[0].get_editor_property("ability") == ability_class
              and same_tag(tool_set[0].get_editor_property("input_tag"), make_tag(input_tag)), "DA_AbilitySet_%s grants its tool" % spec["asset"])
    cue = unreal.get_default_object(load(CUE_PATH).generated_class())
    check(same_tag(cue.get_editor_property("gameplay_cue_tag"), make_tag("GameplayCue.Hollow.Tool.Beam")), "beam cue tag")
    mount = unreal.get_default_object(load(EXOFRAME_BP).generated_class()).get_editor_property("tool_mount")
    check(mount.get_editor_property("default_tool") == load(DEFAULT_HELD_TOOL), "BP_ExoFrame holds the cutter at spawn")
    targets = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.HollowToolTestTarget)]
    check(len(targets) >= expected_targets, "tool targets in L_Hub_Dev")
    log("validation passed")


def main():
    ensure_inputs()
    effects = {
        "fuel": ensure_effect("GE_Tool_FuelDrain", unreal.GameplayEffect, configure_fuel_drain),
        "heat": ensure_effect("GE_Tool_Heat", unreal.GameplayEffect, configure_heat),
        "overheat": ensure_effect("GE_Tool_Overheat", unreal.HollowToolOverheatEffect, configure_overheat),
    }
    cooling = ensure_effect("GE_Tool_Cooling", unreal.GameplayEffect, configure_cooling)
    abilities = [(ensure_tool(spec, effects), spec["input"]) for spec in TOOLS.values()]
    ensure_ability_set(abilities, cooling)
    for (ability_class, _), spec in zip(abilities, TOOLS.values()):
        ensure_tool_set(spec, ability_class)
    ensure_cue()
    ensure_tool_mount()
    validate(abilities, ensure_target())
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_tool_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
