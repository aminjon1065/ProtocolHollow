#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "HollowGeometryCollectionAuthoring.generated.h"

class UGeometryCollection;
class UMaterialInterface;
class UStaticMesh;

/**
 * Editor-only geometry collection authoring for the content scripts (ADR-0010, ADR-0014). Greybox bulkhead zones are
 * boxes split by a Voronoi fracture from a fixed seed, so every run gives the same pieces. Production fractures are
 * authored by an artist in the Fracture Editor or a Dataflow graph instead (Docs/Architecture/03 sec. 3.4).
 */
UCLASS()
class HOLLOWEDITOR_API UHollowGeometryCollectionAuthoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Creates the geometry collection asset AssetPath (a package path such as /Game/X/GC_Y; it must not exist yet): BoxMesh
	 * scaled to Size and centred on the origin, cut into Pieces Voronoi cells seeded with Seed. Clustering is off, so every
	 * piece simulates on its own; resting pieces are removed after SleepSeconds (min, max) over RemovalSeconds (min, max).
	 * The caller saves the asset. Returns null on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Destruction")
	static UGeometryCollection* CreateFracturedBox(const FString& AssetPath, UStaticMesh* BoxMesh, UMaterialInterface* Material, FVector Size,
		int32 Pieces, int32 Seed, FVector2D SleepSeconds, FVector2D RemovalSeconds);

	/** Rigid pieces (geometry entries) of a collection; 0 without one. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Destruction")
	static int32 GetPieceCount(const UGeometryCollection* Collection);

	/** Size of the collection's bounds in its own space. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Destruction")
	static FVector GetBoundsSize(const UGeometryCollection* Collection);
};
