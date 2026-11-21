"""Creates the S4-04 sound base (idempotent, ADR-0010, ADR-0022). Nothing heavy: every sound is a procedural MetaSound
(oscillators, noise, filters, envelopes) built from the specs below - no WAV banks, so the LFS footprint stays in kilobytes.

  /Game/Hollow/Audio/MetaSounds/MS_*     one MetaSound per gameplay sound and two room tones (sound class SC_Hollow_World)
  /Game/Hollow/Audio/DA_SoundBank        UHollowSoundBank: sound, attenuation, concurrency and range per Hollow.Noise / Sound tag
  /Game/Hollow/Audio/ATT_Hollow_World    attenuation template (the range per tag comes from the noise rules or the bank)
  /Game/Hollow/Audio/SCON_*              concurrency groups (footsteps, impacts, tools)
  /Game/Hollow/Audio/Reverb/RE_*         reverb presets (Vault, Hangar, Corridor)
  Every Docks room level                 an Audio Gameplay Volume over the room (reverb + muffled outside) and a room tone
  DA_Enemy_IronPilgrim / EchoStalker     WindupSound, FootstepSound and FootstepCadence

MetaSounds and levels keep a hash of their spec (asset metadata, or a marker actor in a level): a MetaSound is rebuilt and
a level re-saved only when its spec changes - every save is a new LFS object.
Run after create_lighting_assets.py:  Build/Scripts/run-editor-python.sh Build/Python/create_audio_assets.py
"""
import hashlib
import json
import re
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

AUDIO_DIR = "/Game/Hollow/Audio"
MS_DIR = AUDIO_DIR + "/MetaSounds"
REVERB_DIR = AUDIO_DIR + "/Reverb"
BANK = AUDIO_DIR + "/DA_SoundBank"
ATTENUATION = AUDIO_DIR + "/ATT_Hollow_World"
SOUND_CLASS = AUDIO_DIR + "/SC_Hollow_World"   # create_resonance_assets.py (low-pass on CB_Resonance)
ROOM_DATA_DIR = "/HollowSector_Docks/Data"
# Enemy voices and steps. Cadence: a step every interval (s) faster than min_speed, full volume at reference_speed (its
# chase): the Pilgrim's slow heavy tread is heard from its 180 patrol on, the stalker skitters (cm/s).
ENEMIES = {
    "/Game/Hollow/Characters/Enemies/IronPilgrim/DA_Enemy_IronPilgrim": {
        "tags": {"windup_sound": "Hollow.Sound.Enemy.PilgrimWindup", "footstep_sound": "Hollow.Sound.Footstep.Heavy"},
        "cadence": {"interval": 0.7, "min_speed": 60.0, "reference_speed": 330.0}},
    "/Game/Hollow/Characters/Enemies/EchoStalker/DA_Enemy_EchoStalker": {
        "tags": {"windup_sound": "Hollow.Sound.Enemy.StalkerShriek", "footstep_sound": "Hollow.Sound.Footstep.Stalker"},
        "cadence": {"interval": 0.28, "min_speed": 60.0, "reference_speed": 480.0}},
}
SPEC_TAG = "HollowSpec"
MANAGED_TAG = "HollowAudio"
MARKER_LABEL = "HollowAudioSpec"
AUTHOR = "Protocol: Hollow (create_audio_assets.py)"
VOLUME_BRUSH = 200.0          # a spawned volume's default cube brush (uu)

# ---------------------------------------------------------------- MetaSound graphs (data)
# A graph: nodes {id: [class name, {input: default}]}, links [[from id, output, to id, input]]. "@play" is the graph's
# On Play, "@finish" its On Finished (one-shots end when it fires), "@out" the mono output.
SINE, SAW, SQUARE, TRIANGLE = ("UE", "Sine", "Audio"), ("UE", "Saw", "Audio"), ("UE", "Square", "Audio"), ("UE", "Triangle", "Audio")
NOISE, LFO = ("UE", "Noise", "Audio"), ("UE", "LFO", "Audio")
ENVELOPE = ("AD Envelope", "AD Envelope", "Audio")
SVF, LOWPASS, HIGHPASS = ("UE", "State Variable Filter", "Audio"), ("UE", "One-Pole Low Pass Filter", "Audio"), ("UE", "One-Pole High Pass Filter", "Audio")
MULTIPLY, SCALE = ("UE", "Multiply", "Audio"), ("UE", "Multiply", "Audio by Float")
MIX2, MIX3 = ("AudioMixer", "Audio Mixer (Mono, 2)", ""), ("AudioMixer", "Audio Mixer (Mono, 3)", "")
DELAY, ANY2, REPEAT = ("UE", "Trigger Delay", ""), ("TriggerAny", "Trigger Any (2)", ""), ("UE", "TriggerRepeat", "")
PINK, WHITE = 0, 1                       # MetaSound noise types
LFO_SINE, LFO_SAW = 0, 1                 # MetaSound LFO shapes
BAND_PASS, LOW_PASS = "Band Pass", "Low Pass Filter"


class Graph(object):
    def __init__(self, one_shot):
        self.spec = {"one_shot": one_shot, "nodes": {}, "links": []}

    def node(self, node_id, class_name, defaults=None):
        self.spec["nodes"][node_id] = [list(class_name), defaults or {}]
        return self

    def link(self, source, output, target, pin):
        self.spec["links"].append([source, output, target, pin])
        return self

    def burst(self, node_id, source, output, attack, decay, trigger=("@play", "On Play")):
        """source x an AD envelope (attack, decay in s) started by trigger; the product is node_id."""
        self.node(node_id + "_env", ENVELOPE, {"Attack Time": attack, "Decay Time": decay})
        self.link(trigger[0], trigger[1], node_id + "_env", "Trigger")
        self.node(node_id, MULTIPLY)
        self.link(source, output, node_id, "PrimaryOperand").link(node_id + "_env", "Out Envelope", node_id, "AdditionalOperands")
        return self

    def mix(self, node_id, parts):
        """parts: [(node, output, gain)] into one mixer."""
        self.node(node_id, MIX2 if len(parts) == 2 else MIX3, {"Gain %d" % i: gain for i, (_, _, gain) in enumerate(parts)})
        for i, (source, output, _) in enumerate(parts):
            self.link(source, output, node_id, "In %d" % i)
        return self

    def out(self, source, output="Out"):
        return self.link(source, output, "@out", "Out Mono")

    def finish_on(self, source, output="On Done"):
        return self.link(source, output, "@finish", "On Finished")


def welder():
    g = Graph(one_shot=False)
    g.node("hiss", NOISE, {"Type": WHITE}).node("band", SVF, {"Cutoff Frequency": 3200.0, "Resonance": 3.0}).link("hiss", "Audio", "band", "In")
    g.node("crackle", LFO, {"Frequency": 23.0, "Shape": LFO_SAW, "Min Value": 0.3, "Max Value": 1.0})
    g.node("crackled", SCALE).link("band", BAND_PASS, "crackled", "PrimaryOperand").link("crackle", "Out", "crackled", "AdditionalOperands")
    g.node("hum", SINE, {"Frequency": 100.0})
    return g.mix("mix", [("crackled", "Out", 0.5), ("hum", "Audio", 0.12)]).out("mix").spec


def cutter():
    g = Graph(one_shot=False)
    g.node("roar", SAW, {"Frequency": 82.0}).node("roar_lp", LOWPASS, {"Cutoff Frequency": 900.0}).link("roar", "Audio", "roar_lp", "In")
    g.node("plasma", NOISE, {"Type": PINK}).node("plasma_bp", SVF, {"Cutoff Frequency": 1400.0, "Resonance": 2.0}).link("plasma", "Audio", "plasma_bp", "In")
    g.node("wobble", LFO, {"Frequency": 7.0, "Min Value": 0.7, "Max Value": 1.0})
    g.node("wobbled", SCALE).link("plasma_bp", BAND_PASS, "wobbled", "PrimaryOperand").link("wobble", "Out", "wobbled", "AdditionalOperands")
    return g.mix("mix", [("roar_lp", "Out", 0.35), ("wobbled", "Out", 0.5)]).out("mix").spec


def door_servo():
    g = Graph(one_shot=True)
    g.node("whine_lfo", LFO, {"Frequency": 4.0, "Min Value": 118.0, "Max Value": 142.0})
    g.node("whine", SAW).link("whine_lfo", "Out", "whine", "Frequency")
    g.node("whine_lp", LOWPASS, {"Cutoff Frequency": 700.0}).link("whine", "Audio", "whine_lp", "In")
    g.burst("motor", "whine_lp", "Out", 0.08, 1.4)
    g.node("stop", DELAY, {"Delay Time": 1.35}).link("@play", "On Play", "stop", "In")
    g.node("thunk", SINE, {"Frequency": 70.0}).burst("latch", "thunk", "Audio", 0.002, 0.25, trigger=("stop", "Out"))
    return g.mix("mix", [("motor", "Out", 0.5), ("latch", "Out", 0.6)]).out("mix").finish_on("latch_env").spec


def impact_metal():
    g = Graph(one_shot=True)
    g.node("p1", SINE, {"Frequency": 320.0}).node("p2", SINE, {"Frequency": 787.0}).node("p3", SINE, {"Frequency": 1253.0})
    g.mix("partials", [("p1", "Audio", 0.5), ("p2", "Audio", 0.3), ("p3", "Audio", 0.2)]).burst("ring", "partials", "Out", 0.002, 0.9)
    g.node("hit", NOISE, {"Type": WHITE}).node("hit_lp", LOWPASS, {"Cutoff Frequency": 2500.0}).link("hit", "Audio", "hit_lp", "In")
    g.burst("crack", "hit_lp", "Out", 0.001, 0.07)
    return g.mix("mix", [("ring", "Out", 0.6), ("crack", "Out", 0.5)]).out("mix").finish_on("ring_env").spec


def breach_crash():
    g = Graph(one_shot=True)
    g.node("rumble", NOISE, {"Type": PINK}).node("rumble_lp", LOWPASS, {"Cutoff Frequency": 800.0}).link("rumble", "Audio", "rumble_lp", "In")
    g.burst("body", "rumble_lp", "Out", 0.004, 2.2)
    g.node("sub", SINE, {"Frequency": 42.0}).burst("boom", "sub", "Audio", 0.01, 1.6)
    g.node("debris", NOISE, {"Type": WHITE}).node("debris_hp", HIGHPASS, {"Cutoff Frequency": 2500.0}).link("debris", "Audio", "debris_hp", "In")
    g.burst("shards", "debris_hp", "Out", 0.001, 0.35)
    return g.mix("mix", [("body", "Out", 0.7), ("boom", "Out", 0.6), ("shards", "Out", 0.3)]).out("mix").finish_on("body_env").spec


def core_drop():
    g = Graph(one_shot=True)
    g.node("sub", SINE, {"Frequency": 58.0}).burst("thud", "sub", "Audio", 0.002, 0.45)
    g.node("knock", NOISE, {"Type": PINK}).node("knock_lp", LOWPASS, {"Cutoff Frequency": 350.0}).link("knock", "Audio", "knock_lp", "In")
    g.burst("body", "knock_lp", "Out", 0.001, 0.12)
    return g.mix("mix", [("thud", "Out", 0.8), ("body", "Out", 0.5)]).out("mix").finish_on("thud_env").spec


def core_hum():
    g = Graph(one_shot=False)
    g.node("low", SINE, {"Frequency": 55.0}).node("beat", SINE, {"Frequency": 110.7}).node("shimmer", TRIANGLE, {"Frequency": 220.0})
    g.mix("chord", [("low", "Audio", 0.4), ("beat", "Audio", 0.25), ("shimmer", "Audio", 0.08)])
    g.node("breath", LFO, {"Frequency": 0.25, "Min Value": 0.7, "Max Value": 1.0})
    g.node("breathing", SCALE).link("chord", "Out", "breathing", "PrimaryOperand").link("breath", "Out", "breathing", "AdditionalOperands")
    return g.out("breathing").spec


def mine_arm():
    g = Graph(one_shot=True)
    g.node("second", DELAY, {"Delay Time": 0.16}).link("@play", "On Play", "second", "In")
    g.node("beeps", ANY2).link("@play", "On Play", "beeps", "In 0").link("second", "Out", "beeps", "In 1")
    g.node("tone", SQUARE, {"Frequency": 1760.0}).node("tone_lp", LOWPASS, {"Cutoff Frequency": 4000.0}).link("tone", "Audio", "tone_lp", "In")
    g.burst("beep", "tone_lp", "Out", 0.003, 0.09, trigger=("beeps", "Out"))
    g.node("end", DELAY, {"Delay Time": 0.45}).link("@play", "On Play", "end", "In")
    return g.node("level", SCALE, {"AdditionalOperands": 0.4}).link("beep", "Out", "level", "PrimaryOperand").out("level").finish_on("end", "Out").spec


def helmet_lamp_click():
    """S4-08: a small switch, two contacts 25 ms apart; quiet, players only."""
    g = Graph(one_shot=True)
    g.node("snap", NOISE, {"Type": WHITE}).node("snap_hp", HIGHPASS, {"Cutoff Frequency": 2500.0}).link("snap", "Audio", "snap_hp", "In")
    g.burst("contact", "snap_hp", "Out", 0.001, 0.018)
    g.node("second", DELAY, {"Delay Time": 0.025}).link("@play", "On Play", "second", "In")
    g.node("tick", SQUARE, {"Frequency": 3100.0}).burst("latch", "tick", "Audio", 0.001, 0.012, trigger=("second", "Out"))
    g.node("end", DELAY, {"Delay Time": 0.15}).link("@play", "On Play", "end", "In")
    g.mix("mix", [("contact", "Out", 0.6), ("latch", "Out", 0.25)])
    return g.node("level", SCALE, {"AdditionalOperands": 0.5}).link("mix", "Out", "level", "PrimaryOperand").out("level").finish_on("end", "Out").spec


def mine_blast():
    g = Graph(one_shot=True)
    g.node("sub", SINE, {"Frequency": 75.0}).burst("thump", "sub", "Audio", 0.001, 0.7)
    g.node("zap", SQUARE, {"Frequency": 2400.0}).burst("arc", "zap", "Audio", 0.001, 0.06)
    g.node("air", NOISE, {"Type": WHITE}).node("air_lp", LOWPASS, {"Cutoff Frequency": 1800.0}).link("air", "Audio", "air_lp", "In")
    g.burst("blast", "air_lp", "Out", 0.001, 0.45)
    return g.mix("mix", [("thump", "Out", 0.8), ("arc", "Out", 0.25), ("blast", "Out", 0.5)]).out("mix").finish_on("thump_env").spec


def footstep_light():
    g = Graph(one_shot=True)
    g.node("scuff", NOISE, {"Type": PINK}).node("scuff_bp", SVF, {"Cutoff Frequency": 1100.0, "Resonance": 2.0}).link("scuff", "Audio", "scuff_bp", "In")
    g.burst("heel", "scuff_bp", BAND_PASS, 0.001, 0.09)
    g.node("plate", SINE, {"Frequency": 110.0}).burst("thud", "plate", "Audio", 0.001, 0.05)
    return g.mix("mix", [("heel", "Out", 0.6), ("thud", "Out", 0.4)]).out("mix").finish_on("heel_env").spec


def footstep_heavy():
    g = Graph(one_shot=True)
    g.node("sub", SINE, {"Frequency": 48.0}).burst("stomp", "sub", "Audio", 0.003, 0.4)
    g.node("grind", NOISE, {"Type": PINK}).node("grind_lp", LOWPASS, {"Cutoff Frequency": 450.0}).link("grind", "Audio", "grind_lp", "In")
    g.burst("weight", "grind_lp", "Out", 0.002, 0.18)
    g.node("plate", SINE, {"Frequency": 640.0}).burst("clank", "plate", "Audio", 0.001, 0.25)
    return g.mix("mix", [("stomp", "Out", 0.8), ("weight", "Out", 0.5), ("clank", "Out", 0.15)]).out("mix").finish_on("stomp_env").spec


def footstep_stalker():
    g = Graph(one_shot=True)
    g.node("claws", REPEAT, {"Period": 0.04, "Num Repeats": 3}).link("@play", "On Play", "claws", "Start")
    g.node("tick", NOISE, {"Type": WHITE}).node("tick_hp", HIGHPASS, {"Cutoff Frequency": 3500.0}).link("tick", "Audio", "tick_hp", "In")
    g.burst("click", "tick_hp", "Out", 0.001, 0.025, trigger=("claws", "RepeatOut"))
    g.node("end", DELAY, {"Delay Time": 0.2}).link("@play", "On Play", "end", "In")
    return g.node("level", SCALE, {"AdditionalOperands": 0.35}).link("click", "Out", "level", "PrimaryOperand").out("level").finish_on("end", "Out").spec


def pilgrim_windup():
    g = Graph(one_shot=True)
    g.node("rise", LFO, {"Frequency": 1.1, "Shape": LFO_SAW, "Min Value": 45.0, "Max Value": 150.0})
    g.node("growl", SAW).link("rise", "Out", "growl", "Frequency")
    g.node("growl_lp", LOWPASS, {"Cutoff Frequency": 900.0}).link("growl", "Audio", "growl_lp", "In")
    g.node("breath", NOISE, {"Type": PINK}).node("breath_lp", LOWPASS, {"Cutoff Frequency": 300.0}).link("breath", "Audio", "breath_lp", "In")
    g.mix("voice", [("growl_lp", "Out", 0.5), ("breath_lp", "Out", 0.3)]).burst("windup", "voice", "Out", 0.55, 0.3)
    return g.out("windup").finish_on("windup_env").spec


def stalker_shriek():
    g = Graph(one_shot=True)
    g.node("vibrato", LFO, {"Frequency": 9.0, "Min Value": 860.0, "Max Value": 980.0})
    g.node("voice_a", SAW).link("vibrato", "Out", "voice_a", "Frequency").node("voice_b", SAW, {"Frequency": 1390.0})
    g.mix("voices", [("voice_a", "Audio", 0.5), ("voice_b", "Audio", 0.4)])
    g.node("throat", SVF, {"Cutoff Frequency": 2100.0, "Resonance": 4.0}).link("voices", "Out", "throat", "In")
    g.burst("shriek", "throat", BAND_PASS, 0.04, 1.0)
    g.node("rasp", NOISE, {"Type": WHITE}).node("rasp_hp", HIGHPASS, {"Cutoff Frequency": 4000.0}).link("rasp", "Audio", "rasp_hp", "In")
    g.burst("hiss", "rasp_hp", "Out", 0.01, 0.6)
    return g.mix("mix", [("shriek", "Out", 0.5), ("hiss", "Out", 0.2)]).out("mix").finish_on("shriek_env").spec


def stalker_vent():
    g = Graph(one_shot=True)
    g.node("rattle", REPEAT, {"Period": 0.05, "Num Repeats": 9}).link("@play", "On Play", "rattle", "Start")
    g.node("grille", NOISE, {"Type": WHITE}).node("grille_bp", SVF, {"Cutoff Frequency": 2600.0, "Resonance": 5.0}).link("grille", "Audio", "grille_bp", "In")
    g.burst("knock", "grille_bp", BAND_PASS, 0.001, 0.03, trigger=("rattle", "RepeatOut"))
    g.node("ring", SINE, {"Frequency": 330.0}).burst("sheet", "ring", "Audio", 0.002, 0.5)
    g.node("end", DELAY, {"Delay Time": 0.6}).link("@play", "On Play", "end", "In")
    return g.mix("mix", [("knock", "Out", 0.6), ("sheet", "Out", 0.2)]).out("mix").finish_on("end", "Out").spec


def room_tone():
    g = Graph(one_shot=False)
    g.node("air", NOISE, {"Type": PINK}).node("air_lp", LOWPASS, {"Cutoff Frequency": 220.0}).link("air", "Audio", "air_lp", "In")
    g.node("mains", SINE, {"Frequency": 60.0})
    return g.mix("mix", [("air_lp", "Out", 0.25), ("mains", "Audio", 0.03)]).out("mix").spec


def room_tone_vault():
    g = Graph(one_shot=False)
    g.node("air", NOISE, {"Type": PINK}).node("air_lp", LOWPASS, {"Cutoff Frequency": 180.0}).link("air", "Audio", "air_lp", "In")
    g.node("drone", SINE, {"Frequency": 41.0}).node("swell", LFO, {"Frequency": 0.12, "Min Value": 0.2, "Max Value": 1.0})
    g.node("swelling", SCALE).link("drone", "Audio", "swelling", "PrimaryOperand").link("swell", "Out", "swelling", "AdditionalOperands")
    g.node("mains", SINE, {"Frequency": 60.0})
    return g.mix("mix", [("air_lp", "Out", 0.22), ("swelling", "Out", 0.08), ("mains", "Audio", 0.03)]).out("mix").spec


METASOUNDS = {
    "MS_Tool_Welder": welder, "MS_Tool_Cutter": cutter,
    "MS_Door_Servo": door_servo, "MS_Impact_Metal": impact_metal, "MS_Breach_Crash": breach_crash,
    "MS_Core_Drop": core_drop, "MS_Core_Hum": core_hum, "MS_Mine_Arm": mine_arm, "MS_Mine_Blast": mine_blast,
    "MS_HelmetLamp_Click": helmet_lamp_click,
    "MS_Footstep_Light": footstep_light, "MS_Footstep_Heavy": footstep_heavy, "MS_Footstep_Stalker": footstep_stalker,
    "MS_Pilgrim_Windup": pilgrim_windup, "MS_Stalker_Shriek": stalker_shriek, "MS_Stalker_Vent": stalker_vent,
    "MS_Room_Tone": room_tone, "MS_Room_Tone_Vault": room_tone_vault,
}

# ---------------------------------------------------------------- bank, attenuation, concurrency (data)

# Inner radius at full volume, then a natural falloff to silence at the tag's range; air absorption with distance.
ATTENUATION_SETTINGS = {"attenuation_shape_extents": 150.0, "distance_algorithm": "NATURAL_SOUND", "attenuate_with_lpf": True,
                        "lpf_radius_min": 500.0, "lpf_radius_max": 3000.0, "lpf_frequency_at_max": 2500.0, "enable_reverb_send": True}
CONCURRENCY = {"SCON_Footsteps": 8, "SCON_Impacts": 6, "SCON_Tools": 4}   # voices per group; the quietest is dropped
# tag -> MetaSound, concurrency, own range (players-only sounds; noises take their rule's), volume, pitch variation, noise link.
ENTRIES = [
    ("Hollow.Noise.Tool.Welder", "MS_Tool_Welder", "SCON_Tools", 0.0, 0.8, 0.0, None),
    ("Hollow.Noise.Tool.Cutter", "MS_Tool_Cutter", "SCON_Tools", 0.0, 0.8, 0.0, None),
    ("Hollow.Noise.Footstep", "MS_Footstep_Light", "SCON_Footsteps", 0.0, 0.7, 0.08, None),
    ("Hollow.Noise.Core.Drop", "MS_Core_Drop", "SCON_Impacts", 0.0, 1.0, 0.05, None),
    ("Hollow.Noise.Impact.Heavy", "MS_Impact_Metal", "SCON_Impacts", 0.0, 1.0, 0.1, None),
    ("Hollow.Noise.Structural.Breach", "MS_Breach_Crash", "SCON_Impacts", 0.0, 1.0, 0.05, None),
    ("Hollow.Sound.Door.Servo", "MS_Door_Servo", None, 1500.0, 0.6, 0.04, None),
    ("Hollow.Sound.Footstep.Heavy", "MS_Footstep_Heavy", "SCON_Footsteps", 2500.0, 1.0, 0.06, None),
    ("Hollow.Sound.Footstep.Stalker", "MS_Footstep_Stalker", "SCON_Footsteps", 1000.0, 0.6, 0.1, None),
    ("Hollow.Sound.Enemy.PilgrimWindup", "MS_Pilgrim_Windup", None, 2500.0, 1.0, 0.05, None),
    ("Hollow.Sound.Enemy.StalkerShriek", "MS_Stalker_Shriek", None, 4000.0, 1.0, 0.08, None),
    ("Hollow.Sound.Stalker.Vent", "MS_Stalker_Vent", None, 1800.0, 0.8, 0.1, None),
    ("Hollow.Sound.Mine.Arm", "MS_Mine_Arm", None, 1000.0, 0.7, 0.0, None),
    ("Hollow.Sound.Mine.Blast", "MS_Mine_Blast", "SCON_Impacts", 0.0, 1.0, 0.05, "Hollow.Noise.Impact.Heavy"),
    ("Hollow.Sound.Core.Hum", "MS_Core_Hum", None, 900.0, 0.5, 0.0, None),
    ("Hollow.Sound.HelmetLamp.Toggle", "MS_HelmetLamp_Click", None, 600.0, 0.4, 0.06, None),   # S4-08: within a room
]

# ---------------------------------------------------------------- room acoustics (data)

REVERBS = {
    "RE_Vault": {"density": 1.0, "diffusion": 0.9, "gain": 0.35, "gain_hf": 0.7, "decay_time": 3.2, "decay_hf_ratio": 0.6,
                 "reflections_gain": 0.2, "reflections_delay": 0.03, "late_gain": 1.4, "late_delay": 0.04, "air_absorption_gain_hf": 0.99},
    "RE_Hangar": {"density": 1.0, "diffusion": 1.0, "gain": 0.32, "gain_hf": 0.8, "decay_time": 2.2, "decay_hf_ratio": 0.75,
                  "reflections_gain": 0.15, "reflections_delay": 0.02, "late_gain": 1.2, "late_delay": 0.025, "air_absorption_gain_hf": 0.994},
    "RE_Corridor": {"density": 0.7, "diffusion": 0.8, "gain": 0.3, "gain_hf": 0.85, "decay_time": 0.9, "decay_hf_ratio": 0.8,
                    "reflections_gain": 0.4, "reflections_delay": 0.008, "late_gain": 1.0, "late_delay": 0.012, "air_absorption_gain_hf": 0.994},
}
ROOM_ACOUSTICS = {"CoreVault": ("RE_Vault", "MS_Room_Tone_Vault", 0.5), "Hangar": ("RE_Hangar", "MS_Room_Tone", 0.45),
                  "Storage": ("RE_Hangar", "MS_Room_Tone", 0.4), "Spawn": ("RE_Hangar", "MS_Room_Tone", 0.4),
                  "Extraction": ("RE_Hangar", "MS_Room_Tone", 0.4), "Corridor": ("RE_Corridor", "MS_Room_Tone", 0.3),
                  "Junction": ("RE_Corridor", "MS_Room_Tone", 0.3)}   # reverb, room tone, tone volume
# Reverb wet level and fade on entering; what comes from outside the room is low-passed (the walls; the AI hears it
# InterRoomRange as far, DA_NoiseConfig). The room tone fills the room and fades out past its walls.
ACOUSTICS = {"reverb_volume": 0.6, "reverb_fade": 0.6, "exterior_lpf": 1500.0, "exterior_lpf_time": 0.4, "tone_falloff": 300.0}

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
builders = unreal.get_engine_subsystem(unreal.MetaSoundBuilderSubsystem)
metasound_editor = unreal.get_editor_subsystem(unreal.MetaSoundEditorSubsystem)
SUCCEEDED = unreal.MetaSoundBuilderResult.SUCCEEDED


def log(message):
    unreal.log("create_audio_assets: " + message)


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


def digest(*specs):
    return hashlib.sha1(json.dumps(specs, sort_keys=True, default=str).encode("ascii")).hexdigest()


def from_text(struct, text):
    if not struct.import_text(text):
        raise RuntimeError("Could not import " + text)
    return struct


def make_tag(name):
    return from_text(unreal.GameplayTag(), '(TagName="%s")' % name) if name else unreal.GameplayTag()


def object_path(path):
    return "%s.%s" % (path, path.rsplit("/", 1)[1])


def ok(result, what):
    status = result[-1] if isinstance(result, tuple) else result
    if status != SUCCEEDED:
        raise RuntimeError("MetaSound builder failed: " + what)
    return result[0] if isinstance(result, tuple) and len(result) > 1 else None


# ---------------------------------------------------------------- MetaSounds

def literal(value):
    """A MetaSound literal (the create calls also return its data type)."""
    if isinstance(value, bool):
        made = builders.create_bool_meta_sound_literal(value)
    elif isinstance(value, int):
        made = builders.create_int_meta_sound_literal(value)
    else:
        made = builders.create_float_meta_sound_literal(float(value))
    return made[0] if isinstance(made, tuple) else made


def build_graph(name, spec):
    made = builders.create_source_builder("Hollow_" + name, output_format=unreal.MetaSoundOutputAudioFormat.MONO, is_one_shot=spec["one_shot"])
    builder, on_play, on_finished, outputs = made[0], made[1], made[2], made[3]
    ok(made[4], name + " builder")
    graph = {"@play": {"On Play": on_play}, "@finish": {"On Finished": on_finished}, "@out": {"Out Mono": outputs[0]}}
    nodes = {}
    for node_id, (class_name, defaults) in sorted(spec["nodes"].items()):
        node = ok(builder.add_node_by_class_name(unreal.MetasoundFrontendClassName(*class_name), 1), "%s: node %s" % (name, node_id))
        nodes[node_id] = node
        for pin, value in sorted(defaults.items()):
            handle = ok(builder.find_node_input_by_name(node, pin), "%s: %s.%s" % (name, node_id, pin))
            ok(builder.set_node_input_default(handle, literal(value)), "%s: default %s.%s" % (name, node_id, pin))
    for source, output, target, pin in spec["links"]:
        out_handle = graph[source][output] if source in graph else ok(builder.find_node_output_by_name(nodes[source], output), "%s: %s.%s" % (name, source, output))
        in_handle = graph[target][pin] if target in graph else ok(builder.find_node_input_by_name(nodes[target], pin), "%s: %s.%s" % (name, target, pin))
        ok(builder.connect_nodes(out_handle, in_handle), "%s: %s.%s -> %s.%s" % (name, source, output, target, pin))
    return builder


def ensure_metasound(name, spec, sound_class):
    path = MS_DIR + "/" + name
    wanted = digest(spec)
    if assets.does_asset_exist(path):
        existing = load(path)
        if assets.get_metadata_tag(existing, SPEC_TAG) == wanted and existing.get_editor_property("sound_class_object") == sound_class:
            return existing, False
        if not assets.delete_asset(path):   # the spec changed: rebuild (the bank refers to it by path)
            raise RuntimeError("Could not replace " + path)
    builder = build_graph(name, spec)
    built = metasound_editor.build_to_asset(builder, AUTHOR, name, MS_DIR)
    asset = built[0] if isinstance(built, tuple) else built
    if asset is None or not assets.does_asset_exist(path):
        raise RuntimeError("Could not build " + path)
    asset = load(path)
    asset.set_editor_property("sound_class_object", sound_class)
    assets.set_metadata_tag(asset, SPEC_TAG, wanted)
    save(asset)
    log("%s built (%d nodes)" % (name, len(spec["nodes"])))
    return asset, True


def ensure_metasounds():
    sound_class = load(SOUND_CLASS)
    made = {}
    built = 0
    for name in sorted(METASOUNDS):
        made[name], fresh = ensure_metasound(name, METASOUNDS[name](), sound_class)
        built += int(fresh)
    return made, built


# ---------------------------------------------------------------- attenuation, concurrency, reverb, bank

def apply(obj, values):
    """Sets the values that differ (enums by name); True if any did."""
    changed = False
    for name, value in values.items():
        current = obj.get_editor_property(name)
        if isinstance(value, str) and not isinstance(current, str):
            value = getattr(type(current), value)   # an enum by name
        if isinstance(value, float):
            differs = abs(float(current) - value) > 1e-4
        else:
            differs = current != value
        if differs:
            obj.set_editor_property(name, value)
            changed = True
    return changed


def ensure_attenuation():
    attenuation, created = existing_or_new(ATTENUATION, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
    settings = attenuation.get_editor_property("attenuation")
    values = dict(ATTENUATION_SETTINGS)
    extents = settings.get_editor_property("attenuation_shape_extents")
    changed = abs(extents.x - values.pop("attenuation_shape_extents")) > 1e-4
    if changed:
        settings.set_editor_property("attenuation_shape_extents", unreal.Vector(ATTENUATION_SETTINGS["attenuation_shape_extents"], 0.0, 0.0))
    changed = apply(settings, values) or changed
    if created or changed:
        attenuation.set_editor_property("attenuation", settings)
        save(attenuation)
        log("ATT_Hollow_World ready")
    return attenuation


def ensure_concurrency():
    made = {}
    for name, count in sorted(CONCURRENCY.items()):
        concurrency, created = existing_or_new(AUDIO_DIR + "/" + name, unreal.SoundConcurrency, unreal.SoundConcurrencyFactory())
        settings = concurrency.get_editor_property("concurrency")
        if apply(settings, {"max_count": count, "resolution_rule": unreal.MaxConcurrentResolutionRule.STOP_QUIETEST}) or created:
            concurrency.set_editor_property("concurrency", settings)
            save(concurrency)
            log("%s: %d voices" % (name, count))
        made[name] = concurrency
    return made


def ensure_reverbs():
    made = {}
    for name, values in sorted(REVERBS.items()):
        reverb, created = existing_or_new(REVERB_DIR + "/" + name, unreal.ReverbEffect, unreal.ReverbEffectFactory())
        if apply(reverb, values) or created:
            save(reverb)
            log(name + " ready")
        made[name] = reverb
    return made


def ensure_bank(sounds, attenuation, concurrency):
    bank, created = existing_or_new(BANK, unreal.HollowSoundBank, data_asset_factory(unreal.HollowSoundBank))
    entries = []
    for tag, sound, group, audible_range, volume, pitch, noise in ENTRIES:
        entry = unreal.HollowSoundEntry()
        entry.set_editor_property("sound_tag", make_tag(tag))
        entry.set_editor_property("sound", sounds[sound])
        entry.set_editor_property("noise", make_tag(noise))
        entry.set_editor_property("audible_range", audible_range)
        entry.set_editor_property("volume", volume)
        entry.set_editor_property("pitch_variation", pitch)
        entry.set_editor_property("attenuation", attenuation)
        entry.set_editor_property("concurrency", concurrency[group] if group else None)
        entries.append(entry)
    current = bank.get_editor_property("entries")
    if created or len(current) != len(entries) or any(a.export_text() != b.export_text() for a, b in zip(current, entries)):
        bank.set_editor_property("entries", entries)
        save(bank)
        log("DA_SoundBank: %d sounds" % len(entries))
    return bank


def data_asset_factory(asset_class):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    return factory


def ensure_enemies():
    for path, values in sorted(ENEMIES.items()):
        definition = load(path)
        changed = False
        for name, tag in sorted(values["tags"].items()):
            if definition.get_editor_property(name).export_text() != make_tag(tag).export_text():
                definition.set_editor_property(name, make_tag(tag))
                changed = True
        cadence = definition.get_editor_property("footstep_cadence")
        if apply(cadence, values["cadence"]):
            definition.set_editor_property("footstep_cadence", cadence)
            changed = True
        if changed:
            save(definition)
            log("%s: wind-up and footstep sounds" % path.rsplit("/", 1)[1])


# ---------------------------------------------------------------- room acoustics

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


def room_size(room):
    cdo = unreal.get_default_object(unreal.HollowLevelMetrics)
    footprint = room.get_editor_property("footprint")
    return (footprint.x * float(cdo.get_editor_property("cell_size")), footprint.y * float(cdo.get_editor_property("cell_size")),
            footprint.z * float(cdo.get_editor_property("cell_height")))


def acoustics_spec(room):
    reverb, tone, tone_volume = ROOM_ACOUSTICS[room_type(room)]
    return {"room": room.get_name(), "size": room_size(room), "reverb": reverb, "reverb_values": REVERBS[reverb], "tone": tone,
            "tone_volume": tone_volume, "acoustics": ACOUSTICS}


def managed(actor, label):
    if actor is None:
        raise RuntimeError("Could not spawn " + label)
    actor.set_actor_label(label)
    actor.set_folder_path("Audio")
    actor.set_editor_property("tags", [MANAGED_TAG])
    return actor


def tags_of(actor):
    return [str(t) for t in actor.get_editor_property("tags")]


def ensure_room(room, reverbs, sounds):
    spec = acoustics_spec(room)
    wanted = digest(spec)
    level_path = room.get_editor_property("room_level").get_path_name().split(".")[0]
    if not levels.load_level(level_path):
        raise RuntimeError("Could not load " + level_path)
    level_actors = actors.get_all_level_actors()
    marker = next((a for a in level_actors if a.get_actor_label() == MARKER_LABEL), None)
    if marker is not None and wanted in tags_of(marker):
        return False
    for actor in level_actors:
        if MANAGED_TAG in tags_of(actor):
            actors.destroy_actor(actor)
    size = spec["size"]
    centre = unreal.Vector(size[0] / 2.0, size[1] / 2.0, size[2] / 2.0)
    zero = unreal.Rotator(0.0, 0.0, 0.0)
    volume = managed(actors.spawn_actor_from_class(unreal.AudioGameplayVolume, centre, zero), "RoomAcoustics")
    volume.set_actor_scale3d(unreal.Vector(size[0] / VOLUME_BRUSH, size[1] / VOLUME_BRUSH, size[2] / VOLUME_BRUSH))
    reverb = unreal.HollowAssetAuthoring.add_instance_component(volume, unreal.ReverbVolumeComponent, "Reverb")
    settings = reverb.get_editor_property("reverb_settings")
    settings.set_editor_property("apply_reverb", True)
    settings.set_editor_property("reverb_effect", reverbs[spec["reverb"]])
    settings.set_editor_property("volume", ACOUSTICS["reverb_volume"])
    settings.set_editor_property("fade_time", ACOUSTICS["reverb_fade"])
    reverb.set_editor_property("reverb_settings", settings)
    walls = unreal.HollowAssetAuthoring.add_instance_component(volume, unreal.FilterVolumeComponent, "Walls")
    walls.set_editor_property("exterior_lpf", ACOUSTICS["exterior_lpf"])
    walls.set_editor_property("exterior_lpf_time", ACOUSTICS["exterior_lpf_time"])
    tone = managed(actors.spawn_actor_from_class(unreal.AmbientSound, centre, zero), "RoomTone")
    component = tone.get_editor_property("audio_component")
    component.set_editor_property("sound", sounds[spec["tone"]])
    component.set_editor_property("volume_multiplier", spec["tone_volume"])
    component.set_editor_property("override_attenuation", True)
    attenuation = component.get_editor_property("attenuation_overrides")
    attenuation.set_editor_property("attenuation_shape", unreal.AttenuationShape.BOX)
    attenuation.set_editor_property("attenuation_shape_extents", unreal.Vector(size[0] / 2.0, size[1] / 2.0, size[2] / 2.0))
    attenuation.set_editor_property("falloff_distance", ACOUSTICS["tone_falloff"])
    attenuation.set_editor_property("spatialize", False)   # it is the room itself, from everywhere
    component.set_editor_property("attenuation_overrides", attenuation)
    note = managed(actors.spawn_actor_from_class(unreal.Note, unreal.Vector(0.0, 0.0, 0.0), zero), MARKER_LABEL)
    note.set_editor_property("tags", [MANAGED_TAG, wanted])
    if not levels.save_current_level():
        raise RuntimeError("Could not save " + level_path)
    log("%s: %s, %s" % (room.get_name(), spec["reverb"], spec["tone"]))
    return True


# ---------------------------------------------------------------- validation

def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def ini_has(line):
    with open(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_config_dir()) + "DefaultGame.ini") as ini:
        return any(entry.strip() == line for entry in ini)


def validate(rooms, sounds):
    validator = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
    result = validator.is_object_valid(load(BANK), unreal.DataValidationUsecase.SCRIPT)
    check(result[0] != unreal.DataValidationResult.INVALID, "DA_SoundBank matches the noise rules: %s" % [str(e) for e in result[1]])
    sound_class = load(SOUND_CLASS)
    for name, sound in sorted(sounds.items()):
        check(sound.get_editor_property("sound_class_object") == sound_class, name + " plays through SC_Hollow_World")
        check(assets.get_metadata_tag(sound, SPEC_TAG) == digest(METASOUNDS[name]()), name + " matches its spec")
    check(sound_class.get_editor_property("properties").get_editor_property("apply_ambient_volumes"), "room volumes apply to SC_Hollow_World")
    check(ini_has("SoundBank=" + object_path(BANK)), "UHollowAudioSettings::SoundBank")
    for room in rooms:
        check(room_type(room) in ROOM_ACOUSTICS, "acoustics for " + room.get_name())
    for path in sorted(ENEMIES):
        result = validator.is_object_valid(load(path), unreal.DataValidationUsecase.SCRIPT)
        check(result[0] != unreal.DataValidationResult.INVALID, "%s is valid: %s" % (path, [str(e) for e in result[1]]))
    log("validation passed (%d sounds, %d rooms)" % (len(sounds), len(rooms)))


def main():
    sounds, built = ensure_metasounds()
    attenuation = ensure_attenuation()
    concurrency = ensure_concurrency()
    reverbs = ensure_reverbs()
    ensure_bank(sounds, attenuation, concurrency)
    ensure_enemies()
    rooms = room_definitions()
    acoustics = sum(1 for room in rooms if ensure_room(room, reverbs, sounds))
    log("built %d MetaSound(s), %d room acoustics" % (built, acoustics))
    validate(rooms, sounds)
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_audio_assets failed:\n" + traceback.format_exc())
