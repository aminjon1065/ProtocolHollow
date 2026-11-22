"""Creates the S6-04 occult relics (idempotent, ADR-0010, ADR-0037).

  /Game/Hollow/Relics/GE_Relic_<Name>                infinite: one gear multiplier (MultiplyCompound) while the relic is carried
  /Game/Hollow/Relics/DA_AbilitySet_Relic_<Name>     grants that effect, nothing else
  /Game/Hollow/Items/Relics/DA_Item_Relic_<Name>     the item: a relic (Hollow.Item.Type.Relic) in the relic equip slot,
                                                     insurable, one a slot; its set is the raid inventory's while carried (ADR-0030)

The four relics: Ring of Silence (footstep noise x0.7), Resonance Stabilizer (Resonance gain x0.75), Plasma Capacitor
(cutter and welder work x1.2), Kinetic Brace (carry speed x1.15). Their recipes and the Reliquary (a second relic slot)
are create_economy_assets.py's; the equip slot limits and relic raid type are config (DefaultGame.ini), checked here.
Every asset is created when missing and refreshed only when it differs from the spec below.
Run after create_ward_assets.py, before create_economy_assets.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_relic_assets.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

RELICS_DIR = "/Game/Hollow/Relics"
ITEMS_DIR = "/Game/Hollow/Items/Relics"
ATTRIBUTE_MODULE = "/Script/HollowGAS"
INI = "Config/DefaultGame.ini"
INI_LINES = ['+EquipSlotLimits=(Slot=(TagName="Hollow.Item.Slot.%s"),Limit=1)' % s for s in ("Primary", "Secondary", "Gadget", "Relic")]
RELIC_TYPE = "Hollow.Item.Type.Relic"
RELIC_SLOT = "Hollow.Item.Slot.Relic"

# S6-04 clarifications 4 and 7 (data, balance later): name -> (display name, attribute set, attribute, multiplier).
RELICS = {
    "RingOfSilence": ("Ring of Silence", "HollowGearSet", "FootstepNoiseMultiplier", 0.7),
    "ResonanceStabilizer": ("Resonance Stabilizer", "HollowResonanceSet", "ResonanceGainMultiplier", 0.75),
    "PlasmaCapacitor": ("Plasma Capacitor", "HollowGearSet", "ToolWorkMultiplier", 1.2),
    "KineticBrace": ("Kinetic Brace", "HollowGearSet", "CarrySpeedMultiplier", 1.15),
}
ITEM = {"base_value": 150, "insurable": True, "max_stack": 1}   # sells for 75, insured for 45 (DA_EconomyConfig percents)

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_relic_assets: " + message)


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


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def make_modifier(set_name, attribute, value):
    # Modifier fields are EditDefaultsOnly, so the struct is built from text (as create_lantern_assets.py does).
    path = "%s.%s:%s" % (ATTRIBUTE_MODULE, set_name, attribute)
    text = ('(Attribute=(AttributeName="{attr}",Attribute={path},AttributeOwner="/Script/CoreUObject.Class\'{module}.{set}\'"),'
            'ModifierOp=MultiplyCompound,ModifierMagnitude=(MagnitudeCalculationType=ScalableFloat,ScalableFloatMagnitude=(Value={value:.6f})))'
            ).format(attr=attribute, path=path, module=ATTRIBUTE_MODULE, set=set_name, value=value)
    modifier = unreal.GameplayModifierInfo()
    if not modifier.import_text(text) or path not in modifier.export_text() or "ModifierOp=MultiplyCompound" not in modifier.export_text():
        raise RuntimeError("Could not build modifier on %s.%s" % (set_name, attribute))
    return modifier


# ---------------------------------------------------------------- relics

def ensure_relic(name, spec):
    display, set_name, attribute, value = spec
    effect_bp, created = blueprint("%s/GE_Relic_%s" % (RELICS_DIR, name), unreal.GameplayEffect)
    effect = unreal.get_default_object(effect_bp.generated_class())
    if refresh(effect, {"duration_policy": unreal.GameplayEffectDurationType.INFINITE, "modifiers": [make_modifier(set_name, attribute, value)]}) or created:
        save(effect_bp)
        log("GE_Relic_%s: %s x%.2f" % (name, attribute, value))

    ability_set, created = data_asset("%s/DA_AbilitySet_Relic_%s" % (RELICS_DIR, name), unreal.HollowAbilitySet)
    entry = unreal.HollowAbilitySetEffect()
    entry.set_editor_property("gameplay_effect", effect_bp.generated_class())
    if refresh(ability_set, {"granted_abilities": [], "granted_effects": [entry]}) or created:
        save(ability_set)
        log("DA_AbilitySet_Relic_%s: its effect while carried" % name)

    item, created = data_asset("%s/DA_Item_Relic_%s" % (ITEMS_DIR, name), unreal.HollowItemDefinition)
    wanted = dict(ITEM, display_name=unreal.Text(display), item_type=make_tag(RELIC_TYPE), equip_slot=make_tag(RELIC_SLOT))
    changed = refresh(item, wanted)
    if text_of(item.get_editor_property("ability_set")) != text_of(ability_set):
        item.set_editor_property("ability_set", ability_set)
        changed = True
    if changed or created:
        save(item)
        log("DA_Item_Relic_%s: %s, %d cr, insurable" % (name, display, ITEM["base_value"]))


# ---------------------------------------------------------------- validation

def validate():
    with open(unreal.Paths.project_dir() + INI) as ini:
        text = ini.read()
    lines = set(line.strip() for line in text.splitlines())
    for line in INI_LINES:
        check(line in lines, "DefaultGame.ini: " + line)
    check('(TagName="%s")' % RELIC_TYPE in text, "DefaultGame.ini: relics are raid items (LoadoutItemTypes)")
    for name, (display, set_name, attribute, value) in RELICS.items():
        effect = unreal.get_default_object(load("%s/GE_Relic_%s" % (RELICS_DIR, name)).generated_class())
        check(effect.get_editor_property("duration_policy") == unreal.GameplayEffectDurationType.INFINITE, name + " lasts while carried")
        modifiers = effect.get_editor_property("modifiers")
        check(len(modifiers) == 1 and attribute in modifiers[0].export_text(), name + " modifies " + attribute)
        effects = load("%s/DA_AbilitySet_Relic_%s" % (RELICS_DIR, name)).get_editor_property("granted_effects")
        check(len(effects) == 1, name + "'s set grants its effect")
        item = load("%s/DA_Item_Relic_%s" % (ITEMS_DIR, name))
        check(unreal.SystemLibrary.get_primary_asset_id_from_object(item).export_text().endswith("DA_Item_Relic_" + name), name + " primary asset id")
        check(item.get_editor_property("equip_slot").export_text() == make_tag(RELIC_SLOT).export_text(), name + " takes a relic slot")
    log("validation passed")


def main():
    for name, spec in RELICS.items():
        ensure_relic(name, spec)
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_relic_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
