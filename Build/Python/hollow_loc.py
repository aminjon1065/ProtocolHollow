"""LOC-01 (ADR-0048): the player-facing texts of the content scripts (not a content script itself).

Every text a player reads in the data (names, descriptions, prompts, labels, widget texts) is an entry of one of the
string tables below, and the asset keeps a text that refers to that entry, never a plain string: the gather of the Game
target then finds it under a stable key, and Content/Localization/Game/ru/Game.po holds its translation
(Build/Scripts/localize-mac.sh). The scripts hold the English source strings only, never a translation.

Use from a content script:
    import os, sys
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import hollow_loc as loc
    loc.set_text(item, "display_name", loc.ITEMS, "Item.Scrap.Name", "Scrap")
    ...
    loc.save()   # the tables this script changed
"""
import unreal

LOCALIZATION_DIR = "/Game/Hollow/Localization"
MENUS = "ST_Menus"   # front-end and system menu, debrief
HUB = "ST_Hub"       # hub screens and terminals
VISOR = "ST_Visor"   # visor readouts, controls cheat sheet, key names
ITEMS = "ST_Items"   # items, weapons, ammo, relics, tools, recipes, ship upgrades
WORLD = "ST_World"   # classes, contracts, sectors, prompts of things in the world
RADIO = "ST_Radio"   # NAR-01a: the handler's radio (call signs, every line)
LORE = "ST_Lore"     # NAR-01b: the crew logs (title, author, stamp, text)
TABLES = (MENUS, HUB, VISOR, ITEMS, WORLD, RADIO, LORE)

_authoring = unreal.HollowLocalizationAuthoring
_loaded = {}


def table(name):
    """The string table asset Name (created when missing)."""
    if name not in TABLES:
        raise ValueError("%s is not one of the string tables %s" % (name, ", ".join(TABLES)))
    found = _loaded.get(name)
    if found is None:
        found = _authoring.find_or_create_string_table(LOCALIZATION_DIR, name)
        if found is None:
            raise RuntimeError("Could not make the string table " + name)
        _loaded[name] = found
    return found


def text(table_name, key, source):
    """A text referring to Key of the table; the entry's English source becomes Source if it differs."""
    if not key or not source:
        raise ValueError("a player text needs a key and a source string (%s, %r)" % (key, source))
    return _authoring.table_text(table(table_name), key, source)


def entry(value):
    """(table name, key) Value refers to, or None for any other text."""
    found = _authoring.get_table_entry(value)   # a bool UFUNCTION with out parameters: None when false
    if found is None:
        return None
    table_id, key = found
    return str(table_id).rsplit(".", 1)[-1], str(key)


def is_text(value, table_name, key):
    return entry(value) == (table_name, key)


def set_text(obj, prop, table_name, key, source):
    """Points obj.prop at the table entry (its source kept up to date); True if the object changed."""
    wanted = text(table_name, key, source)
    if is_text(obj.get_editor_property(prop), table_name, key):
        return False
    obj.set_editor_property(prop, wanted)
    return True


def save():
    """Saves the string tables this script changed."""
    for found in _loaded.values():
        unreal.EditorAssetLibrary.save_loaded_asset(found, only_if_is_dirty=True)
