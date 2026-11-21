#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Assembly/HollowSectorManifest.h"
#include "CollisionQueryParams.h"
#include "Data/HollowLevelMetrics.h"
#include "Data/HollowRoomDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Generation/HollowGridMath.h"
#include "Rules/HollowLightBudget.h"
#include "Tests/AutomationCommon.h"
#include "Util/HollowEnumUtils.h"

namespace HollowArtLockTests
{
	const TCHAR* const DocksData = TEXT("/HollowSector_Docks/Data");
	constexpr int32 DocksRooms = 8;
	/** Rooms are streamed side by side this far apart (test data: far more than any room's footprint). */
	constexpr double RoomSpacing = 20000.0;
	constexpr double StreamTimeoutSeconds = 60.0;
	/** A pawn-sized capsule (test data, as in Hollow.PCG.Sector.Assembly; the ExoFrame's own is BP_ExoFrame data). */
	constexpr float PawnRadius = 34.f;
	constexpr float PawnHalfHeight = 96.f;
	/** Clearance above the floor and past the wall, so only what stands in the opening can stop the sweep. */
	constexpr double FloorClearance = 10.0;
	constexpr double SweepMargin = 20.0;
	/** The self-check's blocker: an engine cube (100 uu) set into the first opening. */
	const TCHAR* const CubeMesh = TEXT("/Engine/BasicShapes/Cube.Cube");
	constexpr double CubeSize = 100.0;

	TArray<UHollowRoomDefinition*> DocksRoomDefinitions()
	{
		IAssetRegistry& Registry = IAssetRegistry::GetChecked();
		Registry.ScanPathsSynchronous({FString(DocksData)}, /*bForceRescan=*/false);
		FARFilter Filter;
		Filter.PackagePaths.Add(FName(DocksData));
		Filter.bRecursivePaths = true;
		Filter.ClassPaths.Add(UHollowRoomDefinition::StaticClass()->GetClassPathName());
		TArray<FAssetData> Assets;
		Registry.GetAssets(Filter, Assets);
		Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });
		TArray<UHollowRoomDefinition*> Rooms;
		for (const FAssetData& Asset : Assets)
		{
			if (UHollowRoomDefinition* Room = Cast<UHollowRoomDefinition>(Asset.GetAsset()))
			{
				Rooms.Add(Room);
			}
		}
		return Rooms;
	}

	FHollowLightInfo Light(const bool bShadowed, const bool bLightFunction)
	{
		FHollowLightInfo Info;
		Info.bShadowed = bShadowed;
		Info.bLightFunction = bLightFunction;
		return Info;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowArtLockTest, "Hollow.PCG.DocksArtLock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowArtLockTest::RunTest(const FString& Parameters)
{
	// ADR-0027: a room finished by hand is art-locked on its definition. The flag is editor data (nothing of it is
	// cooked); the strict checks keep running on a locked room, and only the hero room's exact rig gives way to the
	// budget plus one failing panel.
	using namespace HollowArtLockTests;
	const FProperty* Flag = FindFProperty<FProperty>(UHollowRoomDefinition::StaticClass(), TEXT("bArtLocked"));
	TestTrue(TEXT("The lock is editor-only data"), Flag && Flag->HasAnyPropertyFlags(CPF_EditorOnly));

	const TArray<UHollowRoomDefinition*> Rooms = DocksRoomDefinitions();
	TestEqual(TEXT("Docks rooms"), Rooms.Num(), DocksRooms);
#if WITH_EDITORONLY_DATA
	for (UHollowRoomDefinition* Room : Rooms)
	{
		TestEqual(FString::Printf(TEXT("%s: IsArtLocked follows the flag"), *Room->GetName()), Room->IsArtLocked(), Room->bArtLocked);
	}
#endif

	const TArray<FHollowLightInfo> Rig = {Light(true, false), Light(true, true), Light(false, true), Light(false, false)};
	TestTrue(TEXT("The scripted rig: two shadowed panels, one failing"), HollowLightBudget::CheckHeroRig(Rig, /*bArtLocked=*/false).IsEmpty());
	const TArray<FHollowLightInfo> ByHand = {Light(true, true), Light(false, true), Light(false, false), Light(false, false), Light(false, false)};
	TestFalse(TEXT("A hand-lit room is not the scripted rig"), HollowLightBudget::CheckHeroRig(ByHand, /*bArtLocked=*/false).IsEmpty());
	TestTrue(TEXT("...but art-locked it only needs its failing panel"), HollowLightBudget::CheckHeroRig(ByHand, /*bArtLocked=*/true).IsEmpty());
	const TArray<FHollowLightInfo> NoFailing = {Light(true, false), Light(false, true)};
	TestFalse(TEXT("Art-locked without a failing panel: refused"), HollowLightBudget::CheckHeroRig(NoFailing, /*bArtLocked=*/true).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowDocksOpeningsTest, "Hollow.PCG.DocksOpenings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowDocksOpeningsTest::RunTest(const FString& Parameters)
{
	// ADR-0027: whatever stands in a room level (greybox or art), every connector's opening lets a player and the
	// largest agent (the Iron Pilgrim, UHollowLevelMetrics) walk through: each Docks room is streamed as a level
	// instance and both capsules are swept out through each opening with the pawns' own collision. A cube set into an
	// opening must stop the sweep (the check sees what it should).
	using namespace HollowArtLockTests;
	const TArray<UHollowRoomDefinition*> Rooms = DocksRoomDefinitions();
	if (!TestEqual(TEXT("Docks rooms"), Rooms.Num(), DocksRooms))
	{
		return false;
	}
	struct FState
	{
		FTestWorldWrapper World;
		TArray<TPair<UHollowRoomDefinition*, FVector>> Placed;
		TArray<ULevelStreamingDynamic*> Levels;
		double Deadline = 0.0;
	};
	const TSharedRef<FState> State = MakeShared<FState>();
	if (!State->World.CreateTestWorld(EWorldType::Game))
	{
		State->World.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = State->World.GetTestWorld();
	for (int32 Index = 0; Index < Rooms.Num(); ++Index)
	{
		const FVector Origin(Index * RoomSpacing, 0.0, 0.0);
		bool bLoaded = false;
		ULevelStreamingDynamic* Level = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(World, Rooms[Index]->RoomLevel, FTransform(Origin), bLoaded,
			FString::Printf(TEXT("OpeningsTest_%d"), Index));
		if (TestTrue(FString::Printf(TEXT("%s streams"), *Rooms[Index]->GetName()), bLoaded && Level))
		{
			State->Levels.Add(Level);
			State->Placed.Emplace(Rooms[Index], Origin);
		}
	}
	State->Deadline = FPlatformTime::Seconds() + StreamTimeoutSeconds;
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		UWorld* TestWorld = State->World.GetTestWorld();
		if (State->Levels.ContainsByPredicate([](const ULevelStreamingDynamic* Level) { return !Level->IsLevelVisible(); }))
		{
			if (FPlatformTime::Seconds() > State->Deadline)
			{
				AddError(TEXT("The room levels did not stream in time"));
				return true;
			}
			TestWorld->FlushLevelStreaming(EFlushLevelStreamingType::Full);
			State->World.TickTestWorld();
			return false;
		}

		const UHollowLevelMetrics* Metrics = GetDefault<UHollowLevelMetrics>();
		struct FCapsule
		{
			const TCHAR* Who;
			float Radius;
			float HalfHeight;
		};
		const FCapsule Capsules[] = {{TEXT("player"), PawnRadius, PawnHalfHeight},
			{TEXT("largest agent"), static_cast<float>(Metrics->LargestAgentRadius), 0.5f * static_cast<float>(Metrics->LargestAgentHeight)}};
		// A capsule swept out of the room through an opening, with the pawns' own collision; returns what stops it.
		const auto Sweep = [TestWorld, Metrics](const FTransform& Opening, const FVector& Out, const FCapsule& Capsule, FHitResult& Hit)
		{
			const FVector Centre = Opening.GetLocation() + FVector(0.0, 0.0, Capsule.HalfHeight + FloorClearance);
			const double Reach = Metrics->WallThickness + Capsule.Radius + SweepMargin;
			return TestWorld->SweepSingleByChannel(Hit, Centre - Out * Reach, Centre + Out * (Capsule.Radius + SweepMargin), FQuat::Identity, ECC_Pawn,
				FCollisionShape::MakeCapsule(Capsule.Radius, Capsule.HalfHeight), FCollisionQueryParams(SCENE_QUERY_STAT(HollowOpeningsTest)));
		};
		int32 Swept = 0;
		TOptional<TPair<FTransform, FVector>> First;
		for (const TPair<UHollowRoomDefinition*, FVector>& Room : State->Placed)
		{
			for (const FHollowRoomConnector& Connector : Room.Key->Connectors)
			{
				const FIntPoint Cell(Connector.Cell.X, Connector.Cell.Y);
				const FTransform Opening = HollowSectorManifest::OpeningTransform(Cell, Connector.Side, *Metrics, Room.Value);
				const FIntPoint Step = HollowGridMath::Offset(Connector.Side);
				const FVector Out(Step.X, Step.Y, 0.0);
				First = First.IsSet() ? First : TOptional<TPair<FTransform, FVector>>(TPair<FTransform, FVector>(Opening, Out));
				for (const FCapsule& Capsule : Capsules)
				{
					FHitResult Hit;
					TestFalse(FString::Printf(TEXT("%s, opening at %s %s: a %s walks through (blocked by %s)"), *Room.Key->GetName(), *Cell.ToString(),
						*HollowEnumToString(Connector.Side), Capsule.Who, *GetNameSafe(Hit.GetActor())), Sweep(Opening, Out, Capsule, Hit));
					++Swept;
				}
			}
		}
		AddInfo(FString::Printf(TEXT("%d sweeps through the openings of %d rooms"), Swept, State->Placed.Num()));
		TestTrue(TEXT("Every opening swept"), Swept > 0);

		// Self-check: a cube set into an opening (as art could be) stops the sweep.
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CubeMesh);
		if (First.IsSet() && TestNotNull(TEXT("Engine cube"), Cube))
		{
			const FVector Middle = First->Key.GetLocation() + FVector(0.0, 0.0, Capsules[0].HalfHeight + FloorClearance);
			AStaticMeshActor* Blocker = TestWorld->SpawnActor<AStaticMeshActor>(Middle, FRotator::ZeroRotator);
			Blocker->GetStaticMeshComponent()->SetStaticMesh(Cube);
			Blocker->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Blocker->SetActorScale3D(FVector(Metrics->DoorWidth / CubeSize, Metrics->DoorWidth / CubeSize, 1.0));   // whichever way the wall runs
			FHitResult Hit;
			TestTrue(TEXT("A mesh in the opening stops the sweep"), Sweep(First->Key, First->Value, Capsules[0], Hit) && Hit.GetActor() == Blocker);
			Blocker->Destroy();
		}
		for (ULevelStreamingDynamic* Level : State->Levels)
		{
			Level->SetIsRequestingUnloadAndRemoval(true);
		}
		TestWorld->FlushLevelStreaming(EFlushLevelStreamingType::Full);
		return true;
	}));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
