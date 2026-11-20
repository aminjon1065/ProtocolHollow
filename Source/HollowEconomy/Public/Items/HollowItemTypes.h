#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"

#include "HollowItemTypes.generated.h"

/** A number of one item (loot, losses on the debrief). */
USTRUCT(BlueprintType)
struct HOLLOWECONOMY_API FHollowItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (AllowedTypes = "HollowItem"))
	FPrimaryAssetId Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0"))
	int32 Count = 0;

	bool operator==(const FHollowItemStack&) const = default;
};

/** One slot of the loadout a player takes into the raid; insured slots survive death (ADR-0016). */
USTRUCT(BlueprintType)
struct HOLLOWECONOMY_API FHollowLoadoutSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout", meta = (AllowedTypes = "HollowItem"))
	FPrimaryAssetId Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout", meta = (ClampMin = "0"))
	int32 Count = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	bool bInsured = false;

	bool operator==(const FHollowLoadoutSlot&) const = default;
};

/** The economy facts of items by id, for the pure rules (built from the definitions; tests fill it by hand). */
struct HOLLOWECONOMY_API FHollowItemCatalog
{
	void Add(const FPrimaryAssetId& Item, int32 Value, bool bInsurable);
	bool Contains(const FPrimaryAssetId& Item) const { return Entries.Contains(Item); }
	/** Value of one; unknown items are worth nothing. */
	int32 GetValue(const FPrimaryAssetId& Item) const;
	/** Unknown items cannot be insured. */
	bool IsInsurable(const FPrimaryAssetId& Item) const;

	/** Loads (synchronously: tiny data assets) the definitions of Items and catalogs them. */
	static FHollowItemCatalog Load(TConstArrayView<FPrimaryAssetId> Items);

private:
	struct FEntry
	{
		int32 Value = 0;
		bool bInsurable = false;
	};
	TMap<FPrimaryAssetId, FEntry> Entries;
};
