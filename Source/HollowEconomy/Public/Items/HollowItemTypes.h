#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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

/**
 * How many different items of one equip slot a loadout may hold (S6-04, ADR-0037): one tool per hand, one gadget, the
 * relics. In the settings it is the limit; on a ship upgrade, the slots it adds.
 */
USTRUCT(BlueprintType)
struct HOLLOWECONOMY_API FHollowEquipSlotLimit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout", meta = (Categories = "Hollow.Item.Slot"))
	FGameplayTag Slot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout", meta = (ClampMin = "0"))
	int32 Limit = 0;
};

/** The economy facts of items by id, for the pure rules (built from the definitions; tests fill it by hand). */
struct HOLLOWECONOMY_API FHollowItemCatalog
{
	void Add(const FPrimaryAssetId& Item, int32 Value, bool bInsurable, int32 MaxStack = 1, const FGameplayTag& ItemType = FGameplayTag(),
		const FGameplayTag& EquipSlot = FGameplayTag());
	bool Contains(const FPrimaryAssetId& Item) const { return Entries.Contains(Item); }
	/** Value of one; unknown items are worth nothing. */
	int32 GetValue(const FPrimaryAssetId& Item) const;
	/** Unknown items cannot be insured. */
	bool IsInsurable(const FPrimaryAssetId& Item) const;
	/** How many one loadout slot holds (at least 1); 0 for unknown items. */
	int32 GetMaxStack(const FPrimaryAssetId& Item) const;
	/** Its Hollow.Item.Type.* child; none for unknown items. */
	FGameplayTag GetItemType(const FPrimaryAssetId& Item) const;
	/** Its Hollow.Item.Slot.* child (S6-04); none for unknown items and items without a slot (fuel cells). */
	FGameplayTag GetEquipSlot(const FPrimaryAssetId& Item) const;

	/** Loads (synchronously: tiny data assets) the definitions of Items and catalogs them. */
	static FHollowItemCatalog Load(TConstArrayView<FPrimaryAssetId> Items);

private:
	struct FEntry
	{
		int32 Value = 0;
		bool bInsurable = false;
		int32 MaxStack = 1;
		FGameplayTag ItemType;
		FGameplayTag EquipSlot;
	};
	TMap<FPrimaryAssetId, FEntry> Entries;
};
