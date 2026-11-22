#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Assembly/HollowSectorManifest.h"
#include "Data/HollowBiomeDefinition.h"
#include "Data/HollowRoomDefinition.h"
#include "Generation/HollowGridMath.h"
#include "Generation/HollowRandom.h"
#include "Generation/HollowSectorGenerator.h"
#include "Generation/HollowSectorTemplates.h"
#include "HAL/PlatformTime.h"
#include "Tags/HollowGameplayTags.h"
#include "Util/HollowEnumUtils.h"

namespace HollowGenTests
{
	using namespace HollowGridMath;

	/** Seeds swept by the property tests on the synthetic pool. */
	constexpr int32 SweepSeeds = 200;
	/** Seeds swept on each real sector's pool (S7-01: every seed of 1000 must generate). */
	constexpr int32 PoolSeeds = 1000;
	/** Replicated manifest budget (CLAUDE.md sec. 5, ADR-0003). */
	constexpr int32 MaxManifestBytes = 2048;
	constexpr int32 PoolBuildId = 1;
	constexpr int32 FirstSeed = 1;
	/** SeedParity: share of distinct layouts expected among SweepSeeds / 2 seeds. */
	constexpr int32 UniquePercentMin = 90;
	constexpr int32 PercentScale = 100;

	/**
	 * GoldenLayout: hash of GoldenSeed on the synthetic pool. Changes only with the algorithm (update on purpose).
	 * S3-01: bulkheads skip links that would cut a room off (the golden seed had one on a bridge).
	 */
	constexpr int32 GoldenSeed = 20261119;
	constexpr uint32 GoldenHash = 0x583C99A7u;

	constexpr uint8 Door = static_cast<uint8>(EHollowConnectorKind::Door);
	constexpr uint8 Bulkhead = static_cast<uint8>(EHollowConnectorKind::Bulkhead);
	constexpr uint8 Open = static_cast<uint8>(EHollowConnectorKind::Open);
	constexpr uint8 Any = Door | Bulkhead | Open;

	const TCHAR* const DocksData = TEXT("/HollowSector_Docks/Data");
	const TCHAR* const DocksPlugin = TEXT("HollowSector_Docks");
	const TCHAR* const CathedralData = TEXT("/HollowSector_Cathedral/Data");
	const TCHAR* const CathedralPlugin = TEXT("HollowSector_Cathedral");

	FHollowRoomTemplate Room(const TCHAR* Id, const EHollowRoomRole Role, const FIntPoint Footprint,
		std::initializer_list<FHollowTemplateConnector> Connectors)
	{
		FHollowRoomTemplate Template;
		Template.Id = Id;
		Template.Role = Role;
		Template.Footprint = Footprint;
		Template.Connectors = Connectors;
		return Template;
	}

	FHollowTemplateConnector Conn(const int32 X, const int32 Y, const EHollowRoomSide Side, const uint8 Kinds)
	{
		FHollowTemplateConnector Connector;
		Connector.Cell = FIntPoint(X, Y);
		Connector.Side = Side;
		Connector.Kinds = Kinds;
		return Connector;
	}

	/** Pure-C++ copy of the S1-07 Docks kit, so the property tests need no content. */
	TArray<FHollowRoomTemplate> SyntheticPool()
	{
		constexpr EHollowRoomSide North = EHollowRoomSide::North;
		constexpr EHollowRoomSide East = EHollowRoomSide::East;
		constexpr EHollowRoomSide South = EHollowRoomSide::South;
		constexpr EHollowRoomSide West = EHollowRoomSide::West;
		return {
			Room(TEXT("Room:Spawn"), EHollowRoomRole::Spawn, {2, 2}, {Conn(1, 0, North, Door)}),
			Room(TEXT("Room:Extraction"), EHollowRoomRole::Extraction, {3, 2}, {Conn(0, 0, South, Door | Open)}),
			Room(TEXT("Room:CoreVault"), EHollowRoomRole::CoreVault, {2, 2}, {Conn(0, 0, South, Door | Bulkhead), Conn(1, 1, North, Door | Bulkhead)}),
			Room(TEXT("Room:Hangar"), EHollowRoomRole::Path, {3, 3}, {Conn(2, 1, North, Any), Conn(1, 2, East, Any), Conn(0, 1, South, Any), Conn(1, 0, West, Any)}),
			Room(TEXT("Room:Straight"), EHollowRoomRole::Path, {1, 2}, {Conn(0, 0, West, Any), Conn(0, 1, East, Any)}),
			Room(TEXT("Room:Corner"), EHollowRoomRole::Path, {1, 1}, {Conn(0, 0, South, Any), Conn(0, 0, East, Any)}),
			Room(TEXT("Room:JunctionT"), EHollowRoomRole::Path, {1, 1}, {Conn(0, 0, South, Any), Conn(0, 0, East, Any), Conn(0, 0, West, Any)}),
			Room(TEXT("Room:Storage"), EHollowRoomRole::Branch, {2, 2}, {Conn(0, 0, West, Door | Bulkhead), Conn(1, 1, East, Door | Bulkhead)}),
		};
	}

	/** Test data mirroring DA_Biome_Docks. */
	FHollowSectorGenRules SyntheticRules()
	{
		FHollowSectorGenRules Rules;
		Rules.SpawnRoomType = HollowTags::Room_Type_Spawn;
		Rules.ExtractionRoomType = HollowTags::Room_Type_Extraction;
		Rules.CoreVaultRoomType = HollowTags::Room_Type_CoreVault;
		Rules.PathRoomTypes.AddTag(HollowTags::Room_Type_Corridor);
		Rules.BranchRoomTypes.AddTag(HollowTags::Room_Type_Storage);
		Rules.CriticalPathMin = 4;
		Rules.CriticalPathMax = 6;
		Rules.CoreVaultCount = 1;
		Rules.OptionalBranchesMin = 0;
		Rules.OptionalBranchesMax = 2;
		Rules.MaxBranchDepth = 2;
		Rules.MinLoops = 2;
		Rules.MaxBridgeRooms = 3;
		Rules.MinBulkheadConnections = 2;
		Rules.MaxRooms = 14;
		Rules.MaxExtentCells = 24;
		Rules.MaxAttempts = 32;
		Rules.MaxPlacementsPerAttempt = 3000;
		return Rules;
	}

	FHollowSectorGenInput SyntheticInput(const int32 Seed)
	{
		FHollowSectorGenInput Input;
		Input.Seed = Seed;
		Input.Rules = SyntheticRules();
		Input.Templates = SyntheticPool();
		return Input;
	}

	const FHollowRoomTemplate* FindTemplate(const TArray<FHollowRoomTemplate>& Pool, const FString& Id)
	{
		return Pool.FindByPredicate([&Id](const FHollowRoomTemplate& T) { return T.Id == Id; });
	}

	// ------------------------------------------------------------------ independent oracles

	/** Every cell is covered by at most one room (sorted cell list, no shared code with the generator). */
	bool CellsDisjoint(const FHollowSectorLayout& Layout)
	{
		TArray<FIntPoint> Cells;
		for (const FHollowPlacedRoom& R : Layout.Rooms)
		{
			for (int32 X = 0; X < R.Size.X; ++X)
			{
				for (int32 Y = 0; Y < R.Size.Y; ++Y)
				{
					Cells.Add(R.Cell + FIntPoint(X, Y));
				}
			}
		}
		Cells.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
		for (int32 Index = 1; Index < Cells.Num(); ++Index)
		{
			if (Cells[Index] == Cells[Index - 1])
			{
				return false;
			}
		}
		return true;
	}

	/** Every connection joins facing connectors of adjacent cells; every connector is used or sealed exactly once. */
	bool ConnectionsConsistent(const FHollowSectorLayout& Layout, const TArray<FHollowRoomTemplate>& Pool, FString& OutWhy)
	{
		auto WorldConnector = [&Layout, &Pool](const int32 RoomIndex, const int32 ConnectorIndex, FIntPoint& OutCell, EHollowRoomSide& OutSide)
		{
			const FHollowPlacedRoom& R = Layout.Rooms[RoomIndex];
			const FHollowRoomTemplate* T = FindTemplate(Pool, Layout.TemplateIds[R.Template]);
			if (!T || !T->Connectors.IsValidIndex(ConnectorIndex))
			{
				return false;
			}
			OutCell = R.Cell + RotateCell(T->Connectors[ConnectorIndex].Cell, T->Footprint, R.Yaw90);
			OutSide = RotateSide(T->Connectors[ConnectorIndex].Side, R.Yaw90);
			return true;
		};

		TArray<FIntPoint> Used;
		for (const FHollowPlacedConnection& C : Layout.Connections)
		{
			FIntPoint CellA, CellB;
			EHollowRoomSide SideA, SideB;
			if (!WorldConnector(C.RoomA, C.ConnectorA, CellA, SideA) || !WorldConnector(C.RoomB, C.ConnectorB, CellB, SideB))
			{
				OutWhy = TEXT("connection references a missing connector");
				return false;
			}
			if (CellA != C.CellA || SideA != C.SideA || CellB != CellA + Offset(SideA) || SideB != Opposite(SideA) || C.Kind == EHollowConnectorKind::None)
			{
				OutWhy = FString::Printf(TEXT("rooms %d-%d do not face each other"), C.RoomA, C.RoomB);
				return false;
			}
			Used.Add(FIntPoint(C.RoomA, C.ConnectorA));
			Used.Add(FIntPoint(C.RoomB, C.ConnectorB));
		}
		for (const FHollowSealedConnector& S : Layout.Sealed)
		{
			Used.Add(FIntPoint(S.Room, S.Connector));
		}
		int32 Total = 0;
		for (const FHollowPlacedRoom& R : Layout.Rooms)
		{
			Total += FindTemplate(Pool, Layout.TemplateIds[R.Template])->Connectors.Num();
		}
		Used.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
		for (int32 Index = 1; Index < Used.Num(); ++Index)
		{
			if (Used[Index] == Used[Index - 1])
			{
				OutWhy = TEXT("a connector is used twice");
				return false;
			}
		}
		if (Used.Num() != Total)
		{
			OutWhy = TEXT("some connectors are neither connected nor sealed");
			return false;
		}
		return true;
	}

	TArray<TArray<int32>> Neighbours(const FHollowSectorLayout& Layout, const int32 Without = INDEX_NONE)
	{
		TArray<TArray<int32>> Result;
		Result.SetNum(Layout.Rooms.Num());
		for (const FHollowPlacedConnection& C : Layout.Connections)
		{
			if (C.RoomA != Without && C.RoomB != Without)
			{
				Result[C.RoomA].AddUnique(C.RoomB);
				Result[C.RoomB].AddUnique(C.RoomA);
			}
		}
		return Result;
	}

	/** Plain BFS distance in links, INDEX_NONE if unreachable. */
	int32 Distance(const FHollowSectorLayout& Layout, const int32 From, const int32 To, const int32 Without = INDEX_NONE)
	{
		const TArray<TArray<int32>> Adjacent = Neighbours(Layout, Without);
		TArray<int32> Steps;
		Steps.Init(INDEX_NONE, Layout.Rooms.Num());
		TArray<int32> Queue = {From};
		Steps[From] = 0;
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			for (const int32 Next : Adjacent[Queue[Head]])
			{
				if (Steps[Next] == INDEX_NONE)
				{
					Steps[Next] = Steps[Queue[Head]] + 1;
					Queue.Add(Next);
				}
			}
		}
		return Steps[To];
	}

	/** Rooms that Spawn cannot reach while every bulkhead is shut (independent BFS over the other connections). */
	int32 CutOffByBulkheads(const FHollowSectorLayout& Layout)
	{
		TArray<TArray<int32>> Adjacent;
		Adjacent.SetNum(Layout.Rooms.Num());
		int32 Spawn = INDEX_NONE;
		for (int32 Index = 0; Index < Layout.Rooms.Num(); ++Index)
		{
			Spawn = Layout.Rooms[Index].Role == EHollowRoomRole::Spawn ? Index : Spawn;
		}
		for (const FHollowPlacedConnection& C : Layout.Connections)
		{
			if (C.Kind != EHollowConnectorKind::Bulkhead)
			{
				Adjacent[C.RoomA].AddUnique(C.RoomB);
				Adjacent[C.RoomB].AddUnique(C.RoomA);
			}
		}
		if (Spawn == INDEX_NONE)
		{
			return Layout.Rooms.Num();
		}
		TArray<bool> Seen;
		Seen.Init(false, Layout.Rooms.Num());
		Seen[Spawn] = true;
		TArray<int32> Queue = {Spawn};
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			for (const int32 Next : Adjacent[Queue[Head]])
			{
				if (!Seen[Next])
				{
					Seen[Next] = true;
					Queue.Add(Next);
				}
			}
		}
		return Layout.Rooms.Num() - Queue.Num();
	}

	int32 CountBulkheads(const FHollowSectorLayout& Layout)
	{
		int32 Count = 0;
		for (const FHollowPlacedConnection& C : Layout.Connections)
		{
			Count += C.Kind == EHollowConnectorKind::Bulkhead ? 1 : 0;
		}
		return Count;
	}

	int32 FindRole(const FHollowSectorLayout& Layout, const EHollowRoomRole Role, int32& OutCount)
	{
		int32 Found = INDEX_NONE;
		OutCount = 0;
		for (int32 Index = 0; Index < Layout.Rooms.Num(); ++Index)
		{
			if (Layout.Rooms[Index].Role == Role)
			{
				Found = Index;
				++OutCount;
			}
		}
		return Found;
	}

	/** Runs the sweep; calls Check for every successful layout; fails the test for every seed that does not generate. */
	template <typename TCheck>
	void Sweep(FAutomationTestBase& Test, const FHollowSectorGenInput& Base, TCheck&& Check, int32* OutTotalAttempts = nullptr, const int32 Seeds = SweepSeeds)
	{
		int32 TotalAttempts = 0;
		for (int32 Seed = FirstSeed; Seed < FirstSeed + Seeds; ++Seed)
		{
			FHollowSectorGenInput Input = Base;
			Input.Seed = Seed;
			FHollowSectorLayout Layout;
			FHollowSectorGenReport Report;
			if (FHollowSectorGenerator::Generate(Input, Layout, &Report) != EHollowGenResult::Success)
			{
				FString Reasons;
				for (const EHollowGenFailure Failure : Report.AttemptFailures)
				{
					Reasons += HollowEnumToString(Failure).Left(4) + TEXT(" ");
				}
				Test.AddError(FString::Printf(TEXT("Seed %d did not generate: %s"), Seed, *Reasons));
				continue;
			}
			TotalAttempts += Layout.Attempts;
			Check(Seed, Layout);
		}
		if (OutTotalAttempts)
		{
			*OutTotalAttempts = TotalAttempts;
		}
	}

	/**
	 * A real sector's pool (its plugin's rooms and biome) on PoolSeeds seeds: every seed generates; rooms never overlap,
	 * connections match the kit, the sector stays in its limits; one spawn, one extraction, the vaults off the critical
	 * route, every room reachable; the biome's loops; no room behind bulkheads; the manifest rebuilds the same layout
	 * and stays under its budget.
	 */
	bool CheckRealPool(FAutomationTestBase& Test, const TCHAR* DataPath, const TCHAR* Plugin)
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

		UHollowBiomeDefinition* Biome = nullptr;
		TArray<UHollowRoomDefinition*> Rooms;
		for (const FAssetData& Asset : Assets)
		{
			UObject* Object = Asset.GetAsset();
			Biome = Biome ? Biome : Cast<UHollowBiomeDefinition>(Object);
			if (UHollowRoomDefinition* RoomDefinition = Cast<UHollowRoomDefinition>(Object))
			{
				Rooms.Add(RoomDefinition);
			}
		}
		if (!Test.TestNotNull(FString::Printf(TEXT("%s: a biome in the sector plugin"), Plugin), Biome))
		{
			return false;
		}

		const FHollowSectorGenRules& Rules = Biome->GenRules;
		FHollowSectorGenInput Base;
		Base.Rules = Rules;
		Base.Templates = HollowSectorTemplates::MakeTemplates(TArray<const UHollowRoomDefinition*>(Rooms), Rules);
		Test.TestEqual(FString::Printf(TEXT("%s: every room has a role under its rules"), Plugin), Base.Templates.Num(), Rooms.Num());
		const TArray<FString> Errors = FHollowSectorGenerator::ValidateInput(Base);
		for (const FString& Error : Errors)
		{
			Test.AddError(Error);
		}
		if (!Errors.IsEmpty())
		{
			return false;
		}

		int32 Generated = 0;
		int32 TotalAttempts = 0;
		int32 TotalRooms = 0;
		int32 TotalLoops = 0;
		int32 MaxBytes = 0;
		const double Start = FPlatformTime::Seconds();
		Sweep(Test, Base, [&](const int32 Seed, const FHollowSectorLayout& Layout)
		{
			++Generated;
			TotalRooms += Layout.Rooms.Num();
			auto Fail = [&Test, Plugin, Seed](const FString& Why) { Test.AddError(FString::Printf(TEXT("%s seed %d: %s"), Plugin, Seed, *Why)); };

			// Geometry: disjoint rooms, connections the kit allows, inside the sector's limits.
			if (!CellsDisjoint(Layout))
			{
				Fail(TEXT("rooms overlap"));
			}
			FString Why;
			if (!ConnectionsConsistent(Layout, Base.Templates, Why))
			{
				Fail(Why);
			}
			int32 MaxX = 0;
			int32 MaxY = 0;
			for (const FHollowPlacedRoom& R : Layout.Rooms)
			{
				MaxX = FMath::Max(MaxX, R.Cell.X + R.Size.X);
				MaxY = FMath::Max(MaxY, R.Cell.Y + R.Size.Y);
			}
			if (MaxX > Rules.MaxExtentCells || MaxY > Rules.MaxExtentCells || Layout.Rooms.Num() > Rules.MaxRooms)
			{
				Fail(TEXT("the sector exceeds its limits"));
			}

			// Route: one spawn, one extraction, the vaults as detours, every room reachable.
			int32 Spawns = 0, Extractions = 0, Vaults = 0;
			const int32 Spawn = FindRole(Layout, EHollowRoomRole::Spawn, Spawns);
			const int32 Extraction = FindRole(Layout, EHollowRoomRole::Extraction, Extractions);
			const int32 Vault = FindRole(Layout, EHollowRoomRole::CoreVault, Vaults);
			if (Spawns != 1 || Extractions != 1 || Vaults != Rules.CoreVaultCount)
			{
				Fail(FString::Printf(TEXT("%d spawn, %d extraction, %d core vault rooms"), Spawns, Extractions, Vaults));
				return;
			}
			if (Distance(Layout, Spawn, Extraction) < Rules.CriticalPathMin + 1)
			{
				Fail(TEXT("the route is shorter than the minimum"));
			}
			for (int32 Index = 0; Index < Layout.Rooms.Num(); ++Index)
			{
				if (Distance(Layout, Spawn, Index) == INDEX_NONE)
				{
					Fail(FString::Printf(TEXT("room %d is unreachable"), Index));
				}
			}
			if (Distance(Layout, Spawn, Extraction, Vault) == INDEX_NONE || Layout.Rooms[Vault].bCriticalPath)
			{
				Fail(TEXT("the core vault lies on the critical path"));
			}

			// Loops and bulkheads (S3-01: a shut bulkhead may close a loop, never cut a room off).
			TArray<FIntPoint> Pairs;
			for (const FHollowPlacedConnection& C : Layout.Connections)
			{
				Pairs.AddUnique(FIntPoint(FMath::Min(C.RoomA, C.RoomB), FMath::Max(C.RoomA, C.RoomB)));
			}
			const int32 Loops = Pairs.Num() - Layout.Rooms.Num() + 1;
			TotalLoops += Loops;
			if (Loops < Rules.MinLoops)
			{
				Fail(FString::Printf(TEXT("%d loops"), Loops));
			}
			const int32 CutOff = CutOffByBulkheads(Layout);
			if (CutOff > 0 || CountBulkheads(Layout) < Rules.MinBulkheadConnections)
			{
				Fail(FString::Printf(TEXT("%d room(s) behind bulkheads, %d bulkheads"), CutOff, CountBulkheads(Layout)));
			}

			// The manifest the server replicates: the same layout back, under its budget.
			TArray<UHollowRoomDefinition*> Definitions;
			for (const FString& Id : Layout.TemplateIds)
			{
				UHollowRoomDefinition* const* Found = Rooms.FindByPredicate([&Id](const UHollowRoomDefinition* R) { return R->GetPrimaryAssetId().ToString() == Id; });
				Definitions.Add(Found ? *Found : nullptr);
			}
			FHollowSectorManifest Manifest;
			FHollowSectorLayout Rebuilt;
			FString Error;
			if (!HollowSectorManifest::Build(Layout, Definitions, Biome, FName(Plugin), PoolBuildId, Manifest, Error)
				|| !HollowSectorManifest::Reconstruct(Manifest, Rebuilt, Error))
			{
				Fail(Error);
				return;
			}
			if (FHollowSectorGenerator::ComputeLayoutHash(Rebuilt) != Manifest.LayoutHash)
			{
				Fail(TEXT("the rebuilt layout hashes differently"));
			}
			MaxBytes = FMath::Max(MaxBytes, HollowSectorManifest::EstimateNetBytes(Manifest));

			// The biome's mechanics are in every sector (S7-02: the Cathedral's altar and arcs).
			for (const FGameplayTag& Required : Rules.RequiredSocketTypes)
			{
				const bool bPlaced = Layout.Rooms.ContainsByPredicate([&Layout, &Base, &Required](const FHollowPlacedRoom& Room)
				{
					const FHollowRoomTemplate* Template = FindTemplate(Base.Templates, Layout.TemplateIds[Room.Template]);
					return Template && Template->SocketTypes.HasTagExact(Required);
				});
				if (!bPlaced)
				{
					Fail(FString::Printf(TEXT("no room with %s"), *Required.ToString()));
				}
			}
		}, &TotalAttempts, PoolSeeds);

		Test.TestEqual(FString::Printf(TEXT("%s: every seed generates"), Plugin), Generated, PoolSeeds);
		Test.TestTrue(FString::Printf(TEXT("%s: manifests under %d bytes (largest %d)"), Plugin, MaxManifestBytes, MaxBytes), MaxBytes < MaxManifestBytes);
		const double Seeds = PoolSeeds;
		Test.AddInfo(FString::Printf(TEXT("%s: %d/%d seeds, avg %.1f rooms, %.2f loops, %.2f attempts, %.2f ms per sector, manifest <= %d bytes"), Plugin,
			Generated, PoolSeeds, TotalRooms / Seeds, TotalLoops / Seeds, TotalAttempts / Seeds, (FPlatformTime::Seconds() - Start) * 1000.0 / Seeds, MaxBytes));
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGenRandomTest, "Hollow.PCG.Generator.Random",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGenRandomTest::RunTest(const FString& Parameters)
{
	// Known-answer vector of pcg-c-basic (pcg32-demo, pcg32_srandom_r(42, 54)): proves the PCG32 implementation.
	constexpr uint64 DemoState = 42u;
	constexpr uint64 DemoStream = 54u;
	const uint32 Expected[] = {0xa15c02b7u, 0x7b47f409u, 0xba1d3330u, 0x83d2f293u, 0xbfa4784bu, 0xcbed606eu};
	FHollowRandom Demo(DemoState, DemoStream);
	for (const uint32 Value : Expected)
	{
		const uint32 Got = Demo.Next();
		TestTrue(FString::Printf(TEXT("PCG32 reference output %08x (got %08x)"), Value, Got), Got == Value);
	}

	constexpr int32 Draws = 1000;
	constexpr int32 Low = -3;
	constexpr int32 High = 4;
	FHollowRandom Rng(HollowRandom::DeriveSeed(HollowGenTests::GoldenSeed, 0));
	bool bInRange = true;
	bool bSawLow = false;
	bool bSawHigh = false;
	for (int32 Draw = 0; Draw < Draws; ++Draw)
	{
		const int32 Value = Rng.RangeInclusive(Low, High);
		bInRange &= Value >= Low && Value <= High;
		bSawLow |= Value == Low;
		bSawHigh |= Value == High;
	}
	TestTrue(TEXT("RangeInclusive stays in range and reaches both ends"), bInRange && bSawLow && bSawHigh);
	TestTrue(TEXT("Derived seeds differ per attempt"), HollowRandom::DeriveSeed(1, 0) != HollowRandom::DeriveSeed(1, 1));
	TestTrue(TEXT("Derived seeds differ per seed"), HollowRandom::DeriveSeed(1, 0) != HollowRandom::DeriveSeed(2, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGenRotationTest, "Hollow.PCG.Generator.Rotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGenRotationTest::RunTest(const FString& Parameters)
{
	using namespace HollowGridMath;
	constexpr int32 MaxSize = 3;

	TestTrue(TEXT("North turns to East"), RotateSide(EHollowRoomSide::North, 1) == EHollowRoomSide::East);
	TestTrue(TEXT("Opposite of West is East"), Opposite(EHollowRoomSide::West) == EHollowRoomSide::East);

	bool bAllOk = true;
	for (int32 SizeX = 1; SizeX <= MaxSize; ++SizeX)
	{
		for (int32 SizeY = 1; SizeY <= MaxSize; ++SizeY)
		{
			const FIntPoint Footprint(SizeX, SizeY);
			for (int32 X = 0; X < SizeX; ++X)
			{
				for (int32 Y = 0; Y < SizeY; ++Y)
				{
					const FIntPoint Cell(X, Y);
					// Four quarter turns are the identity.
					FIntPoint Turned = Cell;
					FIntPoint TurnedSize = Footprint;
					for (int32 Step = 0; Step < SideCount; ++Step)
					{
						Turned = RotateCell(Turned, TurnedSize, 1);
						TurnedSize = RotateSize(TurnedSize, 1);
					}
					bAllOk &= Turned == Cell && TurnedSize == Footprint;

					for (uint8 Yaw = 0; Yaw < SideCount; ++Yaw)
					{
						const FIntPoint Rotated = RotateCell(Cell, Footprint, Yaw);
						const FIntPoint Size = RotateSize(Footprint, Yaw);
						bAllOk &= Rotated.X >= 0 && Rotated.Y >= 0 && Rotated.X < Size.X && Rotated.Y < Size.Y;

						// Same answer as rotating the cell centre continuously about the room origin and adding the pivot
						// (doubled coordinates keep it integer): this is what the assembly does with the Level Instance.
						FIntPoint Centre(2 * X + 1, 2 * Y + 1);
						for (uint8 Step = 0; Step < Yaw; ++Step)
						{
							Centre = FIntPoint(-Centre.Y, Centre.X);
						}
						bAllOk &= Centre + PivotOffset(Footprint, Yaw) * 2 == Rotated * 2 + FIntPoint(1, 1);
					}
				}
			}
		}
	}
	TestTrue(TEXT("Cell rotation: identity after four turns, stays in the footprint, matches the pivot transform"), bAllOk);

	bool bSidesOk = true;
	for (int32 Side = 0; Side < SideCount; ++Side)
	{
		const EHollowRoomSide S = static_cast<EHollowRoomSide>(Side);
		const FIntPoint Step = Offset(S);
		bSidesOk &= Offset(RotateSide(S, 1)) == FIntPoint(-Step.Y, Step.X) && Offset(Opposite(S)) == FIntPoint(-Step.X, -Step.Y);
	}
	TestTrue(TEXT("Side rotation matches vector rotation"), bSidesOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGenInputTest, "Hollow.PCG.Generator.InputValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGenInputTest::RunTest(const FString& Parameters)
{
	using namespace HollowGenTests;
	TestTrue(TEXT("The synthetic input is valid"), FHollowSectorGenerator::ValidateInput(SyntheticInput(GoldenSeed)).IsEmpty());

	AddExpectedMessage(TEXT("Sector generator input:"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/0, /*IsRegex=*/false);
	FHollowSectorLayout Layout;

	FHollowSectorGenInput NoSpawn = SyntheticInput(GoldenSeed);
	NoSpawn.Templates.RemoveAll([](const FHollowRoomTemplate& T) { return T.Role == EHollowRoomRole::Spawn; });
	TestTrue(TEXT("A pool without a spawn room is rejected"), FHollowSectorGenerator::Generate(NoSpawn, Layout) == EHollowGenResult::InvalidInput);

	FHollowSectorGenInput BadRules = SyntheticInput(GoldenSeed);
	BadRules.Rules.CriticalPathMin = BadRules.Rules.CriticalPathMax + 1;
	TestTrue(TEXT("CriticalPathMin > Max is rejected"), FHollowSectorGenerator::Generate(BadRules, Layout) == EHollowGenResult::InvalidInput);

	FHollowSectorGenInput InnerConnector = SyntheticInput(GoldenSeed);
	InnerConnector.Templates[0].Connectors[0].Side = EHollowRoomSide::South;   // (1,0) South of a 2x2 room faces cell (0,0)
	TestTrue(TEXT("A connector on an inner edge is rejected"), FHollowSectorGenerator::Generate(InnerConnector, Layout) == EHollowGenResult::InvalidInput);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGenSeedParityTest, "Hollow.PCG.Generator.SeedParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGenSeedParityTest::RunTest(const FString& Parameters)
{
	using namespace HollowGenTests;

	FHollowSectorLayout First, Second, Shuffled;
	const FHollowSectorGenInput Input = SyntheticInput(GoldenSeed);
	const bool bOk = FHollowSectorGenerator::Generate(Input, First) == EHollowGenResult::Success
		&& FHollowSectorGenerator::Generate(Input, Second) == EHollowGenResult::Success;
	if (!TestTrue(TEXT("The golden seed generates"), bOk))
	{
		return false;
	}
	TestTrue(TEXT("Same seed, same layout (every field)"), First == Second);

	FHollowSectorGenInput Reordered = Input;
	Algo::Reverse(Reordered.Templates);
	Reordered.Templates.Swap(0, Reordered.Templates.Num() / 2);
	TestTrue(TEXT("Pool order does not matter"), FHollowSectorGenerator::Generate(Reordered, Shuffled) == EHollowGenResult::Success && Shuffled == First);
	TestTrue(TEXT("The hash is reproducible"), FHollowSectorGenerator::ComputeLayoutHash(First) == First.LayoutHash);

	TArray<uint32> Hashes;
	for (int32 Seed = FirstSeed; Seed < FirstSeed + SweepSeeds / 2; ++Seed)
	{
		FHollowSectorLayout Layout;
		if (FHollowSectorGenerator::Generate(SyntheticInput(Seed), Layout) == EHollowGenResult::Success)
		{
			Hashes.AddUnique(Layout.LayoutHash);
		}
	}
	TestTrue(FString::Printf(TEXT("Different seeds give different layouts (%d unique of %d)"), Hashes.Num(), SweepSeeds / 2),
		Hashes.Num() * PercentScale >= (SweepSeeds / 2) * UniquePercentMin);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGenNoOverlapTest, "Hollow.PCG.Generator.NoOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGenNoOverlapTest::RunTest(const FString& Parameters)
{
	using namespace HollowGenTests;
	const TArray<FHollowRoomTemplate> Pool = SyntheticPool();
	const FHollowSectorGenRules Rules = SyntheticRules();
	Sweep(*this, SyntheticInput(FirstSeed), [this, &Pool, &Rules](const int32 Seed, const FHollowSectorLayout& Layout)
	{
		if (!CellsDisjoint(Layout))
		{
			AddError(FString::Printf(TEXT("Seed %d: rooms overlap"), Seed));
		}
		FString Why;
		if (!ConnectionsConsistent(Layout, Pool, Why))
		{
			AddError(FString::Printf(TEXT("Seed %d: %s"), Seed, *Why));
		}
		int32 MaxX = 0;
		int32 MaxY = 0;
		for (const FHollowPlacedRoom& R : Layout.Rooms)
		{
			MaxX = FMath::Max(MaxX, R.Cell.X + R.Size.X);
			MaxY = FMath::Max(MaxY, R.Cell.Y + R.Size.Y);
		}
		if (MaxX > Rules.MaxExtentCells || MaxY > Rules.MaxExtentCells || Layout.Rooms.Num() > Rules.MaxRooms)
		{
			AddError(FString::Printf(TEXT("Seed %d: sector exceeds its limits"), Seed));
		}
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGenPathTest, "Hollow.PCG.Generator.PathSpawnToExtraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGenPathTest::RunTest(const FString& Parameters)
{
	using namespace HollowGenTests;
	const FHollowSectorGenRules Rules = SyntheticRules();
	Sweep(*this, SyntheticInput(FirstSeed), [this, &Rules](const int32 Seed, const FHollowSectorLayout& Layout)
	{
		int32 Spawns = 0, Extractions = 0, Vaults = 0;
		const int32 Spawn = FindRole(Layout, EHollowRoomRole::Spawn, Spawns);
		const int32 Extraction = FindRole(Layout, EHollowRoomRole::Extraction, Extractions);
		const int32 Vault = FindRole(Layout, EHollowRoomRole::CoreVault, Vaults);
		if (Spawns != 1 || Extractions != 1 || Vaults != Rules.CoreVaultCount)
		{
			AddError(FString::Printf(TEXT("Seed %d: %d spawn, %d extraction, %d core vault rooms"), Seed, Spawns, Extractions, Vaults));
			return;
		}
		const int32 Route = Distance(Layout, Spawn, Extraction);
		if (Route < Rules.CriticalPathMin + 1)
		{
			AddError(FString::Printf(TEXT("Seed %d: route of %d links is shorter than the minimum"), Seed, Route));
		}
		for (int32 Index = 0; Index < Layout.Rooms.Num(); ++Index)
		{
			if (Distance(Layout, Spawn, Index) == INDEX_NONE)
			{
				AddError(FString::Printf(TEXT("Seed %d: room %d is unreachable"), Seed, Index));
			}
		}
		// The vault is a detour: extraction stays reachable without walking through it.
		if (Distance(Layout, Spawn, Extraction, Vault) == INDEX_NONE || Layout.Rooms[Vault].bCriticalPath)
		{
			AddError(FString::Printf(TEXT("Seed %d: the core vault lies on the critical path"), Seed));
		}
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGenMinLoopsTest, "Hollow.PCG.Generator.MinLoops",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGenMinLoopsTest::RunTest(const FString& Parameters)
{
	using namespace HollowGenTests;
	const FHollowSectorGenRules Rules = SyntheticRules();
	int32 TotalLoops = 0;
	int32 TotalRooms = 0;
	int32 TotalAttempts = 0;
	const double Start = FPlatformTime::Seconds();
	Sweep(*this, SyntheticInput(FirstSeed), [this, &Rules, &TotalLoops, &TotalRooms](const int32 Seed, const FHollowSectorLayout& Layout)
	{
		TArray<FIntPoint> Pairs;
		for (const FHollowPlacedConnection& C : Layout.Connections)
		{
			Pairs.AddUnique(FIntPoint(FMath::Min(C.RoomA, C.RoomB), FMath::Max(C.RoomA, C.RoomB)));
		}
		const int32 Loops = Pairs.Num() - Layout.Rooms.Num() + 1;   // connected (checked by PathSpawnToExtraction)
		TotalLoops += Loops;
		TotalRooms += Layout.Rooms.Num();
		if (Loops < Rules.MinLoops)
		{
			AddError(FString::Printf(TEXT("Seed %d: %d loops"), Seed, Loops));
		}
	}, &TotalAttempts);
	const double Milliseconds = (FPlatformTime::Seconds() - Start) * 1000.0;
	AddInfo(FString::Printf(TEXT("%d seeds: avg %.2f loops, %.1f rooms, %.2f attempts, %.2f ms per sector"), SweepSeeds,
		double(TotalLoops) / SweepSeeds, double(TotalRooms) / SweepSeeds, double(TotalAttempts) / SweepSeeds, Milliseconds / SweepSeeds));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGenBulkheadDetourTest, "Hollow.PCG.Generator.BulkheadsAreDetours",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGenBulkheadDetourTest::RunTest(const FString& Parameters)
{
	// A shut bulkhead is a wall until something breaks it (S3-01): it may close a loop, never cut a room off.
	using namespace HollowGenTests;
	const FHollowSectorGenRules Rules = SyntheticRules();
	if (!TestTrue(TEXT("The synthetic rules ask for bulkheads"), Rules.MinBulkheadConnections > 0))
	{
		return false;
	}
	Sweep(*this, SyntheticInput(FirstSeed), [this, &Rules](const int32 Seed, const FHollowSectorLayout& Layout)
	{
		const int32 CutOff = CutOffByBulkheads(Layout);
		if (CutOff > 0)
		{
			AddError(FString::Printf(TEXT("Seed %d: %d room(s) only reachable through a bulkhead"), Seed, CutOff));
		}
		if (CountBulkheads(Layout) < Rules.MinBulkheadConnections)
		{
			AddError(FString::Printf(TEXT("Seed %d: %d bulkheads, quota %d"), Seed, CountBulkheads(Layout), Rules.MinBulkheadConnections));
		}
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGenGoldenTest, "Hollow.PCG.Generator.GoldenLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGenGoldenTest::RunTest(const FString& Parameters)
{
	using namespace HollowGenTests;
	FHollowSectorLayout Layout;
	if (!TestTrue(TEXT("The golden seed generates"), FHollowSectorGenerator::Generate(SyntheticInput(GoldenSeed), Layout) == EHollowGenResult::Success))
	{
		return false;
	}
	// Same value on every platform (Mac arm64 / Win64 x64): integer-only generation and hashing.
	TestTrue(FString::Printf(TEXT("Golden layout hash %08X (got %08X)"), GoldenHash, Layout.LayoutHash), Layout.LayoutHash == GoldenHash);
	for (const FString& Line : FHollowSectorGenerator::RenderAscii(Layout))
	{
		AddInfo(Line);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGenRequiredSocketsTest, "Hollow.PCG.Generator.RequiredSockets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGenRequiredSocketsTest::RunTest(const FString& Parameters)
{
	// S7-02 (ADR-0039): a socket type the rules require is in a placed room of every layout. On the synthetic pool only
	// the hangar carries loot: required, every layout made has a hangar, attempts without one are retried (MissingSocket),
	// the seed still decides; a socket no template has is an input error. (Some synthetic seeds run out of attempts:
	// the pool's loops are tight; the real pools' every seed is Hollow.PCG.Generator.{Docks,Cathedral}Pool.)
	using namespace HollowGenTests;
	FHollowSectorGenInput Base = SyntheticInput(FirstSeed);
	FHollowRoomTemplate* Hangar = Base.Templates.FindByPredicate([](const FHollowRoomTemplate& T) { return T.Id == TEXT("Room:Hangar"); });
	if (!TestNotNull(TEXT("The hangar template"), Hangar))
	{
		return false;
	}
	Hangar->SocketTypes.AddTag(HollowTags::Socket_Loot);
	Base.Rules.RequiredSocketTypes.AddTag(HollowTags::Socket_Loot);
	TestTrue(TEXT("Input with the socket in the pool is valid"), FHollowSectorGenerator::ValidateInput(Base).IsEmpty());

	int32 Generated = 0;
	int32 MissingSocketFailures = 0;
	for (int32 Seed = FirstSeed; Seed < FirstSeed + SweepSeeds; ++Seed)
	{
		FHollowSectorGenInput Input = Base;
		Input.Seed = Seed;
		FHollowSectorLayout Layout;
		FHollowSectorGenReport Report;
		const bool bMade = FHollowSectorGenerator::Generate(Input, Layout, &Report) == EHollowGenResult::Success;
		MissingSocketFailures += Report.AttemptFailures.FilterByPredicate([](const EHollowGenFailure F) { return F == EHollowGenFailure::MissingSocket; }).Num();
		if (!bMade)
		{
			continue;
		}
		++Generated;
		if (!Layout.Rooms.ContainsByPredicate([&Layout](const FHollowPlacedRoom& Room) { return Layout.TemplateIds[Room.Template] == TEXT("Room:Hangar"); }))
		{
			AddError(FString::Printf(TEXT("Seed %d: no room with the required socket"), Seed));
		}
		FHollowSectorLayout Second;
		if (FHollowSectorGenerator::Generate(Input, Second) != EHollowGenResult::Success || Second.LayoutHash != Layout.LayoutHash)
		{
			AddError(FString::Printf(TEXT("Seed %d: not the same layout twice"), Seed));
		}
	}
	AddInfo(FString::Printf(TEXT("%d/%d seeds made, %d attempt(s) retried for the socket"), Generated, SweepSeeds, MissingSocketFailures));
	TestTrue(TEXT("Layouts are made"), Generated > 0);
	TestTrue(TEXT("Attempts without the socket are retried"), MissingSocketFailures > 0);

	FHollowSectorGenInput Missing = SyntheticInput(FirstSeed);
	Missing.Rules.RequiredSocketTypes.AddTag(HollowTags::Socket_Altar);
	TestFalse(TEXT("A socket no template has: invalid input"), FHollowSectorGenerator::ValidateInput(Missing).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGenDocksPoolTest, "Hollow.PCG.Generator.DocksPool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGenDocksPoolTest::RunTest(const FString& Parameters)
{
	using namespace HollowGenTests;
	return CheckRealPool(*this, DocksData, DocksPlugin);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowGenCathedralPoolTest, "Hollow.PCG.Generator.CathedralPool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowGenCathedralPoolTest::RunTest(const FString& Parameters)
{
	// S7-01 (ADR-0038): the Reactor Cathedral's own kit and rules generate on every seed of 1000.
	using namespace HollowGenTests;
	return CheckRealPool(*this, CathedralData, CathedralPlugin);
}

#endif // WITH_DEV_AUTOMATION_TESTS
