#include "Effects/HollowResonanceGainCalculation.h"

#include "Attributes/HollowResonanceSet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowResonanceGainCalculation)

UHollowResonanceGainCalculation::UHollowResonanceGainCalculation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, GainMultiplier(UHollowResonanceSet::GetResonanceGainMultiplierAttribute(), EGameplayEffectAttributeCaptureSource::Target, /*bSnapshot=*/false)
{
	RelevantAttributesToCapture.Add(GainMultiplier);
}

float UHollowResonanceGainCalculation::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	FAggregatorEvaluateParameters Parameters;
	Parameters.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Parameters.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	float Multiplier = 1.f;
	GetCapturedAttributeMagnitude(GainMultiplier, Spec, Parameters, Multiplier);
	return FMath::Max(Multiplier, 0.f);
}
