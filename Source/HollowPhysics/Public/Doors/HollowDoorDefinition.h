#pragma once

#include "Doors/HollowDoorTypes.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "HollowDoorDefinition.generated.h"

class UNavArea;

/** Tuning of a door type, primary asset type HollowDoor (Docs/Architecture/03 sec. 3.3). */
UCLASS(BlueprintType)
class HOLLOWPHYSICS_API UHollowDoorDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	explicit UHollowDoorDefinition(const FObjectInitializer& ObjectInitializer);

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Tool whose work welds this door. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weld", meta = (Categories = "Hollow.Ability.Tool"))
	FGameplayTag WeldToolTag;

	/** Tool whose work cuts the weld. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weld", meta = (Categories = "Hollow.Ability.Tool"))
	FGameplayTag CutToolTag;

	/** Weld integrity gained per tool work unit (integrity is 0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weld", meta = (ClampMin = "0"))
	float WeldPerWorkUnit = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weld", meta = (ClampMin = "0"))
	float CutPerWorkUnit = 0.f;

	/** A closed door becomes Welded at this integrity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weld", meta = (ClampMin = "0", ClampMax = "1"))
	float SealThreshold = 0.f;

	/** Weld changes go out at most this often while a welder runs; the final value follows when it stops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Network", meta = (ClampMin = "0", Units = "Seconds"))
	float WeldSendInterval = 0.f;

	/** A client plays a transition (and its one-shot effects) only if it started less than this long ago; otherwise it snaps. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Network", meta = (ClampMin = "0", Units = "Seconds"))
	float TransitionReplayWindow = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leaf", meta = (ClampMin = "0.01", Units = "Seconds"))
	float OpenDuration = 0.f;

	/** Ease in/out of the leaf motion (1 = linear). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leaf", meta = (ClampMin = "1"))
	float OpenEaseExponent = 0.f;

	/** Fraction of the opening travel after which the leaf stops blocking pawns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leaf", meta = (ClampMin = "0", ClampMax = "1"))
	float PassableFraction = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	EHollowDoorState InitialState = EHollowDoorState::Closed;

	/** Structural damage the bare door takes before it is Breached (sec. 3.3 rows 7-8, S3-04). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure", meta = (ClampMin = "0"))
	float StructureHitPoints = 0.f;

	/** Structural damage a full weld soaks up before the structure takes any; a partial weld soaks its share. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure", meta = (ClampMin = "0"))
	float WeldAbsorbFactor = 0.f;

	/** Speed of the cosmetic leaf torn off by a breach (each machine on its own, never collides with pawns). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float TornLeafSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText PromptOpen;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText PromptClose;

	/** Nav link areas per door state (Docs/Architecture/03 sec. 3.7); costs and exclusions live in the query filters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	TSubclassOf<UNavArea> AreaDoorway;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	TSubclassOf<UNavArea> AreaClosed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	TSubclassOf<UNavArea> AreaLocked;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	TSubclassOf<UNavArea> AreaWeldedLow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	TSubclassOf<UNavArea> AreaWeldedHigh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	TSubclassOf<UNavArea> AreaBarricaded;

	/** Welded doors at or above this integrity use AreaWeldedHigh (dearer to breach). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (ClampMin = "0", ClampMax = "1"))
	float WeldedHighThreshold = 0.f;
};
