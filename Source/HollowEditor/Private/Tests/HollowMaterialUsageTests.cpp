#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/IAssetRegistry.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectHash.h"

namespace HollowMaterialUsageTests
{
	// AK-14 (POLISH-01, ADR-0053): a cooked game draws a material without the usage flag of what it is on with the engine's
	// default material (the editor sets the flag on the fly and never saves it), so the masters carry their flags
	// (create_lighting_assets.py, create_art_kit.py MASTER_USAGE).
	struct FMasterUsage
	{
		const TCHAR* Path;
		EMaterialUsage Usage;
	};
	const FMasterUsage Masters[] = {
		{TEXT("/Game/Hollow/World/Materials/M_Hollow_Surface.M_Hollow_Surface"), MATUSAGE_InstancedStaticMeshes},
		{TEXT("/Game/Hollow/World/Materials/M_Hollow_Emissive.M_Hollow_Emissive"), MATUSAGE_InstancedStaticMeshes},
		{TEXT("/Game/Hollow/Art/Materials/M_Kit_Surface.M_Kit_Surface"), MATUSAGE_GeometryCollections},
	};
	/** The Cathedral's arcs: their emitters and beams are instanced static meshes (S7-02, ADR-0039). */
	const TCHAR* const ArcClass = TEXT("/HollowSector_Cathedral/Mechanics/BP_ArcHazard.BP_ArcHazard_C");
	/** Where the game's geometry collections live (loot crates, breachable bulkheads). */
	const TCHAR* const CollectionPaths[] = {TEXT("/Game/Hollow"), TEXT("/HollowSector_Docks"), TEXT("/HollowSector_Cathedral")};

	const TCHAR* UsageName(const EMaterialUsage Usage)
	{
		return Usage == MATUSAGE_InstancedStaticMeshes ? TEXT("InstancedStaticMeshes") : TEXT("GeometryCollections");
	}

	/** The master a material comes from, unless it is the engine's own (always flagged as needed). */
	const UMaterial* ProjectMaster(const UMaterialInterface* Material)
	{
		const UMaterial* Master = Material ? Material->GetMaterial() : nullptr;
		return Master && !Master->GetPathName().StartsWith(TEXT("/Engine/")) ? Master : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowMaterialUsageTest, "Hollow.Editor.MaterialUsage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHollowMaterialUsageTest::RunTest(const FString& Parameters)
{
	using namespace HollowMaterialUsageTests;
	for (const FMasterUsage& Expected : Masters)
	{
		const UMaterial* Master = LoadObject<UMaterial>(nullptr, Expected.Path);
		if (TestNotNull(Expected.Path, Master))
		{
			TestTrue(*FString::Printf(TEXT("%s is flagged for %s"), *Master->GetName(), UsageName(Expected.Usage)), Master->GetUsageByFlag(Expected.Usage));
		}
	}

	// The arcs as placed: every instanced mesh of BP_ArcHazard draws its own material.
	const UClass* Arc = LoadObject<UClass>(nullptr, ArcClass);
	if (TestNotNull(TEXT("BP_ArcHazard"), Arc))
	{
		TArray<UObject*> Templates;
		GetObjectsWithOuter(Arc->GetDefaultObject(), Templates, EGetObjectsFlags::None);
		int32 Instanced = 0;
		for (const UObject* Template : Templates)
		{
			const UInstancedStaticMeshComponent* Component = Cast<UInstancedStaticMeshComponent>(Template);
			if (!Component)
			{
				continue;
			}
			++Instanced;
			for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
			{
				const UMaterialInterface* Material = Component->GetMaterial(Slot);
				if (const UMaterial* Master = ProjectMaster(Material))
				{
					TestTrue(*FString::Printf(TEXT("BP_ArcHazard %s: %s draws on instanced meshes (%s)"), *Component->GetName(), *GetNameSafe(Material),
						*Master->GetName()), Master->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes));
				}
			}
		}
		TestEqual(TEXT("BP_ArcHazard: its emitters and its beams"), Instanced, 2);
	}

	// Every geometry collection of the game: its pieces draw their own materials.
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	FARFilter Filter;
	for (const TCHAR* Path : CollectionPaths)
	{
		Filter.PackagePaths.Add(FName(Path));
	}
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UGeometryCollection::StaticClass()->GetClassPathName());
	TArray<FString> Scanned;
	for (const TCHAR* Path : CollectionPaths)
	{
		Scanned.Add(Path);
	}
	Registry.ScanPathsSynchronous(Scanned, /*bForceRescan=*/false);
	TArray<FAssetData> Collections;
	Registry.GetAssets(Filter, Collections);
	TestTrue(TEXT("The game's geometry collections are found"), Collections.Num() > 0);
	for (const FAssetData& Asset : Collections)
	{
		const UGeometryCollection* Collection = Cast<UGeometryCollection>(Asset.GetAsset());
		if (!TestNotNull(*Asset.AssetName.ToString(), Collection))
		{
			continue;
		}
		for (const UMaterialInterface* Material : Collection->Materials)
		{
			if (const UMaterial* Master = ProjectMaster(Material))
			{
				TestTrue(*FString::Printf(TEXT("%s: %s draws on geometry collections (%s)"), *Collection->GetName(), *GetNameSafe(Material), *Master->GetName()),
					Master->GetUsageByFlag(MATUSAGE_GeometryCollections));
			}
		}
	}
	return true;
}

#endif
