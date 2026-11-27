#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimSequence.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "Rendering/SkeletalMeshRenderData.h"

namespace HollowFabTests
{
	// FAB-01 (ADR-0051): what Build/Art/fab_manifest.json keeps (prepare_fab_assets.py) and create_fab_dressing.py places.
	const TCHAR* const FabRoot = TEXT("/Game/Fab");
	const TCHAR* const ForbiddenClasses[] = {TEXT("SoundWave"), TEXT("SoundCue"), TEXT("SoundAttenuation"), TEXT("SoundClass"), TEXT("MetaSoundSource"),
		TEXT("World"), TEXT("Blueprint"), TEXT("WidgetBlueprint"), TEXT("LevelSequence"), TEXT("InputAction"), TEXT("InputMappingContext"),
		TEXT("UserDefinedStruct"), TEXT("UserDefinedEnum"), TEXT("DataTable"), TEXT("AnimBlueprint"), TEXT("ParticleSystem")};
	constexpr int32 NiagaraSystems = 3;    // Teal, Purple, Yellow
	constexpr int32 PropMeshes = 19;       // OvergrownFactoryENV's kept props
	constexpr int32 EffectMeshes = 1;      // the absorption's own core mesh
	// DRONE-01 (ADR-0054): AdvancedCompanionDroneSystem gave only the Legacy drone's static mesh, two materials, four textures.
	const TCHAR* const DronePack = TEXT("/Game/Fab/AdvancedCompanionDroneSystem/");
	constexpr int32 DroneMeshes = 1;
	constexpr int32 DroneTextures = 4;
	constexpr int32 DroneTextureMax = 1024;
	constexpr int64 TargetMegabytes = 450; // the ceiling for Content/Fab (CHAR-01: 300 + the three characters; budget_mb is GitHub's, 2000)
	// CHAR-01 (ADR-0055): three Paragon characters, each its own textures at most its cap, its material layers' at 512.
	constexpr int32 CharacterMeshes = 3;
	struct FCharacterPack
	{
		const TCHAR* Root;
		const TCHAR* Own;   // the character's own folder
		int32 OwnTextureMax;
	};
	const FCharacterPack CharacterPacks[] = {
		{TEXT("/Game/Fab/ParagonWraith/"), TEXT("/Game/Fab/ParagonWraith/Characters/Heroes/Wraith/"), 2048},
		{TEXT("/Game/Fab/ParagonRiktor/"), TEXT("/Game/Fab/ParagonRiktor/Characters/Heroes/Riktor/"), 1024},
		{TEXT("/Game/Fab/ParagonSevarog/"), TEXT("/Game/Fab/ParagonSevarog/Characters/Heroes/Sevarog/"), 1024},
	};
	constexpr int32 LayerTextureMax = 512;
	/** What the packs' characters held, hidden (fab_manifest.json hidden_slots): Wraith's rifle, Riktor's long hook chain. */
	struct FHiddenSlot
	{
		const TCHAR* Mesh;
		const TCHAR* Slot;
	};
	const FHiddenSlot HiddenSlots[] = {
		{TEXT("/Game/Fab/ParagonWraith/Characters/Heroes/Wraith/Meshes/Wraith.Wraith"), TEXT("M03_weapon")},
		{TEXT("/Game/Fab/ParagonRiktor/Characters/Heroes/Riktor/Meshes/Riktor.Riktor"), TEXT("M06_chain_r_long")},
		{TEXT("/Game/Fab/ParagonRiktor/Characters/Heroes/Riktor/Meshes/Riktor.Riktor"), TEXT("M12_long_chain_r_fx_wide")},
		{TEXT("/Game/Fab/ParagonRiktor/Characters/Heroes/Riktor/Meshes/Riktor.Riktor"), TEXT("M13_long_chain_r_fx_thin")},
	};
	const TCHAR* const HiddenMaterial = TEXT("M_Hollow_Hidden");
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
	int32 SkeletalMeshes = 0;
	int32 DroneTextureCount = 0;
	for (const FAssetData& Asset : Assets)
	{
		const FString Class = Asset.AssetClassPath.GetAssetName().ToString();
		const FString Package = Asset.PackageName.ToString();
		for (const FCharacterPack& Pack : CharacterPacks)
		{
			if (Package.StartsWith(Pack.Root) && Class == TEXT("Texture2D"))
			{
				const int32 Max = Package.StartsWith(Pack.Own) ? Pack.OwnTextureMax : LayerTextureMax;
				const UTexture2D* Texture = Cast<UTexture2D>(Asset.GetAsset());
				const FIntPoint Size = Texture ? Texture->GetImportedSize() : FIntPoint::ZeroValue;
				TestTrue(FString::Printf(TEXT("%s: its source is at most %d (%dx%d)"), *Package, Max, Size.X, Size.Y), Size.X > 0 && Size.X <= Max && Size.Y <= Max);
			}
		}
		if (Class == TEXT("AnimSequence"))
		{
			const UAnimSequence* Sequence = Cast<UAnimSequence>(Asset.GetAsset());
			TestTrue(Package + TEXT(": no root motion (the server moves the bodies)"), Sequence && !Sequence->bEnableRootMotion);
		}
		SkeletalMeshes += Class == TEXT("SkeletalMesh") ? 1 : 0;
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
	TestEqual(TEXT("The three characters' meshes (CHAR-01)"), SkeletalMeshes, CharacterMeshes);
	for (const FHiddenSlot& Hidden : HiddenSlots)
	{
		const USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, Hidden.Mesh);
		const FName SlotName(Hidden.Slot);
		const FSkeletalMaterial* Slot = Mesh ? Mesh->GetMaterials().FindByPredicate([&SlotName](const FSkeletalMaterial& Each) { return Each.MaterialSlotName == SlotName; }) : nullptr;
		TestTrue(FString::Printf(TEXT("%s %s: hidden (the empty material of its pack)"), Hidden.Mesh, Hidden.Slot), Slot && Slot->MaterialInterface
			&& Slot->MaterialInterface->GetName() == HiddenMaterial);
		// ...and not drawn at all: its sections are off in every LOD (create_character_assets.py; an empty material still
		// costs depth and shadow).
		const int32 SlotIndex = Mesh ? Mesh->GetMaterials().IndexOfByPredicate([&SlotName](const FSkeletalMaterial& Each) { return Each.MaterialSlotName == SlotName; }) : INDEX_NONE;
		const FSkeletalMeshRenderData* RenderData = Mesh ? Mesh->GetResourceForRendering() : nullptr;
		int32 Sections = 0;
		int32 Off = 0;
		for (int32 Lod = 0; RenderData && Lod < RenderData->LODRenderData.Num(); ++Lod)
		{
			const FSkeletalMeshLODInfo* Info = Mesh->GetLODInfo(Lod);
			const TArray<FSkelMeshRenderSection>& RenderSections = RenderData->LODRenderData[Lod].RenderSections;
			for (int32 Index = 0; Index < RenderSections.Num(); ++Index)
			{
				const int32 Material = Info && Info->LODMaterialMap.IsValidIndex(Index) && Info->LODMaterialMap[Index] != INDEX_NONE
					? Info->LODMaterialMap[Index] : RenderSections[Index].MaterialIndex;
				if (Material == SlotIndex)
				{
					++Sections;
					Off += RenderSections[Index].bDisabled ? 1 : 0;
				}
			}
		}
		TestTrue(FString::Printf(TEXT("%s %s: its %d sections are off (%d)"), Hidden.Mesh, Hidden.Slot, Sections, Off), Sections > 0 && Off == Sections);
	}

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
