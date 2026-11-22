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
	Unequip,
	/** Ship upgrade Upgrade, once and for good (ADR-0032). */
	BuyUpgrade,
	/** Play class ClassId (ADR-0034): the current loadout is kept for the current class, the new class's comes back. */
	SelectClass
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
	InsuranceLimit,
	UnknownUpgrade,
	/** The ship has this upgrade already. */
	AlreadyOwned,
	UnknownClass,
	/** The player plays this class already. */
	AlreadySelected
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

	/** BuyUpgrade. */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	FPrimaryAssetId Upgrade;

	/** SelectClass. */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	FPrimaryAssetId ClassId;

	/** New transactions, each with a new id. */
	static FHollowProfileTransaction Buy(const FPrimaryAssetId& Item, int32 Count);
	static FHollowProfileTransaction Sell(const FPrimaryAssetId& Item, int32 Count);
	static FHollowProfileTransaction Craft(const FPrimaryAssetId& Recipe, int32 Times);
	static FHollowProfileTransaction Insure(int32 Slot);
	static FHollowProfileTransaction Equip(const FPrimaryAssetId& Item, int32 Count);
	static FHollowProfileTransaction Unequip(int32 Slot, int32 Count = 0);
	static FHollowProfileTransaction BuyUpgrade(const FPrimaryAssetId& Upgrade);
	static FHollowProfileTransaction SelectClass(const FPrimaryAssetId& ClassId);

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

/** A ship upgrade as the rules see it (UHollowShipUpgradeDefinition::ToTerms; tests fill it by hand). */
struct HOLLOWECONOMY_API FHollowUpgradeTerms
{
	FPrimaryAssetId Id;
	int32 Price = 0;
	int32 ExtraInsuredSlots = 0;
	int32 CoreValuePercent = 0;
	TArray<FHollowItemStack> RaidItems;
};

/** A player class as the rules see it (UHollowClassDefinition::ToTerms; tests fill it by hand). */
struct HOLLOWECONOMY_API FHollowClassTerms
{
	FPrimaryAssetId Id;
	/** The loadout a profile gets the first time it picks the class. */
	TArray<FHollowLoadoutSlot> StartingLoadout;
};

/** What a ship's upgrades add up to (HollowCraftingRules::SumEffects). */
struct HOLLOWECONOMY_API FHollowUpgradeEffects
{
	/** More loadout slots may be insured at once. */
	int32 ExtraInsuredSlots = 0;
	/** Extra percent of their value sold Aether cores bring. */
	int32 CoreValuePercent = 0;
	/** Given to the player's raid inventory every raid, free: used first, never taken from the profile. */
	TArray<FHollowItemStack> RaidItems;
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
	/** By ship upgrade id (lookups only). */
	TMap<FPrimaryAssetId, FHollowUpgradeTerms> Upgrades;

	/** By player class id (lookups only). */
	TMap<FPrimaryAssetId, FHollowClassTerms> Classes;

	/** The class of a profile that names none (profiles from before S6-01). */
	FPrimaryAssetId DefaultClass;
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
	/** At most MaxInsuredSlots plus what the ship's upgrades add. */
	HOLLOWECONOMY_API EHollowTransactionResult Insure(FHollowProfile& Profile, int32 Slot, const FHollowEconomyTerms& Terms);
	/** Fills uninsured slots of Item first (an insured slot never grows for free), then new slots of at most MaxStack. */
	HOLLOWECONOMY_API EHollowTransactionResult Equip(FHollowProfile& Profile, const FPrimaryAssetId& Item, int32 Count, const FHollowEconomyTerms& Terms);
	/** Count 0 or the whole slot: the slot goes, with its insurance. */
	HOLLOWECONOMY_API EHollowTransactionResult Unequip(FHollowProfile& Profile, int32 Slot, int32 Count, const FHollowEconomyTerms& Terms);
	HOLLOWECONOMY_API EHollowTransactionResult BuyUpgrade(FHollowProfile& Profile, const FPrimaryAssetId& Upgrade, const FHollowEconomyTerms& Terms);

	/**
	 * Play ClassId (ADR-0034): the loadout now is kept for the class played so far, the new class's loadout comes back
	 * (its starting loadout the first time it is picked). The stash is shared; nothing is lost or bought.
	 */
	HOLLOWECONOMY_API EHollowTransactionResult SelectClass(FHollowProfile& Profile, const FPrimaryAssetId& ClassId, const FHollowEconomyTerms& Terms);

	/** The class Profile plays: its own, or the default for a profile that names none. */
	HOLLOWECONOMY_API FPrimaryAssetId CurrentClass(const FHollowProfile& Profile, const FHollowEconomyTerms& Terms);

	/** The effects of the upgrades Owned names (unknown ids are ignored, each upgrade counts once). */
	HOLLOWECONOMY_API FHollowUpgradeEffects SumEffects(TConstArrayView<FPrimaryAssetId> Owned, const TMap<FPrimaryAssetId, FHollowUpgradeTerms>& Upgrades);

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
