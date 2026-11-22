"""Creates the S5-04 hub terminal screens (idempotent, ADR-0010, ADR-0013, ADR-0033).

  /Game/Hollow/UI/Input/IA_UI_TabPrev|IA_UI_TabNext   previous / next tab (Q / E, LB / RB), mapped in IMC_UI
  /Game/Hollow/UI/Hub/BP_ButtonStyle_Tab|BP_ButtonStyle_Row, W_TabButton, W_RowButton   tab and row buttons
  /Game/Hollow/UI/Hub/W_HubTabList, W_HubRow           the workbench's tab bar, one line of a screen's list
  /Game/Hollow/UI/Hub/W_Workbench, W_Loadout, W_ContractTerminal   the three terminals' screens (Menu layer)
  W_Loadout's class list (S6-01, ADR-0034), added to a locker from before it

The screens' classes and the shop's bulk count are config (DefaultGame.ini, Hollow UI settings), checked here.
Widget trees are composed through UHollowWidgetAuthoring (HollowEditor). Creates only what is missing (an existing
widget is kept as is), then validates. Run after create_ui_assets.py:
  Build/Scripts/run-editor-python.sh Build/Python/create_hub_ui_assets.py
"""
import traceback

import unreal

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

UI_DIR = "/Game/Hollow/UI"
INPUT_DIR = UI_DIR + "/Input"
FOUNDATION_DIR = UI_DIR + "/Foundation"
HUB_DIR = UI_DIR + "/Hub"
MENU_BUTTON = FOUNDATION_DIR + "/W_MenuButton"

# Tab actions and their keys (keyboard first, then gamepad); they never swallow gameplay keys (E interacts in game).
TAB_ACTIONS = {
    "IA_UI_TabPrev": ["Q", "Gamepad_LeftShoulder"],
    "IA_UI_TabNext": ["E", "Gamepad_RightShoulder"],
}

# Config the screens need (DefaultGame.ini, [/Script/HollowUI.HollowUISettings]).
INI = "Config/DefaultGame.ini"
INI_LINES = [
    "WorkbenchScreenClass=" + HUB_DIR + "/W_Workbench.W_Workbench_C",
    "LoadoutScreenClass=" + HUB_DIR + "/W_Loadout.W_Loadout_C",
    "ContractScreenClass=" + HUB_DIR + "/W_ContractTerminal.W_ContractTerminal_C",
    "ShopBulkCount=10",
]

# Greybox look (data): text only, like the system menu (icons come with the art pass).
TITLE_SIZE = 32
SECTION_SIZE = 20
ROW_NAME_SIZE = 18
ROW_DETAIL_SIZE = 14
ROW_BUTTON_TEXT_SIZE = 16
TAB_TEXT_SIZE = 20
LINE_SIZE = 16
PANEL_PADDING = 8.0
PANEL_WIDTH = 1200.0
LIST_HEIGHT = 420.0
CONTRACT_LIST_HEIGHT = 300.0
CLASS_LIST_HEIGHT = 120.0   # S6-01: two classes in sight; more scroll
STASH_WIDTH = 480.0
LOADOUT_WIDTH = 704.0
ROW_BUTTON_MIN_WIDTH = 120
TAB_MIN_WIDTH = 200
ROW_BUTTON_PADDING = unreal.Margin(12.0, 6.0, 12.0, 6.0)
TAB_PADDING = unreal.Margin(24.0, 8.0, 24.0, 8.0)
BACKDROP_COLOR = unreal.LinearColor(0.0, 0.0, 0.0, 0.75)
NAME_COLOR = unreal.LinearColor(0.9, 0.92, 0.95, 1.0)
HIGHLIGHT_COLOR = unreal.LinearColor(1.0, 0.75, 0.3, 1.0)
HEADER_COLOR = unreal.LinearColor(0.55, 0.75, 0.85, 1.0)
DETAIL_COLOR = unreal.LinearColor(0.62, 0.66, 0.7, 1.0)
MESSAGE_COLOR = unreal.LinearColor(1.0, 0.82, 0.5, 1.0)
HINT_COLOR = unreal.LinearColor(0.62, 0.66, 0.7, 1.0)
ROW_BUTTON_COLORS = {"normal_base": (0.12, 0.13, 0.15, 0.9), "normal_hovered": (0.3, 0.32, 0.36, 1.0),
                     "normal_pressed": (0.5, 0.35, 0.15, 1.0), "disabled": (0.1, 0.1, 0.1, 0.5)}
TAB_COLORS = {"normal_base": (0.1, 0.11, 0.13, 0.9), "normal_hovered": (0.25, 0.27, 0.3, 1.0), "normal_pressed": (0.5, 0.35, 0.15, 1.0),
              "selected_base": (0.55, 0.38, 0.12, 1.0), "selected_hovered": (0.65, 0.45, 0.15, 1.0), "selected_pressed": (0.65, 0.45, 0.15, 1.0),
              "disabled": (0.1, 0.1, 0.1, 0.5)}


tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
author = unreal.HollowWidgetAuthoring
H = unreal.HorizontalAlignment
V = unreal.VerticalAlignment
NO_PADDING = unreal.Margin(0.0, 0.0, 0.0, 0.0)


def log(message):
    unreal.log("create_hub_ui_assets: " + message)


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


def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


# ---------------------------------------------------------------- input


def ensure_tab_actions():
    for name in TAB_ACTIONS:
        path = INPUT_DIR + "/" + name
        if assets.does_asset_exist(path):
            continue
        action = create(path, unreal.InputAction, unreal.InputAction_Factory())
        action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        action.set_editor_property("consume_input", False)   # E stays Interact in game, LB / RB stay the tools'
        save(action)
        log("created " + name)


def mapped(context):
    """(action path, key) of every mapping of a mapping context."""
    entries = context.get_editor_property("default_key_mappings").get_editor_property("mappings")
    return set((entry.get_editor_property("action").get_path_name(), entry.get_editor_property("key").export_text()) for entry in entries
               if entry.get_editor_property("action") is not None)


def ensure_tab_mappings():
    """IMC_UI (create_ui_assets.py) gets the tab keys it lacks, so CommonUI finds them."""
    context = load(INPUT_DIR + "/IMC_UI")
    data = context.get_editor_property("default_key_mappings")
    entries = list(data.get_editor_property("mappings"))
    known = mapped(context)
    added = 0
    for name, keys in TAB_ACTIONS.items():
        action = load(INPUT_DIR + "/" + name)
        for key_name in keys:
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
        log("IMC_UI: %d tab key(s) mapped" % added)


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


def text(widget, value, size, justification=unreal.TextJustify.LEFT, tint=None):
    widget.set_editor_property("text", unreal.Text(value))
    font = widget.get_editor_property("font")
    font.set_editor_property("size", size)
    widget.set_editor_property("font", font)
    widget.set_editor_property("justification", justification)
    if tint is not None:
        widget.set_editor_property("color_and_opacity", unreal.SlateColor(tint))


def backdrop(blueprint):
    image = add(blueprint, unreal.Image, "Backdrop", "Root")
    image.set_editor_property("color_and_opacity", BACKDROP_COLOR)
    box_slot(image)


def compile_and_save(blueprint):
    if not author.compile(blueprint):
        raise RuntimeError(blueprint.get_name() + " does not compile")
    save(blueprint)
    log("created " + blueprint.get_name())


def ensure_button_style(path, colors, padding):
    if assets.does_asset_exist(path):
        return
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.CommonButtonStyle)
    blueprint = create(path, unreal.Blueprint, factory)
    style = unreal.get_default_object(blueprint.generated_class())
    for prop, rgba in colors.items():
        brush = unreal.SlateBrush()
        brush.set_editor_property("tint_color", unreal.SlateColor(unreal.LinearColor(*rgba)))
        style.set_editor_property(prop, brush)
    style.set_editor_property("button_padding", padding)
    save(blueprint)
    log("created " + path.rsplit("/", 1)[1])


def ensure_button(path, style_path, text_size, min_width):
    """A UHollowButton WBP with a centred label in the given style (class defaults: for buttons made at run time)."""
    blueprint = new_widget_blueprint(path, unreal.HollowButton)
    if blueprint is None:
        return
    label = add(blueprint, unreal.TextBlock, "LabelText")
    text(label, "", text_size, unreal.TextJustify.CENTER)
    compile_and_save(blueprint)
    button = unreal.get_default_object(blueprint.generated_class())
    button.set_editor_property("style", load(style_path).generated_class())
    button.set_editor_property("min_width", min_width)
    save(blueprint)


def ensure_tab_list():
    blueprint = new_widget_blueprint(HUB_DIR + "/W_HubTabList", unreal.HollowTabListWidget)
    if blueprint is None:
        return
    add(blueprint, unreal.HorizontalBox, "Root")
    previous = add(blueprint, unreal.TextBlock, "PreviousHint", "Root")
    text(previous, "", TAB_TEXT_SIZE, tint=HINT_COLOR)
    box_slot(previous, H.H_ALIGN_LEFT, V.V_ALIGN_CENTER, unreal.Margin(0.0, 0.0, PANEL_PADDING * 2.0, 0.0))
    box_slot(add(blueprint, unreal.HorizontalBox, "TabBox", "Root"), H.H_ALIGN_LEFT, V.V_ALIGN_CENTER)
    following = add(blueprint, unreal.TextBlock, "NextHint", "Root")
    text(following, "", TAB_TEXT_SIZE, tint=HINT_COLOR)
    box_slot(following, H.H_ALIGN_LEFT, V.V_ALIGN_CENTER, unreal.Margin(PANEL_PADDING * 2.0, 0.0, 0.0, 0.0))
    compile_and_save(blueprint)
    tab_list = unreal.get_default_object(blueprint.generated_class())
    tab_list.set_editor_property("tab_button_class", load(HUB_DIR + "/W_TabButton").generated_class())
    save(blueprint)


def style_button(button, style_path, min_width):
    """On the instance: a widget nested in another WBP does not see class defaults set by the same run (KnownIssues UI-H1)."""
    button.set_editor_property("style", load(style_path).generated_class())
    button.set_editor_property("min_width", min_width)


def ensure_row():
    blueprint = new_widget_blueprint(HUB_DIR + "/W_HubRow", unreal.HollowHubRowWidget)
    if blueprint is None:
        return
    add(blueprint, unreal.HorizontalBox, "Root")
    column = add(blueprint, unreal.VerticalBox, "TextColumn", "Root")
    box_slot(column, H.H_ALIGN_FILL, V.V_ALIGN_CENTER, unreal.Margin(PANEL_PADDING, PANEL_PADDING / 2.0, PANEL_PADDING, PANEL_PADDING / 2.0), fill=1.0)
    name = add(blueprint, unreal.TextBlock, "NameText", "TextColumn")
    text(name, "", ROW_NAME_SIZE)
    box_slot(name, H.H_ALIGN_LEFT, V.V_ALIGN_TOP)
    detail = add(blueprint, unreal.TextBlock, "DetailText", "TextColumn")
    text(detail, "", ROW_DETAIL_SIZE, tint=DETAIL_COLOR)
    detail.set_editor_property("auto_wrap_text", True)
    box_slot(detail, H.H_ALIGN_FILL, V.V_ALIGN_TOP)
    button_class = load(HUB_DIR + "/W_RowButton").generated_class()
    for slot_name in ("FirstAction", "SecondAction", "ThirdAction"):
        button = add(blueprint, button_class, slot_name, "Root")
        style_button(button, HUB_DIR + "/BP_ButtonStyle_Row", ROW_BUTTON_MIN_WIDTH)
        box_slot(button, H.H_ALIGN_FILL, V.V_ALIGN_CENTER, unreal.Margin(PANEL_PADDING / 2.0, PANEL_PADDING / 2.0, PANEL_PADDING / 2.0, PANEL_PADDING / 2.0))
    compile_and_save(blueprint)
    row = unreal.get_default_object(blueprint.generated_class())
    row.set_editor_property("name_color", NAME_COLOR)
    row.set_editor_property("highlight_color", HIGHLIGHT_COLOR)
    row.set_editor_property("header_color", HEADER_COLOR)
    save(blueprint)


def screen_frame(blueprint, title):
    """Backdrop and the centred panel with its title; returns nothing (the panel is "Panel")."""
    add(blueprint, unreal.Overlay, "Root")
    backdrop(blueprint)
    panel = add(blueprint, unreal.VerticalBox, "Panel", "Root")
    box_slot(panel, H.H_ALIGN_CENTER, V.V_ALIGN_CENTER)
    heading = add(blueprint, unreal.TextBlock, "Title", "Panel")
    text(heading, title, TITLE_SIZE, unreal.TextJustify.CENTER)
    box_slot(heading, H.H_ALIGN_CENTER, V.V_ALIGN_TOP, unreal.Margin(0.0, 0.0, 0.0, PANEL_PADDING * 2.0))


def scrolled_list(blueprint, parent, prefix, list_name, width, height):
    """A fixed-size box scrolling a vertical list (rows go into list_name); the focused row scrolls into view."""
    box = add(blueprint, unreal.SizeBox, prefix + "Box", parent)
    box.set_width_override(width)
    box.set_height_override(height)
    box_slot(box, H.H_ALIGN_FILL, V.V_ALIGN_TOP, unreal.Margin(0.0, PANEL_PADDING, 0.0, PANEL_PADDING))
    scroll = add(blueprint, unreal.ScrollBox, prefix + "Scroll", prefix + "Box")
    scroll.set_scroll_when_focus_changes(unreal.ScrollWhenFocusChanges.INSTANT_SCROLL)
    add(blueprint, unreal.VerticalBox, list_name, prefix + "Scroll")


def screen_footer(blueprint):
    """The message line, then the status line with Close."""
    message = add(blueprint, unreal.TextBlock, "MessageText", "Panel")
    text(message, "", LINE_SIZE, tint=MESSAGE_COLOR)
    box_slot(message, H.H_ALIGN_LEFT, V.V_ALIGN_TOP, unreal.Margin(PANEL_PADDING, PANEL_PADDING, 0.0, PANEL_PADDING))
    footer = add(blueprint, unreal.HorizontalBox, "Footer", "Panel")
    box_slot(footer, H.H_ALIGN_FILL, V.V_ALIGN_TOP)
    status = add(blueprint, unreal.TextBlock, "StatusText", "Footer")
    text(status, "", LINE_SIZE)
    box_slot(status, H.H_ALIGN_LEFT, V.V_ALIGN_CENTER, unreal.Margin(PANEL_PADDING, 0.0, PANEL_PADDING, 0.0), fill=1.0)
    close = add(blueprint, load(MENU_BUTTON).generated_class(), "CloseButton", "Footer")
    close.set_editor_property("label", unreal.Text("Close"))
    box_slot(close, H.H_ALIGN_RIGHT, V.V_ALIGN_CENTER)


def set_row_class(blueprint):
    screen = unreal.get_default_object(blueprint.generated_class())
    screen.set_editor_property("row_class", load(HUB_DIR + "/W_HubRow").generated_class())
    save(blueprint)


def ensure_workbench():
    blueprint = new_widget_blueprint(HUB_DIR + "/W_Workbench", unreal.HollowWorkbenchWidget)
    if blueprint is None:
        return
    screen_frame(blueprint, "WORKBENCH")
    tabs = add(blueprint, load(HUB_DIR + "/W_HubTabList").generated_class(), "Tabs", "Panel")
    tabs.set_editor_property("tab_button_class", load(HUB_DIR + "/W_TabButton").generated_class())
    tabs.set_editor_property("previous_tab_enhanced_input_action", load(INPUT_DIR + "/IA_UI_TabPrev"))
    tabs.set_editor_property("next_tab_enhanced_input_action", load(INPUT_DIR + "/IA_UI_TabNext"))
    box_slot(tabs, H.H_ALIGN_CENTER, V.V_ALIGN_TOP, unreal.Margin(0.0, 0.0, 0.0, PANEL_PADDING))
    scrolled_list(blueprint, "Panel", "List", "RowList", PANEL_WIDTH, LIST_HEIGHT)
    screen_footer(blueprint)
    compile_and_save(blueprint)
    set_row_class(blueprint)


def column(blueprint, name, title, list_name, width):
    holder = add(blueprint, unreal.VerticalBox, name, "Columns")
    box_slot(holder, H.H_ALIGN_FILL, V.V_ALIGN_TOP, unreal.Margin(PANEL_PADDING, 0.0, PANEL_PADDING, 0.0))
    heading = add(blueprint, unreal.TextBlock, name + "Title", name)
    text(heading, title, SECTION_SIZE, tint=HEADER_COLOR)
    box_slot(heading, H.H_ALIGN_LEFT, V.V_ALIGN_TOP)
    scrolled_list(blueprint, name, name.replace("Column", ""), list_name, width, LIST_HEIGHT)


def ensure_loadout():
    blueprint = new_widget_blueprint(HUB_DIR + "/W_Loadout", unreal.HollowLoadoutWidget)
    if blueprint is None:
        return
    screen_frame(blueprint, "LOCKER")
    box_slot(add(blueprint, unreal.HorizontalBox, "Columns", "Panel"), H.H_ALIGN_CENTER, V.V_ALIGN_TOP)
    column(blueprint, "StashColumn", "STASH", "StashList", STASH_WIDTH)
    column(blueprint, "LoadoutColumn", "LOADOUT", "LoadoutList", LOADOUT_WIDTH)
    screen_footer(blueprint)
    compile_and_save(blueprint)
    set_row_class(blueprint)


def ensure_locker_classes():
    """S6-01 (ADR-0034): the player classes above the locker's columns, added to a W_Loadout from before them."""
    blueprint = load(HUB_DIR + "/W_Loadout")
    if author.find_widget(blueprint, "ClassList") is not None:
        return
    heading = author.insert_widget(blueprint, unreal.TextBlock, "ClassTitle", "Panel", 1)   # after the screen's title
    box = author.insert_widget(blueprint, unreal.SizeBox, "ClassBox", "Panel", 2)
    if heading is None or box is None:
        raise RuntimeError("Could not add the class list to W_Loadout")
    text(heading, "CLASS", SECTION_SIZE, tint=HEADER_COLOR)
    box_slot(heading, H.H_ALIGN_LEFT, V.V_ALIGN_TOP, unreal.Margin(PANEL_PADDING, 0.0, 0.0, 0.0))
    box.set_width_override(STASH_WIDTH + LOADOUT_WIDTH + PANEL_PADDING * 4.0)
    box.set_height_override(CLASS_LIST_HEIGHT)
    box_slot(box, H.H_ALIGN_CENTER, V.V_ALIGN_TOP, unreal.Margin(0.0, PANEL_PADDING, 0.0, PANEL_PADDING))
    scroll = add(blueprint, unreal.ScrollBox, "ClassScroll", "ClassBox")
    scroll.set_scroll_when_focus_changes(unreal.ScrollWhenFocusChanges.INSTANT_SCROLL)
    add(blueprint, unreal.VerticalBox, "ClassList", "ClassScroll")
    if not author.compile(blueprint):
        raise RuntimeError("W_Loadout does not compile with the class list")
    save(blueprint)
    log("W_Loadout: class list added")


def ensure_contract_terminal():
    blueprint = new_widget_blueprint(HUB_DIR + "/W_ContractTerminal", unreal.HollowContractTerminalWidget)
    if blueprint is None:
        return
    screen_frame(blueprint, "CONTRACTS")
    role = add(blueprint, unreal.TextBlock, "RoleText", "Panel")
    text(role, "", LINE_SIZE, unreal.TextJustify.CENTER, DETAIL_COLOR)
    box_slot(role, H.H_ALIGN_CENTER, V.V_ALIGN_TOP)
    scrolled_list(blueprint, "Panel", "List", "RowList", PANEL_WIDTH, CONTRACT_LIST_HEIGHT)
    launch = add(blueprint, load(MENU_BUTTON).generated_class(), "LaunchButton", "Panel")
    launch.set_editor_property("label", unreal.Text("Launch the raid"))
    # Full width: the D-pad reaches it from any row's buttons above and Close below (Slate navigates by overlap).
    box_slot(launch, H.H_ALIGN_FILL, V.V_ALIGN_TOP, unreal.Margin(0.0, PANEL_PADDING, 0.0, PANEL_PADDING))
    screen_footer(blueprint)
    compile_and_save(blueprint)
    set_row_class(blueprint)


# ---------------------------------------------------------------- validation


def derives(path, parent):
    generated = load(path).generated_class()
    return generated is not None and unreal.MathLibrary.class_is_child_of(generated, parent.static_class())


def validate():
    imc = mapped(load(INPUT_DIR + "/IMC_UI"))
    for name, keys in TAB_ACTIONS.items():
        action = load(INPUT_DIR + "/" + name)
        check(not action.get_editor_property("consume_input"), name + " never swallows gameplay keys")
        for key_name in keys:
            check((action.get_path_name(), make_key(key_name).export_text()) in imc, "IMC_UI maps %s to %s" % (key_name, name))
    for path, parent in ((HUB_DIR + "/W_TabButton", unreal.HollowButton), (HUB_DIR + "/W_RowButton", unreal.HollowButton),
                         (HUB_DIR + "/W_HubTabList", unreal.HollowTabListWidget), (HUB_DIR + "/W_HubRow", unreal.HollowHubRowWidget),
                         (HUB_DIR + "/W_Workbench", unreal.HollowWorkbenchWidget), (HUB_DIR + "/W_Loadout", unreal.HollowLoadoutWidget),
                         (HUB_DIR + "/W_ContractTerminal", unreal.HollowContractTerminalWidget)):
        check(derives(path, parent), path + " derives from " + parent.__name__)
    row_class = load(HUB_DIR + "/W_HubRow").generated_class()
    check(author.find_widget(load(HUB_DIR + "/W_Loadout"), "ClassList") is not None, "W_Loadout lists the classes (S6-01)")
    row_style = load(HUB_DIR + "/BP_ButtonStyle_Row").generated_class()
    for slot_name in ("FirstAction", "SecondAction", "ThirdAction"):
        button = author.find_widget(load(HUB_DIR + "/W_HubRow"), slot_name)
        check(button is not None and button.get_editor_property("style") == row_style
              and button.get_editor_property("min_width") == ROW_BUTTON_MIN_WIDTH, "W_HubRow " + slot_name + " style")
    for name in ("W_Workbench", "W_Loadout", "W_ContractTerminal"):
        screen = unreal.get_default_object(load(HUB_DIR + "/" + name).generated_class())
        check(screen.get_editor_property("row_class") == row_class, name + " rows are W_HubRow")
    tab_list = unreal.get_default_object(load(HUB_DIR + "/W_HubTabList").generated_class())
    check(tab_list.get_editor_property("tab_button_class") == load(HUB_DIR + "/W_TabButton").generated_class(), "W_HubTabList buttons are W_TabButton")
    tabs = author.find_widget(load(HUB_DIR + "/W_Workbench"), "Tabs")
    check(tabs is not None and tabs.get_editor_property("tab_button_class") == load(HUB_DIR + "/W_TabButton").generated_class(), "W_Workbench tab buttons")
    check(tabs.get_editor_property("next_tab_enhanced_input_action") == load(INPUT_DIR + "/IA_UI_TabNext")
          and tabs.get_editor_property("previous_tab_enhanced_input_action") == load(INPUT_DIR + "/IA_UI_TabPrev"), "W_Workbench tab actions")
    with open(unreal.Paths.project_dir() + INI) as ini:
        lines = set(line.strip() for line in ini)
    for line in INI_LINES:
        check(line in lines, "DefaultGame.ini: " + line)
    log("validation passed")


def main():
    ensure_tab_actions()
    ensure_tab_mappings()
    ensure_button_style(HUB_DIR + "/BP_ButtonStyle_Tab", TAB_COLORS, TAB_PADDING)
    ensure_button_style(HUB_DIR + "/BP_ButtonStyle_Row", ROW_BUTTON_COLORS, ROW_BUTTON_PADDING)
    ensure_button(HUB_DIR + "/W_TabButton", HUB_DIR + "/BP_ButtonStyle_Tab", TAB_TEXT_SIZE, TAB_MIN_WIDTH)
    ensure_button(HUB_DIR + "/W_RowButton", HUB_DIR + "/BP_ButtonStyle_Row", ROW_BUTTON_TEXT_SIZE, ROW_BUTTON_MIN_WIDTH)
    ensure_tab_list()
    ensure_row()
    ensure_workbench()
    ensure_loadout()
    ensure_locker_classes()
    ensure_contract_terminal()
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_hub_ui_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
