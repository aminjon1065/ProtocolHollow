"""Creates the GP-02 Whisperer content (idempotent, ADR-0010, ADR-0047).

  /Game/Hollow/GAS/Effects/GE_Whisperer_InitAttributes        instant Override of a Whisperer's health set
  /Game/Hollow/GAS/Effects/GE_Whisperer_Lit                   UHollowLitEffect: Hollow.State.Lit, slowed, while a beam is on it
  /Game/Hollow/GAS/Effects/GE_Whisperer_Frenzy                UHollowFrenzyEffect: Hollow.State.Frenzy, faster, in the dark
  /Game/Hollow/GAS/Cues/GCN_Whisperer_Windup                  AHollowEnemyWindupCue: a small, dim glow while a bite winds up
  /Game/Hollow/Characters/Enemies/Whisperer/
      DA_Attack_Whisperer_Bite, GA_WhispererBite                its bite (UHollowGA_WhispererBite)
      DA_AbilitySet_Whisperer                                   the bite + init
      EQ_WhispererFlank                                         EQS from QUERY below: a dark, reachable spot by its prey
      ST_Whisperer                                              StateTree from TREE below
      DA_Enemy_Whisperer                                        UHollowWhispererDefinition (and its drop: an Aether shard)
      BP_Whisperer                                              AHollowWhisperer, AHollowWhispererController, small capsule
      BP_WhispererNest                                          AHollowWhispererNest: 2..4 of BP_Whisperer by its seed
  /Game/Hollow/AI/DA_ThreatConfig                             its Swarm tuning (light checks, bite slots)
  DA_Biome_Docks / DA_Biome_Cathedral                         EnemySpawn -> up to two nests, two rooms or more from Spawn
                                                              (right after the Iron Pilgrim's rule)
  Config/DefaultGame.ini                                      UHollowAISettings::DevNestClass (checked)

Creates or fills only what is missing or differs, then validates. Needs the art kit's SM_Whisperer_* (create_art_kit.py),
the loot pickup (create_loot_assets.py), the stalker's DA_ThreatConfig (create_stalker_assets.py) and the weapons'
GE_Status_Stagger (create_weapon_assets.py). Run after create_art_kit.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_whisperer_assets.py
"""
import json
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

EFFECTS_DIR = "/Game/Hollow/GAS/Effects"
KINETIC_EFFECT = EFFECTS_DIR + "/GE_Damage_Kinetic"
STAGGER_EFFECT = EFFECTS_DIR + "/GE_Status_Stagger"
INIT_EFFECT = EFFECTS_DIR + "/GE_Whisperer_InitAttributes"
LIT_EFFECT = EFFECTS_DIR + "/GE_Whisperer_Lit"
FRENZY_EFFECT = EFFECTS_DIR + "/GE_Whisperer_Frenzy"
CUE_PATH = "/Game/Hollow/GAS/Cues/GCN_Whisperer_Windup"
WHISPERER_DIR = "/Game/Hollow/Characters/Enemies/Whisperer"
BITE_ATTACK = WHISPERER_DIR + "/DA_Attack_Whisperer_Bite"
BITE_ABILITY = WHISPERER_DIR + "/GA_WhispererBite"
ABILITY_SET = WHISPERER_DIR + "/DA_AbilitySet_Whisperer"
QUERY_NAME = "EQ_WhispererFlank"
QUERY_PATH = WHISPERER_DIR + "/" + QUERY_NAME
TREE_NAME = "ST_Whisperer"
TREE_PATH = WHISPERER_DIR + "/" + TREE_NAME
DEFINITION = WHISPERER_DIR + "/DA_Enemy_Whisperer"
WHISPERER_BP = WHISPERER_DIR + "/BP_Whisperer"
NEST_BP = WHISPERER_DIR + "/BP_WhispererNest"
STANDARD_FILTER = "/Game/Hollow/AI/Nav/NAV_Filter_Standard"
THREAT_CONFIG = "/Game/Hollow/AI/DA_ThreatConfig"
PICKUP_BP = "/Game/Hollow/World/Loot/BP_LootPickup"
BODY_MESH = "/Game/Hollow/Art/Kit/SM_Whisperer_Body"
NEST_MESH = "/Game/Hollow/Art/Kit/SM_Whisperer_Nest"
BIOMES = {"Docks": "/HollowSector_Docks/Data/DA_Biome_Docks", "Cathedral": "/HollowSector_Cathedral/Data/DA_Biome_Cathedral"}
ENEMY_SOCKET = "Hollow.Socket.EnemySpawn"
PILGRIM_BP = "/Game/Hollow/Characters/Enemies/IronPilgrim/BP_IronPilgrim"
ATTRIBUTE_MODULE = "/Script/HollowGAS"
EFFECT_LEVEL = 1.0
INI_SECTION = "[/Script/HollowAI.HollowAISettings]"
INI_LINE = "DevNestClass=%s.%s_C" % (NEST_BP, NEST_BP.rsplit("/", 1)[1])

# Body: a crawler a forearm long (SM_Whisperer_Body, its origin on the floor) in a small capsule; it walks the Standard
# navmesh (the smallest agent). Bodies pass through it and it through them (the stalker's way: a swarm never pins a
# player in a doorway); shots, blows, the lights and the pulse mine still find the capsule.
CAPSULE_RADIUS = 28.0
CAPSULE_HALF_HEIGHT = 28.0
BODY_LOCATION = unreal.Vector(0.0, 0.0, -CAPSULE_HALF_HEIGHT)
PAWN_RESPONSE = unreal.CollisionResponseType.ECR_OVERLAP
# A swarm keeps apart as it moves (the movement's RVO avoidance, server side, no extra traffic).
MOVEMENT = {"use_rvo_avoidance": True, "avoidance_consideration_radius": 180.0, "avoidance_weight": 0.5}

# Attributes (data): the nailgun's 20 a nail kills it in three, a close shotgun blast at once, a blow in three.
WHISPERER_ATTRIBUTES = [("HollowHealthSet", "MaxHealth", 60.0), ("HollowHealthSet", "Health", 60.0), ("HollowHealthSet", "Armor", 0.0)]
# Lit: slowed to 0.45 while a beam is on it (it lingers a light check longer); frenzy: 1.25 times as fast in the dark.
LIT_SECONDS = 0.4
LIT_MODIFIERS = [("HollowMovementSet", "MoveSpeedMultiplier", "MultiplyCompound", 0.45)]
FRENZY_SECONDS = 0.4
FRENZY_MODIFIERS = [("HollowMovementSet", "MoveSpeedMultiplier", "MultiplyCompound", 1.25)]

# The bite (data): a quick lunge, little damage, the swarm bites many times.
BITE = {"windup_seconds": 0.3, "recovery_seconds": 0.7, "reach": 90.0, "radius": 35.0, "damage": 8.0, "structural_damage": 0.0}
WINDUP_CUE = "GameplayCue.Hollow.Enemy.Windup.Whisperer"
MELEE_EVENT = "Hollow.Event.Enemy.Melee"
# Its wind-up glow: small and dim (four of them may wind up at once).
GLOW_COLOR = unreal.Color(r=190, g=160, b=255, a=255)
GLOW_INTENSITY = 1500.0
GLOW_RADIUS = 220.0

# Definition (data). Patrol: its idle scuttle by the nest; chase: faster than a player walks (400), slower than a sprint
# (640): a frenzy (x1.25 = 700) is not.
DEFINITION_VALUES = {
    "patrol_speed": 250.0, "chase_speed": 560.0, "turn_rate": 720.0, "move_acceptance_radius": 40.0, "net_update_frequency": 20.0,
    "patrol_radius": 400.0, "patrol_pause_seconds": 2.5,
    "sight_radius": 1400.0, "lose_sight_radius": 1800.0, "sight_half_angle": 75.0, "hearing_range": 2500.0, "stimulus_max_age": 8.0,
    "target_memory_seconds": 2.0,
    "stimulus_memory_seconds": 12.0, "investigate_seconds": 2.5,
    "engage_range": 70.0, "chase_stop_distance": 30.0, "obstacle_recheck_seconds": 0.4, "retry_seconds": 1.0,
    "death_linger_seconds": 4.0,
    "knockback_scale": 1.0, "stagger_impulse": 300.0,
    "flinch_seconds": 0.4, "recoil_distance": 300.0, "exposed_flee_distance": 800.0, "recoil_cooldown_seconds": 2.0,
    "lunge_seconds": 1.5, "flank_pause_seconds": 0.6, "lit_prey_distance_scale": 2.0, "frenzy_whisper_volume": 1.8,
}
SOUNDS = {"windup_sound": "Hollow.Sound.Whisperer.Bite", "footstep_sound": "Hollow.Sound.Footstep.Whisperer"}
CADENCE = {"interval": 0.16, "min_speed": 60.0, "reference_speed": 560.0}   # a skitter, heard from its idle scuttle on
# Its drop (clarification 6): an Aether shard in three deaths of ten (rolled from its seed), popped up out of the body.
DROP = {"item": "DA_Item_AetherShard", "count": 1, "chance": 0.3, "pop_speed": 250.0}

# The nest: 2..4 by its seed, on a ring round it.
NEST = {"min_brood": 2, "max_brood": 4, "spawn_radius": 150.0, "spawn_search_extent": 200.0, "spawn_retry_seconds": 1.0, "max_spawn_attempts": 5}
# Biome rule (clarification 5): up to two nests a sector, on EnemySpawn sockets two rooms or more from Spawn (every such
# room holds loot crates too: the swarm keeps the loot).
NEST_RULE = {"max_per_sector": 2, "min_room_distance": 2}

# Swarm tuning (DA_ThreatConfig): the lights checked five times a second; two may bite a player with a light at once.
SWARM = {"light_check_seconds": 0.2, "max_biters_per_prey": 2}

# EQS: points on a ring round its prey, out of every light (at a crawler's height), reachable on the Standard filter, the
# nearest to it.
QUERY = {"options": [{
    "generator": {"class": "/Script/AIModule.EnvQueryGenerator_OnCircle",
                  "properties": {"CircleRadius": "(DefaultValue=260.000000)", "SpaceBetween": "(DefaultValue=70.000000)",
                                 "CircleCenter": "/Script/HollowAI.HollowEnvQueryContext_Target"}},
    "tests": [
        {"class": "/Script/HollowAI.HollowEnvQueryTest_OutOfLightCone",
         "properties": {"PointHeight": "30.000000", "TestPurpose": "Filter", "FilterType": "Match", "BoolValue": "(DefaultValue=True)"}},
        {"class": "/Script/AIModule.EnvQueryTest_Pathfinding",
         "properties": {"TestMode": "PathExist", "TestPurpose": "Filter", "FilterType": "Match", "BoolValue": "(DefaultValue=True)",
                        "PathFromContext": "(DefaultValue=True)", "SkipUnreachable": "(DefaultValue=True)",
                        "FilterClass": STANDARD_FILTER + ".NAV_Filter_Standard_C"}},
        {"class": "/Script/AIModule.EnvQueryTest_Distance",
         "properties": {"TestMode": "Distance3D", "DistanceTo": "/Script/AIModule.EnvQueryContext_Querier",
                        "TestPurpose": "Score", "ScoringEquation": "InverseLinear"}},
    ]}]}


def state(name, conditions=(), tasks=(), transitions=(), children=()):
    return {"name": name, "conditions": list(conditions), "tasks": list(tasks), "transitions": list(transitions), "children": list(children)}


def back_to_root():
    return {"trigger": "OnStateCompleted", "target": "Root"}


# Brain: children are tried in order; any fact change (Reconsider: a beam on it, a sighting) or finished activity picks again.
TREE = state("Root", transitions=[{"trigger": "OnEvent", "event": "Hollow.AI.Event.Reconsider", "target": "Root"}], children=[
    state("Stunned", ["HollowSTCond_IsStunned"], ["HollowSTTask_Stunned"], [back_to_root()]),
    state("Recoil", ["HollowSTCond_ShouldRecoil"], ["HollowSTTask_Recoil"], [back_to_root()]),
    state("OpenDoor", ["HollowSTCond_HasObstacle"], ["HollowSTTask_Breach"], [back_to_root()]),
    state("Bite", ["HollowSTCond_HasTarget", "HollowSTCond_TargetInEngageRange", "HollowSTCond_CanBite"], ["HollowSTTask_Attack"], [back_to_root()]),
    state("Flank", ["HollowSTCond_ShouldFlank"], ["HollowSTTask_Flank"], [back_to_root()]),
    state("Chase", ["HollowSTCond_HasTarget"], ["HollowSTTask_Chase"], [back_to_root()]),
    state("Investigate", ["HollowSTCond_HasStimulus"], ["HollowSTTask_Investigate"], [back_to_root()]),
    state("Lurk", [], ["HollowSTTask_Patrol"], [back_to_root()]),
])

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_whisperer_assets: " + message)


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


def item_id(name):
    return from_text(unreal.PrimaryAssetId(), "HollowItem:" + name)


def text_of(value):
    """A comparable form of a property value (structs and arrays by their text, objects by their path)."""
    if isinstance(value, (list, tuple, unreal.Array)):
        return [text_of(v) for v in value]
    if isinstance(value, unreal.Text):
        return str(value)
    if hasattr(value, "export_text"):
        return value.export_text()
    if hasattr(value, "get_path_name"):
        return value.get_path_name()
    if isinstance(value, float):
        return round(value, 4)
    return value


def refresh(obj, wanted):
    """Sets every wanted property that differs; True if any did."""
    changed = False
    for key, value in wanted.items():
        if text_of(obj.get_editor_property(key)) != text_of(value):
            obj.set_editor_property(key, value)
            changed = True
    return changed


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


# ---------------------------------------------------------------- effects and cue

def attribute_text(set_name, attribute):
    path = "%s.%s:%s" % (ATTRIBUTE_MODULE, set_name, attribute)
    return path, '(AttributeName="{attr}",Attribute={path},AttributeOwner="/Script/CoreUObject.Class\'{module}.{set}\'")'.format(
        attr=attribute, path=path, module=ATTRIBUTE_MODULE, set=set_name)


def make_modifier(set_name, attribute, op, value):
    # Modifier fields are EditDefaultsOnly, so the struct is built from text (as create_class_assets.py does).
    path, attribute_struct = attribute_text(set_name, attribute)
    text = "(Attribute=%s,ModifierOp=%s,ModifierMagnitude=(MagnitudeCalculationType=ScalableFloat,ScalableFloatMagnitude=(Value=%.6f)))" % (
        attribute_struct, op, value)
    modifier = unreal.GameplayModifierInfo()
    if not modifier.import_text(text) or path not in modifier.export_text() or ("ModifierOp=" + op) not in modifier.export_text():
        raise RuntimeError("Could not build modifier %s %s.%s" % (op, set_name, attribute))
    return modifier


def duration(seconds):
    return from_text(unreal.GameplayEffectModifierMagnitude(), "(MagnitudeCalculationType=ScalableFloat,ScalableFloatMagnitude=(Value=%.6f))" % seconds)


def ensure_init_effect():
    effect_bp, created = blueprint(INIT_EFFECT, unreal.GameplayEffect)
    effect = unreal.get_default_object(effect_bp.generated_class())
    wanted = {"duration_policy": unreal.GameplayEffectDurationType.INSTANT,
              "modifiers": [make_modifier(s, a, "Override", v) for s, a, v in WHISPERER_ATTRIBUTES]}
    if refresh(effect, wanted) or created:
        save(effect_bp)
        log("GE_Whisperer_InitAttributes: %s" % ", ".join("%s %.0f" % (a, v) for _, a, v in WHISPERER_ATTRIBUTES))
    return effect_bp.generated_class()


def ensure_renewed_effect(path, parent, seconds, modifiers):
    effect_bp, created = blueprint(path, parent)
    effect = unreal.get_default_object(effect_bp.generated_class())
    if refresh(effect, {"duration_magnitude": duration(seconds), "modifiers": [make_modifier(*m) for m in modifiers]}) or created:
        save(effect_bp)
        log("%s: %.1f s, %s" % (path.rsplit("/", 1)[1], seconds, ", ".join("%s %s %.2f" % (m[1], m[2], m[3]) for m in modifiers)))
    return effect_bp.generated_class()


def ensure_cue():
    cue_bp, created = blueprint(CUE_PATH, unreal.HollowEnemyWindupCue)
    cue = unreal.get_default_object(cue_bp.generated_class())
    changed = refresh(cue, {"gameplay_cue_tag": make_tag(WINDUP_CUE)})
    glow = cue.get_editor_property("glow")
    changed = refresh(glow, {"intensity": GLOW_INTENSITY, "attenuation_radius": GLOW_RADIUS, "cast_shadows": False, "light_color": GLOW_COLOR}) or changed
    if changed or created:
        save(cue_bp)
        log("GCN_Whisperer_Windup ready")


# ---------------------------------------------------------------- attack, ability, ability set

def ensure_attack():
    attack, created = data_asset(BITE_ATTACK, unreal.HollowEnemyAttackDefinition)
    wanted = dict(BITE)
    wanted.update({"windup_cue": make_tag(WINDUP_CUE), "montage": unreal.HollowEnemyMontage.MELEE,
                   "damage_effect": load(KINETIC_EFFECT).generated_class()})
    if refresh(attack, wanted) or created:
        save(attack)
        log("DA_Attack_Whisperer_Bite: %.0f damage, wind-up %.2f s" % (BITE["damage"], BITE["windup_seconds"]))
    return attack


def ensure_ability(attack):
    ability_bp, created = blueprint(BITE_ABILITY, unreal.HollowGA_WhispererBite)
    ability = unreal.get_default_object(ability_bp.generated_class())
    if refresh(ability, {"attack": attack}) or created:
        save(ability_bp)
        log("GA_WhispererBite ready")
    return ability_bp.generated_class()


def ensure_ability_set(ability_class, init_effect):
    ability_set, created = data_asset(ABILITY_SET, unreal.HollowAbilitySet)
    granted = list(ability_set.get_editor_property("granted_abilities"))
    effects = list(ability_set.get_editor_property("granted_effects"))
    changed = created
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
        log("DA_AbilitySet_Whisperer ready")
    return ability_set


# ---------------------------------------------------------------- query, brain, definition, pawn, nest

def ensure_query():
    query = load(QUERY_PATH) if assets.does_asset_exist(QUERY_PATH) else unreal.HollowEnvQueryAuthoring.create_env_query(WHISPERER_DIR, QUERY_NAME)
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
    tree = load(TREE_PATH) if assets.does_asset_exist(TREE_PATH) else unreal.HollowStateTreeAuthoring.create_ai_state_tree(WHISPERER_DIR, TREE_NAME)
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


def make_drop():
    drop = unreal.HollowEnemyDrop()
    drop.set_editor_property("item", item_id(DROP["item"]))
    drop.set_editor_property("count", DROP["count"])
    drop.set_editor_property("chance", DROP["chance"])
    drop.set_editor_property("pickup_class", load(PICKUP_BP).generated_class())
    drop.set_editor_property("pop_speed", DROP["pop_speed"])
    return drop


def ensure_definition(ability_set, tree, query, lit, frenzy):
    definition, created = data_asset(DEFINITION, unreal.HollowWhispererDefinition)
    wanted = dict(DEFINITION_VALUES)
    wanted.update({"ability_set": ability_set, "state_tree": tree, "flank_query": query, "navigation_filter": load(STANDARD_FILTER).generated_class(),
                   "lit_effect": lit, "frenzy_effect": frenzy, "stagger_effect": load(STAGGER_EFFECT).generated_class(),
                   "melee_event": make_tag(MELEE_EVENT), "breach_event": unreal.GameplayTag(), "drop": make_drop()})
    wanted.update({name: make_tag(tag) for name, tag in SOUNDS.items()})
    changed = refresh(definition, wanted)
    cadence = definition.get_editor_property("footstep_cadence")
    if refresh(cadence, CADENCE):
        definition.set_editor_property("footstep_cadence", cadence)
        changed = True
    if created or changed:
        save(definition)
        log("DA_Enemy_Whisperer ready")
    return definition


def ensure_pawn(definition):
    pawn_bp, created = blueprint(WHISPERER_BP, unreal.HollowWhisperer)
    pawn = unreal.get_default_object(pawn_bp.generated_class())
    changed = refresh(pawn, {"definition": definition, "ai_controller_class": unreal.HollowWhispererController.static_class()})
    capsule = pawn.get_editor_property("capsule_component")
    changed = refresh(capsule, {"capsule_radius": CAPSULE_RADIUS, "capsule_half_height": CAPSULE_HALF_HEIGHT}) or changed
    if capsule.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN) != PAWN_RESPONSE:
        capsule.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN, PAWN_RESPONSE)
        changed = True
    body = pawn.get_editor_property("body")
    changed = refresh(body, {"static_mesh": load(BODY_MESH), "relative_location": BODY_LOCATION}) or changed
    changed = refresh(pawn.get_editor_property("character_movement"), MOVEMENT) or changed
    if changed or created:
        save(pawn_bp)
        log("BP_Whisperer ready")
    return pawn_bp.generated_class()


def ensure_nest(whisperer_class):
    nest_bp, created = blueprint(NEST_BP, unreal.HollowWhispererNest)
    nest = unreal.get_default_object(nest_bp.generated_class())
    wanted = dict(NEST)
    wanted["whisperer_class"] = whisperer_class
    changed = refresh(nest, wanted)
    changed = refresh(nest.get_editor_property("mesh"), {"static_mesh": load(NEST_MESH)}) or changed
    if changed or created:
        save(nest_bp)
        log("BP_WhispererNest: %d..%d Whisperers" % (NEST["min_brood"], NEST["max_brood"]))
    return nest_bp.generated_class()


# ---------------------------------------------------------------- swarm tuning, biome rules

def ensure_threat_config():
    config = load(THREAT_CONFIG)
    swarm = config.get_editor_property("swarm")
    if refresh(swarm, SWARM):
        config.set_editor_property("swarm", swarm)
        save(config)
        log("DA_ThreatConfig: swarm light checks every %.1f s, %d biters a lit player" % (SWARM["light_check_seconds"], SWARM["max_biters_per_prey"]))


def is_enemy_rule(rule):
    return rule.get_editor_property("socket_type").export_text() == make_tag(ENEMY_SOCKET).export_text()


def ensure_biome_rule(sector, nest_class):
    biome = load(BIOMES[sector])
    rules = list(biome.get_editor_property("socket_actors"))
    wanted = {"socket_type": make_tag(ENEMY_SOCKET), "actor_class": nest_class, "spawn_offset": unreal.Vector(0.0, 0.0, 0.0),
              "max_per_sector": NEST_RULE["max_per_sector"], "pick": unreal.HollowSocketPick.MANIFEST_ORDER,
              "min_room_distance": NEST_RULE["min_room_distance"]}
    mine = [r for r in rules if is_enemy_rule(r) and r.get_editor_property("actor_class") == nest_class]
    if mine:
        if refresh(mine[0], wanted):
            biome.set_editor_property("socket_actors", rules)
            save(biome)
            log("DA_Biome_%s: the nests' rule" % sector)
        return
    rule = unreal.HollowSocketActorRule()
    refresh(rule, wanted)
    # Right after the Pilgrim's (it takes its one socket first); the markers behind keep the rest.
    pilgrim = load(PILGRIM_BP).generated_class()
    after = next((i + 1 for i, r in enumerate(rules) if is_enemy_rule(r) and r.get_editor_property("actor_class") == pilgrim), None)
    if after is None:
        after = next((i for i, r in enumerate(rules) if is_enemy_rule(r)), len(rules))
    rules.insert(after, rule)
    biome.set_editor_property("socket_actors", rules)
    save(biome)
    log("DA_Biome_%s: EnemySpawn sockets %d+ rooms from Spawn grow up to %d Whisperer nests" % (sector, NEST_RULE["min_room_distance"], NEST_RULE["max_per_sector"]))


# ---------------------------------------------------------------- validation

def validate(whisperer_class, nest_class):
    definition = load(DEFINITION)
    check(json.loads(unreal.HollowStateTreeAuthoring.describe_state_tree(load(TREE_PATH))) == TREE, "ST_Whisperer matches the spec")
    check(unreal.HollowEnvQueryAuthoring.env_query_matches(load(QUERY_PATH), json.dumps(QUERY)), "EQ_WhispererFlank matches the spec")
    check(definition.get_editor_property("state_tree") == load(TREE_PATH) and definition.get_editor_property("flank_query") == load(QUERY_PATH), "definition brain")
    check(definition.get_editor_property("chase_stop_distance") < definition.get_editor_property("engage_range"), "chase stops in bite range")
    check(load(BITE_ATTACK).get_editor_property("reach") >= definition.get_editor_property("engage_range"), "the bite reaches the engage range")
    check(definition.get_editor_property("lit_effect") == load(LIT_EFFECT).generated_class(), "lit effect")
    check(definition.get_editor_property("frenzy_effect") == load(FRENZY_EFFECT).generated_class(), "frenzy effect")
    drop = definition.get_editor_property("drop")
    check(drop.get_editor_property("pickup_class") == load(PICKUP_BP).generated_class() and drop.get_editor_property("count") > 0, "its drop")
    check(assets.does_asset_exist("/Game/Hollow/Items/Materials/" + DROP["item"]), "it drops a real item")
    check(len(load(ABILITY_SET).get_editor_property("granted_abilities")) == 1, "one ability: the bite")
    pawn = unreal.get_default_object(whisperer_class)
    check(pawn.get_editor_property("definition") == definition, "BP_Whisperer definition")
    check(pawn.get_editor_property("ai_controller_class") == unreal.HollowWhispererController.static_class(), "Whisperer brain")
    capsule = pawn.get_editor_property("capsule_component")
    check(abs(capsule.get_editor_property("capsule_radius") - CAPSULE_RADIUS) < 1e-3, "small capsule")
    check(capsule.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN) == PAWN_RESPONSE, "pawns pass through it")
    check(pawn.get_editor_property("body").get_editor_property("static_mesh") == load(BODY_MESH), "its body")
    check(pawn.get_editor_property("character_movement").get_editor_property("use_rvo_avoidance"), "a swarm keeps apart")
    nest = unreal.get_default_object(nest_class)
    check(nest.get_editor_property("whisperer_class") == whisperer_class and 0 < nest.get_editor_property("min_brood") <= nest.get_editor_property("max_brood"),
          "the nest's brood")
    swarm = load(THREAT_CONFIG).get_editor_property("swarm")
    check(swarm.get_editor_property("light_check_seconds") > 0.0 and swarm.get_editor_property("max_biters_per_prey") > 0, "swarm tuning")
    for sector in BIOMES:
        rules = list(load(BIOMES[sector]).get_editor_property("socket_actors"))
        enemy = [r.get_editor_property("actor_class") for r in rules if is_enemy_rule(r)]
        check(len(enemy) >= 2 and enemy[1] == nest_class, "DA_Biome_%s: the nests right after the Pilgrim" % sector)
    with open(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_config_dir()) + "DefaultGame.ini") as ini:
        text = ini.read()
    check(INI_SECTION in text and INI_LINE in text, "UHollowAISettings::DevNestClass names BP_WhispererNest (DefaultGame.ini)")
    log("validation passed")


def main():
    init = ensure_init_effect()
    lit = ensure_renewed_effect(LIT_EFFECT, unreal.HollowLitEffect, LIT_SECONDS, LIT_MODIFIERS)
    frenzy = ensure_renewed_effect(FRENZY_EFFECT, unreal.HollowFrenzyEffect, FRENZY_SECONDS, FRENZY_MODIFIERS)
    ensure_cue()
    attack = ensure_attack()
    ability = ensure_ability(attack)
    ability_set = ensure_ability_set(ability, init)
    query = ensure_query()
    tree = ensure_tree()
    definition = ensure_definition(ability_set, tree, query, lit, frenzy)
    whisperer_class = ensure_pawn(definition)
    nest_class = ensure_nest(whisperer_class)
    ensure_threat_config()
    for sector in sorted(BIOMES):
        ensure_biome_rule(sector, nest_class)
    validate(whisperer_class, nest_class)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_whisperer_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
