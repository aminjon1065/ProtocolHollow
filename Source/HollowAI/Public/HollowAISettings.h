#pragma once

#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"

#include "HollowAISettings.generated.h"

class AHollowEnemyCharacter;
class AHollowWhispererNest;
class UHollowThreatConfig;

/** AI settings (DefaultGame.ini): the threat config of a raid and the Hollow.AI.* development commands; enemy tuning lives in their definitions. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow AI"))
class HOLLOWAI_API UHollowAISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Heat, vents and the director's leash of a raid (ADR-0019). */
	UPROPERTY(Config, EditAnywhere, Category = "Threat")
	TSoftObjectPtr<UHollowThreatConfig> ThreatConfig;

	/** Enemy that Hollow.AI.Spawn places. */
	UPROPERTY(Config, EditAnywhere, Category = "Development")
	TSoftClassPtr<AHollowEnemyCharacter> DevEnemyClass;

	/** Nest that Hollow.AI.Nest places (GP-02). */
	UPROPERTY(Config, EditAnywhere, Category = "Development")
	TSoftClassPtr<AHollowWhispererNest> DevNestClass;

	/** Default distance of Hollow.AI.Spawn: in front of the player, or behind the nearest obstacle. */
	UPROPERTY(Config, EditAnywhere, Category = "Development", meta = (ClampMin = "0", Units = "Centimeters"))
	float DevSpawnDistance = 0.f;

	/** How far around the wanted spot a spawn may land on the enemy's navmesh. */
	UPROPERTY(Config, EditAnywhere, Category = "Development", meta = (ClampMin = "0", Units = "Centimeters"))
	float DevSpawnSearchExtent = 0.f;

	/** Noise Hollow.AI.Noise makes without a tag (its rule in DA_NoiseConfig sets how far it carries). */
	UPROPERTY(Config, EditAnywhere, Category = "Development", meta = (Categories = "Hollow.Noise"))
	FGameplayTag DevNoiseTag;
};
