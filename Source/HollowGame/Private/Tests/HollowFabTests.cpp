#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace HollowFabTests
{
	// FAB-01 (ADR-0051): what Build/Art/fab_manifest.json keeps (prepare_fab_assets.py) and create_fab_dressing.py places.
	const TCHAR* const FabRoot = TEXT("/Game/Fab");
	const TCHAR* const ForbiddenClasses[] = {TEXT("SoundWave"), TEXT("SoundCue"), TEXT("SoundAttenuation"), TEXT("SoundClass"), TEXT("MetaSoundSource"),
		TEXT("World"), TEXT("Blueprint"), TEXT("WidgetBlueprint"), TEXT("LevelSequence"), TEXT("InputAction"), TEXT("InputMappingContext"),
		TEXT("UserDefinedStruct"), TEXT("UserDefinedEnum"), TEXT("DataTable")};
	constexpr int32 NiagaraSystems = 3;    // Teal, Purple, Yellow
	constexpr int32 PropMeshes = 19;       // OvergrownFactoryENV's kept props
	constexpr int32 EffectMeshes = 1;      // the absorption's own core mesh
	// DRONE-01 (ADR-0054): AdvancedCompanionDroneSystem gave only the Legacy drone's static mesh, two materials, four textures.
	const TCHAR* const DronePack = TEXT("/Game/Fab/AdvancedCompanionDroneSystem/");
	constexpr int32 DroneMeshes = 1;
	constexpr int32 DroneTextures = 4;
	constexpr int32 DroneTextureMax = 1024;
	constexpr int64 TargetMegabytes = 300; // the task's ceiling for Content/Fab (its budget_mb is GitHub's, 2000)
	const FName PropTag(TEXT("HollowFab"));
	struct FDressed
	{
		const TCHAR* Level;
		int32 Props;
	};
	const FDressed Levels[] = {
		{TEXT("/Game/Hollow/Maps/L_Hub_Skiff.L_Hub_Skiff"), 18},
		{TEXT("/HollowSector_Docks/Rooms/LV_Docks_CoreVault.LV_Docks_CoreVault"), 14},
		{TEXT("/HollowSector_Docks/Rooms/LV_Docks_Spawn_Airlock.LV_Docks_Spawn_Airlock"), 11},
		{TEXT("/HollowSector_Docks/Rooms/LV_Docks_Hangar.LV_Docks_Hangar"), 15},
		{TEXT("/HollowSector_Docks/Rooms/LV_Docks_Corridor_Straight.LV_Docks_Corridor_Straight"), 12},
		{TEXT("/HollowSector_Docks/Rooms/LV_Docks_Corridor_Corner.LV_Docks_Corridor_Corner"), 9},
		{TEXT("/HollowSector_Docks/Rooms/LV_Docks_Junction_T.LV_Docks_Junction_T"), 5},
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowFabContentTest, "Hollow.Game.Fab.Content",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowFabContentTest::RunTest(const FString& Parameters)
{
	// FAB-01 (ADR-0051): Content/Fab holds only the chosen effects and props and what they need: no sound (the Cathedral
	// stays silent of WAVs), no map, Blueprint, input, UI or data of the packs; it stays compact.
	using namespace HollowFabTests;
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	Registry.ScanPathsSynchronous(TArray<FString>{FabRoot}, /*bForceRescan=*/false);
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(FabRoot));
	Filter.bRecursivePaths = true;
	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);
	int32 Systems = 0;
	int32 Meshes = 0;
	int32 DroneTextureCount = 0;
	for (const FAssetData& Asset : Assets)
	{
		const FString Class = Asset.AssetClassPath.GetAssetName().ToString();
		if (Asset.PackageName.ToString().StartsWith(DronePack) && Class == TEXT("Texture2D"))
		{
			++DroneTextureCount;
			const UTexture2D* Texture = Cast<UTexture2D>(Asset.GetAsset());
			const FIntPoint Size = Texture ? Texture->GetImportedSize() : FIntPoint::ZeroValue;
			TestTrue(FString::Printf(TEXT("%s: its source is at most %d (%dx%d)"), *Asset.AssetName.ToString(), DroneTextureMax, Size.X, Size.Y),
				Size.X > 0 && Size.X <= DroneTextureMax && Size.Y <= DroneTextureMax);
		}
		for (const TCHAR* Forbidden : ForbiddenClasses)
		{
			if (Class == Forbidden)
			{
				AddError(FString::Printf(TEXT("%s is a %s: it stays in the staging"), *Asset.PackageName.ToString(), *Class));
			}
		}
		Systems += Class == TEXT("NiagaraSystem") ? 1 : 0;
		Meshes += Class == TEXT("StaticMesh") ? 1 : 0;
	}
	TestEqual(TEXT("The three absorption colours"), Systems, NiagaraSystems);
	TestEqual(TEXT("The chosen props, the effect's core mesh and the drone"), Meshes, PropMeshes + EffectMeshes + DroneMeshes);
	TestEqual(TEXT("The drone's four textures"), DroneTextureCount, DroneTextures);

	int64 Bytes = 0;
	IFileManager::Get().IterateDirectoryStatRecursively(*(FPaths::ProjectContentDir() / TEXT("Fab")), [&Bytes](const TCHAR*, const FFileStatData& Stat)
	{
		Bytes += Stat.bIsDirectory ? 0 : Stat.FileSize;
		return true;
	});
	const int64 Megabytes = Bytes / (1024 * 1024);
	TestTrue(*FString::Printf(TEXT("Content/Fab stays compact: %lld MB of %lld"), Megabytes, TargetMegabytes), Bytes > 0 && Megabytes <= TargetMegabytes);
	AddInfo(FString::Printf(TEXT("%d Fab assets, %lld MB"), Assets.Num(), Megabytes));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowFabDressingTest, "Hollow.Game.Fab.Dressing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowFabDressingTest::RunTest(const FString& Parameters)
{
	// FAB-01 (ADR-0051): the hub and six Docks rooms wear the Fab props: in the decor folders, looks only (no collision,
	// no navmesh), no light of their own (DA_LightBudget unchanged), every one a mesh of /Game/Fab. The openings, the
	// sockets and the light budget are Hollow.PCG.DocksOpenings, the layout's own checks and Hollow.Game.Light.RoomBudgets.
	using namespace HollowFabTests;
	int32 Total = 0;
	for (const FDressed& Dressed : Levels)
	{
		const UWorld* Level = LoadObject<UWorld>(nullptr, Dressed.Level);
		if (!TestNotNull(Dressed.Level, Level) || !Level->PersistentLevel)
		{
			continue;
		}
		int32 Props = 0;
		for (const AActor* Actor : Level->PersistentLevel->Actors)
		{
			if (!Actor || !Actor->Tags.Contains(PropTag))
			{
				continue;
			}
			++Props;
			const FString Name = FString::Printf(TEXT("%s %s"), *FPaths::GetBaseFilename(Dressed.Level), *Actor->GetActorNameOrLabel());
#if WITH_EDITOR   // Outliner folders exist in the editor only
			const FString Folder = Actor->GetFolderPath().ToString();
			TestTrue(*FString::Printf(TEXT("%s: in a decor folder (%s)"), *Name, *Folder), Folder.StartsWith(TEXT("Dressing/")) || Folder.StartsWith(TEXT("Fixtures/")));
#endif
			const AStaticMeshActor* Prop = Cast<AStaticMeshActor>(Actor);
			const UStaticMeshComponent* Mesh = Prop ? Prop->GetStaticMeshComponent() : nullptr;
			const UStaticMesh* Asset = Mesh ? Mesh->GetStaticMesh() : nullptr;
			if (!TestNotNull(*FString::Printf(TEXT("%s: a static mesh"), *Name), Asset))
			{
				continue;
			}
			TestTrue(*FString::Printf(TEXT("%s: a Fab mesh"), *Name), Asset->GetPathName().StartsWith(FString(FabRoot) + TEXT("/")));
			TestTrue(*FString::Printf(TEXT("%s: looks only"), *Name), Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Mesh->CanEverAffectNavigation());
			TestNull(*FString::Printf(TEXT("%s: no light of its own"), *Name), Actor->FindComponentByClass<ULightComponent>());
		}
		TestEqual(*FString::Printf(TEXT("%s: its props"), *FPaths::GetBaseFilename(Dressed.Level)), Props, Dressed.Props);
		Total += Props;
	}
	AddInfo(FString::Printf(TEXT("%d Fab props in %d levels"), Total, static_cast<int32>(UE_ARRAY_COUNT(Levels))));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
