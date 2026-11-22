#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"

#include "HollowAssetAuthoring.generated.h"

/**
 * Editor-only helpers for the content scripts (ADR-0010) where editor Python stops: assets whose factory asks for their
 * class in a dialog (Sound Modulation Parameters), components on placed actors (Audio Gameplay Volume mutators).
 */
UCLASS()
class HOLLOWEDITOR_API UHollowAssetAuthoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** A new, unsaved asset of a concrete Class at PackagePath/AssetName, or null. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Assets")
	static UObject* CreateObjectAsset(UClass* Class, const FString& PackagePath, const FString& AssetName);

	/**
	 * A component of Class added to a placed actor of the open level (saved with it), or the actor's existing one of that
	 * name: e.g. the reverb and filter mutators of an Audio Gameplay Volume, which editor Python cannot add itself. A scene
	 * component attaches to the actor's root, or becomes the root of an actor without one (the Cathedral's decor).
	 */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Assets")
	static UActorComponent* AddInstanceComponent(AActor* Actor, TSubclassOf<UActorComponent> Class, FName Name);
};
