"""UI-02 (ADR-0052): the menus and the settings screen in the techno-occult look (idempotent, ADR-0010, ADR-0013).

  /Game/Hollow/UI/Foundation/BP_ButtonStyle_Menu, W_MenuButton   the glass menu button with the UI sounds
  /Game/Hollow/UI/Foundation/DA_UIStyle   UHollowUIStyle: badge colours, refused buttons, the terminal sound
  /Game/Hollow/UI/Hub/W_HubBadge          a coloured badge of the hub screens (UHollowBadgeWidget)
  /Game/Hollow/UI/Menus/W_MainMenu        over the menu scene: title and subtitle, the left column (Play solo, Host
                                          co-op, Settings, Quit), the version and the language at the foot
  /Game/Hollow/UI/Menus/W_SystemMenu      Resume, Invite friends, Start raid, Settings, Leave session, Quit
  /Game/Hollow/UI/Menus/W_Debrief         the raid's result
  /Game/Hollow/UI/Menus/BP_ButtonStyle_Setting, W_SettingRow, DA_SettingsLayout, W_Settings   the settings screen: four
                                          tabs (Q / E, LB / RB), one stepping row per setting (UHollowSettingsWidget)
  Config/DefaultGame.ini                  UHollowUISettings::SettingsScreenClass and UIStyle (checked here)

The look is hollow_ui_style.py's; every player text is a loc.MENUS entry (LOC-01). A widget tree or button style is
rebuilt when its spec (the look, the layout below) changes, kept as it is otherwise.
Run after create_ui_assets.py, create_hub_ui_assets.py (the tab bar) and create_audio_assets.py (the UI sounds):
  Build/Scripts/run-editor-python.sh Build/Python/create_menu_ui_assets.py
"""
import os
import sys
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hollow_loc as loc        # LOC-01 (ADR-0048): player texts are string table entries
import hollow_ui_style as style  # UI-02: the look

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

SPEC_TAG = "HollowSpec"
UI_DIR = "/Game/Hollow/UI"
INPUT_DIR = UI_DIR + "/Input"
FOUNDATION_DIR = UI_DIR + "/Foundation"
MENUS_DIR = UI_DIR + "/Menus"
HUB_DIR = UI_DIR + "/Hub"
MENU_BUTTON = FOUNDATION_DIR + "/W_MenuButton"
MENU_BUTTON_STYLE = FOUNDATION_DIR + "/BP_ButtonStyle_Menu"
SETTING_STYLE = MENUS_DIR + "/BP_ButtonStyle_Setting"
UI_STYLE = FOUNDATION_DIR + "/DA_UIStyle"
BADGE = HUB_DIR + "/W_HubBadge"
LAYOUT = MENUS_DIR + "/DA_SettingsLayout"
INI = "Config/DefaultGame.ini"
INI_LINES = [
    "SettingsScreenClass=" + MENUS_DIR + "/W_Settings.W_Settings_C",
    "UIStyle=" + UI_STYLE + ".DA_UIStyle",
]

# Texts: (widget, string table key, English source) in loc.MENUS.
MAIN_TITLE = ("Title", "Menu.Main.Title", "PROTOCOL: HOLLOW")   # the logo: the same in every language
MAIN_SUBTITLE = ("Subtitle", "Menu.Main.Subtitle", "The Ark has returned. Its cores are still warm.")   # Docs/Narrative/Canon.md
MAIN_SLOGAN = ("Slogan", "Menu.Main.Slogan", "The ship hears you.")   # POLISH-01 (ADR-0053): the tagline under the logo
MAIN_BUTTONS = [("SoloButton", "Menu.Main.Solo", "Play solo"), ("HostButton", "Menu.Main.Host", "Host co-op"),
                ("SettingsButton", "Menu.Settings", "Settings"), ("QuitButton", "Menu.Quit", "Quit")]
LANGUAGE_BUTTON = ("LanguageButton", "Menu.Language", "Language: English")   # every translation names its own language
SYSTEM_TITLE = ("Title", "Menu.System.Title", "SYSTEM")
# Top to bottom; HollowMenuRules shows the session buttons where they apply.
SYSTEM_BUTTONS = [("ResumeButton", "Menu.System.Resume", "Resume"), ("InviteButton", "Menu.System.Invite", "Invite friends"),
                  ("StartRaidButton", "Menu.System.StartRaid", "Start raid"), ("SettingsButton", "Menu.Settings", "Settings"),
                  ("LeaveButton", "Menu.System.Leave", "Leave session"), ("QuitButton", "Menu.Quit", "Quit")]
GONE_TEXTS = ["Menu.System.Voice", "Menu.System.Flashes"]   # UI-02: the system menu's toggles moved to the settings screen
DEBRIEF_CLOSE = ("CloseButton", "Menu.Debrief.Continue", "Continue")
SETTINGS_TITLE = ("Title", "Menu.Settings.Title", "SETTINGS")
SETTINGS_CLOSE = ("CloseButton", "Menu.Settings.Close", "Close")

# The settings screen's tabs and rows (DA_SettingsLayout): (tab id, key, English), [(setting, key, English)].
TABS = [
    (("Graphics", "Menu.Settings.Tab.Graphics", "Graphics"), [
        ("GRAPHICS_PRESET", "Menu.Settings.Preset", "Graphics preset"),
        ("WINDOW_MODE", "Menu.Settings.WindowMode", "Display mode"),
        ("V_SYNC", "Menu.Settings.VSync", "Vertical sync"),
        ("FRAME_RATE_LIMIT", "Menu.Settings.FrameLimit", "Frame rate limit")]),
    (("Audio", "Menu.Settings.Tab.Audio", "Audio"), [
        ("MASTER_VOLUME", "Menu.Settings.MasterVolume", "Master volume"),
        ("EFFECTS_VOLUME", "Menu.Settings.EffectsVolume", "Effects"),
        ("VOICE_VOLUME", "Menu.Settings.VoiceVolume", "Voice chat"),
        ("VOICE_MODE", "Menu.Settings.VoiceMode", "Microphone")]),
    (("Controls", "Menu.Settings.Tab.Controls", "Controls"), [
        ("MOUSE_SENSITIVITY", "Menu.Settings.MouseSensitivity", "Mouse sensitivity"),
        ("INVERT_LOOK", "Menu.Settings.InvertLook", "Invert vertical look"),
        ("FIELD_OF_VIEW", "Menu.Settings.FieldOfView", "Field of view"),
        ("CONTROLS_HINT", "Menu.Settings.ControlsHint", "Controls cheat sheet")]),
    (("Accessibility", "Menu.Settings.Tab.Accessibility", "Language and accessibility"), [
        ("LANGUAGE", "Menu.Settings.Language", "Language"),
        ("REDUCED_FLASHES", "Menu.Settings.ReducedFlashes", "Reduced flashes"),
        ("WAYPOINTS", "Menu.Settings.Waypoints", "Waypoints")]),   # POLISH-01 (ADR-0053)
]
# FHollowSettingsTexts: property -> (key, English).
VALUE_TEXTS = {
    "on": ("Menu.Settings.Value.On", "On"), "off": ("Menu.Settings.Value.Off", "Off"),
    "preset_low": ("Menu.Settings.Value.PresetLow", "Low (Lumen Lite)"), "preset_high": ("Menu.Settings.Value.PresetHigh", "High"),
    "fullscreen": ("Menu.Settings.Value.Fullscreen", "Fullscreen"),
    "windowed_fullscreen": ("Menu.Settings.Value.WindowedFullscreen", "Windowed fullscreen"),
    "windowed": ("Menu.Settings.Value.Windowed", "Windowed"), "unlimited": ("Menu.Settings.Value.Unlimited", "Unlimited"),
    "frames_format": ("Menu.Settings.Value.Frames", "{0} FPS"), "degrees_format": ("Menu.Settings.Value.Degrees", "{0}\u00b0"),
    "push_to_talk": ("Menu.Settings.Value.PushToTalk", "Push-to-talk"), "open_mic": ("Menu.Settings.Value.OpenMic", "Open mic"),
    "muted": ("Menu.Settings.Value.Muted", "Muted"),
    # POLISH-01: always on the visor, or hidden in a raid but for a few seconds when the objective changes or on H.
    "waypoints_full": ("Menu.Settings.Value.WaypointsFull", "Full"),
    "waypoints_immersive": ("Menu.Settings.Value.WaypointsImmersive", "Immersive"),
}

# Layout (data).
MENU_BUTTON_MIN_WIDTH = 380
MENU_BUTTON_PADDING = (28.0, 12.0, 28.0, 12.0)
BUTTON_GAP = 6.0
MAIN_MARGIN = (120.0, 104.0, 120.0, 56.0)    # the main menu's distance from the screen's edges
HERO_ACCENT_WIDTH = 420.0
SLOGAN_SIZE = 20            # POLISH-01: the tagline, cyan and spaced like the logo but small
SLOGAN_SPACING = 180
LANGUAGE_MIN_WIDTH = 260
SYSTEM_TITLE_GAP = 20.0
DEBRIEF_WIDTH = 720.0
SETTINGS_WIDTH = 980.0
SETTING_ROW_MIN_WIDTH = 900
SETTING_ROW_PADDING = (20.0, 12.0, 20.0, 12.0)
SETTING_ROW_GAP = (0.0, 4.0, 0.0, 4.0)
SETTING_VALUE_WIDTH = 300.0
SETTING_ARROW_SIZE = 22
SETTINGS_ROWS_HEIGHT = 4 * 66.0               # the longest tab without a jump when the tabs change
TAB_GAP = 18.0

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
author = unreal.HollowWidgetAuthoring
H = unreal.HorizontalAlignment
V = unreal.VerticalAlignment
NO_PADDING = unreal.Margin(0.0, 0.0, 0.0, 0.0)


def log(message):
    unreal.log("create_menu_ui_assets: " + message)


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


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def menu_text(entry):
    return loc.text(loc.MENUS, entry[1], entry[2])


# ---------------------------------------------------------------- widget trees


def widget_blueprint(path, parent_class, *spec):
    """(blueprint to build, its spec hash): a new WBP, an existing one emptied for a rebuild, or None if current."""
    wanted = style.digest(path, *spec)
    if assets.does_asset_exist(path):
        blueprint = load(path)
        if assets.get_metadata_tag(blueprint, SPEC_TAG) == wanted:
            return None, wanted
        if not author.clear_widgets(blueprint):
            raise RuntimeError("Could not clear " + path)
        log("%s: rebuilt (%s)" % (path.rsplit("/", 1)[1], "UI-02 look"))
        return blueprint, wanted
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return create(path, unreal.WidgetBlueprint, factory), wanted


def finish(blueprint, wanted):
    if not author.compile(blueprint):
        raise RuntimeError(blueprint.get_name() + " does not compile")
    assets.set_metadata_tag(blueprint, SPEC_TAG, wanted)
    save(blueprint)
    log("built " + blueprint.get_name())


def add(blueprint, widget_class, name, parent=None):
    widget = author.add_widget(blueprint, widget_class, name, parent or "None")
    if widget is None:
        raise RuntimeError("Could not add %s to %s" % (name, blueprint.get_name()))
    return widget


def box_slot(widget, h=H.H_ALIGN_FILL, v=V.V_ALIGN_FILL, padding=NO_PADDING, fill=0.0):
    if not author.set_box_slot(widget, h, v, padding, fill):
        raise RuntimeError("No box slot on " + widget.get_name())


def text(widget, value, size, tint=style.TEXT, typeface=style.BODY_TYPEFACE, spacing=0, justification=unreal.TextJustify.LEFT):
    """Value: an entry (widget, key, English) of loc.MENUS, or None (filled at run time)."""
    widget.set_editor_property("text", menu_text(value) if value else unreal.Text(""))
    style.font(widget, size, typeface, spacing)
    widget.set_editor_property("color_and_opacity", style.slate_color(tint))
    widget.set_editor_property("justification", justification)


def border(blueprint, name, parent, brush, padding):
    frame = add(blueprint, unreal.Border, name, parent)
    frame.set_editor_property("background", brush)
    frame.set_editor_property("padding", style.margin(padding))
    return frame


def image(blueprint, name, parent, rgba, size=None):
    picture = add(blueprint, unreal.Image, name, parent)
    brush = style.solid(rgba)
    if size:
        image_size = unreal.DeprecateSlateVector2D()   # Slate's float vector type
        image_size.set_editor_property("x", size[0])
        image_size.set_editor_property("y", size[1])
        brush.set_editor_property("image_size", image_size)
    picture.set_editor_property("brush", brush)
    return picture


def accent(blueprint, name, parent, width, h=H.H_ALIGN_CENTER, padding=NO_PADDING):
    """The thin cyan line under a title."""
    line = image(blueprint, name, parent, style.CYAN, (width, style.ACCENT_HEIGHT))
    box_slot(line, h, V.V_ALIGN_TOP, padding)
    return line


def backdrop(blueprint):
    box_slot(image(blueprint, "Backdrop", "Root", style.GLASS_DEEP))


def menu_button(blueprint, entry, parent, min_width=MENU_BUTTON_MIN_WIDTH):
    """A W_MenuButton labelled with its loc.MENUS entry, its style set on the instance (KnownIssues UI-H1)."""
    button = add(blueprint, load(MENU_BUTTON).generated_class(), entry[0], parent)
    button.set_editor_property("label", menu_text(entry))
    button.set_editor_property("style", load(MENU_BUTTON_STYLE).generated_class())
    button.set_editor_property("min_width", min_width)
    button.set_editor_property("focus_brush", style.focus_ring())
    return button


def title_block(blueprint, entry, parent):
    title = add(blueprint, unreal.TextBlock, entry[0], parent)
    text(title, entry, style.TITLE_SIZE, typeface=style.TITLE_TYPEFACE, spacing=style.TITLE_SPACING, justification=unreal.TextJustify.CENTER)
    box_slot(title, H.H_ALIGN_CENTER, V.V_ALIGN_TOP)
    accent(blueprint, "Accent", parent, style.ACCENT_WIDTH, padding=unreal.Margin(0.0, 10.0, 0.0, SYSTEM_TITLE_GAP))


# ---------------------------------------------------------------- button styles


def ensure_button_style(path, states, padding):
    """A CommonUI button style in the glass look with the UI sounds (rebuilt when the look changes)."""
    wanted = style.digest(path, states, padding)
    if assets.does_asset_exist(path):
        blueprint = load(path)
        if assets.get_metadata_tag(blueprint, SPEC_TAG) == wanted:
            return
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.CommonButtonStyle)
        blueprint = create(path, unreal.Blueprint, factory)
    style.style_button_style(unreal.get_default_object(blueprint.generated_class()), states, style.margin(padding))
    assets.set_metadata_tag(blueprint, SPEC_TAG, wanted)
    save(blueprint)
    log("styled " + path.rsplit("/", 1)[1])


def ensure_menu_button():
    """W_MenuButton (create_ui_assets.py) with the look's bold label."""
    blueprint, wanted = widget_blueprint(MENU_BUTTON, unreal.HollowButton, MENU_BUTTON_MIN_WIDTH)
    if blueprint is None:
        return
    label = add(blueprint, unreal.TextBlock, "LabelText")
    text(label, None, style.BUTTON_SIZE, typeface=style.BUTTON_TYPEFACE, justification=unreal.TextJustify.CENTER)
    finish(blueprint, wanted)
    button = unreal.get_default_object(blueprint.generated_class())
    button.set_editor_property("style", load(MENU_BUTTON_STYLE).generated_class())
    button.set_editor_property("min_width", MENU_BUTTON_MIN_WIDTH)
    button.set_editor_property("focus_brush", style.focus_ring())
    save(blueprint)


# ---------------------------------------------------------------- menus


def ensure_main_menu():
    blueprint, wanted = widget_blueprint(MENUS_DIR + "/W_MainMenu", unreal.HollowMainMenuWidget, MAIN_TITLE, MAIN_SLOGAN, MAIN_SUBTITLE, MAIN_BUTTONS,
                                         LANGUAGE_BUTTON, MAIN_MARGIN, HERO_ACCENT_WIDTH, SLOGAN_SIZE, SLOGAN_SPACING)
    if blueprint is None:
        return
    add(blueprint, unreal.Overlay, "Root")   # over the dark menu scene: no backdrop
    content = add(blueprint, unreal.VerticalBox, "Content", "Root")
    box_slot(content, H.H_ALIGN_FILL, V.V_ALIGN_FILL, style.margin(MAIN_MARGIN))
    title = add(blueprint, unreal.TextBlock, MAIN_TITLE[0], "Content")
    text(title, MAIN_TITLE, style.HERO_SIZE, typeface=style.TITLE_TYPEFACE, spacing=style.HERO_SPACING)
    box_slot(title, H.H_ALIGN_LEFT, V.V_ALIGN_TOP)
    slogan = add(blueprint, unreal.TextBlock, MAIN_SLOGAN[0], "Content")
    text(slogan, MAIN_SLOGAN, SLOGAN_SIZE, tint=style.CYAN, typeface=style.TITLE_TYPEFACE, spacing=SLOGAN_SPACING)
    box_slot(slogan, H.H_ALIGN_LEFT, V.V_ALIGN_TOP, unreal.Margin(6.0, 4.0, 0.0, 0.0))
    accent(blueprint, "Accent", "Content", HERO_ACCENT_WIDTH, H.H_ALIGN_LEFT, unreal.Margin(4.0, 12.0, 0.0, 0.0))
    subtitle = add(blueprint, unreal.TextBlock, MAIN_SUBTITLE[0], "Content")
    text(subtitle, MAIN_SUBTITLE, style.SUBTITLE_SIZE, tint=style.TEXT_SECONDARY)
    box_slot(subtitle, H.H_ALIGN_LEFT, V.V_ALIGN_TOP, unreal.Margin(4.0, 16.0, 0.0, 0.0))
    box_slot(add(blueprint, unreal.Spacer, "Above", "Content"), fill=1.0)
    column = border(blueprint, "Column", "Content", style.glass(), style.PANEL_PADDING)
    box_slot(column, H.H_ALIGN_LEFT, V.V_ALIGN_TOP)
    add(blueprint, unreal.VerticalBox, "Panel", "Column")
    for entry in MAIN_BUTTONS:
        box_slot(menu_button(blueprint, entry, "Panel"), H.H_ALIGN_FILL, V.V_ALIGN_TOP, unreal.Margin(0.0, BUTTON_GAP, 0.0, BUTTON_GAP))
    box_slot(add(blueprint, unreal.Spacer, "Below", "Content"), fill=1.0)
    footer = add(blueprint, unreal.HorizontalBox, "Footer", "Content")
    box_slot(footer, H.H_ALIGN_FILL, V.V_ALIGN_BOTTOM)
    version = add(blueprint, unreal.TextBlock, "VersionText", "Footer")
    text(version, None, style.SMALL_SIZE, tint=style.TEXT_MUTED)
    box_slot(version, H.H_ALIGN_LEFT, V.V_ALIGN_CENTER)
    box_slot(add(blueprint, unreal.Spacer, "FooterGap", "Footer"), fill=1.0)
    box_slot(menu_button(blueprint, LANGUAGE_BUTTON, "Footer", LANGUAGE_MIN_WIDTH), H.H_ALIGN_RIGHT, V.V_ALIGN_CENTER)
    finish(blueprint, wanted)


def ensure_system_menu():
    blueprint, wanted = widget_blueprint(MENUS_DIR + "/W_SystemMenu", unreal.HollowSystemMenuWidget, SYSTEM_TITLE, SYSTEM_BUTTONS)
    if blueprint is None:
        return
    add(blueprint, unreal.Overlay, "Root")
    backdrop(blueprint)
    glass = border(blueprint, "Glass", "Root", style.glass(), style.PANEL_PADDING)
    box_slot(glass, H.H_ALIGN_CENTER, V.V_ALIGN_CENTER)
    add(blueprint, unreal.VerticalBox, "Panel", "Glass")
    title_block(blueprint, SYSTEM_TITLE, "Panel")
    for entry in SYSTEM_BUTTONS:
        box_slot(menu_button(blueprint, entry, "Panel"), H.H_ALIGN_FILL, V.V_ALIGN_TOP, unreal.Margin(0.0, BUTTON_GAP, 0.0, BUTTON_GAP))
    finish(blueprint, wanted)


def ensure_debrief():
    blueprint, wanted = widget_blueprint(MENUS_DIR + "/W_Debrief", unreal.HollowDebriefWidget, DEBRIEF_CLOSE, DEBRIEF_WIDTH)
    if blueprint is None:
        return
    add(blueprint, unreal.Overlay, "Root")
    backdrop(blueprint)
    glass = border(blueprint, "Glass", "Root", style.glass(), style.PANEL_PADDING)
    box_slot(glass, H.H_ALIGN_CENTER, V.V_ALIGN_CENTER)
    add(blueprint, unreal.VerticalBox, "Panel", "Glass")
    title = add(blueprint, unreal.TextBlock, "TitleText", "Panel")
    text(title, None, style.TITLE_SIZE, typeface=style.TITLE_TYPEFACE, spacing=style.TITLE_SPACING, justification=unreal.TextJustify.CENTER)
    box_slot(title, H.H_ALIGN_CENTER, V.V_ALIGN_TOP)
    accent(blueprint, "Accent", "Panel", style.ACCENT_WIDTH, padding=unreal.Margin(0.0, 10.0, 0.0, SYSTEM_TITLE_GAP))
    details = add(blueprint, unreal.TextBlock, "DetailsText", "Panel")
    text(details, None, style.BODY_SIZE, tint=style.TEXT_SECONDARY)
    details.set_editor_property("auto_wrap_text", True)
    details.set_editor_property("wrap_text_at", DEBRIEF_WIDTH)
    box_slot(details, H.H_ALIGN_LEFT, V.V_ALIGN_TOP, unreal.Margin(0.0, 0.0, 0.0, SYSTEM_TITLE_GAP))
    box_slot(menu_button(blueprint, DEBRIEF_CLOSE, "Panel"), H.H_ALIGN_FILL, V.V_ALIGN_TOP, unreal.Margin(0.0, BUTTON_GAP, 0.0, 0.0))
    finish(blueprint, wanted)


# ---------------------------------------------------------------- the settings screen


def ensure_setting_row():
    blueprint, wanted = widget_blueprint(MENUS_DIR + "/W_SettingRow", unreal.HollowSettingRowWidget, SETTING_VALUE_WIDTH, SETTING_ARROW_SIZE,
                                         SETTING_ROW_MIN_WIDTH)
    if blueprint is None:
        return
    add(blueprint, unreal.HorizontalBox, "Root")
    label = add(blueprint, unreal.TextBlock, "LabelText", "Root")
    text(label, None, style.BODY_SIZE)
    box_slot(label, H.H_ALIGN_LEFT, V.V_ALIGN_CENTER, fill=1.0)
    # The arrows' glyphs are the code's (UHollowSettingRowWidget): no player text.
    decrease = add(blueprint, unreal.TextBlock, "DecreaseHint", "Root")
    text(decrease, None, SETTING_ARROW_SIZE, tint=style.alpha(style.CYAN, 0.85))
    box_slot(decrease, H.H_ALIGN_RIGHT, V.V_ALIGN_CENTER, unreal.Margin(12.0, 0.0, 12.0, 0.0))
    value_box = add(blueprint, unreal.SizeBox, "ValueBox", "Root")
    value_box.set_width_override(SETTING_VALUE_WIDTH)
    box_slot(value_box, H.H_ALIGN_FILL, V.V_ALIGN_CENTER)
    value = add(blueprint, unreal.TextBlock, "ValueText", "ValueBox")
    text(value, None, style.BODY_SIZE, typeface=style.BUTTON_TYPEFACE, justification=unreal.TextJustify.CENTER)
    increase = add(blueprint, unreal.TextBlock, "IncreaseHint", "Root")
    text(increase, None, SETTING_ARROW_SIZE, tint=style.alpha(style.CYAN, 0.85))
    box_slot(increase, H.H_ALIGN_LEFT, V.V_ALIGN_CENTER, unreal.Margin(12.0, 0.0, 0.0, 0.0))
    finish(blueprint, wanted)
    row = unreal.get_default_object(blueprint.generated_class())
    row.set_editor_property("style", load(SETTING_STYLE).generated_class())
    row.set_editor_property("min_width", SETTING_ROW_MIN_WIDTH)
    row.set_editor_property("focus_brush", style.focus_ring())
    save(blueprint)


def layout_spec():
    return [TABS, sorted(VALUE_TEXTS.items())]


def ensure_settings_layout():
    wanted = style.digest(LAYOUT, layout_spec())
    if assets.does_asset_exist(LAYOUT):
        layout = load(LAYOUT)
        if assets.get_metadata_tag(layout, SPEC_TAG) == wanted:
            return layout
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.HollowSettingsLayout)
        layout = create(LAYOUT, unreal.HollowSettingsLayout, factory)
    tabs = []
    for (tab_id, key, source), rows in TABS:
        tab = unreal.HollowSettingsTabSpec()
        tab.set_editor_property("id", tab_id)
        tab.set_editor_property("label", loc.text(loc.MENUS, key, source))
        specs = []
        for setting, row_key, row_source in rows:
            row = unreal.HollowSettingsRowSpec()
            row.set_editor_property("setting", getattr(unreal.HollowSettingId, setting))
            row.set_editor_property("label", loc.text(loc.MENUS, row_key, row_source))
            specs.append(row)
        tab.set_editor_property("rows", specs)
        tabs.append(tab)
    layout.set_editor_property("tabs", tabs)
    texts = layout.get_editor_property("texts")
    for prop, (key, source) in sorted(VALUE_TEXTS.items()):
        texts.set_editor_property(prop, loc.text(loc.MENUS, key, source))
    layout.set_editor_property("texts", texts)
    assets.set_metadata_tag(layout, SPEC_TAG, wanted)
    save(layout)
    log("DA_SettingsLayout: %d tabs" % len(tabs))
    return layout


def ensure_settings_screen(layout):
    blueprint, wanted = widget_blueprint(MENUS_DIR + "/W_Settings", unreal.HollowSettingsWidget, SETTINGS_TITLE, SETTINGS_CLOSE, SETTINGS_WIDTH,
                                         SETTINGS_ROWS_HEIGHT, SETTING_ROW_GAP, layout_spec())
    if blueprint is None:
        return
    add(blueprint, unreal.Overlay, "Root")
    backdrop(blueprint)
    glass = border(blueprint, "Glass", "Root", style.glass(), style.PANEL_PADDING)
    box_slot(glass, H.H_ALIGN_CENTER, V.V_ALIGN_CENTER)
    add(blueprint, unreal.VerticalBox, "Panel", "Glass")
    title_block(blueprint, SETTINGS_TITLE, "Panel")
    tabs = add(blueprint, load(HUB_DIR + "/W_HubTabList").generated_class(), "Tabs", "Panel")
    tabs.set_editor_property("tab_button_class", load(HUB_DIR + "/W_TabButton").generated_class())
    tabs.set_editor_property("previous_tab_enhanced_input_action", load(INPUT_DIR + "/IA_UI_TabPrev"))
    tabs.set_editor_property("next_tab_enhanced_input_action", load(INPUT_DIR + "/IA_UI_TabNext"))
    box_slot(tabs, H.H_ALIGN_CENTER, V.V_ALIGN_TOP, unreal.Margin(0.0, 0.0, 0.0, TAB_GAP))
    rows_box = add(blueprint, unreal.SizeBox, "RowsBox", "Panel")
    rows_box.set_width_override(SETTINGS_WIDTH)
    rows_box.set_min_desired_height(SETTINGS_ROWS_HEIGHT)
    box_slot(rows_box, H.H_ALIGN_CENTER, V.V_ALIGN_TOP)
    add(blueprint, unreal.VerticalBox, "RowList", "RowsBox")
    footer = add(blueprint, unreal.HorizontalBox, "Footer", "Panel")
    box_slot(footer, H.H_ALIGN_FILL, V.V_ALIGN_TOP, unreal.Margin(0.0, TAB_GAP, 0.0, 0.0))
    box_slot(add(blueprint, unreal.Spacer, "FooterGap", "Footer"), fill=1.0)
    box_slot(menu_button(blueprint, SETTINGS_CLOSE, "Footer", MENU_BUTTON_MIN_WIDTH // 2), H.H_ALIGN_RIGHT, V.V_ALIGN_CENTER)
    finish(blueprint, wanted)
    screen = unreal.get_default_object(blueprint.generated_class())
    screen.set_editor_property("layout", layout)
    screen.set_editor_property("row_class", load(MENUS_DIR + "/W_SettingRow").generated_class())
    screen.set_editor_property("row_padding", style.margin(SETTING_ROW_GAP))
    save(blueprint)


# ---------------------------------------------------------------- badges and the run-time style


def ensure_badge():
    blueprint, wanted = widget_blueprint(BADGE, unreal.HollowBadgeWidget)
    if blueprint is None:
        return
    border(blueprint, "Gap", None, style.no_brush(), (0.0, 0.0, style.BADGE_GAP, 0.0))   # apart from the next badge
    frame = border(blueprint, "Frame", "Gap", style.rounded(style.GLASS, style.FRAME, style.BADGE_RADIUS), style.BADGE_PADDING)
    label = add(blueprint, unreal.TextBlock, "Label", "Frame")
    text(label, None, style.BADGE_SIZE, typeface=style.BUTTON_TYPEFACE)
    frame.set_editor_property("horizontal_alignment", H.H_ALIGN_CENTER)
    frame.set_editor_property("vertical_alignment", V.V_ALIGN_CENTER)
    finish(blueprint, wanted)


def ensure_ui_style():
    wanted = style.digest(UI_STYLE)
    if assets.does_asset_exist(UI_STYLE):
        ui_style = load(UI_STYLE)
        if assets.get_metadata_tag(ui_style, SPEC_TAG) == wanted:
            return
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.HollowUIStyle)
        ui_style = create(UI_STYLE, unreal.HollowUIStyle, factory)
    looks = []
    for kind, (fill, frame, label) in sorted(style.BADGES.items()):
        look = unreal.HollowBadgeLook()
        look.set_editor_property("kind", getattr(unreal.HollowHubBadgeKind, kind))
        look.set_editor_property("fill", style.color(fill))
        look.set_editor_property("frame", style.color(frame))
        look.set_editor_property("text", style.color(label))
        looks.append(look)
    ui_style.set_editor_property("badges", looks)
    ui_style.set_editor_property("badge_class", load(BADGE).generated_class())
    ui_style.set_editor_property("refused_opacity", style.REFUSED_OPACITY)
    ui_style.set_editor_property("terminal_sound", load(style.TERMINAL_SOUND))
    ui_style.set_editor_property("terminal_close_pitch", style.TERMINAL_CLOSE_PITCH)
    assets.set_metadata_tag(ui_style, SPEC_TAG, wanted)
    save(ui_style)
    log("DA_UIStyle ready")


def drop_gone_texts():
    for key in GONE_TEXTS:
        if loc.remove(loc.MENUS, key):
            log("%s: %s dropped" % (loc.MENUS, key))


# ---------------------------------------------------------------- validation


def derives(path, parent):
    generated = load(path).generated_class()
    return generated is not None and unreal.MathLibrary.class_is_child_of(generated, parent.static_class())


def validate():
    for path, parent in ((MENUS_DIR + "/W_MainMenu", unreal.HollowMainMenuWidget), (MENUS_DIR + "/W_SystemMenu", unreal.HollowSystemMenuWidget),
                         (MENUS_DIR + "/W_Debrief", unreal.HollowDebriefWidget), (MENUS_DIR + "/W_Settings", unreal.HollowSettingsWidget),
                         (MENUS_DIR + "/W_SettingRow", unreal.HollowSettingRowWidget), (BADGE, unreal.HollowBadgeWidget),
                         (MENU_BUTTON, unreal.HollowButton)):
        check(derives(path, parent), path + " derives from " + parent.__name__)
    main_menu = load(MENUS_DIR + "/W_MainMenu")
    for name, key, _ in [MAIN_TITLE, MAIN_SLOGAN, MAIN_SUBTITLE, LANGUAGE_BUTTON] + MAIN_BUTTONS:
        widget = author.find_widget(main_menu, name)
        check(widget is not None, "W_MainMenu has " + name)
        check(loc.is_text(widget.get_editor_property("text" if isinstance(widget, unreal.TextBlock) else "label"), loc.MENUS, key),
              "W_MainMenu %s reads %s" % (name, key))
    system = load(MENUS_DIR + "/W_SystemMenu")
    for name, key, _ in SYSTEM_BUTTONS:
        check(author.find_widget(system, name) is not None, "W_SystemMenu has " + name)
    for gone in ("VoiceButton", "FlashesButton", "LanguageButton"):
        check(author.find_widget(system, gone) is None, "W_SystemMenu has no " + gone + " (the settings screen has it)")
    layout = load(LAYOUT)
    check([str(tab.get_editor_property("id")) for tab in layout.get_editor_property("tabs")] == [tab[0][0] for tab in TABS], "DA_SettingsLayout tabs")
    screen = unreal.get_default_object(load(MENUS_DIR + "/W_Settings").generated_class())
    check(screen.get_editor_property("layout") == layout, "W_Settings shows DA_SettingsLayout")
    check(screen.get_editor_property("row_class") == load(MENUS_DIR + "/W_SettingRow").generated_class(), "W_Settings rows are W_SettingRow")
    tabs = author.find_widget(load(MENUS_DIR + "/W_Settings"), "Tabs")
    check(tabs is not None and tabs.get_editor_property("next_tab_enhanced_input_action") == load(INPUT_DIR + "/IA_UI_TabNext"), "W_Settings tab keys")
    menu_style = unreal.get_default_object(load(MENU_BUTTON_STYLE).generated_class())
    check(menu_style.get_editor_property("pressed_slate_sound").get_editor_property("resource_object") == load(style.CONFIRM_SOUND),
          "menu buttons confirm with MS_UI_Confirm")
    ui_style = load(UI_STYLE)
    check(len(ui_style.get_editor_property("badges")) == len(style.BADGES) and ui_style.get_editor_property("terminal_sound") == load(style.TERMINAL_SOUND),
          "DA_UIStyle badges and terminal sound")
    with open(unreal.Paths.project_dir() + INI) as ini:
        lines = set(line.strip() for line in ini)
    for line in INI_LINES:
        check(line in lines, "DefaultGame.ini: " + line)
    log("validation passed")


def main():
    ensure_button_style(MENU_BUTTON_STYLE, style.MENU_BUTTON, MENU_BUTTON_PADDING)
    ensure_button_style(SETTING_STYLE, style.SETTING_ROW, SETTING_ROW_PADDING)
    ensure_menu_button()
    ensure_badge()
    ensure_ui_style()
    ensure_main_menu()
    ensure_system_menu()
    ensure_debrief()
    ensure_setting_row()
    ensure_settings_screen(ensure_settings_layout())
    drop_gone_texts()
    loc.save()
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_menu_ui_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
