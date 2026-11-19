#pragma once

#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "HollowToolDefinition.generated.h"

class UGameplayEffect;

/**
 * Stats of a continuous tool (welder, plasma cutter), primary asset type HollowTool. Everything a tool ability does
 * is driven from here (CLAUDE.md sec. 1: data, not code); tool modules modify these numbers later (Economy).
 */
UCLASS(BlueprintType)
class HOLLOWGAS_API UHollowToolDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tool", meta = (Categories = "Hollow.Ability.Tool"))
	FGameplayTag ToolTag;

	/** Server samples per second (trace + work + heat). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sampling", meta = (ClampMin = "1", Units = "Hertz"))
	float SampleRateHz = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sampling", meta = (ClampMin = "1", Units = "Centimeters"))
	float Range = 0.f;

	/** Radius of the server's sphere trace: forgiving aim at 100+ ms ping. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sampling", meta = (ClampMin = "0", Units = "Centimeters"))
	float HitToleranceRadius = 0.f;

	/** The current target is kept while the view stays within this angle of its last impact... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sampling", meta = (ClampMin = "0", Units = "Degrees"))
	float StickyAngleDegrees = 0.f;

	/** ...and its last hit is at most this old. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sampling", meta = (ClampMin = "0", Units = "Seconds"))
	float StickyGraceSeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sampling")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Work units per second of server time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Work", meta = (ClampMin = "0"))
	float WorkRate = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fuel", meta = (ClampMin = "0"))
	float FuelPerSecond = 0.f;

	/** The tool does not start below this much fuel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fuel", meta = (ClampMin = "0"))
	float MinFuelToStart = 0.f;

	/** Infinite periodic effect; its period comes from the asset, its cost per period from FuelPerSecond (Hollow.Data.FuelCost). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fuel")
	TSubclassOf<UGameplayEffect> FuelDrainEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat", meta = (ClampMin = "0"))
	float HeatPerSample = 0.f;

	/** Instant effect adding Hollow.Data.HeatGain to ToolHeat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat")
	TSubclassOf<UGameplayEffect> HeatEffect;

	/** Applied when ToolHeat reaches MaxToolHeat; grants Hollow.State.Tool.Overheated for its duration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat")
	TSubclassOf<UGameplayEffect> OverheatEffect;

	/** Hollow.Noise.* reported to UAISense_Hearing every NoiseEveryNSamples samples (CLAUDE.md sec. 6). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (Categories = "Hollow.Noise"))
	FGameplayTag NoiseTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0"))
	float NoiseLoudness = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0", Units = "Centimeters"))
	float NoiseRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "1"))
	int32 NoiseEveryNSamples = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	FLinearColor BeamColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetics", meta = (ClampMin = "0", Units = "Centimeters"))
	float BeamWidth = 0.f;
};
