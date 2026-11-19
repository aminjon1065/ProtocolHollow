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

/** Gameplay actor the server spawns at every room socket of a type. */
USTRUCT(BlueprintType)
struct FHollowSocketActorRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assembly", meta = (Categories = "Hollow.Socket"))
	FGameplayTag SocketType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assembly")
	TSoftClassPtr<AActor> ActorClass;
};

/** Gameplay actor the server spawns in every connection of a kind (door, bulkhead, open frame). */
USTRUCT(BlueprintType)
struct FHollowConnectionActorRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assembly")
	EHollowConnectorKind Kind = EHollowConnectorKind::Door;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assembly")
	TSoftClassPtr<AActor> ActorClass;
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

	/** Searched linearly (a handful of entries); a socket type without a rule spawns nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assembly")
	TArray<FHollowSocketActorRule> SocketActors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assembly")
	TArray<FHollowConnectionActorRule> ConnectionActors;

	TSoftClassPtr<AActor> FindSocketActor(const FGameplayTag& SocketType) const;
	TSoftClassPtr<AActor> FindConnectionActor(EHollowConnectorKind Kind) const;
};
