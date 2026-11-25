"""Creates the GP-01c game-feel content (idempotent, ADR-0010, ADR-0046).

  /Game/Hollow/Core/DA_FeelConfig    UHollowFeelConfig: the shake, rumble and view kick of each trigger (data)
                                     - one player's own (Hollow.Feel.*): its shots, its blow, the hands' swap, damage taken
                                     - a sound's (Hollow.Noise.* / Hollow.Sound.*), felt by every player near it as it
                                       plays: a breach, a heavy blow, a mine's blast, a Pilgrim's step, a crate, a core
                                     - the first-person hands' sway and the reduced-flashes scale
  Config/DefaultGame.ini             [/Script/HollowCore.HollowFeelSettings] Config (checked, written once)

Creates or fills only what is missing or differs, then validates. Needs only the native tags: run any time after the
editor builds, e.g. after create_noise_assets.py:  Build/Scripts/run-editor-python.sh Build/Python/create_feel_assets.py
"""
import os
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

CONFIG_PATH = "/Game/Hollow/Core/DA_FeelConfig"
INI_SECTION = "[/Script/HollowCore.HollowFeelSettings]"
INI_LINE = "Config=/Game/Hollow/Core/DA_FeelConfig.DA_FeelConfig"

# Shakes (data): seconds, blend-in seconds, swing of pitch/yaw/roll in degrees, of forward/right/up in cm, Hz.
# Rumble: intensity 0..1, seconds, heavy motors. Kick: pitch up (degrees), yaw to either side, hands back (cm; a blow
# thrusts them forward), seconds back to rest. Radii (cm) for a sound's: full within the inner, nothing past the outer.
PERSONAL = {
    # The nailgun ticks, the shotgun bucks; the blow lunges; the swap dips; damage jolts by how much was lost.
    "Hollow.Feel.Shot.Nailgun": {"shake": (0.12, 0.01, (0.25, 0.12, 0.15), (0.0, 0.0, 0.0), 22.0), "rumble": (0.25, 0.06, False),
                                 "kick": (0.9, 0.2, 1.5, 0.14)},
    "Hollow.Feel.Shot.Shotgun": {"shake": (0.28, 0.01, (1.1, 0.5, 0.8), (-1.0, 0.0, 0.4), 15.0), "rumble": (0.85, 0.16, True),
                                 "kick": (3.2, 0.6, 5.0, 0.30)},
    "Hollow.Feel.Melee": {"shake": (0.14, 0.01, (0.4, 0.3, 0.6), (0.0, 0.0, 0.0), 18.0), "rumble": (0.45, 0.08, True),
                          "kick": (-1.2, 0.4, -6.0, 0.22)},
    "Hollow.Feel.Swap": {"kick": (-2.5, 0.0, 2.0, 0.30)},
    "Hollow.Feel.Damage": {"shake": (0.32, 0.01, (1.4, 1.0, 1.8), (0.0, 0.6, 0.6), 11.0), "rumble": (0.6, 0.22, True)},
}
SOUNDS = {
    # The Iron Pilgrim's tread through the floor; a breach and a mine's blast across rooms; the rest close by.
    "Hollow.Sound.Footstep.Heavy": {"shake": (0.30, 0.02, (0.35, 0.1, 0.25), (0.0, 0.0, 0.6), 9.0), "rumble": (0.18, 0.08, True),
                                    "radii": (300.0, 1600.0)},
    "Hollow.Noise.Structural.Breach": {"shake": (0.70, 0.02, (1.6, 0.9, 1.2), (0.0, 0.0, 1.2), 12.0), "rumble": (0.75, 0.35, True),
                                       "radii": (500.0, 2800.0)},
    "Hollow.Noise.Impact.Heavy": {"shake": (0.25, 0.01, (0.6, 0.3, 0.4), (0.0, 0.0, 0.0), 14.0), "rumble": (0.3, 0.1, True),
                                  "radii": (200.0, 1200.0)},
    "Hollow.Sound.Mine.Blast": {"shake": (0.80, 0.02, (2.2, 1.2, 1.6), (0.0, 0.0, 1.5), 11.0), "rumble": (0.95, 0.4, True),
                                "radii": (600.0, 3200.0)},
    "Hollow.Noise.Impact.Crate": {"shake": (0.18, 0.01, (0.3, 0.15, 0.25), (0.0, 0.0, 0.0), 16.0), "rumble": (0.2, 0.06, False),
                                  "radii": (150.0, 900.0)},
    "Hollow.Noise.Core.Drop": {"shake": (0.25, 0.01, (0.5, 0.2, 0.3), (0.0, 0.0, 0.5), 10.0), "rumble": (0.35, 0.12, True),
                               "radii": (250.0, 1200.0)},
}
# Reduced flashes (ADR-0029): shakes and kicks at 40 %. Sway: 60 degrees a second of turning leaves the hands about
# 0.7 degrees behind, never more than 2.5; they catch up at 10 Hz.
SCALARS = {"reduced_flashes_scale": 0.4, "sway_per_turn_rate": 0.012, "max_sway": 2.5, "sway_return_rate": 10.0}

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_feel_assets: " + message)


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


def make_shake(spec):
    shake = unreal.HollowShakeParams()
    if spec is None:
        return shake
    duration, blend_in, (pitch, yaw, roll), (x, y, z), frequency = spec
    shake.set_editor_property("duration", duration)
    shake.set_editor_property("blend_in", blend_in)
    shake.set_editor_property("rotation", unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    shake.set_editor_property("location", unreal.Vector(x, y, z))
    shake.set_editor_property("frequency", frequency)
    return shake


def make_rumble(spec):
    rumble = unreal.HollowRumble()
    if spec is None:
        return rumble
    intensity, seconds, heavy = spec
    rumble.set_editor_property("intensity", intensity)
    rumble.set_editor_property("duration", seconds)
    rumble.set_editor_property("heavy", heavy)
    return rumble


def make_kick(spec):
    kick = unreal.HollowViewKick()
    if spec is None:
        return kick
    pitch, yaw, push_back, recover = spec
    kick.set_editor_property("pitch", pitch)
    kick.set_editor_property("yaw", yaw)
    kick.set_editor_property("push_back", push_back)
    kick.set_editor_property("recover", recover)
    return kick


def make_entry(trigger, spec):
    entry = unreal.HollowFeelEntry()
    entry.set_editor_property("trigger", make_tag(trigger))
    entry.set_editor_property("shake", make_shake(spec.get("shake")))
    entry.set_editor_property("rumble", make_rumble(spec.get("rumble")))
    entry.set_editor_property("kick", make_kick(spec.get("kick")))
    inner, outer = spec.get("radii", (0.0, 0.0))
    entry.set_editor_property("inner_radius", inner)
    entry.set_editor_property("outer_radius", outer)
    return entry


def wanted_entries():
    return [make_entry(trigger, spec) for trigger, spec in list(PERSONAL.items()) + list(SOUNDS.items())]


def entry_key(entry):
    shake = entry.get_editor_property("shake")
    rumble = entry.get_editor_property("rumble")
    kick = entry.get_editor_property("kick")
    rotation = shake.get_editor_property("rotation")
    location = shake.get_editor_property("location")
    numbers = [shake.get_editor_property("duration"), shake.get_editor_property("blend_in"), rotation.pitch, rotation.yaw, rotation.roll,
               location.x, location.y, location.z, shake.get_editor_property("frequency"),
               rumble.get_editor_property("intensity"), rumble.get_editor_property("duration"), float(rumble.get_editor_property("heavy")),
               kick.get_editor_property("pitch"), kick.get_editor_property("yaw"), kick.get_editor_property("push_back"),
               kick.get_editor_property("recover"), entry.get_editor_property("inner_radius"), entry.get_editor_property("outer_radius")]
    return (entry.get_editor_property("trigger").export_text(),) + tuple(round(n, 4) for n in numbers)


def ensure_config():
    if assets.does_asset_exist(CONFIG_PATH):
        config = load(CONFIG_PATH)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.HollowFeelConfig)
        package_path, name = CONFIG_PATH.rsplit("/", 1)
        config = tools.create_asset(name, package_path, unreal.HollowFeelConfig, factory)
        if config is None:
            raise RuntimeError("Could not create " + CONFIG_PATH)
    wanted = wanted_entries()
    changed = [entry_key(e) for e in config.get_editor_property("entries")] != [entry_key(e) for e in wanted]
    if changed:
        config.set_editor_property("entries", wanted)
    for name, value in SCALARS.items():
        if abs(config.get_editor_property(name) - value) > 1e-4:
            config.set_editor_property(name, value)
            changed = True
    if changed:
        save(config)
        log("DA_FeelConfig ready (%d entries)" % len(wanted))
    return config


def ini_path():
    return os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_config_dir()), "DefaultGame.ini")


def ensure_ini():
    path = ini_path()
    with open(path, "r", encoding="ascii", newline="") as ini:
        text = ini.read()
    if INI_SECTION in text:
        return
    block = "\n%s\n; GP-01c (ADR-0046): shakes, rumble and view kicks by trigger (create_feel_assets.py).\n%s\n" % (INI_SECTION, INI_LINE)
    with open(path, "w", encoding="ascii", newline="\n") as ini:
        ini.write(text.rstrip("\n") + "\n" + block)
    log("DefaultGame.ini: HollowFeelSettings written (takes effect at the next editor start)")


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate():
    config = load(CONFIG_PATH)
    keys = [entry_key(e) for e in config.get_editor_property("entries")]
    check(keys == [entry_key(e) for e in wanted_entries()], "DA_FeelConfig entries")
    for name, value in SCALARS.items():
        check(abs(config.get_editor_property(name) - value) <= 1e-4, "DA_FeelConfig " + name)
    for trigger in PERSONAL:
        check(trigger.startswith("Hollow.Feel."), trigger + " is one player's own")
    for trigger, spec in SOUNDS.items():
        check(not trigger.startswith("Hollow.Feel.") and "kick" not in spec and spec["radii"][1] > spec["radii"][0], trigger + " is a sound's")
    with open(ini_path(), "r", encoding="ascii") as ini:
        text = ini.read()
    check(INI_SECTION in text and INI_LINE in text, "DefaultGame.ini names DA_FeelConfig")
    log("validation passed")


def main():
    ensure_config()
    ensure_ini()
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_feel_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
