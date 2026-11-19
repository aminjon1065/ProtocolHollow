#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowToolSet.h"
#include "Components/StaticMeshComponent.h"
#include "Data/HollowToolDefinition.h"
#include "Engine/StaticMesh.h"
#include "GameplayEffect.h"
#include "Tags/HollowGameplayTags.h"
#include "Tools/HollowGA_ContinuousTool.h"
#include "UObject/Package.h"
#include "World/HollowToolTestTarget.h"

namespace HollowToolGameTests
{
	// Content under test (create_tool_assets.py, create_gas_assets.py).
	const TCHAR* const WelderAbilityClass = TEXT("/Game/Hollow/Tools/Welder/GA_Tool_Welder.GA_Tool_Welder_C");
	const TCHAR* const InitEffectClass = TEXT("/Game/Hollow/GAS/Effects/GE_ExoFrame_InitAttributes.GE_ExoFrame_InitAttributes_C");
	const TCHAR* const CubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	/** Loaded up front: a cue class still loading when the test ends would spawn into the next test's world. */
	const TCHAR* const BeamCueClass = TEXT("/Game/Hollow/GAS/Cues/GCN_Tool_Beam.GCN_Tool_Beam_C");

	/** Test data. */
	constexpr float TickSeconds = 1.f / 60.f;
	constexpr float TargetDistance = 100.f;
	constexpr float TargetScale = 0.5f;
	constexpr float WeldSeconds = 1.f;
	constexpr float HeatBelowMax = 0.1f;

	struct FRig
	{
		FTestWorldWrapper World;
		AHollowExoFrameCharacter* Character = nullptr;
		UHollowAbilitySystemComponent* AbilitySystem = nullptr;
		AHollowToolTestTarget* Target = nullptr;
		FGameplayAbilitySpecHandle Welder;
		const UHollowToolDefinition* Tool = nullptr;
	};

	/** One engine frame per step: the timer manager ticks once per GFrameCounter (tool samples, periodic effects). */
	void Advance(FRig& Rig, const float Seconds)
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += TickSeconds)
		{
			++GFrameCounter;
			Rig.World.TickTestWorld(TickSeconds);
		}
	}

	void SetAttribute(const FRig& Rig, const FGameplayAttribute& Attribute, const float Value)
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
		Rig.AbilitySystem->ApplyGameplayEffectToSelf(Effect, 1.f, Rig.AbilitySystem->MakeEffectContext());
	}

	float Attribute(const FRig& Rig, const FGameplayAttribute& Attribute)
	{
		return Rig.AbilitySystem->GetNumericAttribute(Attribute);
	}

	bool IsWelding(const FRig& Rig)
	{
		const FGameplayAbilitySpec* Spec = Rig.AbilitySystem->FindAbilitySpecFromHandle(Rig.Welder);
		return Spec && Spec->IsActive();
	}

	/** ExoFrame possessed by a player, real init attributes, the real welder on SecondaryUse, a target in front of the eyes. */
	bool Build(FAutomationTestBase& Test, FRig& Rig)
	{
		UWorld* World = HollowGameTests::CreateWorld(Test, Rig.World);
		Rig.Character = HollowGameTests::SpawnExoFrame(Test, World);
		const TSubclassOf<UGameplayAbility> WelderClass = LoadClass<UGameplayAbility>(nullptr, WelderAbilityClass);
		const TSubclassOf<UGameplayEffect> InitClass = LoadClass<UGameplayEffect>(nullptr, InitEffectClass);
		const UClass* BeamCue = LoadClass<AActor>(nullptr, BeamCueClass);
		if (!Test.TestNotNull(TEXT("ExoFrame"), Rig.Character) || !Test.TestNotNull(TEXT("GA_Tool_Welder"), WelderClass.Get())
			|| !Test.TestNotNull(TEXT("GE_ExoFrame_InitAttributes"), InitClass.Get()) || !Test.TestNotNull(TEXT("GCN_Tool_Beam"), BeamCue))
		{
			return false;
		}
		// A local player sets up input; the bare C++ class has no InputConfig (BP_ExoFrame data), which it reports.
		Test.AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
		APlayerController* Controller = HollowGameTests::PossessAsPlayer(World, Rig.Character, /*bLocalPlayer=*/true);
		Controller->SetControlRotation(FRotator::ZeroRotator);
		Rig.AbilitySystem = Cast<UHollowAbilitySystemComponent>(Rig.Character->GetAbilitySystemComponent());
		if (!Test.TestNotNull(TEXT("Ability system on the player state"), Rig.AbilitySystem))
		{
			return false;
		}
		Rig.AbilitySystem->ApplyGameplayEffectToSelf(InitClass.GetDefaultObject(), 1.f, Rig.AbilitySystem->MakeEffectContext());

		FGameplayAbilitySpec Spec(WelderClass, 1);
		Spec.GetDynamicSpecSourceTags().AddTag(HollowTags::Input_SecondaryUse);
		Rig.Welder = Rig.AbilitySystem->GiveAbility(Spec);
		Rig.Tool = CastChecked<UHollowGA_ContinuousTool>(WelderClass.GetDefaultObject())->GetToolDefinition();

		FVector Eyes;
		FRotator View;
		Rig.Character->GetActorEyesViewPoint(Eyes, View);
		Rig.Target = World->SpawnActor<AHollowToolTestTarget>(AHollowToolTestTarget::StaticClass(), FTransform(Eyes + FVector::ForwardVector * TargetDistance));
		UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Rig.Target->GetRootComponent());
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, CubeMeshPath));
		Mesh->SetWorldScale3D(FVector(TargetScale));
		return Test.TestNotNull(TEXT("Welder definition"), Rig.Tool);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowToolWorkOnTargetTest, "Hollow.Game.Tool.WorkOnTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowToolWorkOnTargetTest::RunTest(const FString& Parameters)
{
	using namespace HollowToolGameTests;
	FRig Rig;
	if (!Build(*this, Rig))
	{
		return false;
	}
	const FGameplayTag Input = HollowTags::Input_SecondaryUse;
	const FGameplayTag ToolTag = Rig.Tool->ToolTag;

	// Hold for a second: samples at SampleRateHz, work for the server time.
	Rig.AbilitySystem->AbilityInputTagPressed(Input);
	TestTrue(TEXT("Pressing the input starts the welder"), IsWelding(Rig));
	Advance(Rig, WeldSeconds);
	const int32 Samples = Rig.Target->GetSamples(ToolTag);
	const float Work = Rig.Target->GetTotalWork(ToolTag);
	const int32 ExpectedSamples = FMath::RoundToInt(Rig.Tool->SampleRateHz * WeldSeconds);
	TestTrue(FString::Printf(TEXT("Samples at SampleRateHz (%d, expected %d +- 1)"), Samples, ExpectedSamples), FMath::Abs(Samples - ExpectedSamples) <= 1);
	const float WorkPerSample = Rig.Tool->WorkRate / Rig.Tool->SampleRateHz;
	TestTrue(FString::Printf(TEXT("Work follows WorkRate x time (%.2f)"), Work), FMath::IsNearlyEqual(Work, Samples * WorkPerSample, WorkPerSample * 0.5f));
	TestTrue(TEXT("Fuel was used"), Attribute(Rig, UHollowToolSet::GetToolFuelAttribute()) < Attribute(Rig, UHollowToolSet::GetMaxToolFuelAttribute()));

	Rig.AbilitySystem->AbilityInputTagReleased(Input);
	TestFalse(TEXT("Releasing the input stops the welder"), IsWelding(Rig));
	Advance(Rig, WeldSeconds * 0.5f);
	TestEqual(TEXT("No work after release"), Rig.Target->GetSamples(ToolTag), Samples);

	// Overheat: the next sample reaches MaxToolHeat, the tool stops and stays blocked.
	SetAttribute(Rig, UHollowToolSet::GetToolHeatAttribute(), Attribute(Rig, UHollowToolSet::GetMaxToolHeatAttribute()) - HeatBelowMax);
	Rig.AbilitySystem->AbilityInputTagPressed(Input);
	Advance(Rig, 2.f / Rig.Tool->SampleRateHz);
	TestFalse(TEXT("Overheating stops the welder"), IsWelding(Rig));
	TestTrue(TEXT("...and marks the tool overheated"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_Tool_Overheated));
	Rig.AbilitySystem->AbilityInputTagReleased(Input);
	Rig.AbilitySystem->AbilityInputTagPressed(Input);
	TestFalse(TEXT("An overheated tool cannot start"), IsWelding(Rig));
	Rig.AbilitySystem->AbilityInputTagReleased(Input);

	// Wait the overheat out, then run dry: the welder ends itself at zero fuel.
	float OverheatSeconds = 0.f;
	Rig.Tool->OverheatEffect.GetDefaultObject()->DurationMagnitude.GetStaticMagnitudeIfPossible(1.f, OverheatSeconds);
	Advance(Rig, OverheatSeconds + WeldSeconds * 0.1f);
	SetAttribute(Rig, UHollowToolSet::GetToolHeatAttribute(), 0.f);
	SetAttribute(Rig, UHollowToolSet::GetToolFuelAttribute(), Rig.Tool->MinFuelToStart * 1.5f);
	Rig.AbilitySystem->AbilityInputTagPressed(Input);
	TestTrue(TEXT("The welder starts with enough fuel"), IsWelding(Rig));
	Advance(Rig, WeldSeconds);
	TestEqual(TEXT("Fuel runs out"), Attribute(Rig, UHollowToolSet::GetToolFuelAttribute()), 0.f);
	TestFalse(TEXT("...and the welder stops by itself"), IsWelding(Rig));
	Rig.AbilitySystem->AbilityInputTagReleased(Input);
	Rig.AbilitySystem->AbilityInputTagPressed(Input);
	TestFalse(TEXT("Without fuel it does not start"), IsWelding(Rig));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
