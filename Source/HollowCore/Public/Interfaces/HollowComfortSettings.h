#pragma once

#include "UObject/Interface.h"

#include "HollowComfortSettings.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UHollowComfortSettings : public UInterface
{
	GENERATED_BODY()
};

/**
 * The player's comfort choices as the lower modules see them (GP-01c, ADR-0046). The game's user settings implement
 * it (HollowGame: UHollowGameUserSettings), so presentation in HollowCore, HollowPhysics or HollowAI can honour them
 * without depending on the game module.
 */
class HOLLOWCORE_API IHollowComfortSettings
{
	GENERATED_BODY()

public:
	/** Reduced flashes (ADR-0029): no strobing light, steady glows, softer camera motion. */
	virtual bool IsReducedFlashes() const = 0;

	/** The engine's user settings' choice; false without user settings that implement this. */
	static bool IsReducedFlashesChosen();
};
