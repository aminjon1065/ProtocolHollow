"""Creates the S4-03 lighting and atmosphere (idempotent, ADR-0010, ADR-0021). Nothing heavy: engine shapes and
node-only materials (no project textures, no packs), so the LFS footprint stays in kilobytes.

  /Game/Hollow/World/Lighting/DA_LightBudget       UHollowLightBudgetConfig: limits per room type and per view
  /Game/Hollow/World/Lighting/M_LF_Emergency       light function: steady at Low, flicker from Medium, strobe at Critical
  /Game/Hollow/World/Lighting/M_LF_DyingTube       light function: brown-outs from High (the hero room's failing panel)
  /Game/Hollow/World/Materials/M_Hollow_Surface    hull master: world-space panel seams, grime noise, hazard stripes
  /Game/Hollow/World/Materials/M_Hollow_Emissive   strips and screens; optionally flickers with the emergency lights
  /Game/Hollow/World/Materials/MI_*                instances (MATERIAL_INSTANCES)
  Every Docks room level                           a light rig in budget (practical panels, emergency light, fixtures)
  LV_Docks_CoreVault (hero room)                   hull materials, dressing from engine shapes, local fog
  /Game/Hollow/Maps/L_Raid_Dev                     volumetric height fog, unbound post-process (exposure, Lumen quality)

Flicker comes from MPC_Resonance (S4-02): Intensity and Time + FlickerTimeOffset, so it is in phase on every machine.
With the player's reduced flashes (MPC_Resonance ReducedFlashes, H-1, ADR-0029) the drop-outs and brown-outs hold a
steady dim and the Critical strobe becomes a slow smooth pulse (REDUCED: nothing over 2 Hz). REDUCED is part of the
material hashes only, so changing it rebuilds those materials, never a room level.
Material graphs and level contents are built from the specs below; a hash of each spec is kept (asset metadata,
or a marker actor in a level), so a level is re-saved only when its spec changes - every save is a new LFS object.
A room level art-locked on its definition (bArtLocked, ADR-0027: finished by hand) is never touched; the tests still check it.
Run after create_resonance_assets.py:  Build/Scripts/run-editor-python.sh Build/Python/create_lighting_assets.py
"""
import hashlib
import json
import math
import re
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

LIGHT_DIR = "/Game/Hollow/World/Lighting"
MATERIAL_DIR = "/Game/Hollow/World/Materials"
BUDGET = LIGHT_DIR + "/DA_LightBudget"
LF_EMERGENCY = LIGHT_DIR + "/M_LF_Emergency"
LF_DYING = LIGHT_DIR + "/M_LF_DyingTube"
SURFACE = MATERIAL_DIR + "/M_Hollow_Surface"
EMISSIVE = MATERIAL_DIR + "/M_Hollow_Emissive"
COLLECTION = "/Game/Hollow/FX/Resonance/MPC_Resonance"
ROOM_DATA_DIR = "/HollowSector_Docks/Data"
HERO_ROOM = "DA_Room_Docks_CoreVault"
RAID_MAP = "/Game/Hollow/Maps/L_Raid_Dev"
CUBE = "/Engine/BasicShapes/Cube.Cube"
CYLINDER = "/Engine/BasicShapes/Cylinder.Cylinder"
SHAPE_SIZE = 100.0            # engine basic shapes are 100 uu
SPEC_TAG = "HollowSpec"
# Bumped when the way rigs are built changes (2: fixtures and dressing without collision through their profile).
RIG_VERSION = 2
CLEAR_ATTEMPTS = 16
MANAGED_TAG = "HollowLighting"
MARKER_LABEL = "HollowLightingSpec"
LEGACY_LIGHT = "Light"        # the single greybox light of create_docks_greybox.py before S4-03
INTENSITY = "Intensity"
TIME_OFFSET = "FlickerTimeOffset"
REDUCED_FLASHES = "ReducedFlashes"

# ---------------------------------------------------------------- budget (data, S4-03 clarification 4)

BIG_ROOM = {"max_lights": 6, "max_shadowed": 2, "max_shadowed_flickering": 1, "max_light_functions": 3, "max_volumetric_shadows": 1, "pass_through": False}
PASS_THROUGH = {"max_lights": 3, "max_shadowed": 0, "max_shadowed_flickering": 0, "max_light_functions": 2, "max_volumetric_shadows": 0, "pass_through": True}
# The hub shuttle (S5-03 clarification 4): calm, no flicker; not a sector room (create_hub_assets.py lights it).
HUB = {"max_lights": 8, "max_shadowed": 2, "max_shadowed_flickering": 0, "max_light_functions": 0, "max_volumetric_shadows": 1, "pass_through": False}
BUDGET_ROOMS = {"Spawn": BIG_ROOM, "Extraction": BIG_ROOM, "CoreVault": BIG_ROOM, "Hangar": BIG_ROOM, "Storage": BIG_ROOM,
                "Corridor": PASS_THROUGH, "Junction": PASS_THROUGH, "Hub": HUB}
MAX_SHADOWED_IN_VIEW = 4      # Block 3 sec. 3.9 (RTX 3060)

# ---------------------------------------------------------------- materials (data)

FLICKER = {"version": 1, "wave_a_hz": 5.3, "wave_b_hz": 1.7, "dropout_gain": 3.0, "dropout_bias": 0.6, "flicker_from": 0.25, "flicker_span": 0.5,
           "strobe_from": 0.75, "strobe_span": 0.25, "strobe_hz": 2.0}
DYING = {"version": 1, "from": 0.5, "span": 0.5, "slow_hz": 0.43, "fast_hz": 9.1, "dip_gain": 2.0, "depth": 0.75}
# Reduced flashes (H-1, ADR-0029): drop-outs and brown-outs become a steady dim of this share of their depth, the
# Critical strobe a sine of strobe_hz dimming down to 1 - strobe_depth.
REDUCED = {"version": 1, "dropout_level": 0.8, "brownout_share": 0.5, "strobe_hz": 0.5, "strobe_depth": 0.5}
MAX_REDUCED_HZ = 2.0
SURFACE_SPEC = {"version": 2, "grime_floor": 0.6, "grime_gain": 3.0, "grime_roughness": 0.5, "grime_darken": 0.6,
                # Art-01 (ADR-0043): CC0 textures behind the static switch UseTextures, projected along the face's dominant
                # world axis (the greybox boxes have no useful UVs); the axis picks with this sharpness.
                "axis_sharpness": 1000.0, "ao_strength": 0.6}
TEXTURE_DIR = "/Game/Hollow/Art/Textures"   # create_art_kit.py (CC0, Build/Art/cc0_textures.json)
SURFACE_TEXTURES = {"BaseColorMap": ("T_RustyMetal_D", "SAMPLERTYPE_COLOR"), "NormalMap": ("T_RustyMetal_N", "SAMPLERTYPE_NORMAL"),
                    "ARMMap": ("T_RustyMetal_ARM", "SAMPLERTYPE_MASKS")}
SURFACE_DEFAULTS = {"scalars": {"Metallic": 0.7, "Roughness": 0.45, "PanelSize": 200.0, "SeamWidth": 1.5, "SeamDarken": 0.6, "GrimeScale": 150.0,
                                "GrimeAmount": 0.35, "StripeWidth": 30.0, "StripeAmount": 0.0, "TextureSize": 200.0, "NormalStrength": 0.8},
                    "vectors": {"BaseColor": (0.18, 0.19, 0.2), "StripeColor": (0.02, 0.02, 0.02), "TextureTint": (1.0, 1.0, 1.0)}}
EMISSIVE_DEFAULTS = {"scalars": {"Strength": 5.0, "FollowResonance": 0.0}, "vectors": {"Color": (1.0, 1.0, 1.0)}}
# Art-01: the hull wears CC0 textures (UseTextures); its tint keeps the dark Docks palette.
HULL_WALL_TEXTURES = {"BaseColorMap": "T_RustyMetal_D", "NormalMap": "T_RustyMetal_N", "ARMMap": "T_RustyMetal_ARM"}
HULL_FLOOR_TEXTURES = {"BaseColorMap": "T_MetalPlate_D", "NormalMap": "T_MetalPlate_N", "ARMMap": "T_MetalPlate_ARM"}
HULL_CEILING_TEXTURES = {"BaseColorMap": "T_CorrugatedIron_D", "NormalMap": "T_CorrugatedIron_N", "ARMMap": "T_CorrugatedIron_ARM"}
MATERIAL_INSTANCES = {
    "MI_Hull_Wall": (SURFACE, {"scalars": {"Metallic": 0.6, "Roughness": 0.5, "TextureSize": 250.0, "GrimeAmount": 0.25},
                               "vectors": {"BaseColor": (0.2, 0.21, 0.22), "TextureTint": (0.3, 0.33, 0.37)},
                               "textures": HULL_WALL_TEXTURES, "switches": {"UseTextures": True}}),
    "MI_Hull_Floor": (SURFACE, {"scalars": {"Metallic": 0.8, "Roughness": 0.65, "PanelSize": 100.0, "GrimeAmount": 0.4, "TextureSize": 150.0},
                                "vectors": {"BaseColor": (0.08, 0.085, 0.09), "TextureTint": (0.5, 0.52, 0.55)},
                                "textures": HULL_FLOOR_TEXTURES, "switches": {"UseTextures": True}}),
    "MI_Hull_Ceiling": (SURFACE, {"scalars": {"Metallic": 0.5, "Roughness": 0.7, "TextureSize": 200.0},
                                  "vectors": {"BaseColor": (0.12, 0.12, 0.13), "TextureTint": (0.35, 0.37, 0.4)},
                                  "textures": HULL_CEILING_TEXTURES, "switches": {"UseTextures": True}}),
    "MI_Pipe": (SURFACE, {"scalars": {"Metallic": 0.9, "Roughness": 0.35, "PanelSize": 1000.0, "GrimeAmount": 0.25},
                          "vectors": {"BaseColor": (0.35, 0.3, 0.24)}}),
    "MI_Hazard": (SURFACE, {"scalars": {"Metallic": 0.2, "Roughness": 0.6, "StripeAmount": 1.0, "StripeWidth": 25.0},
                            "vectors": {"BaseColor": (0.75, 0.55, 0.05)}}),
    "MI_Vault_Trim": (SURFACE, {"scalars": {"Metallic": 0.9, "Roughness": 0.3, "PanelSize": 50.0}, "vectors": {"BaseColor": (0.05, 0.06, 0.08)}}),
    "MI_Strip_Cool": (EMISSIVE, {"scalars": {"Strength": 8.0}, "vectors": {"Color": (0.78, 0.88, 1.0)}}),
    "MI_Strip_Emergency": (EMISSIVE, {"scalars": {"Strength": 6.0, "FollowResonance": 1.0}, "vectors": {"Color": (1.0, 0.18, 0.08)}}),
    "MI_Screen": (EMISSIVE, {"scalars": {"Strength": 3.0}, "vectors": {"Color": (0.2, 0.9, 0.7)}}),
}

# ---------------------------------------------------------------- lights (data, S4-03 clarifications 3-5)

PRACTICAL = {"intensity": 3500.0, "source_width": 120.0, "source_height": 30.0, "barn_door_angle": 70.0, "color": (200, 225, 255),
             "drop": 12.0, "volumetric_scattering": 1.0, "reach": 0.9}
PRACTICAL_PASS_THROUGH = {"intensity": 2000.0}
EMERGENCY = {"intensity": 900.0, "color": (255, 45, 25), "radius": 600.0, "height": 300.0, "inset": 60.0, "volumetric_scattering": 3.0}
HERO_FILL = {"intensity": 250.0, "color": (90, 200, 255), "radius": 400.0, "height": 60.0, "volumetric_scattering": 1.0}
FIXTURE_PLATE = (130.0, 36.0, 3.0)       # emissive panel above a practical light
EMERGENCY_HOUSING = (24.0, 12.0, 12.0)   # emissive box on the wall at an emergency light
HERO_PRACTICALS = [(400.0, 250.0), (400.0, 550.0)]   # between the ceiling beams; the second one is the dying tube
HERO_EMERGENCY_WALLS = ["WEST", "EAST"]

# Hero room dressing (CoreVault, 800 x 800 x 400; inner wall faces at 20 and 760; doorways S y 50..350, N y 450..750).
# Boxes: (label, material, (x0, y0, z0), (x1, y1, z1)); cylinders: (label, material, start, end, radius).
HERO_BOXES = [
    ("Rib_W_%d" % i, "MI_Vault_Trim", (x - 12, 20, 0), (x + 12, 44, 400)) for i, x in enumerate((100, 300, 500, 700))
] + [
    ("Rib_E_%d" % i, "MI_Vault_Trim", (x - 12, 736, 0), (x + 12, 760, 400)) for i, x in enumerate((100, 300, 500, 700))
] + [
    ("Rib_S_%d" % i, "MI_Vault_Trim", (20, y - 12, 0), (44, y + 12, 400)) for i, y in enumerate((450, 650))
] + [
    ("Rib_N_%d" % i, "MI_Vault_Trim", (736, y - 12, 0), (760, y + 12, 400)) for i, y in enumerate((150, 350))
] + [
    ("Beam_%d" % i, "MI_Vault_Trim", (x - 15, 20, 364), (x + 15, 760, 400)) for i, x in enumerate((200, 600))
] + [
    ("CoreFrame_S", "MI_Hazard", (260, 260, 0), (540, 280, 4)), ("CoreFrame_N", "MI_Hazard", (260, 520, 0), (540, 540, 4)),
    ("CoreFrame_W", "MI_Hazard", (260, 280, 0), (280, 520, 4)), ("CoreFrame_E", "MI_Hazard", (520, 280, 0), (540, 520, 4)),
    ("CorePlate", "MI_Vault_Trim", (280, 280, 0.5), (520, 520, 2.5)),
    ("Console_A", "MI_Hull_Wall", (180, 715, 0), (280, 755, 95)), ("Console_B", "MI_Hull_Wall", (520, 715, 0), (620, 755, 95)),
    ("Screen_A", "MI_Screen", (185, 711, 100), (275, 715, 155)), ("Screen_B", "MI_Screen", (525, 711, 100), (615, 715, 155)),
    ("VentGrille", "MI_Hull_Floor", (355, 754, 5), (445, 758, 95)),
    ("FloorStrip_W", "MI_Strip_Emergency", (50, 22, 0), (750, 26, 2)), ("FloorStrip_E", "MI_Strip_Emergency", (50, 754, 0), (750, 758, 2)),
]
HERO_CYLINDERS = [
    ("Pipe_W_Low", "MI_Pipe", (30, 58, 330), (770, 58, 330), 7.0),
    ("Pipe_W_High", "MI_Pipe", (30, 74, 352), (770, 74, 352), 7.0),
    ("Cable_E", "MI_Vault_Trim", (30, 742, 372), (770, 742, 372), 4.0),
]
HERO_SHELL = {"Floor": "MI_Hull_Floor", "Ceiling": "MI_Hull_Ceiling", "Wall": "MI_Hull_Wall", "Lintel": "MI_Hull_Wall"}
HERO_FOG = {"location": (400.0, 400.0, 150.0), "scale": 3.5, "properties": {"radial_fog_extinction": 0.6, "height_fog_extinction": 0.4}}

# ---------------------------------------------------------------- raid atmosphere (data, S4-03 clarification 6)

FOG = {"fog_density": 0.015, "fog_height_falloff": 0.2, "enable_volumetric_fog": True, "volumetric_fog_scattering_distribution": 0.4,
       "volumetric_fog_extinction_scale": 1.0, "volumetric_fog_distance": 3000.0}
FOG_COLORS = {"fog_inscattering_luminance": (0.01, 0.012, 0.016, 1.0), "volumetric_fog_albedo": (150, 150, 160)}
POST = {"auto_exposure_method": "AEM_HISTOGRAM", "auto_exposure_min_brightness": -2.0, "auto_exposure_max_brightness": 4.0,
        "auto_exposure_bias": 0.5, "auto_exposure_speed_up": 3.0, "auto_exposure_speed_down": 1.0, "bloom_intensity": 0.4,
        "local_exposure_highlight_contrast_scale": 0.8, "local_exposure_shadow_contrast_scale": 0.8, "lumen_scene_lighting_quality": 1.0,
        "lumen_final_gather_quality": 1.0, "lumen_reflection_quality": 1.0, "vignette_intensity": 0.3, "film_grain_intensity": 0.15}

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel = unreal.MaterialEditingLibrary


def log(message):
    unreal.log("create_lighting_assets: " + message)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset " + path)
    return asset


def save(asset):
    if not assets.save_loaded_asset(asset, False):
        raise RuntimeError("Could not save " + asset.get_path_name())


def existing_or_new(path, asset_class, factory):
    if assets.does_asset_exist(path):
        return load(path), False
    package_path, name = path.rsplit("/", 1)
    asset = tools.create_asset(name, package_path, asset_class, factory)
    if asset is None:
        raise RuntimeError("Could not create " + path)
    return asset, True


def make_tag(name):
    tag = unreal.GameplayTag()
    if not tag.import_text('(TagName="%s")' % name):
        raise RuntimeError("Unknown gameplay tag " + name)
    return tag


def digest(*specs):
    return hashlib.sha1(json.dumps(specs, sort_keys=True, default=str).encode("ascii")).hexdigest()


def colour(rgb):
    return unreal.Color(r=rgb[0], g=rgb[1], b=rgb[2], a=255)


def linear(rgb):
    return unreal.LinearColor(rgb[0], rgb[1], rgb[2], rgb[3] if len(rgb) > 3 else 1.0)


# ---------------------------------------------------------------- budget

def ensure_budget():
    budget, created = existing_or_new(BUDGET, unreal.HollowLightBudgetConfig, data_asset_factory(unreal.HollowLightBudgetConfig))
    wanted = []
    for room_type in sorted(BUDGET_ROOMS):
        limits = unreal.HollowRoomLightLimits()
        limits.set_editor_property("room_type", make_tag("Hollow.Room.Type." + room_type))
        for name, value in BUDGET_ROOMS[room_type].items():
            limits.set_editor_property(name, value)
        wanted.append(limits)
    current = budget.get_editor_property("rooms")
    if created or [l.export_text() for l in current] != [l.export_text() for l in wanted] \
            or budget.get_editor_property("max_shadowed_in_view") != MAX_SHADOWED_IN_VIEW:
        budget.set_editor_property("rooms", wanted)
        budget.set_editor_property("max_shadowed_in_view", MAX_SHADOWED_IN_VIEW)
        save(budget)
        log("DA_LightBudget ready")
    return budget


def data_asset_factory(asset_class):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    return factory


# ---------------------------------------------------------------- material graphs

class Graph(object):
    """Places material expressions in columns and wires them (material or light function)."""

    def __init__(self, material, collection=None):
        self.material = material
        self.collection = collection
        self.row = 0

    def node(self, expression_class, column, **properties):
        expression = mel.create_material_expression(self.material, expression_class, -260 * column, 110 * self.row)
        self.row += 1
        for name, value in properties.items():
            expression.set_editor_property(name, value)
        return expression

    def wire(self, source, target, target_input="", source_output=""):
        if not mel.connect_material_expressions(source, source_output, target, target_input):
            raise RuntimeError("Could not connect %s -> %s.%s" % (source.get_name(), target.get_name(), target_input))

    def math(self, expression_class, column, a, b=None, const_b=None):
        properties = {} if const_b is None else {"const_b": const_b}
        expression = self.node(expression_class, column, **properties)
        self.wire(a, expression, "A")
        if b is not None:
            self.wire(b, expression, "B")
        return expression

    def single(self, expression_class, column, source, **properties):
        expression = self.node(expression_class, column, **properties)
        self.wire(source, expression)
        return expression

    def add(self, column, a, b=None, const=None):
        return self.math(unreal.MaterialExpressionAdd, column, a, b, const)

    def sub(self, column, a, b=None, const=None):
        return self.math(unreal.MaterialExpressionSubtract, column, a, b, const)

    def mul(self, column, a, b=None, const=None):
        return self.math(unreal.MaterialExpressionMultiply, column, a, b, const)

    def div(self, column, a, b=None, const=None):
        return self.math(unreal.MaterialExpressionDivide, column, a, b, const)

    def saturate(self, column, source):
        return self.single(unreal.MaterialExpressionSaturate, column, source)

    def lerp(self, column, a, b, alpha):
        expression = self.node(unreal.MaterialExpressionLinearInterpolate, column)
        self.wire(a, expression, "A")
        self.wire(b, expression, "B")
        self.wire(alpha, expression, "Alpha")
        return expression

    def const(self, column, value):
        return self.node(unreal.MaterialExpressionConstant, column, r=value)

    def scalar(self, column, name, value):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=value)

    def vector(self, column, name, rgb):
        return self.node(unreal.MaterialExpressionVectorParameter, column, parameter_name=name, default_value=linear(rgb))

    def mask(self, column, source, channel):
        return self.single(unreal.MaterialExpressionComponentMask, column, source, r=channel == "r", g=channel == "g", b=channel == "b", a=False)

    def wave(self, column, time, hertz):
        return self.single(unreal.MaterialExpressionSine, column, self.mul(column + 1, time, const=hertz), period=1.0)

    def ramp(self, column, value, start, span):
        """saturate((value - start) / span)"""
        return self.saturate(column, self.mul(column + 1, self.sub(column + 2, value, const=start), const=1.0 / span))

    def resonance(self):
        intensity = self.node(unreal.MaterialExpressionCollectionParameter, 12, collection=self.collection, parameter_name=INTENSITY)
        offset = self.node(unreal.MaterialExpressionCollectionParameter, 12, collection=self.collection, parameter_name=TIME_OFFSET)
        time = self.add(11, self.node(unreal.MaterialExpressionTime, 12), offset)   # in phase on every machine
        return intensity, time

    def reduced(self):
        """MPC_Resonance ReducedFlashes: 1 if the player chose reduced flashes (H-1)."""
        return self.node(unreal.MaterialExpressionCollectionParameter, 12, collection=self.collection, parameter_name=REDUCED_FLASHES)

    def emergency(self, intensity, time):
        """1 at Low, drop-outs growing from Medium, a strobe at Critical (FLICKER); steady dim and a slow pulse when reduced."""
        f, r = FLICKER, REDUCED
        wave = self.mul(8, self.wave(9, time, f["wave_a_hz"]), self.wave(9, time, f["wave_b_hz"]))
        dropout = self.saturate(6, self.add(7, self.mul(7, wave, const=f["dropout_gain"]), const=f["dropout_bias"]))
        one = self.const(6, 1.0)
        flicker_ramp = self.ramp(6, intensity, f["flicker_from"], f["flicker_span"])
        flicker = self.lerp(5, one, dropout, flicker_ramp)
        pulse = self.single(unreal.MaterialExpressionFloor, 6,
                            self.mul(7, self.single(unreal.MaterialExpressionFrac, 8, self.mul(9, time, const=f["strobe_hz"])), const=2.0))
        strobe_ramp = self.ramp(6, intensity, f["strobe_from"], f["strobe_span"])
        strobe = self.lerp(5, one, pulse, strobe_ramp)
        full = self.mul(4, flicker, strobe)
        # Reduced: the same ramps, a steady drop-out level and a smooth slow pulse instead of the square strobe.
        calm = self.lerp(5, one, self.const(6, r["dropout_level"]), flicker_ramp)
        slow = self.sub(6, one, self.mul(7, self.add(8, self.mul(9, self.wave(10, time, r["strobe_hz"]), const=0.5), const=0.5), const=r["strobe_depth"]))
        soft = self.lerp(5, one, slow, strobe_ramp)
        return self.lerp(3, full, self.mul(4, calm, soft), self.reduced())


def rebuild_if_changed(path, factory, spec, build):
    material, created = existing_or_new(path, unreal.Material, factory)
    if not created and assets.get_metadata_tag(material, SPEC_TAG) == digest(spec):
        return material
    for _ in range(CLEAR_ATTEMPTS):   # delete_all_material_expressions removes about half a graph per call
        if not mel.get_num_material_expressions(material):
            break
        mel.delete_all_material_expressions(material)
    build(material)
    mel.recompile_material(material)
    assets.set_metadata_tag(material, SPEC_TAG, digest(spec))
    save(material)
    log("%s built (%d expressions)" % (path.rsplit("/", 1)[1], mel.get_num_material_expressions(material)))
    return material


def build_emergency(material, collection):
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_LIGHT_FUNCTION)
    g = Graph(material, collection)
    intensity, time = g.resonance()
    if not mel.connect_material_property(g.emergency(intensity, time), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Could not connect M_LF_Emergency")


def build_dying(material, collection):
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_LIGHT_FUNCTION)
    g = Graph(material, collection)
    d = DYING
    intensity, time = g.resonance()
    dip = g.saturate(5, g.mul(6, g.mul(7, g.wave(8, time, d["slow_hz"]), g.wave(8, time, d["fast_hz"])), const=d["dip_gain"]))
    ramp = g.ramp(6, intensity, d["from"], d["span"])
    brownout = g.mul(4, g.mul(5, dip, ramp), const=d["depth"])
    steady = g.mul(4, ramp, const=d["depth"] * REDUCED["brownout_share"])   # reduced flashes: a steady dim (H-1)
    dimming = g.lerp(3, brownout, steady, g.reduced())
    if not mel.connect_material_property(g.single(unreal.MaterialExpressionOneMinus, 2, dimming), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Could not connect M_LF_DyingTube")


def build_surface(material):
    s = SURFACE_SPEC
    scalars, vectors = SURFACE_DEFAULTS["scalars"], SURFACE_DEFAULTS["vectors"]
    g = Graph(material)
    position = g.node(unreal.MaterialExpressionWorldPosition, 12)
    normal = g.single(unreal.MaterialExpressionAbs, 11, g.node(unreal.MaterialExpressionVertexNormalWS, 12))
    panel = g.scalar(12, "PanelSize", scalars["PanelSize"])
    # Seams: distance to the nearest panel edge along each world axis, then the two axes of each face's plane.
    cells = g.single(unreal.MaterialExpressionFrac, 10, g.div(11, position, panel))
    edge = g.mul(8, g.math(unreal.MaterialExpressionMin, 9, cells, g.single(unreal.MaterialExpressionOneMinus, 9, cells)), panel)
    seam = g.single(unreal.MaterialExpressionOneMinus, 6, g.saturate(7, g.div(7, edge, g.scalar(8, "SeamWidth", scalars["SeamWidth"]))))
    sx, sy, sz = g.mask(5, seam, "r"), g.mask(5, seam, "g"), g.mask(5, seam, "b")
    Max = unreal.MaterialExpressionMax
    planes = g.math(unreal.MaterialExpressionAppendVector, 3, g.math(unreal.MaterialExpressionAppendVector, 4, g.math(Max, 4, sy, sz), g.math(Max, 4, sx, sz)),
                    g.math(Max, 4, sx, sy))
    seams = g.math(unreal.MaterialExpressionDotProduct, 2, planes, normal)
    # Grime: engine noise in world space.
    noise = g.node(unreal.MaterialExpressionNoise, 9, scale=1.0, levels=4, output_min=0.0, output_max=1.0)
    g.wire(g.div(10, position, g.scalar(11, "GrimeScale", scalars["GrimeScale"])), noise, "")   # its first input, Position
    threshold = g.add(8, g.mul(9, g.scalar(10, "GrimeAmount", scalars["GrimeAmount"]), const=-s["grime_floor"]), const=1.0)
    grime = g.saturate(6, g.mul(7, g.sub(8, noise, threshold), const=s["grime_gain"]))
    # Hazard stripes along x + y + z.
    diagonal = g.add(9, g.add(10, g.mask(11, position, "r"), g.mask(11, position, "g")), g.mask(11, position, "b"))
    stripes = g.single(unreal.MaterialExpressionFloor, 6, g.mul(7, g.single(unreal.MaterialExpressionFrac, 8,
                                                                             g.div(9, diagonal, g.scalar(10, "StripeWidth", scalars["StripeWidth"]))), const=2.0))
    stripe = g.mul(5, stripes, g.scalar(6, "StripeAmount", scalars["StripeAmount"]))
    base = g.lerp(4, g.vector(5, "BaseColor", vectors["BaseColor"]), g.vector(5, "StripeColor", vectors["StripeColor"]), stripe)
    seam_shade = g.single(unreal.MaterialExpressionOneMinus, 2, g.mul(3, seams, g.scalar(4, "SeamDarken", scalars["SeamDarken"])))
    grime_shade = g.single(unreal.MaterialExpressionOneMinus, 2, g.mul(3, grime, const=s["grime_darken"]))
    colour_out = g.mul(1, g.mul(2, base, seam_shade), grime_shade)
    roughness = g.lerp(1, g.scalar(2, "Roughness", scalars["Roughness"]), g.const(2, 1.0), g.mul(2, grime, const=s["grime_roughness"]))
    metallic = g.scalar(1, "Metallic", scalars["Metallic"])

    # Art-01 (ADR-0043): CC0 textures, one sample each, projected along the face's dominant world axis.
    OneMinus, Append = unreal.MaterialExpressionOneMinus, unreal.MaterialExpressionAppendVector
    nx, ny, nz = g.mask(11, normal, "r"), g.mask(11, normal, "g"), g.mask(11, normal, "b")
    is_x = g.saturate(9, g.mul(10, g.sub(10, nx, g.math(Max, 11, ny, nz)), const=s["axis_sharpness"]))
    is_y = g.mul(9, g.saturate(10, g.mul(10, g.sub(11, ny, nz), const=s["axis_sharpness"])), g.single(OneMinus, 10, is_x))
    is_z = g.single(OneMinus, 8, g.add(9, is_x, is_y))
    px, py, pz = g.mask(11, position, "r"), g.mask(11, position, "g"), g.mask(11, position, "b")
    uv = g.add(7, g.add(8, g.mul(9, g.math(Append, 10, py, pz), is_x), g.mul(9, g.math(Append, 10, px, pz), is_y)),
               g.mul(8, g.math(Append, 10, px, py), is_z))
    uv = g.div(6, uv, g.scalar(7, "TextureSize", scalars["TextureSize"]))
    maps = {}
    for name, (texture, sampler) in sorted(SURFACE_TEXTURES.items()):
        sample = g.node(unreal.MaterialExpressionTextureSampleParameter2D, 5, parameter_name=name, texture=load(TEXTURE_DIR + "/" + texture),
                        sampler_type=getattr(unreal.MaterialSamplerType, sampler))
        g.wire(uv, sample, "UVs")
        maps[name] = sample
    arm = maps["ARMMap"]
    ao = g.lerp(3, g.const(4, 1.0), g.mask(4, arm, "r"), g.const(4, s["ao_strength"]))
    textured_colour = g.mul(1, g.mul(2, g.mul(3, g.mul(4, maps["BaseColorMap"], g.vector(5, "TextureTint", vectors["TextureTint"])), ao), seam_shade), grime_shade)
    textured_roughness = g.lerp(1, g.mask(2, arm, "g"), g.const(2, 1.0), g.mul(2, grime, const=s["grime_roughness"]))
    textured_metallic = g.mul(1, g.mask(2, arm, "b"), metallic)
    flat = g.node(unreal.MaterialExpressionConstant3Vector, 3, constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    textured_normal = g.lerp(1, flat, maps["NormalMap"], g.scalar(2, "NormalStrength", scalars["NormalStrength"]))

    def switch(textured, plain):
        expression = g.node(unreal.MaterialExpressionStaticSwitchParameter, 0, parameter_name="UseTextures", default_value=False)
        g.wire(textured, expression, "True")
        g.wire(plain, expression, "False")
        return expression

    for source, prop in ((switch(textured_colour, colour_out), unreal.MaterialProperty.MP_BASE_COLOR),
                         (switch(textured_roughness, roughness), unreal.MaterialProperty.MP_ROUGHNESS),
                         (switch(textured_metallic, metallic), unreal.MaterialProperty.MP_METALLIC),
                         (switch(textured_normal, flat), unreal.MaterialProperty.MP_NORMAL)):
        if not mel.connect_material_property(source, "", prop):
            raise RuntimeError("Could not connect M_Hollow_Surface")


def build_emissive(material, collection):
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    scalars, vectors = EMISSIVE_DEFAULTS["scalars"], EMISSIVE_DEFAULTS["vectors"]
    g = Graph(material, collection)
    intensity, time = g.resonance()
    flicker = g.lerp(3, g.const(4, 1.0), g.emergency(intensity, time), g.scalar(4, "FollowResonance", scalars["FollowResonance"]))
    glow = g.mul(1, g.mul(2, g.vector(3, "Color", vectors["Color"]), g.scalar(3, "Strength", scalars["Strength"])), flicker)
    if not mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Could not connect M_Hollow_Emissive")


# Material hashes: REDUCED only here (not in the room hashes), so it never re-saves a level.
EMERGENCY_SPEC = ["emergency", FLICKER, REDUCED]
DYING_SPEC = ["dying", DYING, REDUCED]
EMISSIVE_SPEC = ["emissive", FLICKER, EMISSIVE_DEFAULTS, REDUCED]
# AK-14 (POLISH-01, ADR-0053): what the masters are drawn on besides plain meshes. A cooked game draws a material without
# the flag with the engine's default one (the editor sets it on the fly and never saves it): the Cathedral's decor, its
# arcs' emitters and beams and its altar are instanced static meshes. Kept out of the graph specs: setting a flag never
# rebuilds a graph.
MASTER_USAGE = {SURFACE: ["used_with_instanced_static_meshes"], EMISSIVE: ["used_with_instanced_static_meshes"]}


def ensure_usage(path, material):
    missing = [flag for flag in MASTER_USAGE.get(path, []) if not material.get_editor_property(flag)]
    for flag in missing:
        material.set_editor_property(flag, True)
    if missing:
        mel.recompile_material(material)
        save(material)
        log("%s: %s" % (path.rsplit("/", 1)[1], ", ".join(missing)))


def ensure_materials():
    collection = load(COLLECTION)
    factory = unreal.MaterialFactoryNew()
    made = {
        LF_EMERGENCY: rebuild_if_changed(LF_EMERGENCY, factory, EMERGENCY_SPEC, lambda m: build_emergency(m, collection)),
        LF_DYING: rebuild_if_changed(LF_DYING, factory, DYING_SPEC, lambda m: build_dying(m, collection)),
        SURFACE: rebuild_if_changed(SURFACE, factory, ["surface", SURFACE_SPEC, SURFACE_DEFAULTS], build_surface),
        EMISSIVE: rebuild_if_changed(EMISSIVE, factory, EMISSIVE_SPEC, lambda m: build_emissive(m, collection)),
    }
    for path, material in sorted(made.items()):
        ensure_usage(path, material)
    instances = {}
    for name, (parent_path, values) in sorted(MATERIAL_INSTANCES.items()):
        path = MATERIAL_DIR + "/" + name
        instance, created = existing_or_new(path, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        changed = created
        if instance.get_editor_property("parent") != made[parent_path]:
            instance.set_editor_property("parent", made[parent_path])
            changed = True
        for parameter, value in values.get("scalars", {}).items():
            if abs(mel.get_material_instance_scalar_parameter_value(instance, parameter) - value) > 1e-4:
                mel.set_material_instance_scalar_parameter_value(instance, parameter, value)
                changed = True
        for parameter, rgb in values.get("vectors", {}).items():
            current = mel.get_material_instance_vector_parameter_value(instance, parameter)
            if any(abs(a - b) > 1e-4 for a, b in zip((current.r, current.g, current.b), rgb)):
                mel.set_material_instance_vector_parameter_value(instance, parameter, linear(rgb))
                changed = True
        for parameter, texture in values.get("textures", {}).items():
            wanted = load(TEXTURE_DIR + "/" + texture)
            if mel.get_material_instance_texture_parameter_value(instance, parameter) != wanted:
                mel.set_material_instance_texture_parameter_value(instance, parameter, wanted)
                changed = True
        for parameter, enabled in values.get("switches", {}).items():
            if mel.get_material_instance_static_switch_parameter_value(instance, parameter) != enabled:
                mel.set_material_instance_static_switch_parameter_value(instance, parameter, enabled)
                changed = True
        if changed:
            mel.update_material_instance(instance)
            save(instance)
            log(name + " ready")
        instances[name] = instance
    return made, instances


# ---------------------------------------------------------------- room rigs

def room_definitions():
    rooms = []
    for path in sorted(assets.list_assets(ROOM_DATA_DIR, recursive=False, include_folder=False)):
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.HollowRoomDefinition):
            rooms.append(asset)
    if not rooms:
        raise RuntimeError("No room definitions in " + ROOM_DATA_DIR)
    return rooms


def room_type(room):
    match = re.search(r'TagName="Hollow\.Room\.Type\.(\w+)"', room.get_editor_property("room_tags").export_text())
    if not match:
        raise RuntimeError("Room %s has no Hollow.Room.Type.*" % room.get_name())
    return match.group(1)


class Metrics(object):
    def __init__(self):
        cdo = unreal.get_default_object(unreal.HollowLevelMetrics)
        self.cell = float(cdo.get_editor_property("cell_size"))
        self.height = float(cdo.get_editor_property("cell_height"))
        self.wall = float(cdo.get_editor_property("wall_thickness"))


WALL_NORMALS = {"EAST": (0.0, -1.0), "WEST": (0.0, 1.0), "NORTH": (-1.0, 0.0), "SOUTH": (1.0, 0.0)}   # into the room


def wall_face(side, size, metrics):
    """Middle of a wall's inner face (room-local x, y)."""
    w = metrics.wall
    return {"EAST": (size[0] / 2.0, size[1] - w), "WEST": (size[0] / 2.0, w), "NORTH": (size[0] - w, size[1] / 2.0), "SOUTH": (w, size[1] / 2.0)}[side]


def free_wall(room):
    """A wall without connectors (East first), for the emergency light."""
    sides = [c.get_editor_property("side") for c in room.get_editor_property("connectors")]
    return next((name for name in ("EAST", "WEST", "NORTH", "SOUTH") if getattr(unreal.HollowRoomSide, name) not in sides), "EAST")


def rig_spec(room, metrics, hero):
    """Lights and fixtures of one room (room-local uu); also the input of the level's spec hash."""
    footprint = room.get_editor_property("footprint")
    size = (footprint.x * metrics.cell, footprint.y * metrics.cell)
    reach = math.sqrt(size[0] ** 2 + size[1] ** 2 + (footprint.z * metrics.height) ** 2)
    kind = room_type(room)
    pass_through = BUDGET_ROOMS[kind]["pass_through"]
    lights = []
    if hero:
        practicals = HERO_PRACTICALS
    else:
        count = 2 if footprint.x * footprint.y >= 9 else 1
        long_x = size[0] >= size[1]
        practicals = [((i + 0.5) * size[0] / count, size[1] / 2.0) if long_x else (size[0] / 2.0, (i + 0.5) * size[1] / count) for i in range(count)]
    for index, (x, y) in enumerate(practicals):
        lights.append({"kind": "practical", "label": "Practical_%d" % index, "location": (x, y, metrics.height - PRACTICAL["drop"]),
                       "shadowed": not pass_through, "volumetric_shadow": index == 0 and not pass_through,
                       "dying": hero and index == 1, "radius": reach * PRACTICAL["reach"],
                       "intensity": PRACTICAL_PASS_THROUGH["intensity"] if pass_through else PRACTICAL["intensity"]})
    walls = HERO_EMERGENCY_WALLS if hero else [free_wall(room)]
    for index, side in enumerate(walls):
        face = wall_face(side, size, metrics)
        normal = WALL_NORMALS[side]
        housing = (face[0] + normal[0] * EMERGENCY_HOUSING[1] / 2.0, face[1] + normal[1] * EMERGENCY_HOUSING[1] / 2.0, EMERGENCY["height"])
        light = (face[0] + normal[0] * EMERGENCY["inset"], face[1] + normal[1] * EMERGENCY["inset"], EMERGENCY["height"])
        lights.append({"kind": "emergency", "label": "Emergency_%d" % index, "location": light, "housing": housing, "side": side,
                       "radius": min(EMERGENCY["radius"], reach)})
    if hero:
        lights.append({"kind": "fill", "label": "CoreFill", "location": (size[0] / 2.0, size[1] / 2.0, HERO_FILL["height"]), "radius": HERO_FILL["radius"]})
    return {"room": room.get_name(), "size": size, "lights": lights, "practical": PRACTICAL, "emergency": EMERGENCY, "fill": HERO_FILL,
            "fixture": FIXTURE_PLATE, "housing": EMERGENCY_HOUSING,
            "hero": [HERO_BOXES, HERO_CYLINDERS, HERO_SHELL, HERO_FOG] if hero else None}


def vector(xyz):
    return unreal.Vector(xyz[0], xyz[1], xyz[2])


def managed(actor, label, folder, extra_tags=()):
    if actor is None:
        raise RuntimeError("Could not spawn " + label)
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.set_editor_property("tags", [MANAGED_TAG] + list(extra_tags))
    return actor


def tags_of(actor):
    return [str(t) for t in actor.get_editor_property("tags")]


def spawn_box(label, material, lo, hi, folder):
    centre = ((lo[0] + hi[0]) / 2.0, (lo[1] + hi[1]) / 2.0, (lo[2] + hi[2]) / 2.0)
    actor = managed(actors.spawn_actor_from_class(unreal.StaticMeshActor, vector(centre), unreal.Rotator(0.0, 0.0, 0.0)), label, folder)
    dressing(actor, CUBE, material, unreal.Vector((hi[0] - lo[0]) / SHAPE_SIZE, (hi[1] - lo[1]) / SHAPE_SIZE, (hi[2] - lo[2]) / SHAPE_SIZE))
    return actor


def spawn_cylinder(label, material, start, end, radius, folder):
    """A cylinder between two points along X (the engine cylinder stands along Z)."""
    centre = tuple((a + b) / 2.0 for a, b in zip(start, end))
    length = math.dist(start, end)
    actor = managed(actors.spawn_actor_from_class(unreal.StaticMeshActor, vector(centre), unreal.Rotator(roll=0.0, pitch=90.0, yaw=0.0)), label, folder)
    dressing(actor, CYLINDER, material, unreal.Vector(2.0 * radius / SHAPE_SIZE, 2.0 * radius / SHAPE_SIZE, length / SHAPE_SIZE))
    return actor


def dressing(actor, mesh, material, scale):
    component = actor.static_mesh_component
    component.set_static_mesh(load(mesh))
    component.set_material(0, material)
    # Looks only: gameplay, sockets and navmesh untouched. The profile itself, not just the flag: a loaded actor takes its
    # collision from its profile, so a BlockAll actor with the flag off collided again (found by Hollow.Editor.ArtBudget, ADR-0027).
    component.set_collision_profile_name("NoCollision")
    component.set_editor_property("can_ever_affect_navigation", False)
    actor.set_actor_scale3d(scale)


def light_actor(spec, materials):
    folder = "Lights"
    location = vector(spec["location"])
    if spec["kind"] == "practical":
        actor = actors.spawn_actor_from_class(unreal.RectLight, location, unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0))
        component = actor.rect_light_component
        p = PRACTICAL
        component.set_editor_property("source_width", p["source_width"])
        component.set_editor_property("source_height", p["source_height"])
        component.set_editor_property("barn_door_angle", p["barn_door_angle"])
        values = {"intensity": spec["intensity"], "color": p["color"], "shadows": spec["shadowed"], "scattering": p["volumetric_scattering"]}
    elif spec["kind"] == "emergency":
        actor = actors.spawn_actor_from_class(unreal.PointLight, location, unreal.Rotator(0.0, 0.0, 0.0))
        component = actor.point_light_component
        e = EMERGENCY
        values = {"intensity": e["intensity"], "color": e["color"], "shadows": False, "scattering": e["volumetric_scattering"]}
        component.set_editor_property("light_function_material", materials[LF_EMERGENCY])
    else:
        actor = actors.spawn_actor_from_class(unreal.PointLight, location, unreal.Rotator(0.0, 0.0, 0.0))
        component = actor.point_light_component
        f = HERO_FILL
        values = {"intensity": f["intensity"], "color": f["color"], "shadows": False, "scattering": f["volumetric_scattering"]}
    managed(actor, spec["label"], folder)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
    component.set_editor_property("intensity", values["intensity"])
    component.set_editor_property("light_color", colour(values["color"]))
    component.set_editor_property("attenuation_radius", spec["radius"])
    component.set_editor_property("cast_shadows", values["shadows"])
    component.set_editor_property("cast_volumetric_shadow", spec.get("volumetric_shadow", False))
    component.set_editor_property("volumetric_scattering_intensity", values["scattering"])
    if spec.get("dying"):
        component.set_editor_property("light_function_material", materials[LF_DYING])
    return actor


def fixture(spec, instances):
    x, y, z = spec["location"]
    if spec["kind"] == "practical":
        w, d, h = FIXTURE_PLATE
        spawn_box(spec["label"] + "_Panel", instances["MI_Strip_Cool"], (x - w / 2, y - d / 2, z + 6), (x + w / 2, y + d / 2, z + 6 + h), "Fixtures")
    elif spec["kind"] == "emergency":
        w, d, h = EMERGENCY_HOUSING
        if spec["side"] in ("NORTH", "SOUTH"):
            w, d = d, w   # the housing lies along its wall
        hx, hy, hz = spec["housing"]
        spawn_box(spec["label"] + "_Housing", instances["MI_Strip_Emergency"], (hx - w / 2, hy - d / 2, hz - h / 2), (hx + w / 2, hy + d / 2, hz + h / 2), "Fixtures")


def strip_fixture_shadows(level_actors):
    for actor in level_actors:
        if MANAGED_TAG in tags_of(actor) and actor.get_folder_path() == "Fixtures":
            actor.static_mesh_component.set_editor_property("cast_shadow", False)   # a lamp's own housing never shades it


def level_marker(level_actors):
    for actor in level_actors:
        if actor.get_actor_label() == MARKER_LABEL:
            return actor
    return None


def art_locked(room):
    """ADR-0027: a room level finished by hand (bArtLocked on its definition) is never touched; the tests still check it."""
    if room.get_editor_property("art_locked"):
        log("%s: art-locked, level kept as is" % room.get_name())
        return True
    return False


def ensure_room(room, metrics, materials, instances, hero):
    if art_locked(room):
        return False
    spec = rig_spec(room, metrics, hero)
    wanted = digest(spec, FLICKER, DYING, RIG_VERSION)
    level_path = room.get_editor_property("room_level").get_path_name().split(".")[0]
    if not levels.load_level(level_path):
        raise RuntimeError("Could not load " + level_path)
    level_actors = actors.get_all_level_actors()
    marker = level_marker(level_actors)
    if marker is not None and wanted in tags_of(marker):
        return False
    for actor in level_actors:
        if MANAGED_TAG in tags_of(actor) or actor.get_actor_label() in (LEGACY_LIGHT, MARKER_LABEL):
            actors.destroy_actor(actor)
    for light in spec["lights"]:
        light_actor(light, materials)
        fixture(light, instances)
    if hero:
        build_hero(instances)
    strip_fixture_shadows(actors.get_all_level_actors())
    managed(actors.spawn_actor_from_class(unreal.Note, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0)), MARKER_LABEL, "Lights", [wanted])
    if not levels.save_current_level():
        raise RuntimeError("Could not save " + level_path)
    log("%s: %d lights%s" % (room.get_name(), len(spec["lights"]), " + hero dressing" if hero else ""))
    return True


def build_hero(instances):
    for label, material, lo, hi in HERO_BOXES:
        spawn_box(label, instances[material], lo, hi, "Dressing")
    for label, material, start, end, radius in HERO_CYLINDERS:
        spawn_cylinder(label, instances[material], start, end, radius, "Dressing")
    for actor in actors.get_all_level_actors():
        label = actor.get_actor_label()
        prefix = next((p for p in HERO_SHELL if label == p or label.startswith(p + "_")), None)
        if prefix and isinstance(actor, unreal.StaticMeshActor) and MANAGED_TAG not in [str(t) for t in actor.tags]:
            actor.static_mesh_component.set_material(0, instances[HERO_SHELL[prefix]])
    fog = managed(actors.spawn_actor_from_class(unreal.LocalFogVolume, vector(HERO_FOG["location"]), unreal.Rotator(0.0, 0.0, 0.0)), "CoreHaze", "Atmosphere")
    fog.set_actor_scale3d(unreal.Vector(HERO_FOG["scale"], HERO_FOG["scale"], HERO_FOG["scale"]))
    component = fog.get_component_by_class(unreal.LocalFogVolumeComponent)
    for name, value in HERO_FOG["properties"].items():
        component.set_editor_property(name, value)


# ---------------------------------------------------------------- raid atmosphere

def ensure_atmosphere():
    wanted = digest(FOG, FOG_COLORS, POST)
    if not levels.load_level(RAID_MAP):
        raise RuntimeError("Could not load " + RAID_MAP)
    level_actors = actors.get_all_level_actors()
    marker = level_marker(level_actors)
    if marker is not None and wanted in tags_of(marker):
        return False
    for actor in level_actors:
        if MANAGED_TAG in tags_of(actor):
            actors.destroy_actor(actor)
    fog = managed(actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0)), "RaidFog", "Atmosphere")
    component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    for name, value in FOG.items():
        component.set_editor_property(name, value)
    component.set_editor_property("fog_inscattering_luminance", linear(FOG_COLORS["fog_inscattering_luminance"]))
    component.set_editor_property("volumetric_fog_albedo", colour(FOG_COLORS["volumetric_fog_albedo"]))
    volume = managed(actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0)), "RaidPost", "Atmosphere")
    volume.set_editor_property("unbound", True)
    settings = volume.get_editor_property("settings")
    for name, value in POST.items():
        if name == "auto_exposure_method":
            value = getattr(unreal.AutoExposureMethod, value)
        settings.set_editor_property("override_" + name, True)
        settings.set_editor_property(name, value)
    volume.set_editor_property("settings", settings)
    managed(actors.spawn_actor_from_class(unreal.Note, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0)), MARKER_LABEL, "Atmosphere", [wanted])
    if not levels.save_current_level():
        raise RuntimeError("Could not save " + RAID_MAP)
    log("L_Raid_Dev: volumetric fog and post-process")
    return True


# ---------------------------------------------------------------- validation

def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate(rooms):
    budget = load(BUDGET)
    check(len(budget.get_editor_property("rooms")) == len(BUDGET_ROOMS), "a limit per room type")
    for path, spec in ((LF_EMERGENCY, EMERGENCY_SPEC), (LF_DYING, DYING_SPEC)):
        material = load(path)
        check(material.get_editor_property("material_domain") == unreal.MaterialDomain.MD_LIGHT_FUNCTION, path + " is a light function")
        check(assets.get_metadata_tag(material, SPEC_TAG) == digest(spec), path + " matches its spec")
    check(assets.get_metadata_tag(load(EMISSIVE), SPEC_TAG) == digest(EMISSIVE_SPEC), EMISSIVE + " matches its spec")
    for path, flags in sorted(MASTER_USAGE.items()):
        for flag in flags:
            check(load(path).get_editor_property(flag), "%s: %s" % (path, flag))
    check(REDUCED["strobe_hz"] <= MAX_REDUCED_HZ, "no reduced-flashes pulse over 2 Hz")
    validator = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
    for room in rooms:
        level = load(room.get_editor_property("room_level").get_path_name().split(".")[0])
        result = validator.is_object_valid(level, unreal.DataValidationUsecase.SCRIPT)
        check(result[0] != unreal.DataValidationResult.INVALID, "%s fits its light budget: %s" % (room.get_name(), [str(e) for e in result[1]]))
    with open(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_config_dir()) + "DefaultGame.ini") as ini:
        check(any(line.strip() == "LightBudget=%s.%s" % (BUDGET, BUDGET.rsplit("/", 1)[1]) for line in ini), "UHollowLevelMetrics::LightBudget")
    log("validation passed (%d rooms)" % len(rooms))


def main():
    ensure_budget()
    materials, instances = ensure_materials()
    metrics = Metrics()
    rooms = room_definitions()
    built = sum(1 for room in rooms if ensure_room(room, metrics, materials, instances, room.get_name() == HERO_ROOM))
    atmosphere = ensure_atmosphere()
    log("rebuilt %d room rig(s)%s" % (built, ", the raid atmosphere" if atmosphere else ""))
    validate(rooms)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_lighting_assets failed:\n" + traceback.format_exc())
