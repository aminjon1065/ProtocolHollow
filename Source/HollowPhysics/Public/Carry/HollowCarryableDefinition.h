#pragma once

#include "Carry/HollowCarryCoop.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"
#include "Templates/SubclassOf.h"

#include "HollowCarryableDefinition.generated.h"

class UGameplayEffect;
class UMaterialInterface;
class UPhysicalMaterial;

/** How the object looks while it counts as LootItem (S7-02: a purified Aether core glows another colour). */
USTRUCT(BlueprintType)
struct FHollowCarryableLook
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction", meta = (AllowedTypes = "HollowItem"))
	FPrimaryAssetId LootItem;

	/** Material of the visual's first slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction")
	TSoftObjectPtr<UMaterialInterface> Material;
};

/** Where a carried object's frame sits on a carrier component (its visual keeps its own offset in that frame). */
USTRUCT(BlueprintType)
struct FHollowCarryAttach
{
	GENERATED_BODY()

	/** Socket of the carrier component; none attaches to the component itself. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	FName Socket;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	FTransform Offset;
};

/**
 * Data of a heavy carryable object (Aether core, later heavy props): grip, local visual attachment, physics after a
 * drop, rest and dormancy timing, effects on the carrier (ADR-0015). Movement slowdown per carry mode is NOT here: it
 * belongs to the carrier's UHollowMovementTuning, because the server applies it from the claimed mode alone.
 */
UCLASS(BlueprintType)
class HOLLOWPHYSICS_API UHollowCarryableDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText PickupPrompt;

	/** Prompt for a second player taking the other side of an object someone carries (ADR-0028). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText JoinPrompt;

	/** The item this object counts as when the squad extracts with it (an Aether core is sold to the corporation). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction", meta = (AllowedTypes = "HollowItem"))
	FPrimaryAssetId LootItem;

	/** Looks for the items it can come to count as instead (UHollowCarryableComponent::SetLootItemOverride). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction")
	TArray<FHollowCarryableLook> Looks;

	const FHollowCarryableLook* FindLook(const FPrimaryAssetId& Item) const
	{
		return Looks.FindByPredicate([&Item](const FHollowCarryableLook& Look) { return Look.LootItem == Item; });
	}

	/**
	 * Where the object's body rides while carried, in the carrier's capsule space. This is the server truth (zone
	 * overlaps, AI perception) and the same on every machine: it is a function of the capsule only.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	FTransform GripOffset;

	/** The carrier's own view (first person): frame on the carrier's first-person attach component (its camera). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	FHollowCarryAttach FirstPerson;

	/** Everyone else: frame on the carrier's third-person attach component (its smoothed body mesh). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	FHollowCarryAttach ThirdPerson;

	/** Applied to a solo carrier by the server for as long as it carries (state tag, stamina drain, Resonance). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	TSubclassOf<UGameplayEffect> SoloEffect;

	/** Applied to each of two carriers instead of SoloEffect (state tag, a lighter stamina drain, Resonance; ADR-0028). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coop")
	TSubclassOf<UGameplayEffect> CoopEffect;

	/** Spacing and tether of two carriers; the pose between them is HollowCarryCoop::Pose at the grip height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coop")
	FHollowCoopCarryTuning Coop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics", meta = (ClampMin = "1", Units = "Kilograms"))
	float Mass = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	TObjectPtr<UPhysicalMaterial> PhysicalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics", meta = (ClampMin = "0"))
	float LinearDamping = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics", meta = (ClampMin = "0"))
	float AngularDamping = 0.f;

	/** Share of the carrier's velocity the object keeps when dropped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics", meta = (ClampMin = "0", ClampMax = "1"))
	float InheritVelocityFraction = 0.f;

	/** Replication rate while the dropped object is awake (Predictive Interpolation). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Replication", meta = (ClampMin = "1", Units = "Hertz"))
	float AwakeNetUpdateFrequency = 1.f;

	/**
	 * After the body falls asleep, the object waits this long (still replicating its final pose) before it turns
	 * kinematic, stops replicating movement and goes dormant. Gives clients time to converge on the rest pose.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Replication", meta = (ClampMin = "0", Units = "Seconds"))
	float DormancyDelaySeconds = 0.f;

	/** A dropped object that is still awake this long is put to sleep (jitter must never keep it replicating). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Replication", meta = (ClampMin = "0.1", Units = "Seconds"))
	float MaxAwakeSeconds = 1.f;
};
