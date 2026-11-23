"""Creates the S5-03 hub shuttle Skiff (idempotent, ADR-0010, ADR-0032).

  /Game/Hollow/Hub/BP_HubTerminal                  AHollowHubTerminal: greybox cabinet and screen (the art pass replaces them)
  /Game/Hollow/Hub/Contracts/DA_Contract_Docks     UHollowContractDefinition: salvage in the Orbital Docks (HollowSector_Docks)
  /Game/Hollow/Hub/Contracts/DA_Contract_Cathedral UHollowContractDefinition: purification in the Reactor Cathedral
                                                   (HollowSector_Cathedral, S7-04, ADR-0041); a contract picks the sector
  /Game/Hollow/Maps/L_Hub_Skiff                    the shuttle's cabin: shell, the contracts terminal in the cockpit, the
                                                   workbench and the locker along the sides, four player starts, a calm
                                                   light rig within the hub's DA_LightBudget limits, the hangar's reverb
                                                   and room tone, the raid's exposure

Every level actor it places is tagged HollowHub; a Note "HollowHubSpec" keeps the spec's hash, so the level is re-saved
only when the spec changes. Run after create_lighting_assets.py and create_audio_assets.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_hub_assets.py
"""
import hashlib
import json
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

HUB_DIR = "/Game/Hollow/Hub"
TERMINAL_BP = HUB_DIR + "/BP_HubTerminal"
CONTRACT_DIR = HUB_DIR + "/Contracts"
HUB_MAP = "/Game/Hollow/Maps/L_Hub_Skiff"
MATERIALS = "/Game/Hollow/World/Materials"
REVERB = "/Game/Hollow/Audio/Reverb/RE_Hangar"
ROOM_TONE = "/Game/Hollow/Audio/MetaSounds/MS_Room_Tone"
CUBE = "/Engine/BasicShapes/Cube.Cube"
SHAPE_SIZE = 100.0             # engine basic shapes are 100 uu
VOLUME_BRUSH = 200.0           # a spawned volume's default cube brush (uu)
MANAGED_TAG = "HollowHub"
MARKER_LABEL = "HollowHubSpec"
INI = "Config/DefaultGame.ini"
INI_LINES = ["HubMap=/Game/Hollow/Maps/L_Hub_Skiff.L_Hub_Skiff", '+MapsToCook=(FilePath="/Game/Hollow/Maps/L_Hub_Skiff")',
             "DefaultContract=HollowContract:DA_Contract_Docks", 'PrimaryAssetType="HollowContract"']

# The contracts (data, S5-03 clarification 6; the Cathedral's S7-04): the sector Game Feature each raid assembles, its name
# on the screen, and the items it pays with (priced on the screen as their extraction pays them, ADR-0041).
CONTRACTS = [
    {"name": "DA_Contract_Docks", "display_name": "Orbital Docks salvage", "sector_feature": "HollowSector_Docks",
     "sector_name": "Sector I - Orbital Docks",
     "description": "Recover an Aether core from the derelict docks and bring it to the extraction shuttle.",
     "rewards": ["DA_Item_AetherCore"]},
    {"name": "DA_Contract_Cathedral", "display_name": "Reactor Cathedral purification", "sector_feature": "HollowSector_Cathedral",
     "sector_name": "Sector II - Reactor Cathedral",
     "description": "Carry the Aether core to the Purification Altar, hold out through the ritual and bring the purified core home.",
     "rewards": ["DA_Item_AetherCore", "DA_Item_PurifiedCore"]},
]

# The terminal (data): a cabinet (depth along its X, which points at its user), a screen on its face, where its user stands.
# Chest high: a player looking ahead from the eyes (about 160 cm) at the use spot still focuses it (ProbeRadius 8, range 250).
TERMINAL = {"version": 4, "cabinet": {"location": (0.0, 0.0, 70.0), "size": (60.0, 120.0, 140.0), "material": "MI_Hull_Wall"},
            "screen": {"location": (31.0, 0.0, 105.0), "size": (2.0, 100.0, 50.0), "material": "MI_Screen"}, "use_distance": 140.0}

# The cabin (data, S5-03 clarification 4): X runs from the ramp (0) to the cockpit (1600), Y across, Z up.
CABIN = (1600.0, 800.0, 400.0)
SKIN = 20.0                    # shell thickness, outside the cabin
SHELL = [   # (label, material, lo, hi): collision, the pawns walk inside it
    ("Floor", "MI_Hull_Floor", (-SKIN, -SKIN, -SKIN), (CABIN[0] + SKIN, CABIN[1] + SKIN, 0.0)),
    ("Ceiling", "MI_Hull_Ceiling", (-SKIN, -SKIN, CABIN[2]), (CABIN[0] + SKIN, CABIN[1] + SKIN, CABIN[2] + SKIN)),
    ("Wall_Port", "MI_Hull_Wall", (-SKIN, -SKIN, 0.0), (CABIN[0] + SKIN, 0.0, CABIN[2])),
    ("Wall_Starboard", "MI_Hull_Wall", (-SKIN, CABIN[1], 0.0), (CABIN[0] + SKIN, CABIN[1] + SKIN, CABIN[2])),
    ("Wall_Ramp", "MI_Hull_Wall", (-SKIN, 0.0, 0.0), (0.0, CABIN[1], CABIN[2])),
    ("Wall_Cockpit", "MI_Hull_Wall", (CABIN[0], 0.0, 0.0), (CABIN[0] + SKIN, CABIN[1], CABIN[2])),
]
DRESSING = [   # (label, material, lo, hi): looks only, no collision
    ("RampDoor", "MI_Hazard", (0.0, 200.0, 0.0), (4.0, 600.0, 350.0)),
    ("CeilingStrip_Port", "MI_Strip_Cool", (100.0, 40.0, 394.0), (1500.0, 46.0, 400.0)),
    ("CeilingStrip_Starboard", "MI_Strip_Cool", (100.0, 754.0, 394.0), (1500.0, 760.0, 400.0)),
    ("CockpitWindow", "MI_Screen", (1596.0, 220.0, 150.0), (1600.0, 580.0, 300.0)),
]
TERMINALS = [   # (label, kind, location, yaw, prompt, guest prompt)
    ("Terminal_Contracts", "CONTRACTS", (1490.0, 400.0, 0.0), 180.0, "Contracts", "Contracts (host decides)"),
    ("Terminal_Workbench", "WORKBENCH", (800.0, 765.0, 0.0), -90.0, "Workbench", ""),
    ("Terminal_Locker", "LOCKER", (800.0, 35.0, 0.0), 90.0, "Locker", ""),
]
PLAYER_STARTS = [(450.0, 300.0), (450.0, 500.0), (650.0, 300.0), (650.0, 500.0)]
PLAYER_START_HEIGHT = 100.0
# Lights (S5-03 clarification 4: calm, at most 8, 2 shadowed, no light function). Lumens; radius in uu. A sealed cabin
# keeps its bounce light: a quarter of a raid room's practical each, dim pools at the terminals (the first build at raid
# strength washed the dark hull out to white, screenshot of the hub smoke).
LIGHTS = [
    {"label": "Practical_Ramp", "kind": "rect", "location": (300.0, 400.0, 388.0), "intensity": 800.0, "radius": 900.0, "shadowed": False},
    {"label": "Practical_Middle", "kind": "rect", "location": (800.0, 400.0, 388.0), "intensity": 1000.0, "radius": 900.0, "shadowed": True},
    {"label": "Practical_Fore", "kind": "rect", "location": (1300.0, 400.0, 388.0), "intensity": 800.0, "radius": 900.0, "shadowed": False},
    {"label": "Cockpit", "kind": "spot", "location": (1420.0, 400.0, 380.0), "intensity": 900.0, "radius": 700.0, "shadowed": True},
    {"label": "Workbench", "kind": "point", "location": (800.0, 690.0, 240.0), "intensity": 300.0, "radius": 400.0, "shadowed": False,
     "color": (255, 210, 160)},
    {"label": "Locker", "kind": "point", "location": (800.0, 110.0, 240.0), "intensity": 300.0, "radius": 400.0, "shadowed": False,
     "color": (170, 210, 255)},
]
LIGHT_COLOR = (200, 225, 255)
RECT = {"source_width": 120.0, "source_height": 30.0, "barn_door_angle": 70.0}
SPOT = {"inner_cone_angle": 25.0, "outer_cone_angle": 45.0, "pitch": -70.0}
HUB_LIMITS = {"max_lights": 8, "max_shadowed": 2, "max_light_functions": 0}   # DA_LightBudget, Hollow.Room.Type.Hub
# The cabin keeps its dim look: the raid's histogram exposure would brighten the dim rig back to daylight (it may lift a
# scene to -2 EV), so the hub may not brighten below 0 EV and sits a stop under it.
POST = {"auto_exposure_method": "AEM_HISTOGRAM", "auto_exposure_min_brightness": 0.0, "auto_exposure_max_brightness": 4.0,
        "auto_exposure_bias": -1.0, "bloom_intensity": 0.4, "vignette_intensity": 0.3, "film_grain_intensity": 0.1}
ACOUSTICS = {"reverb_volume": 0.6, "reverb_fade": 0.6, "tone_volume": 0.35, "tone_falloff": 300.0}

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(message):
    unreal.log("create_hub_assets: " + message)


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


def material(name):
    return load(MATERIALS + "/" + name)


def vector(xyz):
    return unreal.Vector(xyz[0], xyz[1], xyz[2])


def colour(rgb):
    return unreal.Color(r=rgb[0], g=rgb[1], b=rgb[2], a=255)


def digest(*specs):
    return hashlib.sha1(json.dumps(specs, sort_keys=True).encode("utf-8")).hexdigest()


def tags_of(actor):
    return [str(t) for t in actor.get_editor_property("tags")]


def same_vector(a, b):
    return all(abs(x - y) < 0.01 for x, y in zip((a.x, a.y, a.z), b))


# ---------------------------------------------------------------- contract and terminal

def item_id(name):
    asset_id = unreal.PrimaryAssetId()
    if not asset_id.import_text("HollowItem:" + name):
        raise RuntimeError("Bad item id " + name)
    return asset_id


def contract_matches(contract, spec):
    return str(contract.get_editor_property("display_name")) == spec["display_name"] \
        and str(contract.get_editor_property("description")) == spec["description"] \
        and str(contract.get_editor_property("sector_name")) == spec["sector_name"] \
        and contract.get_editor_property("sector_feature") == spec["sector_feature"] \
        and [r.export_text() for r in contract.get_editor_property("reward_items")] == [item_id(n).export_text() for n in spec["rewards"]]


def ensure_contract(spec):
    path = CONTRACT_DIR + "/" + spec["name"]
    if assets.does_asset_exist(path):
        contract, created = load(path), False
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.HollowContractDefinition)
        contract, created = create(path, unreal.HollowContractDefinition, factory), True
    if not created and contract_matches(contract, spec):
        return contract
    contract.set_editor_property("display_name", unreal.Text(spec["display_name"]))
    contract.set_editor_property("description", unreal.Text(spec["description"]))
    contract.set_editor_property("sector_name", unreal.Text(spec["sector_name"]))
    contract.set_editor_property("sector_feature", spec["sector_feature"])
    contract.set_editor_property("reward_items", [item_id(n) for n in spec["rewards"]])
    save(contract)
    log(spec["name"] + " ready")
    return contract


def set_part(component, spec):
    """A greybox part of the terminal: the engine cube sized and placed relative to its base; True if it changed.
    A part the art pass gave its own mesh is left alone."""
    current = component.get_editor_property("static_mesh")
    if current is not None and current != load(CUBE):
        return False   # the art pass dressed it (create_art_kit.py, Art-01): its mesh, place and materials are art's
    scale = tuple(s / SHAPE_SIZE for s in spec["size"])
    wanted_material = material(spec["material"])
    if component.get_editor_property("static_mesh") == load(CUBE) and same_vector(component.get_editor_property("relative_location"), spec["location"]) \
            and same_vector(component.get_editor_property("relative_scale3d"), scale) and component.get_material(0) == wanted_material:
        return False
    component.set_editor_property("static_mesh", load(CUBE))
    component.set_editor_property("relative_location", vector(spec["location"]))
    component.set_editor_property("relative_scale3d", vector(scale))
    component.set_material(0, wanted_material)
    return True


def ensure_terminal():
    if assets.does_asset_exist(TERMINAL_BP):
        terminal_bp = load(TERMINAL_BP)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.HollowHubTerminal)
        terminal_bp = create(TERMINAL_BP, unreal.Blueprint, factory)
    terminal = unreal.get_default_object(terminal_bp.generated_class())
    changed = set_part(terminal.get_editor_property("cabinet"), TERMINAL["cabinet"])
    changed = set_part(terminal.get_editor_property("screen"), TERMINAL["screen"]) or changed
    if abs(terminal.get_editor_property("use_distance") - TERMINAL["use_distance"]) > 0.01:
        terminal.set_editor_property("use_distance", TERMINAL["use_distance"])
        changed = True
    if changed:
        save(terminal_bp)
        log("BP_HubTerminal ready")
    return terminal_bp.generated_class()


# ---------------------------------------------------------------- the cabin

def hub_spec():
    return {"version": 1, "cabin": CABIN, "shell": SHELL, "dressing": DRESSING, "terminals": TERMINALS, "starts": PLAYER_STARTS,
            "start_height": PLAYER_START_HEIGHT, "lights": LIGHTS, "light_color": LIGHT_COLOR, "rect": RECT, "spot": SPOT, "post": POST,
            "acoustics": ACOUSTICS, "terminal": TERMINAL}


def managed(actor, label, folder):
    if actor is None:
        raise RuntimeError("Could not spawn " + label)
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.set_editor_property("tags", [MANAGED_TAG])
    return actor


def spawn_box(label, material_name, lo, hi, folder, collision):
    centre = tuple((a + b) / 2.0 for a, b in zip(lo, hi))
    actor = managed(actors.spawn_actor_from_class(unreal.StaticMeshActor, vector(centre), unreal.Rotator(0.0, 0.0, 0.0)), label, folder)
    component = actor.static_mesh_component
    component.set_static_mesh(load(CUBE))
    component.set_material(0, material(material_name))
    if not collision:
        component.set_collision_profile_name("NoCollision")   # the profile itself: a loaded actor takes its collision from it (ADR-0027)
        component.set_editor_property("can_ever_affect_navigation", False)
        component.set_editor_property("cast_shadow", False)
    actor.set_actor_scale3d(unreal.Vector(*[(b - a) / SHAPE_SIZE for a, b in zip(lo, hi)]))
    return actor


def spawn_light(spec):
    location = vector(spec["location"])
    if spec["kind"] == "rect":
        actor = actors.spawn_actor_from_class(unreal.RectLight, location, unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0))
        component = actor.rect_light_component
        for key, value in RECT.items():
            component.set_editor_property(key, value)
    elif spec["kind"] == "spot":
        actor = actors.spawn_actor_from_class(unreal.SpotLight, location, unreal.Rotator(roll=0.0, pitch=SPOT["pitch"], yaw=0.0))
        component = actor.spot_light_component
        component.set_editor_property("inner_cone_angle", SPOT["inner_cone_angle"])
        component.set_editor_property("outer_cone_angle", SPOT["outer_cone_angle"])
    else:
        actor = actors.spawn_actor_from_class(unreal.PointLight, location, unreal.Rotator(0.0, 0.0, 0.0))
        component = actor.point_light_component
    managed(actor, spec["label"], "Lights")
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
    component.set_editor_property("intensity", spec["intensity"])
    component.set_editor_property("light_color", colour(spec.get("color", LIGHT_COLOR)))
    component.set_editor_property("attenuation_radius", spec["radius"])
    component.set_editor_property("cast_shadows", spec["shadowed"])
    component.set_editor_property("cast_volumetric_shadow", False)
    return actor


def spawn_post():
    post = managed(actors.spawn_actor_from_class(unreal.PostProcessVolume, vector((CABIN[0] / 2.0, CABIN[1] / 2.0, CABIN[2] / 2.0)),
                                                 unreal.Rotator(0.0, 0.0, 0.0)), "CabinPost", "Atmosphere")
    post.set_editor_property("unbound", True)
    settings = post.get_editor_property("settings")
    for key, value in POST.items():
        settings.set_editor_property("override_" + key, True)
        settings.set_editor_property(key, getattr(unreal.AutoExposureMethod, value) if key == "auto_exposure_method" else value)
    post.set_editor_property("settings", settings)


def spawn_acoustics():
    centre = vector((CABIN[0] / 2.0, CABIN[1] / 2.0, CABIN[2] / 2.0))
    zero = unreal.Rotator(0.0, 0.0, 0.0)
    volume = managed(actors.spawn_actor_from_class(unreal.AudioGameplayVolume, centre, zero), "CabinAcoustics", "Atmosphere")
    volume.set_actor_scale3d(unreal.Vector(CABIN[0] / VOLUME_BRUSH, CABIN[1] / VOLUME_BRUSH, CABIN[2] / VOLUME_BRUSH))
    reverb = unreal.HollowAssetAuthoring.add_instance_component(volume, unreal.ReverbVolumeComponent, "Reverb")
    settings = reverb.get_editor_property("reverb_settings")
    settings.set_editor_property("apply_reverb", True)
    settings.set_editor_property("reverb_effect", load(REVERB))
    settings.set_editor_property("volume", ACOUSTICS["reverb_volume"])
    settings.set_editor_property("fade_time", ACOUSTICS["reverb_fade"])
    reverb.set_editor_property("reverb_settings", settings)
    tone = managed(actors.spawn_actor_from_class(unreal.AmbientSound, centre, zero), "CabinTone", "Atmosphere")
    component = tone.get_editor_property("audio_component")
    component.set_editor_property("sound", load(ROOM_TONE))
    component.set_editor_property("volume_multiplier", ACOUSTICS["tone_volume"])
    component.set_editor_property("override_attenuation", True)
    attenuation = component.get_editor_property("attenuation_overrides")
    attenuation.set_editor_property("attenuation_shape", unreal.AttenuationShape.BOX)
    attenuation.set_editor_property("attenuation_shape_extents", unreal.Vector(CABIN[0] / 2.0, CABIN[1] / 2.0, CABIN[2] / 2.0))
    attenuation.set_editor_property("falloff_distance", ACOUSTICS["tone_falloff"])
    attenuation.set_editor_property("spatialize", False)   # it is the cabin itself, from everywhere
    component.set_editor_property("attenuation_overrides", attenuation)


def ensure_map(terminal_class):
    wanted = digest(hub_spec())
    if assets.does_asset_exist(HUB_MAP):
        if not levels.load_level(HUB_MAP):
            raise RuntimeError("Could not load " + HUB_MAP)
    elif not levels.new_level(HUB_MAP):
        raise RuntimeError("new_level failed for " + HUB_MAP)
    level_actors = actors.get_all_level_actors()
    marker = next((a for a in level_actors if a.get_actor_label() == MARKER_LABEL), None)
    if marker is not None and wanted in tags_of(marker):
        return False
    for actor in level_actors:
        if MANAGED_TAG in tags_of(actor):
            actors.destroy_actor(actor)
    for label, material_name, lo, hi in SHELL:
        spawn_box(label, material_name, lo, hi, "Shell", collision=True)
    for label, material_name, lo, hi in DRESSING:
        spawn_box(label, material_name, lo, hi, "Dressing", collision=False)
    for label, kind, location, yaw, prompt, guest in TERMINALS:
        terminal = managed(actors.spawn_actor_from_class(terminal_class, vector(location), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw)), label, "Terminals")
        terminal.set_editor_property("kind", getattr(unreal.HollowTerminalKind, kind))
        terminal.set_editor_property("prompt", unreal.Text(prompt))
        terminal.set_editor_property("guest_prompt", unreal.Text(guest))
        # On the instance too: a value set on the blueprint's class defaults did not reach the placed terminals.
        terminal.set_editor_property("use_distance", TERMINAL["use_distance"])
    for index, (x, y) in enumerate(PLAYER_STARTS):
        managed(actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(x, y, PLAYER_START_HEIGHT), unreal.Rotator(0.0, 0.0, 0.0)),
                "PlayerStart_%d" % index, "Starts")
    for spec in LIGHTS:
        spawn_light(spec)
    spawn_post()
    spawn_acoustics()
    note = managed(actors.spawn_actor_from_class(unreal.Note, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0)), MARKER_LABEL, "Atmosphere")
    note.set_editor_property("tags", [MANAGED_TAG, wanted])
    if not levels.save_current_level():
        raise RuntimeError("Could not save " + HUB_MAP)
    log("L_Hub_Skiff built")
    return True


# ---------------------------------------------------------------- validation

def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate(terminal_class):
    with open(unreal.Paths.project_dir() + INI) as ini:
        text = ini.read()
    for line in INI_LINES:
        check(line in text, "DefaultGame.ini: " + line)
    check("RaidSector=" not in text, "DefaultGame.ini: no RaidSector (the contract picks the sector, ADR-0041)")
    for spec in CONTRACTS:
        contract = load(CONTRACT_DIR + "/" + spec["name"])
        check(contract_matches(contract, spec), spec["name"] + " matches its spec")
        sector = spec["sector_feature"]
        check(assets.does_asset_exist("/%s/%s" % (sector, sector)), "%s: sector %s is a Game Feature (its GameFeatureData)" % (spec["name"], sector))
    check(unreal.MathLibrary.class_is_child_of(terminal_class, unreal.HollowHubTerminal.static_class()), "BP_HubTerminal is a hub terminal")
    if not levels.load_level(HUB_MAP):
        raise RuntimeError("Could not load " + HUB_MAP)
    level_actors = actors.get_all_level_actors()
    placed = [a for a in level_actors if isinstance(a, unreal.HollowHubTerminal)]
    kinds = sorted(str(a.get_editor_property("kind")) for a in placed)
    check(len(kinds) == 3 and len(set(kinds)) == 3, "one terminal of each kind (%s)" % ", ".join(kinds))
    check(all(abs(a.get_editor_property("use_distance") - TERMINAL["use_distance"]) < 0.01 for a in placed), "every terminal's user stands in front of it")
    check(sum(1 for a in level_actors if isinstance(a, unreal.PlayerStart)) == len(PLAYER_STARTS), "four player starts")
    lights = [c for a in level_actors for c in a.get_components_by_class(unreal.LightComponent)]
    check(len(lights) <= HUB_LIMITS["max_lights"], "at most %d lights (%d)" % (HUB_LIMITS["max_lights"], len(lights)))
    check(sum(1 for c in lights if c.get_editor_property("cast_shadows")) <= HUB_LIMITS["max_shadowed"], "at most 2 shadowed lights")
    check(not any(c.get_editor_property("light_function_material") for c in lights), "no light function (a calm cabin)")
    log("validation passed")


def main():
    for spec in CONTRACTS:
        ensure_contract(spec)
    terminal_class = ensure_terminal()
    ensure_map(terminal_class)
    validate(terminal_class)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_hub_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
