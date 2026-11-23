"""Art-01: the Orbital Docks off greybox (idempotent, ADR-0010, ADR-0043).

Every room of the Docks (LV_Docks_*) built by create_docks_greybox.py still wore the engine's WorldGridMaterial on its
shell, all but the hero CoreVault (create_lighting_assets.py). This puts the hull's materials on every shell piece:

  Floor            MI_Hull_Floor      tread plate (CC0 metal_plate, create_lighting_assets.py)
  Ceiling          MI_Hull_Ceiling    corrugated iron (CC0 corrugated_iron_03)
  Wall_* Lintel_*  MI_Hull_Wall       worn painted steel (CC0 rusty_metal_02)

Only materials change: no actor is added or moved, so the openings, the light budget and the acoustics stay exactly
as they are. A room art-locked on its definition (bArtLocked, ADR-0027) is never opened. A level is saved only when a
material actually changed (every save is a new LFS object).
Run after create_lighting_assets.py:  Build/Scripts/run-editor-python.sh Build/Python/create_docks_look.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

ROOM_DATA_DIR = "/HollowSector_Docks/Data"
MATERIAL_DIR = "/Game/Hollow/World/Materials"
GREYBOX_MATERIAL = "/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"
SHELL = [("Floor", "MI_Hull_Floor"), ("Ceiling", "MI_Hull_Ceiling"), ("Wall_", "MI_Hull_Wall"), ("Lintel_", "MI_Hull_Wall")]

assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def log(message):
    unreal.log("create_docks_look: " + message)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset " + path)
    return asset


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def rooms():
    found = []
    for path in sorted(assets.list_assets(ROOM_DATA_DIR, recursive=False, include_folder=False)):
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.HollowRoomDefinition):
            found.append(asset)
    return found


def shell_material(label, materials):
    for prefix, name in SHELL:
        if label == prefix or (prefix.endswith("_") and label.startswith(prefix)):
            return materials[name]
    return None


def shell_pieces():
    """(actor, its wanted material) of every shell piece of the loaded level."""
    pieces = []
    for actor in actors.get_all_level_actors():
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        pieces.append(actor)
    return pieces


def dress_room(room, materials):
    if room.get_editor_property("art_locked"):
        log("%s: art-locked, level kept as is" % room.get_name())
        return False
    level_path = room.get_editor_property("room_level").get_path_name().split(".")[0]
    if not levels.load_level(level_path):
        raise RuntimeError("Could not load " + level_path)
    changed = 0
    for actor in shell_pieces():
        wanted = shell_material(actor.get_actor_label(), materials)
        component = actor.get_editor_property("static_mesh_component")
        if wanted is not None and component.get_material(0) != wanted:
            component.set_material(0, wanted)
            changed += 1
    if changed:
        if not levels.save_current_level():
            raise RuntimeError("Could not save " + level_path)
        log("%s: %d shell pieces dressed" % (room.get_name(), changed))
    return changed > 0


def validate(materials):
    greybox = load(GREYBOX_MATERIAL)
    for room in rooms():
        if room.get_editor_property("art_locked"):
            continue
        level_path = room.get_editor_property("room_level").get_path_name().split(".")[0]
        if not levels.load_level(level_path):
            raise RuntimeError("Could not load " + level_path)
        pieces = [a for a in shell_pieces() if shell_material(a.get_actor_label(), materials) is not None]
        check(len(pieces) > 0, room.get_name() + " has its shell")
        for actor in pieces:
            material = actor.get_editor_property("static_mesh_component").get_material(0)
            check(material != greybox, "%s: %s is off greybox" % (room.get_name(), actor.get_actor_label()))
            check(material == shell_material(actor.get_actor_label(), materials), "%s: %s wears its hull material" % (room.get_name(), actor.get_actor_label()))
    log("validation passed")


def main():
    materials = {name: load(MATERIAL_DIR + "/" + name) for _, name in SHELL}
    dressed = sum(1 for room in rooms() if dress_room(room, materials))
    log("%d room(s) dressed" % dressed)
    validate(materials)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_docks_look failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
