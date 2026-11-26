"""UI-02 (ADR-0052): the live scene behind the main menu, in L_Boot (idempotent, ADR-0010).

A dark Docks airlock bay from the art we have: the hull's materials on the greybox shell (MI_Hull_*), the
OvergrownFactoryENV props (FAB-01: pipes, tank, panels, darkened ivy; looks only, no collision), the volumetric height
fog, three lights in budget (the core's cyan glow, a violet shaft through the inner door that breathes slowly with a
light function, a cold fill from the ceiling) and an Aether core hovering on the right of the frame: its material turns
and bobs it in the shader (no tick), UHollowFxPlayerComponent plays the lean FAB-01 aura on it (NS_Fab_CorePulse, its
dimmer variant under Reduced Flashes). A camera that auto-activates for player 0 frames it all: the engine views
through it rather than the front end's spectator. A room tone fills it.

  /Game/Hollow/World/Menu/M_LF_MenuBreath        light function: the shaft's slow breath (a period of some 12 s)
  /Game/Hollow/World/Menu/M_Menu_CoreHover       the core's surface: colour and glow, turned and bobbed by world position offset
  /Game/Hollow/World/Menu/MI_Menu_Core_Crystal|Shell   its crystal and its container
  /Game/Hollow/Maps/L_Boot                       the scene (tagged HollowMenuScene, in the Menu folders)

The level keeps the hash of its spec on a marker (a Note): it is rebuilt and saved only when the spec changes.
Run after create_ui_assets.py (L_Boot's game mode), the FAB-01 scripts (prepare_fab_assets.py, create_fab_dressing.py,
create_fab_fx.py), create_art_kit.py and create_audio_assets.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_menu_scene.py
"""
import hashlib
import json
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

VERSION = 1
SPEC_TAG = "HollowSpec"
SCENE_TAG = "HollowMenuScene"
MARKER_LABEL = "HollowMenuSceneSpec"
BOOT_MAP = "/Game/Hollow/Maps/L_Boot"
MENU_DIR = "/Game/Hollow/World/Menu"
LIGHT_FUNCTION = MENU_DIR + "/M_LF_MenuBreath"
CORE_MATERIAL = MENU_DIR + "/M_Menu_CoreHover"
CORE_CRYSTAL = MENU_DIR + "/MI_Menu_Core_Crystal"
CORE_SHELL = MENU_DIR + "/MI_Menu_Core_Shell"
CUBE = "/Engine/BasicShapes/Cube.Cube"   # 100 uu a side
HULL = "/Game/Hollow/World/Materials"
FAB = "/Game/Fab/OvergrownFactoryENV/Meshes"
CORE_MESH = "/Game/Hollow/Art/Kit/SM_AetherCore"
AURA = ("/Game/Hollow/FX/Fab/NS_Fab_CorePulse", "/Game/Hollow/FX/Fab/NS_Fab_CorePulse_Reduced")
ROOM_TONE = "/Game/Hollow/Audio/MetaSounds/MS_Room_Tone"
DARK_MATERIALS = {"MI_IVY_Leafs_01": "MI_Fab_Ivy_Dark", "MI_PlantStem_01": "MI_Fab_PlantStem_Dark", "MI_SpanishMoss_01": "MI_Fab_Moss_Dark"}
MAX_LIGHTS = 3   # the scene's own lights (Hollow.Game.MenuScene)

# ---------------------------------------------------------------- the shell (cm; x away from the camera, y right, z up)
# Cubes of the hull: (label, material, centre, size).
SHELL = [
    ("Floor", "MI_Hull_Floor", (250.0, 0.0, -10.0), (2200.0, 1300.0, 20.0)),
    ("Ceiling", "MI_Hull_Ceiling", (250.0, 0.0, 660.0), (2200.0, 1300.0, 20.0)),
    ("WallLeft", "MI_Hull_Wall", (0.0, -640.0, 325.0), (1700.0, 20.0, 650.0)),
    ("WallRight", "MI_Hull_Wall", (0.0, 640.0, 325.0), (1700.0, 20.0, 650.0)),
    ("BackLeft", "MI_Hull_Wall", (850.0, -425.0, 325.0), (30.0, 430.0, 650.0)),
    ("BackRight", "MI_Hull_Wall", (850.0, 425.0, 325.0), (30.0, 430.0, 650.0)),
    ("BackLintel", "MI_Hull_Wall", (850.0, 0.0, 530.0), (30.0, 420.0, 240.0)),
    ("Threshold", "MI_Hazard", (850.0, 0.0, 4.0), (60.0, 420.0, 8.0)),
    ("InnerLeft", "MI_Hull_Wall", (1150.0, -260.0, 325.0), (600.0, 20.0, 650.0)),
    ("InnerRight", "MI_Hull_Wall", (1150.0, 260.0, 325.0), (600.0, 20.0, 650.0)),
    ("InnerEnd", "MI_Hull_Wall", (1450.0, 0.0, 325.0), (20.0, 540.0, 650.0)),
    ("CoolStripLeft", "MI_Strip_Cool", (845.0, -212.0, 210.0), (6.0, 6.0, 380.0)),
    ("CoolStripRight", "MI_Strip_Cool", (845.0, 212.0, 210.0), (6.0, 6.0, 380.0)),
]
# Props of OvergrownFactoryENV: (label, mesh under FAB, (x, y, z or None: on the floor), yaw, scale).
PROPS = [
    ("Tank", "Props/SM_WaterTank", (420.0, -470.0, None), 90.0, 1.0),
    ("Barrel", "Props/SM_Barrel_02", (120.0, -520.0, None), 20.0, 1.0),
    ("CableDrum", "Props/SM_WoodenCableDrum_01", (-140.0, -500.0, None), 60.0, 1.0),
    ("Pump", "Props/SM_Industrial_PumpMotorSkid_01", (640.0, 480.0, None), -90.0, 1.0),
    ("Panel", "Props/SM_ElectricalPanel_01", (160.0, 625.0, 150.0), -90.0, 1.0),
    ("PanelBox", "Props/SM_ElectricalPanel_01_Box", (330.0, 625.0, 130.0), -90.0, 1.0),
    ("Junction", "Props/SM_ElectricBox_02", (-120.0, 625.0, 160.0), -90.0, 1.0),
    ("VentLeft", "Props/SM_Air_Vents_02", (830.0, -420.0, 430.0), 180.0, 1.0),
    ("VentRight", "Props/SM_ACVent_01", (830.0, 420.0, 470.0), 180.0, 1.0),
    ("PipeLow", "Props/SM_SteelPipe_01", (200.0, -615.0, 470.0), 0.0, 1.0),
    ("PipeHigh", "Props/SM_SteelPipe_02", (200.0, -600.0, 560.0), 0.0, 1.0),
    ("BeamNear", "Architecture/SM_SteelBeam_01", (250.0, 0.0, 630.0), 90.0, 1.0),
    ("BeamFar", "Architecture/SM_SteelBeam_01", (650.0, 0.0, 630.0), 90.0, 1.0),
    ("Tube", "Props/SM_FluorescentLight_01", (650.0, 0.0, 600.0), 0.0, 1.0),
    ("Wire", "Props/SM_ElectricWire_01", (420.0, 300.0, 640.0), 0.0, 1.0),
    ("IvyLeft", "Foliage/SM_Ivy_01", (830.0, -300.0, 600.0), 180.0, 1.0),
    ("IvyRight", "Foliage/SM_Ivy_01", (830.0, 300.0, 620.0), 200.0, 1.0),
    ("IvyWall", "Foliage/SM_IvyDecal_01", (600.0, -625.0, 380.0), 90.0, 1.0),
]
# The hovering core, right of the frame (the menu's column is on the left).
CORE = {"location": (180.0, 240.0, 165.0), "yaw": 25.0, "scale": 2.2, "aura_scale": 0.3}
CORE_LOOK = {"bob_cm": 4.0, "bob_hz": 0.18, "spin_turns_per_s": 0.025,
             "crystal": {"Color": (0.30, 0.85, 0.95, 1.0), "Glow": 10.0, "Metallic": 0.0, "Roughness": 0.15},
             "shell": {"Color": (0.05, 0.06, 0.07, 1.0), "Glow": 0.0, "Metallic": 0.85, "Roughness": 0.35}}
BREATH = {"low": 0.65, "high": 1.0, "period_s": 12.0}
CAMERA = {"location": (-700.0, -40.0, 175.0), "rotation": (0.0, -3.0, 2.0), "fov": 68.0}   # (roll, pitch, yaw)
# Lights (lumens): the core's glow, the shaft through the inner door (shadowed, volumetric, breathing), a cold fill.
LIGHTS = [
    {"label": "CoreGlow", "kind": "point", "location": (180.0, 240.0, 180.0), "intensity": 260.0, "radius": 800.0, "color": (90, 215, 245),
     "shadowed": False, "scattering": 3.0},
    {"label": "Shaft", "kind": "spot", "location": (1300.0, 0.0, 620.0), "rotation": (0.0, -38.0, 180.0), "intensity": 1100.0, "radius": 2400.0,
     "color": (170, 120, 255), "shadowed": True, "volumetric_shadow": True, "scattering": 3.0, "inner": 14.0, "outer": 30.0,
     "light_function": True},
    {"label": "Fill", "kind": "rect", "location": (0.0, -300.0, 640.0), "rotation": (0.0, -90.0, 0.0), "intensity": 90.0, "radius": 1200.0,
     "color": (150, 190, 230), "shadowed": False, "scattering": 0.0, "width": 300.0, "height": 40.0},
]
FOG = {"fog_density": 0.035, "fog_height_falloff": 0.35, "enable_volumetric_fog": True, "volumetric_fog_scattering_distribution": 0.55,
       "volumetric_fog_extinction_scale": 1.2, "volumetric_fog_distance": 2500.0}
FOG_COLORS = {"fog_inscattering_luminance": (0.002, 0.003, 0.005, 1.0), "volumetric_fog_albedo": (120, 140, 160)}
# A dark bay: the exposure may not lift it to daylight (EV100 1..2), as the hub keeps its dim look.
POST = {"auto_exposure_method": "AEM_HISTOGRAM", "auto_exposure_min_brightness": 1.0, "auto_exposure_max_brightness": 2.0,
        "auto_exposure_bias": 0.0, "bloom_intensity": 0.5, "vignette_intensity": 0.55, "film_grain_intensity": 0.12}
TONE_VOLUME = 0.3

assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary


def log(message):
    unreal.log("create_menu_scene: " + message)


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset " + path)
    return asset


def save(asset):
    if not assets.save_loaded_asset(asset, False):
        raise RuntimeError("Could not save " + asset.get_path_name())


def digest(*parts):
    return hashlib.sha1(json.dumps(parts, sort_keys=True, default=str).encode("ascii")).hexdigest()


def vector(xyz):
    return unreal.Vector(*xyz)


def rotator(rpy):
    return unreal.Rotator(roll=rpy[0], pitch=rpy[1], yaw=rpy[2])


def colour(rgb):
    return unreal.Color(r=rgb[0], g=rgb[1], b=rgb[2], a=255)


# ---------------------------------------------------------------- materials


class Graph(object):
    """A few expressions of a material, wired by input name ("" for an expression's only input)."""

    def __init__(self, material):
        self.material = material
        self.count = 0

    def node(self, expression_class, **properties):
        self.count += 1
        expression = mel.create_material_expression(self.material, expression_class, -300 * (1 + self.count % 4), 120 * self.count)
        for name, value in properties.items():
            expression.set_editor_property(name, value)
        return expression

    def wire(self, source, target, pin=""):
        if not mel.connect_material_expressions(source, "", target, pin):
            raise RuntimeError("Could not wire %s into %s.%s" % (source.get_name(), target.get_name(), pin))
        return target

    def op(self, expression_class, a, b=None, const_b=None):
        expression = self.node(expression_class, **({"const_b": const_b} if const_b is not None else {}))
        self.wire(a, expression, "A")
        if b is not None:
            self.wire(b, expression, "B")
        return expression

    def output(self, source, prop):
        if not mel.connect_material_property(source, "", prop):
            raise RuntimeError("Could not connect %s of %s" % (prop, self.material.get_name()))


def rebuild_material(path, spec, build):
    wanted = digest(VERSION, spec)
    if assets.does_asset_exist(path):
        material = load(path)
        if assets.get_metadata_tag(material, SPEC_TAG) == wanted:
            return material
        mel.delete_all_material_expressions(material)
        while mel.get_num_material_expressions(material):   # it removes about half a graph a call (KnownIssues AK-1)
            mel.delete_all_material_expressions(material)
    else:
        package_path, name = path.rsplit("/", 1)
        material = tools.create_asset(name, package_path, unreal.Material, unreal.MaterialFactoryNew())
    build(material, Graph(material))
    mel.recompile_material(material)
    assets.set_metadata_tag(material, SPEC_TAG, wanted)
    save(material)
    log("built " + path.rsplit("/", 1)[1])
    return material


def build_breath(material, g):
    """Emissive = low + (high - low) * (sin(time) + 1) / 2: a slow breath of the light, never a flash."""
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_LIGHT_FUNCTION)
    wave = g.node(unreal.MaterialExpressionSine, period=BREATH["period_s"])
    g.wire(g.node(unreal.MaterialExpressionTime), wave)
    half = (BREATH["high"] - BREATH["low"]) / 2.0
    level = g.op(unreal.MaterialExpressionAdd, g.op(unreal.MaterialExpressionMultiply, wave, const_b=half), const_b=BREATH["low"] + half)
    g.output(level, unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def build_core(material, g):
    """Colour, metal, roughness and glow by parameter; turned about its pivot and bobbed by world position offset."""
    color = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="Color", default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    glow = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="Glow", default_value=0.0)
    g.output(color, unreal.MaterialProperty.MP_BASE_COLOR)
    g.output(g.op(unreal.MaterialExpressionMultiply, color, glow), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.output(g.node(unreal.MaterialExpressionScalarParameter, parameter_name="Metallic", default_value=0.0), unreal.MaterialProperty.MP_METALLIC)
    g.output(g.node(unreal.MaterialExpressionScalarParameter, parameter_name="Roughness", default_value=0.5), unreal.MaterialProperty.MP_ROUGHNESS)
    time = g.node(unreal.MaterialExpressionTime)
    spin = g.node(unreal.MaterialExpressionRotateAboutAxis, period=1.0)
    g.wire(g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.0, 0.0, 1.0, 0.0)), spin, "NormalizedRotationAxis")
    g.wire(g.op(unreal.MaterialExpressionMultiply, time, const_b=CORE_LOOK["spin_turns_per_s"]), spin, "RotationAngle")
    g.wire(g.node(unreal.MaterialExpressionObjectPositionWS), spin, "PivotPoint")
    g.wire(g.node(unreal.MaterialExpressionWorldPosition), spin, "Position")
    bob = g.node(unreal.MaterialExpressionSine, period=1.0 / CORE_LOOK["bob_hz"])
    g.wire(time, bob)
    lift = g.op(unreal.MaterialExpressionMultiply, g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.0, 0.0, 1.0, 0.0)),
                g.op(unreal.MaterialExpressionMultiply, bob, const_b=CORE_LOOK["bob_cm"]))
    g.output(g.op(unreal.MaterialExpressionAdd, spin, lift), unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)


def ensure_instance(path, parent, values):
    wanted = digest(VERSION, values)
    if assets.does_asset_exist(path):
        instance = load(path)
        if assets.get_metadata_tag(instance, SPEC_TAG) == wanted:
            return instance
    else:
        package_path, name = path.rsplit("/", 1)
        instance = tools.create_asset(name, package_path, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, parent)
    for name, value in sorted(values.items()):
        if isinstance(value, tuple):
            mel.set_material_instance_vector_parameter_value(instance, name, unreal.LinearColor(*value))
        else:
            mel.set_material_instance_scalar_parameter_value(instance, name, value)
    assets.set_metadata_tag(instance, SPEC_TAG, wanted)
    save(instance)
    log("built " + path.rsplit("/", 1)[1])
    return instance


def ensure_materials():
    breath = rebuild_material(LIGHT_FUNCTION, BREATH, build_breath)
    core = rebuild_material(CORE_MATERIAL, {k: v for k, v in CORE_LOOK.items() if k not in ("crystal", "shell")}, build_core)
    return breath, ensure_instance(CORE_CRYSTAL, core, CORE_LOOK["crystal"]), ensure_instance(CORE_SHELL, core, CORE_LOOK["shell"])


# ---------------------------------------------------------------- the scene


def tags_of(actor):
    return [str(tag) for tag in actor.get_editor_property("tags")]


def managed(actor, label, folder, extra=()):
    actor.set_actor_label(label)
    actor.set_folder_path("Menu/" + folder)
    actor.set_editor_property("tags", [SCENE_TAG] + list(extra))
    return actor


def looks_only(component):
    component.set_collision_profile_name("NoCollision")
    component.set_editor_property("can_ever_affect_navigation", False)


def mesh_actor(label, folder, mesh, location, rotation, scale):
    actor = managed(actors.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation), label, folder)
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)   # before the profile: a mesh set after it brings its own collision back (AK-15)
    actor.set_actor_scale3d(scale)
    looks_only(component)
    return actor, component


def spawn_shell():
    cube = load(CUBE)
    for label, material, centre, size in SHELL:
        _, component = mesh_actor(label, "Shell", cube, vector(centre), unreal.Rotator(0.0, 0.0, 0.0), vector([s / 100.0 for s in size]))
        component.set_material(0, load(HULL + "/" + material))
        component.set_editor_property("cast_shadow", "Strip" not in label)   # the glowing strips light, they cast nothing


def spawn_props():
    dark = {name: load(HULL + "/Fab/" + mi) for name, mi in DARK_MATERIALS.items()}
    for label, mesh_path, (x, y, z), yaw, scale in PROPS:
        mesh = load(FAB + "/" + mesh_path)
        if z is None:   # stood on the floor: its lowest point at the floor's top
            z = -mesh.get_bounding_box().min.z * scale
        _, component = mesh_actor(label, "Dressing", mesh, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(scale, scale, scale))
        for index, material in enumerate(component.get_materials()):
            if material is not None and material.get_name() in dark:
                component.set_material(index, dark[material.get_name()])


def spawn_core(crystal, shell):
    mesh = load(CORE_MESH)
    s = CORE["scale"]
    actor, component = mesh_actor("AetherCore", "Core", mesh, vector(CORE["location"]), unreal.Rotator(0.0, 0.0, CORE["yaw"]), unreal.Vector(s, s, s))
    for index in range(len(component.get_materials())):
        component.set_material(index, crystal if index == 0 else shell)   # slot 0: the crystal (the item's look, Art-01)
    component.set_editor_property("cast_shadow", False)
    player = unreal.HollowAssetAuthoring.add_instance_component(actor, unreal.HollowFxPlayerComponent, "Aura")
    if player is None:
        raise RuntimeError("Could not add the core's aura")
    effect = unreal.HollowFxSpec()
    effect.set_editor_property("system", load(AURA[0]))
    effect.set_editor_property("reduced_flashes_system", load(AURA[1]))
    effect.set_editor_property("scale", CORE["aura_scale"] / s)   # the component is scaled with the core
    player.set_editor_property("effect", effect)


def spawn_light(spec, breath):
    location, rotation = vector(spec["location"]), rotator(spec.get("rotation", (0.0, 0.0, 0.0)))
    if spec["kind"] == "spot":
        actor = actors.spawn_actor_from_class(unreal.SpotLight, location, rotation)
        component = actor.spot_light_component
        component.set_editor_property("inner_cone_angle", spec["inner"])
        component.set_editor_property("outer_cone_angle", spec["outer"])
    elif spec["kind"] == "rect":
        actor = actors.spawn_actor_from_class(unreal.RectLight, location, rotation)
        component = actor.rect_light_component
        component.set_editor_property("source_width", spec["width"])
        component.set_editor_property("source_height", spec["height"])
    else:
        actor = actors.spawn_actor_from_class(unreal.PointLight, location, rotation)
        component = actor.point_light_component
    managed(actor, spec["label"], "Lights")
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
    component.set_editor_property("intensity", spec["intensity"])
    component.set_editor_property("light_color", colour(spec["color"]))
    component.set_editor_property("attenuation_radius", spec["radius"])
    component.set_editor_property("cast_shadows", spec["shadowed"])
    component.set_editor_property("cast_volumetric_shadow", spec.get("volumetric_shadow", False))
    component.set_editor_property("volumetric_scattering_intensity", spec["scattering"])
    if spec.get("light_function"):
        component.set_editor_property("light_function_material", breath)


def spawn_atmosphere():
    zero = unreal.Rotator(0.0, 0.0, 0.0)
    fog = managed(actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0.0, 0.0, 0.0), zero), "Fog", "Atmosphere")
    component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    for name, value in FOG.items():
        component.set_editor_property(name, value)
    component.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(*FOG_COLORS["fog_inscattering_luminance"]))
    component.set_editor_property("volumetric_fog_albedo", colour(FOG_COLORS["volumetric_fog_albedo"]))
    post = managed(actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0.0, 0.0, 0.0), zero), "Post", "Atmosphere")
    post.set_editor_property("unbound", True)
    settings = post.get_editor_property("settings")
    for name, value in POST.items():
        settings.set_editor_property("override_" + name, True)
        settings.set_editor_property(name, getattr(unreal.AutoExposureMethod, value) if name == "auto_exposure_method" else value)
    post.set_editor_property("settings", settings)
    tone = managed(actors.spawn_actor_from_class(unreal.AmbientSound, vector(CAMERA["location"]), zero), "RoomTone", "Atmosphere")
    audio = tone.get_editor_property("audio_component")
    audio.set_editor_property("sound", load(ROOM_TONE))
    audio.set_editor_property("volume_multiplier", TONE_VOLUME)
    audio.set_editor_property("allow_spatialization", False)   # all round the listener


def spawn_camera():
    camera = managed(actors.spawn_actor_from_class(unreal.CameraActor, vector(CAMERA["location"]), rotator(CAMERA["rotation"])), "MenuCamera", "Camera")
    camera.set_editor_property("auto_activate_for_player", unreal.AutoReceiveInput.PLAYER0)
    component = camera.camera_component
    component.set_editor_property("field_of_view", CAMERA["fov"])
    component.set_editor_property("constrain_aspect_ratio", False)


def scene_spec():
    return [VERSION, SHELL, PROPS, CORE, CORE_LOOK, BREATH, CAMERA, LIGHTS, FOG, FOG_COLORS, POST, TONE_VOLUME, DARK_MATERIALS, AURA]


def ensure_scene(breath, crystal, shell):
    wanted = digest(scene_spec())
    if not levels.load_level(BOOT_MAP):
        raise RuntimeError("Could not load " + BOOT_MAP)
    level_actors = actors.get_all_level_actors()
    marker = next((a for a in level_actors if a.get_actor_label() == MARKER_LABEL), None)
    if marker is not None and wanted in tags_of(marker):
        return False
    for actor in level_actors:
        if SCENE_TAG in tags_of(actor) or actor.get_actor_label() == MARKER_LABEL:
            actors.destroy_actor(actor)
    spawn_shell()
    spawn_props()
    spawn_core(crystal, shell)
    for spec in LIGHTS:
        spawn_light(spec, breath)
    spawn_atmosphere()
    spawn_camera()
    managed(actors.spawn_actor_from_class(unreal.Note, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0)), MARKER_LABEL, "Atmosphere", [wanted])
    if not levels.save_current_level():
        raise RuntimeError("Could not save " + BOOT_MAP)
    log("L_Boot: the menu scene (%d hull pieces, %d props, %d lights)" % (len(SHELL), len(PROPS), len(LIGHTS)))
    return True


def validate():
    check(load(LIGHT_FUNCTION).get_editor_property("material_domain") == unreal.MaterialDomain.MD_LIGHT_FUNCTION, "M_LF_MenuBreath is a light function")
    if not levels.load_level(BOOT_MAP):
        raise RuntimeError("Could not load " + BOOT_MAP)
    scene = [a for a in actors.get_all_level_actors() if SCENE_TAG in tags_of(a)]
    lights = [a for a in scene if isinstance(a, unreal.Light)]
    check(len(lights) <= MAX_LIGHTS, "the scene has at most %d lights (%d)" % (MAX_LIGHTS, len(lights)))
    cameras = [a for a in scene if isinstance(a, unreal.CameraActor)]
    check(len(cameras) == 1 and cameras[0].get_editor_property("auto_activate_for_player") == unreal.AutoReceiveInput.PLAYER0, "one camera for player 0")
    for actor in scene:
        if isinstance(actor, unreal.StaticMeshActor):
            component = actor.static_mesh_component
            check(str(component.get_collision_profile_name()) == "NoCollision", actor.get_actor_label() + " has no collision")
    core = next((a for a in scene if a.get_actor_label() == "AetherCore"), None)
    aura = core.get_component_by_class(unreal.HollowFxPlayerComponent) if core else None
    check(aura is not None and aura.get_editor_property("effect").get_editor_property("reduced_flashes_system") is not None,
          "the core's aura, with its Reduced Flashes variant")
    game_mode = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_world_settings().get_editor_property("default_game_mode")
    check(game_mode is not None and "BP_GameMode_FrontEnd" in game_mode.get_name(), "L_Boot keeps the front end's game mode")
    log("validation passed")


def main():
    breath, crystal, shell = ensure_materials()
    ensure_scene(breath, crystal, shell)
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_menu_scene failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
