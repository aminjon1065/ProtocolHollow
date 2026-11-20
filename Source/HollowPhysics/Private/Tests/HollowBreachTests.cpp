#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Breach/HollowBreachableBulkhead.h"
#include "Breach/HollowBreachableDefinition.h"
#include "Breach/HollowBreachRules.h"
#include "Breach/HollowStructuralIntegrityComponent.h"
#include "Collision/HollowCollisionChannels.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Destruction/HollowDestructionBudgetSubsystem.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Nav/HollowNavAreas.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Tags/HollowGameplayTags.h"
#include "Tests/AutomationCommon.h"
#include "UObject/Package.h"

namespace HollowBreachTests
{
	// Test data (the shipped numbers live in DA_Bulkhead_*).
	constexpr float ZoneHealth = 100.f;
	constexpr float ZoneHalfWidth = 75.f;
	constexpr float ZoneHalfHeight = 160.f;
	constexpr float ZoneHalfThickness = 10.f;
	constexpr float FreshWindow = 1.5f;
	constexpr int32 NetStateBits = 72;
	/** Impact direction after the byte quantization of yaw and pitch: within about a degree. */
	constexpr double DirectionTolerance = 0.02;

	const TCHAR* const TwinClass = TEXT("/Game/Hollow/World/Kit/Bulkheads/BP_Bulkhead_Twin.BP_Bulkhead_Twin_C");
	const TCHAR* const PlateClass = TEXT("/Game/Hollow/World/Kit/Bulkheads/BP_Bulkhead_Plate.BP_Bulkhead_Plate_C");
	const TCHAR* const CubeMesh = TEXT("/Engine/BasicShapes/Cube.Cube");
	constexpr double CubeSize = 100.0;
	constexpr float TickSeconds = 1.f / 60.f;
	constexpr int32 SettleFrames = 4;
	/** A capsule sweep through the bulkhead: from this far in front of it to as far behind. */
	constexpr double SweepReach = 150.0;

	/** Two side-by-side zones; the passage needs Required (both by default). */
	UHollowBreachableDefinition* MakeDefinition(const bool bSecondRequired = true)
	{
		UHollowBreachableDefinition* Definition = NewObject<UHollowBreachableDefinition>(GetTransientPackage());
		for (const double Y : {-ZoneHalfWidth, ZoneHalfWidth})
		{
			FHollowBreachZone& Zone = Definition->Zones.AddDefaulted_GetRef();
			Zone.Center = FVector(0.0, Y, ZoneHalfHeight);
			Zone.HalfSize = FVector(ZoneHalfThickness, ZoneHalfWidth, ZoneHalfHeight);
			Zone.MaxHealth = ZoneHealth;
		}
		Definition->Zones[1].bRequiredForPassage = bSecondRequired;
		Definition->FreshWindowSeconds = FreshWindow;
		return Definition;
	}

	FHollowBreachHit Hit(const double Y, const float Amount, const float Time = 0.f)
	{
		FHollowBreachHit Result;
		Result.LocalPoint = FVector(0.0, Y, ZoneHalfHeight);
		Result.Amount = Amount;
		Result.Direction = FVector(1.0, 0.0, 0.0);
		Result.ServerTime = Time;
		return Result;
	}

	FHollowDestructionBudget Budget()
	{
		FHollowDestructionBudget Result;
		Result.MaxLiveBreaches = 2;
		Result.MaxActiveBodies = 40;
		Result.MaxDebrisLifetime = 5.f;
		Result.MaxSimDistance = 2000.f;
		return Result;
	}

	/** A fresh break, near, on screen, nothing else live: the one case that plays live. */
	FHollowBreachPresentationInput FreshInput()
	{
		FHollowBreachPresentationInput Input;
		Input.AgeSeconds = 0.1;
		Input.FreshWindowSeconds = FreshWindow;
		Input.ViewDistance = 500.0;
		Input.bRecentlyRendered = true;
		Input.NewBodies = 20;
		return Input;
	}

	void Advance(FTestWorldWrapper& World, const float Seconds)
	{
		// One engine frame per step: the timer manager ticks once per GFrameCounter.
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += TickSeconds)
		{
			++GFrameCounter;
			World.TickTestWorld(TickSeconds);
		}
	}

	FHollowStructuralHit StructuralHit(const AHollowBreachableBulkhead& Bulkhead, const int32 Zone, const float Amount)
	{
		FHollowStructuralHit Result;
		Result.Amount = Amount;
		Result.Location = Bulkhead.GetZoneCenter(Zone);
		Result.Direction = Bulkhead.GetActorForwardVector();
		Result.DamageType = HollowTags::Damage_Structural;
		return Result;
	}

	/** Does a pawn capsule moving straight through Location get stopped (the CMC's sweep, Pawn channel and responses)? */
	bool PawnSweepBlocked(UWorld* World, const ACharacter& Pawn, const FVector& Location, const FVector& Across, FHitResult& OutHit)
	{
		const UCapsuleComponent* Capsule = Pawn.GetCapsuleComponent();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HollowBreachTest), false, &Pawn);
		FCollisionResponseParams Responses;
		Capsule->InitSweepCollisionParams(Params, Responses);
		return World->SweepSingleByChannel(OutHit, Location - Across * SweepReach, Location + Across * SweepReach, FQuat::Identity,
			Capsule->GetCollisionObjectType(), Capsule->GetCollisionShape(), Params, Responses);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowBreachRulesTest, "Hollow.Physics.Breach.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowBreachRulesTest::RunTest(const FString& Parameters)
{
	using namespace HollowBreachTests;
	const UHollowBreachableDefinition* Def = MakeDefinition();
	FHollowBreachServerState S = HollowBreachRules::MakeInitial(*Def);
	TestEqual(TEXT("Full health in every zone"), S.Health.Num(), 2);
	TestTrue(TEXT("Starts intact"), S.Net.State == EHollowBreachState::Intact && S.Net.ZoneMask == 0 && S.Net.DamageStageQ == 0);

	// Zone choice: nearest centre among the zones still standing, the lower index on a tie.
	TestEqual(TEXT("A blow on the left lands on zone 0"), HollowBreachRules::PickZone(S, Hit(-ZoneHalfWidth, 1.f).LocalPoint, *Def), 0);
	TestEqual(TEXT("A blow on the right lands on zone 1"), HollowBreachRules::PickZone(S, Hit(ZoneHalfWidth, 1.f).LocalPoint, *Def), 1);
	TestEqual(TEXT("A blow dead centre: the lower index"), HollowBreachRules::PickZone(S, Hit(0.0, 1.f).LocalPoint, *Def), 0);

	// Damage: Weakened at once, the stage follows the share of health lost, no zone broken yet.
	TestTrue(TEXT("No damage, no change"), HollowBreachRules::ApplyHit(S, Hit(0.0, 0.f), *Def) == EHollowBreachOutcome::Rejected);
	TestTrue(TEXT("Half a zone"), HollowBreachRules::ApplyHit(S, Hit(-ZoneHalfWidth, ZoneHealth * 0.5f), *Def) == EHollowBreachOutcome::Damaged);
	TestTrue(TEXT("Weakened, a quarter of the total lost"), S.Net.State == EHollowBreachState::Weakened && S.Net.ZoneMask == 0
		&& S.Net.DamageStageQ == HollowBreachRules::DamageStage(S, *Def) && FMath::IsNearlyEqual(S.Net.DamageStageQ / 255.f, 0.25f, 0.01f));
	const uint8 SerialBefore = S.Net.TransitionSerial;

	// One blow breaks one zone at most: the excess is lost, the other zone keeps its health.
	TestTrue(TEXT("A huge blow breaks zone 0"), HollowBreachRules::ApplyHit(S, Hit(-ZoneHalfWidth, ZoneHealth * 10.f, 42.f), *Def) == EHollowBreachOutcome::ZoneBroken);
	TestTrue(TEXT("Zone 0 broken, zone 1 untouched"), S.Net.IsZoneBroken(0) && !S.Net.IsZoneBroken(1) && S.Health[0] == 0.f && S.Health[1] == ZoneHealth);
	TestTrue(TEXT("Still shut: the passage needs both zones"), S.Net.State == EHollowBreachState::Weakened);
	TestTrue(TEXT("The break is stamped: serial and server time"), S.Net.TransitionSerial == SerialBefore + 1 && S.Net.BreachServerTime == 42.f);
	TestTrue(TEXT("The next blow on the left finds zone 1"), HollowBreachRules::PickZone(S, Hit(-ZoneHalfWidth, 1.f).LocalPoint, *Def) == 1);

	// The second zone opens the passage; Breached is terminal.
	TestTrue(TEXT("Zone 1 breaks"), HollowBreachRules::ApplyHit(S, Hit(ZoneHalfWidth, ZoneHealth), *Def) == EHollowBreachOutcome::ZoneBroken);
	TestTrue(TEXT("Breached, all health gone"), S.Net.State == EHollowBreachState::Breached && S.Net.ZoneMask == 0b11 && S.Net.DamageStageQ == 255);
	const FHollowBreachServerState Final = S;
	TestTrue(TEXT("Breached takes no more blows"), HollowBreachRules::ApplyHit(S, Hit(0.0, ZoneHealth), *Def) == EHollowBreachOutcome::Rejected);
	TestTrue(TEXT("...and nothing changed"), S.Net == Final.Net);
	TestEqual(TEXT("Nothing left to pick"), HollowBreachRules::PickZone(S, FVector::ZeroVector, *Def), INDEX_NONE);

	// A zone that is not required: breaking only the required one opens the passage.
	const UHollowBreachableDefinition* OneRequired = MakeDefinition(/*bSecondRequired=*/false);
	FHollowBreachServerState T = HollowBreachRules::MakeInitial(*OneRequired);
	HollowBreachRules::ApplyHit(T, Hit(ZoneHalfWidth, ZoneHealth), *OneRequired);
	TestTrue(TEXT("Only the optional zone broken: still shut"), T.Net.State == EHollowBreachState::Weakened && !HollowBreachRules::IsPassageOpen(T.Net.ZoneMask, *OneRequired));
	HollowBreachRules::ApplyHit(T, Hit(-ZoneHalfWidth, ZoneHealth), *OneRequired);
	TestTrue(TEXT("The required zone opens it"), T.Net.State == EHollowBreachState::Breached);
	TestTrue(TEXT("Newly broken = what has not been shown yet"), HollowBreachRules::NewlyBroken(0b01, T.Net) == 0b10 && HollowBreachRules::NewlyBroken(0b11, T.Net) == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowBreachNetStateTest, "Hollow.Physics.Breach.NetStateRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowBreachNetStateTest::RunTest(const FString& Parameters)
{
	using namespace HollowBreachTests;
	bool bAllEqual = true;
	bool bAllSized = true;
	for (const EHollowBreachState State : {EHollowBreachState::Intact, EHollowBreachState::Weakened, EHollowBreachState::Breached})
	{
		for (const uint8 Mask : {uint8(0), uint8(0b1), uint8(0b101010), uint8((1u << FHollowBreachNetState::MaxZones) - 1u)})
		{
			for (const uint8 Edge : {uint8(0), uint8(1), uint8(127), uint8(254), uint8(255)})
			{
				FHollowBreachNetState Sent;
				Sent.State = State;
				Sent.ZoneMask = Mask;
				Sent.DamageStageQ = Edge;
				Sent.ImpactYawQ = static_cast<uint8>(255 - Edge);
				Sent.ImpactPitchQ = Edge;
				Sent.BreachServerTime = 1234.567f + Edge;
				Sent.TransitionSerial = Edge;

				FBitWriter Writer(0, /*bAllowResize=*/true);
				bool bOk = false;
				Sent.NetSerialize(Writer, nullptr, bOk);
				FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
				FHollowBreachNetState Received;
				Received.NetSerialize(Reader, nullptr, bOk);
				bAllEqual &= bOk && Received == Sent;
				bAllSized &= Writer.GetNumBits() == NetStateBits;
			}
		}
	}
	TestTrue(TEXT("Every state, zone mask and edge value survives NetSerialize"), bAllEqual);
	TestTrue(TEXT("The net state is 9 bytes"), bAllSized);

	// The impact direction survives its two bytes closely enough to throw debris the right way.
	for (const FVector& Direction : {FVector(1.0, 0.0, 0.0), FVector(-1.0, 0.0, 0.0), FVector(0.3, -0.9, 0.2), FVector(0.0, 0.7, -0.7)})
	{
		FHollowBreachNetState State;
		State.SetImpactDirection(Direction);
		TestTrue(FString::Printf(TEXT("Direction %s survives quantization"), *Direction.ToCompactString()),
			State.GetImpactDirection().Equals(Direction.GetSafeNormal(), DirectionTolerance));
	}

	// Late join and regained relevancy: an old break snaps, whatever the budget.
	FHollowBreachPresentationInput Late = FreshInput();
	Late.AgeSeconds = FreshWindow * 20.0;
	TestTrue(TEXT("A late joiner snaps an old break"), HollowBreachPolicy::Choose(Late, Budget()) == EHollowBreachPresentation::Snap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowBreachPresentationPolicyTest, "Hollow.Physics.Breach.PresentationPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowBreachPresentationPolicyTest::RunTest(const FString& Parameters)
{
	using namespace HollowBreachTests;
	using EP = EHollowBreachPresentation;
	const FHollowDestructionBudget Limits = Budget();
	auto Choose = [&Limits](const FHollowBreachPresentationInput& Input) { return HollowBreachPolicy::Choose(Input, Limits); };
	FHollowBreachPresentationInput In = FreshInput();
	TestTrue(TEXT("Fresh, near, seen, within budget: Full"), Choose(In) == EP::Full);

	// Age.
	In = FreshInput(); In.AgeSeconds = FreshWindow;
	TestTrue(TEXT("Age at the fresh window: Snap (exclusive)"), Choose(In) == EP::Snap);
	In = FreshInput(); In.AgeSeconds = FreshWindow - TickSeconds;
	TestTrue(TEXT("Just inside the window: Full"), Choose(In) == EP::Full);
	In = FreshInput(); In.AgeSeconds = -0.2;
	TestTrue(TEXT("A client clock slightly behind the server: still fresh"), Choose(In) == EP::Full);

	// Distance and the viewer.
	In = FreshInput(); In.ViewDistance = Limits.MaxSimDistance;
	TestTrue(TEXT("At MaxSimDistance: Snap (exclusive)"), Choose(In) == EP::Snap);
	In = FreshInput(); In.ViewDistance.Reset();
	TestTrue(TEXT("Nobody watching (server-only machine): Snap"), Choose(In) == EP::Snap);
	In = FreshInput(); In.bRecentlyRendered = false;
	TestTrue(TEXT("Not on screen: Snap"), Choose(In) == EP::Snap);

	// Budget: slots and bodies.
	In = FreshInput(); In.LiveBreaches = Limits.MaxLiveBreaches;
	TestTrue(TEXT("Every live slot taken: Snap"), Choose(In) == EP::Snap);
	In = FreshInput(); In.LiveBreaches = Limits.MaxLiveBreaches - 1;
	TestTrue(TEXT("Last free slot: Full"), Choose(In) == EP::Full);
	In = FreshInput(); In.LiveBodies = Limits.MaxActiveBodies - In.NewBodies;
	TestTrue(TEXT("Bodies exactly at the cap: Full (inclusive)"), Choose(In) == EP::Full);
	In = FreshInput(); In.LiveBodies = Limits.MaxActiveBodies - In.NewBodies + 1;
	TestTrue(TEXT("One body over the cap: Snap"), Choose(In) == EP::Snap);
	TestTrue(TEXT("No budget at all (empty config): Snap"), HollowBreachPolicy::Choose(FreshInput(), FHollowDestructionBudget()) == EP::Snap);

	// The shipped table: one entry per effects quality, never shrinking with quality, clamped at both ends.
	const UHollowDestructionBudgetConfig* Config = GetDefault<UHollowDestructionBudgetConfig>();
	TestTrue(TEXT("A budget per effects quality (Low..Cinematic)"), Config->BudgetsByEffectsQuality.Num() >= 4);
	for (int32 Quality = 1; Quality < Config->BudgetsByEffectsQuality.Num(); ++Quality)
	{
		const FHollowDestructionBudget& Lower = Config->BudgetsByEffectsQuality[Quality - 1];
		const FHollowDestructionBudget& Higher = Config->BudgetsByEffectsQuality[Quality];
		TestTrue(FString::Printf(TEXT("Quality %d allows at least as much as %d"), Quality, Quality - 1), Higher.MaxLiveBreaches >= Lower.MaxLiveBreaches
			&& Higher.MaxActiveBodies >= Lower.MaxActiveBodies && Higher.MaxDebrisLifetime >= Lower.MaxDebrisLifetime && Higher.MaxSimDistance >= Lower.MaxSimDistance);
	}
	TestTrue(TEXT("Below the table: the lowest budget"), Config->GetBudget(-1).MaxLiveBreaches == Config->BudgetsByEffectsQuality[0].MaxLiveBreaches);
	TestTrue(TEXT("Above the table: the highest budget"), Config->GetBudget(99).MaxActiveBodies == Config->BudgetsByEffectsQuality.Last().MaxActiveBodies);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowBreachDebrisCollisionTest, "Hollow.Physics.Breach.DebrisCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowBreachDebrisCollisionTest::RunTest(const FString& Parameters)
{
	using namespace HollowBreachTests;
	FCollisionResponseTemplate Profile;
	if (!TestTrue(TEXT("The HollowDebris profile exists"), UCollisionProfile::Get()->GetProfileTemplate(HollowCollision::DebrisProfile(), Profile)))
	{
		return false;
	}
	TestTrue(TEXT("Debris is physics only (no query ever hits it)"), Profile.CollisionEnabled == ECollisionEnabled::PhysicsOnly);
	TestTrue(TEXT("Debris is its own object type"), Profile.ObjectType == HollowCollision::Debris);
	for (const ECollisionChannel Ignored : {ECC_Pawn, ECC_PhysicsBody, ECC_Visibility, ECC_Camera, HollowCollision::Interaction})
	{
		TestTrue(FString::Printf(TEXT("Debris ignores channel %d"), static_cast<int32>(Ignored)), Profile.ResponseToChannels.GetResponse(Ignored) == ECR_Ignore);
	}
	for (const ECollisionChannel Blocked : {ECC_WorldStatic, ECC_WorldDynamic, HollowCollision::Debris})
	{
		TestTrue(FString::Printf(TEXT("Debris rests on channel %d"), static_cast<int32>(Blocked)), Profile.ResponseToChannels.GetResponse(Blocked) == ECR_Block);
	}

	// In a world: walls stop debris, a pawn walks straight through a debris body (the CMC's own sweep).
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	World->InitializeActorsForPlay(FURL());
	const FVector DebrisAt(0.0, 0.0, CubeSize);
	AStaticMeshActor* Debris = World->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(DebrisAt));
	Debris->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Debris->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, CubeMesh));
	Debris->GetStaticMeshComponent()->SetCollisionProfileName(HollowCollision::DebrisProfile());
	Debris->FinishSpawning(FTransform(DebrisAt));
	const FVector WallAt(0.0, CubeSize * 10.0, CubeSize);
	AStaticMeshActor* Wall = World->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(WallAt));
	Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, CubeMesh));
	Wall->FinishSpawning(FTransform(WallAt));
	ACharacter* Pawn = World->SpawnActor<ACharacter>(ACharacter::StaticClass(), FTransform(FVector(-CubeSize * 5.0, 0.0, CubeSize)));
	if (!TestNotNull(TEXT("Test pawn"), Pawn))
	{
		return false;
	}

	TestTrue(TEXT("A plain wall blocks the Debris channel (debris settles on the world)"), Wall->GetStaticMeshComponent()->GetCollisionResponseToChannel(HollowCollision::Debris) == ECR_Block);
	FHitResult Hit;
	TestFalse(TEXT("A pawn's sweep passes through debris"), PawnSweepBlocked(World, *Pawn, DebrisAt, FVector::ForwardVector, Hit));
	TestTrue(TEXT("...while the same sweep stops at a wall"), PawnSweepBlocked(World, *Pawn, WallAt, FVector::ForwardVector, Hit) && Hit.GetActor() == Wall);
	FHitResult Trace;
	TestFalse(TEXT("Tool and sight traces pass through debris"), World->LineTraceSingleByChannel(Trace, DebrisAt - FVector::ForwardVector * SweepReach,
		DebrisAt + FVector::ForwardVector * SweepReach, ECC_Visibility));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowBreachBudgetTest, "Hollow.Physics.Breach.Budget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowBreachBudgetTest::RunTest(const FString& Parameters)
{
	using namespace HollowBreachTests;
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	World->InitializeActorsForPlay(FURL());
	UHollowDestructionBudgetSubsystem* Budget = World->GetSubsystem<UHollowDestructionBudgetSubsystem>();
	if (!TestNotNull(TEXT("Budget subsystem in a game world"), Budget))
	{
		return false;
	}
	const FHollowDestructionBudget Limits = Budget->GetBudget();
	if (!TestTrue(TEXT("This machine has a budget"), Limits.MaxLiveBreaches > 0 && Limits.MaxActiveBodies > 1 && Limits.MaxDebrisLifetime > 0.f))
	{
		return false;
	}
	TestFalse(TEXT("No local player: nobody watches"), Budget->GetViewLocation().IsSet());

	// Slots: every slot fills, the next one is refused.
	int32 Expired = 0;
	TArray<int32> Handles;
	for (int32 Slot = 0; Slot < Limits.MaxLiveBreaches; ++Slot)
	{
		Handles.Add(Budget->BeginLive(1, FSimpleDelegate::CreateLambda([&Expired]() { ++Expired; })));
	}
	TestFalse(TEXT("Every slot granted"), Handles.Contains(INDEX_NONE));
	TestEqual(TEXT("Over the slot limit: refused"), Budget->BeginLive(1, FSimpleDelegate()), INDEX_NONE);
	Budget->EndLive(Handles[0]);
	TestEqual(TEXT("Ending one frees a slot"), Budget->GetLiveBreaches(), Limits.MaxLiveBreaches - 1);
	Budget->EndLive(Handles[0]);
	TestEqual(TEXT("Ending it twice is harmless"), Budget->GetLiveBreaches(), Limits.MaxLiveBreaches - 1);

	// Lifetime: the rest expire on their own, once each.
	Advance(TestWorld, Limits.MaxDebrisLifetime + TickSeconds * SettleFrames);
	TestEqual(TEXT("Every live fracture expired after the debris lifetime"), Budget->GetLiveBreaches(), 0);
	TestEqual(TEXT("...each telling its owner once (the ended one never)"), Expired, Limits.MaxLiveBreaches - 1);

	// Bodies: a fracture bigger than the whole budget is refused.
	TestEqual(TEXT("Over the body cap: refused"), Budget->BeginLive(Limits.MaxActiveBodies + 1, FSimpleDelegate()), INDEX_NONE);
	const int32 Big = Budget->BeginLive(Limits.MaxActiveBodies, FSimpleDelegate());
	TestTrue(TEXT("Exactly the cap: granted"), Big != INDEX_NONE && Budget->GetLiveBodies() == Limits.MaxActiveBodies);
	TestEqual(TEXT("...and nothing fits beside it"), Budget->BeginLive(1, FSimpleDelegate()), INDEX_NONE);
	Budget->EndLive(Big);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowBreachBulkheadTest, "Hollow.Physics.Breach.Bulkhead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowBreachBulkheadTest::RunTest(const FString& Parameters)
{
	using namespace HollowBreachTests;
	const TSubclassOf<AHollowBreachableBulkhead> Twin = LoadClass<AHollowBreachableBulkhead>(nullptr, TwinClass);
	const TSubclassOf<AHollowBreachableBulkhead> Plate = LoadClass<AHollowBreachableBulkhead>(nullptr, PlateClass);
	FTestWorldWrapper TestWorld;
	if (!TestNotNull(TEXT("BP_Bulkhead_Twin"), Twin.Get()) || !TestNotNull(TEXT("BP_Bulkhead_Plate"), Plate.Get()) || !TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	World->InitializeActorsForPlay(FURL());
	AHollowBreachableBulkhead* Bulkhead = World->SpawnActor<AHollowBreachableBulkhead>(Twin, FTransform::Identity);
	Bulkhead->DispatchBeginPlay();
	const UHollowBreachableDefinition* Def = Bulkhead->GetIntegrity()->GetDefinition();
	ACharacter* Pawn = World->SpawnActor<ACharacter>(ACharacter::StaticClass(), FTransform(FVector(-SweepReach * 2.0, 0.0, CubeSize)));
	if (!TestNotNull(TEXT("Twin definition"), Def) || !TestEqual(TEXT("Twin: two zones"), Bulkhead->GetZoneCount(), 2) || !TestNotNull(TEXT("Test pawn"), Pawn))
	{
		return false;
	}
	auto Blocks = [Bulkhead](const int32 Zone) { return Bulkhead->GetPanel(Zone)->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block
		&& Bulkhead->GetPanel(Zone)->IsCollisionEnabled() && Bulkhead->GetPanel(Zone)->IsVisible(); };
	const FVector Across = Bulkhead->GetActorForwardVector();
	FHitResult Hit;

	// Intact: both panels block a pawn, the link carries the intact area, the blocker is in place.
	TestTrue(TEXT("Intact: both panels block"), Blocks(0) && Blocks(1));
	TestTrue(TEXT("Intact: a pawn cannot walk through zone 0"), PawnSweepBlocked(World, *Pawn, Bulkhead->GetZoneCenter(0), Across, Hit));
	TestTrue(TEXT("Intact nav link area"), Bulkhead->GetNavArea() == Def->AreaIntact && Bulkhead->IsNavBlockerActive());
	TestTrue(TEXT("It takes structural damage"), Bulkhead->CanTakeStructuralDamage());

	// A blow that breaks nothing: cracks only.
	TestTrue(TEXT("A light blow changes it"), Bulkhead->ApplyStructuralDamage(StructuralHit(*Bulkhead, 0, Def->Zones[0].MaxHealth * 0.5f)));
	const FHollowBreachNetState& State = Bulkhead->GetIntegrity()->GetNetState();
	TestTrue(TEXT("Weakened with cracks, nothing broken"), State.State == EHollowBreachState::Weakened && State.DamageStageQ > 0 && State.ZoneMask == 0 && Blocks(0));

	// Zone 0 breaks. Nobody watches in a test world, so it snaps: the panel is gone everywhere, rubble lies there.
	Bulkhead->ApplyStructuralDamage(StructuralHit(*Bulkhead, 0, Def->Zones[0].MaxHealth));
	TestTrue(TEXT("Zone 0 broken, the passage still shut"), State.IsZoneBroken(0) && State.State == EHollowBreachState::Weakened);
	TestTrue(TEXT("Zone 0 no longer blocks, zone 1 still does"), !Blocks(0) && Blocks(1));
	TestFalse(TEXT("A pawn walks through zone 0's hole"), PawnSweepBlocked(World, *Pawn, Bulkhead->GetZoneCenter(0), Across, Hit));
	TestTrue(TEXT("Snapped (no viewer here), rubble shown, no live debris"), Bulkhead->GetZonePresentation(0) == EHollowBreachPresentation::Snap
		&& Bulkhead->GetRubble(0)->IsVisible() && !Bulkhead->GetLiveFracture(0));
	TestTrue(TEXT("Rubble never blocks anyone"), !Bulkhead->GetRubble(0)->IsCollisionEnabled());
	TestTrue(TEXT("Shut: the link keeps the intact area"), Bulkhead->GetNavArea() == Def->AreaIntact && Bulkhead->IsNavBlockerActive());

	// Zone 1 breaks: Breached, the link opens at once, the blocker goes after its delay.
	Bulkhead->ApplyStructuralDamage(StructuralHit(*Bulkhead, 1, Def->Zones[1].MaxHealth));
	TestTrue(TEXT("Breached"), State.State == EHollowBreachState::Breached && State.ZoneMask == 0b11 && !Blocks(1));
	TestTrue(TEXT("The link switched to the breached area at once"), Bulkhead->GetNavArea() == Def->AreaBreached);
	TestTrue(TEXT("...the blocker is still there (it goes later)"), Def->BlockerRemovalDelay <= 0.f || Bulkhead->IsNavBlockerActive());
	Advance(TestWorld, Def->BlockerRemovalDelay + TickSeconds * SettleFrames);
	TestFalse(TEXT("The blocker left the navmesh after its delay"), Bulkhead->IsNavBlockerActive());
	TestFalse(TEXT("A breached bulkhead takes no more damage"), Bulkhead->CanTakeStructuralDamage() || Bulkhead->ApplyStructuralDamage(StructuralHit(*Bulkhead, 0, 1.f)));

	// Full, as a client near a fresh break would choose: live cosmetic debris on the Debris channel, within budget.
	UHollowDestructionBudgetSubsystem* Budget = World->GetSubsystem<UHollowDestructionBudgetSubsystem>();
	AHollowBreachableBulkhead* Live = World->SpawnActor<AHollowBreachableBulkhead>(Plate, FTransform(FVector(0.0, CubeSize * 10.0, 0.0)));
	Live->DispatchBeginPlay();
	UGeometryCollection* Collection = Live->GetIntegrity()->GetDefinition()->Zones[0].Fracture.LoadSynchronous();
	if (!TestNotNull(TEXT("Plate fracture collection"), Collection))
	{
		return false;
	}
	Live->PresentZoneBreak(0, EHollowBreachPresentation::Full, Live->GetActorForwardVector());
	UGeometryCollectionComponent* Fracture = Live->GetLiveFracture(0);
	if (!TestNotNull(TEXT("Full: a live fracture"), Fracture))
	{
		return false;
	}
	TestTrue(TEXT("Full: presented live, no rubble yet"), Live->GetZonePresentation(0) == EHollowBreachPresentation::Full && !Live->GetRubble(0)->IsVisible());
	TestTrue(TEXT("Debris profile and object type"), Fracture->GetCollisionProfileName() == HollowCollision::DebrisProfile()
		&& Fracture->GetCollisionObjectType() == HollowCollision::Debris && Fracture->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore);
	TestTrue(TEXT("Debris never touches navigation and is never replicated"), !Fracture->CanEverAffectNavigation() && !Fracture->GetIsReplicated());
	TestTrue(TEXT("The fracture is the zone's collection"), Fracture->GetRestCollection() == Collection);
	TestTrue(TEXT("Budget: one live fracture with its bodies"), Budget->GetLiveBreaches() == 1
		&& Budget->GetLiveBodies() == AHollowBreachableBulkhead::CountFractureBodies(Collection) && Budget->GetLiveBodies() > 1);
	Advance(TestWorld, TickSeconds * SettleFrames);
	TestFalse(TEXT("A pawn walks through the live debris"), PawnSweepBlocked(World, *Pawn, Live->GetZoneCenter(0), Live->GetActorForwardVector(), Hit));
	Advance(TestWorld, Budget->GetBudget().MaxDebrisLifetime);
	TestTrue(TEXT("After the debris lifetime: debris gone, rubble left, budget free"), !Live->GetLiveFracture(0) && Live->GetRubble(0)->IsVisible()
		&& Budget->GetLiveBreaches() == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowBreachLateJoinTest, "Hollow.Physics.Breach.LateJoin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowBreachLateJoinTest::RunTest(const FString& Parameters)
{
	// A machine that first receives a bulkhead already broken (relevancy regained, a slow loader): the net driver writes
	// NetState before BeginPlay, so BeginPlay must show the broken zones at once, without live debris.
	using namespace HollowBreachTests;
	const TSubclassOf<AHollowBreachableBulkhead> Twin = LoadClass<AHollowBreachableBulkhead>(nullptr, TwinClass);
	const FStructProperty* NetStateProperty = FindFProperty<FStructProperty>(UHollowStructuralIntegrityComponent::StaticClass(), TEXT("NetState"));
	FTestWorldWrapper TestWorld;
	if (!TestNotNull(TEXT("BP_Bulkhead_Twin"), Twin.Get()) || !TestNotNull(TEXT("Replicated NetState property"), NetStateProperty)
		|| !TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	World->InitializeActorsForPlay(FURL());
	Advance(TestWorld, FreshWindow * 2.f);   // the break below happened well before this machine saw the bulkhead

	FHollowBreachNetState Received;
	Received.State = EHollowBreachState::Breached;
	Received.ZoneMask = 0b11;
	Received.DamageStageQ = TNumericLimits<uint8>::Max();
	Received.SetImpactDirection(FVector::ForwardVector);
	Received.TransitionSerial = 2;
	Received.BreachServerTime = 0.f;
	AHollowBreachableBulkhead* Bulkhead = World->SpawnActorDeferred<AHollowBreachableBulkhead>(Twin, FTransform::Identity);
	UHollowStructuralIntegrityComponent* Integrity = Bulkhead->GetIntegrity();
	Integrity->GetDefinition()->Zones[0].Fracture.LoadSynchronous();   // even with the debris loaded, an old break snaps
	NetStateProperty->CopyCompleteValue(NetStateProperty->ContainerPtrToValuePtr<void>(Integrity), &Received);
	Bulkhead->FinishSpawning(FTransform::Identity);
	Bulkhead->DispatchBeginPlay();

	for (int32 Zone = 0; Zone < Bulkhead->GetZoneCount(); ++Zone)
	{
		UStaticMeshComponent* Panel = Bulkhead->GetPanel(Zone);
		TestTrue(FString::Printf(TEXT("Zone %d: snapped at BeginPlay"), Zone), Bulkhead->GetZonePresentation(Zone) == EHollowBreachPresentation::Snap);
		TestTrue(FString::Printf(TEXT("Zone %d: no panel, no collision, rubble shown, no live debris"), Zone), !Panel->IsVisible()
			&& !Panel->IsCollisionEnabled() && Bulkhead->GetRubble(Zone)->IsVisible() && !Bulkhead->GetLiveFracture(Zone));
	}
	TestEqual(TEXT("Nothing live on this machine"), World->GetSubsystem<UHollowDestructionBudgetSubsystem>()->GetLiveBreaches(), 0);
	TestFalse(TEXT("It takes no more damage"), Bulkhead->CanTakeStructuralDamage());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
