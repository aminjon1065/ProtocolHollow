#pragma once

#include "Doors/HollowDoorTypes.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "HollowDoorDefinition.generated.h"

/** Tuning of a door type, primary asset type HollowDoor (Docs/Architecture/03 sec. 3.3). */
UCLASS(BlueprintType)
class HOLLOWPHYSICS_API UHollowDoorDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText PromptOpen;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText PromptClose;
};
