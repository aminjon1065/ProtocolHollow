"""FAB-01 (ADR-0051): the Fab props of OvergrownFactoryENV in the hub and the Docks rooms (idempotent, ADR-0010).

The layout lives in hollow_fab_layout.py (pure Python, its checks run outside the editor too). This script:
  makes the darkened overgrowth      MI_Fab_{Ivy,PlantStem,Moss}_Dark in /Game/Hollow/World/Materials/Fab: children of
                                     the pack's foliage instances, tinted to the ark's dead biomechanics (a Fab asset is
                                     never edited: an update of the pack would undo it)
  checks every level's layout        against its room definition: the connector openings (300 x 350 plus a margin) and
                                     the sockets stay free, nothing leaves the room (the hub: terminals, use spots,
                                     starts and ramp); a problem stops the run before any level changes
  places the props                   StaticMeshActors in Dressing/Fab (lamp housings in Fixtures/Fab): NoCollision,
                                     CanEverAffectNavigation off, shadows only from the big floor props, no light
                                     (DA_LightBudget unchanged); tagged HollowFab + the digest of the level's layout

A level is opened only when its layout's digest differs from its props' (every save is a new LFS object). A room
art-locked on its definition (bArtLocked, ADR-0027) is never opened.
Run after prepare_fab_assets.py (game):  Build/Scripts/run-editor-python.sh Build/Python/create_fab_dressing.py
"""
import hashlib
import json
import os
import sys
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hollow_fab_layout as layout  # noqa: E402

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

VERSION = 2
TAG = "HollowFab"
DECOR_FOLDER = "Dressing/Fab"
FIXTURE_FOLDER = "Fixtures/Fab"
ROOM_DATA_DIR = "/HollowSector_Docks/Data"
MATERIAL_DIR = "/Game/Hollow/World/Materials/Fab"
FOLIAGE_DIR = "/Game/Fab/OvergrownFactoryENV/Material"
SPEC_TAG = "HollowSpec"
# The darkened overgrowth: near-black violet leaves, a faint Aether-violet light through them.
DARK = {
    "vector": {"BaseLayer_Tint": (0.16, 0.12, 0.18, 1.0), "SSS_Tint": (0.20, 0.06, 0.26, 1.0)},
    "scalar": {"BaseLayer_B_Desaturation": 0.6, "BaseLayer_B_Brightness": 0.45},
}

assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(message):
    unreal.log("create_fab_dressing: " + message)


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset " + path)
    return asset


def digest(*specs):
    return hashlib.sha1(json.dumps(specs, sort_keys=True).encode("ascii")).hexdigest()


def find_parent(name):
    found = [p for p in assets.list_assets(FOLIAGE_DIR, recursive=True, include_folder=False) if p.split(".")[0].endswith("/" + name)]
    check(len(found) == 1, "%s: one foliage instance in %s (found %d)" % (name, FOLIAGE_DIR, len(found)))
    return load(found[0])


def ensure_dark_materials():
    made = {}
    for parent_name, name in sorted(layout.DARK_MATERIALS.items()):
        path = MATERIAL_DIR + "/" + name
        spec_hash = digest(VERSION, parent_name, DARK)
        mi = unreal.load_asset(path) if assets.does_asset_exist(path) else None
        if mi is None:
            mi = tools.create_asset(name, MATERIAL_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        if assets.get_metadata_tag(mi, SPEC_TAG) != spec_hash:
            mi.set_editor_property("parent", find_parent(parent_name))
            for key, (r, g, b, a) in DARK["vector"].items():
                unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, key, unreal.LinearColor(r, g, b, a))
            for key, value in DARK["scalar"].items():
                unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, key, value)
            unreal.MaterialEditingLibrary.update_material_instance(mi)
            assets.set_metadata_tag(mi, SPEC_TAG, spec_hash)
            if not assets.save_loaded_asset(mi, False):
                raise RuntimeError("Could not save " + path)
            log("%s (child of %s)" % (name, parent_name))
        made[parent_name] = mi
    return made


def room_definitions():
    """Room level path -> its definition (the Docks rooms)."""
    found = {}
    for path in sorted(assets.list_assets(ROOM_DATA_DIR, recursive=False, include_folder=False)):
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.HollowRoomDefinition):
            found[asset.get_editor_property("room_level").get_path_name().split(".")[0]] = asset
    return found


def side_name(side):
    return str(side).split(".")[-1].split(":")[0].strip().capitalize()


def constraints(level, definition, metrics):
    """(inner box, connectors, keep-free points) of a level."""
    if definition is None:
        check(level == layout.HUB, level + ": a Docks room has its definition")
        return layout.HUB_INNER, [], layout.HUB_KEEP_FREE
    footprint = definition.get_editor_property("footprint")
    cell = float(metrics.get_editor_property("cell_size"))
    wall = float(metrics.get_editor_property("wall_thickness"))
    size = (footprint.x * cell, footprint.y * cell)
    connectors = []
    for connector in definition.get_editor_property("connectors"):
        c = connector.get_editor_property("cell")
        connectors.append(((c.x, c.y), side_name(connector.get_editor_property("side")), size, cell))
    keep = []
    for socket in definition.get_editor_property("sockets"):
        t = socket.get_editor_property("transform").translation
        keep.append((socket.get_editor_property("socket_type").export_text(), t.x, t.y, layout.SOCKET_CLEARANCE))
    return (wall, wall, size[0] - wall, size[1] - wall), connectors, keep


def tags_of(actor):
    return [str(t) for t in actor.get_editor_property("tags")]


def dress_level(level, placed, wanted, meshes, dark):
    if not levels.load_level(level):
        raise RuntimeError("Could not load " + level)
    owned = [a for a in actors.get_all_level_actors() if TAG in tags_of(a)]
    if owned and len(owned) == len(placed) and all(wanted in tags_of(a) for a in owned):
        return False
    for actor in owned:
        actors.destroy_actor(actor)
    for label, mesh_name, location, rot, _ in placed:
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
        component = actor.static_mesh_component
        mesh = meshes[mesh_name]
        component.set_static_mesh(mesh)
        component.set_cast_shadow(mesh_name in layout.SHADOWED)
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            material = slot.get_editor_property("material_interface")
            if material is not None and material.get_name() in dark:
                component.set_material(index, dark[material.get_name()])
        # Looks only. The profile itself and last (create_lighting_assets.py): a mesh set through its property re-takes
        # the mesh's own profile when the level loads again (a BlockAll prop collided after a reload).
        component.set_collision_profile_name("NoCollision")
        component.set_editor_property("can_ever_affect_navigation", False)
        actor.set_actor_label(label)
        actor.set_folder_path(FIXTURE_FOLDER if mesh_name in layout.FIXTURES else DECOR_FOLDER)
        actor.set_editor_property("tags", [TAG, wanted])
    if not levels.save_current_level():
        raise RuntimeError("Could not save " + level)
    log("%s: %d props" % (level.rsplit("/", 1)[1], len(placed)))
    return True


def validate(plans, meshes):
    for level, (placed, wanted) in sorted(plans.items()):
        if not levels.load_level(level):
            raise RuntimeError("Could not load " + level)
        owned = {a.get_actor_label(): a for a in actors.get_all_level_actors() if TAG in tags_of(a)}
        check(len(owned) == len(placed), "%s: %d props (found %d)" % (level, len(placed), len(owned)))
        for label, mesh_name, _, _, _ in placed:
            actor = owned.get(label)
            check(actor is not None, "%s: %s placed" % (level, label))
            component = actor.get_editor_property("static_mesh_component")
            check(component.get_editor_property("static_mesh") == meshes[mesh_name], "%s: %s is %s" % (level, label, mesh_name))
            check(str(component.get_collision_profile_name()) == "NoCollision", "%s: %s has no collision" % (level, label))
            check(not component.get_editor_property("can_ever_affect_navigation"), "%s: %s leaves the navmesh alone" % (level, label))
            check(str(actor.get_folder_path()).startswith(("Dressing/", "Fixtures/")), "%s: %s in a decor folder" % (level, label))
            check(wanted in tags_of(actor), "%s: %s carries the layout's digest" % (level, label))
    log("validation passed: %d props in %d levels" % (sum(len(p) for p, _ in plans.values()), len(plans)))


def main():
    meshes = {name: load(layout.mesh_path(name)) for name in layout.used_meshes()}
    boxes = {}
    for name, mesh in meshes.items():
        box = mesh.get_bounding_box()
        boxes[name] = ((box.min.x, box.min.y, box.min.z), (box.max.x, box.max.y, box.max.z))
    dark = ensure_dark_materials()
    metrics = unreal.get_default_object(unreal.HollowLevelMetrics)
    definitions = room_definitions()
    plans, problems = {}, []
    for level, placements in sorted(layout.ROOMS.items()):
        definition = definitions.get(level)
        if definition is not None and definition.get_editor_property("art_locked"):
            log("%s: art-locked, level kept as is" % definition.get_name())
            continue
        inner, connectors, keep = constraints(level, definition, metrics)
        found, placed = layout.check_layout(level, placements, boxes, inner, connectors, keep)
        problems += found
        rounded = {name: [[round(v, 1) for v in corner] for corner in boxes[name]] for name in sorted({m for _, m, _ in placements})}
        wanted = digest(VERSION, placements, rounded, layout.FRONT, layout.SHADOWED, layout.FIXTURES, layout.DARK_MATERIALS, DARK,
                        DECOR_FOLDER, FIXTURE_FOLDER)
        plans[level] = (placed, wanted)
    check(not problems, "the layout keeps the openings, sockets and terminals free:\n  " + "\n  ".join(problems))
    dressed = sum(1 for level, (placed, wanted) in sorted(plans.items()) if dress_level(level, placed, wanted, meshes, dark))
    log("%d level(s) dressed" % dressed)
    validate(plans, meshes)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_fab_dressing failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
