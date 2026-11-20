#include "Tests/HollowGASTestRig.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Attributes/HollowResonanceSet.h"
#include "Curves/CurveFloat.h"
#include "Effects/HollowResonanceGainCalculation.h"
#include "Resonance/HollowResonanceMath.h"

namespace HollowResonanceTests
{
	// Test data (the shipped tiers live in DA_ResonanceConfig, the curves in their owners' data assets).
	const TArray<float> Thresholds = {0.25f, 0.5f, 0.75f};
	constexpr float Hysteresis = 0.03f;
	constexpr float Max = 100.f;
	constexpr float Coefficient = 2.f;
	constexpr float Boosted = 1.5f;
	constexpr float Period = 1.f;
	constexpr float TickSeconds = 1.f / 60.f;
	constexpr float Tolerance = 1.0e-3f;

	FRuntimeFloatCurve Curve(const TArray<TPair<float, float>>& Keys)
	{
		FRuntimeFloatCurve Result;
		for (const TPair<float, float>& Key : Keys)
		{
			Result.GetRichCurve()->AddKey(Key.Key, Key.Value);
		}
		for (FRichCurveKey& Key : Result.GetRichCurve()->Keys)
		{
			Key.InterpMode = RCIM_Linear;
		}
		return Result;
	}

	EHollowResonanceTier Tier(const float Value, const EHollowResonanceTier From)
	{
		return HollowResonanceMath::TierFor(Value, From, Thresholds, Hysteresis);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowResonanceCurveTest, "Hollow.GAS.ResonanceCurve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowResonanceCurveTest::RunTest(const FString& Parameters)
{
	// Squad Resonance math (ADR-0020): the byte the server sends, the tier with hysteresis, the squad's value, and the
	// multiplier curves (the stalker's hearing) and their validation.
	using namespace HollowResonanceTests;
	using ETier = EHollowResonanceTier;

	// Quantization: every byte survives the round trip; any value comes back within half a step.
	TestEqual(TEXT("0 -> 0"), HollowResonanceMath::Quantize(0.f), static_cast<uint8>(0));
	TestEqual(TEXT("1 -> 255"), HollowResonanceMath::Quantize(1.f), static_cast<uint8>(255));
	TestEqual(TEXT("Below 0 clamps"), HollowResonanceMath::Quantize(-1.f), static_cast<uint8>(0));
	TestEqual(TEXT("Above 1 clamps"), HollowResonanceMath::Quantize(2.f), static_cast<uint8>(255));
	bool bRoundTrip = true;
	for (int32 Level = 0; Level <= MAX_uint8; ++Level)
	{
		bRoundTrip &= HollowResonanceMath::Quantize(HollowResonanceMath::Dequantize(static_cast<uint8>(Level))) == Level;
	}
	TestTrue(TEXT("Every level survives the round trip"), bRoundTrip);
	float WorstError = 0.f;
	for (int32 Step = 0; Step <= 1000; ++Step)
	{
		const float Value = static_cast<float>(Step) / 1000.f;
		WorstError = FMath::Max(WorstError, FMath::Abs(HollowResonanceMath::Dequantize(HollowResonanceMath::Quantize(Value)) - Value));
	}
	TestTrue(FString::Printf(TEXT("Within half a step (worst %.5f)"), WorstError), WorstError <= 0.5f / 255.f + KINDA_SMALL_NUMBER);

	// Tiers: up at the threshold, down only past the hysteresis, several at once.
	TestTrue(TEXT("Below the first threshold: Low"), Tier(0.2f, ETier::Low) == ETier::Low);
	TestTrue(TEXT("At it: Medium"), Tier(0.25f, ETier::Low) == ETier::Medium);
	TestTrue(TEXT("Straight to Critical"), Tier(0.8f, ETier::Low) == ETier::Critical);
	TestTrue(TEXT("Just under High's threshold from High: still High"), Tier(0.5f - Hysteresis * 0.5f, ETier::High) == ETier::High);
	TestTrue(TEXT("Past the hysteresis: Medium"), Tier(0.5f - Hysteresis * 1.5f, ETier::High) == ETier::Medium);
	TestTrue(TEXT("Critical to Low at once"), Tier(0.1f, ETier::Critical) == ETier::Low);
	EHollowResonanceTier Edge = ETier::High;
	for (int32 Wobble = 0; Wobble < 10; ++Wobble)
	{
		Edge = Tier(Wobble % 2 == 0 ? 0.49f : 0.51f, Edge);
	}
	TestTrue(TEXT("Wobbling round a threshold does not flicker"), Edge == ETier::High);
	TestTrue(TEXT("Without thresholds: Low"), HollowResonanceMath::TierFor(1.f, ETier::Critical, {}, Hysteresis) == ETier::Low);

	// The squad: its most exposed living player.
	TestEqual(TEXT("Nobody: 0"), HollowResonanceMath::Aggregate({}), 0.f);
	TestEqual(TEXT("The highest"), HollowResonanceMath::Aggregate({0.2f, 0.7f, 0.4f}), 0.7f);
	TestEqual(TEXT("Clamped"), HollowResonanceMath::Aggregate({1.5f}), 1.f);

	// Multiplier curves.
	const FRuntimeFloatCurve Hearing = Curve({{0.f, 1.f}, {0.5f, 1.6f}, {1.f, 2.f}});
	TestEqual(TEXT("No keys: x1"), HollowResonanceMath::EvaluateMultiplier(FRuntimeFloatCurve(), 0.7f), 1.f);
	TestNearlyEqual(TEXT("At a key"), HollowResonanceMath::EvaluateMultiplier(Hearing, 0.5f), 1.6f, Tolerance);
	TestNearlyEqual(TEXT("Between keys"), HollowResonanceMath::EvaluateMultiplier(Hearing, 0.75f), 1.8f, Tolerance);
	TestNearlyEqual(TEXT("Clamped above 1"), HollowResonanceMath::EvaluateMultiplier(Hearing, 3.f), 2.f, Tolerance);
	TestNearlyEqual(TEXT("Clamped below 0"), HollowResonanceMath::EvaluateMultiplier(Hearing, -1.f), 1.f, Tolerance);
	FString Problem;
	TestTrue(TEXT("A rising curve from x1 is valid"), HollowResonanceMath::IsValidMultiplierCurve(*Hearing.GetRichCurveConst(), 1.f, Problem));
	TestFalse(TEXT("A dip is not"), HollowResonanceMath::IsValidMultiplierCurve(*Curve({{0.f, 1.f}, {0.5f, 2.f}, {1.f, 1.5f}}).GetRichCurveConst(), 1.f, Problem));
	TestFalse(TEXT("Nor a wrong start"), HollowResonanceMath::IsValidMultiplierCurve(*Curve({{0.f, 1.2f}, {1.f, 2.f}}).GetRichCurveConst(), 1.f, Problem));
	TestFalse(TEXT("Nor keys past 1"), HollowResonanceMath::IsValidMultiplierCurve(*Curve({{0.f, 1.f}, {2.f, 2.f}}).GetRichCurveConst(), 1.f, Problem));
	TestFalse(TEXT("Nor no keys"), HollowResonanceMath::IsValidMultiplierCurve(*FRuntimeFloatCurve().GetRichCurveConst(), 1.f, Problem));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowResonanceGainTest, "Hollow.GAS.Resonance.Gain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowResonanceGainTest::RunTest(const FString& Parameters)
{
	// UHollowResonanceGainCalculation: a periodic gain is its coefficient times the target's ResonanceGainMultiplier,
	// live (a new multiplier counts from the next period), and Resonance stops at MaxResonance.
	using namespace HollowResonanceTests;
	FHollowGASTestRig Rig;
	if (!Rig.Create<UHollowResonanceSet>(*this))
	{
		return false;
	}
	Rig.Apply(UHollowResonanceSet::GetMaxResonanceAttribute(), EGameplayModOp::Override, Max);
	Rig.Apply(UHollowResonanceSet::GetResonanceGainMultiplierAttribute(), EGameplayModOp::Override, 1.f);

	UGameplayEffect* Gain = NewObject<UGameplayEffect>(GetTransientPackage());
	Gain->DurationPolicy = EGameplayEffectDurationType::Infinite;
	Gain->Period = FScalableFloat(Period);
	Gain->bExecutePeriodicEffectOnApplication = false;
	FCustomCalculationBasedFloat Calculation;
	Calculation.CalculationClassMagnitude = UHollowResonanceGainCalculation::StaticClass();
	Calculation.Coefficient = FScalableFloat(Coefficient);
	FGameplayModifierInfo& Modifier = Gain->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UHollowResonanceSet::GetResonanceAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Calculation);
	Rig.AbilitySystem->ApplyGameplayEffectToSelf(Gain, 1.f, Rig.AbilitySystem->MakeEffectContext());

	auto Advance = [&Rig](const float Seconds)
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += TickSeconds)
		{
			++GFrameCounter;
			Rig.World.TickTestWorld(TickSeconds);
		}
	};
	auto Resonance = [&Rig] { return Rig.Value(UHollowResonanceSet::GetResonanceAttribute()); };
	Advance(Period * 0.5f);
	TestEqual(TEXT("Nothing before the first period"), Resonance(), 0.f);
	Advance(Period);
	TestNearlyEqual(TEXT("A period: the coefficient (multiplier 1)"), Resonance(), Coefficient, Tolerance);
	Rig.Apply(UHollowResonanceSet::GetResonanceGainMultiplierAttribute(), EGameplayModOp::Override, Boosted);
	Advance(Period);
	TestNearlyEqual(TEXT("A raised multiplier counts from the next period"), Resonance(), Coefficient + Coefficient * Boosted, Tolerance);
	Rig.Apply(UHollowResonanceSet::GetResonanceGainMultiplierAttribute(), EGameplayModOp::Override, 0.f);
	Advance(Period);
	TestNearlyEqual(TEXT("Multiplier 0: immune"), Resonance(), Coefficient + Coefficient * Boosted, Tolerance);
	Rig.Apply(UHollowResonanceSet::GetResonanceGainMultiplierAttribute(), EGameplayModOp::Override, 1.f);
	Rig.Apply(UHollowResonanceSet::GetResonanceAttribute(), EGameplayModOp::Override, Max - Coefficient * 0.5f);
	Advance(Period);
	TestEqual(TEXT("Never above MaxResonance"), Resonance(), Max);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
