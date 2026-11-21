#pragma once

#include "Engine/DataAsset.h"
#include "Items/HollowItemTypes.h"

#include "HollowCraftingRecipe.generated.h"

struct FHollowRecipeTerms;

/**
 * A workbench recipe (ADR-0031), primary asset type HollowRecipe: what it takes from the stash (items and credits) and
 * what it puts there. Every recipe is known from the start; blueprints that unlock recipes come later.
 */
UCLASS(BlueprintType)
class HOLLOWECONOMY_API UHollowCraftingRecipe : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** The plain terms the rules work with (HollowCraftingRules). */
	FHollowRecipeTerms ToTerms() const;

	/** Every recipe of the project, in PrimaryAssetId order (loaded synchronously: tiny data assets). */
	static TArray<FHollowRecipeTerms> LoadAll();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FText DisplayName;

	/** Items one craft takes from the stash. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe", meta = (TitleProperty = "Item"))
	TArray<FHollowItemStack> Inputs;

	/** Credits one craft costs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = "0"))
	int32 Credits = 0;

	/** What one craft puts in the stash. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FHollowItemStack Output;

	/** The workbench level it needs (hub upgrades later: every recipe is 1 now). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = "1"))
	int32 WorkbenchLevel = 1;
};
