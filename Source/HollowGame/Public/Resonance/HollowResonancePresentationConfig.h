#pragma once

#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"

#include "HollowResonancePresentationConfig.generated.h"

class UMaterialParameterCollection;
class USoundControlBus;

/**
 * How the squad's Resonance looks and sounds on every machine (ADR-0020, DA_ResonancePresentation): the material
 * parameter collection the post-process reads, the Audio Modulation bus, and how fast they follow the replicated value.
 */
UCLASS(BlueprintType)
class HOLLOWGAME_API UHollowResonancePresentationConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The config named by UHollowResonancePresentationSettings (loaded on first use), or null. */
	static const UHollowResonancePresentationConfig* Get();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** MPC_Resonance: read by M_PP_Resonance on the player's camera. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UMaterialParameterCollection> Collection;

	/** Scalar 0..1: strength of every distortion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FName IntensityParameter;

	/** Scalar: server time minus this machine's world time, so the flicker is in phase on every screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FName TimeOffsetParameter;

	/** CB_Resonance: the sound classes it modulates muffle as Resonance rises. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundControlBus> Bus;

	/** Bus value (0..1, the bus parameter's units) without Resonance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0", ClampMax = "1"))
	float BusValueCalm = 0.f;

	/** Bus value at full Resonance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0", ClampMax = "1"))
	float BusValueFull = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0", Units = "Seconds"))
	float BusFadeSeconds = 0.f;

	/** The shown intensity goes from 0 to 1 in this time (linear), so a jump of the squad value never pops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smoothing", meta = (ClampMin = "0.01", Units = "Seconds"))
	float EaseSeconds = 0.f;

	/** Step of the easing timer; it runs only while the shown value moves. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Smoothing", meta = (ClampMin = "0.01", Units = "Seconds"))
	float StepSeconds = 0.f;
};

/** Which Resonance presentation the game uses (DefaultGame.ini). */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hollow Resonance Presentation"))
class HOLLOWGAME_API UHollowResonancePresentationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Resonance")
	TSoftObjectPtr<UHollowResonancePresentationConfig> Presentation;
};
