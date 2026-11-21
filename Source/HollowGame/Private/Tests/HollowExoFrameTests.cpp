#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowHealthSet.h"
#include "Attributes/HollowResonanceSet.h"
#include "Attributes/HollowSurvivalSet.h"
#include "Attributes/HollowToolSet.h"
#include "Camera/CameraComponent.h"
#include "Character/HollowExoFrameCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Framework/HollowPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Input/HollowInputConfig.h"
#include "InputAction.h"
#include "Carry/HollowCarrySettings.h"
#include "Movement/HollowCharacterMovementComponent.h"
#include "Movement/HollowMovementTuning.h"
#include "Tags/HollowGameplayTags.h"
#include "Tests/AutomationCommon.h"
#include "UObject/Package.h"

namespace HollowExoFrameTests
{
	/** One client frame at 60 Hz and a combine window well above it. */
	constexpr float MoveDeltaTime = 1.f / 60.f;
	constexpr float MaxCombineDelta = 0.1f;

	AHollowExoFrameCharacter* SpawnCharacter(FAutomationTestBase& Test, FTestWorldWrapper& World)
	{
		return HollowGameTests::SpawnExoFrame(Test, HollowGameTests::CreateWorld(Test, World));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowExoFrameAbilitySystemLifecycleTest, "Hollow.Game.ExoFrame.AbilitySystemLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowExoFrameAbilitySystemLifecycleTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper World;
	AHollowExoFrameCharacter* Character = HollowExoFrameTests::SpawnCharacter(*this, World);
	if (!TestNotNull(TEXT("ExoFrame spawns"), Character))
	{
		return false;
	}

	UWorld* TestWorld = World.GetTestWorld();
	APlayerController* Controller = TestWorld->SpawnActor<APlayerController>();
	AHollowPlayerState* PlayerState = TestWorld->SpawnActor<AHollowPlayerState>();
	PlayerState->SetOwner(Controller);
	Controller->PlayerState = PlayerState;

	UHollowAbilitySystemComponent* AbilitySystem = PlayerState->GetHollowAbilitySystemComponent();
	TestNotNull(TEXT("Health set lives on the player state"), AbilitySystem->GetSet<UHollowHealthSet>());
	TestNotNull(TEXT("Survival set lives on the player state"), AbilitySystem->GetSet<UHollowSurvivalSet>());
	TestNotNull(TEXT("Resonance set lives on the player state"), AbilitySystem->GetSet<UHollowResonanceSet>());
	TestNotNull(TEXT("Tool set lives on the player state"), AbilitySystem->GetSet<UHollowToolSet>());
	TestNull(TEXT("No ability system before possession"), Character->GetAbilitySystemComponent());

	Controller->Possess(Character);
	TestTrue(TEXT("Possessed ExoFrame uses the player state's ability system"), Character->GetAbilitySystemComponent() == AbilitySystem);
	TestTrue(TEXT("The player state owns the ability system"), AbilitySystem->GetOwnerActor() == PlayerState);
	TestTrue(TEXT("The ExoFrame is the avatar"), AbilitySystem->GetAvatarActor() == Character);

	Controller->UnPossess();
	TestNull(TEXT("Unpossessed ExoFrame releases the ability system"), Character->GetAbilitySystemComponent());
	TestTrue(TEXT("The ability system no longer uses the ExoFrame as avatar"), AbilitySystem->GetAvatarActor() != Character);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSavedMoveCarryBitsTest, "Hollow.Physics.Movement.SavedMoveCarryBits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSavedMoveCarryBitsTest::RunTest(const FString& Parameters)
{
	// ADR-0015: the carry claim travels in two move flags; the server simulates a remote carrier (no local controller
	// here) with max(claim, server state after the grace). Test tuning only: real values are DA_MovementTuning_ExoFrame.
	using namespace HollowExoFrameTests;
	using EClaim = EHollowCarryClaim;
	constexpr float WalkSpeed = 400.f;
	constexpr float SprintMultiplier = 1.5f;
	constexpr float Acceleration = 2000.f;
	constexpr float SoloScale = 0.45f;
	constexpr float SoloAccelerationScale = 0.6f;
	constexpr float SprintMaxAngle = 45.f;
	constexpr double Start = 100.0;

	FTestWorldWrapper World;
	AHollowExoFrameCharacter* Character = SpawnCharacter(*this, World);
	if (!TestNotNull(TEXT("ExoFrame spawns"), Character))
	{
		return false;
	}
	UHollowMovementTuning* Tuning = NewObject<UHollowMovementTuning>(GetTransientPackage());
	Tuning->Ground.WalkSpeed = WalkSpeed;
	Tuning->Ground.CrouchSpeed = WalkSpeed;
	Tuning->Ground.SprintSpeedMultiplier = SprintMultiplier;
	Tuning->Ground.SprintMaxAngle = SprintMaxAngle;
	Tuning->MaxAcceleration = Acceleration;
	Tuning->Carry.Solo.SpeedScale = SoloScale;
	Tuning->Carry.Solo.AccelerationScale = SoloAccelerationScale;
	UHollowCharacterMovementComponent* Movement = Character->GetHollowMovement();
	Movement->SetMovementTuning(Tuning);

	// Encoding: all four claims round-trip and never touch the sprint flag.
	for (const EClaim Claim : {EClaim::None, EClaim::Solo, EClaim::Coop, EClaim::Dragged})
	{
		TestTrue(FString::Printf(TEXT("Claim %d round-trips"), static_cast<int32>(Claim)), HollowMoveFlags::DecodeCarry(HollowMoveFlags::EncodeCarry(Claim)) == Claim);
		TestEqual(FString::Printf(TEXT("Claim %d keeps clear of the sprint flag"), static_cast<int32>(Claim)),
			static_cast<int32>(HollowMoveFlags::EncodeCarry(Claim) & HollowMoveFlags::Sprint), 0);
	}

	// Saved moves record the live claim and never combine across a change.
	FNetworkPredictionData_Client_Character* ClientData = Movement->GetPredictionData_Client_Character();
	auto RecordMove = [&]()
	{
		FSavedMovePtr Move = ClientData->CreateSavedMove();
		Move->SetMoveFor(Character, MoveDeltaTime, FVector::ZeroVector, *ClientData);
		return Move;
	};
	Movement->SetClaimedCarry(EClaim::None);
	const FSavedMovePtr FreeMove = RecordMove();
	const FSavedMovePtr OtherFreeMove = RecordMove();
	Movement->SetClaimedCarry(EClaim::Solo);
	const FSavedMovePtr CarryMove = RecordMove();
	TestTrue(TEXT("A carrying move claims Solo in its flags"), HollowMoveFlags::DecodeCarry(CarryMove->GetCompressedFlags()) == EClaim::Solo);
	TestTrue(TEXT("Equal claims still combine"), FreeMove->CanCombineWith(OtherFreeMove, Character, MaxCombineDelta));
	TestFalse(TEXT("A claim change never combines"), FreeMove->CanCombineWith(CarryMove, Character, MaxCombineDelta));

	// Server: the pick-up was decided at Start (not predicted): the old free state binds through the grace.
	Movement->SetMovementMode(MOVE_Walking);
	Movement->Acceleration = Character->GetActorForwardVector();   // moving forward: a sprint request may be honoured
	UWorld* TestWorld = World.GetTestWorld();
	const float Grace = GetDefault<UHollowCarrySettings>()->ServerSlowdownGraceSeconds;
	TestTrue(TEXT("A grace is configured (DefaultGame.ini)"), Grace > 0.f);
	TestWorld->TimeSeconds = Start;
	Movement->SetServerCarry(EClaim::Solo);
	Movement->UpdateFromCompressedFlags(FreeMove->GetCompressedFlags());
	TestTrue(TEXT("Grace: a move without the claim walks free"), Movement->GetEffectiveCarry() == EClaim::None);
	TestEqual(TEXT("Grace: full speed"), Movement->GetMaxSpeed(), WalkSpeed);
	TestWorld->TimeSeconds = Start + Grace;
	TestTrue(TEXT("After the grace the server state binds a client that never claims"), Movement->GetEffectiveCarry() == EClaim::Solo);
	TestEqual(TEXT("Carry speed"), Movement->GetMaxSpeed(), WalkSpeed * SoloScale);
	TestEqual(TEXT("Carry acceleration"), Movement->GetMaxAcceleration(), Acceleration * SoloAccelerationScale);
	TestFalse(TEXT("No jump while carrying"), Movement->CanAttemptJump());
	Movement->SetWantsToSprint(true);
	TestFalse(TEXT("No sprint while carrying"), Movement->IsSprinting());
	TestEqual(TEXT("A sprint request does not speed a carrier up"), Movement->GetMaxSpeed(), WalkSpeed * SoloScale);
	Movement->UpdateFromCompressedFlags(CarryMove->GetCompressedFlags());
	TestTrue(TEXT("The decoded claim agrees"), Movement->GetClaimedCarry() == EClaim::Solo && Movement->GetEffectiveCarry() == EClaim::Solo);

	// Drop decided by the server: the client's slower claim is accepted until it learns it; its free moves at once.
	TestWorld->TimeSeconds = Start + 2.0 * Grace;
	Movement->SetServerCarry(EClaim::None);
	TestTrue(TEXT("Still claiming Solo after the drop: accepted"), Movement->GetEffectiveCarry() == EClaim::Solo);
	Movement->UpdateFromCompressedFlags(FreeMove->GetCompressedFlags());
	TestTrue(TEXT("A free move right after the drop walks free"), Movement->GetEffectiveCarry() == EClaim::None);
	Movement->SetWantsToSprint(true);   // the free move carried no sprint request
	TestTrue(TEXT("Free again: sprint honoured"), Movement->IsSprinting());
	TestEqual(TEXT("Free again: sprint speed"), Movement->GetMaxSpeed(), WalkSpeed * SprintMultiplier);
	TestTrue(TEXT("Free again: jump allowed"), Movement->CanAttemptJump());

	// Client replay: moves replay with their own claims, the live claim survives.
	Movement->SetWantsToSprint(false);
	Movement->SetClaimedCarry(EClaim::None);
	ClientData->SavedMoves.Add(CarryMove);
	ClientData->bUpdatePosition = true;
	Movement->ClientUpdatePositionAfterServerUpdate();
	TestTrue(TEXT("Replay restores the live claim"), Movement->GetClaimedCarry() == EClaim::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCoopAuthorityTest, "Hollow.Physics.Movement.CoopAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCoopAuthorityTest::RunTest(const FString& Parameters)
{
	// ADR-0028: a remote co-op carrier's position comes from its client (no controller here: the server simulating a
	// remote player). After the carry the server takes over only once its moves show it knows (a move without the Coop
	// claim after one with it), the settle time has passed and its move agrees, or after the timeout; a carrier the
	// sanity check refuses at once, put back where it last passed.
	using namespace HollowExoFrameTests;
	using EClaim = EHollowCarryClaim;
	constexpr double Start = 100.0;
	constexpr double Joined = Start + 10.0;
	constexpr double LearnsAfter = 0.2;
	constexpr double Earlier = 0.01;
	const FVector PartnerAt(140.0, 0.0, 0.0);
	const FVector Sane(0.0, 50.0, 0.0);

	FTestWorldWrapper World;
	AHollowExoFrameCharacter* Character = SpawnCharacter(*this, World);
	AHollowExoFrameCharacter* Partner = Character ? HollowGameTests::SpawnExoFrame(*this, World.GetTestWorld(), FTransform(PartnerAt)) : nullptr;
	if (!TestNotNull(TEXT("ExoFrame spawns"), Character) || !TestNotNull(TEXT("Its partner too"), Partner))
	{
		return false;
	}
	UHollowCharacterMovementComponent* Movement = Character->GetHollowMovement();
	const UHollowCarrySettings* Settings = GetDefault<UHollowCarrySettings>();
	const double Settle = Settings->CoopAuthoritySettleSeconds;
	const double Timeout = Settings->CoopAuthorityTimeoutSeconds;
	TestTrue(TEXT("Settle and timeout configured (DefaultGame.ini)"), Settle > 0.0 && Timeout > Settle * 2.0);
	UWorld* TestWorld = World.GetTestWorld();
	FHollowCoopCarryTuning Tether;
	Tether.CarrierSpacing = static_cast<float>(PartnerAt.X);
	auto Move = [Movement](const EClaim Claim) { Movement->UpdateFromCompressedFlags(HollowMoveFlags::EncodeCarry(Claim)); };

	TestWorld->TimeSeconds = Start;
	TestFalse(TEXT("A carrier alone is server-authoritative"), Movement->IsClientAuthoritative());
	Movement->SetCoopPartner(Partner, &Tether);
	TestTrue(TEXT("With a partner: client-authoritative and never corrected"), Movement->IsClientAuthoritative()
		&& Movement->bIgnoreClientMovementErrorChecksAndCorrection && Movement->GetCoopPartner() == Partner);

	// Leaves before its client ever claimed Coop (a quick join and leave): its moves cannot show it knows; the timeout.
	Movement->SetCoopPartner(nullptr, nullptr);
	TestTrue(TEXT("Left: still client-authoritative, on the way back"), Movement->IsClientAuthoritative() && Movement->IsReturningAuthority());
	Move(EClaim::Solo);
	TestWorld->TimeSeconds = Start + Settle * 2.0;
	Movement->UpdateCoopReturn(/*bInSync=*/true);
	TestTrue(TEXT("Never claimed Coop: still client-authoritative"), Movement->IsClientAuthoritative());
	TestWorld->TimeSeconds = Start + Timeout;
	AddExpectedMessage(TEXT("without agreeing moves"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);
	Movement->UpdateCoopReturn(/*bInSync=*/false);
	TestTrue(TEXT("After the timeout: server-authoritative again"), !Movement->IsClientAuthoritative() && !Movement->IsReturningAuthority());

	// The clean way back.
	TestWorld->TimeSeconds = Joined;
	Movement->SetCoopPartner(Partner, &Tether);
	Move(EClaim::Coop);
	Movement->SetCoopPartner(nullptr, nullptr);
	Move(EClaim::Coop);   // sent before it learnt it left
	TestWorld->TimeSeconds = Joined + LearnsAfter;
	Move(EClaim::Solo);   // it knows: it carries on alone
	TestWorld->TimeSeconds = Joined + LearnsAfter + Settle - Earlier;
	Movement->UpdateCoopReturn(/*bInSync=*/true);
	TestTrue(TEXT("Settling: still client-authoritative"), Movement->IsClientAuthoritative());
	TestWorld->TimeSeconds = Joined + LearnsAfter + Settle;
	Movement->UpdateCoopReturn(/*bInSync=*/false);
	TestTrue(TEXT("Settled, but its move disagrees: still client-authoritative"), Movement->IsClientAuthoritative());
	Movement->UpdateCoopReturn(/*bInSync=*/true);
	TestTrue(TEXT("Settled and agreeing: server-authoritative, nothing to correct"), !Movement->IsClientAuthoritative() && !Movement->IsReturningAuthority());

	// Rejoining on the way back keeps it client-authoritative; refused: back at once.
	Movement->SetCoopPartner(Partner, &Tether);
	Movement->SetCoopPartner(nullptr, nullptr);
	Movement->SetCoopPartner(Partner, &Tether);
	TestTrue(TEXT("Rejoined on the way back: client-authoritative, not returning"), Movement->IsClientAuthoritative() && !Movement->IsReturningAuthority());
	AddExpectedMessage(TEXT("server-authoritative again, put back"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, /*IsRegex=*/false);
	Movement->ForceServerAuthority(Sane);
	TestTrue(TEXT("Refused: server-authoritative at once, where it last passed"), !Movement->IsClientAuthoritative() && Character->GetActorLocation().Equals(Sane, 0.1));
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
