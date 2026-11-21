#pragma once

#include "Engine/DeveloperSettings.h"

#include "HollowCarrySettings.generated.h"

/** Network rules of heavy carrying (Project Settings > Game > Hollow Carry, DefaultGame.ini; ADR-0015, ADR-0028). */
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

	/** Co-op carry (ADR-0028): the server checks the client-authoritative carriers this often. */
	UPROPERTY(Config, EditAnywhere, Category = "Coop", meta = (ClampMin = "0.05", Units = "Seconds"))
	float CoopSanityIntervalSeconds = 1.f;

	/** A carrier moving faster along the ground than its co-op carry speed times this breaks the carry. */
	UPROPERTY(Config, EditAnywhere, Category = "Coop", meta = (ClampMin = "1"))
	float CoopSpeedTolerance = 1.f;

	/** The sanity check's capsule is this much smaller than the carrier's, so touching a wall is not being inside it. */
	UPROPERTY(Config, EditAnywhere, Category = "Coop", meta = (ClampMin = "0", Units = "Centimeters"))
	float CoopPenetrationTolerance = 0.f;

	/**
	 * A carrier that left a co-op carry stays client-authoritative this long after its moves show it knows (so its
	 * velocity, still shaped by the tether, has settled to the server's) before the server checks its moves again.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Coop", meta = (ClampMin = "0", Units = "Seconds"))
	float CoopAuthoritySettleSeconds = 0.f;

	/** ...and at most this long after the leave, whatever its moves show. */
	UPROPERTY(Config, EditAnywhere, Category = "Coop", meta = (ClampMin = "0", Units = "Seconds"))
	float CoopAuthorityTimeoutSeconds = 0.f;
};
