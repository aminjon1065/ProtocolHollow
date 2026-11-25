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

	/** The ship upgrades the player owns (ADR-0032): the raid gives their RaidItems on top, free. */
	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	TArray<FPrimaryAssetId> Upgrades;

	/** The class the player plays (ADR-0034): the host gives it to the player state, a class it does not know: the default. */
	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	FPrimaryAssetId ClassId;

	/**
	 * NAR-01b (ADR-0050): the crew logs the player has found: the host gives the squad's datapads the logs the fewest of
	 * them know. Trusted like the rest (the player's own profile); it only decides which log a datapad holds.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	TArray<FPrimaryAssetId> KnownLore;
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
	TooManySlots,
	/** Its equip slot is full: another tool on its input, another gadget, every relic slot used, or a second one-of (S6-04). */
	SlotTaken
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

	/**
	 * The same with equip slot limits (S6-04, ADR-0037): a slot whose item the loadout cannot take into its equip slot
	 * (HollowCraftingRules::CanTakeIntoSlot, against the slots accepted before it) is dropped: the first one wins.
	 */
	HOLLOWECONOMY_API TArray<FHollowItemStack> Validate(TConstArrayView<FHollowItemStack> Slots, const FHollowItemCatalog& Catalog,
		const FGameplayTagContainer& RaidTypes, int32 MaxSlots, const TMap<FGameplayTag, int32>& EquipLimits, TArray<FHollowLoadoutIssue>* OutIssues = nullptr);

	/**
	 * Validate with UHollowEconomySettings, the slots' item definitions (loaded synchronously: tiny data assets) and the
	 * equip slot limits the ship's Upgrades give (a Reliquary: a second relic).
	 */
	HOLLOWECONOMY_API TArray<FHollowItemStack> ValidateForRaid(TConstArrayView<FHollowItemStack> Slots, TConstArrayView<FPrimaryAssetId> Upgrades,
		TArray<FHollowLoadoutIssue>* OutIssues = nullptr);

	/**
	 * A profile's loadout as the stacks a snapshot sends: one per slot, never merged (Validate's result merges the slots
	 * of one item, and a merged stack over MaxStack would be cut down when checked again).
	 */
	HOLLOWECONOMY_API TArray<FHollowItemStack> ToStacks(TConstArrayView<FHollowLoadoutSlot> Loadout);

	/** "DA_Item_Welder x1, DA_Item_FuelCell x3" ("none" if empty), for logs. */
	HOLLOWECONOMY_API FString Describe(TConstArrayView<FHollowItemStack> Stacks);
	HOLLOWECONOMY_API FString Describe(TConstArrayView<FHollowLoadoutIssue> Issues);
}
