#pragma once

#include "Engine/AssetManager.h"

#include "HollowAssetManager.generated.h"

/**
 * Project asset manager, selected via [/Script/Engine.Engine] AssetManagerClassName in DefaultEngine.ini.
 * Primary asset types are registered in DefaultGame.ini together with their Definition classes
 * (Docs/Architecture/02_Folder_Structure_and_Class_Hierarchy.md sec. 2.1).
 */
UCLASS()
class HOLLOWCORE_API UHollowAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	/** Returns the project asset manager. Asserts if the engine is configured with another class (configuration error). */
	static UHollowAssetManager& Get();
};
