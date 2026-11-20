#include "Tests/HollowEnemyTestScene.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Assembly/HollowSectorManifest.h"
#include "Brain/HollowStalkerController.h"
#include "Components/SpotLightComponent.h"
#include "Data/HollowLevelMetrics.h"
#include "Data/HollowRoomDefinition.h"
#include "Enemies/HollowStalkerDefinition.h"
#include "EngineUtils.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "Gadgets/HollowGA_PulseMine.h"
#include "Gadgets/HollowPulseMine.h"
#include "Gadgets/HollowPulseMineDefinition.h"
#include "Noise/HollowNoiseSubsystem.h"
#include "Sector/HollowSectorTopologyBuilder.h"
#include "Sector/HollowSectorTopologySubsystem.h"
#include "Threat/HollowActivityHeatmapSubsystem.h"
#include "Threat/HollowThreatConfig.h"
#include "Threat/HollowThreatDirectorSubsystem.h"
#include "Threat/HollowVentNetworkSubsystem.h"

namespace HollowStalkerTests
{
	using namespace HollowEnemyTests;

	// Content under test (create_stalker_assets.py, create_gadget_assets.py, create_pilgrim_assets.py).
	const TCHAR* const StalkerClass = TEXT("/Game/Hollow/Characters/Enemies/EchoStalker/BP_EchoStalker.BP_EchoStalker_C");
	const TCHAR* const AmbushAttack = TEXT("/Game/Hollow/Characters/Enemies/EchoStalker/DA_Attack_Stalker_Ambush.DA_Attack_Stalker_Ambush");
	const TCHAR* const KineticEffect = TEXT("/Game/Hollow/GAS/Effects/GE_Damage_Kinetic.GE_Damage_Kinetic_C");
	const TCHAR* const MineClass = TEXT("/Game/Hollow/Gadgets/PulseMine/BP_PulseMine.BP_PulseMine_C");
	const TCHAR* const MineAbilityClass = TEXT("/Game/Hollow/Gadgets/PulseMine/GA_PulseMine.GA_PulseMine_C");

	// The scene's floor (X 0..2000, Y -2300..500) as sector rooms (test data): West of the wall line and East of it;
	// for the ambush, East split into South-East and North-East.
	constexpr double RoomTop = 400.0;
	constexpr double SplitY = -900.0;
	const FVector WestVent(150.0, 350.0, 0.0);
	const FVector SouthEastVent(1850.0, -2150.0, 0.0);
	const FVector NorthEastVent(1850.0, 350.0, 0.0);
	enum ERoom : int32 { West = 0, East = 1, SouthEast = 1, NorthEast = 2 };

	FHollowTopologyRoom Room(const double MinX, const double MinY, const double MaxX, const double MaxY)
	{
		return {FBox(FVector(MinX, MinY, 0.0), FVector(MaxX, MaxY, RoomTop))};
	}

	FHollowSectorTopology TwoRooms()
	{
		FHollowSectorTopology Topology;
		Topology.Rooms = {Room(FloorMinX, FloorMinY, WallX, FloorMaxY), Room(WallX, FloorMinY, FloorMaxX, FloorMaxY)};
		Topology.Portals = {{West, East, FVector(WallX, 0.0, 0.0), EHollowPortalKind::Open}};
		Topology.Vents = {{West, WestVent}, {East, SouthEastVent}};
		return Topology;
	}

	FHollowSectorTopology ThreeRooms()
	{
		FHollowSectorTopology Topology;
		Topology.Rooms = {Room(FloorMinX, FloorMinY, WallX, FloorMaxY), Room(WallX, FloorMinY, FloorMaxX, SplitY), Room(WallX, SplitY, FloorMaxX, FloorMaxY)};
		Topology.Portals = {{West, SouthEast, FVector(WallX, -1600.0, 0.0), EHollowPortalKind::Open},
			{West, NorthEast, FVector(WallX, -200.0, 0.0), EHollowPortalKind::Open},
			{SouthEast, NorthEast, FVector(1500.0, SplitY, 0.0), EHollowPortalKind::Open}};
		Topology.Vents = {{West, WestVent}, {SouthEast, SouthEastVent}, {NorthEast, NorthEastVent}};
		return Topology;
	}

	/** Test tuning: short timers; heat that stays put unless a test lets it decay. */
	UHollowThreatConfig* MakeConfig(const TSubclassOf<AHollowEnemyCharacter> Stalker = nullptr)
	{
		UHollowThreatConfig* Config = NewObject<UHollowThreatConfig>(GetTransientPackage());
		Config->Heatmap.SampleSeconds = 0.5f;
		Config->Heatmap.PresencePerSecond = 1.f;
		Config->Heatmap.NoisePerLoudness = 2.f;
		Config->Heatmap.CrossingHeat = 3.f;
		Config->Heatmap.HalfLifeSeconds = 1.0e6f;
		Config->Vents.MaxRoomLinks = 2;
		Config->Vents.SecondsPerLink = 1.f;
		Config->Director.StalkerClass = Stalker.Get();
		Config->Director.SpawnDelaySeconds = 1.f;
		Config->Director.SpawnSearchExtent = 300.f;
		Config->Director.StalkSeconds = 3.f;
		Config->Director.HuntHeatThreshold = 5.f;
		Config->Director.HuntSeconds = 2.f;
		Config->Director.RetreatSeconds = 1.5f;
		Config->Director.HeatCheckSeconds = 0.25f;
		return Config;
	}

	/** The threat subsystems of World use Config, then the sector topology is set (that starts them). */
	void StartThreat(UWorld* World, const UHollowThreatConfig* Config, const FHollowSectorTopology& Topology)
	{
		UHollowActivityHeatmapSubsystem::Get(World)->SetConfig(Config);
		UHollowVentNetworkSubsystem::Get(World)->SetConfig(Config);
		UHollowThreatDirectorSubsystem::Get(World)->SetConfig(Config);
		UHollowSectorTopologySubsystem::Get(World)->SetTopology(Topology);
	}

	AHollowStalkerController* BrainOf(const AHollowEnemyCharacter* Enemy)
	{
		return Enemy ? Cast<AHollowStalkerController>(Enemy->GetController()) : nullptr;
	}

	/** The player looks along Yaw (its lamp with it); the lamp gets the shipped cone of BP_ExoFrame (test data). */
	void Aim(AHollowExoFrameCharacter* Player, const double Yaw)
	{
		Player->GetController()->SetControlRotation(FRotator(0.0, Yaw, 0.0));
		Player->GetHelmetLight()->SetOuterConeAngle(26.f);
		Player->GetHelmetLight()->SetAttenuationRadius(1800.f);
	}

	float StunSeconds(const AHollowPulseMine* Mine)
	{
		float Seconds = 0.f;
		const UGameplayEffect* Stun = Mine && Mine->GetDefinition() && Mine->GetDefinition()->StunEffect ? Mine->GetDefinition()->StunEffect.GetDefaultObject() : nullptr;
		return Stun && Stun->DurationMagnitude.GetStaticMagnitudeIfPossible(1.f, Seconds) ? Seconds : 0.f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSectorTopologyBuilderTest, "Hollow.Game.Sector.Topology",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSectorTopologyBuilderTest::RunTest(const FString& Parameters)
{
	// The AI's view of an assembled sector (ADR-0019): room boxes from the layout, a portal per connection at its
	// opening, a vent per VentNode socket in world space (a turned room turns its sockets).
	const UHollowLevelMetrics& Metrics = *GetDefault<UHollowLevelMetrics>();
	const double Cell = Metrics.CellSize;
	const FVector Origin(1000.0, -500.0, 50.0);
	auto Socket = [](const FGameplayTag& Type, const FVector& Location)
	{
		FHollowRoomSocket Result;
		Result.SocketType = Type;
		Result.Transform = FTransform(Location);
		return Result;
	};
	UHollowRoomDefinition* Hall = NewObject<UHollowRoomDefinition>(GetTransientPackage());
	Hall->Footprint = FIntVector(2, 1, 1);
	Hall->Sockets = {Socket(HollowTags::Socket_Loot, FVector(50.0, 50.0, 0.0)), Socket(HollowTags::Socket_VentNode, FVector(100.0, 300.0, 0.0))};
	UHollowRoomDefinition* Corridor = NewObject<UHollowRoomDefinition>(GetTransientPackage());
	Corridor->Footprint = FIntVector(1, 2, 1);
	Corridor->Sockets = {Socket(HollowTags::Socket_VentNode, FVector(Cell * 0.25, Cell * 1.5, 0.0))};

	FHollowSectorManifest Manifest;
	Manifest.Rooms.SetNum(2);
	Manifest.Rooms[0].Room = Hall;
	Manifest.Rooms[1].Room = Corridor;
	FHollowSectorLayout Layout;
	Layout.Rooms.SetNum(2);
	Layout.Rooms[0].Cell = FIntPoint(0, 0);
	Layout.Rooms[0].Size = FIntPoint(2, 1);
	Layout.Rooms[1].Cell = FIntPoint(2, 0);
	Layout.Rooms[1].Size = FIntPoint(2, 1);   // the 1 x 2 corridor turned a quarter
	Layout.Rooms[1].Yaw90 = 1;
	FHollowPlacedConnection& Connection = Layout.Connections.AddDefaulted_GetRef();
	Connection.RoomA = 0;
	Connection.RoomB = 1;
	Connection.CellA = FIntPoint(1, 0);
	Connection.SideA = EHollowRoomSide::North;
	Connection.Kind = EHollowConnectorKind::Bulkhead;

	const FHollowSectorTopology Topology = HollowSectorTopologyBuilder::Build(Manifest, Layout, Metrics, Origin);
	if (!TestEqual(TEXT("A room per placed room"), Topology.Rooms.Num(), 2) || !TestEqual(TEXT("A portal per connection"), Topology.Portals.Num(), 1)
		|| !TestEqual(TEXT("A vent per VentNode socket (the loot socket is not one)"), Topology.Vents.Num(), 2))
	{
		return false;
	}
	TestTrue(TEXT("Hall bounds"), Topology.Rooms[0].Bounds == FBox(Origin, Origin + FVector(2.0 * Cell, Cell, Metrics.CellHeight)));
	TestTrue(TEXT("Turned corridor bounds: its rotated footprint"), Topology.Rooms[1].Bounds
		== FBox(Origin + FVector(2.0 * Cell, 0.0, 0.0), Origin + FVector(4.0 * Cell, Cell, Metrics.CellHeight)));
	const FHollowTopologyPortal& Portal = Topology.Portals[0];
	TestTrue(TEXT("The portal links the two rooms as a bulkhead"), Portal.RoomA == 0 && Portal.RoomB == 1 && Portal.Kind == EHollowPortalKind::Bulkhead);
	TestEqual(TEXT("...at the opening between them"), Portal.Location, Origin + FVector(2.0 * Cell, 0.5 * Cell, 0.0));
	TestEqual(TEXT("The hall's vent where its socket is"), Topology.Vents[0].Location, Origin + FVector(100.0, 300.0, 0.0));
	TestTrue(TEXT("The turned corridor's vent inside its room, on the floor"), Topology.Vents[1].Room == 1
		&& Topology.Rooms[1].Bounds.IsInsideOrOn(Topology.Vents[1].Location) && FMath::IsNearlyEqual(Topology.Vents[1].Location.Z, Origin.Z));
	TestEqual(TEXT("A room the portal touches"), Topology.RoomAt(Portal.Location), 0);
	TestTrue(TEXT("Mismatched manifest and layout: nothing"), HollowSectorTopologyBuilder::Build(FHollowSectorManifest(), Layout, Metrics, Origin).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowStalkerHeatmapTest, "Hollow.Game.Stalker.Heatmap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowStalkerHeatmapTest::RunTest(const FString& Parameters)
{
	// Activity heat (ADR-0019): living players warm the room they are in, crossing a portal warms it, players' noise
	// warms its room (by loudness), anything else's does not; all of it halves every HalfLifeSeconds.
	using namespace HollowStalkerTests;
	FEnemyScene Scene;
	if (!Scene.Create(*this, EWall::None))
	{
		return false;
	}
	AHollowExoFrameCharacter* Player = nullptr;
	if (!Scene.SpawnPlayer(*this, RoomA, Player))
	{
		return false;
	}
	UHollowThreatConfig* Config = MakeConfig();
	Config->Heatmap.SampleSeconds = 1.0e6f;   // the test samples by hand
	Config->Heatmap.HalfLifeSeconds = 10.f;
	StartThreat(Scene.World, Config, TwoRooms());
	UHollowActivityHeatmapSubsystem* Heatmap = UHollowActivityHeatmapSubsystem::Get(Scene.World);
	const FHollowHeatmapTuning& Tuning = Config->Heatmap;
	constexpr float Tolerance = 1.0e-3f;

	Heatmap->Sample(1.f);
	TestNearlyEqual(TEXT("A second in the West: presence"), Heatmap->GetRoomHeat(West), Tuning.PresencePerSecond, Tolerance);
	Heatmap->Sample(Tuning.HalfLifeSeconds);
	const float WestHeat = Tuning.PresencePerSecond * 0.5f + Tuning.PresencePerSecond * Tuning.HalfLifeSeconds;
	TestNearlyEqual(TEXT("A half-life later: halved, plus the new presence"), Heatmap->GetRoomHeat(West), WestHeat, Tolerance);
	TestEqual(TEXT("The East is cold"), Heatmap->GetRoomHeat(East), 0.f);
	TestEqual(TEXT("Hottest room: the West"), Heatmap->HottestRoom(/*bWithoutPlayers=*/false), static_cast<int32>(West));
	TestEqual(TEXT("...but nobody-in rooms have no heat"), Heatmap->HottestRoom(/*bWithoutPlayers=*/true), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("The squad is in the West"), Heatmap->SquadRoom() == West && Heatmap->IsOccupied(West) && !Heatmap->IsOccupied(East));

	Player->SetActorLocation(RoomB);
	Heatmap->Sample(1.f);
	const float Decay = FMath::Exp(-UE_LN2 / Tuning.HalfLifeSeconds);
	TestNearlyEqual(TEXT("Through the portal: crossing heat"), Heatmap->GetPortalHeat(0), Tuning.CrossingHeat, Tolerance);
	TestTrue(TEXT("...the portal of both rooms"), Heatmap->HottestPortalOf(West) == 0 && Heatmap->HottestPortalOf(East) == 0);
	TestNearlyEqual(TEXT("The West cools"), Heatmap->GetRoomHeat(West), WestHeat * Decay, Tolerance);
	TestEqual(TEXT("The hottest room nobody is in: the West now"), Heatmap->HottestRoom(/*bWithoutPlayers=*/true), static_cast<int32>(West));
	TestEqual(TEXT("The squad moved East"), Heatmap->SquadRoom(), static_cast<int32>(East));

	float Loudness = 0.f;
	const FDelegateHandle Listen = UHollowNoiseSubsystem::Get(Scene.World)->OnNoiseReported.AddLambda([&Loudness](const FHollowNoiseReport& Report) { Loudness = Report.Loudness; });
	const float Before = Heatmap->GetRoomHeat(West);
	TestTrue(TEXT("The player's blow in the West is reported"),
		UHollowNoiseSubsystem::Get(Scene.World)->ReportNoise(FHollowNoiseEvent(HollowTags::Noise_Impact_Heavy, RoomA, Player)));
	TestTrue(TEXT("...and warms the West by its loudness"), Loudness > 0.f
		&& FMath::IsNearlyEqual(Heatmap->GetRoomHeat(West), Before + Tuning.NoisePerLoudness * Loudness, Tolerance));
	AActor* Machine = Scene.World->SpawnActor<AActor>();
	const float EastBefore = Heatmap->GetRoomHeat(East);
	UHollowNoiseSubsystem::Get(Scene.World)->ReportNoise(FHollowNoiseEvent(HollowTags::Noise_Impact_Heavy, RoomB, Machine));
	TestEqual(TEXT("Noise that is not a player's leaves the heat alone"), Heatmap->GetRoomHeat(East), EastBefore);
	UHollowNoiseSubsystem::Get(Scene.World)->OnNoiseReported.Remove(Listen);

	UHollowSectorTopologySubsystem::Get(Scene.World)->SetTopology(FHollowSectorTopology());
	TestEqual(TEXT("A new (empty) sector starts cold"), Heatmap->GetRoomHeat(West), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowStalkerVentGraphTest, "Hollow.Game.Stalker.VentGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowStalkerVentGraphTest::RunTest(const FString& Parameters)
{
	// The vents as an abstract graph (ADR-0019): openings MaxRoomLinks rooms apart or closer are linked, a trip costs
	// SecondsPerLink per room (at least one), shortest trips add up, the far end is the one farthest from the players.
	using namespace HollowStalkerTests;
	FTestWorldWrapper TestWorld;
	UWorld* World = HollowGameTests::CreateWorld(*this, TestWorld);
	if (!World)
	{
		return false;
	}
	// Five rooms in a row (0-1-2-3-4) and one apart; vents in rooms 0, 1, 3, the apart one, and a second in room 0.
	constexpr double Size = 1000.0;
	FHollowSectorTopology Topology;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		Topology.Rooms.Add(Room(Index * Size, 0.0, (Index + 1) * Size, Size));
		if (Index > 0)
		{
			Topology.Portals.Add({Index - 1, Index, FVector(Index * Size, Size * 0.5, 0.0), EHollowPortalKind::Door});
		}
	}
	Topology.Rooms.Add(Room(10.0 * Size, 0.0, 11.0 * Size, Size));
	Topology.Vents = {{0, FVector(100.0, 500.0, 0.0)}, {1, FVector(1500.0, 500.0, 0.0)}, {3, FVector(3500.0, 500.0, 0.0)},
		{5, FVector(10500.0, 500.0, 0.0)}, {0, FVector(900.0, 100.0, 0.0)}};
	UHollowThreatConfig* Config = MakeConfig();
	Config->Vents.SecondsPerLink = 3.f;
	StartThreat(World, Config, Topology);
	const UHollowVentNetworkSubsystem* Vents = UHollowVentNetworkSubsystem::Get(World);
	const float Link = Config->Vents.SecondsPerLink;

	TestEqual(TEXT("An opening per vent"), Vents->NumNodes(), 5);
	TestEqual(TEXT("Links within two rooms: 0-1, 0-4 (same room), 1-2, 1-4"), Vents->GetLinks().Num(), 4);
	TestEqual(TEXT("Same room: one link's time"), Vents->TravelSeconds(0, 4), Link);
	TestEqual(TEXT("Two rooms apart: two links' time"), Vents->TravelSeconds(1, 2), 2.f * Link);
	TestEqual(TEXT("Three rooms apart: through the middle vent"), Vents->TravelSeconds(0, 2), 3.f * Link);
	TestEqual(TEXT("Shortest of the ways from the second vent of room 0"), Vents->TravelSeconds(4, 2), 3.f * Link);
	TestTrue(TEXT("The vent of the room apart is not linked"), Vents->TravelSeconds(0, 3) < 0.f);
	TestEqual(TEXT("Nowhere to go"), Vents->TravelSeconds(2, 2), 0.f);
	TestEqual(TEXT("Nearest opening"), Vents->NearestNode(FVector(1400.0, 450.0, 0.0)), 1);
	const TArray<FVector> Players = {FVector(1000.0, 500.0, 0.0)};
	TestEqual(TEXT("Far end reachable from 0: the vent of room 3"), Vents->FarthestNodeFrom(0, Players), 2);
	TestEqual(TEXT("Farthest of all (a spawn): the one apart"), Vents->FarthestNodeFrom(INDEX_NONE, Players), 3);
	TestEqual(TEXT("Alone: it stays"), Vents->FarthestNodeFrom(3, Players), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowStalkerDirectorTest, "Hollow.Game.Stalker.Director",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowStalkerDirectorTest::RunTest(const FString& Parameters)
{
	// Director-lite (ADR-0019): the stalker comes out of the vent farthest from the players SpawnDelaySeconds after
	// the sector is up; Stalk -> Hunt -> Retreat -> Stalk on its timers; a hot room nobody is in starts the hunt early;
	// a strike or a stun sends it back; a new sector takes it away.
	using namespace HollowStalkerTests;
	FEnemyScene Scene;
	const TSubclassOf<AHollowEnemyCharacter> Stalker = LoadClass<AHollowEnemyCharacter>(nullptr, StalkerClass);
	if (!TestNotNull(TEXT("BP_EchoStalker"), Stalker.Get()) || !Scene.Create(*this, EWall::None))
	{
		return false;
	}
	AHollowExoFrameCharacter* Player = nullptr;
	if (!Scene.SpawnPlayer(*this, RoomA, Player))
	{
		return false;
	}
	UHollowThreatConfig* Config = MakeConfig(Stalker);
	const FHollowDirectorTuning& Tuning = Config->Director;
	UHollowThreatDirectorSubsystem* Director = UHollowThreatDirectorSubsystem::Get(Scene.World);
	StartThreat(Scene.World, Config, TwoRooms());
	constexpr float Slack = 0.5f;

	Advance(Scene.TestWorld, Tuning.SpawnDelaySeconds * 0.5f);
	TestNull(TEXT("Not before the delay"), Director->GetStalker());
	Advance(Scene.TestWorld, Tuning.SpawnDelaySeconds, [Director] { return Director->GetStalker() != nullptr; });
	AHollowEnemyCharacter* Spawned = Director->GetStalker();
	if (!TestNotNull(TEXT("It comes after the delay"), Spawned))
	{
		return false;
	}
	TestTrue(TEXT("...out of the vent farthest from the player"), FVector::Dist2D(Spawned->GetActorLocation(), SouthEastVent) < Tuning.SpawnSearchExtent);
	TestTrue(TEXT("...stalking, with its brain"), Director->GetMode() == EHollowThreatMode::Stalk && BrainOf(Spawned));
	TestTrue(TEXT("Other enemies are never on the leash"), Director->GetModeFor(Player) == EHollowThreatMode::Stalk);

	Advance(Scene.TestWorld, Tuning.StalkSeconds + Slack, [Director] { return Director->GetMode() == EHollowThreatMode::Hunt; });
	TestTrue(TEXT("Hunts after StalkSeconds"), Director->GetMode() == EHollowThreatMode::Hunt);
	TestTrue(TEXT("...the brain knows"), BrainOf(Spawned)->IsHunting());
	Advance(Scene.TestWorld, Tuning.HuntSeconds + Slack, [Director] { return Director->GetMode() == EHollowThreatMode::Retreat; });
	TestTrue(TEXT("Retreats after HuntSeconds"), Director->GetMode() == EHollowThreatMode::Retreat);
	Advance(Scene.TestWorld, Tuning.RetreatSeconds + Slack, [Director] { return Director->GetMode() == EHollowThreatMode::Stalk; });
	TestTrue(TEXT("Stalks again after RetreatSeconds"), Director->GetMode() == EHollowThreatMode::Stalk);

	UHollowActivityHeatmapSubsystem::Get(Scene.World)->AddRoomHeat(East, Tuning.HuntHeatThreshold + 1.f);
	Advance(Scene.TestWorld, Tuning.HeatCheckSeconds + Slack, [Director] { return Director->GetMode() == EHollowThreatMode::Hunt; });
	TestTrue(TEXT("A hot room nobody is in: the hunt starts early"), Director->GetMode() == EHollowThreatMode::Hunt);
	Director->ReportStrike(Spawned);
	TestTrue(TEXT("Struck: back on the leash"), Director->GetMode() == EHollowThreatMode::Retreat);
	Director->SetMode(EHollowThreatMode::Stalk);
	Director->ReportStrike(Spawned);
	TestTrue(TEXT("A strike only ends a hunt"), Director->GetMode() == EHollowThreatMode::Stalk);
	Director->ReportStunned(Player);
	TestTrue(TEXT("Only its own stun counts"), Director->GetMode() == EHollowThreatMode::Stalk);
	Director->ReportStunned(Spawned);
	TestTrue(TEXT("Stunned: it goes"), Director->GetMode() == EHollowThreatMode::Retreat);

	UHollowSectorTopologySubsystem::Get(Scene.World)->SetTopology(FHollowSectorTopology());
	TestTrue(TEXT("A new sector: the stalker is gone"), !IsValid(Spawned) && !Director->GetStalker());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowStalkerAmbushQueryTest, "Hollow.Game.Stalker.AmbushQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowStalkerAmbushQueryTest::RunTest(const FString& Parameters)
{
	// EQ_StalkerFocusRoom (EQS): spots in the focus room, none in a lamp's cone, reachable, not on the hot portal, and
	// the best of them as close to it as that allows.
	using namespace HollowStalkerTests;
	FEnemyScene Scene;
	if (!Scene.Create(*this, EWall::None))
	{
		return false;
	}
	StartThreat(Scene.World, MakeConfig(), TwoRooms());
	AHollowEnemyCharacter* Stalker = Scene.SpawnEnemy(*this, StalkerClass, FVector(1700.0, -1800.0, 120.0));
	AHollowExoFrameCharacter* Player = nullptr;
	AHollowStalkerController* Brain = BrainOf(Stalker);
	const UHollowStalkerDefinition* Definition = Stalker ? Cast<UHollowStalkerDefinition>(Stalker->GetDefinition()) : nullptr;
	if (!TestNotNull(TEXT("Stalker brain"), Brain) || !TestNotNull(TEXT("Stalker definition"), Definition) || !Scene.SpawnPlayer(*this, FVector(900.0, 0.0, 150.0), Player))
	{
		return false;
	}
	Brain->Shutdown();   // the query alone
	Advance(Scene.TestWorld, StepSeconds * 0.1f);
	Aim(Player, 0.0);   // along +X, into the East
	Brain->SetFocusRoom(East);
	UHollowActivityHeatmapSubsystem::Get(Scene.World)->AddPortalHeat(0, 1.f);
	FHollowLightCone Cone;
	TestTrue(TEXT("The player's lamp is on"), Player->GetLightCone(Cone));

	const TSharedPtr<FEnvQueryResult> Result = UEnvQueryManager::GetCurrent(Scene.World)->RunInstantQuery(FEnvQueryRequest(Definition->FocusQuery, Stalker),
		EEnvQueryRunMode::AllMatching);
	if (!TestTrue(TEXT("Dark spots found"), Result.IsValid() && Result->IsSuccessful() && Result->Items.Num() > 0))
	{
		return false;
	}
	const FBox EastRoom = TwoRooms().Rooms[East].Bounds;
	const FVector Portal = TwoRooms().Portals[0].Location;
	constexpr double BodyHeight = 100.0;   // the test's PointHeight in EQ_StalkerFocusRoom
	constexpr double MinFromPortal = 150.0;
	constexpr double ProjectionSlack = 5.0;
	int32 Best = 0;
	bool bInRoom = true, bDark = true, bOffPortal = true;
	for (int32 Index = 0; Index < Result->Items.Num(); ++Index)
	{
		const FVector Spot = Result->GetItemAsLocation(Index);
		bInRoom &= EastRoom.ExpandBy(FVector(0.0, 0.0, ProjectionSlack)).IsInsideOrOn(Spot);
		bDark &= !Cone.Contains(Spot + FVector(0.0, 0.0, BodyHeight));
		bOffPortal &= FVector::Dist(Spot, Portal) >= MinFromPortal - ProjectionSlack;
		Best = Result->GetItemScore(Index) > Result->GetItemScore(Best) ? Index : Best;
	}
	TestTrue(TEXT("Every spot in the focus room"), bInRoom);
	TestTrue(TEXT("Every spot out of the lamp"), bDark);
	TestTrue(TEXT("Every spot off the portal"), bOffPortal);
	double Nearest = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index < Result->Items.Num(); ++Index)
	{
		Nearest = FMath::Min(Nearest, FVector::Dist(Result->GetItemAsLocation(Index), Portal));
	}
	TestNearlyEqual(TEXT("The best spot is the nearest to the hot portal"), FVector::Dist(Result->GetItemAsLocation(Best), Portal), Nearest, ProjectionSlack);
	int32 LitGrid = 0;
	for (double X = EastRoom.Min.X + 60.0; X <= EastRoom.Max.X - 60.0; X += 100.0)
	{
		LitGrid += Cone.Contains(FVector(X, 0.0, BodyHeight)) ? 1 : 0;
	}
	TestTrue(TEXT("...while the lamp does light part of the room"), LitGrid > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowStalkerAmbushInHottestRoomTest, "Hollow.Game.Stalker.AmbushInHottestRoom",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowStalkerAmbushInHottestRoomTest::RunTest(const FString& Parameters)
{
	// S4-01 acceptance, part 1: hunting, the stalker lies in wait in the hottest room nobody is in (not the squad's,
	// not a cooler one), out of the lamp; whoever comes close takes its occult strike (armor does not help); struck,
	// it goes back on the leash and into the vents.
	using namespace HollowStalkerTests;
	FEnemyScene Scene;
	const UHollowEnemyAttackDefinition* Attack = LoadObject<UHollowEnemyAttackDefinition>(nullptr, AmbushAttack);
	if (!TestNotNull(TEXT("DA_Attack_Stalker_Ambush"), Attack) || !Scene.Create(*this, EWall::None))
	{
		return false;
	}
	UHollowThreatConfig* Config = MakeConfig();
	Config->Director.HuntSeconds = 60.f;
	Config->Director.RetreatSeconds = 60.f;
	StartThreat(Scene.World, Config, ThreeRooms());
	AHollowExoFrameCharacter* Player = nullptr;
	UHollowAbilitySystemComponent* PlayerAbilities = Scene.SpawnPlayer(*this, RoomA, Player, VictimArmor);
	AHollowEnemyCharacter* Stalker = Scene.SpawnEnemy(*this, StalkerClass, FVector(1500.0, -1800.0, 120.0));
	AHollowStalkerController* Brain = BrainOf(Stalker);
	if (!PlayerAbilities || !TestNotNull(TEXT("Stalker brain"), Brain))
	{
		return false;
	}
	Aim(Player, 0.0);
	UHollowActivityHeatmapSubsystem* Heatmap = UHollowActivityHeatmapSubsystem::Get(Scene.World);
	Heatmap->AddRoomHeat(West, 50.f);        // the squad's own room: hottest, but they are in it
	Heatmap->AddRoomHeat(SouthEast, 10.f);
	Heatmap->AddRoomHeat(NorthEast, 30.f);   // the hottest room nobody is in
	Heatmap->AddPortalHeat(1, 5.f);          // West - North-East: where they come and go
	UHollowThreatDirectorSubsystem* Director = UHollowThreatDirectorSubsystem::Get(Scene.World);
	Director->Register(Stalker);
	Director->SetMode(EHollowThreatMode::Hunt);

	const FHollowSectorTopology Topology = ThreeRooms();
	auto Waiting = [Brain, Stalker, &Topology]
	{
		return Brain->GetActivity() == EHollowAIActivity::Ambush && Topology.RoomAt(Stalker->GetActorLocation()) == NorthEast
			&& Stalker->GetVelocity().Size2D() < 1.0;
	};
	Advance(Scene.TestWorld, StepSeconds * 2.f, Waiting);
	TestEqual(TEXT("It lies in wait in the hottest room nobody is in"), Brain->GetFocusRoom(), static_cast<int32>(NorthEast));
	TestTrue(TEXT("...there, still"), Waiting());
	FHollowLightCone Cone;
	TestTrue(TEXT("...out of the player's lamp"), Player->GetLightCone(Cone) && !Cone.Contains(Stalker->GetActorLocation()));

	// The player walks up to it.
	const float FullHealth = Health(PlayerAbilities);
	const FVector Toward = (Stalker->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
	const double Gap = Stalker->GetSimpleCollisionRadius() + Player->GetSimpleCollisionRadius() + Attack->Reach * 0.5;
	Player->SetActorLocation(Stalker->GetActorLocation() - Toward * Gap + FVector(0.0, 0.0, Player->GetActorLocation().Z - Stalker->GetActorLocation().Z));
	Aim(Player, Toward.Rotation().Yaw);
	Advance(Scene.TestWorld, StepSeconds * 0.3f, [PlayerAbilities, FullHealth] { return Health(PlayerAbilities) < FullHealth; });
	TestNearlyEqual(TEXT("The ambush strikes: occult damage, armor does not stop it"), Health(PlayerAbilities), FullHealth - Attack->Damage, KINDA_SMALL_NUMBER);
	Advance(Scene.TestWorld, StepSeconds * 0.3f, [Director] { return Director->GetMode() == EHollowThreatMode::Retreat; });
	TestTrue(TEXT("Struck: back on the leash"), Director->GetMode() == EHollowThreatMode::Retreat);
	Advance(Scene.TestWorld, StepSeconds * 2.f, [Stalker] { return Stalker->IsInVent(); });
	TestTrue(TEXT("...into the vents: hidden, out of reach"), Stalker->IsInVent() && Stalker->IsHidden() && !Stalker->GetActorEnableCollision());
	TestTrue(TEXT("...its brain retreating"), Brain->GetActivity() == EHollowAIActivity::Retreat);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowStalkerStunAndImmunityTest, "Hollow.Game.Stalker.StunAndImmunity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowStalkerStunAndImmunityTest::RunTest(const FString& Parameters)
{
	// S4-01 acceptance, part 2: kinetic damage does nothing to the stalker (Hollow.State.Invulnerable.Kinetic); a pulse
	// mine stuns it, and the Pilgrim, for the stun effect's N seconds: they stop, cannot attack, and come round after
	// N s; the stalker's director sends it away.
	using namespace HollowStalkerTests;
	FEnemyScene Scene;
	const TSubclassOf<UGameplayEffect> Kinetic = LoadClass<UGameplayEffect>(nullptr, KineticEffect);
	const TSubclassOf<AHollowPulseMine> Mine = LoadClass<AHollowPulseMine>(nullptr, MineClass);
	if (!TestNotNull(TEXT("GE_Damage_Kinetic"), Kinetic.Get()) || !TestNotNull(TEXT("BP_PulseMine"), Mine.Get()) || !Scene.Create(*this, EWall::None))
	{
		return false;
	}
	UHollowThreatConfig* Config = MakeConfig();
	Config->Director.HuntSeconds = 60.f;
	Config->Director.RetreatSeconds = 60.f;
	StartThreat(Scene.World, Config, TwoRooms());
	AHollowExoFrameCharacter* Player = nullptr;
	UHollowAbilitySystemComponent* PlayerAbilities = Scene.SpawnPlayer(*this, RoomA, Player);
	AHollowEnemyCharacter* Stalker = Scene.SpawnEnemy(*this, StalkerClass, FVector(1500.0, -1000.0, 120.0));
	AHollowEnemyCharacter* Pilgrim = Scene.SpawnPilgrim(*this, FVector(1500.0, -1300.0, 150.0));
	AHollowStalkerController* Brain = BrainOf(Stalker);
	if (!PlayerAbilities || !TestNotNull(TEXT("Stalker brain"), Brain) || !Pilgrim)
	{
		return false;
	}
	Aim(Player, 180.0);   // looking away: neither sees nor lights them
	UHollowThreatDirectorSubsystem* Director = UHollowThreatDirectorSubsystem::Get(Scene.World);
	Director->Register(Stalker);
	Director->SetMode(EHollowThreatMode::Hunt);
	Advance(Scene.TestWorld, StepSeconds * 0.1f);
	UAbilitySystemComponent* StalkerAbilities = Stalker->GetAbilitySystemComponent();
	UAbilitySystemComponent* PilgrimAbilities = Pilgrim->GetAbilitySystemComponent();

	// Kinetic immunity.
	auto Hit = [PlayerAbilities, &Kinetic](UAbilitySystemComponent* Target)
	{
		constexpr float Damage = 50.f;
		const FGameplayEffectSpecHandle Spec = PlayerAbilities->MakeOutgoingSpec(Kinetic, 1.f, PlayerAbilities->MakeEffectContext());
		Spec.Data->SetSetByCallerMagnitude(HollowTags::Data_Damage, Damage);
		PlayerAbilities->ApplyGameplayEffectSpecToTarget(*Spec.Data, Target);
	};
	TestTrue(TEXT("The stalker's traits: kinetic immunity"), StalkerAbilities->HasMatchingGameplayTag(HollowTags::State_Invulnerable_Kinetic));
	const float StalkerHealth = Health(StalkerAbilities);
	const float PilgrimHealth = Health(PilgrimAbilities);
	Hit(StalkerAbilities);
	Hit(PilgrimAbilities);
	TestEqual(TEXT("A kinetic hit does nothing to the stalker"), Health(StalkerAbilities), StalkerHealth);
	TestTrue(TEXT("...while it hurts the Pilgrim"), Health(PilgrimAbilities) < PilgrimHealth);

	// The pulse.
	FActorSpawnParameters Params;
	Params.Instigator = Player;
	AHollowPulseMine* Pulse = Scene.World->SpawnActor<AHollowPulseMine>(Mine, FTransform(FVector(1500.0, -1150.0, 0.0)), Params);
	const float N = StunSeconds(Pulse);
	if (!TestNotNull(TEXT("Mine"), Pulse) || !TestTrue(TEXT("The stun lasts N > 0 s (GE_State_Stunned)"), N > 0.f))
	{
		return false;
	}
	TestEqual(TEXT("The blast stuns both"), Pulse->Detonate(), 2);
	TestTrue(TEXT("...stunned"), StalkerAbilities->HasMatchingGameplayTag(HollowTags::State_Stunned) && PilgrimAbilities->HasMatchingGameplayTag(HollowTags::State_Stunned));
	TestTrue(TEXT("Stunned, the stalker is sent away"), Director->GetMode() == EHollowThreatMode::Retreat);
	Advance(Scene.TestWorld, StepSeconds * 0.05f);
	const AHollowAIController* PilgrimBrain = Cast<AHollowAIController>(Pilgrim->GetController());
	TestTrue(TEXT("Both brains are stunned"), Brain->GetActivity() == EHollowAIActivity::Stunned && PilgrimBrain && PilgrimBrain->GetActivity() == EHollowAIActivity::Stunned);
	FGameplayEventData Payload;
	Payload.Instigator = Stalker;
	Payload.Target = Player;
	TestEqual(TEXT("...and cannot strike"), StalkerAbilities->HandleGameplayEvent(HollowTags::Event_Enemy_Ambush, &Payload), 0);
	TestTrue(TEXT("...nor move"), Stalker->GetVelocity().Size2D() < 1.0 && Pilgrim->GetVelocity().Size2D() < 1.0);
	Advance(Scene.TestWorld, N - StepSeconds * 0.1f);
	TestTrue(TEXT("Still stunned just before N s"), StalkerAbilities->HasMatchingGameplayTag(HollowTags::State_Stunned));
	Advance(Scene.TestWorld, StepSeconds * 0.1f);
	TestFalse(TEXT("Over after N s"), StalkerAbilities->HasMatchingGameplayTag(HollowTags::State_Stunned) || PilgrimAbilities->HasMatchingGameplayTag(HollowTags::State_Stunned));
	Advance(Scene.TestWorld, StepSeconds * 0.05f);
	TestTrue(TEXT("The stalker comes round and retreats"), Brain->GetActivity() == EHollowAIActivity::Retreat);
	TestTrue(TEXT("The Pilgrim comes round too"), PilgrimBrain->GetActivity() != EHollowAIActivity::Stunned);
	Advance(Scene.TestWorld, Pulse->GetDefinition()->FlashSeconds);
	TestFalse(TEXT("The spent mine is gone"), IsValid(Pulse));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGadgetPulseMineTest, "Hollow.Game.Gadget.PulseMine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGadgetPulseMineTest::RunTest(const FString& Parameters)
{
	// GA_PulseMine (ADR-0019): a mine on the floor at the feet, then the cooldown; one mine per player (a new one takes
	// the old one back); armed after ArmSeconds; players never set it off, an enemy in TriggerRadius does; the blast
	// is the player's noise; the spent mine goes.
	using namespace HollowStalkerTests;
	FEnemyScene Scene;
	const TSubclassOf<UGameplayAbility> Ability = LoadClass<UGameplayAbility>(nullptr, MineAbilityClass);
	if (!TestNotNull(TEXT("GA_PulseMine"), Ability.Get()) || !Scene.Create(*this, EWall::None))
	{
		return false;
	}
	AHollowExoFrameCharacter* Player = nullptr;
	UHollowAbilitySystemComponent* Abilities = Scene.SpawnPlayer(*this, RoomA, Player, 0.f, /*bLocalPlayer=*/true);
	if (!Abilities)
	{
		return false;
	}
	Abilities->GiveAbility(FGameplayAbilitySpec(Ability, 1));
	Advance(Scene.TestWorld, StepSeconds * 0.1f);   // on the floor
	auto Mines = [&Scene]
	{
		TArray<AHollowPulseMine*> Found;
		for (TActorIterator<AHollowPulseMine> It(Scene.World); It; ++It)
		{
			if (IsValid(*It))
			{
				Found.Add(*It);
			}
		}
		return Found;
	};

	TestTrue(TEXT("Laid"), Abilities->TryActivateAbilityByClass(Ability));
	TArray<AHollowPulseMine*> Laid = Mines();
	if (!TestEqual(TEXT("One mine"), Laid.Num(), 1))
	{
		return false;
	}
	AHollowPulseMine* First = Laid[0];
	const UHollowPulseMineDefinition* Definition = First->GetDefinition();
	constexpr double FloorSlack = 5.0;
	TestTrue(TEXT("...on the floor at the feet"), FMath::Abs(First->GetActorLocation().Z) < FloorSlack
		&& FVector::Dist2D(First->GetActorLocation(), Player->GetActorLocation()) < FloorSlack);
	TestTrue(TEXT("...the player's"), First->GetInstigator() == Player);
	TestTrue(TEXT("Cooldown"), Abilities->HasMatchingGameplayTag(HollowTags::Cooldown_Gadget_PulseMine));
	TestFalse(TEXT("No second mine during the cooldown"), Abilities->TryActivateAbilityByClass(Ability));
	TestEqual(TEXT("...still one"), Mines().Num(), 1);

	Abilities->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(HollowTags::Cooldown_Gadget_PulseMine));
	Player->SetActorLocation(Player->GetActorLocation() + FVector(0.0, 300.0, 0.0));
	TestTrue(TEXT("After the cooldown: laid again"), Abilities->TryActivateAbilityByClass(Ability));
	Laid = Mines();
	TestTrue(TEXT("One mine per player: the old one is taken back"), !IsValid(First) && Laid.Num() == 1);
	AHollowPulseMine* Second = Laid.Num() == 1 ? Laid[0] : nullptr;
	if (!TestNotNull(TEXT("The new mine"), Second))
	{
		return false;
	}
	Advance(Scene.TestWorld, Definition->ArmSeconds * 0.5f);
	TestFalse(TEXT("Not armed at once"), Second->IsArmed());
	Advance(Scene.TestWorld, Definition->ArmSeconds);
	TestTrue(TEXT("Armed after ArmSeconds"), Second->IsArmed());
	Advance(Scene.TestWorld, Definition->CheckSeconds * 3.f);
	TestFalse(TEXT("The player standing on it never sets it off"), Second->HasDetonated());

	FGameplayTag Heard;
	const AActor* HeardFrom = nullptr;
	const FDelegateHandle Listen = UHollowNoiseSubsystem::Get(Scene.World)->OnNoiseReported.AddLambda([&Heard, &HeardFrom](const FHollowNoiseReport& Report)
	{
		Heard = Report.Event.NoiseTag;
		HeardFrom = Report.Event.Instigator.Get();
	});
	AHollowEnemyCharacter* Pilgrim = Scene.SpawnPilgrim(*this, Second->GetActorLocation() + FVector(Definition->TriggerRadius * 0.8, 0.0, 150.0));
	if (!Pilgrim)
	{
		return false;
	}
	Cast<AHollowAIController>(Pilgrim->GetController())->Shutdown();
	Advance(Scene.TestWorld, Definition->CheckSeconds * 3.f, [Second] { return Second->HasDetonated(); });
	TestTrue(TEXT("An enemy in reach sets it off"), Second->HasDetonated());
	TestTrue(TEXT("...stunned"), Pilgrim->GetAbilitySystemComponent()->HasMatchingGameplayTag(HollowTags::State_Stunned));
	TestTrue(TEXT("The blast is the player's heavy noise"), Heard == Definition->BlastNoise && HeardFrom == Player);
	UHollowNoiseSubsystem::Get(Scene.World)->OnNoiseReported.Remove(Listen);
	Advance(Scene.TestWorld, Definition->FlashSeconds + TickSeconds * 4.f);
	TestFalse(TEXT("The spent mine goes"), IsValid(Second));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
