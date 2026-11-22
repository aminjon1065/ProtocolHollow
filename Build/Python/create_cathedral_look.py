"""Dresses the Reactor Cathedral's rooms: light, materials and acoustics (idempotent, ADR-0010, ADR-0040). Nothing heavy:
material instances of the project's node-only masters, engine cubes, procedural MetaSounds - no textures, no WAV.

  /HollowSector_Cathedral/Look/MI_*        the palette: cold-cyan reactor light and violet-gold glass over dark stone
                                           (instances of M_Hollow_Surface and M_Hollow_Emissive, create_lighting_assets.py)
For every Cathedral room level (create_cathedral_greybox.py), unless art-locked on its definition (ADR-0027):
  materials        the shell (floor, vault, walls), the decor (piers and shafts, gilt ribs and arches)
  Decor actor      GlassViolet / GlassGold: the panes of the upper tier's lancets; ReactorGlow: cyan channels in the floor
                   (instanced, no collision, no navigation)
  CathedralLook    one actor whose components are the room's lights and fixtures (candle stands and flames, emergency
                   housings): an actor weighs ~2 KB in the level, a component a fraction of it (LFS)
  lights           a rig within the room type's budget (DA_LightBudget): halls lit through their clerestory (shadowed, one
                   volumetric), warm candles, a violet lantern, cyan reactor glow; an emergency light with M_LF_Emergency in
                   every room (it flickers with MPC_Resonance)
  acoustics        an Audio Gameplay Volume over the room (RE_Cathedral 4.5 s in the halls, RE_Crypt, RE_Cloister; muffled
                   outside) and the reactor's hum as the room tone (MS_Reactor_Hum, create_audio_assets.py)

A level keeps a hash of its spec in the tags of its acoustics volume: it is re-saved only when the spec changes (every
save is a new LFS object). Run after create_cathedral_mechanics.py (last in the order):
  Build/Scripts/run-editor-python.sh Build/Python/create_cathedral_look.py
"""
import hashlib
import json
import math
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

DATA_DIR = "/HollowSector_Cathedral/Data"
LOOK_DIR = "/HollowSector_Cathedral/Look"
ROOM_PREFIX = "DA_Room_Cathedral_"
SURFACE = "/Game/Hollow/World/Materials/M_Hollow_Surface"           # create_lighting_assets.py
EMISSIVE = "/Game/Hollow/World/Materials/M_Hollow_Emissive"
EMERGENCY_STRIP = "/Game/Hollow/World/Materials/MI_Strip_Emergency"  # flickers with MPC_Resonance (FollowResonance)
LF_EMERGENCY = "/Game/Hollow/World/Lighting/M_LF_Emergency"
REVERB_DIR = "/Game/Hollow/Audio/Reverb"                             # create_audio_assets.py
ROOM_TONE = "/Game/Hollow/Audio/MetaSounds/MS_Reactor_Hum"
CUBE = "/Engine/BasicShapes/Cube.Cube"
SHAPE_SIZE = 100.0
VOLUME_BRUSH = 200.0          # a spawned volume's default cube brush (uu)
MANAGED_TAG = "HollowCathedralLook"
RIG_LABEL = "CathedralLook"   # one actor: the room's lights and fixtures as its components
MARKER_LABEL = "RoomAcoustics"   # the room's acoustics volume also carries the spec hash (no marker actor: LFS)
DECOR_LABEL = "Decor"         # create_cathedral_greybox.py: DecorSolid (root) and DecorVault
LOOK_VERSION = 5              # 2: no empty components; 3: the hash on the acoustics volume; 4: one rig actor (LFS); 5: reach

# ---------------------------------------------------------------- palette (data)

PALETTE = {
    "MI_Cathedral_Stone": (SURFACE, {"Metallic": 0.05, "Roughness": 0.85, "PanelSize": 50.0, "SeamWidth": 1.0, "SeamDarken": 0.5,
                                     "GrimeScale": 120.0, "GrimeAmount": 0.5}, {"BaseColor": (0.1, 0.11, 0.13)}),
    "MI_Cathedral_Floor": (SURFACE, {"Metallic": 0.1, "Roughness": 0.7, "PanelSize": 100.0, "SeamWidth": 1.5, "SeamDarken": 0.6,
                                     "GrimeAmount": 0.6}, {"BaseColor": (0.06, 0.065, 0.075)}),
    "MI_Cathedral_Vault": (SURFACE, {"Metallic": 0.05, "Roughness": 0.9, "PanelSize": 50.0, "GrimeAmount": 0.4}, {"BaseColor": (0.05, 0.055, 0.07)}),
    "MI_Cathedral_Pier": (SURFACE, {"Metallic": 0.05, "Roughness": 0.8, "PanelSize": 25.0, "SeamWidth": 0.8, "GrimeAmount": 0.35},
                          {"BaseColor": (0.16, 0.165, 0.18)}),
    "MI_Cathedral_Gilt": (SURFACE, {"Metallic": 0.9, "Roughness": 0.35, "PanelSize": 1000.0, "GrimeAmount": 0.3}, {"BaseColor": (0.6, 0.45, 0.2)}),
    "MI_Glass_Violet": (EMISSIVE, {"Strength": 3.0}, {"Color": (0.55, 0.25, 1.0)}),
    "MI_Glass_Gold": (EMISSIVE, {"Strength": 2.5}, {"Color": (1.0, 0.7, 0.3)}),
    "MI_Reactor_Glow": (EMISSIVE, {"Strength": 6.0, "FollowResonance": 1.0}, {"Color": (0.2, 0.9, 1.0)}),
    "MI_Candle_Flame": (EMISSIVE, {"Strength": 12.0}, {"Color": (1.0, 0.65, 0.3)}),
}
SHELL = {"Floor": "MI_Cathedral_Floor", "Ceiling": "MI_Cathedral_Vault", "Wall": "MI_Cathedral_Stone", "Lintel": "MI_Cathedral_Stone"}
DECOR = {"DecorSolid": "MI_Cathedral_Pier", "DecorVault": "MI_Cathedral_Gilt"}

# The greybox's lancets (create_cathedral_greybox.py: 80 wide, 10 deep, sill 120 over the string course of 20 on the
# first storey, head 100 under the ceiling): a pane 1 uu proud of each, a little inside its frame.
LANCET = {"width": 80.0, "depth": 10.0, "sill": 120.0, "head": 100.0, "band": 20.0}
PANE = {"inset": 10.0, "proud": 1.0}
# Reactor channels in the floor: thin cyan strips, 8 wide, flush with it.
CHANNEL = {"width": 8.0, "height": 0.6}

# ---------------------------------------------------------------- lights (data, within DA_LightBudget)

CLERESTORY = {"intensity": 6000.0, "color": (150, 215, 255), "source_width": 100.0, "source_height": 40.0, "barn_door_angle": 60.0,
              "scattering": 2.0, "inset": 60.0, "below_ceiling": 180.0, "pitch": -50.0}
CLERESTORY_CHAPEL = {"color": (200, 150, 255)}              # violet-gold through the chapel's glass
CANDLE = {"intensity": 400.0, "color": (255, 180, 90), "radius": 350.0, "height": 140.0, "scattering": 1.0,
          "stand": (14.0, 14.0, 120.0), "flame": (6.0, 6.0, 10.0)}
LANTERN = {"intensity": 2500.0, "color": (175, 120, 255), "source_width": 200.0, "source_height": 200.0, "barn_door_angle": 80.0,
           "below_ceiling": 20.0, "scattering": 1.5}
GLASS_FILL = {"intensity": 1500.0, "color": (180, 120, 255), "source_width": 200.0, "source_height": 100.0, "barn_door_angle": 80.0,
              "below_ceiling": 40.0, "scattering": 1.0}
REACTOR_LIGHT = {"intensity": 300.0, "color": (90, 220, 255), "radius": 400.0, "height": 30.0, "scattering": 2.0}
KEY = {"intensity": 3000.0, "source_width": 120.0, "source_height": 40.0, "barn_door_angle": 70.0, "below_ceiling": 12.0, "scattering": 1.0}
KEY_COLORS = {"cold": (170, 215, 255), "warm": (255, 200, 140), "cyan": (110, 220, 255)}
EMERGENCY = {"intensity": 900.0, "color": (255, 45, 25), "radius": 600.0, "height": 300.0, "inset": 60.0, "scattering": 3.0,
             "housing": (24.0, 12.0, 12.0)}

# Room rigs (by room name; room-local cells): clerestory = (cell x, cell y, wall the light hangs on, chapel colour);
# candles, keys (cell, colour, shadowed), reactor glow cells, lantern / glass fill.
RIGS = {
    "Nave": {"clerestory": [(1.25, 0.0, "WEST", False), (2.75, 3.0, "EAST", False)], "volumetric": 0,
             "candles": [(3.6, 1.15), (3.6, 1.85)]},
    "Extraction_Belfry": {"clerestory": [(0.75, 0.0, "WEST", False), (2.25, 2.0, "EAST", False)], "volumetric": 0, "fill": (1.5, 1.0)},
    "Chapel": {"clerestory": [(1.0, 0.0, "WEST", True)], "volumetric": 0, "candles": [(0.4, 1.6)]},
    "ReactorCrypt": {"keys": [((1.0, 1.0), "cyan", True)], "reactor": [(1.0, 1.0)], "channels": "ring"},
    "Narthex": {"keys": [((1.5, 1.0), "warm", True)], "reactor": [(0.5, 1.0)]},
    "Crossing": {"lantern": (1.5, 1.5), "reactor": [(1.5, 1.5)], "channels": "cross"},
    "Cloister_Straight": {"keys": [((0.5, 1.5), "cold", False)]},
    "Cloister_Corner": {"keys": [((0.5, 0.5), "cold", False)]},
}
# Acoustics (by room name): reverb, room tone volume (the reactor is loudest in its crypt).
ACOUSTICS = {"Nave": ("RE_Cathedral", 0.35), "Crossing": ("RE_Cathedral", 0.35), "Chapel": ("RE_Cathedral", 0.35),
             "Extraction_Belfry": ("RE_Cathedral", 0.35), "ReactorCrypt": ("RE_Crypt", 0.6), "Narthex": ("RE_Cloister", 0.3),
             "Cloister_Straight": ("RE_Cloister", 0.25), "Cloister_Corner": ("RE_Cloister", 0.25)}
REACH = 1.05                  # a light reaches what it lights in its room and a little more, never the room next door:
                              # a longer reach shades and lights neighbours through the openings (VSM pages, light passes)
ROOM_AUDIO = {"reverb_volume": 0.6, "reverb_fade": 0.6, "exterior_lpf": 1500.0, "exterior_lpf_time": 0.4, "tone_falloff": 300.0}

SIDES = ("NORTH", "EAST", "SOUTH", "WEST")   # North = +X, East = +Y

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel = unreal.MaterialEditingLibrary


def log(message):
    unreal.log("create_cathedral_look: " + message)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset " + path)
    return asset


def save(asset):
    if not assets.save_loaded_asset(asset, False):
        raise RuntimeError("Could not save " + asset.get_path_name())


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def digest(*specs):
    return hashlib.sha1(json.dumps(specs, sort_keys=True, default=str).encode("ascii")).hexdigest()


def colour(rgb):
    return unreal.Color(r=rgb[0], g=rgb[1], b=rgb[2], a=255)


def vector(xyz):
    return unreal.Vector(xyz[0], xyz[1], xyz[2])


def tags_of(actor):
    return [str(t) for t in actor.get_editor_property("tags")]


# ---------------------------------------------------------------- palette

def ensure_palette():
    made = {}
    for name, (parent_path, scalars, vectors) in sorted(PALETTE.items()):
        path = LOOK_DIR + "/" + name
        created = not assets.does_asset_exist(path)
        if created:
            package_path, asset_name = path.rsplit("/", 1)
            instance = tools.create_asset(asset_name, package_path, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            if instance is None:
                raise RuntimeError("Could not create " + path)
        else:
            instance = load(path)
        parent = load(parent_path)
        changed = created
        if instance.get_editor_property("parent") != parent:
            mel.set_material_instance_parent(instance, parent)
            changed = True
        for key, value in scalars.items():
            if abs(mel.get_material_instance_scalar_parameter_value(instance, key) - value) > 1e-4:
                mel.set_material_instance_scalar_parameter_value(instance, key, value)
                changed = True
        for key, rgb in vectors.items():
            current = mel.get_material_instance_vector_parameter_value(instance, key)
            if any(abs(a - b) > 1e-4 for a, b in zip((current.r, current.g, current.b), rgb)):
                mel.set_material_instance_vector_parameter_value(instance, key, unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
                changed = True
        if changed:
            mel.update_material_instance(instance)
            save(instance)
            log(name + " ready")
        made[name] = instance
    return made


# ---------------------------------------------------------------- room spec

class Metrics(object):
    def __init__(self):
        cdo = unreal.get_default_object(unreal.HollowLevelMetrics)
        self.cell = float(cdo.get_editor_property("cell_size"))
        self.height = float(cdo.get_editor_property("cell_height"))
        self.wall = float(cdo.get_editor_property("wall_thickness"))


def rooms():
    found = []
    for path in sorted(assets.list_assets(DATA_DIR, recursive=False, include_folder=False)):
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.HollowRoomDefinition):
            found.append(asset)
    check(len(found) == len(RIGS), "every Cathedral room has a rig (%d rooms)" % len(found))
    return found


def room_name(room):
    return room.get_name()[len(ROOM_PREFIX):]


def connector_sides(room):
    """{side name: [cells along that wall with an opening]}."""
    sides = {}
    for c in room.get_editor_property("connectors"):
        side = next(name for name in SIDES if c.get_editor_property("side") == getattr(unreal.HollowRoomSide, name))
        cell = c.get_editor_property("cell")
        sides.setdefault(side, []).append(cell.y if side in ("NORTH", "SOUTH") else cell.x)
    return sides


def wall_point(side, along, depth, size, metrics):
    """A point against a wall's inner face: along the wall (Y for North/South, X for East/West), depth into the room."""
    w = metrics.wall
    if side == "NORTH":
        return (size[0] - w - depth, along)
    if side == "SOUTH":
        return (w + depth, along)
    if side == "EAST":
        return (along, size[1] - w - depth)
    return (along, w + depth)


def emergency_spot(room, size, metrics):
    """The emergency light: the middle of a wall without an opening, else a cell of a wall without one there."""
    openings = connector_sides(room)
    footprint = room.get_editor_property("footprint")
    for side in ("EAST", "WEST", "NORTH", "SOUTH"):
        cells = footprint.x if side in ("EAST", "WEST") else footprint.y
        if side not in openings:
            return side, cells * metrics.cell / 2.0
    for side in ("EAST", "WEST", "NORTH", "SOUTH"):
        cells = footprint.x if side in ("EAST", "WEST") else footprint.y
        free = [i for i in range(cells) if i not in openings.get(side, [])]
        if free:
            return side, (free[0] + 0.5) * metrics.cell
    raise RuntimeError("%s: no wall cell for the emergency light" % room.get_name())


def own_reach(location, size):
    """How far a light at location (room-local) reaches: its room's farthest floor corner, REACH of it (within the diagonal)."""
    corners = [(cx, cy) for cx in (0.0, size[0]) for cy in (0.0, size[1])]
    farthest = max(math.sqrt((location[0] - cx) ** 2 + (location[1] - cy) ** 2 + location[2] ** 2) for cx, cy in corners)
    return round(min(farthest * REACH, math.sqrt(size[0] ** 2 + size[1] ** 2 + size[2] ** 2)), 1)


def across_reach(location, side, size, metrics):
    """A wall panel shines across its room: it reaches the opposite wall's foot, REACH of it (the far half of a hall is
    its other panel's)."""
    x, y, z = location
    w = metrics.wall
    across = {"NORTH": x - w, "SOUTH": size[0] - w - x, "EAST": y - w, "WEST": size[1] - w - y}[side]
    return round(min(math.sqrt(across ** 2 + z ** 2) * REACH, math.sqrt(size[0] ** 2 + size[1] ** 2 + size[2] ** 2)), 1)


def room_spec(room, metrics):
    name = room_name(room)
    rig = RIGS[name]
    footprint = room.get_editor_property("footprint")
    c = metrics.cell
    size = (footprint.x * c, footprint.y * c, footprint.z * metrics.height)
    lights = []
    for index, (cx, cy, side, chapel) in enumerate(rig.get("clerestory", [])):
        x, y = wall_point(side, (cx if side in ("EAST", "WEST") else cy) * c, CLERESTORY["inset"], size, metrics)
        yaw = {"NORTH": 180.0, "SOUTH": 0.0, "EAST": -90.0, "WEST": 90.0}[side]   # into the room
        lights.append({"kind": "clerestory", "label": "Clerestory_%d" % index, "location": (x, y, size[2] - CLERESTORY["below_ceiling"]),
                       "rotation": (CLERESTORY["pitch"], yaw), "shadowed": True, "volumetric": index == rig.get("volumetric", -1),
                       "color": CLERESTORY_CHAPEL["color"] if chapel else CLERESTORY["color"],
                       "radius": across_reach((x, y, size[2] - CLERESTORY["below_ceiling"]), side, size, metrics)})
    for index, (cx, cy) in enumerate(rig.get("candles", [])):
        lights.append({"kind": "candle", "label": "Candle_%d" % index, "location": (cx * c, cy * c, CANDLE["height"]), "radius": CANDLE["radius"]})
    for index, ((cx, cy), tint, shadowed) in enumerate(rig.get("keys", [])):
        lights.append({"kind": "key", "label": "Key_%d" % index, "location": (cx * c, cy * c, size[2] - KEY["below_ceiling"]),
                       "color": KEY_COLORS[tint], "shadowed": shadowed, "radius": own_reach((cx * c, cy * c, size[2] - KEY["below_ceiling"]), size)})
    for index, (cx, cy) in enumerate(rig.get("reactor", [])):
        lights.append({"kind": "reactor", "label": "Reactor_%d" % index, "location": (cx * c, cy * c, REACTOR_LIGHT["height"]),
                       "radius": REACTOR_LIGHT["radius"]})
    if "lantern" in rig:
        cx, cy = rig["lantern"]
        where = (cx * c, cy * c, size[2] - LANTERN["below_ceiling"])
        lights.append({"kind": "lantern", "label": "Lantern", "location": where, "radius": own_reach(where, size)})
    if "fill" in rig:
        cx, cy = rig["fill"]
        where = (cx * c, cy * c, size[2] - GLASS_FILL["below_ceiling"])
        lights.append({"kind": "fill", "label": "GlassFill", "location": where, "radius": own_reach(where, size)})
    side, along = emergency_spot(room, size, metrics)
    x, y = wall_point(side, along, EMERGENCY["inset"], size, metrics)
    hx, hy = wall_point(side, along, EMERGENCY["housing"][1] / 2.0, size, metrics)
    lights.append({"kind": "emergency", "label": "Emergency", "location": (x, y, EMERGENCY["height"]), "housing": (hx, hy, EMERGENCY["height"]),
                   "side": side, "radius": min(EMERGENCY["radius"], own_reach((x, y, EMERGENCY["height"]), size))})
    reverb, tone_volume = ACOUSTICS[name]
    return {"room": name, "size": size, "lights": lights, "glass": panes(room, size, metrics), "channels": channels(rig.get("channels"), size, metrics),
            "reverb": reverb, "tone_volume": tone_volume, "palette": PALETTE, "shell": SHELL, "decor": DECOR, "audio": ROOM_AUDIO,
            "data": [CLERESTORY, CLERESTORY_CHAPEL, CANDLE, LANTERN, GLASS_FILL, REACTOR_LIGHT, KEY, KEY_COLORS, EMERGENCY, LANCET, PANE, CHANNEL, REACH],
            "version": LOOK_VERSION}


def panes(room, size, metrics):
    """Glass of the upper tier's lancets (two-storey halls): (colour, centre, size) per wall cell, violet and gold in turn."""
    footprint = room.get_editor_property("footprint")
    if footprint.z < 2:
        return []
    sill = metrics.height + LANCET["band"] + LANCET["sill"] + PANE["inset"]
    head = size[2] - LANCET["head"] - PANE["inset"]
    width = LANCET["width"] - 2.0 * PANE["inset"]
    result = []
    for side_index, side in enumerate(SIDES):
        cells = footprint.y if side in ("NORTH", "SOUTH") else footprint.x
        for k in range(cells):
            along = (k + 0.5) * metrics.cell
            x, y = wall_point(side, along, LANCET["depth"] + PANE["proud"] / 2.0, size, metrics)
            across = (PANE["proud"], width) if side in ("NORTH", "SOUTH") else (width, PANE["proud"])
            result.append(("GlassViolet" if (k + side_index) % 2 == 0 else "GlassGold", (x, y, (sill + head) / 2.0), (across[0], across[1], head - sill)))
    return result


def channels(kind, size, metrics):
    """Cyan reactor channels flush with the floor: a ring round the core (crypt) or a cross through the middle bay."""
    w, h = CHANNEL["width"], CHANNEL["height"]
    cx, cy = size[0] / 2.0, size[1] / 2.0
    if kind == "ring":
        r = 160.0   # round the core (50 x 50), well inside the crypt
        return [((cx, cy - r, h / 2.0), (2.0 * r, w, h)), ((cx, cy + r, h / 2.0), (2.0 * r, w, h)),
                ((cx - r, cy, h / 2.0), (w, 2.0 * r, h)), ((cx + r, cy, h / 2.0), (w, 2.0 * r, h))]
    if kind == "cross":
        half = metrics.cell / 2.0 - 40.0   # the middle bay between the four piers' plinths
        return [((cx, cy, h / 2.0), (2.0 * half, w, h)), ((cx, cy, h / 2.0), (w, 2.0 * half, h))]
    return []


# ---------------------------------------------------------------- level

def managed(actor, label, folder):
    if actor is None:
        raise RuntimeError("Could not spawn " + label)
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.set_editor_property("tags", [MANAGED_TAG])
    return actor


def part(rig, component_class, name, location, rotation=None):
    """A component of the room's rig actor at a room-local place (one actor for the whole rig: each actor weighs ~2 KB)."""
    component = unreal.HollowAssetAuthoring.add_instance_component(rig, component_class, name)
    if component is None:
        raise RuntimeError("Could not add " + name)
    component.set_relative_location(vector(location), False, False)
    component.set_relative_rotation(rotation or unreal.Rotator(0.0, 0.0, 0.0), False, False)
    return component


def box(rig, name, material, centre, extent):
    component = part(rig, unreal.StaticMeshComponent, name, centre)
    component.set_mobility(unreal.ComponentMobility.STATIC)
    component.set_static_mesh(load(CUBE))
    component.set_material(0, material)
    component.set_collision_profile_name("NoCollision")   # looks only: no gameplay, no navmesh
    component.set_editor_property("can_ever_affect_navigation", False)
    component.set_editor_property("cast_shadow", False)    # a lamp's own housing never shades it
    component.set_relative_scale3d(unreal.Vector(extent[0] / SHAPE_SIZE, extent[1] / SHAPE_SIZE, extent[2] / SHAPE_SIZE))
    return component


def setup_light(component, spec, intensity, rgb, shadowed, scattering):
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
    component.set_editor_property("intensity", intensity)
    component.set_editor_property("light_color", colour(rgb))
    component.set_editor_property("attenuation_radius", spec["radius"])
    component.set_editor_property("cast_shadows", shadowed)
    component.set_editor_property("cast_volumetric_shadow", spec.get("volumetric", False))
    component.set_editor_property("volumetric_scattering_intensity", scattering)


def rect(rig, spec, data, rgb, shadowed, rotation):
    component = part(rig, unreal.RectLightComponent, spec["label"], spec["location"], rotation)
    for key in ("source_width", "source_height", "barn_door_angle"):
        component.set_editor_property(key, data[key])
    setup_light(component, spec, data["intensity"], rgb, shadowed, data["scattering"])
    return component


def point(rig, spec, data, rgb):
    component = part(rig, unreal.PointLightComponent, spec["label"], spec["location"])
    setup_light(component, spec, data["intensity"], rgb, False, data["scattering"])
    return component


def build_light(rig, spec, palette, emergency_strip, emergency_function):
    down = unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0)
    kind = spec["kind"]
    if kind == "clerestory":
        pitch, yaw = spec["rotation"]
        rect(rig, spec, CLERESTORY, spec["color"], True, unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    elif kind == "key":
        rect(rig, spec, KEY, spec["color"], spec["shadowed"], down)
    elif kind == "lantern":
        rect(rig, spec, LANTERN, LANTERN["color"], False, down)
    elif kind == "fill":
        rect(rig, spec, GLASS_FILL, GLASS_FILL["color"], False, down)
    elif kind == "candle":
        point(rig, spec, CANDLE, CANDLE["color"])
        x, y, z = spec["location"]
        sw, sd, sh = CANDLE["stand"]
        box(rig, spec["label"] + "_Stand", palette["MI_Cathedral_Gilt"], (x, y, sh / 2.0), (sw, sd, sh))
        fw, fd, fh = CANDLE["flame"]
        box(rig, spec["label"] + "_Flame", palette["MI_Candle_Flame"], (x, y, sh + fh / 2.0), (fw, fd, fh))
    elif kind == "reactor":
        point(rig, spec, REACTOR_LIGHT, REACTOR_LIGHT["color"])
    else:
        component = point(rig, spec, EMERGENCY, EMERGENCY["color"])
        component.set_editor_property("light_function_material", emergency_function)   # MPC_Resonance flicker (S4-03)
        w, d, h = EMERGENCY["housing"]
        if spec["side"] in ("NORTH", "SOUTH"):
            w, d = d, w   # the housing lies along its wall
        box(rig, "Emergency_Housing", emergency_strip, spec["housing"], (w, d, h))


def build_rig(spec, palette, emergency_strip, emergency_function):
    """The room's lights and fixtures: components of one actor at the room's origin, under a static root."""
    zero = unreal.Rotator(0.0, 0.0, 0.0)
    rig = managed(actors.spawn_actor_from_class(unreal.Actor, unreal.Vector(0.0, 0.0, 0.0), zero), RIG_LABEL, "Lights")
    root = unreal.HollowAssetAuthoring.add_instance_component(rig, unreal.SceneComponent, "Root")
    if root is None:
        raise RuntimeError("Could not add the rig's root")
    root.set_mobility(unreal.ComponentMobility.STATIC)
    for light in spec["lights"]:
        build_light(rig, light, palette, emergency_strip, emergency_function)


def instanced(owner, name, material, transforms):
    """The decor's instanced component of that name, or none at all when it has nothing to show (each one weighs)."""
    if not transforms:
        for existing in owner.get_components_by_class(unreal.InstancedStaticMeshComponent):
            if existing.get_name() == name:
                existing.destroy_component(owner)
        return
    component = unreal.HollowAssetAuthoring.add_instance_component(owner, unreal.InstancedStaticMeshComponent, name)
    if component is None:
        raise RuntimeError("Could not add " + name)
    component.set_mobility(unreal.ComponentMobility.STATIC)
    component.set_static_mesh(load(CUBE))
    component.set_material(0, material)
    component.set_collision_profile_name("NoCollision")
    component.set_editor_property("can_ever_affect_navigation", False)
    component.set_editor_property("cast_shadow", False)   # glass and channels glow; they never shade
    component.clear_instances()
    for centre, extent in transforms:
        component.add_instance(unreal.Transform(location=vector(centre), rotation=unreal.Rotator(0.0, 0.0, 0.0),
                                                scale=unreal.Vector(extent[0] / SHAPE_SIZE, extent[1] / SHAPE_SIZE, extent[2] / SHAPE_SIZE)), False)


def dress_materials(level_actors, palette, glass, channel_list):
    decor = None
    for actor in level_actors:
        label = actor.get_actor_label()
        if label == DECOR_LABEL:
            decor = actor
            continue
        prefix = next((p for p in SHELL if label == p or label.startswith(p + "_")), None)
        if prefix and isinstance(actor, unreal.StaticMeshActor) and MANAGED_TAG not in tags_of(actor):
            actor.static_mesh_component.set_material(0, palette[SHELL[prefix]])
    if decor is None:
        raise RuntimeError("no Decor actor (create_cathedral_greybox.py)")
    for component in decor.get_components_by_class(unreal.InstancedStaticMeshComponent):
        material = DECOR.get(component.get_name())
        if material:
            component.set_material(0, palette[material])
    for name in ("GlassViolet", "GlassGold"):
        instanced(decor, name, palette["MI_" + name.replace("Glass", "Glass_")], [(c, e) for kind, c, e in glass if kind == name])
    instanced(decor, "ReactorGlow", palette["MI_Reactor_Glow"], channel_list)


def build_acoustics(spec, reverbs, tone_sound, wanted):
    size = spec["size"]
    centre = unreal.Vector(size[0] / 2.0, size[1] / 2.0, size[2] / 2.0)
    zero = unreal.Rotator(0.0, 0.0, 0.0)
    volume = managed(actors.spawn_actor_from_class(unreal.AudioGameplayVolume, centre, zero), MARKER_LABEL, "Audio")
    volume.set_editor_property("tags", [MANAGED_TAG, wanted])   # the level's spec hash: dressed again only when it changes
    volume.set_actor_scale3d(unreal.Vector(size[0] / VOLUME_BRUSH, size[1] / VOLUME_BRUSH, size[2] / VOLUME_BRUSH))
    reverb = unreal.HollowAssetAuthoring.add_instance_component(volume, unreal.ReverbVolumeComponent, "Reverb")
    settings = reverb.get_editor_property("reverb_settings")
    settings.set_editor_property("apply_reverb", True)
    settings.set_editor_property("reverb_effect", reverbs[spec["reverb"]])
    settings.set_editor_property("volume", ROOM_AUDIO["reverb_volume"])
    settings.set_editor_property("fade_time", ROOM_AUDIO["reverb_fade"])
    reverb.set_editor_property("reverb_settings", settings)
    walls = unreal.HollowAssetAuthoring.add_instance_component(volume, unreal.FilterVolumeComponent, "Walls")
    walls.set_editor_property("exterior_lpf", ROOM_AUDIO["exterior_lpf"])
    walls.set_editor_property("exterior_lpf_time", ROOM_AUDIO["exterior_lpf_time"])
    tone = managed(actors.spawn_actor_from_class(unreal.AmbientSound, centre, zero), "RoomTone", "Audio")
    component = tone.get_editor_property("audio_component")
    component.set_editor_property("sound", tone_sound)
    component.set_editor_property("volume_multiplier", spec["tone_volume"])
    component.set_editor_property("override_attenuation", True)
    attenuation = component.get_editor_property("attenuation_overrides")
    attenuation.set_editor_property("attenuation_shape", unreal.AttenuationShape.BOX)
    attenuation.set_editor_property("attenuation_shape_extents", unreal.Vector(size[0] / 2.0, size[1] / 2.0, size[2] / 2.0))
    attenuation.set_editor_property("falloff_distance", ROOM_AUDIO["tone_falloff"])
    attenuation.set_editor_property("spatialize", False)   # it is the room itself, from everywhere
    component.set_editor_property("attenuation_overrides", attenuation)


def ensure_room(room, metrics, palette, reverbs, tone_sound, emergency_strip, emergency_function):
    if room.get_editor_property("art_locked"):
        log("%s: art-locked, level kept as is" % room.get_name())
        return False
    spec = room_spec(room, metrics)
    wanted = digest(spec)
    level_path = room.get_editor_property("room_level").get_path_name().split(".")[0]
    if not levels.load_level(level_path):
        raise RuntimeError("Could not load " + level_path)
    level_actors = actors.get_all_level_actors()
    marker = next((a for a in level_actors if a.get_actor_label() == MARKER_LABEL and MANAGED_TAG in tags_of(a)), None)
    if marker is not None and wanted in tags_of(marker):
        return False
    for actor in level_actors:
        if MANAGED_TAG in tags_of(actor):
            actors.destroy_actor(actor)
    level_actors = actors.get_all_level_actors()
    dress_materials(level_actors, palette, spec["glass"], spec["channels"])
    build_rig(spec, palette, emergency_strip, emergency_function)
    build_acoustics(spec, reverbs, tone_sound, wanted)
    if not levels.save_current_level():
        raise RuntimeError("Could not save " + level_path)
    log("%s: %d lights, %d panes, %s" % (room.get_name(), len(spec["lights"]), len(spec["glass"]), spec["reverb"]))
    return True


# ---------------------------------------------------------------- validation

def validate(room_list, palette):
    for name, (parent_path, _, _) in PALETTE.items():
        check(palette[name].get_editor_property("parent") == load(parent_path), name + " is an instance of " + parent_path)
    check(abs(mel.get_material_instance_scalar_parameter_value(palette["MI_Reactor_Glow"], "FollowResonance") - 1.0) < 1e-4,
          "the reactor glow follows MPC_Resonance")
    check(abs(load(REVERB_DIR + "/RE_Cathedral").get_editor_property("decay_time") - 4.5) < 0.05, "RE_Cathedral rings 4.5 s")
    validator = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
    for room in room_list:
        level = load(room.get_editor_property("room_level").get_path_name().split(".")[0])
        result = validator.is_object_valid(level, unreal.DataValidationUsecase.SCRIPT)
        check(result[0] != unreal.DataValidationResult.INVALID, "%s fits its light budget: %s" % (room.get_name(), [str(e) for e in result[1]]))
    log("validation passed (%d rooms dressed, %d instances in the palette)" % (len(room_list), len(PALETTE)))


def main():
    metrics = Metrics()
    palette = ensure_palette()
    reverbs = {name: load(REVERB_DIR + "/" + name) for name in sorted(set(r for r, _ in ACOUSTICS.values()))}
    tone_sound = load(ROOM_TONE)
    emergency_strip = load(EMERGENCY_STRIP)
    emergency_function = load(LF_EMERGENCY)
    room_list = rooms()
    built = sum(1 for room in room_list if ensure_room(room, metrics, palette, reverbs, tone_sound, emergency_strip, emergency_function))
    log("dressed %d room level(s) of %d" % (built, len(room_list)))
    validate(room_list, palette)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_cathedral_look failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
