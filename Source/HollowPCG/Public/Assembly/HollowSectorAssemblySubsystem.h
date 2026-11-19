#pragma once

#include "Assembly/HollowSectorManifest.h"
#include "Subsystems/WorldSubsystem.h"

#include "HollowSectorAssemblySubsystem.generated.h"

class AActor;
class ULevelStreamingDynamic;
struct FStreamableHandle;

/**
 * Local assembly of a sector from its manifest, the same on every machine (ADR-0003): rebuilds the layout from the
 * local room definitions, streams every room as a Level Instance and plugs unused connectors with static geometry.
 * Gameplay actors are not its business: the server spawns them once OnAssembled fires (HollowGame). No Tick.
 */
UCLASS()
class HOLLOWPCG_API UHollowSectorAssemblySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** BuildId and the hash of the layout rebuilt here (0 if the manifest could not be rebuilt or streamed). */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHollowSectorAssembled, int32 /*BuildId*/, uint32 /*LocalLayoutHash*/);

	/** Sectors are assembled at the world origin (S2-02): the layout's cell (0,0) corner on the floor. */
	static FVector GetSectorOrigin() { return FVector::ZeroVector; }

	/** Replaces any previous sector. */
	void Assemble(const FHollowSectorManifest& Manifest);

	/** Unloads the rooms and removes the plugs. */
	void Teardown();

	bool IsAssembled() const { return bAssembled; }
	int32 GetBuildId() const { return Manifest.BuildId; }
	uint32 GetLocalHash() const { return LocalHash; }
	const FHollowSectorManifest& GetManifest() const { return Manifest; }
	/** Valid once Assemble has rebuilt the layout. */
	const FHollowSectorLayout& GetLayout() const { return Layout; }
	int32 NumRoomInstances() const { return RoomLevels.Num(); }
	int32 NumVisibleRooms() const;
	int32 NumPlugs() const { return Plugs.Num(); }

	FOnHollowSectorAssembled OnAssembled;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	UFUNCTION()
	void HandleRoomShown();

	void HandlePlugAssetsLoaded(int32 ForBuildId);
	void SpawnPlugs();
	void TryComplete();
	void Fail(const FString& Why);

	FHollowSectorManifest Manifest;
	FHollowSectorLayout Layout;
	uint32 LocalHash = 0;
	bool bPlugsReady = false;
	bool bAssembled = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ULevelStreamingDynamic>> RoomLevels;

	TArray<TWeakObjectPtr<AActor>> Plugs;
	TSharedPtr<FStreamableHandle> PlugAssets;
};
