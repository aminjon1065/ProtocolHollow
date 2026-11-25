"""FAB-01 (ADR-0051): the AbsorbtionFX Niagara systems on the altar, the Aether core and the pulse mine (idempotent, ADR-0010).

The pack has one effect, ribbons of energy drawn into a glowing core, in ten colours; three of them are in the game
(prepare_fab_assets.py, Build/Art/fab_manifest.json): Teal (cold cyan), Purple (violet) and Yellow (gold). This script:
  makes our systems in /Game/Hollow/FX/Fab     copies of the pack's systems (HollowNiagaraAuthoring: the pack's own
                                             assets are never edited, an update would undo it), all of them without the
                                             pack's per-particle lights (a light on every ribbon) and its haze emitter
                                             (huge translucent sprites): measured on Mac Low, the ritual seen up close
                                             cost 31 FPS against 61 with them, 55 without (the altar keeps its own light);
                                             *_Reduced: a dimmer core for Reduced Flashes; NS_Fab_MineBlast: a short-lived core
  fills the data's effects (FHollowFxSpec)   DA_RitualAltar RitualFx (violet, during the 20 s ritual) and PurifiedFx (gold
                                             finale, once); DA_Carryable_AetherCore DefaultAura (cyan pulse) and the purified
                                             look's Aura (gold glow); DA_PulseMine BlastFx (gold collapse)
Under Reduced Flashes the steady glows play their dimmer variant, the finale and the blast do not play at all.
Run after create_cathedral_mechanics.py and create_gadget_assets.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_fab_fx.py
"""
import hashlib
import json
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

VERSION = 4
SPEC_TAG = "HollowSpec"
FAB_FX = "/Game/Fab/AbsorbtionFX/FX"
FX_DIR = "/Game/Hollow/FX/Fab"
ALTAR = "/HollowSector_Cathedral/Mechanics/DA_RitualAltar"
CORE = "/Game/Hollow/Items/AetherCore/DA_Carryable_AetherCore"
MINE = "/Game/Hollow/Gadgets/PulseMine/DA_PulseMine"

DIM_CORE = "(R=30.0,G=30.0,B=30.0,A=1.0)"   # the core's emissive, 100 in the pack
OFF_EMITTERS = ("NE_Haze",)                   # every system: no haze (overdraw)
OFF_RENDERERS = (("", "Light"),               # every system: no per-particle lights
                 ("NE_Lead", "Sprite"))       # the leads' sprites are 0 cm: drawn for nothing (the leads only guide the ribbons)
# Ribbons untessellated: their points come 99 times a second from the leads, close enough to draw straight between.
RENDERER_PROPERTIES = (("NE_Ribbon", "Ribbon", "TessellationMode", "Disabled"),)
# The GPU sparks were nearly all the ritual's remaining cost (Mac Low: 57.5 FPS with them, 60.6 without), and halving
# them changed nothing: the GPU simulation's own passes cost it. On the CPU instead (a worker thread), half their life from
# half as far (they still reach the core), so half as many to simulate.
CPU_EMITTERS = ("NE_Particulate",)
SPARKS = {"NE_Particulate.InitializeParticle.Lifetime": "0.5", "NE_Particulate.SphereLocation.Sphere Radius": "100.0"}
VARIANTS = {   # our system: (the pack's, {constant suffix: value})
    "NS_Fab_CorePulse": ("NS_Absorption_Teal", {}),
    "NS_Fab_CorePulse_Reduced": ("NS_Absorption_Teal", {"NE_Core.InitializeParticle.Color": DIM_CORE}),
    "NS_Fab_AltarRitual": ("NS_Absorption_Purple", {}),
    "NS_Fab_AltarRitual_Reduced": ("NS_Absorption_Purple", {"NE_Core.InitializeParticle.Color": DIM_CORE}),
    "NS_Fab_CorePurified": ("NS_Absorption_Yellow", {}),
    "NS_Fab_CorePurified_Reduced": ("NS_Absorption_Yellow", {"NE_Core.InitializeParticle.Color": DIM_CORE}),
    # The blast's core goes with its ribbons (5 s in the pack: a ball would hang where the mine was).
    "NS_Fab_MineBlast": ("NS_Absorption_Yellow", {"NE_Core.InitializeParticle.Lifetime": "0.8"}),
}
# (system, reduced system or None, scale, offset, seconds). The pack's systems are some 3 m across.
EFFECTS = {
    (ALTAR, "ritual_fx"): ("NS_Fab_AltarRitual", "NS_Fab_AltarRitual_Reduced", 0.35, (0.0, 0.0, 0.0), 0.0),
    (ALTAR, "purified_fx"): ("NS_Fab_CorePurified", None, 0.6, (0.0, 0.0, 0.0), 1.2),
    (CORE, "default_aura"): ("NS_Fab_CorePulse", "NS_Fab_CorePulse_Reduced", 0.12, (0.0, 0.0, 0.0), 0.0),
    (CORE, "looks.aura"): ("NS_Fab_CorePurified", "NS_Fab_CorePurified_Reduced", 0.18, (0.0, 0.0, 0.0), 0.0),
    (MINE, "blast_fx"): ("NS_Fab_MineBlast", None, 0.4, (0.0, 0.0, 30.0), 0.4),
}

assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_fab_fx: " + message)


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset " + path)
    return asset


def save(asset):
    if not assets.save_loaded_asset(asset, False):
        raise RuntimeError("Could not save " + asset.get_path_name())


def digest(*specs):
    return hashlib.sha1(json.dumps(specs, sort_keys=True).encode("ascii")).hexdigest()


def system(name):
    return load((FX_DIR if name in VARIANTS else FAB_FX) + "/" + name)


def ensure_variants():
    for name, (source, constants) in sorted(VARIANTS.items()):
        path = FX_DIR + "/" + name
        constants = dict(SPARKS, **constants)
        spec_hash = digest(VERSION, source, constants, OFF_EMITTERS, OFF_RENDERERS, RENDERER_PROPERTIES, CPU_EMITTERS)
        if assets.does_asset_exist(path):
            copy = load(path)
            if assets.get_metadata_tag(copy, SPEC_TAG) == spec_hash:
                continue
            # Changed in place: a deleted asset the data still references keeps its name in memory, so a copy to that
            # name fails (KnownIssues AK-10). Every step below only sets values, so it is the same as on a fresh copy.
        else:
            copy = assets.duplicate_asset(FAB_FX + "/" + source, path)
        check(copy is not None, "%s copied from %s" % (name, source))
        for suffix, value in sorted(constants.items()):
            check(unreal.HollowNiagaraAuthoring.set_constant(copy, suffix, value) > 0, "%s: %s set" % (name, suffix))
        for emitter in OFF_EMITTERS:
            check(unreal.HollowNiagaraAuthoring.set_emitter_enabled(copy, emitter, False), "%s: %s off" % (name, emitter))
        for emitter in CPU_EMITTERS:
            check(unreal.HollowNiagaraAuthoring.set_emitter_on_gpu(copy, emitter, False), "%s: %s on the CPU" % (name, emitter))
        for emitter, renderer in OFF_RENDERERS:
            unreal.HollowNiagaraAuthoring.set_renderer_enabled(copy, emitter, renderer, False)   # 0 when already off (an in-place update)
        for emitter, renderer, prop, value in RENDERER_PROPERTIES:
            unreal.HollowNiagaraAuthoring.set_renderer_property(copy, emitter, renderer, prop, value)
        check(unreal.HollowNiagaraAuthoring.compile(copy), name + " compiles")
        assets.set_metadata_tag(copy, SPEC_TAG, spec_hash)
        save(copy)
        log("%s: %s with %s" % (name, source, constants))


def make_spec(system_name, reduced_name, scale, offset, seconds):
    spec = unreal.HollowFxSpec()
    spec.set_editor_property("system", system(system_name))
    if reduced_name:
        spec.set_editor_property("reduced_flashes_system", system(reduced_name))
    spec.set_editor_property("scale", scale)
    spec.set_editor_property("offset", unreal.Vector(*offset))
    spec.set_editor_property("seconds", seconds)
    return spec


def ensure_effects():
    for path in sorted({p for p, _ in EFFECTS}):
        definition = load(path)
        changed = []
        for (owner, field), values in sorted(EFFECTS.items()):
            if owner != path:
                continue
            wanted = make_spec(*values)
            if field == "looks.aura":   # every look of the core (the purified one)
                looks = list(definition.get_editor_property("looks"))
                check(looks, path + " has its purified look (create_cathedral_mechanics.py)")
                if any(look.get_editor_property("aura").export_text() != wanted.export_text() for look in looks):
                    for look in looks:
                        look.set_editor_property("aura", wanted)
                    definition.set_editor_property("looks", looks)
                    changed.append(field)
            elif definition.get_editor_property(field).export_text() != wanted.export_text():
                definition.set_editor_property(field, wanted)
                changed.append(field)
        if changed:
            save(definition)
            log("%s: %s" % (path.rsplit("/", 1)[1], ", ".join(changed)))


def validate():
    for name in VARIANTS:
        copy = load(FX_DIR + "/" + name)
        check(isinstance(copy, unreal.NiagaraSystem), name + " is a Niagara system")
        lines = unreal.HollowNiagaraAuthoring.describe_renderers(copy)
        for emitter in OFF_EMITTERS:
            check("emitter %s off" % emitter in lines, "%s: %s off" % (name, emitter))
        for emitter, renderer in OFF_RENDERERS:
            check(not any(line.startswith("renderer " + (emitter or "")) and renderer in line and line.endswith(" on") for line in lines),
                  "%s: its %s %s renderers off" % (name, emitter or "every", renderer))
    for (path, field), (system_name, reduced_name, scale, _, seconds) in EFFECTS.items():
        definition = load(path)
        specs = [look.get_editor_property("aura") for look in definition.get_editor_property("looks")] if field == "looks.aura" \
            else [definition.get_editor_property(field)]
        for spec in specs:
            check(spec.get_editor_property("system") == system(system_name), "%s %s plays %s" % (path, field, system_name))
            reduced = spec.get_editor_property("reduced_flashes_system")
            check(reduced == (system(reduced_name) if reduced_name else None), "%s %s: its Reduced Flashes choice" % (path, field))
    log("validation passed")


def main():
    ensure_variants()
    ensure_effects()
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_fab_fx failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
