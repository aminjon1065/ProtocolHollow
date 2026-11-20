"""Creates the S3-04 Iron Pilgrim content (idempotent, ADR-0010, ADR-0017).

  /Game/Hollow/GAS/Effects/GE_Damage_Kinetic                  UHollowDamageEffect (UHollowDamageExecution), Hollow.Damage.Kinetic
  /Game/Hollow/GAS/Effects/GE_Pilgrim_InitAttributes           instant Override of the Pilgrim's health set
  /Game/Hollow/GAS/Cues/GCN_Enemy_Windup                       AHollowEnemyWindupCue: greybox glow while a blow winds up
  /Game/Hollow/Characters/Enemies/IronPilgrim/
      DA_Attack_Pilgrim_Breach|Melee                           UHollowEnemyAttackDefinition (timings, reach, damage)
      GA_PilgrimBreach|GA_PilgrimMelee                         BP children of the native abilities, pointing at their DA
      DA_AbilitySet_IronPilgrim                                both attacks + the init effect
      ST_IronPilgrim                                           StateTree (AI component schema) from TREE below
      DA_Enemy_IronPilgrim                                     UHollowEnemyDefinition (movement, senses, behaviour)
      BP_IronPilgrim                                           AHollowEnemyCharacter: Heavy capsule, placeholder Manny scaled up
  DA_Door_Standard structure numbers, BP_Door damaged tint, DA_Biome_Docks rule EnemySpawn -> one BP_IronPilgrim

Creates or fills only what is missing or differs, then validates.
Run after create_raid_flow_assets.py:  Build/Scripts/run-editor-python.sh Build/Python/create_pilgrim_assets.py
"""
import json
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

EFFECTS_DIR = "/Game/Hollow/GAS/Effects"
KINETIC_EFFECT = EFFECTS_DIR + "/GE_Damage_Kinetic"
INIT_EFFECT = EFFECTS_DIR + "/GE_Pilgrim_InitAttributes"
CUE_PATH = "/Game/Hollow/GAS/Cues/GCN_Enemy_Windup"
PILGRIM_DIR = "/Game/Hollow/Characters/Enemies/IronPilgrim"
BREACH_ATTACK = PILGRIM_DIR + "/DA_Attack_Pilgrim_Breach"
MELEE_ATTACK = PILGRIM_DIR + "/DA_Attack_Pilgrim_Melee"
BREACH_ABILITY = PILGRIM_DIR + "/GA_PilgrimBreach"
MELEE_ABILITY = PILGRIM_DIR + "/GA_PilgrimMelee"
ABILITY_SET = PILGRIM_DIR + "/DA_AbilitySet_IronPilgrim"
TREE_NAME = "ST_IronPilgrim"
TREE_PATH = PILGRIM_DIR + "/" + TREE_NAME
DEFINITION = PILGRIM_DIR + "/DA_Enemy_IronPilgrim"
PILGRIM_BP = PILGRIM_DIR + "/BP_IronPilgrim"
HEAVY_FILTER = "/Game/Hollow/AI/Nav/NAV_Filter_Heavy"
DOOR_DEFINITION = "/Game/Hollow/World/Kit/Doors/DA_Door_Standard"
DOOR_BP = "/Game/Hollow/World/Kit/Doors/BP_Door"
BIOME_PATH = "/HollowSector_Docks/Data/DA_Biome_Docks"
ENEMY_SOCKET = "Hollow.Socket.EnemySpawn"

PLACEHOLDER_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"   # ADR-0011: imported per machine
PLACEHOLDER_ANIM = "/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"
ATTRIBUTE_MODULE = "/Script/HollowGAS"
EFFECT_LEVEL = 1.0

# Body: the Heavy nav agent (Docs/Architecture/03 sec. 3.7: radius 80, height 280); Manny (192 tall) scaled to fit.
CAPSULE_RADIUS = 80.0
CAPSULE_HALF_HEIGHT = 140.0
MANNY_HEIGHT = 192.0
MESH_SCALE = 2.0 * CAPSULE_HALF_HEIGHT / MANNY_HEIGHT
MESH_LOCATION = unreal.Vector(0.0, 0.0, -CAPSULE_HALF_HEIGHT)
MESH_ROTATION = unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0)

# Attributes (data): players cannot hurt it yet (S3-04 clarification 6); the numbers wait for S4 weapons.
PILGRIM_ATTRIBUTES = [("HollowHealthSet", "MaxHealth", 2000.0), ("HollowHealthSet", "Health", 2000.0), ("HollowHealthSet", "Armor", 100.0)]

# Attacks (data): property -> value.
BREACH = {"windup_seconds": 1.2, "recovery_seconds": 1.0, "reach": 200.0, "radius": 0.0, "damage": 0.0, "structural_damage": 120.0}
MELEE = {"windup_seconds": 0.8, "recovery_seconds": 1.0, "reach": 150.0, "radius": 60.0, "damage": 35.0, "structural_damage": 0.0}
WINDUP_CUE = "GameplayCue.Hollow.Enemy.Windup"

# Enemy definition (data).
DEFINITION_VALUES = {
    "patrol_speed": 180.0, "chase_speed": 330.0, "turn_rate": 240.0, "move_acceptance_radius": 75.0, "net_update_frequency": 30.0,
    "patrol_radius": 2500.0, "patrol_pause_seconds": 3.0,
    "sight_radius": 2000.0, "lose_sight_radius": 2600.0, "sight_half_angle": 60.0, "hearing_range": 3000.0, "stimulus_max_age": 10.0,
    "target_memory_seconds": 3.0,
    "stimulus_memory_seconds": 20.0, "investigate_seconds": 4.0,
    "engage_range": 120.0, "chase_stop_distance": 60.0, "obstacle_recheck_seconds": 0.5, "retry_seconds": 2.0,
    "death_linger_seconds": 5.0,
}
MELEE_EVENT = "Hollow.Event.Enemy.Melee"
BREACH_EVENT = "Hollow.Event.Enemy.Breach"

# Door structure (data): a full weld soaks 600, the bare door 300, so a welded door takes ceil(900 / 120) = 8 blows.
DOOR_STRUCTURE = {"structure_hit_points": 300.0, "weld_absorb_factor": 600.0, "torn_leaf_speed": 900.0}
DOOR_DAMAGED_COLOR = unreal.LinearColor(0.12, 0.05, 0.03, 1.0)

# Windup glow (greybox).
GLOW_COLOR = unreal.Color(r=255, g=40, b=20, a=255)
GLOW_INTENSITY = 8000.0
GLOW_RADIUS = 600.0


def state(name, conditions=(), tasks=(), transitions=(), children=()):
    return {"name": name, "conditions": list(conditions), "tasks": list(tasks), "transitions": list(transitions), "children": list(children)}


def back_to_root():
    return {"trigger": "OnStateCompleted", "target": "Root"}


# Brain: children are tried in order; any fact change (Reconsider) or finished activity picks again from the top.
TREE = state("Root", transitions=[{"trigger": "OnEvent", "event": "Hollow.AI.Event.Reconsider", "target": "Root"}], children=[
    state("Breach", ["HollowSTCond_HasObstacle"], ["HollowSTTask_Breach"], [back_to_root()]),
    state("Attack", ["HollowSTCond_HasTarget", "HollowSTCond_TargetInEngageRange"], ["HollowSTTask_Attack"], [back_to_root()]),
    state("Chase", ["HollowSTCond_HasTarget"], ["HollowSTTask_Chase"], [back_to_root()]),
    state("Investigate", ["HollowSTCond_HasStimulus"], ["HollowSTTask_Investigate"], [back_to_root()]),
    state("Patrol", [], ["HollowSTTask_Patrol"], [back_to_root()]),
])

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_pilgrim_assets: " + message)


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


def blueprint(path, parent_class):
    if assets.does_asset_exist(path):
        return load(path), False
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return create(path, unreal.Blueprint, factory), True


def data_asset(path, asset_class):
    if assets.does_asset_exist(path):
        return load(path), False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    return create(path, asset_class, factory), True


def from_text(struct, text):
    if not struct.import_text(text):
        raise RuntimeError("Could not import " + text)
    return struct


def make_tag(name):
    return from_text(unreal.GameplayTag(), '(TagName="%s")' % name)


def same(a, b):
    return a.export_text() == b.export_text() if hasattr(a, "export_text") else a == b


def apply_values(obj, values):
    """Sets the properties that differ; True if any did."""
    changed = False
    for name, value in values.items():
        current = obj.get_editor_property(name)
        if isinstance(value, float) and isinstance(current, float):
            differs = abs(current - value) > 1e-4
        else:
            differs = not same(current, value)
        if differs:
            obj.set_editor_property(name, value)
            changed = True
    return changed


def attribute_path(set_name, attribute):
    return "%s.%s:%s" % (ATTRIBUTE_MODULE, set_name, attribute)


def make_override(set_name, attribute, value):
    # Modifier fields are EditDefaultsOnly, so the struct is built from text (as in create_gas_assets.py).
    text = ('(Attribute=(AttributeName="{attr}",Attribute={path},AttributeOwner="/Script/CoreUObject.Class\'{module}.{set}\'"),'
            'ModifierOp=Override,ModifierMagnitude=(MagnitudeCalculationType=ScalableFloat,ScalableFloatMagnitude=(Value={value:.6f})))'
            ).format(attr=attribute, path=attribute_path(set_name, attribute), module=ATTRIBUTE_MODULE, set=set_name, value=value)
    modifier = from_text(unreal.GameplayModifierInfo(), text)
    if attribute_path(set_name, attribute) not in modifier.export_text():
        raise RuntimeError("Could not build the %s.%s modifier" % (set_name, attribute))
    return modifier


# ---------------------------------------------------------------- effects and cue

def ensure_kinetic_effect():
    effect_bp, created = blueprint(KINETIC_EFFECT, unreal.HollowDamageEffect)
    effect = unreal.get_default_object(effect_bp.generated_class())
    if created or not same(effect.get_editor_property("damage_type"), make_tag("Hollow.Damage.Kinetic")):
        effect.set_editor_property("damage_type", make_tag("Hollow.Damage.Kinetic"))
        save(effect_bp)
        log("GE_Damage_Kinetic ready")
    return effect_bp.generated_class()


def ensure_init_effect():
    effect_bp, created = blueprint(INIT_EFFECT, unreal.GameplayEffect)
    if created:
        effect = unreal.get_default_object(effect_bp.generated_class())
        effect.set_editor_property("duration_policy", unreal.GameplayEffectDurationType.INSTANT)
        effect.set_editor_property("modifiers", [make_override(*entry) for entry in PILGRIM_ATTRIBUTES])
        save(effect_bp)
        log("created GE_Pilgrim_InitAttributes")
    return effect_bp.generated_class()


def ensure_cue():
    cue_bp, created = blueprint(CUE_PATH, unreal.HollowEnemyWindupCue)
    cue = unreal.get_default_object(cue_bp.generated_class())
    glow = cue.get_editor_property("glow")
    changed = created
    if not same(cue.get_editor_property("gameplay_cue_tag"), make_tag(WINDUP_CUE)):
        cue.set_editor_property("gameplay_cue_tag", make_tag(WINDUP_CUE))
        changed = True
    changed = apply_values(glow, {"intensity": GLOW_INTENSITY, "attenuation_radius": GLOW_RADIUS, "cast_shadows": False}) or changed
    if not same(glow.get_editor_property("light_color"), GLOW_COLOR):
        glow.set_editor_property("light_color", GLOW_COLOR)
        changed = True
    if changed:
        save(cue_bp)
        log("GCN_Enemy_Windup ready")


# ---------------------------------------------------------------- attacks, abilities, ability set

def ensure_attack(path, values, damage_effect):
    attack, created = data_asset(path, unreal.HollowEnemyAttackDefinition)
    wanted = dict(values)
    wanted["windup_cue"] = make_tag(WINDUP_CUE)
    changed = apply_values(attack, wanted)
    if values["damage"] > 0.0 and attack.get_editor_property("damage_effect") != damage_effect:
        attack.set_editor_property("damage_effect", damage_effect)
        changed = True
    if created or changed:
        save(attack)
        log(path.rsplit("/", 1)[1] + " ready")
    return attack


def ensure_ability(path, parent_class, attack):
    ability_bp, created = blueprint(path, parent_class)
    ability = unreal.get_default_object(ability_bp.generated_class())
    if created or ability.get_editor_property("attack") != attack:
        ability.set_editor_property("attack", attack)
        save(ability_bp)
        log(path.rsplit("/", 1)[1] + " ready")
    return ability_bp.generated_class()


def ensure_ability_set(abilities, init_effect):
    ability_set, created = data_asset(ABILITY_SET, unreal.HollowAbilitySet)
    granted = list(ability_set.get_editor_property("granted_abilities"))
    effects = list(ability_set.get_editor_property("granted_effects"))
    changed = created
    for ability_class in abilities:
        if not any(e.get_editor_property("ability") == ability_class for e in granted):
            entry = unreal.HollowAbilitySetAbility()
            entry.set_editor_property("ability", ability_class)
            granted.append(entry)
            changed = True
    if not any(e.get_editor_property("gameplay_effect") == init_effect for e in effects):
        entry = unreal.HollowAbilitySetEffect()
        entry.set_editor_property("gameplay_effect", init_effect)
        entry.set_editor_property("effect_level", EFFECT_LEVEL)
        effects.append(entry)
        changed = True
    if changed:
        ability_set.set_editor_property("granted_abilities", granted)
        ability_set.set_editor_property("granted_effects", effects)
        save(ability_set)
        log("DA_AbilitySet_IronPilgrim ready")
    return ability_set


# ---------------------------------------------------------------- brain, definition, pawn

def ensure_tree():
    tree = load(TREE_PATH) if assets.does_asset_exist(TREE_PATH) else unreal.HollowStateTreeAuthoring.create_ai_state_tree(PILGRIM_DIR, TREE_NAME)
    if tree is None:
        raise RuntimeError("Could not create " + TREE_PATH)
    described = unreal.HollowStateTreeAuthoring.describe_state_tree(tree)
    if described and json.loads(described) == TREE:
        return tree
    error = unreal.HollowStateTreeAuthoring.build_state_tree(tree, json.dumps(TREE))
    if error:
        raise RuntimeError("ST_IronPilgrim: " + error)
    save(tree)
    log("ST_IronPilgrim built")
    return tree


def ensure_definition(ability_set, tree):
    definition, created = data_asset(DEFINITION, unreal.HollowEnemyDefinition)
    changed = apply_values(definition, DEFINITION_VALUES)
    wanted_objects = {"ability_set": ability_set, "state_tree": tree, "navigation_filter": load(HEAVY_FILTER).generated_class()}
    for name, value in wanted_objects.items():
        if definition.get_editor_property(name) != value:
            definition.set_editor_property(name, value)
            changed = True
    changed = apply_values(definition, {"melee_event": make_tag(MELEE_EVENT), "breach_event": make_tag(BREACH_EVENT)}) or changed
    if created or changed:
        save(definition)
        log("DA_Enemy_IronPilgrim ready")
    return definition


def ensure_pawn(definition):
    pawn_bp, created = blueprint(PILGRIM_BP, unreal.HollowEnemyCharacter)
    pawn = unreal.get_default_object(pawn_bp.generated_class())
    changed = created
    if pawn.get_editor_property("definition") != definition:
        pawn.set_editor_property("definition", definition)
        changed = True
    capsule = pawn.get_editor_property("capsule_component")
    changed = apply_values(capsule, {"capsule_radius": CAPSULE_RADIUS, "capsule_half_height": CAPSULE_HALF_HEIGHT}) or changed
    mesh = pawn.get_editor_property("mesh")
    if mesh.get_editor_property("skeletal_mesh_asset") is None:
        mesh.set_editor_property("skeletal_mesh_asset", load(PLACEHOLDER_MESH))
        mesh.set_editor_property("anim_class", load(PLACEHOLDER_ANIM).generated_class())
        changed = True
    changed = apply_values(mesh, {"relative_location": MESH_LOCATION, "relative_rotation": MESH_ROTATION,
                                  "relative_scale3d": unreal.Vector(MESH_SCALE, MESH_SCALE, MESH_SCALE)}) or changed
    if changed:
        save(pawn_bp)
        log("BP_IronPilgrim ready")
    return pawn_bp.generated_class()


# ---------------------------------------------------------------- door, biome

def ensure_door():
    definition = load(DOOR_DEFINITION)
    if apply_values(definition, DOOR_STRUCTURE):
        save(definition)
        log("DA_Door_Standard: structure set")
    door_bp = load(DOOR_BP)
    door = unreal.get_default_object(door_bp.generated_class())
    if not same(door.get_editor_property("damaged_color"), DOOR_DAMAGED_COLOR):
        door.set_editor_property("damaged_color", DOOR_DAMAGED_COLOR)
        save(door_bp)
        log("BP_Door: damaged tint set")


def ensure_biome_rule(pilgrim_class):
    biome = load(BIOME_PATH)
    tag = make_tag(ENEMY_SOCKET)
    offset = unreal.Vector(0.0, 0.0, CAPSULE_HALF_HEIGHT)   # the capsule stands on the floor socket
    rules = list(biome.get_editor_property("socket_actors"))
    mine = [r for r in rules if same(r.get_editor_property("socket_type"), tag) and r.get_editor_property("actor_class") == pilgrim_class]
    rule = mine[0] if mine else unreal.HollowSocketActorRule()
    wanted = {"socket_type": tag, "spawn_offset": offset, "max_per_sector": 1, "pick": unreal.HollowSocketPick.FARTHEST_FROM_SPAWN}
    if mine and not apply_values(rule, wanted):
        return
    rule.set_editor_property("actor_class", pilgrim_class)
    apply_values(rule, wanted)
    if mine:
        rules[rules.index(mine[0])] = rule
    else:
        # Before the marker rule of the socket: the Pilgrim takes its one socket first, markers keep the rest.
        first = next((i for i, r in enumerate(rules) if same(r.get_editor_property("socket_type"), tag)), len(rules))
        rules.insert(first, rule)
    biome.set_editor_property("socket_actors", rules)
    save(biome)
    log("DA_Biome_Docks: one BP_IronPilgrim at the EnemySpawn socket farthest from Spawn")


# ---------------------------------------------------------------- validation

def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate(pilgrim_class):
    definition = load(DEFINITION)
    check(json.loads(unreal.HollowStateTreeAuthoring.describe_state_tree(load(TREE_PATH))) == TREE, "ST_IronPilgrim matches the spec")
    check(definition.get_editor_property("state_tree") == load(TREE_PATH), "definition brain")
    check(definition.get_editor_property("chase_stop_distance") < definition.get_editor_property("engage_range"), "chase stops in strike range")
    melee = load(MELEE_ATTACK)
    check(melee.get_editor_property("reach") >= definition.get_editor_property("engage_range"), "melee reaches as far as the brain engages")
    check(melee.get_editor_property("damage_effect") == load(KINETIC_EFFECT).generated_class(), "melee damage effect")
    ability_set = load(ABILITY_SET)
    check(len(ability_set.get_editor_property("granted_abilities")) == 2 and len(ability_set.get_editor_property("granted_effects")) == 1, "ability set")
    pawn = unreal.get_default_object(pilgrim_class)
    check(pawn.get_editor_property("definition") == definition, "BP_IronPilgrim definition")
    check(abs(pawn.get_editor_property("capsule_component").get_editor_property("capsule_radius") - CAPSULE_RADIUS) < 1e-3, "Heavy capsule")
    check(load(DOOR_DEFINITION).get_editor_property("structure_hit_points") > 0.0, "door structure")
    rules = load(BIOME_PATH).get_editor_property("socket_actors")
    tag = make_tag(ENEMY_SOCKET)
    enemy_rules = [r for r in rules if same(r.get_editor_property("socket_type"), tag)]
    check(enemy_rules and enemy_rules[0].get_editor_property("actor_class") == pilgrim_class
          and enemy_rules[0].get_editor_property("max_per_sector") == 1, "biome spawns one Pilgrim first")
    cue = unreal.get_default_object(load(CUE_PATH).generated_class())
    check(same(cue.get_editor_property("gameplay_cue_tag"), make_tag(WINDUP_CUE)), "wind-up cue tag")
    log("validation passed")


def main():
    kinetic = ensure_kinetic_effect()
    init_effect = ensure_init_effect()
    ensure_cue()
    breach = ensure_attack(BREACH_ATTACK, BREACH, kinetic)
    melee = ensure_attack(MELEE_ATTACK, MELEE, kinetic)
    abilities = [ensure_ability(BREACH_ABILITY, unreal.HollowGA_PilgrimBreach, breach), ensure_ability(MELEE_ABILITY, unreal.HollowGA_PilgrimMelee, melee)]
    ability_set = ensure_ability_set(abilities, init_effect)
    tree = ensure_tree()
    definition = ensure_definition(ability_set, tree)
    pilgrim_class = ensure_pawn(definition)
    ensure_door()
    ensure_biome_rule(pilgrim_class)
    validate(pilgrim_class)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_pilgrim_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
