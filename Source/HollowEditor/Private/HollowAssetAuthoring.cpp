#include "HollowAssetAuthoring.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HollowEditorLog.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowAssetAuthoring)

UObject* UHollowAssetAuthoring::CreateObjectAsset(UClass* Class, const FString& PackagePath, const FString& AssetName)
{
	if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
	{
		UE_LOG(LogHollowEditor, Error, TEXT("Cannot create %s/%s of %s."), *PackagePath, *AssetName, *GetNameSafe(Class));
		return nullptr;
	}
	UPackage* Package = CreatePackage(*FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName));
	UObject* Asset = Package ? NewObject<UObject>(Package, Class, *AssetName, RF_Public | RF_Standalone | RF_Transactional) : nullptr;
	if (!Asset)
	{
		UE_LOG(LogHollowEditor, Error, TEXT("Could not create %s/%s."), *PackagePath, *AssetName);
		return nullptr;
	}
	FAssetRegistryModule::AssetCreated(Asset);
	Asset->MarkPackageDirty();
	return Asset;
}
