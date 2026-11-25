"""Art-01: the procedural art kit and its CC0 textures, and where they go (idempotent, ADR-0010, ADR-0043).

  /Game/Hollow/Art/Kit/SM_*            static meshes built by Blender in the background from Build/Blender/hollow_kit.py
                                       (tools with a Muzzle socket, the Aether core's container, the door leaf with its
                                       weld seam, hub dressing, armour of the ExoFrame, the Iron Pilgrim, the Echo-Stalker)
  /Game/Hollow/Art/Materials/M_Kit_*   procedural masters (no textures): surfaces, glow with a slow pulse (reduced
                                       flashes hold it steady), a tool's glow while it runs (custom primitive data), the
                                       visor in the class's colour (custom primitive data), the red-hot weld seam that cools
                                       by itself, hazard stripes, screens; MI_Kit_* their looks (Docs/Art bible palette)
  /Game/Hollow/Art/Textures/T_*        CC0 textures of Poly Haven at 1K (Build/Art/cc0_textures.json, fetched by
                                       Build/Scripts/fetch-cc0-textures.sh into Intermediate; their SHA-256 is checked)
  /Game/Hollow/Art/Parts/DA_Parts_*    UHollowCosmeticPartSet: the armour on the placeholder skeleton's bones
  /Game/Hollow/FX/NS_Tool_BeamSparks   the beam's sparks (AK-8): the engine's Fountain emitter with its Collision module, built
                                       by UHollowNiagaraAuthoring; M_Kit_Spark their additive streak
Bindings (only fields the base scripts set once at creation, never again):
  DA_Tool_PlasmaCutter/Welder/AetherLantern   HeldMesh, HeldTransform, MuzzleSocket
  DA_Class_Engineer/Occultist                 HeldTool (the class's tool in hand)
  BP_AetherCore                                its visual: the container, the crystal on slot 0 (the item's look)
  BP_Door                                      the leaf with its seam slot and parameters
  BP_HubTerminal                               the console as its cabinet
  GCN_Tool_Beam                                the sparks
  BP_ExoFrame, BP_IronPilgrim, BP_EchoStalker  their part sets and the body's materials
Every asset keeps a hash of its spec (metadata "HollowSpec") and is rebuilt only when it changes: every save is a new
LFS object. Blender runs only when a mesh's spec changed; a machine without Blender keeps the meshes it has.
Run after the other create_* scripts:  Build/Scripts/run-editor-python.sh Build/Python/create_art_kit.py
"""
import hashlib
import json
import os
import shutil
import subprocess
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh
SPEC_TAG = "HollowSpec"

ART = "/Game/Hollow/Art"
KIT_DIR = ART + "/Kit"
MAT_DIR = ART + "/Materials"
TEX_DIR = ART + "/Textures"
PARTS_DIR = ART + "/Parts"
FX_DIR = "/Game/Hollow/FX"
SPARKS_PATH = FX_DIR + "/NS_Tool_BeamSparks"
LANTERN_CUE = "/Game/Hollow/GAS/Cues/GCN_Tool_Lantern"
LANTERN_FLASH = {"flash_intensity_scale": 2.0, "flash_seconds": 0.3}   # AK-8: the cone flares as the lantern lights
# AK-8: one stream of orange-white sparks, each a short streak along its flight, falling and bouncing off the floor
# (CPU sim, ray-traced collision). Built by UHollowNiagaraAuthoring: the engine's Fountain emitter, the engine's Collision
# module, these constants by the end of their names (Hollow.Editor.ArtSparks checks the budget). The beam cue turns the
# system so its up is the way out of the surface it bites.
SPARKS = {
    "version": 3,
    "emitter": "/Niagara/DefaultAssets/Templates/Emitters/Fountain.Fountain",
    "modules": ["/Niagara/Modules/Collision/Collision.Collision"],
    "constants": [
        ("SpawnRate.SpawnRate", "220"),                        # about 170 alive per tool: within the CPU budget, even four at once
        ("AddVelocity.Cone Angle", "50"),
        ("RandomRangeFloat.Minimum", "260"), ("RandomRangeFloat.Maximum", "700"),   # cm/s out of the cut
        ("InitializeParticle.Lifetime Min", "0.45"), ("InitializeParticle.Lifetime Max", "1.1"),
        ("InitializeParticle.Uniform Sprite Size Min", "7"), ("InitializeParticle.Uniform Sprite Size Max", "14"),   # streak length
        ("InitializeParticle.Color", "(R=8.0,G=2.0,B=0.3,A=1.0)"),   # deep orange; M_Kit_Spark makes the core white-hot
        ("Drag.Drag", "0.8"),
        ("GravityForce.Gravity", "(X=0,Y=0,Z=-980)"),
        ("Collision.Kill On Collision", "0"), ("Collision.Max Number Of Collisions", "3"),
        ("Collision.Restitution", "0.4"), ("Collision.Friction", "0.35"),
    ],
}
# GP-01 (ADR-0044): a shot's effects, one-shot bursts (DirectionalBurst: one burst, then done, so the pool takes them back)
# with +Z along the shot (UHollowWeaponFireCue). The tracer is one fast streak killed where it collides.
BURST_EMITTER = "/Niagara/DefaultAssets/Templates/Emitters/DirectionalBurst.DirectionalBurst"
COLLISION_MODULE = "/Niagara/Modules/Collision/Collision.Collision"
WEAPON_FX = {
    "NS_Weapon_MuzzleFlash": {"version": 1, "modules": [], "constants": [
        ("SpawnBurst_Instantaneous.Spawn Count", "40"), ("AddVelocity.Cone Axis", "(X=0,Y=0,Z=1)"), ("AddVelocity.Cone Angle", "22"),
        ("RandomRangeFloat001.Minimum", "400"), ("RandomRangeFloat001.Maximum", "1400"),
        ("InitializeParticle.Lifetime Min", "0.035"), ("InitializeParticle.Lifetime Max", "0.07"),
        ("InitializeParticle.Sprite Size Min", "(X=2,Y=6)"), ("InitializeParticle.Sprite Size Max", "(X=4,Y=16)"),
        ("InitializeParticle.Color", "(R=12.0,G=6.0,B=2.5,A=1.0)"), ("GravityForce.Gravity", "(X=0,Y=0,Z=0)"),
        ("SolveForcesAndVelocity.Speed Limit", "3000")]},
    "NS_Weapon_Tracer": {"version": 1, "modules": [COLLISION_MODULE], "constants": [
        ("SpawnBurst_Instantaneous.Spawn Count", "1"), ("AddVelocity.Cone Axis", "(X=0,Y=0,Z=1)"), ("AddVelocity.Cone Angle", "0"),
        ("RandomRangeFloat001.Minimum", "24000"), ("RandomRangeFloat001.Maximum", "24000"),
        ("InitializeParticle.Lifetime Min", "0.07"), ("InitializeParticle.Lifetime Max", "0.07"),
        ("InitializeParticle.Sprite Size Min", "(X=2.5,Y=90)"), ("InitializeParticle.Sprite Size Max", "(X=2.5,Y=90)"),
        ("ScaleSpriteSizeBySpeed.Min Scale Factor", "(X=1,Y=1)"), ("ScaleSpriteSizeBySpeed.Max Scale Factor", "(X=1,Y=1)"),
        ("InitializeParticle.Color", "(R=6.0,G=4.0,B=1.6,A=1.0)"), ("GravityForce.Gravity", "(X=0,Y=0,Z=0)"),
        ("SolveForcesAndVelocity.Speed Limit", "30000"), ("Collision.Kill On Collision", "1")]},
    "NS_Weapon_Impact": {"version": 1, "modules": [], "constants": [
        ("SpawnBurst_Instantaneous.Spawn Count", "14"), ("AddVelocity.Cone Axis", "(X=0,Y=0,Z=1)"), ("AddVelocity.Cone Angle", "55"),
        ("RandomRangeFloat001.Minimum", "200"), ("RandomRangeFloat001.Maximum", "700"),
        ("InitializeParticle.Lifetime Min", "0.15"), ("InitializeParticle.Lifetime Max", "0.35"),
        ("InitializeParticle.Sprite Size Min", "(X=1.5,Y=4)"), ("InitializeParticle.Sprite Size Max", "(X=2.5,Y=9)"),
        ("InitializeParticle.Color", "(R=6.0,G=2.0,B=0.4,A=1.0)")]},
}
WEAPON_FX_SLOTS = {"muzzle_flash": "NS_Weapon_MuzzleFlash", "tracer": "NS_Weapon_Tracer", "impact": "NS_Weapon_Impact"}
SPARK_MATERIAL = MAT_DIR + "/M_Kit_Spark"
SPARK_STREAK = (0.015, 0.07, 2.5)   # the streak's half width from..to (of the sprite's width), the white core's boost
COLLECTION = "/Game/Hollow/FX/Resonance/MPC_Resonance"
REDUCED_FLASHES = "ReducedFlashes"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
BLENDER_SCRIPT = os.path.join(PROJECT, "Build", "Blender", "hollow_kit.py")
# GP-01 (ADR-0044): the combat kit (weapons) is its own script reusing hollow_kit.py's primitives, so a mesh's hash
# follows its own script and the art kit's meshes stay as they are.
COMBAT_SCRIPT = os.path.join(PROJECT, "Build", "Blender", "hollow_combat_kit.py")
# GP-02 (ADR-0047): the creature kit (the Whisperers and their nests), its own script for the same reason.
CREATURE_SCRIPT = os.path.join(PROJECT, "Build", "Blender", "hollow_creature_kit.py")
SCRIPTS = {"kit": BLENDER_SCRIPT, "combat": COMBAT_SCRIPT, "creature": CREATURE_SCRIPT}
WORK = os.path.join(PROJECT, "Intermediate", "HollowArt")
CC0_MANIFEST = os.path.join(PROJECT, "Build", "Art", "cc0_textures.json")
BLENDER = [os.environ.get("HOLLOW_BLENDER", ""), "/Applications/Blender.app/Contents/MacOS/Blender",
           "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe"]
BLENDER_TIMEOUT = 900
FBX_SCALE = 100.0   # Blender writes metres; the FBX importer reads them as centimetres

# ---------------------------------------------------------------- the meshes (sizes in metres, from the project's data)

CORE_HALF = [0.25, 0.25, 0.35]             # create_carry_assets.py CORE_HALF (25, 25, 35 cm)
LEAF_THICKNESS = 0.1                       # create_door_assets.py LEAF_THICKNESS (10 cm)
KIT_VERSION = 3
MESHES = {
    "SM_Tool_PlasmaCutter": {"kind": "cutter"},
    "SM_Tool_Welder": {"kind": "welder"},
    "SM_Tool_AetherLantern": {"kind": "lantern"},
    "SM_AetherCore": {"kind": "core", "half": CORE_HALF},
    "SM_Door_Leaf": {"kind": "door_leaf", "size": None},           # from UHollowLevelMetrics at run time
    "SM_Hub_Console": {"kind": "console"},
    "SM_Hub_PilotSeat": {"kind": "pilot_seat"},
    "SM_Hub_Crate": {"kind": "crate"},
    "SM_Hub_PipeRun": {"kind": "pipe_run"},
    "SM_Exo_Helmet": {"kind": "exo_helmet"},
    "SM_Exo_Chest": {"kind": "exo_chest"},
    "SM_Exo_Pack": {"kind": "exo_pack"},
    "SM_Exo_Pauldron": {"kind": "exo_pauldron"},
    "SM_Exo_Bracer": {"kind": "exo_bracer"},
    "SM_Exo_Shin": {"kind": "exo_shin"},
    "SM_Pilgrim_Pauldron": {"kind": "pilgrim_pauldron"},
    "SM_Pilgrim_Chest": {"kind": "pilgrim_chest"},
    "SM_Pilgrim_Helm": {"kind": "pilgrim_helm"},
    "SM_Pilgrim_Gauntlet": {"kind": "pilgrim_gauntlet"},
    "SM_Stalker_Crest": {"kind": "stalker_crest"},
    "SM_Stalker_Spines": {"kind": "stalker_spines"},
    "SM_Stalker_Eyes": {"kind": "stalker_eyes"},
    "SM_Stalker_Blade": {"kind": "stalker_blade"},
    # GP-01 (ADR-0044): the weapons, muzzles along +X, the tools' grip.
    "SM_Weapon_Nailgun": {"kind": "nailgun", "script": "combat"},
    "SM_Weapon_Shotgun": {"kind": "shotgun", "script": "combat"},
    # GP-01b (ADR-0045): loot on the floor.
    "SM_Pickup_Scrap": {"kind": "pickup_scrap", "script": "combat"},
    "SM_Pickup_Shard": {"kind": "pickup_shard", "script": "combat"},
    "SM_Pickup_FuelCell": {"kind": "pickup_fuelcell", "script": "combat"},
    "SM_Pickup_Nails": {"kind": "pickup_ammo", "script": "combat"},
    "SM_Pickup_Shells": {"kind": "pickup_ammo", "script": "combat", "tall": True},
    # GP-02 (ADR-0047): a Whisperer (front along +X, origin on the floor) and its nest.
    "SM_Whisperer_Body": {"kind": "whisperer", "script": "creature"},
    "SM_Whisperer_Nest": {"kind": "whisperer_nest", "script": "creature"},
}
COLLIDING = {"SM_Door_Leaf", "SM_Hub_Console"}   # a box round it; everything else is looks only
MAX_TRIANGLES = 6000                              # per kit mesh (Hollow.Editor.ArtKit checks the same)

# ---------------------------------------------------------------- materials (Docs/Art bible palette, linear colours)

SURFACE_MASTER = MAT_DIR + "/M_Kit_Surface"
GLOW_MASTER = MAT_DIR + "/M_Kit_Glow"
TOOL_GLOW_MASTER = MAT_DIR + "/M_Kit_ToolGlow"
VISOR_MASTER = MAT_DIR + "/M_Kit_Visor"
SEAM_MASTER = MAT_DIR + "/M_Kit_WeldSeam"
HAZARD_MASTER = MAT_DIR + "/M_Kit_Hazard"
SCREEN_MASTER = MAT_DIR + "/M_Kit_Screen"
CREATURE_MASTER = MAT_DIR + "/M_Kit_Creature"   # GP-02: shell, scuttling legs and eyes of a Whisperer, its nest
MASTERS_VERSION = 2
# A pulse never faster than 3 Hz (photosensitivity); the reduced-flashes choice holds every one steady (H-1, ADR-0029).
MATERIALS = {
    "MI_Kit_DarkAlloy": (SURFACE_MASTER, {"Color": (0.03, 0.035, 0.04), "Metallic": 0.85, "Roughness": 0.42}),
    "MI_Kit_Rubber": (SURFACE_MASTER, {"Color": (0.018, 0.018, 0.02), "Metallic": 0.0, "Roughness": 0.85}),
    "MI_Kit_Steel": (SURFACE_MASTER, {"Color": (0.42, 0.43, 0.45), "Metallic": 1.0, "Roughness": 0.32}),
    "MI_Kit_AmberPaint": (SURFACE_MASTER, {"Color": (0.86, 0.13, 0.035), "Metallic": 0.15, "Roughness": 0.55}),
    "MI_Kit_Gilt": (SURFACE_MASTER, {"Color": (0.46, 0.32, 0.1), "Metallic": 1.0, "Roughness": 0.28}),
    "MI_Kit_Glass": (SURFACE_MASTER, {"Color": (0.01, 0.014, 0.018), "Metallic": 0.0, "Roughness": 0.05}),
    "MI_Kit_PilgrimIron": (SURFACE_MASTER, {"Color": (0.05, 0.042, 0.036), "Metallic": 0.9, "Roughness": 0.62}),
    "MI_Kit_Obsidian": (SURFACE_MASTER, {"Color": (0.004, 0.004, 0.006), "Metallic": 0.0, "Roughness": 0.34}),
    "MI_Kit_ExoArmour": (SURFACE_MASTER, {"Color": (0.045, 0.05, 0.055), "Metallic": 0.6, "Roughness": 0.5}),
    "MI_Kit_DoorLeaf": (SURFACE_MASTER, {"Color": (0.16, 0.17, 0.18), "Metallic": 0.7, "Roughness": 0.48}),
    "MI_Kit_AetherGlow": (GLOW_MASTER, {"GlowColor": (0.32, 0.85, 0.84), "Strength": 4.0, "PulseHz": 0.5, "PulseDepth": 0.3}),
    "MI_Kit_Runes": (GLOW_MASTER, {"GlowColor": (0.46, 0.28, 0.78), "GlowColorB": (0.71, 0.6, 0.35), "SwapHz": 0.25, "Strength": 9.0,
                                   "PulseHz": 0.6, "PulseDepth": 0.55}),
    "MI_Kit_Furnace": (GLOW_MASTER, {"GlowColor": (1.0, 0.3, 0.04), "Strength": 4.0, "PulseHz": 0.7, "PulseDepth": 0.4}),
    "MI_Kit_StalkerEyes": (GLOW_MASTER, {"GlowColor": (0.75, 0.04, 0.02), "Strength": 2.5, "PulseHz": 0.3, "PulseDepth": 0.5}),
    "MI_Kit_LanternGlow": (GLOW_MASTER, {"GlowColor": (0.35, 0.9, 0.85), "Strength": 6.0, "PulseHz": 0.4, "PulseDepth": 0.2}),
    "MI_Kit_ConsoleKeys": (GLOW_MASTER, {"GlowColor": (1.0, 0.55, 0.15), "Strength": 3.0, "PulseHz": 0.0, "PulseDepth": 0.0}),
    "MI_Kit_StatusGlow": (GLOW_MASTER, {"GlowColor": (0.3, 0.9, 0.5), "Strength": 4.0, "PulseHz": 0.5, "PulseDepth": 0.4}),
    "MI_Kit_CutterGlow": (TOOL_GLOW_MASTER, {"GlowColor": (1.0, 0.42, 0.12), "Idle": 0.4, "Firing": 30.0}),
    "MI_Kit_WelderGlow": (TOOL_GLOW_MASTER, {"GlowColor": (0.45, 0.7, 1.0), "Idle": 0.3, "Firing": 35.0}),
    "MI_Kit_LanternFlame": (TOOL_GLOW_MASTER, {"GlowColor": (0.35, 0.9, 0.85), "Idle": 4.0, "Firing": 28.0}),   # AK-8: bright while it runs
    "MI_Kit_Visor": (VISOR_MASTER, {"Strength": 5.0}),
    "MI_Kit_WeldSeam": (SEAM_MASTER, {"HotColor": (1.0, 0.32, 0.04), "HotStrength": 25.0, "ScarColor": (0.3, 0.13, 0.05), "ScarStrength": 0.8,
                                      "CoolSeconds": 4.0}),
    "MI_Kit_Hazard": (HAZARD_MASTER, {"ColorA": (0.86, 0.32, 0.03), "ColorB": (0.02, 0.02, 0.02), "Stripes": 4.0}),
    "MI_Kit_Screen": (SCREEN_MASTER, {"ScreenColor": (0.25, 0.85, 0.8), "Strength": 2.5, "Lines": 120.0}),
    # GP-02: a Whisperer - dark oily chitin, steel-dark legs that scuttle (their tips lift, 3 Hz), pale violet eyes that
    # flare white-hot in a beam (AHollowWhisperer's custom data; steady with reduced flashes); its nest's flesh and pods.
    "MI_Kit_WhispererShell": (CREATURE_MASTER, {"Color": (0.022, 0.018, 0.026), "Metallic": 0.5, "Roughness": 0.3}),
    "MI_Kit_WhispererLegs": (CREATURE_MASTER, {"Color": (0.035, 0.032, 0.034), "Metallic": 0.85, "Roughness": 0.38, "Swing": 2.5,
                                               "SwingHz": 3.0, "LegHeight": 18.0}),
    "MI_Kit_WhispererEyes": (CREATURE_MASTER, {"Color": (0.02, 0.02, 0.025), "Roughness": 0.15, "GlowColor": (0.62, 0.55, 1.0),
                                               "GlowStrength": 4.0, "FlareStrength": 40.0}),
    "MI_Kit_NestFlesh": (CREATURE_MASTER, {"Color": (0.045, 0.026, 0.032), "Metallic": 0.15, "Roughness": 0.55}),
    "MI_Kit_NestPods": (CREATURE_MASTER, {"Color": (0.03, 0.02, 0.04), "Roughness": 0.2, "GlowColor": (0.5, 0.35, 0.95), "GlowStrength": 2.5}),
}
SLOT_MATERIALS = {"Body": "MI_Kit_DarkAlloy", "Metal": "MI_Kit_Steel", "Trim": "MI_Kit_AmberPaint", "Glow": "MI_Kit_AetherGlow",
                  "Gilt": "MI_Kit_Gilt", "Glass": "MI_Kit_Glass", "Runes": "MI_Kit_Runes", "Visor": "MI_Kit_Visor",
                  "Seam": "MI_Kit_WeldSeam", "Screen": "MI_Kit_Screen"}
MESH_SLOTS = {   # per mesh, slot -> material where it differs from SLOT_MATERIALS
    "SM_Tool_PlasmaCutter": {"Glow": "MI_Kit_CutterGlow"},
    "SM_Tool_Welder": {"Glow": "MI_Kit_WelderGlow"},
    "SM_Tool_AetherLantern": {"Glow": "MI_Kit_LanternFlame", "Glass": "MI_Kit_LanternFlame"},   # the glass glows with its flame
    "SM_Weapon_Nailgun": {"Glow": "MI_Kit_CutterGlow"},   # GP-01: the charged tips, idle
    "SM_Weapon_Shotgun": {"Glow": "MI_Kit_CutterGlow"},
    "SM_Pickup_FuelCell": {"Glow": "MI_Kit_WelderGlow"},
    "SM_AetherCore": {"Glow": "/Game/Hollow/Items/AetherCore/MI_AetherCore"},
    "SM_Door_Leaf": {"Body": "MI_Kit_DoorLeaf", "Trim": "MI_Kit_Hazard"},
    "SM_Hub_Console": {"Glow": "MI_Kit_ConsoleKeys", "Trim": "MI_Kit_Hazard"},
    "SM_Hub_PilotSeat": {"Body": "MI_Kit_Rubber"},
    "SM_Exo_Helmet": {"Body": "MI_Kit_ExoArmour", "Glow": "MI_Kit_StatusGlow"},
    "SM_Exo_Chest": {"Body": "MI_Kit_ExoArmour", "Glow": "MI_Kit_StatusGlow"},
    "SM_Exo_Pack": {"Body": "MI_Kit_ExoArmour"},
    "SM_Exo_Pauldron": {"Body": "MI_Kit_ExoArmour"},
    "SM_Exo_Bracer": {"Body": "MI_Kit_ExoArmour", "Glow": "MI_Kit_StatusGlow"},
    "SM_Exo_Shin": {"Body": "MI_Kit_ExoArmour"},
    "SM_Pilgrim_Pauldron": {"Body": "MI_Kit_PilgrimIron"},
    "SM_Pilgrim_Chest": {"Body": "MI_Kit_PilgrimIron", "Glow": "MI_Kit_Furnace"},
    "SM_Pilgrim_Helm": {"Body": "MI_Kit_PilgrimIron", "Glow": "MI_Kit_Furnace"},
    "SM_Pilgrim_Gauntlet": {"Body": "MI_Kit_PilgrimIron"},
    "SM_Stalker_Crest": {"Body": "MI_Kit_Obsidian"},
    "SM_Stalker_Spines": {"Body": "MI_Kit_Obsidian"},
    "SM_Stalker_Eyes": {"Glow": "MI_Kit_StalkerEyes"},
    "SM_Stalker_Blade": {"Body": "MI_Kit_Obsidian"},
    "SM_Whisperer_Body": {"Shell": "MI_Kit_WhispererShell", "Legs": "MI_Kit_WhispererLegs", "Eyes": "MI_Kit_WhispererEyes"},
    "SM_Whisperer_Nest": {"Shell": "MI_Kit_NestFlesh", "Pods": "MI_Kit_NestPods"},
}

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
meshes = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
mel = unreal.MaterialEditingLibrary


def log(message):
    unreal.log("create_art_kit: " + message)


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


def existing_or_new(path, asset_class, factory):
    if assets.does_asset_exist(path):
        return load(path), False
    return create(path, asset_class, factory), True


def digest(*specs):
    return hashlib.sha1(json.dumps(specs, sort_keys=True).encode("ascii")).hexdigest()


def file_digest(path):
    with open(path, "rb") as f:
        return hashlib.sha1(f.read()).hexdigest()


def up_to_date(asset, spec_hash):
    return assets.get_metadata_tag(asset, SPEC_TAG) == spec_hash


def stamp(asset, spec_hash):
    assets.set_metadata_tag(asset, SPEC_TAG, spec_hash)
    save(asset)


def linear(rgb):
    return unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0)


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def material_path(name):
    return name if name.startswith("/") else MAT_DIR + "/" + name


# ---------------------------------------------------------------- material graphs

class Graph(object):
    def __init__(self, material):
        self.material = material
        self.row = 0
        for _ in range(16):   # delete_all_material_expressions removes about half a graph per call
            if not mel.get_num_material_expressions(material):
                break
            mel.delete_all_material_expressions(material)
        if mel.get_num_material_expressions(material):
            raise RuntimeError(material.get_name() + " kept old expressions")

    def node(self, expression_class, column, **properties):
        expression = mel.create_material_expression(self.material, expression_class, -280 * column, 110 * self.row)
        self.row += 1
        for name, value in properties.items():
            expression.set_editor_property(name, value)
        return expression

    def wire(self, source, target, target_input=""):
        if not mel.connect_material_expressions(source, "", target, target_input):
            raise RuntimeError("Could not connect %s -> %s.%s" % (source.get_name(), target.get_name(), target_input))

    def math(self, expression_class, column, a, b=None, const_b=None):
        properties = {} if const_b is None else {"const_b": const_b}
        expression = self.node(expression_class, column, **properties)
        self.wire(a, expression, "A")
        if b is not None:
            self.wire(b, expression, "B")
        return expression

    def mul(self, column, a, b=None, const=None):
        return self.math(unreal.MaterialExpressionMultiply, column, a, b, const)

    def add(self, column, a, b=None, const=None):
        return self.math(unreal.MaterialExpressionAdd, column, a, b, const)

    def sub(self, column, a, b=None, const=None):
        return self.math(unreal.MaterialExpressionSubtract, column, a, b, const)

    def single(self, expression_class, column, source, **properties):
        expression = self.node(expression_class, column, **properties)
        self.wire(source, expression)
        return expression

    def lerp(self, column, a, b, alpha):
        expression = self.node(unreal.MaterialExpressionLinearInterpolate, column)
        self.wire(a, expression, "A")
        self.wire(b, expression, "B")
        self.wire(alpha, expression, "Alpha")
        return expression

    def scalar(self, column, name, value):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=value)

    def vector(self, column, name, rgb):
        return self.node(unreal.MaterialExpressionVectorParameter, column, parameter_name=name, default_value=linear(rgb))

    def time(self, column):
        return self.node(unreal.MaterialExpressionTime, column)

    def node_smooth(self, column, low, high, value):
        """0 below low, 1 above high, smooth between."""
        expression = self.node(unreal.MaterialExpressionSmoothStep, column, const_min=low, const_max=high)
        self.wire(value, expression, "Value")
        return expression

    def half_wave(self, column, hertz):
        """0..1 at hertz: 0.5 + 0.5 sin(2 pi f t)."""
        wave = self.single(unreal.MaterialExpressionSine, column + 1, self.mul(column + 2, self.time(column + 3), hertz), period=1.0)
        return self.add(column, self.mul(column, wave, const=0.5), const=0.5)

    def steady(self, column):
        """1 - ReducedFlashes: every pulse holds still for a player who chose reduced flashes."""
        reduced = self.node(unreal.MaterialExpressionCollectionParameter, column + 1, collection=load(COLLECTION), parameter_name=REDUCED_FLASHES)
        return self.single(unreal.MaterialExpressionOneMinus, column, reduced)

    def out(self, source, prop):
        if not mel.connect_material_property(source, "", prop):
            raise RuntimeError("Could not connect %s of %s" % (prop, self.material.get_name()))


def build_surface(material):
    g = Graph(material)
    g.out(g.vector(1, "Color", (0.2, 0.2, 0.2)), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.scalar(1, "Metallic", 0.5), unreal.MaterialProperty.MP_METALLIC)
    g.out(g.scalar(1, "Roughness", 0.5), unreal.MaterialProperty.MP_ROUGHNESS)


def pulse(g, column):
    """1 - PulseDepth * wave * steady."""
    depth = g.mul(column + 1, g.mul(column + 2, g.half_wave(column + 3, g.scalar(column + 6, "PulseHz", 0.5)), g.scalar(column + 3, "PulseDepth", 0.3)), g.steady(column + 2))
    return g.single(unreal.MaterialExpressionOneMinus, column, depth)


def build_glow(material):
    g = Graph(material)
    swap = g.mul(4, g.half_wave(5, g.scalar(8, "SwapHz", 0.0)), g.steady(5))
    colour = g.lerp(3, g.vector(4, "GlowColor", (0.3, 0.8, 0.8)), g.vector(4, "GlowColorB", (0.3, 0.8, 0.8)), swap)
    glow = g.mul(1, g.mul(2, colour, g.scalar(3, "Strength", 5.0)), pulse(g, 2))
    g.out(g.vector(1, "Color", (0.02, 0.02, 0.02)), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(glow, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(g.scalar(1, "Roughness", 0.3), unreal.MaterialProperty.MP_ROUGHNESS)


def build_tool_glow(material):
    g = Graph(material)
    firing = g.node(unreal.MaterialExpressionScalarParameter, 4, parameter_name="FiringFlag", default_value=0.0,
                    use_custom_primitive_data=True, primitive_data_index=0)   # UHollowToolMountComponent::FiringDataIndex
    strength = g.add(2, g.scalar(3, "Idle", 0.4), g.mul(3, firing, g.scalar(4, "Firing", 30.0)))
    g.out(g.vector(1, "Color", (0.05, 0.05, 0.05)), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.mul(1, g.vector(2, "GlowColor", (1.0, 0.4, 0.1)), strength), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(g.scalar(1, "Roughness", 0.3), unreal.MaterialProperty.MP_ROUGHNESS)


def build_visor(material):
    g = Graph(material)
    tint = g.node(unreal.MaterialExpressionVectorParameter, 3, parameter_name="Tint", default_value=linear((0.6, 0.88, 1.0)),
                  use_custom_primitive_data=True, primitive_data_index=0)   # UHollowCosmeticPartsComponent::TintDataIndex
    fresnel = g.node(unreal.MaterialExpressionFresnel, 3, exponent=3.0, base_reflect_fraction=0.35)
    glow = g.mul(1, g.mul(2, tint, g.scalar(3, "Strength", 5.0)), fresnel)
    g.out(g.vector(1, "Color", (0.01, 0.012, 0.015)), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(glow, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(g.scalar(1, "Roughness", 0.05), unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.scalar(1, "Metallic", 0.0), unreal.MaterialProperty.MP_METALLIC)


def build_seam(material):
    """Red-hot right after a weld or a cut (HeatTime = that world time), cooling over CoolSeconds; a dull scar where welded."""
    g = Graph(material)
    since = g.sub(5, g.time(6), g.scalar(6, "HeatTime", -1000.0))
    cooled = g.single(unreal.MaterialExpressionSaturate, 3, g.math(unreal.MaterialExpressionDivide, 4, since, g.scalar(5, "CoolSeconds", 4.0)))
    heat = g.single(unreal.MaterialExpressionOneMinus, 2, cooled)
    hot = g.mul(1, g.mul(2, g.vector(3, "HotColor", (1.0, 0.3, 0.04)), g.scalar(3, "HotStrength", 25.0)), g.mul(2, heat, heat))
    scar = g.mul(1, g.mul(2, g.vector(3, "ScarColor", (0.3, 0.13, 0.05)), g.scalar(3, "ScarStrength", 0.8)), g.scalar(2, "Weld", 0.0))
    g.out(g.vector(1, "Color", (0.06, 0.05, 0.045)), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.add(0, hot, scar), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(g.scalar(1, "Roughness", 0.7), unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.scalar(1, "Metallic", 0.8), unreal.MaterialProperty.MP_METALLIC)


def build_hazard(material):
    g = Graph(material)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 6)
    u = g.node(unreal.MaterialExpressionComponentMask, 5, r=True, g=False, b=False, a=False)
    g.wire(uv, u)
    v = g.node(unreal.MaterialExpressionComponentMask, 5, r=False, g=True, b=False, a=False)
    g.wire(uv, v)
    band = g.single(unreal.MaterialExpressionFrac, 3, g.mul(4, g.add(5, u, v), g.scalar(5, "Stripes", 4.0)))
    stripe = g.single(unreal.MaterialExpressionRound, 2, band)
    g.out(g.lerp(1, g.vector(2, "ColorA", (0.86, 0.32, 0.03)), g.vector(2, "ColorB", (0.02, 0.02, 0.02)), stripe), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.scalar(1, "Roughness", 0.55), unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.scalar(1, "Metallic", 0.2), unreal.MaterialProperty.MP_METALLIC)


def build_screen(material):
    g = Graph(material)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 6)
    v = g.node(unreal.MaterialExpressionComponentMask, 5, r=False, g=True, b=False, a=False)
    g.wire(uv, v)
    lines = g.single(unreal.MaterialExpressionSine, 3, g.mul(4, v, g.scalar(5, "Lines", 120.0)), period=1.0)
    scan = g.add(2, g.mul(3, lines, const=0.12), const=0.88)
    glow = g.mul(1, g.mul(2, g.vector(3, "ScreenColor", (0.25, 0.85, 0.8)), g.scalar(3, "Strength", 2.5)), scan)
    g.out(g.vector(1, "Color", (0.0, 0.0, 0.0)), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(glow, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(g.scalar(1, "Roughness", 0.15), unreal.MaterialProperty.MP_ROUGHNESS)


def build_creature(material):
    """GP-02: one master for a Whisperer's parts and its nest. Surface colour; an emissive glow that flares with
    AHollowWhisperer::FlareDataIndex (steady for reduced flashes) and goes out with DeadDataIndex; and the scuttle: a
    vertex low in the mesh (a leg's tip) lifts by up to Swing cm at SwingHz, phased along the body, never under the floor,
    still once dead. A slot with Swing 0 (the shell, the eyes) keeps its shape."""
    g = Graph(material)
    flare = g.node(unreal.MaterialExpressionScalarParameter, 6, parameter_name="FlareFlag", default_value=0.0,
                   use_custom_primitive_data=True, primitive_data_index=0)   # AHollowWhisperer::FlareDataIndex
    dead = g.node(unreal.MaterialExpressionScalarParameter, 6, parameter_name="DeadFlag", default_value=0.0,
                  use_custom_primitive_data=True, primitive_data_index=1)    # AHollowWhisperer::DeadDataIndex
    alive = g.single(unreal.MaterialExpressionOneMinus, 5, dead)
    flaring = g.mul(4, g.mul(5, flare, g.scalar(6, "FlareStrength", 0.0)), g.steady(5))
    strength = g.mul(2, g.add(3, g.scalar(4, "GlowStrength", 0.0), flaring), alive)
    g.out(g.vector(1, "Color", (0.03, 0.03, 0.03)), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.scalar(1, "Metallic", 0.0), unreal.MaterialProperty.MP_METALLIC)
    g.out(g.scalar(1, "Roughness", 0.5), unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.mul(1, g.vector(2, "GlowColor", (0.6, 0.5, 1.0)), strength), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    local = g.node(unreal.MaterialExpressionLocalPosition, 9)
    along = g.node(unreal.MaterialExpressionComponentMask, 8, r=True, g=False, b=False, a=False)
    g.wire(local, along)
    across = g.node(unreal.MaterialExpressionComponentMask, 8, r=False, g=True, b=False, a=False)
    g.wire(local, across)
    height = g.node(unreal.MaterialExpressionComponentMask, 8, r=False, g=False, b=True, a=False)
    g.wire(local, height)
    phase = g.add(6, g.mul(7, g.time(8), g.scalar(8, "SwingHz", 3.0)), g.mul(7, g.add(8, along, across), const=0.04))
    lift = g.single(unreal.MaterialExpressionSaturate, 4, g.single(unreal.MaterialExpressionSine, 5, phase, period=1.0))
    tip = g.single(unreal.MaterialExpressionOneMinus, 4, g.single(unreal.MaterialExpressionSaturate, 5,
                   g.math(unreal.MaterialExpressionDivide, 6, height, g.scalar(7, "LegHeight", 18.0))))
    offset = g.mul(2, g.mul(3, g.mul(4, lift, tip), g.scalar(4, "Swing", 0.0)), alive)
    up = g.node(unreal.MaterialExpressionConstant3Vector, 2, constant=unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    g.out(g.mul(1, up, offset), unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)


MASTERS = {SURFACE_MASTER: build_surface, GLOW_MASTER: build_glow, TOOL_GLOW_MASTER: build_tool_glow, VISOR_MASTER: build_visor,
           SEAM_MASTER: build_seam, HAZARD_MASTER: build_hazard, SCREEN_MASTER: build_screen, CREATURE_MASTER: build_creature}


def ensure_materials():
    made = {}
    for path, builder in sorted(MASTERS.items()):
        material, created = existing_or_new(path, unreal.Material, unreal.MaterialFactoryNew())
        spec_hash = digest(MASTERS_VERSION, path, builder.__name__)
        if created or not up_to_date(material, spec_hash):
            builder(material)
            material.set_editor_property("used_with_static_lighting", False)
            material.set_editor_property("used_with_skeletal_mesh", True)   # the bodies wear them too (cooked games need the flag)
            mel.recompile_material(material)
            stamp(material, spec_hash)
            log("%s built (%d expressions)" % (path.rsplit("/", 1)[1], mel.get_num_material_expressions(material)))
        made[path] = material
    for name, (parent, values) in sorted(MATERIALS.items()):
        path = MAT_DIR + "/" + name
        instance, created = existing_or_new(path, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        spec_hash = digest(MASTERS_VERSION, parent, values)
        if created or not up_to_date(instance, spec_hash):
            instance.set_editor_property("parent", made[parent])
            for parameter, value in sorted(values.items()):
                if isinstance(value, tuple):
                    mel.set_material_instance_vector_parameter_value(instance, parameter, linear(value))
                else:
                    mel.set_material_instance_scalar_parameter_value(instance, parameter, value)
            mel.update_material_instance(instance)
            stamp(instance, spec_hash)
            log(name + " ready")
    return made


# ---------------------------------------------------------------- the meshes

def metrics():
    m = unreal.get_default_object(unreal.HollowLevelMetrics)
    return {"door_width": m.get_editor_property("door_width"), "door_height": m.get_editor_property("door_height")}


def mesh_specs():
    m = metrics()
    specs = {}
    kit = file_digest(BLENDER_SCRIPT)
    # The combat kit reads the art kit's primitives: its meshes follow both scripts; the art kit's only its own.
    scripts = {"kit": kit, "combat": file_digest(COMBAT_SCRIPT) + kit, "creature": file_digest(CREATURE_SCRIPT) + kit}
    for name, entry in MESHES.items():
        spec = dict(entry)
        if spec["kind"] == "door_leaf":
            spec["size"] = [LEAF_THICKNESS, m["door_width"] / 100.0, m["door_height"] / 100.0]
        spec["hash"] = digest(KIT_VERSION, name, spec, scripts[spec.get("script", "kit")], MESH_SLOTS.get(name, {}), SLOT_MATERIALS, name in COLLIDING)
        specs[name] = spec
    return specs


def find_blender():
    for candidate in BLENDER:
        if candidate and os.path.isfile(candidate):
            return candidate
    return None


def run_blender(specs, out_dir, script=BLENDER_SCRIPT):
    blender = find_blender()
    if blender is None:
        raise RuntimeError("Blender 5.2 is needed to rebuild %s (set HOLLOW_BLENDER)" % ", ".join(sorted(specs)))
    if os.path.isdir(out_dir):
        shutil.rmtree(out_dir)
    os.makedirs(out_dir)
    spec_path = os.path.join(out_dir, "kit_spec.json")
    with open(spec_path, "w") as f:
        json.dump({"meshes": specs}, f, sort_keys=True)
    result = subprocess.run([blender, "-b", "--factory-startup", "--python", script, "--", spec_path, out_dir],
                            capture_output=True, text=True, timeout=BLENDER_TIMEOUT)
    if "HOLLOW_KIT_OK" not in result.stdout:
        raise RuntimeError("Blender failed:\n" + result.stdout[-4000:] + result.stderr[-2000:])
    with open(os.path.join(out_dir, "kit_report.json")) as f:
        return json.load(f)


def import_fbx(name, path):
    """The legacy FBX importer (its options are explicit), no materials or textures from the file."""
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = options.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("generate_lightmap_u_vs", False)
    data.set_editor_property("remove_degenerates", True)
    data.set_editor_property("build_nanite", False)
    data.set_editor_property("import_uniform_scale", FBX_SCALE)   # Blender's metres arrive as the engine's units
    existing = unreal.load_asset(KIT_DIR + "/" + name) if assets.does_asset_exist(KIT_DIR + "/" + name) else None
    if existing is not None:
        # A re-import keeps the asset's own import settings over the task's: give them the same scale.
        existing_data = existing.get_editor_property("asset_import_data")
        if isinstance(existing_data, unreal.FbxStaticMeshImportData):
            existing_data.set_editor_property("import_uniform_scale", FBX_SCALE)
            existing_data.set_editor_property("combine_meshes", True)
            existing_data.set_editor_property("auto_generate_collision", False)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", KIT_DIR)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    task.set_editor_property("options", options)
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset(KIT_DIR + "/" + name)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Could not import " + path)
    return mesh


def slot_names(mesh):
    return [str(m.get_editor_property("material_slot_name")) for m in mesh.get_editor_property("static_materials")]


def dress_mesh(name, mesh):
    for index, slot in enumerate(slot_names(mesh)):
        material = MESH_SLOTS.get(name, {}).get(slot, SLOT_MATERIALS.get(slot))
        if material is None:
            raise RuntimeError("%s: no material for slot %s" % (name, slot))
        mesh.set_material(index, load(material_path(material)))
    meshes.remove_collisions(mesh)
    if name in COLLIDING:
        meshes.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
    else:   # looks only: nothing collides with it by default (prepare_fab_assets.py asks the same of Fab props)
        body = mesh.get_editor_property("body_setup")
        instance = body.get_editor_property("default_instance")
        instance.set_editor_property("collision_profile_name", "NoCollision")
        body.set_editor_property("default_instance", instance)


def ensure_meshes():
    specs = mesh_specs()
    stale = {name: spec for name, spec in specs.items()
             if not assets.does_asset_exist(KIT_DIR + "/" + name) or not up_to_date(load(KIT_DIR + "/" + name), spec["hash"])}
    if not stale:
        return
    for kind, script in sorted(SCRIPTS.items()):
        batch = {name: spec for name, spec in stale.items() if spec.get("script", "kit") == kind}
        if not batch:
            continue
        out_dir = os.path.join(WORK, "Kit" if kind == "kit" else "Kit_" + kind)
        report = run_blender(batch, out_dir, script)
        for name in sorted(batch):
            mesh = import_fbx(name, os.path.join(out_dir, name + ".fbx"))
            dress_mesh(name, mesh)
            stamp(mesh, batch[name]["hash"])
            log("%s built (%d triangles, slots %s)" % (name, report[name]["triangles"], ", ".join(slot_names(mesh))))


# ---------------------------------------------------------------- CC0 textures (Build/Art/cc0_textures.json)

TEXTURE_KINDS = {   # map -> (compression, sRGB, LOD group)
    "D": ("TC_DEFAULT", True, "TEXTUREGROUP_WORLD"),
    "N": ("TC_NORMALMAP", False, "TEXTUREGROUP_WORLD_NORMAL_MAP"),
    "ARM": ("TC_MASKS", False, "TEXTUREGROUP_WORLD"),
}
TEXTURE_MAX_SIZE = 1024


def texture_manifest():
    with open(CC0_MANIFEST) as f:
        return json.load(f)["textures"]


def ensure_textures():
    for entry in texture_manifest():
        for kind, spec in sorted(entry["maps"].items()):
            name = "%s_%s" % (entry["name"], kind)
            path = TEX_DIR + "/" + name
            compression, srgb, group = TEXTURE_KINDS[kind]
            spec_hash = digest(spec["sha256"], compression, srgb, group, TEXTURE_MAX_SIZE)
            if assets.does_asset_exist(path) and up_to_date(load(path), spec_hash):
                continue
            source = os.path.join(WORK, "cc0", entry["id"], spec["url"].rsplit("/", 1)[1])
            if not os.path.isfile(source) or hashlib.sha256(open(source, "rb").read()).hexdigest() != spec["sha256"]:
                raise RuntimeError("%s is missing or changed: run Build/Scripts/fetch-cc0-textures.sh" % source)
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", source)
            task.set_editor_property("destination_path", TEX_DIR)
            task.set_editor_property("destination_name", name)
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("automated", True)
            task.set_editor_property("save", False)
            tools.import_asset_tasks([task])
            texture = load(path)
            texture.set_editor_property("compression_settings", getattr(unreal.TextureCompressionSettings, compression))
            texture.set_editor_property("srgb", srgb)
            texture.set_editor_property("lod_group", getattr(unreal.TextureGroup, group))
            texture.set_editor_property("max_texture_size", TEXTURE_MAX_SIZE)
            stamp(texture, spec_hash)
            log("%s imported (%s)" % (name, entry["id"]))


# ---------------------------------------------------------------- the beam's sparks

def build_spark_material(material):
    """Additive, unlit: a thin streak along the sprite's length (the sprite flies along its velocity), white-hot inside."""
    g = Graph(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("used_with_niagara_sprites", True)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 8)
    u = g.node(unreal.MaterialExpressionComponentMask, 7, r=True, g=False, b=False, a=False)
    g.wire(uv, u)
    v = g.node(unreal.MaterialExpressionComponentMask, 7, r=False, g=True, b=False, a=False)
    g.wire(uv, v)
    across = g.single(unreal.MaterialExpressionAbs, 6, g.sub(7, u, const=0.5))
    line = g.single(unreal.MaterialExpressionOneMinus, 4, g.node_smooth(5, SPARK_STREAK[0], SPARK_STREAK[1], across))
    taper = g.mul(4, g.node_smooth(5, 0.0, 0.3, v), g.single(unreal.MaterialExpressionOneMinus, 5, g.node_smooth(6, 0.55, 1.0, v)))
    glow = g.mul(3, line, taper)
    core = g.mul(2, g.mul(3, glow, glow), g.mul(3, glow, g.scalar(4, "CoreBoost", SPARK_STREAK[2])))
    colour = g.node(unreal.MaterialExpressionComponentMask, 3, r=True, g=True, b=True, a=False)
    g.wire(g.node(unreal.MaterialExpressionParticleColor, 4), colour)
    emissive = g.add(1, g.mul(2, colour, glow), core)
    g.out(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def ensure_sparks():
    """AK-8: the beam's sparks, built from SPARKS (rebuilt in place when the spec changes; the beam cue keeps its reference)."""
    material, created = existing_or_new(SPARK_MATERIAL, unreal.Material, unreal.MaterialFactoryNew())
    material_hash = digest(MASTERS_VERSION, "spark", SPARK_STREAK)
    if created or not up_to_date(material, material_hash):
        build_spark_material(material)
        mel.recompile_material(material)
        stamp(material, material_hash)
        log("M_Kit_Spark built")
    sparks, created = existing_or_new(SPARKS_PATH, unreal.NiagaraSystem, unreal.NiagaraSystemFactoryNew())
    spec_hash = digest(SPARKS, material_hash)
    if not created and up_to_date(sparks, spec_hash):
        return sparks
    authoring = unreal.HollowNiagaraAuthoring
    modules = [load(path) for path in SPARKS["modules"]]
    if not authoring.build_single_emitter_system(sparks, load(SPARKS["emitter"]), "Sparks", modules):
        raise RuntimeError("Could not build " + SPARKS_PATH)
    for suffix, value in SPARKS["constants"]:
        check(authoring.set_constant(sparks, suffix, value) >= 1, "%s has the constant %s" % (SPARKS_PATH, suffix))
    check(authoring.set_sprite_renderer(sparks, material, True) == 1, SPARKS_PATH + " has one sprite renderer")
    check(authoring.compile(sparks), SPARKS_PATH + " compiles")
    stamp(sparks, spec_hash)
    log("NS_Tool_BeamSparks: Fountain + Collision, %d constants" % len(SPARKS["constants"]))
    return sparks


# ---------------------------------------------------------------- parts on the placeholder skeleton (ADR-0011)

PLACEHOLDER = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
BONES = ["head", "spine_05", "spine_03", "upperarm_l", "upperarm_r", "lowerarm_l", "lowerarm_r", "hand_l", "hand_r", "calf_l", "calf_r",
         "foot_l", "foot_r", "index_01_r", "middle_01_r", "pinky_01_r"]
# Each part where it sits in the mannequin's component space (cm; +Y is its front, +X its left, +Z up): bone, location,
# rotation (pitch, yaw, roll), scale, mesh (the right side mirrors the left with a scale of -1 across X).
FORE = "fore"   # the location is the middle of the forearm, the part along it
SHIN = "shin"
EXO_PARTS = [
    ("SM_Exo_Helmet", "Body", "head", (0.0, 1.5, 170.0), (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)),
    ("SM_Exo_Chest", "Body", "spine_05", (0.0, 4.0, 131.0), (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)),
    ("SM_Exo_Pack", "Body", "spine_05", (0.0, -4.0, 128.0), (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)),
    ("SM_Exo_Pauldron", "Body", "upperarm_l", (21.0, -1.0, 146.0), (0.0, 0.0, -20.0), (1.0, 1.0, 1.0)),
    ("SM_Exo_Pauldron", "Body", "upperarm_r", (-21.0, -1.0, 146.0), (0.0, 0.0, 20.0), (-1.0, 1.0, 1.0)),
    ("SM_Exo_Bracer", "Body", "lowerarm_l", FORE, None, (1.0, 1.0, 1.0)),
    ("SM_Exo_Bracer", "Body", "lowerarm_r", FORE, None, (1.0, 1.0, 1.0)),
    ("SM_Exo_Bracer", "FirstPerson", "lowerarm_l", FORE, None, (1.0, 1.0, 1.0)),
    ("SM_Exo_Bracer", "FirstPerson", "lowerarm_r", FORE, None, (1.0, 1.0, 1.0)),
    ("SM_Exo_Shin", "Body", "calf_l", SHIN, None, (1.0, 1.0, 1.0)),
    ("SM_Exo_Shin", "Body", "calf_r", SHIN, None, (1.0, 1.0, 1.0)),
]
PILGRIM_PARTS = [
    ("SM_Pilgrim_Helm", "Body", "head", (0.0, 2.0, 171.0), (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)),
    ("SM_Pilgrim_Chest", "Body", "spine_05", (0.0, 5.0, 132.0), (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)),
    ("SM_Pilgrim_Pauldron", "Body", "upperarm_l", (22.0, -1.0, 149.0), (0.0, 0.0, -15.0), (1.0, 1.0, 1.0)),
    ("SM_Pilgrim_Pauldron", "Body", "upperarm_r", (-22.0, -1.0, 149.0), (0.0, 0.0, 15.0), (-1.0, 1.0, 1.0)),
    ("SM_Pilgrim_Gauntlet", "Body", "lowerarm_l", FORE, None, (1.0, 1.0, 1.0)),
    ("SM_Pilgrim_Gauntlet", "Body", "lowerarm_r", FORE, None, (1.0, 1.0, 1.0)),
]
STALKER_PARTS = [
    ("SM_Stalker_Crest", "Body", "head", (0.0, -2.0, 174.0), (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)),
    ("SM_Stalker_Eyes", "Body", "head", (0.0, 0.7, 167.0), (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)),
    ("SM_Stalker_Spines", "Body", "spine_05", (0.0, -10.0, 128.0), (0.0, 0.0, 0.0), (1.0, 1.0, 1.0)),
    ("SM_Stalker_Blade", "Body", "lowerarm_l", FORE, None, (1.0, 1.0, 1.0)),
    ("SM_Stalker_Blade", "Body", "lowerarm_r", FORE, None, (1.0, 1.0, 1.0)),
]
PART_SETS = {"DA_Parts_ExoFrame": EXO_PARTS, "DA_Parts_IronPilgrim": PILGRIM_PARTS, "DA_Parts_EchoStalker": STALKER_PARTS}
CHARACTERS = {   # blueprint, its part set, the body's materials per slot (the placeholder has two)
    "/Game/Hollow/Characters/ExoFrame/BP_ExoFrame": ("DA_Parts_ExoFrame", ["MI_Kit_Rubber", "MI_Kit_ExoArmour"]),
    "/Game/Hollow/Characters/Enemies/IronPilgrim/BP_IronPilgrim": ("DA_Parts_IronPilgrim", ["MI_Kit_PilgrimIron", "MI_Kit_PilgrimIron"]),
    "/Game/Hollow/Characters/Enemies/EchoStalker/BP_EchoStalker": ("DA_Parts_EchoStalker", ["MI_Kit_Obsidian", "MI_Kit_Obsidian"]),
}


def bone_frames():
    """Component-space transforms of BONES in the placeholder's reference pose (a mesh actor in the editor's world)."""
    mesh = unreal.load_asset(PLACEHOLDER)
    if mesh is None:
        raise RuntimeError("The placeholder mannequin is missing: run Build/Scripts/import-placeholders.sh (ADR-0011)")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0.0, 0.0, 0.0))
    component = actor.get_editor_property("skeletal_mesh_component")
    component.set_skinned_asset_and_update(mesh)
    frames = {b: component.get_socket_transform(b, unreal.RelativeTransformSpace.RTS_COMPONENT) for b in BONES}
    actors.destroy_actor(actor)
    return frames


def vec(t):
    return unreal.Vector(t[0], t[1], t[2])


def part_transform(frames, bone, location, rotation, scale):
    """The part's transform relative to its bone, from where it sits in component space."""
    if location == FORE:
        side = bone[-1]
        start, end = frames["lowerarm_" + side].translation, frames["hand_" + side].translation
        middle = (start + end) * 0.5
        component = unreal.Transform(middle, unreal.MathLibrary.make_rot_from_x(end - start), vec(scale))
    elif location == SHIN:
        side = bone[-1]
        start, end = frames["calf_" + side].translation, frames["foot_" + side].translation
        middle = (start + end) * 0.5 + unreal.Vector(0.0, 3.0, 0.0)
        component = unreal.Transform(middle, unreal.Rotator(0.0, 0.0, 0.0), vec(scale))
    else:
        component = unreal.Transform(vec(location), unreal.Rotator(rotation[2], rotation[0], rotation[1]), vec(scale))
    return unreal.MathLibrary.make_relative_transform(component, frames[bone])


def ensure_part_sets(frames_cache):
    sets = {}
    for name, parts in sorted(PART_SETS.items()):
        path = PARTS_DIR + "/" + name
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.HollowCosmeticPartSet)
        part_set, created = existing_or_new(path, unreal.HollowCosmeticPartSet, factory)
        # Only its own meshes' slots: a mesh added to the kit later (GP-02) leaves the set as it is.
        spec_hash = digest(KIT_VERSION, parts, {mesh: MESH_SLOTS.get(mesh, {}) for mesh, _, _, _, _, _ in parts}, SLOT_MATERIALS)
        if created or not up_to_date(part_set, spec_hash):
            if not frames_cache:
                frames_cache.update(bone_frames())
            entries = []
            for mesh, on, bone, location, rotation, scale in parts:
                entry = unreal.HollowCosmeticPart()
                entry.set_editor_property("mesh", load(KIT_DIR + "/" + mesh))
                entry.set_editor_property("on", unreal.HollowPartMesh.FIRST_PERSON if on == "FirstPerson" else unreal.HollowPartMesh.BODY)
                entry.set_editor_property("bone", bone)
                entry.set_editor_property("offset", part_transform(frames_cache, bone, location, rotation, scale))
                entries.append(entry)
            part_set.set_editor_property("parts", entries)
            stamp(part_set, spec_hash)
            log("%s ready (%d parts)" % (name, len(entries)))
        sets[name] = part_set
    return sets


# ---------------------------------------------------------------- bindings

TOOLS = {   # tool data -> its held mesh
    "/Game/Hollow/Tools/PlasmaCutter/DA_Tool_PlasmaCutter": "SM_Tool_PlasmaCutter",
    "/Game/Hollow/Tools/Welder/DA_Tool_Welder": "SM_Tool_Welder",
    "/Game/Hollow/Tools/AetherLantern/DA_Tool_AetherLantern": "SM_Tool_AetherLantern",
}
GRIP = (-6.0, 0.0, -8.5)       # the tools' grip in their own space (cm, Build/Blender/hollow_kit.py tool_body)
WEAPONS = {   # GP-01: weapon data -> its held mesh (the same fields as a tool's) and its shot effects
    "/Game/Hollow/Weapons/Nailgun/DA_Weapon_Nailgun": "SM_Weapon_Nailgun",
    "/Game/Hollow/Weapons/Shotgun/DA_Weapon_Shotgun": "SM_Weapon_Shotgun",
}
PICKUPS = {   # GP-01b: item -> how it lies on the floor (UHollowItemDefinition::PickupMesh)
    "/Game/Hollow/Items/Materials/DA_Item_Scrap": "SM_Pickup_Scrap",
    "/Game/Hollow/Items/Materials/DA_Item_AetherShard": "SM_Pickup_Shard",
    "/Game/Hollow/Items/Consumables/DA_Item_FuelCell": "SM_Pickup_FuelCell",
    "/Game/Hollow/Items/Ammo/DA_Item_Ammo_Nails": "SM_Pickup_Nails",
    "/Game/Hollow/Items/Ammo/DA_Item_Ammo_Shells": "SM_Pickup_Shells",
}
CLASS_TOOLS = {"/Game/Hollow/Classes/DA_Class_Engineer": "/Game/Hollow/Tools/PlasmaCutter/DA_Tool_PlasmaCutter",
               "/Game/Hollow/Classes/DA_Class_Occultist": "/Game/Hollow/Tools/AetherLantern/DA_Tool_AetherLantern"}
CORE_BP = "/Game/Hollow/Items/AetherCore/BP_AetherCore"
DOOR_BP = "/Game/Hollow/World/Kit/Doors/BP_Door"
TERMINAL_BP = "/Game/Hollow/Hub/BP_HubTerminal"
BEAM_CUE = "/Game/Hollow/GAS/Cues/GCN_Tool_Beam"
SEAM = {"seam_slot": "Seam", "seam_weld_parameter": "Weld", "seam_heat_parameter": "HeatTime"}
# The first-person tool in the camera's space: forward, right, down; turned a little in, toward the crosshair.
VIEW_OFFSET = unreal.Transform(unreal.Vector(52.0, 17.0, -20.0), unreal.Rotator(0.0, 3.0, -4.0), unreal.Vector(0.85, 0.85, 0.85))


def held_transform(frames):
    """A natural pistol grip: the barrel along the fingers, the grip across the palm, the grip at the palm's middle."""
    hand = frames["hand_r"]
    fingers = (frames["middle_01_r"].translation - hand.translation)
    across = (frames["index_01_r"].translation - frames["pinky_01_r"].translation)
    forward = fingers.normal()
    up = (across - forward * across.dot(forward)).normal()
    rotation = unreal.MathLibrary.make_rot_from_xz(forward, up)
    palm = hand.translation + fingers * 0.5
    grip = unreal.MathLibrary.greater_greater_vector_rotator(vec(GRIP), rotation)   # the grip rotated into component space
    component = unreal.Transform(palm - grip, rotation, unreal.Vector(1.0, 1.0, 1.0))
    return unreal.MathLibrary.make_relative_transform(component, hand)


def same_transform(a, b, tolerance=0.01):
    return ((a.translation - b.translation).length() <= tolerance and (a.scale3d - b.scale3d).length() <= tolerance
            and abs(abs(a.rotation.x * b.rotation.x + a.rotation.y * b.rotation.y + a.rotation.z * b.rotation.z + a.rotation.w * b.rotation.w) - 1.0) <= 1e-5)


def ensure_tools(frames_cache):
    if not frames_cache:
        frames_cache.update(bone_frames())
    held = held_transform(frames_cache)
    for path, mesh_name in sorted(TOOLS.items()):
        tool = load(path)
        mesh = load(KIT_DIR + "/" + mesh_name)
        if tool.get_editor_property("held_mesh") != mesh or not same_transform(tool.get_editor_property("held_transform"), held) \
                or str(tool.get_editor_property("muzzle_socket")) != "Muzzle":
            tool.set_editor_property("held_mesh", mesh)
            tool.set_editor_property("held_transform", held)
            tool.set_editor_property("muzzle_socket", "Muzzle")
            save(tool)
            log("%s holds %s" % (path.rsplit("/", 1)[1], mesh_name))
    for path, tool_path in sorted(CLASS_TOOLS.items()):
        definition = load(path)
        if definition.get_editor_property("held_tool") != load(tool_path):
            definition.set_editor_property("held_tool", load(tool_path))
            save(definition)
            log("%s holds %s" % (path.rsplit("/", 1)[1], tool_path.rsplit("/", 1)[1]))


def ensure_weapon_fx():
    """GP-01: the shot's three bursts, rebuilt in place when their spec changes (the weapons keep their references)."""
    material = load(SPARK_MATERIAL)
    authoring = unreal.HollowNiagaraAuthoring
    systems = {}
    for name, spec in sorted(WEAPON_FX.items()):
        path = FX_DIR + "/" + name
        system, created = existing_or_new(path, unreal.NiagaraSystem, unreal.NiagaraSystemFactoryNew())
        spec_hash = digest(spec, BURST_EMITTER, digest(MASTERS_VERSION, "spark", SPARK_STREAK))
        systems[name] = system
        if not created and up_to_date(system, spec_hash):
            continue
        if not authoring.build_single_emitter_system(system, load(BURST_EMITTER), "Burst", [load(m) for m in spec["modules"]]):
            raise RuntimeError("Could not build " + path)
        for suffix, value in spec["constants"]:
            check(authoring.set_constant(system, suffix, value) >= 1, "%s has the constant %s" % (path, suffix))
        check(authoring.set_sprite_renderer(system, material, True) == 1, path + " has one sprite renderer")
        check(authoring.compile(system), path + " compiles")
        stamp(system, spec_hash)
        log("%s built (%d constants)" % (name, len(spec["constants"])))
    return systems


def ensure_weapons(frames_cache, systems):
    if not frames_cache:
        frames_cache.update(bone_frames())
    held = held_transform(frames_cache)
    for path, mesh_name in sorted(WEAPONS.items()):
        weapon = load(path)
        mesh = load(KIT_DIR + "/" + mesh_name)
        changed = False
        if weapon.get_editor_property("held_mesh") != mesh or not same_transform(weapon.get_editor_property("held_transform"), held) \
                or str(weapon.get_editor_property("muzzle_socket")) != "Muzzle" or not same_transform(weapon.get_editor_property("view_offset"), VIEW_OFFSET):
            weapon.set_editor_property("held_mesh", mesh)
            weapon.set_editor_property("held_transform", held)
            weapon.set_editor_property("muzzle_socket", "Muzzle")
            weapon.set_editor_property("view_offset", VIEW_OFFSET)   # where the tools ride the camera (ADR-0043)
            changed = True
        for slot, system_name in sorted(WEAPON_FX_SLOTS.items()):
            if weapon.get_editor_property(slot) != systems[system_name]:
                weapon.set_editor_property(slot, systems[system_name])
                changed = True
        if changed:
            save(weapon)
            log("%s holds %s, its shots %s" % (path.rsplit("/", 1)[1], mesh_name, ", ".join(sorted(WEAPON_FX_SLOTS.values()))))


def ensure_pickups():
    for path, mesh_name in sorted(PICKUPS.items()):
        item = load(path)
        mesh = load(KIT_DIR + "/" + mesh_name)
        if item.get_editor_property("pickup_mesh") != mesh:
            item.set_editor_property("pickup_mesh", mesh)
            save(item)
            log("%s lies on the floor as %s" % (path.rsplit("/", 1)[1], mesh_name))


def ensure_mesh_component(component, mesh, materials, extra=None):
    """The component shows mesh at scale 1 with these slot materials; True if it changed."""
    changed = False
    if component.get_editor_property("static_mesh") != mesh:
        component.set_editor_property("static_mesh", mesh)
        changed = True
    if (component.get_editor_property("relative_scale3d") - unreal.Vector(1.0, 1.0, 1.0)).length() > 1e-4:
        component.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
        changed = True
    for index, material in enumerate(materials):
        if material is not None and component.get_material(index) != material:
            component.set_material(index, material)
            changed = True
    for name, value in (extra or {}).items():
        if component.get_editor_property(name) != value:
            component.set_editor_property(name, value)
            changed = True
    return changed


def kit_materials(mesh_name):
    mesh = load(KIT_DIR + "/" + mesh_name)
    return [m.get_editor_property("material_interface") for m in mesh.get_editor_property("static_materials")]


def ensure_blueprints(sets, sparks):
    # The Aether core: the container round its body; slot 0 (the crystal) keeps the item's look the actor sets.
    core_bp = load(CORE_BP)
    core = unreal.get_default_object(core_bp.generated_class())
    materials = kit_materials("SM_AetherCore")
    materials[0] = core.get_editor_property("visual").get_material(0)
    if ensure_mesh_component(core.get_editor_property("visual"), load(KIT_DIR + "/SM_AetherCore"), materials):
        save(core_bp)
        log("BP_AetherCore: the container")
    # The door: the kit's leaf, its seam slot heated by the weld.
    door_bp = load(DOOR_BP)
    door = unreal.get_default_object(door_bp.generated_class())
    changed = ensure_mesh_component(door.get_editor_property("leaf"), load(KIT_DIR + "/SM_Door_Leaf"), kit_materials("SM_Door_Leaf"))
    for name, value in SEAM.items():
        if str(door.get_editor_property(name)) != value:
            door.set_editor_property(name, value)
            changed = True
    if changed:
        save(door_bp)
        log("BP_Door: the leaf with its weld seam")
    # The hub's terminals: the console as the cabinet (it keeps the focus and the block); the old screen goes.
    terminal_bp = load(TERMINAL_BP)
    terminal = unreal.get_default_object(terminal_bp.generated_class())
    changed = ensure_mesh_component(terminal.get_editor_property("cabinet"), load(KIT_DIR + "/SM_Hub_Console"), kit_materials("SM_Hub_Console"),
                                    {"relative_location": unreal.Vector(0.0, 0.0, 0.0), "relative_rotation": unreal.Rotator(0.0, 0.0, 180.0)})
    screen = terminal.get_editor_property("screen")
    if screen.get_editor_property("visible"):
        screen.set_editor_property("visible", False)
        changed = True
    if changed:
        save(terminal_bp)
        log("BP_HubTerminal: the console")
    # The beam's sparks.
    cue_bp = load(BEAM_CUE)
    cue = unreal.get_default_object(cue_bp.generated_class())
    component = cue.get_editor_property("sparks")
    if component.get_editor_property("asset") != sparks:
        component.set_editor_property("asset", sparks)
        save(cue_bp)
        log("GCN_Tool_Beam: sparks")
    lantern_bp = load(LANTERN_CUE)
    lantern = unreal.get_default_object(lantern_bp.generated_class())
    if any(abs(lantern.get_editor_property(k) - v) > 1e-4 for k, v in LANTERN_FLASH.items()):
        for name, value in LANTERN_FLASH.items():
            lantern.set_editor_property(name, value)
        save(lantern_bp)
        log("GCN_Tool_Lantern: flares x%.1f over %.2f s" % (LANTERN_FLASH["flash_intensity_scale"], LANTERN_FLASH["flash_seconds"]))
    # The characters: their part sets and body materials (the placeholder skeleton stays, ADR-0011).
    for path, (set_name, body_materials) in sorted(CHARACTERS.items()):
        blueprint = load(path)
        character = unreal.get_default_object(blueprint.generated_class())
        changed = False
        parts = character.get_editor_property("cosmetic_parts")
        if parts.get_editor_property("part_set") != sets[set_name]:
            parts.set_editor_property("part_set", sets[set_name])
            changed = True
        meshes_to_dress = [character.get_editor_property("mesh")]
        if path.endswith("BP_ExoFrame"):
            meshes_to_dress.append(character.get_editor_property("first_person_mesh"))
            # The first-person tool rides the camera (the placeholder arms hold nothing up): in view, lower right.
            mount = character.get_editor_property("tool_mount")
            if not mount.get_editor_property("first_person_on_camera") \
                    or not same_transform(mount.get_editor_property("first_person_view_offset"), VIEW_OFFSET):
                mount.set_editor_property("first_person_on_camera", True)
                mount.set_editor_property("first_person_view_offset", VIEW_OFFSET)
                changed = True
        for skeletal in meshes_to_dress:
            asset = skeletal.get_editor_property("skeletal_mesh_asset")
            slots = len(asset.get_editor_property("materials")) if asset else 0   # a template component has no render state to ask
            for index, name in enumerate(body_materials[:slots]):
                material = load(MAT_DIR + "/" + name)
                if skeletal.get_material(index) != material:
                    skeletal.set_material(index, material)
                    changed = True
        if changed:
            save(blueprint)
            log("%s: %s and its body's materials" % (path.rsplit("/", 1)[1], set_name))


# ---------------------------------------------------------------- the hub's dressing (L_Hub_Skiff, create_hub_assets.py)

HUB_MAP = "/Game/Hollow/Maps/L_Hub_Skiff"
HUB_TAG = "HollowArtKit"   # actors this script owns in the hub (create_hub_assets.py owns its own tag)
HUB_FOLDER = "Dressing/ArtKit"
# The cabin: X from the ramp (0) to the cockpit (1600), Y across (0..800), Z up; looks only, no collision.
HUB_DRESSING = [   # (label, mesh, location, yaw)
    ("PilotSeat", "SM_Hub_PilotSeat", (1450.0, 400.0, 0.0), 180.0),
    ("Crate_Ramp_Port", "SM_Hub_Crate", (170.0, 95.0, 0.0), 8.0),
    ("Crate_Ramp_Port_Top", "SM_Hub_Crate", (175.0, 95.0, 60.0), -6.0),
    ("Crate_Ramp_Starboard", "SM_Hub_Crate", (190.0, 705.0, 0.0), -10.0),
    ("Crate_Fore_Port", "SM_Hub_Crate", (1240.0, 90.0, 0.0), 90.0),
    ("Pipes_Port_Aft", "SM_Hub_PipeRun", (520.0, 45.0, 355.0), 0.0),
    ("Pipes_Port_Fore", "SM_Hub_PipeRun", (1080.0, 45.0, 355.0), 0.0),
    ("Pipes_Starboard_Aft", "SM_Hub_PipeRun", (520.0, 755.0, 355.0), 180.0),
    ("Pipes_Starboard_Fore", "SM_Hub_PipeRun", (1080.0, 755.0, 355.0), 180.0),
]
HUB_VERSION = 2
LABEL_HEIGHT = 175.0
LABEL_SIZE = 18.0
HUB_LABELS = [   # (label, text, the terminal's location and yaw: its X axis points at its user, create_hub_assets.py)
    ("Label_Contracts", "CONTRACTS", (1490.0, 400.0, 0.0), 180.0, (255, 184, 90)),
    ("Label_Workbench", "WORKBENCH", (800.0, 765.0, 0.0), -90.0, (150, 225, 255)),
    ("Label_Locker", "LOCKER", (800.0, 35.0, 0.0), 90.0, (150, 225, 255)),
]


def tags_of(actor):
    return [str(t) for t in actor.get_editor_property("tags")]


def ensure_hub_dressing():
    wanted = digest(HUB_VERSION, HUB_DRESSING, HUB_LABELS, LABEL_HEIGHT, LABEL_SIZE)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not levels.load_level(HUB_MAP):
        raise RuntimeError("Could not load " + HUB_MAP)
    owned = [a for a in actors.get_all_level_actors() if HUB_TAG in tags_of(a)]
    if owned and all(wanted in tags_of(a) for a in owned) and len(owned) == len(HUB_DRESSING) + len(HUB_LABELS):
        return
    for actor in owned:
        actors.destroy_actor(actor)
    for label, mesh_name, location, yaw in HUB_DRESSING:
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, vec(location), unreal.Rotator(0.0, 0.0, yaw))
        component = actor.get_editor_property("static_mesh_component")
        component.set_editor_property("static_mesh", load(KIT_DIR + "/" + mesh_name))
        component.set_collision_profile_name("NoCollision")
        component.set_editor_property("can_ever_affect_navigation", False)
        actor.set_actor_label(label)
        actor.set_folder_path(HUB_FOLDER)
        actor.set_editor_property("tags", [HUB_TAG, wanted])
    for label, text, location, yaw, rgb in HUB_LABELS:
        place = vec(location) + unreal.Vector(0.0, 0.0, LABEL_HEIGHT)
        actor = actors.spawn_actor_from_class(unreal.TextRenderActor, place, unreal.Rotator(0.0, 0.0, yaw))
        component = actor.get_editor_property("text_render")
        component.set_editor_property("text", unreal.Text(text))
        component.set_editor_property("world_size", LABEL_SIZE)
        component.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
        component.set_editor_property("text_render_color", unreal.Color(r=rgb[0], g=rgb[1], b=rgb[2], a=255))
        actor.set_actor_label(label)
        actor.set_folder_path(HUB_FOLDER)
        actor.set_editor_property("tags", [HUB_TAG, wanted])
    if not levels.save_current_level():
        raise RuntimeError("Could not save " + HUB_MAP)
    log("L_Hub_Skiff: %d pieces and %d labels" % (len(HUB_DRESSING), len(HUB_LABELS)))


# ---------------------------------------------------------------- main

def validate():
    for name, spec in mesh_specs().items():
        mesh = load(KIT_DIR + "/" + name)
        check(up_to_date(mesh, spec["hash"]), name + " matches its spec")
        check(mesh.get_num_triangles(0) <= MAX_TRIANGLES, "%s under %d triangles" % (name, MAX_TRIANGLES))
        check(all(m.get_editor_property("material_interface") is not None for m in mesh.get_editor_property("static_materials")),
              name + " has a material on every slot")
    for name in ("SM_Tool_PlasmaCutter", "SM_Tool_Welder", "SM_Tool_AetherLantern", "SM_Weapon_Nailgun", "SM_Weapon_Shotgun"):
        check(load(KIT_DIR + "/" + name).find_socket("Muzzle") is not None, name + " has its Muzzle socket")
    for path, mesh_name in PICKUPS.items():
        check(load(path).get_editor_property("pickup_mesh") == load(KIT_DIR + "/" + mesh_name), path + " lies as " + mesh_name)
    for path, mesh_name in WEAPONS.items():
        weapon = load(path)
        check(weapon.get_editor_property("held_mesh") == load(KIT_DIR + "/" + mesh_name), path + " holds " + mesh_name)
        for slot, system_name in WEAPON_FX_SLOTS.items():
            check(weapon.get_editor_property(slot) == load(FX_DIR + "/" + system_name), "%s: %s is %s" % (path, slot, system_name))
    log("validation passed")


def main():
    ensure_materials()
    ensure_meshes()
    ensure_textures()
    sparks = ensure_sparks()
    frames = {}
    sets = ensure_part_sets(frames)
    ensure_tools(frames)
    ensure_weapons(frames, ensure_weapon_fx())
    ensure_pickups()
    ensure_blueprints(sets, sparks)
    ensure_hub_dressing()
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_art_kit failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
