#include "Destruction/HollowGeometryCollectionAuthoring.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Breach/HollowBreachableBulkhead.h"
#include "Dataflow/DataflowSelection.h"
#include "Engine/StaticMesh.h"
#include "FractureEngineFracturing.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "HollowEditorLog.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "Misc/PackageName.h"
#include "PlanarCut.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowGeometryCollectionAuthoring)

namespace HollowGeometryCollectionAuthoringPrivate
{
	// Plain Voronoi cells: no noise displacement on the cut surfaces and no grout. PlanarCut still remeshes every cut
	// surface at the noise point spacing (even without amplitude), so the spacing is the box's largest edge: flat cuts.
	constexpr float ChanceToFracture = 1.f;
	constexpr float Grout = 0.f;
	constexpr float NoiseAmplitude = 0.f;
	constexpr float NoiseFrequency = 0.f;
	constexpr float NoisePersistence = 0.f;
	constexpr float NoiseLacunarity = 0.f;
	constexpr int32 NoiseOctaves = 0;
	constexpr bool bAddCollisionSamples = false;
	constexpr float CollisionSampleSpacing = 0.f;
	constexpr bool bAddInternalMaterials = true;
}

UGeometryCollection* UHollowGeometryCollectionAuthoring::CreateFracturedBox(const FString& AssetPath, UStaticMesh* BoxMesh, UMaterialInterface* Material,
	const FVector Size, const int32 Pieces, const int32 Seed, const FVector2D SleepSeconds, const FVector2D RemovalSeconds)
{
	using namespace HollowGeometryCollectionAuthoringPrivate;
	FText Reason;
	if (!BoxMesh || !Material || Size.GetMin() <= 0.0 || Pieces < 2 || !FPackageName::IsValidLongPackageName(AssetPath, /*bIncludeReadOnlyRoots=*/false, &Reason))
	{
		UE_LOG(LogHollowEditor, Error, TEXT("CreateFracturedBox %s: needs a mesh, a material, a positive size, 2+ pieces and a package path. %s"), *AssetPath, *Reason.ToString());
		return nullptr;
	}
	if (FPackageName::DoesPackageExist(AssetPath))
	{
		UE_LOG(LogHollowEditor, Error, TEXT("CreateFracturedBox %s: the asset exists already."), *AssetPath);
		return nullptr;
	}

	UPackage* Package = CreatePackage(*AssetPath);
	UGeometryCollection* Collection = NewObject<UGeometryCollection>(Package, *FPackageName::GetLongPackageAssetName(AssetPath), RF_Public | RF_Standalone | RF_Transactional);

	// The box, scaled to Size and centred on the origin (its own bounds give the scale).
	const FBoxSphereBounds Bounds = BoxMesh->GetBounds();
	const FVector HalfSize = Size * 0.5;
	const FVector Scale = HalfSize / Bounds.BoxExtent.ComponentMax(FVector(UE_KINDA_SMALL_NUMBER));
	const FTransform MeshTransform(FQuat::Identity, -Bounds.Origin * Scale, Scale);
	if (!FGeometryCollectionEngineConversion::AppendStaticMesh(BoxMesh, TArray<UMaterialInterface*>{Material}, MeshTransform, Collection,
		/*bReindexMaterials=*/true, bAddInternalMaterials))
	{
		UE_LOG(LogHollowEditor, Error, TEXT("CreateFracturedBox %s: %s could not be appended."), *AssetPath, *BoxMesh->GetName());
		return nullptr;
	}
	Collection->InitializeMaterials(bAddInternalMaterials);

	// Voronoi sites from the seed, inside the box: the same pieces on every run (FRandomStream is platform independent).
	FGeometryCollection& Geometry = *Collection->GetGeometryCollection();
	FRandomStream Random(Seed);
	TArray<FVector> Sites;
	for (int32 Index = 0; Index < Pieces; ++Index)
	{
		Sites.Add(FVector(Random.FRandRange(-HalfSize.X, HalfSize.X), Random.FRandRange(-HalfSize.Y, HalfSize.Y), Random.FRandRange(-HalfSize.Z, HalfSize.Z)));
	}
	FDataflowTransformSelection Selection;
	Selection.Initialize(Geometry.NumElements(FGeometryCollection::TransformGroup), false);
	Selection.SetSelected(0);   // the single box bone
	const int32 FirstNew = FFractureEngineFracturing::VoronoiFracture(Geometry, Selection, Sites, FTransform::Identity, Seed, ChanceToFracture,
		FIslandSplitSettings(), Grout, NoiseAmplitude, NoiseFrequency, NoisePersistence, NoiseLacunarity, NoiseOctaves, static_cast<float>(Size.GetMax()),
		bAddCollisionSamples, CollisionSampleSpacing);
	if (FirstNew == INDEX_NONE)
	{
		UE_LOG(LogHollowEditor, Error, TEXT("CreateFracturedBox %s: the Voronoi fracture produced nothing."), *AssetPath);
		return nullptr;
	}

	// A burst, not a cluster that has to break: every piece is a body from the start; resting debris shrinks away.
	Collection->EnableClustering = false;
	Collection->bRemoveOnMaxSleep = true;
	Collection->MaximumSleepTime = SleepSeconds;
	Collection->RemovalDuration = RemovalSeconds;
	Collection->InvalidateCollection();
	Collection->RebuildRenderData();
	::GeometryCollection::GenerateTemporaryGuids(&Geometry, 0, /*bForceInit=*/true);
	Collection->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Collection);
	UE_LOG(LogHollowEditor, Log, TEXT("CreateFracturedBox %s: %d pieces"), *AssetPath, GetPieceCount(Collection));
	return Collection;
}

int32 UHollowGeometryCollectionAuthoring::GetPieceCount(const UGeometryCollection* Collection)
{
	return AHollowBreachableBulkhead::CountFractureBodies(Collection);
}

FVector UHollowGeometryCollectionAuthoring::GetBoundsSize(const UGeometryCollection* Collection)
{
	const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Geometry = Collection ? Collection->GetGeometryCollection() : nullptr;
	if (!Geometry.IsValid() || !Geometry->HasAttribute(FGeometryCollection::BoundingBoxAttribute, FGeometryCollection::GeometryGroup))
	{
		return FVector::ZeroVector;
	}
	// Leaf pieces sit in the box's space (one level below the box bone, identity transforms after the cut).
	FBox Box(ForceInit);
	for (const FBox& Piece : Geometry->BoundingBox)
	{
		Box += Piece;
	}
	return Box.GetSize();
}
