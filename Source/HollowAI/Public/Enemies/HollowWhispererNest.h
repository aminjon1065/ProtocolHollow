#pragma once

#include "GameFramework/Actor.h"
#include "Interfaces/HollowSeededActor.h"

#include "HollowWhispererNest.generated.h"

class AHollowEnemyCharacter;
class UStaticMeshComponent;

/**
 * A Whisperer nest (GP-02, ADR-0047): the sector puts one on an EnemySpawn socket of a dark room with loot (a biome rule,
 * at least MinRoomDistance from Spawn). Its seed (the sector's and the socket's) decides its brood: MinBrood..MaxBrood
 * Whisperers, spawned by the server on the navmesh round it the tick after it begins play (the sector builds its
 * navmesh right after spawning its actors), one swarm group. No respawn: a brood killed stays dead. Every machine sees
 * the nest (one initial bunch, then dormant); nothing about it changes.
 */
UCLASS()
class HOLLOWAI_API AHollowWhispererNest : public AActor, public IHollowSeededActor
{
	GENERATED_BODY()

public:
	explicit AHollowWhispererNest(const FObjectInitializer& ObjectInitializer);

	/** Server, before FinishSpawning (the sector), or before BeginPlay (tests, dev). */
	virtual void SetSpawnSeed(int32 InSeed) override { Seed = InSeed; }
	int32 GetSeed() const { return Seed; }

	/** How many Whisperers its seed holds. */
	int32 GetBroodSize() const;
	/** Its brood (server), as spawned; the dead and gone are null. */
	const TArray<TWeakObjectPtr<AHollowEnemyCharacter>>& GetBrood() const { return Brood; }
	bool HasSpawned() const { return bSpawned; }

	/** Server: spawns its brood now if it has not yet (BeginPlay schedules it); how many came out. */
	int32 SpawnBrood();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Hollow|Nest")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Nest")
	TSubclassOf<AHollowEnemyCharacter> WhispererClass;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Nest", meta = (ClampMin = "0"))
	int32 MinBrood = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Nest", meta = (ClampMin = "0"))
	int32 MaxBrood = 0;

	/** The brood comes out on a ring this far round the nest. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Nest", meta = (ClampMin = "0", Units = "Centimeters"))
	float SpawnRadius = 0.f;

	/** How far from its spot on the ring a Whisperer may land on its navmesh. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Nest", meta = (ClampMin = "0", Units = "Centimeters"))
	float SpawnSearchExtent = 0.f;

	/** No navmesh yet (a sector still building): it tries again this often, up to MaxSpawnAttempts. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Nest", meta = (ClampMin = "0.05", Units = "Seconds"))
	float SpawnRetrySeconds = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Nest", meta = (ClampMin = "1"))
	int32 MaxSpawnAttempts = 1;

private:
	void TrySpawn();

	int32 Seed = 0;
	int32 Attempts = 0;
	bool bSpawned = false;
	TArray<TWeakObjectPtr<AHollowEnemyCharacter>> Brood;
	FTimerHandle SpawnTimer;
};
