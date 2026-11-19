#include "Data/HollowAssetManager.h"

#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowAssetManager)

UHollowAssetManager& UHollowAssetManager::Get()
{
	check(GEngine);

	UHollowAssetManager* AssetManager = Cast<UHollowAssetManager>(GEngine->AssetManager);
	checkf(AssetManager, TEXT("AssetManagerClassName in DefaultEngine.ini must be /Script/HollowCore.HollowAssetManager"));
	return *AssetManager;
}
