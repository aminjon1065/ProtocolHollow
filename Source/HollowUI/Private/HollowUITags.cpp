#include "HollowUITags.h"

#include "Tags/HollowTagRules.h"

namespace HollowUITags
{
	HOLLOW_DEFINE_GAMEPLAY_TAG(UI_Layer_Game, "Hollow.UI.Layer.Game", "The visor (HUD): game input.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(UI_Layer_GameMenu, "Hollow.UI.Layer.GameMenu", "In-game panels that keep the game running (inventory, map).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(UI_Layer_Menu, "Hollow.UI.Layer.Menu", "Menus that take input (system menu).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(UI_Layer_Modal, "Hollow.UI.Layer.Modal", "Confirmations and errors above everything.");
}
