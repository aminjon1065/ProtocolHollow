#pragma once

#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"

#include "HollowInteractionSettings.generated.h"

/** Player interaction tuning (Project Settings > Game > Hollow Interaction, stored in DefaultGame.ini). */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Interaction"))
class HOLLOWGAME_API UHollowInteractionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** How often the local focus probe runs (no Tick, CLAUDE.md sec. 1). */
	UPROPERTY(Config, EditAnywhere, Category = "Focus", meta = (ClampMin = "1", Units = "Hertz"))
	float FocusRateHz = 0.f;

	/** Reach from the eyes to an interactable. */
	UPROPERTY(Config, EditAnywhere, Category = "Focus", meta = (ClampMin = "0", Units = "Centimeters"))
	float InteractionRange = 0.f;

	/** Radius of the focus sweep, so small targets are easy to aim at. */
	UPROPERTY(Config, EditAnywhere, Category = "Focus", meta = (ClampMin = "0", Units = "Centimeters"))
	float ProbeRadius = 0.f;

	/** Extra reach accepted by the server, which sees remote players slightly late. */
	UPROPERTY(Config, EditAnywhere, Category = "Server", meta = (ClampMin = "0", Units = "Centimeters"))
	float ServerRangeTolerance = 0.f;

	/** Minimum time between two accepted interactions of one player. */
	UPROPERTY(Config, EditAnywhere, Category = "Server", meta = (ClampMin = "0", Units = "Seconds"))
	float MinInteractInterval = 0.f;

	/** States of the interacting character that forbid interaction (e.g. Hollow.State.Dead). */
	UPROPERTY(Config, EditAnywhere, Category = "Server", meta = (Categories = "Hollow.State"))
	FGameplayTagContainer BlockingTags;
};
