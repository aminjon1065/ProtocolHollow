#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "HollowItemDefinition.generated.h"

/**
 * An item of the economy (Docs/Architecture/02 sec. 2.5): identity, type, value and insurance. Light by design: every
 * definition may be loaded at once and referenced by FPrimaryAssetId in profiles and network messages. Behaviour
 * fragments (equippable, stack rules, Resonance) arrive with the inventory.
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStack = 1;
};
