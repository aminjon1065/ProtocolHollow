#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Resonance/HollowResonanceTypes.h"

struct FRichCurve;
struct FRuntimeFloatCurve;

/** Pure Resonance math (ADR-0020), no UObjects: tested by Hollow.GAS.ResonanceCurve. */
namespace HollowResonanceMath
{
	/** 0..1 to 0..255, rounded; outside values are clamped. */
	HOLLOWGAS_API uint8 Quantize(float Normalized);
	HOLLOWGAS_API float Dequantize(uint8 Level);

	/**
	 * Tier of a squad at Normalized, coming from Current: it rises as soon as a threshold is reached, falls only once
	 * the value is Hysteresis below it (no flicker at the edge). Thresholds: ascending, one per tier above Low.
	 */
	HOLLOWGAS_API EHollowResonanceTier TierFor(float Normalized, EHollowResonanceTier Current, TConstArrayView<float> Thresholds, float Hysteresis);

	/** Hollow.State.Resonance.<Tier>: the tag every player carries for the squad's tier (S4-08). */
	HOLLOWGAS_API FGameplayTag TierTag(EHollowResonanceTier Tier);

	/** Squad Resonance from the living players' 0..1 values: the highest (solo and co-op alike); 0 without anyone. */
	HOLLOWGAS_API float Aggregate(TConstArrayView<float> LivingNormalized);

	/** A multiplier curve at Normalized (clamped to 0..1); 1 when the curve has no keys. */
	HOLLOWGAS_API float EvaluateMultiplier(const FRuntimeFloatCurve& Curve, float Normalized);

	/** A usable multiplier curve: keys on 0..1, the value at 0 is AtZero, never decreasing, never below AtZero. */
	HOLLOWGAS_API bool IsValidMultiplierCurve(const FRichCurve& Curve, float AtZero, FString& OutProblem);
}
