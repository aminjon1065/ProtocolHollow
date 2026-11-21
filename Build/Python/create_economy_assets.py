"""Creates the S5-02 hub economy (idempotent, ADR-0010, ADR-0031).

  /Game/Hollow/Economy/DA_EconomyConfig              UHollowEconomyConfig: the shop's offers, sell and insurance percents,
                                                     the stash's size, how many slots may be insured
  /Game/Hollow/Economy/Recipes/DA_Recipe_FuelCell    UHollowCraftingRecipe: 2 scrap + 5 cr -> a fuel cell
  /Game/Hollow/Economy/Recipes/DA_Recipe_PulseMine   3 scrap + 1 Aether shard + 10 cr -> a pulse mine
  /Game/Hollow/Economy/Upgrades/DA_Upgrade_*         UHollowShipUpgradeDefinition (S5-03, ADR-0032): the Skiff's fuel
                                                     reserve, an extra insured slot, the core contract bonus

The materials themselves are items (create_raid_flow_assets.py). Every asset is set to its spec when it differs, then
the whole economy is validated: offers and recipe items exist, recipes make consumables, no loop of buying, crafting
and selling earns credits, and a new profile's stash can craft. Run after create_inventory_assets.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_economy_assets.py
"""
import re
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

ECONOMY_DIR = "/Game/Hollow/Economy"
CONFIG_PATH = ECONOMY_DIR + "/DA_EconomyConfig"
RECIPES_DIR = ECONOMY_DIR + "/Recipes"
UPGRADES_DIR = ECONOMY_DIR + "/Upgrades"
ITEMS_DIR = "/Game/Hollow/Items"
INI = "Config/DefaultGame.ini"
INI_LINE = "EconomyConfig=/Game/Hollow/Economy/DA_EconomyConfig.DA_EconomyConfig"

# S5-02 clarifications (data). Item names are DA_Item_* (their folders under ITEMS_DIR below).
ITEM_DIRS = {"DA_Item_Welder": "Tools", "DA_Item_PlasmaCutter": "Tools", "DA_Item_FuelCell": "Consumables",
             "DA_Item_PulseMine": "Consumables", "DA_Item_Scrap": "Materials", "DA_Item_AetherShard": "Materials"}
# Clarification 4: tools and materials at their BaseValue; consumables only from the workbench.
OFFERS = [("DA_Item_Welder", 300), ("DA_Item_PlasmaCutter", 400), ("DA_Item_Scrap", 8), ("DA_Item_AetherShard", 20)]
CONFIG = {
    "sell_percent": 50,          # clarification 4: half the value back, rounded down
    "insurance_percent": 30,     # clarification 1: 30 % of the slot's value, rounded up, single-use
    "max_insured_slots": 2,      # clarification 1 (a ship upgrade adds one in S5-03)
    "stash_slots": 40,           # clarification 2
}
# Clarification 5: name -> (display name, inputs, credits, output, output count).
RECIPES = {
    "DA_Recipe_FuelCell": ("Fuel cell", [("DA_Item_Scrap", 2)], 5, "DA_Item_FuelCell", 1),
    "DA_Recipe_PulseMine": ("Pulse mine", [("DA_Item_Scrap", 3), ("DA_Item_AetherShard", 1)], 10, "DA_Item_PulseMine", 1),
}
CONSUMABLE = "Hollow.Item.Type.Consumable"
# S5-03 clarifications 1-3: name -> (display name, description, price, extra insured slots, core value percent, raid items).
UPGRADES = {
    "DA_Upgrade_FuelReserve": ("Fuel reserve", "The Skiff stocks two fuel cells for every raid: used first, never from your stash.", 600, 0, 0,
                               [("DA_Item_FuelCell", 2)]),
    "DA_Upgrade_InsuranceSlot": ("Insurance rider", "One more loadout slot can be insured at a time.", 800, 1, 0, []),
    "DA_Upgrade_CoreBonus": ("Core contract", "The corporation pays 20 % more for every Aether core brought out.", 1000, 0, 20, []),
}

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_economy_assets: " + message)


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


def item_id(name):
    return from_text(unreal.PrimaryAssetId(), "HollowItem:" + name)


def item(name):
    return load("%s/%s/%s" % (ITEMS_DIR, ITEM_DIRS[name], name))


def stack(name, count):
    result = unreal.HollowItemStack()
    result.set_editor_property("item", item_id(name))
    result.set_editor_property("count", count)
    return result


def texts(values):
    return [v.export_text() for v in values]


# ---------------------------------------------------------------- config and recipes

def ensure_config():
    config, created = data_asset(CONFIG_PATH, unreal.HollowEconomyConfig)
    offers = []
    for name, price in OFFERS:
        offer = unreal.HollowShopOffer()
        offer.set_editor_property("item", item_id(name))
        offer.set_editor_property("price", price)
        offers.append(offer)
    changed = created or texts(config.get_editor_property("offers")) != texts(offers)
    for key, value in CONFIG.items():
        changed = changed or config.get_editor_property(key) != value
    if not changed:
        return config
    config.set_editor_property("offers", offers)
    for key, value in CONFIG.items():
        config.set_editor_property(key, value)
    save(config)
    log("DA_EconomyConfig: %d offers, sell %d %%, insurance %d %%, %d stash slots" % (len(offers), CONFIG["sell_percent"], CONFIG["insurance_percent"],
                                                                                    CONFIG["stash_slots"]))
    return config


def ensure_recipe(name, spec):
    display, inputs, credits, output, output_count = spec
    recipe, created = data_asset(RECIPES_DIR + "/" + name, unreal.HollowCraftingRecipe)
    wanted_inputs = [stack(n, c) for n, c in inputs]
    wanted_output = stack(output, output_count)
    if not created and str(recipe.get_editor_property("display_name")) == display \
            and texts(recipe.get_editor_property("inputs")) == texts(wanted_inputs) and recipe.get_editor_property("credits") == credits \
            and recipe.get_editor_property("output").export_text() == wanted_output.export_text():
        return recipe
    recipe.set_editor_property("display_name", unreal.Text(display))
    recipe.set_editor_property("inputs", wanted_inputs)
    recipe.set_editor_property("credits", credits)
    recipe.set_editor_property("output", wanted_output)
    save(recipe)
    log("%s: %s + %d cr -> %s x%d" % (name, ", ".join("%s x%d" % i for i in inputs), credits, output, output_count))
    return recipe


def ensure_upgrade(name, spec):
    display, description, price, slots, core_percent, raid_items = spec
    upgrade, created = data_asset(UPGRADES_DIR + "/" + name, unreal.HollowShipUpgradeDefinition)
    wanted_items = [stack(n, c) for n, c in raid_items]
    values = {"price": price, "extra_insured_slots": slots, "core_value_percent": core_percent}
    if not created and str(upgrade.get_editor_property("display_name")) == display and str(upgrade.get_editor_property("description")) == description \
            and all(upgrade.get_editor_property(k) == v for k, v in values.items()) and texts(upgrade.get_editor_property("raid_items")) == texts(wanted_items):
        return upgrade
    upgrade.set_editor_property("display_name", unreal.Text(display))
    upgrade.set_editor_property("description", unreal.Text(description))
    for key, value in values.items():
        upgrade.set_editor_property(key, value)
    upgrade.set_editor_property("raid_items", wanted_items)
    save(upgrade)
    log("%s: %d cr" % (name, price))
    return upgrade


# ---------------------------------------------------------------- validation

def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate(config):
    prices = {name: price for name, price in OFFERS}
    values = {name: item(name).get_editor_property("base_value") for name in ITEM_DIRS}
    check(len(config.get_editor_property("offers")) == len(OFFERS), "DA_EconomyConfig offers")
    check(0 <= config.get_editor_property("sell_percent") < 100, "selling pays less than buying")
    for name, price in OFFERS:
        check(price > 0 and values[name] * CONFIG["sell_percent"] // 100 < price, "%s: buying to sell never pays" % name)
    for name, (_, inputs, credits, output, output_count) in RECIPES.items():
        recipe = load(RECIPES_DIR + "/" + name)
        check(unreal.SystemLibrary.get_primary_asset_id_from_object(recipe).export_text().endswith(name), name + " primary asset id")
        check(item(output).get_editor_property("item_type").export_text() == from_text(unreal.GameplayTag(), '(TagName="%s")' % CONSUMABLE).export_text(),
              name + " makes a consumable")
        check(all(n in prices for n, _ in inputs), name + ": its materials are sold in the shop")
        cost = credits + sum(prices[n] * c for n, c in inputs)
        sold = values[output] * CONFIG["sell_percent"] // 100 * output_count
        check(sold < cost, "%s: crafting to sell never pays (%d cr in, %d out)" % (name, cost, sold))
    with open(unreal.Paths.project_dir() + INI) as ini:
        text = ini.read()
    check(INI_LINE in text, "DefaultGame.ini: " + INI_LINE)
    check("PrimaryAssetType=\"HollowRecipe\"" in text, "DefaultGame.ini scans the recipes")
    check("PrimaryAssetType=\"HollowUpgrade\"" in text, "DefaultGame.ini scans the ship upgrades")
    for name, (_, _, price, slots, core_percent, raid_items) in UPGRADES.items():
        upgrade = load(UPGRADES_DIR + "/" + name)
        check(unreal.SystemLibrary.get_primary_asset_id_from_object(upgrade).export_text().endswith(name), name + " primary asset id")
        check(price > 0 and (slots > 0 or core_percent > 0 or raid_items), name + " costs something and does something")
        for item_name, count in raid_items:
            check(item(item_name).get_editor_property("item_type").export_text() == from_text(unreal.GameplayTag(), '(TagName="%s")' % CONSUMABLE).export_text()
                  and count > 0, "%s gives a raid consumable (%s)" % (name, item_name))
    starting = dict((n, int(c)) for n, c in re.findall(r"^\+StartingStash=\(Item=HollowItem:(\w+),Count=(\d+)", text, re.MULTILINE))
    _, mine_inputs, _, _, _ = RECIPES["DA_Recipe_PulseMine"]
    check(all(starting.get(n, 0) >= c for n, c in mine_inputs), "a new profile's stash crafts a pulse mine")
    log("validation passed")


def main():
    config = ensure_config()
    for name, spec in RECIPES.items():
        ensure_recipe(name, spec)
    for name, spec in UPGRADES.items():
        ensure_upgrade(name, spec)
    validate(config)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_economy_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
