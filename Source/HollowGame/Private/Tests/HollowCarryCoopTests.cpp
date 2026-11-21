#include "Tests/HollowGameTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Attributes/HollowSurvivalSet.h"
#include "Camera/CameraComponent.h"
#include "Carry/HollowAetherCore.h"
#include "Carry/HollowCarrierComponent.h"
#include "Carry/HollowCarryCoop.h"
#include "Carry/HollowCarrySettings.h"
#include "Carry/HollowCarryableComponent.h"
#include "Carry/HollowCarryableDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameplayEffect.h"
#include "Movement/HollowCharacterMovementComponent.h"
#include "Tags/HollowGameplayTags.h"

namespace HollowCarryCoopGameTests
{
	// Content under test (create_carry_assets.py, create_gas_assets.py).
	const TCHAR* const CoreClass = TEXT("/Game/Hollow/Items/AetherCore/BP_AetherCore.BP_AetherCore_C");
	const TCHAR* const InitEffectClass = TEXT("/Game/Hollow/GAS/Effects/GE_ExoFrame_InitAttributes.GE_ExoFrame_InitAttributes_C");
	const TCHAR* const CubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");

	/** Test data. */
	constexpr float TickSeconds = 1.f / 60.f;
	constexpr double CoreDistance = 120.0;
	constexpr double PositionTolerance = 0.5;
	constexpr float CarrySeconds = 1.5f;
	/** The floor: a 60 x 60 m slab (the cube mesh is 100 cm), its top just under the spawned capsules. */
	const FVector FloorScale(60.0, 60.0, 1.0);
	constexpr double FloorGap = 1.0;
	/** A 10 cm wall, 200 x 200 cm. */
	const FVector WallScale(0.1, 2.0, 2.0);
	constexpr double WallHalfThickness = 5.0;
	/** How far the host walks off to break the tether. */
	constexpr double WalkAway = 500.0;
	/** A shift of a remote carrier's smoothed mesh (network smoothing). */
	constexpr double SmoothingShift = 20.0;

	struct FCoopRig
	{
		FTestWorldWrapper World;
		AHollowExoFrameCharacter* Host = nullptr;
		AHollowExoFrameCharacter* Other = nullptr;
		AHollowAetherCore* Core = nullptr;
		UHollowCarryableComponent* Carryable = nullptr;
		const UHollowCarryableDefinition* Definition = nullptr;
		TSubclassOf<UGameplayEffect> InitClass;
	};

	/** One engine frame per step: the timer manager ticks once per GFrameCounter (periodic effects, carry timers). */
	void Advance(FCoopRig& Rig, const float Seconds)
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += TickSeconds)
		{
			++GFrameCounter;
			Rig.World.TickTestWorld(TickSeconds);
		}
	}

	AStaticMeshActor* SpawnBlock(UWorld* World, const FVector& Centre, const FVector& Scale)
	{
		AStaticMeshActor* Block = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FRotator::ZeroRotator, Centre, Scale));
		Block->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);   // a registered static component refuses meshes in play
		Block->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, CubeMeshPath));
		return Block;
	}

	/** An ExoFrame possessed by a player (bLocal: this machine's), playing, with real init attributes. */
	AHollowExoFrameCharacter* SpawnPlayer(FAutomationTestBase& Test, FCoopRig& Rig, const FVector& Location, const bool bLocal)
	{
		UWorld* World = Rig.World.GetTestWorld();
		AHollowExoFrameCharacter* Player = HollowGameTests::SpawnExoFrame(Test, World, FTransform(Location));
		if (!Player)
		{
			return nullptr;
		}
		HollowGameTests::PossessAsPlayer(World, Player, bLocal);
		Player->DispatchBeginPlay();
		if (UAbilitySystemComponent* AbilitySystem = Player->GetAbilitySystemComponent())
		{
			AbilitySystem->ApplyGameplayEffectToSelf(Rig.InitClass.GetDefaultObject(), 1.f, AbilitySystem->MakeEffectContext());
		}
		return Player;
	}

	/** On a floor: the host (this machine's player, or a remote one), another remote player beside it, the real core in front. */
	bool Build(FAutomationTestBase& Test, FCoopRig& Rig, const bool bLocalHost)
	{
		UWorld* World = HollowGameTests::CreateWorld(Test, Rig.World);
		const TSubclassOf<AHollowAetherCore> Core = LoadClass<AHollowAetherCore>(nullptr, CoreClass);
		Rig.InitClass = LoadClass<UGameplayEffect>(nullptr, InitEffectClass);
		if (!World || !Test.TestNotNull(TEXT("BP_AetherCore"), Core.Get()) || !Test.TestNotNull(TEXT("GE_ExoFrame_InitAttributes"), Rig.InitClass.Get()))
		{
			return false;
		}
		if (bLocalHost)
		{
			// A local player sets up input; the bare C++ class has no InputConfig (BP_ExoFrame data), which it reports.
			Test.AddExpectedMessage(TEXT("InputConfig is not set"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
		}
		Rig.Host = SpawnPlayer(Test, Rig, FVector::ZeroVector, bLocalHost);
		Rig.Other = SpawnPlayer(Test, Rig, FVector(0.0, CoreDistance, 0.0), /*bLocal=*/false);
		if (!Test.TestNotNull(TEXT("Host"), Rig.Host) || !Test.TestNotNull(TEXT("Other player"), Rig.Other))
		{
			return false;
		}
		const double HalfHeight = Rig.Host->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		SpawnBlock(World, FVector(0.0, 0.0, -HalfHeight - FloorGap - FloorScale.Z * 50.0), FloorScale);
		Rig.Core = World->SpawnActor<AHollowAetherCore>(Core, FTransform(FVector(CoreDistance, 0.0, 0.0)));
		Rig.Core->DispatchBeginPlay();
		Rig.Carryable = Rig.Core->GetCarryable();
		Rig.Definition = Rig.Carryable->GetDefinition();
		return Test.TestNotNull(TEXT("Core definition"), Rig.Definition);
	}

	/** Where the server truth of a core carried by Primary and Secondary sits (HollowCarryCoop::Pose from the capsules). */
	FTransform Pose(const FCoopRig& Rig, const APawn& Primary, const APawn& Secondary)
	{
		return HollowCarryCoop::Pose(Primary.GetActorTransform(), Secondary.GetActorTransform(), Rig.Definition->GripOffset.GetLocation().Z);
	}

	bool IsCarriedBy(const FCoopRig& Rig, const APawn* Primary, const APawn* Secondary)
	{
		const FHollowCarryNetState& State = Rig.Carryable->GetCarryState();
		return State.Mode == (Secondary ? EHollowCarryMode::Coop : EHollowCarryMode::Solo) && State.Primary == Primary && State.Secondary == Secondary;
	}

	bool HasTag(const AHollowExoFrameCharacter& Player, const FGameplayTag& Tag)
	{
		const UAbilitySystemComponent* AbilitySystem = Player.GetAbilitySystemComponent();
		return AbilitySystem && AbilitySystem->HasMatchingGameplayTag(Tag);
	}

	float Stamina(const AHollowExoFrameCharacter& Player)
	{
		return Player.GetAbilitySystemComponent()->GetNumericAttribute(UHollowSurvivalSet::GetStaminaAttribute());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCarryCoopCycleTest, "Hollow.Game.Carry.CoopCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCarryCoopCycleTest::RunTest(const FString& Parameters)
{
	// S3-06 (ADR-0028): a second player takes the other side of a solo carry (Solo -> Coop) and every way out leaves the
	// other one carrying alone (Coop -> Solo), the last one out drops it. The host is this machine's player (claims,
	// prediction, its own view), the other a remote client (client-authoritative while it carries with someone).
	using namespace HollowCarryCoopGameTests;
	FCoopRig Rig;
	if (!Build(*this, Rig, /*bLocalHost=*/true))
	{
		return false;
	}
	AHollowExoFrameCharacter* Host = Rig.Host;
	AHollowExoFrameCharacter* Other = Rig.Other;
	UHollowCarrierComponent* HostCarrier = Host->GetCarrierComponent();
	UHollowCarrierComponent* OtherCarrier = Other->GetCarrierComponent();
	UHollowCharacterMovementComponent* HostMovement = Host->GetHollowMovement();
	UHollowCharacterMovementComponent* OtherMovement = Other->GetHollowMovement();
	UBoxComponent* Body = Rig.Core->GetBody();
	UStaticMeshComponent* Visual = Rig.Core->GetVisual();
	const FTransform VisualRest = Visual->GetRelativeTransform();
	const float Interval = GetDefault<UHollowCarrySettings>()->CoopSanityIntervalSeconds;
	TestTrue(TEXT("A co-op effect and tether are configured"), Rig.Definition->CoopEffect != nullptr && Rig.Definition->Coop.CarrierSpacing > 0.f
		&& !Rig.Definition->JoinPrompt.IsEmpty());

	// The other player lifts it alone.
	Rig.Core->Interact(Other);
	TestTrue(TEXT("Carried alone by the other player"), IsCarriedBy(Rig, Other, nullptr) && HasTag(*Other, HollowTags::State_CarryingHeavy_Solo));
	TestTrue(TEXT("The host may take the other side: the join prompt"), Rig.Core->CanInteract(Host)
		&& Rig.Core->GetInteractionInfo(Host).Prompt.EqualTo(Rig.Definition->JoinPrompt));
	TestFalse(TEXT("The solo carrier cannot join itself"), Rig.Carryable->CanJoin(Other));
	const float OtherSoloStart = Stamina(*Other);
	Advance(Rig, CarrySeconds);
	const float SoloDrain = OtherSoloStart - Stamina(*Other);

	// Joining is predicted: the host claims Coop at once (slower than free: the server accepts it).
	TestFalse(TEXT("Interact on a carried core is not consumed: it goes on to the server"), HostCarrier->HandleInteractInput(Rig.Core));
	TestTrue(TEXT("Predicted join: claims Coop"), HostCarrier->GetPendingPickup() == Rig.Carryable && HostCarrier->GetPendingClaim() == EHollowCarryClaim::Coop
		&& HostMovement->GetClaimedCarry() == EHollowCarryClaim::Coop);
	const uint8 SerialBefore = Rig.Carryable->GetCarryState().TransitionSerial;
	Rig.Core->Interact(Host);
	TestTrue(TEXT("Solo -> Coop: the lifter stays primary"), IsCarriedBy(Rig, Other, Host) && Rig.Carryable->GetCarryState().TransitionSerial == SerialBefore + 1);
	TestTrue(TEXT("Both know their partner"), HostCarrier->GetPartner() == Other && OtherCarrier->GetPartner() == Host && !HostCarrier->GetPendingPickup());
	TestTrue(TEXT("Both carry with GE_Carry_Coop, the solo effect is gone"), HasTag(*Host, HollowTags::State_CarryingHeavy_Coop)
		&& HasTag(*Other, HollowTags::State_CarryingHeavy_Coop) && !HasTag(*Other, HollowTags::State_CarryingHeavy_Solo));
	TestTrue(TEXT("Both hands full: tools blocked for both"), Host->GetAbilitySystemComponent()->AreAbilityTagsBlocked(FGameplayTagContainer(HollowTags::Ability_Tool_Welder))
		&& Other->GetAbilitySystemComponent()->AreAbilityTagsBlocked(FGameplayTagContainer(HollowTags::Ability_Tool_Welder)));
	TestTrue(TEXT("The server knows Coop for both; the host claims it"), HostMovement->GetServerCarry().Current == EHollowCarryClaim::Coop
		&& OtherMovement->GetServerCarry().Current == EHollowCarryClaim::Coop && HostMovement->GetClaimedCarry() == EHollowCarryClaim::Coop);
	TestTrue(TEXT("The remote carrier is client-authoritative, the host is the authority itself"), OtherMovement->IsClientAuthoritative()
		&& !HostMovement->IsClientAuthoritative());
	TestTrue(TEXT("Between the two: a world object, no first-person view"), Rig.Carryable->GetPresentation() == EHollowCarryPresentation::Coop
		&& Visual->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None && Body->GetAttachParent() == Other->GetCapsuleComponent());
	TestEqual(TEXT("The body midway between the capsules"), Body->GetComponentLocation(), Pose(Rig, *Other, *Host).GetLocation(), PositionTolerance);
	TestEqual(TEXT("The visual there too"), Visual->GetComponentLocation(), (VisualRest * Pose(Rig, *Other, *Host)).GetLocation(), PositionTolerance);
	AHollowExoFrameCharacter* Third = HollowCarryCoopGameTests::SpawnPlayer(*this, Rig, FVector(0.0, -CoreDistance, 0.0), /*bLocal=*/false);
	TestFalse(TEXT("Nobody joins a carry by two"), Rig.Core->CanInteract(Third));

	// The pose follows either carrier (their meshes' transform updates, no tick).
	Host->SetActorLocation(Host->GetActorLocation() + FVector(0.0, -CoreDistance * 0.25, 0.0));
	TestEqual(TEXT("A carrier moves: the body follows"), Body->GetComponentLocation(), Pose(Rig, *Other, *Host).GetLocation(), PositionTolerance);
	const float OtherCoopStart = Stamina(*Other);
	Advance(Rig, CarrySeconds);
	const float CoopDrain = OtherCoopStart - Stamina(*Other);
	TestTrue(TEXT("Carrying by two drains stamina, less than alone"), CoopDrain > 0.f && CoopDrain < SoloDrain);
	TestEqual(TEXT("After walking: still midway"), Body->GetComponentLocation(), Pose(Rig, *Other, *Host).GetLocation(), PositionTolerance);

	// The secondary lets go (Interact): the primary carries on alone, nothing drops.
	TestTrue(TEXT("Interact while carrying by two is consumed"), HostCarrier->HandleInteractInput(nullptr));
	TestTrue(TEXT("Coop -> Solo: the other carries alone"), IsCarriedBy(Rig, Other, nullptr) && !HostCarrier->IsCarrying());
	TestTrue(TEXT("...with the solo effect again; the host has none"), HasTag(*Other, HollowTags::State_CarryingHeavy_Solo)
		&& !HasTag(*Other, HollowTags::State_CarryingHeavy_Coop) && !HasTag(*Host, HollowTags::State_CarryingHeavy));
	TestTrue(TEXT("The host walks free; the other is on its way back to server authority"), HostMovement->GetClaimedCarry() == EHollowCarryClaim::None
		&& OtherMovement->GetServerCarry().Current == EHollowCarryClaim::Solo && OtherMovement->IsClientAuthoritative() && OtherMovement->IsReturningAuthority());
	TestTrue(TEXT("Shown on the other's body again"), Rig.Carryable->GetPresentation() == EHollowCarryPresentation::ThirdPerson
		&& Visual->GetAttachParent() == Other->GetMesh() && !Visual->IsUsingAbsoluteLocation());

	// Rejoin, then the primary lets go: the secondary becomes the primary.
	Rig.Core->Interact(Host);
	TestTrue(TEXT("Rejoined"), IsCarriedBy(Rig, Other, Host) && !OtherMovement->IsReturningAuthority());
	OtherCarrier->ServerDrop();
	TestTrue(TEXT("The primary lets go: the host carries alone"), IsCarriedBy(Rig, Host, nullptr));
	TestTrue(TEXT("Its own view: first person on its camera"), Rig.Carryable->GetPresentation() == EHollowCarryPresentation::FirstPerson
		&& Visual->GetAttachParent() == Host->GetFirstPersonCamera() && Body->GetAttachParent() == Host->GetCapsuleComponent());

	// The tether breaks: the one who walked away lets go.
	Rig.Core->Interact(Other);
	TestTrue(TEXT("The other joins the host"), IsCarriedBy(Rig, Host, Other));
	Host->SetActorLocation(Host->GetActorLocation() + FVector(-WalkAway, 0.0, 0.0));
	TestTrue(TEXT("Now beyond the tether"), HollowCarryTether::IsBroken(Host->GetActorLocation(), Other->GetActorLocation(), Rig.Definition->Coop));
	Advance(Rig, Interval + 2.f * TickSeconds);
	TestTrue(TEXT("Broken: the host walked away, the other carries alone"), IsCarriedBy(Rig, Other, nullptr) && !HostCarrier->IsCarrying());
	Host->SetActorLocation(FVector(0.0, 0.0, Host->GetActorLocation().Z));

	// A carrier dies or leaves (despawned) while carrying by two: the other carries alone.
	Rig.Core->Interact(Host);
	TestTrue(TEXT("The host joins again"), IsCarriedBy(Rig, Other, Host));
	Other->Destroy();
	TestTrue(TEXT("The other is gone: the host carries alone"), IsCarriedBy(Rig, Host, nullptr));

	// Unpossessed (a player left): the same.
	Rig.Core->Interact(Third);
	TestTrue(TEXT("A third player joins"), IsCarriedBy(Rig, Host, Third));
	Third->GetController()->UnPossess();
	TestTrue(TEXT("Unpossessed: the host carries alone"), IsCarriedBy(Rig, Host, nullptr));

	// The last one out drops it.
	HostCarrier->ServerDrop();
	TestTrue(TEXT("Solo -> None: free, simulating"), !Rig.Carryable->GetCarryState().IsCarried() && Body->IsSimulatingPhysics() && !HostCarrier->IsCarrying());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGameCarryCoopSanityTest, "Hollow.Game.Carry.CoopSanity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGameCarryCoopSanityTest::RunTest(const FString& Parameters)
{
	// ADR-0028: the server checks a client-authoritative carrier every CoopSanityIntervalSeconds. Inside the world,
	// through a wall or too fast: it lets go, is server-authoritative again and back where it last passed.
	using namespace HollowCarryCoopGameTests;
	FCoopRig Rig;
	if (!Build(*this, Rig, /*bLocalHost=*/true))
	{
		return false;
	}
	AHollowExoFrameCharacter* Other = Rig.Other;
	UHollowCharacterMovementComponent* Movement = Other->GetHollowMovement();
	const UHollowCarrySettings* Settings = GetDefault<UHollowCarrySettings>();
	const double Radius = Other->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const double Limit = Movement->GetCarryWalkSpeed(EHollowCarryClaim::Coop) * Settings->CoopSpeedTolerance * Settings->CoopSanityIntervalSeconds;
	const FVector Start = Other->GetActorLocation();
	auto Check = [&Rig, Settings]() { Advance(Rig, Settings->CoopSanityIntervalSeconds + 2.f * TickSeconds); };
	constexpr int32 Refusals = 3;   // inside, through, too fast: each refused and put back with a warning
	AddExpectedMessage(TEXT("sanity check refuses"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, Refusals, /*IsRegex=*/false);
	AddExpectedMessage(TEXT("server-authoritative again, put back"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, Refusals, /*IsRegex=*/false);
	Rig.Core->Interact(Rig.Host);

	// Inside a wall (a short step: not too fast, nothing between).
	AStaticMeshActor* Wall = SpawnBlock(Rig.World.GetTestWorld(), Start + FVector(Radius + 20.0 + WallHalfThickness, 0.0, 0.0), WallScale);
	Rig.Core->Interact(Other);
	TestTrue(TEXT("Joined: client-authoritative"), IsCarriedBy(Rig, Rig.Host, Other) && Movement->IsClientAuthoritative());
	Other->SetActorLocation(Start + FVector(40.0, 0.0, 0.0));
	Check();
	TestTrue(TEXT("Inside the wall: refused, the host carries alone"), IsCarriedBy(Rig, Rig.Host, nullptr));
	TestTrue(TEXT("...server-authoritative again at once"), !Movement->IsClientAuthoritative() && !Movement->IsReturningAuthority());
	TestEqual(TEXT("...back where it last passed"), Other->GetActorLocation(), Start, PositionTolerance);
	Wall->Destroy();

	// Through a wall: a step that ends clear of it, under the speed limit.
	const double Step = 2.0 * Radius + 20.0;
	TestTrue(TEXT("The step is under the speed limit"), Step < Limit);
	Wall = SpawnBlock(Rig.World.GetTestWorld(), Start + FVector(Radius + 10.0, 0.0, 0.0), WallScale);
	Rig.Core->Interact(Other);
	Other->SetActorLocation(Start + FVector(Step, 0.0, 0.0));
	Check();
	TestTrue(TEXT("Through the wall: refused and back"), IsCarriedBy(Rig, Rig.Host, nullptr) && !Movement->IsClientAuthoritative()
		&& Other->GetActorLocation().Equals(Start, PositionTolerance));
	Wall->Destroy();

	// Too fast (nothing in the way).
	Rig.Core->Interact(Other);
	Other->SetActorLocation(Start + FVector(-(Limit + 10.0), 0.0, 0.0));
	Check();
	TestTrue(TEXT("Too fast: refused and back"), IsCarriedBy(Rig, Rig.Host, nullptr) && !Movement->IsClientAuthoritative()
		&& Other->GetActorLocation().Equals(Start, PositionTolerance));

	// An honest step passes.
	Rig.Core->Interact(Other);
	Other->SetActorLocation(Start + FVector(-Limit * 0.5, 0.0, 0.0));
	Check();
	TestTrue(TEXT("An honest step: still carried by two, still client-authoritative"), IsCarriedBy(Rig, Rig.Host, Other) && Movement->IsClientAuthoritative());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCarryCoopPresentationTest, "Hollow.Game.Carry.CoopPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCarryCoopPresentationTest::RunTest(const FString& Parameters)
{
	// A core two players already carry reaches this machine (relevancy, a slow loader): BeginPlay shows it between
	// them and both learn their partner. The visual follows the carriers as this machine draws them (a remote pawn's
	// smoothed mesh), the body the capsules.
	using namespace HollowCarryCoopGameTests;
	FCoopRig Rig;
	if (!Build(*this, Rig, /*bLocalHost=*/false))
	{
		return false;
	}
	const FStructProperty* StateProperty = FindFProperty<FStructProperty>(UHollowCarryableComponent::StaticClass(), TEXT("CarryState"));
	if (!TestNotNull(TEXT("Replicated CarryState property"), StateProperty))
	{
		return false;
	}
	FHollowCarryNetState Received;
	Received.Mode = EHollowCarryMode::Coop;
	Received.Primary = Rig.Host;
	Received.Secondary = Rig.Other;
	Received.TransitionSerial = 5;
	const FTransform Spawn(FVector(-CoreDistance, 0.0, 0.0));
	AHollowAetherCore* Late = Rig.World.GetTestWorld()->SpawnActorDeferred<AHollowAetherCore>(Rig.Core->GetClass(), Spawn);
	StateProperty->CopyCompleteValue(StateProperty->ContainerPtrToValuePtr<void>(Late->GetCarryable()), &Received);
	Late->FinishSpawning(Spawn);
	const FTransform VisualRest = Late->GetVisual()->GetRelativeTransform();
	Late->DispatchBeginPlay();
	TestTrue(TEXT("Late: shown between the two"), Late->GetCarryable()->GetPresentation() == EHollowCarryPresentation::Coop
		&& Late->GetVisual()->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None);
	TestTrue(TEXT("Late: the body rides on the primary"), Late->GetBody()->GetAttachParent() == Rig.Host->GetCapsuleComponent());
	TestTrue(TEXT("Late: both learn their partner"), Rig.Host->GetCarrierComponent()->GetPartner() == Rig.Other
		&& Rig.Other->GetCarrierComponent()->GetPartner() == Rig.Host);
	const FVector BodyAt = Late->GetBody()->GetComponentLocation();
	const FVector VisualAt = Late->GetVisual()->GetComponentLocation();
	TestEqual(TEXT("Late: the body midway"), BodyAt, Pose(Rig, *Rig.Host, *Rig.Other).GetLocation(), PositionTolerance);
	TestEqual(TEXT("Late: the visual there too"), VisualAt, (VisualRest * Pose(Rig, *Rig.Host, *Rig.Other)).GetLocation(), PositionTolerance);

	// Network smoothing drags the other's mesh behind its capsule: the visual follows the mesh, the body the capsule.
	USkeletalMeshComponent* Mesh = Rig.Other->GetMesh();
	Mesh->SetRelativeLocation(Mesh->GetRelativeLocation() + FVector(SmoothingShift, 0.0, 0.0));
	TestEqual(TEXT("Smoothing: the visual moves half the shift (midway)"), Late->GetVisual()->GetComponentLocation(), VisualAt + FVector(SmoothingShift * 0.5, 0.0, 0.0), PositionTolerance);
	TestEqual(TEXT("Smoothing: the body stays on the capsules"), Late->GetBody()->GetComponentLocation(), BodyAt, PositionTolerance);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
