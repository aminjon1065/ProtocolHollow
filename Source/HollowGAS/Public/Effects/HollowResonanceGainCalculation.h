#pragma once

#include "GameplayModMagnitudeCalculation.h"

#include "HollowResonanceGainCalculation.generated.h"

/**
 * Magnitude of a Resonance gain (ADR-0020): the modifier's coefficient (data, per application) times the target's
 * ResonanceGainMultiplier, live (not snapshotted: a changed multiplier counts from the next period). Gains go through
 * effects with this calculation (GE_Carry_Solo), never straight into the attribute.
 */
UCLASS()
class HOLLOWGAS_API UHollowResonanceGainCalculation : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	explicit UHollowResonanceGainCalculation(const FObjectInitializer& ObjectInitializer);

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	FGameplayEffectAttributeCaptureDefinition GainMultiplier;
};
