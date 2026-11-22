#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Data/HollowBiomeDefinition.h"
#include "Data/HollowLevelMetrics.h"
#include "Data/HollowRoomDefinition.h"
#include "Engine/AssetManager.h"
#include "Misc/PackageName.h"
#include "NavigationSystem.h"
#include "Rules/HollowRoomRules.h"
#include "Tags/HollowGameplayTags.h"
#include "UObject/Package.h"
#include "Util/HollowEnumUtils.h"

namespace HollowRoomTests
{
	// Test data only: the real metrics live in DefaultGame.ini (HollowLevelMetrics).
	constexpr int32 CellSize = 400;
	constexpr int32 CellHeight = 400;
	constexpr int32 MaxRoomStoreys = 2;
	const TCHAR* const FakeLevelPath = TEXT("/Game/Tests/LV_Fake.LV_Fake");

	/** Sector content checked by DocksRooms: the S1-07 greybox kit inside its Game Feature plugin (ADR-0012). */
	const TCHAR* const DocksRoot = TEXT("/HollowSector_Docks");
	/** Pre-plugin location (S1-07); must stay empty. */
	const TCHAR* const LegacySectorsRoot = TEXT("/Game/Hollow/Sectors");
	constexpr int32 DocksMinRooms = 8;
	/** Sector II, the Reactor Cathedral (S7-01, ADR-0038): 8 rooms, four of them two-storey halls. */
	const TCHAR* const CathedralRoot = TEXT("/HollowSector_Cathedral");
	constexpr int32 CathedralRooms = 8;
	constexpr int32 CathedralTallHalls = 4;
	constexpr int32 TallHallStoreys = 2;

	UHollowLevelMetrics* MakeMetrics()
	{
		UHollowLevelMetrics* Metrics = NewObject<UHollowLevelMetrics>(GetTransientPackage());
		Metrics->CellSize = CellSize;
		Metrics->CellHeight = CellHeight;
		Metrics->MaxRoomStoreys = MaxRoomStoreys;
		Metrics->WallThickness = 20;
		Metrics->SlabThickness = 20;
		Metrics->DoorWidth = 300;
		Metrics->DoorHeight = 350;
		Metrics->MinCorridorWidth = 300;
		Metrics->VentSize = 90;
		Metrics->LargestAgentRadius = 80;
		Metrics->LargestAgentHeight = 280;
		return Metrics;
	}

	FHollowRoomConnector MakeConnector(const FIntVector& Cell, const EHollowRoomSide Side)
	{
		FHollowRoomConnector Connector;
		Connector.Cell = Cell;
		Connector.Side = Side;
		return Connector;
	}

	/** Valid 2x1 storage room: connectors on the north and west edges, one loot socket. */
	UHollowRoomDefinition* MakeValidRoom()
	{
		UHollowRoomDefinition* Room = NewObject<UHollowRoomDefinition>(GetTransientPackage());
		Room->RoomLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(FakeLevelPath));
		Room->Footprint = FIntVector(2, 1, 1);
		Room->Connectors.Add(MakeConnector(FIntVector(1, 0, 0), EHollowRoomSide::North));
		Room->Connectors.Add(MakeConnector(FIntVector(0, 0, 0), EHollowRoomSide::West));
		FHollowRoomSocket& Loot = Room->Sockets.AddDefaulted_GetRef();
		Loot.SocketType = HollowTags::Socket_Loot;
		Loot.Transform.SetLocation(FVector(CellSize * 0.5, CellSize * 0.5, 0.0));
		Room->RoomTags.AddTag(HollowTags::Room_Type_Storage);
		return Room;
	}

	/** What one sector's room kit must hold. */
	struct FSectorKitExpectation
	{
		const TCHAR* Root = nullptr;
		/** At least this many rooms (or exactly, with bExactRooms). */
		int32 Rooms = 0;
		bool bExactRooms = false;
		/** Rooms with Footprint.Z == TallHallStoreys; INDEX_NONE: not checked. */
		int32 TallHalls = INDEX_NONE;
	};

	/**
	 * The rooms of one sector plugin: every one passes the room rules, its level lies inside the plugin, it is a
	 * registered HollowRoom primary asset; one spawn, one extraction, at least one core vault (the generator's quotas).
	 */
	void CheckSectorKit(FAutomationTestBase& Test, const FSectorKitExpectation& Expect)
	{
		IAssetRegistry& Registry = IAssetRegistry::GetChecked();
		Registry.ScanPathsSynchronous({FString(Expect.Root)}, /*bForceRescan=*/false);

		FARFilter Filter;
		Filter.PackagePaths.Add(FName(Expect.Root));
		Filter.bRecursivePaths = true;
		Filter.ClassPaths.Add(UHollowRoomDefinition::StaticClass()->GetClassPathName());
		TArray<FAssetData> Assets;
		Registry.GetAssets(Filter, Assets);
		Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });

		if (Expect.bExactRooms)
		{
			Test.TestEqual(FString::Printf(TEXT("%s: rooms"), Expect.Root), Assets.Num(), Expect.Rooms);
		}
		else
		{
			Test.TestTrue(FString::Printf(TEXT("%s: at least %d rooms (found %d)"), Expect.Root, Expect.Rooms, Assets.Num()), Assets.Num() >= Expect.Rooms);
		}

		const UHollowLevelMetrics* Metrics = GetDefault<UHollowLevelMetrics>();
		const bool bAssetManagerReady = UAssetManager::IsInitialized();
		int32 SpawnRooms = 0;
		int32 ExtractionRooms = 0;
		int32 CoreVaults = 0;
		int32 TallHalls = 0;
		for (const FAssetData& Asset : Assets)
		{
			const UHollowRoomDefinition* Room = Cast<UHollowRoomDefinition>(Asset.GetAsset());
			if (!Test.TestNotNull(*FString::Printf(TEXT("%s loads"), *Asset.AssetName.ToString()), Room))
			{
				continue;
			}
			for (const FHollowRoomIssue& Issue : HollowRoomRules::ValidateRoom(*Room, *Metrics))
			{
				Test.AddError(FString::Printf(TEXT("%s: %s"), *Room->GetName(), *HollowRoomRules::Describe(Issue).ToString()));
			}
			Test.TestTrue(*FString::Printf(TEXT("%s: room level exists inside the plugin"), *Room->GetName()),
				!Room->RoomLevel.IsNull() && FPackageName::DoesPackageExist(Room->RoomLevel.GetLongPackageName())
				&& Room->RoomLevel.GetLongPackageName().StartsWith(FString(Expect.Root) + TEXT("/")));
			if (bAssetManagerReady)
			{
				Test.TestTrue(*FString::Printf(TEXT("%s is a registered HollowRoom primary asset"), *Room->GetName()),
					UAssetManager::Get().GetPrimaryAssetPath(Room->GetPrimaryAssetId()).IsValid());
			}

			SpawnRooms += Room->RoomTags.HasTagExact(HollowTags::Room_Type_Spawn) ? 1 : 0;
			ExtractionRooms += Room->RoomTags.HasTagExact(HollowTags::Room_Type_Extraction) ? 1 : 0;
			CoreVaults += Room->RoomTags.HasTagExact(HollowTags::Room_Type_CoreVault) ? 1 : 0;
			TallHalls += Room->Footprint.Z == TallHallStoreys ? 1 : 0;
		}
		Test.TestTrue(TEXT("Asset manager is initialized"), bAssetManagerReady);

		// Quotas the sector generator (S2-01) relies on.
		Test.TestEqual(FString::Printf(TEXT("%s: exactly one spawn room"), Expect.Root), SpawnRooms, 1);
		Test.TestEqual(FString::Printf(TEXT("%s: exactly one extraction room"), Expect.Root), ExtractionRooms, 1);
		Test.TestTrue(FString::Printf(TEXT("%s: at least one core vault"), Expect.Root), CoreVaults >= 1);
		if (Expect.TallHalls != INDEX_NONE)
		{
			Test.TestEqual(FString::Printf(TEXT("%s: two-storey halls"), Expect.Root), TallHalls, Expect.TallHalls);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowRoomRulesTest, "Hollow.PCG.RoomRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowRoomRulesTest::RunTest(const FString& Parameters)
{
	using namespace HollowRoomTests;
	const UHollowLevelMetrics* Metrics = MakeMetrics();

	TestTrue(TEXT("A valid room has no issues"), HollowRoomRules::ValidateRoom(*MakeValidRoom(), *Metrics).IsEmpty());

	// Each mutation must produce exactly the expected issue (at the expected index).
	auto ExpectOnly = [this, Metrics](const TCHAR* What, TFunctionRef<void(UHollowRoomDefinition&)> Mutate,
		const EHollowRoomIssue Expected, const int32 ExpectedIndex = INDEX_NONE)
	{
		UHollowRoomDefinition* Room = MakeValidRoom();
		Mutate(*Room);
		const TArray<FHollowRoomIssue> Issues = HollowRoomRules::ValidateRoom(*Room, *Metrics);
		const bool bOnlyExpected = Issues.Num() == 1 && Issues[0].Code == Expected && Issues[0].Index == ExpectedIndex;
		TestTrue(FString::Printf(TEXT("%s -> %s"), What, *HollowEnumToString(Expected)), bOnlyExpected);
		for (const FHollowRoomIssue& Issue : Issues)
		{
			if (!bOnlyExpected)
			{
				AddInfo(FString::Printf(TEXT("  got %s"), *HollowRoomRules::Describe(Issue).ToString()));
			}
		}
	};

	ExpectOnly(TEXT("Zero-size footprint"), [](UHollowRoomDefinition& R) { R.Footprint = FIntVector(0, 1, 1); }, EHollowRoomIssue::EmptyFootprint);
	TestTrue(TEXT("A two-storey hall is valid"), [&Metrics]()
	{
		UHollowRoomDefinition* Hall = MakeValidRoom();
		Hall->Footprint.Z = MaxRoomStoreys;
		return HollowRoomRules::ValidateRoom(*Hall, *Metrics).IsEmpty();
	}());
	ExpectOnly(TEXT("Taller than MaxRoomStoreys"), [](UHollowRoomDefinition& R) { R.Footprint.Z = MaxRoomStoreys + 1; }, EHollowRoomIssue::TooManyStoreys);
	ExpectOnly(TEXT("An opening up in a tall hall"), [](UHollowRoomDefinition& R) { R.Footprint.Z = MaxRoomStoreys; R.Connectors[0].Cell.Z = 1; },
		EHollowRoomIssue::ConnectorAboveFloor, 0);
	ExpectOnly(TEXT("No level"), [](UHollowRoomDefinition& R) { R.RoomLevel.Reset(); }, EHollowRoomIssue::NoLevel);
	ExpectOnly(TEXT("No connectors"), [](UHollowRoomDefinition& R) { R.Connectors.Reset(); }, EHollowRoomIssue::NoConnectors);
	ExpectOnly(TEXT("Connector cell outside"), [](UHollowRoomDefinition& R) { R.Connectors[0].Cell = FIntVector(2, 0, 0); },
		EHollowRoomIssue::ConnectorOutsideFootprint, 0);
	ExpectOnly(TEXT("Connector on an inner edge"), [](UHollowRoomDefinition& R) { R.Connectors[1].Side = EHollowRoomSide::North; },
		EHollowRoomIssue::ConnectorFacesInside, 1);
	ExpectOnly(TEXT("Duplicate connector"), [](UHollowRoomDefinition& R) { const FHollowRoomConnector Copy = R.Connectors[0]; R.Connectors.Add(Copy); },
		EHollowRoomIssue::DuplicateConnector, 2);
	ExpectOnly(TEXT("Connector without kinds"), [](UHollowRoomDefinition& R) { R.Connectors[0].AllowedKinds = 0; },
		EHollowRoomIssue::ConnectorWithoutKinds, 0);
	ExpectOnly(TEXT("Socket without type"), [](UHollowRoomDefinition& R) { R.Sockets[0].SocketType = FGameplayTag(); },
		EHollowRoomIssue::SocketWithoutType, 0);
	ExpectOnly(TEXT("Socket typed with the parent tag"), [](UHollowRoomDefinition& R) { R.Sockets[0].SocketType = HollowTags::Socket; },
		EHollowRoomIssue::SocketWithoutType, 0);
	ExpectOnly(TEXT("Socket outside the footprint"), [](UHollowRoomDefinition& R) { R.Sockets[0].Transform.SetLocation(FVector(CellSize * 3.0, 0.0, 0.0)); },
		EHollowRoomIssue::SocketOutsideFootprint, 0);
	ExpectOnly(TEXT("No room type"), [](UHollowRoomDefinition& R) { R.RoomTags.Reset(); }, EHollowRoomIssue::RoomTypeCount);
	ExpectOnly(TEXT("Two room types"), [](UHollowRoomDefinition& R) { R.RoomTags.AddTag(HollowTags::Room_Type_Hangar); },
		EHollowRoomIssue::RoomTypeCount);
	ExpectOnly(TEXT("Parent room type tag"), [](UHollowRoomDefinition& R) { R.RoomTags.Reset(); R.RoomTags.AddTag(HollowTags::Room_Type); },
		EHollowRoomIssue::RoomTypeCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowLevelMetricsTest, "Hollow.PCG.LevelMetrics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowLevelMetricsTest::RunTest(const FString& Parameters)
{
	using namespace HollowRoomTests;

	const TArray<EHollowMetricsIssue> Configured = HollowRoomRules::ValidateMetrics(*GetDefault<UHollowLevelMetrics>());
	TestTrue(TEXT("DefaultGame.ini metrics are consistent"), Configured.IsEmpty());
	for (const EHollowMetricsIssue Issue : Configured)
	{
		AddError(FString::Printf(TEXT("DefaultGame.ini HollowLevelMetrics: %s"), *HollowEnumToString(Issue)));
	}
	TestTrue(TEXT("The test baseline is consistent"), HollowRoomRules::ValidateMetrics(*MakeMetrics()).IsEmpty());

	// The kit is sized for the largest nav agent (DefaultEngine.ini SupportedAgents, ADR-0004): both must say the same.
	const TArray<FNavDataConfig>& Agents = GetDefault<UNavigationSystemV1>()->GetSupportedAgents();
	float AgentRadius = 0.f;
	float AgentHeight = 0.f;
	for (const FNavDataConfig& Agent : Agents)
	{
		AgentRadius = FMath::Max(AgentRadius, Agent.AgentRadius);
		AgentHeight = FMath::Max(AgentHeight, Agent.AgentHeight);
	}
	TestTrue(TEXT("Nav agents are configured"), Agents.Num() > 1);
	TestEqual(TEXT("Largest nav agent radius = LargestAgentRadius"), AgentRadius, static_cast<float>(GetDefault<UHollowLevelMetrics>()->LargestAgentRadius));
	TestEqual(TEXT("Largest nav agent height = LargestAgentHeight"), AgentHeight, static_cast<float>(GetDefault<UHollowLevelMetrics>()->LargestAgentHeight));

	auto ExpectOnly = [this](const TCHAR* What, TFunctionRef<void(UHollowLevelMetrics&)> Mutate, const EHollowMetricsIssue Expected)
	{
		UHollowLevelMetrics* Metrics = MakeMetrics();
		Mutate(*Metrics);
		const TArray<EHollowMetricsIssue> Issues = HollowRoomRules::ValidateMetrics(*Metrics);
		TestTrue(FString::Printf(TEXT("%s -> %s"), What, *HollowEnumToString(Expected)), Issues.Num() == 1 && Issues[0] == Expected);
	};

	ExpectOnly(TEXT("Missing value"), [](UHollowLevelMetrics& M) { M.CellSize = 0; }, EHollowMetricsIssue::NonPositiveValue);
	ExpectOnly(TEXT("No storey allowed"), [](UHollowLevelMetrics& M) { M.MaxRoomStoreys = 0; }, EHollowMetricsIssue::NonPositiveValue);
	ExpectOnly(TEXT("Door plus jambs wider than a cell"), [](UHollowLevelMetrics& M) { M.DoorWidth = M.CellSize - M.WallThickness; },
		EHollowMetricsIssue::DoorDoesNotFitCell);
	ExpectOnly(TEXT("Door as tall as the storey"), [](UHollowLevelMetrics& M) { M.DoorHeight = M.CellHeight; },
		EHollowMetricsIssue::DoorReachesCeiling);
	ExpectOnly(TEXT("One-cell corridor too narrow"), [](UHollowLevelMetrics& M) { M.MinCorridorWidth = M.CellSize; },
		EHollowMetricsIssue::CorridorTooNarrow);
	ExpectOnly(TEXT("Vent as wide as a door"), [](UHollowLevelMetrics& M) { M.VentSize = M.DoorWidth; },
		EHollowMetricsIssue::VentNotSmallerThanDoor);
	ExpectOnly(TEXT("Pilgrim taller than the door"), [](UHollowLevelMetrics& M) { M.LargestAgentHeight = M.DoorHeight + 1; },
		EHollowMetricsIssue::AgentDoesNotFitDoor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowDocksRoomsTest, "Hollow.PCG.DocksRooms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowDocksRoomsTest::RunTest(const FString& Parameters)
{
	using namespace HollowRoomTests;

	TArray<FAssetData> Legacy;
	IAssetRegistry::GetChecked().GetAssetsByPath(FName(LegacySectorsRoot), Legacy, /*bRecursive=*/true);
	TestEqual(TEXT("No sector content left under /Game/Hollow/Sectors"), Legacy.Num(), 0);

	FSectorKitExpectation Expect;
	Expect.Root = DocksRoot;
	Expect.Rooms = DocksMinRooms;
	CheckSectorKit(*this, Expect);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCathedralRoomsTest, "Hollow.PCG.CathedralRooms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCathedralRoomsTest::RunTest(const FString& Parameters)
{
	// S7-01 (ADR-0038): the Reactor Cathedral is a second content-only sector plugin. Its 8 rooms pass the same rules
	// as the Docks (connectors 300 x 350 on the floor), four of them are two-storey halls; its biome lives beside them.
	using namespace HollowRoomTests;
	FSectorKitExpectation Expect;
	Expect.Root = CathedralRoot;
	Expect.Rooms = CathedralRooms;
	Expect.bExactRooms = true;
	Expect.TallHalls = CathedralTallHalls;
	CheckSectorKit(*this, Expect);

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(CathedralRoot));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UHollowBiomeDefinition::StaticClass()->GetClassPathName());
	TArray<FAssetData> Biomes;
	IAssetRegistry::GetChecked().GetAssets(Filter, Biomes);
	TestEqual(TEXT("One biome in the Cathedral plugin"), Biomes.Num(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
