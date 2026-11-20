#pragma once

#include "CoreMinimal.h"

class UWorld;

/** Where the local player stands when a menu opens (S4-06). */
struct HOLLOWUI_API FHollowMenuContext
{
	/** In a session (solo counts: it is a listen session of one). */
	bool bInSession = false;
	/** This machine hosts it. */
	bool bHost = false;
	/** Other players can be invited (not solo). */
	bool bOnline = false;
	/** In the hub, where a raid starts. */
	bool bInHub = false;
};

/** Which system menu buttons apply in a context. */
struct HOLLOWUI_API FHollowSystemMenuOptions
{
	bool bInvite = false;
	bool bStartRaid = false;
	bool bLeave = false;
};

/** Pure menu rules: the menus call the same session and raid paths as the console (one path for solo and co-op). */
namespace HollowMenuRules
{
	HOLLOWUI_API FHollowSystemMenuOptions SystemMenuOptions(const FHollowMenuContext& Context);

	/** The context of World's local player: its session (UHollowSessionSubsystem) and map. */
	HOLLOWUI_API FHollowMenuContext ReadContext(const UWorld* World);

	/** Whether World is the front-end map (UHollowNetworkSettings::FrontEndMap): the main menu instead of the visor. */
	HOLLOWUI_API bool IsFrontEnd(const UWorld* World);
}
