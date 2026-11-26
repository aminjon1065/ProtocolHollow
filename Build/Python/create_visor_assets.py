"""Creates the S4-08 diegetic visor (idempotent, ADR-0010, ADR-0013, ADR-0026).

  /Game/Hollow/UI/Visor/M_UI_VisorGlass       UI material: the helmet's glass (vignette, rim, scan lines) and its alert
                                              glow, pulse and interference (parameters set by UHollowVisorFrameWidget)
  /Game/Hollow/UI/Visor/DA_VisorStyle         UHollowVisorStyle: Resonance tiers, telemetry texts and colours
  /Game/Hollow/UI/Visor/W_VisorFrame          the glass over the whole view
  /Game/Hollow/UI/Visor/W_ResonanceReadout    "RES 63%  HIGH" and its bar
  /Game/Hollow/UI/Visor/W_VoiceIndicator      microphone mode, transmission level, who is talking
  /Game/Hollow/UI/Visor/W_LampIndicator       "[F] LAMP ON"
  /Game/Hollow/UI/Visor/W_SupplyReadout       "PULSE MINE 2   FUEL CELL 3": the consumables of the raid inventory (S5-01)
  /Game/Hollow/UI/Visor/W_WeaponReadout       "NAILGUN  9/12  [36]": the weapon in hand, its magazine, the rounds carried (GP-01)
  /Game/Hollow/UI/Visor/W_Visor               the visor: glass, crosshair, target, prompt and the telemetry at the edges
                                              (health and stamina, tool fuel and heat, supplies, voice and lamp, Resonance, raid)
UX-01 (ADR-0042):
  /Game/Hollow/UI/Visor/M_UI_WaypointMarker   UI material: a diamond (Shape 0) or an edge chevron (Shape 1), tinted by
                                              the image's colour; MI_UI_WaypointPip / MI_UI_WaypointArrow
  /Game/Hollow/UI/Visor/W_VisorObjective      "OBJECTIVE" over what to do now
  /Game/Hollow/UI/Visor/W_VisorWaypoint       one waypoint: pip or arrow, "EVAC 42 m"
  /Game/Hollow/UI/Visor/W_VisorWaypoints      the full-screen layer the waypoints are placed on
  /Game/Hollow/UI/Visor/W_ControlsHint        "[H] CONTROLS" and a line per action
  /Game/Hollow/UI/Visor/DA_ControlsHint       UHollowControlsHintSet: the cheat sheet's rows and class labels
  /Game/Hollow/UI/Input/IA_ControlsHint       folds the cheat sheet (H / View), mapped in IMC_UI
NAR-01a (ADR-0049):
  /Game/Hollow/UI/Visor/W_RadioSubtitle       the handler's radio: call sign over the line on the air, low in the middle
NAR-01b (ADR-0050):
  /Game/Hollow/UI/Visor/W_LoreNotice          "CREW LOG 3/12", the log's title, its bonus: under the objective a few seconds
GP-01c (ADR-0046):
  /Game/Hollow/UI/Visor/M_UI_HitMarker        UI material: four diagonal ticks round the crosshair, tinted by the image
  /Game/Hollow/UI/Visor/W_HitMarker           the hit marker (hit / armour / crit: colour and size from DA_VisorStyle)
  M_UI_VisorGlass                             + the damage edge (Damage, DamageColor); DA_VisorStyle + marker and flash data

Nothing heavy: one node-only material and small widget trees. Every asset keeps a hash of its spec (metadata
"HollowSpec") and is rebuilt only when its spec changes - every save is a new LFS object.
Run after create_ui_assets.py and create_lamp_assets.py:  Build/Scripts/run-editor-python.sh Build/Python/create_visor_assets.py
"""
import hashlib
import json
import os
import sys
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hollow_loc as loc   # LOC-01 (ADR-0048): player texts are string table entries

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

VISOR_DIR = "/Game/Hollow/UI/Visor"
GLASS_PATH = VISOR_DIR + "/M_UI_VisorGlass"
STYLE_PATH = VISOR_DIR + "/DA_VisorStyle"
TELEMETRY_FONT = "/Engine/EngineFonts/DroidSansMono"
ATTRIBUTE_MODULE = "/Script/HollowGAS"
OVERHEATED_TAG = "Hollow.State.Tool.Overheated"
SPEC_TAG = "HollowSpec"
INI = "Config/DefaultGame.ini"
INI_LINES = ["VisorStyle=/Game/Hollow/UI/Visor/DA_VisorStyle.DA_VisorStyle",
             "HelmetLampAction=/Game/Hollow/Core/Input/Actions/IA_HelmetLamp.IA_HelmetLamp",
             "ControlsHintAction=/Game/Hollow/UI/Input/IA_ControlsHint.IA_ControlsHint",
             "ControlsHint=/Game/Hollow/UI/Visor/DA_ControlsHint.DA_ControlsHint"]

# UX-01 (ADR-0042): the waypoint marks, the cheat sheet's input and rows.
MARKER_PATH = VISOR_DIR + "/M_UI_WaypointMarker"
CLEAR_ATTEMPTS = 16
MARKER_INSTANCES = {"MI_UI_WaypointPip": 0.0, "MI_UI_WaypointArrow": 1.0}   # Shape
MARKER = {
    "version": 3,
    "diamond": [0.44, 0.48, 0.26, 0.30, 0.35],   # outer edge from..to, hole from..to, fill opacity inside the ring
    "chevron": [0.22, 1.1, 0.03, 0.17, 0.38],    # tip v, slope, edge softness, band width, half span in u
}
INPUT_DIR = "/Game/Hollow/UI/Input"
CONTROLS_ACTION = "IA_ControlsHint"
CONTROLS_KEYS = ["H", "Gamepad_Special_Left"]
CONTROLS_PATH = VISOR_DIR + "/DA_ControlsHint"
GAME_INPUT_DIR = "/Game/Hollow/Core/Input/Actions"
ENGINEER, OCCULTIST = "Hollow.Class.Engineer", "Hollow.Class.Occultist"
# (action, label, {class: own label, "" hides it}, raid only). The keys come from the mapping contexts at run time; the
# weapon in hand fires and aims on the tool's inputs (GP-01). Labels are (key, English source) in loc.VISOR (LOC-01).
CONTROLS_ROWS = [
    ("IA_PrimaryUse", ("Controls.Tool", "Tool"),
     {ENGINEER: ("Controls.Tool.Engineer", "Plasma cutter"), OCCULTIST: ("Controls.Tool.Occultist", "Aether lantern")}, True),
    ("IA_SecondaryUse", ("Controls.Welder", "Welder"), {OCCULTIST: ""}, True),
    ("IA_HandsCycle", ("Controls.Hands", "Tools / weapon"), {}, True),
    ("IA_Gadget", ("Controls.Gadget", "Gadget"),
     {ENGINEER: ("Controls.Gadget.Engineer", "Pulse mine"), OCCULTIST: ("Controls.Gadget.Occultist", "Ritual ward")}, True),
    ("IA_Reload", ("Controls.Reload", "Reload / refuel"), {}, True),
    ("IA_Melee", ("Controls.Melee", "Melee"), {}, True),
    ("IA_HelmetLamp", ("Controls.Lamp", "Helmet lamp"), {}, False),
    ("IA_PushToTalk", ("Controls.Voice", "Voice"), {}, False),
    ("IA_Interact", ("Controls.Interact", "Interact"), {}, False),
]

# The glass (data). r is the distance from the centre in half-screens: 0 centre, 1 the middle of an edge, 1.41 a corner.
GLASS = {
    "version": 3,
    "vignette": [0.75, 1.5, 0.4],             # from r, to r, darkest opacity
    "rim": [1.32, 0.14, 0.09],                # radius, half width, opacity
    "rim_color": [0.75, 0.9, 1.0],
    "scan_lines": 270.0, "scan_depth": 0.02,  # lines over the height, opacity
    "alert": [0.7, 1.4],                      # the glow grows from r to r
    "pulse_depth": 0.5,                       # how far the pulse dims the glow
    "bands": [3.0, 0.08, 0.07, 0.35],         # count over the height, drift per second, width, from r (the centre stays clear)
    "alert_color": [1.0, 0.3, 0.1],           # defaults; DA_VisorStyle sets them per tier
    "damage": [0.55, 1.35],                   # GP-01c: the damage edge grows from r to r
    "damage_color": [0.95, 0.08, 0.05],       # default; DA_VisorStyle's DamageColor
}
GLASS_PARAMETERS = ["Alert", "PulseHz", "Interference", "Damage"]
GLASS_COLOR_PARAMETER = "AlertColor"
GLASS_DAMAGE_COLOR_PARAMETER = "DamageColor"

# GP-01c: the hit marker's ticks in the image's UV (0.5 = its edge): from, to, half width, edge softness.
HIT_MARKER_PATH = VISOR_DIR + "/M_UI_HitMarker"
HIT_MARKER = {"version": 1, "ticks": [0.16, 0.44, 0.035, 0.015]}

# The look per Resonance tier (S4-08 clarification 7): calm Low and Medium; High amber at 1 Hz; Critical red at 2 Hz with
# interference. Every pulse stays at or under 3 Hz (UHollowVisorStyle::MaxPulseHz, photosensitivity).
# Texts are (key, English source) in loc.VISOR (LOC-01, ADR-0048).
TIERS = [
    (("Visor.Tier.Low", "LOW"), (0.55, 0.85, 1.0, 1.0), 0.0, 0.0, 0.0),
    (("Visor.Tier.Medium", "MEDIUM"), (0.85, 0.9, 0.5, 1.0), 0.0, 0.0, 0.0),
    (("Visor.Tier.High", "HIGH"), (1.0, 0.68, 0.25, 1.0), 0.35, 1.0, 0.0),
    (("Visor.Tier.Critical", "CRITICAL"), (1.0, 0.36, 0.28, 1.0), 0.6, 2.0, 0.12),
]
STYLE_TEXTS = {
    "resonance_format": ("Visor.Resonance.Format", "RES {0}%"),
    "push_to_talk_label": ("Visor.Voice.PushToTalk", "MIC  PTT"), "open_mic_label": ("Visor.Voice.OpenMic", "MIC  OPEN"),
    "muted_label": ("Visor.Voice.Muted", "MIC  MUTED"),
    "transmitting_format": ("Visor.Voice.Transmitting", "{0}  >> TX"), "talking_format": ("Visor.Voice.Talking", "VOX  {0}"),
    "lamp_on_label": ("Visor.Lamp.On", "LAMP ON"), "lamp_off_label": ("Visor.Lamp.Off", "LAMP OFF"), "lamp_format": ("Visor.Lamp.Format", "[{0}] {1}"),
    "supply_format": ("Visor.Supply.Format", "{0} {1}"), "supply_separator": ("Visor.Supply.Separator", "   "),
    "objective_title": ("Visor.Objective.Title", "OBJECTIVE"), "waypoint_format": ("Visor.Waypoint.Format", "{0}  {1} m"),
    "controls_title_format": ("Visor.Controls.TitleFormat", "[{0}] CONTROLS"), "controls_title": ("Visor.Controls.Title", "CONTROLS"),
    # NAR-01b: a downloaded crew log.
    "lore_notice_format": ("Visor.Lore.Format", "CREW LOG {0}/{1}"), "lore_notice_new": ("Visor.Lore.New", "+{0} cr on extraction"),
    "lore_notice_known": ("Visor.Lore.Known", "Already in your archive"),
}
STYLE_COLORS = {
    "telemetry_color": (0.6, 0.88, 1.0, 0.9),
    "transmitting_color": (0.45, 1.0, 0.55, 1.0), "muted_color": (0.65, 0.45, 0.45, 0.85),
    "lamp_on_color": (1.0, 0.95, 0.85, 0.9), "lamp_off_color": (0.5, 0.55, 0.6, 0.75),
    "objective_color": (0.92, 0.95, 1.0, 0.95),
    # GP-01c: the hit marker white for a hit, steel for armour, amber for a crit; the damage edge red.
    "hit_color": (1.0, 1.0, 1.0, 0.95), "armor_color": (0.55, 0.62, 0.7, 0.85), "crit_color": (1.0, 0.72, 0.25, 1.0),
    "damage_color": (0.95, 0.1, 0.06, 1.0),
    # NAR-01a: the radio's subtitle near white; the Echo-Stalker's corrupted transmissions a sick violet.
    "radio_text_color": (0.94, 0.96, 1.0, 0.95), "radio_echo_color": (0.78, 0.45, 1.0, 0.95),
}
STYLE_SCALARS = {"waypoint_rate_hz": 12.0, "waypoint_hide_metres": 2.5, "waypoint_edge_margin": 96.0,
                 # GP-01c: a marker for 0.18 s (armour smaller, a crit larger); the damage edge flashes over 0.45 s, full
                 # strength at a quarter of the health at once; reduced flashes: a dim edge fading over 1.2 s, no flash.
                 "hit_marker_seconds": 0.18, "armor_marker_scale": 0.8, "crit_marker_scale": 1.3,
                 "damage_flash_seconds": 0.45, "damage_full_share": 0.25, "reduced_damage_level": 0.35, "reduced_damage_seconds": 1.2,
                 "damage_fade_rate_hz": 30.0,
                 "lore_notice_seconds": 6.0,   # NAR-01b: a downloaded crew log's notice
                 # UI-02 (ADR-0052): a marker keeps this far off the readouts below; it is some 50 px each way with its label.
                 "waypoint_keep_out_padding": 12.0, "waypoint_marker_radius": 52.0}
# UI-02 (ADR-0052): the readouts the markers keep off (widgets of W_Visor, while shown): the cheat sheet, the health and
# tool bars, the supply and weapon lines, the objective and the Resonance readout.
WAYPOINT_KEEP_OUT = ["ControlsHint", "HealthBar", "StaminaBar", "ToolFuelBar", "ToolHeatBar", "SupplyReadout", "WeaponReadout",
                     "Objective", "ResonanceReadout"]
# Hub terminals pale cyan (the contracts console amber: it launches), extraction green, the vault aether cyan, the
# altar ritual gold (Docs/Art bible palette).
WAYPOINT_LOOKS = [
    ("CONTRACTS", ("Visor.Waypoint.Contracts", "CONTRACTS"), (1.0, 0.72, 0.35, 1.0)),
    ("WORKBENCH", ("Visor.Waypoint.Workbench", "WORKBENCH"), (0.6, 0.88, 1.0, 1.0)),
    ("LOCKER", ("Visor.Waypoint.Locker", "LOCKER"), (0.6, 0.88, 1.0, 1.0)),
    ("EXTRACTION", ("Visor.Waypoint.Evac", "EVAC"), (0.45, 1.0, 0.55, 1.0)),
    ("VAULT", ("Visor.Waypoint.Vault", "VAULT"), (0.32, 0.85, 0.84, 1.0)),
    ("ALTAR", ("Visor.Waypoint.Altar", "ALTAR"), (0.95, 0.8, 0.45, 1.0)),
]
# The visor's health and tool bars (W_Visor): bar -> (key, English source) in loc.VISOR.
BAR_LABELS = {"HealthBar": ("Visor.Bar.Health", "Health"), "StaminaBar": ("Visor.Bar.Stamina", "Stamina"),
              "ToolFuelBar": ("Visor.Bar.Fuel", "Fuel"), "ToolHeatBar": ("Visor.Bar.Heat", "Heat")}

# Telemetry widgets (data).
TELEMETRY_TEXT = 16
TELEMETRY_SMALL = 13
TEXT_SHADOW = (1.0, 1.0)
SHADOW_COLOR = (0.0, 0.0, 0.0, 0.6)
GAP = 4.0
READOUT_BAR = (200.0, 4.0)
VOICE_BAR = (140.0, 3.0)
WIDGETS = {
    "W_VisorFrame": {"version": 1},
    "W_ResonanceReadout": {"version": 1, "text": TELEMETRY_TEXT, "bar": READOUT_BAR, "gap": GAP},
    "W_VoiceIndicator": {"version": 1, "text": TELEMETRY_TEXT, "small": TELEMETRY_SMALL, "bar": VOICE_BAR, "gap": GAP},
    "W_LampIndicator": {"version": 1, "text": TELEMETRY_TEXT},
    "W_SupplyReadout": {"version": 1, "text": TELEMETRY_TEXT},
    "W_WeaponReadout": {"version": 1, "text": TELEMETRY_TEXT},
    "W_VisorObjective": {"version": 1, "title": TELEMETRY_SMALL, "text": TELEMETRY_TEXT, "wrap": 620.0, "gap": GAP},
    "W_VisorWaypoint": {"version": 1, "pip": 14.0, "arrow": 26.0, "text": TELEMETRY_SMALL, "gap": GAP},
    "W_VisorWaypoints": {"version": 1},
    "W_ControlsHint": {"version": 1, "title": TELEMETRY_SMALL, "text": TELEMETRY_SMALL, "gap": GAP},
    "W_HitMarker": {"version": 1, "size": 44.0},
    # NAR-01a: the subtitle in the UI's reading font (Roboto), wrapped short enough to stay clear of the bars at the bottom.
    "W_RadioSubtitle": {"version": 1, "speaker": TELEMETRY_SMALL, "text": 20, "wrap": 860.0, "gap": GAP},
    "W_LoreNotice": {"version": 1, "title": TELEMETRY_SMALL, "text": TELEMETRY_TEXT, "wrap": 620.0, "gap": GAP},   # NAR-01b
}

# W_Visor layout (data): distances from the screen edges, inside the glass's rim.
LAYOUT = {
    "version": 8,
    "edge": 48.0,
    "crosshair": 6.0,
    "hit_marker": 44.0,                       # GP-01c: round the crosshair
    "target": [360.0, 48.0, 60.0],             # size, below the crosshair
    "prompt_offset": 120.0,
    "health": [320.0, 26.0], "stamina": [320.0, 20.0], "bar_gap": 32.0,
    "tool_bar": [300.0, 24.0], "tool_gap": 36.0,
    "supply_above_tools": 72.0,
    "weapon_above_supply": 30.0,              # GP-01: the weapon's line over the supplies
    "lamp_below_voice": 64.0,
    "objective_below_voice": 108.0,           # UX-01: under the lamp line
    "controls_below_readout": 76.0,           # UX-01: under the Resonance readout
    "subtitle_above_bottom": 120.0,           # NAR-01a: the radio's subtitle, over the bars' line
    "notice_below_objective": 84.0,           # NAR-01b: a crew log's notice, under the objective's two lines
    "colors": {"health": (0.75, 0.15, 0.15, 1.0), "stamina": (0.35, 0.85, 0.55, 1.0), "fuel": (0.2, 0.6, 1.0, 1.0),
               "heat": (1.0, 0.55, 0.1, 1.0), "alert": (1.0, 0.1, 0.05, 1.0), "crosshair": (1.0, 1.0, 1.0, 1.0)},
}

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
mel = unreal.MaterialEditingLibrary
author = unreal.HollowWidgetAuthoring
H = unreal.HorizontalAlignment
V = unreal.VerticalAlignment
NO_PADDING = unreal.Margin(0.0, 0.0, 0.0, 0.0)


def log(message):
    unreal.log("create_visor_assets: " + message)


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


def up_to_date(asset, spec_hash):
    return assets.get_metadata_tag(asset, SPEC_TAG) == spec_hash


def stamp(asset, spec_hash):
    assets.set_metadata_tag(asset, SPEC_TAG, spec_hash)
    save(asset)


def color(rgba):
    return unreal.LinearColor(*rgba)


def from_text(struct, text):
    if not struct.import_text(text):
        raise RuntimeError("Could not import " + text)
    return struct


def make_tag(name):
    return from_text(unreal.GameplayTag(), '(TagName="%s")' % name)


def make_attribute(set_name, attribute):
    attr = unreal.GameplayAttribute()
    path = "%s.%s:%s" % (ATTRIBUTE_MODULE, set_name, attribute)
    text = '(AttributeName="%s",Attribute=%s,AttributeOwner="/Script/CoreUObject.Class\'%s.%s\'")' % (attribute, path, ATTRIBUTE_MODULE, set_name)
    if not attr.import_text(text) or path not in attr.export_text():
        raise RuntimeError("Could not build attribute " + path)
    return attr


# ---------------------------------------------------------------- the glass

class Graph(object):
    """Places material expressions in columns and wires them."""

    def __init__(self, material):
        self.material = material
        self.row = 0

    def node(self, expression_class, column, **properties):
        expression = mel.create_material_expression(self.material, expression_class, -300 * column, 120 * self.row)
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

    def smooth(self, column, low, high, value):
        """0 below low, 1 above high, smooth between."""
        expression = self.node(unreal.MaterialExpressionSmoothStep, column, const_min=low, const_max=high)
        self.wire(value, expression, "Value")
        return expression

    def half_wave(self, column, wave):
        """-1..1 to 0..1."""
        return self.math(unreal.MaterialExpressionAdd, column, self.math(unreal.MaterialExpressionMultiply, column, wave, const_b=0.5), const_b=0.5)

    def scalar(self, name, column, default=0.0):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=default)


def build_glass(material):
    m = GLASS
    Add, Sub, Mul = unreal.MaterialExpressionAdd, unreal.MaterialExpressionSubtract, unreal.MaterialExpressionMultiply
    clear_graph(material)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    g = Graph(material)

    # r: distance from the centre in half-screens (the glass is the screen's ellipse).
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 12)
    centre = g.node(unreal.MaterialExpressionConstant2Vector, 12, r=0.5, g=0.5)
    distance = g.node(unreal.MaterialExpressionDistance, 11)
    g.wire(uv, distance, "A")
    g.wire(centre, distance, "B")
    r = g.math(Mul, 10, distance, const_b=2.0)
    height = g.node(unreal.MaterialExpressionComponentMask, 11, r=False, g=True, b=False, a=False)
    g.wire(uv, height)

    # The calm glass: a dark edge, a faint rim of reflected light, scan lines.
    vignette = g.math(Mul, 7, g.smooth(8, m["vignette"][0], m["vignette"][1], r), const_b=m["vignette"][2])
    off_rim = g.single(unreal.MaterialExpressionAbs, 8, g.math(Sub, 9, r, const_b=m["rim"][0]))
    rim = g.math(Mul, 5, g.single(unreal.MaterialExpressionSaturate, 6,
                                  g.single(unreal.MaterialExpressionOneMinus, 7, g.math(Mul, 7, off_rim, const_b=1.0 / m["rim"][1]))), const_b=m["rim"][2])
    lines = g.single(unreal.MaterialExpressionSine, 8, g.math(Mul, 9, height, const_b=m["scan_lines"]), period=1.0)
    scan = g.math(Mul, 6, g.half_wave(7, lines), const_b=m["scan_depth"])

    # The alert: a glow at the edges in the tier's colour, pulsing; interference bands drifting outside the centre.
    alert = g.scalar("Alert", 10)
    pulse_hz = g.scalar("PulseHz", 10)
    interference = g.scalar("Interference", 10)
    alert_color = g.node(unreal.MaterialExpressionVectorParameter, 10, parameter_name=GLASS_COLOR_PARAMETER, default_value=color(m["alert_color"] + [1.0]))
    time = g.node(unreal.MaterialExpressionTime, 10)
    wave = g.single(unreal.MaterialExpressionSine, 8, g.math(Mul, 9, time, pulse_hz), period=1.0)
    pulse = g.single(unreal.MaterialExpressionOneMinus, 6, g.math(Mul, 7, g.half_wave(7, wave), const_b=m["pulse_depth"]))
    glow = g.math(Mul, 5, g.math(Mul, 6, g.smooth(7, m["alert"][0], m["alert"][1], r), alert), pulse)
    count, drift, width, from_r = m["bands"]
    phase = g.single(unreal.MaterialExpressionFrac, 7, g.math(Add, 8, g.math(Mul, 9, height, const_b=count), g.math(Mul, 9, time, const_b=drift)))
    band = g.single(unreal.MaterialExpressionOneMinus, 6, g.smooth(7, 0.0, width, phase))
    bands = g.math(Mul, 5, g.math(Mul, 5, band, interference), g.smooth(6, from_r, m["rim"][0], r))
    tint = g.math(Add, 4, glow, bands)

    # GP-01c: the damage edge, in its own colour, as strong as the widget's Damage (its flash and fade are the widget's).
    damage = g.scalar("Damage", 10)
    damage_color = g.node(unreal.MaterialExpressionVectorParameter, 10, parameter_name=GLASS_DAMAGE_COLOR_PARAMETER,
                          default_value=color(m["damage_color"] + [1.0]))
    hurt = g.math(Mul, 5, g.smooth(6, m["damage"][0], m["damage"][1], r), damage)

    # Darkening plus light: opacity is all of it, the colour only the light (divided back out of the blend).
    opacity = g.single(unreal.MaterialExpressionSaturate, 2, g.math(Add, 3, g.math(Add, 4, vignette, rim),
                                                                   g.math(Add, 4, g.math(Add, 5, tint, scan), hurt)))
    rim_color = g.node(unreal.MaterialExpressionConstant3Vector, 5, constant=color(m["rim_color"] + [1.0]))
    light = g.math(Add, 3, g.math(Add, 4, g.math(Mul, 5, rim_color, rim), g.math(Mul, 5, alert_color, tint)), g.math(Mul, 4, damage_color, hurt))
    final = g.math(unreal.MaterialExpressionDivide, 1, light, g.math(unreal.MaterialExpressionMax, 2, opacity, const_b=0.001))
    if not mel.connect_material_property(final, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Could not connect the final colour")
    if not mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError("Could not connect the opacity")
    mel.recompile_material(material)


def ensure_glass():
    material, created = existing_or_new(GLASS_PATH, unreal.Material, unreal.MaterialFactoryNew())
    spec_hash = digest(GLASS)
    if not created and up_to_date(material, spec_hash):
        return material
    build_glass(material)
    stamp(material, spec_hash)
    log("M_UI_VisorGlass built (%d expressions)" % mel.get_num_material_expressions(material))
    return material


# ---------------------------------------------------------------- the style

def ensure_style():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.HollowVisorStyle)
    style, created = existing_or_new(STYLE_PATH, unreal.HollowVisorStyle, factory)
    spec_hash = digest(TIERS, STYLE_TEXTS, STYLE_COLORS, STYLE_SCALARS, WAYPOINT_LOOKS, WAYPOINT_KEEP_OUT)
    if not created and up_to_date(style, spec_hash):
        return style
    looks = []
    for label, rgba, alert, pulse_hz, interference in TIERS:
        look = unreal.HollowVisorTierLook()
        look.set_editor_property("label", loc.text(loc.VISOR, label[0], label[1]))
        look.set_editor_property("color", color(rgba))
        look.set_editor_property("alert", alert)
        look.set_editor_property("pulse_hz", pulse_hz)
        look.set_editor_property("interference", interference)
        looks.append(look)
    style.set_editor_property("tiers", looks)
    for name, (key, value) in STYLE_TEXTS.items():
        style.set_editor_property(name, loc.text(loc.VISOR, key, value))
    for name, rgba in STYLE_COLORS.items():
        style.set_editor_property(name, color(rgba))
    for name, value in STYLE_SCALARS.items():
        style.set_editor_property(name, value)
    waypoints = []
    for kind, label, rgba in WAYPOINT_LOOKS:
        look = unreal.HollowWaypointLook()
        look.set_editor_property("kind", getattr(unreal.HollowWaypointKind, kind))
        look.set_editor_property("label", loc.text(loc.VISOR, label[0], label[1]))
        look.set_editor_property("color", color(rgba))
        waypoints.append(look)
    style.set_editor_property("waypoints", waypoints)
    style.set_editor_property("waypoint_keep_out", WAYPOINT_KEEP_OUT)
    stamp(style, spec_hash)
    log("DA_VisorStyle ready")
    return style


# ---------------------------------------------------------------- waypoint marks (UX-01)

def clear_graph(material):
    """Every expression gone: delete_all_material_expressions removes about half of a graph per call (it walks the list it edits)."""
    for _ in range(CLEAR_ATTEMPTS):
        if not mel.get_num_material_expressions(material):
            break
        mel.delete_all_material_expressions(material)
    if mel.get_num_material_expressions(material):
        raise RuntimeError("%s kept %d old expressions" % (material.get_name(), mel.get_num_material_expressions(material)))


def build_hit_marker(material):
    """Four ticks on the diagonals (an X with its middle open), tinted by the image's colour."""
    near, far, half, soft = HIT_MARKER["ticks"]
    Add, Sub, Mul = unreal.MaterialExpressionAdd, unreal.MaterialExpressionSubtract, unreal.MaterialExpressionMultiply
    clear_graph(material)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    g = Graph(material)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 12)
    offset = g.math(Sub, 11, uv, const_b=0.5)
    x = g.node(unreal.MaterialExpressionComponentMask, 10, r=True, g=False, b=False, a=False)
    g.wire(offset, x)
    y = g.node(unreal.MaterialExpressionComponentMask, 10, r=False, g=True, b=False, a=False)
    g.wire(offset, y)
    # Distances along the two diagonals: (x + y) / sqrt 2 and (x - y) / sqrt 2.
    diagonal = g.single(unreal.MaterialExpressionAbs, 8, g.math(Mul, 9, g.math(Add, 9, x, y), const_b=0.70710678))
    anti = g.single(unreal.MaterialExpressionAbs, 8, g.math(Mul, 9, g.math(Sub, 9, x, y), const_b=0.70710678))

    def tick(along, across):
        thin = g.single(unreal.MaterialExpressionOneMinus, 6, g.smooth(7, half - soft, half + soft, across))
        start = g.smooth(7, near - soft, near + soft, along)
        end = g.single(unreal.MaterialExpressionOneMinus, 6, g.smooth(7, far - soft, far + soft, along))
        return g.math(Mul, 4, g.math(Mul, 5, thin, start), end)

    mask = g.single(unreal.MaterialExpressionSaturate, 2, g.math(Add, 3, tick(diagonal, anti), tick(anti, diagonal)))
    tint = g.node(unreal.MaterialExpressionVertexColor, 4)   # UMG passes the image's colour as the vertex colour
    if not mel.connect_material_property(tint, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Could not connect the hit marker colour")
    if not mel.connect_material_property(mask, "", unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError("Could not connect the hit marker opacity")
    mel.recompile_material(material)


def ensure_hit_marker_material():
    material, created = existing_or_new(HIT_MARKER_PATH, unreal.Material, unreal.MaterialFactoryNew())
    spec_hash = digest(HIT_MARKER)
    if created or not up_to_date(material, spec_hash):
        build_hit_marker(material)
        stamp(material, spec_hash)
        log("M_UI_HitMarker built (%d expressions)" % mel.get_num_material_expressions(material))
    return material


def build_marker(material):
    m = MARKER
    Add, Sub, Mul = unreal.MaterialExpressionAdd, unreal.MaterialExpressionSubtract, unreal.MaterialExpressionMultiply
    clear_graph(material)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    g = Graph(material)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 12)
    offset = g.single(unreal.MaterialExpressionAbs, 10, g.math(Sub, 11, uv, const_b=0.5))
    ax = g.node(unreal.MaterialExpressionComponentMask, 9, r=True, g=False, b=False, a=False)
    g.wire(offset, ax)
    v = g.node(unreal.MaterialExpressionComponentMask, 9, r=False, g=True, b=False, a=False)
    g.wire(uv, v)

    # The diamond: an opaque ring and a faint inside (|x| + |y| is the distance in the diamond's own metric).
    out_from, out_to, hole_from, hole_to, fill = m["diamond"]
    ay = g.node(unreal.MaterialExpressionComponentMask, 9, r=False, g=True, b=False, a=False)
    g.wire(offset, ay)
    metric = g.math(Add, 8, ax, ay)
    inside = g.single(unreal.MaterialExpressionOneMinus, 6, g.smooth(7, out_from, out_to, metric))
    core = g.single(unreal.MaterialExpressionOneMinus, 6, g.smooth(7, hole_from, hole_to, metric))
    ring = g.math(Sub, 5, inside, core)
    diamond = g.math(Add, 4, ring, g.math(Mul, 5, core, const_b=fill))

    # The chevron pointing up: a band under the line v = tip + slope |x|, cut at the span's ends.
    tip, slope, soft, band, span = m["chevron"]
    above = g.math(Sub, 7, v, g.math(Add, 8, g.math(Mul, 9, ax, const_b=slope), const_b=tip))
    lower = g.smooth(6, -soft, soft, above)
    upper = g.single(unreal.MaterialExpressionOneMinus, 5, g.smooth(6, band - soft, band + soft, above))
    ends = g.single(unreal.MaterialExpressionOneMinus, 5, g.smooth(6, span - soft, span + soft, ax))
    chevron = g.math(Mul, 3, g.math(Mul, 4, lower, upper), ends)

    shape = g.scalar("Shape", 4)
    lerp = g.node(unreal.MaterialExpressionLinearInterpolate, 2)
    g.wire(diamond, lerp, "A")
    g.wire(chevron, lerp, "B")
    g.wire(shape, lerp, "Alpha")
    # The image's colour tints it (UMG passes it as the vertex colour; its main output is RGB only).
    tint = g.node(unreal.MaterialExpressionVertexColor, 4)
    opacity = g.single(unreal.MaterialExpressionSaturate, 1, lerp)
    if not mel.connect_material_property(tint, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Could not connect the marker colour")
    if not mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError("Could not connect the marker opacity")
    mel.recompile_material(material)


def ensure_marker_material():
    material, created = existing_or_new(MARKER_PATH, unreal.Material, unreal.MaterialFactoryNew())
    spec_hash = digest(MARKER)
    if created or not up_to_date(material, spec_hash):
        build_marker(material)
        stamp(material, spec_hash)
        log("M_UI_WaypointMarker built (%d expressions)" % mel.get_num_material_expressions(material))
    instances = {}
    for name, shape in MARKER_INSTANCES.items():
        instance, made = existing_or_new(VISOR_DIR + "/" + name, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        instance_hash = digest(MARKER, name, shape)
        if made or not up_to_date(instance, instance_hash):
            instance.set_editor_property("parent", material)
            mel.set_material_instance_scalar_parameter_value(instance, "Shape", shape)
            stamp(instance, instance_hash)
            log(name + " ready")
        instances[name] = instance
    return instances


# ---------------------------------------------------------------- the cheat sheet (UX-01)

def make_key(name):
    key = unreal.Key()
    if not key.import_text(name):
        raise RuntimeError("Unknown key " + name)
    return key


def ensure_controls_input():
    """IA_ControlsHint, a UI action like the system menu's, and its keys in IMC_UI (create_ui_assets.py)."""
    path = INPUT_DIR + "/" + CONTROLS_ACTION
    action, created = existing_or_new(path, unreal.InputAction, unreal.InputAction_Factory())
    if created:
        action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        action.set_editor_property("consume_input", False)   # CommonUI reads it; it never takes a key from the game
        save(action)
        log("created " + CONTROLS_ACTION)
    context = load(INPUT_DIR + "/IMC_UI")
    data = context.get_editor_property("default_key_mappings")
    entries = list(data.get_editor_property("mappings"))
    known = set((e.get_editor_property("action").get_path_name(), e.get_editor_property("key").export_text()) for e in entries
                if e.get_editor_property("action") is not None)
    added = 0
    for key_name in CONTROLS_KEYS:
        key = make_key(key_name)
        if (action.get_path_name(), key.export_text()) in known:
            continue
        entry = unreal.EnhancedActionKeyMapping()
        entry.set_editor_property("action", action)
        entry.set_editor_property("key", key)
        entries.append(entry)
        added += 1
    if added:
        data.set_editor_property("mappings", entries)
        context.set_editor_property("default_key_mappings", data)
        save(context)
        log("IMC_UI: %d cheat sheet key(s) mapped" % added)


def label_text(label):
    """(key, English source) in loc.VISOR; "" hides a row for a class."""
    return loc.text(loc.VISOR, label[0], label[1]) if label else unreal.Text("")


def ensure_controls_rows():
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.HollowControlsHintSet)
    rows_asset, created = existing_or_new(CONTROLS_PATH, unreal.HollowControlsHintSet, factory)
    spec_hash = digest(CONTROLS_ROWS)
    if not created and up_to_date(rows_asset, spec_hash):
        return
    rows = []
    for action, label, class_labels, raid_only in CONTROLS_ROWS:
        row = unreal.HollowControlsHintRow()
        row.set_editor_property("action", load(GAME_INPUT_DIR + "/" + action))
        row.set_editor_property("label", label_text(label))
        own = []
        for class_tag, class_label in sorted(class_labels.items()):
            entry = unreal.HollowControlsClassLabel()
            entry.set_editor_property("class_tag", make_tag(class_tag))
            entry.set_editor_property("label", label_text(class_label))
            own.append(entry)
        row.set_editor_property("class_labels", own)
        row.set_editor_property("raid_only", raid_only)
        rows.append(row)
    rows_asset.set_editor_property("rows", rows)
    stamp(rows_asset, spec_hash)
    log("DA_ControlsHint ready (%d rows)" % len(rows))


# ---------------------------------------------------------------- widgets

def widget_blueprint(name, parent_class, spec):
    """The WBP to build, emptied; None if it matches its spec already."""
    path = VISOR_DIR + "/" + name
    if assets.does_asset_exist(path):
        blueprint = load(path)
        if up_to_date(blueprint, digest(spec)):
            return None
        if not author.clear_widgets(blueprint):
            raise RuntimeError("Could not clear " + name)
        return blueprint
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    blueprint = create(path, unreal.WidgetBlueprint, factory)
    if author.find_widget(blueprint, "CanvasPanel_0") is not None or author.find_widget(blueprint, "RootCanvas") is not None:
        raise RuntimeError(path + ": the factory added a root widget; clear it first")
    return blueprint


def add(blueprint, widget_class, name, parent=None):
    widget = author.add_widget(blueprint, widget_class, name, parent or "None")
    if widget is None:
        raise RuntimeError("Could not add %s to %s" % (name, blueprint.get_name()))
    return widget


def box_slot(widget, h=H.H_ALIGN_FILL, v=V.V_ALIGN_FILL, padding=NO_PADDING, fill=0.0):
    if not author.set_box_slot(widget, h, v, padding, fill):
        raise RuntimeError("No box slot on " + widget.get_name())


def canvas_slot(widget, anchor, alignment, position, size=unreal.Vector2D(0.0, 0.0), auto_size=False):
    if not author.set_canvas_slot(widget, anchor, alignment, position, size, auto_size):
        raise RuntimeError("No canvas slot on " + widget.get_name())


def telemetry_text(widget, size, justification=unreal.TextJustify.LEFT):
    widget.set_editor_property("text", unreal.Text(""))
    font = widget.get_editor_property("font")
    font.set_editor_property("font_object", load(TELEMETRY_FONT))
    font.set_editor_property("size", size)
    widget.set_editor_property("font", font)
    widget.set_editor_property("justification", justification)
    widget.set_editor_property("shadow_offset", unreal.Vector2D(*TEXT_SHADOW))
    widget.set_editor_property("shadow_color_and_opacity", color(SHADOW_COLOR))


def thin_bar(blueprint, name, parent, size):
    box = add(blueprint, unreal.SizeBox, name + "Box", parent)
    box.set_width_override(size[0])
    box.set_height_override(size[1])
    bar = add(blueprint, unreal.ProgressBar, name, box.get_name())
    return box, bar


def finish(blueprint, spec):
    if not author.compile(blueprint):
        raise RuntimeError(blueprint.get_name() + " does not compile")
    stamp(blueprint, digest(spec))
    log("built " + blueprint.get_name())


def ensure_frame(glass):
    spec = WIDGETS["W_VisorFrame"]
    blueprint = widget_blueprint("W_VisorFrame", unreal.HollowVisorFrameWidget, spec)
    if blueprint is None:
        return
    add(blueprint, unreal.Overlay, "Root")
    image = add(blueprint, unreal.Image, "Glass", "Root")
    brush = image.get_editor_property("brush")
    brush.set_editor_property("resource_object", glass)   # the material itself: the widget makes its dynamic instance
    brush.set_editor_property("draw_as", unreal.SlateBrushDrawType.IMAGE)
    image.set_editor_property("brush", brush)
    box_slot(image)
    finish(blueprint, spec)


def ensure_readout():
    spec = WIDGETS["W_ResonanceReadout"]
    blueprint = widget_blueprint("W_ResonanceReadout", unreal.HollowResonanceReadoutWidget, spec)
    if blueprint is None:
        return
    add(blueprint, unreal.VerticalBox, "Root")
    line = add(blueprint, unreal.HorizontalBox, "Line", "Root")
    box_slot(line, H.H_ALIGN_RIGHT, V.V_ALIGN_TOP)
    value = add(blueprint, unreal.TextBlock, "ValueText", "Line")
    telemetry_text(value, spec["text"], unreal.TextJustify.RIGHT)
    box_slot(value, H.H_ALIGN_RIGHT, V.V_ALIGN_CENTER)
    tier = add(blueprint, unreal.TextBlock, "TierText", "Line")
    telemetry_text(tier, spec["text"], unreal.TextJustify.RIGHT)
    box_slot(tier, H.H_ALIGN_RIGHT, V.V_ALIGN_CENTER, unreal.Margin(spec["gap"] * 3.0, 0.0, 0.0, 0.0))
    box, _ = thin_bar(blueprint, "Bar", "Root", spec["bar"])
    box_slot(box, H.H_ALIGN_RIGHT, V.V_ALIGN_TOP, unreal.Margin(0.0, spec["gap"], 0.0, 0.0))
    finish(blueprint, spec)


def ensure_voice():
    spec = WIDGETS["W_VoiceIndicator"]
    blueprint = widget_blueprint("W_VoiceIndicator", unreal.HollowVoiceIndicatorWidget, spec)
    if blueprint is None:
        return
    add(blueprint, unreal.VerticalBox, "Root")
    mode = add(blueprint, unreal.TextBlock, "ModeText", "Root")
    telemetry_text(mode, spec["text"])
    box_slot(mode, H.H_ALIGN_LEFT, V.V_ALIGN_TOP)
    box, _ = thin_bar(blueprint, "LevelBar", "Root", spec["bar"])
    box_slot(box, H.H_ALIGN_LEFT, V.V_ALIGN_TOP, unreal.Margin(0.0, spec["gap"], 0.0, 0.0))
    talking = add(blueprint, unreal.TextBlock, "TalkingText", "Root")
    telemetry_text(talking, spec["small"])
    box_slot(talking, H.H_ALIGN_LEFT, V.V_ALIGN_TOP, unreal.Margin(0.0, spec["gap"], 0.0, 0.0))
    finish(blueprint, spec)


def ensure_lamp():
    spec = WIDGETS["W_LampIndicator"]
    blueprint = widget_blueprint("W_LampIndicator", unreal.HollowLampIndicatorWidget, spec)
    if blueprint is None:
        return
    add(blueprint, unreal.Overlay, "Root")
    lamp = add(blueprint, unreal.TextBlock, "LampText", "Root")
    telemetry_text(lamp, spec["text"])
    box_slot(lamp, H.H_ALIGN_LEFT, V.V_ALIGN_TOP)
    finish(blueprint, spec)


def ensure_supply():
    spec = WIDGETS["W_SupplyReadout"]
    blueprint = widget_blueprint("W_SupplyReadout", unreal.HollowSupplyReadoutWidget, spec)
    if blueprint is None:
        return
    add(blueprint, unreal.Overlay, "Root")
    supply = add(blueprint, unreal.TextBlock, "SupplyText", "Root")
    telemetry_text(supply, spec["text"], unreal.TextJustify.RIGHT)
    box_slot(supply, H.H_ALIGN_RIGHT, V.V_ALIGN_BOTTOM)
    finish(blueprint, spec)


def ensure_weapon_readout():
    spec = WIDGETS["W_WeaponReadout"]
    blueprint = widget_blueprint("W_WeaponReadout", unreal.HollowWeaponReadoutWidget, spec)
    if blueprint is None:
        return
    add(blueprint, unreal.Overlay, "Root")
    readout = add(blueprint, unreal.TextBlock, "ReadoutText", "Root")
    telemetry_text(readout, spec["text"], unreal.TextJustify.RIGHT)
    box_slot(readout, H.H_ALIGN_RIGHT, V.V_ALIGN_BOTTOM)
    finish(blueprint, spec)


def image_size(size):
    vector = unreal.DeprecateSlateVector2D()   # Slate's float vector type
    vector.set_editor_property("x", size)
    vector.set_editor_property("y", size)
    return vector


def marker_image(blueprint, name, parent, instance, size):
    image = add(blueprint, unreal.Image, name, parent)
    brush = image.get_editor_property("brush")
    brush.set_editor_property("resource_object", instance)
    brush.set_editor_property("draw_as", unreal.SlateBrushDrawType.IMAGE)
    brush.set_editor_property("image_size", image_size(size))
    image.set_editor_property("brush", brush)
    box_slot(image, H.H_ALIGN_CENTER, V.V_ALIGN_CENTER)
    return image


def ensure_hit_marker(material):
    spec = WIDGETS["W_HitMarker"]
    built = dict(spec, marker=HIT_MARKER)
    blueprint = widget_blueprint("W_HitMarker", unreal.HollowHitMarkerWidget, built)
    if blueprint is None:
        return
    add(blueprint, unreal.Overlay, "Root")
    image = marker_image(blueprint, "Marker", "Root", material, spec["size"])
    image.set_editor_property("visibility", unreal.SlateVisibility.HIDDEN)   # until the first hit
    finish(blueprint, built)


def ensure_objective():
    spec = WIDGETS["W_VisorObjective"]
    blueprint = widget_blueprint("W_VisorObjective", unreal.HollowObjectiveWidget, spec)
    if blueprint is None:
        return
    add(blueprint, unreal.VerticalBox, "Root")
    title = add(blueprint, unreal.TextBlock, "TitleText", "Root")
    telemetry_text(title, spec["title"])
    box_slot(title, H.H_ALIGN_LEFT, V.V_ALIGN_TOP)
    text = add(blueprint, unreal.TextBlock, "ObjectiveText", "Root")
    telemetry_text(text, spec["text"])
    text.set_editor_property("wrap_text_at", spec["wrap"])
    box_slot(text, H.H_ALIGN_LEFT, V.V_ALIGN_TOP, unreal.Margin(0.0, spec["gap"], 0.0, 0.0))
    finish(blueprint, spec)


def ensure_waypoint(instances):
    spec = WIDGETS["W_VisorWaypoint"]
    blueprint = widget_blueprint("W_VisorWaypoint", unreal.HollowWaypointMarkerWidget, dict(spec, marks=sorted(MARKER_INSTANCES), marker=MARKER))
    if blueprint is None:
        return
    add(blueprint, unreal.VerticalBox, "Root")
    marks = add(blueprint, unreal.Overlay, "Marks", "Root")
    box_slot(marks, H.H_ALIGN_CENTER, V.V_ALIGN_TOP)
    marker_image(blueprint, "Pip", "Marks", instances["MI_UI_WaypointPip"], spec["pip"])
    marker_image(blueprint, "Arrow", "Marks", instances["MI_UI_WaypointArrow"], spec["arrow"])
    label = add(blueprint, unreal.TextBlock, "LabelText", "Root")
    telemetry_text(label, spec["text"], unreal.TextJustify.CENTER)
    box_slot(label, H.H_ALIGN_CENTER, V.V_ALIGN_TOP, unreal.Margin(0.0, spec["gap"], 0.0, 0.0))
    finish(blueprint, dict(spec, marks=sorted(MARKER_INSTANCES), marker=MARKER))


def ensure_waypoint_layer():
    spec = WIDGETS["W_VisorWaypoints"]
    blueprint = widget_blueprint("W_VisorWaypoints", unreal.HollowWaypointLayerWidget, spec)
    if blueprint is None:
        return
    add(blueprint, unreal.CanvasPanel, "MarkerCanvas")
    finish(blueprint, spec)


def ensure_controls():
    spec = WIDGETS["W_ControlsHint"]
    blueprint = widget_blueprint("W_ControlsHint", unreal.HollowControlsHintWidget, spec)
    if blueprint is None:
        return
    add(blueprint, unreal.VerticalBox, "Root")
    title = add(blueprint, unreal.TextBlock, "TitleText", "Root")
    telemetry_text(title, spec["title"])
    box_slot(title, H.H_ALIGN_LEFT, V.V_ALIGN_TOP)
    rows = add(blueprint, unreal.TextBlock, "ListText", "Root")
    telemetry_text(rows, spec["text"])
    box_slot(rows, H.H_ALIGN_LEFT, V.V_ALIGN_TOP, unreal.Margin(0.0, spec["gap"], 0.0, 0.0))
    finish(blueprint, spec)


def ensure_radio_subtitle():
    spec = WIDGETS["W_RadioSubtitle"]
    blueprint = widget_blueprint("W_RadioSubtitle", unreal.HollowRadioSubtitleWidget, spec)
    if blueprint is None:
        return
    add(blueprint, unreal.VerticalBox, "Root")
    speaker = add(blueprint, unreal.TextBlock, "SpeakerText", "Root")
    telemetry_text(speaker, spec["speaker"], unreal.TextJustify.CENTER)
    box_slot(speaker, H.H_ALIGN_CENTER, V.V_ALIGN_TOP)
    line = add(blueprint, unreal.TextBlock, "LineText", "Root")
    line.set_editor_property("text", unreal.Text(""))
    font = line.get_editor_property("font")   # the default UI font (Roboto: every Cyrillic glyph, LOC-01)
    font.set_editor_property("size", spec["text"])
    line.set_editor_property("font", font)
    line.set_editor_property("justification", unreal.TextJustify.CENTER)
    line.set_editor_property("wrap_text_at", spec["wrap"])
    line.set_editor_property("shadow_offset", unreal.Vector2D(*TEXT_SHADOW))
    line.set_editor_property("shadow_color_and_opacity", color(SHADOW_COLOR))
    box_slot(line, H.H_ALIGN_CENTER, V.V_ALIGN_TOP, unreal.Margin(0.0, spec["gap"], 0.0, 0.0))
    finish(blueprint, spec)


def ensure_lore_notice():
    spec = WIDGETS["W_LoreNotice"]
    blueprint = widget_blueprint("W_LoreNotice", unreal.HollowLoreNoticeWidget, spec)
    if blueprint is None:
        return
    add(blueprint, unreal.VerticalBox, "Root")
    title = add(blueprint, unreal.TextBlock, "TitleText", "Root")
    telemetry_text(title, spec["title"])
    box_slot(title, H.H_ALIGN_LEFT, V.V_ALIGN_TOP)
    entry = add(blueprint, unreal.TextBlock, "EntryText", "Root")
    telemetry_text(entry, spec["text"])
    entry.set_editor_property("wrap_text_at", spec["wrap"])
    box_slot(entry, H.H_ALIGN_LEFT, V.V_ALIGN_TOP, unreal.Margin(0.0, spec["gap"], 0.0, 0.0))
    detail = add(blueprint, unreal.TextBlock, "DetailText", "Root")
    telemetry_text(detail, spec["title"])
    box_slot(detail, H.H_ALIGN_LEFT, V.V_ALIGN_TOP, unreal.Margin(0.0, spec["gap"], 0.0, 0.0))
    finish(blueprint, spec)


def configure_bar(bar, set_name, attribute, max_attribute, fill_color, alert_tag=None):
    key, label = BAR_LABELS[bar.get_name()]
    bar.set_editor_property("label", loc.text(loc.VISOR, key, label))
    bar.set_editor_property("attribute", make_attribute(set_name, attribute))
    bar.set_editor_property("max_attribute", make_attribute(set_name, max_attribute))
    bar.set_editor_property("fill_color", color(fill_color))
    bar.set_editor_property("alert_color", color(LAYOUT["colors"]["alert"]))
    if alert_tag:
        bar.set_editor_property("alert_tag", make_tag(alert_tag))


def visor_spec():
    return {"layout": LAYOUT, "widgets": sorted(WIDGETS), "waypoint": "W_VisorWaypoint", "bars": BAR_LABELS}


def ensure_visor():
    spec = visor_spec()
    blueprint = widget_blueprint("W_Visor", unreal.HollowVisorWidget, spec)
    if blueprint is None:
        return
    L = LAYOUT
    edge = L["edge"]
    centre, top_centre = unreal.Vector2D(0.5, 0.5), unreal.Vector2D(0.5, 0.0)
    top_left, top_right = unreal.Vector2D(0.0, 0.0), unreal.Vector2D(1.0, 0.0)
    bottom_left, bottom_right = unreal.Vector2D(0.0, 1.0), unreal.Vector2D(1.0, 1.0)

    def cls(name):
        return load(VISOR_DIR + "/" + name).generated_class()

    # The glass under everything, on its own layer: an overlay draws its slots in order, while a canvas lets Slate batch a
    # material drawn in the same layer as text after the text (the glass covered the telemetry).
    add(blueprint, unreal.Overlay, "Layers")
    box_slot(add(blueprint, cls("W_VisorFrame"), "VisorFrame", "Layers"))
    # UX-01: the waypoints over the glass, under the telemetry; the layer places its markers itself.
    waypoints = add(blueprint, cls("W_VisorWaypoints"), "Waypoints", "Layers")
    waypoints.set_editor_property("marker_class", cls("W_VisorWaypoint"))
    box_slot(waypoints)
    root = add(blueprint, unreal.CanvasPanel, "Root", "Layers")
    box_slot(root)

    crosshair = add(blueprint, unreal.Image, "Crosshair", "Root")
    size = unreal.Vector2D(L["crosshair"], L["crosshair"])
    brush = crosshair.get_editor_property("brush")
    image_size = unreal.DeprecateSlateVector2D()   # Slate's float vector type
    image_size.set_editor_property("x", size.x)
    image_size.set_editor_property("y", size.y)
    brush.set_editor_property("image_size", image_size)
    crosshair.set_editor_property("brush", brush)
    crosshair.set_editor_property("color_and_opacity", color(L["colors"]["crosshair"]))
    canvas_slot(crosshair, centre, centre, unreal.Vector2D(0.0, 0.0), size)
    hit = add(blueprint, cls("W_HitMarker"), "HitMarker", "Root")
    canvas_slot(hit, centre, centre, unreal.Vector2D(0.0, 0.0), unreal.Vector2D(L["hit_marker"], L["hit_marker"]))
    target = add(blueprint, cls("W_TargetStatus"), "TargetStatus", "Root")
    canvas_slot(target, centre, top_centre, unreal.Vector2D(0.0, L["target"][2]), unreal.Vector2D(L["target"][0], L["target"][1]))
    prompt = add(blueprint, cls("W_InteractionPrompt"), "InteractionPrompt", "Root")
    canvas_slot(prompt, centre, top_centre, unreal.Vector2D(0.0, L["prompt_offset"]), auto_size=True)

    # Bottom: the body on the left, the tool on the right.
    bar_class = cls("W_StatBar")
    colors = L["colors"]
    health = add(blueprint, bar_class, "HealthBar", "Root")
    configure_bar(health, "HollowHealthSet", "Health", "MaxHealth", colors["health"])
    canvas_slot(health, bottom_left, bottom_left, unreal.Vector2D(edge, -edge - L["bar_gap"]), unreal.Vector2D(*L["health"]))
    stamina = add(blueprint, bar_class, "StaminaBar", "Root")
    configure_bar(stamina, "HollowSurvivalSet", "Stamina", "MaxStamina", colors["stamina"])
    canvas_slot(stamina, bottom_left, bottom_left, unreal.Vector2D(edge, -edge), unreal.Vector2D(*L["stamina"]))
    fuel = add(blueprint, bar_class, "ToolFuelBar", "Root")
    configure_bar(fuel, "HollowToolSet", "ToolFuel", "MaxToolFuel", colors["fuel"])
    canvas_slot(fuel, bottom_right, bottom_right, unreal.Vector2D(-edge, -edge - L["tool_gap"]), unreal.Vector2D(*L["tool_bar"]))
    heat = add(blueprint, bar_class, "ToolHeatBar", "Root")
    configure_bar(heat, "HollowToolSet", "ToolHeat", "MaxToolHeat", colors["heat"], OVERHEATED_TAG)
    canvas_slot(heat, bottom_right, bottom_right, unreal.Vector2D(-edge, -edge), unreal.Vector2D(*L["tool_bar"]))
    supply = add(blueprint, cls("W_SupplyReadout"), "SupplyReadout", "Root")
    canvas_slot(supply, bottom_right, bottom_right, unreal.Vector2D(-edge, -edge - L["supply_above_tools"]), auto_size=True)
    weapon = add(blueprint, cls("W_WeaponReadout"), "WeaponReadout", "Root")
    canvas_slot(weapon, bottom_right, bottom_right, unreal.Vector2D(-edge, -edge - L["supply_above_tools"] - L["weapon_above_supply"]), auto_size=True)

    # Top: voice and lamp on the left, the raid in the middle, Resonance on the right.
    voice = add(blueprint, cls("W_VoiceIndicator"), "VoiceIndicator", "Root")
    canvas_slot(voice, top_left, top_left, unreal.Vector2D(edge, edge), auto_size=True)
    lamp = add(blueprint, cls("W_LampIndicator"), "LampIndicator", "Root")
    canvas_slot(lamp, top_left, top_left, unreal.Vector2D(edge, edge + L["lamp_below_voice"]), auto_size=True)
    raid = add(blueprint, cls("W_RaidStatus"), "RaidStatus", "Root")
    canvas_slot(raid, top_centre, top_centre, unreal.Vector2D(0.0, edge), auto_size=True)
    readout = add(blueprint, cls("W_ResonanceReadout"), "ResonanceReadout", "Root")
    canvas_slot(readout, top_right, top_right, unreal.Vector2D(-edge, edge), auto_size=True)

    # UX-01: what to do now under the lamp, the controls under Resonance.
    objective = add(blueprint, cls("W_VisorObjective"), "Objective", "Root")
    canvas_slot(objective, top_left, top_left, unreal.Vector2D(edge, edge + L["objective_below_voice"]), auto_size=True)
    controls = add(blueprint, cls("W_ControlsHint"), "ControlsHint", "Root")
    canvas_slot(controls, top_right, top_right, unreal.Vector2D(-edge, edge + L["controls_below_readout"]), auto_size=True)

    # NAR-01b: a downloaded crew log's notice under the objective.
    notice = add(blueprint, cls("W_LoreNotice"), "LoreNotice", "Root")
    canvas_slot(notice, top_left, top_left, unreal.Vector2D(edge, edge + L["objective_below_voice"] + L["notice_below_objective"]), auto_size=True)

    # NAR-01a: the radio's subtitle low in the middle, its bottom over the bars' line.
    subtitle = add(blueprint, cls("W_RadioSubtitle"), "RadioSubtitle", "Root")
    bottom_centre = unreal.Vector2D(0.5, 1.0)
    canvas_slot(subtitle, bottom_centre, bottom_centre, unreal.Vector2D(0.0, -edge - L["subtitle_above_bottom"]), auto_size=True)
    finish(blueprint, spec)


# ---------------------------------------------------------------- validation

def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate():
    glass = load(GLASS_PATH)
    check(glass.get_editor_property("material_domain") == unreal.MaterialDomain.MD_UI, "M_UI_VisorGlass is a UI material")
    check(up_to_date(glass, digest(GLASS)), "M_UI_VisorGlass matches its spec")
    names = [str(n) for n in mel.get_scalar_parameter_names(glass)]
    check(all(p in names for p in GLASS_PARAMETERS), "M_UI_VisorGlass parameters (%s)" % ", ".join(names))
    check(GLASS_COLOR_PARAMETER in [str(n) for n in mel.get_vector_parameter_names(glass)], "M_UI_VisorGlass colour parameter")
    style = load(STYLE_PATH)
    tiers = style.get_editor_property("tiers")
    check(len(tiers) == 4, "DA_VisorStyle has a look per tier")
    check(all(0.0 <= t.get_editor_property("pulse_hz") <= 3.0 for t in tiers), "no visor pulse over 3 Hz")
    for path, parent in (("W_VisorFrame", unreal.HollowVisorFrameWidget), ("W_ResonanceReadout", unreal.HollowResonanceReadoutWidget),
                         ("W_VoiceIndicator", unreal.HollowVoiceIndicatorWidget), ("W_LampIndicator", unreal.HollowLampIndicatorWidget),
                         ("W_SupplyReadout", unreal.HollowSupplyReadoutWidget), ("W_VisorObjective", unreal.HollowObjectiveWidget),
                         ("W_WeaponReadout", unreal.HollowWeaponReadoutWidget), ("W_HitMarker", unreal.HollowHitMarkerWidget),
                         ("W_VisorWaypoint", unreal.HollowWaypointMarkerWidget), ("W_VisorWaypoints", unreal.HollowWaypointLayerWidget),
                         ("W_ControlsHint", unreal.HollowControlsHintWidget), ("W_RadioSubtitle", unreal.HollowRadioSubtitleWidget),
                         ("W_LoreNotice", unreal.HollowLoreNoticeWidget),
                         ("W_Visor", unreal.HollowVisorWidget)):
        blueprint = load(VISOR_DIR + "/" + path)
        generated = blueprint.generated_class()
        check(generated is not None and unreal.MathLibrary.class_is_child_of(generated, parent.static_class()), path + " derives from " + parent.__name__)
        spec = visor_spec() if path == "W_Visor" else WIDGETS[path]
        if path == "W_VisorWaypoint":
            spec = dict(spec, marks=sorted(MARKER_INSTANCES), marker=MARKER)
        if path == "W_HitMarker":
            spec = dict(spec, marker=HIT_MARKER)
        check(up_to_date(blueprint, digest(spec)), path + " matches its spec")
    visor = load(VISOR_DIR + "/W_Visor")
    for name in ("Layers", "VisorFrame", "Root", "Crosshair", "TargetStatus", "InteractionPrompt", "HealthBar", "StaminaBar", "ToolFuelBar", "ToolHeatBar",
                 "VoiceIndicator", "LampIndicator", "SupplyReadout", "WeaponReadout", "RaidStatus", "ResonanceReadout", "Waypoints", "Objective",
                 "ControlsHint", "HitMarker", "RadioSubtitle", "LoreNotice"):
        check(author.find_widget(visor, name) is not None, "W_Visor shows " + name)
    hit_marker = load(HIT_MARKER_PATH)
    check(hit_marker.get_editor_property("material_domain") == unreal.MaterialDomain.MD_UI and up_to_date(hit_marker, digest(HIT_MARKER)), "M_UI_HitMarker")
    check(GLASS_DAMAGE_COLOR_PARAMETER in [str(n) for n in mel.get_vector_parameter_names(glass)], "M_UI_VisorGlass damage colour parameter")
    check(author.find_widget(visor, "Waypoints").get_editor_property("marker_class") == load(VISOR_DIR + "/W_VisorWaypoint").generated_class(),
          "the waypoint layer places W_VisorWaypoint")
    # UX-01: every kind of place has its look; the marks; the cheat sheet's action, keys and rows.
    looks = style.get_editor_property("waypoints")
    check(len(looks) == len(WAYPOINT_LOOKS) and len(set(str(l.get_editor_property("kind")) for l in looks)) == len(WAYPOINT_LOOKS), "a look per waypoint kind")
    check(1.0 <= style.get_editor_property("waypoint_rate_hz") <= 30.0, "waypoints follow the view at 1..30 Hz")
    marker = load(MARKER_PATH)
    check(marker.get_editor_property("material_domain") == unreal.MaterialDomain.MD_UI and up_to_date(marker, digest(MARKER)), "M_UI_WaypointMarker")
    check("Shape" in [str(n) for n in mel.get_scalar_parameter_names(marker)], "M_UI_WaypointMarker has Shape")
    for name, shape in MARKER_INSTANCES.items():
        instance = load(VISOR_DIR + "/" + name)
        check(instance.get_editor_property("parent") == marker and abs(mel.get_material_instance_scalar_parameter_value(instance, "Shape") - shape) < 1e-4,
              name + " is the marker with Shape %.0f" % shape)
    action = load(INPUT_DIR + "/" + CONTROLS_ACTION)
    check(not action.get_editor_property("consume_input"), CONTROLS_ACTION + " never consumes its keys")
    mappings = load(INPUT_DIR + "/IMC_UI").get_editor_property("default_key_mappings").get_editor_property("mappings")
    known = set((m.get_editor_property("action").get_path_name(), m.get_editor_property("key").export_text()) for m in mappings if m.get_editor_property("action"))
    for key_name in CONTROLS_KEYS:
        check((action.get_path_name(), make_key(key_name).export_text()) in known, "IMC_UI maps %s to %s" % (key_name, CONTROLS_ACTION))
    rows = load(CONTROLS_PATH)
    check(up_to_date(rows, digest(CONTROLS_ROWS)) and len(rows.get_editor_property("rows")) == len(CONTROLS_ROWS), "DA_ControlsHint matches its spec")
    with open(unreal.Paths.project_dir() + INI) as ini:
        text = ini.read()
    for line in INI_LINES:
        check(line in text, "DefaultGame.ini: " + line)
    log("validation passed")


def main():
    glass = ensure_glass()
    ensure_style()
    instances = ensure_marker_material()
    hit_material = ensure_hit_marker_material()
    ensure_controls_input()
    ensure_controls_rows()
    ensure_frame(glass)
    ensure_readout()
    ensure_voice()
    ensure_lamp()
    ensure_supply()
    ensure_weapon_readout()
    ensure_hit_marker(hit_material)
    ensure_objective()
    ensure_waypoint(instances)
    ensure_waypoint_layer()
    ensure_controls()
    ensure_radio_subtitle()
    ensure_lore_notice()
    ensure_visor()
    loc.save()
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_visor_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
