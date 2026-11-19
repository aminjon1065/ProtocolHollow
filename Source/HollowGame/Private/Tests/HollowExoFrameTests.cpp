#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraComponent.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Input/HollowInputConfig.h"
#include "InputAction.h"
#include "Movement/HollowCharacterMovementComponent.h"
#include "Tags/HollowGameplayTags.h"
#include "Tests/AutomationCommon.h"

namespace HollowExoFrameTests
{
	/** One client frame at 60 Hz and a combine window well above it. */
	constexpr float MoveDeltaTime = 1.f / 60.f;
	constexpr float MaxCombineDelta = 0.1f;

	AHollowExoFrameCharacter* SpawnCharacter(FAutomationTestBase& Test, FTestWorldWrapper& World)
	{
		// The bare C++ class has no skeletal mesh (meshes are BP_ExoFrame data), so the camera's socket lookup warns.
		Test.AddExpectedMessage(FString::Printf(TEXT("GetSocketInfoByName(%s): No SkeletalMesh"), *AHollowExoFrameCharacter::CameraSocketName.ToString()),
			ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);

		if (!World.CreateTestWorld(EWorldType::Game))
		{
			World.ForwardErrorMessages(&Test);
			return nullptr;
		}
		return World.GetTestWorld()->SpawnActor<AHollowExoFrameCharacter>();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowExoFrameCompositionTest, "Hollow.Game.ExoFrame.Composition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowExoFrameCompositionTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper World;
	const AHollowExoFrameCharacter* Character = HollowExoFrameTests::SpawnCharacter(*this, World);
	if (!TestNotNull(TEXT("ExoFrame spawns"), Character))
	{
		return false;
	}

	const USkeletalMeshComponent* FirstPersonMesh = Character->GetFirstPersonMesh();
	const UCameraComponent* Camera = Character->GetFirstPersonCamera();
	TestNotNull(TEXT("Uses the Hollow movement component"), Cast<UHollowCharacterMovementComponent>(Character->GetCharacterMovement()));
	TestFalse(TEXT("Actor never ticks (CLAUDE.md sec. 1)"), Character->PrimaryActorTick.bCanEverTick);
	TestTrue(TEXT("Characters can crouch"), Character->GetCharacterMovement()->CanEverCrouch());

	TestTrue(TEXT("Arms render as first person"), FirstPersonMesh->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::FirstPerson);
	TestTrue(TEXT("Only the owner sees the arms"), FirstPersonMesh->bOnlyOwnerSee);
	TestTrue(TEXT("Body is the world-space representation"),
		Character->GetMesh()->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::WorldSpaceRepresentation);

	TestTrue(TEXT("Camera follows the arms' head socket"),
		Camera->GetAttachParent() == FirstPersonMesh && Camera->GetAttachSocketName() == AHollowExoFrameCharacter::CameraSocketName);
	TestTrue(TEXT("Camera uses the control rotation"), Camera->bUsePawnControlRotation);
	TestTrue(TEXT("First-person FOV is enabled"), Camera->bEnableFirstPersonFieldOfView);
	TestTrue(TEXT("First-person scale is enabled"), Camera->bEnableFirstPersonScale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSavedMoveSprintFlagTest, "Hollow.Physics.Movement.SavedMoveSprintFlag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSavedMoveSprintFlagTest::RunTest(const FString& Parameters)
{
	using namespace HollowExoFrameTests;

	FTestWorldWrapper World;
	AHollowExoFrameCharacter* Character = SpawnCharacter(*this, World);
	if (!TestNotNull(TEXT("ExoFrame spawns"), Character))
	{
		return false;
	}

	UHollowCharacterMovementComponent* Movement = Character->GetHollowMovement();
	FNetworkPredictionData_Client_Character* ClientData = Movement->GetPredictionData_Client_Character();
	if (!TestNotNull(TEXT("Client prediction data exists"), ClientData))
	{
		return false;
	}

	auto RecordMove = [&]()
	{
		FSavedMovePtr Move = ClientData->CreateSavedMove();
		Move->SetMoveFor(Character, MoveDeltaTime, FVector::ZeroVector, *ClientData);
		return Move;
	};

	Movement->SetWantsToSprint(false);
	const FSavedMovePtr WalkMove = RecordMove();
	const FSavedMovePtr OtherWalkMove = RecordMove();
	Movement->SetWantsToSprint(true);
	const FSavedMovePtr SprintMove = RecordMove();

	TestTrue(TEXT("Sprint request is encoded in FLAG_Custom_0"), (SprintMove->GetCompressedFlags() & HollowMoveFlags::Sprint) != 0);
	TestFalse(TEXT("Walking moves carry no sprint flag"), (WalkMove->GetCompressedFlags() & HollowMoveFlags::Sprint) != 0);
	TestTrue(TEXT("Moves with equal requests still combine"), WalkMove->CanCombineWith(OtherWalkMove, Character, MaxCombineDelta));
	TestFalse(TEXT("Moves with different sprint requests never combine"), WalkMove->CanCombineWith(SprintMove, Character, MaxCombineDelta));

	// Server side: the request is rebuilt from the received flags only.
	Movement->SetWantsToSprint(false);
	Movement->UpdateFromCompressedFlags(SprintMove->GetCompressedFlags());
	TestTrue(TEXT("Server decodes the sprint request"), Movement->WantsToSprint());
	Movement->UpdateFromCompressedFlags(WalkMove->GetCompressedFlags());
	TestFalse(TEXT("Server clears the sprint request"), Movement->WantsToSprint());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowInputConfigLookupTest, "Hollow.Game.Input.ConfigLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowInputConfigLookupTest::RunTest(const FString& Parameters)
{
	UHollowInputConfig* Config = NewObject<UHollowInputConfig>();
	const UInputAction* MoveAction = NewObject<UInputAction>();
	const UInputAction* JumpAction = NewObject<UInputAction>();

	auto AddEntry = [Config](const UInputAction* Action, const FGameplayTag& Tag)
	{
		FHollowInputAction& Entry = Config->NativeActions.AddDefaulted_GetRef();
		Entry.InputAction = Action;
		Entry.InputTag = Tag;
	};
	AddEntry(MoveAction, HollowTags::Input_Move);
	AddEntry(JumpAction, HollowTags::Input_Jump);
	AddEntry(nullptr, HollowTags::Input_Crouch);

	TestTrue(TEXT("Finds the move action"), Config->FindNativeAction(HollowTags::Input_Move) == MoveAction);
	TestTrue(TEXT("Finds the jump action"), Config->FindNativeAction(HollowTags::Input_Jump) == JumpAction);
	TestNull(TEXT("Entries without an action are skipped"), Config->FindNativeAction(HollowTags::Input_Crouch));
	TestNull(TEXT("Unmapped tags return nothing"), Config->FindNativeAction(HollowTags::Input_Sprint));
	TestNull(TEXT("An empty tag returns nothing"), Config->FindNativeAction(FGameplayTag()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
