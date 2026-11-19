#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Interaction/HollowInteractionComponent.h"
#include "Interaction/HollowInteractionRules.h"
#include "Interaction/HollowInteractionSettings.h"
#include "World/HollowSwitch.h"

namespace HollowInteractionTests
{
	// Test data only: real values live in DefaultGame.ini (HollowInteractionSettings).
	constexpr float MaxDistance = 300.f;
	constexpr float MinInterval = 0.2f;
	constexpr float CubeScale = 0.25f;
	const TCHAR* const CubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");

	FHollowInteractionCheck ValidCheck()
	{
		FHollowInteractionCheck Check;
		Check.bTargetValid = true;
		Check.bTargetInteractable = true;
		Check.bHasLineOfSight = true;
		Check.bTargetAllows = true;
		Check.Distance = MaxDistance * 0.5f;
		return Check;
	}

	AHollowSwitch* SpawnSwitch(UWorld* World, const FVector& Location)
	{
		AHollowSwitch* Switch = World->SpawnActor<AHollowSwitch>(AHollowSwitch::StaticClass(), FTransform(Location));
		Switch->GetMesh()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, CubeMeshPath));
		Switch->GetMesh()->SetWorldScale3D(FVector(CubeScale));
		return Switch;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowInteractionRulesTest, "Hollow.Game.Interaction.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowInteractionRulesTest::RunTest(const FString& Parameters)
{
	using namespace HollowInteractionTests;
	using EReject = EHollowInteractionRejection;
	auto Validate = [](const FHollowInteractionCheck& Check) { return HollowInteractionRules::ValidateRequest(Check, MaxDistance, MinInterval); };

	TestTrue(TEXT("A valid request is accepted"), Validate(ValidCheck()) == EReject::None);

	FHollowInteractionCheck Check = ValidCheck();
	Check.Distance = MaxDistance;
	TestTrue(TEXT("The range limit is inclusive"), Validate(Check) == EReject::None);
	Check.Distance = MaxDistance + 1.f;
	TestTrue(TEXT("Beyond range is rejected"), Validate(Check) == EReject::OutOfRange);

	Check = ValidCheck(); Check.bTargetValid = false;
	TestTrue(TEXT("Missing target is rejected"), Validate(Check) == EReject::NoTarget);
	Check = ValidCheck(); Check.bTargetInteractable = false;
	TestTrue(TEXT("Non-interactable target is rejected"), Validate(Check) == EReject::NotInteractable);
	Check = ValidCheck(); Check.bInteractorBlocked = true;
	TestTrue(TEXT("Dead/downed/stunned players cannot interact"), Validate(Check) == EReject::InteractorBlocked);
	Check = ValidCheck(); Check.SecondsSinceLastInteraction = MinInterval * 0.5f;
	TestTrue(TEXT("Spamming is rejected"), Validate(Check) == EReject::TooSoon);
	Check = ValidCheck(); Check.SecondsSinceLastInteraction = MinInterval;
	TestTrue(TEXT("The rate limit is inclusive"), Validate(Check) == EReject::None);
	Check = ValidCheck(); Check.bHasLineOfSight = false;
	TestTrue(TEXT("Targets behind walls are rejected"), Validate(Check) == EReject::NoLineOfSight);
	Check = ValidCheck(); Check.bTargetAllows = false;
	TestTrue(TEXT("The target can refuse"), Validate(Check) == EReject::TargetRefused);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowInteractionFocusTest, "Hollow.Game.Interaction.FocusAndInteract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowInteractionFocusTest::RunTest(const FString& Parameters)
{
	using namespace HollowInteractionTests;

	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	AHollowExoFrameCharacter* Character = HollowGameTests::SpawnExoFrame(*this, World);
	if (!TestNotNull(TEXT("ExoFrame spawns"), Character))
	{
		return false;
	}
	APlayerController* Controller = HollowGameTests::PossessAsPlayer(World, Character);
	Controller->SetControlRotation(FRotator::ZeroRotator);

	const UHollowInteractionSettings* Settings = GetDefault<UHollowInteractionSettings>();
	FVector Eyes;
	FRotator EyeRotation;
	Character->GetActorEyesViewPoint(Eyes, EyeRotation);
	const FVector Ahead = FVector::ForwardVector * (Settings->InteractionRange * 0.5f);

	AHollowSwitch* Switch = SpawnSwitch(World, Eyes + Ahead);
	UHollowInteractionComponent* Interaction = Character->GetInteractionComponent();

	Interaction->UpdateFocus();
	TestTrue(TEXT("The switch in front is focused"), Interaction->GetFocusedActor() == Switch);

	Interaction->RequestInteract();
	TestTrue(TEXT("Interacting toggles the switch on the server"), Switch->IsOn());
	Interaction->RequestInteract();
	TestTrue(TEXT("A second request within MinInteractInterval is ignored"), Switch->IsOn());

	// Out of reach: no focus, and a forged request is rejected.
	Switch->SetActorLocation(Eyes + FVector::ForwardVector * (Settings->InteractionRange + Settings->ServerRangeTolerance) * 2.f);
	Interaction->UpdateFocus();
	TestNull(TEXT("A distant switch is not focused"), Interaction->GetFocusedActor());
	Interaction->LastInteractionTime.Reset();
	Interaction->Server_Interact(Switch);
	TestTrue(TEXT("The server rejects an out-of-range request"), Switch->IsOn());

	// Behind a wall: no focus, and the server's line-of-sight check rejects the request.
	Switch->SetActorLocation(Eyes + Ahead);
	AStaticMeshActor* Wall = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Eyes + Ahead * 0.5f));
	Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, CubeMeshPath));
	Interaction->UpdateFocus();
	TestTrue(TEXT("A switch behind a wall is not focused"), Interaction->GetFocusedActor() != Switch);
	Interaction->LastInteractionTime.Reset();
	Interaction->Server_Interact(Switch);
	TestTrue(TEXT("The server rejects a request without line of sight"), Switch->IsOn());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSwitchStateTest, "Hollow.Game.Switch.State",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSwitchStateTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	if (!World)
	{
		return false;
	}

	AHollowSwitch* Switch = HollowInteractionTests::SpawnSwitch(World, FVector::ZeroVector);
	int32 Notifications = 0;
	Switch->OnToggled.AddLambda([&Notifications](bool) { ++Notifications; });

	TestFalse(TEXT("Switches start off"), Switch->IsOn());
	Switch->Interact(nullptr);
	TestTrue(TEXT("Interact turns it on"), Switch->IsOn());
	TestTrue(TEXT("The lamp follows the state"), Switch->GetLamp()->IsVisible());
	Switch->Interact(nullptr);
	TestFalse(TEXT("Interact turns it off again"), Switch->IsOn());
	TestFalse(TEXT("The lamp goes dark"), Switch->GetLamp()->IsVisible());
	TestEqual(TEXT("Every change is broadcast"), Notifications, 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
