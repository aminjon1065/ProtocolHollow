#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "HollowItemDefinition.generated.h"

class UHollowAbilitySet;

/**
 * An item of the economy (Docs/Architecture/02 sec. 2.5): identity, type, value and insurance. Light by design: every
 * definition may be loaded at once and referenced by FPrimaryAssetId in profiles and network messages; what it does in
 * a raid is a soft reference (AbilitySet), loaded only when it is taken into one (ADR-0030).
 */
UCLASS(BlueprintType)
class HOLLOWECONOMY_API UHollowItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	/** Exactly one Hollow.Item.Type.* child. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (Categories = "Hollow.Item.Type"))
	FGameplayTag ItemType;

	/** What the corporation pays for one (an extracted Aether core is sold at once). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = "0"))
	int32 BaseValue = 0;

	/** Can a loadout slot holding this item be insured (kept on death)? */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy")
	bool bInsurable = false;

	/** How many one loadout slot holds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStack = 1;

	/**
	 * Abilities and effects the player has while the item is in their raid inventory (ADR-0030): a tool on its input, a
	 * consumable's use (its abilities take the item as their cost: UHollowGameplayAbility::ItemCostPerUse). None: the
	 * item does nothing in a raid.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Raid")
	TSoftObjectPtr<UHollowAbilitySet> AbilitySet;
};
