"""Creates the S4-02 squad Resonance content (idempotent, ADR-0010, ADR-0020).

  /Game/Hollow/GAS/Resonance/DA_ResonanceConfig        UHollowResonanceConfig: tiers, hysteresis, server rate
  /Game/Hollow/GAS/Effects/GE_Dev_ResonanceAdd         instant Resonance change (SetByCaller Hollow.Data.ResonanceGain), dev only
  /Game/Hollow/FX/Resonance/
      MPC_Resonance                                    Intensity 0..1, FlickerTimeOffset (server time - world time),
                                                       ReducedFlashes 0/1 (the player's choice, H-1, ADR-0029)
      M_PP_Resonance                                   post-process: chromatic split, desaturation, vignette, flicker
                                                       (steady with ReducedFlashes)
      DA_ResonancePresentation                         UHollowResonancePresentationConfig
  /Game/Hollow/Audio/Modulation/SMP_Resonance_Lowpass  low-pass frequency parameter of the bus
  /Game/Hollow/Audio/Modulation/CB_Resonance           Audio Modulation control bus the presentation drives
  /Game/Hollow/Audio/SC_Hollow_World                   sound class whose low-pass follows CB_Resonance; room volumes apply (S4-04)
  BP_ExoFrame                                          M_PP_Resonance on the first-person camera
  DA_Enemy_EchoStalker                                 HearingResonanceCurve

The material graph is built from MATERIAL below; a hash of it is kept in the asset's metadata, so the graph is rebuilt
only when the spec changes. Creates or fills only what is missing or differs, then validates.
Run after create_gadget_assets.py:  Build/Scripts/run-editor-python.sh Build/Python/create_resonance_assets.py
"""
import hashlib
import json
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

CONFIG = "/Game/Hollow/GAS/Resonance/DA_ResonanceConfig"
DEV_EFFECT = "/Game/Hollow/GAS/Effects/GE_Dev_ResonanceAdd"
FX_DIR = "/Game/Hollow/FX/Resonance"
COLLECTION = FX_DIR + "/MPC_Resonance"
MATERIAL_PATH = FX_DIR + "/M_PP_Resonance"
PRESENTATION = FX_DIR + "/DA_ResonancePresentation"
AUDIO_DIR = "/Game/Hollow/Audio/Modulation"
LOWPASS_PARAMETER = AUDIO_DIR + "/SMP_Resonance_Lowpass"
BUS = AUDIO_DIR + "/CB_Resonance"
SOUND_CLASS = "/Game/Hollow/Audio/SC_Hollow_World"
EXOFRAME_BP = "/Game/Hollow/Characters/ExoFrame/BP_ExoFrame"
STALKER_DEFINITION = "/Game/Hollow/Characters/Enemies/EchoStalker/DA_Enemy_EchoStalker"
ATTRIBUTE_MODULE = "/Script/HollowGAS"
SPEC_TAG = "HollowSpec"

# Tiers (data, S4-02 clarification 4): Medium, High and Critical start at these fractions of MaxResonance.
CONFIG_VALUES = {"tier_thresholds": [0.25, 0.5, 0.75], "hysteresis": 0.03, "update_seconds": 0.5}

# Presentation (data). The bus is a low-pass frequency parameter: 1.0 = 20 kHz, 0.7 = about 2.5 kHz (log scale).
INTENSITY = "Intensity"
TIME_OFFSET = "FlickerTimeOffset"
REDUCED_FLASHES = "ReducedFlashes"
PRESENTATION_VALUES = {"intensity_parameter": INTENSITY, "time_offset_parameter": TIME_OFFSET, "reduced_flashes_parameter": REDUCED_FLASHES,
                       "bus_value_calm": 1.0, "bus_value_full": 0.7, "bus_fade_seconds": 0.25, "ease_seconds": 1.5, "step_seconds": 0.05}

# Hearing of the Echo-Stalker by squad Resonance (data, S4-02 clarification 7): x1 at 0 up to x2 at full.
HEARING_KEYS = [(0.0, 1.0), (0.25, 1.25), (0.5, 1.6), (1.0, 2.0)]

# Post-process look at full Resonance (data). Flicker starts with the High tier. With ReducedFlashes (ADR-0029) the
# flicker becomes its average dimming, held steady (0 Hz).
MATERIAL = {
    "version": 2,
    "flicker_hz": 7.0, "flicker_from": 0.5, "flicker_depth": 0.15,
    "reduced_flicker_hz": 0.0,
    "chroma_uv": 0.004, "desaturation": 0.4, "vignette_scale": 1.4, "vignette_depth": 0.6,
}
MAX_REDUCED_HZ = 2.0   # H-1: nothing faster than this with reduced flashes

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
mel = unreal.MaterialEditingLibrary


def log(message):
    unreal.log("create_resonance_assets: " + message)


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


def existing_or_new(path, asset_class, factory):
    if assets.does_asset_exist(path):
        return load(path), False
    return create(path, asset_class, factory), True


def data_asset(path, asset_class):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    return existing_or_new(path, asset_class, factory)


def blueprint(path, parent_class):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return existing_or_new(path, unreal.Blueprint, factory)


def from_text(struct, text):
    if not struct.import_text(text):
        raise RuntimeError("Could not import " + text)
    return struct


def same(a, b):
    return a.export_text() == b.export_text() if hasattr(a, "export_text") else a == b


def apply_values(obj, values):
    """Sets the properties that differ; True if any did."""
    changed = False
    for name, value in values.items():
        current = obj.get_editor_property(name)
        if isinstance(value, float) and isinstance(current, float):
            differs = abs(current - value) > 1e-4
        elif isinstance(value, list):
            differs = len(current) != len(value) or any(abs(a - b) > 1e-4 for a, b in zip(current, value))
        elif isinstance(value, str):
            differs = str(current) != value
        else:
            differs = not same(current, value)
        if differs:
            obj.set_editor_property(name, value)
            changed = True
    return changed


# ---------------------------------------------------------------- gameplay data

def ensure_config():
    config, created = data_asset(CONFIG, unreal.HollowResonanceConfig)
    if apply_values(config, CONFIG_VALUES) or created:
        save(config)
        log("DA_ResonanceConfig ready")
    return config


def ensure_dev_effect():
    effect_bp, created = blueprint(DEV_EFFECT, unreal.GameplayEffect)
    if created:
        effect = unreal.get_default_object(effect_bp.generated_class())
        effect.set_editor_property("duration_policy", unreal.GameplayEffectDurationType.INSTANT)
        path = "%s.HollowResonanceSet:Resonance" % ATTRIBUTE_MODULE
        text = ('(Attribute=(AttributeName="Resonance",Attribute={path},AttributeOwner="/Script/CoreUObject.Class\'{module}.HollowResonanceSet\'"),'
                'ModifierOp=Additive,ModifierMagnitude=(MagnitudeCalculationType=SetByCaller,SetByCallerMagnitude=(DataTag=(TagName="Hollow.Data.ResonanceGain"))))'
                ).format(path=path, module=ATTRIBUTE_MODULE)
        modifier = from_text(unreal.GameplayModifierInfo(), text)
        if path not in modifier.export_text():
            raise RuntimeError("Could not build the Resonance modifier")
        effect.set_editor_property("modifiers", [modifier])
        save(effect_bp)
        log("created GE_Dev_ResonanceAdd")
    return effect_bp.generated_class()


def hearing_curve():
    keys = ",".join("(InterpMode=RCIM_Linear,Time=%.6f,Value=%.6f)" % key for key in HEARING_KEYS)
    return from_text(unreal.RuntimeFloatCurve(), "(EditorCurveData=(Keys=(%s)))" % keys)


def ensure_stalker_hearing():
    definition = load(STALKER_DEFINITION)
    wanted = hearing_curve()
    if not same(definition.get_editor_property("hearing_resonance_curve"), wanted):
        definition.set_editor_property("hearing_resonance_curve", wanted)
        save(definition)
        log("DA_Enemy_EchoStalker: hearing grows with squad Resonance")


# ---------------------------------------------------------------- visual

def ensure_collection():
    collection, created = existing_or_new(COLLECTION, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    scalars = list(collection.get_editor_property("scalar_parameters"))
    names = [str(p.get_editor_property("parameter_name")) for p in scalars]
    missing = [name for name in (INTENSITY, TIME_OFFSET, REDUCED_FLASHES) if name not in names]
    for name in missing:
        parameter = unreal.CollectionScalarParameter()
        parameter.set_editor_property("parameter_name", name)
        parameter.set_editor_property("default_value", 0.0)
        scalars.append(parameter)
    if missing or created:
        collection.set_editor_property("scalar_parameters", scalars)
        save(collection)
        log("MPC_Resonance ready (%s added)" % ", ".join(missing))
    return collection


def spec_hash():
    return hashlib.sha1(json.dumps(MATERIAL, sort_keys=True).encode("ascii")).hexdigest()


class Graph(object):
    """Places material expressions in columns and wires them."""

    def __init__(self, material, collection):
        self.material = material
        self.collection = collection
        self.row = 0

    def node(self, expression_class, column, **properties):
        expression = mel.create_material_expression(self.material, expression_class, -300 * column, 120 * self.row)
        self.row += 1
        for name, value in properties.items():
            expression.set_editor_property(name, value)
        return expression

    def wire(self, source, target, target_input="", source_output=""):
        if not mel.connect_material_expressions(source, source_output, target, target_input):
            raise RuntimeError("Could not connect %s -> %s.%s" % (source.get_name(), target.get_name(), target_input))

    def parameter(self, name, column):
        return self.node(unreal.MaterialExpressionCollectionParameter, column, collection=self.collection, parameter_name=name)

    def math(self, expression_class, column, a, b=None, const_b=None):
        properties = {} if const_b is None else {"const_b": const_b}
        expression = self.node(expression_class, column, **properties)
        self.wire(a, expression, "A")
        if b is not None:
            self.wire(b, expression, "B")
        return expression

    def single(self, expression_class, column, source, **properties):
        expression = self.node(expression_class, column, **properties)
        self.wire(source, expression)
        return expression

    def scene(self, column, uv, mask):
        sample = self.node(unreal.MaterialExpressionSceneTexture, column, scene_texture_id=unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
        self.wire(uv, sample, "UVs")
        channel = self.node(unreal.MaterialExpressionComponentMask, column - 1, r=mask == "r", g=mask == "g", b=mask == "b", a=False)
        self.wire(sample, channel, "", "Color")
        return channel


def build_material(material, collection):
    m = MATERIAL
    mel.delete_all_material_expressions(material)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    material.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    g = Graph(material, collection)
    Add, Sub, Mul = unreal.MaterialExpressionAdd, unreal.MaterialExpressionSubtract, unreal.MaterialExpressionMultiply
    intensity = g.parameter(INTENSITY, 9)
    offset = g.parameter(TIME_OFFSET, 9)

    # Flicker in phase on every machine: Time + (server time - world time).
    time = g.node(unreal.MaterialExpressionTime, 9)
    server_time = g.math(Add, 8, time, offset)
    wave = g.single(unreal.MaterialExpressionSine, 6, g.math(Mul, 7, server_time, const_b=m["flicker_hz"]), period=1.0)
    wave01 = g.math(Add, 5, g.math(Mul, 5, wave, const_b=0.5), const_b=0.5)
    ramp = g.math(Mul, 7, g.math(Sub, 8, intensity, const_b=m["flicker_from"]), const_b=1.0 / (1.0 - m["flicker_from"]))
    depth = g.math(Mul, 5, g.single(unreal.MaterialExpressionSaturate, 6, ramp), const_b=m["flicker_depth"])
    flickering = g.single(unreal.MaterialExpressionOneMinus, 4, g.math(Mul, 4, depth, wave01))
    # Reduced flashes: the flicker's average (the wave's mean is one half), steady.
    steady = g.single(unreal.MaterialExpressionOneMinus, 4, g.math(Mul, 4, depth, const_b=0.5))
    brightness = g.node(unreal.MaterialExpressionLinearInterpolate, 3)
    g.wire(flickering, brightness, "A")
    g.wire(steady, brightness, "B")
    g.wire(g.parameter(REDUCED_FLASHES, 4), brightness, "Alpha")

    # Chromatic split: red and blue sampled apart along X.
    uv = g.node(unreal.MaterialExpressionScreenPosition, 9)
    shift = g.math(unreal.MaterialExpressionAppendVector, 7, g.math(Mul, 8, intensity, const_b=m["chroma_uv"]),
                   g.node(unreal.MaterialExpressionConstant, 8, r=0.0))
    uv_red = g.node(Add, 6)
    g.wire(uv, uv_red, "A", "ViewportUV")
    g.wire(shift, uv_red, "B")
    uv_blue = g.node(Sub, 6)
    g.wire(uv, uv_blue, "A", "ViewportUV")
    g.wire(shift, uv_blue, "B")
    uv_green = g.node(unreal.MaterialExpressionComponentMask, 6, r=True, g=True, b=False, a=False)
    g.wire(uv, uv_green, "", "ViewportUV")
    red = g.scene(5, uv_red, "r")
    green = g.scene(5, uv_green, "g")
    blue = g.scene(5, uv_blue, "b")
    colour = g.math(unreal.MaterialExpressionAppendVector, 3, g.math(unreal.MaterialExpressionAppendVector, 3, red, green), blue)

    # Desaturation and vignette.
    desaturated = g.node(unreal.MaterialExpressionDesaturation, 2)
    g.wire(colour, desaturated, "")
    g.wire(g.math(Mul, 3, intensity, const_b=m["desaturation"]), desaturated, "Fraction")
    centre = g.node(unreal.MaterialExpressionConstant2Vector, 6, r=0.5, g=0.5)
    distance = g.node(unreal.MaterialExpressionDistance, 5)
    g.wire(uv, distance, "A", "ViewportUV")
    g.wire(centre, distance, "B")
    edge = g.single(unreal.MaterialExpressionSaturate, 4, g.math(Mul, 4, distance, const_b=m["vignette_scale"]))
    falloff = g.single(unreal.MaterialExpressionPower, 3, edge, const_exponent=2.0)
    vignette = g.single(unreal.MaterialExpressionOneMinus, 2, g.math(Mul, 2, g.math(Mul, 3, falloff, intensity), const_b=m["vignette_depth"]))

    result = g.math(Mul, 1, g.math(Mul, 1, desaturated, vignette), brightness)
    if not mel.connect_material_property(result, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Could not connect the emissive colour")
    mel.recompile_material(material)


def ensure_material(collection):
    material, created = existing_or_new(MATERIAL_PATH, unreal.Material, unreal.MaterialFactoryNew())
    if not created and assets.get_metadata_tag(material, SPEC_TAG) == spec_hash():
        return material
    build_material(material, collection)
    assets.set_metadata_tag(material, SPEC_TAG, spec_hash())
    save(material)
    log("M_PP_Resonance built (%d expressions)" % mel.get_num_material_expressions(material))
    return material


def ensure_camera(material):
    exo_bp = load(EXOFRAME_BP)
    camera = unreal.get_default_object(exo_bp.generated_class()).get_editor_property("first_person_camera")
    settings = camera.get_editor_property("post_process_settings")
    blendables = settings.get_editor_property("weighted_blendables")
    entries = list(blendables.get_editor_property("array"))
    if any(e.get_editor_property("object") == material and abs(e.get_editor_property("weight") - 1.0) < 1e-4 for e in entries):
        return
    entries = [e for e in entries if e.get_editor_property("object") != material]
    entry = unreal.WeightedBlendable()
    entry.set_editor_property("weight", 1.0)
    entry.set_editor_property("object", material)
    entries.append(entry)
    blendables.set_editor_property("array", entries)
    settings.set_editor_property("weighted_blendables", blendables)
    camera.set_editor_property("post_process_settings", settings)
    save(exo_bp)
    log("BP_ExoFrame: M_PP_Resonance on the camera")


# ---------------------------------------------------------------- audio

def ensure_audio():
    if assets.does_asset_exist(LOWPASS_PARAMETER):
        parameter = load(LOWPASS_PARAMETER)
    else:
        package_path, name = LOWPASS_PARAMETER.rsplit("/", 1)
        parameter = unreal.HollowAssetAuthoring.create_object_asset(unreal.SoundModulationParameterLPFFrequency.static_class(), package_path, name)
        if parameter is None:
            raise RuntimeError("Could not create " + LOWPASS_PARAMETER)
        save(parameter)
        log("created SMP_Resonance_Lowpass")
    bus, created = existing_or_new(BUS, unreal.SoundControlBus, unreal.SoundControlBusFactory())
    if created or bus.get_editor_property("parameter") != parameter:
        bus.set_editor_property("parameter", parameter)
        save(bus)
        log("CB_Resonance ready")
    sound_class, created = existing_or_new(SOUND_CLASS, unreal.SoundClass, unreal.SoundClassFactory())
    properties = sound_class.get_editor_property("properties")
    modulation = properties.get_editor_property("modulation_settings")
    lowpass = modulation.get_editor_property("lowpass_modulation_destination")
    modulators = list(lowpass.get_editor_property("modulators"))
    ambient = properties.get_editor_property("apply_ambient_volumes")
    if created or modulators != [bus] or not lowpass.get_editor_property("enable_modulation") or not ambient:
        lowpass.set_editor_property("enable_modulation", True)
        lowpass.set_editor_property("modulators", [bus])
        modulation.set_editor_property("lowpass_modulation_destination", lowpass)
        properties.set_editor_property("modulation_settings", modulation)
        properties.set_editor_property("apply_ambient_volumes", True)   # S4-04: the room acoustics volumes filter these sounds
        sound_class.set_editor_property("properties", properties)
        save(sound_class)
        log("SC_Hollow_World: low-pass follows CB_Resonance, room volumes apply")
    return bus


def ensure_presentation(collection, bus):
    presentation, created = data_asset(PRESENTATION, unreal.HollowResonancePresentationConfig)
    wanted = dict(PRESENTATION_VALUES)
    changed = apply_values(presentation, wanted)
    for name, value in (("collection", collection), ("bus", bus)):
        if presentation.get_editor_property(name) != value:
            presentation.set_editor_property(name, value)
            changed = True
    if created or changed:
        save(presentation)
        log("DA_ResonancePresentation ready")
    return presentation


# ---------------------------------------------------------------- validation

def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def ini_has(line):
    with open(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_config_dir()) + "DefaultGame.ini") as ini:
        return any(entry.strip() == line for entry in ini)


def object_path(path):
    return "%s.%s" % (path, path.rsplit("/", 1)[1])


def validate():
    config = load(CONFIG)
    thresholds = list(config.get_editor_property("tier_thresholds"))
    check(len(thresholds) == 3 and thresholds == sorted(thresholds), "three ascending tier thresholds")
    check(all(b - a > config.get_editor_property("hysteresis") for a, b in zip(thresholds, thresholds[1:])), "tiers further apart than the hysteresis")
    check(assets.get_metadata_tag(load(MATERIAL_PATH), SPEC_TAG) == spec_hash(), "M_PP_Resonance matches the spec")
    names = [str(p.get_editor_property("parameter_name")) for p in load(COLLECTION).get_editor_property("scalar_parameters")]
    check(all(name in names for name in (INTENSITY, TIME_OFFSET, REDUCED_FLASHES)), "MPC_Resonance scalars")
    check(MATERIAL["reduced_flicker_hz"] <= MAX_REDUCED_HZ, "no reduced-flashes flicker over 2 Hz")
    presentation = load(PRESENTATION)
    check(presentation.get_editor_property("collection") == load(COLLECTION) and presentation.get_editor_property("bus") == load(BUS), "presentation assets")
    check(same(load(STALKER_DEFINITION).get_editor_property("hearing_resonance_curve"), hearing_curve()), "stalker hearing curve")
    # The settings classes are not exposed to Python: read the config file they come from.
    check(ini_has("Config=" + object_path(CONFIG)), "UHollowResonanceSettings::Config in DefaultGame.ini")
    check(ini_has("Presentation=" + object_path(PRESENTATION)), "UHollowResonancePresentationSettings::Presentation in DefaultGame.ini")
    check(ini_has("DevResonanceEffect=%s_C" % object_path(DEV_EFFECT)), "UHollowRaidSettings::DevResonanceEffect in DefaultGame.ini")
    log("validation passed")


def main():
    ensure_config()
    ensure_dev_effect()
    ensure_stalker_hearing()
    collection = ensure_collection()
    material = ensure_material(collection)
    ensure_camera(material)
    bus = ensure_audio()
    ensure_presentation(collection, bus)
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_resonance_assets failed:\n" + traceback.format_exc())
