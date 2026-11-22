#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Assembly/HollowSectorAssemblySubsystem.h"
#include "Assembly/HollowSectorManifest.h"
#include "Data/HollowBiomeDefinition.h"
#include "Data/HollowLevelMetrics.h"
#include "Data/HollowRoomDefinition.h"
#include "Engine/World.h"
#include "Generation/HollowGridMath.h"
#include "Generation/HollowSectorGenerator.h"
#include "Generation/HollowSectorTemplates.h"
#include "HAL/PlatformTime.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UObjectGlobals.h"

namespace HollowAssemblyTests
{
	const TCHAR* const DocksData = TEXT("/HollowSector_Docks/Data");
	const TCHAR* const DocksPlugin = TEXT("HollowSector_Docks");
	/** Sector II (S7-01, ADR-0038). */
	const TCHAR* const CathedralData = TEXT("/HollowSector_Cathedral/Data");
	const TCHAR* const CathedralPlugin = TEXT("HollowSector_Cathedral");
	constexpr int32 RoundTripSeeds = 50;
	constexpr int32 FirstSeed = 1;
	constexpr int32 AssemblySeed = 42;
	/** ADR-0003: the manifest stays under 2 KB. */
	constexpr int32 MaxManifestBytes = 2048;
	/** Test data: streaming a dozen greybox rooms takes well under this. */
	constexpr double AssemblyTimeoutSeconds = 60.0;
	/** Distance tolerance for transforms built from integer cells, in uu. */
	constexpr double Tolerance = 0.01;
	constexpr int32 BuildId = 7;
	/** A pawn-sized capsule (test data: the ExoFrame's is in BP_ExoFrame) swept across every sealed opening. */
	constexpr float PawnRadius = 34.f;
	constexpr float PawnHalfHeight = 96.f;
	constexpr double SweepReach = 100.0;
	/** Clearance above the floor, so the sweep starts free and only the opening can stop it. */
	constexpr double FloorClearance = 10.0;

	/** One sector plugin's rooms and biome. */
	struct FSector
	{
		const TCHAR* Plugin = nullptr;
		UHollowBiomeDefinition* Biome = nullptr;
		TArray<UHollowRoomDefinition*> Rooms;
	};

	FSector LoadSector(const TCHAR* DataPath, const TCHAR* Plugin)
	{
		IAssetRegistry& Registry = IAssetRegistry::GetChecked();
		Registry.ScanPathsSynchronous({FString(DataPath)}, /*bForceRescan=*/false);
		FARFilter Filter;
		Filter.PackagePaths.Add(FName(DataPath));
		Filter.bRecursivePaths = true;
		Filter.ClassPaths.Add(UHollowRoomDefinition::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UHollowBiomeDefinition::StaticClass()->GetClassPathName());
		TArray<FAssetData> Assets;
		Registry.GetAssets(Filter, Assets);

		FSector Sector;
		Sector.Plugin = Plugin;
		for (const FAssetData& Asset : Assets)
		{
			UObject* Object = Asset.GetAsset();
			Sector.Biome = Sector.Biome ? Sector.Biome : Cast<UHollowBiomeDefinition>(Object);
			if (UHollowRoomDefinition* Room = Cast<UHollowRoomDefinition>(Object))
			{
				Sector.Rooms.Add(Room);
			}
		}
		return Sector;
	}

	FSector LoadDocks()
	{
		return LoadSector(DocksData, DocksPlugin);
	}

	/** Generates a sector and its manifest, as the raid authority does. */
	bool MakeSector(const FSector& Sector, const int32 Seed, FHollowSectorLayout& OutLayout, FHollowSectorManifest& OutManifest, FString& OutError)
	{
		FHollowSectorGenInput Input;
		Input.Seed = Seed;
		Input.Rules = Sector.Biome->GenRules;
		Input.Templates = HollowSectorTemplates::MakeTemplates(TArray<const UHollowRoomDefinition*>(Sector.Rooms), Sector.Biome->GenRules);
		if (FHollowSectorGenerator::Generate(Input, OutLayout) != EHollowGenResult::Success)
		{
			OutError = FString::Printf(TEXT("seed %d did not generate"), Seed);
			return false;
		}
		TArray<UHollowRoomDefinition*> Definitions;
		for (const FString& Id : OutLayout.TemplateIds)
		{
			UHollowRoomDefinition* const* Room = Sector.Rooms.FindByPredicate([&Id](const UHollowRoomDefinition* R) { return R->GetPrimaryAssetId().ToString() == Id; });
			Definitions.Add(Room ? *Room : nullptr);
		}
		return HollowSectorManifest::Build(OutLayout, Definitions, Sector.Biome, FName(Sector.Plugin), BuildId, OutManifest, OutError);
	}

	/** Same sector: rooms (by template id), connections and sealed connectors. Template order may differ. */
	bool SameSector(const FHollowSectorLayout& A, const FHollowSectorLayout& B, FString& OutWhy)
	{
		if (A.Rooms.Num() != B.Rooms.Num() || A.Connections != B.Connections || A.Sealed != B.Sealed)
		{
			OutWhy = TEXT("room count, connections or sealed connectors differ");
			return false;
		}
		for (int32 Index = 0; Index < A.Rooms.Num(); ++Index)
		{
			const FHollowPlacedRoom& RA = A.Rooms[Index];
			const FHollowPlacedRoom& RB = B.Rooms[Index];
			if (A.TemplateIds[RA.Template] != B.TemplateIds[RB.Template] || RA.Role != RB.Role || RA.Cell != RB.Cell || RA.Size != RB.Size || RA.Yaw90 != RB.Yaw90)
			{
				OutWhy = FString::Printf(TEXT("room %d differs"), Index);
				return false;
			}
		}
		return true;
	}

	/**
	 * One sector of the plugin assembled from its manifest in a game world: every room a visible level instance, the
	 * local hash equal to the server's, every unused connector plugged against a pawn; teardown removes it all.
	 */
	bool RunAssemblyTest(FAutomationTestBase& Test, const TCHAR* DataPath, const TCHAR* Plugin)
	{
		const FSector Sector = LoadSector(DataPath, Plugin);
		FHollowSectorLayout Layout;
		FHollowSectorManifest Manifest;
		FString Error;
		if (!Test.TestNotNull(FString::Printf(TEXT("%s biome"), Plugin), Sector.Biome) || !Test.TestTrue(TEXT("Sector generates"), MakeSector(Sector, AssemblySeed, Layout, Manifest, Error)))
		{
			return false;
		}

		struct FState
		{
			FTestWorldWrapper World;
			int32 AssembledBuild = 0;
			uint32 AssembledHash = 0;
			int32 Broadcasts = 0;
			double Deadline = 0.0;
		};
		const TSharedRef<FState> State = MakeShared<FState>();
		if (!State->World.CreateTestWorld(EWorldType::Game))
		{
			State->World.ForwardErrorMessages(&Test);
			return false;
		}
		// A raid assembles in a world that has begun play (static components then refuse new meshes): the same here.
		State->World.GetTestWorld()->SetBegunPlay(true);
		UHollowSectorAssemblySubsystem* Assembly = State->World.GetTestWorld()->GetSubsystem<UHollowSectorAssemblySubsystem>();
		if (!Test.TestNotNull(TEXT("Game worlds get the assembly subsystem"), Assembly))
		{
			return false;
		}
		Assembly->OnAssembled.AddLambda([State](const int32 Build, const uint32 Hash)
		{
			State->AssembledBuild = Build;
			State->AssembledHash = Hash;
			++State->Broadcasts;
		});
		State->Deadline = FPlatformTime::Seconds() + AssemblyTimeoutSeconds;
		Assembly->Assemble(Manifest);

		const int32 Rooms = Manifest.Rooms.Num();
		const int32 Sealed = Layout.Sealed.Num();
		const uint32 Hash = Manifest.LayoutHash;
		const TArray<FHollowSealedConnector> SealedConnectors = Layout.Sealed;
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([&Test, State, Assembly, Rooms, Sealed, Hash, SealedConnectors]()
		{
			if (!Assembly->IsAssembled())
			{
				if (FPlatformTime::Seconds() > State->Deadline)
				{
					Test.AddError(TEXT("The sector did not assemble in time"));
					return true;
				}
				State->World.GetTestWorld()->FlushLevelStreaming(EFlushLevelStreamingType::Full);
				State->World.TickTestWorld();
				return false;
			}

			Test.TestEqual(TEXT("OnAssembled fired once"), State->Broadcasts, 1);
			Test.TestEqual(TEXT("For our build"), State->AssembledBuild, BuildId);
			Test.TestTrue(TEXT("Local hash equals the server's"), State->AssembledHash == Hash && Assembly->GetLocalHash() == Hash);
			Test.TestEqual(TEXT("Every room is a visible level instance"), Assembly->NumVisibleRooms(), Rooms);
			Test.TestEqual(TEXT("Every unused connector is plugged"), Assembly->NumPlugs(), Sealed);

			// A plug closes its opening for players: a pawn capsule walking out through it is stopped.
			const UHollowLevelMetrics* Metrics = GetDefault<UHollowLevelMetrics>();
			for (const FHollowSealedConnector& Connector : SealedConnectors)
			{
				const FTransform Opening = HollowSectorManifest::OpeningTransform(Connector.Cell, Connector.Side, *Metrics, UHollowSectorAssemblySubsystem::GetSectorOrigin());
				const FIntPoint Step = HollowGridMath::Offset(Connector.Side);
				const FVector Out(Step.X, Step.Y, 0.0);
				const FVector Centre = Opening.GetLocation() + FVector(0.0, 0.0, PawnHalfHeight + FloorClearance);
				FCollisionResponseParams Responses;
				Responses.CollisionResponse.SetAllChannels(ECR_Block);
				FHitResult Hit;
				const bool bBlocked = State->World.GetTestWorld()->SweepSingleByChannel(Hit, Centre - Out * SweepReach, Centre + Out * SweepReach, FQuat::Identity,
					ECC_Pawn, FCollisionShape::MakeCapsule(PawnRadius, PawnHalfHeight), FCollisionQueryParams(SCENE_QUERY_STAT(HollowPlugTest)), Responses);
				Test.TestTrue(FString::Printf(TEXT("Room %d connector %d (%s %s): the plug stops a pawn (hit %s)"), Connector.Room, Connector.Connector,
					*Connector.Cell.ToString(), *UEnum::GetValueAsString(Connector.Side), *GetNameSafe(Hit.GetActor())), bBlocked && !Hit.bStartPenetrating);
			}

			Assembly->Teardown();
			Test.TestEqual(TEXT("Teardown unloads the rooms"), Assembly->NumRoomInstances(), 0);
			Test.TestEqual(TEXT("Teardown removes the plugs"), Assembly->NumPlugs(), 0);
			Test.TestFalse(TEXT("Nothing assembled after teardown"), Assembly->IsAssembled());
			return true;
		}));
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowManifestRoundTripTest, "Hollow.PCG.Sector.ManifestRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowManifestRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace HollowAssemblyTests;
	const FSector Docks = LoadDocks();
	if (!TestNotNull(TEXT("Docks biome"), Docks.Biome))
	{
		return false;
	}

	int32 MaxBytes = 0;
	for (int32 Seed = FirstSeed; Seed < FirstSeed + RoundTripSeeds; ++Seed)
	{
		FHollowSectorLayout Layout, Rebuilt;
		FHollowSectorManifest Manifest;
		FString Error;
		if (!MakeSector(Docks, Seed, Layout, Manifest, Error) || !HollowSectorManifest::Reconstruct(Manifest, Rebuilt, Error))
		{
			AddError(FString::Printf(TEXT("Seed %d: %s"), Seed, *Error));
			continue;
		}
		FString Why;
		if (!SameSector(Layout, Rebuilt, Why))
		{
			AddError(FString::Printf(TEXT("Seed %d: rebuilt layout differs: %s"), Seed, *Why));
		}
		if (FHollowSectorGenerator::ComputeLayoutHash(Rebuilt) != Manifest.LayoutHash)
		{
			AddError(FString::Printf(TEXT("Seed %d: rebuilt hash differs from the server's"), Seed));
		}
		MaxBytes = FMath::Max(MaxBytes, HollowSectorManifest::EstimateNetBytes(Manifest));
	}
	TestTrue(FString::Printf(TEXT("Manifest stays under %d bytes (largest %d)"), MaxManifestBytes, MaxBytes), MaxBytes < MaxManifestBytes);

	// A client whose content differs builds another layout: the hash must catch it.
	FHollowSectorLayout Layout, Rebuilt;
	FHollowSectorManifest Manifest;
	FString Error;
	if (MakeSector(Docks, FirstSeed, Layout, Manifest, Error))
	{
		Manifest.Rooms.Last().Yaw90 = static_cast<uint8>((Manifest.Rooms.Last().Yaw90 + 1) % HollowGridMath::SideCount);
		TestTrue(TEXT("A rotated room changes the hash"), HollowSectorManifest::Reconstruct(Manifest, Rebuilt, Error)
			&& FHollowSectorGenerator::ComputeLayoutHash(Rebuilt) != Manifest.LayoutHash);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSectorTransformsTest, "Hollow.PCG.Sector.Transforms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSectorTransformsTest::RunTest(const FString& Parameters)
{
	using namespace HollowAssemblyTests;
	using namespace HollowGridMath;
	const FSector Docks = LoadDocks();
	FHollowSectorLayout Layout;
	FHollowSectorManifest Manifest;
	FString Error;
	if (!TestNotNull(TEXT("Docks biome"), Docks.Biome) || !TestTrue(TEXT("Sector generates"), MakeSector(Docks, AssemblySeed, Layout, Manifest, Error)))
	{
		return false;
	}

	const UHollowLevelMetrics* Metrics = GetDefault<UHollowLevelMetrics>();
	const double Cell = Metrics->CellSize;
	const FVector Origin(1000.0, -2000.0, 300.0);   // any origin: the transforms must carry it through
	bool bCellsOk = true;
	bool bOpeningsOk = true;
	for (int32 Index = 0; Index < Manifest.Rooms.Num(); ++Index)
	{
		const UHollowRoomDefinition* Room = Manifest.Rooms[Index].Room;
		const FIntPoint Footprint(Room->Footprint.X, Room->Footprint.Y);
		const FHollowPlacedRoom& Placed = Layout.Rooms[Index];
		const FTransform RoomTransform = HollowSectorManifest::RoomTransform(Placed, Footprint, *Metrics, Origin);

		// Every cell centre of the level instance lands on its layout cell.
		for (int32 X = 0; X < Footprint.X; ++X)
		{
			for (int32 Y = 0; Y < Footprint.Y; ++Y)
			{
				const FVector Local((X + 0.5) * Cell, (Y + 0.5) * Cell, 0.0);
				const FIntPoint Expected = Placed.Cell + RotateCell(FIntPoint(X, Y), Footprint, Placed.Yaw90);
				const FVector World = Origin + FVector((Expected.X + 0.5) * Cell, (Expected.Y + 0.5) * Cell, 0.0);
				bCellsOk &= RoomTransform.TransformPosition(Local).Equals(World, Tolerance);
			}
		}

		// Every connector's opening in the room shell lands where the server puts its door or plug.
		for (const FHollowRoomConnector& Connector : Room->Connectors)
		{
			const FIntPoint Step = Offset(Connector.Side);
			const FVector LocalEdge((Connector.Cell.X + 0.5 + 0.5 * Step.X) * Cell, (Connector.Cell.Y + 0.5 + 0.5 * Step.Y) * Cell, 0.0);
			const FIntPoint WorldCell = Placed.Cell + RotateCell(FIntPoint(Connector.Cell.X, Connector.Cell.Y), Footprint, Placed.Yaw90);
			const EHollowRoomSide WorldSide = RotateSide(Connector.Side, Placed.Yaw90);
			const FTransform Opening = HollowSectorManifest::OpeningTransform(WorldCell, WorldSide, *Metrics, Origin);
			const FIntPoint WorldStep = Offset(WorldSide);
			bOpeningsOk &= RoomTransform.TransformPosition(LocalEdge).Equals(Opening.GetLocation(), Tolerance)
				&& Opening.GetUnitAxis(EAxis::X).Equals(FVector(WorldStep.X, WorldStep.Y, 0.0), Tolerance);

			const FVector PlugCentre = HollowSectorManifest::PlugTransform(WorldCell, WorldSide, *Metrics, Origin).GetLocation();
			const FVector ExpectedPlug = Opening.GetLocation() - FVector(WorldStep.X, WorldStep.Y, 0.0) * (0.5 * Metrics->WallThickness)
				+ FVector(0.0, 0.0, 0.5 * Metrics->DoorHeight);
			bOpeningsOk &= PlugCentre.Equals(ExpectedPlug, Tolerance);
		}
	}
	TestTrue(TEXT("Room cells land on their layout cells"), bCellsOk);
	TestTrue(TEXT("Openings, doors and plugs line up with the room shells"), bOpeningsOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowSectorAssemblyTest, "Hollow.PCG.Sector.Assembly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowSectorAssemblyTest::RunTest(const FString& Parameters)
{
	using namespace HollowAssemblyTests;
	return RunAssemblyTest(*this, DocksData, DocksPlugin);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowCathedralAssemblyTest, "Hollow.PCG.Sector.CathedralAssembly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowCathedralAssemblyTest::RunTest(const FString& Parameters)
{
	// S7-01 (ADR-0038): a Cathedral sector assembles from its manifest like the Docks, two-storey halls included.
	using namespace HollowAssemblyTests;
	return RunAssemblyTest(*this, CathedralData, CathedralPlugin);
}

#endif // WITH_DEV_AUTOMATION_TESTS
