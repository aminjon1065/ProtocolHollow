#pragma once

#include "Engine/DeveloperSettings.h"

#include "HollowArtBudget.generated.h"

class ULevel;
class UTexture;

/** Art-pass budget (ADR-0027; Project Settings > Editor > Hollow Art Budget, DefaultEditor.ini). */
UCLASS(Config = Editor, DefaultConfig, meta = (DisplayName = "Hollow Art Budget"))
class HOLLOWEDITOR_API UHollowArtBudgetSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Largest side a texture may cook at (its source, limited by its MaxTextureSize and LODBias). */
	UPROPERTY(Config, EditAnywhere, Category = "Textures", meta = (ClampMin = "1"))
	int32 MaxTextureSize = 0;

	/** Content checked by the budget (package paths). */
	UPROPERTY(Config, EditAnywhere, Category = "Content")
	TArray<FString> ScannedPaths;

	/**
	 * World Outliner folders of a room level whose meshes are decoration: no collision, no navigation. Structure (walls,
	 * floors, anything one stands on or bumps into) goes elsewhere, e.g. Geometry. Subfolders count.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Rooms")
	TArray<FString> DecorFolders;
};

namespace HollowArtBudget
{
	/** The largest side Texture cooks at: its source's, limited by MaxTextureSize and lowered by LODBias. 0 without a source. */
	HOLLOWEDITOR_API int32 CookedMaxSize(const UTexture& Texture);

	/** What breaks the texture budget (empty when within). */
	HOLLOWEDITOR_API TArray<FString> CheckTexture(const UTexture& Texture, int32 MaxSize);

	/** Whether an Outliner folder is (inside) one of DecorFolders. */
	HOLLOWEDITOR_API bool IsDecorFolder(const FString& Folder, TConstArrayView<FString> DecorFolders);

	/** Decoration of a room level that collides or can affect navigation (empty when none). */
	HOLLOWEDITOR_API TArray<FString> CheckDecor(const ULevel& Level, TConstArrayView<FString> DecorFolders);

	/** Whether a package lies under one of the scanned paths. */
	HOLLOWEDITOR_API bool IsScanned(const FString& PackageName, TConstArrayView<FString> ScannedPaths);
}
