#pragma once

#include "Engine/DataAsset.h"
#include "Generation/HollowSectorGenTypes.h"

#include "HollowBiomeDefinition.generated.h"

class AActor;
class UMaterialInterface;
class UStaticMesh;

/** Static geometry that fills an unused connector's opening (spawned locally on every machine, S2-02). */
USTRUCT(BlueprintType)
struct FHollowSealedPlug
{
	GENERATED_BODY()

	/** Scaled to the opening (UHollowLevelMetrics) from its own bounds, so any box-like mesh works. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plug")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plug")
	TSoftObjectPtr<UMaterialInterface> Material;
};

/** Which free sockets a rule limited by MaxPerSector takes. */
UENUM(BlueprintType)
enum class EHollowSocketPick : uint8
{
	/** The first ones in manifest order. */
	ManifestOrder,
	/** Those in rooms the most links away from the Spawn room (manifest order between equals). */
	FarthestFromSpawn
};

/** Gameplay actor the server spawns at the room sockets of a type (all of them, or MaxPerSector picked ones). */
USTRUCT(BlueprintType)
struct FHollowSocketActorRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assembly", meta = (Categories = "Hollow.Socket"))
	FGameplayTag SocketType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assembly")
	TSoftClassPtr<AActor> ActorClass;

	/** The actor's pivot relative to the socket (e.g. half the height of a physics body that rests on the floor socket). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assembly", meta = (Units = "Centimeters"))
	FVector SpawnOffset = FVector::ZeroVector;

	/** At most this many per sector (0: every socket of the type); the sockets it leaves go to the next rule of the type. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assembly", meta = (ClampMin = "0"))
	int32 MaxPerSector = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assembly", meta = (EditCondition = "MaxPerSector > 0"))
	EHollowSocketPick Pick = EHollowSocketPick::ManifestOrder;
};

/** A room socket of a sector, in manifest order, as UHollowBiomeDefinition::AssignSocketRules sees it. */
struct FHollowSocketSlot
{
	FGameplayTag SocketType;
	/** Links from the Spawn room to the socket's room (INDEX_NONE: no route or no Spawn room). */
	int32 RoomDistance = INDEX_NONE;
};

/** Gameplay actor the server spawns in every connection of a kind (door, bulkhead, open frame). */
USTRUCT(BlueprintType)
struct FHollowConnectionActorRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assembly")
	EHollowConnectorKind Kind = EHollowConnectorKind::Door;

	/** Used when Variants is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assembly")
	TSoftClassPtr<AActor> ActorClass;

	/** Optional variants (e.g. bulkhead shapes): each connection gets one, picked from the sector seed and its index. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assembly")
	TArray<TSoftClassPtr<AActor>> Variants;
};

/**
 * One biome (sector) of the game: generator rules now; enemy tables, light/audio presets and door/bulkhead classes
 * later (Docs/Architecture/02 sec. 2.6). Lives in the sector's Game Feature plugin; its room pool is every HollowRoom
 * of that plugin (ADR-0012), sorted by PrimaryAssetId by the generator.
 */
UCLASS(BlueprintType)
class HOLLOWPCG_API UHollowBiomeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation", meta = (ShowOnlyInnerProperties))
	FHollowSectorGenRules GenRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assembly")
	FHollowSealedPlug SealedPlug;

	/** Applied in order (a handful of entries); a socket type without a rule spawns nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assembly")
	TArray<FHollowSocketActorRule> SocketActors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assembly")
	TArray<FHollowConnectionActorRule> ConnectionActors;

	/**
	 * Index into SocketActors of the rule that spawns at each slot (INDEX_NONE: nothing). Rules apply in order; each
	 * takes the free slots of its type, or its MaxPerSector pick of them. Deterministic: arrays and stable sorts only.
	 */
	TArray<int32> AssignSocketRules(TConstArrayView<FHollowSocketSlot> Slots) const;
	/** Class for a connection of Kind; VariantKey (a hash of seed and connection index) picks among the rule's variants. */
	TSoftClassPtr<AActor> FindConnectionActor(EHollowConnectorKind Kind, uint64 VariantKey = 0) const;

	/** Every class a connection rule can spawn (for async preloading). */
	TArray<TSoftClassPtr<AActor>> GetConnectionActorClasses() const;
};
