#pragma once

#include "Engine/DeveloperSettings.h"

#include "HollowGraphicsSettings.generated.h"

/** The two graphics presets (ADR-0023): High is the E9 target, Low is Lumen Lite for the Steam Deck and weak GPUs. */
UENUM(BlueprintType)
enum class EHollowGraphicsPreset : uint8
{
	Low,
	High
};

/** What one preset sets. Its cvars live in DefaultScalability.ini under its scalability level. */
USTRUCT(BlueprintType)
struct HOLLOWGAME_API FHollowGraphicsPresetSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	EHollowGraphicsPreset Preset = EHollowGraphicsPreset::High;

	/** Engine scalability level of every group (0 Low .. 3 Epic); the project's overrides are in DefaultScalability.ini. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "0", ClampMax = "3"))
	int32 ScalabilityLevel = 0;

	/** Internal resolution (TSR upscales the rest). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "25", ClampMax = "100", Units = "Percent"))
	float ScreenPercentage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	bool bDynamicResolution = false;

	/** Dynamic resolution's floor and its frame budget. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "25", ClampMax = "100", Units = "Percent", EditCondition = "bDynamicResolution"))
	float MinScreenPercentage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "1", Units = "Milliseconds", EditCondition = "bDynamicResolution"))
	float DynamicResolutionBudgetMs = 0.f;

	/** 0: no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "0"))
	float FrameRateLimit = 0.f;
};

/** Graphics presets and how the first launch chooses one (DefaultGame.ini). */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Graphics"))
class HOLLOWGAME_API UHollowGraphicsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	const FHollowGraphicsPresetSpec* FindPreset(EHollowGraphicsPreset Preset) const;

	/** The preset for a GPU benchmark score (UGameUserSettings::GetLastGPUBenchmarkResult, 100 = the engine's reference GPU). */
	EHollowGraphicsPreset PresetForBenchmark(float GpuScore) const;

	UPROPERTY(Config, EditAnywhere, Category = "Presets", meta = (TitleProperty = "Preset"))
	TArray<FHollowGraphicsPresetSpec> Presets;

	/** A first launch whose GPU scores below this gets Low. */
	UPROPERTY(Config, EditAnywhere, Category = "First launch", meta = (ClampMin = "0"))
	float LowGpuBenchmarkScore = 0.f;

	/** Steam sets this environment variable to 1 on a Steam Deck: Low at the Deck's own resolution and frame rate. */
	UPROPERTY(Config, EditAnywhere, Category = "Steam Deck")
	FString DeckEnvironmentVariable;

	UPROPERTY(Config, EditAnywhere, Category = "Steam Deck")
	FIntPoint DeckResolution = FIntPoint::ZeroValue;

	UPROPERTY(Config, EditAnywhere, Category = "Steam Deck", meta = (ClampMin = "0"))
	float DeckFrameRateLimit = 0.f;

	/** Console variables Hollow.Graphics.Dump prints (the ones the presets differ in). */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	TArray<FString> DumpConsoleVariables;
};
