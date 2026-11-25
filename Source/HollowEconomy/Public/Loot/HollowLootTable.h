#pragma once

#include "Engine/DataAsset.h"
#include "Items/HollowItemTypes.h"
#include "UObject/PrimaryAssetId.h"

#include "HollowLootTable.generated.h"

/** One thing a loot crate may hold (GP-01b, ADR-0045). */
USTRUCT(BlueprintType)
struct HOLLOWECONOMY_API FHollowLootRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowedTypes = "HollowItem"))
	FPrimaryAssetId Item;

	/** Its share of the rolls against the other rows. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MaxCount = 1;
};

/**
 * What the loot crates of a sector hold (GP-01b, ADR-0045): rows by weight, a few rolls per crate. A crate rolls once,
 * from its seed (the sector's seed and its socket): the same sector always holds the same (HollowLootRules::Roll).
 */
UCLASS(BlueprintType, Const)
class HOLLOWECONOMY_API UHollowLootTableDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** What this table's crate with Seed holds. */
	TArray<FHollowItemStack> Roll(int32 Seed) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (TitleProperty = "Item"))
	TArray<FHollowLootRow> Rows;

	/** Rolls of one crate, from MinDrops to MaxDrops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0"))
	int32 MinDrops = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0"))
	int32 MaxDrops = 1;
};

/** Pure loot rules (GP-01b; covered by Hollow.Economy.Loot.Rules). */
namespace HollowLootRules
{
	/**
	 * MinDrops..MaxDrops rolls of Rows by weight, each MinCount..MaxCount of its item; the same item twice adds up, in the
	 * order first rolled. Deterministic: the same Seed always rolls the same (FRandomStream).
	 */
	HOLLOWECONOMY_API TArray<FHollowItemStack> Roll(TConstArrayView<FHollowLootRow> Rows, int32 MinDrops, int32 MaxDrops, int32 Seed);

	/**
	 * What of an inventory entry is found loot still held at the end of a raid: the held count covers what was taken
	 * from the profile first, then what was found (so the ship's free items go first, then the found, then the
	 * player's own).
	 */
	HOLLOWECONOMY_API int32 FoundHeld(int32 Held, int32 Taken, int32 Found);
}
