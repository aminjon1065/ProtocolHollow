#include "Hub/HollowHubScreenRules.h"

#include "Algo/Count.h"
#include "Classes/HollowClassDefinition.h"
#include "Crafting/HollowCraftingRecipe.h"
#include "Crafting/HollowShipUpgradeDefinition.h"
#include "Engine/AssetManager.h"
#include "Extraction/HollowExtractionResolver.h"
#include "Hub/HollowContractDefinition.h"
#include "Items/HollowItemDefinition.h"
#include "Profile/HollowProfileTypes.h"

#define LOCTEXT_NAMESPACE "HollowHubScreens"

FText FHollowHubNames::GetName(const FPrimaryAssetId& Id) const
{
	const FEntry* Entry = Entries.Find(Id);
	return Entry && !Entry->Name.IsEmpty() ? Entry->Name : FText::FromName(Id.PrimaryAssetName);
}

FText FHollowHubNames::GetDescription(const FPrimaryAssetId& Id) const
{
	const FEntry* Entry = Entries.Find(Id);
	return Entry ? Entry->Description : FText::GetEmpty();
}

void FHollowHubNames::Add(const FPrimaryAssetId& Id, const FText& Name, const FText& Description)
{
	Entries.Add(Id, FEntry{Name, Description});
}

TArray<FHollowContractOffer> FHollowContractOffer::Load(const TConstArrayView<FPrimaryAssetId> Contracts, const int32 CoreValuePercent)
{
	TArray<const UHollowContractDefinition*> Definitions;
	TArray<FPrimaryAssetId> Items;
	for (const FPrimaryAssetId& Id : Contracts)
	{
		const UHollowContractDefinition* Definition = UHollowContractDefinition::Load(Id);
		Definitions.Add(Definition);
		if (Definition)
		{
			for (const FPrimaryAssetId& Item : Definition->RewardItems)
			{
				Items.AddUnique(Item);
			}
		}
	}
	const FHollowItemCatalog Catalog = FHollowItemCatalog::Load(Items);
	TArray<FHollowContractOffer> Offers;
	for (int32 Index = 0; Index < Contracts.Num(); ++Index)
	{
		FHollowContractOffer& Offer = Offers.AddDefaulted_GetRef();
		Offer.Contract = Contracts[Index];
		if (const UHollowContractDefinition* Definition = Definitions[Index])
		{
			Offer.SectorName = Definition->SectorName;
			for (const FPrimaryAssetId& Item : Definition->RewardItems)
			{
				Offer.Rewards.Emplace(Item, FHollowExtractionResolver::ItemEarnings(Catalog, Item, 1, CoreValuePercent));
			}
		}
	}
	return Offers;
}

FHollowHubNames FHollowHubNames::Load(const TConstArrayView<FPrimaryAssetId> Ids)
{
	FHollowHubNames Names;
	if (!UAssetManager::IsInitialized())
	{
		return Names;
	}
	for (const FPrimaryAssetId& Id : Ids)
	{
		const UObject* Asset = Id.IsValid() ? UAssetManager::Get().GetPrimaryAssetPath(Id).TryLoad() : nullptr;
		if (const UHollowItemDefinition* Item = Cast<UHollowItemDefinition>(Asset))
		{
			Names.Add(Id, Item->DisplayName);
		}
		else if (const UHollowCraftingRecipe* Recipe = Cast<UHollowCraftingRecipe>(Asset))
		{
			Names.Add(Id, Recipe->DisplayName);
		}
		else if (const UHollowShipUpgradeDefinition* Upgrade = Cast<UHollowShipUpgradeDefinition>(Asset))
		{
			Names.Add(Id, Upgrade->DisplayName, Upgrade->Description);
		}
		else if (const UHollowContractDefinition* Contract = Cast<UHollowContractDefinition>(Asset))
		{
			Names.Add(Id, Contract->DisplayName, Contract->Description);
		}
		else if (const UHollowClassDefinition* Class = Cast<UHollowClassDefinition>(Asset))
		{
			Names.Add(Id, Class->DisplayName, Class->Description);
		}
	}
	return Names;
}

namespace HollowHubScreenRules
{
	namespace
	{
		/** Map keys in id order: rows never follow a map's iteration order. */
		template <typename ValueType>
		TArray<FPrimaryAssetId> SortedIds(const TMap<FPrimaryAssetId, ValueType>& Map)
		{
			TArray<FPrimaryAssetId> Ids;
			Map.GenerateKeyArray(Ids);
			Ids.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.ToString() < B.ToString(); });
			return Ids;
		}

		/** The stash's items, each once, in the stash's order. */
		TArray<FPrimaryAssetId> StashItems(const FHollowProfile& Profile)
		{
			TArray<FPrimaryAssetId> Items;
			for (const FHollowItemStack& Stack : Profile.Stash)
			{
				Items.AddUnique(Stack.Item);
			}
			return Items;
		}

		int32 InsuredLimit(const FHollowProfile& Profile, const FHollowEconomyTerms& Terms)
		{
			return Terms.MaxInsuredSlots + HollowCraftingRules::SumEffects(Profile.Upgrades, Terms.Upgrades).ExtraInsuredSlots;
		}

		FHollowHubRow Header(const FText& Name)
		{
			FHollowHubRow Row;
			Row.Name = Name;
			Row.bHeader = true;
			return Row;
		}

		/** A line without buttons (an empty list says so). */
		FHollowHubRow Note(const FText& Name)
		{
			FHollowHubRow Row;
			Row.Name = Name;
			return Row;
		}
	}

	FText ResultText(const EHollowTransactionResult Result)
	{
		switch (Result)
		{
		case EHollowTransactionResult::Applied:
			return FText::GetEmpty();
		case EHollowTransactionResult::AlreadyApplied:
			return LOCTEXT("AlreadyApplied", "Already done");
		case EHollowTransactionResult::InRaid:
			return LOCTEXT("InRaid", "A raid is under way: the profile waits for its result");
		case EHollowTransactionResult::BadRequest:
			return LOCTEXT("BadRequest", "Not possible");
		case EHollowTransactionResult::UnknownItem:
			return LOCTEXT("UnknownItem", "Unknown item");
		case EHollowTransactionResult::UnknownRecipe:
			return LOCTEXT("UnknownRecipe", "Unknown recipe");
		case EHollowTransactionResult::NotForSale:
			return LOCTEXT("NotForSale", "The shop does not sell it");
		case EHollowTransactionResult::NotEnoughCredits:
			return LOCTEXT("NotEnoughCredits", "Not enough credits");
		case EHollowTransactionResult::NotEnoughItems:
			return LOCTEXT("NotEnoughItems", "Not enough in the stash");
		case EHollowTransactionResult::StashFull:
			return LOCTEXT("StashFull", "The stash is full");
		case EHollowTransactionResult::LoadoutFull:
			return LOCTEXT("LoadoutFull", "The loadout is full");
		case EHollowTransactionResult::NotForRaids:
			return LOCTEXT("NotForRaids", "Not taken into raids");
		case EHollowTransactionResult::NotInsurable:
			return LOCTEXT("NotInsurable", "Cannot be insured");
		case EHollowTransactionResult::AlreadyInsured:
			return LOCTEXT("AlreadyInsured", "Already insured");
		case EHollowTransactionResult::InsuranceLimit:
			return LOCTEXT("InsuranceLimit", "No insurance slot left");
		case EHollowTransactionResult::UnknownUpgrade:
			return LOCTEXT("UnknownUpgrade", "Unknown upgrade");
		case EHollowTransactionResult::AlreadyOwned:
			return LOCTEXT("AlreadyOwned", "The ship has it already");
		case EHollowTransactionResult::UnknownClass:
			return LOCTEXT("UnknownClass", "Unknown class");
		case EHollowTransactionResult::AlreadySelected:
			return LOCTEXT("AlreadySelected", "The class you play");
		case EHollowTransactionResult::SlotTaken:
			return LOCTEXT("SlotTaken", "Its slot is taken: one tool a hand, one gadget, the relic slots");
		}
		return LOCTEXT("Unknown", "Not possible");
	}

	EHollowTransactionResult Preview(const FHollowProfileState& State, const FHollowProfileTransaction& Transaction, const FHollowEconomyTerms& Terms)
	{
		FHollowProfileState Copy = State;
		return HollowCraftingRules::Apply(Copy, Transaction, Terms);
	}

	FHollowHubAction MakeAction(const FText& Label, const FHollowProfileTransaction& Transaction, const FHollowProfileState& State,
		const FHollowEconomyTerms& Terms)
	{
		FHollowHubAction Action;
		Action.Label = Label;
		Action.Command = EHollowHubCommand::Transaction;
		Action.Transaction = Transaction;
		const EHollowTransactionResult Result = Preview(State, Transaction, Terms);
		Action.bAvailable = Result == EHollowTransactionResult::Applied;
		Action.Reason = ResultText(Result);
		return Action;
	}

	int32 MaxCrafts(const FHollowProfileState& State, const FPrimaryAssetId& Recipe, const FHollowEconomyTerms& Terms)
	{
		const FHollowRecipeTerms* RecipeTerms = Terms.Recipes.Find(Recipe);
		if (!RecipeTerms)
		{
			return 0;
		}
		// What the inputs and credits allow; without either, what the stash could hold.
		int32 Bound = MAX_int32;
		for (const FHollowItemStack& Input : RecipeTerms->Inputs)
		{
			if (Input.Count > 0)
			{
				Bound = FMath::Min(Bound, HollowCraftingRules::CountOf(State.Profile.Stash, Input.Item) / Input.Count);
			}
		}
		if (RecipeTerms->Credits > 0)
		{
			Bound = FMath::Min(Bound, State.Profile.Credits / RecipeTerms->Credits);
		}
		if (Bound == MAX_int32)
		{
			Bound = RecipeTerms->Output.Count > 0 ? Terms.StashSlots * Terms.Catalog.GetMaxStack(RecipeTerms->Output.Item) / RecipeTerms->Output.Count : 0;
		}
		// The rules have the last word (a full stash): the largest count they accept, by bisection (fewer always fit).
		auto Fits = [&](const int32 Times)
		{
			return Preview(State, FHollowProfileTransaction::Craft(Recipe, Times), Terms) == EHollowTransactionResult::Applied;
		};
		int32 Low = 0;
		int32 High = Bound;
		while (Low < High)
		{
			const int32 Middle = Low + (High - Low + 1) / 2;
			if (Fits(Middle))
			{
				Low = Middle;
			}
			else
			{
				High = Middle - 1;
			}
		}
		return Low;
	}

	FText StackText(const FHollowHubNames& Names, const FPrimaryAssetId& Item, const int32 Count)
	{
		const FText Name = Names.GetName(Item);
		return Count > 1 ? FText::Format(LOCTEXT("Stack", "{0} x{1}"), Name, Count) : Name;
	}

	TArray<FHollowHubRow> CraftRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms, const FHollowHubNames& Names)
	{
		TArray<FHollowHubRow> Rows;
		for (const FPrimaryAssetId& Id : SortedIds(Terms.Recipes))
		{
			const FHollowRecipeTerms& Recipe = Terms.Recipes.FindChecked(Id);
			TArray<FText> Inputs;
			for (const FHollowItemStack& Input : Recipe.Inputs)
			{
				Inputs.Add(FText::Format(LOCTEXT("Input", "{0} ({1} in stash)"), StackText(Names, Input.Item, Input.Count),
					HollowCraftingRules::CountOf(State.Profile.Stash, Input.Item)));
			}
			if (Recipe.Credits > 0)
			{
				Inputs.Add(FText::Format(LOCTEXT("Credits", "{0} cr"), Recipe.Credits));
			}
			FHollowHubRow& Row = Rows.AddDefaulted_GetRef();
			Row.Name = Names.GetName(Id);
			Row.Detail = FText::Format(LOCTEXT("Recipe", "{0} -> {1}"), FText::Join(LOCTEXT("Plus", " + "), Inputs),
				StackText(Names, Recipe.Output.Item, Recipe.Output.Count));
			Row.Actions.Add(MakeAction(LOCTEXT("Craft", "Craft"), FHollowProfileTransaction::Craft(Id, 1), State, Terms));
			const int32 Most = MaxCrafts(State, Id, Terms);
			Row.Actions.Add(MakeAction(FText::Format(LOCTEXT("CraftMax", "Craft max ({0})"), Most), FHollowProfileTransaction::Craft(Id, FMath::Max(Most, 1)),
				State, Terms));
		}
		if (Rows.IsEmpty())
		{
			Rows.Add(Note(LOCTEXT("NoRecipes", "No recipes")));
		}
		return Rows;
	}

	TArray<FHollowHubRow> ShopRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms, const FHollowHubNames& Names, const int32 BulkCount)
	{
		TArray<FHollowHubRow> Rows;
		Rows.Add(Header(LOCTEXT("Buy", "BUY")));
		for (const FPrimaryAssetId& Item : SortedIds(Terms.ShopPrices))
		{
			FHollowHubRow& Row = Rows.AddDefaulted_GetRef();
			Row.Name = Names.GetName(Item);
			Row.Detail = FText::Format(LOCTEXT("Offer", "{0} cr each, {1} in stash"), Terms.ShopPrices.FindChecked(Item),
				HollowCraftingRules::CountOf(State.Profile.Stash, Item));
			Row.Badges.Add({EHollowHubBadgeKind::Price, FText::Format(LOCTEXT("Credits", "{0} cr"), Terms.ShopPrices.FindChecked(Item))});
			Row.Actions.Add(MakeAction(LOCTEXT("BuyOne", "Buy 1"), FHollowProfileTransaction::Buy(Item, 1), State, Terms));
			Row.Actions.Add(MakeAction(FText::Format(LOCTEXT("BuyMany", "Buy {0}"), BulkCount), FHollowProfileTransaction::Buy(Item, BulkCount), State, Terms));
		}
		Rows.Add(Header(LOCTEXT("Sell", "SELL")));
		const TArray<FPrimaryAssetId> Stash = StashItems(State.Profile);
		for (const FPrimaryAssetId& Item : Stash)
		{
			const int32 Have = HollowCraftingRules::CountOf(State.Profile.Stash, Item);
			const int32 Price = HollowCraftingRules::SellPrice(Terms, Item);
			FHollowHubRow& Row = Rows.AddDefaulted_GetRef();
			Row.Name = Names.GetName(Item);
			Row.Detail = FText::Format(LOCTEXT("Sale", "{0} in stash, {1} cr each"), Have, Price);
			Row.Actions.Add(MakeAction(LOCTEXT("SellOne", "Sell 1"), FHollowProfileTransaction::Sell(Item, 1), State, Terms));
			Row.Actions.Add(MakeAction(FText::Format(LOCTEXT("SellAll", "Sell all (+{0} cr)"), static_cast<int64>(Price) * Have),
				FHollowProfileTransaction::Sell(Item, Have), State, Terms));
		}
		if (Stash.IsEmpty())
		{
			Rows.Add(Note(LOCTEXT("NothingToSell", "The stash is empty")));
		}
		return Rows;
	}

	TArray<FHollowHubRow> UpgradeRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms, const FHollowHubNames& Names)
	{
		TArray<FHollowHubRow> Rows;
		for (const FPrimaryAssetId& Id : SortedIds(Terms.Upgrades))
		{
			const int32 Price = Terms.Upgrades.FindChecked(Id).Price;
			const bool bOwned = State.Profile.Upgrades.Contains(Id);
			FHollowHubRow& Row = Rows.AddDefaulted_GetRef();
			Row.Name = Names.GetName(Id);
			Row.Detail = Names.GetDescription(Id);
			Row.bHighlighted = bOwned;
			if (!bOwned)
			{
				Row.Badges.Add({EHollowHubBadgeKind::Price, FText::Format(LOCTEXT("Credits", "{0} cr"), Price)});
			}
			Row.Actions.Add(MakeAction(bOwned ? LOCTEXT("Owned", "Owned") : FText::Format(LOCTEXT("BuyUpgrade", "Buy ({0} cr)"), Price),
				FHollowProfileTransaction::BuyUpgrade(Id), State, Terms));
		}
		if (Rows.IsEmpty())
		{
			Rows.Add(Note(LOCTEXT("NoUpgrades", "No ship upgrades")));
		}
		return Rows;
	}

	TArray<FHollowHubRow> StashRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms, const FHollowHubNames& Names)
	{
		TArray<FHollowHubRow> Rows;
		for (const FPrimaryAssetId& Item : StashItems(State.Profile))
		{
			const int32 Have = HollowCraftingRules::CountOf(State.Profile.Stash, Item);
			FHollowHubRow& Row = Rows.AddDefaulted_GetRef();
			Row.Name = Names.GetName(Item);
			Row.Detail = FText::Format(LOCTEXT("InStash", "{0} in stash"), Have);
			Row.Actions.Add(MakeAction(LOCTEXT("TakeOne", "Take 1"), FHollowProfileTransaction::Equip(Item, 1), State, Terms));
			Row.Actions.Add(MakeAction(LOCTEXT("TakeAll", "Take all"), FHollowProfileTransaction::Equip(Item, Have), State, Terms));
		}
		if (Rows.IsEmpty())
		{
			Rows.Add(Note(LOCTEXT("EmptyStash", "The stash is empty")));
		}
		return Rows;
	}

	TArray<FHollowHubRow> LoadoutRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms, const FHollowHubNames& Names)
	{
		TArray<FHollowHubRow> Rows;
		const TArray<FHollowLoadoutSlot>& Loadout = State.Profile.Loadout;
		for (int32 Index = 0; Index < Loadout.Num(); ++Index)
		{
			const FHollowLoadoutSlot& Slot = Loadout[Index];
			const bool bInsurable = Terms.Catalog.IsInsurable(Slot.Item);
			const int32 Price = HollowCraftingRules::InsurancePrice(Terms, Slot);
			FHollowHubRow& Row = Rows.AddDefaulted_GetRef();
			Row.Name = StackText(Names, Slot.Item, Slot.Count);
			Row.Detail = Slot.bInsured ? LOCTEXT("Insured", "Insured: kept if you die")
				: bInsurable ? FText::Format(LOCTEXT("Uninsured", "Not insured (insurance {0} cr)"), Price) : LOCTEXT("Uninsurable", "Cannot be insured");
			Row.bHighlighted = Slot.bInsured;
			if (Slot.bInsured)
			{
				Row.Badges.Add({EHollowHubBadgeKind::Insured, LOCTEXT("InsuredBadge", "INSURED")});
			}
			if (Slot.Count > 1)
			{
				Row.Actions.Add(MakeAction(LOCTEXT("ReturnOne", "Return 1"), FHollowProfileTransaction::Unequip(Index, 1), State, Terms));
			}
			Row.Actions.Add(MakeAction(LOCTEXT("ReturnAll", "Return all"), FHollowProfileTransaction::Unequip(Index), State, Terms));
			const FText InsureLabel = Slot.bInsured ? LOCTEXT("IsInsured", "Insured")
				: bInsurable ? FText::Format(LOCTEXT("Insure", "Insure ({0} cr)"), Price) : LOCTEXT("InsureNot", "Insure");
			Row.Actions.Add(MakeAction(InsureLabel, FHollowProfileTransaction::Insure(Index), State, Terms));
		}
		if (Rows.IsEmpty())
		{
			Rows.Add(Note(LOCTEXT("EmptyLoadout", "The loadout is empty")));
		}
		return Rows;
	}

	TArray<FHollowHubRow> ClassRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms, const FHollowHubNames& Names)
	{
		TArray<FHollowHubRow> Rows;
		const FPrimaryAssetId Current = HollowCraftingRules::CurrentClass(State.Profile, Terms);
		for (const FPrimaryAssetId& Id : SortedIds(Terms.Classes))
		{
			const bool bPlayed = Id == Current;
			FHollowHubRow& Row = Rows.AddDefaulted_GetRef();
			Row.Name = Names.GetName(Id);
			Row.Detail = Names.GetDescription(Id);
			Row.bHighlighted = bPlayed;
			Row.Actions.Add(MakeAction(bPlayed ? LOCTEXT("Playing", "Playing") : LOCTEXT("ChooseClass", "Choose"), FHollowProfileTransaction::SelectClass(Id),
				State, Terms));
		}
		if (Rows.IsEmpty())
		{
			Rows.Add(Note(LOCTEXT("NoClasses", "No classes")));
		}
		return Rows;
	}

	FText OfferText(const FHollowContractOffer& Offer, const FHollowHubNames& Names)
	{
		TArray<FText> Rewards;
		for (const TPair<FPrimaryAssetId, int32>& Reward : Offer.Rewards)
		{
			Rewards.Add(FText::Format(LOCTEXT("RewardItem", "{0} {1} cr"), Names.GetName(Reward.Key), Reward.Value));
		}
		const FText Sector = Offer.SectorName.IsEmpty() ? LOCTEXT("NoSector", "Unknown sector") : Offer.SectorName;
		return Rewards.IsEmpty() ? Sector : FText::Format(LOCTEXT("ContractOffer", "{0}. Reward: {1}"), Sector, FText::Join(FText::FromString(TEXT(", ")), Rewards));
	}

	FText ContractRoleText(const TConstArrayView<FHollowContractOffer> Offers, const FPrimaryAssetId& Selected, const bool bHost, const FHollowHubNames& Names)
	{
		const FText Role = bHost ? LOCTEXT("HostRole", "Choose the squad's contract, then launch.") : LOCTEXT("GuestRole", "The host chooses the contract and launches.");
		const FHollowContractOffer* Chosen = Offers.FindByPredicate([&Selected](const FHollowContractOffer& Offer) { return Offer.Contract == Selected; });
		if (!Selected.IsValid() || !Chosen)
		{
			return Role;
		}
		// Two lines: the role text does not wrap, the summary is as long as a row's offer.
		return FText::Format(bHost ? LOCTEXT("HostChosen", "{0}\nNow: {1} - {2}") : LOCTEXT("GuestChosen", "{0}\nThe host chose: {1} - {2}"), Role,
			Names.GetName(Selected), OfferText(*Chosen, Names));
	}

	int32 CoreValuePercent(const FHollowProfileState& State, const FHollowEconomyTerms& Terms)
	{
		return HollowCraftingRules::SumEffects(State.Profile.Upgrades, Terms.Upgrades).CoreValuePercent;
	}

	TArray<FHollowHubRow> ContractRows(const TConstArrayView<FHollowContractOffer> Offers, const FPrimaryAssetId& Selected, const bool bHost,
		const FHollowHubNames& Names)
	{
		TArray<FHollowContractOffer> Sorted(Offers);
		Sorted.Sort([](const FHollowContractOffer& A, const FHollowContractOffer& B) { return A.Contract.ToString() < B.Contract.ToString(); });
		TArray<FHollowHubRow> Rows;
		for (const FHollowContractOffer& Offer : Sorted)
		{
			const FPrimaryAssetId& Id = Offer.Contract;
			const bool bSelected = Id == Selected;
			FHollowHubRow& Row = Rows.AddDefaulted_GetRef();
			Row.Name = Names.GetName(Id);
			const FText Description = Names.GetDescription(Id);
			Row.Detail = Description.IsEmpty() ? OfferText(Offer, Names) : FText::Format(LOCTEXT("OfferDetail", "{0}. {1}"), OfferText(Offer, Names), Description);
			Row.bHighlighted = bSelected;
			if (bHost)
			{
				FHollowHubAction& Action = Row.Actions.AddDefaulted_GetRef();
				Action.Command = EHollowHubCommand::SelectContract;
				Action.Contract = Id;
				Action.Label = bSelected ? LOCTEXT("Chosen", "Chosen") : LOCTEXT("Choose", "Choose");
				Action.bAvailable = !bSelected;
				Action.Reason = bSelected ? LOCTEXT("AlreadyChosen", "The squad's contract already") : FText::GetEmpty();
			}
			else if (bSelected)
			{
				Row.Detail = FText::Format(LOCTEXT("HostsChoice", "The host's choice. {0}"), Row.Detail);
			}
		}
		if (Rows.IsEmpty())
		{
			Rows.Add(Note(LOCTEXT("NoContracts", "No contracts")));
		}
		return Rows;
	}

	FText StatusLine(const FHollowProfileState& State, const FHollowEconomyTerms& Terms)
	{
		const FHollowProfile& Profile = State.Profile;
		const int32 Insured = static_cast<int32>(Algo::CountIf(Profile.Loadout, [](const FHollowLoadoutSlot& Slot) { return Slot.bInsured; }));
		FFormatNamedArguments Args;
		Args.Add(TEXT("Credits"), Profile.Credits);
		Args.Add(TEXT("Stash"), Profile.Stash.Num());
		Args.Add(TEXT("StashSlots"), Terms.StashSlots);
		Args.Add(TEXT("Loadout"), Profile.Loadout.Num());
		Args.Add(TEXT("LoadoutSlots"), Terms.MaxLoadoutSlots);
		Args.Add(TEXT("Insured"), Insured);
		Args.Add(TEXT("InsuredLimit"), InsuredLimit(Profile, Terms));
		const FText Line = FText::Format(LOCTEXT("Status", "Credits {Credits}    Stash {Stash}/{StashSlots}    Loadout {Loadout}/{LoadoutSlots}    Insured {Insured}/{InsuredLimit}"), Args);
		return State.Escrow.IsActive() ? FText::Format(LOCTEXT("StatusInRaid", "{0}    {1}"), Line, ResultText(EHollowTransactionResult::InRaid)) : Line;
	}

	TArray<FHollowHubBadge> StatusBadges(const FHollowProfileState& State, const FHollowEconomyTerms& Terms)
	{
		const FHollowProfile& Profile = State.Profile;
		const int32 Insured = static_cast<int32>(Algo::CountIf(Profile.Loadout, [](const FHollowLoadoutSlot& Slot) { return Slot.bInsured; }));
		return {
			{EHollowHubBadgeKind::Price, FText::Format(LOCTEXT("Credits", "{0} cr"), Profile.Credits)},
			{EHollowHubBadgeKind::Slot, FText::Format(LOCTEXT("StashBadge", "Stash {0}/{1}"), Profile.Stash.Num(), Terms.StashSlots)},
			{EHollowHubBadgeKind::Slot, FText::Format(LOCTEXT("LoadoutBadge", "Loadout {0}/{1}"), Profile.Loadout.Num(), Terms.MaxLoadoutSlots)},
			{EHollowHubBadgeKind::Insured, FText::Format(LOCTEXT("InsuredCountBadge", "Insured {0}/{1}"), Insured, InsuredLimit(Profile, Terms))},
		};
	}

	TArray<FPrimaryAssetId> NamedIds(const FHollowProfileState& State, const FHollowEconomyTerms& Terms)
	{
		TArray<FPrimaryAssetId> Ids = SortedIds(Terms.Recipes);
		Ids.Append(SortedIds(Terms.Upgrades));
		Ids.Append(SortedIds(Terms.Classes));
		Ids.Append(SortedIds(Terms.ShopPrices));
		for (const TPair<FPrimaryAssetId, FHollowRecipeTerms>& Recipe : Terms.Recipes)   // collected, not ordered: names are looked up
		{
			for (const FHollowItemStack& Input : Recipe.Value.Inputs)
			{
				Ids.AddUnique(Input.Item);
			}
			Ids.AddUnique(Recipe.Value.Output.Item);
		}
		for (const FHollowItemStack& Stack : State.Profile.Stash)
		{
			Ids.AddUnique(Stack.Item);
		}
		for (const FHollowLoadoutSlot& Slot : State.Profile.Loadout)
		{
			Ids.AddUnique(Slot.Item);
		}
		return Ids;
	}
}

#undef LOCTEXT_NAMESPACE
