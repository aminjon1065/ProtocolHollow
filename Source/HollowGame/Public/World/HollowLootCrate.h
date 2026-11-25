#pragma once

#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interfaces/HollowBreakableProp.h"
#include "Interfaces/HollowSeededActor.h"
#include "Interfaces/HollowToolTarget.h"
#include "Items/HollowItemTypes.h"

#include "HollowLootCrate.generated.h"

class AHollowLootPickup;
class UBoxComponent;
class UGeometryCollection;
class UGeometryCollectionComponent;
class UHollowLootTableDefinition;
class UStaticMeshComponent;
struct FStreamableHandle;

/** What every machine knows of a crate (GP-01b): replicated, push model; the crate is dormant until it breaks. */
USTRUCT()
struct FHollowCrateNetState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bBroken = false;

	/** Where the blow that broke it went (its debris fly along). */
	UPROPERTY()
	FVector_NetQuantizeNormal BreakDirection = FVector::ZeroVector;

	/** Server time of the break: a late or far machine shows it already broken (no fracture). */
	UPROPERTY()
	float BreakServerTime = 0.f;
};

/**
 * A loot crate at a room's Loot socket (GP-01b, ADR-0045): shots, blows and the plasma cutter break it open, and the
 * loot it holds (rolled once from the sector's seed and its socket: UHollowLootTableDefinition) spills out as pickups.
 * Its integrity is the server's (a number, like a bulkhead zone's, CLAUDE.md Q4); every machine follows the replicated
 * break and shows it its own way: a live Chaos fracture of cosmetic debris (HollowDebris: never a pawn, a trace or
 * navigation) when it is fresh, near, seen and within the destruction budget, else it is simply gone. Intact, it is a
 * dynamic navigation obstacle; broken, nothing. Never ticks.
 */
UCLASS()
class HOLLOWGAME_API AHollowLootCrate : public AActor, public IHollowBreakableProp, public IHollowToolTarget, public IHollowSeededActor
{
	GENERATED_BODY()

public:
	explicit AHollowLootCrate(const FObjectInitializer& ObjectInitializer);

	bool IsBroken() const { return NetState.bBroken; }
	float GetHealth() const { return Health; }
	int32 GetSeed() const { return Seed; }

	/** What it holds (server; rolled from its seed on its first hit or break). */
	TArray<FHollowItemStack> GetContents() const;

	/** Pickups it spilled (server; tests). */
	const TArray<TWeakObjectPtr<AHollowLootPickup>>& GetSpilled() const { return Spilled; }

	//~ IHollowBreakableProp
	virtual bool ApplyPropHit(const FHollowStructuralHit& Hit) override;

	//~ IHollowToolTarget: the plasma cutter cuts it open.
	virtual bool CanAcceptToolWork(const FHollowToolWork& Work) const override;
	virtual void ApplyToolWork(const FHollowToolWork& Work) override;

	//~ IHollowSeededActor
	virtual void SetSpawnSeed(int32 InSeed) override;

	/** Server: breaks it now along Direction (tests, dev). */
	void Break(const FVector& Direction, AActor* Instigator);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The intact crate's collision (its extent is data): blocks pawns and shots, a dynamic navigation obstacle. */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Loot")
	TObjectPtr<UBoxComponent> Bounds;

	/** Its look only (no collision). */
	UPROPERTY(VisibleAnywhere, Category = "Hollow|Loot")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Loot", meta = (ClampMin = "1"))
	float MaxHealth = 1.f;

	/** Damage per unit of the plasma cutter's work. */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Loot", meta = (ClampMin = "0"))
	float CutDamagePerWork = 0.f;

	/** The tool that cuts it (Hollow.Ability.Tool.*). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Loot", meta = (Categories = "Hollow.Ability.Tool"))
	FGameplayTag CutterTag;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Loot")
	TObjectPtr<const UHollowLootTableDefinition> LootTable;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Loot")
	TSubclassOf<AHollowLootPickup> PickupClass;

	/** How fast the loot spills out (cm/s, up and away from the blow). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Loot", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float SpillSpeed = 0.f;

	/** The break as cosmetic debris (Chaos, local to every machine). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Loot")
	TSoftObjectPtr<UGeometryCollection> Fracture;

	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Loot", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float DebrisSpeed = 0.f;

	/** A break is shown live only this soon after it happened (later or farther: it is just gone). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Loot", meta = (ClampMin = "0", Units = "Seconds"))
	float FreshWindowSeconds = 0.f;

	/** Hollow.Noise.* of the break at the crate (its rule in DA_NoiseConfig; its sound in DA_SoundBank). */
	UPROPERTY(EditDefaultsOnly, Category = "Hollow|Loot", meta = (Categories = "Hollow.Noise"))
	FGameplayTag BreakNoiseTag;

private:
	UFUNCTION()
	void OnRep_NetState();

	/** The break on this machine: the body gone, debris or nothing, the sound (a fresh break only). */
	void PresentBreak();
	void EndFracture();
	void SpillLoot(const FVector& Direction);
	void EnsureContents() const;
	double GetServerTime() const;

	UPROPERTY(ReplicatedUsing = OnRep_NetState)
	FHollowCrateNetState NetState;

	/** Server: integrity left. */
	float Health = 0.f;
	int32 Seed = 0;
	mutable bool bRolled = false;
	mutable TArray<FHollowItemStack> Contents;
	TArray<TWeakObjectPtr<AHollowLootPickup>> Spilled;
	bool bPresented = false;

	UPROPERTY(Transient)
	TObjectPtr<UGeometryCollectionComponent> LiveFracture;

	/** The fracture, loaded while the crate stands where someone may watch it break. */
	TSharedPtr<FStreamableHandle> FractureLoad;

	int32 LiveHandle = INDEX_NONE;
};
