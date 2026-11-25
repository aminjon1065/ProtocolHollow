"""Creates the S2-06 UI content (idempotent, ADR-0010, ADR-0013).

  /Game/Hollow/UI/Input/IA_SystemMenu|IA_UI_Confirm|IA_UI_Back, IMC_UI   UI actions (keyboard + gamepad)
  /Game/Hollow/UI/Input/BP_CommonInputData                             CommonUI click/back actions (Enhanced Input)
  /Game/Hollow/UI/Input/DA_InputGlyphs                                 key labels for prompts
  /Game/Hollow/UI/Foundation/BP_ButtonStyle_Menu, W_MenuButton, W_PrimaryLayout
  /Game/Hollow/UI/Visor/W_StatBar, W_InteractionPrompt, W_TargetStatus   (W_Visor itself: create_visor_assets.py, S4-08)
  /Game/Hollow/UI/Menus/W_SystemMenu (S4-06: + Invite, Start raid, Leave session; S4-07: + Voice; H-1: + Flashes), W_MainMenu
  /Game/Hollow/Core/Framework/BP_GameMode_FrontEnd   AHollowFrontEndGameMode with the HUD; the game mode of L_Boot (S4-06)
  /Game/Hollow/UI/Visor/W_RaidStatus, /Game/Hollow/UI/Menus/W_Debrief      (S3-03)
  DA_Tool_Welder/PlasmaCutter ActionLabel, BP_GameMode_Hub/Raid HUDClass = AHollowHUD   (if unset)

Widget trees are composed through UHollowWidgetAuthoring (HollowEditor): editor Python cannot reach a WBP's tree.
Creates only what is missing (an existing widget is kept as is), then validates.
Run after create_tool_assets.py and create_interaction_assets.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_ui_assets.py
"""
import os
import sys
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hollow_loc as loc   # LOC-01 (ADR-0048): player texts are string table entries

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

UI_DIR = "/Game/Hollow/UI"
INPUT_DIR = UI_DIR + "/Input"
FOUNDATION_DIR = UI_DIR + "/Foundation"
VISOR_DIR = UI_DIR + "/Visor"
MENUS_DIR = UI_DIR + "/Menus"
FRAMEWORK_DIR = "/Game/Hollow/Core/Framework"
GAME_MODES = [FRAMEWORK_DIR + "/BP_GameMode_Hub", FRAMEWORK_DIR + "/BP_GameMode_Raid", FRAMEWORK_DIR + "/BP_GameMode_FrontEnd"]
FRONT_END_GAME_MODE = FRAMEWORK_DIR + "/BP_GameMode_FrontEnd"
BOOT_MAP = "/Game/Hollow/Maps/L_Boot"
# System menu, top to bottom (S4-06): the session buttons go between Resume and Quit; HollowMenuRules shows those that apply.
# Every label is (widget, string table key, English source) in loc.MENUS (LOC-01, ADR-0048).
SESSION_BUTTONS = [("InviteButton", "Menu.System.Invite", "Invite friends"), ("StartRaidButton", "Menu.System.StartRaid", "Start raid"),
                   ("LeaveButton", "Menu.System.Leave", "Leave session")]
SYSTEM_BUTTONS = [("ResumeButton", "Menu.System.Resume", "Resume"), ("QuitButton", "Menu.Quit", "Quit")]
VOICE_BUTTON = ("VoiceButton", "Menu.System.Voice", "Voice: Push-to-talk")   # S4-07: the label follows the mode at run time
FLASHES_BUTTON = ("FlashesButton", "Menu.System.Flashes", "Flashes: Full")    # H-1 (ADR-0029): the label follows the choice at run time
LANGUAGE_BUTTON = ("LanguageButton", "Menu.Language", "Language: English")  # LOC-01: the label follows the language at run time
MAIN_MENU_BUTTONS = [("HostButton", "Menu.Main.Host", "Host co-op"), ("SoloButton", "Menu.Main.Solo", "Play solo"), ("QuitButton", "Menu.Quit", "Quit")]
SYSTEM_TITLE = ("Title", "Menu.System.Title", "SYSTEM")
MAIN_TITLE = ("Title", "Menu.Main.Title", "PROTOCOL: HOLLOW")   # the logo: the same in every language
DEBRIEF_CLOSE = ("CloseButton", "Menu.Debrief.Continue", "Continue")
TOOL_LABELS = {   # loc.ITEMS
    "/Game/Hollow/Tools/Welder/DA_Tool_Welder": ("Tool.Welder.Action", "Welding"),
    "/Game/Hollow/Tools/PlasmaCutter/DA_Tool_PlasmaCutter": ("Tool.PlasmaCutter.Action", "Cutting"),
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
    ("Gamepad_DPad_Up", "D-Up"), ("Gamepad_DPad_Down", "D-Down"), ("Gamepad_DPad_Left", "D-Left"), ("Gamepad_DPad_Right", "D-Right"),
    # GP-01: the hands' keys (Q, R keep the engine's names).
    ("MouseScrollUp", "Wheel"), ("MouseScrollDown", "Wheel"), ("One", "1"), ("Two", "2"),
]
GLYPH_TEXT_KEYS = {"MouseScrollUp": "Glyph.MouseWheel", "MouseScrollDown": "Glyph.MouseWheel"}   # loc.VISOR "Glyph.<Key>" otherwise


def glyph_text_key(key_name):
    return GLYPH_TEXT_KEYS.get(key_name, "Glyph." + key_name)

# Greybox look (data).
LAYER_TRANSITION_SECONDS = 0.0                # instant screen switches; a fade is a number here, no code
TEXT_SIZE = 18
TITLE_SIZE = 32
BUTTON_TEXT_SIZE = 22
PANEL_PADDING = 8.0
DEBRIEF_WIDTH = 720.0
MARKER_SIZE = unreal.Vector2D(3.0, 20.0)
BUTTON_MIN_WIDTH = 280
BUTTON_PADDING = unreal.Margin(24.0, 10.0, 24.0, 10.0)
WELD_COLOR = unreal.LinearColor(1.0, 0.45, 0.1, 1.0)
MARKER_COLOR = unreal.LinearColor(1.0, 1.0, 1.0, 0.9)
BACKDROP_COLOR = unreal.LinearColor(0.0, 0.0, 0.0, 0.6)
BUTTON_COLORS = {"normal_base": (0.12, 0.13, 0.15, 0.9), "normal_hovered": (0.3, 0.32, 0.36, 1.0),
                 "normal_pressed": (0.5, 0.35, 0.15, 1.0), "disabled": (0.1, 0.1, 0.1, 0.5)}


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
    """Creates DA_InputGlyphs, or adds the glyphs it lacks (S4-08: the D-pad)."""
    path = INPUT_DIR + "/DA_InputGlyphs"
    created = not assets.does_asset_exist(path)
    if created:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.HollowInputGlyphSet)
        glyph_set = create(path, unreal.HollowInputGlyphSet, factory)
    else:
        glyph_set = load(path)
    glyphs = list(glyph_set.get_editor_property("glyphs"))
    known = set(g.get_editor_property("key").export_text() for g in glyphs)
    added = 0
    for key_name, label in GLYPHS:
        key = make_key(key_name)
        if key.export_text() in known:
            continue
        glyph = unreal.HollowKeyGlyph()
        glyph.set_editor_property("key", key)
        glyphs.append(glyph)
        added += 1
    sources = dict((make_key(key_name).export_text(), (glyph_text_key(key_name), label)) for key_name, label in GLYPHS)
    relabelled = 0
    for glyph in glyphs:   # LOC-01: every label is a string table entry (glyphs made before it get theirs)
        found = sources.get(glyph.get_editor_property("key").export_text())
        if found and loc.set_text(glyph, "label", loc.VISOR, found[0], found[1]):
            relabelled += 1
    if created or added or relabelled:
        glyph_set.set_editor_property("glyphs", glyphs)
        save(glyph_set)
        log("DA_InputGlyphs: %d glyphs (%d new, %d labels into %s)" % (len(glyphs), added, relabelled, loc.VISOR))


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
    """Value: "" (filled at run time) or (key, English source) in loc.MENUS."""
    widget.set_editor_property("text", loc.text(loc.MENUS, value[0], value[1]) if value else unreal.Text(""))
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
    text(title, SYSTEM_TITLE[1:], TITLE_SIZE, unreal.TextJustify.CENTER)
    box_slot(title, H.H_ALIGN_CENTER, V.V_ALIGN_TOP, unreal.Margin(0.0, 0.0, 0.0, PANEL_PADDING * 3.0))
    button_class = load(FOUNDATION_DIR + "/W_MenuButton").generated_class()
    for name, key, label in SYSTEM_BUTTONS:
        button = add(blueprint, button_class, name, "Panel")
        button.set_editor_property("label", loc.text(loc.MENUS, key, label))
        box_slot(button, H.H_ALIGN_FILL, V.V_ALIGN_TOP, unreal.Margin(0.0, PANEL_PADDING, 0.0, PANEL_PADDING))
    finish(blueprint)


def ensure_system_menu_session_buttons():
    """W_SystemMenu predates the session buttons: insert the missing ones after Resume, in order."""
    blueprint = load(MENUS_DIR + "/W_SystemMenu")
    button_class = load(FOUNDATION_DIR + "/W_MenuButton").generated_class()
    added = []
    for index, (name, key, label) in enumerate(SESSION_BUTTONS):
        if author.find_widget(blueprint, name) is not None:
            continue
        button = author.insert_widget(blueprint, button_class, name, "Panel", 2 + index)   # after the title and Resume
        if button is None:
            raise RuntimeError("Could not add %s to W_SystemMenu" % name)
        button.set_editor_property("label", loc.text(loc.MENUS, key, label))
        box_slot(button, H.H_ALIGN_FILL, V.V_ALIGN_TOP, unreal.Margin(0.0, PANEL_PADDING, 0.0, PANEL_PADDING))
        added.append(name)
    if added:
        if not author.compile(blueprint):
            raise RuntimeError("W_SystemMenu does not compile with the session buttons")
        save(blueprint)
        log("W_SystemMenu: %s added" % ", ".join(added))


def ensure_system_menu_toggle(button, menu="W_SystemMenu"):
    """A settings toggle just before Quit: S4-07 the voice mode, H-1 reduced flashes (after the voice), LOC-01 the language (both menus)."""
    blueprint = load(MENUS_DIR + "/" + menu)
    name, key, label = button
    if author.find_widget(blueprint, name) is not None:
        return
    panel = author.find_widget(blueprint, "Panel")
    quit_index = panel.get_child_index(author.find_widget(blueprint, "QuitButton")) if panel else -1
    if quit_index < 0:
        raise RuntimeError("%s has no Panel with a QuitButton" % menu)
    button = author.insert_widget(blueprint, load(FOUNDATION_DIR + "/W_MenuButton").generated_class(), name, "Panel", quit_index)
    if button is None:
        raise RuntimeError("Could not add %s to %s" % (name, menu))
    button.set_editor_property("label", loc.text(loc.MENUS, key, label))
    box_slot(button, H.H_ALIGN_FILL, V.V_ALIGN_TOP, unreal.Margin(0.0, PANEL_PADDING, 0.0, PANEL_PADDING))
    if not author.compile(blueprint):
        raise RuntimeError("%s does not compile with %s" % (menu, name))
    save(blueprint)
    log("%s: %s added" % (menu, name))


def ensure_main_menu():
    blueprint = new_widget_blueprint(MENUS_DIR + "/W_MainMenu", unreal.HollowMainMenuWidget)
    if blueprint is None:
        return
    add(blueprint, unreal.Overlay, "Root")
    backdrop = add(blueprint, unreal.Image, "Backdrop", "Root")
    image(backdrop, unreal.Vector2D(0.0, 0.0), BACKDROP_COLOR)
    box_slot(backdrop)
    panel = add(blueprint, unreal.VerticalBox, "Panel", "Root")
    box_slot(panel, H.H_ALIGN_CENTER, V.V_ALIGN_CENTER)
    title = add(blueprint, unreal.TextBlock, "Title", "Panel")
    text(title, MAIN_TITLE[1:], TITLE_SIZE, unreal.TextJustify.CENTER)
    box_slot(title, H.H_ALIGN_CENTER, V.V_ALIGN_TOP, unreal.Margin(0.0, 0.0, 0.0, PANEL_PADDING * 3.0))
    button_class = load(FOUNDATION_DIR + "/W_MenuButton").generated_class()
    for name, key, label in MAIN_MENU_BUTTONS:
        button = add(blueprint, button_class, name, "Panel")
        button.set_editor_property("label", loc.text(loc.MENUS, key, label))
        box_slot(button, H.H_ALIGN_FILL, V.V_ALIGN_TOP, unreal.Margin(0.0, PANEL_PADDING, 0.0, PANEL_PADDING))
    finish(blueprint)


def ensure_front_end_game_mode():
    """BP_GameMode_FrontEnd (its HUD is set by ensure_hud_class) as the game mode of L_Boot: the main menu, no ExoFrame."""
    if not assets.does_asset_exist(FRONT_END_GAME_MODE):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.HollowFrontEndGameMode)
        create(FRONT_END_GAME_MODE, unreal.Blueprint, factory)
        save(load(FRONT_END_GAME_MODE))
        log("created BP_GameMode_FrontEnd")


def ensure_boot_game_mode():
    mode = load(FRONT_END_GAME_MODE).generated_class()
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels.load_level(BOOT_MAP):
        raise RuntimeError("Could not load " + BOOT_MAP)
    world_settings = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_world_settings()
    if world_settings.get_editor_property("default_game_mode") == mode:
        return
    world_settings.set_editor_property("default_game_mode", mode)
    if not levels.save_current_level():
        raise RuntimeError("Could not save " + BOOT_MAP)
    log("L_Boot: game mode BP_GameMode_FrontEnd")


def ensure_raid_status():
    blueprint = new_widget_blueprint(VISOR_DIR + "/W_RaidStatus", unreal.HollowRaidStatusWidget)
    if blueprint is None:
        return
    add(blueprint, unreal.VerticalBox, "Root")
    status = add(blueprint, unreal.TextBlock, "StatusText", "Root")
    text(status, "", TEXT_SIZE + 4, unreal.TextJustify.CENTER)
    box_slot(status, H.H_ALIGN_CENTER, V.V_ALIGN_TOP)
    finish(blueprint)


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
    close.set_editor_property("label", loc.text(loc.MENUS, DEBRIEF_CLOSE[1], DEBRIEF_CLOSE[2]))
    box_slot(close, H.H_ALIGN_FILL, V.V_ALIGN_TOP, unreal.Margin(0.0, PANEL_PADDING, 0.0, PANEL_PADDING))
    finish(blueprint)


# ---------------------------------------------------------------- game data


def ensure_tool_labels():
    for path, (key, label) in TOOL_LABELS.items():
        tool = load(path)
        if loc.set_text(tool, "action_label", loc.ITEMS, key, label):
            save(tool)
            log("%s: action label %s" % (path.rsplit("/", 1)[1], key))


def localize_menus():
    """LOC-01 (ADR-0048): the menus' texts made before the string tables point at their entries now."""
    for menu, entries in (("W_SystemMenu", [SYSTEM_TITLE] + SYSTEM_BUTTONS + SESSION_BUTTONS + [VOICE_BUTTON, FLASHES_BUTTON, LANGUAGE_BUTTON]),
                          ("W_MainMenu", [MAIN_TITLE] + MAIN_MENU_BUTTONS + [LANGUAGE_BUTTON]),
                          ("W_Debrief", [DEBRIEF_CLOSE])):
        blueprint = load(MENUS_DIR + "/" + menu)
        changed = []
        for name, key, source in entries:
            widget = author.find_widget(blueprint, name)
            if widget is None:
                raise RuntimeError("%s has no %s" % (menu, name))
            prop = "text" if isinstance(widget, unreal.TextBlock) else "label"
            if loc.set_text(widget, prop, loc.MENUS, key, source):
                changed.append(name)
        if changed:
            if not author.compile(blueprint):
                raise RuntimeError(menu + " does not compile")
            save(blueprint)
            log("%s: %s into %s" % (menu, ", ".join(changed), loc.MENUS))


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
    # Every UI key is mapped (create_hub_ui_assets.py adds the tab keys to IMC_UI, S5-04).
    mappings = load(INPUT_DIR + "/IMC_UI").get_editor_property("default_key_mappings").get_editor_property("mappings")
    known = set((m.get_editor_property("action").get_name(), m.get_editor_property("key").export_text()) for m in mappings)
    for name, keys in UI_ACTIONS.items():
        for key_name in keys:
            check((name, make_key(key_name).export_text()) in known, "IMC_UI maps %s to %s" % (key_name, name))
    data = unreal.get_default_object(load(INPUT_DIR + "/BP_CommonInputData").generated_class())
    check(data.get_editor_property("enhanced_input_back_action") is not None, "CommonUI back action")
    check(len(load(INPUT_DIR + "/DA_InputGlyphs").get_editor_property("glyphs")) == len(GLYPHS), "glyphs")
    for path, parent in ((FOUNDATION_DIR + "/W_PrimaryLayout", unreal.HollowPrimaryLayout),
                         (FOUNDATION_DIR + "/W_MenuButton", unreal.HollowButton),
                         (VISOR_DIR + "/W_StatBar", unreal.HollowStatBarWidget),
                         (VISOR_DIR + "/W_InteractionPrompt", unreal.HollowInteractionPromptWidget),
                         (VISOR_DIR + "/W_TargetStatus", unreal.HollowTargetStatusWidget),
                         (MENUS_DIR + "/W_SystemMenu", unreal.HollowSystemMenuWidget),
                         (MENUS_DIR + "/W_MainMenu", unreal.HollowMainMenuWidget),
                         (VISOR_DIR + "/W_RaidStatus", unreal.HollowRaidStatusWidget),
                         (MENUS_DIR + "/W_Debrief", unreal.HollowDebriefWidget)):
        generated = load(path).generated_class()
        check(generated is not None and unreal.MathLibrary.class_is_child_of(generated, parent.static_class()), path + " derives from " + parent.__name__)
    for name, _, _ in SESSION_BUTTONS + [VOICE_BUTTON, FLASHES_BUTTON, LANGUAGE_BUTTON]:
        check(author.find_widget(load(MENUS_DIR + "/W_SystemMenu"), name) is not None, "W_SystemMenu has " + name)
    check(author.find_widget(load(MENUS_DIR + "/W_MainMenu"), LANGUAGE_BUTTON[0]) is not None, "W_MainMenu has the language button")
    for path, (key, _) in TOOL_LABELS.items():
        check(loc.is_text(load(path).get_editor_property("action_label"), loc.ITEMS, key), path + " action label from " + loc.ITEMS)
    for glyph in load(INPUT_DIR + "/DA_InputGlyphs").get_editor_property("glyphs"):
        check(loc.entry(glyph.get_editor_property("label")) is not None, "glyph %s label from a string table" % glyph.get_editor_property("key").export_text())
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
    ensure_raid_status()
    ensure_system_menu()
    ensure_system_menu_session_buttons()
    ensure_system_menu_toggle(VOICE_BUTTON)
    ensure_system_menu_toggle(FLASHES_BUTTON)
    ensure_system_menu_toggle(LANGUAGE_BUTTON)
    ensure_main_menu()
    ensure_system_menu_toggle(LANGUAGE_BUTTON, "W_MainMenu")
    ensure_debrief()
    ensure_tool_labels()
    localize_menus()
    loc.save()
    ensure_front_end_game_mode()
    ensure_hud_class()
    ensure_boot_game_mode()
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_ui_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
