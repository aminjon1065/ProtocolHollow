#include "Resonance/HollowResonanceMath.h"

#include "Curves/CurveFloat.h"

namespace HollowResonanceMathPrivate
{
	constexpr float LevelSteps = 255.f;
	/** Samples between keys when checking that a curve never decreases (cubic keys can overshoot). */
	constexpr int32 SamplesPerSegment = 16;
	constexpr float Tolerance = 1.0e-4f;
}

uint8 HollowResonanceMath::Quantize(const float Normalized)
{
	return static_cast<uint8>(FMath::RoundToInt32(FMath::Clamp(Normalized, 0.f, 1.f) * HollowResonanceMathPrivate::LevelSteps));
}

float HollowResonanceMath::Dequantize(const uint8 Level)
{
	return static_cast<float>(Level) / HollowResonanceMathPrivate::LevelSteps;
}

EHollowResonanceTier HollowResonanceMath::TierFor(const float Normalized, const EHollowResonanceTier Current, const TConstArrayView<float> Thresholds,
	const float Hysteresis)
{
	const int32 Highest = FMath::Min(Thresholds.Num(), static_cast<int32>(EHollowResonanceTier::Critical));
	int32 Tier = FMath::Min(static_cast<int32>(Current), Highest);
	while (Tier < Highest && Normalized >= Thresholds[Tier])
	{
		++Tier;
	}
	while (Tier > 0 && Normalized < Thresholds[Tier - 1] - Hysteresis)
	{
		--Tier;
	}
	return static_cast<EHollowResonanceTier>(Tier);
}

float HollowResonanceMath::Aggregate(const TConstArrayView<float> LivingNormalized)
{
	float Highest = 0.f;
	for (const float Value : LivingNormalized)
	{
		Highest = FMath::Max(Highest, FMath::Clamp(Value, 0.f, 1.f));
	}
	return Highest;
}

float HollowResonanceMath::EvaluateMultiplier(const FRuntimeFloatCurve& Curve, const float Normalized)
{
	const FRichCurve* Rich = Curve.GetRichCurveConst();
	return Rich && Rich->GetNumKeys() > 0 ? Rich->Eval(FMath::Clamp(Normalized, 0.f, 1.f)) : 1.f;
}

bool HollowResonanceMath::IsValidMultiplierCurve(const FRichCurve& Curve, const float AtZero, FString& OutProblem)
{
	using namespace HollowResonanceMathPrivate;
	if (Curve.GetNumKeys() == 0)
	{
		OutProblem = TEXT("no keys");
		return false;
	}
	float MinTime = 0.f;
	float MaxTime = 0.f;
	Curve.GetTimeRange(MinTime, MaxTime);
	if (MinTime < -Tolerance || MaxTime > 1.f + Tolerance)
	{
		OutProblem = FString::Printf(TEXT("keys outside 0..1 (%.3f..%.3f)"), MinTime, MaxTime);
		return false;
	}
	if (!FMath::IsNearlyEqual(Curve.Eval(0.f), AtZero, Tolerance))
	{
		OutProblem = FString::Printf(TEXT("%.3f at 0 instead of %.3f"), Curve.Eval(0.f), AtZero);
		return false;
	}
	const int32 Samples = FMath::Max(Curve.GetNumKeys(), 2) * SamplesPerSegment;
	float Previous = Curve.Eval(0.f);
	for (int32 Sample = 1; Sample <= Samples; ++Sample)
	{
		const float Time = static_cast<float>(Sample) / static_cast<float>(Samples);
		const float Value = Curve.Eval(Time);
		if (Value < Previous - Tolerance || Value < AtZero - Tolerance)
		{
			OutProblem = FString::Printf(TEXT("decreases at %.3f (%.3f after %.3f)"), Time, Value, Previous);
			return false;
		}
		Previous = Value;
	}
	return true;
}
