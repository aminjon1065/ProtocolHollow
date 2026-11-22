#pragma once

#include "Engine/DeveloperSettings.h"

#include "HollowSectorSettings.generated.h"

class UWorld;

/** Raid sector assembly (Project Settings > Game > Hollow Sector, stored in DefaultGame.ini). */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Sector"))
class HOLLOWGAME_API UHollowSectorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Persistent raid map the host travels to (Hollow.Raid.Start until the raid flow, S3-03). */
	UPROPERTY(Config, EditAnywhere, Category = "Raid")
	TSoftObjectPtr<UWorld> RaidMap;

	/**
	 * The contract the hub starts with (ADR-0032); the host picks another at the contracts terminal. Also the contract,
	 * and so the sector (ADR-0041), of a raid travelled to without one (the raid map opened directly).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Raid", meta = (AllowedTypes = "HollowContract"))
	FPrimaryAssetId DefaultContract;

	/** A player that has not reported its sector by then is kicked and the raid starts without it. */
	UPROPERTY(Config, EditAnywhere, Category = "Readiness", meta = (ClampMin = "1", Units = "Seconds"))
	float ReadyTimeoutSeconds = 0.f;

	/** Retry period of a client report while its player controller is not there yet. */
	UPROPERTY(Config, EditAnywhere, Category = "Readiness", meta = (ClampMin = "0.1", Units = "Seconds"))
	float ReportRetrySeconds = 0.f;
};
