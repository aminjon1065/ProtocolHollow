"""CHAR-01: the three characters' own look from the Paragon packs (idempotent, ADR-0010, ADR-0055).

  ExoFrame (BP_ExoFrame)       Paragon Wraith: the body everyone else sees, the camera at its eyes, the tool in its ToolMount
                               socket (the pack's rifle grip on hand_r), the class's colour on its emissive glow; no
                               first-person arms (Wraith has no first-person poses: the owner's tool rides the camera,
                               create_art_kit.py, KnownIssues AK-4)
  Iron Pilgrim (BP_IronPilgrim)   Paragon Riktor, fitted to the Heavy capsule (r80, h280)
  Echo-Stalker (BP_EchoStalker)   Paragon Sevarog, fitted to the Standard capsule

  /Game/Fab/Paragon*/.../<Hero>_Skeleton    its preview mesh its own, the DefaultSlot known; Wraith's gets ToolMount
  /Game/Hollow/Characters/<Character>/Animation/
     BS_<Character>_Locomotion      by Speed (1D) or by Direction x Speed (2D, the ExoFrame): each move sampled at the game's
                                    speeds, played as fast as its feet need (measured from the planted foot)
     ABP_<Character>[_Body]         UHollowCharacterAnimInstance: locomotion -> [aim offset by AimPitch] -> Slot DefaultSlot
     AM_<Character>_<Moment>        the enemies' montages (ADR-0027): Melee/Breach with Impact where the striking hand stops,
                                    Stunned (start, then a looping section), the Stalker's VentEnter/VentExit
  DA_Enemy_IronPilgrim, DA_Enemy_EchoStalker   their Montages
The capsules, brains and AI stay as create_pilgrim_assets.py / create_stalker_assets.py make them; this script owns the meshes,
their place and animation, the camera's place and the body's tool socket. Creates or fixes only what differs, then validates.
Run after prepare_fab_assets.py (in the game), create_exoframe/pilgrim/stalker_assets.py and create_art_kit.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_character_assets.py
"""
import hashlib
import json
import os
import statistics
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh
MANIFEST = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Art", "fab_manifest.json")
SPEC_TAG = "HollowCharacterSpec"
SLOT = "DefaultSlot"
TOOL_SOCKET = "ToolMount"
CAMERA_SOCKET = "Eyes_Position"       # Paragon's eye point on the head: where the first-person camera sits
IMPACT = "Impact"                     # UHollowEnemyDefinition::ImpactSection

W = "/Game/Fab/ParagonWraith/Characters/Heroes/Wraith"
R = "/Game/Fab/ParagonRiktor/Characters/Heroes/Riktor"
S = "/Game/Fab/ParagonSevarog/Characters/Heroes/Sevarog"

# The game's speeds (cm/s: DA_MovementTuning_ExoFrame, DA_Enemy_*): where the locomotion samples sit.
EXO_WALK, EXO_SPRINT = 400.0, 640.0
PILGRIM_PATROL, PILGRIM_CHASE = 180.0, 330.0
STALKER_PATROL, STALKER_CHASE = 220.0, 480.0
RATE_MIN, RATE_MAX = 0.35, 1.8   # a move played slower or faster than this slides its feet anyway
CROWN = 1.1                     # the top of a head is this times its head bone's height above the feet
GLIDE_BELOW = 100.0             # cm/s: a move whose planted foot is slower than this glides (its pace is not its feet's)

# The class's colour on Wraith's emissive glow (its materials' own colour, M_Wraith_*): each parameter keeps the
# material's brightness.
TINT_PARAMETERS = ["DeepEmissiveColor", "ShallowEmissive"]

CHARACTERS = [
    {"name": "ExoFrame", "bp": "/Game/Hollow/Characters/ExoFrame/BP_ExoFrame", "root": W, "hero": "Wraith",
     "dir": "/Game/Hollow/Characters/ExoFrame/Animation",
     "locomotion": {"idle": "Locomotion_Combat/Idle_Combat", "speeds": [EXO_WALK, EXO_SPRINT],
                    "moves": [(0.0, "Locomotion_Combat/Jog_Fwd_Combat"), (90.0, "Locomotion_Combat/Jog_Right_Combat"),
                              (-90.0, "Locomotion_Combat/Jog_Left_Combat"), (180.0, "Locomotion_Combat/Jog_Bwd_Combat"),
                              (-180.0, "Locomotion_Combat/Jog_Bwd_Combat")]},
     "aim_offset": "Locomotion_Combat/AO/Idle_AO_Combat",
     "blueprints": {"ABP_ExoFrame_Body": "aim"},
     "montages": {}},
    {"name": "IronPilgrim", "bp": "/Game/Hollow/Characters/Enemies/IronPilgrim/BP_IronPilgrim", "root": R, "hero": "Riktor",
     "dir": "/Game/Hollow/Characters/Enemies/IronPilgrim/Animation",
     "definition": "/Game/Hollow/Characters/Enemies/IronPilgrim/DA_Enemy_IronPilgrim",
     "locomotion": {"samples": [("Idle_Combat", 0.0), ("Jog_Fwd", PILGRIM_PATROL), ("Jog_Fwd", PILGRIM_CHASE)]},   # a heavy, slow jog
     "blueprints": {"ABP_IronPilgrim": "plain"},
     "montages": {"melee": {"strike": ("PrimaryAttack_C_Slow", "hand_r")}, "breach": {"strike": ("Ability_Lockdown", "hand_r")},
                  "stunned": {"stun": ("Stun_Start", "Stun_Loop")}}},
    {"name": "EchoStalker", "bp": "/Game/Hollow/Characters/Enemies/EchoStalker/BP_EchoStalker", "root": S, "hero": "Sevarog",
     "dir": "/Game/Hollow/Characters/Enemies/EchoStalker/Animation",
     "definition": "/Game/Hollow/Characters/Enemies/EchoStalker/DA_Enemy_EchoStalker",
     "locomotion": {"samples": [("Idle", 0.0), ("Walk_Fwd", STALKER_PATROL), ("Sprint_Fwd", STALKER_CHASE)]},
     "blueprints": {"ABP_EchoStalker": "plain"},
     "montages": {"melee": {"strike": ("Swing1_Medium", "weapon_r")}, "stunned": {"stun": ("Stun_Start", "Stun_Loop")},
                  # Into a vent: the crouch and burst of its dash; out of one: the rise from the floor of its level start
                  # (1.0 to 2.1 s: from below the floor to standing, before its leap).
                  "vent_enter": {"clip": ("Speedburst", 0.0, 0.0)}, "vent_exit": {"clip": ("LevelStart", 1.0, 2.1)}}},
]
MONTAGE_NAMES = {"melee": "Melee", "breach": "Breach", "stunned": "Stunned", "vent_enter": "VentEnter", "vent_exit": "VentExit"}
BLENDS = {"strike": (0.15, 0.25), "stun": (0.2, 0.3), "clip": (0.1, 0.2)}   # seconds in, out

assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
authoring = unreal.HollowAnimationAuthoring


def log(message):
    unreal.log("create_character_assets: " + message)


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
    return hashlib.md5(json.dumps(parts, sort_keys=True, default=str).encode("utf-8")).hexdigest()


def up_to_date(asset, spec_hash):
    return assets.get_metadata_tag(asset, SPEC_TAG) == spec_hash


def stamp(asset, spec_hash):
    assets.set_metadata_tag(asset, SPEC_TAG, spec_hash)
    save(asset)


def outdated(path, asset_class, factory, spec_hash):
    """The asset at path to (re)build: None when its spec is current; an outdated one is rebuilt in place (what refers
    to it keeps doing so), a missing one is created."""
    if assets.does_asset_exist(path):
        existing = load(path)
        return None if up_to_date(existing, spec_hash) else existing
    folder, name = path.rsplit("/", 1)
    asset = tools.create_asset(name, folder, asset_class, factory)
    check(asset is not None, "created " + path)
    return asset


def same_transform(a, b, tolerance=0.01):
    return ((a.translation - b.translation).length() <= tolerance and (a.scale3d - b.scale3d).length() <= tolerance
            and abs(abs(a.rotation.x * b.rotation.x + a.rotation.y * b.rotation.y + a.rotation.z * b.rotation.z + a.rotation.w * b.rotation.w) - 1.0) <= 1e-5)


# ---------------------------------------------------------------- measuring the animations (the skeleton's own pose)

class Rig:
    """Component-space poses of a mesh's bones in its animations (the bones' local poses composed up the hierarchy)."""

    def __init__(self, mesh):
        self.actor = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0.0, 0.0, 0.0))
        self.component = self.actor.get_editor_property("skeletal_mesh_component")
        self.component.set_skinned_asset_and_update(mesh)
        self.chains = {}

    def close(self):
        actors.destroy_actor(self.actor)

    def chain(self, bone):
        if bone not in self.chains:
            bones = [bone]
            while True:
                parent = str(self.component.get_parent_bone(bones[-1]))
                if parent in ("None", ""):
                    break
                bones.append(parent)
            self.chains[bone] = list(reversed(bones))
        return self.chains[bone]

    def pose(self, anim, bone, time):
        total = unreal.Transform()
        for name in self.chain(bone):
            total = unreal.MathLibrary.compose_transforms(unreal.AnimationLibrary.get_bone_pose_for_time(anim, name, time, False), total)
        return total

    def reference(self, bone):
        return self.component.get_socket_transform(bone, unreal.RelativeTransformSpace.RTS_COMPONENT)

    def stride_speed(self, anim, samples=48):
        """How fast a move carries the body: the planted foot's speed across the floor (cm/s; 0 standing)."""
        length = unreal.AnimationLibrary.get_sequence_length(anim)
        times = [length * i / samples for i in range(samples + 1)]
        bones = ("ball_l", "ball_r") if self.component.get_bone_index("ball_l") >= 0 else ("foot_l", "foot_r")   # Sevarog has no balls of the feet
        feet = {foot: [self.pose(anim, foot, t).translation for t in times] for foot in bones}
        speeds = []
        for i in range(samples):
            foot = min(feet, key=lambda f: feet[f][i].z)   # the planted foot is the lower one
            if feet[foot][i + 1].z - feet[foot][i].z > 2.0:
                continue   # lifting off
            step = feet[foot][i + 1] - feet[foot][i]
            speeds.append(((step.x ** 2 + step.y ** 2) ** 0.5) / (times[i + 1] - times[i]))
        return statistics.median(speeds) if speeds else 0.0

    def impact_time(self, anim, bone, samples=60):
        """Where a blow lands: the striking bone's fast swing stops (below a third of its peak speed after the peak)."""
        length = unreal.AnimationLibrary.get_sequence_length(anim)
        times = [length * i / samples for i in range(samples + 1)]
        points = [self.pose(anim, bone, t).translation for t in times]
        speeds = [(points[i + 1] - points[i]).length() / (times[i + 1] - times[i]) for i in range(samples)]
        peak = max(range(samples), key=lambda i: speeds[i])
        stop = next((i for i in range(peak, samples) if speeds[i] < speeds[peak] / 3.0), samples - 1)
        return round(times[stop], 3)


def rate(speed, natural):
    """How much faster a move plays at a game speed; as it is for a body its feet do not carry (Sevarog glides)."""
    return round(min(RATE_MAX, max(RATE_MIN, speed / natural)), 3) if natural >= GLIDE_BELOW and speed > 0.0 else 1.0


# ---------------------------------------------------------------- meshes and skeletons

def hidden_slots(mesh):
    """What the pack's character held, as fab_manifest.json hides it (hidden_slots of its mesh)."""
    with open(MANIFEST) as f:
        manifest = json.load(f)
    path = mesh.get_path_name().split(".")[0]
    return sorted(set(slot for pack in manifest.get("packs", []) for name, slots in pack.get("hidden_slots", {}).items() if name == path for slot in slots))


def ensure_hidden_sections(mesh):
    """The hidden slots' sections off in every LOD: not drawn at all (an empty material still costs depth and shadow)."""
    slots = hidden_slots(mesh)
    if not slots:
        return
    disabled = authoring.disable_mesh_slots(mesh, slots)
    if disabled:
        save(mesh)
        log("%s: %d sections of %s off" % (mesh.get_name(), disabled, ", ".join(slots)))


# ---------------------------------------------------------------- skeletons

def ensure_skeleton(character, mesh, rig):
    skeleton = mesh.get_editor_property("skeleton")
    changed = False
    if skeleton.get_skeleton_preview_mesh() != mesh:   # not a skin of the pack (it stayed in the staging)
        skeleton.set_skeleton_preview_mesh(mesh)
        changed = True
    check(authoring.ensure_skeleton_slot(skeleton, SLOT), "%s knows %s" % (skeleton.get_name(), SLOT))
    if character["name"] == "ExoFrame":
        changed = ensure_tool_socket(skeleton, rig) or changed
    if changed:
        save(skeleton)
        log("%s: its own preview mesh%s" % (skeleton.get_name(), ", " + TOOL_SOCKET if character["name"] == "ExoFrame" else ""))
    elif not assets.save_loaded_asset(skeleton, True):   # the slot, if it was new (saved only when dirty)
        raise RuntimeError("Could not save " + skeleton.get_path_name())
    return skeleton


def tool_socket_transform(rig):
    """Where the pack's rifle sits in Wraith's right hand in its combat idle: the grip, X along the barrel, Z up."""
    idle = load(W + "/Animations/Locomotion_Combat/Idle_Combat")
    hand = rig.pose(idle, "hand_r", 0.0)
    grip = rig.pose(idle, "weapon_r", 0.0)
    barrel = rig.pose(idle, "gun_barrel", 0.0).translation - grip.translation
    up = unreal.MathLibrary.greater_greater_vector_rotator(unreal.Vector(0.0, 0.0, 1.0), grip.rotation.rotator())
    socket = unreal.Transform(grip.translation, unreal.MathLibrary.make_rot_from_xz(barrel, up), unreal.Vector(1.0, 1.0, 1.0))
    return unreal.MathLibrary.make_relative_transform(socket, hand)


def ensure_tool_socket(skeleton, rig):
    wanted = tool_socket_transform(rig)
    mesh_socket = rig.component.get_skinned_asset().find_socket(TOOL_SOCKET)
    current = None
    if mesh_socket is not None:
        current = unreal.Transform(mesh_socket.get_editor_property("relative_location"), mesh_socket.get_editor_property("relative_rotation"),
                                   mesh_socket.get_editor_property("relative_scale"))
    if current is not None and str(mesh_socket.get_editor_property("bone_name")) == "hand_r" and same_transform(current, wanted, 0.05):
        return False
    check(authoring.set_skeleton_socket(skeleton, TOOL_SOCKET, "hand_r", wanted), "Wraith_Skeleton gets " + TOOL_SOCKET)
    return True


# ---------------------------------------------------------------- blend spaces, animation blueprints, montages

def ensure_locomotion(character, skeleton, rig):
    root = character["root"] + "/Animations/"
    spec = character["locomotion"]
    path = "%s/BS_%s_Locomotion" % (character["dir"], character["name"])
    if "moves" in spec:   # 2D: direction x speed, each direction at every game speed
        idle = load(root + spec["idle"])
        axes = [{"Name": "Direction", "Min": -180.0, "Max": 180.0, "GridDivisions": 4, "bWrap": True},
                {"Name": "Speed", "Min": 0.0, "Max": spec["speeds"][-1], "GridDivisions": 4, "bWrap": False}]
        samples = [(spec["idle"], direction, 0.0, 1.0) for direction, _ in spec["moves"]]
        for direction, name in spec["moves"]:
            natural = rig.stride_speed(load(root + name))
            log("%s: %s carries the body at %.0f cm/s" % (character["name"], name, natural))
            samples += [(name, direction, speed, rate(speed, natural)) for speed in spec["speeds"]]
        factory = unreal.BlendSpaceFactoryNew()
        asset_class = unreal.BlendSpace
    else:   # 1D: speed
        axes = [{"Name": "Speed", "Min": 0.0, "Max": spec["samples"][-1][1], "GridDivisions": 4, "bWrap": False}]
        samples = []
        for name, speed in spec["samples"]:
            natural = rig.stride_speed(load(root + name)) if speed > 0.0 else 0.0
            if speed > 0.0:
                log("%s: %s carries the body at %.0f cm/s" % (character["name"], name, natural))
            samples.append((name, speed, 0.0, rate(speed, natural)))
        factory = unreal.BlendSpaceFactory1D()
        asset_class = unreal.BlendSpace1D
    factory.set_editor_property("target_skeleton", skeleton)
    spec_hash = digest("locomotion", axes, samples)
    blend_space = outdated(path, asset_class, factory, spec_hash)
    if blend_space is None:
        return load(path)
    structs = []
    for axis in axes:
        item = unreal.HollowBlendAxis()
        item.set_editor_property("name", axis["Name"])
        item.set_editor_property("min", axis["Min"])
        item.set_editor_property("max", axis["Max"])
        item.set_editor_property("grid_divisions", axis["GridDivisions"])
        item.set_editor_property("wrap", axis["bWrap"])
        structs.append(item)
    sample_structs = []
    for name, x, y, scale in samples:
        item = unreal.HollowBlendSampleSpec()
        item.set_editor_property("animation", load(root + name))
        item.set_editor_property("value", unreal.Vector(x, y, 0.0))
        item.set_editor_property("rate_scale", scale)
        sample_structs.append(item)
    check(authoring.set_blend_space(blend_space, structs, sample_structs), path + " takes its axes and samples")
    stamp(blend_space, spec_hash)
    log("%s: %s" % (path.rsplit("/", 1)[1], "; ".join(authoring.describe_blend_space(blend_space))))
    return blend_space


def node_names(editor):
    return editor.list_available_nodes([])


def make_node(editor, names, wanted, x, y):
    """A node by its menu name; "|GetX" is the getter of the instance's property X (under Variables)."""
    name = next((n for n in names if n == wanted), None) or next((n for n in names if n.startswith("Variables|") and n.endswith(wanted)), None)
    check(name is not None, "the anim graph offers " + wanted)
    node = editor.create_node_from_name(name, unreal.Vector2D(x, y), [])
    check(node is not None, "the anim graph makes " + name)
    return node


def pin(node, name, direction):
    found = next((p for p in node.list_all_pins() if str(p.get_pin_name()) == name and p.get_pin_direction() == direction), None)
    check(found is not None, "%s has the pin %s (%s)" % (node.get_class().get_name(), name, [str(p.get_pin_name()) for p in node.list_all_pins()]))
    return found


def first_pin(node, direction):
    found = next((p for p in node.list_all_pins() if p.get_pin_direction() == direction), None)
    check(found is not None, "%s has an %s pin" % (node.get_class().get_name(), direction))
    return found


def connect(a, b):
    check(a.try_create_connection(b), "%s -> %s" % (a.get_pin_name(), b.get_pin_name()))


def ensure_blueprint(character, skeleton, blend_space, path, kind):
    aim_offset = load(character["root"] + "/Animations/" + character["aim_offset"]) if kind == "aim" else None
    spec_hash = digest("abp", kind, blend_space.get_path_name(), aim_offset.get_path_name() if aim_offset else None,
                       assets.get_metadata_tag(blend_space, SPEC_TAG))
    factory = unreal.AnimBlueprintFactory()
    factory.set_editor_property("target_skeleton", skeleton)
    factory.set_editor_property("parent_class", unreal.HollowCharacterAnimInstance)
    blueprint = outdated(path, unreal.AnimBlueprint, factory, spec_hash)
    if blueprint is None:
        return load(path)
    editor = unreal.BlueprintGraphEditor.get_graph_editor_by_name(blueprint, "AnimGraph")
    check(editor is not None, path + " has an AnimGraph")
    editor.remove_nodes([n for n in editor.list_all_nodes() if n.get_class().get_name() != "AnimGraphNode_Root"])   # an outdated graph
    names = node_names(editor)
    out_pin, in_pin = unreal.EdGraphPinDirection.EGPD_OUTPUT, unreal.EdGraphPinDirection.EGPD_INPUT
    two_d = isinstance(blend_space, unreal.BlendSpace) and not isinstance(blend_space, unreal.BlendSpace1D)
    player = make_node(editor, names, "Animation|BlendSpaces|BlendspacePlayer'%s'" % blend_space.get_name(), -900, 0)
    speed = make_node(editor, names, "|GetSpeed", -1200, 60)
    if two_d:
        direction = make_node(editor, names, "|GetDirection", -1200, -20)
        connect(first_pin(direction, out_pin), pin(player, "X", in_pin))
        connect(first_pin(speed, out_pin), pin(player, "Y", in_pin))
    else:
        connect(first_pin(speed, out_pin), pin(player, "X", in_pin))
    pose = pin(player, "Pose", out_pin)
    if aim_offset is not None:
        aim = make_node(editor, names, "Animation|BlendSpaces|AimOffsetPlayer'%s'" % aim_offset.get_name(), -550, 0)
        pitch = make_node(editor, names, "|GetAimPitch", -800, 140)
        connect(pose, pin(aim, "BasePose", in_pin))
        connect(first_pin(pitch, out_pin), pin(aim, "Y", in_pin))
        pose = pin(aim, "Pose", out_pin)
    slot = make_node(editor, names, "Animation|Montage|Slot'%s'" % SLOT, -250, 0)
    connect(pose, pin(slot, "Source", in_pin))
    root = next((n for n in editor.list_all_nodes() if n.get_class().get_name() == "AnimGraphNode_Root"), None)
    check(root is not None, path + " has its output")
    connect(pin(slot, "Pose", out_pin), first_pin(root, in_pin))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    status = blueprint.get_editor_property("status")
    check(status in (unreal.BlueprintStatus.BS_UP_TO_DATE, unreal.BlueprintStatus.BS_UP_TO_DATE_WITH_WARNINGS), "%s compiles (%s)" % (path, status))
    stamp(blueprint, spec_hash)
    log("%s: %s%s -> Slot %s" % (path.rsplit("/", 1)[1], blend_space.get_name(), " -> " + aim_offset.get_name() if aim_offset else "", SLOT))
    return blueprint


def montage_spec(character, moment, rig):
    root = character["root"] + "/Animations/"
    kind, value = next(iter(character["montages"][moment].items()))
    if kind == "strike":
        anim = load(root + value[0])
        impact = rig.impact_time(anim, value[1])
        return kind, [(value[0], 0.0, 0.0)], [("Windup", 0.0, IMPACT), (IMPACT, impact, "")]
    if kind == "stun":
        start = load(root + value[0])
        loop_at = round(unreal.AnimationLibrary.get_sequence_length(start), 3)
        return kind, [(value[0], 0.0, 0.0), (value[1], 0.0, 0.0)], [("Start", 0.0, "Loop"), ("Loop", loop_at, "Loop")]
    return kind, [(value[0], value[1], value[2])], [("Default", 0.0, "")]


def ensure_montage(character, skeleton, moment, rig):
    root = character["root"] + "/Animations/"
    kind, segments, sections = montage_spec(character, moment, rig)
    path = "%s/AM_%s_%s" % (character["dir"], character["name"], MONTAGE_NAMES[moment])
    spec_hash = digest("montage", SLOT, segments, sections, BLENDS[kind])
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("target_skeleton", skeleton)
    montage = outdated(path, unreal.AnimMontage, factory, spec_hash)
    if montage is None:
        return load(path)
    segment_structs = []
    for name, start, end in segments:
        item = unreal.HollowMontageSegment()
        item.set_editor_property("animation", load(root + name))
        item.set_editor_property("start_time", start)
        item.set_editor_property("end_time", end)
        segment_structs.append(item)
    section_structs = []
    for name, time, following in sections:
        item = unreal.HollowMontageSection()
        item.set_editor_property("name", name)
        item.set_editor_property("time", time)
        item.set_editor_property("next", following)
        section_structs.append(item)
    blend_in, blend_out = BLENDS[kind]
    check(authoring.set_montage(montage, SLOT, segment_structs, section_structs, blend_in, blend_out), path + " takes its segments and sections")
    stamp(montage, spec_hash)
    log("%s: %s" % (path.rsplit("/", 1)[1], "; ".join(authoring.describe_montage(montage))))
    return montage


# ---------------------------------------------------------------- the characters' blueprints and data

def fit(mesh, rig, half_height):
    """Scale that makes the head's top meet the capsule's top (CROWN x the head bone above the feet)."""
    return round(2.0 * half_height / (rig.reference("head").translation.z * CROWN), 3)


def apply(component, values):
    changed = False
    for name, value in values.items():
        current = component.get_editor_property(name)
        same = (current == value) if not isinstance(value, (unreal.Vector, unreal.Rotator)) else \
            ((current - value).length() < 1e-3 if isinstance(value, unreal.Vector) else
             abs(current.pitch - value.pitch) + abs(current.yaw - value.yaw) + abs(current.roll - value.roll) < 1e-3)
        if not same:
            component.set_editor_property(name, value)
            changed = True
    return changed


def tint_parameters(mesh):
    """Each tinted parameter with the intensity that keeps its material's brightness (the brightest slot that has it)."""
    result = []
    for parameter in TINT_PARAMETERS:
        brightest = 0.0
        for slot in mesh.get_editor_property("materials"):
            material = slot.get_editor_property("material_interface")
            base = material.get_base_material() if material else None
            if base and parameter in [str(n) for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(base)]:
                color = unreal.MaterialEditingLibrary.get_material_default_vector_parameter_value(base, parameter)
                brightest = max(brightest, color.r, color.g, color.b)
        if brightest <= 0.0:
            log("%s: dark by default in Wraith's materials, not tinted" % parameter)
            continue
        item = unreal.HollowTintParameter()
        item.set_editor_property("parameter", parameter)
        item.set_editor_property("intensity", round(brightest, 3))
        result.append(item)
    check(result, "Wraith's materials glow in one of %s" % TINT_PARAMETERS)
    return result


def eyes(rig):
    """Where the first-person camera sits on the body's head: Wraith's eyes (its socket, in the head bone's space)."""
    socket = rig.component.get_skinned_asset().find_socket(CAMERA_SOCKET)
    check(socket is not None, "Wraith has the socket " + CAMERA_SOCKET)
    return socket.get_editor_property("relative_location")


def ensure_character(character, mesh, blueprints, rig):
    blueprint = load(character["bp"])
    pawn = unreal.get_default_object(blueprint.generated_class())
    half_height = pawn.get_editor_property("capsule_component").get_editor_property("capsule_half_height")
    scale = fit(mesh, rig, half_height)
    body = pawn.get_editor_property("mesh")
    exo = character["name"] == "ExoFrame"
    anim = load("%s/%s" % (character["dir"], "ABP_ExoFrame_Body" if exo else next(iter(character["blueprints"])))).generated_class()
    changed = apply(body, {"skeletal_mesh_asset": mesh, "anim_class": anim, "relative_location": unreal.Vector(0.0, 0.0, -half_height),
                           "relative_rotation": unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0), "relative_scale3d": unreal.Vector(scale, scale, scale),
                           "override_materials": []})
    if not exo:   # nothing hangs on an enemy's pose: an unseen one skips it (montage lengths are the assets')
        changed = apply(body, {"visibility_based_anim_tick_option": unreal.VisibilityBasedAnimTickOption.ONLY_TICK_POSE_WHEN_RENDERED}) or changed
    parts = pawn.get_editor_property("cosmetic_parts")
    if parts.get_editor_property("part_set") is not None:
        parts.set_editor_property("part_set", None)   # CHAR-01: the character's own mesh, no armour on a placeholder
        changed = True
    if exo:
        arms = pawn.get_editor_property("first_person_mesh")   # none: Wraith has no first-person poses (AK-4)
        changed = apply(arms, {"skeletal_mesh_asset": None, "anim_class": None, "override_materials": [], "relative_location": unreal.Vector(0.0, 0.0, 0.0),
                               "relative_rotation": unreal.Rotator(0.0, 0.0, 0.0), "relative_scale3d": unreal.Vector(1.0, 1.0, 1.0)}) or changed
        camera = pawn.get_editor_property("first_person_camera")
        changed = apply(camera, {"relative_location": eyes(rig)}) or changed
        wanted_tint = [(str(t.get_editor_property("parameter")), t.get_editor_property("intensity")) for t in tint_parameters(mesh)]
        current_tint = [(str(t.get_editor_property("parameter")), t.get_editor_property("intensity")) for t in parts.get_editor_property("tint_parameters")]
        if current_tint != wanted_tint:
            parts.set_editor_property("tint_parameters", tint_parameters(mesh))
            changed = True
        mount = pawn.get_editor_property("tool_mount")
        changed = apply(mount, {"third_person_socket": TOOL_SOCKET}) or changed
    if changed:
        save(blueprint)
        log("%s: %s x%.3f, %s" % (blueprint.get_name(), mesh.get_name(), scale, anim.get_name()))


def path_of(value):
    """An object or a soft reference as its path ("" for none)."""
    if not value:
        return ""
    return value.get_path_name() if hasattr(value, "get_path_name") else str(value)


def ensure_definition(character, montages):
    definition = load(character["definition"])
    wanted = definition.get_editor_property("montages")
    changed = False
    for moment in MONTAGE_NAMES:
        value = montages.get(moment)
        if path_of(wanted.get_editor_property(moment)) != path_of(value):
            wanted.set_editor_property(moment, value)
            changed = True
    if changed:
        definition.set_editor_property("montages", wanted)
        save(definition)
        log("%s: montages %s" % (definition.get_name(), ", ".join(sorted(MONTAGE_NAMES[m] for m in montages))))


# ---------------------------------------------------------------- validation

def validate():
    for character in CHARACTERS:
        pawn = unreal.get_default_object(load(character["bp"]).generated_class())
        body = pawn.get_editor_property("mesh")
        mesh = body.get_editor_property("skeletal_mesh_asset")
        check(mesh is not None and mesh.get_path_name().startswith(character["root"]), "%s wears %s" % (character["bp"], character["hero"]))
        check(body.get_editor_property("anim_class") is not None, character["bp"] + ": an animation blueprint")
        for name in character["blueprints"]:
            check(unreal.MathLibrary.class_is_child_of(load("%s/%s" % (character["dir"], name)).generated_class(), unreal.HollowCharacterAnimInstance.static_class()),
                  name + " is a UHollowCharacterAnimInstance")
        check(pawn.get_editor_property("cosmetic_parts").get_editor_property("part_set") is None, character["bp"] + ": no parts")
        check(mesh.get_editor_property("skeleton").get_skeleton_preview_mesh() == mesh, character["hero"] + "'s skeleton previews its own mesh")
        if hidden_slots(mesh):
            count = authoring.count_disabled_sections(mesh, hidden_slots(mesh))
            check(count.x == count.y and count.y > 0, "%s: its hidden slots' %d sections are off (%d)" % (character["hero"], count.y, count.x))
        if character["name"] == "ExoFrame":
            check(mesh.find_socket(TOOL_SOCKET) is not None, "Wraith has " + TOOL_SOCKET)
            check(str(pawn.get_editor_property("tool_mount").get_editor_property("third_person_socket")) == TOOL_SOCKET, "the body holds the tool in it")
            check(pawn.get_editor_property("first_person_mesh").get_editor_property("skeletal_mesh_asset") is None, "no first-person arms (AK-4)")
            check(len(pawn.get_editor_property("cosmetic_parts").get_editor_property("tint_parameters")) >= 1, "the visor tint")
        else:
            montages = load(character["definition"]).get_editor_property("montages")
            for moment in character["montages"]:
                montage = montages.get_editor_property(moment)
                check(montage is not None, "%s: %s montage" % (character["definition"], moment))
                if "strike" in character["montages"][moment]:
                    check(any(str(s) == IMPACT for s in [l.split(" ")[1] for l in authoring.describe_montage(montage) if l.startswith("section ")]),
                          "%s: its %s lands at %s" % (character["name"], moment, IMPACT))
    log("validation passed")


def main():
    unreal.load_module("BlueprintEditorLibrary")   # UBlueprintGraphEditor: the animation graphs
    for character in CHARACTERS:
        mesh = load("%s/Meshes/%s" % (character["root"], character["hero"]))
        rig = Rig(mesh)
        try:
            ensure_hidden_sections(mesh)
            skeleton = ensure_skeleton(character, mesh, rig)
            blend_space = ensure_locomotion(character, skeleton, rig)
            blueprints = {name: ensure_blueprint(character, skeleton, blend_space, "%s/%s" % (character["dir"], name), kind)
                          for name, kind in character["blueprints"].items()}
            montages = {moment: ensure_montage(character, skeleton, moment, rig) for moment in character["montages"]}
            ensure_character(character, mesh, blueprints, rig)
            if "definition" in character:
                ensure_definition(character, montages)
        finally:
            rig.close()
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_character_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
