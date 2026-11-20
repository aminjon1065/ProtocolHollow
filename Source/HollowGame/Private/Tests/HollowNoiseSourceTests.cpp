#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Carry/HollowAetherCore.h"
#include "Carry/HollowCarrierComponent.h"
#include "Carry/HollowCarryableComponent.h"
#include "Character/HollowFootstepNoiseComponent.h"
#include "Components/CapsuleComponent.h"
#include "Doors/HollowDoor.h"
#include "Doors/HollowDoorDefinition.h"
#include "Doors/HollowDoorStateComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "Interfaces/HollowStructural.h"
#include "Noise/HollowNoiseConfig.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Tags/HollowGameplayTags.h"

namespace HollowNoiseSourceTests
{
	// Content under test (create_tool_assets.py, create_carry_assets.py, create_door_assets.py, create_noise_assets.py).
	const TCHAR* const WelderAbilityClass = TEXT("/Game/Hollow/Tools/Welder/GA_Tool_Welder.GA_Tool_Welder_C");
	const TCHAR* const InitEffectClass = TEXT("/Game/Hollow/GAS/Effects/GE_ExoFrame_InitAttributes.GE_ExoFrame_InitAttributes_C");
	const TCHAR* const BeamCueClass = TEXT("/Game/Hollow/GAS/Cues/GCN_Tool_Beam.GCN_Tool_Beam_C");   // loaded up front, not into a later test's world
	const TCHAR* const CoreClass = TEXT("/Game/Hollow/Items/AetherCore/BP_AetherCore.BP_AetherCore_C");
	const TCHAR* const DoorClass = TEXT("/Game/Hollow/World/Kit/Doors/BP_Door.BP_Door_C");

	// Test data.
	constexpr float TickSeconds = 1.f / 60.f;
	constexpr float WeldSeconds = 1.f;
	constexpr float StructuralBlow = 120.f;
	const FVector CoreSpot(150.0, 0.0, 0.0);
	const FVector DoorSpot(0.0, 600.0, 0.0);

	void Advance(FTestWorldWrapper& World, const float Seconds)
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += TickSeconds)
		{
			++GFrameCounter;
			World.TickTestWorld(TickSeconds);
		}
	}

	int32 Count(const TArray<FHollowNoiseReport>& Reports, const FGameplayTag& Tag)
	{
		return Reports.FilterByPredicate([&Tag](const FHollowNoiseReport& Report) { return Report.Event.NoiseTag == Tag; }).Num();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowNoiseSourcesTest, "Hollow.Game.Noise.Sources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowNoiseSourcesTest::RunTest(const FString& Parameters)
{
	// Every S3-05 source reports through UHollowNoiseSubsystem with the right tag, place and instigator; the numbers
	// come from DA_NoiseConfig (ADR-0018).
	using namespace HollowNoiseSourceTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	AHollowExoFrameCharacter* Character = HollowGameTests::SpawnExoFrame(*this, World);
	const TSubclassOf<UGameplayAbility> WelderClass = LoadClass<UGameplayAbility>(nullptr, WelderAbilityClass);
	const TSubclassOf<UGameplayEffect> InitClass = LoadClass<UGameplayEffect>(nullptr, InitEffectClass);
	const TSubclassOf<AHollowAetherCore> CoreBlueprint = LoadClass<AHollowAetherCore>(nullptr, CoreClass);
	const TSubclassOf<AHollowDoor> DoorBlueprint = LoadClass<AHollowDoor>(nullptr, DoorClass);
	UHollowNoiseSubsystem* Noise = UHollowNoiseSubsystem::Get(World);
	if (!Character || !TestNotNull(TEXT("GA_Tool_Welder"), WelderClass.Get()) || !TestNotNull(TEXT("Init effect"), InitClass.Get())
		|| !TestNotNull(TEXT("BP_AetherCore"), CoreBlueprint.Get()) || !TestNotNull(TEXT("BP_Door"), DoorBlueprint.Get())
		|| !TestNotNull(TEXT("GCN_Tool_Beam"), LoadClass<AActor>(nullptr, BeamCueClass)) || !TestNotNull(TEXT("Noise subsystem"), Noise)
		|| !TestNotNull(TEXT("DA_NoiseConfig"), Noise->GetConfig()))
	{
		return false;
	}
	const UHollowNoiseConfig* Config = Noise->GetConfig();
	AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
	HollowGameTests::PossessAsPlayer(World, Character, /*bLocalPlayer=*/true);
	Character->DispatchBeginPlay();
	UHollowAbilitySystemComponent* AbilitySystem = Character->GetHollowAbilitySystemComponent();
	if (!TestNotNull(TEXT("Ability system"), AbilitySystem))
	{
		return false;
	}
	AbilitySystem->ApplyGameplayEffectToSelf(InitClass.GetDefaultObject(), 1.f, AbilitySystem->MakeEffectContext());
	TArray<FHollowNoiseReport> Reports;
	Noise->OnNoiseReported.AddLambda([&Reports](const FHollowNoiseReport& Report) { Reports.Add(Report); });
	const float StepLoudness = Config->FindRule(HollowTags::Noise_Footstep)->Loudness;

	// Footsteps: by speed on the ground, never standing, crouching, falling or dead.
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	UHollowFootstepNoiseComponent* Footsteps = Character->GetFootstepNoise();
	Movement->SetMovementMode(MOVE_Walking);
	Movement->Velocity = FVector(Config->FootstepReferenceSpeed, 0.0, 0.0);
	TestTrue(TEXT("Walking makes footsteps"), Footsteps->Step());
	TestTrue(TEXT("...at the rule's loudness, at the feet, by the walker"), Reports.Num() == 1 && FMath::IsNearlyEqual(Reports[0].Loudness, StepLoudness)
		&& Reports[0].Event.Instigator.Get() == Character
		&& FMath::IsNearlyEqual(Reports[0].Event.Location.Z, Character->GetActorLocation().Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	Movement->Velocity = FVector(Config->FootstepReferenceSpeed * 1.6, 0.0, 0.0);
	TestTrue(TEXT("Sprinting is louder"), Footsteps->Step() && Reports.Last().Loudness > StepLoudness);
	Movement->Velocity = FVector::ZeroVector;
	TestFalse(TEXT("Standing is silent"), Footsteps->Step());
	Movement->Velocity = FVector(Config->FootstepReferenceSpeed, 0.0, 0.0);
	Character->bIsCrouched = true;
	TestFalse(TEXT("Crouching is silent"), Footsteps->Step());
	Character->bIsCrouched = false;
	Movement->SetMovementMode(MOVE_Falling);
	TestFalse(TEXT("Falling is silent"), Footsteps->Step());
	Movement->SetMovementMode(MOVE_Walking);
	AbilitySystem->AddLooseGameplayTag(HollowTags::State_Dead);
	TestFalse(TEXT("The dead are silent"), Footsteps->Step());
	AbilitySystem->RemoveLooseGameplayTag(HollowTags::State_Dead);
	Movement->Velocity = FVector::ZeroVector;

	// The real welder: noise at the wielder while it runs, at most once per the rule's interval.
	FGameplayAbilitySpec Spec(WelderClass, 1);
	Spec.GetDynamicSpecSourceTags().AddTag(HollowTags::Input_SecondaryUse);
	AbilitySystem->GiveAbility(Spec);
	Reports.Reset();
	const FVector WelderAt = Character->GetActorLocation();
	AbilitySystem->AbilityInputTagPressed(HollowTags::Input_SecondaryUse);
	Advance(TestWorld, WeldSeconds);
	AbilitySystem->AbilityInputTagReleased(HollowTags::Input_SecondaryUse);
	const float Interval = Config->FindRule(HollowTags::Noise_Tool_Welder)->MinInterval;
	const int32 Welds = Count(Reports, HollowTags::Noise_Tool_Welder);
	const int32 Expected = FMath::FloorToInt(WeldSeconds / Interval);
	TestTrue(FString::Printf(TEXT("Welding is heard once per %.2f s (%d in %.1f s)"), Interval, Welds, WeldSeconds), Welds >= Expected && Welds <= Expected + 1);
	const FHollowNoiseReport* Weld = Reports.FindByPredicate([](const FHollowNoiseReport& R) { return R.Event.NoiseTag == HollowTags::Noise_Tool_Welder; });
	TestTrue(TEXT("...at the welder, by the welder"), Weld && Weld->Event.Instigator.Get() == Character && FVector::Dist2D(Weld->Event.Location, WelderAt) < 1.0);
	Reports.Reset();
	Advance(TestWorld, WeldSeconds);
	TestEqual(TEXT("A stopped welder is silent"), Count(Reports, HollowTags::Noise_Tool_Welder), 0);

	// A dropped core, by its carrier, where it lands.
	AHollowAetherCore* Core = World->SpawnActor<AHollowAetherCore>(CoreBlueprint, FTransform(Character->GetActorLocation() + CoreSpot));
	Core->DispatchBeginPlay();
	Core->Interact(Character);
	Reports.Reset();
	Character->GetCarrierComponent()->ServerDrop(/*bForce=*/true);
	TestTrue(TEXT("Dropping the core thuds"), Count(Reports, HollowTags::Noise_Core_Drop) == 1 && Reports.Last().Event.Instigator.Get() == Character
		&& FVector::Dist(Reports.Last().Event.Location, Core->GetActorLocation()) < 1.0);

	// Structural blows at a door, by the hitter: dents are heavy impacts, the last one a breach.
	AHollowDoor* Door = World->SpawnActor<AHollowDoor>(DoorBlueprint, FTransform(DoorSpot));
	Door->DispatchBeginPlay();
	Reports.Reset();
	FHollowStructuralHit Hit;
	Hit.Amount = StructuralBlow;
	Hit.DamageType = HollowTags::Damage_Structural;
	Hit.Instigator = Character;
	int32 Blows = 0;
	while (Door->CanTakeStructuralDamage() && Blows < 100)
	{
		Door->ApplyStructuralDamage(Hit);
		++Blows;
	}
	TestEqual(TEXT("Every blow but the last is a heavy impact"), Count(Reports, HollowTags::Noise_Impact_Heavy), Blows - 1);
	TestEqual(TEXT("The last one is a breach"), Count(Reports, HollowTags::Noise_Structural_Breach), 1);
	TestTrue(TEXT("...at the door, by the hitter"), Reports.Num() > 0 && Reports.Last().Event.Instigator.Get() == Character
		&& FVector::Dist2D(Reports.Last().Event.Location, Door->GetActorLocation()) < 1.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
