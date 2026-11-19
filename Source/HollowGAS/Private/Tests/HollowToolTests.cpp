#include "Tests/HollowGASTestRig.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Attributes/HollowToolSet.h"
#include "Data/HollowToolDefinition.h"
#include "Tags/HollowGameplayTags.h"
#include "Tools/HollowToolTargeting.h"

namespace HollowToolTests
{
	// Content under test (S2-03, create_tool_assets.py): the real effects and the welder's numbers.
	const TCHAR* const FuelDrainClass = TEXT("/Game/Hollow/GAS/Effects/GE_Tool_FuelDrain.GE_Tool_FuelDrain_C");
	const TCHAR* const HeatClass = TEXT("/Game/Hollow/GAS/Effects/GE_Tool_Heat.GE_Tool_Heat_C");
	const TCHAR* const OverheatClass = TEXT("/Game/Hollow/GAS/Effects/GE_Tool_Overheat.GE_Tool_Overheat_C");
	const TCHAR* const CoolingClass = TEXT("/Game/Hollow/GAS/Effects/GE_Tool_Cooling.GE_Tool_Cooling_C");
	const TCHAR* const WelderDefinition = TEXT("/Game/Hollow/Tools/Welder/DA_Tool_Welder.DA_Tool_Welder");

	/** Test data. */
	constexpr float Full = 100.f;
	constexpr float TickSeconds = 0.025f;
	constexpr float AlmostEmpty = 2.f;

	TSubclassOf<UGameplayEffect> LoadEffect(const TCHAR* Path)
	{
		return LoadClass<UGameplayEffect>(nullptr, Path);
	}

	FActiveGameplayEffectHandle ApplyWithSetByCaller(const FHollowGASTestRig& Rig, const TSubclassOf<UGameplayEffect>& Effect, const FGameplayTag& Tag, const float Value)
	{
		const FGameplayEffectSpecHandle Spec = Rig.AbilitySystem->MakeOutgoingSpec(Effect, 1.f, Rig.AbilitySystem->MakeEffectContext());
		if (Tag.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(Tag, Value);
		}
		return Rig.AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}

	/** One engine frame per step: the timer manager ticks once per GFrameCounter (periodic and timed effects run on it). */
	void Advance(FHollowGASTestRig& Rig, const float Seconds)
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += TickSeconds)
		{
			++GFrameCounter;
			Rig.World.TickTestWorld(TickSeconds);
		}
	}

	bool FillTool(FAutomationTestBase& Test, FHollowGASTestRig& Rig)
	{
		if (!Rig.Create<UHollowToolSet>(Test))
		{
			return false;
		}
		Rig.Apply(UHollowToolSet::GetMaxToolFuelAttribute(), EGameplayModOp::Override, Full);
		Rig.Apply(UHollowToolSet::GetToolFuelAttribute(), EGameplayModOp::Override, Full);
		Rig.Apply(UHollowToolSet::GetMaxToolHeatAttribute(), EGameplayModOp::Override, Full);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowToolTargetingTest, "Hollow.GAS.ToolTargeting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowToolTargetingTest::RunTest(const FString& Parameters)
{
	constexpr float MaxAngle = 15.f;
	constexpr float Grace = 0.3f;
	using EResult = EHollowToolAimResult;
	auto Resolve = [](const bool bHit, const bool bCurrent, const float Angle, const double Since)
	{
		FHollowToolAim Aim;
		Aim.bHitTarget = bHit;
		Aim.bHasCurrent = bCurrent;
		Aim.AngleToCurrentDegrees = Angle;
		Aim.SecondsSinceCurrentHit = Since;
		return HollowToolTargeting::Resolve(Aim, MaxAngle, Grace);
	};

	TestTrue(TEXT("A hit always works on the hit target"), Resolve(true, true, 90.f, 9.0) == EResult::WorkOnHit);
	TestTrue(TEXT("A short miss keeps the current target"), Resolve(false, true, 5.f, 0.1) == EResult::KeepCurrent);
	TestTrue(TEXT("The angle limit is inclusive"), Resolve(false, true, MaxAngle, 0.1) == EResult::KeepCurrent);
	TestTrue(TEXT("The grace limit is inclusive"), Resolve(false, true, 5.f, Grace) == EResult::KeepCurrent);
	TestTrue(TEXT("Looking away loses the target"), Resolve(false, true, MaxAngle + 1.f, 0.1) == EResult::Lose);
	TestTrue(TEXT("A long miss loses the target"), Resolve(false, true, 5.f, Grace + 0.01) == EResult::Lose);
	TestTrue(TEXT("Without a target a miss does nothing"), Resolve(false, false, 0.f, 0.0) == EResult::Lose);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowToolFuelTest, "Hollow.GAS.ToolFuel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowToolFuelTest::RunTest(const FString& Parameters)
{
	using namespace HollowToolTests;
	const TSubclassOf<UGameplayEffect> Drain = LoadEffect(FuelDrainClass);
	const UHollowToolDefinition* Welder = LoadObject<UHollowToolDefinition>(nullptr, WelderDefinition);
	FHollowGASTestRig Rig;
	if (!TestNotNull(TEXT("GE_Tool_FuelDrain"), Drain.Get()) || !TestNotNull(TEXT("DA_Tool_Welder"), Welder) || !FillTool(*this, Rig))
	{
		return false;
	}

	// Exactly what UHollowGA_ContinuousTool applies: cost per period from the welder's fuel per second.
	const float Period = Drain.GetDefaultObject()->Period.GetValue();
	const float CostPerPeriod = Welder->FuelPerSecond * Period;
	const FActiveGameplayEffectHandle Handle = ApplyWithSetByCaller(Rig, Drain, HollowTags::Data_FuelCost, -CostPerPeriod);
	constexpr float Seconds = 1.f;
	Advance(Rig, Seconds);
	// One execution on application, then one per period.
	const float Expected = Full - CostPerPeriod * (1.f + FMath::FloorToFloat(Seconds / Period));
	const float Fuel = Rig.Value(UHollowToolSet::GetToolFuelAttribute());
	TestTrue(FString::Printf(TEXT("Fuel drains at FuelPerSecond (%.2f, expected %.2f +- one period)"), Fuel, Expected), FMath::Abs(Fuel - Expected) <= CostPerPeriod + KINDA_SMALL_NUMBER);

	Rig.AbilitySystem->RemoveActiveGameplayEffect(Handle);
	Advance(Rig, Seconds);
	TestEqual(TEXT("Removing the drain stops it"), Rig.Value(UHollowToolSet::GetToolFuelAttribute()), Fuel);

	Rig.Apply(UHollowToolSet::GetToolFuelAttribute(), EGameplayModOp::Override, AlmostEmpty);
	ApplyWithSetByCaller(Rig, Drain, HollowTags::Data_FuelCost, -CostPerPeriod);
	Advance(Rig, Seconds * 2.f);
	TestEqual(TEXT("Fuel stops at zero, never below"), Rig.Value(UHollowToolSet::GetToolFuelAttribute()), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowToolHeatTest, "Hollow.GAS.ToolHeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowToolHeatTest::RunTest(const FString& Parameters)
{
	using namespace HollowToolTests;
	const TSubclassOf<UGameplayEffect> Heat = LoadEffect(HeatClass);
	const TSubclassOf<UGameplayEffect> Overheat = LoadEffect(OverheatClass);
	const TSubclassOf<UGameplayEffect> Cooling = LoadEffect(CoolingClass);
	const UHollowToolDefinition* Welder = LoadObject<UHollowToolDefinition>(nullptr, WelderDefinition);
	FHollowGASTestRig Rig;
	if (!TestNotNull(TEXT("GE_Tool_Heat"), Heat.Get()) || !TestNotNull(TEXT("GE_Tool_Overheat"), Overheat.Get())
		|| !TestNotNull(TEXT("GE_Tool_Cooling"), Cooling.Get()) || !TestNotNull(TEXT("DA_Tool_Welder"), Welder) || !FillTool(*this, Rig))
	{
		return false;
	}

	ApplyWithSetByCaller(Rig, Heat, HollowTags::Data_HeatGain, Welder->HeatPerSample);
	ApplyWithSetByCaller(Rig, Heat, HollowTags::Data_HeatGain, Welder->HeatPerSample);
	TestEqual(TEXT("Every sample adds HeatPerSample"), Rig.Value(UHollowToolSet::GetToolHeatAttribute()), 2.f * Welder->HeatPerSample);
	ApplyWithSetByCaller(Rig, Heat, HollowTags::Data_HeatGain, Full * 2.f);
	TestEqual(TEXT("Heat is clamped to MaxToolHeat"), Rig.Value(UHollowToolSet::GetToolHeatAttribute()), Full);

	float OverheatSeconds = 0.f;
	TestTrue(TEXT("Overheat has a static duration"), Overheat.GetDefaultObject()->DurationMagnitude.GetStaticMagnitudeIfPossible(1.f, OverheatSeconds));
	ApplyWithSetByCaller(Rig, Overheat, FGameplayTag(), 0.f);
	TestTrue(TEXT("Overheat grants Hollow.State.Tool.Overheated"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Tool_Overheated));
	Advance(Rig, OverheatSeconds * 0.5f);
	TestTrue(TEXT("...for its whole duration"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Tool_Overheated));
	Advance(Rig, OverheatSeconds * 0.5f + 2.f * TickSeconds);
	TestFalse(TEXT("...and then lets the tools work again"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Tool_Overheated));

	// Cooling (passive, granted with the ExoFrame's ability set): constant rate, never below zero.
	const float CoolingPeriod = Cooling.GetDefaultObject()->Period.GetValue();
	float CoolingPerPeriod = 0.f;
	TestTrue(TEXT("Cooling has a static magnitude"), Cooling.GetDefaultObject()->Modifiers.Num() == 1
		&& Cooling.GetDefaultObject()->Modifiers[0].ModifierMagnitude.GetStaticMagnitudeIfPossible(1.f, CoolingPerPeriod) && CoolingPerPeriod < 0.f);
	const float Before = Rig.Value(UHollowToolSet::GetToolHeatAttribute());
	ApplyWithSetByCaller(Rig, Cooling, FGameplayTag(), 0.f);
	constexpr float Seconds = 1.f;
	Advance(Rig, Seconds);
	const float Expected = Before + CoolingPerPeriod * (1.f + FMath::FloorToFloat(Seconds / CoolingPeriod));
	const float Cooled = Rig.Value(UHollowToolSet::GetToolHeatAttribute());
	TestTrue(FString::Printf(TEXT("Cooling lowers heat (%.2f, expected %.2f +- one period)"), Cooled, Expected), FMath::Abs(Cooled - Expected) <= -CoolingPerPeriod + KINDA_SMALL_NUMBER);
	Advance(Rig, Full / -CoolingPerPeriod * CoolingPeriod);
	TestEqual(TEXT("Heat cools down to zero, not below"), Rig.Value(UHollowToolSet::GetToolHeatAttribute()), 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
