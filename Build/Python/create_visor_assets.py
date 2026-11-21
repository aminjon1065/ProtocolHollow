"""Creates the S4-08 diegetic visor (idempotent, ADR-0010, ADR-0013, ADR-0026).

  /Game/Hollow/UI/Visor/M_UI_VisorGlass       UI material: the helmet's glass (vignette, rim, scan lines) and its alert
                                              glow, pulse and interference (parameters set by UHollowVisorFrameWidget)
  /Game/Hollow/UI/Visor/DA_VisorStyle         UHollowVisorStyle: Resonance tiers, telemetry texts and colours
  /Game/Hollow/UI/Visor/W_VisorFrame          the glass over the whole view
  /Game/Hollow/UI/Visor/W_ResonanceReadout    "RES 63%  HIGH" and its bar
  /Game/Hollow/UI/Visor/W_VoiceIndicator      microphone mode, transmission level, who is talking
  /Game/Hollow/UI/Visor/W_LampIndicator       "[F] LAMP ON"
  /Game/Hollow/UI/Visor/W_SupplyReadout       "PULSE MINE 2   FUEL CELL 3": the consumables of the raid inventory (S5-01)
  /Game/Hollow/UI/Visor/W_Visor               the visor: glass, crosshair, target, prompt and the telemetry at the edges
                                              (health and stamina, tool fuel and heat, supplies, voice and lamp, Resonance, raid)

Nothing heavy: one node-only material and small widget trees. Every asset keeps a hash of its spec (metadata
"HollowSpec") and is rebuilt only when its spec changes - every save is a new LFS object.
Run after create_ui_assets.py and create_lamp_assets.py:  Build/Scripts/run-editor-python.sh Build/Python/create_visor_assets.py
"""
import hashlib
import json
import traceback

import unreal

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
             "HelmetLampAction=/Game/Hollow/Core/Input/Actions/IA_HelmetLamp.IA_HelmetLamp"]

# The glass (data). r is the distance from the centre in half-screens: 0 centre, 1 the middle of an edge, 1.41 a corner.
GLASS = {
    "version": 2,
    "vignette": [0.75, 1.5, 0.4],             # from r, to r, darkest opacity
    "rim": [1.32, 0.14, 0.09],                # radius, half width, opacity
    "rim_color": [0.75, 0.9, 1.0],
    "scan_lines": 270.0, "scan_depth": 0.02,  # lines over the height, opacity
    "alert": [0.7, 1.4],                      # the glow grows from r to r
    "pulse_depth": 0.5,                       # how far the pulse dims the glow
    "bands": [3.0, 0.08, 0.07, 0.35],         # count over the height, drift per second, width, from r (the centre stays clear)
    "alert_color": [1.0, 0.3, 0.1],           # defaults; DA_VisorStyle sets them per tier
}
GLASS_PARAMETERS = ["Alert", "PulseHz", "Interference"]
GLASS_COLOR_PARAMETER = "AlertColor"

# The look per Resonance tier (S4-08 clarification 7): calm Low and Medium; High amber at 1 Hz; Critical red at 2 Hz with
# interference. Every pulse stays at or under 3 Hz (UHollowVisorStyle::MaxPulseHz, photosensitivity).
TIERS = [
    ("LOW", (0.55, 0.85, 1.0, 1.0), 0.0, 0.0, 0.0),
    ("MEDIUM", (0.85, 0.9, 0.5, 1.0), 0.0, 0.0, 0.0),
    ("HIGH", (1.0, 0.68, 0.25, 1.0), 0.35, 1.0, 0.0),
    ("CRITICAL", (1.0, 0.36, 0.28, 1.0), 0.6, 2.0, 0.12),
]
STYLE_TEXTS = {
    "resonance_format": "RES {0}%",
    "push_to_talk_label": "MIC  PTT", "open_mic_label": "MIC  OPEN", "muted_label": "MIC  MUTED",
    "transmitting_format": "{0}  >> TX", "talking_format": "VOX  {0}",
    "lamp_on_label": "LAMP ON", "lamp_off_label": "LAMP OFF", "lamp_format": "[{0}] {1}",
    "supply_format": "{0} {1}", "supply_separator": "   ",
}
STYLE_COLORS = {
    "telemetry_color": (0.6, 0.88, 1.0, 0.9),
    "transmitting_color": (0.45, 1.0, 0.55, 1.0), "muted_color": (0.65, 0.45, 0.45, 0.85),
    "lamp_on_color": (1.0, 0.95, 0.85, 0.9), "lamp_off_color": (0.5, 0.55, 0.6, 0.75),
}

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
}

# W_Visor layout (data): distances from the screen edges, inside the glass's rim.
LAYOUT = {
    "version": 3,
    "edge": 48.0,
    "crosshair": 6.0,
    "target": [360.0, 48.0, 60.0],             # size, below the crosshair
    "prompt_offset": 120.0,
    "health": [320.0, 26.0], "stamina": [320.0, 20.0], "bar_gap": 32.0,
    "tool_bar": [300.0, 24.0], "tool_gap": 36.0,
    "supply_above_tools": 72.0,
    "lamp_below_voice": 64.0,
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
    mel.delete_all_material_expressions(material)
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

    # Darkening plus light: opacity is all of it, the colour only the light (divided back out of the blend).
    opacity = g.single(unreal.MaterialExpressionSaturate, 2, g.math(Add, 3, g.math(Add, 4, vignette, rim), g.math(Add, 4, tint, scan)))
    rim_color = g.node(unreal.MaterialExpressionConstant3Vector, 5, constant=color(m["rim_color"] + [1.0]))
    light = g.math(Add, 3, g.math(Mul, 4, rim_color, rim), g.math(Mul, 4, alert_color, tint))
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
    spec_hash = digest(TIERS, STYLE_TEXTS, STYLE_COLORS)
    if not created and up_to_date(style, spec_hash):
        return style
    looks = []
    for label, rgba, alert, pulse_hz, interference in TIERS:
        look = unreal.HollowVisorTierLook()
        look.set_editor_property("label", unreal.Text(label))
        look.set_editor_property("color", color(rgba))
        look.set_editor_property("alert", alert)
        look.set_editor_property("pulse_hz", pulse_hz)
        look.set_editor_property("interference", interference)
        looks.append(look)
    style.set_editor_property("tiers", looks)
    for name, value in STYLE_TEXTS.items():
        style.set_editor_property(name, unreal.Text(value))
    for name, rgba in STYLE_COLORS.items():
        style.set_editor_property(name, color(rgba))
    stamp(style, spec_hash)
    log("DA_VisorStyle ready")
    return style


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


def configure_bar(bar, label, set_name, attribute, max_attribute, fill_color, alert_tag=None):
    bar.set_editor_property("label", unreal.Text(label))
    bar.set_editor_property("attribute", make_attribute(set_name, attribute))
    bar.set_editor_property("max_attribute", make_attribute(set_name, max_attribute))
    bar.set_editor_property("fill_color", color(fill_color))
    bar.set_editor_property("alert_color", color(LAYOUT["colors"]["alert"]))
    if alert_tag:
        bar.set_editor_property("alert_tag", make_tag(alert_tag))


def visor_spec():
    return {"layout": LAYOUT, "widgets": sorted(WIDGETS)}


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
    target = add(blueprint, cls("W_TargetStatus"), "TargetStatus", "Root")
    canvas_slot(target, centre, top_centre, unreal.Vector2D(0.0, L["target"][2]), unreal.Vector2D(L["target"][0], L["target"][1]))
    prompt = add(blueprint, cls("W_InteractionPrompt"), "InteractionPrompt", "Root")
    canvas_slot(prompt, centre, top_centre, unreal.Vector2D(0.0, L["prompt_offset"]), auto_size=True)

    # Bottom: the body on the left, the tool on the right.
    bar_class = cls("W_StatBar")
    colors = L["colors"]
    health = add(blueprint, bar_class, "HealthBar", "Root")
    configure_bar(health, "Health", "HollowHealthSet", "Health", "MaxHealth", colors["health"])
    canvas_slot(health, bottom_left, bottom_left, unreal.Vector2D(edge, -edge - L["bar_gap"]), unreal.Vector2D(*L["health"]))
    stamina = add(blueprint, bar_class, "StaminaBar", "Root")
    configure_bar(stamina, "Stamina", "HollowSurvivalSet", "Stamina", "MaxStamina", colors["stamina"])
    canvas_slot(stamina, bottom_left, bottom_left, unreal.Vector2D(edge, -edge), unreal.Vector2D(*L["stamina"]))
    fuel = add(blueprint, bar_class, "ToolFuelBar", "Root")
    configure_bar(fuel, "Fuel", "HollowToolSet", "ToolFuel", "MaxToolFuel", colors["fuel"])
    canvas_slot(fuel, bottom_right, bottom_right, unreal.Vector2D(-edge, -edge - L["tool_gap"]), unreal.Vector2D(*L["tool_bar"]))
    heat = add(blueprint, bar_class, "ToolHeatBar", "Root")
    configure_bar(heat, "Heat", "HollowToolSet", "ToolHeat", "MaxToolHeat", colors["heat"], OVERHEATED_TAG)
    canvas_slot(heat, bottom_right, bottom_right, unreal.Vector2D(-edge, -edge), unreal.Vector2D(*L["tool_bar"]))
    supply = add(blueprint, cls("W_SupplyReadout"), "SupplyReadout", "Root")
    canvas_slot(supply, bottom_right, bottom_right, unreal.Vector2D(-edge, -edge - L["supply_above_tools"]), auto_size=True)

    # Top: voice and lamp on the left, the raid in the middle, Resonance on the right.
    voice = add(blueprint, cls("W_VoiceIndicator"), "VoiceIndicator", "Root")
    canvas_slot(voice, top_left, top_left, unreal.Vector2D(edge, edge), auto_size=True)
    lamp = add(blueprint, cls("W_LampIndicator"), "LampIndicator", "Root")
    canvas_slot(lamp, top_left, top_left, unreal.Vector2D(edge, edge + L["lamp_below_voice"]), auto_size=True)
    raid = add(blueprint, cls("W_RaidStatus"), "RaidStatus", "Root")
    canvas_slot(raid, top_centre, top_centre, unreal.Vector2D(0.0, edge), auto_size=True)
    readout = add(blueprint, cls("W_ResonanceReadout"), "ResonanceReadout", "Root")
    canvas_slot(readout, top_right, top_right, unreal.Vector2D(-edge, edge), auto_size=True)
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
                         ("W_SupplyReadout", unreal.HollowSupplyReadoutWidget), ("W_Visor", unreal.HollowVisorWidget)):
        blueprint = load(VISOR_DIR + "/" + path)
        generated = blueprint.generated_class()
        check(generated is not None and unreal.MathLibrary.class_is_child_of(generated, parent.static_class()), path + " derives from " + parent.__name__)
        spec = visor_spec() if path == "W_Visor" else WIDGETS[path]
        check(up_to_date(blueprint, digest(spec)), path + " matches its spec")
    visor = load(VISOR_DIR + "/W_Visor")
    for name in ("Layers", "VisorFrame", "Root", "Crosshair", "TargetStatus", "InteractionPrompt", "HealthBar", "StaminaBar", "ToolFuelBar", "ToolHeatBar",
                 "VoiceIndicator", "LampIndicator", "SupplyReadout", "RaidStatus", "ResonanceReadout"):
        check(author.find_widget(visor, name) is not None, "W_Visor shows " + name)
    with open(unreal.Paths.project_dir() + INI) as ini:
        text = ini.read()
    for line in INI_LINES:
        check(line in text, "DefaultGame.ini: " + line)
    log("validation passed")


def main():
    glass = ensure_glass()
    ensure_style()
    ensure_frame(glass)
    ensure_readout()
    ensure_voice()
    ensure_lamp()
    ensure_supply()
    ensure_visor()
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_visor_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
