#pragma once

#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"

#include "HollowEconomyConfig.generated.h"

/** One thing the corporation's shop sells (ADR-0031). */
USTRUCT(BlueprintType)
struct HOLLOWECONOMY_API FHollowShopOffer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (AllowedTypes = "HollowItem"))
	FPrimaryAssetId Item;

	/** Credits for one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "1"))
	int32 Price = 1;
};

/**
 * The hub economy (DA_EconomyConfig, ADR-0031): the shop's offers, what the corporation pays back, what insurance costs,
 * the stash's size and how many slots may be insured. Recipes are their own assets (UHollowCraftingRecipe).
 */
UCLASS(BlueprintType, Const)
class HOLLOWECONOMY_API UHollowEconomyConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The project's config (UHollowEconomySettings::EconomyConfig), loaded on first use; null if it is not set. */
	static const UHollowEconomyConfig* Get();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** What the shop sells (one offer per item). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (TitleProperty = "Item"))
	TArray<FHollowShopOffer> Offers;

	/** Percent of an item's BaseValue the corporation pays for one (rounded down): below 100, so buying to sell never pays. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "0", ClampMax = "99", Units = "Percent"))
	int32 SellPercent = 0;

	/** Insuring a loadout slot costs this percent of its items' BaseValue (rounded up); single-use. Whole percents: exact prices. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Insurance", meta = (ClampMin = "0", ClampMax = "100", Units = "Percent"))
	int32 InsurancePercent = 0;

	/** How many loadout slots may be insured at once (a ship upgrade raises it later). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Insurance", meta = (ClampMin = "0"))
	int32 MaxInsuredSlots = 0;

	/** Slots of the stash; each holds one item up to its MaxStack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stash", meta = (ClampMin = "1"))
	int32 StashSlots = 1;

	/** NAR-01b (ADR-0050): credits for each crew log first found in a raid, paid when the player extracts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lore", meta = (ClampMin = "0"))
	int32 LoreBonusCredits = 0;
};
