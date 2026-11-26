"""UI-02 (ADR-0052): the techno-occult look of every menu and hub screen (not a content script itself).

Dark translucent glass, thin accent frames (cold cyan, violet, gold: the palette of the Aether effects), a readable
hierarchy of type. Everything is Slate's own drawing (rounded boxes with an outline), no texture and no blur: it costs
nothing on Mac Low. The content scripts that build widget trees (create_menu_ui_assets.py, create_hub_ui_assets.py)
take their colours, brushes, sizes and button sounds from here; DA_UIStyle carries what the code reads at run time
(badge colours, refused buttons, the terminal sound).

    import os, sys
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import hollow_ui_style as style
    border.set_editor_property("background", style.glass())
"""
import hashlib
import json

import unreal

VERSION = 1                     # bump to rebuild every styled widget tree and button style

# ---------------------------------------------------------------- palette (linear RGBA)
GLASS = (0.010, 0.014, 0.020, 0.86)          # a panel
GLASS_ROW = (0.030, 0.042, 0.055, 0.55)      # a line of a list
GLASS_DEEP = (0.004, 0.006, 0.010, 0.84)     # behind a screen: the world (and the visor) stays faintly visible
CYAN = (0.30, 0.85, 0.95, 1.0)
VIOLET = (0.62, 0.42, 0.98, 1.0)
GOLD = (0.95, 0.75, 0.35, 1.0)
TEXT = (0.92, 0.95, 0.97, 1.0)               # titles, names, values
TEXT_SECONDARY = (0.64, 0.71, 0.77, 1.0)     # details, subtitles
TEXT_MUTED = (0.42, 0.48, 0.54, 1.0)         # hints, the version line
MESSAGE = (1.0, 0.82, 0.5, 1.0)              # what a press did, why it was refused


def alpha(rgba, a):
    return (rgba[0], rgba[1], rgba[2], a)


FRAME = alpha(CYAN, 0.35)                    # a panel's thin frame

# ---------------------------------------------------------------- type
HERO_SIZE = 72          # the main menu's title
HERO_SPACING = 260      # its letter spacing (1/1000 em)
SUBTITLE_SIZE = 22
TITLE_SIZE = 32
TITLE_SPACING = 180
SECTION_SIZE = 20
BODY_SIZE = 18
DETAIL_SIZE = 14
SMALL_SIZE = 14
BUTTON_SIZE = 22
TITLE_TYPEFACE = "Light"
BODY_TYPEFACE = "Regular"
BUTTON_TYPEFACE = "Bold"

# ---------------------------------------------------------------- shapes
PANEL_RADIUS = 6.0
BUTTON_RADIUS = 4.0
FRAME_WIDTH = 1.0
FOCUS_FRAME_WIDTH = 1.5
FOCUS_RING_WIDTH = 2.0   # UHollowButton::FocusBrush: the keys' and the gamepad's focus
PANEL_PADDING = 28.0
ACCENT_HEIGHT = 2.0
ACCENT_WIDTH = 96.0

# ---------------------------------------------------------------- buttons
# Fill and frame of each state of a CommonUI button style.
MENU_BUTTON = {
    "normal_base": ((0.04, 0.06, 0.08, 0.85), alpha(CYAN, 0.22)),
    "normal_hovered": ((0.05, 0.15, 0.19, 0.95), alpha(CYAN, 0.95)),
    "normal_pressed": ((0.24, 0.17, 0.05, 1.0), alpha(GOLD, 1.0)),
    "disabled": ((0.03, 0.03, 0.04, 0.6), (0.3, 0.3, 0.32, 0.2)),
}
ROW_BUTTON = MENU_BUTTON
TAB_BUTTON = dict(MENU_BUTTON, **{
    "selected_base": ((0.16, 0.09, 0.30, 0.95), alpha(VIOLET, 0.95)),
    "selected_hovered": ((0.20, 0.12, 0.36, 1.0), alpha(VIOLET, 1.0)),
    "selected_pressed": ((0.20, 0.12, 0.36, 1.0), alpha(VIOLET, 1.0)),
})
SETTING_ROW = {
    "normal_base": (GLASS_ROW, alpha(CYAN, 0.10)),
    "normal_hovered": ((0.05, 0.13, 0.17, 0.92), alpha(CYAN, 0.85)),
    "normal_pressed": ((0.07, 0.17, 0.22, 0.95), alpha(CYAN, 1.0)),
    "disabled": ((0.03, 0.03, 0.04, 0.5), (0.3, 0.3, 0.32, 0.2)),
}

# The sounds of every button style (create_audio_assets.py): the mouse's hover and the keys' focus, a press, a
# refused press (a locked button: UHollowButton::SetRefused).
SOUNDS = "/Game/Hollow/Audio/MetaSounds"
FOCUS_SOUND = SOUNDS + "/MS_UI_Focus"
CONFIRM_SOUND = SOUNDS + "/MS_UI_Confirm"
DENY_SOUND = SOUNDS + "/MS_UI_Deny"
TERMINAL_SOUND = SOUNDS + "/MS_UI_Terminal"

# ---------------------------------------------------------------- badges (DA_UIStyle): fill, frame, text
BADGES = {
    "PRICE": ((0.20, 0.14, 0.03, 0.92), alpha(GOLD, 0.9), (1.0, 0.86, 0.55, 1.0)),
    "SLOT": ((0.11, 0.07, 0.21, 0.92), alpha(VIOLET, 0.9), (0.86, 0.78, 1.0, 1.0)),
    "INSURED": ((0.03, 0.15, 0.13, 0.92), (0.3, 0.95, 0.8, 0.9), (0.62, 1.0, 0.9, 1.0)),
}
BADGE_RADIUS = 9.0
BADGE_PADDING = (8.0, 2.0, 8.0, 2.0)
BADGE_SIZE = 13
BADGE_GAP = 6.0
REFUSED_OPACITY = 0.45
TERMINAL_CLOSE_PITCH = 0.8


def spec():
    """Everything above, for the spec hash of a styled asset: a change here rebuilds it."""
    names = [name for name in sorted(globals()) if name.isupper()]
    return {name: globals()[name] for name in names}


def digest(*parts):
    return hashlib.sha1(json.dumps([spec()] + list(parts), sort_keys=True, default=str).encode("ascii")).hexdigest()


# ---------------------------------------------------------------- values

def color(rgba):
    return unreal.LinearColor(*rgba)


def slate_color(rgba):
    return unreal.SlateColor(color(rgba))


def rounded(fill, frame=None, radius=PANEL_RADIUS, width=FRAME_WIDTH):
    """A rounded box of Slate's own drawing, filled, with a thin frame (none: no frame)."""
    brush = unreal.SlateBrush()
    brush.set_editor_property("draw_as", unreal.SlateBrushDrawType.ROUNDED_BOX)
    brush.set_editor_property("tint_color", slate_color(fill))
    outline = unreal.SlateBrushOutlineSettings()
    outline.set_editor_property("corner_radii", unreal.Vector4(radius, radius, radius, radius))
    outline.set_editor_property("rounding_type", unreal.SlateBrushRoundingType.FIXED_RADIUS)
    outline.set_editor_property("color", slate_color(frame or alpha(fill, 0.0)))
    outline.set_editor_property("width", width if frame else 0.0)
    brush.set_editor_property("outline_settings", outline)
    return brush


def glass():
    return rounded(GLASS, FRAME)


def no_brush():
    """Draws nothing (a border that only spaces its content)."""
    brush = unreal.SlateBrush()
    brush.set_editor_property("draw_as", unreal.SlateBrushDrawType.NO_DRAW_TYPE)
    return brush


def solid(rgba):
    brush = unreal.SlateBrush()
    brush.set_editor_property("tint_color", slate_color(rgba))
    return brush


def margin(values):
    if isinstance(values, (int, float)):
        values = (values, values, values, values)
    return unreal.Margin(*values)


def font(widget, size, typeface=BODY_TYPEFACE, spacing=0):
    info = widget.get_editor_property("font")
    info.set_editor_property("size", size)
    info.set_editor_property("typeface_font_name", typeface)
    info.set_editor_property("letter_spacing", spacing)
    widget.set_editor_property("font", info)


def sound(path):
    slate = unreal.SlateSound()
    slate.set_editor_property("resource_object", unreal.load_asset(path))
    return slate


def optional_sound(path):
    optional = unreal.CommonButtonStyleOptionalSlateSound()
    optional.set_editor_property("has_sound", True)
    optional.set_editor_property("sound", sound(path))
    return optional


def focus_ring(radius=BUTTON_RADIUS):
    """The frame a Hollow button draws over itself while the keys or a gamepad focus it."""
    return rounded(alpha(CYAN, 0.0), alpha(CYAN, 1.0), radius, FOCUS_RING_WIDTH)


def style_button_style(style, states, padding, radius=BUTTON_RADIUS):
    """A CommonButtonStyle CDO in the glass look, with the UI sounds."""
    for prop, (fill, frame) in states.items():
        width = FOCUS_FRAME_WIDTH if prop.endswith("hovered") or prop.startswith("selected") else FRAME_WIDTH
        style.set_editor_property(prop, rounded(fill, frame, radius, width))
    style.set_editor_property("button_padding", padding)
    style.set_editor_property("hovered_slate_sound", sound(FOCUS_SOUND))
    style.set_editor_property("pressed_slate_sound", sound(CONFIRM_SOUND))
    style.set_editor_property("locked_pressed_slate_sound", optional_sound(DENY_SOUND))
