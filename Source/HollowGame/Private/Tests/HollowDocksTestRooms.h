#pragma once

// The Docks room definitions under test (create_docks_greybox.py), shared by the lighting and audio tests.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Data/HollowRoomDefinition.h"

namespace HollowDocksTestRooms
{
	const TCHAR* const DocksRoot = TEXT("/HollowSector_Docks");
	const TCHAR* const HeroRoom = TEXT("DA_Room_Docks_CoreVault");
	constexpr int32 DocksRooms = 8;

	/** Every Docks room definition, by package name. */
	inline TArray<const UHollowRoomDefinition*> DocksRoomDefinitions()
	{
		IAssetRegistry& Registry = IAssetRegistry::GetChecked();
		Registry.ScanPathsSynchronous({FString(DocksRoot)}, /*bForceRescan=*/false);
		FARFilter Filter;
		Filter.PackagePaths.Add(FName(DocksRoot));
		Filter.bRecursivePaths = true;
		Filter.ClassPaths.Add(UHollowRoomDefinition::StaticClass()->GetClassPathName());
		TArray<FAssetData> Assets;
		Registry.GetAssets(Filter, Assets);
		Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });
		TArray<const UHollowRoomDefinition*> Rooms;
		for (const FAssetData& Asset : Assets)
		{
			if (const UHollowRoomDefinition* Room = Cast<UHollowRoomDefinition>(Asset.GetAsset()))
			{
				Rooms.Add(Room);
			}
		}
		return Rooms;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
