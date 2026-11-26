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
  With HOLLOW_FAB_SELFTEST=1 it checks itself on a fixture (a 4K texture, a small prop with collision), then deletes it.

  In the game (Protocol: Hollow): the migrated textures get their SOURCE downsized to their pack's texture_source_max
  (more for the textures of its hero_texture_source_max meshes): a MaxTextureSize cap only limits the cook, the LFS
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
# pack's own game logic and demo content stay in the staging).
FORBIDDEN_CLASSES = ("SoundWave", "SoundCue", "SoundAttenuation", "SoundClass", "MetaSoundSource", "World", "Blueprint", "WidgetBlueprint",
                     "LevelSequence", "InputAction", "InputMappingContext", "UserDefinedStruct", "UserDefinedEnum", "DataTable")

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
        return json.load(f)


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
    counts = {"assets": 0, "textures_capped": 0, "props_no_collision": 0, "root_motion_off": 0, "over_budget": []}
    for path in assets.list_assets(folder, recursive=True, include_folder=False):
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
        if not source or not assets.does_directory_exist(source):
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


def staging():
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    report = {}
    stage_packs()
    if not pack_folders():
        log("no packs here yet: add some with the Fab window (Window > Fab > Add to Project)")
    # FAB-01: only the packs the manifest names are prepared; the owner's other packs are never changed.
    named = [FAB_ROOT + "/" + pack["name"] for pack in read_manifest().get("packs", [])]
    for folder in pack_folders():
        if folder not in named:
            log("%s: not in the manifest, left as it is" % folder)
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
    keep = [k for pack in manifest.get("packs", []) for k in pack.get("keep", [])]
    if not keep:
        log("nothing to migrate: Build/Art/fab_manifest.json keeps no asset")
        return
    for name in keep:
        check(name.startswith(FAB_ROOT + "/"), "%s: keep only assets under %s (move the pack there first)" % (name, FAB_ROOT))
        check(assets.does_asset_exist(name), name + " exists in the staging")
    packages = closure(keep)
    forbidden = sorted("%s (%s)" % (name, class_of(name)) for name in packages if class_of(name) in FORBIDDEN_CLASSES)
    check(not forbidden, "kept assets need what never goes into the game: %s" % ", ".join(forbidden[:8]))
    adding = weight(content, packages)
    game_fab = os.path.join(GAME_PROJECT, "Content", "Fab")
    existing = folder_weight(game_fab) if os.path.isdir(game_fab) else 0
    budget = manifest["budget_mb"] * MEGABYTE
    log("migration: %d packages, %.2f MB onto %.2f MB already in the game (budget %.0f MB)" % (len(packages), adding / MEGABYTE, existing / MEGABYTE, budget / MEGABYTE))
    check(existing + adding <= budget, "the game's Content/Fab would weigh %.1f MB, over the %.0f MB budget of GitHub Free LFS"
          % ((existing + adding) / MEGABYTE, budget / MEGABYTE))
    options = unreal.MigrationOptions()
    options.set_editor_property("prompt", False)
    tools.migrate_packages(sorted(packages), os.path.join(GAME_PROJECT, "Content"), options)
    log("migrated %d packages into %s (run this script in the game next: it downsizes their texture sources)"
        % (len(packages), os.path.join(GAME_PROJECT, "Content")))


def closure(names):
    """The packages and what they need inside the project."""
    packages = set()
    frontier = list(name.split(".")[0] for name in names)
    while frontier:
        name = frontier.pop()
        if name in packages:
            continue
        packages.add(name)
        for dependency in registry.get_dependencies(name, unreal.AssetRegistryDependencyOptions()) or []:
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
        if max(texture.blueprint_get_size_x(), texture.blueprint_get_size_y()) <= limit:
            continue
        result = unreal.TextureAssetActionsBlueprintFns.resize_texture_source(texture, limit)
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


def game():
    manifest = read_manifest()
    keep = [k for pack in manifest.get("packs", []) for k in pack.get("keep", []) if assets.does_asset_exist(k)]
    if keep:
        packages = closure(keep)
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
