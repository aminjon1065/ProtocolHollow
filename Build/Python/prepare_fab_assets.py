"""Art-01: Fab packs into the game within its budget (idempotent, ADR-0010, ADR-0043).

Runs in two projects (Build/Scripts/run-editor-python.sh, --project for the first):

  In the Fab staging (Build/Scripts/create-art-staging.sh, outside git), for every pack Build/Art/fab_manifest.json
  names (/Game/Fab/<name>; FAB-01: the owner's other packs are left as they are):
    textures      cooked at most 2048, at most 1024 when every mesh using them is a small prop (under 1 m)
    small props   default collision NoCollision (decoration: the room levels' rule, ADR-0027)
    animations    no root motion (the server moves the bodies, ADR-0027)
    report        Saved/FabReport.json and the log: each pack's assets and its weight on disk (GitHub Free LFS)
  With HOLLOW_FAB_MIGRATE=1 it migrates what Build/Art/fab_manifest.json keeps (and what that needs) into the game's
  Content/Fab, unless the game's Content/Fab would then weigh more than the manifest's budget_mb. FAB-01 (ADR-0051):
    a pack still at the staging's root (its "staged_at") is first moved to /Game/Fab/<Pack> by the editor (references
    rewritten, the redirectors' referencers resaved, the redirectors dropped: a move on disk breaks them);
    the migration refuses a kept asset that needs a sound, a map, a Blueprint, input or UI (Cathedral.Look stays green).
  CHAR-01 (ADR-0055): a "lean" pack (too big, or with broken packages of its own, to move whole) is never moved in the
    staging: its keep paths still name /Game/Fab/<Pack>, read at its staged_at; only what they need is prepared and
    migrated (into the game's Content/<Pack>), and the game moves that under /Game/Fab/<Pack>; "prepare": "kept" prepares
    only what a pack's kept assets need there too (a pack already moved, too big to load whole). Such a pack goes with
    what its kept assets need to run (hard references only: a skeleton's preview mesh and preview attachments,
    editor-only soft references to skins and Cascade effects, stay in the staging), copied file by file (what the
    editor's migration does, without its soft dependencies); a needed package this engine cannot load (UE4's retarget
    Rig) stays behind, its referencers resaved without it. Its meshes' hidden_slots get an empty masked material of the
    pack's own first (the weapon a character holds in the pack: its material and textures stay out of the game).
    HOLLOW_FAB_ONLY=<pack,...> limits a run to the packs it names (the others' files in the game stay as they are).
  With HOLLOW_FAB_SELFTEST=1 it checks itself on a fixture (a 4K texture, a small prop with collision), then deletes it.

  In the game (Protocol: Hollow): the migrated textures get their SOURCE downsized to their pack's texture_source_max
  (more for the textures of its hero_texture_source_max meshes, or under a texture_source_max_by_folder folder: CHAR-01,
  a character's own textures above its material layers' tiles): a MaxTextureSize cap only limits the cook, the LFS
  weight is the source's, and the staging keeps the full-size originals (FAB-01). Then it checks /Game/Fab against the
  same rules and the budget, and applies the manifest's swaps (a property of a project asset -> a Fab asset: a tool's held mesh, a material's texture...). Without packs
  or swaps it changes nothing.
"""
import json
import os
import shutil
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

MANIFEST = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Art", "fab_manifest.json")
GAME_PROJECT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
FAB_ROOT = "/Game/Fab"
SKIPPED_FOLDERS = ("/Game/Collections", "/Game/Developers", "/Game/__ExternalActors__", "/Game/__ExternalObjects__")
MAX_TEXTURE = 2048
SMALL_PROP_TEXTURE = 1024
SMALL_PROP_CM = 100.0
NO_COLLISION = "NoCollision"
FIXTURE_DIR = "/Game/HollowFabSelfTest"
FIXTURE_TEXTURE_SIZE = 4096
FIXTURE_MESH = os.path.join(GAME_PROJECT, "Intermediate", "HollowArt", "Kit", "SM_Hub_Crate.fbx")
MEGABYTE = 1024.0 * 1024.0
# FAB-01: what a migrated asset must never pull in (sounds: Hollow.Game.Cathedral.Look; maps, Blueprints, input, UI: the
# pack's own game logic and demo content stay in the staging). CHAR-01: nor a pack's animation blueprints (the game's
# own are built by create_character_assets.py) or its Cascade effects (the game's effects are Niagara).
FORBIDDEN_CLASSES = ("SoundWave", "SoundCue", "SoundAttenuation", "SoundClass", "MetaSoundSource", "World", "Blueprint", "WidgetBlueprint",
                     "LevelSequence", "InputAction", "InputMappingContext", "UserDefinedStruct", "UserDefinedEnum", "DataTable",
                     "AnimBlueprint", "ParticleSystem")
HIDDEN_MATERIAL = "M_Hollow_Hidden"   # CHAR-01: what a pack mesh's hidden_slots wear (masked, nothing drawn)

assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
registry = unreal.AssetRegistryHelpers.get_asset_registry()
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(message):
    unreal.log("prepare_fab_assets: " + message)


def check(condition, message):
    if not condition:
        raise RuntimeError("check failed: " + message)


def in_game_project():
    project = os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path()))
    return os.path.basename(project) == "ProtocolHollow.uproject"


def read_manifest():
    with open(MANIFEST) as f:
        manifest = json.load(f)
    only = [n for n in os.environ.get("HOLLOW_FAB_ONLY", "").split(",") if n]
    if only:   # CHAR-01: a run for some packs only
        check(all(any(p["name"] == n for p in manifest.get("packs", [])) for n in only), "HOLLOW_FAB_ONLY names packs of the manifest: %s" % only)
        manifest["packs"] = [p for p in manifest.get("packs", []) if p["name"] in only]
    return manifest


def staged(pack, name):
    """CHAR-01: where a /Game/Fab/<Pack> path of a lean pack is in the staging (at its staged_at; others as they are)."""
    root = FAB_ROOT + "/" + pack["name"]
    if pack.get("lean") and (name == root or name.startswith(root + "/")):
        return pack["staged_at"] + name[len(root):]
    return name


def staged_keep(manifest):
    return [staged(pack, k) for pack in manifest.get("packs", []) for k in pack.get("keep", [])]


def kept_only(pack):
    """CHAR-01: a pack that goes with only what its kept assets need to run."""
    return bool(pack.get("lean") or pack.get("prepare") == "kept")


def package_files(content_dir, package_name):
    """The files of a /Game package on disk (.uasset/.umap with .uexp/.ubulk)."""
    stem = os.path.join(content_dir, package_name[len("/Game/"):])
    return [stem + ext for ext in (".uasset", ".umap", ".uexp", ".ubulk") if os.path.isfile(stem + ext)]


def weight(content_dir, package_names):
    return sum(os.path.getsize(f) for name in package_names for f in package_files(content_dir, name))


def folder_weight(folder):
    total = 0
    for root, _, files in os.walk(folder):
        total += sum(os.path.getsize(os.path.join(root, f)) for f in files)
    return total


def pack_folders():
    folders = []
    for path in assets.list_assets("/Game", recursive=False, include_folder=True):
        path = path.rstrip("/")
        if assets.does_directory_exist(path) and not path.startswith(SKIPPED_FOLDERS):
            folders.append(path)
    if assets.does_directory_exist(FAB_ROOT):   # the Fab window's own folder holds one folder per pack
        folders = [f for f in folders if f != FAB_ROOT] + [p.rstrip("/") for p in assets.list_assets(FAB_ROOT, recursive=False, include_folder=True)
                                                           if assets.does_directory_exist(p.rstrip("/"))]
    return sorted(set(folders))


def referencing_meshes(texture_package):
    """Static meshes that use a texture through a material (or a material instance of one)."""
    meshes, seen, frontier = [], set(), [texture_package]
    while frontier:
        package = frontier.pop()
        for referencer in registry.get_referencers(package, unreal.AssetRegistryDependencyOptions()) or []:
            name = str(referencer)
            if name in seen:
                continue
            seen.add(name)
            data = registry.get_assets_by_package_name(name)
            classes = [str(d.asset_class_path.asset_name) for d in data]
            if "StaticMesh" in classes:
                meshes.extend(d.get_asset() for d in data if str(d.asset_class_path.asset_name) == "StaticMesh")
            elif any(c.startswith("Material") for c in classes):
                frontier.append(name)
    return meshes


def is_small(mesh):
    box = mesh.get_bounding_box()
    size = box.max - box.min
    return max(size.x, size.y, size.z) < SMALL_PROP_CM


def prepare_folder(folder, fix):
    """Brings a folder's assets to the rules (fix) or only counts what breaks them; returns the counts."""
    return prepare_packages(assets.list_assets(folder, recursive=True, include_folder=False), fix)


def prepare_packages(paths, fix):
    """prepare_folder for the given assets (CHAR-01: a kept-only pack's kept assets and what they need)."""
    counts = {"assets": 0, "textures_capped": 0, "props_no_collision": 0, "root_motion_off": 0, "over_budget": []}
    for path in paths:
        asset = unreal.load_asset(path)
        if asset is None:
            continue
        counts["assets"] += 1
        changed = False
        if isinstance(asset, unreal.Texture2D):
            meshes = referencing_meshes(path.split(".")[0])
            limit = SMALL_PROP_TEXTURE if meshes and all(is_small(m) for m in meshes) else MAX_TEXTURE
            current = asset.get_editor_property("max_texture_size")
            source = max(asset.blueprint_get_size_x(), asset.blueprint_get_size_y())
            if (current == 0 or current > limit) and source > limit:
                counts["over_budget"].append(path)
                if fix:
                    asset.set_editor_property("max_texture_size", limit)
                    counts["textures_capped"] += 1
                    changed = True
        elif isinstance(asset, unreal.StaticMesh) and is_small(asset):
            body = asset.get_editor_property("body_setup")
            instance = body.get_editor_property("default_instance") if body else None
            if instance is not None and str(instance.get_editor_property("collision_profile_name")) != NO_COLLISION:
                counts["over_budget"].append(path)
                if fix:
                    instance.set_editor_property("collision_profile_name", NO_COLLISION)
                    body.set_editor_property("default_instance", instance)
                    counts["props_no_collision"] += 1
                    changed = True
        elif isinstance(asset, unreal.AnimSequence) and asset.get_editor_property("enable_root_motion"):
            counts["over_budget"].append(path)
            if fix:
                asset.set_editor_property("enable_root_motion", False)
                counts["root_motion_off"] += 1
                changed = True
        if changed and not assets.save_loaded_asset(asset, False):
            raise RuntimeError("Could not save " + path)
    return counts


def stage_packs():
    """FAB-01: packs the manifest names still at the staging's root move to /Game/Fab/<Pack>, by the editor."""
    for pack in read_manifest().get("packs", []):
        source = pack.get("staged_at")
        target = FAB_ROOT + "/" + pack["name"]
        if pack.get("lean") or not source or not assets.does_directory_exist(source):
            continue
        if not assets.does_directory_exist(target):
            log("moving %s to %s (references rewritten)" % (source, target))
            if not assets.rename_directory(source, target):
                raise RuntimeError("Could not move %s to %s" % (source, target))
        drop_redirectors(source, target)


def all_stale(folder, target, packages):
    """DRONE-01: a move whose old copies the editor could not delete (a pack's demo content that loads with errors)
    leaves each asset twice. True if every one of packages is such a stale copy: its twin is under target and nothing
    outside folder references it."""
    options = unreal.AssetRegistryDependencyOptions()
    for name in packages:
        outside = [str(r) for r in (registry.get_referencers(name, options) or []) if not str(r).startswith(folder + "/")]
        if outside or not registry.get_assets_by_package_name(target + name[len(folder):]):
            return False
    return True


def drop_redirectors(folder, target):
    """What a move left behind: the redirectors' referencers are resaved (they then name the new paths), then the
    redirectors go with the old folder. Anything but a redirector left there stops the run, unless it is a stale copy
    (all_stale): those are left for the owner to delete, with a warning, and the redirectors stay with them."""
    left = registry.get_assets_by_path(folder, recursive=True)
    others = [str(data.package_name) for data in left if str(data.asset_class_path.asset_name) != "ObjectRedirector"]
    if others and all_stale(folder, target, others):
        unreal.log_warning("prepare_fab_assets: %s still holds %d stale copies of %s (each has its twin there, nothing references them; "
                           "the editor could not delete them): delete the folder by hand" % (folder, len(others), target))
        return
    check(not others, "%s still holds assets after the move: %s" % (folder, ", ".join(others[:6])))
    resaved = set()
    for data in left:
        for referencer in registry.get_referencers(str(data.package_name), unreal.AssetRegistryDependencyOptions()) or []:
            referencer = str(referencer)
            if referencer.startswith(folder + "/") or referencer in resaved:
                continue
            check(assets.save_asset(referencer, only_if_is_dirty=False), "%s: resaved after the move" % referencer)
            resaved.add(referencer)
    for data in left:
        check(assets.delete_asset(str(data.package_name)), "%s: redirector deleted" % data.package_name)
    assets.delete_directory(folder)
    log("%s: %d redirectors dropped, %d referencers resaved" % (folder, len(left), len(resaved)))


def kept_packages(pack):
    """CHAR-01: what a kept-only pack's kept assets need to run, in the staging."""
    return sorted(p for p in closure([staged(pack, k) for k in pack.get("keep", [])], hard_only=True) if registry.get_assets_by_package_name(p))


def drop_unloadable(pack):
    """CHAR-01: a needed package this engine cannot load (a class it no longer has: UE4's retarget Rig) goes nowhere; its
    referencers are resaved without it (it loads as nothing). Returns the pack's kept packages."""
    packages = kept_packages(pack)
    unloadable = [p for p in packages if unreal.load_asset(p) is None]
    if not unloadable:
        return packages
    options = unreal.AssetRegistryDependencyOptions()
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    resaved = []
    for package in unloadable:
        for referencer in sorted(set(str(r) for r in registry.get_referencers(package, options) or []) & set(packages)):
            if referencer not in unloadable and referencer not in resaved:
                check(assets.save_asset(referencer, only_if_is_dirty=False), "%s: resaved without %s" % (referencer, package))
                resaved.append(referencer)
                log("%s: resaved without %s (%s, unloadable here)" % (referencer, package, class_of(package)))
    registry.scan_files_synchronous([f for r in resaved for f in package_files(content, r) if f.endswith(".uasset")], True)   # their new dependencies
    packages = kept_packages(pack)
    check(not [p for p in packages if p in unloadable], "%s: still needs %s" % (pack["name"], unloadable))
    return packages


def hidden_material(pack):
    """CHAR-01: the pack's own empty material (masked, opacity mask 0) for the slots it hides; made once."""
    folder = staged(pack, FAB_ROOT + "/" + pack["name"]) + "/Hollow"
    path = folder + "/" + HIDDEN_MATERIAL
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    material = tools.create_asset(HIDDEN_MATERIAL, folder, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("used_with_skeletal_mesh", True)
    nothing = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant, -200, 0)
    nothing.set_editor_property("r", 0.0)
    unreal.MaterialEditingLibrary.connect_material_property(nothing, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    unreal.MaterialEditingLibrary.recompile_material(material)
    check(assets.save_loaded_asset(material, False), path + " saved")
    log(path + ": made")
    return material


def hide_slots(pack):
    """CHAR-01: the named material slots of a pack mesh wear the hidden material (what the character held in the pack)."""
    for mesh_path, slots in sorted(pack.get("hidden_slots", {}).items()):
        mesh_path = staged(pack, mesh_path)
        if not assets.does_asset_exist(mesh_path):
            continue
        mesh = unreal.load_asset(mesh_path)
        hidden = hidden_material(pack)
        materials = list(mesh.get_editor_property("materials"))
        names = [str(m.get_editor_property("material_slot_name")) for m in materials]
        check(all(slot in names for slot in slots), "%s: has the slots %s (it has %s)" % (mesh_path, slots, names))
        changed = False
        for material in materials:
            if str(material.get_editor_property("material_slot_name")) in slots and material.get_editor_property("material_interface") != hidden:
                material.set_editor_property("material_interface", hidden)
                changed = True
        if changed:
            mesh.set_editor_property("materials", materials)
            check(assets.save_loaded_asset(mesh, False), mesh_path + " saved")
            content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
            fresh = [f for p in (mesh_path, hidden.get_outermost().get_name()) for f in package_files(content, p) if f.endswith(".uasset")]
            registry.scan_files_synchronous(fresh, True)   # the mesh's new dependency, before what it needs is copied
            log("%s: %s hidden" % (mesh_path, ", ".join(slots)))


def staging():
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    report = {}
    stage_packs()
    for pack in read_manifest().get("packs", []):
        hide_slots(pack)
    if not pack_folders():
        log("no packs here yet: add some with the Fab window (Window > Fab > Add to Project)")
    # FAB-01: only the packs the manifest names are prepared; the owner's other packs are never changed.
    manifest = read_manifest()
    lean = [pack for pack in manifest.get("packs", []) if kept_only(pack)]
    named = [FAB_ROOT + "/" + pack["name"] for pack in manifest.get("packs", []) if not kept_only(pack)]
    for folder in pack_folders():
        if folder not in named and folder not in [staged(pack, FAB_ROOT + "/" + pack["name"]) for pack in lean]:
            log("%s: %s, left as it is" % (folder, "not in this run (HOLLOW_FAB_ONLY)" if os.environ.get("HOLLOW_FAB_ONLY") else "not in the manifest"))
    for pack in lean:   # CHAR-01: only what its kept assets need (a lean pack, or one prepared "kept")
        packages = drop_unloadable(pack)
        counts = prepare_packages(packages, fix=True)
        counts["megabytes"] = round(weight(content, packages) / MEGABYTE, 2)
        counts.pop("over_budget")
        report[staged(pack, FAB_ROOT + "/" + pack["name"])] = counts
        log("%s (kept only): %d kept packages, %.2f MB; capped %d textures, %d animations without root motion"
            % (staged(pack, FAB_ROOT + "/" + pack["name"]), counts["assets"], counts["megabytes"], counts["textures_capped"], counts["root_motion_off"]))
    for folder in [f for f in pack_folders() if f in named]:
        counts = prepare_folder(folder, fix=True)
        names = sorted(set(p.split(".")[0] for p in assets.list_assets(folder, recursive=True, include_folder=False)))
        counts["megabytes"] = round(weight(content, names) / MEGABYTE, 2)
        counts.pop("over_budget")
        report[folder] = counts
        log("%s: %d assets, %.2f MB; capped %d textures, %d props without collision, %d animations without root motion"
            % (folder, counts["assets"], counts["megabytes"], counts["textures_capped"], counts["props_no_collision"], counts["root_motion_off"]))
    with open(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "FabReport.json"), "w") as f:
        json.dump(report, f, indent=1, sort_keys=True)
    if os.environ.get("HOLLOW_FAB_MIGRATE") == "1":
        migrate(content)


def migrate(content):
    manifest = read_manifest()
    keep = staged_keep(manifest)
    if not keep:
        log("nothing to migrate: Build/Art/fab_manifest.json keeps no asset")
        return
    lean_roots = tuple(pack["staged_at"] + "/" for pack in manifest.get("packs", []) if pack.get("lean"))
    for name in keep:
        check(name.startswith((FAB_ROOT + "/",) + lean_roots), "%s: keep only assets under %s (move the pack there first, or make it lean)" % (name, FAB_ROOT))
        check(assets.does_asset_exist(name), name + " exists in the staging")
    others = [pack for pack in manifest.get("packs", []) if not kept_only(pack)]
    migrated = closure([staged(pack, k) for pack in others for k in pack.get("keep", [])]) if others else set()
    copied = set(p for pack in manifest.get("packs", []) if kept_only(pack) for p in kept_packages(pack)) - migrated
    packages = migrated | copied
    forbidden = sorted("%s (%s)" % (name, class_of(name)) for name in packages if class_of(name) in FORBIDDEN_CLASSES)
    check(not forbidden, "kept assets need what never goes into the game: %s" % ", ".join(forbidden[:8]))
    adding = weight(content, packages)
    game_fab = os.path.join(GAME_PROJECT, "Content", "Fab")
    existing = folder_weight(game_fab) if os.path.isdir(game_fab) else 0
    budget = manifest["budget_mb"] * MEGABYTE
    log("migration: %d packages, %.2f MB onto %.2f MB already in the game (budget %.0f MB)" % (len(packages), adding / MEGABYTE, existing / MEGABYTE, budget / MEGABYTE))
    check(existing + adding <= budget, "the game's Content/Fab would weigh %.1f MB, over the %.0f MB budget of GitHub Free LFS"
          % ((existing + adding) / MEGABYTE, budget / MEGABYTE))
    if migrated:
        options = unreal.MigrationOptions()
        options.set_editor_property("prompt", False)
        tools.migrate_packages(sorted(migrated), os.path.join(GAME_PROJECT, "Content"), options)
    for package in sorted(copied):   # CHAR-01: the files, as the migration copies them, without the soft dependencies
        for source in package_files(content, package):
            target = os.path.join(GAME_PROJECT, "Content", os.path.relpath(source, content))
            os.makedirs(os.path.dirname(target), exist_ok=True)
            shutil.copy2(source, target)
    log("migrated %d packages (%d copied with their hard references only) into %s (run this script in the game next: it "
        "downsizes their texture sources)" % (len(packages), len(copied), os.path.join(GAME_PROJECT, "Content")))


def closure(names, hard_only=False):
    """The packages and what they need inside the project (hard_only: what they need to run, CHAR-01)."""
    options = unreal.AssetRegistryDependencyOptions()
    if hard_only:
        options.set_editor_property("include_soft_package_references", False)
    packages = set()
    frontier = list(name.split(".")[0] for name in names)
    while frontier:
        name = frontier.pop()
        if name in packages:
            continue
        packages.add(name)
        for dependency in registry.get_dependencies(name, options) or []:
            dependency = str(dependency)
            if dependency.startswith("/Game/"):
                frontier.append(dependency)
    return packages


def class_of(package):
    data = registry.get_assets_by_package_name(package)
    return str(data[0].asset_class_path.asset_name) if data else ""


def resize_sources(manifest, packages):
    """FAB-01: the migrated textures' sources down to their pack's size (a hero mesh's textures to its own)."""
    limits = {}
    for pack in manifest.get("packs", []):
        root = FAB_ROOT + "/" + pack["name"] + "/"
        for name in packages:
            if name.startswith(root):
                limits[name] = pack.get("texture_source_max", MAX_TEXTURE)
        folders = sorted(pack.get("texture_source_max_by_folder", {}).items(), key=lambda item: len(item[0]))
        for name in packages:   # CHAR-01: the deepest folder that holds it decides (a character's own textures)
            for folder, size in folders:
                if name.startswith(folder.rstrip("/") + "/"):
                    limits[name] = size
        heroes = pack.get("hero_texture_source_max", {})
        for hero, size in heroes.items():   # only what no other kept asset shares: the pack's master textures stay small
            shared = closure([k for p in manifest.get("packs", []) for k in p.get("keep", []) if k != hero and assets.does_asset_exist(k)])
            for name in closure([hero]) - shared:
                if name in limits:
                    limits[name] = max(limits[name], size)
    resized = 0
    for name, limit in sorted(limits.items()):
        if class_of(name) != "Texture2D":
            continue
        texture = unreal.load_asset(name)
        # CHAR-01: the source decides, not the platform size (a LOD group or MaxTextureSize hides a big source).
        result = unreal.TextureAssetActionsBlueprintFns.resize_texture_source(texture, limit)
        if result == unreal.TextureAssetActionsBlueprintReturn.ALREADY_DONE:
            continue
        check(result != unreal.TextureAssetActionsBlueprintReturn.ERROR, "%s: its source resizes to %d" % (name, limit))
        if texture.get_editor_property("max_texture_size") > limit:
            texture.set_editor_property("max_texture_size", limit)
        if not assets.save_loaded_asset(texture, False):
            raise RuntimeError("Could not save " + name)
        resized += 1
    log("sources: %d textures downsized (of %d migrated)" % (resized, sum(1 for name in limits if class_of(name) == "Texture2D")))


def self_test():
    """A 4K texture on a 60 cm prop with collision: capped to 1K, the prop without collision; then gone."""
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    png = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_intermediate_dir()), "HollowFabSelfTest.png")
    write_png(png, FIXTURE_TEXTURE_SIZE)
    check(os.path.isfile(FIXTURE_MESH), "the kit's crate FBX (run Build/Python/create_art_kit.py in the game once)")
    for source, name in ((png, "T_Fixture"), (FIXTURE_MESH, "SM_Fixture")):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source)
        task.set_editor_property("destination_path", FIXTURE_DIR)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        tools.import_asset_tasks([task])
    texture = unreal.load_asset(FIXTURE_DIR + "/T_Fixture")
    mesh = unreal.load_asset(FIXTURE_DIR + "/SM_Fixture")
    check(texture is not None and mesh is not None, "the fixture imported")
    material = tools.create_asset("M_Fixture", FIXTURE_DIR, unreal.Material, unreal.MaterialFactoryNew())
    sample = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureSample, -300, 0)
    sample.set_editor_property("texture", texture)
    unreal.MaterialEditingLibrary.connect_material_property(sample, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mesh.set_material(0, material)
    assets.save_directory(FIXTURE_DIR)
    counts = prepare_folder(FIXTURE_DIR, fix=True)
    check(texture.get_editor_property("max_texture_size") in (SMALL_PROP_TEXTURE, MAX_TEXTURE), "the 4K texture is capped (%d)" % texture.get_editor_property("max_texture_size"))
    body = mesh.get_editor_property("body_setup")
    check(str(body.get_editor_property("default_instance").get_editor_property("collision_profile_name")) == NO_COLLISION, "the small prop has no collision")
    capped = texture.get_editor_property("max_texture_size")
    again = prepare_folder(FIXTURE_DIR, fix=False)
    check(not again["over_budget"], "a second pass finds nothing (%s)" % again["over_budget"])
    # The fixture goes: its packages leave memory with the editor (it quits next), its files go now.
    del texture, mesh, material, sample, body
    unreal.SystemLibrary.collect_garbage()
    shutil.rmtree(os.path.join(content, FIXTURE_DIR[len("/Game/"):]), ignore_errors=True)
    os.remove(png)
    log("self-test passed (texture capped to %d, %d prop without collision)" % (capped, counts["props_no_collision"]))


def write_png(path, size):
    """A plain grey square PNG (no imaging library in the editor's Python)."""
    import struct
    import zlib
    row = b"\x00" + b"\x80\x80\x80" * size
    data = zlib.compress(row * size, 9)
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xffffffff)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)) + chunk(b"IDAT", data) + chunk(b"IEND", b""))


def settle_lean(manifest):
    """CHAR-01: a lean pack's migrated assets arrive at the game's Content/<Pack>: they move under /Game/Fab/<Pack> (a
    second migration replaces the copy there: that folder is the migration's own, ADR-0051)."""
    content = os.path.join(GAME_PROJECT, "Content")
    for pack in manifest.get("packs", []):
        source, target = pack.get("staged_at"), FAB_ROOT + "/" + pack["name"]
        if not pack.get("lean") or not source or not assets.does_directory_exist(source):
            continue
        if assets.does_directory_exist(target):   # a second migration: the pack arrives whole again, its old copy goes
            shutil.rmtree(os.path.join(content, target[len("/Game/"):]))
            registry.scan_paths_synchronous([target], True)
            log("%s: the previous copy removed (the migration brings it whole)" % target)
        log("moving %s to %s (references rewritten)" % (source, target))
        check(assets.rename_directory(source, target), "%s moved to %s" % (source, target))
        # The migration's own copies the editor could not delete (each has its twin, nothing references them): their
        # files go, so the game holds each asset once; then the redirectors, and the registry sees the moved pack.
        left = sorted(set(str(d.package_name) for d in registry.get_assets_by_path(source, recursive=True)
                          if str(d.asset_class_path.asset_name) != "ObjectRedirector"))
        removed = bool(left) and all_stale(source, target, left)
        if removed:
            for package in left:
                for path in package_files(content, package):
                    os.remove(path)
            log("%s: %d stale copies of the migration removed" % (source, len(left)))
        if not removed or any(str(d.asset_class_path.asset_name) == "ObjectRedirector" for d in registry.get_assets_by_path(source, recursive=True)):
            drop_redirectors(source, target)
        for folder, _, files in sorted(os.walk(os.path.join(content, source[len("/Game/"):])), reverse=True):
            if not files and not os.listdir(folder):   # only the emptied folders go
                os.rmdir(folder)
        registry.scan_paths_synchronous([target], True)


def game():
    manifest = read_manifest()
    settle_lean(manifest)
    keep = [k for pack in manifest.get("packs", []) for k in pack.get("keep", []) if assets.does_asset_exist(k)]
    if keep:
        packages = closure(keep)
        for pack in manifest.get("packs", []):   # CHAR-01: and all a pack's folder holds (just moved: not every dependency is known yet)
            folder = FAB_ROOT + "/" + pack["name"]
            if assets.does_directory_exist(folder):
                packages |= set(p.split(".")[0] for p in assets.list_assets(folder, recursive=True, include_folder=False))
        forbidden = sorted("%s (%s)" % (name, class_of(name)) for name in packages if class_of(name) in FORBIDDEN_CLASSES)
        check(not forbidden, "the kept Fab assets need what never goes into the game: %s" % ", ".join(forbidden[:8]))
        resize_sources(manifest, packages)
    if assets.does_directory_exist(FAB_ROOT):
        counts = prepare_folder(FAB_ROOT, fix=False)
        check(not counts["over_budget"], "Fab assets break the rules (run this in the staging first): %s" % counts["over_budget"][:10])
        megabytes = folder_weight(os.path.join(GAME_PROJECT, "Content", "Fab")) / MEGABYTE
        check(megabytes <= manifest["budget_mb"], "Content/Fab weighs %.1f MB, over the %.0f MB budget" % (megabytes, manifest["budget_mb"]))
        log("/Game/Fab: %d assets, %.2f MB of %.0f MB" % (counts["assets"], megabytes, manifest["budget_mb"]))
    swapped = 0
    for swap in manifest.get("swaps", []):
        target = unreal.load_asset(swap["asset"])
        value = unreal.load_asset(swap["value"])
        check(target is not None and value is not None, "swap %s.%s -> %s: both exist" % (swap["asset"], swap["property"], swap["value"]))
        holder = target
        if "component" in swap:   # a blueprint's component: its class default's subobject
            holder = unreal.get_default_object(target.generated_class()).get_editor_property(swap["component"])
        if holder.get_editor_property(swap["property"]) != value:
            holder.set_editor_property(swap["property"], value)
            if not assets.save_loaded_asset(target, False):
                raise RuntimeError("Could not save " + swap["asset"])
            swapped += 1
            log("swap: %s.%s -> %s" % (swap["asset"], swap["property"], swap["value"]))
    log("%d swap(s) applied of %d" % (swapped, len(manifest.get("swaps", []))))


def main():
    if in_game_project():
        game()
    elif os.environ.get("HOLLOW_FAB_SELFTEST") == "1":
        self_test()
    else:
        staging()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("prepare_fab_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
