#pragma once

#include "Framework/HollowGameModeBase.h"

#include "HollowFrontEndGameMode.generated.h"

/**
 * The front-end map's game mode (L_Boot, S4-06): no ExoFrame, only the main menu (its HUD, BP_GameMode_FrontEnd data).
 * Nobody joins here: sessions start from the menu and open the hub.
 */
UCLASS()
class HOLLOWGAME_API AHollowFrontEndGameMode : public AHollowGameModeBase
{
	GENERATED_BODY()

public:
	explicit AHollowFrontEndGameMode(const FObjectInitializer& ObjectInitializer);
};
