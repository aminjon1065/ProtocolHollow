#include "Assembly/HollowSectorAssemblySubsystem.h"

#include "Algo/Count.h"
#include "Components/StaticMeshComponent.h"
#include "Data/HollowBiomeDefinition.h"
#include "Data/HollowLevelMetrics.h"
#include "Data/HollowRoomDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Generation/HollowSectorGenerator.h"
#include "HollowPCGLog.h"
#include "Materials/MaterialInterface.h"

#include "Util/HollowEnumUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowSectorAssemblySubsystem)

bool UHollowSectorAssemblySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHollowSectorAssemblySubsystem::Deinitialize()
{
	Teardown();
	Super::Deinitialize();
}

int32 UHollowSectorAssemblySubsystem::NumVisibleRooms() const
{
	return static_cast<int32>(Algo::CountIf(RoomLevels, [](const ULevelStreamingDynamic* Level) { return Level && Level->IsLevelVisible(); }));
}

void UHollowSectorAssemblySubsystem::Assemble(const FHollowSectorManifest& NewManifest)
{
	Teardown();
	Manifest = NewManifest;

	FString Error;
	if (!HollowSectorManifest::Reconstruct(Manifest, Layout, Error))
	{
		Fail(Error);
		return;
	}
	LocalHash = FHollowSectorGenerator::ComputeLayoutHash(Layout);

	UWorld* World = GetWorld();
	const UHollowLevelMetrics* Metrics = GetDefault<UHollowLevelMetrics>();
	for (int32 Index = 0; Index < Manifest.Rooms.Num(); ++Index)
	{
		const UHollowRoomDefinition* Room = Manifest.Rooms[Index].Room;
		const FTransform Transform = HollowSectorManifest::RoomTransform(Layout.Rooms[Index], FIntPoint(Room->Footprint.X, Room->Footprint.Y), *Metrics, GetSectorOrigin());
		// Unique per build, identical on every machine: "Sector<BuildId>_<Room>".
		const FString InstanceName = FString::Printf(TEXT("Sector%d_%d"), Manifest.BuildId, Index);
		bool bLoaded = false;
		ULevelStreamingDynamic* Level = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(World, Room->RoomLevel, Transform, bLoaded, InstanceName);
		if (!bLoaded || !Level)
		{
			Fail(FString::Printf(TEXT("room %d (%s) could not be streamed"), Index, *Room->RoomLevel.ToString()));
			return;
		}
		Level->OnLevelShown.AddDynamic(this, &ThisClass::HandleRoomShown);
		RoomLevels.Add(Level);
	}

	const FHollowSealedPlug& Plug = Manifest.Biome->SealedPlug;
	if (Plug.Mesh.IsNull() || Layout.Sealed.IsEmpty())
	{
		UE_CLOG(!Layout.Sealed.IsEmpty(), LogHollowPCG, Warning, TEXT("Biome %s has no SealedPlug mesh: %d openings stay open."),
			*GetNameSafe(Manifest.Biome), Layout.Sealed.Num());
		bPlugsReady = true;
	}
	else
	{
		TArray<FSoftObjectPath> Paths = {Plug.Mesh.ToSoftObjectPath()};
		if (!Plug.Material.IsNull())
		{
			Paths.Add(Plug.Material.ToSoftObjectPath());
		}
		PlugAssets = UAssetManager::GetStreamableManager().RequestAsyncLoad(MoveTemp(Paths),
			FStreamableDelegate::CreateUObject(this, &ThisClass::HandlePlugAssetsLoaded, Manifest.BuildId));
	}
	UE_LOG(LogHollowPCG, Log, TEXT("Sector build %d: streaming %d rooms, %d plugs, local hash %08X (server %08X)"), Manifest.BuildId,
		RoomLevels.Num(), Layout.Sealed.Num(), LocalHash, Manifest.LayoutHash);
	TryComplete();
}

void UHollowSectorAssemblySubsystem::Teardown()
{
	for (ULevelStreamingDynamic* Level : RoomLevels)
	{
		if (Level)
		{
			Level->OnLevelShown.RemoveAll(this);
			Level->SetIsRequestingUnloadAndRemoval(true);
		}
	}
	RoomLevels.Reset();
	for (const TWeakObjectPtr<AActor>& Plug : Plugs)
	{
		if (Plug.IsValid())
		{
			Plug->Destroy();
		}
	}
	Plugs.Reset();
	if (PlugAssets.IsValid())
	{
		PlugAssets->CancelHandle();
		PlugAssets.Reset();
	}
	Manifest = FHollowSectorManifest();
	Layout = FHollowSectorLayout();
	LocalHash = 0;
	bPlugsReady = false;
	bAssembled = false;
}

void UHollowSectorAssemblySubsystem::HandleRoomShown()
{
	TryComplete();
}

void UHollowSectorAssemblySubsystem::HandlePlugAssetsLoaded(const int32 ForBuildId)
{
	if (ForBuildId != Manifest.BuildId)
	{
		return;   // a newer build replaced the one that asked
	}
	SpawnPlugs();
	bPlugsReady = true;
	TryComplete();
}

void UHollowSectorAssemblySubsystem::SpawnPlugs()
{
	const FHollowSealedPlug& Plug = Manifest.Biome->SealedPlug;
	UStaticMesh* Mesh = Plug.Mesh.Get();
	const FVector MeshSize = Mesh ? Mesh->GetBoundingBox().GetSize() : FVector::ZeroVector;
	if (!Mesh || MeshSize.GetMin() <= UE_KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogHollowPCG, Error, TEXT("Sealed plug mesh %s is missing or flat."), *Plug.Mesh.ToString());
		return;
	}

	const UHollowLevelMetrics* Metrics = GetDefault<UHollowLevelMetrics>();
	const FVector Scale = HollowSectorManifest::PlugSize(*Metrics) / MeshSize;
	UWorld* World = GetWorld();
	for (const FHollowSealedConnector& Sealed : Layout.Sealed)
	{
		FTransform Transform = HollowSectorManifest::PlugTransform(Sealed.Cell, Sealed.Side, *Metrics, GetSectorOrigin());
		Transform.SetScale3D(Scale);
		// Local geometry on every machine (never replicated): the opening is closed for players, AI and the navmesh.
		AStaticMeshActor* Actor = World->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
		if (!Actor)
		{
			continue;
		}
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		// Movable, not Static: the component is already registered, and once the world has begun play (every raid) a
		// Static one refuses SetStaticMesh, which left every plug without mesh and collision (fixed in S3-02).
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(Mesh);
		if (UMaterialInterface* Material = Plug.Material.Get())
		{
			Component->SetMaterial(0, Material);
		}
		Actor->FinishSpawning(Transform);
		Plugs.Add(Actor);
		UE_CLOG(Component->GetStaticMesh() != Mesh, LogHollowPCG, Error, TEXT("Sector build %d: plug %s did not take its mesh."), Manifest.BuildId, *Actor->GetName());
		UE_LOG(LogHollowPCG, Verbose, TEXT("Sector build %d: plug for room %d connector %d (cell %s, %s) at %s"), Manifest.BuildId, Sealed.Room,
			Sealed.Connector, *Sealed.Cell.ToString(), *HollowEnumToString(Sealed.Side), *Transform.GetLocation().ToCompactString());
	}
}

void UHollowSectorAssemblySubsystem::TryComplete()
{
	if (bAssembled || !bPlugsReady || RoomLevels.IsEmpty() || NumVisibleRooms() != RoomLevels.Num())
	{
		return;
	}
	bAssembled = true;
	UE_LOG(LogHollowPCG, Log, TEXT("Sector build %d assembled: %d rooms visible, %d plugs, local hash %08X"), Manifest.BuildId,
		RoomLevels.Num(), Plugs.Num(), LocalHash);
	OnAssembled.Broadcast(Manifest.BuildId, LocalHash);
}

void UHollowSectorAssemblySubsystem::Fail(const FString& Why)
{
	UE_LOG(LogHollowPCG, Error, TEXT("Sector build %d cannot be assembled here: %s"), Manifest.BuildId, *Why);
	const int32 FailedBuild = Manifest.BuildId;
	LocalHash = 0;
	bAssembled = true;
	OnAssembled.Broadcast(FailedBuild, 0);   // the server rejects hash 0, so a broken client never joins the raid silently
}
