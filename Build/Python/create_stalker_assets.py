"""Creates the S4-01 Echo-Stalker content (idempotent, ADR-0010, ADR-0019).

  /Game/Hollow/GAS/Effects/GE_Damage_Occult                  UHollowDamageEffect, Hollow.Damage.Occult (armor does not stop it)
  /Game/Hollow/GAS/Effects/GE_Stalker_InitAttributes          instant Override of the stalker's health set
  /Game/Hollow/GAS/Effects/GE_Stalker_Traits                  UHollowStalkerTraitsEffect: Hollow.State.Invulnerable.Kinetic
  /Game/Hollow/Characters/Enemies/EchoStalker/
      DA_Attack_Stalker_Ambush, GA_StalkerAmbush               the strike out of an ambush (UHollowGA_StalkerAmbush)
      DA_AbilitySet_EchoStalker                                the strike + init + traits
      EQ_StalkerFocusRoom                                      EQS from QUERY below: a dark, reachable spot near the hot portal
      ST_EchoStalker                                           StateTree from TREE below
      DA_Enemy_EchoStalker                                     UHollowStalkerDefinition
      BP_EchoStalker                                           AHollowEnemyCharacter, AHollowStalkerController, Standard capsule
  /Game/Hollow/AI/DA_ThreatConfig                             UHollowThreatConfig: heat, vents, the director's leash
                                                              (UHollowAISettings::ThreatConfig in DefaultGame.ini)

Creates or fills only what is missing or differs, then validates.
Run after create_pilgrim_assets.py:  Build/Scripts/run-editor-python.sh Build/Python/create_stalker_assets.py
"""
import json
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

EFFECTS_DIR = "/Game/Hollow/GAS/Effects"
OCCULT_EFFECT = EFFECTS_DIR + "/GE_Damage_Occult"
INIT_EFFECT = EFFECTS_DIR + "/GE_Stalker_InitAttributes"
TRAITS_EFFECT = EFFECTS_DIR + "/GE_Stalker_Traits"
STALKER_DIR = "/Game/Hollow/Characters/Enemies/EchoStalker"
AMBUSH_ATTACK = STALKER_DIR + "/DA_Attack_Stalker_Ambush"
AMBUSH_ABILITY = STALKER_DIR + "/GA_StalkerAmbush"
ABILITY_SET = STALKER_DIR + "/DA_AbilitySet_EchoStalker"
QUERY_NAME = "EQ_StalkerFocusRoom"
QUERY_PATH = STALKER_DIR + "/" + QUERY_NAME
TREE_NAME = "ST_EchoStalker"
TREE_PATH = STALKER_DIR + "/" + TREE_NAME
DEFINITION = STALKER_DIR + "/DA_Enemy_EchoStalker"
STALKER_BP = STALKER_DIR + "/BP_EchoStalker"
STANDARD_FILTER = "/Game/Hollow/AI/Nav/NAV_Filter_Standard"
THREAT_CONFIG = "/Game/Hollow/AI/DA_ThreatConfig"

ATTRIBUTE_MODULE = "/Script/HollowGAS"
EFFECT_LEVEL = 1.0

# Body: the Standard nav agent (radius 45, height 220, DefaultEngine.ini); the Paragon Sevarog mesh is fitted to it by
# create_character_assets.py (CHAR-01, ADR-0055).
CAPSULE_RADIUS = 45.0
CAPSULE_HALF_HEIGHT = 110.0
PAWN_RESPONSE = unreal.CollisionResponseType.ECR_OVERLAP

# Attributes (data): kinetic weapons do nothing (traits); the health is there for the occult tools of later sprints.
STALKER_ATTRIBUTES = [("HollowHealthSet", "MaxHealth", 1000.0), ("HollowHealthSet", "Health", 1000.0), ("HollowHealthSet", "Armor", 0.0)]

# The strike (data): 30 occult damage after a short wind-up.
AMBUSH = {"windup_seconds": 0.5, "recovery_seconds": 0.8, "reach": 170.0, "radius": 70.0, "damage": 30.0, "structural_damage": 0.0}
WINDUP_CUE = "GameplayCue.Hollow.Enemy.Windup"
AMBUSH_EVENT = "Hollow.Event.Enemy.Ambush"

# Definition (data): patrol speed is its shadowing speed, chase speed its hunting speed (S4-01 clarification).
DEFINITION_VALUES = {
    "patrol_speed": 220.0, "chase_speed": 480.0, "turn_rate": 360.0, "move_acceptance_radius": 60.0, "net_update_frequency": 30.0,
    "patrol_radius": 2000.0, "patrol_pause_seconds": 2.0,
    "sight_radius": 1800.0, "lose_sight_radius": 2300.0, "sight_half_angle": 70.0, "hearing_range": 3500.0, "stimulus_max_age": 10.0,
    "target_memory_seconds": 2.0,
    "stimulus_memory_seconds": 15.0, "investigate_seconds": 3.0,
    "engage_range": 140.0, "chase_stop_distance": 60.0, "obstacle_recheck_seconds": 0.5, "retry_seconds": 2.0,
    "death_linger_seconds": 5.0,
    "ambush_wait_seconds": 25.0, "ambush_trigger_range": 140.0, "ambush_check_seconds": 0.2, "track_pause_seconds": 3.0,
}

# Threat config (data, ADR-0019).
HEATMAP = {"sample_seconds": 1.0, "presence_per_second": 1.0, "noise_per_loudness": 2.0, "crossing_heat": 3.0, "half_life_seconds": 90.0}
VENTS = {"max_room_links": 2, "seconds_per_link": 3.0}
DIRECTOR = {"spawn_delay_seconds": 60.0, "spawn_search_extent": 300.0, "stalk_seconds": 45.0, "hunt_heat_threshold": 40.0,
            "hunt_seconds": 60.0, "retreat_seconds": 30.0, "heat_check_seconds": 2.0}

# EQS: points on a grid of the focus room, out of every lamp (body height), reachable on the Standard filter, not on top of
# the hottest portal but as close to it as possible.
QUERY = {"options": [{
    "generator": {"class": "/Script/HollowAI.HollowEnvQueryGenerator_FocusRoomPoints",
                  "properties": {"PointSpacing": "100.000000", "WallMargin": "60.000000"}},
    "tests": [
        {"class": "/Script/HollowAI.HollowEnvQueryTest_OutOfLightCone",
         "properties": {"PointHeight": "100.000000", "TestPurpose": "Filter", "FilterType": "Match", "BoolValue": "(DefaultValue=True)"}},
        {"class": "/Script/AIModule.EnvQueryTest_Pathfinding",
         "properties": {"TestMode": "PathExist", "TestPurpose": "Filter", "FilterType": "Match", "BoolValue": "(DefaultValue=True)",
                        "PathFromContext": "(DefaultValue=True)", "SkipUnreachable": "(DefaultValue=True)",
                        "FilterClass": STANDARD_FILTER + ".NAV_Filter_Standard_C"}},
        {"class": "/Script/AIModule.EnvQueryTest_Distance",
         "properties": {"TestMode": "Distance3D", "DistanceTo": "/Script/HollowAI.HollowEnvQueryContext_FocusRoomHotPortal",
                        "TestPurpose": "FilterAndScore", "FilterType": "Minimum", "FloatValueMin": "(DefaultValue=150.000000)",
                        "ScoringEquation": "InverseLinear"}},
    ]}]}


def state(name, conditions=(), tasks=(), transitions=(), children=()):
    return {"name": name, "conditions": list(conditions), "tasks": list(tasks), "transitions": list(transitions), "children": list(children)}


def back_to_root():
    return {"trigger": "OnStateCompleted", "target": "Root"}


# Brain: children are tried in order; any fact change (Reconsider, the director's mode) or finished activity picks again.
TREE = state("Root", transitions=[{"trigger": "OnEvent", "event": "Hollow.AI.Event.Reconsider", "target": "Root"}], children=[
    state("Stunned", ["HollowSTCond_IsStunned"], ["HollowSTTask_Stunned"], [back_to_root()]),
    state("OpenDoor", ["HollowSTCond_HasObstacle"], ["HollowSTTask_Breach"], [back_to_root()]),
    state("Retreat", ["HollowSTCond_IsRetreating"], ["HollowSTTask_Retreat"], [back_to_root()]),
    state("Attack", ["HollowSTCond_IsHunting", "HollowSTCond_HasTarget", "HollowSTCond_TargetInEngageRange"], ["HollowSTTask_Attack"], [back_to_root()]),
    state("Ambush", ["HollowSTCond_IsHunting"], ["HollowSTTask_Ambush"], [back_to_root()]),
    state("Search", ["HollowSTCond_HasStimulus"], ["HollowSTTask_Investigate"], [back_to_root()]),
    state("Track", [], ["HollowSTTask_Track"], [back_to_root()]),
])

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_stalker_assets: " + message)


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


# ---------------------------------------------------------------- effects

def ensure_occult_effect():
    effect_bp, created = blueprint(OCCULT_EFFECT, unreal.HollowDamageEffect)
    effect = unreal.get_default_object(effect_bp.generated_class())
    if created or not same(effect.get_editor_property("damage_type"), make_tag("Hollow.Damage.Occult")):
        effect.set_editor_property("damage_type", make_tag("Hollow.Damage.Occult"))
        save(effect_bp)
        log("GE_Damage_Occult ready")
    return effect_bp.generated_class()


def ensure_init_effect():
    effect_bp, created = blueprint(INIT_EFFECT, unreal.GameplayEffect)
    if created:
        effect = unreal.get_default_object(effect_bp.generated_class())
        effect.set_editor_property("duration_policy", unreal.GameplayEffectDurationType.INSTANT)
        effect.set_editor_property("modifiers", [make_override(*entry) for entry in STALKER_ATTRIBUTES])
        save(effect_bp)
        log("created GE_Stalker_InitAttributes")
    return effect_bp.generated_class()


def ensure_traits_effect():
    effect_bp, created = blueprint(TRAITS_EFFECT, unreal.HollowStalkerTraitsEffect)
    if created:
        save(effect_bp)
        log("created GE_Stalker_Traits")
    return effect_bp.generated_class()


# ---------------------------------------------------------------- attack, ability, ability set

def ensure_attack(damage_effect):
    attack, created = data_asset(AMBUSH_ATTACK, unreal.HollowEnemyAttackDefinition)
    wanted = dict(AMBUSH)
    wanted["windup_cue"] = make_tag(WINDUP_CUE)
    wanted["montage"] = unreal.HollowEnemyMontage.MELEE   # the strike out of an ambush (ADR-0027)
    changed = apply_values(attack, wanted)
    if attack.get_editor_property("damage_effect") != damage_effect:
        attack.set_editor_property("damage_effect", damage_effect)
        changed = True
    if created or changed:
        save(attack)
        log("DA_Attack_Stalker_Ambush ready")
    return attack


def ensure_ability(attack):
    ability_bp, created = blueprint(AMBUSH_ABILITY, unreal.HollowGA_StalkerAmbush)
    ability = unreal.get_default_object(ability_bp.generated_class())
    if created or ability.get_editor_property("attack") != attack:
        ability.set_editor_property("attack", attack)
        save(ability_bp)
        log("GA_StalkerAmbush ready")
    return ability_bp.generated_class()


def ensure_ability_set(ability_class, effect_classes):
    ability_set, created = data_asset(ABILITY_SET, unreal.HollowAbilitySet)
    granted = list(ability_set.get_editor_property("granted_abilities"))
    effects = list(ability_set.get_editor_property("granted_effects"))
    changed = created
    if not any(e.get_editor_property("ability") == ability_class for e in granted):
        entry = unreal.HollowAbilitySetAbility()
        entry.set_editor_property("ability", ability_class)
        granted.append(entry)
        changed = True
    for effect_class in effect_classes:
        if not any(e.get_editor_property("gameplay_effect") == effect_class for e in effects):
            entry = unreal.HollowAbilitySetEffect()
            entry.set_editor_property("gameplay_effect", effect_class)
            entry.set_editor_property("effect_level", EFFECT_LEVEL)
            effects.append(entry)
            changed = True
    if changed:
        ability_set.set_editor_property("granted_abilities", granted)
        ability_set.set_editor_property("granted_effects", effects)
        save(ability_set)
        log("DA_AbilitySet_EchoStalker ready")
    return ability_set


# ---------------------------------------------------------------- query, brain, definition, pawn

def ensure_query():
    query = load(QUERY_PATH) if assets.does_asset_exist(QUERY_PATH) else unreal.HollowEnvQueryAuthoring.create_env_query(STALKER_DIR, QUERY_NAME)
    if query is None:
        raise RuntimeError("Could not create " + QUERY_PATH)
    if unreal.HollowEnvQueryAuthoring.env_query_matches(query, json.dumps(QUERY)):
        return query
    error = unreal.HollowEnvQueryAuthoring.build_env_query(query, json.dumps(QUERY))
    if error:
        raise RuntimeError(QUERY_NAME + ": " + error)
    save(query)
    log(QUERY_NAME + " built")
    return query


def ensure_tree():
    tree = load(TREE_PATH) if assets.does_asset_exist(TREE_PATH) else unreal.HollowStateTreeAuthoring.create_ai_state_tree(STALKER_DIR, TREE_NAME)
    if tree is None:
        raise RuntimeError("Could not create " + TREE_PATH)
    described = unreal.HollowStateTreeAuthoring.describe_state_tree(tree)
    if described and json.loads(described) == TREE:
        return tree
    error = unreal.HollowStateTreeAuthoring.build_state_tree(tree, json.dumps(TREE))
    if error:
        raise RuntimeError(TREE_NAME + ": " + error)
    save(tree)
    log(TREE_NAME + " built")
    return tree


def ensure_definition(ability_set, tree, query):
    definition, created = data_asset(DEFINITION, unreal.HollowStalkerDefinition)
    changed = apply_values(definition, DEFINITION_VALUES)
    wanted_objects = {"ability_set": ability_set, "state_tree": tree, "focus_query": query,
                      "navigation_filter": load(STANDARD_FILTER).generated_class()}
    for name, value in wanted_objects.items():
        if definition.get_editor_property(name) != value:
            definition.set_editor_property(name, value)
            changed = True
    changed = apply_values(definition, {"melee_event": make_tag(AMBUSH_EVENT), "breach_event": unreal.GameplayTag()}) or changed
    if created or changed:
        save(definition)
        log("DA_Enemy_EchoStalker ready")
    return definition


def ensure_pawn(definition):
    pawn_bp, created = blueprint(STALKER_BP, unreal.HollowEnemyCharacter)
    pawn = unreal.get_default_object(pawn_bp.generated_class())
    changed = created
    if pawn.get_editor_property("definition") != definition:
        pawn.set_editor_property("definition", definition)
        changed = True
    if pawn.get_editor_property("ai_controller_class") != unreal.HollowStalkerController.static_class():
        pawn.set_editor_property("ai_controller_class", unreal.HollowStalkerController.static_class())
        changed = True
    capsule = pawn.get_editor_property("capsule_component")
    changed = apply_values(capsule, {"capsule_radius": CAPSULE_RADIUS, "capsule_half_height": CAPSULE_HALF_HEIGHT}) or changed
    # An echo: bodies do not stop it (a player in a doorway cannot pin it), nor does it shove them; traces and sweeps
    # by object type still find it (the pulse mine, perception) and its strike still finds them.
    if capsule.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN) != PAWN_RESPONSE:
        capsule.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN, PAWN_RESPONSE)
        changed = True
    if changed:
        save(pawn_bp)
        log("BP_EchoStalker ready")
    return pawn_bp.generated_class()


# ---------------------------------------------------------------- threat config

def ensure_threat_config(stalker_class):
    config, created = data_asset(THREAT_CONFIG, unreal.HollowThreatConfig)
    changed = created
    for name, values in (("heatmap", HEATMAP), ("vents", VENTS), ("director", DIRECTOR)):
        tuning = config.get_editor_property(name)
        if apply_values(tuning, values):
            config.set_editor_property(name, tuning)
            changed = True
    director = config.get_editor_property("director")
    if director.get_editor_property("stalker_class") != stalker_class:
        director.set_editor_property("stalker_class", stalker_class)
        config.set_editor_property("director", director)
        changed = True
    if changed:
        save(config)
        log("DA_ThreatConfig ready")
    return config


# ---------------------------------------------------------------- validation

def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate(stalker_class):
    definition = load(DEFINITION)
    check(json.loads(unreal.HollowStateTreeAuthoring.describe_state_tree(load(TREE_PATH))) == TREE, "ST_EchoStalker matches the spec")
    check(unreal.HollowEnvQueryAuthoring.env_query_matches(load(QUERY_PATH), json.dumps(QUERY)), "EQ_StalkerFocusRoom matches the spec")
    check(definition.get_editor_property("state_tree") == load(TREE_PATH), "definition brain")
    check(definition.get_editor_property("focus_query") == load(QUERY_PATH), "definition focus query")
    check(definition.get_editor_property("chase_stop_distance") < definition.get_editor_property("engage_range"), "chase stops in strike range")
    attack = load(AMBUSH_ATTACK)
    check(attack.get_editor_property("reach") >= definition.get_editor_property("ambush_trigger_range"), "the strike reaches the ambush range")
    check(attack.get_editor_property("reach") >= definition.get_editor_property("engage_range"), "the strike reaches the engage range")
    check(attack.get_editor_property("damage_effect") == load(OCCULT_EFFECT).generated_class(), "occult damage effect")
    ability_set = load(ABILITY_SET)
    check(len(ability_set.get_editor_property("granted_abilities")) == 1 and len(ability_set.get_editor_property("granted_effects")) == 2, "ability set")
    pawn = unreal.get_default_object(stalker_class)
    check(pawn.get_editor_property("definition") == definition, "BP_EchoStalker definition")
    check(pawn.get_editor_property("ai_controller_class") == unreal.HollowStalkerController.static_class(), "stalker controller")
    capsule = pawn.get_editor_property("capsule_component")
    check(abs(capsule.get_editor_property("capsule_radius") - CAPSULE_RADIUS) < 1e-3, "Standard capsule")
    check(capsule.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN) == PAWN_RESPONSE, "pawns pass through it")
    config = load(THREAT_CONFIG)
    check(config.get_editor_property("director").get_editor_property("stalker_class") == stalker_class, "director spawns BP_EchoStalker")
    # UHollowAISettings is not exposed to Python: read the config file it comes from.
    with open(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_config_dir()) + "DefaultGame.ini") as ini:
        wanted = "ThreatConfig=%s.%s" % (THREAT_CONFIG, THREAT_CONFIG.rsplit("/", 1)[1])
        check(any(line.strip() == wanted for line in ini), "UHollowAISettings::ThreatConfig points at DA_ThreatConfig (DefaultGame.ini)")
    log("validation passed")


def main():
    occult = ensure_occult_effect()
    init = ensure_init_effect()
    traits = ensure_traits_effect()
    attack = ensure_attack(occult)
    ability = ensure_ability(attack)
    ability_set = ensure_ability_set(ability, [init, traits])
    query = ensure_query()
    tree = ensure_tree()
    definition = ensure_definition(ability_set, tree, query)
    stalker_class = ensure_pawn(definition)
    ensure_threat_config(stalker_class)
    validate(stalker_class)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_stalker_assets failed:\n" + traceback.format_exc())
