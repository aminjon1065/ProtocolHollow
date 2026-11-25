"""Creates the GP-01 weapons (idempotent, ADR-0010, ADR-0044).

  /Game/Hollow/Localization/ST_Items                     the weapons' and rounds' names (LOC-01, hollow_loc.py)
  /Game/Hollow/GAS/Effects/GE_Weapon_Cost                instant: one round out of the magazine (the fire abilities' cost)
  /Game/Hollow/GAS/Effects/GE_Weapon_Load                instant: Hollow.Data.AmmoLoaded rounds into the magazine (the reloads)
  /Game/Hollow/GAS/Effects/GE_Hands_Swap                 UHollowHandsSwapEffect: the hands' busy moment (GA_SwapHands' cooldown)
  /Game/Hollow/GAS/Effects/GE_Cooldown_Melee             UHollowMeleeCooldownEffect (GA_MeleeBash's cooldown)
  /Game/Hollow/GAS/Effects/GE_Status_Stagger             UHollowStunnedEffect: a short stun from a strong push (enemies)
  /Game/Hollow/GAS/Cues/GCN_Weapon_Fire                  UHollowWeaponFireCue: report, muzzle flash, tracers, impacts
  /Game/Hollow/Weapons/<Name>/DA_Weapon_<Name>           UHollowWeaponDefinition (the art kit binds its mesh and effects)
  /Game/Hollow/Weapons/<Name>/GE_Weapon_Init_<Name>      instant: the magazine's size, full (granted with the weapon)
  /Game/Hollow/Weapons/<Name>/GE_Cooldown_Weapon_<Name>  UHollowWeaponFireCooldownEffect: the server's floor under its rate of fire
  /Game/Hollow/Weapons/<Name>/GA_Weapon_Fire_<Name>      UHollowGA_WeaponFire (cost GE_Weapon_Cost)
  /Game/Hollow/Weapons/<Name>/GA_Weapon_Reload_<Name>    UHollowGA_WeaponReload (GE_Weapon_Load)
  /Game/Hollow/Weapons/<Name>/DA_AbilitySet_Weapon_<Name>  fire + dry fire on PrimaryUse, aim on SecondaryUse, reload on Reload, its init
  /Game/Hollow/Weapons/GA_Weapon_Aim, GA_Weapon_DryFire  shared by both weapons
  /Game/Hollow/Characters/ExoFrame/GA_SwapHands, GA_MeleeBash   the suit's: three swap specs (Hands.Tools / .Weapon / .Cycle), melee
  /Game/Hollow/Items/Weapons/DA_Item_Weapon_<Name>       the items (Hollow.Item.Type.Weapon, the weapon slot, insurable)
  /Game/Hollow/Items/Ammo/DA_Item_Ammo_<Name>            their rounds (Hollow.Item.Type.Ammo)
  IA_HandsTools (1), IA_HandsWeapon (2), IA_HandsCycle (mouse wheel, gamepad Y), IA_Melee (Q, right stick) in IMC_Raid
  DA_Enemy_IronPilgrim / DA_Enemy_EchoStalker            knockback scale, stagger threshold and GE_Status_Stagger

Numbers are GP-01's starting data (tuned later). The economy (shop, recipes) is create_economy_assets.py's, the classes'
starting loadouts create_class_assets.py's, the meshes and shot effects create_art_kit.py's, the sounds
create_audio_assets.py's, the noise rules create_noise_assets.py's. Every asset is created when missing and refreshed only
when it differs from the spec below, then the whole chain is validated.
Run after create_inventory_assets.py and create_relic_assets.py, before create_economy_assets.py and create_class_assets.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_weapon_assets.py
"""
import os
import sys
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hollow_loc as loc   # LOC-01 (ADR-0048): player texts are string table entries

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

STRING_TABLE = loc.ITEMS   # LOC-01: the weapons' and rounds' names
EFFECTS_DIR = "/Game/Hollow/GAS/Effects"
CUES_DIR = "/Game/Hollow/GAS/Cues"
WEAPONS_DIR = "/Game/Hollow/Weapons"
ITEMS_DIR = "/Game/Hollow/Items"
SUIT_DIR = "/Game/Hollow/Characters/ExoFrame"
SUIT_SET = SUIT_DIR + "/DA_AbilitySet_ExoFrame"
ACTIONS_DIR = "/Game/Hollow/Core/Input/Actions"
IMC_PATH = "/Game/Hollow/Core/Input/IMC_Raid"
INPUT_CONFIG_PATH = "/Game/Hollow/Core/Input/DA_InputConfig_Raid"
DAMAGE_KINETIC = EFFECTS_DIR + "/GE_Damage_Kinetic"
ATTRIBUTE_MODULE = "/Script/HollowGAS"
INI = "Config/DefaultGame.ini"
INI_LINES = ['+EquipSlotLimits=(Slot=(TagName="Hollow.Item.Slot.Weapon"),Limit=1)', "MaxLoadoutSlots=8", "bFriendlyFire=False"]
WEAPON_TYPE, AMMO_TYPE, WEAPON_SLOT = "Hollow.Item.Type.Weapon", "Hollow.Item.Type.Ammo", "Hollow.Item.Slot.Weapon"

# GP-01 starting data. Pilgrim (2000 HP, armour 100, K 100): the shotgun takes 16 blasts, 11 exposed; the nailgun is for
# the light ones. Exposed Stalker (1000 HP, no armour): nailgun 50 rounds, shotgun 6 blasts.
WEAPONS = {
    "Nailgun": {
        "display": ("Weapon.Nailgun.Name", "Nailgun"),
        "tag": "Hollow.Weapon.Nailgun",
        "noise": "Hollow.Noise.Weapon.Nailgun",
        "feel": "Hollow.Feel.Shot.Nailgun",        # GP-01c: DA_FeelConfig (create_feel_assets.py)
        "ammo": "DA_Item_Ammo_Nails",
        "definition": {"fire_interval_seconds": 0.2, "pellets": 1, "spread_degrees": 0.6, "aim_spread_multiplier": 0.35, "range": 4000.0,
                       "damage_per_pellet": 20.0, "falloff_start_range": 2500.0, "falloff_end_range": 4000.0, "min_damage_fraction": 0.6,
                       "armor_penetration": 0.1, "exposed_penetration_bonus": 0.2, "impulse_per_pellet": 150.0,
                       "magazine_size": 12, "reload_seconds": 1.1,
                       "aim_tolerance_degrees": 4.0, "origin_tolerance_cm": 80.0, "bounds_tolerance_cm": 40.0},
        "item": {"base_value": 150, "insurable": True, "max_stack": 1},
    },
    "Shotgun": {
        "display": ("Weapon.Shotgun.Name", "Shotgun"),
        "tag": "Hollow.Weapon.Shotgun",
        "noise": "Hollow.Noise.Weapon.Shotgun",
        "feel": "Hollow.Feel.Shot.Shotgun",
        "ammo": "DA_Item_Ammo_Shells",
        "definition": {"fire_interval_seconds": 0.9, "pellets": 8, "spread_degrees": 5.0, "aim_spread_multiplier": 0.7, "range": 1800.0,
                       "damage_per_pellet": 24.0, "falloff_start_range": 600.0, "falloff_end_range": 1800.0, "min_damage_fraction": 0.35,
                       "armor_penetration": 0.5, "exposed_penetration_bonus": 0.4, "impulse_per_pellet": 175.0,
                       "magazine_size": 4, "reload_seconds": 2.6,
                       "aim_tolerance_degrees": 4.0, "origin_tolerance_cm": 80.0, "bounds_tolerance_cm": 40.0},
        "item": {"base_value": 650, "insurable": True, "max_stack": 1},
    },
}
AMMO = {
    "DA_Item_Ammo_Nails": {"display": ("Item.Ammo.Nails.Name", "Nails"), "item": {"base_value": 1, "insurable": False, "max_stack": 60}},
    "DA_Item_Ammo_Shells": {"display": ("Item.Ammo.Shells.Name", "Shells"), "item": {"base_value": 4, "insurable": False, "max_stack": 24}},
}
COOLDOWN_SHARE = 0.85        # the server's floor: a little under the weapon's interval (packets bunch up)
SWAP_SECONDS = 0.25          # the hands' busy moment
MELEE = {"damage": 25.0, "range": 160.0, "radius": 30.0, "impulse": 600.0, "noise_tag": "Hollow.Noise.Impact.Melee", "feel_tag": "Hollow.Feel.Melee"}
MELEE_COOLDOWN_SECONDS = 0.7
STAGGER_SECONDS = 0.6
# Enemy -> (knockback scale, stagger impulse cm/s): the shotgun's full blast (1400) staggers both, the nailgun neither.
ENEMIES = {
    "/Game/Hollow/Characters/Enemies/IronPilgrim/DA_Enemy_IronPilgrim": (0.25, 300.0),
    "/Game/Hollow/Characters/Enemies/EchoStalker/DA_Enemy_EchoStalker": (0.6, 500.0),
}
# The suit's new inputs (GP-01): action -> (input tag, keys). Keyboard 1 / 2 pick, the mouse wheel and the gamepad's Y switch.
INPUTS = {
    "IA_HandsTools": ("Hollow.Input.Hands.Tools", ["One"]),
    "IA_HandsWeapon": ("Hollow.Input.Hands.Weapon", ["Two"]),
    "IA_HandsCycle": ("Hollow.Input.Hands.Cycle", ["MouseScrollUp", "MouseScrollDown", "Gamepad_FaceButton_Top"]),
    "IA_Melee": ("Hollow.Input.Melee", ["Q", "Gamepad_RightThumbstick"]),
}
SWAP_INPUTS = ["Hollow.Input.Hands.Tools", "Hollow.Input.Hands.Weapon", "Hollow.Input.Hands.Cycle"]

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
localization = unreal.HollowLocalizationAuthoring


def log(message):
    unreal.log("create_weapon_assets: " + message)


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


def item_id(name):
    return from_text(unreal.PrimaryAssetId(), "HollowItem:" + name)


def text_of(value):
    """A comparable form of a property value (structs and arrays by their text, objects by their path)."""
    if isinstance(value, (list, tuple, unreal.Array)):
        return [text_of(v) for v in value]
    if isinstance(value, unreal.Text):
        return [loc.entry(value), str(value)]   # LOC-01: a plain text is not the table's entry
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


def scalable(value):
    return "(MagnitudeCalculationType=ScalableFloat,ScalableFloatMagnitude=(Value=%.6f))" % value


def modifier(set_name, attribute, op, magnitude_text):
    # Modifier fields are EditDefaultsOnly, so the struct is built from text (as create_relic_assets.py does).
    path = "%s.%s:%s" % (ATTRIBUTE_MODULE, set_name, attribute)
    text = ('(Attribute=(AttributeName="{attr}",Attribute={path},AttributeOwner="/Script/CoreUObject.Class\'{module}.{set}\'"),'
            'ModifierOp={op},ModifierMagnitude={magnitude})').format(attr=attribute, path=path, module=ATTRIBUTE_MODULE, set=set_name, op=op,
                                                                    magnitude=magnitude_text)
    result = unreal.GameplayModifierInfo()
    if not result.import_text(text) or path not in result.export_text():
        raise RuntimeError("Could not build modifier on %s.%s" % (set_name, attribute))
    return result


def set_by_caller(tag):
    return '(MagnitudeCalculationType=SetByCaller,SetByCallerMagnitude=(DataTag=(TagName="%s")))' % tag


# ---------------------------------------------------------------- texts

def string_table():
    return loc.table(STRING_TABLE)


def table_text(table, entry):
    key, source = entry
    return localization.table_text(table, key, source)


def is_table_text(text, key):
    entry = localization.get_table_entry(text)   # None for any other text (a bool UFUNCTION with out parameters)
    return entry is not None and str(entry[0]).endswith(STRING_TABLE) and str(entry[1]) == key


# ---------------------------------------------------------------- effects

def ensure_effect(path, parent, wanted, label):
    effect_bp, created = blueprint(path, parent)
    effect = unreal.get_default_object(effect_bp.generated_class())
    if refresh(effect, wanted) or created:
        save(effect_bp)
        log(label)
    return effect_bp.generated_class()


def duration(seconds):
    return from_text(unreal.GameplayEffectModifierMagnitude(), scalable(seconds))


def ensure_shared_effects():
    instant = unreal.GameplayEffectDurationType.INSTANT
    cost = ensure_effect(EFFECTS_DIR + "/GE_Weapon_Cost", unreal.GameplayEffect,
                         {"duration_policy": instant, "modifiers": [modifier("HollowWeaponSet", "Magazine", "Additive", scalable(-1.0))]},
                         "GE_Weapon_Cost: one round")
    load_effect = ensure_effect(EFFECTS_DIR + "/GE_Weapon_Load", unreal.GameplayEffect,
                                {"duration_policy": instant,
                                 "modifiers": [modifier("HollowWeaponSet", "Magazine", "Additive", set_by_caller("Hollow.Data.AmmoLoaded"))]},
                                "GE_Weapon_Load: Hollow.Data.AmmoLoaded rounds")
    swap = ensure_effect(EFFECTS_DIR + "/GE_Hands_Swap", unreal.HollowHandsSwapEffect, {"duration_magnitude": duration(SWAP_SECONDS)},
                         "GE_Hands_Swap: %.2f s" % SWAP_SECONDS)
    melee = ensure_effect(EFFECTS_DIR + "/GE_Cooldown_Melee", unreal.HollowMeleeCooldownEffect, {"duration_magnitude": duration(MELEE_COOLDOWN_SECONDS)},
                          "GE_Cooldown_Melee: %.2f s" % MELEE_COOLDOWN_SECONDS)
    stagger = ensure_effect(EFFECTS_DIR + "/GE_Status_Stagger", unreal.HollowStunnedEffect, {"duration_magnitude": duration(STAGGER_SECONDS)},
                            "GE_Status_Stagger: %.2f s" % STAGGER_SECONDS)
    return cost, load_effect, swap, melee, stagger


# ---------------------------------------------------------------- weapons

def ensure_ability(path, parent, wanted, label):
    ability_bp, created = blueprint(path, parent)
    ability = unreal.get_default_object(ability_bp.generated_class())
    if refresh(ability, wanted) or created:
        save(ability_bp)
        log(label)
    return ability_bp.generated_class()


def ability_entry(ability_class, input_tag):
    entry = unreal.HollowAbilitySetAbility()
    entry.set_editor_property("ability", ability_class)
    entry.set_editor_property("input_tag", make_tag(input_tag))
    return entry


def effect_entry(effect_class):
    entry = unreal.HollowAbilitySetEffect()
    entry.set_editor_property("gameplay_effect", effect_class)
    return entry


def ensure_weapon(name, spec, table, cost, load_effect, aim, dry_fire):
    directory = "%s/%s" % (WEAPONS_DIR, name)
    numbers = spec["definition"]
    definition, created = data_asset("%s/DA_Weapon_%s" % (directory, name), unreal.HollowWeaponDefinition)
    wanted = dict(numbers, weapon_tag=make_tag(spec["tag"]), display_name=table_text(table, spec["display"]), noise_tag=make_tag(spec["noise"]),
                  feel_tag=make_tag(spec["feel"]), ammo_item=item_id(spec["ammo"]), damage_effect=load(DAMAGE_KINETIC).generated_class())
    if refresh(definition, wanted) or created:
        save(definition)
        log("DA_Weapon_%s: %d x %.0f damage, %d rounds" % (name, numbers["pellets"], numbers["damage_per_pellet"], numbers["magazine_size"]))

    size = float(numbers["magazine_size"])
    init = ensure_effect("%s/GE_Weapon_Init_%s" % (directory, name), unreal.GameplayEffect,
                         {"duration_policy": unreal.GameplayEffectDurationType.INSTANT,
                          "modifiers": [modifier("HollowWeaponSet", "MaxMagazine", "Override", scalable(size)),   # the size first: the magazine clamps to it
                                        modifier("HollowWeaponSet", "Magazine", "Override", scalable(size))]},
                         "GE_Weapon_Init_%s: %d rounds, full" % (name, numbers["magazine_size"]))
    cooldown = ensure_effect("%s/GE_Cooldown_Weapon_%s" % (directory, name), unreal.HollowWeaponFireCooldownEffect,
                             {"duration_magnitude": duration(numbers["fire_interval_seconds"] * COOLDOWN_SHARE)},
                             "GE_Cooldown_Weapon_%s: %.3f s" % (name, numbers["fire_interval_seconds"] * COOLDOWN_SHARE))
    fire = ensure_ability("%s/GA_Weapon_Fire_%s" % (directory, name), unreal.HollowGA_WeaponFire,
                          {"weapon_definition": definition, "cost_gameplay_effect_class": cost, "cooldown_gameplay_effect_class": cooldown},
                          "GA_Weapon_Fire_%s ready" % name)
    reload_ability = ensure_ability("%s/GA_Weapon_Reload_%s" % (directory, name), unreal.HollowGA_WeaponReload,
                                    {"weapon_definition": definition, "load_effect": load_effect}, "GA_Weapon_Reload_%s ready" % name)

    ability_set, created = data_asset("%s/DA_AbilitySet_Weapon_%s" % (directory, name), unreal.HollowAbilitySet)
    wanted = {"granted_abilities": [ability_entry(fire, "Hollow.Input.PrimaryUse"), ability_entry(dry_fire, "Hollow.Input.PrimaryUse"),
                                    ability_entry(aim, "Hollow.Input.SecondaryUse"), ability_entry(reload_ability, "Hollow.Input.Reload")],
              "granted_effects": [effect_entry(init)]}
    if refresh(ability_set, wanted) or created:
        save(ability_set)
        log("DA_AbilitySet_Weapon_%s: fire, dry fire, aim, reload, its magazine" % name)

    item, created = data_asset("%s/Weapons/DA_Item_Weapon_%s" % (ITEMS_DIR, name), unreal.HollowItemDefinition)
    wanted = dict(spec["item"], display_name=table_text(table, spec["display"]), item_type=make_tag(WEAPON_TYPE), equip_slot=make_tag(WEAPON_SLOT))
    changed = refresh(item, wanted)
    if text_of(item.get_editor_property("ability_set")) != text_of(ability_set):
        item.set_editor_property("ability_set", ability_set)
        changed = True
    if changed or created:
        save(item)
        log("DA_Item_Weapon_%s: %d cr, insurable" % (name, spec["item"]["base_value"]))


def ensure_ammo(name, spec, table):
    item, created = data_asset("%s/Ammo/%s" % (ITEMS_DIR, name), unreal.HollowItemDefinition)
    wanted = dict(spec["item"], display_name=table_text(table, spec["display"]), item_type=make_tag(AMMO_TYPE), equip_slot=unreal.GameplayTag())
    if refresh(item, wanted) or created:
        save(item)
        log("%s: %d cr a round, %d a slot" % (name, spec["item"]["base_value"], spec["item"]["max_stack"]))


# ---------------------------------------------------------------- the suit

def ensure_suit(swap_effect, melee_cooldown):
    swap = ensure_ability(SUIT_DIR + "/GA_SwapHands", unreal.HollowGA_SwapHands, {"cooldown_gameplay_effect_class": swap_effect}, "GA_SwapHands ready")
    melee_wanted = {"damage": MELEE["damage"], "range": MELEE["range"], "radius": MELEE["radius"], "impulse": MELEE["impulse"],
                    "damage_effect": load(DAMAGE_KINETIC).generated_class(), "noise_tag": make_tag(MELEE["noise_tag"]), "feel_tag": make_tag(MELEE["feel_tag"]),
                    "cooldown_gameplay_effect_class": melee_cooldown}
    melee = ensure_ability(SUIT_DIR + "/GA_MeleeBash", unreal.HollowGA_MeleeBash, melee_wanted, "GA_MeleeBash ready")
    suit = load(SUIT_SET)
    granted = [e for e in suit.get_editor_property("granted_abilities") if e.get_editor_property("ability") not in (swap, melee)]
    wanted = granted + [ability_entry(swap, tag) for tag in SWAP_INPUTS] + [ability_entry(melee, "Hollow.Input.Melee")]
    if text_of(suit.get_editor_property("granted_abilities")) != text_of(wanted):
        suit.set_editor_property("granted_abilities", wanted)
        save(suit)
        log("DA_AbilitySet_ExoFrame: GA_SwapHands on the three hand inputs, GA_MeleeBash on the melee input")
    return swap, melee


def ensure_inputs():
    context = load(IMC_PATH)
    data = context.get_editor_property("default_key_mappings")
    mappings = list(data.get_editor_property("mappings"))
    config = load(INPUT_CONFIG_PATH)
    entries = list(config.get_editor_property("ability_actions"))
    context_changed = config_changed = False
    for action_name, (tag_name, keys) in sorted(INPUTS.items()):
        path = ACTIONS_DIR + "/" + action_name
        if not assets.does_asset_exist(path):
            action = create(path, unreal.InputAction, unreal.InputAction_Factory())
            action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
            save(action)
            log("created " + action_name)
        action = load(path)
        current = sorted(m.get_editor_property("key").export_text() for m in mappings if m.get_editor_property("action") == action)
        if current != sorted(make_key(k).export_text() for k in keys):
            mappings = [m for m in mappings if m.get_editor_property("action") != action]
            for key_name in keys:
                entry = unreal.EnhancedActionKeyMapping()
                entry.set_editor_property("action", action)
                entry.set_editor_property("key", make_key(key_name))
                mappings.append(entry)
            context_changed = True
            log("mapped %s to %s in IMC_Raid" % (action_name, ", ".join(keys)))
        tag = make_tag(tag_name)
        if not any(e.get_editor_property("input_action") == action and text_of(e.get_editor_property("input_tag")) == text_of(tag) for e in entries):
            entries = [e for e in entries if text_of(e.get_editor_property("input_tag")) != text_of(tag)]
            entry = unreal.HollowInputAction()
            entry.set_editor_property("input_action", action)
            entry.set_editor_property("input_tag", tag)
            entries.append(entry)
            config_changed = True
            log("added %s to DA_InputConfig_Raid" % tag_name)
    if context_changed:
        data.set_editor_property("mappings", mappings)
        context.set_editor_property("default_key_mappings", data)
        save(context)
    if config_changed:
        config.set_editor_property("ability_actions", entries)
        save(config)


# ---------------------------------------------------------------- cue and enemies

def ensure_cue():
    cue_bp, created = blueprint(CUES_DIR + "/GCN_Weapon_Fire", unreal.HollowWeaponFireCue)
    cue = unreal.get_default_object(cue_bp.generated_class())
    if refresh(cue, {"gameplay_cue_tag": make_tag("GameplayCue.Hollow.Weapon.Fire")}) or created:
        save(cue_bp)
        log("GCN_Weapon_Fire ready")


def ensure_enemies(stagger):
    for path, (scale, impulse) in sorted(ENEMIES.items()):
        definition = load(path)
        if refresh(definition, {"knockback_scale": scale, "stagger_impulse": impulse, "stagger_effect": stagger}):
            save(definition)
            log("%s: knockback x%.2f, staggered from %.0f cm/s" % (path.rsplit("/", 1)[1], scale, impulse))


# ---------------------------------------------------------------- validation

def validate(swap, melee):
    with open(unreal.Paths.project_dir() + INI) as ini:
        text = ini.read()
    lines = set(line.strip() for line in text.splitlines())
    for line in INI_LINES:
        check(line in lines, "DefaultGame.ini: " + line)
    for tag in (WEAPON_TYPE, AMMO_TYPE):
        check('(TagName="%s")' % tag in text, "DefaultGame.ini: %s is a raid item type (LoadoutItemTypes)" % tag)
    for name, spec in WEAPONS.items():
        directory = "%s/%s" % (WEAPONS_DIR, name)
        definition = load("%s/DA_Weapon_%s" % (directory, name))
        check(is_table_text(definition.get_editor_property("display_name"), spec["display"][0]), name + " is named from " + STRING_TABLE)
        check(definition.get_editor_property("magazine_size") == spec["definition"]["magazine_size"], name + " magazine")
        fire = unreal.get_default_object(load("%s/GA_Weapon_Fire_%s" % (directory, name)).generated_class())
        check(fire.get_editor_property("weapon_definition") == definition, name + ": the fire ability fires it")
        check(fire.get_editor_property("cost_gameplay_effect_class") == load(EFFECTS_DIR + "/GE_Weapon_Cost").generated_class(), name + " costs a round")
        granted = load("%s/DA_AbilitySet_Weapon_%s" % (directory, name)).get_editor_property("granted_abilities")
        inputs = sorted(e.get_editor_property("input_tag").export_text() for e in granted)
        check(inputs == sorted(make_tag(t).export_text() for t in ("Hollow.Input.PrimaryUse", "Hollow.Input.PrimaryUse", "Hollow.Input.SecondaryUse",
                                                                    "Hollow.Input.Reload")), name + "'s set grants its four abilities")
        item = load("%s/Weapons/DA_Item_Weapon_%s" % (ITEMS_DIR, name))
        check(unreal.SystemLibrary.get_primary_asset_id_from_object(item).export_text().endswith("DA_Item_Weapon_" + name), name + " item id")
        check(item.get_editor_property("equip_slot").export_text() == make_tag(WEAPON_SLOT).export_text(), name + " takes the weapon slot")
        check(is_table_text(item.get_editor_property("display_name"), spec["display"][0]), name + " item is named from " + STRING_TABLE)
        check(spec["ammo"] in AMMO, name + " fires rounds made here")
    for name, spec in AMMO.items():
        item = load("%s/Ammo/%s" % (ITEMS_DIR, name))
        check(item.get_editor_property("item_type").export_text() == make_tag(AMMO_TYPE).export_text(), name + " is ammunition")
        check(is_table_text(item.get_editor_property("display_name"), spec["display"][0]), name + " is named from " + STRING_TABLE)
    suit = load(SUIT_SET).get_editor_property("granted_abilities")
    swap_inputs = sorted(e.get_editor_property("input_tag").export_text() for e in suit if e.get_editor_property("ability") == swap)
    check(swap_inputs == sorted(make_tag(t).export_text() for t in SWAP_INPUTS), "the suit swaps on the three hand inputs")
    check(any(e.get_editor_property("ability") == melee for e in suit), "the suit strikes on the melee input")
    context = load(IMC_PATH)
    keys_by_action = {}
    for m in context.get_editor_property("default_key_mappings").get_editor_property("mappings"):
        keys_by_action.setdefault(m.get_editor_property("action").get_name(), []).append(m.get_editor_property("key").export_text())
    for action_name, (_, keys) in INPUTS.items():
        check(sorted(keys_by_action.get(action_name, [])) == sorted(make_key(k).export_text() for k in keys), action_name + " keys")
        for key_name in keys:
            users = [a for a, k in keys_by_action.items() if make_key(key_name).export_text() in k]
            check(users == [action_name], "%s belongs to %s only in IMC_Raid (also: %s)" % (key_name, action_name, ", ".join(users)))
    for path, (scale, impulse) in ENEMIES.items():
        definition = load(path)
        check(definition.get_editor_property("stagger_effect") == load(EFFECTS_DIR + "/GE_Status_Stagger").generated_class(), path + " staggers")
    check(unreal.get_default_object(load(CUES_DIR + "/GCN_Weapon_Fire").generated_class()).get_editor_property("gameplay_cue_tag").export_text()
          == make_tag("GameplayCue.Hollow.Weapon.Fire").export_text(), "fire cue tag")
    log("validation passed")


def main():
    table = string_table()
    cost, load_effect, swap_effect, melee_cooldown, stagger = ensure_shared_effects()
    aim = ensure_ability(WEAPONS_DIR + "/GA_Weapon_Aim", unreal.HollowGA_WeaponAim, {}, "GA_Weapon_Aim ready")
    dry_fire = ensure_ability(WEAPONS_DIR + "/GA_Weapon_DryFire", unreal.HollowGA_WeaponDryFire, {}, "GA_Weapon_DryFire ready")
    for name, spec in AMMO.items():
        ensure_ammo(name, spec, table)
    for name, spec in WEAPONS.items():
        ensure_weapon(name, spec, table, cost, load_effect, aim, dry_fire)
    swap, melee = ensure_suit(swap_effect, melee_cooldown)
    ensure_inputs()
    ensure_cue()
    ensure_enemies(stagger)
    loc.save()   # only the tables whose texts changed
    validate(swap, melee)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_weapon_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
