#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AI/NavigationSystemBase.h"
#include "Breach/HollowBreachableBulkhead.h"
#include "Breach/HollowBreachableDefinition.h"
#include "Breach/HollowStructuralIntegrityComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Doors/HollowDoor.h"
#include "Doors/HollowDoorDefinition.h"
#include "Doors/HollowDoorStateComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Nav/HollowDoorNav.h"
#include "Nav/HollowNavAreas.h"
#include "Nav/HollowNavSettings.h"
#include "Nav/HollowNavUpdateSubsystem.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavLinkCustomComponent.h"
#include "NavMesh/RecastNavMesh.h"
#include "Tests/AutomationCommon.h"
#include "UObject/Package.h"

namespace HollowNavTests
{
	const TCHAR* const DoorClass = TEXT("/Game/Hollow/World/Kit/Doors/BP_Door.BP_Door_C");
	const TCHAR* const BulkheadClass = TEXT("/Game/Hollow/World/Kit/Bulkheads/BP_Bulkhead_Twin.BP_Bulkhead_Twin_C");
	/** Upper bound for an async tile rebuild in the test scene (it takes a few frames). */
	constexpr float MaxRebuildSeconds = 5.f;
	const TCHAR* const CubeMesh = TEXT("/Engine/BasicShapes/Cube.Cube");
	constexpr double CubeSize = 100.0;
	constexpr float TickSeconds = 1.f / 60.f;
	/** Frames to let timers, replication of state and re-path requests settle after an action. */
	constexpr int32 SettleFrames = 4;
	/** Door area table test (Docs/Architecture/03 sec. 3.7): a threshold and a step just below it. */
	constexpr float HighThreshold = 0.5f;
	constexpr float BelowThreshold = 0.01f;

	/**
	 * Test scene (uu): a floor, a wall across it at WallX with the door in the middle, and a detour around the wall's
	 * south end. The detour is long enough that a Heavy agent still prefers breaching a fully welded door.
	 */
	constexpr double FloorMinX = 0.0, FloorMaxX = 2000.0, FloorMinY = -2300.0, FloorMaxY = 500.0;
	constexpr double WallX = 1000.0, WallHalfThickness = 20.0, WallHeight = 400.0, DoorHalfWidth = 150.0;
	constexpr double WallSouthEnd = -2000.0;
	constexpr double SlabThickness = 20.0;
	const FVector RoomA(500.0, 0.0, 50.0);
	const FVector RoomB(1500.0, 0.0, 50.0);

	void Advance(FTestWorldWrapper& World, const float Seconds)
	{
		// One engine frame per step: the timer manager ticks once per GFrameCounter.
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += TickSeconds)
		{
			++GFrameCounter;
			World.TickTestWorld(TickSeconds);
		}
	}

	AStaticMeshActor* SpawnBox(UWorld* World, const FVector& Min, const FVector& Max)
	{
		const FTransform Transform(FQuat::Identity, (Min + Max) * 0.5, (Max - Min) / CubeSize);
		AStaticMeshActor* Box = World->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
		Box->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
		Box->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, CubeMesh));
		Box->FinishSpawning(Transform);
		return Box;
	}

	struct FPathCheck
	{
		bool bFound = false;
		bool bThroughDoor = false;
		double Length = 0.0;
		FNavPathSharedPtr Path;
	};

	FPathCheck FindPath(UNavigationSystemV1* Nav, ANavigationData* NavData, const FSharedConstNavQueryFilter& QueryFilter, const FNavLinkId DoorLink)
	{
		FPathFindingQuery Query(nullptr, *NavData, RoomA, RoomB, QueryFilter);
		Query.SetAllowPartialPaths(false);
		const FPathFindingResult Result = Nav->FindPathSync(Query);
		FPathCheck Check;
		Check.bFound = Result.IsSuccessful() && Result.Path.IsValid() && !Result.IsPartial();
		if (Check.bFound)
		{
			Check.Path = Result.Path;
			Check.Length = Result.Path->GetLength();
			Check.bThroughDoor = UHollowNavUpdateSubsystem::PathUsesLink(*Result.Path, DoorLink);
		}
		return Check;
	}

	FPathCheck FindPath(UNavigationSystemV1* Nav, ANavigationData* NavData, const TSubclassOf<UNavigationQueryFilter>& Filter, const FNavLinkId DoorLink)
	{
		return FindPath(Nav, NavData, UNavigationQueryFilter::GetQueryFilter(*NavData, nullptr, Filter), DoorLink);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowNavDoorAreasTest, "Hollow.Physics.Nav.DoorAreas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowNavDoorAreasTest::RunTest(const FString& Parameters)
{
	using namespace HollowNavTests;
	UHollowDoorDefinition* Def = NewObject<UHollowDoorDefinition>(GetTransientPackage());
	Def->WeldedHighThreshold = HighThreshold;
	auto Area = [Def](const EHollowDoorState State, const float Weld = 0.f, const uint8 Flags = 0)
	{
		FHollowDoorNetState Net;
		Net.State = State;
		Net.Flags = Flags;
		return HollowDoorNav::AreaFor(Net, Weld, *Def).Get();
	};
	const uint8 Locked = static_cast<uint8>(EHollowDoorFlags::Locked);
	const uint8 Barricaded = static_cast<uint8>(EHollowDoorFlags::Barricaded);

	TestTrue(TEXT("Open is a doorway"), Area(EHollowDoorState::Open) == UHollowNavArea_Doorway::StaticClass());
	TestTrue(TEXT("Opening is already a doorway"), Area(EHollowDoorState::Opening) == UHollowNavArea_Doorway::StaticClass());
	TestTrue(TEXT("Breached is a doorway"), Area(EHollowDoorState::Breached, 0.f, Barricaded) == UHollowNavArea_Doorway::StaticClass());
	TestTrue(TEXT("Closed"), Area(EHollowDoorState::Closed) == UHollowNavArea_DoorClosed::StaticClass());
	TestTrue(TEXT("Closing counts as closed"), Area(EHollowDoorState::Closing) == UHollowNavArea_DoorClosed::StaticClass());
	TestTrue(TEXT("Closed + Locked"), Area(EHollowDoorState::Closed, 0.f, Locked) == UHollowNavArea_DoorLocked::StaticClass());
	TestTrue(TEXT("Welded below the threshold"), Area(EHollowDoorState::Welded, HighThreshold - BelowThreshold) == UHollowNavArea_DoorWeldedLow::StaticClass());
	TestTrue(TEXT("Welded at the threshold (inclusive)"), Area(EHollowDoorState::Welded, HighThreshold) == UHollowNavArea_DoorWeldedHigh::StaticClass());
	TestTrue(TEXT("Welded beats Locked"), Area(EHollowDoorState::Welded, 1.f, Locked) == UHollowNavArea_DoorWeldedHigh::StaticClass());
	TestTrue(TEXT("Barricaded beats Welded"), Area(EHollowDoorState::Welded, 1.f, Barricaded) == UHollowNavArea_DoorBarricaded::StaticClass());
	TestTrue(TEXT("Barricaded closed door"), Area(EHollowDoorState::Closed, 0.f, Barricaded | Locked) == UHollowNavArea_DoorBarricaded::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowNavDoorFiltersTest, "Hollow.Physics.Nav.DoorFilters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowNavDoorFiltersTest::RunTest(const FString& Parameters)
{
	using namespace HollowNavTests;
	const UHollowNavSettings* Settings = GetDefault<UHollowNavSettings>();
	const TSubclassOf<UNavigationQueryFilter> StandardFilter = Settings->StandardFilter.LoadSynchronous();
	const TSubclassOf<UNavigationQueryFilter> HeavyFilter = Settings->HeavyFilter.LoadSynchronous();
	const TSubclassOf<AHollowDoor> DoorBlueprint = LoadClass<AHollowDoor>(nullptr, DoorClass);
	FTestWorldWrapper TestWorld;
	if (!TestNotNull(TEXT("NAV_Filter_Standard"), StandardFilter.Get()) || !TestNotNull(TEXT("NAV_Filter_Heavy"), HeavyFilter.Get())
		|| !TestNotNull(TEXT("BP_Door"), DoorBlueprint.Get()) || !TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	// The AI crowd manager starts with the navigation system, before this test builds its navmesh, and says so.
	AddExpectedMessage(TEXT("Unable to find RecastNavMesh instance while trying to create UCrowdManager"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
	// Navigation first: InitializeActorsForPlay creates its octree, which every actor spawned afterwards registers with.
	FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::GameMode);
	World->InitializeActorsForPlay(FURL());
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!TestNotNull(TEXT("Navigation system in game mode"), Nav))
	{
		return false;
	}

	// Scene: floor, wall with an opening, the real door in it.
	SpawnBox(World, FVector(FloorMinX, FloorMinY, -SlabThickness), FVector(FloorMaxX, FloorMaxY, 0.0));
	SpawnBox(World, FVector(WallX - WallHalfThickness, DoorHalfWidth, 0.0), FVector(WallX + WallHalfThickness, FloorMaxY, WallHeight));
	SpawnBox(World, FVector(WallX - WallHalfThickness, WallSouthEnd, 0.0), FVector(WallX + WallHalfThickness, -DoorHalfWidth, WallHeight));
	AHollowDoor* Door = World->SpawnActor<AHollowDoor>(DoorBlueprint, FTransform(FVector(WallX, 0.0, 0.0)));
	Door->DispatchBeginPlay();
	UHollowDoorStateComponent* State = Door->GetDoorState();
	const UHollowDoorDefinition* Def = State->GetDefinition();

	// A test world has no NavMeshBoundsVolume (Build() regathers bounds from those): navigate the whole small scene.
	Nav->bWholeWorldNavigable = true;
	Nav->Build();
	ANavigationData* Standard = Nav->GetNavDataForAgentName(Settings->StandardAgent);
	ANavigationData* Heavy = Nav->GetNavDataForAgentName(Settings->HeavyAgent);
	if (!TestNotNull(TEXT("Standard navmesh"), Standard) || !TestNotNull(TEXT("Heavy navmesh"), Heavy) || !TestNotNull(TEXT("Door definition"), Def))
	{
		return false;
	}
	Standard->DispatchBeginPlay();   // they process re-path requests in their tick
	Heavy->DispatchBeginPlay();
	const FNavLinkId Link = Door->GetNavLink()->GetId();
	const int32 Tiles = Cast<ARecastNavMesh>(Standard)->GetNavMeshTilesCount();
	auto NoRebuild = [this, Nav, Standard, Tiles](const TCHAR* After)
	{
		TestFalse(*FString::Printf(TEXT("%s: no navigation rebuild queued"), After), Nav->IsNavigationDirty() || Nav->IsNavigationBuildInProgress());
		TestEqual(*FString::Printf(TEXT("%s: same tiles"), After), Cast<ARecastNavMesh>(Standard)->GetNavMeshTilesCount(), Tiles);
	};

	// Closed: both open the door.
	FPathCheck S = FindPath(Nav, Standard, StandardFilter, Link);
	FPathCheck H = FindPath(Nav, Heavy, HeavyFilter, Link);
	TestTrue(TEXT("Closed: Standard goes through the door"), S.bFound && S.bThroughDoor);
	TestTrue(TEXT("Closed: Heavy goes through the door"), H.bFound && H.bThroughDoor);
	const double DirectLength = S.Length;

	// Open: still through the door.
	Door->Interact(nullptr);
	Advance(TestWorld, Def->OpenDuration + TickSeconds * SettleFrames);
	NoRebuild(TEXT("Open"));
	TestTrue(TEXT("Open: Standard through the door"), FindPath(Nav, Standard, StandardFilter, Link).bThroughDoor);

	// A watched path through the door re-paths around it when the door is welded.
	Door->Interact(nullptr);
	Advance(TestWorld, Def->OpenDuration + TickSeconds * SettleFrames);
	FPathCheck Watched = FindPath(Nav, Standard, StandardFilter, Link);
	if (!TestTrue(TEXT("Closed again: a Standard path to watch"), Watched.bFound))
	{
		return false;
	}
	int32 PathEvents = 0;
	Watched.Path->AddObserver(FNavigationPath::FPathObserverDelegate::FDelegate::CreateLambda([&PathEvents](FNavigationPath*, ENavPathEvent::Type Event)
	{
		PathEvents += Event == ENavPathEvent::UpdatedDueToNavigationChanged ? 1 : 0;
	}));
	World->GetSubsystem<UHollowNavUpdateSubsystem>()->WatchPath(Watched.Path);

	// Welded (high): Standard detours, Heavy breaches.
	State->ApplyTool(EHollowDoorEvent::Weld, 1.f / Def->WeldPerWorkUnit);
	NoRebuild(TEXT("Welded"));
	S = FindPath(Nav, Standard, StandardFilter, Link);
	H = FindPath(Nav, Heavy, HeavyFilter, Link);
	TestTrue(TEXT("Welded: Standard takes the detour"), S.bFound && !S.bThroughDoor && S.Length > DirectLength);
	TestTrue(TEXT("Welded: Heavy breaks through"), H.bFound && H.bThroughDoor);
	Advance(TestWorld, TickSeconds * SettleFrames);
	TestTrue(TEXT("The watched path re-pathed around the welded door"), PathEvents > 0 && !UHollowNavUpdateSubsystem::PathUsesLink(*Watched.Path, Link));

	// Weakly welded: still closed to Standard, cheaper for Heavy.
	State->ApplyTool(EHollowDoorEvent::Cut, (1.f - Def->WeldedHighThreshold * 0.5f) / Def->CutPerWorkUnit);
	Advance(TestWorld, Def->WeldSendInterval * 2.f);
	NoRebuild(TEXT("Weakly welded"));
	TestTrue(TEXT("Weakly welded: Standard detours"), !FindPath(Nav, Standard, StandardFilter, Link).bThroughDoor);
	TestTrue(TEXT("Weakly welded: Heavy breaks through"), FindPath(Nav, Heavy, HeavyFilter, Link).bThroughDoor);

	// No detour: a welded door locks Standard agents out, Heavy still gets through. The full rebuild also proves the
	// octree keeps the door's current area (a tile rebuild must not bring back the area the link was gathered with).
	SpawnBox(World, FVector(WallX - WallHalfThickness, FloorMinY, 0.0), FVector(WallX + WallHalfThickness, WallSouthEnd, WallHeight));
	Nav->Build();
	TestFalse(TEXT("Welded, no detour: Standard has no path"), FindPath(Nav, Standard, StandardFilter, Link).bFound);
	TestTrue(TEXT("Welded, no detour: Heavy still breaks through"), FindPath(Nav, Heavy, HeavyFilter, Link).bThroughDoor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowNavBulkheadBreachTest, "Hollow.Physics.Nav.BulkheadBreach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowNavBulkheadBreachTest::RunTest(const FString& Parameters)
{
	using namespace HollowNavTests;
	const UHollowNavSettings* Settings = GetDefault<UHollowNavSettings>();
	const TSubclassOf<UNavigationQueryFilter> StandardFilter = Settings->StandardFilter.LoadSynchronous();
	const TSubclassOf<UNavigationQueryFilter> HeavyFilter = Settings->HeavyFilter.LoadSynchronous();
	const TSubclassOf<AHollowBreachableBulkhead> BulkheadBlueprint = LoadClass<AHollowBreachableBulkhead>(nullptr, BulkheadClass);
	FTestWorldWrapper TestWorld;
	if (!TestNotNull(TEXT("NAV_Filter_Standard"), StandardFilter.Get()) || !TestNotNull(TEXT("NAV_Filter_Heavy"), HeavyFilter.Get())
		|| !TestNotNull(TEXT("BP_Bulkhead_Twin"), BulkheadBlueprint.Get()) || !TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	AddExpectedMessage(TEXT("Unable to find RecastNavMesh instance while trying to create UCrowdManager"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
	FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::GameMode);
	World->InitializeActorsForPlay(FURL());
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!TestNotNull(TEXT("Navigation system in game mode"), Nav))
	{
		return false;
	}

	// Scene: floor, a wall right across it, the bulkhead in its only opening (no way around).
	SpawnBox(World, FVector(FloorMinX, FloorMinY, -SlabThickness), FVector(FloorMaxX, FloorMaxY, 0.0));
	SpawnBox(World, FVector(WallX - WallHalfThickness, DoorHalfWidth, 0.0), FVector(WallX + WallHalfThickness, FloorMaxY, WallHeight));
	SpawnBox(World, FVector(WallX - WallHalfThickness, FloorMinY, 0.0), FVector(WallX + WallHalfThickness, -DoorHalfWidth, WallHeight));
	AHollowBreachableBulkhead* Bulkhead = World->SpawnActor<AHollowBreachableBulkhead>(BulkheadBlueprint, FTransform(FVector(WallX, 0.0, 0.0)));
	Bulkhead->DispatchBeginPlay();
	const UHollowBreachableDefinition* Def = Bulkhead->GetIntegrity()->GetDefinition();
	Nav->bWholeWorldNavigable = true;
	Nav->Build();
	ANavigationData* Standard = Nav->GetNavDataForAgentName(Settings->StandardAgent);
	ANavigationData* Heavy = Nav->GetNavDataForAgentName(Settings->HeavyAgent);
	if (!TestNotNull(TEXT("Standard navmesh"), Standard) || !TestNotNull(TEXT("Heavy navmesh"), Heavy) || !TestNotNull(TEXT("Bulkhead definition"), Def))
	{
		return false;
	}
	Standard->DispatchBeginPlay();
	Heavy->DispatchBeginPlay();
	const FNavLinkId Link = Bulkhead->GetNavLink()->GetId();
	// Standard agents that may not use the link at all: they need the navmesh itself to cross the wall.
	const TSharedPtr<FNavigationQueryFilter> MeshOnly = UNavigationQueryFilter::GetQueryFilter(*Standard, nullptr, StandardFilter)->GetCopy();
	MeshOnly->SetExcludedArea(static_cast<uint8>(Standard->GetAreaID(Def->AreaBreached.Get())));
	MeshOnly->SetExcludedArea(static_cast<uint8>(Standard->GetAreaID(Def->AreaIntact.Get())));

	// Shut: Standard agents have no way through, Heavy plans to break it, the navmesh does not cross the wall.
	TestFalse(TEXT("Shut: Standard has no path"), FindPath(Nav, Standard, StandardFilter, Link).bFound);
	TestTrue(TEXT("Shut: Heavy breaks through"), FindPath(Nav, Heavy, HeavyFilter, Link).bThroughDoor);
	TestFalse(TEXT("Shut: the navmesh itself does not cross the wall"), FindPath(Nav, Standard, MeshOnly, Link).bFound);

	// Breached: the link opens at once, with no rebuild yet.
	for (int32 Zone = 0; Zone < Bulkhead->GetZoneCount(); ++Zone)
	{
		FHollowStructuralHit Blow;
		Blow.Amount = Def->Zones[Zone].MaxHealth;
		Blow.Location = Bulkhead->GetZoneCenter(Zone);
		Bulkhead->ApplyStructuralDamage(Blow);
	}
	if (!TestTrue(TEXT("Breached"), Bulkhead->GetIntegrity()->GetNetState().State == EHollowBreachState::Breached))
	{
		return false;
	}
	const FPathCheck Through = FindPath(Nav, Standard, StandardFilter, Link);
	TestTrue(TEXT("Breached: Standard goes through at once, over the link"), Through.bFound && Through.bThroughDoor);
	TestFalse(TEXT("Breached: no rebuild queued before the blocker goes"), Def->BlockerRemovalDelay > 0.f && (Nav->IsNavigationDirty() || Nav->IsNavigationBuildInProgress()));

	// The blocker goes: one bounded async rebuild, after which the navmesh crosses the opening by itself.
	Advance(TestWorld, Def->BlockerRemovalDelay + TickSeconds * SettleFrames);
	TestTrue(TEXT("Blocker removed: the opening's tiles are being rebuilt"), !Bulkhead->IsNavBlockerActive());
	for (float Waited = 0.f; (Nav->IsNavigationDirty() || Nav->IsNavigationBuildInProgress()) && Waited < MaxRebuildSeconds; Waited += TickSeconds)
	{
		Advance(TestWorld, TickSeconds);
	}
	TestFalse(TEXT("The rebuild finished"), Nav->IsNavigationDirty() || Nav->IsNavigationBuildInProgress());
	TestTrue(TEXT("After the rebuild the navmesh crosses the opening (no link needed)"), FindPath(Nav, Standard, MeshOnly, Link).bFound);
	TestTrue(TEXT("Heavy crosses too, without paying for a breach"), FindPath(Nav, Heavy, HeavyFilter, Link).bFound);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
