#pragma once

#include "Crafting/HollowCraftingRules.h"

struct FHollowProfileState;

/** What a hub row's button does (S5-04, ADR-0033). */
enum class EHollowHubCommand : uint8
{
	/** Applies its profile transaction on this machine's profile (workbench, locker). */
	Transaction,
	/** Asks the server to make its contract the squad's (the host's contracts screen). */
	SelectContract
};

/** One button of a hub screen's row: what it does and whether it can now. */
struct HOLLOWUI_API FHollowHubAction
{
	FText Label;
	EHollowHubCommand Command = EHollowHubCommand::Transaction;
	/** Transaction: what it applies (its id is the one previewed, so a double press applies it once). */
	FHollowProfileTransaction Transaction;
	/** SelectContract: which. */
	FPrimaryAssetId Contract;
	/** False: greyed out, still focusable so the reason shows. */
	bool bAvailable = true;
	/** Why not (what the rules would answer now). */
	FText Reason;
};

/** One line of a hub screen's list. */
struct HOLLOWUI_API FHollowHubRow
{
	FText Name;
	FText Detail;
	/** A section title: no detail, no buttons. */
	bool bHeader = false;
	/** Stands out: an owned upgrade, an insured slot, the squad's contract. */
	bool bHighlighted = false;
	/** At most HollowHubScreenRules::MaxActions. */
	TArray<FHollowHubAction> Actions;
};

/** Names and descriptions of the assets a screen shows, by id (lookups only); unknown ids show their asset name. */
struct HOLLOWUI_API FHollowHubNames
{
	FText GetName(const FPrimaryAssetId& Id) const;
	FText GetDescription(const FPrimaryAssetId& Id) const;
	void Add(const FPrimaryAssetId& Id, const FText& Name, const FText& Description = FText::GetEmpty());

	/** Loads (synchronously: tiny data assets) the items, recipes, ship upgrades, contracts and classes Ids name. */
	static FHollowHubNames Load(TConstArrayView<FPrimaryAssetId> Ids);

private:
	struct FEntry
	{
		FText Name;
		FText Description;
	};
	TMap<FPrimaryAssetId, FEntry> Entries;
};

/** A contract as the contracts screen shows it (ADR-0041): its sector, and what it pays this player. */
struct HOLLOWUI_API FHollowContractOffer
{
	FPrimaryAssetId Contract;
	FText SectorName;
	/** Each reward item and the credits one brings this player home (FHollowExtractionResolver::ItemEarnings). */
	TArray<TPair<FPrimaryAssetId, int32>> Rewards;

	/**
	 * The offers of Contracts to a player whose ship adds CoreValuePercent to Aether cores: loads the contracts and their
	 * reward items (tiny data assets). Unknown contracts have no sector and no reward.
	 */
	static TArray<FHollowContractOffer> Load(TConstArrayView<FPrimaryAssetId> Contracts, int32 CoreValuePercent);
};

/**
 * Pure rules of the hub's screens (S5-04, ADR-0033): which rows and buttons they show and what each button would do
 * now. Every button is previewed by HollowCraftingRules on a copy of the profile, so the screens repeat no economy
 * rule: a button the rules would refuse is greyed out with the rules' own reason. Tested by Hollow.UI.Hub.Rules.
 */
namespace HollowHubScreenRules
{
	/** Buttons a row may have. */
	inline constexpr int32 MaxActions = 3;

	/** What a result tells the player ("Not enough credits"); empty for Applied. */
	HOLLOWUI_API FText ResultText(EHollowTransactionResult Result);

	/** What Transaction would do to State now, on a copy: nothing changes. */
	HOLLOWUI_API EHollowTransactionResult Preview(const FHollowProfileState& State, const FHollowProfileTransaction& Transaction, const FHollowEconomyTerms& Terms);

	/** A button applying Transaction: greyed out with the reason unless it would apply now. */
	HOLLOWUI_API FHollowHubAction MakeAction(const FText& Label, const FHollowProfileTransaction& Transaction, const FHollowProfileState& State,
		const FHollowEconomyTerms& Terms);

	/** How many times Recipe can be crafted at once now (0: not even once). */
	HOLLOWUI_API int32 MaxCrafts(const FHollowProfileState& State, const FPrimaryAssetId& Recipe, const FHollowEconomyTerms& Terms);

	/** Workbench, Craft: a row per recipe (id order) with Craft and Craft max. */
	HOLLOWUI_API TArray<FHollowHubRow> CraftRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms, const FHollowHubNames& Names);

	/** Workbench, Shop: what the shop sells (id order; Buy 1, Buy BulkCount), then what the stash holds (Sell 1, Sell all). */
	HOLLOWUI_API TArray<FHollowHubRow> ShopRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms, const FHollowHubNames& Names, int32 BulkCount);

	/** Workbench, Upgrades: a row per ship upgrade (id order), Buy or Owned. */
	HOLLOWUI_API TArray<FHollowHubRow> UpgradeRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms, const FHollowHubNames& Names);

	/** Locker, left: a row per item in the stash (the stash's order), Take 1 and Take all into the loadout. */
	HOLLOWUI_API TArray<FHollowHubRow> StashRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms, const FHollowHubNames& Names);

	/** Locker, right: a row per loadout slot, Return 1 (more than one there), Return all, Insure. */
	HOLLOWUI_API TArray<FHollowHubRow> LoadoutRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms, const FHollowHubNames& Names);

	/** Locker, top: a row per player class (id order), Choose; the class played is highlighted (ADR-0034). */
	HOLLOWUI_API TArray<FHollowHubRow> ClassRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms, const FHollowHubNames& Names);

	/**
	 * Contracts (id order), each with its sector and reward (ADR-0041): the host gets Select on each, a guest sees the
	 * host's choice only.
	 */
	HOLLOWUI_API TArray<FHollowHubRow> ContractRows(TConstArrayView<FHollowContractOffer> Offers, const FPrimaryAssetId& Selected, bool bHost,
		const FHollowHubNames& Names);

	/** "Sector II - Reactor Cathedral. Reward: Aether core 500 cr, Purified Aether core 1250 cr" (the sector alone without a reward). */
	HOLLOWUI_API FText OfferText(const FHollowContractOffer& Offer, const FHollowHubNames& Names);

	/** The contracts screen's role line: who decides, then the squad's contract, its sector and reward (live for a guest). */
	HOLLOWUI_API FText ContractRoleText(TConstArrayView<FHollowContractOffer> Offers, const FPrimaryAssetId& Selected, bool bHost, const FHollowHubNames& Names);

	/** What the ship's upgrades in State add to sold Aether cores, in percent (ADR-0032). */
	HOLLOWUI_API int32 CoreValuePercent(const FHollowProfileState& State, const FHollowEconomyTerms& Terms);

	/** Credits, stash and loadout use, insured slots of the limit (the ship's upgrades included), a raid in escrow. */
	HOLLOWUI_API FText StatusLine(const FHollowProfileState& State, const FHollowEconomyTerms& Terms);

	/** Every id the profile rows may name (items, recipes, upgrades), to load their names. */
	HOLLOWUI_API TArray<FPrimaryAssetId> NamedIds(const FHollowProfileState& State, const FHollowEconomyTerms& Terms);

	/** "Fuel cell", "Scrap x2". */
	HOLLOWUI_API FText StackText(const FHollowHubNames& Names, const FPrimaryAssetId& Item, int32 Count);
}
