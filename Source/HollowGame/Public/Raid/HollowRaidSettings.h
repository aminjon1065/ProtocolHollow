#pragma once

#include "Engine/DeveloperSettings.h"

#include "HollowRaidSettings.generated.h"

class AHollowSoloDrone;
class UGameplayEffect;

/** Raid flow timing (Project Settings > Game > Hollow Raid, DefaultGame.ini; ADR-0016). */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Raid"))
class HOLLOWGAME_API UHollowRaidSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Landing time between the pawns spawning and the raid starting. */
	UPROPERTY(Config, EditAnywhere, Category = "Phases", meta = (ClampMin = "0", Units = "Seconds"))
	float DeploySeconds = 0.f;

	/** The shuttle leaves this long after someone alive entered the zone (reset when the zone empties). */
	UPROPERTY(Config, EditAnywhere, Category = "Extraction", meta = (ClampMin = "0", Units = "Seconds"))
	float ExtractionCountdownSeconds = 0.f;

	/** How often the server checks the zone (no Tick). */
	UPROPERTY(Config, EditAnywhere, Category = "Extraction", meta = (ClampMin = "1", Units = "Hertz"))
	float ZonePollRateHz = 1.f;

	/** Results are out this long before the squad returns to the hub. */
	UPROPERTY(Config, EditAnywhere, Category = "Phases", meta = (ClampMin = "0", Units = "Seconds"))
	float DebriefSeconds = 0.f;

	/** A dead player watches their body this long before the free spectator camera. */
	UPROPERTY(Config, EditAnywhere, Category = "Death", meta = (ClampMin = "0", Units = "Seconds"))
	float DeathSpectateDelaySeconds = 0.f;

	/** DRONE-01 (ADR-0054): the drone a raid of exactly one player gets (BP_SoloDrone; UHollowSoloDroneSubsystem). */
	UPROPERTY(Config, EditAnywhere, Category = "Solo")
	TSoftClassPtr<AHollowSoloDrone> SoloDroneClass;

	/** Instant damage effect (SetByCaller Hollow.Data.Damage) used by Hollow.Dev.Kill. */
	UPROPERTY(Config, EditAnywhere, Category = "Development")
	TSoftClassPtr<UGameplayEffect> DevDamageEffect;

	/** Instant Resonance change (SetByCaller Hollow.Data.ResonanceGain) used by Hollow.Resonance.Add. */
	UPROPERTY(Config, EditAnywhere, Category = "Development")
	TSoftClassPtr<UGameplayEffect> DevResonanceEffect;
};
