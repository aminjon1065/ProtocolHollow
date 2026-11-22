"""Creates the S6-03 ritual ward of the Techno-Occultist (idempotent, ADR-0010, ADR-0036).

  /Game/Hollow/GAS/Effects/GE_Ward_Bind              UHollowWardBindEffect: an enemy inside, slowed
  /Game/Hollow/GAS/Effects/GE_Ward_Calm              UHollowWardCalmEffect: a player inside, slower Resonance
  /Game/Hollow/GAS/Effects/GE_Ward_Drain             instant: Hollow.Data.ResonanceDrain into Resonance (each check)
  /Game/Hollow/GAS/Effects/GE_Cooldown_OccultWard    UHollowOccultWardCooldownEffect: Hollow.Cooldown.Gadget.OccultWard
  /Game/Hollow/FX/Ward/M_FX_RuneCircle               procedural deferred decal: two rings and eight runes, no texture
  /Game/Hollow/Gadgets/OccultWard/
      DA_OccultWard                                  UHollowOccultWardDefinition (life, radius, checks, effects, look, hum)
      BP_OccultWard                                  AHollowOccultWard: the rune decal and a small shadowless light
      GA_OccultWard                                  UHollowGA_OccultWard: ward class and cooldown; costs one ward item per use
      DA_AbilitySet_OccultWard                       GA_OccultWard on Hollow.Input.Gadget (the pulse mine's input)
  /Game/Hollow/Items/Consumables/DA_Item_OccultWard  the item (consumable) granting that set in a raid

Checked here, made elsewhere: the hum (create_audio_assets.py). The recipe and the Occultist's starting loadout are
create_economy_assets.py's and create_class_assets.py's. Every asset is created when missing and refreshed only when it
differs from the spec below; the material is rebuilt only when its spec changes (its hash in the asset's metadata).
Run after create_lantern_assets.py, before create_economy_assets.py and create_class_assets.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_ward_assets.py
"""
import hashlib
import json
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

EFFECTS_DIR = "/Game/Hollow/GAS/Effects"
WARD_DIR = "/Game/Hollow/Gadgets/OccultWard"
BIND_PATH = EFFECTS_DIR + "/GE_Ward_Bind"
CALM_PATH = EFFECTS_DIR + "/GE_Ward_Calm"
DRAIN_PATH = EFFECTS_DIR + "/GE_Ward_Drain"
COOLDOWN_PATH = EFFECTS_DIR + "/GE_Cooldown_OccultWard"
MATERIAL_PATH = "/Game/Hollow/FX/Ward/M_FX_RuneCircle"
DEFINITION_PATH = WARD_DIR + "/DA_OccultWard"
WARD_BP = WARD_DIR + "/BP_OccultWard"
ABILITY_PATH = WARD_DIR + "/GA_OccultWard"
SET_PATH = WARD_DIR + "/DA_AbilitySet_OccultWard"
ITEM_PATH = "/Game/Hollow/Items/Consumables/DA_Item_OccultWard"
SOUND_BANK = "/Game/Hollow/Audio/DA_SoundBank"
ATTRIBUTE_MODULE = "/Script/HollowGAS"
INPUT_TAG = "Hollow.Input.Gadget"          # the pulse mine's: the shared gadget cooldown keeps one press to one gadget
HUM_TAG = "Hollow.Sound.Ward.Hum"
SPEC_TAG = "HollowSpec"
MAX_CLEAR_PASSES = 16   # each pass halves the graph at least

# S6-03 clarifications 2 and 5 (data, balance later). Effects: (attribute set, attribute, op, magnitude).
LINGER_SECONDS = 0.5                       # the bind and the calm linger this long after leaving the circle
BIND_MODIFIERS = [("HollowMovementSet", "MoveSpeedMultiplier", "MultiplyCompound", 0.5)]   # x0.3 with the lantern's x0.6
CALM_MODIFIERS = [("HollowResonanceSet", "ResonanceGainMultiplier", "MultiplyCompound", 0.5)]
COOLDOWN_SECONDS = 20.0                    # as the pulse mine's
WARD = {
    "life_seconds": 15.0, "radius": 250.0, "height_tolerance": 200.0, "check_seconds": 0.25,
    "resonance_drain_per_second": 1.0, "place_trace_distance": 100.0,
    "fade_in_seconds": 0.6, "pulse_seconds": 3.0, "pulse_hz": 2.0,
}
ITEM = {"display_name": "Ritual ward", "item_type": "Hollow.Item.Type.Consumable", "base_value": 40, "insurable": False, "max_stack": 3}
WARDS_PER_USE = 1                          # UHollowGA_OccultWard's item cost (its C++ default)
# The look (greybox data): the decal's depth, the light over it, the material's colour and brightness.
STRENGTH_PARAMETER = "Strength"
WARD_BP_VALUES = {"strength_parameter": STRENGTH_PARAMETER, "look_update_hz": 15.0}
DECAL_DEPTH = 60.0                         # cm above and below the floor point; its width is the definition's radius
GLOW = {"intensity": 600.0, "attenuation_radius": 350.0}
GLOW_COLOR = (170, 110, 255)
GLOW_OFFSET = (0.0, 0.0, 40.0)
# Version 2: brighter and wider, readable on the bright greybox floor (the first smoke's frames). Version 3: rebuilt on a
# clean graph (see clear_graph).
RUNE = {"version": 3, "color": (0.62, 0.38, 1.0), "strength": 12.0, "base_tint": 0.35,
        "outer": (0.93, 0.05), "inner": (0.62, 0.035), "runes": 8, "rune_width": 0.09, "rune_band": (0.68, 0.84), "rune_edge": 0.025}

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
mel = unreal.MaterialEditingLibrary


def log(message):
    unreal.log("create_ward_assets: " + message)


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


def from_text(struct, text):
    if not struct.import_text(text):
        raise RuntimeError("Could not import " + text)
    return struct


def make_tag(name):
    return from_text(unreal.GameplayTag(), '(TagName="%s")' % name)


def text_of(value):
    """A comparable form of a property value (structs and arrays by their text, objects by their path)."""
    if isinstance(value, (list, tuple, unreal.Array)):
        return [text_of(v) for v in value]
    if isinstance(value, unreal.Text):
        return str(value)
    if hasattr(value, "export_text"):
        return value.export_text()
    if hasattr(value, "get_path_name"):
        return value.get_path_name()
    if isinstance(value, float):
        return round(value, 4)
    return value


def refresh(obj, wanted):
    """Sets every wanted property that differs; True if any did."""
    changed = False
    for key, value in wanted.items():
        if text_of(obj.get_editor_property(key)) != text_of(value):
            obj.set_editor_property(key, value)
            changed = True
    return changed


def digest(*specs):
    return hashlib.sha1(json.dumps(specs, sort_keys=True, default=str).encode("ascii")).hexdigest()


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


# ---------------------------------------------------------------- effects

def attribute_text(set_name, attribute):
    path = "%s.%s:%s" % (ATTRIBUTE_MODULE, set_name, attribute)
    return path, '(AttributeName="%s",Attribute=%s,AttributeOwner="/Script/CoreUObject.Class\'%s.%s\'")' % (attribute, path, ATTRIBUTE_MODULE, set_name)


def make_modifier(set_name, attribute, op, magnitude_text):
    # Modifier fields are EditDefaultsOnly, so the struct is built from text (as create_lantern_assets.py does).
    path, attribute_struct = attribute_text(set_name, attribute)
    modifier = unreal.GameplayModifierInfo()
    if not modifier.import_text("(Attribute=%s,ModifierOp=%s,ModifierMagnitude=%s)" % (attribute_struct, op, magnitude_text)) \
            or path not in modifier.export_text() or ("ModifierOp=" + op) not in modifier.export_text():
        raise RuntimeError("Could not build modifier %s %s.%s" % (op, set_name, attribute))
    return modifier


def scalable(value):
    return "(MagnitudeCalculationType=ScalableFloat,ScalableFloatMagnitude=(Value=%.6f))" % value


def set_by_caller(tag):
    return '(MagnitudeCalculationType=SetByCaller,SetByCallerMagnitude=(DataTag=(TagName="%s")))' % tag


def duration(seconds):
    return from_text(unreal.GameplayEffectModifierMagnitude(), scalable(seconds))


def ensure_effect(path, parent, wanted, describe):
    effect_bp, created = blueprint(path, parent)
    effect = unreal.get_default_object(effect_bp.generated_class())
    if refresh(effect, wanted) or created:
        save(effect_bp)
        log("%s: %s" % (path.rsplit("/", 1)[1], describe))
    return effect_bp.generated_class()


def ensure_effects():
    def area(modifiers):
        return {"duration_magnitude": duration(LINGER_SECONDS), "modifiers": [make_modifier(s, a, op, scalable(v)) for s, a, op, v in modifiers]}
    bind = ensure_effect(BIND_PATH, unreal.HollowWardBindEffect, area(BIND_MODIFIERS), "%.1f s, speed x%.2f" % (LINGER_SECONDS, BIND_MODIFIERS[0][3]))
    calm = ensure_effect(CALM_PATH, unreal.HollowWardCalmEffect, area(CALM_MODIFIERS), "%.1f s, Resonance gain x%.2f" % (LINGER_SECONDS, CALM_MODIFIERS[0][3]))
    drain = ensure_effect(DRAIN_PATH, unreal.GameplayEffect, {
        "duration_policy": unreal.GameplayEffectDurationType.INSTANT,
        "modifiers": [make_modifier("HollowResonanceSet", "Resonance", "AddBase", set_by_caller("Hollow.Data.ResonanceDrain"))]},
        "instant, Hollow.Data.ResonanceDrain into Resonance")
    cooldown = ensure_effect(COOLDOWN_PATH, unreal.HollowOccultWardCooldownEffect, {"duration_magnitude": duration(COOLDOWN_SECONDS)},
                             "%.0f s" % COOLDOWN_SECONDS)
    return bind, calm, drain, cooldown


# ---------------------------------------------------------------- the rune circle (material)

class Graph(object):
    """Places material expressions in columns and wires them."""

    def __init__(self, material):
        self.material = material
        self.row = 0

    def node(self, expression_class, column, **properties):
        expression = mel.create_material_expression(self.material, expression_class, -260 * column, 110 * self.row)
        self.row += 1
        for name, value in properties.items():
            expression.set_editor_property(name, value)
        return expression

    def wire(self, source, target, target_input=""):
        if not mel.connect_material_expressions(source, "", target, target_input):
            raise RuntimeError("Could not connect %s -> %s.%s" % (source.get_name(), target.get_name(), target_input))

    def math(self, expression_class, column, a, b=None, const_b=None):
        expression = self.node(expression_class, column, **({} if const_b is None else {"const_b": const_b}))
        self.wire(a, expression, "A")
        if b is not None:
            self.wire(b, expression, "B")
        return expression

    def single(self, expression_class, column, source, **properties):
        expression = self.node(expression_class, column, **properties)
        self.wire(source, expression)
        return expression

    def add(self, column, a, b=None, const=None):
        return self.math(unreal.MaterialExpressionAdd, column, a, b, const)

    def sub(self, column, a, b=None, const=None):
        return self.math(unreal.MaterialExpressionSubtract, column, a, b, const)

    def mul(self, column, a, b=None, const=None):
        return self.math(unreal.MaterialExpressionMultiply, column, a, b, const)

    def saturate(self, column, source):
        return self.single(unreal.MaterialExpressionSaturate, column, source)

    def ramp(self, column, value, start, span):
        """saturate((value - start) / span); a negative span ramps down."""
        return self.saturate(column, self.mul(column + 1, self.sub(column + 2, value, const=start), const=1.0 / span))

    def band(self, column, value, centre, width):
        """1 at centre, 0 from width away: 1 - saturate(|value - centre| / width)."""
        distance = self.single(unreal.MaterialExpressionAbs, column + 2, self.sub(column + 3, value, const=centre))
        return self.single(unreal.MaterialExpressionOneMinus, column, self.saturate(column + 1, self.mul(column + 2, distance, const=1.0 / width)))


def build_rune(material):
    r = RUNE
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    g = Graph(material)
    # Where on the circle: d = 0 in the middle, 1 at the radius; the angle round it.
    centred = g.sub(12, g.node(unreal.MaterialExpressionTextureCoordinate, 13, coordinate_index=0), const=0.5)
    d = g.mul(10, g.single(unreal.MaterialExpressionLength, 11, centred), const=2.0)
    angle = g.node(unreal.MaterialExpressionArctangent2, 10)
    g.wire(g.single(unreal.MaterialExpressionComponentMask, 11, centred, r=False, g=True, b=False, a=False), angle, "Y")
    g.wire(g.single(unreal.MaterialExpressionComponentMask, 11, centred, r=True, g=False, b=False, a=False), angle, "X")
    # Two rings, and runes: a mark in the middle of each of the eight sectors, between the rings.
    outer = g.band(6, d, r["outer"][0], r["outer"][1])
    inner = g.band(6, d, r["inner"][0], r["inner"][1])
    sector = g.single(unreal.MaterialExpressionFrac, 8, g.mul(9, angle, const=r["runes"] / (2.0 * 3.141592653589793)))
    mark = g.band(6, sector, 0.5, r["rune_width"])
    between = g.mul(5, g.ramp(6, d, r["rune_band"][0], r["rune_edge"]), g.ramp(6, d, r["rune_band"][1], -r["rune_edge"]))
    pattern = g.saturate(3, g.add(4, g.add(5, outer, inner), g.mul(5, mark, between)))
    colour = g.node(unreal.MaterialExpressionVectorParameter, 3, parameter_name="Color",
                    default_value=unreal.LinearColor(r["color"][0], r["color"][1], r["color"][2], 1.0))
    strength = g.node(unreal.MaterialExpressionScalarParameter, 3, parameter_name=STRENGTH_PARAMETER, default_value=r["strength"])
    emissive = g.mul(1, g.mul(2, colour, strength), pattern)
    base = g.mul(1, colour, const=r["base_tint"])
    for source, prop in ((emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR), (pattern, unreal.MaterialProperty.MP_OPACITY),
                         (base, unreal.MaterialProperty.MP_BASE_COLOR)):
        if not mel.connect_material_property(source, "", prop):
            raise RuntimeError("Could not connect M_FX_RuneCircle")


def clear_graph(material):
    """delete_all_material_expressions removes about half of the expressions per call (UE 5.8): call it until none is left."""
    for _ in range(MAX_CLEAR_PASSES):
        if mel.get_num_material_expressions(material) == 0:
            return
        mel.delete_all_material_expressions(material)
    raise RuntimeError("Could not clear the graph of " + material.get_name())


def ensure_material():
    if assets.does_asset_exist(MATERIAL_PATH):
        material = load(MATERIAL_PATH)
        if assets.get_metadata_tag(material, SPEC_TAG) == digest(RUNE):
            return material
    else:
        material = create(MATERIAL_PATH, unreal.Material, unreal.MaterialFactoryNew())
    clear_graph(material)
    build_rune(material)
    mel.recompile_material(material)
    assets.set_metadata_tag(material, SPEC_TAG, digest(RUNE))
    save(material)
    log("M_FX_RuneCircle built (%d expressions)" % mel.get_num_material_expressions(material))
    return material


# ---------------------------------------------------------------- definition, ward, ability, set, item

def ensure_definition(bind, calm, drain):
    definition, created = data_asset(DEFINITION_PATH, unreal.HollowOccultWardDefinition)
    wanted = dict(WARD, enemy_effect=bind, player_effect=calm, drain_effect=drain, hum_sound=make_tag(HUM_TAG))
    if refresh(definition, wanted) or created:
        save(definition)
        log("DA_OccultWard: %.0f s, radius %.0f cm, every %.2f s" % (WARD["life_seconds"], WARD["radius"], WARD["check_seconds"]))
    return definition


def ensure_ward(definition, material):
    ward_bp, created = blueprint(WARD_BP, unreal.HollowOccultWard)
    ward = unreal.get_default_object(ward_bp.generated_class())
    changed = refresh(ward, dict(WARD_BP_VALUES, definition=definition))
    rune = ward.get_editor_property("rune")
    changed = refresh(rune, {"decal_material": material, "decal_size": unreal.Vector(DECAL_DEPTH, WARD["radius"], WARD["radius"])}) or changed
    glow = ward.get_editor_property("glow")
    changed = refresh(glow, dict(GLOW, light_color=unreal.Color(r=GLOW_COLOR[0], g=GLOW_COLOR[1], b=GLOW_COLOR[2], a=255),
                                 relative_location=unreal.Vector(*GLOW_OFFSET))) or changed
    if changed or created:
        save(ward_bp)
        log("BP_OccultWard ready")
    return ward_bp.generated_class()


def ensure_ability(ward_class, cooldown):
    ability_bp, created = blueprint(ABILITY_PATH, unreal.HollowGA_OccultWard)
    ability = unreal.get_default_object(ability_bp.generated_class())
    if refresh(ability, {"ward_class": ward_class, "cooldown_gameplay_effect_class": cooldown}) or created:
        save(ability_bp)
        log("GA_OccultWard ready")
    return ability_bp.generated_class()


def ensure_ability_set(ability_class):
    ability_set, created = data_asset(SET_PATH, unreal.HollowAbilitySet)
    entry = unreal.HollowAbilitySetAbility()
    entry.set_editor_property("ability", ability_class)
    entry.set_editor_property("input_tag", make_tag(INPUT_TAG))
    if refresh(ability_set, {"granted_abilities": [entry]}) or created:
        save(ability_set)
        log("DA_AbilitySet_OccultWard: the ward on " + INPUT_TAG)
    return ability_set


def ensure_item(ability_set):
    item, created = data_asset(ITEM_PATH, unreal.HollowItemDefinition)
    changed = refresh(item, dict(ITEM, display_name=unreal.Text(ITEM["display_name"]), item_type=make_tag(ITEM["item_type"])))
    if text_of(item.get_editor_property("ability_set")) != text_of(ability_set):
        item.set_editor_property("ability_set", ability_set)
        changed = True
    if changed or created:
        save(item)
        log("DA_Item_OccultWard: %d cr, up to %d a slot, grants the ward in a raid" % (ITEM["base_value"], ITEM["max_stack"]))


# ---------------------------------------------------------------- validation

def validate():
    validator = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
    definition = load(DEFINITION_PATH)
    result = validator.is_object_valid(definition, unreal.DataValidationUsecase.SCRIPT)
    check(result[0] != unreal.DataValidationResult.INVALID, "DA_OccultWard is valid: %s" % [str(e) for e in result[1]])
    ward_class = load(WARD_BP).generated_class()
    ward = unreal.get_default_object(ward_class)
    check(ward.get_editor_property("definition") == definition, "BP_OccultWard uses DA_OccultWard")
    check(ward.get_editor_property("rune").get_editor_property("decal_material") == load(MATERIAL_PATH), "BP_OccultWard shows M_FX_RuneCircle")
    check(load(MATERIAL_PATH).get_editor_property("material_domain") == unreal.MaterialDomain.MD_DEFERRED_DECAL, "M_FX_RuneCircle is a decal")
    ability_class = load(ABILITY_PATH).generated_class()
    ability = unreal.get_default_object(ability_class)
    check(ability.get_editor_property("ward_class") == ward_class, "GA_OccultWard lays BP_OccultWard")
    check(ability.get_editor_property("cooldown_gameplay_effect_class") == load(COOLDOWN_PATH).generated_class(), "GA_OccultWard cooldown")
    check(ability.get_editor_property("item_cost_per_use") == WARDS_PER_USE, "GA_OccultWard costs one ward")
    granted = load(SET_PATH).get_editor_property("granted_abilities")
    check(len(granted) == 1 and granted[0].get_editor_property("ability") == ability_class, "the set grants the ward")
    item = load(ITEM_PATH)
    check(text_of(item.get_editor_property("ability_set")) == text_of(load(SET_PATH)), "DA_Item_OccultWard grants the set")
    check(unreal.SystemLibrary.get_primary_asset_id_from_object(item).export_text().endswith("DA_Item_OccultWard"), "item primary asset id")
    hum = make_tag(HUM_TAG).export_text()
    check(any(e.get_editor_property("sound_tag").export_text() == hum for e in load(SOUND_BANK).get_editor_property("entries")),
          "DA_SoundBank has the ward's hum (create_audio_assets.py)")
    log("validation passed")


def main():
    bind, calm, drain, cooldown = ensure_effects()
    material = ensure_material()
    definition = ensure_definition(bind, calm, drain)
    ensure_item(ensure_ability_set(ensure_ability(ensure_ward(definition, material), cooldown)))
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_ward_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
