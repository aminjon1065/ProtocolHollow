#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"

#include "HollowRitualAltarDefinition.generated.h"

/**
 * Data of the Purification Altar (S7-02, ADR-0039): what it takes and makes, how long and how loud the ritual is, its
 * greybox shape. Every number is here (CLAUDE.md sec. 1); AHollowRitualAltar only runs it.
 */
UCLASS(BlueprintType)
class HOLLOWGAME_API UHollowRitualAltarDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** The carried object it takes: one that counts as this item now (an Aether core). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ritual", meta = (AllowedTypes = "HollowItem"))
	FPrimaryAssetId AcceptedItem;

	/** What that object counts as after the ritual (a purified core). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ritual", meta = (AllowedTypes = "HollowItem"))
	FPrimaryAssetId ResultItem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ritual", meta = (ClampMin = "0.1", Units = "Seconds"))
	float RitualSeconds = 0.f;

	/** A bell tolls (and the AI hears it) at the start and every PulseSeconds after, while the ritual lasts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ritual", meta = (ClampMin = "0.1", Units = "Seconds"))
	float PulseSeconds = 0.f;

	/** Noise of each toll: its rule in DA_NoiseConfig, its sound in DA_SoundBank (Hollow.Noise.Ritual). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ritual", meta = (Categories = "Hollow.Noise"))
	FGameplayTag NoiseTag;

	/** Heat added at once to the altar's room when the ritual starts (the Echo-Stalker's hunt threshold is 40). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ritual", meta = (ClampMin = "0"))
	float HeatBurst = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText PlacePrompt;

	/** Where the object rests on the altar (its bowl), in the altar's frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	FTransform Seat;

	/** Greybox pieces of the altar (engine cubes, instance transforms in the altar's frame). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	TArray<FTransform> Pieces;

	/** Light over the bowl during the ritual, as a fraction of the light's own intensity; each toll flashes it full. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0", ClampMax = "1"))
	float RitualGlow = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0", Units = "Seconds"))
	float FlashSeconds = 0.f;

	/** After the ritual the light stays at this fraction (the purified core rests on a lit altar). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0", ClampMax = "1"))
	float SpentGlow = 0.f;
};
