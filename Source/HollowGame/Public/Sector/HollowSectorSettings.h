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

	/** Sector Game Feature of the raid until contracts pick it (decision S1-08, ADR-0012). */
	UPROPERTY(Config, EditAnywhere, Category = "Raid")
	FString RaidSector;

	/** A player that has not reported its sector by then is kicked and the raid starts without it. */
	UPROPERTY(Config, EditAnywhere, Category = "Readiness", meta = (ClampMin = "1", Units = "Seconds"))
	float ReadyTimeoutSeconds = 0.f;

	/** Retry period of a client report while its player controller is not there yet. */
	UPROPERTY(Config, EditAnywhere, Category = "Readiness", meta = (ClampMin = "0.1", Units = "Seconds"))
	float ReportRetrySeconds = 0.f;
};
