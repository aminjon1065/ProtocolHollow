#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/HollowAbilitySystemComponent.h"
#include "Attributes/HollowResonanceSet.h"
#include "Attributes/HollowSurvivalSet.h"
#include "Camera/CameraComponent.h"
#include "Carry/HollowAetherCore.h"
#include "Carry/HollowCarrierComponent.h"
#include "Carry/HollowCarrySettings.h"
#include "Carry/HollowCarryableComponent.h"
#include "Carry/HollowCarryableDefinition.h"
#include "Collision/HollowCollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameplayEffect.h"
#include "Movement/HollowCharacterMovementComponent.h"
#include "Tags/HollowGameplayTags.h"

namespace HollowCarryGameTests
{
	// Content under test (create_carry_assets.py, create_gas_assets.py).
	const TCHAR* const CoreClass = TEXT("/Game/Hollow/Items/AetherCore/BP_AetherCore.BP_AetherCore_C");
	const TCHAR* const InitEffectClass = TEXT("/Game/Hollow/GAS/Effects/GE_ExoFrame_InitAttributes.GE_ExoFrame_InitAttributes_C");
	const TCHAR* const CubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");

	/** Test data. */
	constexpr float TickSeconds = 1.f / 60.f;
	constexpr float CoreDistance = 120.f;
	constexpr float CarrierSpeed = 300.f;
	constexpr float CarrySeconds = 1.5f;
	constexpr float PositionTolerance = 0.1f;
	constexpr float VelocityTolerance = 1.f;
	/** A wall right in front of the carrier, through the held core: 10 cm thick (the cube mesh is 100 cm). */
	constexpr float WallDistance = 60.f;
	const FVector WallScale(0.1, 2.0, 2.0);
	constexpr float WallHalfThickness = 5.f;
	constexpr float DropTolerance = 1.f;
	/** Turned towards a corner: the held box's half diagonal (35 cm) then exceeds the capsule radius (34 cm). */
	constexpr double CornerYaw = 45.0;

	struct FRig
	{
		FTestWorldWrapper World;
		AHollowExoFrameCharacter* Character = nullptr;
		UHollowAbilitySystemComponent* AbilitySystem = nullptr;
		AHollowAetherCore* Core = nullptr;
		UHollowCarryableComponent* Carryable = nullptr;
		const UHollowCarryableDefinition* Definition = nullptr;
	};

	/** One engine frame per step: the timer manager ticks once per GFrameCounter (periodic effects, carry timers). */
	void Advance(FRig& Rig, const float Seconds)
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += TickSeconds)
		{
			++GFrameCounter;
			Rig.World.TickTestWorld(TickSeconds);
		}
	}

	float Attribute(const FRig& Rig, const FGameplayAttribute& Attribute)
	{
		return Rig.AbilitySystem->GetNumericAttribute(Attribute);
	}

	/** A second ExoFrame possessed by its own (remote) player, next to the first. */
	AHollowExoFrameCharacter* SpawnOtherPlayer(FAutomationTestBase& Test, FRig& Rig)
	{
		UWorld* World = Rig.World.GetTestWorld();
		AHollowExoFrameCharacter* Other = HollowGameTests::SpawnExoFrame(Test, World, FTransform(FVector(0.0, CoreDistance, 0.0)));
		HollowGameTests::PossessAsPlayer(World, Other);
		Other->DispatchBeginPlay();
		return Other;
	}

	/**
	 * ExoFrame possessed by a player with real init attributes, playing, and the real BP_AetherCore in front of it.
	 * bLocalPlayer: the player on this machine (solo, listen host); otherwise a remote player seen by the server.
	 */
	bool Build(FAutomationTestBase& Test, FRig& Rig, const bool bLocalPlayer)
	{
		UWorld* World = HollowGameTests::CreateWorld(Test, Rig.World);
		Rig.Character = HollowGameTests::SpawnExoFrame(Test, World);
		const TSubclassOf<AHollowAetherCore> Core = LoadClass<AHollowAetherCore>(nullptr, CoreClass);
		const TSubclassOf<UGameplayEffect> InitClass = LoadClass<UGameplayEffect>(nullptr, InitEffectClass);
		if (!Test.TestNotNull(TEXT("ExoFrame"), Rig.Character) || !Test.TestNotNull(TEXT("BP_AetherCore"), Core.Get())
			|| !Test.TestNotNull(TEXT("GE_ExoFrame_InitAttributes"), InitClass.Get()))
		{
			return false;
		}
		if (bLocalPlayer)
		{
			// A local player sets up input; the bare C++ class has no InputConfig (BP_ExoFrame data), which it reports.
			Test.AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
		}
		HollowGameTests::PossessAsPlayer(World, Rig.Character, bLocalPlayer);
		Rig.Character->DispatchBeginPlay();
		Rig.AbilitySystem = Cast<UHollowAbilitySystemComponent>(Rig.Character->GetAbilitySystemComponent());
		if (!Test.TestNotNull(TEXT("Ability system on the player state"), Rig.AbilitySystem))
		{
			return false;
		}
		Rig.AbilitySystem->ApplyGameplayEffectToSelf(InitClass.GetDefaultObject(), 1.f, Rig.AbilitySystem->MakeEffectContext());

		Rig.Core = World->SpawnActor<AHollowAetherCore>(Core, FTransform(FVector(CoreDistance, 0.0, 0.0)));
		Rig.Core->DispatchBeginPlay();
		Rig.Carryable = Rig.Core->GetCarryable();
		Rig.Definition = Rig.Carryable->GetDefinition();
		return Test.TestNotNull(TEXT("Core definition"), Rig.Definition);
	}

	/** A 10 cm wall (X thickness, 200 x 200 cm) at Centre. */
	AStaticMeshActor* SpawnWall(FRig& Rig, const FVector& Centre, const FRotator& Rotation)
	{
		AStaticMeshActor* Wall = Rig.World.GetTestWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Rotation, Centre, WallScale));
		Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);   // a registered static component refuses meshes in play
		Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, CubeMeshPath));
		return Wall;
	}

	bool IsFreeAtRest(const FRig& Rig)
	{
		return !Rig.Carryable->GetCarryState().IsCarried() && Rig.Carryable->GetPresentation() == EHollowCarryPresentation::Resting
			&& !Rig.Core->GetBody()->IsSimulatingPhysics() && !Rig.Core->IsReplicatingMovement() && Rig.Core->NetDormancy == DORM_DormantAll
			&& Rig.Core->GetBody()->GetCollisionProfileName() == HollowCollision::CarryableProfile();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCarrySoloCycleTest, "Hollow.Game.Carry.SoloCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCarrySoloCycleTest::RunTest(const FString& Parameters)
{
	// Solo player (or listen host): lift through Interact, carry with its effect, drop, sleep, settle; drop on despawn.
	using namespace HollowCarryGameTests;
	FRig Rig;
	if (!Build(*this, Rig, /*bLocalPlayer=*/true))
	{
		return false;
	}
	AHollowExoFrameCharacter* Character = Rig.Character;
	UHollowCarrierComponent* Carrier = Character->GetCarrierComponent();
	UHollowCharacterMovementComponent* Movement = Character->GetHollowMovement();
	UBoxComponent* Body = Rig.Core->GetBody();
	TestTrue(TEXT("Spawned resting: kinematic, no movement replication, dormant"), IsFreeAtRest(Rig));
	TestTrue(TEXT("A free core can be lifted"), Rig.Core->CanInteract(Character));

	// Lift (the server side of Interact, after the interaction checks).
	Rig.Core->Interact(Character);
	const FHollowCarryNetState& State = Rig.Carryable->GetCarryState();
	TestTrue(TEXT("Carried alone by the player"), State.Mode == EHollowCarryMode::Solo && State.Primary == Character && State.TransitionSerial == 1);
	TestTrue(TEXT("The carrier knows it"), Carrier->GetCarried() == Rig.Carryable);
	TestTrue(TEXT("The body rides on the capsule"), Body->GetAttachParent() == Character->GetCapsuleComponent());
	TestEqual(TEXT("At the grip"), Body->GetRelativeLocation(), Rig.Definition->GripOffset.GetLocation(), PositionTolerance);
	TestTrue(TEXT("Carried collision: overlaps only"), Body->GetCollisionProfileName() == HollowCollision::CarriedProfile());
	TestFalse(TEXT("No physics while carried"), Body->IsSimulatingPhysics());
	TestTrue(TEXT("Carrying tag from GE_Carry_Solo"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_CarryingHeavy_Solo));
	TestTrue(TEXT("Both hands full: tools blocked"), Rig.AbilitySystem->AreAbilityTagsBlocked(FGameplayTagContainer(HollowTags::Ability_Tool_Welder)));
	TestTrue(TEXT("The local carrier claims Solo; the server knows Solo"), Movement->GetClaimedCarry() == EHollowCarryClaim::Solo
		&& Movement->GetServerCarry().Current == EHollowCarryClaim::Solo && Movement->GetEffectiveCarry() == EHollowCarryClaim::Solo);
	TestFalse(TEXT("No jump while carrying"), Character->CanJump());
	TestTrue(TEXT("Own view: the visual is in first person on the camera"), Rig.Carryable->GetPresentation() == EHollowCarryPresentation::FirstPerson
		&& Rig.Core->GetVisual()->GetAttachParent() == Character->GetFirstPersonCamera()
		&& Rig.Core->GetVisual()->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::FirstPerson);
	TestFalse(TEXT("Nobody lifts a carried core"), Rig.Core->CanInteract(Character));
	AHollowExoFrameCharacter* Other = SpawnOtherPlayer(*this, Rig);
	TestTrue(TEXT("Another player may take the other side (S3-06, Hollow.Game.Carry.CoopCycle)"), Rig.Core->CanInteract(Other)
		&& Rig.Core->GetInteractionInfo(Other).Prompt.EqualTo(Rig.Definition->JoinPrompt));

	const float StaminaAtLift = Attribute(Rig, UHollowSurvivalSet::GetStaminaAttribute());
	Advance(Rig, CarrySeconds);
	const float StaminaAtDrop = Attribute(Rig, UHollowSurvivalSet::GetStaminaAttribute());
	TestTrue(TEXT("Carrying drains stamina"), StaminaAtDrop < StaminaAtLift);
	TestTrue(TEXT("Carrying raises Resonance"), Attribute(Rig, UHollowResonanceSet::GetResonanceAttribute()) > 0.f);

	// Drop while walking: part of the carrier's velocity, physics, awake replication.
	Movement->Velocity = FVector(CarrierSpeed, 0.0, 0.0);
	Carrier->ServerDrop();
	TestTrue(TEXT("Free again"), !State.IsCarried() && State.TransitionSerial == 2 && !Carrier->GetCarried());
	TestFalse(TEXT("Carrying tag removed"), Rig.AbilitySystem->HasMatchingGameplayTag(HollowTags::State_CarryingHeavy));
	TestTrue(TEXT("Claim and server state free"), Movement->GetClaimedCarry() == EHollowCarryClaim::None && Movement->GetServerCarry().Current == EHollowCarryClaim::None);
	TestTrue(TEXT("Dropped body simulates, detached, free collision"), Body->IsSimulatingPhysics() && !Body->GetAttachParent()
		&& Body->GetCollisionProfileName() == HollowCollision::CarryableProfile());
	TestEqual(TEXT("It keeps part of the carrier's velocity"), Body->GetPhysicsLinearVelocity().X,
		static_cast<double>(CarrierSpeed * Rig.Definition->InheritVelocityFraction), static_cast<double>(VelocityTolerance));
	TestTrue(TEXT("Awake: movement replicates, not dormant"), Rig.Core->IsReplicatingMovement() && Rig.Core->NetDormancy == DORM_Awake);
	TestTrue(TEXT("The visual is back on the body"), Rig.Core->GetVisual()->GetAttachParent() == Body
		&& Rig.Core->GetVisual()->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None);
	Advance(Rig, CarrySeconds);
	TestEqual(TEXT("No drain after the drop"), Attribute(Rig, UHollowSurvivalSet::GetStaminaAttribute()), StaminaAtDrop);

	// Sleep -> rest after the dormancy delay.
	Rig.Carryable->HandleBodySleep();
	TestTrue(TEXT("Asleep: still replicating its final pose"), Rig.Core->IsReplicatingMovement());
	Advance(Rig, Rig.Definition->DormancyDelaySeconds + 2.f * TickSeconds);
	TestTrue(TEXT("Settled: kinematic, no movement replication, dormant"), IsFreeAtRest(Rig));

	// Held through a wall (a carried body passes through the world): it is put down beside the carrier, clear of both.
	Rig.Core->Interact(Character);
	const FVector WallCentre = Character->GetActorLocation() + FVector(WallDistance, 0.0, Rig.Definition->GripOffset.GetLocation().Z);
	AStaticMeshActor* Wall = SpawnWall(Rig, WallCentre, FRotator::ZeroRotator);
	const double HeldFront = Body->GetComponentLocation().X + Body->GetScaledBoxExtent().X;
	TestTrue(TEXT("The held core reaches through the wall"), HeldFront > WallCentre.X);
	const FVector CarrierAtDrop = Character->GetActorLocation();
	Carrier->ServerDrop();
	TestFalse(TEXT("Dropped"), State.IsCarried());
	TestTrue(TEXT("Dropped on the carrier's side of the wall"),
		Body->GetComponentLocation().X + Body->GetScaledBoxExtent().X <= WallCentre.X - WallHalfThickness + DropTolerance);
	TestTrue(TEXT("Clear of the carrier's capsule"), FVector::Dist2D(Body->GetComponentLocation(), CarrierAtDrop)
		>= Character->GetCapsuleComponent()->GetScaledCapsuleRadius() + Body->GetScaledBoxExtent().X - DropTolerance);
	Wall->Destroy();

	// In a corner, turned 45 degrees (the held body grazes both walls): it goes behind the carrier, where there is room.
	Rig.Core->Interact(Character);
	Character->SetActorRotation(FRotator(0.0, CornerYaw, 0.0));
	const FVector Corner = Character->GetActorLocation();
	const double Touch = Character->GetCapsuleComponent()->GetScaledCapsuleRadius() + WallHalfThickness;   // both walls touch the capsule
	const FVector WallHeight(0.0, 0.0, Rig.Definition->GripOffset.GetLocation().Z);
	AStaticMeshActor* NorthWall = SpawnWall(Rig, Corner + FVector(Touch, 0.0, 0.0) + WallHeight, FRotator::ZeroRotator);
	AStaticMeshActor* EastWall = SpawnWall(Rig, Corner + FVector(0.0, Touch, 0.0) + WallHeight, FRotator(0.0, 90.0, 0.0));
	Carrier->ServerDrop();
	TestFalse(TEXT("Dropped in the corner"), State.IsCarried());
	TestTrue(TEXT("Behind the carrier, away from both walls"), Body->GetComponentLocation().X < Corner.X && Body->GetComponentLocation().Y < Corner.Y);
	NorthWall->Destroy();
	EastWall->Destroy();
	Character->SetActorRotation(FRotator::ZeroRotator);

	// A body that never sleeps is put to sleep after MaxAwakeSeconds.
	Rig.Core->Interact(Character);
	Carrier->ServerDrop();
	Advance(Rig, Rig.Definition->MaxAwakeSeconds + Rig.Definition->DormancyDelaySeconds + 4.f * TickSeconds);
	TestTrue(TEXT("Forced to sleep and settled"), IsFreeAtRest(Rig));

	// The carrier despawns (death, disconnect): the core stays in the world.
	Rig.Core->Interact(Character);
	TestTrue(TEXT("Lifted again"), State.IsCarriedBy(Character));
	Character->Destroy();
	TestTrue(TEXT("Dropped when the carrier is gone"), !State.IsCarried() && Body->IsSimulatingPhysics() && !Body->GetAttachParent());
	TestTrue(TEXT("The other player can lift it now"), Rig.Core->CanInteract(Other));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCarryPresentationTest, "Hollow.Game.Carry.Presentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCarryPresentationTest::RunTest(const FString& Parameters)
{
	// What a machine shows depends on whose view it renders; a state replicated before BeginPlay is shown at once.
	using namespace HollowCarryGameTests;
	FRig Rig;
	if (!Build(*this, Rig, /*bLocalPlayer=*/false))
	{
		return false;
	}
	AHollowExoFrameCharacter* Remote = Rig.Character;
	UStaticMeshComponent* Visual = Rig.Core->GetVisual();
	const FTransform VisualRest = Visual->GetRelativeTransform();

	Rig.Core->Interact(Remote);
	TestTrue(TEXT("Someone else's carry: third person on the body mesh"), Rig.Carryable->GetPresentation() == EHollowCarryPresentation::ThirdPerson
		&& Visual->GetAttachParent() == Remote->GetMesh() && Visual->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None);
	TestTrue(TEXT("In the third-person frame of the definition"), Visual->GetRelativeTransform().Equals(VisualRest * Rig.Definition->ThirdPerson.Offset));
	TestTrue(TEXT("The server body still rides on the capsule"), Rig.Core->GetBody()->GetAttachParent() == Remote->GetCapsuleComponent());
	TestTrue(TEXT("A remote carrier's claim comes from its moves, not from here"), Remote->GetHollowMovement()->GetClaimedCarry() == EHollowCarryClaim::None
		&& Remote->GetHollowMovement()->GetServerCarry().Current == EHollowCarryClaim::Solo);
	Remote->GetCarrierComponent()->ServerDrop();
	TestTrue(TEXT("Dropped: visual back on the body in its own frame"), Visual->GetAttachParent() == Rig.Core->GetBody()
		&& Visual->GetRelativeTransform().Equals(VisualRest));

	// First replication of a core someone already carries (relevancy, a slow loader): the net driver writes the state
	// before BeginPlay, and BeginPlay shows it carried, with the body on the carrier.
	const FStructProperty* StateProperty = FindFProperty<FStructProperty>(UHollowCarryableComponent::StaticClass(), TEXT("CarryState"));
	if (!TestNotNull(TEXT("Replicated CarryState property"), StateProperty))
	{
		return false;
	}
	FHollowCarryNetState Received;
	Received.Mode = EHollowCarryMode::Solo;
	Received.Primary = Remote;
	Received.TransitionSerial = 3;
	AHollowAetherCore* Late = Rig.World.GetTestWorld()->SpawnActorDeferred<AHollowAetherCore>(Rig.Core->GetClass(), FTransform(FVector(-CoreDistance, 0.0, 0.0)));
	StateProperty->CopyCompleteValue(StateProperty->ContainerPtrToValuePtr<void>(Late->GetCarryable()), &Received);
	Late->FinishSpawning(FTransform(FVector(-CoreDistance, 0.0, 0.0)));
	Late->DispatchBeginPlay();
	TestTrue(TEXT("Late: shown carried in third person"), Late->GetCarryable()->GetPresentation() == EHollowCarryPresentation::ThirdPerson
		&& Late->GetVisual()->GetAttachParent() == Remote->GetMesh());
	TestTrue(TEXT("Late: the body rides on the carrier at the grip"), Late->GetBody()->GetAttachParent() == Remote->GetCapsuleComponent()
		&& Late->GetBody()->GetRelativeLocation().Equals(Rig.Definition->GripOffset.GetLocation(), PositionTolerance));
	TestTrue(TEXT("Late: the carrier learns it"), Remote->GetCarrierComponent()->GetCarried() == Late->GetCarryable());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCarryPredictionTest, "Hollow.Game.Carry.Prediction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCarryPredictionTest::RunTest(const FString& Parameters)
{
	// The owning client's Interact input: a predicted claim, confirmed by the state or dropped after the timeout.
	using namespace HollowCarryGameTests;
	FRig Rig;
	if (!Build(*this, Rig, /*bLocalPlayer=*/true))
	{
		return false;
	}
	UHollowCarrierComponent* Carrier = Rig.Character->GetCarrierComponent();
	const UHollowCharacterMovementComponent* Movement = Rig.Character->GetHollowMovement();
	const float Timeout = GetDefault<UHollowCarrySettings>()->PickupPredictionTimeoutSeconds;
	TestTrue(TEXT("A prediction timeout is configured (DefaultGame.ini)"), Timeout > 0.f);

	TestFalse(TEXT("Focusing anything else predicts nothing"), Carrier->HandleInteractInput(Rig.Character));
	TestTrue(TEXT("No claim"), !Carrier->GetPendingPickup() && Movement->GetClaimedCarry() == EHollowCarryClaim::None);

	// The request is lost or refused: the claim goes after the timeout, never sooner.
	TestFalse(TEXT("Interact on a free core is not consumed: it goes on to the server"), Carrier->HandleInteractInput(Rig.Core));
	TestTrue(TEXT("Predicted: slow at once"), Carrier->GetPendingPickup() == Rig.Carryable && Movement->GetClaimedCarry() == EHollowCarryClaim::Solo);
	Advance(Rig, Timeout * 0.5f);
	TestTrue(TEXT("Still claimed before the timeout"), Movement->GetClaimedCarry() == EHollowCarryClaim::Solo);
	Advance(Rig, Timeout);
	TestTrue(TEXT("Never confirmed: the claim is dropped"), !Carrier->GetPendingPickup() && Movement->GetClaimedCarry() == EHollowCarryClaim::None);

	// Confirmed by the state: the prediction turns into the carry, the timeout no longer matters.
	Carrier->HandleInteractInput(Rig.Core);
	Rig.Core->Interact(Rig.Character);
	TestTrue(TEXT("Confirmed"), Carrier->GetCarried() == Rig.Carryable && !Carrier->GetPendingPickup());
	Advance(Rig, Timeout * 1.5f);
	TestTrue(TEXT("The claim stays while carried"), Movement->GetClaimedCarry() == EHollowCarryClaim::Solo);

	// Interact while carrying drops (not predicted: the claim follows the server's state).
	TestTrue(TEXT("Interact while carrying is consumed"), Carrier->HandleInteractInput(nullptr));
	TestTrue(TEXT("Dropped"), !Rig.Carryable->GetCarryState().IsCarried() && !Carrier->GetCarried()
		&& Movement->GetClaimedCarry() == EHollowCarryClaim::None);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
