// Development console commands of the hub economy (ADR-0031): the shop, the workbench and the loadout locker on the local
// profile, until the hub's terminals and screens (S5-03, S5-04). Never shipped.
#include "Crafting/HollowCraftingRules.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HollowGameLog.h"
#include "Profile/HollowProfileSubsystem.h"
#include "Util/HollowEnumUtils.h"

#if !UE_BUILD_SHIPPING

namespace HollowEconomyConsole
{
	FPrimaryAssetId ItemId(const FString& Name)
	{
		return FPrimaryAssetId(TEXT("HollowItem"), FName(*Name));
	}

	/** The count argument at Index, or Default if there is none. */
	int32 CountArg(const TArray<FString>& Args, const int32 Index, const int32 Default)
	{
		return Args.IsValidIndex(Index) && Args[Index].IsNumeric() ? FCString::Atoi(*Args[Index]) : Default;
	}

	FString Stacks(const TConstArrayView<FHollowItemStack> Items)
	{
		TArray<FString> Parts;
		for (const FHollowItemStack& Stack : Items)
		{
			Parts.Add(FString::Printf(TEXT("%s x%d"), *Stack.Item.PrimaryAssetName.ToString(), Stack.Count));
		}
		return Parts.IsEmpty() ? FString(TEXT("-")) : FString::Join(Parts, TEXT(", "));
	}

	void Run(UWorld* World, const FHollowProfileTransaction& Transaction, const TCHAR* Command)
	{
		UHollowProfileSubsystem* Profiles = UHollowProfileSubsystem::Get(World);
		if (!Profiles)
		{
			UE_LOG(LogHollowGame, Warning, TEXT("%s: no profile."), Command);
			return;
		}
		const EHollowTransactionResult Result = Profiles->ApplyTransaction(Transaction);
		UE_LOG(LogHollowGame, Display, TEXT("%s: %s -> %s, %d credits"), Command, *Transaction.Describe(), *HollowEnumToString(Result),
			Profiles->GetProfile().Credits);
	}

	/** The shop as this profile sees it: offers, what the stash sells for, what each loadout slot's insurance costs. */
	void ShopDump(UWorld* World)
	{
		const UHollowProfileSubsystem* Profiles = UHollowProfileSubsystem::Get(World);
		if (!Profiles)
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Shop.Dump: no profile."));
			return;
		}
		FHollowProfileState State;
		State.Profile = Profiles->GetProfile();
		const FHollowEconomyTerms Terms = FHollowEconomyTerms::Load(State);
		const FHollowProfile& Profile = State.Profile;
		TArray<FString> Offers;
		for (const TPair<FPrimaryAssetId, int32>& Offer : Terms.ShopPrices)
		{
			Offers.Add(FString::Printf(TEXT("%s %d"), *Offer.Key.PrimaryAssetName.ToString(), Offer.Value));
		}
		Offers.Sort();   // the map's order is not stable
		TArray<FString> Sells;
		for (const FHollowItemStack& Stack : Profile.Stash)
		{
			Sells.Add(FString::Printf(TEXT("%s x%d at %d"), *Stack.Item.PrimaryAssetName.ToString(), Stack.Count, HollowCraftingRules::SellPrice(Terms, Stack.Item)));
		}
		TArray<FString> Slots;
		for (int32 Index = 0; Index < Profile.Loadout.Num(); ++Index)
		{
			const FHollowLoadoutSlot& Slot = Profile.Loadout[Index];
			Slots.Add(FString::Printf(TEXT("[%d] %s x%d %s"), Index, *Slot.Item.PrimaryAssetName.ToString(), Slot.Count, Slot.bInsured ? TEXT("insured")
				: Terms.Catalog.IsInsurable(Slot.Item) ? *FString::Printf(TEXT("insure %d"), HollowCraftingRules::InsurancePrice(Terms, Slot)) : TEXT("uninsurable")));
		}
		UE_LOG(LogHollowGame, Display, TEXT("Hollow.Shop.Dump: %d credits | offers %s | stash (%d/%d slots) %s | loadout %s | insured at most %d"), Profile.Credits,
			*FString::Join(Offers, TEXT(", ")), Profile.Stash.Num(), Terms.StashSlots, Sells.IsEmpty() ? TEXT("-") : *FString::Join(Sells, TEXT(", ")),
			Slots.IsEmpty() ? TEXT("-") : *FString::Join(Slots, TEXT(", ")), Terms.MaxInsuredSlots);
	}

	/** Every recipe, and how many times the stash and credits allow it now. */
	void CraftDump(UWorld* World)
	{
		const UHollowProfileSubsystem* Profiles = UHollowProfileSubsystem::Get(World);
		if (!Profiles)
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Craft.Dump: no profile."));
			return;
		}
		FHollowProfileState State;
		State.Profile = Profiles->GetProfile();
		const FHollowEconomyTerms Terms = FHollowEconomyTerms::Load(State);
		TArray<FString> Lines;
		for (const TPair<FPrimaryAssetId, FHollowRecipeTerms>& Pair : Terms.Recipes)
		{
			const FHollowRecipeTerms& Recipe = Pair.Value;
			int32 Times = Recipe.Credits > 0 ? State.Profile.Credits / Recipe.Credits : MAX_int32;
			for (const FHollowItemStack& Input : Recipe.Inputs)
			{
				Times = FMath::Min(Times, Input.Count > 0 ? HollowCraftingRules::CountOf(State.Profile.Stash, Input.Item) / Input.Count : 0);
			}
			Lines.Add(FString::Printf(TEXT("%s: %s + %d cr -> %s x%d, possible now %d"), *Recipe.Id.PrimaryAssetName.ToString(), *Stacks(Recipe.Inputs), Recipe.Credits,
				*Recipe.Output.Item.PrimaryAssetName.ToString(), Recipe.Output.Count, Times));
		}
		Lines.Sort();
		for (const FString& Line : Lines)
		{
			UE_LOG(LogHollowGame, Display, TEXT("Hollow.Craft.Dump: %s"), *Line);
		}
		UE_CLOG(Lines.IsEmpty(), LogHollowGame, Display, TEXT("Hollow.Craft.Dump: no recipes."));
	}

	void Buy(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.IsEmpty())
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage: Hollow.Shop.Buy <Item> [Count]"));
			return;
		}
		Run(World, FHollowProfileTransaction::Buy(ItemId(Args[0]), CountArg(Args, 1, 1)), TEXT("Hollow.Shop.Buy"));
	}

	void Sell(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.IsEmpty())
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage: Hollow.Shop.Sell <Item> [Count]"));
			return;
		}
		Run(World, FHollowProfileTransaction::Sell(ItemId(Args[0]), CountArg(Args, 1, 1)), TEXT("Hollow.Shop.Sell"));
	}

	void Insure(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.IsEmpty() || !Args[0].IsNumeric())
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage: Hollow.Shop.Insure <Slot>"));
			return;
		}
		Run(World, FHollowProfileTransaction::Insure(FCString::Atoi(*Args[0])), TEXT("Hollow.Shop.Insure"));
	}

	void Craft(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.IsEmpty())
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage: Hollow.Craft.Make <Recipe> [Times]"));
			return;
		}
		Run(World, FHollowProfileTransaction::Craft(FPrimaryAssetId(TEXT("HollowRecipe"), FName(*Args[0])), CountArg(Args, 1, 1)), TEXT("Hollow.Craft.Make"));
	}

	void Equip(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.IsEmpty())
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage: Hollow.Profile.Equip <Item> [Count]"));
			return;
		}
		Run(World, FHollowProfileTransaction::Equip(ItemId(Args[0]), CountArg(Args, 1, 1)), TEXT("Hollow.Profile.Equip"));
	}

	void Unequip(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.IsEmpty() || !Args[0].IsNumeric())
		{
			UE_LOG(LogHollowGame, Warning, TEXT("Usage: Hollow.Profile.Unequip <Slot> [Count]"));
			return;
		}
		Run(World, FHollowProfileTransaction::Unequip(FCString::Atoi(*Args[0]), CountArg(Args, 1, 0)), TEXT("Hollow.Profile.Unequip"));
	}

	FAutoConsoleCommandWithWorld ShopDumpCommand(
		TEXT("Hollow.Shop.Dump"),
		TEXT("Prints the shop for the local profile: offers and prices, what the stash sells for, each loadout slot's insurance price."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ShopDump));

	FAutoConsoleCommandWithWorldAndArgs ShopBuyCommand(
		TEXT("Hollow.Shop.Buy"),
		TEXT("Buys an item into the local profile's stash. Usage: Hollow.Shop.Buy <Item> [Count]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Buy));

	FAutoConsoleCommandWithWorldAndArgs ShopSellCommand(
		TEXT("Hollow.Shop.Sell"),
		TEXT("Sells an item from the local profile's stash. Usage: Hollow.Shop.Sell <Item> [Count]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Sell));

	FAutoConsoleCommandWithWorldAndArgs ShopInsureCommand(
		TEXT("Hollow.Shop.Insure"),
		TEXT("Buys single-use insurance for a loadout slot of the local profile. Usage: Hollow.Shop.Insure <Slot>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Insure));

	FAutoConsoleCommandWithWorld CraftDumpCommand(
		TEXT("Hollow.Craft.Dump"),
		TEXT("Prints every workbench recipe and how many times the local profile can craft it now."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&CraftDump));

	FAutoConsoleCommandWithWorldAndArgs CraftMakeCommand(
		TEXT("Hollow.Craft.Make"),
		TEXT("Crafts a recipe on the local profile (stash in, stash out). Usage: Hollow.Craft.Make <Recipe> [Times]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Craft));

	FAutoConsoleCommandWithWorldAndArgs ProfileEquipCommand(
		TEXT("Hollow.Profile.Equip"),
		TEXT("Moves an item from the local profile's stash into its loadout. Usage: Hollow.Profile.Equip <Item> [Count]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Equip));

	FAutoConsoleCommandWithWorldAndArgs ProfileUnequipCommand(
		TEXT("Hollow.Profile.Unequip"),
		TEXT("Moves a loadout slot (or Count of it) back into the stash; a whole slot loses its insurance. Usage: Hollow.Profile.Unequip <Slot> [Count]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Unequip));
}

#endif // !UE_BUILD_SHIPPING
