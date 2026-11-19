#pragma once

#include "Components/ActorComponent.h"
#include "UObject/PrimaryAssetId.h"

#include "HollowSectorAuthorityComponent.generated.h"

class AController;
class APlayerController;
class UHollowBiomeDefinition;
class UHollowRoomDefinition;
class UHollowSectorStateComponent;
struct FStreamableHandle;

/**
 * Server-only sector lifecycle of a raid (Docs/Architecture/02 sec. 2.2/2.9): seed -> generate -> manifest on the
 * GameState; once the server has assembled it, spawns the gameplay actors at sockets and connections and builds the
 * navmesh; collects every player's Server_ReportSectorReady (hash must match, late players are kicked after a
 * timeout); then marks the sector ready and gives players their pawns.
 */
UCLASS(ClassGroup = (Hollow))
class HOLLOWGAME_API UHollowSectorAuthorityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit UHollowSectorAuthorityComponent(const FObjectInitializer& ObjectInitializer);

	/** The one non-deterministic input of a raid: a seed when the host did not pick one (ADR-0003). */
	static int32 MakeRandomSeed();

	/** Replaces the current sector with a new one (also Hollow.Sector.Rebuild). */
	void StartBuild(int32 Seed);

	void HandleReport(APlayerController* Player, int32 BuildId, uint32 LayoutHash);
	void HandlePlayerLeft(AController* Exiting);

	bool IsSectorReady() const { return bSectorReady; }
	FString Describe() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void LoadContent();
	void HandleDefinitionsLoaded();
	void HandleClassesLoaded();
	void Generate();
	void HandleServerAssembled(int32 BuildId, uint32 LocalHash);
	void SpawnSectorActors();
	void BuildNavigation();
	void TryFinish();
	void HandleReadyTimeout();
	void ClearSector();
	UHollowSectorStateComponent* GetSectorState() const;

	UPROPERTY(Transient)
	TObjectPtr<UHollowBiomeDefinition> Biome;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHollowRoomDefinition>> Rooms;

	FPrimaryAssetId BiomeId;
	TArray<FPrimaryAssetId> RoomIds;
	TSharedPtr<FStreamableHandle> ContentHandle;

	TArray<TWeakObjectPtr<AActor>> SpawnedActors;
	TArray<TWeakObjectPtr<APlayerController>> Reported;
	FTimerHandle ReadyTimer;
	FDelegateHandle AssembledHandle;

	int32 PendingSeed = 0;
	int32 BuildCounter = 0;
	bool bContentLoaded = false;
	bool bServerAssembled = false;
	bool bNavigationReady = false;
	bool bSectorReady = false;
};
