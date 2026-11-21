#pragma once

#include "GameplayTagContainer.h"
#include "Items/HollowItemTypes.h"
#include "Misc/Guid.h"

#include "HollowLoadoutRules.generated.h"

/** What one player takes into a raid (ADR-0005, ADR-0030): sent once by the player's machine, checked by the host. */
USTRUCT(BlueprintType)
struct HOLLOWECONOMY_API FHollowLoadoutSnapshot
{
	GENERATED_BODY()

	/** The raid it is for (the host's request names it); a snapshot for another raid is ignored. */
	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	FGuid RaidId;

	/** One stack per loadout slot, in slot order. Insurance stays on the player's machine (its escrow). */
	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	TArray<FHollowItemStack> Slots;
};

/** Why a slot of a loadout was dropped or cut down (log and tests). */
UENUM()
enum class EHollowLoadoutIssue : uint8
{
	UnknownItem,
	/** Its type is not taken into raids (an Aether core). */
	NotForRaids,
	NoCount,
	/** More than one slot holds: cut down to MaxStack. */
	OverStack,
	/** After the most slots a raid accepts. */
	TooManySlots
};

struct HOLLOWECONOMY_API FHollowLoadoutIssue
{
	FPrimaryAssetId Item;
	EHollowLoadoutIssue Issue = EHollowLoadoutIssue::UnknownItem;

	bool operator==(const FHollowLoadoutIssue&) const = default;
};

/** Pure rules of the loadout a raid accepts (ADR-0030). Tested by Hollow.Economy.LoadoutSync.Validate. */
namespace HollowLoadoutRules
{
	/**
	 * What a raid accepts of a loadout's slots, one stack per item in first-slot order. Soft (nobody is kicked,
	 * ADR-0005): a slot of an unknown item, of a type outside RaidTypes or without a count is dropped; a slot over its
	 * item's MaxStack is cut down to it; slots after the first MaxSlots accepted ones are dropped. Accepted slots of
	 * one item are merged.
	 */
	HOLLOWECONOMY_API TArray<FHollowItemStack> Validate(TConstArrayView<FHollowItemStack> Slots, const FHollowItemCatalog& Catalog,
		const FGameplayTagContainer& RaidTypes, int32 MaxSlots, TArray<FHollowLoadoutIssue>* OutIssues = nullptr);

	/** Validate with UHollowEconomySettings and the slots' item definitions (loaded synchronously: tiny data assets). */
	HOLLOWECONOMY_API TArray<FHollowItemStack> ValidateForRaid(TConstArrayView<FHollowItemStack> Slots, TArray<FHollowLoadoutIssue>* OutIssues = nullptr);

	/** A profile's loadout as the stacks a snapshot sends. */
	HOLLOWECONOMY_API TArray<FHollowItemStack> ToStacks(TConstArrayView<FHollowLoadoutSlot> Loadout);

	/** "DA_Item_Welder x1, DA_Item_FuelCell x3" ("none" if empty), for logs. */
	HOLLOWECONOMY_API FString Describe(TConstArrayView<FHollowItemStack> Stacks);
	HOLLOWECONOMY_API FString Describe(TConstArrayView<FHollowLoadoutIssue> Issues);
}
