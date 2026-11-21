#pragma once

#include "GameplayTagContainer.h"
#include "Items/HollowItemTypes.h"
#include "Misc/Guid.h"

#include "HollowCraftingRules.generated.h"

struct FHollowProfile;
struct FHollowProfileState;

/** What a profile transaction does (ADR-0031): the hub's shop, workbench and loadout locker, all local to the player's machine. */
UENUM(BlueprintType)
enum class EHollowTransactionKind : uint8
{
	/** Count of Item from the shop into the stash, for its price. */
	Buy,
	/** Count of Item from the stash to the corporation, for its sell price. */
	Sell,
	/** Recipe Count times: its inputs and credits out of the stash, its output in. */
	Craft,
	/** Insurance for loadout slot Slot, for its price; single-use. */
	Insure,
	/** Count of Item from the stash into the loadout. */
	Equip,
	/** Count of loadout slot Slot back into the stash (0: the whole slot, its insurance gone). */
	Unequip
};

/** How a profile transaction ended; anything but Applied changed nothing. */
UENUM(BlueprintType)
enum class EHollowTransactionResult : uint8
{
	Applied,
	/** A transaction with this id was applied already: nothing happens twice. */
	AlreadyApplied,
	/** A raid is in escrow: the hub's transactions wait for its result. */
	InRaid,
	/** No id, a count below 1, a slot that does not exist. */
	BadRequest,
	UnknownItem,
	UnknownRecipe,
	NotForSale,
	NotEnoughCredits,
	NotEnoughItems,
	StashFull,
	LoadoutFull,
	/** Only raid item types go into the loadout (UHollowEconomySettings::LoadoutItemTypes). */
	NotForRaids,
	NotInsurable,
	AlreadyInsured,
	InsuranceLimit
};

/** One request of the hub to the local profile (ADR-0031). Its Id makes it idempotent. */
USTRUCT(BlueprintType)
struct HOLLOWECONOMY_API FHollowProfileTransaction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	FGuid Id;

	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	EHollowTransactionKind Kind = EHollowTransactionKind::Buy;

	/** Buy, Sell, Equip. */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	FPrimaryAssetId Item;

	/** Craft. */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	FPrimaryAssetId Recipe;

	/** Items, or crafts; for Unequip 0 is the whole slot. */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	int32 Count = 1;

	/** Insure, Unequip: the loadout slot. */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	int32 Slot = INDEX_NONE;

	/** New transactions, each with a new id. */
	static FHollowProfileTransaction Buy(const FPrimaryAssetId& Item, int32 Count);
	static FHollowProfileTransaction Sell(const FPrimaryAssetId& Item, int32 Count);
	static FHollowProfileTransaction Craft(const FPrimaryAssetId& Recipe, int32 Times);
	static FHollowProfileTransaction Insure(int32 Slot);
	static FHollowProfileTransaction Equip(const FPrimaryAssetId& Item, int32 Count);
	static FHollowProfileTransaction Unequip(int32 Slot, int32 Count = 0);

	FString Describe() const;
};

/** A recipe as the rules see it (UHollowCraftingRecipe::ToTerms; tests fill it by hand). */
struct HOLLOWECONOMY_API FHollowRecipeTerms
{
	FPrimaryAssetId Id;
	TArray<FHollowItemStack> Inputs;
	int32 Credits = 0;
	FHollowItemStack Output;
};

/**
 * Everything the transactions need besides the profile: item facts, the shop, recipes and limits. Built from
 * DA_EconomyConfig, the item definitions, the recipe assets and UHollowEconomySettings (Load); tests fill it by hand.
 */
struct HOLLOWECONOMY_API FHollowEconomyTerms
{
	FHollowItemCatalog Catalog;
	/** The shop's price of one, by item (lookups only: never iterated). */
	TMap<FPrimaryAssetId, int32> ShopPrices;
	/** By recipe id (lookups only). */
	TMap<FPrimaryAssetId, FHollowRecipeTerms> Recipes;
	int32 SellPercent = 0;
	int32 InsurancePercent = 0;
	int32 MaxInsuredSlots = 0;
	int32 StashSlots = 0;
	int32 MaxLoadoutSlots = 0;
	FGameplayTagContainer LoadoutItemTypes;

	/** The project's terms for State and Transaction (loads the config, every recipe and the items they name). */
	static FHollowEconomyTerms Load(const FHollowProfileState& State, const FHollowProfileTransaction* Transaction = nullptr);
};

/**
 * Pure rules of the hub economy (ADR-0031), on the player's own machine: shop, workbench, insurance, loadout locker.
 * Every operation is all or nothing: refused, it leaves the profile as it was. Tested by Hollow.Economy.Crafting.*.
 */
namespace HollowCraftingRules
{
	/** How many applied transaction ids a profile remembers (FHollowProfileState::RecentTransactions). */
	inline constexpr int32 RememberedTransactions = 32;

	/**
	 * Applies Transaction to State: a repeated id changes nothing (AlreadyApplied), a raid in escrow refuses it (InRaid,
	 * the loadout is out there); otherwise its operation, all or nothing, and its id is remembered when it applied.
	 */
	HOLLOWECONOMY_API EHollowTransactionResult Apply(FHollowProfileState& State, const FHollowProfileTransaction& Transaction, const FHollowEconomyTerms& Terms);

	// The operations (all or nothing each).
	HOLLOWECONOMY_API EHollowTransactionResult Buy(FHollowProfile& Profile, const FPrimaryAssetId& Item, int32 Count, const FHollowEconomyTerms& Terms);
	HOLLOWECONOMY_API EHollowTransactionResult Sell(FHollowProfile& Profile, const FPrimaryAssetId& Item, int32 Count, const FHollowEconomyTerms& Terms);
	HOLLOWECONOMY_API EHollowTransactionResult Craft(FHollowProfile& Profile, const FPrimaryAssetId& Recipe, int32 Times, const FHollowEconomyTerms& Terms);
	HOLLOWECONOMY_API EHollowTransactionResult Insure(FHollowProfile& Profile, int32 Slot, const FHollowEconomyTerms& Terms);
	/** Fills uninsured slots of Item first (an insured slot never grows for free), then new slots of at most MaxStack. */
	HOLLOWECONOMY_API EHollowTransactionResult Equip(FHollowProfile& Profile, const FPrimaryAssetId& Item, int32 Count, const FHollowEconomyTerms& Terms);
	/** Count 0 or the whole slot: the slot goes, with its insurance. */
	HOLLOWECONOMY_API EHollowTransactionResult Unequip(FHollowProfile& Profile, int32 Slot, int32 Count, const FHollowEconomyTerms& Terms);

	/** Credits the corporation pays for one: BaseValue x SellPercent / 100, rounded down. */
	HOLLOWECONOMY_API int32 SellPrice(const FHollowEconomyTerms& Terms, const FPrimaryAssetId& Item);
	/** Insuring Slot: its items' BaseValue x InsurancePercent / 100, rounded up. */
	HOLLOWECONOMY_API int32 InsurancePrice(const FHollowEconomyTerms& Terms, const FHollowLoadoutSlot& Slot);

	/** Count of Item into Stash: partial stacks first, then new stacks of at most MaxStack; false (nothing added) beyond StashSlots. */
	HOLLOWECONOMY_API bool AddToStash(TArray<FHollowItemStack>& Stash, const FPrimaryAssetId& Item, int32 Count, int32 MaxStack, int32 StashSlots);
	/** Count of Item out of Stash, from its last stacks; false (nothing taken) if fewer are there. */
	HOLLOWECONOMY_API bool TakeFromStash(TArray<FHollowItemStack>& Stash, const FPrimaryAssetId& Item, int32 Count);
	HOLLOWECONOMY_API int32 CountOf(TConstArrayView<FHollowItemStack> Stacks, const FPrimaryAssetId& Item);
}
