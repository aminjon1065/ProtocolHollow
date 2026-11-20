#pragma once

#include "Engine/DeveloperSettings.h"

#include "HollowCarrySettings.generated.h"

/** Network rules of heavy carrying (Project Settings > Game > Hollow Carry, DefaultGame.ini; ADR-0015). */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Carry"))
class HOLLOWPHYSICS_API UHollowCarrySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * A stricter server carry state binds a remote carrier's movement only this long after it changed: time for the
	 * client to learn it and claim it in its saved moves (must exceed the round trip). Gentler states bind at once.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Server", meta = (ClampMin = "0", Units = "Seconds"))
	float ServerSlowdownGraceSeconds = 0.f;

	/** A predicted pick-up the server never confirms stops being claimed after this long (refused or lost request). */
	UPROPERTY(Config, EditAnywhere, Category = "Client", meta = (ClampMin = "0", Units = "Seconds"))
	float PickupPredictionTimeoutSeconds = 0.f;
};
