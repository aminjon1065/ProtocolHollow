"""Creates the S3-05 noise content (idempotent, ADR-0010, ADR-0018).

  /Game/Hollow/Core/DA_NoiseConfig    UHollowNoiseConfig: one rule per Hollow.Noise.* tag + the footstep cadence (data)
  DA_Tool_Welder / DA_Tool_PlasmaCutter  checked for their noise tags; resaved once if they still hold the noise numbers
                                         that moved here (S3-05 removed NoiseLoudness / NoiseRange / NoiseEveryNSamples)

Creates or fills only what is missing or differs, then validates.
Run after create_tool_assets.py:  Build/Scripts/run-editor-python.sh Build/Python/create_noise_assets.py
"""
import os
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

CONFIG_PATH = "/Game/Hollow/Core/DA_NoiseConfig"
TOOLS = {
    "/Game/Hollow/Tools/Welder/DA_Tool_Welder": "Hollow.Noise.Tool.Welder",
    "/Game/Hollow/Tools/PlasmaCutter/DA_Tool_PlasmaCutter": "Hollow.Noise.Tool.Cutter",
}
STALE_TOOL_FIELDS = (b"NoiseEveryNSamples", b"NoiseLoudness", b"NoiseRange")

# Rules (data): tag -> loudness (x the listener's hearing range), max range in cm (0: none), min interval in s (0: none).
# The Iron Pilgrim hears 30 m: welding 15 m, cutting 20 m, walking 7.5 m (sprinting 12 m), a dropped core 24 m,
# a heavy blow 30 m, a breach across the sector.
RULES = [
    ("Hollow.Noise.Tool.Welder", 1.0, 1500.0, 0.5),
    ("Hollow.Noise.Tool.Cutter", 1.2, 2000.0, 0.5),
    ("Hollow.Noise.Footstep", 0.25, 0.0, 0.0),
    ("Hollow.Noise.Core.Drop", 0.8, 2500.0, 0.0),
    ("Hollow.Noise.Impact.Heavy", 1.0, 3000.0, 0.0),
    ("Hollow.Noise.Structural.Breach", 2.0, 6000.0, 0.0),
]
# Footsteps (data): checked twice a second; crouch-walking (180) stays under the minimum; walking (400) is the reference.
FOOTSTEPS = {"footstep_interval": 0.5, "footstep_min_speed": 200.0, "footstep_reference_speed": 400.0,
             "inter_room_range": 0.6}   # S4-04: a noise from another room carries 60 % as far (walls; players hear it muffled too)

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_noise_assets: " + message)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset " + path)
    return asset


def save(asset):
    if not assets.save_loaded_asset(asset, False):
        raise RuntimeError("Could not save " + asset.get_path_name())


def make_tag(name):
    tag = unreal.GameplayTag()
    if not tag.import_text('(TagName="%s")' % name):
        raise RuntimeError("Unknown tag " + name)
    return tag


def same_tag(a, b):
    return a.export_text() == b.export_text()


def make_rule(tag, loudness, max_range, min_interval):
    rule = unreal.HollowNoiseRule()
    rule.set_editor_property("noise_tag", make_tag(tag))
    rule.set_editor_property("loudness", loudness)
    rule.set_editor_property("max_range", max_range)
    rule.set_editor_property("min_interval", min_interval)
    return rule


def rule_key(rule):
    return (rule.get_editor_property("noise_tag").export_text(), round(rule.get_editor_property("loudness"), 4),
            round(rule.get_editor_property("max_range"), 2), round(rule.get_editor_property("min_interval"), 4))


def asset_file(path):
    relative = path[len("/Game/"):] + ".uasset"
    return os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), relative)


def ensure_config():
    if assets.does_asset_exist(CONFIG_PATH):
        config = load(CONFIG_PATH)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.HollowNoiseConfig)
        package_path, name = CONFIG_PATH.rsplit("/", 1)
        config = tools.create_asset(name, package_path, unreal.HollowNoiseConfig, factory)
        if config is None:
            raise RuntimeError("Could not create " + CONFIG_PATH)
    wanted = [make_rule(*rule) for rule in RULES]
    changed = [rule_key(r) for r in config.get_editor_property("rules")] != [rule_key(r) for r in wanted]
    if changed:
        config.set_editor_property("rules", wanted)
    for name, value in FOOTSTEPS.items():
        if abs(config.get_editor_property(name) - value) > 1e-4:
            config.set_editor_property(name, value)
            changed = True
    if changed:
        save(config)
        log("DA_NoiseConfig ready")


def ensure_tools():
    for path, noise in TOOLS.items():
        definition = load(path)
        if not same_tag(definition.get_editor_property("noise_tag"), make_tag(noise)):
            definition.set_editor_property("noise_tag", make_tag(noise))
            save(definition)
            log(path.rsplit("/", 1)[1] + ": noise tag set")
            continue
        with open(asset_file(path), "rb") as package:
            stale = any(field in package.read() for field in STALE_TOOL_FIELDS)
        if stale:
            save(definition)   # drops the serialized values of the removed properties
            log(path.rsplit("/", 1)[1] + ": resaved without the old noise numbers")


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate():
    config = load(CONFIG_PATH)
    check([rule_key(r) for r in config.get_editor_property("rules")] == [rule_key(make_rule(*r)) for r in RULES], "DA_NoiseConfig rules")
    check(config.get_editor_property("footstep_min_speed") < config.get_editor_property("footstep_reference_speed"), "footstep speeds")
    tags = [r.get_editor_property("noise_tag").export_text() for r in config.get_editor_property("rules")]
    for path, noise in TOOLS.items():
        check(make_tag(noise).export_text() in tags, path + " noise has a rule")
        with open(asset_file(path), "rb") as package:
            check(not any(field in package.read() for field in STALE_TOOL_FIELDS), path + " holds no old noise numbers")
    log("validation passed")


def main():
    ensure_config()
    ensure_tools()
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_noise_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
