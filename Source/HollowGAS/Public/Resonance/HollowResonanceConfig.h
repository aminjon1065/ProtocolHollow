#pragma once

#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"

#include "HollowResonanceConfig.generated.h"

/** Squad Resonance tuning (ADR-0020): tiers and how often the server looks (DA_ResonanceConfig). */
UCLASS(BlueprintType)
class HOLLOWGAS_API UHollowResonanceConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The config named by UHollowResonanceSettings (loaded on first use), or null. */
	static const UHollowResonanceConfig* Get();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** Squad Resonance (0..1) at which Medium, High and Critical start; ascending. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tiers", meta = (ClampMin = "0", ClampMax = "1"))
	TArray<float> TierThresholds;

	/** A tier ends only this far below its threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tiers", meta = (ClampMin = "0", ClampMax = "1"))
	float Hysteresis = 0.f;

	/** How often the server aggregates the squad (the replicated value changes no faster). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Network", meta = (ClampMin = "0.05", Units = "Seconds"))
	float UpdateSeconds = 0.f;
};

/** Which Resonance config the game uses (DefaultGame.ini). */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Resonance"))
class HOLLOWGAS_API UHollowResonanceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Resonance")
	TSoftObjectPtr<UHollowResonanceConfig> Config;
};
