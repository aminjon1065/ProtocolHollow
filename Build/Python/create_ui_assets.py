"""Creates the S2-06 UI content (idempotent, ADR-0010, ADR-0013).

  /Game/Hollow/UI/Input/IA_SystemMenu|IA_UI_Confirm|IA_UI_Back, IMC_UI   UI actions (keyboard + gamepad)
  /Game/Hollow/UI/Input/BP_CommonInputData                             CommonUI click/back actions (Enhanced Input)
  /Game/Hollow/UI/Input/DA_InputGlyphs                                 key labels for prompts
  /Game/Hollow/UI/Foundation/BP_ButtonStyle_Menu, W_MenuButton, W_PrimaryLayout
  /Game/Hollow/UI/Visor/W_StatBar, W_InteractionPrompt, W_TargetStatus, W_Visor
  /Game/Hollow/UI/Menus/W_SystemMenu
  /Game/Hollow/UI/Visor/W_RaidStatus (added to W_Visor), /Game/Hollow/UI/Menus/W_Debrief      (S3-03)
  DA_Tool_Welder/PlasmaCutter ActionLabel, BP_GameMode_Hub/Raid HUDClass = AHollowHUD   (if unset)

Widget trees are composed through UHollowWidgetAuthoring (HollowEditor): editor Python cannot reach a WBP's tree.
Creates only what is missing (an existing widget is kept as is), then validates.
Run after create_tool_assets.py and create_interaction_assets.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_ui_assets.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

UI_DIR = "/Game/Hollow/UI"
INPUT_DIR = UI_DIR + "/Input"
FOUNDATION_DIR = UI_DIR + "/Foundation"
VISOR_DIR = UI_DIR + "/Visor"
MENUS_DIR = UI_DIR + "/Menus"
FRAMEWORK_DIR = "/Game/Hollow/Core/Framework"
GAME_MODES = [FRAMEWORK_DIR + "/BP_GameMode_Hub", FRAMEWORK_DIR + "/BP_GameMode_Raid"]
TOOL_LABELS = {
    "/Game/Hollow/Tools/Welder/DA_Tool_Welder": "Welding",
    "/Game/Hollow/Tools/PlasmaCutter/DA_Tool_PlasmaCutter": "Cutting",
}

# UI actions and their keys (keyboard first, then gamepad). Esc / Start open the system menu, Esc / B go back.
UI_ACTIONS = {
    "IA_SystemMenu": ["Escape", "Gamepad_Special_Right"],
    "IA_UI_Confirm": ["Enter", "Gamepad_FaceButton_Bottom"],
    "IA_UI_Back": ["Escape", "Gamepad_FaceButton_Right"],
}

# Prompt labels (Xbox naming until there is icon art; ADR-0013).
GLYPHS = [
    ("E", "E"), ("F", "F"), ("SpaceBar", "Space"), ("Enter", "Enter"), ("Escape", "Esc"), ("LeftShift", "Shift"),
    ("LeftControl", "Ctrl"), ("LeftMouseButton", "LMB"), ("RightMouseButton", "RMB"),
    ("Gamepad_FaceButton_Bottom", "A"), ("Gamepad_FaceButton_Right", "B"), ("Gamepad_FaceButton_Left", "X"),
    ("Gamepad_FaceButton_Top", "Y"), ("Gamepad_LeftTrigger", "LT"), ("Gamepad_RightTrigger", "RT"),
    ("Gamepad_LeftShoulder", "LB"), ("Gamepad_RightShoulder", "RB"), ("Gamepad_LeftThumbstick", "LS"),
    ("Gamepad_RightThumbstick", "RS"), ("Gamepad_Special_Right", "Menu"), ("Gamepad_Special_Left", "View"),
]

# Greybox look (data).
LAYER_TRANSITION_SECONDS = 0.0                # instant screen switches; a fade is a number here, no code
TEXT_SIZE = 18
TITLE_SIZE = 32
BUTTON_TEXT_SIZE = 22
PANEL_PADDING = 8.0
EDGE = 40.0                                   # distance of the bars from the screen edges
HEALTH_SIZE = unreal.Vector2D(360.0, 28.0)
TOOL_BAR_SIZE = unreal.Vector2D(300.0, 24.0)
TOOL_BAR_GAP = 36.0                           # fuel sits this far above heat
TARGET_SIZE = unreal.Vector2D(360.0, 48.0)
TARGET_OFFSET = 60.0                          # below the crosshair
PROMPT_OFFSET = 120.0
RAID_STATUS_OFFSET = 40.0                     # below the top edge
DEBRIEF_WIDTH = 720.0
CROSSHAIR_SIZE = 6.0
MARKER_SIZE = unreal.Vector2D(3.0, 20.0)
BUTTON_MIN_WIDTH = 280
BUTTON_PADDING = unreal.Margin(24.0, 10.0, 24.0, 10.0)
WHITE = unreal.LinearColor(1.0, 1.0, 1.0, 1.0)
HEALTH_COLOR = unreal.LinearColor(0.75, 0.15, 0.15, 1.0)
FUEL_COLOR = unreal.LinearColor(0.2, 0.6, 1.0, 1.0)
HEAT_COLOR = unreal.LinearColor(1.0, 0.55, 0.1, 1.0)
ALERT_COLOR = unreal.LinearColor(1.0, 0.1, 0.05, 1.0)
WELD_COLOR = unreal.LinearColor(1.0, 0.45, 0.1, 1.0)
MARKER_COLOR = unreal.LinearColor(1.0, 1.0, 1.0, 0.9)
BACKDROP_COLOR = unreal.LinearColor(0.0, 0.0, 0.0, 0.6)
BUTTON_COLORS = {"normal_base": (0.12, 0.13, 0.15, 0.9), "normal_hovered": (0.3, 0.32, 0.36, 1.0),
                 "normal_pressed": (0.5, 0.35, 0.15, 1.0), "disabled": (0.1, 0.1, 0.1, 0.5)}

ATTRIBUTE_MODULE = "/Script/HollowGAS"
OVERHEATED_TAG = "Hollow.State.Tool.Overheated"

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
author = unreal.HollowWidgetAuthoring
H = unreal.HorizontalAlignment
V = unreal.VerticalAlignment
NO_PADDING = unreal.Margin(0.0, 0.0, 0.0, 0.0)


def log(message):
    unreal.log("create_ui_assets: " + message)


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


def make_key(name):
    key = unreal.Key()
    if not key.import_text(name):
        raise RuntimeError("Unknown key " + name)
    return key


def make_tag(name):
    tag = unreal.GameplayTag()
    if not tag.import_text('(TagName="%s")' % name):
        raise RuntimeError("Unknown gameplay tag " + name)
    return tag


def make_attribute(set_name, attribute):
    attr = unreal.GameplayAttribute()
    path = "%s.%s:%s" % (ATTRIBUTE_MODULE, set_name, attribute)
    text = '(AttributeName="%s",Attribute=%s,AttributeOwner="/Script/CoreUObject.Class\'%s.%s\'")' % (attribute, path, ATTRIBUTE_MODULE, set_name)
    if not attr.import_text(text) or path not in attr.export_text():
        raise RuntimeError("Could not build attribute " + path)
    return attr


def color(rgba):
    return unreal.LinearColor(*rgba)


# ---------------------------------------------------------------- input


def ensure_actions():
    for name, keys in UI_ACTIONS.items():
        path = INPUT_DIR + "/" + name
        if assets.does_asset_exist(path):
            continue
        action = create(path, unreal.InputAction, unreal.InputAction_Factory())
        action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        # CommonUI reads the keys of UI actions; they must never swallow gameplay keys mapped elsewhere (B = crouch).
        action.set_editor_property("consume_input", False)
        save(action)
        log("created " + name)


def ensure_mapping_context():
    path = INPUT_DIR + "/IMC_UI"
    if assets.does_asset_exist(path):
        return
    context = create(path, unreal.InputMappingContext, unreal.InputMappingContext_Factory())
    data = context.get_editor_property("default_key_mappings")
    mappings = []
    for name, keys in UI_ACTIONS.items():
        for key_name in keys:
            entry = unreal.EnhancedActionKeyMapping()
            entry.set_editor_property("action", load(INPUT_DIR + "/" + name))
            entry.set_editor_property("key", make_key(key_name))
            mappings.append(entry)
    data.set_editor_property("mappings", mappings)
    context.set_editor_property("default_key_mappings", data)
    save(context)
    log("created IMC_UI")


def ensure_common_input_data():
    path = INPUT_DIR + "/BP_CommonInputData"
    if assets.does_asset_exist(path):
        return
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.CommonUIInputData)
    blueprint = create(path, unreal.Blueprint, factory)
    data = unreal.get_default_object(blueprint.generated_class())
    data.set_editor_property("enhanced_input_click_action", load(INPUT_DIR + "/IA_UI_Confirm"))
    data.set_editor_property("enhanced_input_back_action", load(INPUT_DIR + "/IA_UI_Back"))
    save(blueprint)
    log("created BP_CommonInputData")


def ensure_glyphs():
    path = INPUT_DIR + "/DA_InputGlyphs"
    if assets.does_asset_exist(path):
        return
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.HollowInputGlyphSet)
    glyph_set = create(path, unreal.HollowInputGlyphSet, factory)
    glyphs = []
    for key_name, label in GLYPHS:
        glyph = unreal.HollowKeyGlyph()
        glyph.set_editor_property("key", make_key(key_name))
        glyph.set_editor_property("label", unreal.Text(label))
        glyphs.append(glyph)
    glyph_set.set_editor_property("glyphs", glyphs)
    save(glyph_set)
    log("created DA_InputGlyphs")


# ---------------------------------------------------------------- widgets


def new_widget_blueprint(path, parent_class):
    """A new WBP with an empty tree, or None if it already exists (kept as is)."""
    if assets.does_asset_exist(path):
        return None
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


def text(widget, value, size=TEXT_SIZE, justification=unreal.TextJustify.LEFT):
    widget.set_editor_property("text", unreal.Text(value))
    font = widget.get_editor_property("font")
    font.set_editor_property("size", size)
    widget.set_editor_property("font", font)
    widget.set_editor_property("justification", justification)


def image(widget, size, tint):
    brush = widget.get_editor_property("brush")
    image_size = unreal.DeprecateSlateVector2D()   # Slate's float vector type
    image_size.set_editor_property("x", size.x)
    image_size.set_editor_property("y", size.y)
    brush.set_editor_property("image_size", image_size)
    widget.set_editor_property("brush", brush)
    widget.set_editor_property("color_and_opacity", tint)


def finish(blueprint):
    if not author.compile(blueprint):
        raise RuntimeError(blueprint.get_name() + " does not compile")
    save(blueprint)
    log("created " + blueprint.get_name())


def ensure_button_style():
    path = FOUNDATION_DIR + "/BP_ButtonStyle_Menu"
    if assets.does_asset_exist(path):
        return
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.CommonButtonStyle)
    blueprint = create(path, unreal.Blueprint, factory)
    style = unreal.get_default_object(blueprint.generated_class())
    for prop, rgba in BUTTON_COLORS.items():
        brush = unreal.SlateBrush()
        brush.set_editor_property("tint_color", unreal.SlateColor(color(rgba)))
        style.set_editor_property(prop, brush)
    style.set_editor_property("button_padding", BUTTON_PADDING)
    save(blueprint)
    log("created BP_ButtonStyle_Menu")


def ensure_menu_button():
    blueprint = new_widget_blueprint(FOUNDATION_DIR + "/W_MenuButton", unreal.HollowButton)
    if blueprint is None:
        return
    label = add(blueprint, unreal.TextBlock, "LabelText")
    text(label, "", BUTTON_TEXT_SIZE, unreal.TextJustify.CENTER)
    finish(blueprint)
    button = unreal.get_default_object(blueprint.generated_class())
    button.set_editor_property("style", load(FOUNDATION_DIR + "/BP_ButtonStyle_Menu").generated_class())
    button.set_editor_property("min_width", BUTTON_MIN_WIDTH)
    save(blueprint)


def ensure_primary_layout():
    blueprint = new_widget_blueprint(FOUNDATION_DIR + "/W_PrimaryLayout", unreal.HollowPrimaryLayout)
    if blueprint is None:
        return
    add(blueprint, unreal.Overlay, "Root")
    for name in ("GameLayer", "GameMenuLayer", "MenuLayer", "ModalLayer"):   # later = drawn above
        stack = add(blueprint, unreal.CommonActivatableWidgetStack, name, "Root")
        stack.set_editor_property("transition_duration", LAYER_TRANSITION_SECONDS)
        box_slot(stack)
    finish(blueprint)


def ensure_stat_bar():
    blueprint = new_widget_blueprint(VISOR_DIR + "/W_StatBar", unreal.HollowStatBarWidget)
    if blueprint is None:
        return
    add(blueprint, unreal.Overlay, "Root")
    box_slot(add(blueprint, unreal.ProgressBar, "Bar", "Root"))
    label = add(blueprint, unreal.TextBlock, "LabelText", "Root")
    text(label, "")
    box_slot(label, H.H_ALIGN_LEFT, V.V_ALIGN_CENTER, unreal.Margin(PANEL_PADDING, 0.0, PANEL_PADDING, 0.0))
    value = add(blueprint, unreal.TextBlock, "ValueText", "Root")
    text(value, "")
    box_slot(value, H.H_ALIGN_RIGHT, V.V_ALIGN_CENTER, unreal.Margin(PANEL_PADDING, 0.0, PANEL_PADDING, 0.0))
    finish(blueprint)


def ensure_prompt():
    blueprint = new_widget_blueprint(VISOR_DIR + "/W_InteractionPrompt", unreal.HollowInteractionPromptWidget)
    if blueprint is None:
        return
    add(blueprint, unreal.HorizontalBox, "Root")
    key = add(blueprint, unreal.TextBlock, "KeyText", "Root")
    text(key, "")
    box_slot(key, H.H_ALIGN_LEFT, V.V_ALIGN_CENTER, unreal.Margin(0.0, 0.0, PANEL_PADDING, 0.0))
    prompt = add(blueprint, unreal.TextBlock, "PromptText", "Root")
    text(prompt, "")
    box_slot(prompt, H.H_ALIGN_LEFT, V.V_ALIGN_CENTER)
    finish(blueprint)


def ensure_target_status():
    blueprint = new_widget_blueprint(VISOR_DIR + "/W_TargetStatus", unreal.HollowTargetStatusWidget)
    if blueprint is None:
        return
    add(blueprint, unreal.VerticalBox, "Root")
    label = add(blueprint, unreal.TextBlock, "LabelText", "Root")
    text(label, "", TEXT_SIZE, unreal.TextJustify.CENTER)
    box_slot(label, H.H_ALIGN_CENTER, V.V_ALIGN_TOP)
    area = add(blueprint, unreal.Overlay, "BarArea", "Root")
    box_slot(area, fill=1.0)
    bar = add(blueprint, unreal.ProgressBar, "Bar", "BarArea")
    bar.set_editor_property("fill_color_and_opacity", WELD_COLOR)
    box_slot(bar)
    box_slot(add(blueprint, unreal.HorizontalBox, "MarkerRow", "BarArea"))
    box_slot(add(blueprint, unreal.Spacer, "MarkerBefore", "MarkerRow"), fill=1.0)
    marker = add(blueprint, unreal.Image, "Marker", "MarkerRow")
    image(marker, MARKER_SIZE, MARKER_COLOR)
    box_slot(marker)
    box_slot(add(blueprint, unreal.Spacer, "MarkerAfter", "MarkerRow"), fill=1.0)
    finish(blueprint)


def configure_bar(bar, label, set_name, attribute, max_attribute, fill_color, alert_tag=None):
    bar.set_editor_property("label", unreal.Text(label))
    bar.set_editor_property("attribute", make_attribute(set_name, attribute))
    bar.set_editor_property("max_attribute", make_attribute(set_name, max_attribute))
    bar.set_editor_property("fill_color", fill_color)
    bar.set_editor_property("alert_color", ALERT_COLOR)
    if alert_tag:
        bar.set_editor_property("alert_tag", make_tag(alert_tag))


def ensure_visor():
    blueprint = new_widget_blueprint(VISOR_DIR + "/W_Visor", unreal.HollowVisorWidget)
    if blueprint is None:
        return
    centre = unreal.Vector2D(0.5, 0.5)
    top_centre = unreal.Vector2D(0.5, 0.0)
    bottom_left = unreal.Vector2D(0.0, 1.0)
    bottom_right = unreal.Vector2D(1.0, 1.0)
    add(blueprint, unreal.CanvasPanel, "Root")

    crosshair = add(blueprint, unreal.Image, "Crosshair", "Root")
    image(crosshair, unreal.Vector2D(CROSSHAIR_SIZE, CROSSHAIR_SIZE), WHITE)
    canvas_slot(crosshair, centre, centre, unreal.Vector2D(0.0, 0.0), unreal.Vector2D(CROSSHAIR_SIZE, CROSSHAIR_SIZE))

    target = add(blueprint, load(VISOR_DIR + "/W_TargetStatus").generated_class(), "TargetStatus", "Root")
    canvas_slot(target, centre, top_centre, unreal.Vector2D(0.0, TARGET_OFFSET), TARGET_SIZE)
    prompt = add(blueprint, load(VISOR_DIR + "/W_InteractionPrompt").generated_class(), "InteractionPrompt", "Root")
    canvas_slot(prompt, centre, top_centre, unreal.Vector2D(0.0, PROMPT_OFFSET), auto_size=True)

    bar_class = load(VISOR_DIR + "/W_StatBar").generated_class()
    health = add(blueprint, bar_class, "HealthBar", "Root")
    configure_bar(health, "Health", "HollowHealthSet", "Health", "MaxHealth", HEALTH_COLOR)
    canvas_slot(health, bottom_left, bottom_left, unreal.Vector2D(EDGE, -EDGE), HEALTH_SIZE)
    fuel = add(blueprint, bar_class, "ToolFuelBar", "Root")
    configure_bar(fuel, "Fuel", "HollowToolSet", "ToolFuel", "MaxToolFuel", FUEL_COLOR)
    canvas_slot(fuel, bottom_right, bottom_right, unreal.Vector2D(-EDGE, -EDGE - TOOL_BAR_GAP), TOOL_BAR_SIZE)
    heat = add(blueprint, bar_class, "ToolHeatBar", "Root")
    configure_bar(heat, "Heat", "HollowToolSet", "ToolHeat", "MaxToolHeat", HEAT_COLOR, OVERHEATED_TAG)
    canvas_slot(heat, bottom_right, bottom_right, unreal.Vector2D(-EDGE, -EDGE), TOOL_BAR_SIZE)
    finish(blueprint)


def ensure_system_menu():
    blueprint = new_widget_blueprint(MENUS_DIR + "/W_SystemMenu", unreal.HollowSystemMenuWidget)
    if blueprint is None:
        return
    add(blueprint, unreal.Overlay, "Root")
    backdrop = add(blueprint, unreal.Image, "Backdrop", "Root")
    image(backdrop, unreal.Vector2D(0.0, 0.0), BACKDROP_COLOR)
    box_slot(backdrop)
    panel = add(blueprint, unreal.VerticalBox, "Panel", "Root")
    box_slot(panel, H.H_ALIGN_CENTER, V.V_ALIGN_CENTER)
    title = add(blueprint, unreal.TextBlock, "Title", "Panel")
    text(title, "SYSTEM", TITLE_SIZE, unreal.TextJustify.CENTER)
    box_slot(title, H.H_ALIGN_CENTER, V.V_ALIGN_TOP, unreal.Margin(0.0, 0.0, 0.0, PANEL_PADDING * 3.0))
    button_class = load(FOUNDATION_DIR + "/W_MenuButton").generated_class()
    for name, label in (("ResumeButton", "Resume"), ("QuitButton", "Quit")):
        button = add(blueprint, button_class, name, "Panel")
        button.set_editor_property("label", unreal.Text(label))
        box_slot(button, H.H_ALIGN_FILL, V.V_ALIGN_TOP, unreal.Margin(0.0, PANEL_PADDING, 0.0, PANEL_PADDING))
    finish(blueprint)


def ensure_raid_status():
    blueprint = new_widget_blueprint(VISOR_DIR + "/W_RaidStatus", unreal.HollowRaidStatusWidget)
    if blueprint is None:
        return
    add(blueprint, unreal.VerticalBox, "Root")
    status = add(blueprint, unreal.TextBlock, "StatusText", "Root")
    text(status, "", TEXT_SIZE + 4, unreal.TextJustify.CENTER)
    box_slot(status, H.H_ALIGN_CENTER, V.V_ALIGN_TOP)
    finish(blueprint)


def ensure_visor_raid_status():
    """W_Visor predates the raid line: add it to the existing tree once."""
    blueprint = load(VISOR_DIR + "/W_Visor")
    if author.find_widget(blueprint, "RaidStatus") is not None:
        return
    status = add(blueprint, load(VISOR_DIR + "/W_RaidStatus").generated_class(), "RaidStatus", "Root")
    top_centre = unreal.Vector2D(0.5, 0.0)
    canvas_slot(status, top_centre, top_centre, unreal.Vector2D(0.0, RAID_STATUS_OFFSET), auto_size=True)
    if not author.compile(blueprint):
        raise RuntimeError("W_Visor does not compile with the raid line")
    save(blueprint)
    log("W_Visor: raid status line added")


def ensure_debrief():
    blueprint = new_widget_blueprint(MENUS_DIR + "/W_Debrief", unreal.HollowDebriefWidget)
    if blueprint is None:
        return
    add(blueprint, unreal.Overlay, "Root")
    backdrop = add(blueprint, unreal.Image, "Backdrop", "Root")
    image(backdrop, unreal.Vector2D(0.0, 0.0), BACKDROP_COLOR)
    box_slot(backdrop)
    panel = add(blueprint, unreal.VerticalBox, "Panel", "Root")
    box_slot(panel, H.H_ALIGN_CENTER, V.V_ALIGN_CENTER)
    title = add(blueprint, unreal.TextBlock, "TitleText", "Panel")
    text(title, "", TITLE_SIZE, unreal.TextJustify.CENTER)
    box_slot(title, H.H_ALIGN_CENTER, V.V_ALIGN_TOP, unreal.Margin(0.0, 0.0, 0.0, PANEL_PADDING * 3.0))
    details = add(blueprint, unreal.TextBlock, "DetailsText", "Panel")
    text(details, "", TEXT_SIZE)
    details.set_editor_property("auto_wrap_text", True)
    details.set_editor_property("wrap_text_at", DEBRIEF_WIDTH)
    box_slot(details, H.H_ALIGN_LEFT, V.V_ALIGN_TOP, unreal.Margin(0.0, 0.0, 0.0, PANEL_PADDING * 3.0))
    close = add(blueprint, load(FOUNDATION_DIR + "/W_MenuButton").generated_class(), "CloseButton", "Panel")
    close.set_editor_property("label", unreal.Text("Continue"))
    box_slot(close, H.H_ALIGN_FILL, V.V_ALIGN_TOP, unreal.Margin(0.0, PANEL_PADDING, 0.0, PANEL_PADDING))
    finish(blueprint)


# ---------------------------------------------------------------- game data


def ensure_tool_labels():
    for path, label in TOOL_LABELS.items():
        tool = load(path)
        if not tool.get_editor_property("action_label").is_empty():
            continue
        tool.set_editor_property("action_label", unreal.Text(label))
        save(tool)
        log("%s: action label set" % path.rsplit("/", 1)[1])


def ensure_hud_class():
    hud_class = unreal.HollowHUD.static_class()
    for path in GAME_MODES:
        blueprint = load(path)
        mode = unreal.get_default_object(blueprint.generated_class())
        current = mode.get_editor_property("hud_class")
        if current == hud_class:
            continue
        if current not in (None, unreal.HUD.static_class()):
            log("%s uses a custom HUD; kept as is" % path)
            continue
        mode.set_editor_property("hud_class", hud_class)
        save(blueprint)
        log("%s: HUD class set" % path.rsplit("/", 1)[1])


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate():
    for name in UI_ACTIONS:
        check(assets.does_asset_exist(INPUT_DIR + "/" + name), name)
    check(len(load(INPUT_DIR + "/IMC_UI").get_editor_property("default_key_mappings").get_editor_property("mappings")) ==
          sum(len(keys) for keys in UI_ACTIONS.values()), "IMC_UI maps every UI key")
    data = unreal.get_default_object(load(INPUT_DIR + "/BP_CommonInputData").generated_class())
    check(data.get_editor_property("enhanced_input_back_action") is not None, "CommonUI back action")
    check(len(load(INPUT_DIR + "/DA_InputGlyphs").get_editor_property("glyphs")) == len(GLYPHS), "glyphs")
    for path, parent in ((FOUNDATION_DIR + "/W_PrimaryLayout", unreal.HollowPrimaryLayout),
                         (FOUNDATION_DIR + "/W_MenuButton", unreal.HollowButton),
                         (VISOR_DIR + "/W_StatBar", unreal.HollowStatBarWidget),
                         (VISOR_DIR + "/W_InteractionPrompt", unreal.HollowInteractionPromptWidget),
                         (VISOR_DIR + "/W_TargetStatus", unreal.HollowTargetStatusWidget),
                         (VISOR_DIR + "/W_Visor", unreal.HollowVisorWidget),
                         (MENUS_DIR + "/W_SystemMenu", unreal.HollowSystemMenuWidget),
                         (VISOR_DIR + "/W_RaidStatus", unreal.HollowRaidStatusWidget),
                         (MENUS_DIR + "/W_Debrief", unreal.HollowDebriefWidget)):
        generated = load(path).generated_class()
        check(generated is not None and unreal.MathLibrary.class_is_child_of(generated, parent.static_class()), path + " derives from " + parent.__name__)
    check(author.find_widget(load(VISOR_DIR + "/W_Visor"), "RaidStatus") is not None, "W_Visor shows the raid line")
    for path in TOOL_LABELS:
        check(not load(path).get_editor_property("action_label").is_empty(), path + " action label")
    for path in GAME_MODES:
        check(unreal.get_default_object(load(path).generated_class()).get_editor_property("hud_class") == unreal.HollowHUD.static_class(), path + " HUD")
    log("validation passed")


def main():
    ensure_actions()
    ensure_mapping_context()
    ensure_common_input_data()
    ensure_glyphs()
    ensure_button_style()
    ensure_menu_button()
    ensure_primary_layout()
    ensure_stat_bar()
    ensure_prompt()
    ensure_target_status()
    ensure_visor()
    ensure_raid_status()
    ensure_visor_raid_status()
    ensure_system_menu()
    ensure_debrief()
    ensure_tool_labels()
    ensure_hud_class()
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_ui_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
