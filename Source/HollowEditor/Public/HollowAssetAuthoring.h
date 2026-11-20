#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "HollowAssetAuthoring.generated.h"

/**
 * Editor-only asset creation for the content scripts (ADR-0010) where an asset type's factory asks for its class in a
 * dialog (Sound Modulation Parameters): the asset is made directly, unsaved, and registered.
 */
UCLASS()
class HOLLOWEDITOR_API UHollowAssetAuthoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** A new, unsaved asset of a concrete Class at PackagePath/AssetName, or null. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Assets")
	static UObject* CreateObjectAsset(UClass* Class, const FString& PackagePath, const FString& AssetName);
};
