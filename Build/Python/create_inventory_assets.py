"""Creates the S5-01 raid inventory content (idempotent, ADR-0010, ADR-0030).

  /Game/Hollow/GAS/Effects/GE_Tool_Refuel             instant: one fuel cell's fuel into ToolFuel
  /Game/Hollow/GAS/Effects/GE_Cooldown_ToolRefuel     UHollowToolRefuelCooldownEffect: Hollow.Cooldown.Tool.Refuel
  /Game/Hollow/Tools/Refuel/GA_ToolRefuel             UHollowGA_ToolRefuel with those (costs one fuel cell per use)
  /Game/Hollow/Tools/Refuel/DA_AbilitySet_FuelCell    GA_ToolRefuel on Hollow.Input.Reload
  IA_Reload (R, gamepad face button top) in IMC_Raid and DA_InputConfig_Raid (Hollow.Input.Reload)
  DA_Item_* AbilitySet                                what each item does in a raid: the welder, the plasma cutter, the pulse
                                                      mine and the fuel cell grant their sets; the Aether core nothing

Then validates the whole chain: every raid item's set grants its ability on its input, the suit's set
(DA_AbilitySet_ExoFrame) grants none of them, and DefaultGame.ini's starting loadout is made of raid items within their
stacks. Creates or fills only what is missing or differs.
Run last (after create_tool_assets.py, create_raid_flow_assets.py and create_gadget_assets.py):
  Build/Scripts/run-editor-python.sh Build/Python/create_inventory_assets.py
"""
import re
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

EFFECTS_DIR = "/Game/Hollow/GAS/Effects"
REFUEL_EFFECT = EFFECTS_DIR + "/GE_Tool_Refuel"
REFUEL_COOLDOWN = EFFECTS_DIR + "/GE_Cooldown_ToolRefuel"
REFUEL_DIR = "/Game/Hollow/Tools/Refuel"
REFUEL_ABILITY = REFUEL_DIR + "/GA_ToolRefuel"
FUEL_CELL_SET = REFUEL_DIR + "/DA_AbilitySet_FuelCell"
ACTIONS_DIR = "/Game/Hollow/Core/Input/Actions"
IMC_PATH = "/Game/Hollow/Core/Input/IMC_Raid"
INPUT_CONFIG_PATH = "/Game/Hollow/Core/Input/DA_InputConfig_Raid"
SUIT_SET = "/Game/Hollow/Characters/ExoFrame/DA_AbilitySet_ExoFrame"
ITEMS_DIR = "/Game/Hollow/Items"
ATTRIBUTE_MODULE = "/Script/HollowGAS"
INI = "Config/DefaultGame.ini"

# S5-01 clarification 1: R and the gamepad's top face button (both free in IMC_Raid).
RELOAD_INPUT = ("IA_Reload", "Hollow.Input.Reload", ["R", "Gamepad_FaceButton_Top"])
FUEL_PER_CELL = 50.0         # half of the suit's tank (MaxToolFuel 100, GE_ExoFrame_InitAttributes)
REFUEL_COOLDOWN_SECONDS = 1.0   # a held or repeated key burns no second cell
CELLS_PER_USE = 1            # UHollowGA_ToolRefuel's item cost (its C++ default)

# What each item does in a raid (ADR-0030): item -> its ability set and the ability that set grants on its input.
ITEM_SETS = {
    "Tools/DA_Item_Welder": ("/Game/Hollow/Tools/Welder/DA_AbilitySet_Welder", "/Game/Hollow/Tools/Welder/GA_Tool_Welder", "Hollow.Input.SecondaryUse"),
    "Tools/DA_Item_PlasmaCutter": ("/Game/Hollow/Tools/PlasmaCutter/DA_AbilitySet_PlasmaCutter", "/Game/Hollow/Tools/PlasmaCutter/GA_Tool_PlasmaCutter",
                                   "Hollow.Input.PrimaryUse"),
    "Consumables/DA_Item_PulseMine": ("/Game/Hollow/Gadgets/PulseMine/DA_AbilitySet_PulseMine", "/Game/Hollow/Gadgets/PulseMine/GA_PulseMine",
                                      "Hollow.Input.Gadget"),
    "Consumables/DA_Item_FuelCell": (FUEL_CELL_SET, REFUEL_ABILITY, RELOAD_INPUT[1]),
}
NO_RAID_USE = ["AetherCore/DA_Item_AetherCore"]   # loot: taken out, never into a raid
RAID_ITEM_TYPES = ["Hollow.Item.Type.Tool", "Hollow.Item.Type.Consumable"]   # UHollowEconomySettings::LoadoutItemTypes

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_inventory_assets: " + message)


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


def make_key(name):
    return from_text(unreal.Key(), name)


def same(a, b):
    return a.export_text() == b.export_text() if hasattr(a, "export_text") else a == b


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


def path_of(asset):
    return asset.get_path_name() if asset is not None else ""


# ---------------------------------------------------------------- refuel

def ensure_refuel_effect():
    effect_bp, created = blueprint(REFUEL_EFFECT, unreal.GameplayEffect)
    effect = unreal.get_default_object(effect_bp.generated_class())
    wanted = [modifier("HollowToolSet", "ToolFuel", scalable(FUEL_PER_CELL))]
    current = effect.get_editor_property("modifiers")
    if created or effect.get_editor_property("duration_policy") != unreal.GameplayEffectDurationType.INSTANT \
            or len(current) != 1 or not same(current[0], wanted[0]):
        effect.set_editor_property("duration_policy", unreal.GameplayEffectDurationType.INSTANT)
        effect.set_editor_property("modifiers", wanted)
        save(effect_bp)
        log("GE_Tool_Refuel: +%.0f ToolFuel" % FUEL_PER_CELL)
    return effect_bp.generated_class()


def ensure_refuel_cooldown():
    effect_bp, created = blueprint(REFUEL_COOLDOWN, unreal.HollowToolRefuelCooldownEffect)
    effect = unreal.get_default_object(effect_bp.generated_class())
    wanted = from_text(unreal.GameplayEffectModifierMagnitude(), scalable(REFUEL_COOLDOWN_SECONDS))
    if created or not same(effect.get_editor_property("duration_magnitude"), wanted):
        effect.set_editor_property("duration_magnitude", wanted)
        save(effect_bp)
        log("GE_Cooldown_ToolRefuel: %.2f s" % REFUEL_COOLDOWN_SECONDS)
    return effect_bp.generated_class()


def ensure_refuel_ability(refuel, cooldown):
    ability_bp, created = blueprint(REFUEL_ABILITY, unreal.HollowGA_ToolRefuel)
    ability = unreal.get_default_object(ability_bp.generated_class())
    changed = created
    for name, value in (("refuel_effect", refuel), ("cooldown_gameplay_effect_class", cooldown)):
        if ability.get_editor_property(name) != value:
            ability.set_editor_property(name, value)
            changed = True
    if changed:
        save(ability_bp)
        log("GA_ToolRefuel ready")
    return ability_bp.generated_class()


# ---------------------------------------------------------------- input

def mapped_keys(context, action):
    mappings = context.get_editor_property("default_key_mappings").get_editor_property("mappings")
    return [m.get_editor_property("key").export_text() for m in mappings if m.get_editor_property("action") == action]


def ensure_input():
    action_name, tag_name, keys = RELOAD_INPUT
    path = ACTIONS_DIR + "/" + action_name
    if not assets.does_asset_exist(path):
        action = create(path, unreal.InputAction, unreal.InputAction_Factory())
        action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        save(action)
        log("created " + action_name)
    action = load(path)
    context = load(IMC_PATH)
    data = context.get_editor_property("default_key_mappings")
    mappings = list(data.get_editor_property("mappings"))
    if not any(m.get_editor_property("action") == action for m in mappings):
        for key_name in keys:
            entry = unreal.EnhancedActionKeyMapping()
            entry.set_editor_property("action", action)
            entry.set_editor_property("key", make_key(key_name))
            mappings.append(entry)
        data.set_editor_property("mappings", mappings)
        context.set_editor_property("default_key_mappings", data)
        save(context)
        log("mapped %s to %s in IMC_Raid" % (action_name, " and ".join(keys)))
    config = load(INPUT_CONFIG_PATH)
    entries = list(config.get_editor_property("ability_actions"))
    tag = make_tag(tag_name)
    if not any(same(e.get_editor_property("input_tag"), tag) for e in entries):
        entry = unreal.HollowInputAction()
        entry.set_editor_property("input_action", action)
        entry.set_editor_property("input_tag", tag)
        entries.append(entry)
        config.set_editor_property("ability_actions", entries)
        save(config)
        log("added %s to DA_InputConfig_Raid" % tag_name)
    return action


# ---------------------------------------------------------------- item -> abilities

def ensure_fuel_cell_set(ability_class):
    cell_set, _ = data_asset(FUEL_CELL_SET, unreal.HollowAbilitySet)
    granted = cell_set.get_editor_property("granted_abilities")
    if len(granted) == 1 and granted[0].get_editor_property("ability") == ability_class \
            and same(granted[0].get_editor_property("input_tag"), make_tag(RELOAD_INPUT[1])):
        return
    entry = unreal.HollowAbilitySetAbility()
    entry.set_editor_property("ability", ability_class)
    entry.set_editor_property("input_tag", make_tag(RELOAD_INPUT[1]))
    cell_set.set_editor_property("granted_abilities", [entry])
    save(cell_set)
    log("DA_AbilitySet_FuelCell: GA_ToolRefuel on " + RELOAD_INPUT[1])


def ensure_item_links():
    for relative, (set_path, _, _) in ITEM_SETS.items():
        item = load(ITEMS_DIR + "/" + relative)
        wanted = load(set_path)
        if path_of(item.get_editor_property("ability_set")) != path_of(wanted):
            item.set_editor_property("ability_set", wanted)
            save(item)
            log("%s grants %s in a raid" % (relative.rsplit("/", 1)[1], set_path.rsplit("/", 1)[1]))


# ---------------------------------------------------------------- validation

def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def starting_loadout():
    """(item name, count) of every +StartingLoadout line of DefaultGame.ini."""
    with open(unreal.Paths.project_dir() + INI) as ini:
        text = ini.read()
    return [(name, int(count)) for name, count in re.findall(r"^\+StartingLoadout=\(Item=HollowItem:(\w+),Count=(\d+)", text, re.MULTILINE)]


def validate(refuel_class, action):
    ability = unreal.get_default_object(refuel_class)
    check(ability.get_editor_property("refuel_effect") == load(REFUEL_EFFECT).generated_class(), "GA_ToolRefuel refuel effect")
    check(ability.get_editor_property("cooldown_gameplay_effect_class") == load(REFUEL_COOLDOWN).generated_class(), "GA_ToolRefuel cooldown")
    check(ability.get_editor_property("item_cost_per_use") == CELLS_PER_USE, "GA_ToolRefuel costs one fuel cell")
    suit = [path_of(e.get_editor_property("ability")) for e in load(SUIT_SET).get_editor_property("granted_abilities")]
    by_name = {}
    for relative, (set_path, ability_path, input_tag) in ITEM_SETS.items():
        item = load(ITEMS_DIR + "/" + relative)
        by_name[relative.rsplit("/", 1)[1]] = item
        check(path_of(item.get_editor_property("ability_set")) == path_of(load(set_path)), relative + " links its ability set")
        granted = load(set_path).get_editor_property("granted_abilities")
        ability_class = load(ability_path).generated_class()
        check(len(granted) == 1 and granted[0].get_editor_property("ability") == ability_class
              and same(granted[0].get_editor_property("input_tag"), make_tag(input_tag)), "%s grants %s on %s" % (set_path, ability_path, input_tag))
        check(path_of(ability_class) not in suit, "the suit's set does not grant " + ability_path)
    for relative in NO_RAID_USE:
        check(load(ITEMS_DIR + "/" + relative).get_editor_property("ability_set") is None, relative + " does nothing in a raid")
    for name, count in starting_loadout():
        item = by_name.get(name)
        check(item is not None, "starting loadout item %s is a raid item" % name)
        check(item.get_editor_property("item_type").export_text() in [make_tag(t).export_text() for t in RAID_ITEM_TYPES], name + " has a raid type")
        check(1 <= count <= item.get_editor_property("max_stack"), "starting loadout %s x%d within its stack" % (name, count))
    context = load(IMC_PATH)
    check(sorted(mapped_keys(context, action)) == sorted(make_key(k).export_text() for k in RELOAD_INPUT[2]), "IA_Reload keys")
    for key_name in RELOAD_INPUT[2]:
        key = make_key(key_name).export_text()
        users = [m.get_editor_property("action").get_name() for m in context.get_editor_property("default_key_mappings").get_editor_property("mappings")
                 if m.get_editor_property("key").export_text() == key]
        check(users == [RELOAD_INPUT[0]], "%s belongs to the reload only in IMC_Raid (also: %s)" % (key_name, ", ".join(users)))
    config = load(INPUT_CONFIG_PATH)
    check(any(e.get_editor_property("input_action") == action and same(e.get_editor_property("input_tag"), make_tag(RELOAD_INPUT[1]))
              for e in config.get_editor_property("ability_actions")), "DA_InputConfig_Raid routes the reload input")
    log("validation passed")


def main():
    refuel = ensure_refuel_effect()
    cooldown = ensure_refuel_cooldown()
    refuel_class = ensure_refuel_ability(refuel, cooldown)
    action = ensure_input()
    ensure_fuel_cell_set(refuel_class)
    ensure_item_links()
    validate(refuel_class, action)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_inventory_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
