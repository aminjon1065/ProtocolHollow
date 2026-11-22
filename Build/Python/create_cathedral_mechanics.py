"""Creates the Reactor Cathedral's mechanics (idempotent, ADR-0010, ADR-0039): the Purification Altar and the electric arcs.

  /Game/Hollow/Items/AetherCore/DA_Item_PurifiedCore         UHollowItemDefinition: the purified core, 2.5 x a core's value
  /Game/Hollow/Items/AetherCore/MI_AetherCore_Purified       its glow (M_Hollow_Emissive)
  /Game/Hollow/Items/AetherCore/DA_Carryable_AetherCore      Looks: a core counting as the purified one wears that glow
  /Game/Hollow/GAS/Effects/GE_Damage_Electric                UHollowDamageEffect, Hollow.Damage.Electric (armour does not apply)
  /HollowSector_Cathedral/Mechanics/DA_RitualAltar           UHollowRitualAltarDefinition: 20 s, a toll every 2 s, +40 heat
  /HollowSector_Cathedral/Mechanics/BP_RitualAltar           AHollowRitualAltar: greybox stone altar, a light over the bowl
  /HollowSector_Cathedral/Mechanics/DA_ArcHazard             UHollowArcHazardDefinition: 1.5 s warning, 1 s strike, 3.5 s rest, 25 damage
  /HollowSector_Cathedral/Mechanics/BP_ArcHazard             AHollowArcHazard: emitters on the cloister's shafts, three arcs across
  /HollowSector_Cathedral/Mechanics/BP_ArcBreaker            AHollowSwitch (S1-06): the arc's breaker on the wall, on by default
  /HollowSector_Cathedral/Mechanics/MI_Altar_Stone, MI_Arc_Emitter (M_Hollow_Surface), MI_Arc_Beam (M_Hollow_Emissive)
  /HollowSector_Cathedral/Data/DA_Biome_Cathedral            the Cathedral's own socket rules, after the ones mirrored from the
                                                             Docks: one altar (farthest from Spawn), up to two arcs

Material instances only (no new shader). Data is set when created and checked afterwards (tune it in the editor; the
validation keeps it within the task's numbers). Creates or fills only what is missing, then validates.
Run after create_cathedral_greybox.py (last in the order):
  Build/Scripts/run-editor-python.sh Build/Python/create_cathedral_mechanics.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

MECHANICS_DIR = "/HollowSector_Cathedral/Mechanics"
BIOME = "/HollowSector_Cathedral/Data/DA_Biome_Cathedral"
ALTAR_DATA = MECHANICS_DIR + "/DA_RitualAltar"
ALTAR_BP = MECHANICS_DIR + "/BP_RitualAltar"
ARC_DATA = MECHANICS_DIR + "/DA_ArcHazard"
ARC_BP = MECHANICS_DIR + "/BP_ArcHazard"
BREAKER_BP = MECHANICS_DIR + "/BP_ArcBreaker"
STONE_MI = MECHANICS_DIR + "/MI_Altar_Stone"
EMITTER_MI = MECHANICS_DIR + "/MI_Arc_Emitter"
BEAM_MI = MECHANICS_DIR + "/MI_Arc_Beam"
CORE_DIR = "/Game/Hollow/Items/AetherCore"
CORE_ITEM = CORE_DIR + "/DA_Item_AetherCore"
PURIFIED_ITEM = CORE_DIR + "/DA_Item_PurifiedCore"
PURIFIED_MI = CORE_DIR + "/MI_AetherCore_Purified"
CORE_DEFINITION = CORE_DIR + "/DA_Carryable_AetherCore"
ELECTRIC_EFFECT = "/Game/Hollow/GAS/Effects/GE_Damage_Electric"
SURFACE = "/Game/Hollow/World/Materials/M_Hollow_Surface"     # create_lighting_assets.py
EMISSIVE = "/Game/Hollow/World/Materials/M_Hollow_Emissive"
CUBE_MESH = "/Engine/BasicShapes/Cube.Cube"
CUBE_SIZE = 100.0
HAZARD_MI = "/Game/Hollow/World/Materials/MI_Hazard"           # the breaker wears hazard stripes

# The purified core (S7-02 task: x2.5, 1250 instead of 500).
PURIFIED_VALUE_FACTOR = 2.5
PURIFIED_NAME = "Purified Aether core"
CORE_TYPE = "Hollow.Item.Type.AetherCore"

# Material instances: parent, scalars, vectors (linear colours).
# S7-03 (ADR-0040): kept in step with this spec; the altar's stone is the Cathedral's (create_cathedral_look.py palette).
INSTANCES = {
    STONE_MI: (SURFACE, {"Metallic": 0.05, "Roughness": 0.85, "PanelSize": 25.0, "GrimeAmount": 0.5}, {"BaseColor": (0.12, 0.12, 0.14)}),
    EMITTER_MI: (SURFACE, {"Metallic": 0.9, "Roughness": 0.4, "PanelSize": 25.0}, {"BaseColor": (0.35, 0.18, 0.08)}),
    BEAM_MI: (EMISSIVE, {"Strength": 25.0}, {"Color": (0.55, 0.8, 1.0)}),
    PURIFIED_MI: (EMISSIVE, {"Strength": 6.0}, {"Color": (0.85, 0.5, 1.0)}),
}


def piece(x, y, z, sx, sy, sz, yaw=0.0):
    """An engine cube centred at (x, y, z) sized (sx, sy, sz) uu, turned yaw degrees: an instance transform."""
    return unreal.Transform(location=unreal.Vector(x, y, z), rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw),
                            scale=unreal.Vector(sx / CUBE_SIZE, sy / CUBE_SIZE, sz / CUBE_SIZE))


# The altar (its frame: X towards the player who uses it, origin on the floor). A dais, a table with the bowl, a
# reredos behind, four candle pillars; the core (70 high) rests on the bowl.
ALTAR = {
    "ritual_seconds": 20.0, "pulse_seconds": 2.0, "noise_tag": "Hollow.Noise.Ritual", "heat_burst": 40.0,
    "purified_sound": "Hollow.Sound.Altar.Purified",   # S7-03
    "place_prompt": "Lay the core on the altar",
    "ritual_glow": 0.35, "flash_seconds": 0.4, "spent_glow": 0.6,
}
ALTAR_SEAT = unreal.Transform(location=unreal.Vector(-10.0, 0.0, 143.0), rotation=unreal.Rotator(0.0, 0.0, 0.0), scale=unreal.Vector(1.0, 1.0, 1.0))
ALTAR_PIECES = [
    piece(0.0, 0.0, 10.0, 160.0, 120.0, 20.0),         # dais
    piece(-10.0, 0.0, 60.0, 90.0, 70.0, 80.0),         # table
    piece(-10.0, 0.0, 103.0, 70.0, 70.0, 6.0),         # bowl
    piece(-75.0, 0.0, 150.0, 10.0, 110.0, 260.0),      # reredos
    piece(65.0, 48.0, 80.0, 12.0, 12.0, 120.0), piece(65.0, -48.0, 80.0, 12.0, 12.0, 120.0),     # candle pillars
    piece(-55.0, 48.0, 80.0, 12.0, 12.0, 120.0), piece(-55.0, -48.0, 80.0, 12.0, 12.0, 120.0),
]
ALTAR_LIGHT = {"relative_location": unreal.Vector(-10.0, 0.0, 230.0), "intensity": 3000.0, "attenuation_radius": 700.0,
               "light_color": unreal.Color(r=255, g=196, b=120, a=255)}

# The arc (its frame: X along the passage, Y across, origin on the floor between the cloister's shafts, which stand
# 160 uu either side). Emitters on the shafts, three arcs across, a 140 uu deep zone wall to wall, 3 m high.
ARC = {"telegraph_seconds": 1.5, "discharge_seconds": 1.0, "idle_seconds": 3.5, "damage": 25.0, "crackle_hz": 12.0, "telegraph_glow": 0.4}
ARC_SOUNDS = {"telegraph_sound": "Hollow.Sound.Arc.Telegraph", "discharge_sound": "Hollow.Sound.Arc.Discharge"}   # S7-03
ARC_ZONE = unreal.Vector(70.0, 180.0, 150.0)
ARC_HEIGHTS = (60.0, 150.0, 240.0)
ARC_EMITTERS = [piece(0.0, side * 155.0, z, 40.0, 10.0, 30.0) for side in (-1.0, 1.0) for z in ARC_HEIGHTS]
ARC_BEAMS = [piece(0.0, 0.0, z, 8.0, 300.0, 8.0) for z in ARC_HEIGHTS]
# The breaker on the wall 2.5 m back along the passage (outside the zone), facing into the passage.
ARC_BREAKER = unreal.Transform(location=unreal.Vector(-250.0, -172.0, 120.0), rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0),
                               scale=unreal.Vector(1.0, 1.0, 1.0))
ARC_LIGHT = {"relative_location": unreal.Vector(0.0, 0.0, 200.0), "intensity": 8000.0, "attenuation_radius": 500.0,
             "light_color": unreal.Color(r=150, g=205, b=255, a=255)}
BREAKER = {"prompt_turn_on": "Restore the power", "prompt_turn_off": "Cut the power", "is_on": True}
BREAKER_SCALE = unreal.Vector(0.15, 0.4, 0.6)      # a 15 x 40 x 60 cm box on the wall (engine cube)
BREAKER_LAMP = {"relative_location": unreal.Vector(80.0, 0.0, 60.0), "intensity": 400.0, "attenuation_radius": 150.0,
                "light_color": unreal.Color(r=255, g=60, b=40, a=255), "cast_shadows": False}

# The Cathedral's own socket rules (after the mirrored Docks ones): (socket, blueprint, max per sector, pick).
SOCKET_RULES = [
    ("Hollow.Socket.Altar", ALTAR_BP, 1, "FARTHEST_FROM_SPAWN"),
    ("Hollow.Socket.ArcHazard", ARC_BP, 2, "MANIFEST_ORDER"),
]

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
mel = unreal.MaterialEditingLibrary


def log(message):
    unreal.log("create_cathedral_mechanics: " + message)


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


def data_asset(path, asset_class):
    if assets.does_asset_exist(path):
        return load(path), False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    return create(path, asset_class, factory), True


def make_tag(name):
    tag = unreal.GameplayTag()
    if not tag.import_text('(TagName="%s")' % name):
        raise RuntimeError("Unknown gameplay tag " + name)
    return tag


def tag_set(tag):
    """An empty tag exports without a name (its TagName is None)."""
    text = tag.export_text()
    return 'TagName="' in text and 'TagName="None"' not in text and 'TagName=""' not in text


def same(a, b):
    return a.export_text() == b.export_text() if hasattr(a, "export_text") else a == b


def item_id(path):
    asset_id = unreal.PrimaryAssetId()
    if not asset_id.import_text("HollowItem:" + path.rsplit("/", 1)[1]):
        raise RuntimeError("Bad item id for " + path)
    return asset_id


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


# ---------------------------------------------------------------- materials, item, effect

def ensure_instances():
    for path, (parent_path, scalars, vectors) in sorted(INSTANCES.items()):
        created = not assets.does_asset_exist(path)
        instance = create(path, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew()) if created else load(path)
        changed = created
        if instance.get_editor_property("parent") != load(parent_path):
            mel.set_material_instance_parent(instance, load(parent_path))
            changed = True
        for name, value in scalars.items():
            if abs(mel.get_material_instance_scalar_parameter_value(instance, name) - value) > 1e-4:
                mel.set_material_instance_scalar_parameter_value(instance, name, value)
                changed = True
        for name, rgb in vectors.items():
            current = mel.get_material_instance_vector_parameter_value(instance, name)
            if any(abs(a - b) > 1e-4 for a, b in zip((current.r, current.g, current.b), rgb)):
                mel.set_material_instance_vector_parameter_value(instance, name, unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
                changed = True
        if changed:
            mel.update_material_instance(instance)
            save(instance)
            log(path.rsplit("/", 1)[1] + " ready")


def ensure_purified_item():
    item, created = data_asset(PURIFIED_ITEM, unreal.HollowItemDefinition)
    if not created:
        return
    core = load(CORE_ITEM)
    item.set_editor_property("display_name", unreal.Text(PURIFIED_NAME))
    item.set_editor_property("item_type", make_tag(CORE_TYPE))
    item.set_editor_property("base_value", int(round(core.get_editor_property("base_value") * PURIFIED_VALUE_FACTOR)))
    item.set_editor_property("insurable", False)
    item.set_editor_property("max_stack", 1)
    save(item)
    log("created DA_Item_PurifiedCore")


def ensure_core_look():
    definition = load(CORE_DEFINITION)
    looks = list(definition.get_editor_property("looks"))
    purified = item_id(PURIFIED_ITEM)
    if any(same(look.get_editor_property("loot_item"), purified) for look in looks):
        return
    look = unreal.HollowCarryableLook()
    look.set_editor_property("loot_item", purified)
    look.set_editor_property("material", load(PURIFIED_MI))
    looks.append(look)
    definition.set_editor_property("looks", looks)
    save(definition)
    log("DA_Carryable_AetherCore: a purified core glows MI_AetherCore_Purified")


def ensure_electric_effect():
    effect_bp, created = blueprint(ELECTRIC_EFFECT, unreal.HollowDamageEffect)
    effect = unreal.get_default_object(effect_bp.generated_class())
    if created or not same(effect.get_editor_property("damage_type"), make_tag("Hollow.Damage.Electric")):
        effect.set_editor_property("damage_type", make_tag("Hollow.Damage.Electric"))
        save(effect_bp)
        log("GE_Damage_Electric ready")
    return effect_bp.generated_class()


# ---------------------------------------------------------------- altar, arc, breaker

def set_light(light, values):
    for key, value in values.items():
        light.set_editor_property(key, value)


def ensure_altar():
    definition, created = data_asset(ALTAR_DATA, unreal.HollowRitualAltarDefinition)
    if created:
        definition.set_editor_property("accepted_item", item_id(CORE_ITEM))
        definition.set_editor_property("result_item", item_id(PURIFIED_ITEM))
        definition.set_editor_property("noise_tag", make_tag(ALTAR["noise_tag"]))
        definition.set_editor_property("place_prompt", unreal.Text(ALTAR["place_prompt"]))
        for key in ("ritual_seconds", "pulse_seconds", "heat_burst", "ritual_glow", "flash_seconds", "spent_glow"):
            definition.set_editor_property(key, ALTAR[key])
        definition.set_editor_property("seat", ALTAR_SEAT)
        definition.set_editor_property("pieces", ALTAR_PIECES)
        save(definition)
        log("created DA_RitualAltar")
    if not tag_set(definition.get_editor_property("purified_sound")):   # S7-03 added it: filled once, then the editor's
        definition.set_editor_property("purified_sound", make_tag(ALTAR["purified_sound"]))
        save(definition)
        log("DA_RitualAltar: the purified sound")
    altar_bp, created = blueprint(ALTAR_BP, unreal.HollowRitualAltar)
    if created:
        altar = unreal.get_default_object(altar_bp.generated_class())
        altar.set_editor_property("definition", definition)
        pieces = altar.get_editor_property("pieces")
        pieces.set_editor_property("static_mesh", load(CUBE_MESH))
        pieces.set_editor_property("override_materials", [load(STONE_MI)])
        set_light(altar.get_editor_property("glow"), ALTAR_LIGHT)
        save(altar_bp)
        log("created BP_RitualAltar")
    return altar_bp


def ensure_breaker():
    breaker_bp, created = blueprint(BREAKER_BP, unreal.HollowSwitch)
    if created:
        breaker = unreal.get_default_object(breaker_bp.generated_class())
        breaker.set_editor_property("prompt_turn_on", unreal.Text(BREAKER["prompt_turn_on"]))
        breaker.set_editor_property("prompt_turn_off", unreal.Text(BREAKER["prompt_turn_off"]))
        breaker.set_editor_property("is_on", BREAKER["is_on"])
        mesh = breaker.get_editor_property("mesh")
        mesh.set_editor_property("static_mesh", load(CUBE_MESH))
        mesh.set_editor_property("relative_scale3d", BREAKER_SCALE)
        mesh.set_editor_property("override_materials", [load(HAZARD_MI)])
        set_light(breaker.get_editor_property("lamp"), BREAKER_LAMP)
        save(breaker_bp)
        log("created BP_ArcBreaker")
    return breaker_bp


def ensure_arc(electric_class, breaker_bp):
    definition, created = data_asset(ARC_DATA, unreal.HollowArcHazardDefinition)
    if created:
        for key in ("telegraph_seconds", "discharge_seconds", "idle_seconds", "damage", "crackle_hz", "telegraph_glow"):
            definition.set_editor_property(key, ARC[key])
        definition.set_editor_property("zone_extent", ARC_ZONE)
        definition.set_editor_property("damage_effect", electric_class)
        definition.set_editor_property("breaker_class", breaker_bp.generated_class())
        definition.set_editor_property("breaker_offset", ARC_BREAKER)
        definition.set_editor_property("emitters", ARC_EMITTERS)
        definition.set_editor_property("beams", ARC_BEAMS)
        save(definition)
        log("created DA_ArcHazard")
    if not tag_set(definition.get_editor_property("telegraph_sound")):   # S7-03 added them: filled once, then the editor's
        for key, tag in ARC_SOUNDS.items():
            definition.set_editor_property(key, make_tag(tag))
        save(definition)
        log("DA_ArcHazard: the warning and strike sounds")
    arc_bp, created = blueprint(ARC_BP, unreal.HollowArcHazard)
    if created:
        arc = unreal.get_default_object(arc_bp.generated_class())
        arc.set_editor_property("definition", definition)
        for name, material in (("emitters", EMITTER_MI), ("beams", BEAM_MI)):
            component = arc.get_editor_property(name)
            component.set_editor_property("static_mesh", load(CUBE_MESH))
            component.set_editor_property("override_materials", [load(material)])
        set_light(arc.get_editor_property("glow"), ARC_LIGHT)
        save(arc_bp)
        log("created BP_ArcHazard")
    return arc_bp


def ensure_socket_rules():
    biome = load(BIOME)
    rules = list(biome.get_editor_property("socket_actors"))
    changed = False
    for socket, path, max_per_sector, pick in SOCKET_RULES:
        tag = make_tag(socket)
        actor_class = load(path).generated_class()
        if any(same(r.get_editor_property("socket_type"), tag) and r.get_editor_property("actor_class") == actor_class for r in rules):
            continue
        rule = unreal.HollowSocketActorRule()
        rule.set_editor_property("socket_type", tag)
        rule.set_editor_property("actor_class", actor_class)
        rule.set_editor_property("max_per_sector", max_per_sector)
        rule.set_editor_property("pick", getattr(unreal.HollowSocketPick, pick))
        rules.append(rule)   # after the rules mirrored from the Docks (create_cathedral_greybox.py keeps them first)
        changed = True
    if changed:
        biome.set_editor_property("socket_actors", rules)
        save(biome)
        log("DA_Biome_Cathedral: an altar and the arcs at their sockets")


# ---------------------------------------------------------------- validation

def validate(electric_class):
    core = load(CORE_ITEM)
    purified = load(PURIFIED_ITEM)
    check(purified.get_editor_property("base_value") == int(round(core.get_editor_property("base_value") * PURIFIED_VALUE_FACTOR)),
          "a purified core is worth %.1f cores" % PURIFIED_VALUE_FACTOR)
    check(not purified.get_editor_property("insurable") and same(purified.get_editor_property("item_type"), make_tag(CORE_TYPE)), "DA_Item_PurifiedCore")
    looks = load(CORE_DEFINITION).get_editor_property("looks")
    check(any(same(l.get_editor_property("loot_item"), item_id(PURIFIED_ITEM)) and l.get_editor_property("material") is not None for l in looks),
          "the purified core's look")
    check(same(unreal.get_default_object(electric_class).get_editor_property("damage_type"), make_tag("Hollow.Damage.Electric")), "GE_Damage_Electric")

    altar_data = load(ALTAR_DATA)
    check(same(altar_data.get_editor_property("accepted_item"), item_id(CORE_ITEM))
          and same(altar_data.get_editor_property("result_item"), item_id(PURIFIED_ITEM)), "the altar purifies cores")
    check(altar_data.get_editor_property("ritual_seconds") > 0.0 and 0.0 < altar_data.get_editor_property("pulse_seconds") <= altar_data.get_editor_property("ritual_seconds")
          and same(altar_data.get_editor_property("noise_tag"), make_tag(ALTAR["noise_tag"])), "the ritual's times and noise")
    check(len(altar_data.get_editor_property("pieces")) > 0, "the altar's pieces")
    check(tag_set(altar_data.get_editor_property("purified_sound")), "the altar's purified sound")
    altar = unreal.get_default_object(load(ALTAR_BP).generated_class())
    check(altar.get_editor_property("definition") == altar_data and altar.get_editor_property("pieces").get_editor_property("static_mesh") is not None,
          "BP_RitualAltar")

    arc_data = load(ARC_DATA)
    check(min(arc_data.get_editor_property(k) for k in ("telegraph_seconds", "discharge_seconds", "idle_seconds")) > 0.0
          and arc_data.get_editor_property("damage") > 0.0, "the arc's cycle and damage")
    check(all(tag_set(arc_data.get_editor_property(key)) for key in ARC_SOUNDS), "the arc's sounds")
    check(arc_data.get_editor_property("damage_effect") == electric_class
          and arc_data.get_editor_property("breaker_class") == load(BREAKER_BP).generated_class(), "the arc's effect and breaker")
    breaker = unreal.get_default_object(load(BREAKER_BP).generated_class())
    check(isinstance(breaker, unreal.HollowSwitch) and breaker.get_editor_property("is_on"), "BP_ArcBreaker is a switch, on by default")
    arc = unreal.get_default_object(load(ARC_BP).generated_class())
    check(arc.get_editor_property("definition") == arc_data, "BP_ArcHazard")

    rules = load(BIOME).get_editor_property("socket_actors")
    for socket, path, max_per_sector, _ in SOCKET_RULES:
        actor_class = load(path).generated_class()
        check(any(same(r.get_editor_property("socket_type"), make_tag(socket)) and r.get_editor_property("actor_class") == actor_class
                  and r.get_editor_property("max_per_sector") == max_per_sector for r in rules), "the biome rule for " + socket)
    required = load(BIOME).get_editor_property("gen_rules").get_editor_property("required_socket_types")
    for socket, _, _, _ in SOCKET_RULES:
        check('"%s"' % socket in required.export_text(), "every Cathedral sector has " + socket)
    log("validation passed (altar 20 s, arcs 6 s, purified core %d cr)" % purified.get_editor_property("base_value"))


def main():
    ensure_instances()
    ensure_purified_item()
    ensure_core_look()
    electric_class = ensure_electric_effect()
    ensure_altar()
    breaker_bp = ensure_breaker()
    ensure_arc(electric_class, breaker_bp)
    ensure_socket_rules()
    validate(electric_class)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_cathedral_mechanics failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
