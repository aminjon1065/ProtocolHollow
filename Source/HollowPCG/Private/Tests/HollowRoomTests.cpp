#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Data/HollowLevelMetrics.h"
#include "Data/HollowRoomDefinition.h"
#include "Engine/AssetManager.h"
#include "Misc/PackageName.h"
#include "Rules/HollowRoomRules.h"
#include "Tags/HollowGameplayTags.h"
#include "UObject/Package.h"
#include "Util/HollowEnumUtils.h"

namespace HollowRoomTests
{
	// Test data only: the real metrics live in DefaultGame.ini (HollowLevelMetrics).
	constexpr int32 CellSize = 400;
	constexpr int32 CellHeight = 400;
	const TCHAR* const FakeLevelPath = TEXT("/Game/Tests/LV_Fake.LV_Fake");

	/** Sector content checked by DocksRooms: the S1-07 greybox kit inside its Game Feature plugin (ADR-0012). */
	const TCHAR* const DocksRoot = TEXT("/HollowSector_Docks");
	/** Pre-plugin location (S1-07); must stay empty. */
	const TCHAR* const LegacySectorsRoot = TEXT("/Game/Hollow/Sectors");
	constexpr int32 DocksMinRooms = 8;

	UHollowLevelMetrics* MakeMetrics()
	{
		UHollowLevelMetrics* Metrics = NewObject<UHollowLevelMetrics>(GetTransientPackage());
		Metrics->CellSize = CellSize;
		Metrics->CellHeight = CellHeight;
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
	ExpectOnly(TEXT("Two storeys"), [](UHollowRoomDefinition& R) { R.Footprint.Z = 2; }, EHollowRoomIssue::MultiStorey);
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

	auto ExpectOnly = [this](const TCHAR* What, TFunctionRef<void(UHollowLevelMetrics&)> Mutate, const EHollowMetricsIssue Expected)
	{
		UHollowLevelMetrics* Metrics = MakeMetrics();
		Mutate(*Metrics);
		const TArray<EHollowMetricsIssue> Issues = HollowRoomRules::ValidateMetrics(*Metrics);
		TestTrue(FString::Printf(TEXT("%s -> %s"), What, *HollowEnumToString(Expected)), Issues.Num() == 1 && Issues[0] == Expected);
	};

	ExpectOnly(TEXT("Missing value"), [](UHollowLevelMetrics& M) { M.CellSize = 0; }, EHollowMetricsIssue::NonPositiveValue);
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

	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	Registry.ScanPathsSynchronous({FString(DocksRoot)}, /*bForceRescan=*/false);

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(DocksRoot));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UHollowRoomDefinition::StaticClass()->GetClassPathName());
	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);
	Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });

	TestTrue(FString::Printf(TEXT("At least %d Docks rooms (found %d)"), DocksMinRooms, Assets.Num()), Assets.Num() >= DocksMinRooms);

	TArray<FAssetData> Legacy;
	Registry.GetAssetsByPath(FName(LegacySectorsRoot), Legacy, /*bRecursive=*/true);
	TestEqual(TEXT("No sector content left under /Game/Hollow/Sectors"), Legacy.Num(), 0);

	const UHollowLevelMetrics* Metrics = GetDefault<UHollowLevelMetrics>();
	const bool bAssetManagerReady = UAssetManager::IsInitialized();
	int32 SpawnRooms = 0;
	int32 ExtractionRooms = 0;
	int32 CoreVaults = 0;
	for (const FAssetData& Asset : Assets)
	{
		const UHollowRoomDefinition* Room = Cast<UHollowRoomDefinition>(Asset.GetAsset());
		if (!TestNotNull(*FString::Printf(TEXT("%s loads"), *Asset.AssetName.ToString()), Room))
		{
			continue;
		}
		for (const FHollowRoomIssue& Issue : HollowRoomRules::ValidateRoom(*Room, *Metrics))
		{
			AddError(FString::Printf(TEXT("%s: %s"), *Room->GetName(), *HollowRoomRules::Describe(Issue).ToString()));
		}
		TestTrue(*FString::Printf(TEXT("%s: room level exists inside the plugin"), *Room->GetName()),
			!Room->RoomLevel.IsNull() && FPackageName::DoesPackageExist(Room->RoomLevel.GetLongPackageName())
			&& Room->RoomLevel.GetLongPackageName().StartsWith(FString(DocksRoot) + TEXT("/")));
		if (bAssetManagerReady)
		{
			TestTrue(*FString::Printf(TEXT("%s is a registered HollowRoom primary asset"), *Room->GetName()),
				UAssetManager::Get().GetPrimaryAssetPath(Room->GetPrimaryAssetId()).IsValid());
		}

		SpawnRooms += Room->RoomTags.HasTagExact(HollowTags::Room_Type_Spawn) ? 1 : 0;
		ExtractionRooms += Room->RoomTags.HasTagExact(HollowTags::Room_Type_Extraction) ? 1 : 0;
		CoreVaults += Room->RoomTags.HasTagExact(HollowTags::Room_Type_CoreVault) ? 1 : 0;
	}
	TestTrue(TEXT("Asset manager is initialized"), bAssetManagerReady);

	// Quotas the sector generator (S2-01) relies on.
	TestEqual(TEXT("Exactly one spawn room"), SpawnRooms, 1);
	TestEqual(TEXT("Exactly one extraction room"), ExtractionRooms, 1);
	TestTrue(TEXT("At least one core vault"), CoreVaults >= 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
