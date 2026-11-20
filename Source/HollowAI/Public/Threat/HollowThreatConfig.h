#pragma once

#include "Engine/DataAsset.h"

#include "HollowThreatConfig.generated.h"

class AHollowEnemyCharacter;

/** Activity heat (UHollowActivityHeatmapSubsystem). */
USTRUCT(BlueprintType)
struct FHollowHeatmapTuning
{
	GENERATED_BODY()

	/** Presence is sampled (and heat decays) this often. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heatmap", meta = (ClampMin = "0.05", Units = "Seconds"))
	float SampleSeconds = 0.f;

	/** Heat a room gains per second per living player in it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heatmap", meta = (ClampMin = "0"))
	float PresencePerSecond = 0.f;

	/** Heat a room gains per unit of loudness of a player's noise in it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heatmap", meta = (ClampMin = "0"))
	float NoisePerLoudness = 0.f;

	/** Heat a portal gains each time a player goes through it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heatmap", meta = (ClampMin = "0"))
	float CrossingHeat = 0.f;

	/** Heat halves in this time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heatmap", meta = (ClampMin = "1", Units = "Seconds"))
	float HalfLifeSeconds = 0.f;
};

/** The abstract vent graph (UHollowVentNetworkSubsystem). */
USTRUCT(BlueprintType)
struct FHollowVentTuning
{
	GENERATED_BODY()

	/** Two vents are linked when their rooms are at most this many portals apart. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vents", meta = (ClampMin = "0"))
	int32 MaxRoomLinks = 0;

	/** Time in the shaft per room passed (a vent in the same room counts as one). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vents", meta = (ClampMin = "0.01", Units = "Seconds"))
	float SecondsPerLink = 0.f;
};

/** The aggression leash (UHollowThreatDirectorSubsystem). */
USTRUCT(BlueprintType)
struct FHollowDirectorTuning
{
	GENERATED_BODY()

	/** The stalker of the sector, spawned by the director. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Director")
	TSoftClassPtr<AHollowEnemyCharacter> StalkerClass;

	/** After the sector is assembled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Director", meta = (ClampMin = "0.01", Units = "Seconds"))
	float SpawnDelaySeconds = 0.f;

	/** How far from the chosen vent the stalker may land on its navmesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Director", meta = (ClampMin = "0", Units = "Centimeters"))
	float SpawnSearchExtent = 0.f;

	/** Shadowing before it may hunt. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Director", meta = (ClampMin = "0.01", Units = "Seconds"))
	float StalkSeconds = 0.f;

	/** It hunts at once when the hottest room nobody is in gets this hot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Director", meta = (ClampMin = "0"))
	float HuntHeatThreshold = 0.f;

	/** A hunt without a strike ends after this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Director", meta = (ClampMin = "0.01", Units = "Seconds"))
	float HuntSeconds = 0.f;

	/** Hidden in the vents after a strike, a stun or a hunt that went nowhere. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Director", meta = (ClampMin = "0.01", Units = "Seconds"))
	float RetreatSeconds = 0.f;

	/** How often the heat is checked while it stalks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Director", meta = (ClampMin = "0.05", Units = "Seconds"))
	float HeatCheckSeconds = 0.f;
};

/**
 * Tuning of the threat systems of a raid (ADR-0019): heat, vents and the director's leash. One data asset
 * (DA_ThreatConfig, chosen by UHollowAISettings::ThreatConfig), so the pacing is tuned without a rebuild.
 */
UCLASS(BlueprintType)
class HOLLOWAI_API UHollowThreatConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The project's config (loaded once; small). */
	static const UHollowThreatConfig* Get();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Threat")
	FHollowHeatmapTuning Heatmap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Threat")
	FHollowVentTuning Vents;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Threat")
	FHollowDirectorTuning Director;
};
