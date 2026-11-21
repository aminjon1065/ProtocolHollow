"""Creates the S4-07 voice content (idempotent, ADR-0010, ADR-0025). Nothing heavy: engine Synthesis effect presets and
settings only, no audio files.

  /Game/Hollow/Core/Input/Actions/IA_PushToTalk    held: transmits the voice
  /Game/Hollow/Core/Input/IMC_Voice                V / LB, added by every local player controller (alive, dead, hub)
  /Game/Hollow/Audio/Voice/SC_Hollow_Voice         the VOIP sound class (DefaultEngine.ini AudioSettings.VoiPSoundClass)
  /Game/Hollow/Audio/Voice/ATT_Hollow_Voice        a living player's voice: 3D from the helmet, reverb send on
  /Game/Hollow/Audio/Voice/SEP_*                   filter and wave shaper presets (Synthesis)
  /Game/Hollow/Audio/Voice/SEC_Hollow_Helmet|Radio effect chains: the helmet (alive) and the radio (no living body)
  /Game/Hollow/Audio/Voice/DA_VoiceConfig          UHollowVoiceConfig (UHollowVoiceSettings::VoiceConfig)

The voice folder is always cooked (DefaultGame.ini): DA_VoiceConfig and the sound class are only named in config.
Run: Build/Scripts/run-editor-python.sh Build/Python/create_voice_assets.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

INPUT_DIR = "/Game/Hollow/Core/Input"
ACTION = INPUT_DIR + "/Actions/IA_PushToTalk"
MAPPING = INPUT_DIR + "/IMC_Voice"
PUSH_TO_TALK_KEYS = ["V", "Gamepad_LeftShoulder"]
MAPPING_PRIORITY = 0          # no key of its own clashes with IMC_Raid

VOICE_DIR = "/Game/Hollow/Audio/Voice"
SOUND_CLASS = VOICE_DIR + "/SC_Hollow_Voice"
ATTENUATION = VOICE_DIR + "/ATT_Hollow_Voice"
CONFIG = VOICE_DIR + "/DA_VoiceConfig"

# A voice carries 25 m from the helmet (full up to 2 m), dulls with distance and sends to the room's reverb; walls
# muffle it through the rooms' Audio Gameplay Volumes (S4-04). To tune by ear (KnownIssues).
ATTENUATION_SETTINGS = {"attenuation_shape_extents": 200.0, "falloff_distance": 2500.0, "distance_algorithm": "NATURAL_SOUND",
                        "attenuate_with_lpf": True, "lpf_radius_min": 600.0, "lpf_radius_max": 2500.0,
                        "lpf_frequency_at_max": 3000.0, "enable_reverb_send": True}
# Effect presets: the helmet narrows the voice a little and roughens it; the radio is a narrower band, rougher.
PRESETS = {
    "SEP_Hollow_Helmet_Band": ("SourceEffectFilterPreset", {"filter_circuit": "STATE_VARIABLE", "filter_type": "BAND_PASS",
                                                            "cutoff_frequency": 1700.0, "filter_q": 0.6}),
    "SEP_Hollow_Helmet_Grit": ("SourceEffectWaveShaperPreset", {"amount": 1.5, "output_gain_db": -1.0}),
    "SEP_Hollow_Radio_Band": ("SourceEffectFilterPreset", {"filter_circuit": "STATE_VARIABLE", "filter_type": "BAND_PASS",
                                                           "cutoff_frequency": 1800.0, "filter_q": 1.4}),
    "SEP_Hollow_Radio_Grit": ("SourceEffectWaveShaperPreset", {"amount": 4.0, "output_gain_db": -4.0}),
}
CHAINS = {
    "SEC_Hollow_Helmet": ["SEP_Hollow_Helmet_Band", "SEP_Hollow_Helmet_Grit"],
    "SEC_Hollow_Radio": ["SEP_Hollow_Radio_Band", "SEP_Hollow_Radio_Grit"],
}

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_voice_assets: " + message)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset " + path)
    return asset


def save(asset):
    if not assets.save_loaded_asset(asset, False):
        raise RuntimeError("Could not save " + asset.get_path_name())


def existing_or_new(path, asset_class, factory):
    if assets.does_asset_exist(path):
        return load(path), False
    package_path, name = path.rsplit("/", 1)
    asset = tools.create_asset(name, package_path, asset_class, factory)
    if asset is None:
        raise RuntimeError("Could not create " + path)
    return asset, True


def make_key(name):
    key = unreal.Key()
    if not key.import_text(name):
        raise RuntimeError("Unknown key " + name)
    return key


def apply(obj, values):
    """Sets the values that differ (enums by name); True if any did."""
    changed = False
    for name, value in values.items():
        current = obj.get_editor_property(name)
        if isinstance(value, str) and not isinstance(current, str):
            value = getattr(type(current), value)   # an enum by name
        if isinstance(value, float):
            differs = abs(float(current) - value) > 1e-4
        else:
            differs = current != value
        if differs:
            obj.set_editor_property(name, value)
            changed = True
    return changed


def check(condition, what):
    if not condition:
        raise RuntimeError("Validation failed: " + what)


# ---------------------------------------------------------------- input

def ensure_input():
    action, created = existing_or_new(ACTION, unreal.InputAction, unreal.InputAction_Factory())
    if created or action.get_editor_property("value_type") != unreal.InputActionValueType.BOOLEAN:
        action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        save(action)
        log("IA_PushToTalk ready")
    context, created = existing_or_new(MAPPING, unreal.InputMappingContext, unreal.InputMappingContext_Factory())
    data = context.get_editor_property("default_key_mappings")
    current = [(m.get_editor_property("action"), m.get_editor_property("key").export_text())
               for m in data.get_editor_property("mappings")]
    wanted = [(action, make_key(name).export_text()) for name in PUSH_TO_TALK_KEYS]
    if created or current != wanted:
        mappings = []
        for name in PUSH_TO_TALK_KEYS:
            entry = unreal.EnhancedActionKeyMapping()
            entry.set_editor_property("action", action)
            entry.set_editor_property("key", make_key(name))
            mappings.append(entry)
        data.set_editor_property("mappings", mappings)
        context.set_editor_property("default_key_mappings", data)
        save(context)
        log("IMC_Voice: " + ", ".join(PUSH_TO_TALK_KEYS))
    return action, context


# ---------------------------------------------------------------- sound

def ensure_sound_class():
    sound_class, created = existing_or_new(SOUND_CLASS, unreal.SoundClass, unreal.SoundClassFactory())
    if created:
        save(sound_class)
        log("SC_Hollow_Voice ready")
    return sound_class


def ensure_attenuation():
    attenuation, created = existing_or_new(ATTENUATION, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
    settings = attenuation.get_editor_property("attenuation")
    values = dict(ATTENUATION_SETTINGS)
    inner = values.pop("attenuation_shape_extents")
    changed = abs(settings.get_editor_property("attenuation_shape_extents").x - inner) > 1e-4
    if changed:
        settings.set_editor_property("attenuation_shape_extents", unreal.Vector(inner, 0.0, 0.0))
    changed = apply(settings, values) or changed
    if created or changed:
        attenuation.set_editor_property("attenuation", settings)
        save(attenuation)
        log("ATT_Hollow_Voice ready")
    return attenuation


def ensure_presets():
    made = {}
    for name, (class_name, values) in sorted(PRESETS.items()):
        preset_class = getattr(unreal, class_name)
        factory = unreal.SoundSourceEffectFactory()
        factory.set_editor_property("sound_effect_sourcepreset_class", preset_class)
        preset, created = existing_or_new(VOICE_DIR + "/" + name, preset_class, factory)
        settings = preset.get_editor_property("settings")
        if apply(settings, values) or created:
            preset.set_editor_property("settings", settings)
            save(preset)
            log(name + " ready")
        made[name] = preset
    return made


def ensure_chains(presets):
    made = {}
    for name, parts in sorted(CHAINS.items()):
        chain, created = existing_or_new(VOICE_DIR + "/" + name, unreal.SoundEffectSourcePresetChain, unreal.SoundSourceEffectChainFactory())
        current = [entry.get_editor_property("preset") for entry in chain.get_editor_property("chain")]
        wanted = [presets[part] for part in parts]
        if created or current != wanted:
            entries = []
            for preset in wanted:
                entry = unreal.SourceEffectChainEntry()
                entry.set_editor_property("preset", preset)
                entries.append(entry)
            chain.set_editor_property("chain", entries)
            save(chain)
            log("%s: %s" % (name, " -> ".join(parts)))
        made[name] = chain
    return made


def ensure_config(action, context, attenuation, chains):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.HollowVoiceConfig)
    config, created = existing_or_new(CONFIG, unreal.HollowVoiceConfig, factory)
    values = {"push_to_talk_action": action, "mapping_context": context, "mapping_priority": MAPPING_PRIORITY,
              "attenuation": attenuation, "helmet_chain": chains["SEC_Hollow_Helmet"], "radio_chain": chains["SEC_Hollow_Radio"]}
    if apply(config, values) or created:
        save(config)
        log("DA_VoiceConfig ready")
    return config


def validate(config):
    check(config.get_editor_property("push_to_talk_action").get_editor_property("value_type") == unreal.InputActionValueType.BOOLEAN,
          "IA_PushToTalk is a button")
    keys = [m.get_editor_property("key").export_text() for m in
            config.get_editor_property("mapping_context").get_editor_property("default_key_mappings").get_editor_property("mappings")]
    check(keys == [make_key(name).export_text() for name in PUSH_TO_TALK_KEYS], "IMC_Voice keys")
    for name, parts in CHAINS.items():
        chain = load(VOICE_DIR + "/" + name)
        check([e.get_editor_property("preset").get_name() for e in chain.get_editor_property("chain")] == parts, name + " order")
    attenuation = config.get_editor_property("attenuation").get_editor_property("attenuation")
    check(attenuation.get_editor_property("enable_reverb_send"), "ATT_Hollow_Voice sends to reverb")
    check(abs(attenuation.get_editor_property("falloff_distance") - ATTENUATION_SETTINGS["falloff_distance"]) < 1e-3, "ATT_Hollow_Voice range")
    check(config.get_editor_property("helmet_chain") != config.get_editor_property("radio_chain"), "helmet and radio differ")


def main():
    action, context = ensure_input()
    ensure_sound_class()
    attenuation = ensure_attenuation()
    chains = ensure_chains(ensure_presets())
    validate(ensure_config(action, context, attenuation, chains))
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_voice_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
